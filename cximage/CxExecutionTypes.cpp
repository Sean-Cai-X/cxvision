#include "CxExecutionTypes.h"

bool ValidateCxTorchTaskSpec(const CxTorchTaskSpec& task, std::string& reason)
{
    if (task.kind == CxTorchTaskKind::Unknown)
    {
        reason = "torch task kind is unknown";
        return false;
    }

    if (task.task_id.empty())
    {
        reason = "torch task id is empty";
        return false;
    }

    if (task.requested_device != "cpu" &&
        task.requested_device != "cuda" &&
        task.requested_device != "auto")
    {
        reason = "unsupported torch device: " + task.requested_device;
        return false;
    }

    if (task.timeout_ms < 0)
    {
        reason = "torch timeout cannot be negative";
        return false;
    }

    const bool needs_image =
        task.kind == CxTorchTaskKind::Segmentation ||
        task.kind == CxTorchTaskKind::Detection ||
        task.kind == CxTorchTaskKind::Classification ||
        task.kind == CxTorchTaskKind::FeatureExtraction ||
        task.kind == CxTorchTaskKind::TemplateDifference;

    if (needs_image && task.input_image_path.empty())
    {
        reason = "torch input image path is empty";
        return false;
    }

    reason.clear();
    return true;
}