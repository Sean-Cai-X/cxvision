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
#include <algorithm>
#include <cstdlib>
#include <unordered_map>
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

std::vector<std::string> ExtractGlobalNames(const std::string& text)
{
  std::vector<std::string> names;
  const std::string prefix = "global.";
  std::size_t position = 0;
  while ((position = text.find(prefix, position)) != std::string::npos)
  {
    const std::size_t begin = position + prefix.size();
    std::size_t end = begin;
    while (end < text.size() &&
           (std::isalnum(static_cast<unsigned char>(text[end])) ||
            text[end] == '_')) ++end;
    const std::string name = text.substr(begin, end - begin);
    if (!name.empty() && std::find(names.begin(), names.end(), name) == names.end())
      names.push_back(name);
    position = end;
  }
  if (std::find(names.begin(), names.end(), "matInput") == names.end())
    names.insert(names.begin(), "matInput");
  return names;
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


static bool IsBraceOpenLine(const std::string& line)
{
    return TrimLine(line) == "{";
}

static bool IsBraceCloseLine(const std::string& line)
{
    return TrimLine(line) == "}";
}

static bool IsIfLine(const std::string& line)
{
    const std::string s = TrimLine(line);
    return s.rfind("if", 0) == 0 &&
        s.find('(') != std::string::npos &&
        s.rfind(')') != std::string::npos;
}

static std::string ExtractIfCondition(const std::string& line)
{
    const std::size_t l = line.find('(');
    const std::size_t r = line.rfind(')');

    if (l == std::string::npos || r == std::string::npos || r <= l)
        return {};

    return TrimLine(line.substr(l + 1, r - l - 1));
}

static std::vector<std::string> SplitArgs(const std::string& params)
{
    std::vector<std::string> out;
    std::string current;
    int quote = 0;

    for (char ch : params)
    {
        if (ch == '"')
            quote = !quote;

        if (ch == ',' && quote == 0)
        {
            out.push_back(TrimLine(current));
            current.clear();
        }
        else
        {
            current.push_back(ch);
        }
    }

    if (!TrimLine(current).empty())
        out.push_back(TrimLine(current));

    return out;
}

struct ParsedMethodCall
{
    bool valid = false;
    std::string object;
    std::string method;
    std::string params;
    std::vector<std::string> args;
};

static ParsedMethodCall ParseMethodCall(const std::string& statement)
{
    ParsedMethodCall result;

    std::string s = TrimLine(statement);
    if (!s.empty() && s.back() == ';')
        s.pop_back();

    const std::size_t open = s.find('(');
    const std::size_t close = s.rfind(')');

    if (open == std::string::npos || close == std::string::npos || close <= open)
        return result;

    const std::string callable = TrimLine(s.substr(0, open));
    const std::size_t dot = callable.rfind('.');

    if (dot == std::string::npos)
        return result;

    result.object = TrimLine(callable.substr(0, dot));
    result.method = TrimLine(callable.substr(dot + 1));
    result.params = s.substr(open + 1, close - open - 1);
    result.args = SplitArgs(result.params);
    result.valid = !result.object.empty() && !result.method.empty();

    return result;
}

static RuntimeObjectView* FindRuntimeObject(ManualTestContext& context,
    const std::string& name)
{
    for (RuntimeObjectView& object : context.runtime_objects)
    {
        if (object.name == name)
            return &object;
    }

    return nullptr;
}

static RuntimeObjectView& EnsureRuntimeObject(ManualTestContext& context,
    const std::string& name,
    const std::string& type,
    int declaredLine)
{
    if (RuntimeObjectView* existing = FindRuntimeObject(context, name))
        return *existing;

    RuntimeObjectView object;
    object.name = name;
    object.type = type;
    object.declared_line = declaredLine;
    object.exists_in_parser = true;
    object.runtime_state = "declared";
    object.last_runtime_status = "PENDING";
    object.last_method = "declare";
    object.last_update_line = declaredLine;
    object.display_summary = "declared";
    object.visualizable = false;
    object.visual_source = "runtime_object";
    object.stale = false;
    object.has_circle = false;

    context.runtime_objects.push_back(object);
    return context.runtime_objects.back();
}

static void UpsertVariableView(ManualTestContext& context,
    const std::string& type,
    const std::string& name,
    const std::string& value,
    int lineNo,
    const std::string& status)
{
    for (ScriptVariableView& variable : context.variable_views)
    {
        if (variable.name == name)
        {
            variable.type = type;
            variable.value = value;
            variable.declared_line = lineNo;
            variable.status = status;
            return;
        }
    }

    ScriptVariableView variable;
    variable.type = type;
    variable.name = name;
    variable.value = value;
    variable.declared_line = lineNo;
    variable.status = status;
    context.variable_views.push_back(variable);
}

static void ResetDebugRuntimeForReplay(ManualTestContext& context)
{
    context.runtime_objects.clear();
    context.runtime_int_vars.clear();

    // 当前 find_circle_direct_test.cxsc 中 m_isetcircle 参与 if 判断。
    // replay 调试模式下必须默认从 0 开始，否则第二次运行会跳过 setcircle。
    context.runtime_int_vars["m_isetcircle"] = 0;

    context.variable_views.clear();
    UpsertVariableView(context, "int", "m_isetcircle", "0", 0, "runtime_initialized");
    UpsertVariableView(context, "string", "global.current_status", "PENDING", 0, "runtime_initialized");

    for (ScriptLineView& line : context.line_views)
    {
        if (!TrimLine(line.statement).empty())
        {
            line.status = "source_analyzed";
            line.reason = "not executed";
            line.timestamp.clear();
        }
    }

    context.current_line = 0;
    context.run_state = "ready";
    context.debug_status = "PENDING";
    context.debug_reason = "runtime reset for replay";
    context.runtime_current_status = "PENDING";
}

static int FindNextNonEmptyLine(const ManualTestContext& context, int fromIndex)
{
    for (int i = fromIndex; i < static_cast<int>(context.line_views.size()); ++i)
    {
        if (!TrimLine(context.line_views[static_cast<std::size_t>(i)].statement).empty())
            return i;
    }

    return static_cast<int>(context.line_views.size());
}

static int FindMatchingBraceLine(const ManualTestContext& context, int openBraceIndex)
{
    int depth = 0;

    for (int i = openBraceIndex; i < static_cast<int>(context.line_views.size()); ++i)
    {
        const std::string s = TrimLine(context.line_views[static_cast<std::size_t>(i)].statement);

        if (s == "{")
            ++depth;
        else if (s == "}")
        {
            --depth;
            if (depth == 0)
                return i;
        }
    }

    return -1;
}

static int FindIfBodyStartLine(const ManualTestContext& context, int ifIndex)
{
    const int next = FindNextNonEmptyLine(context, ifIndex + 1);

    if (next < static_cast<int>(context.line_views.size()) &&
        IsBraceOpenLine(context.line_views[static_cast<std::size_t>(next)].statement))
    {
        return FindNextNonEmptyLine(context, next + 1);
    }

    return next;
}

static int FindIfAfterBlockLine(const ManualTestContext& context, int ifIndex)
{
    const int next = FindNextNonEmptyLine(context, ifIndex + 1);

    if (next < static_cast<int>(context.line_views.size()) &&
        IsBraceOpenLine(context.line_views[static_cast<std::size_t>(next)].statement))
    {
        const int close = FindMatchingBraceLine(context, next);
        if (close >= 0)
            return FindNextNonEmptyLine(context, close + 1);
    }

    return FindNextNonEmptyLine(context, ifIndex + 1);
}

static bool ReadRuntimeInt(ManualTestContext& context,
    const std::string& name,
    int& value)
{
    const auto it = context.runtime_int_vars.find(name);
    if (it != context.runtime_int_vars.end())
    {
        value = it->second;
        return true;
    }

    if (name == "m_isetcircle")
    {
        value = 0;
        context.runtime_int_vars[name] = 0;
        return true;
    }

    return false;
}

static bool EvalSimpleCondition(ManualTestContext& context,
    const std::string& condition,
    bool& value)
{
    const std::string s = TrimLine(condition);

    std::size_t op = s.find("==");
    bool equal = true;

    if (op == std::string::npos)
    {
        op = s.find("!=");
        equal = false;
    }

    if (op == std::string::npos)
        return false;

    const std::string lhs = TrimLine(s.substr(0, op));
    const std::string rhs = TrimLine(s.substr(op + 2));

    int lv = 0;
    if (!ReadRuntimeInt(context, lhs, lv))
        return false;

    const int rv = std::atoi(rhs.c_str());

    value = equal ? (lv == rv) : (lv != rv);
    return true;
}

static bool TryExecuteSimpleAssignment(ManualTestContext& context,
    int lineIndex,
    const std::string& statement)
{
    std::string s = TrimLine(statement);

    if (s.empty() || s.find('=') == std::string::npos || s.find("==") != std::string::npos)
        return false;

    if (!s.empty() && s.back() == ';')
        s.pop_back();

    const std::size_t eq = s.find('=');
    const std::string lhs = TrimLine(s.substr(0, eq));
    const std::string rhs = TrimLine(s.substr(eq + 1));

    if (lhs != "m_isetcircle")
        return false;

    const int v = std::atoi(rhs.c_str());
    context.runtime_int_vars[lhs] = v;

    ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
    line.status = "runtime_executed";
    line.reason = "assignment executed";
    line.return_variable = lhs;
    line.timestamp = CurrentTimestamp();

    UpsertVariableView(context, "int", lhs, std::to_string(v), line.line_no, "runtime_value");

    context.current_line = FindNextNonEmptyLine(context, lineIndex + 1);
    context.run_state = "runtime_step";
    context.debug_status = "PENDING";
    context.debug_reason = "assignment executed";

    return true;
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

bool IsObjectType(const std::string& type)
{
    return ModuleForType(type) != "cxscript";
}
static bool TryExecuteDeclaration(ManualTestContext& context,
    int lineIndex,
    const std::string& statement)
{
    std::istringstream tokens(TrimLine(statement));

    std::string type;
    std::string name;

    tokens >> type >> name;

    if (type.empty() || name.empty())
        return false;

    if (statement.find('(') != std::string::npos)
        return false;

    if (!name.empty() && name.back() == ';')
        name.pop_back();

    if (!IsObjectType(type))
        return false;

    RuntimeObjectView& object = EnsureRuntimeObject(
        context,
        name,
        type,
        context.line_views[static_cast<std::size_t>(lineIndex)].line_no);

    object.exists_in_parser = true;
    object.runtime_state = "declared";
    object.last_runtime_status = "PENDING";
    object.last_method = "declare";
    object.display_summary = "declared only; no visual geometry";
    object.last_update_line = context.line_views[static_cast<std::size_t>(lineIndex)].line_no;

    // 关键：声明 Findcircle 不能画圆。
    object.visualizable = false;
    object.has_circle = false;
    object.stale = false;

    ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
    line.status = "runtime_executed";
    line.reason = "object declared";
    line.timestamp = CurrentTimestamp();

    context.current_line = FindNextNonEmptyLine(context, lineIndex + 1);
    context.run_state = "runtime_step";
    context.debug_status = "PENDING";
    context.debug_reason = "object declaration executed";

    return true;
}

static bool TryExecuteFindcircleSetcircle(ManualTestContext& context,
    int lineIndex,
    const std::string& statement)
{
    const ParsedMethodCall call = ParseMethodCall(statement);

    if (!call.valid)
        return false;

    if (call.method != "setcircle")
        return false;

    if (call.args.size() < 4)
    {
        ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
        line.status = "BLOCKED";
        line.reason = "setcircle requires 4 parameters";
        context.run_state = "blocked";
        context.debug_status = "BLOCKED";
        context.debug_reason = line.reason;
        return true;
    }

    RuntimeObjectView& object = EnsureRuntimeObject(
        context,
        call.object,
        "Findcircle",
        context.line_views[static_cast<std::size_t>(lineIndex)].line_no);

    object.circle_cx = std::stof(call.args[0]);
    object.circle_cy = std::stof(call.args[1]);
    object.circle_inner = std::stof(call.args[2]);
    object.circle_radius = std::stof(call.args[3]);

    object.has_circle = true;
    object.visualizable = true;
    object.exists_in_parser = true;
    object.stale = false;
    object.visual_source = "runtime_object";
    object.last_method = "setcircle";
    object.last_runtime_status = "PENDING";
    object.runtime_state = "runtime_param_set";
    object.last_update_line = context.line_views[static_cast<std::size_t>(lineIndex)].line_no;

    std::ostringstream summary;
    summary << "circle=("
        << object.circle_cx << ", "
        << object.circle_cy << ", "
        << object.circle_inner << ", "
        << object.circle_radius << ")";
    object.display_summary = summary.str();

    ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
    line.status = "runtime_executed";
    line.reason = "Findcircle.setcircle executed";
    line.timestamp = CurrentTimestamp();

    context.runtime_current_status = "PENDING";
    context.current_line = FindNextNonEmptyLine(context, lineIndex + 1);
    context.run_state = "runtime_step";
    context.debug_status = "PENDING";
    context.debug_reason = "setcircle updated runtime object";

    return true;
}

static bool TryExecutePendingRuntimeMethod(ManualTestContext& context,
    int lineIndex,
    const std::string& statement)
{
    const ParsedMethodCall call = ParseMethodCall(statement);

    if (!call.valid)
        return false;

    if (call.method != "measure" &&
        call.method != "fitcircle" &&
        call.method != "FitResultMeasure" &&
        call.method != "match" &&
        call.method != "infer" &&
        call.method != "predict" &&
        call.method != "optimize_step")
    {
        return false;
    }

    RuntimeObjectView& object = EnsureRuntimeObject(
        context,
        call.object,
        call.object.find("circle") != std::string::npos ? "Findcircle" : "unknown",
        context.line_views[static_cast<std::size_t>(lineIndex)].line_no);

    object.last_method = call.method;
    object.last_runtime_status = "PENDING";
    object.runtime_state = "pending_runtime_bridge";
    object.last_update_line = context.line_views[static_cast<std::size_t>(lineIndex)].line_no;
    object.display_summary = call.method + " pending real runtime bridge";
    object.stale = false;

    ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
    line.status = "PENDING";
    line.reason = call.method + " requires real parser/runtime callback";
    line.timestamp = CurrentTimestamp();

    context.run_state = "blocked";
    context.debug_status = "PENDING";
    context.debug_reason = line.reason;

    // 注意：这里不要 current_line++，让用户看到当前 runtime pending 行。
    return true;
}
void AddObservedGlobalVariables(ManualTestContext& context,
    const std::string& statement)
{
    std::size_t position = 0;
    while ((position = statement.find("global.", position)) != std::string::npos)
    {
        std::size_t end = position + 7;
        while (end < statement.size() &&
            (std::isalnum(static_cast<unsigned char>(statement[end])) ||
                statement[end] == '_'))
            ++end;
        const std::string name = statement.substr(position, end - position);
        const auto existing = std::find_if(
            context.global_variable_views.begin(), context.global_variable_views.end(),
            [&](const ScriptVariableView& variable) { return variable.name == name; });
        if (existing == context.global_variable_views.end())
        {
            const bool is_image = name == "global.matInput";
            context.global_variable_views.push_back(
                { is_image ? "Image" : "auto", name, "uninitialized", 0,
                 "observed_source", is_image ? context.image_file_path : std::string(),
                 false });
        }
        position = end;
    }
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
void AnalyzeScript(ManualTestContext& context)
{
    if (context.analyzed_text == context.editor_text) return;
    context.analyzed_text = context.editor_text;
    context.line_views.clear();
    context.variable_views.clear();
    context.object_views.clear();
    if (context.global_variable_views.size() > 1)
        context.global_variable_views.erase(
            context.global_variable_views.begin() + 1,
            context.global_variable_views.end());
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
        AddObservedGlobalVariables(context, statement);
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

                ScriptObjectView object;
                object.module = line.module;
                object.type = declared_type;
                object.name = declared_name;
                object.status = line.module == "cximage" ? "declared_source_only" : "pending_binding";
                object.runtime_state = "not_executed";
                object.runtime_source_line = 0;
                object.declared_line = line.line_no;
                context.object_views.push_back(object);

                if (declared_type == "Image")
                    context.variable_views.push_back(
                        { declared_type, declared_name, "uninitialized", line.line_no,
                         "not_initialized", context.image_file_path, false });
            }
            else
            {
                const std::size_t equal = statement.find('=');
                const std::string value = equal == std::string::npos ? "uninitialized" :
                    TrimLine(statement.substr(equal + 1, statement.size() - equal - 2));
                context.variable_views.push_back({ declared_type, declared_name, value,
                                                  line.line_no, "observed_source" });
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

static void DebugStepOnce(ManualTestContext& context)
{
    AnalyzeScript(context);

    if (context.line_views.empty())
    {
        context.run_state = "blocked";
        context.debug_status = "BLOCKED";
        context.debug_reason = "no script lines";
        return;
    }

    if (context.current_line < 0)
        context.current_line = 0;

    if (context.current_line >= static_cast<int>(context.line_views.size()))
    {
        context.run_state = "finished";
        context.debug_status = "PENDING";
        context.debug_reason = "end of script";
        return;
    }

    const int lineIndex = context.current_line;
    ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
    const std::string statement = TrimLine(line.statement);

    if (statement.empty())
    {
        line.status = "skipped_empty";
        line.reason = "empty line";
        context.current_line = FindNextNonEmptyLine(context, lineIndex + 1);
        return;
    }

    if (IsBraceOpenLine(statement) || IsBraceCloseLine(statement))
    {
        line.status = "structural";
        line.reason = "brace skipped by debugger";
        line.timestamp = CurrentTimestamp();
        context.current_line = FindNextNonEmptyLine(context, lineIndex + 1);
        return;
    }

    if (IsIfLine(statement))
    {
        bool conditionValue = false;
        const std::string condition = ExtractIfCondition(statement);

        if (!EvalSimpleCondition(context, condition, conditionValue))
        {
            line.status = "BLOCKED";
            line.reason = "cannot evaluate if condition: " + condition;
            line.timestamp = CurrentTimestamp();
            context.run_state = "blocked";
            context.debug_status = "BLOCKED";
            context.debug_reason = line.reason;
            return;
        }

        line.status = conditionValue ? "control_true" : "control_false";
        line.reason = conditionValue ? "if condition true" : "if condition false";
        line.timestamp = CurrentTimestamp();

        context.current_line = conditionValue ?
            FindIfBodyStartLine(context, lineIndex) :
            FindIfAfterBlockLine(context, lineIndex);

        context.run_state = "runtime_step";
        context.debug_status = "PENDING";
        context.debug_reason = line.reason;
        return;
    }

    if (TryExecuteSimpleAssignment(context, lineIndex, statement))
        return;

    if (TryExecuteDeclaration(context, lineIndex, statement))
        return;

    if (TryExecuteFindcircleSetcircle(context, lineIndex, statement))
        return;

    if (TryExecutePendingRuntimeMethod(context, lineIndex, statement))
        return;

    // 其它 setmethod / Setgap / setthre / setlinegap 这类参数行，先作为轻量已执行。
    if (ParseMethodCall(statement).valid)
    {
        const ParsedMethodCall call = ParseMethodCall(statement);

        RuntimeObjectView& object = EnsureRuntimeObject(
            context,
            call.object,
            call.object.find("circle") != std::string::npos ? "Findcircle" : "unknown",
            line.line_no);

        object.last_method = call.method;
        object.last_runtime_status = "PENDING";
        object.runtime_state = "runtime_param_set";
        object.last_update_line = line.line_no;
        object.display_summary = call.method + "(" + call.params + ")";
        object.stale = false;

        line.status = "runtime_executed";
        line.reason = "method parameter line executed in debug shim";
        line.timestamp = CurrentTimestamp();

        context.current_line = FindNextNonEmptyLine(context, lineIndex + 1);
        context.run_state = "runtime_step";
        context.debug_status = "PENDING";
        context.debug_reason = "method parameter line executed";
        return;
    }

    line.status = "source_analyzed";
    line.reason = "statement not executable by debug shim";
    line.timestamp = CurrentTimestamp();
    context.current_line = FindNextNonEmptyLine(context, lineIndex + 1);
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
  return m_parserDebugBridge.QueryObjectExists(type, name);
}

Image* ViewController::QueryParserImage(const std::string& name)
{
  return m_parserDebugBridge.QueryImage(name);
}

bool ViewController::QueryParserDouble(const std::string& name, double& value)
{
  return m_parserDebugBridge.QueryDouble(name, value);
}

bool ViewController::SetParserDouble(const std::string& name, double value)
{
  return m_parserDebugBridge.SetDouble(name, value);
}

void ViewController::RefreshRuntimeObjectTable(const std::string& lastMethod,
                                               const std::string& runtimeStatus)
{
  int lastUpdateLine = 0;
  if (m_manualTest.current_line >= 0 &&
      m_manualTest.current_line < static_cast<int>(m_manualTest.line_views.size()))
    lastUpdateLine = m_manualTest.line_views[
      static_cast<std::size_t>(m_manualTest.current_line)].line_no;
  const std::vector<ParserDebugObjectSnapshot> snapshots =
    m_parserDebugBridge.SnapshotRuntimeObjects(lastMethod, lastUpdateLine,
                                               runtimeStatus);
  m_manualTest.runtime_objects.clear();
  bool freshParserImage = false;
  for (const ParserDebugObjectSnapshot& snapshot : snapshots)
  {
    RuntimeObjectView entry;
    entry.name = snapshot.name;
    entry.type = snapshot.type;
    for (const ScriptObjectView& source : m_manualTest.object_views)
      if (source.name == entry.name && source.type == entry.type)
        entry.declared_line = source.declared_line;
    entry.exists_in_parser = snapshot.exists_in_parser;
    entry.last_runtime_status = runtimeStatus;
    entry.runtime_state = snapshot.runtime_state;
    entry.last_method = snapshot.last_method;
    entry.last_update_line = snapshot.last_update_line;
    entry.display_summary = snapshot.value_summary;
    entry.visualizable = snapshot.visualizable;
    entry.visual_source = snapshot.visual_source;
    entry.stale = snapshot.stale;
    entry.has_circle = snapshot.has_circle;
    entry.circle_cx = snapshot.circle_cx;
    entry.circle_cy = snapshot.circle_cy;
    entry.circle_inner = snapshot.circle_inner;
    entry.circle_radius = snapshot.circle_radius;
    m_manualTest.runtime_objects.push_back(entry);
    if (entry.type == "Image" && entry.exists_in_parser && !entry.stale)
    {
      Image* image = m_parserDebugBridge.QueryImage(entry.name);
      if (image != nullptr && !image->getmat().empty())
      {
        UpdateImageViewImage(image->getmat());
        m_scriptResult.image_ref = "runtime_object:" + entry.name;
        freshParserImage = true;
      }
    }
  }

  if (!freshParserImage && runtimeStatus == "compiled")
  {
    const cv::Mat viewImage = cv::imread(m_manualTest.image_file_path);
    if (!viewImage.empty())
    {
      UpdateImageViewImage(viewImage);
      m_scriptResult.image_ref = m_manualTest.image_file_path;
    }
    else m_scriptResult.image_ref.clear();
  }

  const std::vector<ParserDebugVariableSnapshot> variables =
    m_parserDebugBridge.SnapshotRuntimeVariables();
  std::string doutputValue = "PENDING";
  for (const ParserDebugVariableSnapshot& variable : variables)
  {
    RuntimeObjectView entry;
    entry.name = variable.name;
    entry.type = "double";
    entry.exists_in_parser = variable.exists_in_parser;
    entry.last_runtime_status = variable.exists_in_parser ? runtimeStatus : "PENDING";
    entry.runtime_state = variable.exists_in_parser ? "alive" : "PENDING";
    entry.last_method = lastMethod;
    entry.last_update_line = lastUpdateLine;
    entry.display_summary = variable.exists_in_parser ?
      std::to_string(variable.value) : "not found in parser";
    entry.visualizable = false;
    entry.visual_source = variable.exists_in_parser ? "runtime_object" :
                                                        "stale_runtime";
    entry.stale = !variable.exists_in_parser;
    m_manualTest.runtime_objects.push_back(entry);
    if (variable.name == "doutputvalue")
      doutputValue = variable.exists_in_parser ? std::to_string(variable.value) :
                                                "PENDING";
    if (variable.name == "current_status")
      m_manualTest.runtime_current_status = variable.exists_in_parser ?
        std::to_string(variable.value) : "PENDING";
  }
  int runtimeObjectCount = 0;
  for (const RuntimeObjectView& object : m_manualTest.runtime_objects)
    if (object.exists_in_parser) ++runtimeObjectCount;
  m_semanticFlowGraph.SetRuntimeDebugSummary(
    doutputValue, m_manualTest.runtime_current_status, runtimeObjectCount,
    m_scriptResult.reason.empty() ? "runtime table refreshed" :
                                    m_scriptResult.reason);
}

void ViewController::drawManualStateTestConsole()
{
  if (!m_showManualStateTestConsole) return;

  ImGui::SetNextWindowPos(ImVec2(70, 45), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(820, 620), ImGuiCond_Once);
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
      m_parserDebugBridge.SetGlobalMatInput(image);
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
    m_parserDebugBridge.ClearGlobalInputs();
    m_manualTest = ManualTestContext();
  }

  ImGui::Separator();
  ImGui::Text("Global Runtime Inputs");
  const std::vector<std::string> globalNames =
    ExtractGlobalNames(m_manualTest.editor_text);
  for (const std::string& name : globalNames)
  {
    ImGui::BulletText("global.%s", name.c_str());
    if (name == "matInput")
    {
      ImGui::Text("type: Image");
      ImGui::Text("source: %s", m_parserDebugBridge.HasGlobalMatInput() ?
                  "view_image" : "none");
      ImGui::Text("status: %s", m_parserDebugBridge.HasGlobalMatInput() ?
                  "initialized" : "not_initialized");
      ImGui::Text("image: %s", m_manualTest.image_file_path.empty() ? "(none)" :
                                                 m_manualTest.image_file_path.c_str());
      ImGui::Text("size: %dx%d", m_parserDebugBridge.GlobalMatInputWidth(),
                  m_parserDebugBridge.GlobalMatInputHeight());
    }
    else
    {
      ImGui::Text("type: unresolved");
      ImGui::Text("source: script_reference");
      ImGui::Text("status: pending_binding");
    }
  }
  if (ImGui::Button("Initialize global.matInput from View Image"))
  {
    cv::Mat globalImage = m_imageViewImage;
    if (globalImage.empty()) globalImage = cv::imread(m_manualTest.image_file_path);
    const bool initialized = m_parserDebugBridge.SetGlobalMatInput(globalImage);
    m_manualTest.debug_action = "Initialize global.matInput";
    m_manualTest.debug_status = initialized ? "PENDING" : "BLOCKED";
    m_manualTest.debug_reason = initialized ?
      "global.matInput initialized as parser Image global_matInput" :
      "global.matInput image is empty or parser binding failed";
    m_scriptResult.status = m_manualTest.debug_status;
    m_scriptResult.reason = m_manualTest.debug_reason;
    RefreshRuntimeObjectTable("global.matInput",
      initialized ? "runtime_queried" : "BLOCKED");
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
        m_parserDebugBridge.SetGlobalMatInput(image);
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
                               ImVec2(-1.0f, 140.0f)))
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
      AnalyzeScript(m_manualTest);
      ResetDebugRuntimeForReplay(m_manualTest);

      m_manualTest.run_state = "compiled";
      m_manualTest.debug_action = "Compile";
      m_manualTest.debug_status = "PENDING";
      m_manualTest.debug_reason = "source compiled for debug replay; runtime not executed";

      m_scriptResult = ScriptResult();
      m_scriptResult.source = m_manualTest.editor_source;
      m_scriptResult.script_path = m_manualTest.loaded_script_path;
      m_scriptResult.status = "PENDING";
      m_scriptResult.reason = "compiled for debug replay; no PASS without runtime result";
      m_scriptResult.runtime_fillback_status = "debug_replay_ready";
  }

  ImGui::SameLine();
  if (ImGui::Button("Run"))
  {
      AnalyzeScript(m_manualTest);
      ResetDebugRuntimeForReplay(m_manualTest);

      m_manualTest.run_state = "runtime_run";
      m_manualTest.stop_requested = false;

      int guard = 0;
      const int maxSteps = static_cast<int>(m_manualTest.line_views.size()) * 4 + 16;

      while (!m_manualTest.stop_requested &&
          m_manualTest.current_line < static_cast<int>(m_manualTest.line_views.size()) &&
          guard++ < maxSteps)
      {
          DebugStepOnce(m_manualTest);

          // 遇到真实 runtime 缺失的算法行，先停下来让用户看。
          if (m_manualTest.run_state == "blocked")
              break;
      }

      m_scriptResult.status = m_manualTest.debug_status;
      m_scriptResult.reason = m_manualTest.debug_reason;
      m_scriptResult.runtime_fillback_status = "debug_run";
  }
  ImGui::SameLine();
  if (ImGui::Button("Step"))
  {
      if (m_manualTest.run_state == "idle" ||
          m_manualTest.run_state == "compiled" ||
          m_manualTest.run_state == "ready")
      {
          // 如果还没初始化 runtime，就初始化一次。
          if (m_manualTest.runtime_objects.empty() &&
              m_manualTest.runtime_int_vars.empty())
          {
              ResetDebugRuntimeForReplay(m_manualTest);
          }
      }

      DebugStepOnce(m_manualTest);

      m_scriptResult.status = m_manualTest.debug_status;
      m_scriptResult.reason = m_manualTest.debug_reason;
      m_scriptResult.runtime_fillback_status = "debug_step";
  }

  ImGui::SameLine();
  if (ImGui::Button("Continue"))
  {
      m_manualTest.stop_requested = false;
      m_manualTest.run_state = "runtime_continue";

      int guard = 0;
      const int maxSteps = static_cast<int>(m_manualTest.line_views.size()) * 4 + 16;

      while (!m_manualTest.stop_requested &&
          m_manualTest.current_line < static_cast<int>(m_manualTest.line_views.size()) &&
          guard++ < maxSteps)
      {
          DebugStepOnce(m_manualTest);

          if (m_manualTest.run_state == "blocked")
              break;
      }

      m_scriptResult.status = m_manualTest.debug_status;
      m_scriptResult.reason = m_manualTest.debug_reason;
      m_scriptResult.runtime_fillback_status = "debug_continue";
  }
  ImGui::SameLine();
  if (ImGui::Button("Stop"))
  {
      m_manualTest.stop_requested = true;
      m_manualTest.run_state = "stopped";
      m_manualTest.debug_status = "PENDING";
      m_manualTest.debug_reason = "debug run stopped by user";

      m_scriptResult.status = "PENDING";
      m_scriptResult.reason = m_manualTest.debug_reason;
      m_scriptResult.runtime_fillback_status = "stopped";
  }
  ImGui::SameLine();
  if (ImGui::Button("Reset"))
  {
      AnalyzeScript(m_manualTest);
      ResetDebugRuntimeForReplay(m_manualTest);

      m_scriptResult = ScriptResult();
      m_scriptResult.status = "PENDING";
      m_scriptResult.reason = "debug runtime reset";
      m_scriptResult.runtime_fillback_status = "reset";
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
    m_manualTest.debug_action = "Clear Result";
    m_manualTest.debug_status = "PENDING";
    m_manualTest.debug_reason = m_scriptResult.reason;
    m_manualTest.debug_parser_output.clear();
  }

  ImGui::Separator();
  ImGui::Text("Last Debug Result");
  ImGui::Text("action: %s | status: %s", m_manualTest.debug_action.c_str(),
              m_manualTest.debug_status.c_str());
  ImGui::TextWrapped("reason: %s", m_manualTest.debug_reason.c_str());
  if (!m_manualTest.debug_parser_output.empty())
  {
    ImGui::Text("parser output:");
    ImGui::BeginChild("debug_parser_output", ImVec2(0.0f, 72.0f), true);
    ImGui::TextUnformatted(m_manualTest.debug_parser_output.c_str());
    ImGui::EndChild();
  }

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

  ImGui::BeginChild("cxscript_line_view", ImVec2(0.0f, 150.0f), true);
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
  const auto drawVariableList =
    [&](const char* title, std::vector<ScriptVariableView>& variables)
  {
    ImGui::Text("%s (%d)", title, static_cast<int>(variables.size()));
    ImGui::PushID(title);
    for (std::size_t index = 0; index < variables.size(); ++index)
    {
      ScriptVariableView& variable = variables[index];
      ImGui::PushID(static_cast<int>(index));
      ImGui::BulletText("%s %s = %s", variable.type.c_str(),
                        variable.name.c_str(), variable.value.c_str());
      ImGui::SameLine();
      ImGui::TextDisabled("[%s]", variable.status.c_str());
      if (variable.type == "Image")
      {
        if (variable.image_path.empty())
          variable.image_path = m_manualTest.image_file_path;
        ImGui::SetNextItemWidth(420.0f);
        InputTextString("Image path", variable.image_path);
        ImGui::SameLine();
        if (ImGui::Button("Initialize"))
        {
          const cv::Mat image = cv::imread(variable.image_path);
          if (image.empty())
          {
            variable.image_initialized = false;
            variable.status = "load_failed";
            variable.value = "uninitialized";
            m_scriptResult.status = "FAIL";
            m_scriptResult.reason =
              variable.name + ": image file not found or unreadable";
          }
          else
          {
            UpdateImageViewImage(image);
            m_manualTest.image_file_path = variable.image_path;
            bool runtimeBound = false;
            if (variable.name == "global.matInput")
              runtimeBound = m_parserDebugBridge.SetGlobalMatInput(image);
            else
            {
              Image* runtimeImage = QueryParserImage(variable.name);
              if (runtimeImage != nullptr)
              {
                runtimeImage->copyFromMat(image);
                runtimeBound = true;
              }
            }
            variable.image_initialized = true;
            variable.status = runtimeBound ? "initialized" :
                                             "ui_initialized_runtime_binding_pending";
            variable.value = variable.image_path;
            m_scriptResult.image_ref = variable.name;
            m_scriptResult.status = "PENDING";
            m_scriptResult.reason = runtimeBound ?
              variable.name + ": image initialized and bound" :
              variable.name +
              ": image initialized in variable list; parser binding pending";
          }
        }
      }
      ImGui::PopID();
    }
    ImGui::PopID();
  };
  drawVariableList("Global Variables", m_manualTest.global_variable_views);
  ImGui::Spacing();
  drawVariableList("Local Variables", m_manualTest.variable_views);

  ImGui::Separator();
  ImGui::Text("Runtime Object Table");
  for (const RuntimeObjectView& object : m_manualTest.runtime_objects)
  {
    ImGui::BulletText("%s %s", object.type.c_str(), object.name.c_str());
    ImGui::Text("declared_line: %d", object.declared_line);
    ImGui::Text("exists_in_parser: %s", object.exists_in_parser ? "true" : "false");
    ImGui::Text("runtime_state: %s", object.runtime_state.c_str());
    ImGui::Text("last_runtime_status: %s", object.last_runtime_status.c_str());
    ImGui::Text("last_method: %s", object.last_method.empty() ? "(none)" :
                                                   object.last_method.c_str());
    ImGui::Text("last_update_line: %d", object.last_update_line);
    ImGui::TextWrapped("value_summary: %s", object.display_summary.c_str());
    ImGui::Text("visualizable: %s", object.visualizable ? "true" : "false");
    ImGui::Text("visual_source: %s", object.visual_source.c_str());
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