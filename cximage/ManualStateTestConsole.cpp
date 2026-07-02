#include "ViewController.h"
#include "Image.h"
#include "Findcircle.h"
#include "imagemanager.h"

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

#include <memory>
#include <cmath>
#include <exception>
#include <opencv2/imgcodecs.hpp>
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
struct DebugCximageRuntime
{
    std::unordered_map<std::string, std::unique_ptr<Image>> images;
    std::unordered_map<std::string, std::unique_ptr<Findcircle>> circles;
};

static std::unordered_map<ManualTestContext*, DebugCximageRuntime> g_cximageRuntime;

static DebugCximageRuntime& CxRuntime(ManualTestContext& context)
{
    return g_cximageRuntime[&context];
}

static void PrepareFindcircleDebugRuntime()
{
    // Findcircle resolves its scratch image from the current ImageManager
    // module during construction. The direct debugger owns module slot 1.
    ImageManager::m_imodulid = 1;
    ImageManager::GetBackImage(1);
}

static std::string GetGlobalMatInputPath(const ManualTestContext& context)
{
    if (!context.image_file_path.empty())
        return context.image_file_path;

    for (const ScriptVariableView& variable : context.global_variable_views)
    {
        if (variable.name == "global.matInput")
        {
            if (!variable.image_path.empty())
                return variable.image_path;

            if (!variable.value.empty() &&
                variable.value != "uninitialized" &&
                variable.value != "none")
                return variable.value;
        }
    }

    return {};
}

static std::string StripAddressPrefix(std::string s)
{
    s = TrimLine(s);
    if (!s.empty() && s.front() == '&')
        s.erase(s.begin());
    return TrimLine(s);
}
static RuntimeObjectView* FindRuntimeObjectByName(ManualTestContext& context,
    const std::string& name)
{
    for (RuntimeObjectView& object : context.runtime_objects)
    {
        if (object.name == name)
            return &object;
    }

    return nullptr;
}

static const RuntimeObjectView* FindRuntimeObjectByName(const ManualTestContext& context,
    const std::string& name)
{
    for (const RuntimeObjectView& object : context.runtime_objects)
    {
        if (object.name == name)
            return &object;
    }

    return nullptr;
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
    object.last_runtime_status = "runtime_executed";
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

static void UpsertGlobalVariableViewCore(
    ManualTestContext& context,
    const std::string& type,
    const std::string& name,
    const std::string& value,
    int lineNo,
    const std::string& status,
    const std::string& imagePath,
    bool imageInitialized)
{
    for (ScriptVariableView& variable : context.global_variable_views)
    {
        if (variable.name == name)
        {
            variable.type = type;
            variable.value = value;
            variable.declared_line = lineNo;
            variable.status = status;

            if (!imagePath.empty())
            {
                variable.image_path = imagePath;
            }

            if (imageInitialized)
            {
                variable.image_initialized = true;
            }

            return;
        }
    }

    ScriptVariableView variable;
    variable.type = type;
    variable.name = name;
    variable.value = value;
    variable.declared_line = lineNo;
    variable.status = status;
    variable.image_path = imagePath;
    variable.image_initialized = imageInitialized;

    context.global_variable_views.push_back(variable);
}

// 普通 global 变量：global.current_status / global.circle_ref 等。
// 注意：这个函数只有 6 个参数，不要再给它加默认参数。
static void UpsertGlobalVariableView(
    ManualTestContext& context,
    const std::string& type,
    const std::string& name,
    const std::string& value,
    int lineNo,
    const std::string& status)
{
    UpsertGlobalVariableViewCore(
        context,
        type,
        name,
        value,
        lineNo,
        status,
        std::string(),
        false);
}

// 图像 global 变量：global.matInput。
// 注意：用不同函数名，避免和普通变量函数重载冲突。
static void UpsertGlobalImageVariableView(
    ManualTestContext& context,
    const std::string& type,
    const std::string& name,
    const std::string& value,
    int lineNo,
    const std::string& status,
    const std::string& imagePath,
    bool imageInitialized)
{
    UpsertGlobalVariableViewCore(
        context,
        type,
        name,
        value,
        lineNo,
        status,
        imagePath,
        imageInitialized);
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
    g_cximageRuntime.erase(&context);

    context.runtime_objects.clear();
    context.debug_snapshots.clear();
    context.current_debug_snapshot = DebugStepSnapshot();
    context.runtime_int_vars.clear();

    // 当前 find_circle_direct_test.cxsc 中 m_isetcircle 参与 if 判断。
    // replay 调试模式下必须默认从 0 开始，否则第二次运行会跳过 setcircle。
    context.runtime_int_vars["m_isetcircle"] = 0;

    context.variable_views.clear();
    UpsertVariableView(context, "int", "m_isetcircle", "0", 0, "runtime_initialized");
    UpsertVariableView(context, "string", "global.current_status", "PENDING", 0, "runtime_initialized");
    for (ScriptVariableView& variable : context.global_variable_views)
    {
        if (variable.name == "global.matInput") continue;
        variable.value = "uninitialized";
        variable.status = "observed_source";
    }

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

    if (s.empty() ||
        s.find('=') == std::string::npos ||
        s.find("==") != std::string::npos)
    {
        return false;
    }

    if (!s.empty() && s.back() == ';')
        s.pop_back();

    const std::size_t eq = s.find('=');

    if (eq == std::string::npos)
        return false;

    const std::string lhs = TrimLine(s.substr(0, eq));
    std::string rhs = TrimLine(s.substr(eq + 1));

    ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];

    // 1. 调试控制变量：m_isetcircle = 1;
    if (lhs == "m_isetcircle")
    {
        const int v = std::atoi(rhs.c_str());

        context.runtime_int_vars[lhs] = v;

        line.status = "runtime_executed";
        line.reason = "assignment executed";
        line.return_variable = lhs;
        line.timestamp = CurrentTimestamp();

        UpsertVariableView(
            context,
            "int",
            lhs,
            std::to_string(v),
            line.line_no,
            "runtime_value");

        context.current_line = FindNextNonEmptyLine(context, lineIndex + 1);
        context.run_state = "runtime_step";
        context.debug_status = "PENDING";
        context.debug_reason = "assignment executed";

        return true;
    }

    // 2. 全局状态变量：global.current_status = "PENDING";
    if (lhs == "global.current_status")
    {
        // 去掉字符串两侧引号
        if (!rhs.empty() && rhs.front() == '"')
            rhs.erase(rhs.begin());

        if (!rhs.empty() && rhs.back() == '"')
            rhs.pop_back();

        context.runtime_current_status = rhs;

        UpsertGlobalVariableView(
            context,
            "string",
            lhs,
            rhs,
            line.line_no,
            "runtime_value");

        // 同步到 Local Variables / Variable Snapshot，方便界面统一观察
        UpsertVariableView(
            context,
            "string",
            lhs,
            rhs,
            line.line_no,
            "runtime_initialized");

        line.status = "runtime_executed";
        line.reason = "global.current_status remains " + rhs + "; judge/rule not executed";
        line.return_variable = lhs;
        line.timestamp = CurrentTimestamp();

        context.current_line = FindNextNonEmptyLine(context, lineIndex + 1);
        context.run_state = "runtime_step";

        // 注意：这里不能因为赋值成功就 PASS。
        context.debug_status = "PENDING";
        context.debug_reason = line.reason;

        return true;
    }

    return false;
}

static bool TryExecuteCurrentStatusAssignment(ManualTestContext& context,
    int lineIndex,
    const std::string& statement)
{
    const std::string trimmed = TrimLine(statement);
    if (trimmed.find("global.current_status") == std::string::npos ||
        trimmed.find("PENDING") == std::string::npos)
        return false;

    ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
    UpsertGlobalVariableView(context, "string", "global.current_status",
        "PENDING", line.line_no, "runtime_value");
    context.runtime_current_status = "PENDING";
    line.status = "runtime_executed";
    line.reason = "global.current_status remains PENDING; judge/rule not executed";
    line.timestamp = CurrentTimestamp();
    context.current_line = FindNextNonEmptyLine(context, lineIndex + 1);
    context.run_state = "runtime_step";
    context.debug_status = "PENDING";
    context.debug_reason = line.reason;
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

    DebugCximageRuntime& runtime = CxRuntime(context);

    if (type == "Image")
    {
        runtime.images[name] = std::make_unique<Image>();
        object.exists_in_parser = true;
        object.runtime_state = "runtime_object_created";
        object.last_runtime_status = "PENDING";
        object.display_summary = "Image runtime object created";
        object.visualizable = false;
    }
    else if (type == "Findcircle")
    {
        PrepareFindcircleDebugRuntime();
        runtime.circles[name] = std::make_unique<Findcircle>();
        object.exists_in_parser = true;
        object.runtime_state = "runtime_object_created";
        object.last_runtime_status = "PENDING";
        object.display_summary = "Findcircle runtime object created";
        object.visualizable = false;
        object.has_circle = false;
        object.has_measure_points = false;
        object.has_fit_result = false;
        object.has_result_measure = false;
    }



    object.exists_in_parser = true;
    object.runtime_state = "declared";
    object.last_runtime_status = "runtime_executed";
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
static bool TryExecuteImageCopyFromMat(ManualTestContext& context,
    int lineIndex,
    const std::string& statement)
{
    const ParsedMethodCall call = ParseMethodCall(statement);

    if (!call.valid)
        return false;

    if (call.method != "copyFromMat")
        return false;

    DebugCximageRuntime& runtime = CxRuntime(context);

    auto imageIt = runtime.images.find(call.object);
    if (imageIt == runtime.images.end() || !imageIt->second)
    {
        RuntimeObjectView& object = EnsureRuntimeObject(
            context,
            call.object,
            "Image",
            context.line_views[static_cast<std::size_t>(lineIndex)].line_no);

        object.last_runtime_status = "BLOCKED";
        object.runtime_state = "missing_runtime_image_object";
        object.display_summary = "Image object was not created before copyFromMat";

        ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
        line.status = "BLOCKED";
        line.reason = "Image object missing before copyFromMat";
        line.timestamp = CurrentTimestamp();

        context.run_state = "blocked";
        context.debug_status = "BLOCKED";
        context.debug_reason = line.reason;
        return true;
    }

    const std::string imagePath = GetGlobalMatInputPath(context);

    if (imagePath.empty())
    {
        ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
        line.status = "BLOCKED";
        line.reason = "global.matInput image path is empty";
        line.timestamp = CurrentTimestamp();

        context.run_state = "blocked";
        context.debug_status = "BLOCKED";
        context.debug_reason = line.reason;
        return true;
    }

    cv::Mat src = cv::imread(imagePath, cv::IMREAD_COLOR);

    if (src.empty())
    {
        ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
        line.status = "BLOCKED";
        line.reason = "failed to load image: " + imagePath;
        line.timestamp = CurrentTimestamp();

        context.run_state = "blocked";
        context.debug_status = "BLOCKED";
        context.debug_reason = line.reason;
        return true;
    }

    imageIt->second->copyFromMat(src);

    RuntimeObjectView& object = EnsureRuntimeObject(
        context,
        call.object,
        "Image",
        context.line_views[static_cast<std::size_t>(lineIndex)].line_no);

    object.exists_in_parser = true;
    object.last_method = "copyFromMat";
    object.last_runtime_status = "runtime_executed";
    object.runtime_state = "runtime_image_ready";
    object.last_update_line = context.line_views[static_cast<std::size_t>(lineIndex)].line_no;
    object.display_summary = "image loaded: " + imagePath;
    object.visualizable = false;
    object.stale = false;

    ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
    line.status = "runtime_executed";
    line.reason = "Image.copyFromMat executed from global.matInput";
    line.timestamp = CurrentTimestamp();

    context.current_line = FindNextNonEmptyLine(context, lineIndex + 1);
    context.run_state = "runtime_step";
    context.debug_status = "PENDING";
    context.debug_reason = "image runtime object ready";

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

    DebugCximageRuntime& runtime = CxRuntime(context);

    auto circleIt = runtime.circles.find(call.object);
    if (circleIt == runtime.circles.end() || !circleIt->second)
    {
        PrepareFindcircleDebugRuntime();
        runtime.circles[call.object] = std::make_unique<Findcircle>();
        circleIt = runtime.circles.find(call.object);
    }

    const int cx = static_cast<int>(object.circle_cx);
    const int cy = static_cast<int>(object.circle_cy);
    const int scriptThird = static_cast<int>(object.circle_inner);
    const int scriptFourth = static_cast<int>(object.circle_radius);
    int perimeterX = scriptThird;
    int perimeterY = scriptFourth;

    // Direct-test scripts use setcircle(cx, cy, 0, radius). The native
    // Findcircle API expects a perimeter point instead of a radius.
    if (scriptThird == 0 && scriptFourth > 0)
    {
        perimeterX = cx;
        perimeterY = cy + scriptFourth;
    }

    circleIt->second->setcircle(cx, cy, perimeterX, perimeterY);

    object.has_circle = true;
    object.visualizable = true;
    object.exists_in_parser = true;
    object.stale = false;
    object.visual_source = "runtime_object";
    object.last_method = "setcircle";
    object.last_runtime_status = "runtime_executed";
    object.runtime_state = "runtime_param_set";
    object.last_update_line = context.line_views[static_cast<std::size_t>(lineIndex)].line_no;

    std::ostringstream summary;
    summary << "circle=("
        << object.circle_cx << ", "
        << object.circle_cy << ", "
        << object.circle_inner << ", "
        << object.circle_radius << ")"
        << " | native_perimeter=(" << perimeterX << ", " << perimeterY << ")";
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
static bool TryExecuteFindcircleParamMethod(ManualTestContext& context,
    int lineIndex,
    const std::string& statement)
{
    const ParsedMethodCall call = ParseMethodCall(statement);

    if (!call.valid)
        return false;

    const bool isCircleParamMethod =
        call.method == "setmethod" ||
        call.method == "Setgap" ||
        call.method == "setthre" ||
        call.method == "setlinegap" ||
        call.method == "setfitmeasuregap";

    if (!isCircleParamMethod)
        return false;

    DebugCximageRuntime& runtime = CxRuntime(context);

    auto circleIt = runtime.circles.find(call.object);
    if (circleIt == runtime.circles.end() || !circleIt->second)
    {
        PrepareFindcircleDebugRuntime();
        runtime.circles[call.object] = std::make_unique<Findcircle>();
        circleIt = runtime.circles.find(call.object);
    }

    if (call.args.empty())
    {
        ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
        line.status = "BLOCKED";
        line.reason = call.method + " requires one parameter";
        line.timestamp = CurrentTimestamp();

        context.run_state = "blocked";
        context.debug_status = "BLOCKED";
        context.debug_reason = line.reason;
        return true;
    }

    const int value = std::atoi(call.args[0].c_str());

    if (call.method == "setmethod")
        circleIt->second->setmethod(value);
    else if (call.method == "Setgap")
        circleIt->second->Setgap(value);
    else if (call.method == "setthre")
        circleIt->second->setthre(value);
    else if (call.method == "setlinegap")
        circleIt->second->setlinegap(value);
    else if (call.method == "setfitmeasuregap")
        circleIt->second->setfitmeasuregap(value);

    RuntimeObjectView& object = EnsureRuntimeObject(
        context,
        call.object,
        "Findcircle",
        context.line_views[static_cast<std::size_t>(lineIndex)].line_no);

    object.exists_in_parser = true;
    object.last_method = call.method;
    object.last_runtime_status = "runtime_executed";
    object.runtime_state = "runtime_param_set";
    object.last_update_line = context.line_views[static_cast<std::size_t>(lineIndex)].line_no;
    object.display_summary = call.method + "(" + call.params + ")";
    object.stale = false;

    ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
    line.status = "runtime_executed";
    line.reason = "Findcircle." + call.method + " executed";
    line.timestamp = CurrentTimestamp();

    context.current_line = FindNextNonEmptyLine(context, lineIndex + 1);
    context.run_state = "runtime_step";
    context.debug_status = "PENDING";
    context.debug_reason = "Findcircle parameter method executed";

    return true;
}

static void FillFindcircleResultView(RuntimeObjectView& object,
    Findcircle& circle,
    const std::string& methodName)
{
    object.exists_in_parser = true;
    object.visualizable = true;
    object.visual_source = "runtime_object";
    object.stale = false;
    object.last_method = methodName;
    object.last_runtime_status = "runtime_executed";
    object.runtime_state = "runtime_executed";

    object.fit_cx = static_cast<float>(circle.getresultcentx());
    object.fit_cy = static_cast<float>(circle.getresultcenty());
    object.fit_radius = static_cast<float>(circle.getradius());
    object.fit_avgdist = static_cast<float>(circle.getavgdist());

    object.has_fit_result = false;

    if (methodName != "measure" &&
        std::isfinite(object.fit_cx) &&
        std::isfinite(object.fit_cy) &&
        std::isfinite(object.fit_radius) &&
        object.fit_radius > 0.0f)
    {
        object.has_fit_result = true;
    }

    object.has_measure_points = false;
    object.measure_points_xy.clear();

    PointsShape& points = circle.getresultpoints();

    const int pointCount = points.size();
    object.measure_points_count = pointCount;

    for (int i = 0; i < pointCount; ++i)
    {
        const double x = points.getx(i);
        const double y = points.gety(i);

        if (!std::isfinite(x) || !std::isfinite(y))
            continue;

        object.measure_points_xy.push_back(static_cast<float>(x));
        object.measure_points_xy.push_back(static_cast<float>(y));
    }

    object.has_measure_points = !object.measure_points_xy.empty();
    object.valid_points_count =
        static_cast<int>(object.measure_points_xy.size() / 2);
    if (object.has_measure_points || object.has_fit_result)
        object.runtime_state = "geometry_result_available";

    std::ostringstream summary;
    summary << methodName
        << " executed"
        << " | fit=(" << object.fit_cx
        << "," << object.fit_cy
        << ", r=" << object.fit_radius
        << ")"
        << " | avgdist=" << object.fit_avgdist
        << " | points=" << pointCount
        << " | valid_points=" << (object.measure_points_xy.size() / 2);

    object.display_summary = summary.str();

    object.measure_points_count = pointCount;
    object.valid_points_count = static_cast<int>(object.measure_points_xy.size() / 2);
    object.has_measure_points = !object.measure_points_xy.empty();
}
static std::string BuildFindcircleGeometrySummary(const RuntimeObjectView& object)
{
    std::ostringstream ss;

    ss << "geometry: object=" << object.name;

    if (object.has_circle)
    {
        ss << " | roi_circle=("
            << object.circle_cx << ","
            << object.circle_cy << ", r="
            << object.circle_radius << ")";
    }
    else
    {
        ss << " | roi_circle=(none)";
    }

    ss << " | measure_points_count=" << object.measure_points_count;
    ss << " | valid_points_count=" << object.valid_points_count;

    if (object.has_fit_result)
    {
        ss << " | fit_circle=("
            << object.fit_cx << ","
            << object.fit_cy << ", r="
            << object.fit_radius << ")"
            << " | avgdist=" << object.fit_avgdist;
    }
    else
    {
        ss << " | fit_circle=(none)";
    }

    ss << " | has_result_measure="
        << (object.has_result_measure ? "true" : "false");

    if (object.scan_path > 0)
        ss << " | scan_path=" << object.scan_path;

    if (object.image_width > 0 && object.image_height > 0)
        ss << " | image=" << object.image_width << "x" << object.image_height;

    if (object.back_image_width > 0 && object.back_image_height > 0)
        ss << " | back_image=" << object.back_image_width << "x" << object.back_image_height;

    return ss.str();
}

static std::string BuildFindcircleOverlaySummary(const ManualTestContext& context,
    const RuntimeObjectView& object)
{
    std::ostringstream ss;

    ss << "image overlay:"
        << " green_roi_circle=" << (object.has_circle ? "true" : "false")
        << " | red_measure_points=" << object.valid_points_count
        << " | yellow_fit_circle=" << (object.has_fit_result ? "true" : "false")
        << " | source_preview_enabled=false"
        << " | manual_elements_count=0";

    return ss.str();
}

static void UpdateFindcircleDebugSnapshot(ManualTestContext& context,
    const RuntimeObjectView& object,
    int lineNo,
    const std::string& statement)
{
    context.geometry_summary = BuildFindcircleGeometrySummary(object);
    context.image_overlay_summary = BuildFindcircleOverlaySummary(context, object);

    std::ostringstream ss;

    ss << "Findcircle Debug Snapshot Summary\n"
        << "script_path: " << context.loaded_script_path << "\n"
        << "flow_block_id: cximage_find_circle_explore.N0\n"
        << "line: " << lineNo << "\n"
        << "statement: " << statement << "\n"
        << "object: " << object.name << "\n"
        << "runtime_state: " << object.runtime_state << "\n"
        << "last_method: " << object.last_method << "\n"
        << context.geometry_summary << "\n"
        << context.image_overlay_summary << "\n";

    if (!context.current_result_ref.name.empty())
    {
        ss << "result_ref: "
            << context.current_result_ref.name
            << " = "
            << context.current_result_ref.value
            << " | status="
            << context.current_result_ref.status
            << "\n";
    }

    context.findcircle_debug_snapshot_summary = ss.str();
}

static std::string EscapeJsonString(const std::string& s)
{
    std::ostringstream out;

    for (char c : s)
    {
        switch (c)
        {
        case '\\': out << "\\\\"; break;
        case '"': out << "\\\""; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default: out << c; break;
        }
    }

    return out.str();
}

static bool SaveFindcircleDebugSnapshotJson(const ManualTestContext& context,
    std::string& outPath,
    std::string& outReason)
{
    try
    {
        fs::path caseDir(context.case_directory.empty()
            ? "docs/notes/cxscript_case"
            : context.case_directory);

        fs::create_directories(caseDir);

        fs::path filePath = caseDir / "findcircle_debug_snapshot.json";

        std::ofstream file(filePath.string(), std::ios::out | std::ios::trunc);

        if (!file.is_open())
        {
            outReason = "failed to open snapshot file: " + filePath.string();
            return false;
        }

        file << "{\n";
        file << "  \"script_path\": \"" << EscapeJsonString(context.loaded_script_path) << "\",\n";
        file << "  \"flow_block_id\": \"cximage_find_circle_explore.N0\",\n";
        file << "  \"current_line\": " << context.current_line << ",\n";
        file << "  \"runtime_current_status\": \"" << EscapeJsonString(context.runtime_current_status) << "\",\n";

        file << "  \"current_result_ref\": {\n";
        file << "    \"name\": \"" << EscapeJsonString(context.current_result_ref.name) << "\",\n";
        file << "    \"value\": \"" << EscapeJsonString(context.current_result_ref.value) << "\",\n";
        file << "    \"source_object\": \"" << EscapeJsonString(context.current_result_ref.source_object) << "\",\n";
        file << "    \"result_type\": \"" << EscapeJsonString(context.current_result_ref.result_type) << "\",\n";
        file << "    \"status\": \"" << EscapeJsonString(context.current_result_ref.status) << "\",\n";
        file << "    \"reason\": \"" << EscapeJsonString(context.current_result_ref.reason) << "\",\n";
        file << "    \"fit_cx\": " << context.current_result_ref.fit_cx << ",\n";
        file << "    \"fit_cy\": " << context.current_result_ref.fit_cy << ",\n";
        file << "    \"fit_radius\": " << context.current_result_ref.fit_radius << ",\n";
        file << "    \"avgdist\": " << context.current_result_ref.avgdist << ",\n";
        file << "    \"points_count\": " << context.current_result_ref.points_count << ",\n";
        file << "    \"valid_points_count\": " << context.current_result_ref.valid_points_count << "\n";
        file << "  },\n";

        file << "  \"geometry_summary\": \"" << EscapeJsonString(context.geometry_summary) << "\",\n";
        file << "  \"image_overlay_summary\": \"" << EscapeJsonString(context.image_overlay_summary) << "\",\n";
        file << "  \"last_debug_result\": \"" << EscapeJsonString(context.debug_reason) << "\",\n";

        file << "  \"runtime_objects\": [\n";

        for (std::size_t i = 0; i < context.runtime_objects.size(); ++i)
        {
            const RuntimeObjectView& object = context.runtime_objects[i];

            file << "    {\n";
            file << "      \"name\": \"" << EscapeJsonString(object.name) << "\",\n";
            file << "      \"type\": \"" << EscapeJsonString(object.type) << "\",\n";
            file << "      \"runtime_state\": \"" << EscapeJsonString(object.runtime_state) << "\",\n";
            file << "      \"last_method\": \"" << EscapeJsonString(object.last_method) << "\",\n";
            file << "      \"display_summary\": \"" << EscapeJsonString(object.display_summary) << "\",\n";
            file << "      \"visualizable\": " << (object.visualizable ? "true" : "false") << ",\n";
            file << "      \"has_circle\": " << (object.has_circle ? "true" : "false") << ",\n";
            file << "      \"has_measure_points\": " << (object.has_measure_points ? "true" : "false") << ",\n";
            file << "      \"has_fit_result\": " << (object.has_fit_result ? "true" : "false") << ",\n";
            file << "      \"circle\": [" << object.circle_cx << ", " << object.circle_cy << ", "
                << object.circle_inner << ", " << object.circle_radius << "],\n";
            file << "      \"fit_circle\": [" << object.fit_cx << ", " << object.fit_cy << ", "
                << object.fit_radius << "],\n";
            file << "      \"avgdist\": " << object.fit_avgdist << ",\n";
            file << "      \"measure_points_count\": " << object.measure_points_count << ",\n";
            file << "      \"valid_points_count\": " << object.valid_points_count << "\n";
            file << "    }";

            if (i + 1 < context.runtime_objects.size())
                file << ",";

            file << "\n";
        }

        file << "  ]\n";
        file << "}\n";

        outPath = filePath.string();
        outReason = "snapshot saved";
        return true;
    }
    catch (const std::exception& e)
    {
        outReason = std::string("snapshot exception: ") + e.what();
        return false;
    }
    catch (...)
    {
        outReason = "snapshot unknown exception";
        return false;
    }
}
static bool TryExecuteFindcircleRuntimeMethod(ManualTestContext& context,
    int lineIndex,
    const std::string& statement)
{
    const ParsedMethodCall call = ParseMethodCall(statement);

    if (!call.valid)
        return false;

    const bool isFindcircleRuntimeMethod =
        call.method == "measure" ||
        call.method == "fitcircle" ||
        call.method == "FitResultMeasure";

    if (!isFindcircleRuntimeMethod)
        return false;

    DebugCximageRuntime& runtime = CxRuntime(context);

    auto circleIt = runtime.circles.find(call.object);
    if (circleIt == runtime.circles.end() || !circleIt->second)
    {
        ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
        line.status = "BLOCKED";
        line.reason = "Findcircle runtime object missing: " + call.object;
        line.timestamp = CurrentTimestamp();

        context.run_state = "blocked";
        context.debug_status = "BLOCKED";
        context.debug_reason = line.reason;
        return true;
    }

    Image* image = nullptr;

    if (call.method == "measure" || call.method == "FitResultMeasure")
    {
        if (call.args.empty())
        {
            ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
            line.status = "BLOCKED";
            line.reason = call.method + " requires image argument";
            line.timestamp = CurrentTimestamp();

            context.run_state = "blocked";
            context.debug_status = "BLOCKED";
            context.debug_reason = line.reason;
            return true;
        }

        const std::string imageName = StripAddressPrefix(call.args[0]);

        auto imageIt = runtime.images.find(imageName);
        if (imageIt == runtime.images.end() || !imageIt->second)
        {
            ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
            line.status = "BLOCKED";
            line.reason = "Image runtime object missing: " + imageName;
            line.timestamp = CurrentTimestamp();

            context.run_state = "blocked";
            context.debug_status = "BLOCKED";
            context.debug_reason = line.reason;
            return true;
        }

        image = imageIt->second.get();
    }

    RuntimeObjectView& object = EnsureRuntimeObject(
        context,
        call.object,
        "Findcircle",
        context.line_views[static_cast<std::size_t>(lineIndex)].line_no);

    try
    {
        bool balancedFallbackUsed = false;
        if (call.method == "measure")
        {
            circleIt->second->measure(image);
            if (circleIt->second->getresultpoints().size() == 0 && image != nullptr)
            {
                circleIt->second->MeasureBalanced(*image);
                balancedFallbackUsed = true;
            }
            FillFindcircleResultView(object, *circleIt->second, "measure");
        }
        else if (call.method == "fitcircle")
        {
            circleIt->second->fitcircle();
            FillFindcircleResultView(object, *circleIt->second, "fitcircle");
        }
        else if (call.method == "FitResultMeasure")
        {
            circleIt->second->FitResultMeasure(image);
            FillFindcircleResultView(object, *circleIt->second, "FitResultMeasure");
            object.has_result_measure =
                object.has_fit_result || object.has_measure_points;
        }

        std::ostringstream diagnostics;
        diagnostics << object.display_summary
            << " | scan_path=" << circleIt->second->getpath().ElementCount();
        if (image != nullptr)
            diagnostics << " | image=" << image->getWidth() << "x" << image->getHeight();
        Image* backImage = ImageManager::GetBackImage(1);
        diagnostics << " | back_image="
            << (backImage == nullptr ? "null" :
                std::to_string(backImage->getWidth()) + "x" +
                std::to_string(backImage->getHeight()));
        if (balancedFallbackUsed)
            diagnostics << " | fallback=MeasureBalanced";
        object.display_summary = diagnostics.str();
    }
    catch (const std::exception& e)
    {
        ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
        line.status = "BLOCKED";
        line.reason = std::string("Findcircle runtime exception: ") + e.what();
        line.timestamp = CurrentTimestamp();

        object.last_runtime_status = "BLOCKED";
        object.runtime_state = "runtime_exception";
        object.display_summary = line.reason;

        context.run_state = "blocked";
        context.debug_status = "BLOCKED";
        context.debug_reason = line.reason;
        return true;
    }
    catch (...)
    {
        ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
        line.status = "BLOCKED";
        line.reason = "Findcircle runtime unknown exception";
        line.timestamp = CurrentTimestamp();

        object.last_runtime_status = "BLOCKED";
        object.runtime_state = "runtime_exception";
        object.display_summary = line.reason;

        context.run_state = "blocked";
        context.debug_status = "BLOCKED";
        context.debug_reason = line.reason;
        return true;
    }

    ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
    line.status = "runtime_executed";
    line.reason = "Findcircle." + call.method +
        " executed by direct runtime bridge | " + object.display_summary;
    line.timestamp = CurrentTimestamp();

    context.current_line = FindNextNonEmptyLine(context, lineIndex + 1);
    context.run_state = "runtime_step";
    context.debug_status = "PENDING";
    context.debug_reason = line.reason;
    context.runtime_current_status = "PENDING";

    return true;
}

static bool TryExecuteGetResultBinding(ManualTestContext& context,
    int lineIndex,
    const std::string& statement)
{
    std::string s = TrimLine(statement);

    if (s.empty())
        return false;

    if (!s.empty() && s.back() == ';')
        s.pop_back();

    const std::size_t eq = s.find('=');

    if (eq == std::string::npos)
        return false;

    const std::string lhs = TrimLine(s.substr(0, eq));
    const std::string rhs = TrimLine(s.substr(eq + 1));

    const std::string suffix = ".get_result()";
    const std::size_t getPos = rhs.find(suffix);

    if (getPos == std::string::npos)
        return false;

    const std::string sourceObjectName = TrimLine(rhs.substr(0, getPos));

    if (lhs.empty() || sourceObjectName.empty())
        return false;

    RuntimeObjectView* sourceObject = FindRuntimeObjectByName(context, sourceObjectName);

    ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];

    if (sourceObject == nullptr)
    {
        line.status = "PENDING_BINDING";
        line.reason = "get_result source object not found: " + sourceObjectName;
        line.timestamp = CurrentTimestamp();

        UpsertGlobalVariableView(
            context,
            "geometry_ref",
            lhs,
            "uninitialized",
            line.line_no,
            "PENDING_BINDING");

        context.current_result_ref = ResultRefView();
        context.current_result_ref.name = lhs;
        context.current_result_ref.source_object = sourceObjectName;
        context.current_result_ref.result_type = "FindcircleResult";
        context.current_result_ref.status = "PENDING_BINDING";
        context.current_result_ref.reason = line.reason;
        context.current_result_ref.line_no = line.line_no;

        context.debug_status = "PENDING";
        context.debug_reason = line.reason;
        context.run_state = "runtime_step";
        context.current_line = FindNextNonEmptyLine(context, lineIndex + 1);

        return true;
    }

    const bool hasGeometry =
        sourceObject->has_fit_result ||
        sourceObject->runtime_state == "geometry_result_available";

    if (!hasGeometry)
    {
        line.status = "PENDING_BINDING";
        line.reason = "get_result requires a valid fit result; no result fabricated";
        line.timestamp = CurrentTimestamp();

        UpsertGlobalVariableView(
            context,
            "geometry_ref",
            lhs,
            "uninitialized",
            line.line_no,
            "PENDING_BINDING");

        context.current_result_ref = ResultRefView();
        context.current_result_ref.name = lhs;
        context.current_result_ref.source_object = sourceObjectName;
        context.current_result_ref.result_type = "FindcircleResult";
        context.current_result_ref.status = "PENDING_BINDING";
        context.current_result_ref.reason = line.reason;
        context.current_result_ref.line_no = line.line_no;

        // 注意：这里不 BLOCKED，不伪造 PASS，允许脚本继续到 global.current_status。
        context.debug_status = "PENDING";
        context.debug_reason = line.reason;
        context.run_state = "runtime_step";
        context.current_line = FindNextNonEmptyLine(context, lineIndex + 1);

        return true;
    }

    const std::string refValue = "runtime_object:" + sourceObjectName;

    UpsertGlobalVariableView(
        context,
        "geometry_ref",
        lhs,
        refValue,
        line.line_no,
        "geometry_result_available");

    context.current_result_ref = ResultRefView();
    context.current_result_ref.name = lhs;
    context.current_result_ref.value = refValue;
    context.current_result_ref.source_object = sourceObjectName;
    context.current_result_ref.result_type = "FindcircleResult";
    context.current_result_ref.status = "geometry_result_available";
    context.current_result_ref.reason = "bound to runtime object geometry result";
    context.current_result_ref.fit_cx = sourceObject->fit_cx;
    context.current_result_ref.fit_cy = sourceObject->fit_cy;
    context.current_result_ref.fit_radius = sourceObject->fit_radius;
    context.current_result_ref.avgdist = sourceObject->fit_avgdist;
    context.current_result_ref.points_count = sourceObject->measure_points_count;
    context.current_result_ref.valid_points_count = sourceObject->valid_points_count;
    context.current_result_ref.line_no = line.line_no;

    line.status = "runtime_executed";
    line.reason = lhs + " bound to " + refValue;
    line.return_variable = lhs;
    line.timestamp = CurrentTimestamp();

    context.debug_status = "PENDING";
    context.debug_reason = "get_result bound; global.current_status remains PENDING until judge/rule";
    context.runtime_current_status = "PENDING";
    context.run_state = "runtime_step";

    UpdateFindcircleDebugSnapshot(context, *sourceObject, line.line_no, statement);

    context.current_line = FindNextNonEmptyLine(context, lineIndex + 1);

    return true;
}
static bool TryExecutePendingRuntimeMethod(ManualTestContext& context,
    int lineIndex,
    const std::string& statement)
{
    const ParsedMethodCall call = ParseMethodCall(statement);

    if (!call.valid)
        return false;

    const bool isRuntimeAlgorithmCall =
        call.method == "measure" ||
        call.method == "fitcircle" ||
        call.method == "FitResultMeasure" ||
        call.method == "match" ||
        call.method == "learn" ||
        call.method == "infer" ||
        call.method == "predict" ||
        call.method == "optimize_step";

    if (!isRuntimeAlgorithmCall)
        return false;

    RuntimeObjectView& object = EnsureRuntimeObject(
        context,
        call.object,
        call.object.find("circle") != std::string::npos ? "Findcircle" : "unknown",
        context.line_views[static_cast<std::size_t>(lineIndex)].line_no);

    object.exists_in_parser = true;
    object.last_method = call.method;
    object.last_runtime_status = "PENDING";
    object.runtime_state = "runtime_deferred";
    object.last_update_line = context.line_views[static_cast<std::size_t>(lineIndex)].line_no;
    object.display_summary = call.method + " deferred; real parser/runtime callback not connected";
    object.stale = false;

    /*
     * 注意：
     * measure / fitcircle / FitResultMeasure 是真实算法运行行。
     * 当前 debug shim 不能伪造算法结果。
     * 但它也不能 BLOCKED，否则后续 fitcircle / setfitmeasuregap / FitResultMeasure 无法继续行级分析。
     *
     * 因此这里标记为 runtime_deferred + PENDING，
     * 允许调试器继续下一行。
     */
    ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
    line.status = "runtime_deferred";
    line.reason = call.method + " deferred; real parser/runtime callback not connected";
    line.timestamp = CurrentTimestamp();

    context.runtime_current_status = "PENDING";
    context.run_state = "runtime_step";
    context.debug_status = "PENDING";
    context.debug_reason = line.reason;

    context.current_line = FindNextNonEmptyLine(context, lineIndex + 1);

    return true;
}

static bool TryHandleFindcircleGetResult(ManualTestContext& context,
    int lineIndex,
    const std::string& statement)
{
    const ParsedMethodCall call = ParseMethodCall(statement);
    if (!call.valid || call.method != "get_result")
        return false;

    RuntimeObjectView& object = EnsureRuntimeObject(
        context, call.object, "Findcircle",
        context.line_views[static_cast<std::size_t>(lineIndex)].line_no);
    object.last_method = call.method;
    object.last_update_line =
        context.line_views[static_cast<std::size_t>(lineIndex)].line_no;

    ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
    if (object.has_fit_result)
    {
        const std::string geometryRef = "runtime_object:" + call.object;
        object.last_runtime_status = "runtime_executed";
        object.runtime_state = "geometry_result_available";
        UpsertGlobalVariableView(context, "geometry_ref", "global.circle_ref",
            geometryRef, line.line_no, "geometry_result_available");
        line.status = "runtime_executed";
        line.reason = "get_result bound global.circle_ref to " + geometryRef;
    }
    else
    {
        object.last_runtime_status = "PENDING_BINDING";
        object.runtime_state = "pending_binding";
        object.display_summary =
            "get_result requires a valid fit result; no result fabricated";
        UpsertGlobalVariableView(context, "geometry_ref", "global.circle_ref",
            "uninitialized", line.line_no, "PENDING_BINDING");
        line.status = "PENDING_BINDING";
        line.reason = object.display_summary;
    }
    line.timestamp = CurrentTimestamp();

    context.current_line = FindNextNonEmptyLine(context, lineIndex + 1);
    context.run_state = "runtime_step";
    context.debug_status = "PENDING";
    context.debug_reason = line.reason;
    context.runtime_current_status = "PENDING";
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

    if (TryExecuteCurrentStatusAssignment(context, lineIndex, statement))
        return;

    if (TryExecuteSimpleAssignment(context, lineIndex, statement))
        return;

    if (TryExecuteDeclaration(context, lineIndex, statement))
        return;

    if (TryExecuteImageCopyFromMat(context, lineIndex, statement))
        return;

    if (TryExecuteFindcircleSetcircle(context, lineIndex, statement))
        return;

    if (TryExecuteFindcircleParamMethod(context, lineIndex, statement))
        return;

    /*
     * Findcircle 的 measure / fitcircle / FitResultMeasure
     * 当前必须优先走真实 direct runtime bridge。
     */
    if (TryExecuteFindcircleRuntimeMethod(context, lineIndex, statement))
        return;
    if (TryExecuteGetResultBinding(context, lineIndex, statement))
        return;
    if (TryHandleFindcircleGetResult(context, lineIndex, statement))
        return;

    /*
     * 其它模块暂时才走 deferred。
     */
    if (TryExecutePendingRuntimeMethod(context, lineIndex, statement))
        return;

    if (ParseMethodCall(statement).valid)
    {
        const ParsedMethodCall call = ParseMethodCall(statement);

        const bool isRuntimeAlgorithmCall =
            call.method == "measure" ||
            call.method == "fitcircle" ||
            call.method == "FitResultMeasure" ||
            call.method == "match" ||
            call.method == "learn" ||
            call.method == "infer" ||
            call.method == "predict" ||
            call.method == "optimize_step";

        if (isRuntimeAlgorithmCall)
        {
            // 理论上已经被 TryExecutePendingRuntimeMethod 处理。
            // 这里做二次保护。
            if (TryExecutePendingRuntimeMethod(context, lineIndex, statement))
                return;
        }

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

static void CaptureDebugStepSnapshot(ManualTestContext& context, int lineIndex)
{
    if (lineIndex < 0 ||
        lineIndex >= static_cast<int>(context.line_views.size()))
        return;

    const ScriptLineView& line =
        context.line_views[static_cast<std::size_t>(lineIndex)];
    DebugStepSnapshot snapshot;
    snapshot.script_path = context.loaded_script_path;
    snapshot.flow_block_id = "cximage_find_circle_explore.N0";
    snapshot.current_line = line.line_no;
    snapshot.statement = line.statement;
    snapshot.object = line.object;
    snapshot.method = line.method;
    snapshot.params = line.params;
    snapshot.reason = line.reason;
    snapshot.last_debug_result = context.debug_status + ": " + context.debug_reason;
    for (const ScriptVariableView& variable : context.global_variable_views)
        if (variable.name == "global.circle_ref")
            snapshot.current_result_ref = variable.value;

    RuntimeObjectView* object = line.object.empty() ? nullptr :
        FindRuntimeObject(context, line.object);
    if (object == nullptr)
        object = FindRuntimeObject(context, "afindcircle0");
    if (object != nullptr)
    {
        snapshot.runtime_state = object->runtime_state;
        snapshot.object_summary = object->display_summary;
        std::ostringstream geometry;
        geometry << "object=" << object->name << " | roi_circle=";
        if (object->has_circle)
            geometry << "(" << object->circle_cx << "," << object->circle_cy
                     << ",r=" << object->circle_radius << ")";
        else geometry << "none";
        geometry << " | measure_points_count=" << object->measure_points_count
                 << " | valid_points_count=" << object->valid_points_count
                 << " | fit_circle=";
        if (object->has_fit_result)
            geometry << "(" << object->fit_cx << "," << object->fit_cy
                     << ",r=" << object->fit_radius << ")";
        else geometry << "none";
        geometry << " | avgdist=" << object->fit_avgdist
                 << " | has_result_measure="
                 << (object->has_result_measure ? "true" : "false")
                 << " | " << object->display_summary;
        snapshot.geometry_summary = geometry.str();

        std::ostringstream overlay;
        overlay << "green_roi_circle=" << (object->has_circle ? "true" : "false")
                << " | red_measure_points=" << object->valid_points_count
                << " | yellow_fit_circle=" << (object->has_fit_result ? "true" : "false")
                << " | source_preview_enabled="
                << (context.source_preview_enabled ? "true" : "false")
                << " | manual_elements_count=" << context.manual_elements_count;
        snapshot.image_overlay_summary = overlay.str();
    }
    else
    {
        snapshot.runtime_state = line.status;
        snapshot.object_summary = "no runtime object for current line";
        snapshot.geometry_summary = "none";
        snapshot.image_overlay_summary = "none";
    }

    context.current_debug_snapshot = snapshot;
    context.debug_snapshots.push_back(snapshot);
}

static void DebugStepOnceWithSnapshot(ManualTestContext& context)
{
    const int lineIndex = context.current_line;
    DebugStepOnce(context);
    CaptureDebugStepSnapshot(context, lineIndex);
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
    << "  \"flow_block_id\": \"cximage_find_circle_explore.N0\",\n"
    << "  \"current_line\": " << context.current_debug_snapshot.current_line << ",\n"
    << "  \"image_file_path\": \"" << JsonEscape(context.image_file_path) << "\",\n"
    << "  \"data_file_path\": \"" << JsonEscape(context.data_file_path) << "\",\n"
    << "  \"model_file_path\": \"" << JsonEscape(context.model_file_path) << "\",\n"
    << "  \"param_file_path\": \"" << JsonEscape(context.param_file_path) << "\",\n"
    << "  \"current_status\": \"" << JsonEscape(result_status) << "\",\n"
    << "  \"geometry_summary\": \""
    << JsonEscape(context.current_debug_snapshot.geometry_summary) << "\",\n"
    << "  \"image_overlay_summary\": \""
    << JsonEscape(context.current_debug_snapshot.image_overlay_summary) << "\",\n"
    << "  \"last_debug_result\": \""
    << JsonEscape(context.current_debug_snapshot.last_debug_result) << "\"\n}\n";

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
  bool firstVariable = true;
  const auto appendVariable = [&](const ScriptVariableView& variable)
  {
    if (!firstVariable) variables << ",\n";
    firstVariable = false;
    variables << "  {\"scope\":\""
      << (variable.name.rfind("global.", 0) == 0 ? "global" : "local")
      << "\",\"type\":\"" << JsonEscape(variable.type)
      << "\",\"name\":\"" << JsonEscape(variable.name)
      << "\",\"value\":\"" << JsonEscape(variable.value)
      << "\",\"declared_line\":" << variable.declared_line
      << ",\"status\":\"" << JsonEscape(variable.status) << "\"}";
  };
  for (const ScriptVariableView& variable : context.global_variable_views)
    appendVariable(variable);
  for (const ScriptVariableView& variable : context.variable_views)
    appendVariable(variable);
  if (!firstVariable) variables << "\n";
  variables << "]\n";

  std::ostringstream objects;
  objects << "[\n";
  for (std::size_t i = 0; i < context.runtime_objects.size(); ++i)
  {
    const RuntimeObjectView& object = context.runtime_objects[i];
    objects << "  {\"type\":\"" << JsonEscape(object.type)
      << "\",\"name\":\"" << JsonEscape(object.name)
      << "\",\"runtime_state\":\"" << JsonEscape(object.runtime_state)
      << "\",\"method_status\":\"" << JsonEscape(object.last_runtime_status)
      << "\",\"last_method\":\"" << JsonEscape(object.last_method)
      << "\",\"summary\":\"" << JsonEscape(object.display_summary)
      << "\",\"fit_cx\":" << object.fit_cx
      << ",\"fit_cy\":" << object.fit_cy
      << ",\"fit_radius\":" << object.fit_radius
      << ",\"avgdist\":" << object.fit_avgdist
      << ",\"measure_points_count\":" << object.measure_points_count
      << ",\"valid_points_count\":" << object.valid_points_count
      << ",\"has_result_measure\":"
      << (object.has_result_measure ? "true" : "false")
      << ",\"visual_source\":\"" << JsonEscape(object.visual_source) << "\"}"
      << (i + 1 == context.runtime_objects.size() ? "\n" : ",\n");
  }
  objects << "]\n";

  std::ostringstream sourceObjects;
  sourceObjects << "[\n";
  bool firstSourceObject = true;
  for (const ScriptObjectView& object : context.object_views)
  {
    if (object.type != "Image" && object.type != "Findcircle") continue;
    if (!firstSourceObject) sourceObjects << ",\n";
    firstSourceObject = false;
    sourceObjects << "  {\"type\":\"" << JsonEscape(object.type)
      << "\",\"name\":\"" << JsonEscape(object.name)
      << "\",\"declared_line\":" << object.declared_line
      << ",\"status\":\"declared_source_only\""
      << ",\"execution_status\":\"not_executed\"}";
  }
  if (!firstSourceObject) sourceObjects << "\n";
  sourceObjects << "]\n";

  std::ostringstream findcircleSnapshot;
  findcircleSnapshot << "{\n"
    << "  \"script_path\": \""
    << JsonEscape(context.current_debug_snapshot.script_path) << "\",\n"
    << "  \"flow_block_id\": \""
    << JsonEscape(context.current_debug_snapshot.flow_block_id) << "\",\n"
    << "  \"current_line\": " << context.current_debug_snapshot.current_line << ",\n"
    << "  \"current_statement\": \""
    << JsonEscape(context.current_debug_snapshot.statement) << "\",\n"
    << "  \"current_result_ref\": \""
    << JsonEscape(context.current_debug_snapshot.current_result_ref) << "\",\n"
    << "  \"geometry_summary\": \""
    << JsonEscape(context.current_debug_snapshot.geometry_summary) << "\",\n"
    << "  \"image_overlay_summary\": \""
    << JsonEscape(context.current_debug_snapshot.image_overlay_summary) << "\",\n"
    << "  \"last_debug_result\": \""
    << JsonEscape(context.current_debug_snapshot.last_debug_result) << "\"\n}\n";

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
    << "  \"flow_block_id\": \"cximage_find_circle_explore.N0\",\n"
    << "  \"script_path\": \"" << JsonEscape(context.loaded_script_path) << "\",\n"
    << "  \"line_no\": " << (current == nullptr ? 0 : current->line_no) << ",\n"
    << "  \"statement\": \"" << JsonEscape(current == nullptr ? "" : current->statement) << "\",\n"
    << "  \"object\": \"" << JsonEscape(current == nullptr ? "" : current->object) << "\",\n"
    << "  \"method\": \"" << JsonEscape(current == nullptr ? "" : current->method) << "\",\n"
    << "  \"params\": \"" << JsonEscape(current == nullptr ? "" : current->params) << "\",\n"
    << "  \"current_status\": \"" << JsonEscape(result_status) << "\",\n"
    << "  \"geometry_summary\": \""
    << JsonEscape(context.current_debug_snapshot.geometry_summary) << "\",\n"
    << "  \"image_overlay_summary\": \""
    << JsonEscape(context.current_debug_snapshot.image_overlay_summary) << "\",\n"
    << "  \"last_debug_result\": \""
    << JsonEscape(context.current_debug_snapshot.last_debug_result) << "\",\n"
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
    WriteTextFile(root / "source_object_state.json", sourceObjects.str()) &&
    WriteTextFile(root / "object_state.json", objects.str()) &&
    WriteTextFile(root / "findcircle_debug_snapshot.json", findcircleSnapshot.str()) &&
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

  m_manualTest.source_preview_enabled = m_showSourcePreviewOverlay;
  m_manualTest.manual_elements_count =
    static_cast<int>(m_annotationLayer.Elements().size());

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
  ImGui::Text("Current Flow Block");
  ImGui::Text("Flow Block: cximage_find_circle_explore.N0");
  ImGui::TextWrapped("Current Script: %s",
    m_manualTest.loaded_script_path.empty() ?
      "cxparser/cxscript/module/cximage/find_circle_direct_test.cxsc" :
      m_manualTest.loaded_script_path.c_str());
  RuntimeObjectView* currentCircle = FindRuntimeObject(m_manualTest, "afindcircle0");
  ImGui::Text("Current Runtime Object: afindcircle0");
  if (currentCircle == nullptr)
    ImGui::TextDisabled("Current Geometry Result: unavailable");
  else
  {
    ImGui::TextWrapped("Current Geometry Result: %s",
                       currentCircle->display_summary.c_str());
    ImGui::Text("fit=(%.3f, %.3f, r=%.3f) | avgdist=%.3f | points=%d",
                currentCircle->fit_cx, currentCircle->fit_cy,
                currentCircle->fit_radius, currentCircle->fit_avgdist,
                static_cast<int>(currentCircle->measure_points_xy.size() / 2));
  }
  std::string currentResultRef;
  for (const ScriptVariableView& variable : m_manualTest.global_variable_views)
    if (variable.name == "global.circle_ref") currentResultRef = variable.value;
 
  ImGui::TextWrapped("Current Result Ref: %s",
      m_manualTest.current_result_ref.name.empty()
      ? "uninitialized"
      : m_manualTest.current_result_ref.value.c_str());

  if (!m_manualTest.current_result_ref.name.empty())
  {
      ImGui::TextWrapped("result name: %s",
          m_manualTest.current_result_ref.name.c_str());

      ImGui::TextWrapped("source object: %s",
          m_manualTest.current_result_ref.source_object.c_str());

      ImGui::TextWrapped("result type: %s",
          m_manualTest.current_result_ref.result_type.c_str());

      ImGui::TextWrapped("result status: %s",
          m_manualTest.current_result_ref.status.c_str());

      ImGui::TextWrapped(
          "fit=(%.3f, %.3f, r=%.3f) | avgdist=%.4f | points=%d | valid_points=%d",
          m_manualTest.current_result_ref.fit_cx,
          m_manualTest.current_result_ref.fit_cy,
          m_manualTest.current_result_ref.fit_radius,
          m_manualTest.current_result_ref.avgdist,
          m_manualTest.current_result_ref.points_count,
          m_manualTest.current_result_ref.valid_points_count);

      if (!m_manualTest.current_result_ref.reason.empty())
      {
          ImGui::TextWrapped("result reason: %s",
              m_manualTest.current_result_ref.reason.c_str());
      }
  }



  ImGui::TextWrapped("Method Chain: setcircle -> setmethod -> Setgap -> setthre -> setlinegap -> measure -> fitcircle -> setfitmeasuregap -> FitResultMeasure -> get_result");
  ImGui::TextWrapped("Debug Line Targets: copyFromMat / setcircle / measure / fitcircle / setfitmeasuregap / FitResultMeasure / get_result");
  ImGui::TextWrapped("Expected Geometry: ROI circle / measure points / fit circle / final result overlay");
  ImGui::TextWrapped("Status: runtime_executed = C++ call completed; geometry_result_available = geometry exists; PENDING = case not judged; PASS = judge/rule only; BLOCKED = cannot continue; PENDING_BINDING = statement recognized but result binding unavailable");

  ImGui::Separator();
  ImGui::Text("Script Debug Compiler");
  ImGui::Text("run_state: %s", m_manualTest.run_state.c_str());
  const auto syncGeometryResult = [&]()
  {
    for (const ScriptVariableView& variable : m_manualTest.global_variable_views)
    {
      if (variable.name != "global.circle_ref" ||
          variable.value.rfind("runtime_object:", 0) != 0) continue;
      m_scriptResult.result_ref = variable.value;
      m_scriptResult.overlay_ref = variable.value;
      return;
    }
    m_scriptResult.result_ref.clear();
    m_scriptResult.overlay_ref.clear();
  };
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
          DebugStepOnceWithSnapshot(m_manualTest);

          // 遇到真实 runtime 缺失的算法行，先停下来让用户看。
          if (m_manualTest.run_state == "blocked")
              break;
      }

      m_scriptResult.status = m_manualTest.debug_status;
      m_scriptResult.reason = m_manualTest.debug_reason;
      m_scriptResult.runtime_fillback_status = "debug_run";
      syncGeometryResult();
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

      DebugStepOnceWithSnapshot(m_manualTest);

      m_scriptResult.status = m_manualTest.debug_status;
      m_scriptResult.reason = m_manualTest.debug_reason;
      m_scriptResult.runtime_fillback_status = "debug_step";
      syncGeometryResult();
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
          DebugStepOnceWithSnapshot(m_manualTest);

          if (m_manualTest.run_state == "blocked")
              break;
      }

      m_scriptResult.status = m_manualTest.debug_status;
      m_scriptResult.reason = m_manualTest.debug_reason;
      m_scriptResult.runtime_fillback_status = "debug_continue";
      syncGeometryResult();
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
  {
    std::string boundScript;
    const bool boundReady = !m_manualTest.bound_state_script_path.empty() &&
      ReadTextFile(ResolveWorkspaceFile(m_manualTest.bound_state_script_path).generic_string(),
                   boundScript);
    if (boundReady)
    {
      m_manualTest.editor_text = boundScript;
      m_manualTest.loaded_script_path = m_manualTest.bound_state_script_path;
      m_manualTest.editor_source = "bound_state";
      m_manualTest.analyzed_text.clear();
    }
    if (!boundReady)
    {
      m_manualTest.run_state = "blocked";
      m_manualTest.debug_status = "BLOCKED";
      m_manualTest.debug_reason = "bound N0 script unavailable";
    }
    else
    {
      AnalyzeScript(m_manualTest);
      ResetDebugRuntimeForReplay(m_manualTest);
      m_manualTest.stop_requested = false;
      int guard = 0;
      const int maxSteps = static_cast<int>(m_manualTest.line_views.size()) * 4 + 16;
      while (!m_manualTest.stop_requested &&
             m_manualTest.current_line < static_cast<int>(m_manualTest.line_views.size()) &&
             guard++ < maxSteps)
      {
        DebugStepOnceWithSnapshot(m_manualTest);
        if (m_manualTest.run_state == "blocked") break;
      }
    }
    m_scriptResult.status = m_manualTest.run_state == "blocked" ?
      "BLOCKED" : "PENDING";
    m_scriptResult.reason = m_manualTest.debug_reason;
    m_scriptResult.runtime_fillback_status = "bound_block_debug_steps";
    syncGeometryResult();
  }
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
  ImGui::Separator();
  ImGui::Text("Findcircle Debug Snapshot Summary");
  const DebugStepSnapshot& debugSnapshot = m_manualTest.current_debug_snapshot;
  ImGui::TextWrapped("script_path: %s", debugSnapshot.script_path.empty() ?
                     "(none)" : debugSnapshot.script_path.c_str());
  ImGui::TextWrapped("flow_block_id: %s", debugSnapshot.flow_block_id.empty() ?
                     "(none)" : debugSnapshot.flow_block_id.c_str());
  ImGui::Text("line: %d", debugSnapshot.current_line);
  ImGui::TextWrapped("statement: %s", debugSnapshot.statement.empty() ?
                     "(none)" : debugSnapshot.statement.c_str());
  ImGui::TextWrapped("object: %s | method: %s | params: %s",
                     debugSnapshot.object.empty() ? "(none)" : debugSnapshot.object.c_str(),
                     debugSnapshot.method.empty() ? "(none)" : debugSnapshot.method.c_str(),
                     debugSnapshot.params.empty() ? "(none)" : debugSnapshot.params.c_str());
  ImGui::Text("Current Runtime Object");
  ImGui::TextWrapped("state: %s", debugSnapshot.runtime_state.empty() ?
                     "(none)" : debugSnapshot.runtime_state.c_str());
  ImGui::TextWrapped("summary: %s", debugSnapshot.object_summary.empty() ?
                     "(none)" : debugSnapshot.object_summary.c_str());
  ImGui::Text("Current Geometry Result");
  ImGui::TextWrapped("geometry: %s", debugSnapshot.geometry_summary.empty() ?
                     "(none)" : debugSnapshot.geometry_summary.c_str());
  ImGui::TextWrapped("image overlay: %s", debugSnapshot.image_overlay_summary.empty() ?
                     "(none)" : debugSnapshot.image_overlay_summary.c_str());
  ImGui::TextWrapped("current_result_ref: %s", debugSnapshot.current_result_ref.empty() ?
                     "(uninitialized)" : debugSnapshot.current_result_ref.c_str());
  ImGui::TextWrapped("last_debug_result: %s", debugSnapshot.last_debug_result.empty() ?
                     "(none)" : debugSnapshot.last_debug_result.c_str());
  ImGui::TextWrapped("reason: %s", debugSnapshot.reason.empty() ?
                     "(none)" : debugSnapshot.reason.c_str());
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
  for (const ScriptVariableView& variable : m_manualTest.global_variable_views)
  {
    if (variable.name != "global.circle_ref" ||
        variable.value.rfind("runtime_object:", 0) != 0) continue;
    const std::string objectName = variable.value.substr(15);
    RuntimeObjectView* geometry = FindRuntimeObject(m_manualTest, objectName);
    if (geometry == nullptr) continue;
    ImGui::TextWrapped("circle_ref: %s", variable.value.c_str());
    ImGui::Text("source_object: %s | result_type: FindcircleResult",
                objectName.c_str());
    ImGui::Text("fit_cx: %.3f | fit_cy: %.3f | fit_radius: %.3f",
                geometry->fit_cx, geometry->fit_cy, geometry->fit_radius);
    ImGui::Text("avgdist: %.3f | measure_points_count: %d | valid_points_count: %d",
                geometry->fit_avgdist,
                geometry->measure_points_count,
                geometry->valid_points_count);
    ImGui::Text("has_result_measure: %s | result_status: %s",
                geometry->has_result_measure ? "true" : "false",
                geometry->runtime_state.c_str());
  }
  ImGui::Spacing();
  drawVariableList("Local Variables", m_manualTest.variable_views);

  ImGui::Separator();
  ImGui::Text("Runtime Object Table");
  if (!m_manualTest.findcircle_debug_snapshot_summary.empty())
  {
      ImGui::Separator();
      ImGui::TextUnformatted("Findcircle Debug Snapshot Summary");
      ImGui::TextWrapped("%s", m_manualTest.findcircle_debug_snapshot_summary.c_str());
  }

  if (!m_manualTest.geometry_summary.empty())
  {
      ImGui::TextWrapped("%s", m_manualTest.geometry_summary.c_str());
  }

  if (!m_manualTest.image_overlay_summary.empty())
  {
      ImGui::TextWrapped("%s", m_manualTest.image_overlay_summary.c_str());
  }
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
    if (object.type == "Findcircle")
    {
      ImGui::Text("fit=(%.3f, %.3f, r=%.3f) | avgdist=%.3f",
                  object.fit_cx, object.fit_cy,
                  object.fit_radius, object.fit_avgdist);
      ImGui::Text("measure_points_count=%d | valid_points_count=%d",
                  object.measure_points_count, object.valid_points_count);
    }
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
  ImGui::Text("Source Object Panel");
  ImGui::TextDisabled("Static .cxsc declarations only; never a runtime result");
  bool hasSourceObject = false;
  for (const ScriptObjectView& object : m_manualTest.object_views)
  {
    if (object.type != "Image" && object.type != "Findcircle") continue;
    ImGui::BulletText("%s %s", object.type.c_str(), object.name.c_str());
    ImGui::Text("declared_line: %d | status: declared_source_only",
                object.declared_line);
    ImGui::TextDisabled("source_analyzed / not_executed");
    hasSourceObject = true;
  }
  if (!hasSourceObject) ImGui::TextDisabled("no Image/Findcircle declaration");

  ImGui::Separator();
  ImGui::Text("Direct Capability Directory");
  const char* modules[] = {"cximage", "torch", "mlpack", "ensmallen"};
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
      std::string savedPath;
      std::string saveReason;

      if (SaveFindcircleDebugSnapshotJson(m_manualTest, savedPath, saveReason))
      {
          m_scriptResult.status = "PENDING";
          m_scriptResult.reason = "Findcircle debug snapshot saved: " + savedPath;
          m_scriptResult.runtime_fillback_status = "case_snapshot_saved";
      }
      else
      {
          m_scriptResult.status = "PENDING";
          m_scriptResult.reason = saveReason;
          m_scriptResult.runtime_fillback_status = "case_snapshot_save_failed";
      }
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
