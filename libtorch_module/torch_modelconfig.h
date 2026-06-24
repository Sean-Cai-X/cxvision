#ifndef TORCH_MODELCONFIG_H
#define TORCH_MODELCONFIG_H

// NOTE: formatting normalized during the libtorch_module cleanup pass.

#include <torch/torch.h>
#include <string>
#include <unordered_map>
#include <stdexcept>
#include <vector>
#include <iostream>

struct ModelConfig {
    float depth_multiple;
    float width_multiple;
    float ratio;

    int64_t num_classes;
    int64_t max_channels;
    std::vector<int64_t> strides;

    float obj_threshold;
    float nms_threshold;

    ModelConfig()
        : depth_multiple(0.33f), width_multiple(0.50f), ratio(2.0f),
        num_classes(80), max_channels(1024), strides({ 8, 16, 32 }),
        obj_threshold(0.25f), nms_threshold(0.45f) {
    }

    static ModelConfig get_config(const std::string& model_type, int64_t custom_num_classes = 80) {
        // n: d=0.33, w=0.25, ratio=2.0
        // s: d=0.33, w=0.50, ratio=2.0
        // m: d=0.67, w=0.75, ratio=1.5
        // l: d=1.00, w=1.00, ratio=1.0
        // x: d=1.00, w=1.25, ratio=1.0

        std::unordered_map<std::string, std::tuple<float, float, float>> params = {
            {"nano",   {0.33f, 0.25f, 2.0f}},
            {"small",  {0.33f, 0.50f, 2.0f}},
            {"medium", {0.67f, 0.75f, 1.5f}},
            {"large",  {1.00f, 1.00f, 1.0f}},
            {"xlarge", {1.00f, 1.25f, 1.0f}}
        };

        if (params.find(model_type) == params.end()) {
            TORCH_CHECK(false, "Unsupported model type: ", model_type,
                " (supported: nano/small/medium/large/xlarge)");
        }

        auto [d, w, r] = params[model_type];

        ModelConfig cfg;
        cfg.depth_multiple = d;
        cfg.width_multiple = w;
        cfg.ratio = r;
        cfg.num_classes = custom_num_classes;

        cfg.validate();

        return cfg;
    }

    void validate() const {
        TORCH_CHECK(width_multiple > 0, "Invalid width_multiple");
        TORCH_CHECK(depth_multiple > 0, "Invalid depth_multiple");
        TORCH_CHECK(num_classes > 0, "Invalid num_classes");
        TORCH_CHECK(!strides.empty(), "Strides cannot be empty");
    }
};

#endif // TORCH_MODELCONFIG_H