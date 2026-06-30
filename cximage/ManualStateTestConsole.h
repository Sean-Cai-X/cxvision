#ifndef CXIMAGE_MANUAL_STATE_TEST_CONSOLE_H
#define CXIMAGE_MANUAL_STATE_TEST_CONSOLE_H

#include <string>
#include <vector>

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
  std::string editor_source = "manual";
  std::string loaded_script_path;
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

#endif