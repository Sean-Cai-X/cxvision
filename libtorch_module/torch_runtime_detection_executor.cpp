#include "torch_runtime_detection_executor.h"
#include "torch_runtime_task_types.h"
#include "torch_runtime_manifest.h"
#include "torch_runtime_artifact_writer.h"
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

torch::Tensor RunDetectionInference(
    torch::jit::Module& model,
    const torch::Tensor& input_tensor,
    const std::string& device)
{
    torch::Device torch_device(device == "cuda" ? torch::kCUDA : torch::kCPU);
    model.to(torch_device);
    model.eval();

    auto input = input_tensor.to(torch_device);
    torch::NoGradGuard no_grad;
    auto output = model.forward({input}).toTensor();

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

        float conf_threshold = 0.5f;
        torch::Tensor conf = output_tensor.select(-1, 4);
        torch::Tensor keep = conf > conf_threshold;
        torch::Tensor detections = output_tensor.index_select(0, keep.nonzero().squeeze());

        std::ostringstream json_os;
        json_os << "{";
        json_os << "\"num_detections\":" << detections.size(0) << ",";
        json_os << "\"detections\":[";

        for (int i = 0; i < detections.size(0); ++i)
        {
            float x = detections[i][0].item<float>();
            float y = detections[i][1].item<float>();
            float w = detections[i][2].item<float>();
            float h = detections[i][3].item<float>();
            float conf = detections[i][4].item<float>();
            int cls = detections[i][5].item<int>();

            if (i > 0) json_os << ",";
            json_os << "{";
            json_os << "\"x\":" << x << ",";
            json_os << "\"y\":" << y << ",";
            json_os << "\"width\":" << w << ",";
            json_os << "\"height\":" << h << ",";
            json_os << "\"confidence\":" << conf << ",";
            json_os << "\"class\":" << cls;
            json_os << "}";
        }
        json_os << "]";
        json_os << "}";

        std::filesystem::path summary_path = output_dir / "detection_summary.json";
        WriteTorchTextArtifact(summary_path, json_os.str(), reason);

        return true;
    }
    catch (const std::exception& e)
    {
        reason = "postprocess exception: " + std::string(e.what());
        return false;
    }
}

} // namespace

TorchTaskResultCpp ExecuteTorchDetectionTask(
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
            result.error_message = "input image is required for detection";
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
            result.error_message = "manifest_path is required for detection";
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

        torch::jit::Module model;
        try
        {
            model = torch::jit::load(manifest.model_path.string());
        }
        catch (const c10::Error& e)
        {
            result.ok = false;
            result.error_code = static_cast<int>(TorchRuntimeErrorCode::ModelLoadFailed);
            result.status = "failed";
            result.error_message = "failed to load model: " + std::string(e.what());
            result.result_json =
                "{\"schema\":\"cxvision.torch.error.v1\","
                "\"failure_stage\":\"runtime_model_loading\","
                "\"reason\":\"model_load_failed\"}";
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
        torch::Tensor output_tensor = RunDetectionInference(
            model, input_tensor, config.device);
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
        json_os << "\"schema\":\"cxvision.torch.detection.v1\",";
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

        result.result_ref = "torch_detection_result";
        result.evidence_ref = "torch_detection_evidence";
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
        result.error_message = "Unknown exception during detection";
        result.result_json =
            "{\"schema\":\"cxvision.torch.error.v1\","
            "\"failure_stage\":\"runtime_unknown_exception\"}";
    }

    return result;
}