#ifndef TORCH_MOBILEVITV2_H
#define TORCH_MOBILEVITV2_H

#include <torch/torch.h>
#include <vector>
#include <string>
#include <cmath>
#include <iostream>
#include <fstream>
#include <iterator>
#include "torch_nnmodule.h"

// Comment tags used in this file:
// KEY: important execution path.
// MODIFIED: behavior adjusted during the current cleanup pass.
// CHECK: confirm during integration/debug.
// RISK: known limitation or possible issue.
// EVOLVE: recommended future improvement.
// VERIFY: needs explicit runtime or numerical validation.

// =========================================================================
// MobileViT Block (Transformer + Conv)
// =========================================================================
class MobileViTBlockImpl : public torch::nn::Module {
public:
    // KEY: local CNN feature -> token-like transformer feature -> fused local feature.
    // in_channels: input channel count.
    // transformer_dim: internal transformer channel width.
    // transformer_depth: reserved for future block stacking.
    // patch_size: local patch size, typically 2x2.
    MobileViTBlockImpl(int64_t in_channels, int64_t transformer_dim,
                       int64_t transformer_depth = 2, int64_t patch_size = 2)
        : in_channels_(in_channels), transformer_dim_(transformer_dim),
          transformer_depth_(transformer_depth), patch_size_(patch_size) {

        // KEY: local representation projection.
        // 1. Local Representation (1x1 Conv to transformer_dim)
        conv_proj = register_module("conv_proj",
            torch::nn::Conv2d(torch::nn::Conv2dOptions(in_channels, transformer_dim, 1).bias(false)));

        // MODIFIED: explicit attention path kept simple for easier LibTorch debugging.
        // RISK: this is a simplified MobileViT-style block, not a paper-faithful implementation.
        // 2. Transformer Encoder (Simplified: Self-Attention + MLP)
        // KEY: normalization before attention and MLP.
        ln1 = register_module("ln1", torch::nn::LayerNorm(torch::nn::LayerNormOptions({transformer_dim})));
        ln2 = register_module("ln2", torch::nn::LayerNorm(torch::nn::LayerNormOptions({transformer_dim})));

        // CHECK: attention head count is fixed to 4 in the current implementation.
        num_heads_ = 4;

        qkv = register_module("qkv",
            torch::nn::Linear(torch::nn::LinearOptions(transformer_dim, transformer_dim * 3).bias(true)));
        proj = register_module("proj",
            torch::nn::Linear(torch::nn::LinearOptions(transformer_dim, transformer_dim).bias(true)));

        // KEY: feed-forward projection after attention.
        fc1 = register_module("fc1",
            torch::nn::Linear(torch::nn::LinearOptions(transformer_dim, transformer_dim * 4).bias(true)));
        fc2 = register_module("fc2",
            torch::nn::Linear(torch::nn::LinearOptions(transformer_dim * 4, transformer_dim).bias(true)));
        gelu = register_module("gelu", torch::nn::GELU());

        // KEY: project transformer output back to CNN channels.
        // 3. Global to Local (1x1 Conv back to in_channels)
        conv_global = register_module("conv_global",
            torch::nn::Conv2d(torch::nn::Conv2dOptions(transformer_dim, in_channels, 1).bias(false)));

        // KEY: fuse residual local branch and transformer branch.
        // 4. Fusion (3x3 Conv)
        conv_fusion = register_module("conv_fusion",
            torch::nn::Conv2d(torch::nn::Conv2dOptions(in_channels * 2, in_channels, 3).padding(1).bias(false)));
    }

    torch::Tensor forward(torch::Tensor x) {
        // KEY: input shape is [B, C, H, W].
        // x: [B, C, H, W]
        auto identity = x;

        // KEY: extract local representation before tokenization.
        auto h = conv_proj->forward(x); // [B, D, H, W]

        // KEY: unfold feature map into a token-like sequence.
        int64_t B = h.size(0);
        int64_t C = h.size(1);
        int64_t H = h.size(2);
        int64_t W = h.size(3);
        // MODIFIED: fail fast on invalid patch geometry.
        TORCH_CHECK(H % patch_size_ == 0 && W % patch_size_ == 0,
            "MobileViTBlock expects H/W divisible by patch size, got H=", H,
            ", W=", W, ", patch_size=", patch_size_);

        // RISK: this unfolding/folding path is an approximation for a stable C++ prototype.
        // VERIFY: compare against a Python reference before broad pretrained-weight reuse.
        auto h_unfold = h.unfold(2, patch_size_, patch_size_).unfold(3, patch_size_, patch_size_);
        // [B, D, H/p, W/p, p, p] -> [B, D, N_patches, p*p]
        h_unfold = h_unfold.contiguous().view({B, C, -1, patch_size_ * patch_size_});
        h_unfold = h_unfold.permute({0, 2, 3, 1}).contiguous(); // [B, N_patches, p*p, D]
        h_unfold = h_unfold.view({B, -1, C}); // [B, Total_Pixels, D] (Simplified)

        // KEY: self-attention and MLP sublayers.
        // 3. Transformer (Self Attention)
        auto ln_out = ln1->forward(h_unfold);
        auto qkv_out = qkv->forward(ln_out); // [B, N, 3D]
        auto query = qkv_out.narrow(2, 0, transformer_dim_);
        auto key = qkv_out.narrow(2, transformer_dim_, transformer_dim_);
        auto value = qkv_out.narrow(2, transformer_dim_ * 2, transformer_dim_);

        // KEY: scaled dot-product attention.
        float scale = 1.0f / std::sqrt(static_cast<float>(transformer_dim_));
        auto attn_scores = torch::bmm(query, key.transpose(1, 2)) * scale;
        auto attn_probs = torch::softmax(attn_scores, -1);
        auto attn_out = torch::bmm(attn_probs, value); // [B, N, D]

        attn_out = proj->forward(attn_out);
        h_unfold = h_unfold + attn_out; // Residual

        // MLP
        auto mlp_out = ln2->forward(h_unfold);
        mlp_out = fc1->forward(mlp_out);
        mlp_out = gelu->forward(mlp_out);
        mlp_out = fc2->forward(mlp_out);
        h_unfold = h_unfold + mlp_out;

        // RISK: fold-back is approximate and should be revisited if feature fidelity matters.
        h_unfold = h_unfold.view({B, H, W, C}).permute({0, 3, 1, 2}); // Approximation

        // 5. Global to Local
        h = conv_global->forward(h_unfold);

        // 6. Fusion
        auto cat = torch::cat({identity, h}, 1);
        return conv_fusion->forward(cat);
    }

private:
    int64_t in_channels_, transformer_dim_, transformer_depth_, patch_size_, num_heads_;
    torch::nn::Conv2d conv_proj{nullptr}, conv_global{nullptr}, conv_fusion{nullptr};
    torch::nn::LayerNorm ln1{nullptr}, ln2{nullptr};
    torch::nn::Linear qkv{nullptr}, proj{nullptr}, fc1{nullptr}, fc2{nullptr};
    torch::nn::GELU gelu{nullptr};
};
TORCH_MODULE(MobileViTBlock);

// =========================================================================
// Inverted Residual (MobileNetV2 Style)
// =========================================================================
class MobileViTv2ResidualImpl : public torch::nn::Module {
public:
    MobileViTv2ResidualImpl(int64_t in_channels, int64_t out_channels, int64_t stride = 1, float expansion = 2.0f) {
        int64_t hidden_channels = static_cast<int64_t>(in_channels * expansion);

        // 1. PW Conv (Expand)
        conv1 = register_module("conv1",
            torch::nn::Conv2d(torch::nn::Conv2dOptions(in_channels, hidden_channels, 1).bias(false)));
        bn1 = register_module("bn1", torch::nn::BatchNorm2d(hidden_channels));
        act1 = register_module("act1", torch::nn::SiLU());

        // 2. DW Conv
        conv2 = register_module("conv2",
            torch::nn::Conv2d(torch::nn::Conv2dOptions(hidden_channels, hidden_channels, 3)
                .stride(stride).padding(1).groups(hidden_channels).bias(false)));
        bn2 = register_module("bn2", torch::nn::BatchNorm2d(hidden_channels));
        act2 = register_module("act2", torch::nn::SiLU());

        // 3. PW Conv (Project)
        conv3 = register_module("conv3",
            torch::nn::Conv2d(torch::nn::Conv2dOptions(hidden_channels, out_channels, 1).bias(false)));
        bn3 = register_module("bn3", torch::nn::BatchNorm2d(out_channels));

        use_skip = (stride == 1 && in_channels == out_channels);
    }

    torch::Tensor forward(torch::Tensor x) {
        auto out = act1->forward(bn1->forward(conv1->forward(x)));
        out = act2->forward(bn2->forward(conv2->forward(out)));
        out = bn3->forward(conv3->forward(out));

        if (use_skip) {
            out = out + x;
        }
        return out;
    }

private:
    bool use_skip;
    torch::nn::Conv2d conv1{nullptr}, conv2{nullptr}, conv3{nullptr};
    torch::nn::BatchNorm2d bn1{nullptr}, bn2{nullptr}, bn3{nullptr};
    torch::nn::SiLU act1{nullptr}, act2{nullptr};
};
TORCH_MODULE(MobileViTv2Residual);

// =========================================================================
// MobileViTv2 Model (1.0 ImageNet 256)
// =========================================================================
class MobileViTv2Impl : public torch::nn::Module {
public:
    MobileViTv2Impl(int64_t num_classes = 1000, const std::string& variant = "small") {
        num_classes_ = num_classes;
        // CHECK: variant is currently accepted for API compatibility only.

        // Stem: 3x3 Conv, stride 2, 16 channels
        stem = register_module("stem",
            torch::nn::Conv2d(torch::nn::Conv2dOptions(3, 16, 3).stride(2).padding(1).bias(false)));
        bn_stem = register_module("bn_stem", torch::nn::BatchNorm2d(16));
        act_stem = register_module("act_stem", torch::nn::SiLU());

        // Stage 1: 1x Residual (stride 2), 32 channels
        stage1 = register_module("stage1", MobileViTv2Residual(16, 32, 2));

        // Stage 2: 1x Residual (stride 2), 64 channels + 2x MobileViT Block
        stage2_res = register_module("stage2_res", MobileViTv2Residual(32, 64, 2));
        stage2_mvit = register_module("stage2_mvit", MobileViTBlock(64, 96, 2));

        // Stage 3: 1x Residual (stride 2), 96 channels + 4x MobileViT Block
        stage3_res = register_module("stage3_res", MobileViTv2Residual(64, 96, 2));
        stage3_mvit = register_module("stage3_mvit", MobileViTBlock(96, 144, 4));

        // Stage 4: 1x Residual (stride 2), 128 channels + 3x MobileViT Block
        stage4_res = register_module("stage4_res", MobileViTv2Residual(96, 128, 2));
        stage4_mvit = register_module("stage4_mvit", MobileViTBlock(128, 192, 3));

        // KEY: classification head.
        conv_head = register_module("conv_head",
            torch::nn::Conv2d(torch::nn::Conv2dOptions(128, 640, 1).bias(false)));
        bn_head = register_module("bn_head", torch::nn::BatchNorm2d(640));
        act_head = register_module("act_head", torch::nn::SiLU());

        pool = register_module("pool", torch::nn::AdaptiveAvgPool2d(torch::nn::AdaptiveAvgPool2dOptions({1, 1})));
        classifier = register_module("classifier", torch::nn::Linear(640, num_classes));
    }

    torch::Tensor forward(torch::Tensor x) {
        // KEY: stem -> residual/mobilevit stages -> pooled classifier head.
        // Stem
        x = act_stem->forward(bn_stem->forward(stem->forward(x)));

        // Stage 1
        x = stage1->forward(x);

        // Stage 2
        x = stage2_res->forward(x);
        x = stage2_mvit->forward(x);

        // Stage 3
        x = stage3_res->forward(x);
        x = stage3_mvit->forward(x);

        // Stage 4
        x = stage4_res->forward(x);
        x = stage4_mvit->forward(x);

        // Head
        x = act_head->forward(bn_head->forward(conv_head->forward(x)));
        x = pool->forward(x);
        x = x.flatten(1);
        x = classifier->forward(x);

        return x;
    }

    // MODIFIED: local pickle/state_dict loader for direct debug and migration experiments.
    // CHECK: assumes the external state_dict keys already match the C++ module names.
    // VERIFY: HuggingFace or timm checkpoints may still need explicit key remapping.
    void load_weights(const std::string& path) {
        std::ifstream input(path, std::ios::binary);
        TORCH_CHECK(input.is_open(), "Failed to open MobileViTv2 weight file: ", path);
        std::vector<char> f((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        torch::IValue data = torch::pickle_load(f);
        TORCH_CHECK(data.isGenericDict(),
                    "MobileViTv2 weight file must be a pickled state_dict GenericDict, got tag=",
                    data.tagKind(), " path=", path);
        auto dict = data.toGenericDict();

        auto params = this->named_parameters(true);
        auto buffers = this->named_buffers(true);
        torch::NoGradGuard no_grad;

        int loaded = 0;
        int skipped_shape_mismatch = 0;
        int skipped_non_tensor = 0;
        for (auto& item : dict) {
            std::string py_key = item.key().toStringRef();
            if (!item.value().isTensor()) {
                skipped_non_tensor++;
                continue;
            }
            torch::Tensor val = item.value().toTensor();

            // CHECK: this assumes Python and C++ module keys are already aligned.
            // EVOLVE: add an explicit key remapping layer for timm/HuggingFace checkpoints.

            if (params.contains(py_key)) {
                auto target = params[py_key];
                if (!target.sizes().equals(val.sizes())) {
                    skipped_shape_mismatch++;
                    std::cout << "[MobileViTv2] Skip parameter shape mismatch key=" << py_key
                              << " target=" << target.sizes()
                              << " source=" << val.sizes() << std::endl;
                    continue;
                }
                target.copy_(val.to(target.device(), target.scalar_type()));
                loaded++;
            } else if (buffers.contains(py_key)) {
                auto target = buffers[py_key];
                if (!target.sizes().equals(val.sizes())) {
                    skipped_shape_mismatch++;
                    std::cout << "[MobileViTv2] Skip buffer shape mismatch key=" << py_key
                              << " target=" << target.sizes()
                              << " source=" << val.sizes() << std::endl;
                    continue;
                }
                target.copy_(val.to(target.device(), target.scalar_type()));
                loaded++;
            }
        }
        std::cout << "[MobileViTv2] Loaded " << loaded << " tensors from " << path
                  << " skipped_shape_mismatch=" << skipped_shape_mismatch
                  << " skipped_non_tensor=" << skipped_non_tensor
                  << std::endl;
    }

    //
    // KEY: reset the classifier head for transfer learning.
    // EVOLVE: preserve old head metadata if multi-head fine-tuning becomes necessary.
    void reset_head(int64_t new_num_classes) {
        unregister_module("classifier");
        classifier = register_module("classifier", torch::nn::Linear(640, new_num_classes));
        num_classes_ = new_num_classes;
    }

private:
    int64_t num_classes_;
    torch::nn::Conv2d stem{nullptr}, conv_head{nullptr};
    torch::nn::BatchNorm2d bn_stem{nullptr}, bn_head{nullptr};
    torch::nn::SiLU act_stem{nullptr}, act_head{nullptr};

    MobileViTv2Residual stage1{nullptr}, stage2_res{nullptr}, stage3_res{nullptr}, stage4_res{nullptr};
    MobileViTBlock stage2_mvit{nullptr}, stage3_mvit{nullptr}, stage4_mvit{nullptr};

    torch::nn::AdaptiveAvgPool2d pool{nullptr};
    torch::nn::Linear classifier{nullptr};
};
TORCH_MODULE(MobileViTv2);

#endif // TORCH_MOBILEVITV2_H
