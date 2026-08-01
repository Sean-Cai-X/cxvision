#ifndef CXIMAGE_CXSCRIPT_HEADLESS_RUNTIME_H
#define CXIMAGE_CXSCRIPT_HEADLESS_RUNTIME_H

#include <string>
#include <vector>
#include <map>
#include "CxRuntimeProjectionTypes.h"

struct CxFindLineScanDiagnosticSnapshot
{
    int scan_index = -1;
    int scan_type = 0;
    double x0 = 0.0;
    double y0 = 0.0;
    double x1 = 0.0;
    double y1 = 0.0;
    int candidate_count = 0;
    bool accepted = false;
    double accepted_x = 0.0;
    double accepted_y = 0.0;
    std::string reject_reason;
};

struct CxScriptExecutionCapture
{
    bool script_compiled = false;
    bool runtime_completed = false;

    int elapsed_ms = 0;
    int budget_ms = 0;
    int max_steps = 0;
    int max_scan_lines = 0;
    int max_samples = 0;
    int scan_line_count = 0;
    int sample_count = 0;
    int tool_method = 0;
    int tool_threshold = 0;
    int tool_wgap = 0;
    int tool_hgap = 0;
    int tool_linegap = 0;

    int scan_rows_examined = 0;
    int scan_rows_with_foreground = 0;
    int scan_runs_total = 0;
    int scan_runs_within_length_limit = 0;
    int scan_runs_over_length_limit = 0;
    int scan_runs_rejected_by_selection = 0;
    int scan_runs_rejected_near_endpoint = 0;
    int scan_points_emitted = 0;

    int strategy_id = 0;
    int selected_method = 0;
    int selected_threshold = 0;
    int selected_wgap = 0;
    int selected_hgap = 0;
    int selected_linegap = 0;
    int selected_filterprofile = 0;

    int valid_points_count = 0;
    bool has_fit_line = false;
    bool has_fit_circle = false;
    bool has_fit_ellipse = false;
    bool has_result_rect = false;

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

    double avgdist = 0.0;

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
    int object_filter_strategy_id = 0;
    int object_filter_borw = 0;
    int object_filter_min = 0;
    int object_filter_max = 0;
    int object_component_count = 0;
    int object_component_accepted_count = 0;
    int object_component_rejected_count = 0;
    int object_component_max_area = 0;
    int object_component_max_width = 0;
    int object_component_max_height = 0;
    int object_foreground_before = 0;
    int object_foreground_after = 0;
    int object_white_component_count = 0;
    int object_white_accepted_count = 0;
    int object_white_rejected_count = 0;
    int object_black_component_count = 0;
    int object_black_accepted_count = 0;
    int object_black_rejected_count = 0;
    std::string object_algorithm_branch;
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
    double torch_train_ms = 0.0;
    double torch_infer_ms = 0.0;
    double torch_total_ms = 0.0;
    int torch_result_count = 0;
    std::string torch_status;
    std::string torch_failure_stage;
    std::string torch_reason;
    std::string torch_evidence_ref;
    std::string torch_primary_visual_ref;
    std::string torch_trainer_lifecycle_summary;
    std::string torch_unified_mainline_summary;

    bool budget_exceeded = false;

    int rendered_roi_count = 0;
    int rendered_scan_count = 0;
    int rendered_measure_points_count = 0;
    int rendered_result_count = 0;

    int result_overlay_changed_pixels = 0;

    std::string failure_stage;
    std::string reason;
    bool contract_context = false;
    bool contract_pass = false;
    std::string contract_status;
    std::string contract_conclusion;
    std::map<std::string, double> runtime_globals;

    std::vector<CxShapeElementSnapshot> shapes;
    std::vector<CxFindLineScanDiagnosticSnapshot> findline_scan_diagnostics;

    bool smoke_pass = false;
    std::string smoke_findline_object_name;
    bool smoke_findline_roi = false;
    bool smoke_findline_scan = false;
    std::string smoke_findcircle_object_name;
    std::string smoke_findcircle_roi_shape_kind;
    double smoke_findcircle_roi_radius = 0.0;
    double smoke_findcircle_outer_scan_radius = 0.0;
};

struct CxScriptResultPackage
{
    std::map<std::string, double> runtime_globals_before;
    std::map<std::string, double> runtime_globals_after;

    std::vector<CxShapeElementSnapshot> shapes;

    std::map<std::string, double> metrics;
    std::map<std::string, std::string> facts;

    std::string tool;
    std::string object_name;
    std::string status;
    std::string failure_stage;
    std::string reason;
};

struct CxScriptHeadlessOptions
{
    bool enabled = false;

    std::string case_id;
    std::string script_path;
    std::string image_path;
    std::string template_image_path;
    std::string output_dir;

    std::string globals_path;
    std::string manifest_path;
    std::string image_id;
    std::string target_id;

    int timeout_sec = 30;
    int max_steps = 10000;

    bool contract_context_enabled = false;
    bool runtime_capture_smoke = false;

    std::map<std::string, double> cli_global_overrides;

    int roi_x0 = 0;
    int roi_y0 = 0;
    int roi_x1 = 0;
    int roi_y1 = 0;

    int circle_cx = 0;
    int circle_cy = 0;
    int circle_px = 0;
    int circle_py = 0;

    int ellipse_x0 = 0;
    int ellipse_y0 = 0;
    int ellipse_x1 = 0;
    int ellipse_y1 = 0;

    int tool_half_width = 20;
    int wgap = 32;
    int hgap = 8;
    int gap = 5;
    int linegap = 6;
    int threshold = 20;
    int method = 0;
    int filterprofile = 0;
    int samplerate = 1;
    double min_score = 0.0;
    int find_num = 1;
    int compare_gap = 0;
    int strategy_id = 0;
    int algorithm_executed = 0;

    int learn_roi_x = 0;
    int learn_roi_y = 0;
    int learn_roi_w = 0;
    int learn_roi_h = 0;
    int search_roi_x = 0;
    int search_roi_y = 0;
    int search_roi_w = 0;
    int search_roi_h = 0;
    int expected_rect_x = 0;
    int expected_rect_y = 0;
    int expected_rect_w = 0;
    int expected_rect_h = 0;

    bool enable_evidence_analysis = true;
    int max_elapsed_ms = 5000;
    int max_scan_lines = 4096;
    int max_samples = 200000;

    int contract_headless_ok = 0;
    int contract_pass_initial = 0;
    int contract_algorithm_executed = 0;
    int contract_budget_exceeded = 0;
    int contract_rendered_measure_points_count = 0;
    int contract_rendered_result_count = 0;
    int contract_result_overlay_changed_pixels = 0;
    int points_count = 0;
    int valid_points_count = 0;
    int has_fit_line = 0;
    int has_fit_circle = 0;
    int runtime_valid_points_count = 0;
    int global_valid_points_count = 0;
    int runtime_has_fit_line = 0;
    int global_has_fit_line = 0;
    int runtime_has_fit_circle = 0;
    int global_has_fit_circle = 0;
    int runtime_global_valid_points_count_mismatch = 0;
    int runtime_global_has_fit_line_mismatch = 0;
    int runtime_global_has_fit_circle_mismatch = 0;
    int runtime_global_result_mismatch = 0;
    double local_support = 0.0;
    double local_mean_distance = 0.0;
    double fit_offset = 0.0;
    double circle_radius = 0.0;
    double avgdist = 0.0;
    std::string policy_guard;
    int policy_guard_match = 0;
    std::string result_status;
    std::string failure_stage;
    std::string result_overlay_path;
    std::string evidence_overlay_path;
    std::string tool_display_path;

    double double_method = 0.0;
    double double_threshold = 0.0;
    double double_gap = 0.0;
    double double_linegap = 0.0;
    double double_wgap = 0.0;
    double double_hgap = 0.0;
    double double_strategy_id = 0.0;

    std::string case_name;
    std::string snapshot_path;
    std::string overlay_path;
    std::string summary_path;
    std::string runtime_log_path;
    bool save_overlay = true;

    std::string stage25_image_id;
    std::string stage25_level;
    std::string stage25_target_id;
    std::string stage25_tool;

    bool save_evidence_candidate = false;
    std::string evidence_candidate_root;
    std::string evidence_candidate_id;
};

struct CxScriptHeadlessResult
{
    bool launched = false;
    bool executed = false;
    bool runtime_ok = false;
    bool assets_complete = false;

    int exit_code = 0;

    std::string failure_stage;
    std::string reason;

    std::string output_dir;
    std::string snapshot_path;
    std::string summary_path;
    std::string result_overlay_path;
    std::string evidence_overlay_path;
    std::string tool_display_path;
    std::string runtime_log_path;

    bool ok = false;
    bool timed_out = false;
    bool support_available = false;

    std::string overlay_path;

    std::string run_state;
    std::string debug_status;
    std::string debug_reason;

    std::string current_result_name;
    std::string current_result_status;
    std::string current_result_reason;

    int points_count = 0;
    int valid_points_count = 0;
    bool has_fit_line = false;
    bool has_fit_circle = false;
    bool has_fit_ellipse = false;
    bool has_result_rect = false;

    int model_point_count = 0;
    int fastmatch_learn_a_count = 0;
    int fastmatch_learn_b_count = 0;
    int fastmatch_learn_a2_count = 0;
    int fastmatch_learn_b2_count = 0;
    int fastmatch_learn_status_code = 0;
    int candidate_count = 0;
    double best_score = 0.0;
    bool has_result_box = false;
    bool has_best_result = false;

    double local_support = 0.0;
    double local_mean_distance = 0.0;
    double fit_offset = 0.0;
    double circle_radius = 0.0;
    double avgdist = 0.0;
};

bool RunCxScriptHeadless(
    const CxScriptHeadlessOptions& options,
    CxScriptHeadlessResult& result);

#endif
