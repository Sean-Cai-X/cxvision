#include "torch_runtime_detection_executor.h"
#include "torch_runtime_task_types.h"
#include "torch_runtime_manifest.h"
#include "torch_runtime_artifact_writer.h"
#include "torch_v8.h"
#include "torch_detect.h"
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <torch/torch.h>
#include <torch/csrc/jit/serialization/import.h>
#include <filesystem>
#include <sstream>
#include <chrono>
#include <fstream>

namespace
{

struct LetterboxResult {
    cv::Mat image;
    cv::Mat original_image;
    float scale;
    float pad_x;
    float pad_y;
    int original_width;
    int original_height;
};

LetterboxResult LetterboxImage(const cv::Mat& image, int target_width, int target_height)
{
    LetterboxResult result;
    result.original_image = image.clone();
    result.original_width = image.cols;
    result.original_height = image.rows;

    float scale_w = static_cast<float>(target_width) / image.cols;
    float scale_h = static_cast<float>(target_height) / image.rows;
    result.scale = std::min(scale_w, scale_h);

    int resized_w = static_cast<int>(image.cols * result.scale);
    int resized_h = static_cast<int>(image.rows * result.scale);

    cv::Mat resized;
    cv::resize(image, resized, cv::Size(resized_w, resized_h));

    result.pad_x = (target_width - resized_w) / 2.0f;
    result.pad_y = (target_height - resized_h) / 2.0f;

    cv::copyMakeBorder(resized, result.image,
        static_cast<int>(std::floor(result.pad_y)),
        static_cast<int>(std::ceil(result.pad_y)),
        static_cast<int>(std::floor(result.pad_x)),
        static_cast<int>(std::ceil(result.pad_x)),
        cv::BORDER_CONSTANT, cv::Scalar(114, 114, 114));

    return result;
}

bool LoadAndPreprocessImage(
    const std::string& image_path,
    const TorchModelManifest& manifest,
    torch::Tensor& tensor,
    LetterboxResult& letterbox_info,
    std::string& reason)
{
    cv::Mat image = cv::imread(image_path);
    if (image.empty())
    {
        reason = "failed to load input image: " + image_path;
        return false;
    }

    LetterboxResult lb_result;
    if (manifest.letterbox)
    {
        lb_result = LetterboxImage(image, manifest.input_width, manifest.input_height);
    }
    else
    {
        cv::resize(image, lb_result.image, cv::Size(manifest.input_width, manifest.input_height));
        lb_result.scale = 1.0f;
        lb_result.pad_x = 0.0f;
        lb_result.pad_y = 0.0f;
        lb_result.original_width = image.cols;
        lb_result.original_height = image.rows;
        lb_result.original_image = image.clone();
    }

    cv::Mat rgb_img;
    cv::cvtColor(lb_result.image, rgb_img, cv::COLOR_BGR2RGB);

    cv::Mat float_img;
    rgb_img.convertTo(float_img, CV_32F);
    float_img *= manifest.input_scale;

    tensor = torch::from_blob(
        float_img.data,
        {1, manifest.input_height, manifest.input_width, 3},
        torch::kFloat32).clone();
    tensor = tensor.permute({0, 3, 1, 2});

    if (!manifest.mean.empty())
    {
        tensor = tensor - torch::tensor(manifest.mean).view({1, 3, 1, 1}).to(tensor.device());
    }
    if (!manifest.std.empty())
    {
        tensor = tensor / torch::tensor(manifest.std).view({1, 3, 1, 1}).to(tensor.device());
    }

    letterbox_info = lb_result;
    return true;
}

bool LooksLikeTorchScriptArchive(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        return false;
    }

    std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return bytes.find("constants.pkl") != std::string::npos &&
           bytes.find("code/__torch__") != std::string::npos;
}

torch::Tensor NormalizeYoloPredictionLayout(const torch::Tensor& output_tensor, int64_t num_classes)
{
    torch::Tensor pred = output_tensor;
    if (pred.dim() == 2)
    {
        pred = pred.unsqueeze(0);
    }

    if (pred.dim() == 3 && pred.size(1) == 4 + num_classes && pred.size(2) != 4 + num_classes)
    {
        pred = pred.transpose(1, 2).contiguous();
    }

    return pred;
}

torch::Tensor RunDetectionInferenceJIT(
    torch::jit::Module& model,
    const torch::Tensor& input_tensor,
    const std::string& device)
{
    torch::Device torch_device(device == "cuda" ? torch::kCUDA : torch::kCPU);
    model.to(torch_device);
    model.eval();

    auto input = input_tensor.to(torch_device);
    torch::NoGradGuard no_grad;

    try {
        auto output = model.forward({input}).toTensor();
        return output.to(torch::kCPU);
    } catch (const c10::Error& e) {
        try {
            auto output = model.get_method("infer")({input}).toTensor();
            return output.to(torch::kCPU);
        } catch (const c10::Error& e2) {
            auto methods = model.get_methods();
            for (const auto& m : methods) {
                std::cerr << "Available method: " << m.name() << std::endl;
            }
            throw e;
        }
    }
}

torch::Tensor RunDetectionInferenceYOLOv8(
    YOLOv8& model,
    const torch::Tensor& input_tensor,
    const std::string& device)
{
    torch::Device torch_device(device == "cuda" ? torch::kCUDA : torch::kCPU);
    model->to(torch_device);
    model->eval();

    auto input = input_tensor.to(torch_device);
    torch::NoGradGuard no_grad;

    return model->forward(input).to(torch::kCPU);
}

bool PostprocessAndWriteOutput(
    const torch::Tensor& output_tensor,
    const LetterboxResult& letterbox_info,
    const TorchModelManifest& manifest,
    const std::filesystem::path& output_dir,
    std::string& reason,
    std::string& detections_ref,
    std::string& bbox_candidate_list_ref,
    std::string& overlay_ref,
    int& runtime_num_classes)
{
    try
    {
        std::filesystem::create_directories(output_dir);

        torch::Tensor normalized_output = NormalizeYoloPredictionLayout(output_tensor, manifest.num_classes);
        int64_t effective_num_classes = manifest.num_classes;
        if (normalized_output.dim() == 3 && normalized_output.size(2) > 4)
        {
            const int64_t output_num_classes = normalized_output.size(2) - 4;
            if (output_num_classes != manifest.num_classes)
            {
                effective_num_classes = output_num_classes;
            }
        }
        runtime_num_classes = static_cast<int>(effective_num_classes);

        YoloPostProcessConfig post_config;
        post_config.num_classes = effective_num_classes;
        post_config.conf_threshold = manifest.confidence_threshold;
        post_config.iou_threshold = manifest.iou_threshold;
        auto detections = post_process(normalized_output, post_config);

        std::vector<BBox> scaled_detections;
        for (const auto& det : detections)
        {
            BBox scaled = det;
            scaled.x1 = (det.x1 - letterbox_info.pad_x) / letterbox_info.scale;
            scaled.y1 = (det.y1 - letterbox_info.pad_y) / letterbox_info.scale;
            scaled.x2 = (det.x2 - letterbox_info.pad_x) / letterbox_info.scale;
            scaled.y2 = (det.y2 - letterbox_info.pad_y) / letterbox_info.scale;

            scaled.x1 = std::max(0.0f, std::min(static_cast<float>(letterbox_info.original_width - 1), scaled.x1));
            scaled.y1 = std::max(0.0f, std::min(static_cast<float>(letterbox_info.original_height - 1), scaled.y1));
            scaled.x2 = std::max(0.0f, std::min(static_cast<float>(letterbox_info.original_width - 1), scaled.x2));
            scaled.y2 = std::max(0.0f, std::min(static_cast<float>(letterbox_info.original_height - 1), scaled.y2));

            scaled_detections.push_back(scaled);
            if (static_cast<int>(scaled_detections.size()) >= manifest.max_detections)
            {
                break;
            }
        }

        cv::Mat detection_overlay = letterbox_info.original_image.empty()
            ? cv::Mat(letterbox_info.original_height, letterbox_info.original_width, CV_8UC3, cv::Scalar(0, 0, 0))
            : letterbox_info.original_image.clone();

        std::ostringstream detections_os;
        detections_os << "{";
        detections_os << "\"schema\":\"cxvision.torch.detection.detections.v1\",";
        detections_os << "\"num_detections\":" << scaled_detections.size() << ",";
        detections_os << "\"detections\":[";

        for (size_t i = 0; i < scaled_detections.size(); ++i)
        {
            const auto& det = scaled_detections[i];
            if (i > 0) detections_os << ",";
            detections_os << "{";
            detections_os << "\"x1\":" << det.x1 << ",";
            detections_os << "\"y1\":" << det.y1 << ",";
            detections_os << "\"x2\":" << det.x2 << ",";
            detections_os << "\"y2\":" << det.y2 << ",";
            detections_os << "\"confidence\":" << det.score << ",";
            detections_os << "\"class_id\":" << det.cls;
            detections_os << "}";

            cv::rectangle(
                detection_overlay,
                cv::Rect(
                    cv::Point(static_cast<int>(std::round(det.x1)), static_cast<int>(std::round(det.y1))),
                    cv::Point(static_cast<int>(std::round(det.x2)), static_cast<int>(std::round(det.y2)))),
                cv::Scalar(0, 255, 0),
                2);
        }
        detections_os << "]";
        detections_os << "}";

        std::filesystem::path detections_path = output_dir / "detections.json";
        WriteTorchTextArtifact(detections_path, detections_os.str(), reason);
        detections_ref = detections_path.string();

        std::filesystem::path candidate_path = output_dir / "bbox_candidate_list.json";
        WriteTorchTextArtifact(candidate_path, detections_os.str(), reason);
        bbox_candidate_list_ref = candidate_path.string();

        std::filesystem::path overlay_path = output_dir / "detection_overlay.png";
        WriteTorchImageArtifact(overlay_path, detection_overlay, reason);
        overlay_ref = overlay_path.string();

        std::ostringstream request_os;
        request_os << "{";
        request_os << "\"schema\":\"cxvision.torch.detection.request.v1\",";
        request_os << "\"letterbox\":{";
        request_os << "\"scale\":" << letterbox_info.scale << ",";
        request_os << "\"pad_x\":" << letterbox_info.pad_x << ",";
        request_os << "\"pad_y\":" << letterbox_info.pad_y << ",";
        request_os << "\"original_width\":" << letterbox_info.original_width << ",";
        request_os << "\"original_height\":" << letterbox_info.original_height;
        request_os << "}";
        request_os << "}";

        std::filesystem::path request_path = output_dir / "torch_detection_task_request.json";
        WriteTorchTextArtifact(request_path, request_os.str(), reason);

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

        torch::Tensor input_tensor;
        LetterboxResult letterbox_info;
        if (!LoadAndPreprocessImage(request.input_image, manifest, input_tensor, letterbox_info, reason))
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

        torch::Tensor output_tensor;
        double elapsed_ms = 0;

        if (manifest.weights_format == "cpp_archive")
        {
            try
            {
                torch::jit::Module model = torch::jit::load(manifest.model_path.string());
                auto start_time = std::chrono::steady_clock::now();
                output_tensor = RunDetectionInferenceJIT(model, input_tensor, config.device);
                auto end_time = std::chrono::steady_clock::now();
                elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
            }
            catch (const c10::Error& e)
            {
                result.ok = false;
                result.error_code = static_cast<int>(TorchRuntimeErrorCode::ModelLoadFailed);
                result.status = "failed";
                result.error_message = "failed to load cpp_archive model: " + std::string(e.what());
                result.result_json =
                    "{\"schema\":\"cxvision.torch.error.v1\","
                    "\"failure_stage\":\"runtime_model_loading\","
                    "\"reason\":\"model_load_failed\"}";
                return result;
            }
        }
        else if (manifest.weights_format == "python_state_dict")
        {
            try
            {
                auto start_time = std::chrono::steady_clock::now();
                if (LooksLikeTorchScriptArchive(manifest.model_path))
                {
                    try
                    {
                        torch::jit::Module model = torch::jit::load(manifest.model_path.string());
                        output_tensor = RunDetectionInferenceJIT(model, input_tensor, config.device);
                    }
                    catch (const c10::Error&)
                    {
                        ModelConfig model_config = ModelConfig::get_config(manifest.variant);
                        model_config.num_classes = manifest.num_classes;

                        YOLOv8 model(model_config);
                        model->load_checkpoint(manifest.model_path.string());
                        output_tensor = RunDetectionInferenceYOLOv8(model, input_tensor, config.device);
                    }
                }
                else
                {
                    ModelConfig model_config = ModelConfig::get_config(manifest.variant);
                    model_config.num_classes = manifest.num_classes;

                    YOLOv8 model(model_config);
                    model->load_weights(manifest.model_path.string());
                    output_tensor = RunDetectionInferenceYOLOv8(model, input_tensor, config.device);
                }
                auto end_time = std::chrono::steady_clock::now();
                elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
            }
            catch (const c10::Error& e)
            {
                result.ok = false;
                result.error_code = static_cast<int>(TorchRuntimeErrorCode::ModelLoadFailed);
                result.status = "failed";
                result.error_message = "failed to load python_state_dict model: " + std::string(e.what());
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
            result.error_message = "unsupported weights format: " + manifest.weights_format;
            result.result_json =
                "{\"schema\":\"cxvision.torch.error.v1\","
                "\"failure_stage\":\"runtime_weight_load\","
                "\"reason\":\"unsupported_weights_format\"}";
            return result;
        }

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

        std::string detections_ref;
        std::string bbox_candidate_list_ref;
        std::string overlay_ref;
        int runtime_num_classes = manifest.num_classes;
        if (!PostprocessAndWriteOutput(
            output_tensor,
            letterbox_info,
            manifest,
            output_dir,
            reason,
            detections_ref,
            bbox_candidate_list_ref,
            overlay_ref,
            runtime_num_classes))
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
        result.actual_device = config.device;
        result.infer_runtime_ms = elapsed_ms;
        result.algorithm_runtime_ms = elapsed_ms;

        std::ostringstream result_os;
        result_os << "{";
        result_os << "\"schema\":\"cxvision.torch.detection.v1\",";
        result_os << "\"status\":\"success\",";
        result_os << "\"model_id\":\"" << manifest.model_id << "\",";
        result_os << "\"manifest_num_classes\":" << manifest.num_classes << ",";
        result_os << "\"runtime_num_classes\":" << runtime_num_classes << ",";
        result_os << "\"infer_runtime_ms\":" << elapsed_ms;
        result_os << "}";
        result.result_json = result_os.str();

        std::filesystem::path result_path = output_dir / "torch_detection_task_result.json";
        WriteTorchTextArtifact(result_path, result.result_json, reason);
        result.result_ref = result_path.string();

        std::filesystem::path evidence_path = output_dir / "torch_runtime_evidence.json";
        std::ostringstream evidence_os;
        evidence_os << "{";
        evidence_os << "\"schema\":\"cxvision.torch.evidence.v1\",";
        evidence_os << "\"task_id\":\"torch.infer.detection.yolov8.v1\",";
        evidence_os << "\"input_image\":\"" << request.input_image << "\",";
        evidence_os << "\"model_path\":\"" << manifest.model_path.string() << "\",";
        evidence_os << "\"device\":\"" << config.device << "\",";
        evidence_os << "\"runtime_ms\":" << elapsed_ms << ",";
        evidence_os << "\"evidence_items\":[";
        evidence_os << "{\"role\":\"detections\",\"path\":" << QuoteTorchJsonString("detections.json") << "},";
        evidence_os << "{\"role\":\"bbox_candidates\",\"path\":" << QuoteTorchJsonString("bbox_candidate_list.json") << "},";
        evidence_os << "{\"role\":\"overlay\",\"path\":" << QuoteTorchJsonString("detection_overlay.png") << "}";
        evidence_os << "]";
        evidence_os << "}";
        WriteTorchTextArtifact(evidence_path, evidence_os.str(), reason);
        result.evidence_ref = evidence_path.string();

        result.input_image_ref = request.input_image;
        result.primary_visual_ref = overlay_ref;
        result.visualization_refs = overlay_ref;
        result.bbox_candidate_list_ref = bbox_candidate_list_ref;

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
            "\"reason\":\"" + EscapeTorchJsonString(e.what()) + "\"}";
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
            "\"reason\":\"" + EscapeTorchJsonString(e.what()) + "\"}";
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
