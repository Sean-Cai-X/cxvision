#ifndef TORCH_CROSS_SCALE_ATTENTION_H
#define TORCH_CROSS_SCALE_ATTENTION_H

// NOTE: formatting normalized during the libtorch_module cleanup pass.

#include <torch/torch.h>
#include "torch_nnmodule.h"

// =========================================================================
// Cross-Scale Non-local Module
// =========================================================================
// Query: Low-level Feature (e.g., P3) -> [B, C_inter, H3*W3]
// Key:   High-level Feature (e.g., P4) -> [B, C_inter, H4*W4]
// Value: High-level Feature (e.g., P4) -> [B, C_val,   H4*W4]
//
// Attention = Softmax(Query * Key^T)   -> [B, H3*W3, H4*W4]
// Output    = Attention * Value^T      -> [B, H3*W3, C_val] -> Reshape -> [B, C_val, H3, W3]
// Final     = Conv(Output) + P3
class CrossScaleAttentionImpl : public torch::nn::Module {
public:
    CrossScaleAttentionImpl(int64_t in_channels_q, int64_t in_channels_kv, int64_t reduction = 2) {

        int64_t inter_channels = in_channels_q / reduction;
        if (inter_channels < 16) inter_channels = 16;

        // 1. Query Transform (P3 -> inter_channels)
        conv_query = register_module("conv_query",
            torch::nn::Conv2d(torch::nn::Conv2dOptions(in_channels_q, inter_channels, 1).bias(false)));

        // 2. Key Transform (P4 -> inter_channels)
        conv_key = register_module("conv_key",
            torch::nn::Conv2d(torch::nn::Conv2dOptions(in_channels_kv, inter_channels, 1).bias(false)));

        // 3. Value Transform (P4 -> in_channels_q)
        conv_value = register_module("conv_value",
            torch::nn::Conv2d(torch::nn::Conv2dOptions(in_channels_kv, in_channels_q, 1).bias(false)));

        // 4. Output Transform (W_z)
        conv_out = register_module("conv_out",
            torch::nn::Conv2d(torch::nn::Conv2dOptions(in_channels_q, in_channels_q, 1).bias(false)));

        torch::nn::init::constant_(conv_out->weight, 0);

        scale_ = 1.0f / std::sqrt(static_cast<float>(inter_channels));
    }

    // Forward
    // x_q:  Query Feature (e.g., P3) [B, Cq, H3, W3]
    // x_kv: Key/Value Feature (e.g., P4) [B, Ckv, H4, W4]
    torch::Tensor forward(torch::Tensor x_q, torch::Tensor x_kv) {
        int64_t batch_size = x_q.size(0);

        // 1. Generate Query: [B, C_inter, H3, W3] -> [B, C_inter, N3] -> [B, N3, C_inter]
        auto query = conv_query->forward(x_q);
        int64_t h3 = query.size(2);
        int64_t w3 = query.size(3);
        int64_t n3 = h3 * w3;
        // view: [B, C, N], permute: [B, N, C]
        query = query.view({ batch_size, -1, n3 }).permute({ 0, 2, 1 });

        // 2. Generate Key: [B, C_inter, H4, W4] -> [B, C_inter, N4]
        auto key = conv_key->forward(x_kv);
        int64_t n4 = key.size(2) * key.size(3);
        key = key.view({ batch_size, -1, n4 });

        // 3. Generate Value: [B, C_q, H4, W4] -> [B, C_q, N4] -> [B, N4, C_q]
        auto value = conv_value->forward(x_kv);
        value = value.view({ batch_size, -1, n4 }).permute({ 0, 2, 1 });

        // 4. Attention Map: [B, N3, C] * [B, C, N4] -> [B, N3, N4]
        // Q * K^T
        auto attn = torch::bmm(query, key);
        attn = attn * scale_; // Scaled Dot-Product
        attn = torch::softmax(attn, -1);

        // 5. Aggregation: [B, N3, N4] * [B, N4, C_q] -> [B, N3, C_q]
        auto out = torch::bmm(attn, value);

        // 6. Reshape back: [B, N3, C_q] -> [B, C_q, N3] -> [B, C_q, H3, W3]
        out = out.permute({ 0, 2, 1 }).view({ batch_size, -1, h3, w3 });

        // 7. Output Project & Residual
        out = conv_out->forward(out);

        return x_q + out;
    }

private:
    torch::nn::Conv2d conv_query{ nullptr };
    torch::nn::Conv2d conv_key{ nullptr };
    torch::nn::Conv2d conv_value{ nullptr };
    torch::nn::Conv2d conv_out{ nullptr };
    float scale_;
};

TORCH_MODULE(CrossScaleAttention);

#endif // TORCH_CROSS_SCALE_ATTENTION_H