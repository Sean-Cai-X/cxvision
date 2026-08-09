#ifndef CXIMAGE_MANUAL_STATE_TEST_CONSOLE_H
#define CXIMAGE_MANUAL_STATE_TEST_CONSOLE_H

#include "ParserDebugBridge.h"
#include "CxScriptCatalogRuntime.h"
#include "CxParamRegressionRuntime.h"
#include "CxScriptHeadlessRuntime.h"
#include "CxEvidenceSelfTestRuntime.h"
#include "metrology_analytics/tests/ManualConsoleAnalyticsSmoke.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <iomanip>
#include <array>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <imgui.h>
struct ScriptLineView
{
  int line_no = 0;
  std::string statement;
  std::string module;
  std::string object_type;
  std::string object;
  std::string method;
  std::string params;
  std::string return_variable;
  std::string status = "PENDING";
  std::string reason = "not executed";
  std::string timestamp;
};

struct ScriptVariableView
{
  std::string type;
  std::string name;
  std::string value;
  int declared_line = 0;
  std::string status = "observed_source";
  std::string image_path;
  bool image_initialized = false;
};

struct ScriptObjectView
{
  std::string module;
  std::string type;
  std::string name;
  std::string status;
  std::string runtime_state;
  int runtime_source_line = 0;
  int declared_line = 0;
};

struct RuntimeObjectView
{
    std::string name;
    std::string type;
    int declared_line = 0;

    bool exists_in_parser = false;

    std::string last_runtime_status = "PENDING";
    std::string runtime_state = "PENDING";
    std::string last_method;
    int last_update_line = 0;

    std::string display_summary;

    bool visualizable = false;
    std::string visual_source = "stale_runtime";
    bool stale = true;

    // FindEllipse ROI / measure status.  Current FindEllipse runtime exposes
    // ROI and measurement points; fitted ellipse result is a later binding.
    bool has_ellipse_roi = false;
    float ellipse_cx = 0.0f;
    float ellipse_cy = 0.0f;
    float ellipse_rx = 0.0f;
    float ellipse_ry = 0.0f;
    bool has_fit_ellipse = false;
    float fit_ellipse_cx = 0.0f;
    float fit_ellipse_cy = 0.0f;
    float fit_ellipse_rx = 0.0f;
    float fit_ellipse_ry = 0.0f;
    float fit_ellipse_angle_deg = 0.0f;
    float fit_ellipse_avgdist = 0.0f;
    std::string ellipse_result_status;
    std::string ellipse_result_reason;

    int ellipse_scan_line_count = 0;
    int ellipse_scan_line_length = 0;
    int ellipse_selected_edge_index = 0;
    int ellipse_scan_lines_cross_outside_ellipse_count = 0;
    int ellipse_accepted_points_outside_ellipse_count = 0;
    double ellipse_accepted_point_norm_min = 0.0;
    double ellipse_accepted_point_norm_avg = 0.0;
    double ellipse_accepted_point_norm_max = 0.0;
    int ellipse_rejected_boundary_band_candidate_count = 0;
    double ellipse_rejected_boundary_band_norm_min = 0.0;
    double ellipse_rejected_boundary_band_norm_avg = 0.0;
    double ellipse_rejected_boundary_band_norm_max = 0.0;
    int ellipse_point_consistency_enabled = 0;
    double ellipse_point_consistency_range = 0.0;
    int ellipse_point_consistency_input_points = 0;
    int ellipse_point_consistency_output_points = 0;
    int ellipse_point_consistency_removed_points = 0;
    std::string ellipse_scan_geometry_policy;
    std::string ellipse_candidate_policy;

    // setcircle(...) 参数圆
    bool has_circle = false;
    float circle_cx = 0.0f;
    float circle_cy = 0.0f;
    // The third and fourth setcircle() arguments are a boundary point,
    // not an inner radius / radius pair.  Keep explicit coordinates for
    // ManualGaugeState synchronization; the legacy fields below remain for
    // existing diagnostic/report code.
    float circle_px = 0.0f;
    float circle_py = 0.0f;
    float circle_inner = 0.0f;
    float circle_radius = 0.0f;


    // measure / FitResultMeasure 后的测量点
    bool has_measure_points = false;
    int measure_points_count = 0;
    int valid_points_count = 0;
    std::vector<float> measure_points_xy;

    // fitcircle 后的拟合结果圆
    bool has_fit_result = false;
    float fit_cx = 0.0f;
    float fit_cy = 0.0f;
    float fit_radius = 0.0f;
    float fit_avgdist = 0.0f;

    bool has_result_measure = false;

    // 可选：用于 summary，不强依赖算法内部接口。
    int scan_path = 0;
    int image_width = 0;
    int image_height = 0;
    int back_image_width = 0;
    int back_image_height = 0;

    // Findcircle measure budget stats
    int circle_scan_lines_processed = 0;
    int circle_total_samples = 0;
    int circle_elapsed_ms = 0;
    int circle_budget_max_scan_lines = 2048;
    int circle_budget_max_samples = 2000000;
    int circle_budget_max_elapsed_ms = 3000;


    // Findline ROI center line.
    bool has_line_roi = false;
    float line_x0 = 0.0f;
    float line_y0 = 0.0f;
    float line_x1 = 0.0f;
    float line_y1 = 0.0f;
    float line_scale = 1.0f;
    std::string line_orientation;
    double line_dx = 0.0;
    double line_dy = 0.0;
    double line_length = 0.0;
    double requested_tool_half_width = 0.0;
    double effective_tool_half_width = 0.0;

    // Findline scan box / scan band.
    bool has_line_scan_box = false;
    float line_scan_half_width = 3.0f;
    int linegap = 3;
    int line_tool_wgap = 0;
    int line_tool_hgap = 0;
    std::string line_display_source;
    std::array<float, 8> line_scan_box_xy = {
        0.0f, 0.0f,
        0.0f, 0.0f,
        0.0f, 0.0f,
        0.0f, 0.0f
    };

    // Findline measure points.
    bool has_line_measure_points = false;
    std::vector<float> line_measure_points_xy;
    int line_measure_points_count = 0;
    int valid_line_points_count = 0;

    // Raw split counts, used to determine whether tool produced w/h points.
    int line_pointsw_count = 0;
    int line_pointsh_count = 0;

    // CircleRingGauge fields
    bool has_ring_gauge = false;
    double ring_outer_radius = 0.0;
    double ring_inner_radius = 0.0;
    double ring_thickness = 0.0;
    double ring_center_distance = 0.0;
    bool ring_concentric_ok = false;
    bool ring_inside_ok = false;
    bool ring_thickness_ok = false;
    double ring_score = 0.0;
    std::string ring_status;
    std::string ring_reason;
    std::string ring_result_ref;

    // FindSegmentation runtime / torch bridge visibility.
    std::string segmentation_backend;
    std::string segmentation_backend_status;
    std::string segmentation_device;
    std::string segmentation_model_path;
    std::string segmentation_result_ref;
    std::string segmentation_mask_ref;
    std::string segmentation_contour_ref;
    std::string segmentation_overlay_ref;
    std::string segmentation_reason;
    int segmentation_status_code = 0;
    int segmentation_contour_count = 0;
    double segmentation_primary_area = 0.0;
    bool segmentation_has_prompt_rect = false;
    bool segmentation_has_boundary = false;
    bool segmentation_has_libtorch_contract = false;
    bool segmentation_real_mask_attach_ready = false;

    // TorchTask runtime / artifact evidence visibility.
    bool is_torch_task = false;
    int torch_ok = 0;
    int torch_error_code = 0;
    int torch_result_count = 0;
    int torch_mask_available = 0;
    double torch_infer_ms = 0.0;
    double torch_train_ms = 0.0;
    double torch_total_ms = 0.0;
    std::string torch_status;
    std::string torch_reason;
    std::string torch_failure_stage;
    std::string torch_actual_device;
    std::string torch_result_ref;
    std::string torch_evidence_ref;
    std::string torch_primary_visual_ref;
    std::string torch_mask_ref;
    std::string torch_overlay_ref;
    std::string torch_trainer_lifecycle_summary;
    std::string torch_unified_mainline_summary;

    // Findline fit result.
    bool has_fit_line = false;
    float fit_line_x0 = 0.0f;
    float fit_line_y0 = 0.0f;
    float fit_line_x1 = 0.0f;
    float fit_line_y1 = 0.0f;
    float line_avgdist = 0.0f;

    std::string line_fit_status;
    std::string line_fit_mode;
    std::string line_measure_status;
    std::string line_measure_hint;
    std::string line_measure_failure_hint;
    bool line_filter_min_exceeds_component_p90 = false;
    std::string line_result_status;
    std::string line_result_reason;

    bool has_line_seek_points = false;
    std::vector<float> line_seek_points_xy;
    int line_seek_points_count = 0;

    int line_profile_point_count = 0;
    int line_edgeband_count = 0;
    int line_chain_length = 0;
    std::string line_measure_failure_stage;

    bool line_measure_image_ready = false;
    int line_measure_image_width = 0;
    int line_measure_image_height = 0;
    int line_measure_image_channels = 0;
    int line_measure_image_type = 0;

    bool line_measure_roi_intersects_image = false;
    bool line_measure_roi_fully_inside_image = false;

    int line_measure_method = 0;
    int line_measure_threshold = 0;
    int line_measure_linegap = 0;
    int line_measure_wgap = 0;
    int line_measure_hgap = 0;

    int line_measure_profile_count = 0;
    int line_measure_sampled_pixel_count = 0;

    double line_measure_gray_min = 0.0;
    double line_measure_gray_max = 0.0;
    double line_measure_gray_mean = 0.0;
    double line_measure_max_gradient = 0.0;

    std::string line_measure_image_source;
    std::string line_measure_input_failure_stage;
    std::string line_measure_input_detail;

    bool line_measure_fallback_allowed = false;
    bool line_measure_fallback_used = false;

    std::string line_measure_source;
    std::string line_measure_original_failure_stage;
    std::string line_measure_original_detail;

    int line_measure_original_point_count = 0;
    int line_measure_original_edgeband_count = 0;
    int line_measure_original_chain_length = 0;

    bool line_measure_geometry_request_valid = false;
    bool line_measure_geometry_dirty = false;
    bool line_measure_geometry_ready = false;

    std::uint64_t line_measure_geometry_version = 0;
    std::uint64_t line_measure_geometry_built_version = 0;

    double line_measure_geometry_half_width = 0.0;

    int line_original_scan_w_count = 0;
    int line_original_scan_h_count = 0;
    int line_original_scan_w_length = 0;
    int line_original_scan_h_length = 0;
    int line_original_process_width = 0;

    int line_scan_rows_examined = 0;
    int line_scan_rows_with_foreground = 0;
    int line_scan_runs_total = 0;
    int line_scan_runs_within_length_limit = 0;
    int line_scan_runs_over_length_limit = 0;
    int line_scan_runs_rejected_by_selection = 0;
    int line_scan_runs_rejected_near_endpoint = 0;
    int line_scan_points_emitted = 0;
    int line_point_consistency_enabled = 0;
    double line_point_consistency_range = 0.0;
    int line_point_consistency_input_points = 0;
    int line_point_consistency_output_points = 0;
    int line_point_consistency_removed_points = 0;
    int line_selected_edge_index = 0;
    int line_evaluated_edge_count = 0;
    int line_best_edge_index = 0;
    double line_best_edge_score = 0.0;
    std::vector<CxFindLineEdgeEvaluationSnapshot> line_edge_evaluations;

    bool line_measure_backimage_ready = false;
    bool line_measure_findobject_ready = false;

    int line_measure_objfilterset = 0;
    int line_measure_filter_borw = 0;
    int line_measure_filter_min = 0;
    int line_measure_filter_max = 0;

    int line_measure_filter_profile = 0;
    bool line_measure_filter_explicit = false;

    int line_measure_effective_filter_borw = 0;
    int line_measure_effective_filter_min = 0;
    int line_measure_effective_filter_max = 0;

    bool line_measure_findobject_called = false;
    bool line_measure_findobject_skipped = false;

    int line_measure_binary_foreground_pixels = 0;
    int line_measure_binary_roi_width = 0;
    int line_measure_binary_roi_height = 0;

    std::string line_measure_result_empty_reason;

    int line_findobject_component_total = 0;
    int line_findobject_component_accepted = 0;
    int line_findobject_component_rejected_by_min = 0;
    int line_findobject_component_rejected_by_max = 0;
    int line_findobject_component_rejected_by_borw = 0;

    int line_findobject_area_min_observed = 0;
    int line_findobject_area_max_observed = 0;
    double line_findobject_area_mean_observed = 0.0;
    int line_findobject_area_min = 0;
    int line_findobject_area_max = 0;
    double line_findobject_area_median = 0.0;
    double line_findobject_area_p90 = 0.0;

    std::string line_measure_cc_selected_foreground;

    int line_measure_cc_white_total = 0;
    int line_measure_cc_white_accepted = 0;
    int line_measure_cc_white_rejected_min = 0;
    double line_measure_cc_white_area_median = 0.0;
    double line_measure_cc_white_area_p90 = 0.0;

    int line_measure_cc_black_total = 0;
    int line_measure_cc_black_accepted = 0;
    int line_measure_cc_black_rejected_min = 0;
    double line_measure_cc_black_area_median = 0.0;
    double line_measure_cc_black_area_p90 = 0.0;

    int line_measure_cc_selected_total = 0;
    int line_measure_cc_selected_accepted = 0;
    int line_measure_cc_selected_rejected_min = 0;
    double line_measure_cc_selected_area_median = 0.0;
    double line_measure_cc_selected_area_p90 = 0.0;

    // Findcircle display snapshot.
    bool has_circle_roi_outer_polyline = false;
    std::vector<float> circle_roi_outer_xy;
    bool has_circle_roi_inner_polyline = false;
    std::vector<float> circle_roi_inner_xy;
    std::uint32_t circle_roi_segment_count = 0;

    bool has_fit_circle_polyline = false;
    std::vector<float> fit_circle_xy;
    std::uint32_t fit_circle_segment_count = 0;

    std::uint64_t display_version = 0;

    bool circle_measure_geometry_request_valid = false;
    bool circle_measure_geometry_dirty = false;
    bool circle_measure_geometry_ready = false;

    std::uint64_t circle_measure_geometry_version = 0;
    std::uint64_t circle_measure_geometry_built_version = 0;

    int circle_scan_line_count = 0;
    int circle_scan_line_length = 0;
    int circle_process_width = 0;
    int circle_selected_edge_index = 0;
    int circle_candidate_runs_total = 0;
    int circle_candidate_runs_max_per_line = 0;
    int circle_selected_edge_hits = 0;
    int circle_selected_edge_misses = 0;
    int circle_scan_boundary_clipped_lines = 0;
    int circle_scan_boundary_extended_samples = 0;
    int circle_candidate_boundary_reject_count = 0;
    double circle_selected_edge_radius_avg = 0.0;
    double circle_selected_edge_radius_min = 0.0;
    double circle_selected_edge_radius_max = 0.0;
    int circle_point_consistency_enabled = 0;
    double circle_point_consistency_range = 0.0;
    int circle_point_consistency_input_points = 0;
    int circle_point_consistency_output_points = 0;
    int circle_point_consistency_removed_points = 0;

    bool circle_measure_image_ready = false;
    int circle_measure_image_width = 0;
    int circle_measure_image_height = 0;
    int circle_measure_image_channels = 0;

    bool circle_measure_backimage_ready = false;
    bool circle_measure_findobject_ready = false;

    std::string circle_measure_source;
    std::string circle_measure_failure_stage;
    std::string circle_measure_detail;

    bool has_fastmatch_diagnostic = false;
    bool fastmatch_allowed = false;

    std::string fastmatch_status;
    std::string fastmatch_reason;
    std::string fastmatch_result_ref;
    std::string fastmatch_policy;
    std::string fastmatch_source_tool;
    std::string fastmatch_profile;
    std::string fastmatch_level;
    int fastmatch_model_point_count = 0;
    int fastmatch_learn_a_count = 0;
    int fastmatch_learn_b_count = 0;
    int fastmatch_learn_a2_count = 0;
    int fastmatch_learn_b2_count = 0;
    int fastmatch_learn_status_code = 0;
    int fastmatch_pattern_a_count = 0;
    int fastmatch_pattern_b_count = 0;
    int fastmatch_candidate_count = 0;
    double fastmatch_best_score = 0.0;
    int fastmatch_learn_rect_x0 = 0;
    int fastmatch_learn_rect_y0 = 0;
    int fastmatch_learn_rect_x1 = 0;
    int fastmatch_learn_rect_y1 = 0;
    int fastmatch_match_rect_x0 = 0;
    int fastmatch_match_rect_y0 = 0;
    int fastmatch_match_rect_x1 = 0;
    int fastmatch_match_rect_y1 = 0;

    bool has_grid_pattern = false;
    int grid_pattern_status_code = 0;
    int grid_pattern_active_cell_count = 0;
    int grid_pattern_descriptor_dim = 0;
    int grid_pattern_level_count = 0;
    int grid_pattern_overlay_count = 0;
    bool grid_pattern_overlay_truncated = false;
    double grid_pattern_elapsed_ms = 0.0;
    std::string grid_pattern_summary;

    bool has_region_pattern = false;
    int region_pattern_status_code = 0;
    int region_pattern_descriptor_dim = 0;
    int region_pattern_foreground_permille = 0;
    int region_pattern_mean_permille = 0;
    int region_pattern_std_permille = 0;
    int region_pattern_pooling_rows = 0;
    int region_pattern_pooling_cols = 0;
    int region_pattern_overlay_count = 0;
    bool region_pattern_overlay_truncated = false;
    double region_pattern_elapsed_ms = 0.0;
    std::string region_pattern_summary;

};

struct DebugStepSnapshot
{
  std::string script_path;
  std::string flow_block_id;
  int current_line = 0;
  std::string statement;
  std::string object;
  std::string method;
  std::string params;
  std::string runtime_state;
  std::string object_summary;
  std::string geometry_summary;
  std::string image_overlay_summary;
  std::string current_result_ref;
  std::string last_debug_result;
  std::string reason;
};

struct ResultRefView
{
    std::string name;             // global_circle_ref
    std::string value;            // runtime_object:afindcircle0
    std::string source_object;    // afindcircle0
    std::string result_type;      // FindcircleResult
    std::string status = "uninitialized";
    std::string reason;

    float fit_cx = 0.0f;
    float fit_cy = 0.0f;
    float fit_radius = 0.0f;
    float avgdist = 0.0f;

    int points_count = 0;
    int valid_points_count = 0;

    int line_no = 0;

    // Line result fields.
    float line_x0 = 0.0f;
    float line_y0 = 0.0f;
    float line_x1 = 0.0f;
    float line_y1 = 0.0f;
    float line_avgdist = 0.0f;
    int line_points_count = 0;
    int valid_line_points_count = 0;

    std::string line_result_status;
    std::string line_result_reason;
    std::string line_measure_status;
    std::string line_measure_hint;
    std::string line_measure_failure_hint;
    bool line_filter_min_exceeds_component_p90 = false;

    std::string line_measure_source;
    bool line_measure_fallback_used = false;

};

enum class GaugeHandleType
{
    None,
    LineP0,
    LineP1,
    LineCenter,
    LineWidthPlus,
    LineWidthMinus,
    CircleCenter,
    CircleRadius,
    CircleInner,
    CircleOuter
};

struct GaugeHandle
{
    GaugeHandleType type = GaugeHandleType::None;
    float screen_x = 0.0f;
    float screen_y = 0.0f;
    float radius = 6.0f;
    bool hovered = false;
    bool active = false;
};

struct ManualGaugeState
{
  std::string case_id;
  std::string image_id;
  std::string target_id;
  std::string tool = "FindLine"; // Findline / Findcircle
  std::string primary_object_type;
  std::string primary_object_name;
  std::string primary_object_status = "unresolved";

  std::string source = "manual"; // manifest / replay / ai_suggested / manual
  std::string review_status = "editing"; // editing / accepted / rejected / promoted

  bool has_line_gauge = false;
  int line_x0 = 0;
  int line_y0 = 0;
  int line_x1 = 0;
  int line_y1 = 0;
  int tool_half_width = 20;
  int wgap = 32;
  int hgap = 8;
  int scan_direction = 2; // 1=W-only, 2=H-only; Manual UI is exclusive.
  int linegap = 6;
  int threshold = 20;
  int filterprofile = 1;
  int method = 2;
  // Unified UI setting for the object prefilter switch.
  // FindLine maps this to setobjfilter(); FindCircle/FindEllipse/FindRect
  // map it to setfindsetting().  -1 means "use the tool's native default".
  int findsetting = -1;

  bool has_circle_gauge = false;
  int circle_cx = 0;
  int circle_cy = 0;
  int circle_px = 0;
  int circle_py = 0;
  int gap = 5;

  bool has_ellipse_gauge = false;
  int ellipse_x0 = 0;
  int ellipse_y0 = 0;
  int ellipse_x1 = 0;
  int ellipse_y1 = 0;

  bool has_segmentation_prompt_rect = false;
  int segmentation_prompt_x0 = 120;
  int segmentation_prompt_y0 = 120;
  int segmentation_prompt_x1 = 980;
  int segmentation_prompt_y1 = 820;
  int segmentation_mode = 2;

  int radius = 0;
  // For FindCircle, these are absolute display radii for the auxiliary
  // inner/outer Gauge rings.  They are visual/editing aids and must not be
  // confused with algorithm gap/linegap.
  int inner_radius = 0;
  int outer_radius = 0;
  bool circle_arc_enabled = false;
  int circle_arc_start_deg = 0;
  int circle_arc_end_deg = 360;

  bool dirty = false;
  bool accepted = false;
};

struct ManualParamRegressionState
{
  bool initialized = false;
  std::string status = "disabled";
  std::string reason = "Manual gauge must be accepted first.";
  CxParamRegressionTask task;
  CxParamRangeSet range_set;
  std::vector<CxParamCandidate> candidates;
  std::vector<CxParamEvalRecord> records;
  std::vector<CxParamAccuracyStats> accuracy_stats;
  int max_candidates = 12;
  int max_case_seconds = 10;
  int max_total_seconds = 60;
  int selected_candidate_index = 0;
  int edge_mode = 0; // 0 auto, 1 black-to-white, 2 white-to-black
  int contrast_percent = 20;
  int valid_length_percent = 50;
  int interference_length_percent = 20;
  int roughness = 8;
  int burr_filter_percent = 0;
  int measure_order = 3;
  int black_index = 50;
  int sample_points = 8;
  int catch_method = 0;
  bool enable_fast_measure = true;
  bool enable_filter = true;
  int tuning_tab = 0;
  std::string output_dir;
  std::string last_export_status;
  std::string last_export_reason;
  std::vector<std::string> exported_files;
};

struct ManualMetrologyUiState
{
  bool enabled = false;
  int active_tab = 0;

  // S1 behavior capture / scan profile preview.
  bool show_scan_profile = false;
  int scan_profile_source = 0; // 0 runtime, 1 gauge preview, 2 saved evidence
  int scan_profile_max_lines = 256;
  int scan_profile_sample_stride = 1;
  int scan_profile_edge_band_index = 0;
  int scan_profile_smoothing_radius = 1;

  // Candidate and feature-map review.
  bool show_edge_band_candidates = false;
  int candidate_rank = 0;
  int candidate_min_gradient = 8;
  int candidate_max_width = 80;
  int feature_map_mode = 0; // 0 gradient, 1 connected component, 2 confidence
  int feature_map_normalize = 1;

  // Surface field / area / statistics.
  int surface_source = 0; // 0 image-gray, 1 segmentation-mask, 2 synthetic
  int surface_width = 256;
  int surface_height = 256;
  int surface_stride = 1;
  int surface_z_channel = 0;
  int surface_area_method = 1; // 1 four-triangle fan
  int histogram_bins = 256;
  int histogram_mode = 0; // 0 ADF, 1 BCDF, 2 both
  bool histogram_log_scale = false;

  // Plane correction.
  bool enable_plane_correction = false;
  int plane_method = 1; // 0 three-points, 1 OLS, 2 Huber
  int plane_reference_mode = 0; // 0 whole surface, 1 ROI, 2 mask
  int plane_huber_delta_permille = 100;

  // Physical unit conversion and Z perturbation.
  int x_unit = 2; // 0 pixel, 1 nm, 2 um, 3 mm
  int y_unit = 2;
  int z_unit = 2;
  int x_scale_permille = 1000;
  int y_scale_permille = 1000;
  int z_scale_permille = 1000;
  bool enable_gaussian_z = false;
  int gaussian_z_sigma_permille = 0;
  int gaussian_seed = 42;

  // ISO 1D roughness.
  bool enable_iso_roughness_1d = false;
  int roughness_profile_axis = 0; // 0 x, 1 y, 2 selected line
  int roughness_profile_index = 0;
  int roughness_cutoff_px = 0;
  int roughness_bins = 1024;

  unsigned long long edit_revision = 0;
  std::string last_summary;
};

struct EvidenceChainThumb
{
    std::string case_id;
    std::string script_id;
    std::string script_path;
    std::string image_id;
    std::string image_path;
    std::string thumbnail_path;
    std::string target_id;
    std::string tool;

    std::string parameter_summary;
    std::string status;
    std::string reason;

    unsigned int texture_id = 0;
    int texture_w = 0;
    int texture_h = 0;
    bool texture_loaded = false;
};

struct ManualEvidenceItem
{
    std::string case_id;
    std::string level;
    std::string image_id;
    std::string target_id;
    std::string tool;

    std::string script_id;
    std::string parameter_profile_id;

    std::string gauge_status;
    std::string probe_status;
    std::string contract_status;
    std::string review_status;

    std::string image_path;
    std::string replay_package_path;
    std::string gauge_annotation_path;
};

struct ManifestImageItem
{
    std::string image_id;
    std::string image_path;
    std::string level;
    std::string status;
};

struct TorchTrainingAnnotationShapeSnapshot
{
    std::string stable_ref;
    std::string tool_id;
    std::string owner_type;
    std::string owner_ref;
    std::string owner_binding;
    std::string semantic_role;
    std::string shape_kind;
    std::string status = "editing";
    std::vector<double> points_xy;
    double center_x = 0.0;
    double center_y = 0.0;
    double radius = 0.0;
    double inner_radius = 0.0;
    double radius_x = 0.0;
    double radius_y = 0.0;
    double angle = 0.0;
    double half_width = 0.0;
    bool closed = false;
    bool editable = true;
    bool visible = true;
    bool result_element = false;
};

struct TorchTrainingImageItem
{
    std::string image_id;
    std::string image_path;
    std::string case_id;
    std::string target_id;
    std::string source = "manual"; // evidence / manifest / manual
    std::string split = "train";   // train / val / test
    std::string label = "unlabeled"; // good / anomaly / unlabeled / pending
    std::string status = "pending";

    unsigned int texture_id = 0;
    int texture_w = 0;
    int texture_h = 0;
    bool texture_loaded = false;
    bool texture_failed = false;
    bool texture_placeholder = false;

    std::string annotation_status = "unlabeled"; // unlabeled / editing / reviewed
    std::string annotation_reason;
    int annotation_shape_count = 0;
    int annotation_overlay_count = 0;
    std::vector<TorchTrainingAnnotationShapeSnapshot> annotation_shapes;
};

struct ScriptEvidenceThumb
{
    std::string candidate_id;
    std::string candidate_dir;
    std::string evidence_binding_path;
    std::string parameter_snapshot_path;
    std::string runtime_globals_path;
    std::string gauge_annotation_path;
    std::string working_script_snapshot_path;
    bool is_candidate = false;
    bool has_saved_state = false;
    std::string source_evidence_script_path;
    std::string case_id;
    std::string script_id;
    std::string script_path;
    std::string image_id;
    std::string image_path;
    std::string thumbnail_path;
    std::string target_id;
    std::string tool;
    std::string parameter_summary;
    std::string evidence_category_override;
    std::string status;
    std::string reason;
    std::string primary_object_type;
    std::string primary_object_name;
    std::string primary_object_status;
    unsigned int texture_id = 0;
    int texture_w = 0;
    int texture_h = 0;
    bool texture_loaded = false;
    bool texture_failed = false;
    bool texture_placeholder = false;
};

struct CxEvidenceEditableObjectRef
{
    std::string type;
    std::string name;
    int declared_line = 0;
};

struct ScriptEvidenceGroup
{
    std::string script_id;
    std::string script_path;
    std::string label;
    std::vector<ScriptEvidenceThumb> thumbs;
};

struct CxEvidenceSelectionSnapshot
{
    bool valid = false;

    int group_index = -1;
    int thumb_index = -1;

    std::string case_id;

    std::string candidate_id;
    std::string candidate_dir;
    std::string evidence_binding_path;
    std::string parameter_snapshot_path;
    std::string runtime_globals_path;
    std::string gauge_annotation_path;
    std::string working_script_snapshot_path;
    bool is_candidate = false;
    bool has_saved_state = false;
    std::string source_evidence_script_path;

    std::string script_id;
    std::string script_path;

    std::string image_id;
    std::string image_path;

    std::string target_id;
    std::string tool;

    std::string parameter_profile_id;
    std::string parameter_summary;

    std::string status;
    std::string reason;

    std::string source;

    std::string primary_object_type;
    std::string primary_object_name;
    std::string primary_object_status;
    std::vector<CxEvidenceEditableObjectRef> editable_objects;
};

struct ScriptEvidenceRowRef
{
    int group_index = -1;
    int thumb_index = -1;
    bool is_group_header = false;
    std::string label;
};

struct ManualFindLineEdgeParamState
{
  bool initialized = false;
  int threshold = 20;
  int method = 0;
  int linegap = 6;
  int wgap = 8;
  int hgap = 32;
  int filterprofile = 0;
};

struct ManualFindCircleEdgeParamState
{
  bool initialized = false;
  int threshold = 20;
  int method = 0;
  int linegap = 3;
  int gap = 6;
};

struct ManualTestContext
{
  std::string script_file_path;
  std::string image_file_path;
  std::string data_file_path;
  std::string model_file_path;
  std::string param_file_path;
  std::string bound_state_node_id;
  std::string bound_state_script_path;
  std::string editor_text;
  std::string analyzed_text;
  std::string editor_source = "manual";
  std::string loaded_script_path;
  std::string case_directory = "docs/notes/cxscript_case";
  std::string manual_gauge_output_root = "cxscript_runs/manual_gauge_workbench";
  std::string trace_status = "PENDING";
  std::string trace_reason = "not executed";
  std::string run_state = "idle";
  std::string debug_action = "none";
  std::string debug_status = "PENDING";
  std::string debug_reason = "not started";
  bool cxparser_ext_debug_ok = false;
  std::string cxparser_ext_debug_status;
  std::string cxparser_ext_debug_reason;
  std::string debug_parser_output;
  std::string user_expected;
  std::string codex_task;
  std::string forbidden_changes = "No coordinators, routers, UnifiedEntry, operator catalogs, automatic long-chain runs, fake PASS, Qt migration, or dev_analysis_gui business logic.";

  std::string catalog_path;
  bool catalog_loaded = false;
  std::vector<CxScriptCatalogEntry> catalog_entries;
  int current_line = 0;
  std::vector<ScriptLineView> line_views;
  std::vector<CxScriptLineView> cxparser_ext_line_views;
  std::vector<CxScriptStatementView> cxparser_ext_statement_views;
  std::vector<CxScriptObjectAssignmentView> cxparser_ext_object_assignments;
  std::vector<ScriptVariableView> global_variable_views = {
    {"Image", "global_matInput", "uninitialized", 0, "not_initialized",
     "D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/01.jpg", false}
  };
  std::vector<ScriptVariableView> variable_views;
  std::vector<ScriptObjectView> object_views;
  std::vector<RuntimeObjectView> runtime_objects;
  std::uint64_t runtime_overlay_version = 0;
  std::vector<DebugStepSnapshot> debug_snapshots;
  DebugStepSnapshot current_debug_snapshot;
  std::unordered_map<std::string, int> runtime_int_vars;
  ResultRefView current_result_ref;

  std::string geometry_summary;
  std::string image_overlay_summary;
  std::string findcircle_debug_snapshot_summary;

  std::string active_script_case_name;
  std::string active_script_case_path;
  std::string active_script_case_purpose;

  std::string active_case_id;
  std::string active_image_id;
  std::string active_target_id;

  std::string runtime_current_status = "PENDING";
  std::string runtime_current_node;
  std::string runtime_current_connect;
  bool editor_dirty = false;
  bool stop_requested = false;
  bool show_image = true;
  bool pick_points = false;
  bool test_points = false;
  bool test_rectangle = false;
  bool line_scan = false;
  bool attach_line = false;
  bool show_roi = false;
  bool show_result_overlay = false;
  bool show_line_gauge_scan_lines = false;
  bool show_circle_gauge_scan_lines = false;
  bool show_ellipse_gauge_scan_lines = false;
  int findline_selected_scan_edge = 0; // 0 = all edges, 1..N = selected edge.
  int findline_scan_edge_count = 4;
  int findline_best_fit_edge = 0; // Runtime/manual best fitting point-set edge.
  int findline_recommended_fit_edge = 0; // Future advisor/param regression recommendation.
  int findline_relation_edge = 0; // Future combined/related point-set edge.
  int findline_attach_edge = 0; // Future annotation attach/binding edge.
  bool findline_point_consistency_enabled = false;
  int findline_point_consistency_range = 0; // UI materializes default to half of search half-width.
  std::vector<ManualFindLineEdgeParamState> findline_edge_params;
  // 0 = accept all eligible crossings; 1..N = Nth candidate crossing on each
  // radial scan line.  This is not the angular A0/A1 scan-sector selection.
  int findcircle_selected_scan_edge = 0;
  int findcircle_scan_edge_count = 4;
  int findcircle_best_fit_edge = 0; // Runtime/manual best fitting arc.
  int findcircle_recommended_fit_edge = 0; // Future advisor/param regression recommendation.
  int findcircle_relation_edge = 0; // Future combined/related arc point-set.
  int findcircle_attach_edge = 0; // Future annotation attach/binding arc.
  bool findcircle_point_consistency_enabled = false;
  int findcircle_point_consistency_range = 0;
  std::vector<ManualFindCircleEdgeParamState> findcircle_edge_params;
  int findellipse_selected_scan_edge = 0;
  int findellipse_scan_edge_count = 4;
  int findellipse_best_fit_edge = 0;
  int findellipse_recommended_fit_edge = 0;
  int findellipse_relation_edge = 0;
  int findellipse_attach_edge = 0;
  bool findellipse_point_consistency_enabled = false;
  int findellipse_point_consistency_range = 0;
  std::vector<ManualFindCircleEdgeParamState> findellipse_edge_params;
  bool source_preview_enabled = false;
  int manual_elements_count = 0;
  ManualGaugeState current_gauge;
  ManualParamRegressionState param_regression;
  ManualMetrologyUiState metrology_ui;
  cxvision::metrology_analytics::ManualConsoleAnalyticsSmokeUiState
      analytics_smoke_ui;

  GaugeHandleType active_gauge_handle = GaugeHandleType::None;
  float gauge_drag_start_x = 0.0f;
  float gauge_drag_start_y = 0.0f;
  ManualGaugeState drag_start_gauge;

  std::vector<ManualEvidenceItem> evidence_items;
  bool evidence_items_seed_attempted = false;
  std::size_t evidence_items_seed_catalog_count = 0;

  bool workbench_assets_loaded = false;
  std::string manifest_path;
  bool manifest_loaded = false;
  std::string manifest_load_reason;
  std::vector<std::string> image_manifest_entries;
  std::vector<ManifestImageItem> image_manifest_items;

  std::vector<TorchTrainingImageItem> torch_training_images;
  int selected_torch_training_image = -1;
  std::string torch_training_new_image_path;
  std::string torch_training_image_status = "PENDING";
  std::string torch_training_image_reason = "training image set not initialized";

  std::vector<EvidenceChainThumb> evidence_chain_thumbs;
  int selected_evidence_thumb = -1;

  std::vector<ScriptEvidenceGroup> script_evidence_groups;
  int selected_evidence_group = -1;
  bool script_evidence_groups_dirty = true;
  unsigned long long script_evidence_groups_revision = 0;
  unsigned long long script_evidence_groups_debug_revision = 0;

  std::vector<ScriptEvidenceRowRef> script_evidence_row_refs;
  bool script_evidence_row_refs_dirty = true;

  int script_evidence_thumb_load_budget_per_frame = 4;
  int script_evidence_thumb_load_count_this_frame = 0;
  int last_evidence_click_group = -1;
  int last_evidence_click_thumb = -1;
  double last_evidence_click_time = -1.0;
  std::unordered_map<std::string, std::string> evidence_category_overrides;

  CxEvidenceSelectionSnapshot current_evidence_selection;
  std::string last_evidence_candidate_id;
  std::string last_evidence_candidate_dir;
  std::string last_evidence_candidate_reason;

  // A Save-And-Run request is a value snapshot, not a reference to the live
  // UI state.  The next ImGui frame may rebuild Evidence/Runtime views, so the
  // execution path must consume this frozen copy rather than re-reading a
  // possibly reseeded current_gauge/runtime_int_vars pair.
  bool has_pending_execution_snapshot = false;
  ManualGaugeState pending_execution_gauge;
  std::unordered_map<std::string, int> pending_execution_globals;
  std::string pending_execution_candidate_id;

  unsigned long long key_parameter_edit_revision = 0;
  std::string last_key_parameter_edit_summary;
  bool apply_gauge_to_shape_requested = false;

  CxEvidenceSelfTestResult last_evidence_selftest_result;
};

void SeedDefaultManualGlobals(
    ManualTestContext& context,
    const std::string& scriptPath);

struct ScriptSnippet
{
  std::string name;
  std::string description;
  std::string text;
  std::string source_path;
  bool runnable = true;

  std::string parameter_policy_id;
  std::string parameter_role;
  bool is_product_default = false;
  bool is_stage25_default = false;
  bool recommended = false;

  std::string script_id;
  std::string expected_result;
  std::string expected_result_status;
  std::string expected_policy_guard;
  std::string contract_path;
  std::string label;
  std::string category;
  std::string failure_hint;
  bool expects_measure_points = false;
  bool expects_fit_line = false;
  bool expected_filter_failure = false;
};

struct ManualCatalogVisibleEntry
{
    std::string script_id;
    std::string label;
    std::string path;
    std::string tool;
    std::string expected_result;
    std::string expected_policy_guard;
    std::string contract_path;
    std::string parameter_policy_id;
    std::string parameter_role;
};

struct ManualCatalogHiddenEntry
{
    std::string script_id;
    std::string label;
    std::string path;
    std::string tool;
    std::string expected_result;
    std::string hidden_reason;
    std::string contract_path;
};


struct ManualCatalogUiState
{
    bool loaded = false;
    std::string catalog_path;
    std::string load_status;
    std::string load_reason;

    std::vector<ManualCatalogVisibleEntry> visible_scripts;
    std::vector<ManualCatalogHiddenEntry> hidden_scripts;
    std::vector<ManualCatalogHiddenEntry> advanced_scripts;
};

struct DirectCapabilityMethod
{
  std::string name;
  std::string status;
};

struct DirectCapability
{
  std::string module;
  std::string type;
  std::string status;
  std::vector<DirectCapabilityMethod> methods;
};

bool UpdateRuntimeFindlineSetlineFromUi(
    ManualTestContext& context,
    const std::string& objectName,
    float x0,
    float y0,
    float x1,
    float y1,
    float scale,
    std::string& outReason);

inline float ManualGaugeDistanceSquared(
    float x1,
    float y1,
    float x2,
    float y2)
{
    const float dx = x2 - x1;
    const float dy = y2 - y1;
    return dx * dx + dy * dy;
}

struct LineGaugeGeometry
{
    ImVec2 p0;
    ImVec2 p1;
    ImVec2 center;
    ImVec2 tangent;
    ImVec2 normal;
    float length = 0.0f;
    float half_width = 0.0f;

    ImVec2 corner0;
    ImVec2 corner1;
    ImVec2 corner2;
    ImVec2 corner3;

    ImVec2 w_plus;
    ImVec2 w_minus;

    bool valid = false;
};

struct CircleGaugeGeometry
{
    ImVec2 center;
    float radius;
    float innerRadius;
    float outerRadius;

    ImVec2 radiusHandle;
    ImVec2 innerHandle;
    ImVec2 outerHandle;
};

inline CircleGaugeGeometry BuildCircleGaugeGeometry(const ManualGaugeState& gauge)
{
    CircleGaugeGeometry geo;

    geo.center = ImVec2((float)gauge.circle_cx, (float)gauge.circle_cy);

    geo.radius = (float)std::max(1, gauge.radius);

    const float fallbackBand = (float)std::max(1, gauge.linegap);
    geo.innerRadius = gauge.inner_radius > 0
        ? (float)gauge.inner_radius
        : std::max(1.0f, geo.radius - fallbackBand);
    geo.outerRadius = gauge.outer_radius > 0
        ? (float)gauge.outer_radius
        : geo.radius + fallbackBand;

    if (geo.outerRadius <= geo.innerRadius)
        geo.outerRadius = geo.innerRadius + 1.0f;

    geo.radiusHandle = ImVec2(geo.center.x + geo.radius, geo.center.y);
    geo.innerHandle = ImVec2(geo.center.x + geo.innerRadius, geo.center.y);
    geo.outerHandle = ImVec2(geo.center.x + geo.outerRadius, geo.center.y);

    return geo;
}

inline LineGaugeGeometry BuildLineGaugeGeometry(const ManualGaugeState& gauge)
{
    LineGaugeGeometry g;

    g.p0 = ImVec2((float)gauge.line_x0, (float)gauge.line_y0);
    g.p1 = ImVec2((float)gauge.line_x1, (float)gauge.line_y1);

    const float dx = g.p1.x - g.p0.x;
    const float dy = g.p1.y - g.p0.y;
    const float len = std::sqrt(dx * dx + dy * dy);

    if (len < 1.0f)
        return g;

    g.valid = true;
    g.length = len;
    g.half_width = std::max(1.0f, (float)gauge.tool_half_width);

    g.tangent = ImVec2(dx / len, dy / len);
    g.normal = ImVec2(-g.tangent.y, g.tangent.x);

    g.center = ImVec2(
        (g.p0.x + g.p1.x) * 0.5f,
        (g.p0.y + g.p1.y) * 0.5f);

    g.corner0 = ImVec2(g.p0.x + g.normal.x * g.half_width,
                       g.p0.y + g.normal.y * g.half_width);
    g.corner1 = ImVec2(g.p1.x + g.normal.x * g.half_width,
                       g.p1.y + g.normal.y * g.half_width);
    g.corner2 = ImVec2(g.p1.x - g.normal.x * g.half_width,
                       g.p1.y - g.normal.y * g.half_width);
    g.corner3 = ImVec2(g.p0.x - g.normal.x * g.half_width,
                       g.p0.y - g.normal.y * g.half_width);

    g.w_plus = ImVec2(g.center.x + g.normal.x * g.half_width,
                      g.center.y + g.normal.y * g.half_width);
    g.w_minus = ImVec2(g.center.x - g.normal.x * g.half_width,
                       g.center.y - g.normal.y * g.half_width);

    return g;
}

inline GaugeHandleType HitTestGaugeHandle(
    const ManualGaugeState& gauge,
    float mouse_x,
    float mouse_y,
    float handle_radius)
{
    const float r2 = handle_radius * handle_radius;

    if (gauge.tool == "Findcircle" || gauge.has_circle_gauge)
    {
        if (!gauge.has_circle_gauge)
            return GaugeHandleType::None;

        if (ManualGaugeDistanceSquared(
                (float)gauge.circle_cx,
                (float)gauge.circle_cy,
                mouse_x,
                mouse_y) <= r2)
        {
            return GaugeHandleType::CircleCenter;
        }

        const CircleGaugeGeometry geom = BuildCircleGaugeGeometry(gauge);

        if (ManualGaugeDistanceSquared(
                geom.radiusHandle.x,
                geom.radiusHandle.y,
                mouse_x,
                mouse_y) <= r2)
        {
            return GaugeHandleType::CircleRadius;
        }

        if (ManualGaugeDistanceSquared(
                geom.innerHandle.x,
                geom.innerHandle.y,
                mouse_x,
                mouse_y) <= r2)
        {
            return GaugeHandleType::CircleInner;
        }

        if (ManualGaugeDistanceSquared(
                geom.outerHandle.x,
                geom.outerHandle.y,
                mouse_x,
                mouse_y) <= r2)
        {
            return GaugeHandleType::CircleOuter;
        }

        return GaugeHandleType::None;
    }

    if (!gauge.has_line_gauge)
        return GaugeHandleType::None;

    LineGaugeGeometry geom = BuildLineGaugeGeometry(gauge);
    if (!geom.valid)
        return GaugeHandleType::None;

    if (ManualGaugeDistanceSquared(geom.p0.x, geom.p0.y, mouse_x, mouse_y) <= r2)
        return GaugeHandleType::LineP0;

    if (ManualGaugeDistanceSquared(geom.p1.x, geom.p1.y, mouse_x, mouse_y) <= r2)
        return GaugeHandleType::LineP1;

    if (ManualGaugeDistanceSquared(geom.center.x, geom.center.y, mouse_x, mouse_y) <= r2)
        return GaugeHandleType::LineCenter;

    if (ManualGaugeDistanceSquared(geom.w_plus.x, geom.w_plus.y, mouse_x, mouse_y) <= r2)
        return GaugeHandleType::LineWidthPlus;

    if (ManualGaugeDistanceSquared(geom.w_minus.x, geom.w_minus.y, mouse_x, mouse_y) <= r2)
        return GaugeHandleType::LineWidthMinus;

    return GaugeHandleType::None;
}

inline int ClampCircleRadiusToImage(
    int cx,
    int cy,
    int radius,
    int imageW,
    int imageH)
{
    const int maxR = std::min(
        std::min(cx, imageW - 1 - cx),
        std::min(cy, imageH - 1 - cy));
    return std::clamp(radius, 1, maxR);
}

inline void DragGaugeHandle(
    ManualGaugeState& gauge,
    const ManualGaugeState& drag_start_gauge,
    GaugeHandleType handle,
    const ImVec2& mouse_image_pos,
    const ImVec2& drag_start_mouse_image,
    bool shift_down,
    int imageW = 0,
    int imageH = 0)
{
    if (gauge.tool == "Findcircle" || gauge.has_circle_gauge)
    {
        if (!gauge.has_circle_gauge)
            return;

        switch (handle)
        {
        case GaugeHandleType::CircleCenter:
        {
            const float dx = mouse_image_pos.x - drag_start_mouse_image.x;
            const float dy = mouse_image_pos.y - drag_start_mouse_image.y;
            gauge.circle_cx = drag_start_gauge.circle_cx + static_cast<int>(std::round(dx));
            gauge.circle_cy = drag_start_gauge.circle_cy + static_cast<int>(std::round(dy));
            gauge.circle_px = drag_start_gauge.circle_px + static_cast<int>(std::round(dx));
            gauge.circle_py = drag_start_gauge.circle_py + static_cast<int>(std::round(dy));
            break;
        }
        case GaugeHandleType::CircleRadius:
        {
            float dx = mouse_image_pos.x - (float)drag_start_gauge.circle_cx;
            float dy = mouse_image_pos.y - (float)drag_start_gauge.circle_cy;
            int r = std::max(1, static_cast<int>(std::round(std::sqrt(dx * dx + dy * dy))));

            if (imageW > 0 && imageH > 0)
                r = ClampCircleRadiusToImage(drag_start_gauge.circle_cx, drag_start_gauge.circle_cy, r, imageW, imageH);

            gauge.radius = r;
            gauge.circle_px = drag_start_gauge.circle_cx + r;
            gauge.circle_py = drag_start_gauge.circle_cy;

            break;
        }
        case GaugeHandleType::CircleInner:
        {
            float dx = mouse_image_pos.x - (float)drag_start_gauge.circle_cx;
            float dy = mouse_image_pos.y - (float)drag_start_gauge.circle_cy;
            int r = static_cast<int>(std::round(std::sqrt(dx * dx + dy * dy)));

            if (imageW > 0 && imageH > 0)
                r = ClampCircleRadiusToImage(drag_start_gauge.circle_cx, drag_start_gauge.circle_cy, r, imageW, imageH);

            const CircleGaugeGeometry startGeom =
                BuildCircleGaugeGeometry(drag_start_gauge);
            r = std::min(
                r,
                std::max(1, static_cast<int>(std::round(startGeom.outerRadius)) - 1));
            gauge.inner_radius = std::max(1, r);
            break;
        }
        case GaugeHandleType::CircleOuter:
        {
            float dx = mouse_image_pos.x - (float)drag_start_gauge.circle_cx;
            float dy = mouse_image_pos.y - (float)drag_start_gauge.circle_cy;
            int r = static_cast<int>(std::round(std::sqrt(dx * dx + dy * dy)));

            if (imageW > 0 && imageH > 0)
                r = ClampCircleRadiusToImage(drag_start_gauge.circle_cx, drag_start_gauge.circle_cy, r, imageW, imageH);

            const CircleGaugeGeometry startGeom =
                BuildCircleGaugeGeometry(drag_start_gauge);
            r = std::max(
                r,
                static_cast<int>(std::round(startGeom.innerRadius)) + 1);
            gauge.outer_radius = std::max(1, r);
            break;
        }
        default:
            break;
        }
    }
    else
    {
        if (!gauge.has_line_gauge)
            return;

        switch (handle)
        {
        case GaugeHandleType::LineP0:
            gauge.line_x0 = static_cast<int>(std::round(mouse_image_pos.x));
            gauge.line_y0 = static_cast<int>(std::round(mouse_image_pos.y));
            gauge.line_x1 = drag_start_gauge.line_x1;
            gauge.line_y1 = drag_start_gauge.line_y1;
            break;

        case GaugeHandleType::LineP1:
            gauge.line_x0 = drag_start_gauge.line_x0;
            gauge.line_y0 = drag_start_gauge.line_y0;
            gauge.line_x1 = static_cast<int>(std::round(mouse_image_pos.x));
            gauge.line_y1 = static_cast<int>(std::round(mouse_image_pos.y));
            break;

        case GaugeHandleType::LineCenter:
        {
            const float dx = mouse_image_pos.x - drag_start_mouse_image.x;
            const float dy = mouse_image_pos.y - drag_start_mouse_image.y;

            gauge.line_x0 = drag_start_gauge.line_x0 + static_cast<int>(std::round(dx));
            gauge.line_y0 = drag_start_gauge.line_y0 + static_cast<int>(std::round(dy));
            gauge.line_x1 = drag_start_gauge.line_x1 + static_cast<int>(std::round(dx));
            gauge.line_y1 = drag_start_gauge.line_y1 + static_cast<int>(std::round(dy));
            break;
        }

        case GaugeHandleType::LineWidthPlus:
        case GaugeHandleType::LineWidthMinus:
        {
            LineGaugeGeometry geom = BuildLineGaugeGeometry(drag_start_gauge);
            if (!geom.valid)
                return;

            const float fromCenterX = mouse_image_pos.x - geom.center.x;
            const float fromCenterY = mouse_image_pos.y - geom.center.y;
            const float signedDistance = fromCenterX * geom.normal.x + fromCenterY * geom.normal.y;
            const int newHalfWidth = std::max(1, static_cast<int>(std::round(std::abs(signedDistance))));
            gauge.tool_half_width = newHalfWidth;

            gauge.line_x0 = drag_start_gauge.line_x0;
            gauge.line_y0 = drag_start_gauge.line_y0;
            gauge.line_x1 = drag_start_gauge.line_x1;
            gauge.line_y1 = drag_start_gauge.line_y1;
            break;
        }

        default:
            break;
        }
    }

    gauge.dirty = true;
    gauge.accepted = false;
    gauge.review_status = "editing";
}

static constexpr const char* kCxImageCatalogPath =
    "cxparser/cxscript/module/cximage/catalog/cximage_catalog.cxsc";

#endif
