#ifndef TORCH_YOLOV8_LOSS_H
#define TORCH_YOLOV8_LOSS_H

// NOTE: formatting normalized during the libtorch_module cleanup pass.

#include <torch/torch.h>
#include <vector>
#include <tuple>
#include <cmath>
#include <algorithm>
#include <unordered_map>

#include "torch_taskalignedassigner.h"
#include "torch_util.h"
#include "torch_detect.h"

namespace F = torch::nn::functional;

// Comment tags used in this file:
// KEY: important execution path.
// MODIFIED: behavior adjusted during the current cleanup pass.
// CHECK: confirm during integration/debug.
// RISK: known limitation or possible issue.
// EVOLVE: recommended future improvement.
// VERIFY: needs explicit runtime or numerical validation.

enum class YoloBoxDecodeStrategy {
    DirectStrideScaled
};

struct YoloLossConfig {
    float box_weight = 7.5f;
    float cls_weight = 0.5f;
    float dfl_weight = 1.5f;
    float fl_gamma = 0.0f;
    int64_t topk = 10;
    int64_t reg_max = 16;
    YoloBoxDecodeStrategy decode_strategy = YoloBoxDecodeStrategy::DirectStrideScaled;
    bool enable_dfl = false;

    void validate(int64_t num_classes) const {
        TORCH_CHECK(num_classes > 0, "num_classes must be positive");
        TORCH_CHECK(box_weight >= 0.0f, "box_weight must be non-negative");
        TORCH_CHECK(cls_weight >= 0.0f, "cls_weight must be non-negative");
        TORCH_CHECK(dfl_weight >= 0.0f, "dfl_weight must be non-negative");
        TORCH_CHECK(fl_gamma >= 0.0f, "fl_gamma must be non-negative");
        TORCH_CHECK(topk > 0, "topk must be positive");
        TORCH_CHECK(reg_max > 0, "reg_max must be positive");
    }

    int64_t box_channels() const {
        return enable_dfl ? reg_max * 4 : 4;
    }
};

class YOLOv8LossImpl : public torch::nn::Module {
public:
    YOLOv8LossImpl(int64_t num_classes,
        float box_weight = 7.5f,
        float cls_weight = 0.5f,
        float dfl_weight = 1.5f,
        float fl_gamma = 0.0f,
        int64_t topk = 10)
        : num_classes_(num_classes),
        config_({ box_weight, cls_weight, dfl_weight, fl_gamma, topk, 16 }) {
        initialize_modules();
    }

    YOLOv8LossImpl(int64_t num_classes, const YoloLossConfig& config)
        : num_classes_(num_classes), config_(config) {
        initialize_modules();
    }

private:
    void initialize_modules() {
        config_.validate(num_classes_);

        assigner = register_module("assigner", TaskAlignedAssigner(config_.topk, num_classes_, 1.0f, 6.0f));

        // KEY: fixed detection strides for the current three-scale YOLO head.
        strides_ = register_buffer("strides", torch::tensor({ 8, 16, 32 }, torch::kFloat32));
    }

public:

    std::tuple<torch::Tensor, std::unordered_map<std::string, float>> forward(
        const std::vector<torch::Tensor>& preds, // [P3, P4, P5]
        const torch::Tensor& targets_in) {       // [batch, max_gt, 6] (img_idx, cls, x, y, w, h)

        // KEY: move targets onto the same device as the registered stride buffer.
        torch::Device device = strides_.device();
        torch::Tensor targets = targets_in.to(device);

        torch::Tensor loss_box = torch::zeros({ 1 }, device);
        torch::Tensor loss_cls = torch::zeros({ 1 }, device);
        torch::Tensor loss_dfl = torch::zeros({ 1 }, device);
        torch::Tensor total_num_pos = torch::zeros({ 1 }, device);

        int64_t batch_size = targets.size(0);

        // KEY: unpack padded targets into labels, boxes, and validity mask.
        torch::Tensor gt_labels = targets.select(2, 1).to(torch::kLong); // [B, N]
        torch::Tensor gt_bboxes = targets.slice(2, 2, 6);                // [B, N, 4]
        torch::Tensor mask_gt = (targets.select(2, 1) >= 0).to(torch::kFloat32); // [B, N]

        for (size_t i = 0; i < preds.size(); ++i) {
            torch::Tensor pred = preds[i]; // [B, Anchors, 4 + Cls]
            int64_t stride = strides_[i].item<int64_t>();
            int64_t box_channels = config_.box_channels();

            torch::Tensor pred_boxes = pred.slice(2, 0, box_channels);
            torch::Tensor pred_cls = pred.slice(2, box_channels, box_channels + num_classes_);

            torch::Tensor decoded_boxes = decode_boxes(pred_boxes, stride);

            // KEY: decode boxes and match them with GT using TaskAlignedAssigner.
            torch::Tensor assigned_gt_inds = assigner->forward(
                torch::sigmoid(pred_cls).unsqueeze(1), // [B, 1, A, C]
                decoded_boxes.unsqueeze(1),            // [B, 1, A, 4]
                gt_labels.unsqueeze(-1),               // [B, G, 1]
                gt_bboxes,                             // [B, G, 4]
                mask_gt.unsqueeze(-1)                  // [B, G, 1]
            );

            torch::Tensor pos_mask = assigned_gt_inds > 0;
            torch::Tensor num_pos_scale = pos_mask.sum();
            total_num_pos += num_pos_scale;

            if (num_pos_scale.item<float>() > 0) {
                // KEY: CIoU-style box regression loss on positive assignments only.
                torch::Tensor pos_decoded_boxes = decoded_boxes.masked_select(pos_mask.unsqueeze(-1)).view({ -1, 4 });

                auto batch_idx = torch::arange(batch_size, device).view({ -1, 1 }).expand_as(assigned_gt_inds);

                auto valid_batch_idx = batch_idx.masked_select(pos_mask);     // [N_pos]
                auto valid_gt_idx = assigned_gt_inds.masked_select(pos_mask).to(torch::kLong) - 1; // [N_pos] (0-based)

                auto gt_bboxes_flat = gt_bboxes.view({ -1, 4 });
                auto flat_indices = valid_batch_idx * gt_bboxes.size(1) + valid_gt_idx;

                torch::Tensor pos_gt_boxes = gt_bboxes_flat.index_select(0, flat_indices);

                torch::Tensor iou = bbox_iou(pos_decoded_boxes, pos_gt_boxes, true);
                loss_box = loss_box + (1.0f - iou).sum();

                if (config_.enable_dfl) {
                    torch::Tensor pos_pred_boxes = pred_boxes.masked_select(pos_mask.unsqueeze(-1)).view({-1, config_.box_channels()});
                    loss_dfl = loss_dfl + compute_dfl_loss(pos_pred_boxes, pos_gt_boxes, stride).sum();
                }
            }

            // KEY: classification loss is BCE-with-logits over one-hot assigned labels.
            torch::Tensor scale_cls_loss = compute_cls_loss_vectorized(
                pred_cls, gt_labels, assigned_gt_inds, pos_mask);

            loss_cls = loss_cls + scale_cls_loss.sum();
        }

        // MODIFIED: guard against divide-by-zero when a batch has no positives.
        torch::Tensor num_pos_safe = torch::max(total_num_pos, torch::tensor(1.0f, device));

        loss_box = (loss_box / num_pos_safe) * config_.box_weight;
        loss_cls = (loss_cls / num_pos_safe) * config_.cls_weight;
        loss_dfl = (loss_dfl / num_pos_safe) * config_.dfl_weight;

        torch::Tensor total_loss = loss_box + loss_cls + loss_dfl;

        std::unordered_map<std::string, float> loss_components = {
            {"box_loss", loss_box.item<float>()},
            {"cls_loss", loss_cls.item<float>()},
            {"dfl_loss", loss_dfl.item<float>()},
            {"total_loss", total_loss.item<float>()}
        };

        return std::make_tuple(total_loss, loss_components);
    }

private:
    torch::Tensor decode_boxes(const torch::Tensor& pred_boxes, int64_t stride) {
        switch (config_.decode_strategy) {
        case YoloBoxDecodeStrategy::DirectStrideScaled:
        default:
            if (!config_.enable_dfl) {
                // RISK: current non-DFL decode path is simplified and assumes direct xywh * stride decoding.
                return pred_boxes * static_cast<float>(stride);
            }
            return decode_dfl_boxes(pred_boxes, stride);
        }
    }

    torch::Tensor decode_dfl_boxes(const torch::Tensor& pred_boxes, int64_t stride) {
        TORCH_CHECK(config_.enable_dfl, "decode_dfl_boxes requires enable_dfl=true");
        TORCH_CHECK(pred_boxes.size(-1) == config_.box_channels(),
            "DFL box channel mismatch, expected ", config_.box_channels(), " got ", pred_boxes.size(-1));

        auto shape = pred_boxes.sizes();
        auto pred = pred_boxes.view({shape[0], shape[1], 4, config_.reg_max});
        auto probs = torch::softmax(pred, -1);
        auto bins = torch::arange(config_.reg_max, probs.options());
        auto distances = (probs * bins).sum(-1);
        return distances * static_cast<float>(stride);
    }

    torch::Tensor compute_dfl_loss(
        const torch::Tensor& pred_boxes,
        const torch::Tensor& gt_boxes,
        int64_t stride) {
        TORCH_CHECK(config_.enable_dfl, "compute_dfl_loss requires enable_dfl=true");
        TORCH_CHECK(pred_boxes.size(-1) == config_.box_channels(),
            "DFL box channel mismatch, expected ", config_.box_channels(), " got ", pred_boxes.size(-1));
        TORCH_CHECK(gt_boxes.size(-1) == 4, "DFL gt boxes must have 4 channels");

        auto logits = pred_boxes.view({pred_boxes.size(0), 4, config_.reg_max});

        // RISK: the current training path still uses simplified xywh targets.
        // This maps gt box coordinates into per-bin regression targets so the
        // DFL branch is trainable before a fuller anchor-free target redesign.
        auto target = (gt_boxes / static_cast<float>(stride)).clamp(0.0f, static_cast<float>(config_.reg_max - 1) - 1e-3f);
        auto target_left = torch::floor(target).to(torch::kLong);
        auto target_right = torch::clamp(target_left + 1, 0, config_.reg_max - 1);
        auto weight_right = (target - target_left.to(target.dtype())).clamp(0.0f, 1.0f);
        auto weight_left = 1.0f - weight_right;

        auto flat_logits = logits.view({-1, config_.reg_max});
        auto flat_left = target_left.view({-1});
        auto flat_right = target_right.view({-1});
        auto flat_w_left = weight_left.view({-1});
        auto flat_w_right = weight_right.view({-1});

        auto ce_left = F::cross_entropy(
            flat_logits,
            flat_left,
            F::CrossEntropyFuncOptions().reduction(torch::kNone));
        auto ce_right = F::cross_entropy(
            flat_logits,
            flat_right,
            F::CrossEntropyFuncOptions().reduction(torch::kNone));

        auto loss = ce_left * flat_w_left + ce_right * flat_w_right;
        return loss.view({pred_boxes.size(0), 4}).sum(1);
    }

    torch::Tensor compute_cls_loss_vectorized(
        const torch::Tensor& pred_cls,         // [B, A, NumClasses]
        const torch::Tensor& gt_labels,        // [B, MaxGT]
        const torch::Tensor& assigned_gt_inds, // [B, A] (1-based)
        const torch::Tensor& pos_mask)         // [B, A]
    {
        // KEY: build dense one-hot class targets for BCE loss.
        torch::Tensor target_one_hot = torch::zeros_like(pred_cls);

        if (pos_mask.any().item<bool>()) {
            // CHECK: assigned_gt_inds is 1-based; convert to 0-based before gather().
            auto gather_inds = (assigned_gt_inds - 1).clamp_min(0).to(torch::kLong); // [B, A]

            auto target_cls_ids = gt_labels.gather(1, gather_inds);

            auto one_hot = F::one_hot(target_cls_ids, num_classes_).to(pred_cls.dtype());

            // pos_mask: [B, A] -> [B, A, 1]
            target_one_hot = one_hot * pos_mask.unsqueeze(-1);
        }

        auto bce_options = F::BinaryCrossEntropyWithLogitsFuncOptions().reduction(torch::kNone);
        torch::Tensor loss = F::binary_cross_entropy_with_logits(pred_cls, target_one_hot, bce_options);

        if (config_.fl_gamma > 0.0f) {
            // EVOLVE: focal modulation is optional and currently disabled by default.
            torch::Tensor pt = torch::exp(-loss);
            loss = (1.0f - pt).pow(config_.fl_gamma) * loss;
        }

        return loss;
    }

    torch::Tensor bbox_iou(const torch::Tensor& boxes1, const torch::Tensor& boxes2, bool ciou = true) {
        // KEY: IoU/CIoU computation expects [N, 4] boxes in xywh format.
        auto b1_x = boxes1.select(1, 0);
        auto b1_y = boxes1.select(1, 1);
        auto b1_w = boxes1.select(1, 2);
        auto b1_h = boxes1.select(1, 3);

        auto b1_x1 = b1_x - b1_w / 2.0f;
        auto b1_y1 = b1_y - b1_h / 2.0f;
        auto b1_x2 = b1_x + b1_w / 2.0f;
        auto b1_y2 = b1_y + b1_h / 2.0f;

        auto b2_x = boxes2.select(1, 0);
        auto b2_y = boxes2.select(1, 1);
        auto b2_w = boxes2.select(1, 2);
        auto b2_h = boxes2.select(1, 3);

        auto b2_x1 = b2_x - b2_w / 2.0f;
        auto b2_y1 = b2_y - b2_h / 2.0f;
        auto b2_x2 = b2_x + b2_w / 2.0f;
        auto b2_y2 = b2_y + b2_h / 2.0f;

        // KEY: pairwise intersection/union area.
        auto inter_x1 = torch::max(b1_x1, b2_x1);
        auto inter_y1 = torch::max(b1_y1, b2_y1);
        auto inter_x2 = torch::min(b1_x2, b2_x2);
        auto inter_y2 = torch::min(b1_y2, b2_y2);

        auto inter_area = (inter_x2 - inter_x1).clamp_min(0) * (inter_y2 - inter_y1).clamp_min(0);
        auto union_area = (b1_w * b1_h) + (b2_w * b2_h) - inter_area;

        auto iou = inter_area / (union_area + 1e-7f);

        if (ciou) {
            // KEY: CIoU adds enclosing-box, center-distance, and aspect-ratio penalties.
            auto c_x1 = torch::min(b1_x1, b2_x1);
            auto c_y1 = torch::min(b1_y1, b2_y1);
            auto c_x2 = torch::max(b1_x2, b2_x2);
            auto c_y2 = torch::max(b1_y2, b2_y2);

            auto c_diag_sq = (c_x2 - c_x1).pow(2) + (c_y2 - c_y1).pow(2) + 1e-7f;

            // Center distance squared
            auto rho_sq = (b2_x - b1_x).pow(2) + (b2_y - b1_y).pow(2);

            // Aspect ratio penalty
            auto w2_h2 = b2_w / (b2_h + 1e-7f);
            auto w1_h1 = b1_w / (b1_h + 1e-7f);
            auto v = (4.0f / (M_PI * M_PI)) * torch::pow(torch::atan(w2_h2) - torch::atan(w1_h1), 2);

            torch::Tensor alpha;
            {
                torch::NoGradGuard no_grad;
                alpha = v / (1.0f - iou + v + 1e-7f);
            }

            return iou - (rho_sq / c_diag_sq + v * alpha);
        }
        return iou;
    }

    int64_t num_classes_;
    YoloLossConfig config_;

    TaskAlignedAssigner assigner{ nullptr };
    torch::Tensor strides_;
};

TORCH_MODULE(YOLOv8Loss);

#endif
