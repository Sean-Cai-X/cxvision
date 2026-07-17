#include "FindSegmentationEdgeSamBackend.h"
#include "TorchRuntimeBridge.h"

#ifdef _WIN32
#include <Windows.h>
#endif

#include <filesystem>
#include <sstream>

namespace
{
std::string FindSegmentationTorchRuntimeDllPath()
{
#ifdef _WIN32
    char module_path[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameA(nullptr, module_path, MAX_PATH);
    if (length > 0 && length < MAX_PATH)
    {
        std::filesystem::path exe_path(module_path);
        std::filesystem::path dll_path = exe_path.parent_path() / "libtorch_module_runtime.dll";
        if (std::filesystem::exists(dll_path))
            return dll_path.string();
    }
#endif
    return "libtorch_module_runtime.dll";
}
}

bool FindSegmentationEdgeSamBackend::Run(
    const FindSegmentationInput& input,
    FindSegmentationResult& output,
    std::string& reason)
{
    output.backend = input.backend.empty() ? "edgesam" : input.backend;

    const std::string runtime_dll = FindSegmentationTorchRuntimeDllPath();

    TorchRuntimeBridge bridge;
    if (!bridge.Load(runtime_dll))
    {
        output.ok = false;
        output.backend_status = "runtime_load_failed";
        output.status = "runtime_load_failed";
        output.reason = "failed to load torch runtime dll: " + runtime_dll;
        if (!bridge.LastErrorMessage().empty())
            output.reason += "; " + bridge.LastErrorMessage();
        reason = output.reason;
        return false;
    }

    TorchRuntimeGuiConfig config;
    config.device = input.device.empty() ? "cpu" : input.device;
    config.model_root = input.model_path;
    config.log_level = "info";

    if (!bridge.Create(config))
    {
        output.ok = false;
        output.backend_status = "runtime_create_failed";
        output.status = "runtime_create_failed";
        output.reason = "failed to create torch runtime";
        reason = output.reason;
        return false;
    }

    std::ostringstream extra;
    extra << "{";
    extra << "\"backend\":\"" << output.backend << "\"";
    extra << ",\"threshold\":" << input.threshold;
    extra << ",\"mode\":" << input.mode;
    extra << ",\"has_rect\":" << (input.has_rect ? "true" : "false");
    if (input.has_rect)
    {
        extra << ",\"roi\":{";
        extra << "\"x\":" << input.rect.x;
        extra << ",\"y\":" << input.rect.y;
        extra << ",\"width\":" << input.rect.width;
        extra << ",\"height\":" << input.rect.height;
        extra << "}";
    }
    extra << "}";

    TorchRuntimeGuiRequest request;
    request.task = "torch.infer.mobilenetv3_deeplab.unified";
    request.case_name = "find_segmentation_libtorch_backend";
    request.extra_json = extra.str();

    TorchRuntimeGuiResult torch_result = bridge.RunTask(request);
    bridge.Destroy();

    output.ok = torch_result.ok;
    output.backend_status = torch_result.ok ? "libtorch_contract_ready" : "libtorch_contract_failed";
    output.status = torch_result.ok ? "libtorch_contract_ready" : "libtorch_contract_failed";
    output.reason = torch_result.ok
        ? "libtorch segmentation contract executed; real image mask binding pending"
        : (torch_result.error_message.empty() ? "torch runtime task failed" : torch_result.error_message);

    output.result_ref = torch_result.result_ref;
    output.mask_ref = torch_result.attach_back_ref.empty() ? torch_result.result_ref : torch_result.attach_back_ref;
    output.contour_ref = torch_result.evidence_ref;
    output.overlay_ref = torch_result.primary_visual_ref.empty()
        ? torch_result.evidence_ref
        : torch_result.primary_visual_ref;

    reason = output.reason;
    return output.ok;
}
