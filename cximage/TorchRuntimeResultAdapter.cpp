#include "TorchRuntimeResultAdapter.h"
#include <filesystem>
#include <sstream>

namespace
{

bool IsSegmentationTask(const CxTorchTaskSpec& task)
{
    return task.kind == CxTorchTaskKind::Segmentation ||
           task.kind == CxTorchTaskKind::SegmentationContract ||
           task.task_id.find("segmentation") != std::string::npos ||
           task.task_id.find("deeplab") != std::string::npos;
}

void AttachSegmentationMaskRefs(
    const TorchRuntimeGuiResult& source,
    const CxTorchTaskSpec& task,
    CxInferenceResult& target)
{
    if (!IsSegmentationTask(task) || source.result_ref.empty()) {
        return;
    }

    const std::filesystem::path result_path(source.result_ref);
    const std::filesystem::path result_dir = result_path.parent_path();
    const std::filesystem::path mask_path = result_dir / "mask_binary.png";
    const std::filesystem::path contour_path = result_dir / "contours.json";
    const std::filesystem::path overlay_path = source.primary_visual_ref.empty()
        ? result_dir / "mask_overlay.png"
        : std::filesystem::path(source.primary_visual_ref);

    if (!std::filesystem::exists(mask_path)) {
        return;
    }

    CxTorchMask mask;
    mask.available = true;
    mask.mask_ref = mask_path.string();
    if (std::filesystem::exists(contour_path)) {
        mask.contour_ref = contour_path.string();
    }
    if (std::filesystem::exists(overlay_path)) {
        mask.overlay_ref = overlay_path.string();
    }
    target.mask = mask;
}

} // namespace

bool TorchRuntimeResultAdapter::AdaptToInferenceResult(
    const TorchRuntimeGuiResult& source,
    const CxTorchTaskSpec& task,
    CxInferenceResult& target,
    std::string& reason)
{
    target = {};

    target.executed = true;
    target.ok = source.ok;
    target.error_code = source.error_code;

    target.task_id = task.task_id;
    target.case_id = task.case_id;
    target.model_id = task.model_id;

    target.requested_device = source.requested_device;
    target.actual_device = source.actual_device;

    target.status = source.status;
    target.reason = source.error_message;

    target.train_runtime_ms = source.train_runtime_ms;
    target.infer_runtime_ms = source.infer_runtime_ms;
    target.algorithm_runtime_ms = source.algorithm_runtime_ms;
    target.total_runtime_ms =
        source.train_runtime_ms +
        source.infer_runtime_ms +
        source.algorithm_runtime_ms +
        source.placeholder_runtime_ms;

    target.raw_result_json = source.result_json;
    target.evidence_ref = source.evidence_ref;
    target.result_ref = source.result_ref;
    target.primary_visual_ref = source.primary_visual_ref;
    target.bbox_candidate_list_ref = source.bbox_candidate_list_ref;
    target.roi_crop_packet_ref = source.roi_crop_packet_ref;
    target.template_alignment_ref = source.template_alignment_ref;
    target.roi_diff_candidate_ref = source.roi_diff_candidate_ref;

    AttachSegmentationMaskRefs(source, task, target);

    if (!source.ok) {
        target.failure_stage = "torch_task_execute";
        reason = target.reason.empty() ? "torch task returned failure" : target.reason;
        return false;
    }

    reason.clear();
    return true;
}

TorchRuntimeGuiReview TorchRuntimeResultAdapter::AdaptToGuiReview(const CxInferenceResult& result)
{
    TorchRuntimeGuiReview review;

    review.stages.push_back({"torch_runtime_start", "Torch Runtime Start", "completed"});
    review.stages.push_back({"torch_task_execution", "Task Execution", result.status});
    review.stages.push_back({"torch_runtime_end", "Torch Runtime End", result.ok ? "success" : "failed"});

    if (!result.primary_visual_ref.empty()) {
        review.image_refs.push_back({"primary_visual", result.primary_visual_ref, "Primary Visual"});
    }
    if (!result.bbox_candidate_list_ref.empty()) {
        review.image_refs.push_back({"bbox_candidates", result.bbox_candidate_list_ref, "BBox Candidates"});
    }
    if (!result.roi_crop_packet_ref.empty()) {
        review.image_refs.push_back({"roi_crops", result.roi_crop_packet_ref, "ROI Crops"});
    }
    if (!result.template_alignment_ref.empty()) {
        review.image_refs.push_back({"template_alignment", result.template_alignment_ref, "Template Alignment"});
    }
    if (!result.roi_diff_candidate_ref.empty()) {
        review.image_refs.push_back({"roi_diff", result.roi_diff_candidate_ref, "ROI Diff"});
    }

    review.result_fields.push_back({"Status", result.status, ""});
    review.result_fields.push_back({"Requested Device", result.requested_device, ""});
    review.result_fields.push_back({"Actual Device", result.actual_device, ""});

    if (result.train_runtime_ms > 0) {
        std::ostringstream os;
        os << std::fixed << result.train_runtime_ms;
        review.result_fields.push_back({"Train Runtime", os.str(), "ms"});
    }
    if (result.infer_runtime_ms > 0) {
        std::ostringstream os;
        os << std::fixed << result.infer_runtime_ms;
        review.result_fields.push_back({"Infer Runtime", os.str(), "ms"});
    }
    if (result.algorithm_runtime_ms > 0) {
        std::ostringstream os;
        os << std::fixed << result.algorithm_runtime_ms;
        review.result_fields.push_back({"Algorithm Runtime", os.str(), "ms"});
    }

    if (!result.result_ref.empty()) {
        review.result_fields.push_back({"Result Ref", result.result_ref, ""});
    }
    if (!result.evidence_ref.empty()) {
        review.result_fields.push_back({"Evidence Ref", result.evidence_ref, ""});
    }

    if (!result.reason.empty()) {
        review.issue_refs.push_back("error: " + result.reason);
    }

    return review;
}

TorchRuntimeGuiReview TorchRuntimeResultAdapter::Adapt(const TorchRuntimeGuiResult& result)
{
    CxTorchTaskSpec dummy_task;
    CxInferenceResult inference_result;
    std::string reason;

    AdaptToInferenceResult(result, dummy_task, inference_result, reason);
    return AdaptToGuiReview(inference_result);
}
