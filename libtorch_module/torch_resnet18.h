#ifndef TORCH_RESNET18_H
#define TORCH_RESNET18_H

// NOTE: formatting normalized during the libtorch_module cleanup pass.

#include <torch/torch.h>
#include <vector>
#include <string>
#include <fstream>
#include <stdexcept>
#include <ATen/autocast_mode.h>
#include <c10/core/AutogradState.h>

#include <torch/csrc/autograd/grad_mode.h>
#include <torch/data/transforms/tensor.h>
#include <torch/data/dataloader.h>

#include "torch_nnmodule.h"
#include "torch_mermaid.h"

// Comment tags used in this file:
// KEY: important execution path.
// MODIFIED: behavior adjusted during the current cleanup pass.
// CHECK: confirm during integration/debug.
// RISK: known limitation or possible issue.
// EVOLVE: recommended future improvement.
// VERIFY: needs explicit runtime or numerical validation.

// =========================================================================
// ResNet BasicBlock
// =========================================================================
class BasicBlockImpl : public torch::nn::Module {
public:
    BasicBlockImpl(int64_t in_channels, int64_t out_channels, int64_t stride = 1) {
        // KEY: first conv may downsample, second conv keeps spatial size.
        cv1 = register_module("cv1",
            ConvModule(in_channels, out_channels, 3, stride, 1, true, true, "relu"));

        // KEY: second conv is always stride 1 in BasicBlock.
        cv2 = register_module("cv2",
            ConvModule(out_channels, out_channels, 3, 1, 1, true, false, "none"));

        // KEY: shortcut is created when shape or channel count changes.
        if (stride != 1 || in_channels != out_channels) {
            downsample = register_module("downsample",
                torch::nn::Sequential(
                    torch::nn::Conv2d(torch::nn::Conv2dOptions(in_channels, out_channels, 1).stride(stride).bias(false)),
                    torch::nn::BatchNorm2d(out_channels)
                ));
        }

        act = register_module("act", torch::nn::ReLU());
    }

    torch::Tensor forward(torch::Tensor x) {
        // KEY: residual add + final activation.
        auto identity = x;

        auto out = cv1->forward(x);
        out = cv2->forward(out);

        if (!downsample.is_empty()) {
            identity = downsample->forward(x);
        }

        out += identity;
        return act->forward(out);
    }

private:
    ConvModule cv1{ nullptr }, cv2{ nullptr };
    torch::nn::Sequential downsample{ nullptr };
    torch::nn::ReLU act{ nullptr };
};
TORCH_MODULE(BasicBlock);

// =========================================================================
// ResNet18
// =========================================================================
class ResNet18Impl : public torch::nn::Module {
public:
    ResNet18Impl(int64_t num_classes = 1000, bool include_top = true)
        : include_top_(include_top), num_classes_(num_classes) {

        // KEY: stem matches the standard ResNet stem.
        stem = register_module("stem",
            ConvModule(3, 64, 7, 2, 3, true, true, "relu"));

        pool = register_module("pool",
            torch::nn::MaxPool2d(torch::nn::MaxPool2dOptions(3).stride(2).padding(1)));

        // KEY: four residual stages produce progressively lower-resolution features.
        layer1 = make_layer(64, 64, 2, 1);
        register_module("layer1", layer1);

        // Layer 2: 64 -> 128, stride 2
        layer2 = make_layer(64, 128, 2, 2);
        register_module("layer2", layer2);

        // Layer 3: 128 -> 256, stride 2
        layer3 = make_layer(128, 256, 2, 2);
        register_module("layer3", layer3);

        // Layer 4: 256 -> 512, stride 2
        layer4 = make_layer(256, 512, 2, 2);
        register_module("layer4", layer4);

        // KEY: optional classification head for include_top mode.
        if (include_top_) {
            avgpool = register_module("avgpool", torch::nn::AdaptiveAvgPool2d(torch::nn::AdaptiveAvgPool2dOptions({ 1, 1 })));
            fc = register_module("fc", torch::nn::Linear(512, num_classes));
        }
    }

    // =========================================================================
    // Transfer-learning helpers
    // =========================================================================

    void load_pretrained_and_reset_head(const std::string& path, int64_t new_num_classes) {
        // KEY: load backbone weights first, then rebuild FC if class count changes.
        load_weights(path);

        if (include_top_ && new_num_classes != num_classes_) {
            std::cout << "[Transfer] Resetting FC layer: " << num_classes_ << " -> " << new_num_classes << std::endl;

            unregister_module("fc");
            fc = register_module("fc", torch::nn::Linear(512, new_num_classes));
            fc->to(this->parameters()[0].device());
            num_classes_ = new_num_classes;

            this->to(this->parameters()[0].device());
        }
    }

    void freeze_backbone(bool freeze = true) {
        // KEY: keep FC trainable while freezing backbone parameters.
        std::cout << "[Transfer] " << (freeze ? "Freezing" : "Unfreezing") << " backbone..." << std::endl;

        for (auto& pair : this->named_parameters()) {
            if (pair.key().find("fc") == std::string::npos) {
                pair.value().set_requires_grad(!freeze);
            }
            else {
                pair.value().set_requires_grad(true);
            }
        }
    }

    float train_step_amp_o(torch::optim::Optimizer& optimizer,
        ManualGradScaler& scaler,
        torch::Tensor imgs, torch::Tensor targets) {

        // MODIFIED: instance-style AMP helper kept for compatibility with existing callers.
        this->train();
        optimizer.zero_grad();

        torch::Tensor loss;

        {
            bool prev_autocast = at::autocast::is_enabled();
            at::autocast::set_enabled(true);

            auto output = this->forward(imgs);
            loss = torch::nn::functional::cross_entropy(output, targets);

            at::autocast::set_enabled(prev_autocast);
        }

        scaler.scale(loss).backward();
        scaler.step(optimizer,this);
        scaler.update();

        return loss.item<float>();
    }

// =========================================================================
// AMP training helpers
// =========================================================================
    template <typename ModelType, typename OptimizerType>
    float train_step_amp_o(
        ModelType& model,
        OptimizerType& optimizer,
        ManualGradScaler& scaler,
        torch::Tensor imgs,
        torch::Tensor targets,
        torch::Device device = torch::kCUDA
    ) {
        model->train();

        // CHECK: this helper expects the caller to pass tensors already placed on the target device.
        c10::AutoGradMode enable_guard(true);
        torch::Tensor output = model->forward(imgs);

        torch::Tensor loss = torch::nn::functional::cross_entropy(output, targets);

        optimizer.zero_grad();

        // scale(loss).backward()
        scaler.scale(loss).backward();

        scaler.step(optimizer, model.get());

        scaler.update();

        return loss.item<float>();
    }

    template <typename ModelType, typename OptimizerType>
    float train_step_amp(
        ModelType& model,
        OptimizerType& optimizer,
        ManualGradScaler& scaler,
        torch::Tensor imgs,
        torch::Tensor targets,
        torch::Device device = torch::kCUDA
    ) {
        model->train();

        // KEY: manual autocast enable/disable around forward + loss.
        bool prev_amp = at::autocast::is_enabled();
        at::autocast::set_enabled(true);

        torch::Tensor output;
        torch::Tensor loss;

        try {
            output = model->forward(imgs);

            if (targets.scalar_type() != torch::kLong) targets = targets.to(torch::kLong);

            loss = torch::nn::functional::cross_entropy(output.to(torch::kFloat32), targets);
        }
        catch (...) {
            at::autocast::set_enabled(prev_amp);
            throw;
        }

        at::autocast::set_enabled(prev_amp);

        optimizer.zero_grad();

        scaler.scale(loss).backward();

        scaler.step(optimizer, model.get());

        scaler.update();

        return loss.item<float>();
    }

    /*
    float evaluate(torch::data::DataLoader<torch::data::datasets::MapDataset<torch::data::datasets::ImageFolder, torch::data::transforms::Stack<>>>& loader,
        torch::Device device) {
        this->eval();
        torch::NoGradGuard no_grad;

        int correct = 0;
        int total = 0;

        for (auto& batch : loader) {
            auto data = batch.data.to(device);
            auto targets = batch.target.to(device);

            auto output = this->forward(data);
            auto pred = output.argmax(1);

            correct += pred.eq(targets).sum().item<int>();
            total += data.size(0);
        }

        return static_cast<float>(correct) / total;
    }
    */

    float evaluate(std::function<torch::data::Example<torch::Tensor, torch::Tensor>()> data_loader,
        torch::Device device) {
        // KEY: iterator-style evaluation helper for custom loaders.
        this->eval();
        torch::NoGradGuard no_grad;

        int correct = 0;
        int total = 0;
        torch::data::Example<torch::Tensor, torch::Tensor> batch;

        while ((batch = data_loader()).data.defined()) {
            auto data = batch.data.to(device);
            auto targets = batch.target.to(device);

            auto output = this->forward(data);
            auto pred = output.argmax(1);

            correct += pred.eq(targets).sum().item<int>();
            total += data.size(0);
        }

        return total > 0 ? static_cast<float>(correct) / total : 0.0f;
    }

    int predict(torch::Tensor img) {
        // KEY: single-image classification helper.
        this->eval();
        torch::NoGradGuard no_grad;
        // img: [1, 3, H, W]
        auto output = this->forward(img);
        return output.argmax(1).item<int>();
    }

    // KEY: classification forward path, optionally returning logits.
    torch::Tensor forward(torch::Tensor x) {
        // KEY: stem + stage1..stage4.
        x = stem->forward(x);
        x = pool->forward(x);

        x = layer1->forward(x);
        auto p2 = x;

        x = layer2->forward(x);
        auto p3 = x;

        x = layer3->forward(x);
        auto p4 = x;

        x = layer4->forward(x);
        auto p5 = x;

        if (include_top_) {
            x = avgpool->forward(x);
            x = x.flatten(1);
            x = fc->forward(x);
            return x;
        }
        else {
            return x;
        }
    }

    std::vector<torch::Tensor> forward_features(torch::Tensor x) {
        // KEY: backbone-style feature export for segmentation or detection heads.
        x = stem->forward(x);
        x = pool->forward(x);

        auto p2 = layer1->forward(x); // stride 4
        auto p3 = layer2->forward(p2); // stride 8
        auto p4 = layer3->forward(p3); // stride 16
        auto p5 = layer4->forward(p4); // stride 32

        return { p3, p4, p5 };
    }

    std::vector<int64_t> get_out_channels() const {
        return { 128, 256, 512 };
    }

/*
import torch
import torchvision.models as models

# Example export helpers used to create C++-friendly ResNet weight files.
# The common pattern is:
# 1. Build the torchvision model.
# 2. Load either a plain state_dict or a JIT archive.
# 3. Move tensors to CPU and convert keys to plain strings.
# 4. Save a plain .pt file for the C++ loader.
# 5. Optionally inspect the file header for debugging.

def export_resnet(name):
    if name == 'resnet18':
        model = models.resnet18(weights=None)
    elif name == 'resnet50':
        model = models.resnet50(weights=None)
    else:
        raise ValueError(f"Unsupported model: {name}")

    sd = model.state_dict()
    cpu_sd = {str(k): v.cpu() for k, v in sd.items()}
    save_name = f"{name}_weights.pt"
    torch.save(cpu_sd, save_name)
    print(f"Exported {name} to {save_name}")

if __name__ == '__main__':
    export_resnet('resnet18')
    export_resnet('resnet50')
*/

    void load_weights(const std::string& path) {
        // CHECK: this loader remaps torchvision-style keys into the local module naming.
        std::cout << "Loading ResNet18 weights from " << path << "..." << std::endl;

        std::vector<char> f;
        try {
            std::ifstream input(path, std::ios::binary);
            f.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
            input.close();
        }
        catch (...) {
            std::cerr << "Failed to handle file: " << path << std::endl; return;
        }

        torch::IValue data = torch::pickle_load(f);
        auto dict = data.toGenericDict();
        auto params = this->named_parameters(true);
        auto buffers = this->named_buffers(true);

        torch::NoGradGuard no_grad;

        for (auto& item : dict) {
            std::string py_key = item.key().toStringRef();
            torch::Tensor val = item.value().toTensor();

            std::string cpp_key = py_key;

            // KEY: remap torchvision stem keys into local ConvModule naming.
            if (py_key.find("conv1.") == 0) {
                cpp_key.replace(0, 5, "stem.conv"); // conv1 -> stem.conv
            }
            else if (py_key.find("bn1.") == 0) {
                cpp_key.replace(0, 3, "stem.bn"); // bn1 -> stem.bn
            }
            // KEY: remap residual block conv/bn keys into cv1/cv2 naming.
            else if (py_key.find("layer") == 0) {
                size_t c1_pos = cpp_key.find(".conv1.");
                if (c1_pos != std::string::npos) {
                    cpp_key.replace(c1_pos, 7, ".cv1.conv.");
                }

                size_t c2_pos = cpp_key.find(".conv2.");
                if (c2_pos != std::string::npos) {
                    cpp_key.replace(c2_pos, 7, ".cv2.conv.");
                }

                size_t b1_pos = cpp_key.find(".bn1.");
                if (b1_pos != std::string::npos) {
                    cpp_key.replace(b1_pos, 5, ".cv1.bn.");
                }

                size_t b2_pos = cpp_key.find(".bn2.");
                if (b2_pos != std::string::npos) {
                    cpp_key.replace(b2_pos, 5, ".cv2.bn.");
                }

                // EVOLVE: add explicit shortcut/downsample remapping if new checkpoints require it.
            }

            if (params.contains(cpp_key)) {
                params[cpp_key].copy_(val);
            }
            else if (buffers.contains(cpp_key)) {
                buffers[cpp_key].copy_(val);
            }
            else {
                // std::cerr << "Unused weight: " << py_key << " -> " << cpp_key << std::endl;
            }
        }
        std::cout << "Weights loaded." << std::endl;
    }

    void export_structure(const std::string& path = "resnet18.mmd") {
        // KEY: export module structure for graph inspection.
        torch_utils::MermaidGenerator::generate(*this, path, "ResNet-18");

        // torch::Tensor x = torch::randn({1, 3, 224, 224}, this->parameters()[0].device());
        // torch_utils::MermaidGenerator::generate_from_trace(*this, {x}, "resnet18_trace.mmd");
    }
private:
    bool include_top_;
    ConvModule stem{ nullptr };
    torch::nn::MaxPool2d pool{ nullptr };

    torch::nn::Sequential layer1{ nullptr };
    torch::nn::Sequential layer2{ nullptr };
    torch::nn::Sequential layer3{ nullptr };
    torch::nn::Sequential layer4{ nullptr };

    torch::nn::AdaptiveAvgPool2d avgpool{ nullptr };
    torch::nn::Linear fc{ nullptr };

    int64_t num_classes_;
    torch::nn::Sequential make_layer(int64_t in_ch, int64_t out_ch, int64_t blocks, int64_t stride) {
        // KEY: build a standard BasicBlock stage.
        torch::nn::Sequential layers;

        layers->push_back(BasicBlock(in_ch, out_ch, stride));

        for (int64_t i = 1; i < blocks; i++) {
            layers->push_back(BasicBlock(out_ch, out_ch, 1));
        }

        return layers;
    }
};
TORCH_MODULE(ResNet18);

#endif // TORCH_RESNET18_H
