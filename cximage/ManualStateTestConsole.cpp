#include "ViewController.h"
#include "Findcircle.h"

#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <cctype>
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

std::vector<std::string> SplitParameters(const std::string& text)
{
  std::vector<std::string> result;
  std::istringstream input(text);
  std::string value;
  while (std::getline(input, value, ',')) result.push_back(TrimLine(value));
  return result;
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
    line.status = "source_analyzed";
    line.reason = "not_executed";
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
                                        line.module == "cximage" ? "declared" : "pending_runtime",
                                        std::string(), 0, line.line_no});
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

bool StepCurrentLine(ManualTestContext& context)
{
  AnalyzeScript(context);
  if (context.current_line < 0 ||
      context.current_line >= static_cast<int>(context.line_views.size()))
  {
    context.run_state = "finished";
    return false;
  }
  ScriptLineView& line = context.line_views[
    static_cast<std::size_t>(context.current_line)];
  line.status = "source_analyzed";
  line.reason = "not_executed";
  context.trace_status = "source_analyzed";
  context.trace_reason = "not_executed";
  ++context.current_line;
  if (context.current_line >= static_cast<int>(context.line_views.size()))
    context.run_state = "finished";
  return false;
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
                     const std::vector<OverlayElement>& image_elements,
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
      << "\",\"runtime_state\":\"" << JsonEscape(object.runtime_state)
      << "\",\"runtime_source_line\":" << object.runtime_source_line
      << ",\"declared_line\":" << object.declared_line << "}"
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

  const ScriptLineView* current = nullptr;
  if (context.current_line >= 0 &&
      context.current_line < static_cast<int>(context.line_views.size()))
    current = &context.line_views[static_cast<std::size_t>(context.current_line)];
  std::ostringstream debug_request;
  debug_request << "{\n"
    << "  \"module\": \"" << JsonEscape(current == nullptr ? "" : current->module) << "\",\n"
    << "  \"script_path\": \"" << JsonEscape(context.loaded_script_path) << "\",\n"
    << "  \"line_no\": " << (current == nullptr ? 0 : current->line_no) << ",\n"
    << "  \"statement\": \"" << JsonEscape(current == nullptr ? "" : current->statement) << "\",\n"
    << "  \"object\": \"" << JsonEscape(current == nullptr ? "" : current->object) << "\",\n"
    << "  \"method\": \"" << JsonEscape(current == nullptr ? "" : current->method) << "\",\n"
    << "  \"params\": \"" << JsonEscape(current == nullptr ? "" : current->params) << "\",\n"
    << "  \"current_status\": \"" << JsonEscape(result_status) << "\",\n"
    << "  \"current_reason\": \"" << JsonEscape(result_reason) << "\",\n"
    << "  \"user_expected\": \"" << JsonEscape(context.user_expected) << "\",\n"
    << "  \"codex_task\": \"" << JsonEscape(context.codex_task) << "\",\n"
    << "  \"forbidden_changes\": \"" << JsonEscape(context.forbidden_changes) << "\"\n}\n";

  std::ostringstream image_elements_json;
  image_elements_json << "{\n  \"elements\": [\n";
  for (std::size_t i = 0; i < image_elements.size(); ++i)
  {
    const OverlayElement& element = image_elements[i];
    std::string element_type = ImageAnnotationLayer::KindName(element.kind);
    std::transform(element_type.begin(), element_type.end(), element_type.begin(),
      [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    image_elements_json << "    {\n"
      << "      \"id\": \"" << JsonEscape(element.ref) << "\",\n"
      << "      \"type\": \"" << element_type << "\",\n"
      << "      \"role\": \"" << JsonEscape(element.role) << "\",\n"
      << "      \"source\": \"" << JsonEscape(element.source) << "\",\n"
      << "      \"module_hint\": \"" << JsonEscape(element.module_hint) << "\",\n"
      << "      \"visible\": " << (element.visible ? "true" : "false") << ",\n"
      << "      \"points\": [";
    for (std::size_t point = 0; point < element.image_points.size(); ++point)
    {
      image_elements_json << "[" << element.image_points[point].x << ","
                          << element.image_points[point].y << "]"
                          << (point + 1 == element.image_points.size() ? "" : ",");
    }
    image_elements_json << "],\n"
      << "      \"radius\": " << element.radius << ",\n"
      << "      \"generated_statement\": \""
      << JsonEscape(element.generated_statement) << "\",\n"
      << "      \"evidence_ref\": \"" << JsonEscape(element.evidence_ref) << "\"\n"
      << "    }" << (i + 1 == image_elements.size() ? "\n" : ",\n");
  }
  image_elements_json << "  ]\n}\n";

  const bool saved =
    WriteTextFile(root / "global_context.json", global_context.str()) &&
    WriteTextFile(root / "script_snapshot.cxsc", context.editor_text) &&
    WriteTextFile(root / "line_trace.json", trace.str()) &&
    WriteTextFile(root / "variable_snapshot.json", variables.str()) &&
    WriteTextFile(root / "object_state.json", objects.str()) &&
    WriteTextFile(root / "image_elements.json", image_elements_json.str()) &&
    WriteTextFile(root / "debug_request.json", debug_request.str()) &&
    WriteTextFile(root / "result.json", result.str()) &&
    WriteTextFile(root / "evidence.json", evidence.str()) &&
    WriteTextFile(root / "log.txt", log.str());
  reason = saved ? "complete collaborative debug case package saved" :
                   "one or more case files failed to save";
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

  struct CapabilitySeed { const char* module; const char* type; };
  const CapabilitySeed seeds[] = {
    {"cximage", "Image"}, {"cximage", "Findcircle"},
    {"cximage", "Findline"}, {"cximage", "fastmatch"},
    {"torch", "TorchSegModel"}, {"torch", "TorchTensor"},
    {"torch", "TorchRawOutput"}, {"torch", "TorchMask"},
    {"mlpack", "MlpackFeature"}, {"mlpack", "MlpackDataset"},
    {"mlpack", "MlpackLogRegModel"}, {"mlpack", "MlpackPrediction"},
    {"mlpack", "MlpackScore"},
    {"ensmallen", "EnsmallenObjective"},
    {"ensmallen", "EnsmallenParamSpace"},
    {"ensmallen", "EnsmallenOptimizer"},
    {"ensmallen", "EnsmallenCandidate"},
    {"ensmallen", "EnsmallenMetric"},
    {"ensmallen", "EnsmallenBestParam"}
  };
  m_directCapabilities.clear();
  for (const CapabilitySeed& seed : seeds)
  {
    DirectCapability capability;
    capability.module = seed.module;
    capability.type = seed.type;
    bool declaredByScript = false;
    for (const ScriptSnippet& snippet : m_directTestModules)
    {
      ManualTestContext analyzed;
      analyzed.editor_text = snippet.text;
      AnalyzeScript(analyzed);
      for (const ScriptObjectView& object : analyzed.object_views)
      {
        if (object.type != capability.type) continue;
        declaredByScript = true;
        for (const ScriptLineView& line : analyzed.line_views)
        {
          if (line.object != object.name || line.method.empty()) continue;
          const bool known = std::any_of(capability.methods.begin(), capability.methods.end(),
            [&](const DirectCapabilityMethod& method) { return method.name == line.method; });
          if (!known) capability.methods.push_back({line.method,
            capability.module == "cximage" ? "registered" : "pending_binding"});
        }
      }
    }
    capability.status = capability.module == "cximage" ? "registered" :
      (declaredByScript ? "script_only" : "pending_binding");
    m_directCapabilities.push_back(capability);
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

bool ViewController::QueryParserObjectExists(const std::string& type,
                                                   const std::string& name)
{
  if (m_imageparser.GetClassObj(type, name) != nullptr) return true;
  if (type == "Findcircle")
    return m_imageparser.GetClassObj("findcircle", name) != nullptr;
  return false;
}

Image* ViewController::QueryParserImage(const std::string& name)
{
  return static_cast<Image*>(m_imageparser.GetClassObj("Image", name));
}

bool ViewController::QueryParserDouble(const std::string& name, double& value)
{
  if (!m_imageparser.IsObjectVar(name.c_str())) return false;
  double* runtimeValue = static_cast<double*>(m_imageparser.GetDoubleValue(name));
  if (runtimeValue == nullptr) return false;
  value = *runtimeValue;
  return true;
}

bool ViewController::SetParserDouble(const std::string& name, double value)
{
  if (!m_imageparser.IsObjectVar(name.c_str())) return false;
  const std::string statement = name + "=" + std::to_string(value) + ";";
  return m_imageparser.Compile(statement.c_str());
}

void ViewController::RefreshRuntimeObjectTable(const std::string& lastMethod,
                                               const std::string& runtimeStatus)
{
  m_manualTest.runtime_objects.clear();
  auto addObject = [&](const std::string& type, const std::string& name)
  {
    RuntimeObjectView entry;
    entry.name = name;
    entry.type = type;
    entry.exists_in_parser = QueryParserObjectExists(type, name);
    entry.last_runtime_status = entry.exists_in_parser ? runtimeStatus : "PENDING";
    entry.last_method = lastMethod;
    entry.stale = !entry.exists_in_parser ||
      (runtimeStatus != "runtime_executed" && runtimeStatus != "runtime_queried");
    entry.display_summary = entry.exists_in_parser ? "runtime object available" :
                                                     "not found in parser";
    if (type == "Image" && entry.exists_in_parser)
    {
      Image* image = QueryParserImage(name);
      if (image != nullptr)
      {
        const cv::Mat& mat = image->getmat();
        entry.display_summary = "runtime image " + std::to_string(mat.cols) +
          "x" + std::to_string(mat.rows);
        if (!mat.empty())
        {
          UpdateImageViewImage(mat);
          m_scriptResult.image_ref = "runtime_object:" + name;
        }
      }
    }
    else if (type == "Findcircle" && entry.exists_in_parser)
    {
      Findcircle* circle = static_cast<Findcircle*>(
        m_imageparser.GetClassObj(type, name));
      if (circle == nullptr)
        circle = static_cast<Findcircle*>(
          m_imageparser.GetClassObj("findcircle", name));
      if (circle != nullptr)
      {
        entry.has_circle = true;
        entry.circle_cx = static_cast<float>(circle->getcirclecentx());
        entry.circle_cy = static_cast<float>(circle->getcirclecenty());
        entry.circle_inner = static_cast<float>(circle->getcirclepax());
        entry.circle_radius = static_cast<float>(circle->getcirclepay());
        entry.display_summary = "circle=" + std::to_string(circle->getcirclecentx()) +
          "," + std::to_string(circle->getcirclecenty()) + "," +
          std::to_string(circle->getcirclepax()) + "," +
          std::to_string(circle->getcirclepay());
      }
    }
    m_manualTest.runtime_objects.push_back(entry);
  };
  addObject("Image", "m_occtimage");
  addObject("Findcircle", "afindcircle0");
  addObject("Findcircle", "afindcircle1");

  RuntimeObjectView output;
  output.name = "doutputvalue";
  output.type = "double";
  double outputValue = 0.0;
  output.exists_in_parser = QueryParserDouble(output.name, outputValue);
  output.last_runtime_status = output.exists_in_parser ? runtimeStatus : "PENDING";
  output.last_method = lastMethod;
  output.stale = !output.exists_in_parser ||
    (runtimeStatus != "runtime_executed" && runtimeStatus != "runtime_queried");
  output.display_summary = output.exists_in_parser ? std::to_string(outputValue) :
                                                    "not found in parser";
  m_manualTest.runtime_objects.push_back(output);

  double currentStatus = 0.0;
  if (QueryParserDouble("current_status", currentStatus))
    m_manualTest.runtime_current_status = std::to_string(currentStatus);
  else m_manualTest.runtime_current_status = "PENDING";
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

  if (ImGui::Button("Demo: Debug find_circle_direct_test"))
  {
    const std::string target =
      "cxparser/cxscript/module/cximage/find_circle_direct_test.cxsc";
    const auto module = std::find_if(m_directTestModules.begin(),
      m_directTestModules.end(), [&](const ScriptSnippet& snippet)
      { return snippet.source_path == target; });
    if (module == m_directTestModules.end())
    {
      m_scriptResult.status = "FAIL";
      m_scriptResult.reason = "find_circle_direct_test.cxsc not found";
    }
    else
    {
      const cv::Mat image = cv::imread(m_manualTest.image_file_path);
      if (!image.empty())
      {
        UpdateImageViewImage(image);
        m_scriptResult.image_ref = m_manualTest.image_file_path;
      }
      m_manualTest.editor_text = module->text;
      m_manualTest.editor_source = "debug_demo";
      m_manualTest.loaded_script_path = module->source_path;
      m_manualTest.script_file_path = module->source_path;
      m_manualTest.editor_dirty = false;
      m_manualTest.analyzed_text.clear();
      m_manualTest.current_line = 0;
      m_manualTest.show_image = true;
      AnalyzeScript(m_manualTest);
      SetTraceStatus(m_manualTest, "source_analyzed", "not_executed");
      m_scriptResult.status = "PENDING";
      m_scriptResult.reason =
        "runtime line callbacks unavailable; runtime not connected";
      m_scriptResult.runtime_fillback_status = "pending_real_runtime_fillback";
    }
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
  ImGui::Text("Script Debug Compiler");
  ImGui::Text("run_state: %s", m_manualTest.run_state.c_str());
  if (ImGui::Button("Compile"))
  {
    m_manualTest.analyzed_text.clear();
    AnalyzeScript(m_manualTest);
    const bool compiled = !m_manualTest.editor_text.empty() &&
      m_imageparser.Compile(m_manualTest.editor_text.c_str());
    m_manualTest.current_line = 0;
    m_manualTest.run_state = compiled ? "runtime_compiled" : "blocked";
    m_scriptResult.status = compiled ? "PENDING" : "BLOCKED";
    m_scriptResult.reason = compiled ?
      "parser Compile completed; runtime objects queried; no PASS inferred" :
      "parser Compile failed or editor text is empty";
    m_scriptResult.runtime_fillback_status = compiled ? "runtime_objects_queried" :
                                                        "not_started";
    RefreshRuntimeObjectTable("Compile", compiled ? "runtime_executed" : "BLOCKED");
  }
  ImGui::SameLine();
  if (ImGui::Button("Run"))
  {
    const bool ran = !m_manualTest.editor_text.empty() &&
      m_imageparser.Compile(m_manualTest.editor_text.c_str());
    m_manualTest.run_state = ran ? "runtime_executed" : "blocked";
    m_scriptResult.status = ran ? "PENDING" : "BLOCKED";
    m_scriptResult.reason = ran ?
      "parser runtime executed; object table refreshed; result status remains PENDING" :
      "parser runtime execution failed";
    m_scriptResult.runtime_fillback_status = ran ? "runtime_objects_queried" :
                                                   "not_started";
    RefreshRuntimeObjectTable("Run", ran ? "runtime_executed" : "BLOCKED");
  }
  ImGui::SameLine();
  if (ImGui::Button("Step"))
  {
    AnalyzeScript(m_manualTest);
    if (m_manualTest.current_line >= 0 &&
        m_manualTest.current_line < static_cast<int>(m_manualTest.line_views.size()))
    {
      const ScriptLineView& line = m_manualTest.line_views[
        static_cast<std::size_t>(m_manualTest.current_line)];
      const std::string statement = TrimLine(line.statement);
      const bool executable = !statement.empty() && statement[0] != '#';
      const bool stepped = !executable || m_imageparser.Compile(statement.c_str());
      m_scriptResult.status = stepped ? "PENDING" : "BLOCKED";
      m_scriptResult.reason = stepped ?
        "parser step applied; runtime objects queried; no PASS inferred" :
        "parser rejected current statement";
      m_manualTest.run_state = stepped ? "runtime_step" : "blocked";
      RefreshRuntimeObjectTable(line.method,
        stepped && executable ? "runtime_executed" : "PENDING");
      if (stepped) ++m_manualTest.current_line;
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Continue"))
  {
    m_scriptResult.status = "PENDING";
    m_scriptResult.reason = "automatic long-chain continue disabled; use Step or Run";
    m_manualTest.run_state = "PENDING";
  }
  ImGui::SameLine();
  if (ImGui::Button("Stop"))
  {
    m_imageparser.StopRun();
    m_manualTest.stop_requested = true;
    m_manualTest.run_state = "stopped";
    m_scriptResult.status = "PENDING";
    m_scriptResult.reason = "parser runtime stopped by user";
  }
  ImGui::SameLine();
  if (ImGui::Button("Reset"))
  {
    m_imageparser.ResetRun();
    m_manualTest.analyzed_text.clear();
    AnalyzeScript(m_manualTest);
    m_manualTest.current_line = 0;
    m_manualTest.runtime_objects.clear();
    m_manualTest.run_state = "idle";
    m_scriptResult.status = "PENDING";
    m_scriptResult.reason = "runtime debug state reset; source preserved";
    m_scriptResult.runtime_fillback_status = "not_started";
  }
  if (ImGui::Button("Run File (runtime bridge)"))
    m_scriptResult = RunCxScript(m_manualTest.script_file_path);
  ImGui::SameLine();
  if (ImGui::Button("Run Bound State (runtime bridge)"))
    m_scriptResult = RunCxScript(m_manualTest.bound_state_script_path);
  ImGui::SameLine();
  if (ImGui::Button("Clear Result"))
  {
    m_scriptResult = ScriptResult();
    m_scriptResult.status = "PENDING";
    m_scriptResult.reason = "result cleared";
    m_scriptResult.runtime_fillback_status = "not_started";
  }

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
    ImGui::Text("Current Line Inspector");
    ImGui::TextWrapped("object: %s", current.object.empty() ? "(none)" : current.object.c_str());
    ImGui::TextWrapped("method: %s", current.method.empty() ? "(none)" : current.method.c_str());
    const std::vector<std::string> parameters = SplitParameters(current.params);
    if (parameters.empty()) ImGui::TextDisabled("params: (none)");
    for (std::size_t i = 0; i < parameters.size(); ++i)
      ImGui::BulletText("param[%d]: %s", static_cast<int>(i), parameters[i].c_str());
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
  ImGui::Text("Runtime Object Table");
  if (ImGui::Button("Refresh Runtime Objects"))
    RefreshRuntimeObjectTable("Query", "runtime_queried");
  for (const RuntimeObjectView& object : m_manualTest.runtime_objects)
  {
    ImGui::BulletText("%s %s", object.type.c_str(), object.name.c_str());
    ImGui::Text("exists_in_parser: %s", object.exists_in_parser ? "true" : "false");
    ImGui::Text("last_runtime_status: %s", object.last_runtime_status.c_str());
    ImGui::Text("last_method: %s", object.last_method.empty() ? "(none)" :
                                                   object.last_method.c_str());
    ImGui::TextWrapped("display_summary: %s", object.display_summary.c_str());
    ImGui::Text("source: %s", object.stale ? "stale_runtime" : "runtime_object");
    ImGui::Text("stale: %s", object.stale ? "true" : "false");
  }
  ImGui::Text("Runtime Variables");
  for (const RuntimeObjectView& object : m_manualTest.runtime_objects)
    if (object.type == "double")
      ImGui::BulletText("%s = %s", object.name.c_str(), object.display_summary.c_str());
  ImGui::BulletText("current_status = %s", m_manualTest.runtime_current_status.c_str());
  ImGui::BulletText("current_node = %s", m_manualTest.runtime_current_node.empty() ?
                    "(none)" : m_manualTest.runtime_current_node.c_str());
  ImGui::BulletText("current_connect = %s", m_manualTest.runtime_current_connect.empty() ?
                    "(none)" : m_manualTest.runtime_current_connect.c_str());

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
      ImGui::BulletText("object = %s", object.name.c_str());
      ImGui::Text("type = %s", object.type.c_str());
      ImGui::Text("visual_adapter = %s",
                  object.type == "Findcircle" ? "Findcircle" : "none");
      ImGui::Text("status = %s", object.status.c_str());
      if (!object.runtime_state.empty())
        ImGui::TextWrapped("%s", object.runtime_state.c_str());
      if (object.runtime_source_line > 0)
        ImGui::Text("source_line = %d", object.runtime_source_line);
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

  ImGui::Separator();
  ImGui::Text("Direct Capability Directory");
  std::string currentModule;
  std::string currentType;
  std::string currentMethod;
  if (!m_manualTest.line_views.empty() && m_manualTest.current_line >= 0 &&
      m_manualTest.current_line < static_cast<int>(m_manualTest.line_views.size()))
  {
    const ScriptLineView& line = m_manualTest.line_views[
      static_cast<std::size_t>(m_manualTest.current_line)];
    currentModule = line.module;
    currentMethod = line.method;
    for (const ScriptObjectView& object : m_manualTest.object_views)
      if (object.name == line.object) currentType = object.type;
  }
  for (const char* module : modules)
  {
    const bool highlighted = currentModule == module;
    if (highlighted) ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 220, 70, 255));
    const bool open = ImGui::TreeNode(module, "%s%s", module,
                                      highlighted ? "  [current]" : "");
    if (highlighted) ImGui::PopStyleColor();
    if (!open) continue;
    for (const DirectCapability& capability : m_directCapabilities)
    {
      if (capability.module != module) continue;
      ImGui::PushID(capability.type.c_str());
      const bool typeOpen = ImGui::TreeNode("type", "%s [%s]",
        capability.type.c_str(), capability.status.c_str());
      if (typeOpen)
      {
        if (capability.methods.empty()) ImGui::TextDisabled("methods: pending_binding");
        for (const DirectCapabilityMethod& method : capability.methods)
        {
          const bool isCurrent = capability.type == currentType &&
                                 method.name == currentMethod;
          ImGui::BulletText("%s [%s]", method.name.c_str(),
            isCurrent ? "pending_runtime" : method.status.c_str());
        }
        ImGui::TreePop();
      }
      ImGui::PopID();
    }
    ImGui::TreePop();
  }

  InputTextString("Case directory", m_manualTest.case_directory);
  InputTextString("User expected", m_manualTest.user_expected);
  InputTextString("Codex task", m_manualTest.codex_task);
  InputTextString("Forbidden changes", m_manualTest.forbidden_changes);
  if (ImGui::Button("Save Complete Case Package"))
  {
    std::string save_reason;
    const bool saved = SaveCasePackage(m_manualTest,
                                       m_scriptResult.status.empty() ? "PENDING" : m_scriptResult.status,
                                       m_scriptResult.reason,
                                       m_scriptResult.result_ref,
                                       m_scriptResult.evidence_ref,
                                       m_scriptResult.log_lines,
                                       m_annotationLayer.Elements(),
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