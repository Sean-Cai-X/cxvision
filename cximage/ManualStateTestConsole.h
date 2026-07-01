#ifndef CXIMAGE_MANUAL_STATE_TEST_CONSOLE_H
#define CXIMAGE_MANUAL_STATE_TEST_CONSOLE_H

#include <string>
#include <vector>

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
};

struct ScriptObjectView
{
  std::string module;
  std::string type;
  std::string name;
  std::string status;
  int declared_line = 0;
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
  std::string user_expected;
  std::string codex_task;
  std::string forbidden_changes = "No coordinators, routers, UnifiedEntry, operator catalogs, automatic long-chain runs, fake PASS, Qt migration, or dev_analysis_gui business logic.";
  int current_line = 0;
  std::vector<ScriptLineView> line_views;
  std::vector<ScriptVariableView> variable_views;
  std::vector<ScriptObjectView> object_views;
  bool editor_dirty = false;
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