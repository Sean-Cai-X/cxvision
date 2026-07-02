#ifndef CXIMAGE_MANUAL_STATE_TEST_CONSOLE_H
#define CXIMAGE_MANUAL_STATE_TEST_CONSOLE_H

#include <string>
#include <vector>
#include <unordered_map>

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
    std::vector<float> measure_points_xy;

    // fitcircle 后的拟合结果圆
    bool has_fit_result = false;
    float fit_cx = 0.0f;
    float fit_cy = 0.0f;
    float fit_radius = 0.0f;
    float fit_avgdist = 0.0f;

    bool has_result_measure = false;
};

struct DebugStepSnapshot
{
  int current_line = 0;
  std::string statement;
  std::string object;
  std::string method;
  std::string params;
  std::string runtime_state;
  std::string object_summary;
  std::string geometry_summary;
  std::string image_overlay_summary;
  std::string reason;
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
  std::vector<DebugStepSnapshot> debug_snapshots;
  DebugStepSnapshot current_debug_snapshot;
  std::unordered_map<std::string, int> runtime_int_vars;
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

#endif
