#ifndef CXIMAGE_MANUAL_STATE_TEST_CONSOLE_H
#define CXIMAGE_MANUAL_STATE_TEST_CONSOLE_H

#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <iomanip>
#include <array>
#include <cstdint>
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


    // Findline ROI center line.
    bool has_line_roi = false;
    float line_x0 = 0.0f;
    float line_y0 = 0.0f;
    float line_x1 = 0.0f;
    float line_y1 = 0.0f;
    float line_scale = 1.0f;

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
    std::string name;             // global.circle_ref
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

    std::string line_measure_source;
    bool line_measure_fallback_used = false;

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
  std::string trace_status = "PENDING";
  std::string trace_reason = "not executed";
  std::string run_state = "idle";
  std::string debug_action = "none";
  std::string debug_status = "PENDING";
  std::string debug_reason = "not started";
  std::string debug_parser_output;
  std::string user_expected;
  std::string codex_task;
  std::string forbidden_changes = "No coordinators, routers, UnifiedEntry, operator catalogs, automatic long-chain runs, fake PASS, Qt migration, or dev_analysis_gui business logic.";
  int current_line = 0;
  std::vector<ScriptLineView> line_views;
  std::vector<ScriptVariableView> global_variable_views = {
    {"Image", "global.matInput", "uninitialized", 0, "not_initialized",
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
};

struct ScriptSnippet
{
  std::string name;
  std::string description;
  std::string text;
  std::string source_path;
  bool runnable = true;
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

#endif
