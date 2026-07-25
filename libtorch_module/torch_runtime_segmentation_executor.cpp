#include "torch_runtime_segmentation_executor.h"
#include "torch_runtime_task_types.h"
#include "torch_runtime_manifest.h"
#include "torch_runtime_artifact_writer.h"
#include "torch_deeplabv3_plus.h"
#include <opencv2/imgcodecs.hpp>
#include <torch/torch.h>
#include <torch/csrc/jit/serialization/import.h>
#include <filesystem>
#include <sstream>

namespace
{

bool LoadAndPreprocessImage(
    const std::string& image_path,
    const TorchModelManifest& manifest,
    torch::Tensor& tensor,
    std::string& reason)
{
    cv::Mat image = cv::imread(image_path);
    if (image.empty())
    {
        reason = "failed to load input image: " + image_path;
        return false;
    }

    cv::Mat resized;
    cv::resize(image, resized, cv::Size(manifest.input_width, manifest.input_height));

    cv::Mat float_img;
    resized.convertTo(float_img, CV_32F);
    float_img /= 255.0f;

    tensor = torch::from_blob(
        float_img.data,
        {1, manifest.input_height, manifest.input_width, 3},
        torch::kFloat32);
    tensor = tensor.permute({0, 3, 1, 2});

    if (!manifest.mean.empty())
    {
        tensor = tensor - torch::tensor(manifest.mean).view({1, 3, 1, 1}).to(tensor.device());
    }
    if (!manifest.std.empty())
    {
        tensor = tensor / torch::tensor(manifest.std).view({1, 3, 1, 1}).to(tensor.device());
    }

    return true;
}

torch::Tensor RunSegmentationInference(
    std::shared_ptr<torch::jit::Module> jit_model,
    std::shared_ptr<torch::nn::Module> cpp_model,
    const torch::Tensor& input_tensor,
    const std::string& device,
    bool is_jit_model)
{
    torch::Device torch_device(device == "cuda" ? torch::kCUDA : torch::kCPU);

    auto input = input_tensor.to(torch_device);
    torch::NoGradGuard no_grad;

    torch::Tensor output;
    if (is_jit_model) {
        if (!jit_model) {
            throw std::runtime_error("JIT model is null");
        }
        jit_model->to(torch_device);
        jit_model->eval();
        
        std::vector<torch::jit::IValue> inputs;
        inputs.push_back(input);
        auto jit_output = jit_model->forward(inputs);
        if (jit_output.isTensor()) {
            output = jit_output.toTensor();
        } else if (jit_output.isGenericDict()) {
            auto dict = jit_output.toGenericDict();
            if (dict.find("out") != dict.end()) {
                output = dict.at("out").toTensor();
            } else {
                throw std::runtime_error("JIT model output has no 'out' tensor");
            }
        } else {
            throw std::runtime_error("Unknown JIT output type");
        }
    } else if (auto deeplab_model = std::dynamic_pointer_cast<DeepLabV3PlusImpl>(cpp_model)) {
        deeplab_model->to(torch_device);
        deeplab_model->eval();
        
        auto output_map = deeplab_model->forward(input);
        output = output_map["out"];
    } else {
        throw std::runtime_error("Unsupported model type for segmentation");
    }

    return output.to(torch::kCPU);
}

bool PostprocessAndWriteOutput(
    const torch::Tensor& output_tensor,
    const std::filesystem::path& output_dir,
    std::string& reason)
{
    try
    {
        std::filesystem::create_directories(output_dir);

        auto mask = output_tensor.argmax(1).squeeze().to(torch::kInt32);
        cv::Mat mask_mat(mask.size(0), mask.size(1), CV_32S, mask.data_ptr<int32_t>());

        cv::Mat mask_8u;
        mask_mat.convertTo(mask_8u, CV_8U);

        std::filesystem::path mask_path = output_dir / "segmentation_mask.png";
        bool success = cv::imwrite(mask_path.string(), mask_8u);
        if (!success)
        {
            reason = "failed to write segmentation mask";
            return false;
        }

        std::string summary = "{"
            "\"output_channels\":" + std::to_string(output_tensor.size(1)) + ","
            "\"output_height\":" + std::to_string(output_tensor.size(2)) + ","
            "\"output_width\":" + std::to_string(output_tensor.size(3)) + ","
            "\"mask_path\":\"" + mask_path.string() + "\""
            "}";
        std::filesystem::path summary_path = output_dir / "segmentation_summary.json";
        WriteTorchTextArtifact(summary_path, summary, reason);

        return true;
    }
    catch (const std::exception& e)
    {
        reason = "postprocess exception: " + std::string(e.what());
        return false;
    }
}

} // namespace

TorchTaskResultCpp ExecuteTorchSegmentationTask(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request)
{
    TorchTaskResultCpp result;
    std::string reason;

    try
    {
        if (request.input_image.empty())
        {
            result.ok = false;
            result.error_code = static_cast<int>(TorchRuntimeErrorCode::InputImageMissing);
            result.status = "failed";
            result.error_message = "input image is required for segmentation";
            result.result_json =
                "{\"schema\":\"cxvision.torch.error.v1\","
                "\"failure_stage\":\"runtime_input_validation\","
                "\"reason\":\"input_image_missing\"}";
            return result;
        }

        if (request.manifest_path.empty())
        {
            result.ok = false;
            result.error_code = static_cast<int>(TorchRuntimeErrorCode::ManifestMissing);
            result.status = "failed";
            result.error_message = "manifest_path is required for segmentation";
            result.result_json =
                "{\"schema\":\"cxvision.torch.error.v1\","
                "\"failure_stage\":\"runtime_input_validation\","
                "\"reason\":\"manifest_missing\"}";
            return result;
        }

        TorchModelManifest manifest;
        if (!LoadTorchModelManifest(
            std::filesystem::path(request.manifest_path),
            std::filesystem::path(config.model_root),
            manifest,
            reason))
        {
            result.ok = false;
            result.error_code = static_cast<int>(TorchRuntimeErrorCode::ManifestInvalid);
            result.status = "failed";
            result.error_message = "failed to load manifest: " + reason;
            result.result_json =
                "{\"schema\":\"cxvision.torch.error.v1\","
                "\"failure_stage\":\"runtime_manifest_validation\","
                "\"reason\":\"" + reason + "\"}";
            return result;
        }

        if (!std::filesystem::exists(manifest.model_path))
        {
            result.ok = false;
            result.error_code = static_cast<int>(TorchRuntimeErrorCode::ModelNotFound);
            result.status = "failed";
            result.error_message = "model not found: " + manifest.model_path.string();
            result.result_json =
                "{\"schema\":\"cxvision.torch.error.v1\","
                "\"failure_stage\":\"runtime_model_loading\","
                "\"reason\":\"model_not_found\"}";
            return result;
        }

        std::shared_ptr<torch::jit::Module> jit_model = nullptr;
        std::shared_ptr<torch::nn::Module> cpp_model = nullptr;
        bool is_jit_model = false;
        
        if (manifest.weights_format == "cpp_archive")
        {
            try
            {
                jit_model = std::make_shared<torch::jit::Module>(torch::jit::load(manifest.model_path.string()));
                is_jit_model = true;
            }
            catch (const c10::Error& jit_error)
            {
                if (manifest.architecture == "deeplabv3plus")
                {
                    try
                    {
                        DeepLabV3Plus deeplab_model(manifest.backbone, manifest.num_classes);
                        torch::serialize::InputArchive archive;
                        archive.load_from(manifest.model_path.string());
                        deeplab_model->load(archive);
                        cpp_model = deeplab_model->shared_from_this();
                    }
                    catch (const c10::Error& e)
                    {
                        result.ok = false;
                        result.error_code = static_cast<int>(TorchRuntimeErrorCode::ModelLoadFailed);
                        result.status = "failed";
                        result.error_message = "failed to load deeplabv3plus cpp_archive: " + std::string(e.what());
                        result.result_json =
                            "{\"schema\":\"cxvision.torch.error.v1\","
                            "\"failure_stage\":\"runtime_model_loading\","
                            "\"reason\":\"model_load_failed\"}";
                        return result;
                    }
                }
                else
                {
                    result.ok = false;
                    result.error_code = static_cast<int>(TorchRuntimeErrorCode::ModelLoadFailed);
                    result.status = "failed";
                    result.error_message = "failed to load cpp_archive model: " + std::string(jit_error.what());
                    result.result_json =
                        "{\"schema\":\"cxvision.torch.error.v1\","
                        "\"failure_stage\":\"runtime_model_loading\","
                        "\"reason\":\"model_load_failed\"}";
                    return result;
                }
            }
        }
        else if (manifest.weights_format == "python_state_dict")
        {
            if (manifest.architecture == "deeplabv3plus")
            {
                try
                {
                    DeepLabV3Plus deeplab_model(manifest.backbone, manifest.num_classes);
                    torch::load(deeplab_model, manifest.model_path.string());
                    cpp_model = deeplab_model->shared_from_this();
                }
                catch (const c10::Error& e)
                {
                    result.ok = false;
                    result.error_code = static_cast<int>(TorchRuntimeErrorCode::ModelLoadFailed);
                    result.status = "failed";
                    result.error_message = "failed to load deeplabv3plus python_state_dict: " + std::string(e.what());
                    result.result_json =
                        "{\"schema\":\"cxvision.torch.error.v1\","
                        "\"failure_stage\":\"runtime_model_loading\","
                        "\"reason\":\"model_load_failed\"}";
                    return result;
                }
            }
            else
            {
                result.ok = false;
                result.error_code = static_cast<int>(TorchRuntimeErrorCode::WeightFormatUnsupported);
                result.status = "failed";
                result.error_message = "python_state_dict format not supported for architecture: " + manifest.architecture;
                result.result_json =
                    "{\"schema\":\"cxvision.torch.error.v1\","
                    "\"failure_stage\":\"runtime_weight_load\","
                    "\"reason\":\"unsupported_weights_format\"}";
                return result;
            }
        }
        else
        {
            result.ok = false;
            result.error_code = static_cast<int>(TorchRuntimeErrorCode::WeightFormatUnsupported);
            result.status = "failed";
            result.error_message = "unsupported weights format: " + manifest.weights_format;
            result.result_json =
                "{\"schema\":\"cxvision.torch.error.v1\","
                "\"failure_stage\":\"runtime_weight_load\","
                "\"reason\":\"unsupported_weights_format\"}";
            return result;
        }

        torch::Tensor input_tensor;
        if (!LoadAndPreprocessImage(request.input_image, manifest, input_tensor, reason))
        {
            result.ok = false;
            result.error_code = static_cast<int>(TorchRuntimeErrorCode::InputImageInvalid);
            result.status = "failed";
            result.error_message = "failed to preprocess image: " + reason;
            result.result_json =
                "{\"schema\":\"cxvision.torch.error.v1\","
                "\"failure_stage\":\"runtime_preprocessing\","
                "\"reason\":\"" + reason + "\"}";
            return result;
        }

        auto start_time = std::chrono::steady_clock::now();
        torch::Tensor output_tensor = RunSegmentationInference(
            jit_model, cpp_model, input_tensor, config.device, is_jit_model);
        auto end_time = std::chrono::steady_clock::now();
        double elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

        std::filesystem::path output_dir = BuildTorchCaseDirectory(config, request, reason);
        if (output_dir.empty())
        {
            result.ok = false;
            result.error_code = static_cast<int>(TorchRuntimeErrorCode::OutputPathInvalid);
            result.status = "failed";
            result.error_message = "failed to create output directory: " + reason;
            result.result_json =
                "{\"schema\":\"cxvision.torch.error.v1\","
                "\"failure_stage\":\"runtime_output_path\","
                "\"reason\":\"" + reason + "\"}";
            return result;
        }

        if (!PostprocessAndWriteOutput(output_tensor, output_dir, reason))
        {
            result.ok = false;
            result.error_code = static_cast<int>(TorchRuntimeErrorCode::ArtifactWriteFailed);
            result.status = "failed";
            result.error_message = "failed to write output: " + reason;
            result.result_json =
                "{\"schema\":\"cxvision.torch.error.v1\","
                "\"failure_stage\":\"runtime_artifact_write\","
                "\"reason\":\"" + reason + "\"}";
            return result;
        }

        result.ok = true;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::Ok);
        result.status = "success";
        result.requested_device = config.device;
        result.actual_device = torch::cuda::is_available() ? "gpu" : "cpu";
        result.infer_runtime_ms = elapsed_ms;
        result.algorithm_runtime_ms = elapsed_ms;

        std::ostringstream json_os;
        json_os << "{";
        json_os << "\"schema\":\"cxvision.torch.segmentation.v1\",";
        json_os << "\"status\":\"success\",";
        json_os << "\"model_name\":\"" << manifest.model_name << "\",";
        json_os << "\"model_version\":\"" << manifest.model_version << "\",";
        json_os << "\"input_width\":" << manifest.input_width << ",";
        json_os << "\"input_height\":" << manifest.input_height << ",";
        json_os << "\"num_classes\":" << manifest.num_classes << ",";
        json_os << "\"infer_runtime_ms\":" << elapsed_ms << ",";
        json_os << "\"output_dir\":\"" << output_dir.string() << "\"";
        json_os << "}";
        result.result_json = json_os.str();

        result.result_ref = "torch_segmentation_result";
        result.evidence_ref = "torch_segmentation_evidence";
        result.input_image_ref = request.input_image;

    }
    catch (const c10::Error& e)
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::TorchRuntimeError);
        result.status = "torch_exception";
        result.error_message = e.what();
        result.result_json =
            "{\"schema\":\"cxvision.torch.error.v1\","
            "\"failure_stage\":\"runtime_torch_execution\","
            "\"reason\":\"" + std::string(e.what()) + "\"}";
    }
    catch (const std::exception& e)
    {
        result.ok = false;
        result.error_code = -1;
        result.status = "exception";
        result.error_message = e.what();
        result.result_json =
            "{\"schema\":\"cxvision.torch.error.v1\","
            "\"failure_stage\":\"runtime_general_exception\","
            "\"reason\":\"" + std::string(e.what()) + "\"}";
    }
    catch (...)
    {
        result.ok = false;
        result.error_code = -2;
        result.status = "unknown_exception";
        result.error_message = "Unknown exception during segmentation";
        result.result_json =
            "{\"schema\":\"cxvision.torch.error.v1\","
            "\"failure_stage\":\"runtime_unknown_exception\"}";
    }

    return result;
}