#ifndef TORCH_V8_H
#define TORCH_V8_H

#include <torch/torch.h>
#include <vector>
#include <tuple>
#include <string>
#include <map>
#include <filesystem>

#include <fstream>
#include <stdexcept>

#include "torch_modelconfig.h"
#include "torch_backbone.h"
#include "torch_pan.h"
#include "torch_v8loss.h"
#include "torch_yolo_head.h"

#include <torch/csrc/serialization.h>

#include "torch_yolo_dataset.h"
#include "torch_parser.h"
#include "torch_mermaid.h"

// Comment tags used in this file:
// KEY: important execution path.
// MODIFIED: behavior adjusted during the current cleanup pass.
// CHECK: confirm during integration/debug.
// RISK: known limitation or possible issue.
// EVOLVE: recommended future improvement.
// VERIFY: needs explicit runtime or numerical validation.

inline torch::optim::SGDOptions make_yolo_sgd_options(const TrainConfig& train_config) {
    auto optimizer_config = train_config.optimizer_config();
    optimizer_config.validate();
    return torch::optim::SGDOptions(optimizer_config.lr)
        .momentum(optimizer_config.momentum)
        .weight_decay(optimizer_config.weight_decay);
}

inline torch::Device resolve_yolo_runtime_device(const YoloTrainRuntimeConfig& runtime_config) {
    runtime_config.validate();
    if (runtime_config.prefer_cuda && torch::cuda::is_available()) {
        return torch::Device(torch::kCUDA);
    }
    return torch::Device(torch::kCPU);
}

struct YoloTrainProgress {
    float epoch_loss = 0.0f;
    int batch_count = 0;

    void record_batch(float loss_value) {
        epoch_loss += loss_value;
        batch_count += 1;
    }

    float average_loss() const {
        return batch_count > 0 ? epoch_loss / static_cast<float>(batch_count) : 0.0f;
    }
};

inline void print_yolo_train_batch_log(
    int epoch,
    int total_epochs,
    int batch_count,
    float loss_value,
    const YoloTrainRuntimeConfig& runtime_config) {

    if (batch_count % runtime_config.log_interval != 0) {
        return;
    }

    printf("\rEpoch [%d/%d] Batch [%d] Loss: %.4f",
        epoch + 1, total_epochs, batch_count, loss_value);
    fflush(stdout);
}

inline bool should_stop_yolo_train_epoch(
    const YoloTrainProgress& progress,
    const YoloTrainRuntimeConfig& runtime_config) {
    return runtime_config.max_train_batches > 0 &&
        progress.batch_count >= runtime_config.max_train_batches;
}

inline void maybe_save_yolo_checkpoint(
    const torch::nn::Module& module,
    int epoch,
    const YoloTrainRuntimeConfig& runtime_config) {

    if ((epoch + 1) % runtime_config.checkpoint.save_interval != 0) {
        return;
    }

    std::filesystem::create_directories(runtime_config.checkpoint.save_path);
    std::string save_file = runtime_config.checkpoint.save_path + "/epoch_" + std::to_string(epoch + 1) + ".pt";

    torch::serialize::OutputArchive archive;
    module.save(archive);
    archive.save_to(save_file);
    std::cout << "[YOLOv8] Checkpoint saved to " << save_file << std::endl;
}

struct YoloModelBuildConfig {
    YoloDetectHeadConfig head;
    YoloLossConfig loss;
};

struct YoloEvalConfig {
    YoloValidationConfig data;
    YoloPostProcessConfig postprocess;

    void validate() const {
        data.validate();
        postprocess.validate();
        TORCH_CHECK(postprocess.num_classes > 0, "eval postprocess num_classes must be positive");
    }
};

inline std::vector<BBox> collect_valid_target_boxes(const torch::Tensor& targets_for_image) {
    TORCH_CHECK(targets_for_image.dim() == 2 && targets_for_image.size(1) == 6,
        "targets_for_image must be [N, 6], got ", targets_for_image.sizes());

    std::vector<BBox> out;
    auto cpu = targets_for_image.to(torch::kCPU);
    for (int64_t i = 0; i < cpu.size(0); ++i) {
        if (cpu[i][1].item<float>() < 0.0f) {
            continue;
        }
        BBox box;
        box.cls = static_cast<int64_t>(cpu[i][1].item<float>());
        box.x1 = cpu[i][2].item<float>();
        box.y1 = cpu[i][3].item<float>();
        box.x2 = cpu[i][4].item<float>();
        box.y2 = cpu[i][5].item<float>();
        box.score = 1.0f;
        out.push_back(box);
    }
    return out;
}

inline YoloModelBuildConfig normalize_yolo_build_config(YoloModelBuildConfig build_config) {
    if (build_config.head.use_dfl || build_config.loss.enable_dfl) {
        TORCH_CHECK(build_config.head.use_dfl == build_config.loss.enable_dfl,
            "YOLO build config requires head.use_dfl and loss.enable_dfl to match");
        build_config.loss.reg_max = build_config.head.reg_max;
    }
    return build_config;
}

inline YoloEvalConfig make_yolo_eval_config(const ModelConfig& model_config, const YoloValidationConfig& validation_config) {
    YoloEvalConfig config;
    config.data = validation_config;
    config.postprocess.num_classes = model_config.num_classes;
    return config;
}

// =========================================================================
// YOLOv8 model wrapper
// =========================================================================
class YOLOv8Impl : public torch::nn::Module {
public:
    YOLOv8Impl(ModelConfig config)
        : YOLOv8Impl(config, YoloModelBuildConfig{}) {
    }

    YOLOv8Impl(ModelConfig config, YoloModelBuildConfig build_config)
        : config_(config), build_config_(normalize_yolo_build_config(std::move(build_config))) {
        // KEY: backbone extracts multi-scale feature maps.
        std::vector<int64_t> base_channels = { 64, 128, 256, 512, 1024 };
        backbone_ = register_module("backbone",
            YOLOv8Backbone(base_channels, config.depth_multiple, config.width_multiple));

        // KEY: PAN neck fuses multi-scale backbone features.
        auto backbone_out_ch = backbone_->get_out_channels();
        pan_ = register_module("neck",
            PAN(backbone_out_ch, config.depth_multiple, config.width_multiple));

        // KEY: detection head produces box and class outputs.
        head_ = register_module("head",
            YOLOv8Detect(config.num_classes, backbone_out_ch, config.strides, build_config_.head));

        // KEY: loss module is used by the training helpers below.
        loss_fn_ = register_module("loss_fn", YOLOv8Loss(config.num_classes, build_config_.loss));
    }

    // MODIFIED: local helper for pickle/state_dict loading.
    std::vector<char> get_the_bytes(const std::string& path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);

        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file: " + path);
        }

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<char> buffer(size);
        if (!file.read(buffer.data(), size)) {
            throw std::runtime_error("Failed to read file: " + path);
        }

        return buffer;
    }

    void load_weights(const std::string& path) {
        // CHECK: this expects a Python-exported state_dict with matching C++ keys.
        torch::NoGradGuard no_grad;

        std::vector<char> f = get_the_bytes(path);
        torch::IValue data = torch::pickle_load(f);

        auto dict = data.toGenericDict();

        auto params = this->named_parameters();
        auto buffers = this->named_buffers();

        for (auto& item : dict) {
            std::string key = item.key().toStringRef();
            torch::Tensor val = item.value().toTensor();

            auto p_it = params.find(key);
            if (p_it != nullptr) {
                p_it->copy_(val);
                std::cout << "Loaded param: " << key << std::endl;
                continue;
            }

            auto b_it = buffers.find(key);
            if (b_it != nullptr) {
                b_it->copy_(val);
                std::cout << "Loaded buffer: " << key << std::endl;
                continue;
            }

            std::cerr << "Warning: Key not found in C++ model: " << key << std::endl;
        }
    }

    void load_checkpoint(const std::string & path) {
        // MODIFIED: archive-based loader used by checkpoints saved from C++.
        torch::serialize::InputArchive archive;
        archive.load_from(path);
        this->load(archive);
    }

    void export_structure(const std::string& path = "yolov8.mmd") {
        // KEY: structural export for graph/debug inspection.
        torch_utils::MermaidGenerator::generate(*this, path, "YOLOv8");

        // torch::Tensor x = torch::randn({1, 3, 224, 224}, this->parameters()[0].device());
        // torch_utils::MermaidGenerator::generate_from_trace(*this, {x}, "resnet18_trace.mmd");
    }

// =========================================================================
// Training helper
// =========================================================================
    void train(const TrainConfig& train_config, const std::string& pretrained_weights = "") {
        train(train_config, train_config.runtime_config(pretrained_weights));
    }

    void train(const TrainConfig& train_config, const YoloTrainRuntimeConfig& runtime_config) {
        std::cout << "\n[YOLOv8] Starting training..." << std::endl;

        // KEY: choose runtime device before model and dataloader setup.
        torch::Device device = resolve_yolo_runtime_device(runtime_config);
        this->to(device);
        std::cout << "[YOLOv8] Device: " << (device.is_cuda() ? "GPU" : "CPU") << std::endl;

        // CHECK: pretrained loading here uses WeightParser, not the local pickle loader.
        if (!runtime_config.pretrained_weights.empty()) {
            std::cout << "[YOLOv8] Loading pretrained weights from: " << runtime_config.pretrained_weights << std::endl;
            try {
                WeightParser parser(runtime_config.pretrained_weights);
                parser.load_to(*this);
            }
            catch (const std::exception& e) {
                std::cerr << "[Warn] Failed to load weights: " << e.what() << ". Training from scratch." << std::endl;
            }
        }

        // KEY: dataset returns padded targets in [max_gt, 6] format.
        std::cout << "[YOLOv8] Loading dataset from: " << train_config.data_path << std::endl;
        auto train_paths = make_yolo_split_paths(train_config.data_path, "train");
        auto dataset_config = make_yolo_dataset_config(train_config, true);
        auto dataset = YoloDataset(train_paths, dataset_config).map(torch::data::transforms::Stack<>());

        auto data_loader = torch::data::make_data_loader(
            std::move(dataset),
            make_yolo_loader_options(train_config.batch_size, train_config.dataloader_workers)
        );

        // KEY: SGD configuration follows a YOLO-style momentum/weight decay setup.
        torch::optim::SGD optimizer(
            this->parameters(),
            make_yolo_sgd_options(train_config)
        );

        for (int epoch = 0; epoch < train_config.epochs; ++epoch) {
            torch::nn::Module::train(true);

            YoloTrainProgress progress;

            for (auto& batch : *data_loader) {
                auto imgs = batch.data.to(device);
                auto targets = batch.target.to(device);

                // KEY: write batch indices into target column 0 before loss computation.
                for (int b = 0; b < targets.size(0); ++b) {
                    targets[b].select(1, 0).fill_(static_cast<float>(b));
                }

                optimizer.zero_grad();

                // KEY: train_step_internal runs backbone + neck + head + loss in one call.
                auto result = this->train_step_internal(imgs, targets);
                torch::Tensor total_loss = std::get<0>(result);

                total_loss.backward();
                optimizer.step();

                float loss_value = total_loss.item<float>();
                progress.record_batch(loss_value);
                print_yolo_train_batch_log(epoch, train_config.epochs, progress.batch_count, loss_value, runtime_config);
                if (should_stop_yolo_train_epoch(progress, runtime_config)) {
                    std::cout << "\n[YOLOv8] Smoke stop after "
                              << progress.batch_count
                              << " batch(es) in epoch " << epoch + 1 << std::endl;
                    break;
                }
            }

            float avg_loss = progress.average_loss();
            std::cout << "\nEpoch " << epoch + 1 << " Finished. Avg Loss: " << avg_loss << std::endl;

            maybe_save_yolo_checkpoint(*this, epoch, runtime_config);
        }
        std::cout << "[YOLOv8] Training completed." << std::endl;
    }
// =========================================================================
// Validation helper
// =========================================================================
    std::map<std::string, float> val(
        const std::string& data_path,
        int batch_size = 16,
        int img_size = 640,
        YoloResizePolicy resize_policy = YoloResizePolicy::PlainResize,
        int max_gt = 50) {
        YoloValidationConfig config;
        config.batch_size = batch_size;
        config.img_size = img_size;
        config.max_gt = max_gt;
        config.resize_policy = resize_policy;
        return val(data_path, config);
    }

    std::map<std::string, float> val(const std::string& data_path, const YoloValidationConfig& val_config) {
        auto eval_summary = val_summary(data_path, make_yolo_eval_config(config_, val_config));
        return {
            {"val/loss", eval_summary.loss},
            {"val/box_loss", eval_summary.box_loss},
            {"val/cls_loss", eval_summary.cls_loss},
            {"val/dfl_loss", eval_summary.dfl_loss},
            {"val/mAP50", eval_summary.map50_proxy},
            {"val/avg_conf", eval_summary.avg_confidence},
            {"val/precision", eval_summary.precision},
            {"val/recall", eval_summary.recall},
            {"val/f1", eval_summary.f1},
            {"val/matched_iou", eval_summary.matched_iou},
            {"val/tp", static_cast<float>(eval_summary.true_positives)},
            {"val/fp", static_cast<float>(eval_summary.false_positives)},
            {"val/fn", static_cast<float>(eval_summary.false_negatives)},
            {"val/pred_boxes", static_cast<float>(eval_summary.predicted_boxes)},
            {"val/target_boxes", static_cast<float>(eval_summary.target_boxes)}
        };
    }

    struct ValidationSummary {
        using ClassStats = YoloEvalSummary::ClassStats;

        float loss = 0.0f;
        float box_loss = 0.0f;
        float cls_loss = 0.0f;
        float dfl_loss = 0.0f;
        float map50_proxy = 0.0f;
        float avg_confidence = 0.0f;
        float precision = 0.0f;
        float recall = 0.0f;
        float f1 = 0.0f;
        float matched_iou = 0.0f;
        int64_t true_positives = 0;
        int64_t false_positives = 0;
        int64_t false_negatives = 0;
        int64_t predicted_boxes = 0;
        int64_t target_boxes = 0;
        std::map<int64_t, ClassStats> per_class;
    };

    ValidationSummary val_summary(const std::string& data_path, const YoloEvalConfig& eval_config) {
        eval_config.validate();

        std::cout << "\n[YOLOv8] Starting validation..." << std::endl;
        torch::NoGradGuard no_grad;
        torch::nn::Module::eval();

        torch::Device device = this->parameters().begin()->device();

        auto val_paths = make_yolo_split_paths(data_path, "val");
        auto dataset_config = make_yolo_eval_config(eval_config.data);

        auto dataset = YoloDataset(val_paths, dataset_config).map(torch::data::transforms::Stack<>());

        auto data_loader = torch::data::make_data_loader(
            std::move(dataset),
            make_yolo_loader_options(eval_config.data.batch_size, eval_config.data.dataloader_workers)
        );

        ValidationSummary summary;
        int batch_count = 0;
        std::vector<std::vector<BBox>> all_detections;
        std::vector<std::vector<BBox>> all_targets;

        for (auto& batch : *data_loader) {
            auto imgs = batch.data.to(device);
            auto targets = batch.target.to(device);

            for (int b = 0; b < targets.size(0); ++b) {
                targets[b].select(1, 0).fill_(static_cast<float>(b));
            }

            auto result = this->train_step_internal(imgs, targets);

            summary.loss += std::get<0>(result).item<float>();
            auto loss_items = std::get<1>(result);
            summary.box_loss += loss_items["box_loss"];
            summary.cls_loss += loss_items["cls_loss"];
            summary.dfl_loss += loss_items["dfl_loss"];

            auto pred = this->forward(imgs);
            auto batch_detections = post_process_batch(pred, eval_config.postprocess);
            for (auto& dets : batch_detections) {
                all_detections.push_back(std::move(dets));
            }

            for (int64_t b = 0; b < targets.size(0); ++b) {
                auto image_targets = collect_valid_target_boxes(targets[b]);
                summary.target_boxes += static_cast<int64_t>(image_targets.size());
                all_targets.push_back(std::move(image_targets));
            }

            batch_count++;
            printf("\r[Val] Processing batch %d...", batch_count);
        }
        std::cout << std::endl;

        if (batch_count > 0) {
            summary.loss /= batch_count;
            summary.box_loss /= batch_count;
            summary.cls_loss /= batch_count;
            summary.dfl_loss /= batch_count;
        }

        auto detection_summary = summarize_yolo_detections(all_detections, summary.target_boxes);
        auto match_summary = summarize_yolo_matches(all_detections, all_targets, eval_config.postprocess.iou_threshold);
        summary.avg_confidence = detection_summary.avg_confidence;
        summary.predicted_boxes = detection_summary.predicted_boxes;
        summary.target_boxes = detection_summary.target_boxes;
        summary.map50_proxy = match_summary.recall;
        summary.precision = match_summary.precision;
        summary.recall = match_summary.recall;
        summary.f1 = match_summary.f1;
        summary.matched_iou = match_summary.matched_iou;
        summary.true_positives = match_summary.true_positives;
        summary.false_positives = match_summary.false_positives;
        summary.false_negatives = match_summary.false_negatives;
        summary.per_class = match_summary.per_class;

        std::cout << "[YOLOv8] Validation Results: Loss=" << summary.loss
                  << " AvgConf=" << summary.avg_confidence
                  << " TP=" << summary.true_positives
                  << " FP=" << summary.false_positives
                  << " FN=" << summary.false_negatives
                  << " Precision=" << summary.precision
                  << " Recall=" << summary.recall
                  << " F1=" << summary.f1
                  << " MatchIoU=" << summary.matched_iou << std::endl;
        for (const auto& [cls, stats] : summary.per_class) {
            std::cout << "  [Class " << cls << "]"
                      << " Pred=" << stats.predicted_boxes
                      << " Target=" << stats.target_boxes
                      << " TP=" << stats.true_positives
                      << " FP=" << stats.false_positives
                      << " FN=" << stats.false_negatives
                      << " Precision=" << stats.precision
                      << " Recall=" << stats.recall
                      << " F1=" << stats.f1
                      << std::endl;
        }
        return summary;
    }

    torch::Tensor forward(torch::Tensor x) {
        // KEY: plain inference path used by post_process() and two-stage smoke tests.
        auto backbone_outs = backbone_->forward(x);
        auto neck_outs = pan_->forward(backbone_outs);
        return head_->forward(neck_outs);
    }

    std::tuple<torch::Tensor, std::unordered_map<std::string, float>>
        train_step(torch::Tensor imgs, torch::Tensor targets) {
        // KEY: external training-step helper for callers that already prepared targets.
        auto backbone_outs = backbone_->forward(imgs);
        auto neck_outs = pan_->forward(backbone_outs);
        auto preds = head_->forward_train(neck_outs);
        return loss_fn_->forward(preds, targets);
    }

    const ModelConfig& get_config() const { return config_; }
    const YoloModelBuildConfig& get_build_config() const { return build_config_; }

    const Stem& get_stem() const { return backbone_->get_stem(); }
private:
    std::tuple<torch::Tensor, std::unordered_map<std::string, float>>
        train_step_internal(torch::Tensor imgs, torch::Tensor targets) {
        // KEY: shared internal path used by train() and val().
        auto backbone_outs = backbone_->forward(imgs);
        auto neck_outs = pan_->forward(backbone_outs);
        auto preds = head_->forward_train(neck_outs);
        return loss_fn_->forward(preds, targets);
    }

    ModelConfig config_;
    YoloModelBuildConfig build_config_;
    YOLOv8Backbone backbone_{ nullptr };
    PAN pan_{ nullptr };
    YOLOv8Detect head_{ nullptr };
    YOLOv8Loss loss_fn_{ nullptr };
};
TORCH_MODULE(YOLOv8);

#endif // TORCH_V8_H


