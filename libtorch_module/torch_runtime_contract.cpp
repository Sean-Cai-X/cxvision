#include "torch_runtime_task_dispatcher.h"
#include "torch_runtime_task_types.h"
#include "torch_runtime_manifest.h"
#include "torch_runtime_artifact_writer.h"
#include <opencv2/imgcodecs.hpp>

namespace
{

bool CheckInputImage(
    const std::string& input_image_path,
    std::string& reason)
{
    if (input_image_path.empty())
    {
        reason = "input_image is empty";
        return false;
    }

    if (!std::filesystem::exists(input_image_path))
    {
        reason = "input image not found: " + input_image_path;
        return false;
    }

    cv::Mat image = cv::imread(input_image_path);
    if (image.empty())
    {
        reason = "input image is unreadable or empty";
        return false;
    }

    return true;
}

bool CheckDevice(
    const std::string& device,
    std::string& reason)
{
    if (device.empty() || device == "auto" || device == "cpu" || device == "gpu")
        return true;

    reason = "invalid device: " + device;
    return false;
}

bool CheckOutputDirectory(
    const std::filesystem::path& output_dir,
    std::string& reason)
{
    try
    {
        std::filesystem::create_directories(output_dir);
        return true;
    }
    catch (const std::exception& e)
    {
        reason = "failed to create output directory: " + std::string(e.what());
        return false;
    }
}

} // namespace

TorchTaskResultCpp RunTorchCapabilitiesTask(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request)
{
    TorchTaskResultCpp result;

    result.ok = true;
    result.status = "success";
    result.requested_device = config.device.empty() ? "auto" : config.device;
    result.actual_device = "cpu";

    result.result_json =
        "{"
        "\"schema\":\"cxvision.torch.capabilities.v1\","
        "\"schema_version\":1,"
        "\"status\":\"success\","
        "\"supported_tasks\":["
        "\"torch.runtime.capabilities.v1\","
        "\"torch.contract.segmentation.v1\","
        "\"torch.contract.detection.v1\","
        "\"torch.infer.segmentation.deeplabv3plus.v1\","
        "\"torch.infer.detection.yolov8.v1\""
        "],"
        "\"device\":\"cpu\""
        "}";

    return result;
}

TorchTaskResultCpp ValidateTorchSegmentationContract(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request)
{
    TorchTaskResultCpp result;

    result.requested_device = config.device.empty() ? "auto" : config.device;
    result.actual_device = "cpu";

    std::string reason;

    if (!CheckInputImage(request.input_image, reason))
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::InputImageMissing);
        result.status = "input_validation_failed";
        result.error_message = reason;
        result.result_json =
            "{"
            "\"schema\":\"cxvision.torch.contract.v1\","
            "\"schema_version\":1,"
            "\"task\":\"segmentation\","
            "\"status\":\"invalid\","
            "\"input_readable\":false,"
            "\"manifest_valid\":false,"
            "\"weights_available\":false,"
            "\"device_valid\":true,"
            "\"forward_executed\":false,"
            "\"reason\":\"" + reason + "\""
            "}";
        return result;
    }

    if (request.manifest_path.empty())
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::ManifestMissing);
        result.status = "manifest_validation_failed";
        result.error_message = "manifest_path is empty";
        result.result_json =
            "{"
            "\"schema\":\"cxvision.torch.contract.v1\","
            "\"schema_version\":1,"
            "\"task\":\"segmentation\","
            "\"status\":\"invalid\","
            "\"input_readable\":true,"
            "\"manifest_valid\":false,"
            "\"weights_available\":false,"
            "\"device_valid\":true,"
            "\"forward_executed\":false,"
            "\"reason\":\"manifest_path is empty\""
            "}";
        return result;
    }

    TorchModelManifest manifest;
    if (!LoadTorchModelManifest(request.manifest_path, config.model_root, manifest, reason))
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::ManifestInvalid);
        result.status = "manifest_validation_failed";
        result.error_message = reason;
        result.result_json =
            "{"
            "\"schema\":\"cxvision.torch.contract.v1\","
            "\"schema_version\":1,"
            "\"task\":\"segmentation\","
            "\"status\":\"invalid\","
            "\"input_readable\":true,"
            "\"manifest_valid\":false,"
            "\"weights_available\":false,"
            "\"device_valid\":true,"
            "\"forward_executed\":false,"
            "\"reason\":\"" + reason + "\""
            "}";
        return result;
    }

    if (!ValidateSegmentationManifest(manifest, reason))
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::ManifestSchemaMismatch);
        result.status = "manifest_validation_failed";
        result.error_message = reason;
        result.result_json =
            "{"
            "\"schema\":\"cxvision.torch.contract.v1\","
            "\"schema_version\":1,"
            "\"task\":\"segmentation\","
            "\"status\":\"invalid\","
            "\"input_readable\":true,"
            "\"manifest_valid\":false,"
            "\"weights_available\":false,"
            "\"device_valid\":true,"
            "\"forward_executed\":false,"
            "\"reason\":\"" + reason + "\""
            "}";
        return result;
    }

    if (!CheckDevice(config.device, reason))
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::DeviceUnavailable);
        result.status = "device_validation_failed";
        result.error_message = reason;
        result.result_json =
            "{"
            "\"schema\":\"cxvision.torch.contract.v1\","
            "\"schema_version\":1,"
            "\"task\":\"segmentation\","
            "\"status\":\"invalid\","
            "\"input_readable\":true,"
            "\"manifest_valid\":true,"
            "\"weights_available\":true,"
            "\"device_valid\":false,"
            "\"forward_executed\":false,"
            "\"reason\":\"" + reason + "\""
            "}";
        return result;
    }

    std::filesystem::path output_dir = BuildTorchCaseDirectory(config, request, reason);
    if (output_dir.empty())
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::ArtifactWriteFailed);
        result.status = "output_validation_failed";
        result.error_message = reason;
        result.result_json =
            "{"
            "\"schema\":\"cxvision.torch.contract.v1\","
            "\"schema_version\":1,"
            "\"task\":\"segmentation\","
            "\"status\":\"invalid\","
            "\"input_readable\":true,"
            "\"manifest_valid\":true,"
            "\"weights_available\":true,"
            "\"device_valid\":true,"
            "\"forward_executed\":false,"
            "\"reason\":\"" + reason + "\""
            "}";
        return result;
    }

    result.ok = true;
    result.status = "success";
    result.result_json =
        "{"
        "\"schema\":\"cxvision.torch.contract.v1\","
        "\"schema_version\":1,"
        "\"task\":\"segmentation\","
        "\"status\":\"valid\","
        "\"input_readable\":true,"
        "\"manifest_valid\":true,"
        "\"weights_available\":true,"
        "\"device_valid\":true,"
        "\"forward_executed\":false"
        "}";

    return result;
}

TorchTaskResultCpp ValidateTorchDetectionContract(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request)
{
    TorchTaskResultCpp result;

    result.requested_device = config.device.empty() ? "auto" : config.device;
    result.actual_device = "cpu";

    std::string reason;

    if (!CheckInputImage(request.input_image, reason))
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::InputImageMissing);
        result.status = "input_validation_failed";
        result.error_message = reason;
        result.result_json =
            "{"
            "\"schema\":\"cxvision.torch.contract.v1\","
            "\"schema_version\":1,"
            "\"task\":\"detection\","
            "\"status\":\"invalid\","
            "\"input_readable\":false,"
            "\"manifest_valid\":false,"
            "\"weights_available\":false,"
            "\"device_valid\":true,"
            "\"forward_executed\":false,"
            "\"reason\":\"" + reason + "\""
            "}";
        return result;
    }

    if (request.manifest_path.empty())
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::ManifestMissing);
        result.status = "manifest_validation_failed";
        result.error_message = "manifest_path is empty";
        result.result_json =
            "{"
            "\"schema\":\"cxvision.torch.contract.v1\","
            "\"schema_version\":1,"
            "\"task\":\"detection\","
            "\"status\":\"invalid\","
            "\"input_readable\":true,"
            "\"manifest_valid\":false,"
            "\"weights_available\":false,"
            "\"device_valid\":true,"
            "\"forward_executed\":false,"
            "\"reason\":\"manifest_path is empty\""
            "}";
        return result;
    }

    TorchModelManifest manifest;
    if (!LoadTorchModelManifest(request.manifest_path, config.model_root, manifest, reason))
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::ManifestInvalid);
        result.status = "manifest_validation_failed";
        result.error_message = reason;
        result.result_json =
            "{"
            "\"schema\":\"cxvision.torch.contract.v1\","
            "\"schema_version\":1,"
            "\"task\":\"detection\","
            "\"status\":\"invalid\","
            "\"input_readable\":true,"
            "\"manifest_valid\":false,"
            "\"weights_available\":false,"
            "\"device_valid\":true,"
            "\"forward_executed\":false,"
            "\"reason\":\"" + reason + "\""
            "}";
        return result;
    }

    if (!ValidateDetectionManifest(manifest, reason))
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::ManifestSchemaMismatch);
        result.status = "manifest_validation_failed";
        result.error_message = reason;
        result.result_json =
            "{"
            "\"schema\":\"cxvision.torch.contract.v1\","
            "\"schema_version\":1,"
            "\"task\":\"detection\","
            "\"status\":\"invalid\","
            "\"input_readable\":true,"
            "\"manifest_valid\":false,"
            "\"weights_available\":false,"
            "\"device_valid\":true,"
            "\"forward_executed\":false,"
            "\"reason\":\"" + reason + "\""
            "}";
        return result;
    }

    if (!CheckDevice(config.device, reason))
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::DeviceUnavailable);
        result.status = "device_validation_failed";
        result.error_message = reason;
        result.result_json =
            "{"
            "\"schema\":\"cxvision.torch.contract.v1\","
            "\"schema_version\":1,"
            "\"task\":\"detection\","
            "\"status\":\"invalid\","
            "\"input_readable\":true,"
            "\"manifest_valid\":true,"
            "\"weights_available\":true,"
            "\"device_valid\":false,"
            "\"forward_executed\":false,"
            "\"reason\":\"" + reason + "\""
            "}";
        return result;
    }

    std::filesystem::path output_dir = BuildTorchCaseDirectory(config, request, reason);
    if (output_dir.empty())
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::ArtifactWriteFailed);
        result.status = "output_validation_failed";
        result.error_message = reason;
        result.result_json =
            "{"
            "\"schema\":\"cxvision.torch.contract.v1\","
            "\"schema_version\":1,"
            "\"task\":\"detection\","
            "\"status\":\"invalid\","
            "\"input_readable\":true,"
            "\"manifest_valid\":true,"
            "\"weights_available\":true,"
            "\"device_valid\":true,"
            "\"forward_executed\":false,"
            "\"reason\":\"" + reason + "\""
            "}";
        return result;
    }

    result.ok = true;
    result.status = "success";
    result.result_json =
        "{"
        "\"schema\":\"cxvision.torch.contract.v1\","
        "\"schema_version\":1,"
        "\"task\":\"detection\","
        "\"status\":\"valid\","
        "\"input_readable\":true,"
        "\"manifest_valid\":true,"
        "\"weights_available\":true,"
        "\"device_valid\":true,"
        "\"forward_executed\":false"
        "}";

    return result;
}
