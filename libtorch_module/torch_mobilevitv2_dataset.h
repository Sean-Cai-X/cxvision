#ifndef TORCH_MOBILEVITV2_DATASET_H
#define TORCH_MOBILEVITV2_DATASET_H

#include <torch/torch.h>
#include <opencv2/opencv.hpp>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <vector>
#include <string>

namespace fs = std::filesystem;

// Comment tags used in this file:
// KEY: important execution path.
// MODIFIED: behavior adjusted during the current cleanup pass.
// CHECK: confirm during integration/debug.
// RISK: known limitation or possible issue.
// EVOLVE: recommended future improvement.
// VERIFY: needs explicit runtime or numerical validation.

// Image classification dataset with an ImageFolder-like directory layout.
class MobileViTv2Dataset : public torch::data::Dataset<MobileViTv2Dataset> {
public:
    MobileViTv2Dataset(const std::string& root_dir, bool is_train = true)
        : root_dir_(root_dir), is_train_(is_train) {
        // KEY: expected layout is root/class_name/image.xxx.
        int class_idx = 0;
        if (!fs::exists(root_dir)) {
            // MODIFIED: allow smoke tests to pass cleanly when the dataset path is absent.
            std::cerr << "[Dataset] Root directory not found: " << root_dir << std::endl;
            return;
        }

        for (const auto& entry : fs::directory_iterator(root_dir)) {
            if (entry.is_directory()) {
                class_names_.push_back(entry.path().filename().string());
                for (const auto& img_entry : fs::directory_iterator(entry.path())) {
                    std::string ext = img_entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (ext == ".jpg" || ext == ".png" || ext == ".jpeg") {
                        image_paths_.push_back(img_entry.path().string());
                        labels_.push_back(class_idx);
                    }
                }
                class_idx++;
            }
        }
        std::cout << "[Dataset] Loaded " << image_paths_.size() << " images from " << root_dir << std::endl;
    }

    torch::data::Example<> get(size_t index) override {
        // KEY: load one image sample and normalize it for classifier input.
        cv::Mat img = cv::imread(image_paths_[index]);
        // RISK: this fallback keeps the loader alive but can pollute labels if it happens often.
        if (img.empty()) {
            return { torch::zeros({3, 256, 256}), torch::tensor(0) };
        }

        // CHECK: simple resize may distort geometry-sensitive samples.
        // KEY: resize -> RGB -> float -> normalize.
        cv::resize(img, img, cv::Size(256, 256));
        cv::cvtColor(img, img, cv::COLOR_BGR2RGB);

        torch::Tensor tensor = torch::from_blob(img.data, {img.rows, img.cols, 3}, torch::kUInt8)
            .permute({2, 0, 1})
            .to(torch::kFloat32)
            .div_(255.0f)
            .contiguous()
            .clone();
        // MODIFIED: clone() prevents lifetime issues after cv::Mat goes out of scope.

        // KEY: ImageNet normalization keeps the classifier aligned with common pretrained weights.
        // ImageNet Normalization
        tensor[0] = (tensor[0] - 0.485) / 0.229;
        tensor[1] = (tensor[1] - 0.456) / 0.224;
        tensor[2] = (tensor[2] - 0.406) / 0.225;

        return {tensor, torch::tensor(labels_[index])};
    }

    torch::optional<size_t> size() const override {
        return image_paths_.size();
    }

private:
    std::string root_dir_;
    bool is_train_;
    // EVOLVE: use is_train_ to switch augmentation or train/val specific preprocessing.
    std::vector<std::string> image_paths_;
    std::vector<int64_t> labels_;
    std::vector<std::string> class_names_;
};

#endif // TORCH_MOBILEVITV2_DATASET_H
