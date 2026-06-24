#ifndef TORCH_PAN_H
#define TORCH_PAN_H

// NOTE: formatting normalized during the libtorch_module cleanup pass.

#include <torch/torch.h>
#include <vector>
#include <string>
#include <iostream>
#include <tuple>

#include "torch_nnmodule.h"

// Comment tags used in this file:
// KEY: important execution path.
// MODIFIED: behavior adjusted during the current cleanup pass.
// CHECK: confirm during integration/debug.
// RISK: known limitation or possible issue.
// EVOLVE: recommended future improvement.
// VERIFY: needs explicit runtime or numerical validation.

// PAN (Path Aggregation Network) for YOLOv8
// Structure:
// Input: [P3, P4, P5] from Backbone
// 1. UpSample P5 -> Concat P4 -> C2f (N4)
// 2. UpSample N4 -> Concat P3 -> C2f (N3) -> Output P3_out
// 3. DownSample N3 -> Concat N4 -> C2f (N4_out) -> Output P4_out
// 4. DownSample N4_out -> Concat P5 -> C2f (N5_out) -> Output P5_out
class PANImpl : public torch::nn::Module {
public:
    PANImpl(const std::vector<int64_t>& in_channels, float depth_multiple = 1.0f, float width_multiple = 1.0f) {
        TORCH_CHECK(in_channels.size() == 3, "PAN expects 3 input channels (P3, P4, P5)");

        int64_t c3 = in_channels[0];
        int64_t c4 = in_channels[1];
        int64_t c5 = in_channels[2];

        int d = scale_blocks(3, depth_multiple);

        // --- Top-down Pathway ---

        // 1. P5 -> Upsample -> Concat P4

        up = register_module("up", torch::nn::Upsample(
            torch::nn::UpsampleOptions().scale_factor(std::vector<double>{2.0, 2.0}).mode(torch::kNearest)));

        // Layer 1: P5 -> N4 (Top-down)
        // - [-1, 1, nn.Upsample, [None, 2, 'nearest']]
        // - [[-1, 6], 1, Concat, [1]]  (6 is P4)
        // - [-1, 3, C2f, [512]] (Target channel)
        // P5 (1024) + P4 (512) = 1536 -> C2f -> 512

        c2f_p4 = safe_register_module(*this, "c2f_p4", C2f(c5 + c4, c4, d, false));

        // Layer 2: N4 -> N3 (Top-down)
        // N4 (c4) -> Upsample -> Concat P3 (c3) -> C2f -> c3
        c2f_p3 = safe_register_module(*this, "c2f_p3", C2f(c4 + c3, c3, d, false));

        // --- Bottom-up Pathway ---

        // Layer 3: N3 -> N4 (Bottom-up)
        // N3 (c3) -> Conv(stride=2) -> Concat N4 -> C2f -> c4
        down_p3 = safe_register_module(*this, "down_p3", ConvModule(c3, c3, 3, 2));
        // Concat: down_p3 (c3) + N4 (c4) = c3+c4 -> C2f -> c4
        c2f_n4 = safe_register_module(*this, "c2f_n4", C2f(c3 + c4, c4, d, false));

        // Layer 4: N4 -> N5 (Bottom-up)
        // N4 (c4) -> Conv(stride=2) -> Concat P5 -> C2f -> c5
        down_p4 = safe_register_module(*this, "down_p4", ConvModule(c4, c4, 3, 2));
        // Concat: down_p4 (c4) + P5 (c5) = c4+c5 -> C2f -> c5
        c2f_n5 = safe_register_module(*this, "c2f_n5", C2f(c4 + c5, c5, d, false));
    }

    // KEY: fuses top-down and bottom-up paths, returning [P3_out, P4_out, P5_out].
    std::vector<torch::Tensor> forward(const std::vector<torch::Tensor>& x) {
        if (x.size() != 3) {
            throw std::runtime_error("PAN forward expects 3 inputs (P3, P4, P5)");
        }

        auto p3 = x[0]; // 80x80
        auto p4 = x[1]; // 40x40
        auto p5 = x[2]; // 20x20

        // --- Top-down ---

        // P5 -> Up -> Concat P4
        auto p5_up = up->forward(p5); // 40x40
        // CHECK: shape alignment should already hold for standard YOLOv8 strides.
        if (p5_up.size(2) != p4.size(2) || p5_up.size(3) != p4.size(3)) {
        }

        auto p4_cat = torch::cat({ p5_up, p4 }, 1);
        auto p4_td = c2f_p4->forward(p4_cat); // N4 (40x40)

        // N4 -> Up -> Concat P3
        auto p4_up = up->forward(p4_td); // 80x80
        auto p3_cat = torch::cat({ p4_up, p3 }, 1);
        auto p3_out = c2f_p3->forward(p3_cat); // N3 (80x80) -> Output P3

        // --- Bottom-up ---

        // KEY: bottom-up path re-aggregates low-level detail into higher-resolution outputs.

        // N3 -> Down -> Concat N4
        auto p3_down = down_p3->forward(p3_out); // 40x40

        // RISK: these prints are useful during bring-up but noisy in normal training/inference.
        std::cout << "DEBUG PAN: p3_down shape: " << p3_down.sizes() << std::endl;
        std::cout << "DEBUG PAN: p4_td shape: " << p4_td.sizes() << std::endl;

        // CHECK: if these trigger, backbone/PAN stride assumptions are out of sync.
        if (p3_down.size(2) != p4_td.size(2) || p3_down.size(3) != p4_td.size(3)) {
            std::cerr << "ERROR: Cat dimension mismatch H/W!" << std::endl;
        }
        if (p3_down.size(0) != p4_td.size(0)) {
            std::cerr << "ERROR: Cat dimension mismatch Batch!" << std::endl;
        }

        auto p4_bu_cat = torch::cat({ p3_down, p4_td }, 1);
        auto p4_out = c2f_n4->forward(p4_bu_cat); // N4_out (40x40) -> Output P4

        // N4_out -> Down -> Concat P5
        auto p4_down = down_p4->forward(p4_out); // 20x20
        auto p5_bu_cat = torch::cat({ p4_down, p5 }, 1);
        auto p5_out = c2f_n5->forward(p5_bu_cat); // N5_out (20x20) -> Output P5

        return { p3_out, p4_out, p5_out };
    }

private:
    torch::nn::Upsample up{ nullptr };

    // Top-down modules
    C2f c2f_p4{ nullptr };
    C2f c2f_p3{ nullptr };

    // Bottom-up modules
    ConvModule down_p3{ nullptr };
    C2f c2f_n4{ nullptr };

    ConvModule down_p4{ nullptr };
    C2f c2f_n5{ nullptr };
};

TORCH_MODULE(PAN);

#endif // TORCH_PAN_H
