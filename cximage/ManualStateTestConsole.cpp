#include "ViewController.h"

#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <ctime>
#include <sstream>

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
fs::path ResolveWorkspaceFile(const std::string& path)
{
  const fs::path requested(path);
  if (requested.is_absolute() && fs::exists(requested)) return requested;
  if (fs::exists(requested)) return fs::absolute(requested);
  fs::path current = fs::current_path();
  while (!current.empty())
  {
    const fs::path direct = current / requested;
    const fs::path nested = current / "cxvisionai" / "cxvision_repo" / requested;
    if (fs::exists(direct)) return fs::absolute(direct);
    if (fs::exists(nested)) return fs::absolute(nested);
    const fs::path parent = current.parent_path();
    if (parent == current) break;
    current = parent;
  }
  return requested;
}

fs::path ResolveCaseDirectory(const std::string& path)
{
  const fs::path requested(path);
  if (requested.is_absolute()) return requested;
  fs::path current = fs::current_path();
  while (!current.empty())
  {
    const fs::path roots[] = {current, current / "cxvisionai" / "cxvision_repo"};
    for (const fs::path& root : roots)
      if (fs::exists(root / "CMakeLists.txt") && fs::exists(root / "cximage") && fs::exists(root / "cxparser"))
        return root / requested;
    const fs::path parent = current.parent_path();
    if (parent == current) break;
    current = parent;
  }
  return requested;
}

std::string TrimLine(const std::string& text)
{
  const std::size_t first = text.find_first_not_of(" \t\r");
  if (first == std::string::npos) return std::string();
  const std::size_t last = text.find_last_not_of(" \t\r");
  return text.substr(first, last - first + 1);
}

std::string JsonEscape(const std::string& text)
{
  std::ostringstream out;
  for (const char ch : text)
  {
    if (ch == '\\' || ch == '"') out << '\\' << ch;
    else if (ch == '\n') out << "\\n";
    else if (ch == '\r') out << "\\r";
    else if (ch == '\t') out << "\\t";
    else out << ch;
  }
  return out.str();
}

std::string CurrentTimestamp()
{
  const std::time_t now = std::time(nullptr);
  std::tm local_time = {};
#if defined(_WIN32)
  localtime_s(&local_time, &now);
#else
  localtime_r(&now, &local_time);
#endif
  char buffer[32] = {};
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &local_time);
  return buffer;
}

std::string ModuleForType(const std::string& type)
{
  if (type.rfind("Torch", 0) == 0) return "torch";
  if (type.rfind("Mlpack", 0) == 0) return "mlpack";
  if (type.rfind("Ensmallen", 0) == 0) return "ensmallen";
  if (type == "Image" || type.rfind("Find", 0) == 0 || type == "fastmatch" ||
      type == "FormfitGauge" || type == "CxOverlay") return "cximage";
  return "cxscript";
}

std::string ModuleForStatement(const std::string& statement)
{
  if (statement.find("torch.") != std::string::npos || statement.find("Torch") != std::string::npos) return "torch";
  if (statement.find("mlpack.") != std::string::npos || statement.find("Mlpack") != std::string::npos) return "mlpack";
  if (statement.find("ensmallen.") != std::string::npos || statement.find("Ensmallen") != std::string::npos) return "ensmallen";
  if (statement.find("cximage.") != std::string::npos || statement.find("Image") != std::string::npos ||
      statement.find("Find") != std::string::npos || statement.find("fastmatch") != std::string::npos) return "cximage";
  return "cxscript";
}

bool IsObjectType(const std::string& type)
{
  return ModuleForType(type) != "cxscript";
}

void AnalyzeScript(ManualTestContext& context)
{
  if (context.analyzed_text == context.editor_text) return;
  context.analyzed_text = context.editor_text;
  context.line_views.clear();
  context.variable_views.clear();
  context.object_views.clear();
  context.current_line = 0;

  std::istringstream input(context.editor_text);
  std::string raw;
  int line_no = 1;
  while (std::getline(input, raw))
  {
    ScriptLineView line;
    line.line_no = line_no++;
    line.statement = raw;
    const std::string statement = TrimLine(raw);
    line.module = ModuleForStatement(statement);

    std::istringstream tokens(statement);
    std::string declared_type;
    std::string declared_name;
    tokens >> declared_type >> declared_name;
    const bool declaration = !declared_type.empty() && !declared_name.empty() &&
      statement.find('(') == std::string::npos && declared_type != "if" &&
      declared_type != "else" && declared_type != "return";
    if (declaration)
    {
      const std::size_t suffix = declared_name.find_first_of("=;");
      if (suffix != std::string::npos) declared_name.erase(suffix);
      line.object_type = declared_type;
      line.object = declared_name;
      if (IsObjectType(declared_type))
      {
        line.module = ModuleForType(declared_type);
        context.object_views.push_back({line.module, declared_type, declared_name,
                                        line.module == "cximage" ? "declared" : "pending_binding",
                                        line.line_no});
      }
      else
      {
        const std::size_t equal = statement.find('=');
        const std::string value = equal == std::string::npos ? "uninitialized" :
          TrimLine(statement.substr(equal + 1, statement.size() - equal - 2));
        context.variable_views.push_back({declared_type, declared_name, value,
                                          line.line_no, "observed_source"});
      }
    }

    const std::size_t assign = statement.find('=');
    const std::size_t open = statement.find('(');
    const std::size_t close = statement.rfind(')');
    const bool has_assignment = assign != std::string::npos &&
      (open == std::string::npos || assign < open);
    const std::size_t callable_start = has_assignment ? assign + 1 : 0;
    if (has_assignment) line.return_variable = TrimLine(statement.substr(0, assign));
    if (open != std::string::npos)
    {
      const std::string callable = TrimLine(statement.substr(callable_start, open - callable_start));
      const std::size_t dot = callable.rfind('.');
      if (dot != std::string::npos)
      {
        line.object = TrimLine(callable.substr(0, dot));
        line.method = TrimLine(callable.substr(dot + 1));
      }
      else line.method = callable;
      if (close != std::string::npos && close > open)
        line.params = statement.substr(open + 1, close - open - 1);
    }
    context.line_views.push_back(line);
  }
  context.trace_status = "PENDING";
  context.trace_reason = "source analyzed; runtime line callbacks unavailable";
}

void SetTraceStatus(ManualTestContext& context,
                    const std::string& status,
                    const std::string& reason)
{
  AnalyzeScript(context);
  const std::string timestamp = CurrentTimestamp();
  for (ScriptLineView& line : context.line_views)
  {
    if (TrimLine(line.statement).empty()) continue;
    line.status = status;
    line.reason = reason;
    line.timestamp = timestamp;
  }
  context.trace_status = status;
  context.trace_reason = reason;
}

bool WriteTextFile(const fs::path& path, const std::string& text)
{
  std::ofstream output(path, std::ios::binary);
  if (!output) return false;
  output << text;
  return output.good();
}

bool SaveCasePackage(const ManualTestContext& context,
                     const std::string& result_status,
                     const std::string& result_reason,
                     const std::string& result_ref,
                     const std::string& evidence_ref,
                     const std::vector<std::string>& log_lines,
                     std::string& reason)
{
  std::error_code error;
  const fs::path root = ResolveCaseDirectory(context.case_directory);
  fs::create_directories(root, error);
  if (error) { reason = "case directory create failed"; return false; }

  std::ostringstream global_context;
  global_context << "{\n"
    << "  \"script_path\": \"" << JsonEscape(context.loaded_script_path) << "\",\n"
    << "  \"image_file_path\": \"" << JsonEscape(context.image_file_path) << "\",\n"
    << "  \"data_file_path\": \"" << JsonEscape(context.data_file_path) << "\",\n"
    << "  \"model_file_path\": \"" << JsonEscape(context.model_file_path) << "\",\n"
    << "  \"param_file_path\": \"" << JsonEscape(context.param_file_path) << "\",\n"
    << "  \"current_status\": \"" << JsonEscape(result_status) << "\"\n}\n";

  std::ostringstream trace;
  trace << "[\n";
  for (std::size_t i = 0; i < context.line_views.size(); ++i)
  {
    const ScriptLineView& line = context.line_views[i];
    trace << "  {\"line_no\":" << line.line_no
      << ",\"statement\":\"" << JsonEscape(line.statement)
      << "\",\"module\":\"" << JsonEscape(line.module)
      << "\",\"object\":\"" << JsonEscape(line.object)
      << "\",\"method\":\"" << JsonEscape(line.method)
      << "\",\"params\":\"" << JsonEscape(line.params)
      << "\",\"return_variable\":\"" << JsonEscape(line.return_variable)
      << "\",\"status\":\"" << JsonEscape(line.status)
      << "\",\"reason\":\"" << JsonEscape(line.reason)
      << "\",\"timestamp\":\"" << JsonEscape(line.timestamp) << "\"}"
      << (i + 1 == context.line_views.size() ? "\n" : ",\n");
  }
  trace << "]\n";

  std::ostringstream variables;
  variables << "[\n";
  for (std::size_t i = 0; i < context.variable_views.size(); ++i)
  {
    const ScriptVariableView& variable = context.variable_views[i];
    variables << "  {\"type\":\"" << JsonEscape(variable.type)
      << "\",\"name\":\"" << JsonEscape(variable.name)
      << "\",\"value\":\"" << JsonEscape(variable.value)
      << "\",\"declared_line\":" << variable.declared_line
      << ",\"status\":\"" << JsonEscape(variable.status) << "\"}"
      << (i + 1 == context.variable_views.size() ? "\n" : ",\n");
  }
  variables << "]\n";

  std::ostringstream objects;
  objects << "[\n";
  for (std::size_t i = 0; i < context.object_views.size(); ++i)
  {
    const ScriptObjectView& object = context.object_views[i];
    objects << "  {\"module\":\"" << JsonEscape(object.module)
      << "\",\"type\":\"" << JsonEscape(object.type)
      << "\",\"name\":\"" << JsonEscape(object.name)
      << "\",\"status\":\"" << JsonEscape(object.status)
      << "\",\"declared_line\":" << object.declared_line << "}"
      << (i + 1 == context.object_views.size() ? "\n" : ",\n");
  }
  objects << "]\n";

  std::ostringstream result;
  result << "{\n  \"status\": \"" << JsonEscape(result_status)
    << "\",\n  \"reason\": \"" << JsonEscape(result_reason)
    << "\",\n  \"result_ref\": \"" << JsonEscape(result_ref) << "\"\n}\n";
  std::ostringstream evidence;
  evidence << "{\n  \"status\": \"" << (evidence_ref.empty() ? "PENDING" : "AVAILABLE")
    << "\",\n  \"evidence_ref\": \"" << JsonEscape(evidence_ref)
    << "\",\n  \"reason\": \""
    << (evidence_ref.empty() ? "no real runtime result package" : "runtime evidence attached")
    << "\"\n}\n";
  std::ostringstream log;
  for (const std::string& line : log_lines) log << line << '\n';

  const bool saved =
    WriteTextFile(root / "global_context.json", global_context.str()) &&
    WriteTextFile(root / "script_snapshot.cxsc", context.editor_text) &&
    WriteTextFile(root / "line_trace.json", trace.str()) &&
    WriteTextFile(root / "variable_snapshot.json", variables.str()) &&
    WriteTextFile(root / "object_state.json", objects.str()) &&
    WriteTextFile(root / "result.json", result.str()) &&
    WriteTextFile(root / "evidence.json", evidence.str()) &&
    WriteTextFile(root / "log.txt", log.str());
  reason = saved ? "complete eight-file case package saved" : "one or more case files failed to save";
  return saved;
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

  m_directTestModules.clear();
  const fs::path moduleRoot = ResolveWorkspaceFile("cxparser/cxscript/module");
  if (fs::exists(moduleRoot) && fs::is_directory(moduleRoot))
  {
    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(moduleRoot))
    {
      if (!entry.is_regular_file() || entry.path().extension() != ".cxsc" ||
          entry.path().filename().string().find("direct_test") == std::string::npos)
        continue;
      std::string text;
      if (!ReadTextFile(entry.path().generic_string(), text)) continue;
      const std::string relative = fs::relative(entry.path(), moduleRoot).generic_string();
      m_directTestModules.push_back({relative,
        "C/C++ statement-level direct test module.", text,
        "cxparser/cxscript/module/" + relative, true});
    }
    std::sort(m_directTestModules.begin(), m_directTestModules.end(),
      [](const ScriptSnippet& left, const ScriptSnippet& right)
      { return left.source_path < right.source_path; });
  }
}

void ViewController::LoadBoundStateToManualConsole(
  const std::string& nodeId, const std::string& scriptPath)
{
  m_manualTest.bound_state_node_id = nodeId;
  m_manualTest.bound_state_script_path = scriptPath;
  m_manualTest.editor_source = "bound_state";
  m_manualTest.loaded_script_path = scriptPath;
  m_manualTest.editor_dirty = false;
  if (!ReadTextFile(ResolveWorkspaceFile(scriptPath).generic_string(), m_manualTest.editor_text))
  {
    m_scriptResult = ScriptResult();
    m_scriptResult.source = "bound_state";
    m_scriptResult.script_path = scriptPath;
    m_scriptResult.status = "FAIL";
    m_scriptResult.reason = "script file not found";
    m_scriptResult.runtime_fillback_status = "not_started";
  }
  else
  {
    m_manualTest.analyzed_text.clear();
    m_manualTest.current_line = 0;
    m_scriptResult.status = "PENDING";
    m_scriptResult.reason = "bound script loaded; runtime not executed";
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
  ImGui::Text("Builtin Parser Snippets");
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
      m_manualTest.analyzed_text.clear();
      m_manualTest.current_line = 0;
    }
    ImGui::TextWrapped("%s", snippet.description.c_str());
    ImGui::PopID();
  }

  ImGui::Separator();
  ImGui::Text("Direct Test Modules");
  for (std::size_t i = 0; i < m_directTestModules.size(); ++i)
  {
    const ScriptSnippet& module = m_directTestModules[i];
    ImGui::PushID(1000 + static_cast<int>(i));
    if (ImGui::Selectable(module.name.c_str()))
    {
      m_manualTest.editor_text = module.text;
      m_manualTest.editor_source = "direct_test_module";
      m_manualTest.loaded_script_path = module.source_path;
      m_manualTest.script_file_path = module.source_path;
      m_manualTest.editor_dirty = false;
      m_manualTest.analyzed_text.clear();
      m_manualTest.current_line = 0;
      m_scriptResult.status = "PENDING";
      m_scriptResult.reason = "direct test module loaded; runtime not executed";
      m_scriptResult.runtime_fillback_status = "not_started";
    }
    ImGui::TextWrapped("%s", module.source_path.c_str());
    ImGui::PopID();
  }
  if (m_directTestModules.empty())
    ImGui::TextDisabled("No direct_test .cxsc modules found.");
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

  AnalyzeScript(m_manualTest);
  if (m_manualTest.current_line >= static_cast<int>(m_manualTest.line_views.size()))
    m_manualTest.current_line = m_manualTest.line_views.empty() ? 0 :
      static_cast<int>(m_manualTest.line_views.size()) - 1;

  ImGui::Separator();
  ImGui::Text("CxScript Line View");
  ImGui::Text("trace status: %s", m_manualTest.trace_status.c_str());
  ImGui::TextWrapped("trace reason: %s", m_manualTest.trace_reason.c_str());
  if (ImGui::Button("Previous Line") && m_manualTest.current_line > 0)
    --m_manualTest.current_line;
  ImGui::SameLine();
  if (ImGui::Button("Next Line") &&
      m_manualTest.current_line + 1 < static_cast<int>(m_manualTest.line_views.size()))
    ++m_manualTest.current_line;
  ImGui::SameLine();
  ImGui::Text("highlight line: %d", m_manualTest.line_views.empty() ? 0 :
              m_manualTest.line_views[static_cast<std::size_t>(m_manualTest.current_line)].line_no);

  ImGui::BeginChild("cxscript_line_view", ImVec2(0.0f, 220.0f), true);
  for (std::size_t i = 0; i < m_manualTest.line_views.size(); ++i)
  {
    const ScriptLineView& line = m_manualTest.line_views[i];
    const std::string label = std::to_string(line.line_no) + "  [" + line.status + "]  " + line.statement;
    if (ImGui::Selectable(label.c_str(), m_manualTest.current_line == static_cast<int>(i)))
      m_manualTest.current_line = static_cast<int>(i);
  }
  ImGui::EndChild();

  if (!m_manualTest.line_views.empty())
  {
    const ScriptLineView& current =
      m_manualTest.line_views[static_cast<std::size_t>(m_manualTest.current_line)];
    ImGui::Text("line_no: %d | status: %s", current.line_no, current.status.c_str());
    ImGui::TextWrapped("statement: %s", current.statement.c_str());
    ImGui::Text("module: %s | object: %s | method: %s",
                current.module.c_str(), current.object.c_str(), current.method.c_str());
    ImGui::TextWrapped("params: %s", current.params.empty() ? "(none)" : current.params.c_str());
    ImGui::TextWrapped("return variable: %s",
                       current.return_variable.empty() ? "(none)" : current.return_variable.c_str());
    ImGui::TextWrapped("reason: %s | timestamp: %s", current.reason.c_str(),
                       current.timestamp.empty() ? "(none)" : current.timestamp.c_str());
  }

  ImGui::Separator();
  ImGui::Text("Variable Snapshot (%d)", static_cast<int>(m_manualTest.variable_views.size()));
  for (const ScriptVariableView& variable : m_manualTest.variable_views)
    ImGui::BulletText("%s %s = %s", variable.type.c_str(), variable.name.c_str(), variable.value.c_str());

  ImGui::Separator();
  ImGui::Text("Object State Panels");
  const char* modules[] = {"cximage", "torch", "mlpack", "ensmallen"};
  ImGui::Columns(4, "direct_capability_panels", false);
  for (const char* module : modules)
  {
    ImGui::Text("%s", module);
    bool found = false;
    for (const ScriptObjectView& object : m_manualTest.object_views)
    {
      if (object.module != module) continue;
      ImGui::BulletText("%s %s: %s", object.type.c_str(), object.name.c_str(), object.status.c_str());
      found = true;
    }
    if (!found) ImGui::TextDisabled("no object in current script");
    if (std::string(module) == "cximage")
    {
      ImGui::TextDisabled("image / ROI / fit / match / overlay");
    }
    else if (std::string(module) == "torch")
    {
      ImGui::TextWrapped("model_path: %s", m_manualTest.model_file_path.empty() ? "pending_input" : m_manualTest.model_file_path.c_str());
      ImGui::TextDisabled("device / tensor shape / raw shape / mask / overlay: pending_runtime");
    }
    else if (std::string(module) == "mlpack")
    {
      ImGui::TextDisabled("feature shape / dataset shape / prediction / score: pending_runtime");
    }
    else
    {
      ImGui::TextDisabled("objective / param_space / candidate / metric / best / history: pending_runtime");
    }
    ImGui::NextColumn();
  }
  ImGui::Columns(1);

  InputTextString("Case directory", m_manualTest.case_directory);
  if (ImGui::Button("Save Complete Case Package"))
  {
    std::string save_reason;
    const bool saved = SaveCasePackage(m_manualTest,
                                       m_scriptResult.status.empty() ? "PENDING" : m_scriptResult.status,
                                       m_scriptResult.reason,
                                       m_scriptResult.result_ref,
                                       m_scriptResult.evidence_ref,
                                       m_scriptResult.log_lines,
                                       save_reason);
    m_scriptResult.status = saved ? "PENDING" : "FAIL";
    m_scriptResult.reason = save_reason;
  }
  ImGui::Separator();
  const OverlayElement* selectedOverlay = m_annotationLayer.Selected();
  ImGui::Text("selected_element_ref: %s",
              selectedOverlay == nullptr ? "(none)" : selectedOverlay->ref.c_str());
  ImGui::Text("selected_roi_ref: %s", m_annotationLayer.SelectedRef(OverlayKind::Rect).c_str());
  ImGui::Text("selected_point_ref: %s", m_annotationLayer.SelectedRef(OverlayKind::Point).c_str());
  ImGui::Text("selected_scan_line_ref: %s", m_annotationLayer.SelectedRef(OverlayKind::Line).c_str());
  ImGui::Text("selected_circle_ref: %s", m_annotationLayer.SelectedRef(OverlayKind::Circle).c_str());
  ImGui::Text("selected_polyline_ref: %s", m_annotationLayer.SelectedRef(OverlayKind::Polyline).c_str());
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
    SetTraceStatus(m_manualTest,
                   m_manualTest.editor_text.empty() ? "BLOCKED" : "PENDING",
                   m_scriptResult.reason);
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
      SetTraceStatus(m_manualTest, compiled ? "PENDING" : "FAIL",
                     compiled ? "statement callback unavailable; runtime result package pending" :
                                "parser compile failed; exact statement unavailable");
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