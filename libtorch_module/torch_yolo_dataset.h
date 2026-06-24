#ifndef TORCH_YOLO_DATASET_H
#define TORCH_YOLO_DATASET_H

// NOTE: formatting normalized during the libtorch_module cleanup pass.

#include <torch/torch.h>
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include "torch_data_augmenter.h"

namespace fs = std::filesystem;

// Comment tags used in this file:
// KEY: important execution path.
// MODIFIED: behavior adjusted during the current cleanup pass.
// CHECK: confirm during integration/debug.
// RISK: known limitation or possible issue.
// EVOLVE: recommended future improvement.
// VERIFY: needs explicit runtime or numerical validation.

struct YoloDatasetConfig {
    int img_size = 640;
    bool is_train = true;
    int max_gt = 50;
    bool enable_hsv = true;
    bool enable_flip = true;
    YoloResizePolicy resize_policy = YoloResizePolicy::PlainResize;
    int letterbox_pad_value = 114;

    void validate() const {
        TORCH_CHECK(img_size > 0, "img_size must be positive");
        TORCH_CHECK(max_gt > 0, "max_gt must be positive");
        TORCH_CHECK(letterbox_pad_value >= 0 && letterbox_pad_value <= 255,
            "letterbox_pad_value must be in [0, 255]");
    }
};

struct YoloDatasetPaths {
    std::string image_dir;
    std::string label_dir;

    void validate() const {
        TORCH_CHECK(!image_dir.empty(), "image_dir must not be empty");
        TORCH_CHECK(!label_dir.empty(), "label_dir must not be empty");
    }
};

inline YoloDatasetPaths make_yolo_split_paths(const std::string& data_root, const std::string& split) {
    TORCH_CHECK(!data_root.empty(), "data_root must not be empty");
    TORCH_CHECK(!split.empty(), "split must not be empty");

    YoloDatasetPaths paths;
    paths.image_dir = data_root + "/images/" + split;
    paths.label_dir = data_root + "/labels/" + split;
    return paths;
}

inline torch::data::DataLoaderOptions make_yolo_loader_options(int batch_size, int workers) {
    TORCH_CHECK(batch_size > 0, "batch_size must be positive");
    TORCH_CHECK(workers >= 0, "workers must be non-negative");
    return torch::data::DataLoaderOptions().batch_size(batch_size).workers(workers);
}

inline YoloDatasetConfig make_yolo_dataset_config(const TrainConfig& train_config, bool is_train_override) {
    YoloDatasetConfig config;
    config.img_size = train_config.img_size;
    config.is_train = is_train_override;
    config.max_gt = train_config.max_gt;
    config.enable_hsv = is_train_override ? train_config.enable_hsv : false;
    config.enable_flip = is_train_override ? train_config.enable_flip : false;
    config.resize_policy = train_config.resize_policy;
    config.letterbox_pad_value = train_config.letterbox_pad_value;
    return config;
}

inline YoloDatasetConfig make_yolo_eval_config(
    int img_size,
    YoloResizePolicy resize_policy = YoloResizePolicy::PlainResize,
    int max_gt = 50,
    int letterbox_pad_value = 114) {

    YoloDatasetConfig config;
    config.img_size = img_size;
    config.is_train = false;
    config.max_gt = max_gt;
    config.enable_hsv = false;
    config.enable_flip = false;
    config.resize_policy = resize_policy;
    config.letterbox_pad_value = letterbox_pad_value;
    return config;
}

inline YoloDatasetConfig make_yolo_eval_config(const YoloValidationConfig& val_config) {
    val_config.validate();
    return make_yolo_eval_config(
        val_config.img_size,
        val_config.resize_policy,
        val_config.max_gt,
        val_config.letterbox_pad_value);
}

class YoloDataset : public torch::data::Dataset<YoloDataset> {
public:
    YoloDataset(const YoloDatasetPaths& paths, YoloDatasetConfig config)
        : YoloDataset(paths.image_dir, paths.label_dir, std::move(config)) {
        paths.validate();
    }

    YoloDataset(const std::string& img_dir, const std::string& label_dir,
        int img_size = 640, bool is_train = true)
        : YoloDataset(img_dir, label_dir, YoloDatasetConfig{img_size, is_train}) {
    }

    YoloDataset(const std::string& img_dir, const std::string& label_dir,
        YoloDatasetConfig config)
        : img_dir_(img_dir), label_dir_(label_dir), config_(config) {

        config_.validate();

        augmenter_ = DataAugmenter();
        label_loader_ = LabelLoader();

        // KEY: collect supported image files from the image directory.
        if (fs::exists(img_dir_)) {
            for (const auto& entry : fs::directory_iterator(img_dir_)) {
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".jpg" || ext == ".png" || ext == ".jpeg" || ext == ".bmp") {
                    img_paths_.push_back(entry.path().string());
                }
            }
        }
        else {
            // CHECK: missing dataset paths are surfaced early but do not throw.
            std::cerr << "Error: Image directory not found: " << img_dir_ << std::endl;
        }
    }

    // KEY: dataset returns image tensor plus padded targets [max_gt, 6].
    torch::data::Example<> get(size_t index) override {
        std::string img_path = img_paths_[index];
        std::string label_filename = fs::path(img_path).stem().string() + ".txt";
        std::string label_path = (fs::path(label_dir_) / label_filename).string();

        cv::Mat img = cv::imread(img_path);
        if (img.empty()) {
            // MODIFIED: return a padded invalid target tensor instead of crashing the loader.
            std::cerr << "Error: Image empty :  " << img_dir_ << std::endl;
            auto empty_img = torch::zeros({3, config_.img_size, config_.img_size}, torch::kFloat32);
            auto empty_target = torch::full({config_.max_gt, 6}, -1.0f, torch::kFloat32);
            return { empty_img, empty_target };
        }

        // KEY: labels are read as normalized [cls, x1, y1, x2, y2] rows.
        std::vector<Annotation> anns = label_loader_.load_labels(label_path);

        std::vector<std::vector<float>> labels;
        for (const auto& ann : anns) {
            labels.push_back({ (float)ann.class_id, ann.x1, ann.y1, ann.x2, ann.y2 });
        }

        if (config_.is_train) {
            // CHECK: current training augmentation is HSV + random flip only.
            if (config_.enable_hsv) {
                img = augmenter_.hsv_augment(img);
            }
            if (config_.enable_flip) {
                auto [flipped_img, flipped_labels] = augmenter_.random_flip(img, labels);
                img = flipped_img;
                labels = flipped_labels;
            }
        }

        if (config_.resize_policy == YoloResizePolicy::Letterbox) {
            std::tie(img, labels) = letterbox_image_and_labels(img, labels);
        } else {
            // RISK: plain resize can distort geometry-sensitive datasets.
            img = augmenter_.resize_image(img, config_.img_size);
        }

        // KEY: OpenCV (BGR, HWC) -> Torch (RGB, CHW, float, 0-1).
        cv::Mat img_rgb;
        cv::cvtColor(img, img_rgb, cv::COLOR_BGR2RGB);
        torch::Tensor img_tensor = torch::from_blob(img_rgb.data, { img_rgb.rows, img_rgb.cols, 3 }, torch::kUInt8)
            .permute({ 2, 0, 1 })
            .contiguous()
            .clone()
            .to(torch::kFloat32)
            .div_(255.0f);

        // KEY: pad GT targets to a fixed max_gt count for stack-based dataloading.
        torch::Tensor target_tensor = torch::full({ config_.max_gt, 6 }, -1.0f, torch::kFloat32); // [batch_idx, cls, x1, y1, x2, y2]

        int count = 0;
        for (const auto& l : labels) {
            if (count >= config_.max_gt) break;
            target_tensor[count][0] = 0.0f; // Batch Index Placeholder
            target_tensor[count][1] = l[0]; // Class
            target_tensor[count][2] = l[1]; // x1
            target_tensor[count][3] = l[2]; // y1
            target_tensor[count][4] = l[3]; // x2
            target_tensor[count][5] = l[4]; // y2
            count++;
        }

        return { img_tensor, target_tensor };
    }

    torch::optional<size_t> size() const override {
        return img_paths_.size();
    }

private:
    std::tuple<cv::Mat, std::vector<std::vector<float>>> letterbox_image_and_labels(
        const cv::Mat& img,
        const std::vector<std::vector<float>>& labels) const {

        int original_w = img.cols;
        int original_h = img.rows;
        int target = config_.img_size;

        float scale = std::min(
            static_cast<float>(target) / std::max(1, original_w),
            static_cast<float>(target) / std::max(1, original_h));

        int resized_w = std::max(1, static_cast<int>(std::round(original_w * scale)));
        int resized_h = std::max(1, static_cast<int>(std::round(original_h * scale)));

        cv::Mat resized;
        cv::resize(img, resized, cv::Size(resized_w, resized_h));

        cv::Mat canvas(
            target,
            target,
            img.type(),
            cv::Scalar(config_.letterbox_pad_value, config_.letterbox_pad_value, config_.letterbox_pad_value));

        int pad_x = (target - resized_w) / 2;
        int pad_y = (target - resized_h) / 2;
        resized.copyTo(canvas(cv::Rect(pad_x, pad_y, resized_w, resized_h)));

        std::vector<std::vector<float>> adjusted_labels = labels;
        for (auto& label : adjusted_labels) {
            float x1 = label[1] * original_w;
            float y1 = label[2] * original_h;
            float x2 = label[3] * original_w;
            float y2 = label[4] * original_h;

            x1 = (x1 * scale + pad_x) / target;
            y1 = (y1 * scale + pad_y) / target;
            x2 = (x2 * scale + pad_x) / target;
            y2 = (y2 * scale + pad_y) / target;

            label[1] = std::clamp(x1, 0.0f, 1.0f);
            label[2] = std::clamp(y1, 0.0f, 1.0f);
            label[3] = std::clamp(x2, 0.0f, 1.0f);
            label[4] = std::clamp(y2, 0.0f, 1.0f);
        }

        return { canvas, adjusted_labels };
    }

    std::string img_dir_;
    std::string label_dir_;
    YoloDatasetConfig config_;
    // EVOLVE: expose augmentation policy and max_gt as configurable constructor inputs.
    DataAugmenter augmenter_;
    LabelLoader label_loader_;
    std::vector<std::string> img_paths_;
};

#endif // TORCH_YOLO_DATASET_H
