#ifndef CXIMAGE_CXSCRIPT_HEADLESS_RUNTIME_H
#define CXIMAGE_CXSCRIPT_HEADLESS_RUNTIME_H

#include <string>
#include <vector>
#include "CxRuntimeProjectionTypes.h"

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
    double avgdist = 0.0;

    int result_rect_count = 0;

    int model_point_count = 0;
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

    std::vector<CxShapeElementSnapshot> shapes;

    bool smoke_pass = false;
    std::string smoke_findline_object_name;
    bool smoke_findline_roi = false;
    bool smoke_findline_scan = false;
    std::string smoke_findcircle_object_name;
    std::string smoke_findcircle_roi_shape_kind;
    double smoke_findcircle_roi_radius = 0.0;
    double smoke_findcircle_outer_scan_radius = 0.0;
};

struct CxScriptHeadlessOptions
{
    bool enabled = false;

    std::string image_path;
    std::string template_image_path;
    std::string script_path;
    std::string output_dir;

    std::string case_name;
    std::string snapshot_path;
    std::string overlay_path;
    std::string summary_path;
    std::string runtime_log_path;

    bool save_overlay = true;
    int max_steps = 10000;
    int timeout_sec = 30;

    int max_elapsed_ms = 5000;
    int max_scan_lines = 4096;
    int max_samples = 200000;

    std::string stage25_image_id;
    std::string stage25_level;
    std::string stage25_target_id;
    std::string stage25_tool;

    int roi_x0 = 0;
    int roi_y0 = 0;
    int roi_x1 = 0;
    int roi_y1 = 0;

    int circle_cx = 0;
    int circle_cy = 0;
    int circle_px = 0;
    int circle_py = 0;

    int tool_half_width = 20;
    int wgap = 32;
    int hgap = 8;
    int gap = 5;
    int linegap = 6;
    int threshold = 20;
    int method = 2;
    int filterprofile = 0;
    int samplerate = 1;
    double min_score = 0.0;
    int find_num = 1;
    int compare_gap = 0;
    int strategy_id = 0;

    bool contract_context_enabled = false;
    bool enable_evidence_analysis = true;
    bool runtime_capture_smoke = false;
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
};

struct CxScriptHeadlessResult
{
    bool ok = false;
    bool launched = false;
    bool executed = false;
    bool runtime_ok = false;
    bool timed_out = false;
    bool assets_complete = false;
    bool support_available = false;

    int exit_code = 0;

    std::string failure_stage;
    std::string reason;

    std::string snapshot_path;
    std::string overlay_path;
    std::string summary_path;
    std::string result_overlay_path;
    std::string evidence_overlay_path;
    std::string tool_display_path;
    std::string runtime_log_path;

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
