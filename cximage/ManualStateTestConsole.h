#ifndef CXIMAGE_MANUAL_STATE_TEST_CONSOLE_H
#define CXIMAGE_MANUAL_STATE_TEST_CONSOLE_H

#include "ParserDebugBridge.h"
#include "CxScriptCatalogRuntime.h"
#include "CxParamRegressionRuntime.h"
#include "CxScriptHeadlessRuntime.h"

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
  std::string tool = "Findline"; // Findline / Findcircle

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
  int linegap = 6;
  int threshold = 20;
  int filterprofile = 1;
  int method = 2;

  bool has_circle_gauge = false;
  int circle_cx = 0;
  int circle_cy = 0;
  int circle_px = 0;
  int circle_py = 0;
  int gap = 5;

  int radius = 0;
  int inner_radius = 0;
  int outer_radius = 0;

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

struct EvidenceChainThumb
{
    std::string case_id;
    std::string script_id;
    std::string script_path;
    std::string image_id;
    std::string image_path;
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

struct ScriptEvidenceThumb
{
    std::string case_id;
    std::string script_id;
    std::string script_path;
    std::string image_id;
    std::string image_path;
    std::string target_id;
    std::string tool;
    std::string parameter_summary;
    std::string status;
    std::string reason;
    unsigned int texture_id = 0;
    int texture_w = 0;
    int texture_h = 0;
    bool texture_loaded = false;
    bool texture_failed = false;
};

struct ScriptEvidenceGroup
{
    std::string script_id;
    std::string script_path;
    std::string label;
    std::vector<ScriptEvidenceThumb> thumbs;
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
  bool source_preview_enabled = false;
  int manual_elements_count = 0;
  ManualGaugeState current_gauge;
  ManualParamRegressionState param_regression;

  GaugeHandleType active_gauge_handle = GaugeHandleType::None;
  float gauge_drag_start_x = 0.0f;
  float gauge_drag_start_y = 0.0f;
  ManualGaugeState drag_start_gauge;

  std::vector<ManualEvidenceItem> evidence_items;

  bool workbench_assets_loaded = false;
  std::string manifest_path;
  bool manifest_loaded = false;
  std::string manifest_load_reason;
  std::vector<std::string> image_manifest_entries;
  std::vector<ManifestImageItem> image_manifest_items;

  std::vector<EvidenceChainThumb> evidence_chain_thumbs;
  int selected_evidence_thumb = -1;

  std::vector<ScriptEvidenceGroup> script_evidence_groups;
  int selected_evidence_group = -1;
  bool script_evidence_groups_dirty = true;
};

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

    if (gauge.inner_radius > 0)
        geo.innerRadius = (float)gauge.inner_radius;
    else
        geo.innerRadius = std::max(1.0f, geo.radius - (float)std::max(1, gauge.gap));

    if (gauge.outer_radius > 0)
        geo.outerRadius = (float)gauge.outer_radius;
    else
        geo.outerRadius = geo.radius + (float)std::max(1, gauge.gap);

    if (geo.innerRadius >= geo.radius)
        geo.innerRadius = std::max(1.0f, geo.radius - 5.0f);

    if (geo.outerRadius <= geo.radius)
        geo.outerRadius = geo.radius + 5.0f;

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

        int effective_radius = gauge.radius;
        if (effective_radius <= 0)
            effective_radius = gauge.gap > 0 ? gauge.gap : 50;

        if (ManualGaugeDistanceSquared(
                (float)gauge.circle_cx + (float)effective_radius,
                (float)gauge.circle_cy,
                mouse_x,
                mouse_y) <= r2)
        {
            return GaugeHandleType::CircleRadius;
        }

        if (gauge.inner_radius > 0 &&
            ManualGaugeDistanceSquared(
                (float)gauge.circle_cx + (float)gauge.inner_radius,
                (float)gauge.circle_cy,
                mouse_x,
                mouse_y) <= r2)
        {
            return GaugeHandleType::CircleInner;
        }

        if (gauge.outer_radius > 0 &&
            ManualGaugeDistanceSquared(
                (float)gauge.circle_cx + (float)gauge.outer_radius,
                (float)gauge.circle_cy,
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
            gauge.gap = r;
            gauge.circle_px = drag_start_gauge.circle_cx + r;
            gauge.circle_py = drag_start_gauge.circle_cy;

            if (gauge.inner_radius >= gauge.radius)
                gauge.inner_radius = std::max(1, gauge.radius - std::max(1, gauge.gap));

            if (gauge.outer_radius <= gauge.radius)
                gauge.outer_radius = gauge.radius + std::max(1, gauge.gap);

            break;
        }
        case GaugeHandleType::CircleInner:
        {
            float dx = mouse_image_pos.x - (float)drag_start_gauge.circle_cx;
            float dy = mouse_image_pos.y - (float)drag_start_gauge.circle_cy;
            int r = static_cast<int>(std::round(std::sqrt(dx * dx + dy * dy)));

            if (imageW > 0 && imageH > 0)
                r = ClampCircleRadiusToImage(drag_start_gauge.circle_cx, drag_start_gauge.circle_cy, r, imageW, imageH);

            r = std::min(r, drag_start_gauge.radius - 1);
            r = std::min(r, drag_start_gauge.outer_radius - 1);

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

            r = std::max(r, drag_start_gauge.radius + 1);
            r = std::max(r, drag_start_gauge.inner_radius + 1);

            gauge.outer_radius = r;
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
