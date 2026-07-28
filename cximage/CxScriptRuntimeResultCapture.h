#ifndef CXIMAGE_CXSCRIPT_RUNTIME_RESULT_CAPTURE_H
#define CXIMAGE_CXSCRIPT_RUNTIME_RESULT_CAPTURE_H

#include "CxScriptHeadlessRuntime.h"

namespace mu
{
    class CxParserRuntime;
}

class FindLine;
class FindCircle;
class FindEllipse;
class FindObject;
class FindRect;
class FindSegmentation;
class FastMatch;
class TorchTask;

struct CxScriptToolResultCapture
{
    std::string type;
    std::string name;
    std::string owner_ref;

    bool algorithm_executed = false;
    bool measure_completed = false;
    bool fit_completed = false;
    bool budget_exceeded = false;

    int elapsed_ms = 0;
    int scan_line_count = 0;
    int sample_count = 0;

    int valid_points_count = 0;
    bool has_fit_line = false;
    bool has_fit_circle = false;
    bool has_fit_ellipse = false;
    bool has_result_rect = false;

    double avgdist = 0.0;

    double fit_line_x0 = 0.0;
    double fit_line_y0 = 0.0;
    double fit_line_x1 = 0.0;
    double fit_line_y1 = 0.0;

    double circle_cx = 0.0;
    double circle_cy = 0.0;
    double circle_radius = 0.0;

    double ellipse_cx = 0.0;
    double ellipse_cy = 0.0;
    double ellipse_radius_x = 0.0;
    double ellipse_radius_y = 0.0;
    double ellipse_angle_deg = 0.0;

    int ellipse_scan_candidate_lines = 0;
    int ellipse_scan_total_candidates = 0;
    int ellipse_scan_accepted_points_before_gate = 0;
    double ellipse_accepted_min_boundary_ratio = 0.0;
    double ellipse_accepted_max_boundary_ratio = 0.0;
    double ellipse_accepted_avg_boundary_ratio = 0.0;
    std::string ellipse_candidate_policy;

    int ellipse_scan_lines_outside_roi_count = 0;
    int ellipse_scan_lines_cross_outside_ellipse_count = 0;
    double ellipse_scan_endpoint_norm_min = 0.0;
    double ellipse_scan_endpoint_norm_avg = 0.0;
    double ellipse_scan_endpoint_norm_max = 0.0;
    int ellipse_accepted_points_outside_ellipse_count = 0;
    double ellipse_accepted_point_norm_min = 0.0;
    double ellipse_accepted_point_norm_avg = 0.0;
    double ellipse_accepted_point_norm_max = 0.0;
    int ellipse_rejected_boundary_band_candidate_count = 0;
    double ellipse_rejected_boundary_band_norm_min = 0.0;
    double ellipse_rejected_boundary_band_norm_avg = 0.0;
    double ellipse_rejected_boundary_band_norm_max = 0.0;
    std::string ellipse_scan_geometry_policy;

    int result_rect_count = 0;
    int top1_rect_x = 0;
    int top1_rect_y = 0;
    int top1_rect_w = 0;
    int top1_rect_h = 0;
    int model_point_count = 0;
    int fastmatch_learn_a_count = 0;
    int fastmatch_learn_b_count = 0;
    int fastmatch_learn_a2_count = 0;
    int fastmatch_learn_b2_count = 0;
    int fastmatch_learn_status_code = 0;
    int fastmatch_model_width = 0;
    int fastmatch_model_height = 0;
    int fastmatch_pattern_a_count = 0;
    int fastmatch_pattern_b_count = 0;
    double fastmatch_pattern_a_x = 0.0;
    double fastmatch_pattern_a_y = 0.0;
    double fastmatch_pattern_a_width = 0.0;
    double fastmatch_pattern_a_height = 0.0;
    double fastmatch_pattern_b_x = 0.0;
    double fastmatch_pattern_b_y = 0.0;
    double fastmatch_pattern_b_width = 0.0;
    double fastmatch_pattern_b_height = 0.0;
    int candidate_count = 0;
    double best_score = 0.0;
    bool has_result_box = false;
    bool has_best_result = false;
    int fastmatch_match_call_count = 0;
    int fastmatch_match_ab_call_count = 0;
    int fastmatch_match_sample_ab_call_count = 0;
    int fastmatch_match_last_stage = 0;
    int fastmatch_match_image_width = 0;
    int fastmatch_match_image_height = 0;
    int fastmatch_match_rect_x0 = 0;
    int fastmatch_match_rect_y0 = 0;
    int fastmatch_match_rect_x1 = 0;
    int fastmatch_match_rect_y1 = 0;
    int fastmatch_raw_probe_count = 0;
    int fastmatch_raw_threshold_hit_count = 0;
    int fastmatch_result_to_list_count = 0;
    int fastmatch_candidate_insert_count = 0;
    int fastmatch_candidate_replace_count = 0;
    int fastmatch_candidate_reject_count = 0;

    bool object_prefilter_requested = false;
    bool object_prefilter_applied = false;
    int object_filter_borw = 0;
    int object_filter_min = 0;
    int object_filter_max = 0;
    int fit_filter_input_count = 0;
    int fit_filter_kept_count = 0;
    int fit_filter_rejected_count = 0;
    double fit_filter_sigma = 0.0;
    double fit_filter_threshold = 0.0;

    bool findrect_seed_valid = false;
    bool findrect_top_valid = false;
    bool findrect_bottom_valid = false;
    bool findrect_left_valid = false;
    bool findrect_right_valid = false;
    int findrect_top_points = 0;
    int findrect_bottom_points = 0;
    int findrect_left_points = 0;
    int findrect_right_points = 0;
    double findrect_coarse_score = 0.0;
    double findrect_refine_score = 0.0;

    int segmentation_status_code = 0;
    int segmentation_contour_count = 0;
    double segmentation_primary_area = 0.0;
    std::string segmentation_result_ref;
    std::string segmentation_mask_ref;
    std::string segmentation_contour_ref;
    std::string segmentation_overlay_ref;

    int torch_ok = 0;
    int torch_error_code = 0;
    double torch_infer_ms = 0.0;
    std::string torch_status;
    std::string torch_failure_stage;
    std::string torch_reason;
    int torch_result_count = 0;

    std::string failure_stage;
    std::string reason;

    std::vector<CxShapeElementSnapshot> shapes;
};

bool CaptureRuntimeToolResults(
    mu::CxParserRuntime& runtime,
    CxScriptExecutionCapture& capture,
    std::string& reason);

bool CaptureFindLineResult(
    class FindLine& tool,
    const std::string& object_name,
    CxScriptToolResultCapture& output);

bool CaptureFindCircleResult(
    class FindCircle& tool,
    const std::string& object_name,
    CxScriptToolResultCapture& output);

bool CaptureFindEllipseResult(
    class FindEllipse& tool,
    const std::string& object_name,
    CxScriptToolResultCapture& output);

bool CaptureFindRectResult(
    class FindRect& tool,
    const std::string& object_name,
    CxScriptToolResultCapture& output);

bool CaptureFindObjectResult(
    class FindObject& tool,
    const std::string& object_name,
    CxScriptToolResultCapture& output);

bool CaptureFastMatchResult(
    class FastMatch& tool,
    const std::string& object_name,
    CxScriptToolResultCapture& output);

bool CaptureFindSegmentationResult(
    class FindSegmentation& tool,
    const std::string& object_name,
    CxScriptToolResultCapture& output);

bool CaptureTorchTaskResult(
    class TorchTask& tool,
    const std::string& object_name,
    CxScriptToolResultCapture& output);

#endif
