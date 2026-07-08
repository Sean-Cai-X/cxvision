#include "torch_runtime_core.h"
#include "torch_test_host.h"

static void apply_requested_device_env(const std::string& requested_device) {
#ifdef _WIN32
    if (requested_device == "cpu") {
        _putenv_s("LIBTORCH_MODULE_USE_CUDA", "0");
        return;
    }
    if (requested_device == "gpu") {
        _putenv_s("LIBTORCH_MODULE_USE_CUDA", "1");
        return;
    }
    _putenv_s("LIBTORCH_MODULE_USE_CUDA", "");
#else
    if (requested_device == "cpu") {
        setenv("LIBTORCH_MODULE_USE_CUDA", "0", 1);
        return;
    }
    if (requested_device == "gpu") {
        setenv("LIBTORCH_MODULE_USE_CUDA", "1", 1);
        return;
    }
    unsetenv("LIBTORCH_MODULE_USE_CUDA");
#endif
}

TorchTaskResultCpp RunTorchTask(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request)
{
    TorchTaskResultCpp result;

    try {
        const std::string& requested_device = !config.device.empty() ? config.device : "auto";
        apply_requested_device_env(requested_device);

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

        result.placeholder_runtime_ms = 0.0;

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
        result.error_message = "Unknown exception during torch task execution";
    }

    return result;
}