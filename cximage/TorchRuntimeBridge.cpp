#include "TorchRuntimeBridge.h"
#include "libtorch_module_runtime_c_api.h"
#include <cstring>

bool TorchRuntimeBridge::Load(const std::string& dll_path)
{
    Unload();

#ifdef _WIN32
    dll_ = LoadLibraryA(dll_path.c_str());
    if (!dll_) {
        return false;
    }

    create_ = reinterpret_cast<CreateFn>(GetProcAddress(dll_, "torch_runtime_create"));
    destroy_ = reinterpret_cast<DestroyFn>(GetProcAddress(dll_, "torch_runtime_destroy"));
    run_task_ = reinterpret_cast<RunTaskFn>(GetProcAddress(dll_, "torch_runtime_run_task"));
    free_result_ = reinterpret_cast<FreeResultFn>(GetProcAddress(dll_, "torch_runtime_free_result"));
    version_ = reinterpret_cast<VersionFn>(GetProcAddress(dll_, "torch_runtime_version"));

    if (!create_ || !destroy_ || !run_task_ || !free_result_) {
        Unload();
        return false;
    }
#else
    dll_ = dlopen(dll_path.c_str(), RTLD_LAZY);
    if (!dll_) {
        return false;
    }

    create_ = reinterpret_cast<CreateFn>(dlsym(dll_, "torch_runtime_create"));
    destroy_ = reinterpret_cast<DestroyFn>(dlsym(dll_, "torch_runtime_destroy"));
    run_task_ = reinterpret_cast<RunTaskFn>(dlsym(dll_, "torch_runtime_run_task"));
    free_result_ = reinterpret_cast<FreeResultFn>(dlsym(dll_, "torch_runtime_free_result"));
    version_ = reinterpret_cast<VersionFn>(dlsym(dll_, "torch_runtime_version"));

    if (!create_ || !destroy_ || !run_task_ || !free_result_) {
        Unload();
        return false;
    }
#endif

    return true;
}

void TorchRuntimeBridge::Unload()
{
    if (handle_) {
        if (destroy_) {
            destroy_(handle_);
        }
        handle_ = nullptr;
    }

    create_ = nullptr;
    destroy_ = nullptr;
    run_task_ = nullptr;
    free_result_ = nullptr;
    version_ = nullptr;

#ifdef _WIN32
    if (dll_) {
        FreeLibrary(dll_);
        dll_ = nullptr;
    }
#else
    if (dll_) {
        dlclose(dll_);
        dll_ = nullptr;
    }
#endif
}

bool TorchRuntimeBridge::Create(const TorchRuntimeGuiConfig& config)
{
    if (!IsLoaded()) {
        return false;
    }

    if (handle_) {
        destroy_(handle_);
        handle_ = nullptr;
    }

    TorchRuntimeConfig c_api_config{};
    if (!config.model_root.empty()) c_api_config.model_root = config.model_root.c_str();
    if (!config.output_root.empty()) c_api_config.output_root = config.output_root.c_str();
    if (!config.device.empty()) c_api_config.device = config.device.c_str();
    if (!config.log_level.empty()) c_api_config.log_level = config.log_level.c_str();

    int ret = create_(&c_api_config, reinterpret_cast<void**>(&handle_));
    return (ret == 0 && handle_ != nullptr);
}

TorchRuntimeGuiResult TorchRuntimeBridge::RunTask(const TorchRuntimeGuiRequest& request)
{
    TorchRuntimeGuiResult result;

    if (!IsLoaded() || !handle_) {
        result.ok = false;
        result.error_message = "Torch runtime not initialized";
        return result;
    }

    TorchTaskRequest c_api_request{};
    if (!request.task.empty()) c_api_request.task = request.task.c_str();
    if (!request.input_image.empty()) c_api_request.input_image = request.input_image.c_str();
    if (!request.dataset_root.empty()) c_api_request.dataset_root = request.dataset_root.c_str();
    if (!request.manifest_path.empty()) c_api_request.manifest_path = request.manifest_path.c_str();
    if (!request.case_name.empty()) c_api_request.case_name = request.case_name.c_str();
    if (!request.extra_json.empty()) c_api_request.extra_json = request.extra_json.c_str();

    TorchTaskResult c_api_result{};

    int ret = run_task_(handle_, &c_api_request, &c_api_result);
    if (ret != 0) {
        result.ok = false;
        result.error_message = "torch_runtime_run_task failed";
        return result;
    }

    result.ok = (c_api_result.ok != 0);
    result.error_code = c_api_result.error_code;
    result.train_runtime_ms = c_api_result.train_runtime_ms;
    result.infer_runtime_ms = c_api_result.infer_runtime_ms;
    result.algorithm_runtime_ms = c_api_result.algorithm_runtime_ms;
    result.placeholder_runtime_ms = c_api_result.placeholder_runtime_ms;

    if (c_api_result.status) result.status = c_api_result.status;
    if (c_api_result.error_message) result.error_message = c_api_result.error_message;
    if (c_api_result.requested_device) result.requested_device = c_api_result.requested_device;
    if (c_api_result.actual_device) result.actual_device = c_api_result.actual_device;
    if (c_api_result.result_json) result.result_json = c_api_result.result_json;
    if (c_api_result.evidence_ref) result.evidence_ref = c_api_result.evidence_ref;
    if (c_api_result.result_ref) result.result_ref = c_api_result.result_ref;
    if (c_api_result.input_image_ref) result.input_image_ref = c_api_result.input_image_ref;
    if (c_api_result.primary_visual_ref) result.primary_visual_ref = c_api_result.primary_visual_ref;
    if (c_api_result.visualization_refs) result.visualization_refs = c_api_result.visualization_refs;
    if (c_api_result.bbox_candidate_list_ref) result.bbox_candidate_list_ref = c_api_result.bbox_candidate_list_ref;
    if (c_api_result.roi_crop_packet_ref) result.roi_crop_packet_ref = c_api_result.roi_crop_packet_ref;
    if (c_api_result.attach_back_ref) result.attach_back_ref = c_api_result.attach_back_ref;
    if (c_api_result.template_alignment_ref) result.template_alignment_ref = c_api_result.template_alignment_ref;
    if (c_api_result.roi_diff_candidate_ref) result.roi_diff_candidate_ref = c_api_result.roi_diff_candidate_ref;
    if (c_api_result.trainer_lifecycle_summary) result.trainer_lifecycle_summary = c_api_result.trainer_lifecycle_summary;
    if (c_api_result.unified_mainline_summary) result.unified_mainline_summary = c_api_result.unified_mainline_summary;

    free_result_(&c_api_result);

    return result;
}

void TorchRuntimeBridge::Destroy()
{
    if (handle_ && destroy_) {
        destroy_(handle_);
        handle_ = nullptr;
    }
}

bool TorchRuntimeBridge::IsLoaded() const
{
    return dll_ != nullptr && create_ != nullptr && destroy_ != nullptr && run_task_ != nullptr && free_result_ != nullptr;
}