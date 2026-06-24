#ifndef TORCH_BACKBONE_H
#define TORCH_BACKBONE_H

// NOTE: formatting normalized during the libtorch_module cleanup pass.

#include <torch/torch.h>
#include <vector>
#include <string>
#include "torch_nnmodule.h"

// Comment tags used in this file:
// KEY: important execution path.
// MODIFIED: behavior adjusted during the current cleanup pass.
// CHECK: confirm during integration/debug.
// RISK: known limitation or possible issue.
// EVOLVE: recommended future improvement.
// VERIFY: needs explicit runtime or numerical validation.

// =========================================================================
// YOLOv8 Backbone (CSPDarknet)
// =========================================================================
// Structure:
// P1: Stem (Conv)
// P2: Conv(stride=2) -> C2f
// P3: Conv(stride=2) -> C2f
// P4: Conv(stride=2) -> C2f
// P5: Conv(stride=2) -> C2f -> SPPF
class YOLOv8BackboneImpl : public torch::nn::Module {
public:
    YOLOv8BackboneImpl(const std::vector<int64_t>& base_channels, float depth_multiple, float width_multiple) {

        // KEY: width scaling determines the per-stage channel budget.
        channels_ = scale_channels(base_channels, width_multiple);

        // KEY: depth scaling determines the number of residual blocks in each C2f stage.
        int d1 = scale_blocks(3, depth_multiple);
        int d2 = scale_blocks(6, depth_multiple);
        int d3 = scale_blocks(6, depth_multiple);
        int d4 = scale_blocks(3, depth_multiple);

        // --- Layer 0: Stem (P1) ---
        // Input: 3 -> Output: channels_[0] (e.g., 64)
        stem = safe_register_module(*this, "stem", Stem(3, channels_[0]));

        // --- Layer 1: P2 (x4 downsample) ---
        // Downsample: Conv(c0, c1, 3, 2)
        conv1 = safe_register_module(*this, "conv1", ConvModule(channels_[0], channels_[1], 3, 2, 1));
        // C2f
        c2f1 = safe_register_module(*this, "c2f1", C2f(channels_[1], channels_[1], d1, true));

        // --- Layer 2: P3 (x8 downsample) ---
        // Downsample: Conv(c1, c2, 3, 2)
        conv2 = safe_register_module(*this, "conv2", ConvModule(channels_[1], channels_[2], 3, 2, 1));
        // C2f
        c2f2 = safe_register_module(*this, "c2f2", C2f(channels_[2], channels_[2], d2, true));

        // --- Layer 3: P4 (x16 downsample) ---
        // Downsample: Conv(c2, c3, 3, 2)
        conv3 = safe_register_module(*this, "conv3", ConvModule(channels_[2], channels_[3], 3, 2, 1));
        // C2f
        c2f3 = safe_register_module(*this, "c2f3", C2f(channels_[3], channels_[3], d3, true));

        // --- Layer 4: P5 (x32 downsample) ---
        // Downsample: Conv(c3, c4, 3, 2)
        conv4 = safe_register_module(*this, "conv4", ConvModule(channels_[3], channels_[4], 3, 2, 1));
        // C2f
        c2f4 = safe_register_module(*this, "c2f4", C2f(channels_[4], channels_[4], d4, true));
        // SPPF
        sppf = safe_register_module(*this, "sppf", SPPF(channels_[4], channels_[4], 5));
    }

    // KEY: returns the three detection feature levels [P3, P4, P5].
    std::vector<torch::Tensor> forward(torch::Tensor x) {
        // P1
        x = stem->forward(x);

        // P2
        x = conv1->forward(x);
        x = c2f1->forward(x);

        // P3
        x = conv2->forward(x);
        auto p3 = c2f2->forward(x);

        // P4
        x = conv3->forward(x);
        auto p4 = c2f3->forward(x);

        // P5
        x = conv4->forward(x);
        x = c2f4->forward(x);
        auto p5 = sppf->forward(x);

        return { p3, p4, p5 };
    }

    // KEY: PAN/head use these channels to size downstream fusion and prediction layers.
    std::vector<int64_t> get_out_channels() const {
        return { channels_[2], channels_[3], channels_[4] };
    }

    // CHECK: exposed for weight mapping or debugging; callers should not mutate the stem layout.
    const Stem& get_stem() const { return stem; }
private:
    std::vector<int> channels_;

    Stem stem{ nullptr };

    ConvModule conv1{ nullptr };
    C2f c2f1{ nullptr };

    ConvModule conv2{ nullptr };
    C2f c2f2{ nullptr };

    ConvModule conv3{ nullptr };
    C2f c2f3{ nullptr };

    ConvModule conv4{ nullptr };
    C2f c2f4{ nullptr };

    SPPF sppf{ nullptr };
};

TORCH_MODULE(YOLOv8Backbone);

#endif // TORCH_BACKBONE_H
