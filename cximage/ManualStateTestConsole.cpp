#include "ViewController.h"

#include <imgui.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace
{
namespace fs = std::filesystem;

int StringResizeCallback(ImGuiInputTextCallbackData* data)
{
  if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
  {
    std::string* value = static_cast<std::string*>(data->UserData);
    value->resize(static_cast<std::size_t>(data->BufTextLen));
    data->Buf = value->data();
  }
  return 0;
}

bool InputTextString(const char* label, std::string& value)
{
  if (value.capacity() < 256) value.reserve(256);
  return ImGui::InputText(label, value.data(), value.capacity() + 1,
                          ImGuiInputTextFlags_CallbackResize,
                          StringResizeCallback, &value);
}

bool InputTextMultilineString(const char* label, std::string& value,
                              const ImVec2& size)
{
  if (value.capacity() < 4096) value.reserve(4096);
  return ImGui::InputTextMultiline(label, value.data(), value.capacity() + 1,
                                   size,
                                   ImGuiInputTextFlags_CallbackResize |
                                   ImGuiInputTextFlags_AllowTabInput,
                                   StringResizeCallback, &value);
}

bool ReadTextFile(const std::string& path, std::string& text)
{
  std::ifstream stream(fs::path(path), std::ios::binary);
  if (!stream) return false;
  text.assign(std::istreambuf_iterator<char>(stream),
              std::istreambuf_iterator<char>());
  return true;
}
}

void ViewController::initManualStateTestConsole()
{
  m_manualTest.image_file_path =
    "D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/01.jpg";
  m_manualSnippets = {
    {"Parser Run 1", "Image and shape visibility test.",
     "aimage1.Show(1);\nashape0.Show(1);\n", "builtin", true},
    {"Parser Run 2", "Pattern model setup fragment.",
     "amatch0.setmatchrect(50,50,2200,1900);\n", "builtin", true},
    {"Parser Run 3", "Image ROI threshold fragment.",
     "aimage1.roieasythre(255);\naimage1.Show(1);\n", "builtin", true},
    {"Parser Run 4", "Point and line inspection fragment.",
     "apoints0.Show(1);\nafindline.Show(1);\n", "builtin", true},
    {"Parser Run 5", "Manual runtime call fragment.",
     "arun.testrun();\n", "builtin", true},
    {"Parser Run 6", "Empty integration observation fragment.",
     "# enter one manual integration statement\n", "builtin", true},
    {"Custom Manual Text", "Start with an empty manual editor.",
     "", "manual", true}
  };
}

void ViewController::LoadBoundStateToManualConsole(
  const std::string& nodeId, const std::string& scriptPath)
{
  m_manualTest.bound_state_node_id = nodeId;
  m_manualTest.bound_state_script_path = scriptPath;
  m_manualTest.editor_source = "bound_state";
  m_manualTest.loaded_script_path = scriptPath;
  m_manualTest.editor_dirty = false;
  if (!ReadTextFile(scriptPath, m_manualTest.editor_text))
  {
    m_scriptResult = ScriptResult();
    m_scriptResult.source = "bound_state";
    m_scriptResult.script_path = scriptPath;
    m_scriptResult.status = "FAIL";
    m_scriptResult.reason = "script file not found";
    m_scriptResult.runtime_fillback_status = "not_started";
  }
}

void ViewController::drawManualStateTestConsole()
{
  if (!m_showManualStateTestConsole) return;

  ImGui::SetNextWindowPos(ImVec2(70, 45), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(980, 720), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Manual State Test Console",
                    &m_showManualStateTestConsole))
  {
    ImGui::End();
    return;
  }

  ImGui::Text("Input Source");
  InputTextString("Script file path", m_manualTest.script_file_path);
  InputTextString("Image file path", m_manualTest.image_file_path);
  InputTextString("Data file path", m_manualTest.data_file_path);
  InputTextString("Model file path", m_manualTest.model_file_path);
  InputTextString("Param file path", m_manualTest.param_file_path);
  InputTextString("Bound state node id", m_manualTest.bound_state_node_id);
  InputTextString("Bound state script path", m_manualTest.bound_state_script_path);

  if (ImGui::Button("Load Script File"))
  {
    if (ReadTextFile(m_manualTest.script_file_path, m_manualTest.editor_text))
    {
      m_manualTest.editor_source = "file";
      m_manualTest.loaded_script_path = m_manualTest.script_file_path;
      m_manualTest.editor_dirty = false;
    }
    else
    {
      m_scriptResult = ScriptResult();
      m_scriptResult.source = "file";
      m_scriptResult.script_path = m_manualTest.script_file_path;
      m_scriptResult.status = "FAIL";
      m_scriptResult.reason = "script file not found";
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Load Image File"))
  {
    cv::Mat image = cv::imread(m_manualTest.image_file_path);
    if (image.empty())
    {
      m_scriptResult.status = "FAIL";
      m_scriptResult.reason = "image file not found or unreadable";
    }
    else
    {
      UpdateImageViewImage(image);
      m_scriptResult.image_ref = m_manualTest.image_file_path;
      m_scriptResult.status = "PENDING";
      m_scriptResult.reason = "image loaded; no runtime result package";
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Load Data File"))
  {
    m_scriptResult.status = fs::exists(m_manualTest.data_file_path) ? "PENDING" : "FAIL";
    m_scriptResult.reason = fs::exists(m_manualTest.data_file_path) ?
      "data file selected; runtime not connected" : "data file not found";
  }
  ImGui::SameLine();
  if (ImGui::Button("Load Model File"))
  {
    m_scriptResult.status = fs::exists(m_manualTest.model_file_path) ? "PENDING" : "FAIL";
    m_scriptResult.reason = fs::exists(m_manualTest.model_file_path) ?
      "model file selected; runtime not connected" : "model file not found";
  }
  ImGui::SameLine();
  if (ImGui::Button("Clear Inputs"))
  {
    m_manualTest = ManualTestContext();
  }

  ImGui::Separator();
  ImGui::Columns(2, "manual_console_columns", true);
  ImGui::Text("Snippet List");
  for (std::size_t i = 0; i < m_manualSnippets.size(); ++i)
  {
    const ScriptSnippet& snippet = m_manualSnippets[i];
    ImGui::PushID(static_cast<int>(i));
    if (ImGui::Selectable(snippet.name.c_str()))
    {
      m_manualTest.editor_text = snippet.text;
      m_manualTest.editor_source = "snippet";
      m_manualTest.loaded_script_path = snippet.source_path;
      m_manualTest.editor_dirty = false;
    }
    ImGui::TextWrapped("%s", snippet.description.c_str());
    ImGui::PopID();
  }
  ImGui::TextDisabled("rag_script_cases: semantic_reference_only / not runnable");

  ImGui::NextColumn();
  ImGui::Text("Script Editor");
  if (InputTextMultilineString("##manual_script_editor",
                               m_manualTest.editor_text,
                               ImVec2(-1.0f, 190.0f)))
  {
    m_manualTest.editor_dirty = true;
    if (m_manualTest.editor_source.empty())
      m_manualTest.editor_source = "manual";
  }
  ImGui::Text("editor_dirty: %s", m_manualTest.editor_dirty ? "true" : "false");
  ImGui::Text("editor_source: %s", m_manualTest.editor_source.c_str());
  ImGui::TextWrapped("loaded_script_path: %s",
                     m_manualTest.loaded_script_path.empty() ? "(none)" :
                     m_manualTest.loaded_script_path.c_str());
  ImGui::Columns(1);

  ImGui::Separator();
  ImGui::Text("Overlay Options");
  ImGui::Checkbox("Show Image", &m_manualTest.show_image); ImGui::SameLine();
  ImGui::Checkbox("Pick Points", &m_manualTest.pick_points); ImGui::SameLine();
  ImGui::Checkbox("Test Points", &m_manualTest.test_points); ImGui::SameLine();
  ImGui::Checkbox("Test Rectangle", &m_manualTest.test_rectangle);
  ImGui::Checkbox("Line Scan", &m_manualTest.line_scan); ImGui::SameLine();
  ImGui::Checkbox("Attach Line", &m_manualTest.attach_line); ImGui::SameLine();
  ImGui::Checkbox("Show ROI", &m_manualTest.show_roi); ImGui::SameLine();
  ImGui::Checkbox("Show Result Overlay", &m_manualTest.show_result_overlay);
  m_ipickpoints = m_manualTest.pick_points;
  m_ilinescan = m_manualTest.line_scan;
  m_iattachline = m_manualTest.attach_line;
  m_showTestPoints = m_manualTest.test_points;
  m_showTestRectangle = m_manualTest.test_rectangle || m_manualTest.show_roi;
  m_showTestScanLine = m_manualTest.line_scan || m_manualTest.attach_line;
  if (!m_manualTest.show_result_overlay)
    m_scriptResult.overlay_ref.clear();

  ImGui::Separator();
  ImGui::Text("Run");
  if (ImGui::Button("Parse Only"))
  {
    m_scriptResult = ScriptResult();
    m_scriptResult.source = m_manualTest.editor_source;
    m_scriptResult.script_path = m_manualTest.loaded_script_path;
    m_scriptResult.status = m_manualTest.editor_text.empty() ? "BLOCKED" : "PENDING";
    m_scriptResult.reason = m_manualTest.editor_text.empty() ?
      "editor text is empty" : "parse validation pending real parser result";
    m_scriptResult.runtime_fillback_status = "not_started";
  }
  ImGui::SameLine();
  if (ImGui::Button("Run Text"))
  {
    m_scriptResult = ScriptResult();
    m_scriptResult.source = "text";
    m_scriptResult.script_path = m_manualTest.loaded_script_path;
    const auto start = std::chrono::steady_clock::now();
    if (m_manualTest.editor_text.empty())
    {
      m_scriptResult.status = "BLOCKED";
      m_scriptResult.reason = "editor text is empty";
    }
    else
    {
      clearos();
      const bool compiled = m_imageparser.Compile(m_manualTest.editor_text.c_str());
      m_scriptResult.status = compiled ? "PENDING" : "FAIL";
      m_scriptResult.reason = compiled ?
        "text executed; runtime result package unavailable" : "parser compile failed";
      if (!getoutputstring().empty())
        m_scriptResult.log_lines.push_back(getoutputstring());
    }
    const auto end = std::chrono::steady_clock::now();
    m_scriptResult.elapsed_ms =
      std::chrono::duration<double, std::milli>(end - start).count();
    m_scriptResult.runtime_fillback_status = "pending_real_runtime_fillback";
  }
  ImGui::SameLine();
  if (ImGui::Button("Run File"))
  {
    m_scriptResult = RunCxScript(m_manualTest.script_file_path);
    m_scriptResult.source = "file";
  }
  ImGui::SameLine();
  if (ImGui::Button("Run Bound State"))
  {
    m_scriptResult = RunCxScript(m_manualTest.bound_state_script_path);
    m_scriptResult.source = "bound_state";
  }
  ImGui::SameLine();
  if (ImGui::Button("Clear Result"))
  {
    m_scriptResult = ScriptResult();
    m_scriptResult.status = "PENDING";
    m_scriptResult.reason = "result cleared";
    m_scriptResult.runtime_fillback_status = "not_started";
  }

  ImGui::Separator();
  ImGui::Text("Output");
  ImGui::TextWrapped("source: %s", m_scriptResult.source.empty() ? "(none)" : m_scriptResult.source.c_str());
  ImGui::TextWrapped("script_path: %s", m_scriptResult.script_path.empty() ? "(none)" : m_scriptResult.script_path.c_str());
  ImGui::Text("status: %s", m_scriptResult.status.empty() ? "(none)" : m_scriptResult.status.c_str());
  ImGui::TextWrapped("reason: %s", m_scriptResult.reason.empty() ? "(none)" : m_scriptResult.reason.c_str());
  ImGui::Text("runtime_fillback_status: %s", m_scriptResult.runtime_fillback_status.empty() ? "(none)" : m_scriptResult.runtime_fillback_status.c_str());
  ImGui::Text("elapsed_ms: %.3f", m_scriptResult.elapsed_ms);
  ImGui::TextWrapped("image_ref: %s", m_scriptResult.image_ref.empty() ? "(none)" : m_scriptResult.image_ref.c_str());
  ImGui::TextWrapped("overlay_ref: %s", m_scriptResult.overlay_ref.empty() ? "none" : m_scriptResult.overlay_ref.c_str());
  ImGui::TextWrapped("result_ref: %s", m_scriptResult.result_ref.empty() ? "(none)" : m_scriptResult.result_ref.c_str());
  ImGui::TextWrapped("evidence_ref: %s", m_scriptResult.evidence_ref.empty() ? "(none)" : m_scriptResult.evidence_ref.c_str());
  ImGui::TextWrapped("issue_entry_ref: %s", m_scriptResult.issue_entry_ref.empty() ? "(none)" : m_scriptResult.issue_entry_ref.c_str());
  ImGui::Text("overlay_status: %s", m_scriptResult.overlay_ref.empty() ? "pending / unavailable" : "available");
  for (const std::string& line : m_scriptResult.log_lines)
    ImGui::BulletText("%s", line.c_str());

  ImGui::End();
}