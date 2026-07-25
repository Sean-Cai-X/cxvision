#include "torch_runtime_task_dispatcher.h"
#include "torch_runtime_task_types.h"
#include "torch_runtime_manifest.h"
#include "torch_runtime_segmentation_executor.h"
#include "torch_runtime_detection_executor.h"
#include "torch_test_host.h"

namespace
{

TorchTaskResultCpp MakeUnsupportedTask(
    const std::string& task)
{
    TorchTaskResultCpp result;

    result.ok = false;
    result.error_code =
        static_cast<int>(
            TorchRuntimeErrorCode::UnsupportedTask);

    result.status = "unsupported_task";
    result.error_message =
        "unsupported production torch task: " + task;

    result.result_json =
        "{"
        "\"schema\":\"cxvision.torch.error.v1\","
        "\"failure_stage\":\"runtime_task_unsupported\","
        "\"task\":\"" + task + "\""
        "}";

    return result;
}

bool IsLegacyTestHostTask(
    const std::string& task)
{
    return TorchTestHost::find_task_spec(task) != nullptr;
}

} // namespace

TorchTaskResultCpp RunLegacyTorchTestHostTask(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request)
{
    TorchTaskResultCpp result;

    try {
        const std::string& requested_device = !config.device.empty() ? config.device : "auto";

        TorchTestHost host;
        const auto report = host.run_task_report(request.task, requested_device);

        result.ok = (report.failures == 0);
        result.error_code = report.failures;
        result.requested_device = report.requested_device;
        result.actual_device = report.actual_device;

        if (report.failures == 0) {
            result.status = "success";
        } else {
            result.status = "failed";
            result.error_message = report.summary;
        }

        if (request.task.find("train") != std::string::npos) {
            result.train_runtime_ms = static_cast<double>(report.runtime_ms);
        } else if (request.task.find("infer") != std::string::npos) {
            result.infer_runtime_ms = static_cast<double>(report.runtime_ms);
            result.algorithm_runtime_ms = static_cast<double>(report.runtime_ms);
        } else {
            result.algorithm_runtime_ms = static_cast<double>(report.runtime_ms);
        }

        std::ostringstream json_os;
        json_os << "{";
        json_os << "\"task\":\"" << request.task << "\",";
        json_os << "\"status\":\"" << result.status << "\",";
        json_os << "\"failures\":" << report.failures << ",";
        json_os << "\"summary\":\"" << report.summary << "\",";
        json_os << "\"requested_device\":\"" << report.requested_device << "\",";
        json_os << "\"actual_device\":\"" << report.actual_device << "\",";
        json_os << "\"runtime_ms\":" << report.runtime_ms;
        json_os << "}";
        result.result_json = json_os.str();

        const auto* spec = TorchTestHost::find_task_spec(request.task);
        if (spec != nullptr) {
            result.result_ref = torch_make_handoff_ref(spec->task_id, "result");
            result.evidence_ref = torch_make_handoff_ref(spec->task_id, "evidence");
            result.attach_back_ref = spec->attach_back_result;

            if (torch_task_id_contains(spec->task_id, "yolo")) {
                result.bbox_candidate_list_ref = torch_make_handoff_ref(spec->task_id, "bbox_candidates");
            }
            if (torch_task_id_contains(spec->task_id, "mobilevit")) {
                result.roi_crop_packet_ref = torch_make_handoff_ref(spec->task_id, "roi_crops");
                result.template_alignment_ref = torch_make_handoff_ref(spec->task_id, "template_alignment");
                result.roi_diff_candidate_ref = torch_make_handoff_ref(spec->task_id, "roi_diff");
            }
            if (torch_task_id_contains(spec->task_id, "train")) {
                result.trainer_lifecycle_summary = "trainer_lifecycle_completed";
                result.unified_mainline_summary = "unified_mainline_bundle_available";
            }
        }

        if (!request.input_image.empty()) {
            result.input_image_ref = request.input_image;
            result.primary_visual_ref = request.input_image;
        }

        result.visualization_refs = "";

    } catch (const std::exception& e) {
        result.ok = false;
        result.error_code = -1;
        result.status = "exception";
        result.error_message = e.what();
    } catch (...) {
        result.ok = false;
        result.error_code = -2;
        result.status = "unknown_exception";
        result.error_message = "Unknown exception during legacy torch task execution";
    }

    return result;
}

TorchTaskResultCpp DispatchTorchRuntimeTask(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request)
{
    if (request.task ==
        TorchRuntimeTaskIds::Capabilities)
    {
        return RunTorchCapabilitiesTask(
            config,
            request);
    }

    if (request.task ==
        TorchRuntimeTaskIds::SegmentationContract)
    {
        return ValidateTorchSegmentationContract(
            config,
            request);
    }

    if (request.task ==
        TorchRuntimeTaskIds::DetectionContract)
    {
        return ValidateTorchDetectionContract(
            config,
            request);
    }

    if (request.task ==
        TorchRuntimeTaskIds::
            DeepLabV3PlusSegmentation)
    {
        return ExecuteTorchSegmentationTask(
            config,
            request);
    }

    if (request.task ==
        TorchRuntimeTaskIds::YoloV8Detection)
    {
        return ExecuteTorchDetectionTask(
            config,
            request);
    }

    if (IsLegacyTestHostTask(request.task))
    {
        return RunLegacyTorchTestHostTask(
            config,
            request);
    }

    return MakeUnsupportedTask(request.task);
}
