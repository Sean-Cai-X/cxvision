#include "CxTorchExecutionAdapter.h"
#include "TorchRuntimeResultAdapter.h"
#include <filesystem>
#ifdef _WIN32
#include <Windows.h>
#endif

bool CxTorchExecutionAdapter::Execute(const CxTorchTaskSpec& task, CxInferenceResult& result, std::string& reason)
{
    result = {};

    if (!ValidateCxTorchTaskSpec(task, reason)) {
        result.failure_stage = "torch_request_validation";
        result.reason = reason;
        return false;
    }

    if (!EnsureRuntime(task, reason)) {
        result.failure_stage = "torch_runtime_initialize";
        result.reason = reason;
        return false;
    }

    CxTorchTaskRequest runtime_request;
    if (!BuildRuntimeRequest(task, runtime_request, reason)) {
        result.failure_stage = "torch_request_conversion";
        result.reason = reason;
        return false;
    }

    CxTorchTaskResponse runtime_response;
    if (!service_.Execute(runtime_request, runtime_response, reason)) {
        result.executed = true;
        result.failure_stage = "torch_task_execute";
        result.reason = reason;
        return false;
    }

    TorchRuntimeGuiResult gui_result;
    gui_result.ok = runtime_response.ok;
    gui_result.error_code = runtime_response.error_code;
    gui_result.status = runtime_response.status;
    gui_result.error_message = runtime_response.error_message;
    gui_result.requested_device = runtime_response.requested_device;
    gui_result.actual_device = runtime_response.actual_device;
    gui_result.train_runtime_ms = runtime_response.train_runtime_ms;
    gui_result.infer_runtime_ms = runtime_response.infer_runtime_ms;
    gui_result.algorithm_runtime_ms = runtime_response.algorithm_runtime_ms;
    gui_result.placeholder_runtime_ms = runtime_response.placeholder_runtime_ms;
    gui_result.result_json = runtime_response.result_json;
    gui_result.evidence_ref = runtime_response.evidence_ref;
    gui_result.result_ref = runtime_response.result_ref;
    gui_result.input_image_ref = runtime_response.input_image_ref;
    gui_result.primary_visual_ref = runtime_response.primary_visual_ref;
    gui_result.visualization_refs = runtime_response.visualization_refs;
    gui_result.bbox_candidate_list_ref = runtime_response.bbox_candidate_list_ref;
    gui_result.roi_crop_packet_ref = runtime_response.roi_crop_packet_ref;
    gui_result.attach_back_ref = runtime_response.attach_back_ref;
    gui_result.template_alignment_ref = runtime_response.template_alignment_ref;
    gui_result.roi_diff_candidate_ref = runtime_response.roi_diff_candidate_ref;
    gui_result.trainer_lifecycle_summary = runtime_response.trainer_lifecycle_summary;
    gui_result.unified_mainline_summary = runtime_response.unified_mainline_summary;

    if (!TorchRuntimeResultAdapter::AdaptToInferenceResult(gui_result, task, result, reason)) {
        result.failure_stage = "torch_result_parse";
        result.reason = reason;
        return false;
    }

    reason.clear();
    return true;
}

bool CxTorchExecutionAdapter::EnsureRuntime(const CxTorchTaskSpec& task, std::string& reason)
{
    if (service_.IsReady()) {
        return true;
    }

    std::filesystem::path exe_dir;
#ifdef _WIN32
    wchar_t exe_path[MAX_PATH];
    if (GetModuleFileNameW(nullptr, exe_path, MAX_PATH) > 0) {
        exe_dir = std::filesystem::path(exe_path).parent_path();
    } else {
        exe_dir = std::filesystem::current_path();
    }
#else
    exe_dir = std::filesystem::current_path();
#endif

    auto dll_path = exe_dir / "libtorch_module_runtime.dll";

    CxTorchRuntimeConfig config;
    config.runtime_dll_path = dll_path.string();
    config.device = task.requested_device;

    return service_.Initialize(config, reason);
}

bool CxTorchExecutionAdapter::BuildRuntimeRequest(const CxTorchTaskSpec& task, CxTorchTaskRequest& request, std::string& reason) const
{
    request = {};

    request.task = task.task_id;
    request.device = task.requested_device;
    request.input_image = task.input_image_path.string();
    request.dataset_root = task.dataset_root.string();
    request.manifest_path = task.manifest_path.string();
    request.case_name = task.case_id;
    request.extra_json = task.extra_json;
    request.output_dir = task.output_dir.string();

    if (request.task.empty()) {
        reason = "runtime task is empty";
        return false;
    }

    reason.clear();
    return true;
}
