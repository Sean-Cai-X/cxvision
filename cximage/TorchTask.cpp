#include "TorchTask.h"

void TorchTask::settask(const char* value)
{
    task_.task_id = value;
    if (task_.task_id == "torch.runtime.diagnostic")
        task_.kind = CxTorchTaskKind::RuntimeDiagnostic;
    else if (task_.task_id == "torch.device.diagnostic")
        task_.kind = CxTorchTaskKind::DeviceDiagnostic;
    else if (task_.task_id == "torch.weight.diagnostic")
        task_.kind = CxTorchTaskKind::WeightDiagnostic;
    else if (task_.task_id == "torch.runtime.capabilities.v1")
        task_.kind = CxTorchTaskKind::RuntimeCapabilities;
    else if (task_.task_id == "torch.contract.segmentation.v1")
        task_.kind = CxTorchTaskKind::SegmentationContract;
    else if (task_.task_id == "torch.contract.detection.v1")
        task_.kind = CxTorchTaskKind::DetectionContract;
    else if (task_.task_id == "torch.infer.segmentation.deeplabv3plus.v1")
        task_.kind = CxTorchTaskKind::Segmentation;
    else if (task_.task_id == "torch.infer.detection.yolov8.v1")
        task_.kind = CxTorchTaskKind::Detection;
    else if (task_.task_id.find("segmentation") != std::string::npos)
        task_.kind = CxTorchTaskKind::Segmentation;
    else if (task_.task_id.find("detection") != std::string::npos || task_.task_id.find("yolo") != std::string::npos)
        task_.kind = CxTorchTaskKind::Detection;
    else if (task_.task_id.find("classification") != std::string::npos)
        task_.kind = CxTorchTaskKind::Classification;
    else if (task_.task_id.find("feature") != std::string::npos)
        task_.kind = CxTorchTaskKind::FeatureExtraction;
    else if (task_.task_id.find("template") != std::string::npos)
        task_.kind = CxTorchTaskKind::TemplateDifference;
    else if (task_.task_id.find("smoke") != std::string::npos)
        task_.kind = CxTorchTaskKind::RuntimeDiagnostic;
    else if (task_.task_id.find("infer") != std::string::npos)
        task_.kind = CxTorchTaskKind::Segmentation;
    else if (task_.task_id.find("train") != std::string::npos)
        task_.kind = CxTorchTaskKind::TrainingLifecycle;
    else
        task_.kind = CxTorchTaskKind::Unknown;
}

void TorchTask::setcase(const char* value)
{
    task_.case_id = value;
}

void TorchTask::setmodel(const char* value)
{
    task_.model_id = value;
    task_.model_path = value;
}

void TorchTask::setmanifest(const char* value)
{
    task_.manifest_path = value;
}

void TorchTask::setdevice(const char* value)
{
    task_.requested_device = value;
}

void TorchTask::setinputpath(const char* value)
{
    task_.input_image_path = value;
}

void TorchTask::settemplatepath(const char* value)
{
    task_.template_image_path = value;
}

void TorchTask::settimeout(int value)
{
    task_.timeout_ms = value;
}

void TorchTask::run(void* image_object)
{
    result_ = {};
    status_.clear();
    reason_.clear();

    Image* image = static_cast<Image*>(image_object);
    if (image == nullptr || image->getmat().empty()) {
        status_ = "invalid_input_image";
        reason_ = "TorchTask input Image is empty";
        return;
    }

    std::string validation_reason;
    if (!ValidateCxTorchTaskSpec(task_, validation_reason)) {
        status_ = "request_validation_failed";
        reason_ = validation_reason;
        return;
    }

    if (!executor_.Execute(task_, result_, reason_)) {
        status_ = result_.failure_stage.empty() ? "torch_execute_failed" : result_.failure_stage;
        return;
    }

    status_ = result_.status.empty() ? "torch_execute_completed" : result_.status;
}

int TorchTask::getok()
{
    return result_.ok ? 1 : 0;
}

int TorchTask::geterrorcode()
{
    return result_.error_code;
}

int TorchTask::getresultcount()
{
    return static_cast<int>(result_.detections.size());
}

int TorchTask::getmaskavailable()
{
    return (result_.mask.has_value() && result_.mask->available) ? 1 : 0;
}

double TorchTask::getinferms()
{
    return result_.infer_runtime_ms;
}

char* TorchTask::getstatus()
{
    return const_cast<char*>(status_.c_str());
}

char* TorchTask::getreason()
{
    return const_cast<char*>(reason_.c_str());
}

char* TorchTask::getfailstage()
{
    return const_cast<char*>(result_.failure_stage.c_str());
}

char* TorchTask::getactualdevice()
{
    return const_cast<char*>(result_.actual_device.c_str());
}

char* TorchTask::getresultref()
{
    return const_cast<char*>(result_.result_ref.c_str());
}

char* TorchTask::getmaskref()
{
    if (result_.mask.has_value()) {
        return const_cast<char*>(result_.mask->mask_ref.c_str());
    }
    return const_cast<char*>("");
}

char* TorchTask::getoverlayref()
{
    if (result_.mask.has_value()) {
        return const_cast<char*>(result_.mask->overlay_ref.c_str());
    }
    return const_cast<char*>("");
}

const CxInferenceResult& TorchTask::GetInferenceResult() const noexcept
{
    return result_;
}