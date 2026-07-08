#include "TorchRuntimeResultAdapter.h"
#include <sstream>

TorchRuntimeGuiReview TorchRuntimeResultAdapter::Adapt(const TorchRuntimeGuiResult& result)
{
    TorchRuntimeGuiReview review;

    review.stages.push_back({"torch_runtime_start", "Torch Runtime Start", "completed"});
    review.stages.push_back({"torch_task_execution", "Task Execution", result.status});
    review.stages.push_back({"torch_runtime_end", "Torch Runtime End", result.ok ? "success" : "failed"});

    if (!result.input_image_ref.empty()) {
        review.image_refs.push_back({"input_image", result.input_image_ref, "Input Image"});
    }
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
    if (!result.attach_back_ref.empty()) {
        review.result_fields.push_back({"Attach Back", result.attach_back_ref, ""});
    }

    if (!result.trainer_lifecycle_summary.empty()) {
        review.result_fields.push_back({"Trainer Lifecycle", result.trainer_lifecycle_summary, ""});
    }
    if (!result.unified_mainline_summary.empty()) {
        review.result_fields.push_back({"Mainline Summary", result.unified_mainline_summary, ""});
    }

    if (!result.error_message.empty()) {
        review.issue_refs.push_back("error: " + result.error_message);
    }

    return review;
}