#include "ViewController.h"
#include "Image.h"
#include "Findcircle.h"
#include "findline.h"
#include "CircleRingGauge.h"
#include "CxImageRuntimeOverlay.h"
#include "imagemanager.h"
#include "CxScriptImageEvidenceAnalyzer.h"
#include "CxScriptGeometryFrameProbe.h"
#include "FindlineParameterPolicy.h"
#include "CxScriptCatalogRuntime.h"

#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <ctime>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <unordered_map>


#include <iomanip>
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

#ifdef _WIN32
  wchar_t exePath[MAX_PATH];
  if (GetModuleFileNameW(NULL, exePath, MAX_PATH))
  {
    const fs::path exeDir = fs::path(exePath).parent_path();
    const fs::path exeRelative = exeDir / requested;
    if (fs::exists(exeRelative)) return fs::absolute(exeRelative);
  }
#endif

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

static std::string CxDebugJsonEscape(const std::string& text)
{
    std::ostringstream out;

    for (char ch : text)
    {
        switch (ch)
        {
        case '\\':
            out << "\\\\";
            break;
        case '"':
            out << "\\\"";
            break;
        case '\n':
            out << "\\n";
            break;
        case '\r':
            out << "\\r";
            break;
        case '\t':
            out << "\\t";
            break;
        default:
            out << ch;
            break;
        }
    }

    return out.str();
}

static fs::path CxDebugLogDirectory()
{
    return fs::path("docs") / "notes" / "cxscript_case";
}

static fs::path CxDebugRuntimeLogPath()
{
    return CxDebugLogDirectory() / "cxscript_debug_runtime_latest.jsonl";
}

static fs::path CxDebugSnapshotPath()
{
    return CxDebugLogDirectory() / "cxscript_debug_snapshot_latest.txt";
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

static void ResetCxDebugRuntimeLog(const ManualTestContext& context,
    const std::string& reason)
{
    try
    {
        fs::create_directories(CxDebugLogDirectory());

        std::ofstream file(CxDebugRuntimeLogPath().string(),
            std::ios::out | std::ios::trunc);

        if (!file.is_open())
            return;

        file << "{"
            << "\"event\":\"log_reset\","
            << "\"time\":\"" << CxDebugJsonEscape(CurrentTimestamp()) << "\","
            << "\"reason\":\"" << CxDebugJsonEscape(reason) << "\","
            << "\"script_path\":\"" << CxDebugJsonEscape(context.loaded_script_path) << "\","
            << "\"flow_block\":\"cximage_find_circle_explore.N0\""
            << "}\n";
    }
    catch (...)
    {
        // log ä¸èƒ½å½±å“ä¸»è°ƒè¯•æµç¨‹
    }
}

static void AppendCxDebugEvent(const ManualTestContext& context,
    const std::string& event,
    int lineNo,
    const std::string& statement,
    const std::string& object,
    const std::string& method,
    const std::string& status,
    const std::string& reason,
    const std::string& summary)
{
    try
    {
        fs::create_directories(CxDebugLogDirectory());

        std::ofstream file(CxDebugRuntimeLogPath().string(),
            std::ios::out | std::ios::app);

        if (!file.is_open())
            return;

        file << "{"
            << "\"event\":\"" << CxDebugJsonEscape(event) << "\","
            << "\"time\":\"" << CxDebugJsonEscape(CurrentTimestamp()) << "\","
            << "\"flow_block\":\"cximage_find_circle_explore.N0\","
            << "\"script_path\":\"" << CxDebugJsonEscape(context.loaded_script_path) << "\","
            << "\"line_no\":" << lineNo << ","
            << "\"statement\":\"" << CxDebugJsonEscape(statement) << "\","
            << "\"object\":\"" << CxDebugJsonEscape(object) << "\","
            << "\"method\":\"" << CxDebugJsonEscape(method) << "\","
            << "\"status\":\"" << CxDebugJsonEscape(status) << "\","
            << "\"reason\":\"" << CxDebugJsonEscape(reason) << "\","
            << "\"run_state\":\"" << CxDebugJsonEscape(context.run_state) << "\","
            << "\"runtime_current_status\":\"" << CxDebugJsonEscape(context.runtime_current_status) << "\","
            << "\"debug_status\":\"" << CxDebugJsonEscape(context.debug_status) << "\","
            << "\"debug_reason\":\"" << CxDebugJsonEscape(context.debug_reason) << "\","
            << "\"summary\":\"" << CxDebugJsonEscape(summary) << "\""
            << "}\n";
    }
    catch (...)
    {
        // log ä¸èƒ½å½±å“ä¸»è°ƒè¯•æµç¨‹
    }
}
static void AppendCxDebugRuntimeObjectsSnapshot(const ManualTestContext& context,
    const std::string& event)
{
    try
    {
        fs::create_directories(CxDebugLogDirectory());

        std::ofstream file(CxDebugRuntimeLogPath().string(),
            std::ios::out | std::ios::app);

        if (!file.is_open())
            return;

        file << "{"
            << "\"event\":\"" << CxDebugJsonEscape(event) << "\","
            << "\"time\":\"" << CxDebugJsonEscape(CurrentTimestamp()) << "\","
            << "\"script_path\":\"" << CxDebugJsonEscape(context.loaded_script_path) << "\","
            << "\"flow_block\":\"cximage_find_circle_explore.N0\","
            << "\"runtime_objects\":[";

        for (std::size_t i = 0; i < context.runtime_objects.size(); ++i)
        {
            const RuntimeObjectView& object = context.runtime_objects[i];

            file << "{"
                << "\"name\":\"" << CxDebugJsonEscape(object.name) << "\","
                << "\"type\":\"" << CxDebugJsonEscape(object.type) << "\","
                << "\"declared_line\":" << object.declared_line << ","
                << "\"exists_in_parser\":" << (object.exists_in_parser ? "true" : "false") << ","
                << "\"runtime_state\":\"" << CxDebugJsonEscape(object.runtime_state) << "\","
                << "\"last_runtime_status\":\"" << CxDebugJsonEscape(object.last_runtime_status) << "\","
                << "\"last_method\":\"" << CxDebugJsonEscape(object.last_method) << "\","
                << "\"last_update_line\":" << object.last_update_line << ","
                << "\"display_summary\":\"" << CxDebugJsonEscape(object.display_summary) << "\","
                << "\"visualizable\":" << (object.visualizable ? "true" : "false") << ","
                << "\"visual_source\":\"" << CxDebugJsonEscape(object.visual_source) << "\","
                << "\"stale\":" << (object.stale ? "true" : "false") << ","
                << "\"has_circle\":" << (object.has_circle ? "true" : "false") << ","
                << "\"circle\":[" << object.circle_cx << ","
                << object.circle_cy << ","
                << object.circle_inner << ","
                << object.circle_radius << "],"
                << "\"has_measure_points\":" << (object.has_measure_points ? "true" : "false") << ","
                << "\"measure_points_count\":" << object.measure_points_count << ","
                << "\"valid_points_count\":" << object.valid_points_count << ","
                << "\"has_fit_result\":" << (object.has_fit_result ? "true" : "false") << ","
                << "\"fit_circle\":[" << object.fit_cx << ","
                << object.fit_cy << ","
                << object.fit_radius << "],"
                << "\"avgdist\":" << object.fit_avgdist
                << ",\"has_circle_roi_outer_polyline\":" << (object.has_circle_roi_outer_polyline ? "true" : "false")
                << ",\"has_circle_roi_inner_polyline\":" << (object.has_circle_roi_inner_polyline ? "true" : "false")
                << ",\"circle_roi_segment_count\":" << object.circle_roi_segment_count
                << ",\"has_fit_circle_polyline\":" << (object.has_fit_circle_polyline ? "true" : "false")
                << ",\"fit_circle_segment_count\":" << object.fit_circle_segment_count
                << ",\"display_version\":" << object.display_version
                << ",\"has_line_roi\":" << (object.has_line_roi ? "true" : "false")
                << ",\"line_roi\":[" << object.line_x0 << ","
                << object.line_y0 << ","
                << object.line_x1 << ","
                << object.line_y1 << "]"
                << ",\"has_line_scan_box\":" << (object.has_line_scan_box ? "true" : "false")
                << ",\"line_scan_half_width\":" << object.line_scan_half_width
                << ",\"line_scan_box_xy\":["
                << object.line_scan_box_xy[0] << "," << object.line_scan_box_xy[1] << ","
                << object.line_scan_box_xy[2] << "," << object.line_scan_box_xy[3] << ","
                << object.line_scan_box_xy[4] << "," << object.line_scan_box_xy[5] << ","
                << object.line_scan_box_xy[6] << "," << object.line_scan_box_xy[7] << "]"
                << ",\"line_pointsw_count\":" << object.line_pointsw_count
                << ",\"line_pointsh_count\":" << object.line_pointsh_count
                << ",\"line_measure_points_count\":" << object.line_measure_points_count
                << ",\"valid_line_points_count\":" << object.valid_line_points_count
                << ",\"has_fit_line\":" << (object.has_fit_line ? "true" : "false")
                << ",\"fit_line\":[" << object.fit_line_x0 << ","
                << object.fit_line_y0 << ","
                << object.fit_line_x1 << ","
                << object.fit_line_y1 << "]"
                << ",\"line_avgdist\":" << object.line_avgdist
                << ",\"line_fit_mode\":\"" << CxDebugJsonEscape(object.line_fit_mode) << "\""
                << ",\"line_fit_status\":\"" << CxDebugJsonEscape(object.line_fit_status) << "\""
                << ",\"line_measure_status\":\"" << CxDebugJsonEscape(object.line_measure_status) << "\""
                << ",\"line_measure_hint\":\"" << CxDebugJsonEscape(object.line_measure_hint) << "\""
                << "}";

            if (i + 1 < context.runtime_objects.size())
                file << ",";
        }

        file << "]}";
        file << "\n";
    }
    catch (...)
    {
    }
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
static std::string BuildFindcircleGeometrySummary(const RuntimeObjectView& object)
{
    std::ostringstream ss;

    ss << "geometry: object=" << object.name;

    if (object.has_circle)
    {
        ss << " | roi_circle=("
           << object.circle_cx << "," << object.circle_cy
           << ", inner=" << object.circle_inner
           << ", r=" << object.circle_radius << ")";
    }
    else
    {
        ss << " | roi_circle=(none)";
    }

    ss << " | measure_points_count=" << object.measure_points_count
       << " | valid_points_count=" << object.valid_points_count;

    if (object.has_fit_result)
    {
        ss << " | fit_circle=("
           << object.fit_cx << "," << object.fit_cy
           << ", r=" << object.fit_radius << ")"
           << " | avgdist=" << object.fit_avgdist;
    }
    else
    {
        ss << " | fit_circle=(none)";
    }

    ss << " | has_result_measure="
       << (object.has_result_measure ? "true" : "false")
       << " | roi_outer_polyline=" << (object.has_circle_roi_outer_polyline ? "true" : "false")
       << " | roi_inner_polyline=" << (object.has_circle_roi_inner_polyline ? "true" : "false")
       << " | roi_segments=" << object.circle_roi_segment_count
       << " | fit_circle_polyline=" << (object.has_fit_circle_polyline ? "true" : "false")
       << " | fit_segments=" << object.fit_circle_segment_count
       << " | display_version=" << object.display_version
       << " | circle_geometry_ready="
       << (object.circle_measure_geometry_ready ? "true" : "false")
       << " | circle_dirty="
       << (object.circle_measure_geometry_dirty ? "true" : "false")
       << " | circle_scan_lines="
       << object.circle_scan_line_count
       << " | circle_scan_len="
       << object.circle_scan_line_length
       << " | circle_process_w="
       << object.circle_process_width
       << " | circle_measure_source="
       << object.circle_measure_source
       << " | circle_failure_stage="
       << object.circle_measure_failure_stage;

    return ss.str();
}
static std::string BuildFindlineGeometrySummary(const RuntimeObjectView& object)
{
    std::ostringstream ss;

    ss << "geometry: object=" << object.name;

    if (object.has_line_roi)
    {
        ss << " | line_roi=("
           << object.line_x0 << "," << object.line_y0
           << ")->(" << object.line_x1 << "," << object.line_y1 << ")";
    }
    else
    {
        ss << " | line_roi=(none)";
    }

    ss << " | scan_half_width=" << object.line_scan_half_width
       << " | linegap=" << object.linegap
       << " | line_scan_box_xy=[" << object.line_scan_box_xy[0] << "," << object.line_scan_box_xy[1] << ","
       << object.line_scan_box_xy[2] << "," << object.line_scan_box_xy[3] << ","
       << object.line_scan_box_xy[4] << "," << object.line_scan_box_xy[5] << ","
       << object.line_scan_box_xy[6] << "," << object.line_scan_box_xy[7] << "]"
       << " | measure_points_count=" << object.line_measure_points_count
       << " | valid_points_count=" << object.valid_line_points_count
       << " | pointsw=" << object.line_pointsw_count
       << " | pointsh=" << object.line_pointsh_count
       << " | seek_points=" << object.line_seek_points_count
       << " | edgebands=" << object.line_edgeband_count
       << " | chain=" << object.line_chain_length
       << " | measure_failure_stage=" << object.line_measure_failure_stage
       << " | image_ready=" << (object.line_measure_image_ready ? "true" : "false")
       << " | image_size=" << object.line_measure_image_width
       << "x" << object.line_measure_image_height
       << "x" << object.line_measure_image_channels
       << " | roi_intersects_image="
       << (object.line_measure_roi_intersects_image ? "true" : "false")
       << " | threshold=" << object.line_measure_threshold
       << " | max_gradient=" << object.line_measure_max_gradient
       << " | profile_count=" << object.line_measure_profile_count
       << " | sampled_pixels=" << object.line_measure_sampled_pixel_count
       << " | measure_source=" << object.line_measure_source
       << " | fallback_used="
       << (object.line_measure_fallback_used ? "true" : "false")
       << " | original_points="
       << object.line_measure_original_point_count
       << " | original_edgebands="
       << object.line_measure_original_edgeband_count
       << " | original_chain="
       << object.line_measure_original_chain_length
       << " | fit_mode=" << object.line_fit_mode
       << " | fit_status=" << object.line_fit_status
       << " | has_line_scan_box=" << (object.has_line_scan_box ? "true" : "false")
       << " | display_version=" << object.display_version
       << " | geometry_request_valid=" << (object.line_measure_geometry_request_valid ? "true" : "false")
       << " | geometry_ready=" << (object.line_measure_geometry_ready ? "true" : "false")
       << " | geometry_dirty=" << (object.line_measure_geometry_dirty ? "true" : "false")
       << " | geometry_half_width=" << object.line_measure_geometry_half_width
       << " | scan_w=" << object.line_original_scan_w_count
       << " | scan_h=" << object.line_original_scan_h_count
       << " | scan_w_len=" << object.line_original_scan_w_length
       << " | scan_h_len=" << object.line_original_scan_h_length
       << " | process_w=" << object.line_original_process_width;

    return ss.str();
}
static std::string BuildGeometrySummary(const RuntimeObjectView& object)
{
    if (object.type == "Findline")
        return BuildFindlineGeometrySummary(object);

    return BuildFindcircleGeometrySummary(object);
}

static std::string BuildFindcircleOverlaySummary(const ManualTestContext& context,
    const RuntimeObjectView& object)
{
    std::ostringstream ss;

    ss << "image overlay:"
       << " green_roi_circle=" << (object.has_circle ? "true" : "false")
       << " | green_roi_polyline=" << (object.has_circle_roi_outer_polyline ? "true" : "false")
       << " | red_measure_points=" << object.valid_points_count
       << " | yellow_fit_circle=" << (object.has_fit_result ? "true" : "false")
       << " | yellow_fit_polyline=" << (object.has_fit_circle_polyline ? "true" : "false")
       << " | source_preview_enabled=" << (context.source_preview_enabled ? "true" : "false")
       << " | manual_elements_count=" << context.manual_elements_count;

    return ss.str();
}
static std::string BuildFindlineOverlaySummary(const ManualTestContext& context,
    const RuntimeObjectView& object)
{
    std::ostringstream ss;

    ss << "image overlay:"
       << " green_line_roi=" << (object.has_line_roi ? "true" : "false")
       << " | green_line_scan_box=" << (object.has_line_scan_box ? "true" : "false")
       << " | blue_seek_points=" << object.line_seek_points_count
       << " | red_measure_points=" << object.valid_line_points_count
       << " | yellow_fit_line=" << (object.has_fit_line ? "true" : "false")
       << " | fitline_pending="
       << ((object.runtime_state == "fitline_pending_binding" ||
            object.runtime_state == "fitline_pending_implementation")
               ? "true"
               : "false")
       << " | source_preview_enabled=false"
       << " | manual_elements_count=0";

    return ss.str();
}

static std::string BuildOverlaySummary(const ManualTestContext& context,
    const RuntimeObjectView& object)
{
    if (object.type == "Findline")
        return BuildFindlineOverlaySummary(context, object);

    return BuildFindcircleOverlaySummary(context, object);
}

static void UpdateFindcircleDebugSnapshot(ManualTestContext& context,
    const RuntimeObjectView& object,
    int lineNo,
    const std::string& statement)
{
    context.geometry_summary = BuildGeometrySummary(object);
    context.image_overlay_summary = BuildOverlaySummary(context, object);

    std::ostringstream ss;

    ss << object.type << " Debug Snapshot Summary\n"
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

static void RefreshSnapshotFromCurrentResultRef(ManualTestContext& context)
{
    if (context.current_result_ref.source_object.empty())
        return;

    RuntimeObjectView* object =
        FindRuntimeObjectByName(context, context.current_result_ref.source_object);

    if (object == nullptr)
        return;

    UpdateFindcircleDebugSnapshot(
        context,
        *object,
        context.current_result_ref.line_no,
        context.current_result_ref.name + " = " + context.current_result_ref.value);
}
static std::string BuildDebugCursorText(const ManualTestContext& context)
{
    if (context.run_state == "runtime_finished" ||
        context.current_line >= static_cast<int>(context.line_views.size()))
    {
        return "END";
    }

    if (context.current_line >= 0 &&
        context.current_line < static_cast<int>(context.line_views.size()))
    {
        const ScriptLineView& line =
            context.line_views[static_cast<std::size_t>(context.current_line)];

        std::ostringstream ss;
        ss << "line_no=" << line.line_no
            << ", index=" << context.current_line;
        return ss.str();
    }

    return "INVALID";
}
static int LastExecutedLineNo(const ManualTestContext& context)
{
    int last = 0;

    for (const ScriptLineView& line : context.line_views)
    {
        if (line.status == "runtime_executed" ||
            line.status == "runtime_deferred" ||
            line.status == "control_true" ||
            line.status == "control_false" ||
            line.status == "structural")
        {
            last = line.line_no;
        }
    }

    return last;
}

static bool SaveCxDebugSnapshotText(ManualTestContext& context,
    const fs::path& path,
    std::string& outReason);

static bool SaveCxDebugSnapshotText(ManualTestContext& context,
    std::string& outPath,
    std::string& outReason)
{
    fs::path path = CxDebugSnapshotPath();
    outPath = path.string();
    return SaveCxDebugSnapshotText(context, path, outReason);
}

static bool SaveCxDebugSnapshotText(ManualTestContext& context,
    const fs::path& path,
    std::string& outReason)
{
    try
    {
        fs::create_directories(path.parent_path());

        std::ofstream file(path.string(), std::ios::out | std::ios::trunc);

        if (!file.is_open())
        {
            outReason = "failed to open debug snapshot file: " + path.string();
            return false;
        }

        file << "CxScript Debug Snapshot\n";
        file << "=======================\n";
        file << "time: " << CurrentTimestamp() << "\n";
        file << "flow_block: cximage_find_circle_explore.N0\n";
        file << "script_path: " << context.loaded_script_path << "\n";

        if (!context.active_script_case_name.empty())
        {
            file << "active_script_case_name: "
                 << context.active_script_case_name << "\n";

            file << "active_script_case_path: "
                 << context.active_script_case_path << "\n";

            file << "active_script_case_purpose: "
                 << context.active_script_case_purpose << "\n";
        }

        file << "run_state: " << context.run_state << "\n";
        file << "runtime_current_status: " << context.runtime_current_status << "\n";
        file << "debug_status: " << context.debug_status << "\n";
        file << "debug_reason: " << context.debug_reason << "\n";
        file << "cxparser_ext_debug_ok: "
             << (context.cxparser_ext_debug_ok ? "true" : "false") << "\n";
        file << "cxparser_ext_debug_status: "
             << context.cxparser_ext_debug_status << "\n";
        file << "cxparser_ext_debug_reason: "
             << context.cxparser_ext_debug_reason << "\n";
        file << "current_line_index: " << context.current_line << "\n";
        file << "execution_cursor: " << BuildDebugCursorText(context) << "\n";
        file << "last_executed_line_no: " << LastExecutedLineNo(context) << "\n\n";


        file << "CxParserExt Debug Layer\n";
        file << "------------------------\n";
        file << "line_views: " << context.cxparser_ext_line_views.size() << "\n";
        for (const CxScriptLineView& line : context.cxparser_ext_line_views)
        {
            file << "* line " << line.line_no
                 << " type=" << line.statement_type
                 << " status=" << line.status << "\n";
            file << "  source: " << line.source_line << "\n";
            file << "  reason: " << line.reason << "\n";
        }
        file << "statement_views: " << context.cxparser_ext_statement_views.size() << "\n";
        for (const CxScriptStatementView& stmt : context.cxparser_ext_statement_views)
        {
            file << "* #" << stmt.statement_id
                 << " line=" << stmt.line_no
                 << " type=" << stmt.statement_type
                 << " status=" << stmt.status << "\n";
            file << "  lhs: " << stmt.lhs_variable << " : " << stmt.lhs_type << "\n";
            file << "  call: " << stmt.source_object << "." << stmt.method_name << "()\n";
            file << "  return_ref: " << stmt.returned_object_ref << "\n";
            file << "  reason: " << stmt.reason << "\n";
        }
        file << "object_assignments: " << context.cxparser_ext_object_assignments.size() << "\n";
        for (const CxScriptObjectAssignmentView& item : context.cxparser_ext_object_assignments)
        {
            file << "* " << item.lhs_variable << " : " << item.lhs_type << "\n";
            file << "  from: " << item.source_object << "." << item.method_name << "()\n";
            file << "  ref: " << item.returned_object_ref << "\n";
            file << "  line " << item.line_no << ": " << item.source_line << "\n";
            file << "  status: " << item.status << "\n";
            file << "  reason: " << item.reason << "\n";
        }
        file << "\n";
        file << "Current Result Ref\n";
        file << "------------------\n";
        file << "name: " << context.current_result_ref.name << "\n";
        file << "value: " << context.current_result_ref.value << "\n";
        file << "source_object: " << context.current_result_ref.source_object << "\n";
        file << "result_type: " << context.current_result_ref.result_type << "\n";
        file << "status: " << context.current_result_ref.status << "\n";
        file << "reason: " << context.current_result_ref.reason << "\n";
        file << "fit: (" << context.current_result_ref.fit_cx
            << ", " << context.current_result_ref.fit_cy
            << ", r=" << context.current_result_ref.fit_radius << ")\n";
        file << "avgdist: " << context.current_result_ref.avgdist << "\n";
        file << "points_count: " << context.current_result_ref.points_count << "\n";
        file << "valid_points_count: " << context.current_result_ref.valid_points_count << "\n\n";

        file << "Geometry Summary\n";
        file << "----------------\n";
        file << context.geometry_summary << "\n\n";

        file << "Image Overlay Summary\n";
        file << "---------------------\n";
        file << context.image_overlay_summary << "\n\n";

        file << "Runtime Objects\n";
        file << "---------------\n";

        for (const RuntimeObjectView& object : context.runtime_objects)
        {
            file << "* " << object.type << " " << object.name << "\n";
            file << "  declared_line: " << object.declared_line << "\n";
            file << "  exists_in_parser: " << (object.exists_in_parser ? "true" : "false") << "\n";
            file << "  runtime_state: " << object.runtime_state << "\n";
            file << "  last_runtime_status: " << object.last_runtime_status << "\n";
            file << "  last_method: " << object.last_method << "\n";
            file << "  last_update_line: " << object.last_update_line << "\n";
            file << "  display_summary: " << object.display_summary << "\n";
            file << "  visualizable: " << (object.visualizable ? "true" : "false") << "\n";
            file << "  stale: " << (object.stale ? "true" : "false") << "\n";
            file << "  roi_circle: "
                << object.circle_cx << ", "
                << object.circle_cy << ", "
                << object.circle_inner << ", "
                << object.circle_radius << "\n";
            file << "  measure_points_count: " << object.measure_points_count << "\n";
            file << "  valid_points_count: " << object.valid_points_count << "\n";
            file << "  fit_circle: "
                << object.fit_cx << ", "
                << object.fit_cy << ", r="
                << object.fit_radius << "\n";
            file << "  avgdist: " << object.fit_avgdist << "\n";
            file << "  display_version: " << object.display_version << "\n";

            if (object.type == "Findcircle")
            {
                file << "  circle_roi_outer_polyline: "
                     << (object.has_circle_roi_outer_polyline ? "true" : "false") << "\n";
                file << "  circle_roi_inner_polyline: "
                     << (object.has_circle_roi_inner_polyline ? "true" : "false") << "\n";
                file << "  circle_roi_segment_count: "
                     << object.circle_roi_segment_count << "\n";
                file << "  fit_circle_polyline: "
                     << (object.has_fit_circle_polyline ? "true" : "false") << "\n";
                file << "  fit_circle_segment_count: "
                     << object.fit_circle_segment_count << "\n";

                file << "  circle_measure_geometry_request_valid: "
                     << (object.circle_measure_geometry_request_valid ? "true" : "false")
                     << "\n";

                file << "  circle_measure_geometry_dirty: "
                     << (object.circle_measure_geometry_dirty ? "true" : "false")
                     << "\n";

                file << "  circle_measure_geometry_ready: "
                     << (object.circle_measure_geometry_ready ? "true" : "false")
                     << "\n";

                file << "  circle_measure_geometry_version: "
                     << object.circle_measure_geometry_version << "\n";

                file << "  circle_measure_geometry_built_version: "
                     << object.circle_measure_geometry_built_version << "\n";

                file << "  circle_scan_line_count: "
                     << object.circle_scan_line_count << "\n";

                file << "  circle_scan_line_length: "
                     << object.circle_scan_line_length << "\n";

                file << "  circle_process_width: "
                     << object.circle_process_width << "\n";

                file << "  circle_measure_image_ready: "
                     << (object.circle_measure_image_ready ? "true" : "false")
                     << "\n";

                file << "  circle_measure_image_size: "
                     << object.circle_measure_image_width << "x"
                     << object.circle_measure_image_height << "x"
                     << object.circle_measure_image_channels << "\n";

                file << "  circle_measure_backimage_ready: "
                     << (object.circle_measure_backimage_ready ? "true" : "false")
                     << "\n";

                file << "  circle_measure_findobject_ready: "
                     << (object.circle_measure_findobject_ready ? "true" : "false")
                     << "\n";

                file << "  circle_measure_source: "
                     << object.circle_measure_source << "\n";

                file << "  circle_measure_failure_stage: "
                     << object.circle_measure_failure_stage << "\n";

                file << "  circle_measure_detail: "
                     << object.circle_measure_detail << "\n";
            }

            if (object.type == "Findline")
            {
                file << "  line_roi: "
                     << object.line_x0 << ", "
                     << object.line_y0 << " -> "
                     << object.line_x1 << ", "
                     << object.line_y1 << "\n";

                file << "  line_scale: " << object.line_scale << "\n";
                file << "  linegap: " << object.linegap << "\n";
                file << "  line_tool_wgap: " << object.line_tool_wgap << "\n";
                file << "  line_tool_hgap: " << object.line_tool_hgap << "\n";
                file << "  line_display_source: " << object.line_display_source << "\n";
                file << "  has_line_scan_box: "
                     << (object.has_line_scan_box ? "true" : "false") << "\n";
                file << "  line_scan_half_width: "
                     << object.line_scan_half_width << "\n";
                file << "  line_orientation: " << object.line_orientation << "\n";
                file << "  line_dx: " << object.line_dx << "\n";
                file << "  line_dy: " << object.line_dy << "\n";
                file << "  line_length: " << object.line_length << "\n";
                file << "  requested_tool_half_width: "
                     << object.requested_tool_half_width << "\n";
                file << "  effective_tool_half_width: "
                     << object.effective_tool_half_width << "\n";

                file << "  line_pointsw_count: "
                     << object.line_pointsw_count << "\n";
                file << "  line_pointsh_count: "
                     << object.line_pointsh_count << "\n";
                file << "  line_measure_points_count: "
                     << object.line_measure_points_count << "\n";
                file << "  valid_line_points_count: "
                     << object.valid_line_points_count << "\n";

                file << "  has_fit_line: "
                     << (object.has_fit_line ? "true" : "false") << "\n";
                file << "  fit_line: "
                     << object.fit_line_x0 << ", "
                     << object.fit_line_y0 << " -> "
                     << object.fit_line_x1 << ", "
                     << object.fit_line_y1 << "\n";

                file << "  line_scan_box_xy: "
                     << object.line_scan_box_xy[0] << ", "
                     << object.line_scan_box_xy[1] << " | "
                     << object.line_scan_box_xy[2] << ", "
                     << object.line_scan_box_xy[3] << " | "
                     << object.line_scan_box_xy[4] << ", "
                     << object.line_scan_box_xy[5] << " | "
                     << object.line_scan_box_xy[6] << ", "
                     << object.line_scan_box_xy[7] << "\n";

                file << "  line_avgdist: " << object.line_avgdist << "\n";
                file << "  line_fit_mode: " << object.line_fit_mode << "\n";
                file << "  line_fit_status: " << object.line_fit_status << "\n";
                file << "  line_measure_status: " << object.line_measure_status << "\n";
                file << "  line_measure_hint: " << object.line_measure_hint << "\n";
                file << "  line_filter_min_exceeds_component_p90: "
                     << (object.line_filter_min_exceeds_component_p90 ? "true" : "false") << "\n";
                file << "  line_measure_failure_hint: "
                     << object.line_measure_failure_hint << "\n";
                file << "  line_seek_points_count: " << object.line_seek_points_count << "\n";
                file << "  line_edgeband_count: " << object.line_edgeband_count << "\n";
                file << "  line_chain_length: " << object.line_chain_length << "\n";
                file << "  line_profile_point_count: " << object.line_profile_point_count << "\n";
                file << "  line_measure_failure_stage: " << object.line_measure_failure_stage << "\n";

                file << "  line_measure_image_ready: "
                     << (object.line_measure_image_ready ? "true" : "false") << "\n";

                file << "  line_measure_image_size: "
                     << object.line_measure_image_width << "x"
                     << object.line_measure_image_height << "x"
                     << object.line_measure_image_channels << "\n";

                file << "  line_measure_image_type: "
                     << object.line_measure_image_type << "\n";

                file << "  line_measure_image_source: "
                     << object.line_measure_image_source << "\n";

                file << "  line_measure_roi_intersects_image: "
                     << (object.line_measure_roi_intersects_image ? "true" : "false") << "\n";

                file << "  line_measure_roi_fully_inside_image: "
                     << (object.line_measure_roi_fully_inside_image ? "true" : "false") << "\n";

                file << "  line_measure_method: "
                     << object.line_measure_method << "\n";

                file << "  line_measure_threshold: "
                     << object.line_measure_threshold << "\n";

                file << "  line_measure_linegap: "
                     << object.line_measure_linegap << "\n";

                file << "  line_measure_wgap: "
                     << object.line_measure_wgap << "\n";

                file << "  line_measure_hgap: "
                     << object.line_measure_hgap << "\n";

                file << "  line_measure_backimage_ready: "
                     << (object.line_measure_backimage_ready ? "true" : "false") << "\n";

                file << "  line_measure_findobject_ready: "
                     << (object.line_measure_findobject_ready ? "true" : "false") << "\n";

                file << "  line_measure_objfilterset: "
                     << object.line_measure_objfilterset << "\n";

                file << "  line_measure_filter_borw: "
                     << object.line_measure_filter_borw << "\n";

                file << "  line_measure_filter_min: "
                     << object.line_measure_filter_min << "\n";

                file << "  line_measure_filter_max: "
                     << object.line_measure_filter_max << "\n";

                file << "  line_measure_filter_profile: "
                     << object.line_measure_filter_profile << "\n";

                file << "  line_measure_filter_explicit: "
                     << (object.line_measure_filter_explicit ? "true" : "false") << "\n";

                file << "  line_measure_effective_filter_borw: "
                     << object.line_measure_effective_filter_borw << "\n";

                file << "  line_measure_effective_filter_min: "
                     << object.line_measure_effective_filter_min << "\n";

                file << "  line_measure_effective_filter_max: "
                     << object.line_measure_effective_filter_max << "\n";

                file << "  line_measure_findobject_called: "
                     << (object.line_measure_findobject_called ? "true" : "false") << "\n";

                file << "  line_measure_findobject_skipped: "
                     << (object.line_measure_findobject_skipped ? "true" : "false") << "\n";

                file << "  line_measure_binary_foreground_pixels: "
                     << object.line_measure_binary_foreground_pixels << "\n";

                file << "  line_measure_binary_roi: "
                     << object.line_measure_binary_roi_width
                     << "x"
                     << object.line_measure_binary_roi_height
                     << "\n";

                file << "  line_measure_result_empty_reason: "
                     << object.line_measure_result_empty_reason << "\n";

                file << "  line_findobject_component_total: "
                     << object.line_findobject_component_total << "\n";

                file << "  line_findobject_component_accepted: "
                     << object.line_findobject_component_accepted << "\n";

                file << "  line_findobject_component_rejected_by_min: "
                     << object.line_findobject_component_rejected_by_min << "\n";

                file << "  line_findobject_component_rejected_by_max: "
                     << object.line_findobject_component_rejected_by_max << "\n";

                file << "  line_findobject_component_rejected_by_borw: "
                     << object.line_findobject_component_rejected_by_borw << "\n";

                file << "  line_findobject_area_min_observed: "
                     << object.line_findobject_area_min_observed << "\n";

                file << "  line_findobject_area_max_observed: "
                     << object.line_findobject_area_max_observed << "\n";

                file << "  line_findobject_area_mean_observed: "
                     << object.line_findobject_area_mean_observed << "\n";

                file << "  line_findobject_area_median: "
                     << object.line_findobject_area_median << "\n";

                file << "  line_findobject_area_p90: "
                     << object.line_findobject_area_p90 << "\n";

                file << "  line_measure_cc_selected_foreground: "
                     << object.line_measure_cc_selected_foreground << "\n";

                file << "  line_measure_cc_white_total: "
                     << object.line_measure_cc_white_total << "\n";
                file << "  line_measure_cc_white_accepted: "
                     << object.line_measure_cc_white_accepted << "\n";
                file << "  line_measure_cc_white_rejected_min: "
                     << object.line_measure_cc_white_rejected_min << "\n";
                file << "  line_measure_cc_white_area_median: "
                     << object.line_measure_cc_white_area_median << "\n";
                file << "  line_measure_cc_white_area_p90: "
                     << object.line_measure_cc_white_area_p90 << "\n";

                file << "  line_measure_cc_black_total: "
                     << object.line_measure_cc_black_total << "\n";
                file << "  line_measure_cc_black_accepted: "
                     << object.line_measure_cc_black_accepted << "\n";
                file << "  line_measure_cc_black_rejected_min: "
                     << object.line_measure_cc_black_rejected_min << "\n";
                file << "  line_measure_cc_black_area_median: "
                     << object.line_measure_cc_black_area_median << "\n";
                file << "  line_measure_cc_black_area_p90: "
                     << object.line_measure_cc_black_area_p90 << "\n";

                file << "  line_measure_cc_selected_total: "
                     << object.line_measure_cc_selected_total << "\n";
                file << "  line_measure_cc_selected_accepted: "
                     << object.line_measure_cc_selected_accepted << "\n";
                file << "  line_measure_cc_selected_rejected_min: "
                     << object.line_measure_cc_selected_rejected_min << "\n";
                file << "  line_measure_cc_selected_area_median: "
                     << object.line_measure_cc_selected_area_median << "\n";
                file << "  line_measure_cc_selected_area_p90: "
                     << object.line_measure_cc_selected_area_p90 << "\n";

                file << "  line_measure_profile_count: "
                     << object.line_measure_profile_count << "\n";

                file << "  line_measure_sampled_pixel_count: "
                     << object.line_measure_sampled_pixel_count << "\n";

                file << "  line_measure_gray_min: "
                     << object.line_measure_gray_min << "\n";

                file << "  line_measure_gray_max: "
                     << object.line_measure_gray_max << "\n";

                file << "  line_measure_gray_mean: "
                     << object.line_measure_gray_mean << "\n";

                file << "  line_measure_max_gradient: "
                     << object.line_measure_max_gradient << "\n";

                file << "  line_measure_input_failure_stage: "
                     << object.line_measure_input_failure_stage << "\n";

                file << "  line_measure_input_detail: "
                     << object.line_measure_input_detail << "\n";

                file << "  line_measure_source: "
                     << object.line_measure_source << "\n";

                file << "  line_measure_fallback_allowed: "
                     << (object.line_measure_fallback_allowed ? "true" : "false") << "\n";

                file << "  line_measure_fallback_used: "
                     << (object.line_measure_fallback_used ? "true" : "false") << "\n";

                file << "  line_measure_original_point_count: "
                     << object.line_measure_original_point_count << "\n";

                file << "  line_measure_original_edgeband_count: "
                     << object.line_measure_original_edgeband_count << "\n";

                file << "  line_measure_original_chain_length: "
                     << object.line_measure_original_chain_length << "\n";

                file << "  line_measure_original_failure_stage: "
                     << object.line_measure_original_failure_stage << "\n";

                file << "  line_measure_original_detail: "
                     << object.line_measure_original_detail << "\n";

                file << "  line_measure_geometry_request_valid: "
                     << (object.line_measure_geometry_request_valid ? "true" : "false")
                     << "\n";

                file << "  line_measure_geometry_dirty: "
                     << (object.line_measure_geometry_dirty ? "true" : "false")
                     << "\n";

                file << "  line_measure_geometry_ready: "
                     << (object.line_measure_geometry_ready ? "true" : "false")
                     << "\n";

                file << "  line_measure_geometry_version: "
                     << object.line_measure_geometry_version << "\n";

                file << "  line_measure_geometry_built_version: "
                     << object.line_measure_geometry_built_version << "\n";

                file << "  line_measure_geometry_half_width: "
                     << object.line_measure_geometry_half_width << "\n";

                file << "  line_original_scan_w_count: "
                     << object.line_original_scan_w_count << "\n";

                file << "  line_original_scan_h_count: "
                     << object.line_original_scan_h_count << "\n";

                file << "  line_original_scan_w_length: "
                     << object.line_original_scan_w_length << "\n";

                file << "  line_original_scan_h_length: "
                     << object.line_original_scan_h_length << "\n";

                file << "  line_original_process_width: "
                     << object.line_original_process_width << "\n";

                file << "  display_version: " << object.display_version << "\n";
            }

            if (object.type == "CircleRingGauge")
            {
                file << "  ring_outer_radius: " << object.ring_outer_radius << "\n";
                file << "  ring_inner_radius: " << object.ring_inner_radius << "\n";
                file << "  ring_thickness: " << object.ring_thickness << "\n";
                file << "  ring_center_distance: " << object.ring_center_distance << "\n";
                file << "  ring_concentric_ok: " << (object.ring_concentric_ok ? "true" : "false") << "\n";
                file << "  ring_inside_ok: " << (object.ring_inside_ok ? "true" : "false") << "\n";
                file << "  ring_thickness_ok: " << (object.ring_thickness_ok ? "true" : "false") << "\n";
                file << "  ring_score: " << object.ring_score << "\n";
                file << "  ring_status: " << object.ring_status << "\n";
                file << "  ring_reason: " << object.ring_reason << "\n";
                file << "  ring_result_ref: " << object.ring_result_ref << "\n";
            }

            file << "\n";
        }

        file << "Line Trace\n";
        file << "----------\n";

        for (const ScriptLineView& line : context.line_views)
        {
            file << line.line_no
                << " [" << line.status << "] "
                << line.statement
                << " | object=" << line.object
                << " | method=" << line.method
                << " | params=" << line.params
                << " | reason=" << line.reason
                << "\n";
        }

        outReason = "debug snapshot saved";
        return true;
    }
    catch (const std::exception& e)
    {
        outReason = std::string("debug snapshot exception: ") + e.what();
        return false;
    }
    catch (...)
    {
        outReason = "debug snapshot unknown exception";
        return false;
    }
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
    std::unordered_map<std::string, std::unique_ptr<Findline>> lines;
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

static bool RuntimeObjectIsType(ManualTestContext& context,
    const std::string& objectName,
    const std::string& expectedType)
{
    RuntimeObjectView* object = FindRuntimeObjectByName(context, objectName);

    if (object == nullptr)
        return false;

    return object->type == expectedType;
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

// æ™®é€š global å˜é‡ï¼šglobal.current_status / global.circle_ref ç­‰ã€‚
// æ³¨æ„ï¼šè¿™ä¸ªå‡½æ•°åªæœ‰ 6 ä¸ªå‚æ•°ï¼Œä¸è¦å†ç»™å®ƒåŠ é»˜è®¤å‚æ•°ã€‚
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

// å›¾åƒ global å˜é‡ï¼šglobal.matInputã€‚
// æ³¨æ„ï¼šç”¨ä¸åŒå‡½æ•°åï¼Œé¿å…å’Œæ™®é€šå˜é‡å‡½æ•°é‡è½½å†²çªã€‚

static void InjectManualGaugeInt(
    ManualTestContext& context,
    const std::string& name,
    int value)
{
    context.runtime_int_vars[name] = value;
    UpsertGlobalVariableView(
        context,
        "int",
        name,
        std::to_string(value),
        0,
        "manual_gauge_applied");
}

static void DrawGaugeHandle(
    ImDrawList* draw_list,
    float x, float y,
    float radius,
    bool active,
    bool hovered,
    uint32_t color = 0xFF0000FF)
{
    const float r = active ? radius * 1.3f : hovered ? radius * 1.15f : radius;
    draw_list->AddCircleFilled(ImVec2(x, y), r, color);
    draw_list->AddCircle(ImVec2(x, y), r + 2.0f, 0xFFFFFFFF, 0, 4.0f);
}

static void DrawGaugeHandlesLine(
    ImDrawList* draw_list,
    const ManualGaugeState& gauge)
{
    if (!gauge.has_line_gauge)
        return;

    LineGaugeGeometry geom = BuildLineGaugeGeometry(gauge);
    if (!geom.valid)
        return;

    DrawGaugeHandle(draw_list, geom.p0.x, geom.p0.y,
        6.0f, false, false, 0xFF0000FF);
    DrawGaugeHandle(draw_list, geom.p1.x, geom.p1.y,
        6.0f, false, false, 0xFF0000FF);
    DrawGaugeHandle(draw_list, geom.center.x, geom.center.y,
        5.0f, false, false, 0xFFFF0000);
    DrawGaugeHandle(draw_list, geom.w_plus.x, geom.w_plus.y,
        5.0f, false, false, 0xFF00FFFF);
    DrawGaugeHandle(draw_list, geom.w_minus.x, geom.w_minus.y,
        5.0f, false, false, 0xFF00FFFF);
}

static void DrawGaugeHandlesCircle(
    ImDrawList* draw_list,
    const ManualGaugeState& gauge)
{
    if (!gauge.has_circle_gauge)
        return;

    DrawGaugeHandle(draw_list, (float)gauge.circle_cx, (float)gauge.circle_cy,
        6.0f, false, false, 0xFF0000FF);

    int effective_radius = gauge.radius;
    if (effective_radius <= 0)
        effective_radius = gauge.gap > 0 ? gauge.gap : 50;

    const float px = (float)gauge.circle_cx + (float)effective_radius;
    const float py = (float)gauge.circle_cy;
    DrawGaugeHandle(draw_list, px, py,
        6.0f, false, false, 0xFF0000FF);

    if (gauge.inner_radius > 0)
    {
        const float ix = (float)gauge.circle_cx + (float)gauge.inner_radius;
        const float iy = (float)gauge.circle_cy;
        DrawGaugeHandle(draw_list, ix, iy,
            5.0f, false, false, 0xFF00FFFF);
    }

    if (gauge.outer_radius > 0)
    {
        const float ox = (float)gauge.circle_cx + (float)gauge.outer_radius;
        const float oy = (float)gauge.circle_cy;
        DrawGaugeHandle(draw_list, ox, oy,
            5.0f, false, false, 0xFF00FFFF);
    }
}

static void DrawGaugeHandles(
    ImDrawList* draw_list,
    const ManualGaugeState& gauge)
{
    if (gauge.tool == "Findcircle" || gauge.has_circle_gauge)
        DrawGaugeHandlesCircle(draw_list, gauge);
    else
        DrawGaugeHandlesLine(draw_list, gauge);
}

static bool ApplyManualGaugeToGlobals(ManualTestContext& context)
{
    ManualGaugeState& gauge = context.current_gauge;
    if (gauge.tool == "Findcircle" || gauge.has_circle_gauge)
    {
        if (!gauge.has_circle_gauge)
            return false;

        InjectManualGaugeInt(context, "global.circle_cx", gauge.circle_cx);
        InjectManualGaugeInt(context, "global.circle_cy", gauge.circle_cy);
        InjectManualGaugeInt(context, "global.circle_px", gauge.circle_px);
        InjectManualGaugeInt(context, "global.circle_py", gauge.circle_py);
        InjectManualGaugeInt(context, "global.gap", gauge.gap);
        InjectManualGaugeInt(context, "global.linegap", gauge.linegap);
        InjectManualGaugeInt(context, "global.threshold", gauge.threshold);
        InjectManualGaugeInt(context, "global.method", gauge.method);
    }
    else
    {
        if (!gauge.has_line_gauge)
            return false;

        InjectManualGaugeInt(context, "global.roi_x0", gauge.line_x0);
        InjectManualGaugeInt(context, "global.roi_y0", gauge.line_y0);
        InjectManualGaugeInt(context, "global.roi_x1", gauge.line_x1);
        InjectManualGaugeInt(context, "global.roi_y1", gauge.line_y1);
        InjectManualGaugeInt(context, "global.tool_half_width", gauge.tool_half_width);
        InjectManualGaugeInt(context, "global.wgap", gauge.wgap);
        InjectManualGaugeInt(context, "global.hgap", gauge.hgap);
        InjectManualGaugeInt(context, "global.linegap", gauge.linegap);
        InjectManualGaugeInt(context, "global.threshold", gauge.threshold);
        InjectManualGaugeInt(context, "global.filterprofile", gauge.filterprofile);
        InjectManualGaugeInt(context, "global.method", gauge.method);
    }

    UpsertGlobalVariableView(context, "string", "global.gauge_source",
                             gauge.source, 0, "manual_gauge_applied");
    UpsertGlobalVariableView(context, "string", "global.gauge_review_status",
                             gauge.review_status, 0, "manual_gauge_applied");

    gauge.dirty = false;
    context.debug_action = "Apply Gauge To Globals";
    context.debug_status = "PENDING";
    context.debug_reason = "ManualGaugeState injected into runtime globals";
    return true;
}

static fs::path ManualGaugeCaseDir(const ManualTestContext& context)
{
    fs::path dir = context.case_directory.empty()
        ? fs::path("docs/notes/cxscript_case")
        : fs::path(context.case_directory);
    if (!context.current_gauge.case_id.empty())
        dir /= context.current_gauge.case_id;
    return dir;
}

static bool SaveManualGaugeAnnotation(
    const ManualTestContext& context,
    const fs::path& path,
    std::string& reason)
{
    const ManualGaugeState& g = context.current_gauge;
    fs::create_directories(path.parent_path());
    std::ofstream file(path);
    if (!file.is_open())
    {
        reason = "failed to open gauge_annotation.json";
        return false;
    }

    file << "{\n";
    file << "  \"case_id\": \"" << CxDebugJsonEscape(g.case_id) << "\",\n";
    file << "  \"image_id\": \"" << CxDebugJsonEscape(g.image_id) << "\",\n";
    file << "  \"target_id\": \"" << CxDebugJsonEscape(g.target_id) << "\",\n";
    file << "  \"tool\": \"" << CxDebugJsonEscape(g.tool) << "\",\n";
    file << "  \"source\": \"" << CxDebugJsonEscape(g.source) << "\",\n";
    file << "  \"review_status\": \"" << CxDebugJsonEscape(g.review_status) << "\",\n";
    if (g.has_circle_gauge)
    {
        file << "  \"circle_gauge\": {\n";
        file << "    \"cx\": " << g.circle_cx << ",\n";
        file << "    \"cy\": " << g.circle_cy << ",\n";
        file << "    \"px\": " << g.circle_px << ",\n";
        file << "    \"py\": " << g.circle_py << ",\n";
        file << "    \"gap\": " << g.gap << ",\n";
        file << "    \"linegap\": " << g.linegap << ",\n";
        file << "    \"threshold\": " << g.threshold << ",\n";
        file << "    \"method\": " << g.method << "\n";
        file << "  },\n";
    }
    else
    {
        file << "  \"line_gauge\": {\n";
        file << "    \"x0\": " << g.line_x0 << ",\n";
        file << "    \"y0\": " << g.line_y0 << ",\n";
        file << "    \"x1\": " << g.line_x1 << ",\n";
        file << "    \"y1\": " << g.line_y1 << ",\n";
        file << "    \"tool_half_width\": " << g.tool_half_width << ",\n";
        file << "    \"wgap\": " << g.wgap << ",\n";
        file << "    \"hgap\": " << g.hgap << ",\n";
        file << "    \"linegap\": " << g.linegap << ",\n";
        file << "    \"threshold\": " << g.threshold << ",\n";
        file << "    \"filterprofile\": " << g.filterprofile << ",\n";
        file << "    \"method\": " << g.method << "\n";
        file << "  },\n";
    }
    file << "  \"review\": {\n";
    file << "    \"stage\": \"gauge\",\n";
    file << "    \"decision\": \"" << (g.accepted ? "accept_gauge" : "editing") << "\",\n";
    file << "    \"note\": \"ManualGaugeState saved from ManualStateTestConsole.\"\n";
    file << "  }\n";
    file << "}\n";
    reason.clear();
    return true;
}

static bool ExtractManualGaugeJsonInt(
    const std::string& text,
    const std::string& key,
    int& value)
{
    const std::string pattern = "\"" + key + "\"";
    size_t pos = text.find(pattern);
    if (pos == std::string::npos)
        return false;
    pos = text.find(':', pos);
    if (pos == std::string::npos)
        return false;
    ++pos;
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos])))
        ++pos;
    size_t end = pos;
    if (end < text.size() && (text[end] == '-' || text[end] == '+'))
        ++end;
    while (end < text.size() && std::isdigit(static_cast<unsigned char>(text[end])))
        ++end;
    if (end == pos)
        return false;
    try
    {
        value = std::stoi(text.substr(pos, end - pos));
        return true;
    }
    catch (...)
    {
        return false;
    }
}

static bool ExtractManualGaugeJsonString(
    const std::string& text,
    const std::string& key,
    std::string& value)
{
    const std::string pattern = "\"" + key + "\"";
    size_t pos = text.find(pattern);
    if (pos == std::string::npos)
        return false;
    pos = text.find(':', pos);
    if (pos == std::string::npos)
        return false;
    pos = text.find('"', pos + 1);
    if (pos == std::string::npos)
        return false;
    const size_t end = text.find('"', pos + 1);
    if (end == std::string::npos)
        return false;
    value = text.substr(pos + 1, end - pos - 1);
    return true;
}

static bool LoadManualGaugeAnnotation(
    ManualTestContext& context,
    const fs::path& path,
    std::string& reason)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        reason = "failed to open gauge_annotation.json";
        return false;
    }
    const std::string text(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());

    ManualGaugeState gauge;
    ExtractManualGaugeJsonString(text, "case_id", gauge.case_id);
    ExtractManualGaugeJsonString(text, "image_id", gauge.image_id);
    ExtractManualGaugeJsonString(text, "target_id", gauge.target_id);
    ExtractManualGaugeJsonString(text, "tool", gauge.tool);
    ExtractManualGaugeJsonString(text, "source", gauge.source);
    ExtractManualGaugeJsonString(text, "review_status", gauge.review_status);
    gauge.accepted = gauge.review_status == "manual_accepted" ||
                     gauge.review_status == "accepted" ||
                     gauge.review_status == "promoted";

    bool hasLine = false;
    hasLine |= ExtractManualGaugeJsonInt(text, "x0", gauge.line_x0);
    hasLine |= ExtractManualGaugeJsonInt(text, "y0", gauge.line_y0);
    hasLine |= ExtractManualGaugeJsonInt(text, "x1", gauge.line_x1);
    hasLine |= ExtractManualGaugeJsonInt(text, "y1", gauge.line_y1);
    ExtractManualGaugeJsonInt(text, "tool_half_width", gauge.tool_half_width);
    ExtractManualGaugeJsonInt(text, "wgap", gauge.wgap);
    ExtractManualGaugeJsonInt(text, "hgap", gauge.hgap);
    ExtractManualGaugeJsonInt(text, "linegap", gauge.linegap);
    ExtractManualGaugeJsonInt(text, "threshold", gauge.threshold);
    ExtractManualGaugeJsonInt(text, "filterprofile", gauge.filterprofile);
    ExtractManualGaugeJsonInt(text, "method", gauge.method);
    gauge.has_line_gauge = hasLine;

    bool hasCircle = false;
    hasCircle |= ExtractManualGaugeJsonInt(text, "cx", gauge.circle_cx);
    hasCircle |= ExtractManualGaugeJsonInt(text, "cy", gauge.circle_cy);
    hasCircle |= ExtractManualGaugeJsonInt(text, "px", gauge.circle_px);
    hasCircle |= ExtractManualGaugeJsonInt(text, "py", gauge.circle_py);
    ExtractManualGaugeJsonInt(text, "gap", gauge.gap);
    gauge.has_circle_gauge = hasCircle;

    if (!gauge.has_line_gauge && !gauge.has_circle_gauge)
    {
        reason = "gauge_annotation.json has no supported line/circle gauge fields";
        return false;
    }

    gauge.dirty = false;
    context.current_gauge = gauge;
    reason.clear();
    return true;
}

static bool ExportManualGaugeManifestCandidate(
    const ManualTestContext& context,
    const fs::path& path,
    std::string& reason)
{
    const ManualGaugeState& g = context.current_gauge;
    fs::create_directories(path.parent_path());
    std::ofstream file(path);
    if (!file.is_open())
    {
        reason = "failed to open manifest_candidate.json";
        return false;
    }

    file << "{\n";
    file << "  \"schema_version\": \"manual_gauge_manifest_candidate_v1\",\n";
    file << "  \"warning\": \"Do not overwrite stage25_image_manifest.json automatically. Human Promote is required.\",\n";
    file << "  \"case_id\": \"" << CxDebugJsonEscape(g.case_id) << "\",\n";
    file << "  \"image_id\": \"" << CxDebugJsonEscape(g.image_id) << "\",\n";
    file << "  \"target_id\": \"" << CxDebugJsonEscape(g.target_id) << "\",\n";
    file << "  \"tool\": \"" << CxDebugJsonEscape(g.tool) << "\",\n";
    file << "  \"review_status\": \"" << CxDebugJsonEscape(g.review_status) << "\",\n";
    file << "  \"promote_allowed\": " << (g.accepted ? "true" : "false") << ",\n";
    file << "  \"target_patch\": {\n";
    if (g.has_circle_gauge)
    {
        file << "    \"circle_cx\": " << g.circle_cx << ",\n";
        file << "    \"circle_cy\": " << g.circle_cy << ",\n";
        file << "    \"circle_px\": " << g.circle_px << ",\n";
        file << "    \"circle_py\": " << g.circle_py << ",\n";
        file << "    \"gap\": " << g.gap << ",\n";
    }
    else
    {
        file << "    \"roi_x0\": " << g.line_x0 << ",\n";
        file << "    \"roi_y0\": " << g.line_y0 << ",\n";
        file << "    \"roi_x1\": " << g.line_x1 << ",\n";
        file << "    \"roi_y1\": " << g.line_y1 << ",\n";
        file << "    \"tool_half_width\": " << g.tool_half_width << ",\n";
        file << "    \"wgap\": " << g.wgap << ",\n";
        file << "    \"hgap\": " << g.hgap << ",\n";
    }
    file << "    \"linegap\": " << g.linegap << ",\n";
    file << "    \"threshold\": " << g.threshold << ",\n";
    file << "    \"method\": " << g.method << "\n";
    file << "  }\n";
    file << "}\n";
    reason.clear();
    return true;
}

static bool ManualGaugeAcceptedForParamRegression(const ManualGaugeState& gauge)
{
    return gauge.accepted ||
           gauge.review_status == "manual_accepted" ||
           gauge.review_status == "accepted" ||
           gauge.review_status == "promoted";
}

static CxParamRegressionTask BuildParamRegressionTaskFromManualGauge(
    const ManualTestContext& context)
{
    const ManualGaugeState& gauge = context.current_gauge;
    CxParamRegressionTask task;
    task.case_id = gauge.case_id.empty() ? "manual_case" : gauge.case_id;
    task.image_id = gauge.image_id;
    task.target_id = gauge.target_id;
    task.tool = gauge.tool.empty() ? "Findline" : gauge.tool;
    task.task_id = "param_regression_" + task.case_id;
    task.gauge_annotation_path =
        (ManualGaugeCaseDir(context) / "gauge_annotation.json").string();
    task.base_script_id = task.tool == "Findcircle"
        ? "findcircle_stage25_direct_ok"
        : "findline_stage25_filter20_ok";
    task.base_parameter_profile_id = "manual_gauge_seed";
    task.max_candidates = context.param_regression.max_candidates;
    task.max_case_seconds = context.param_regression.max_case_seconds;
    task.max_total_seconds = context.param_regression.max_total_seconds;
    task.require_manual_gauge = true;
    task.allow_mlpack_rank = true;
    task.allow_ensmallen_opt = true;
    task.allow_promote = false;
    return task;
}

static CxParamEvalRecord BuildManualSeedEvalRecord(
    const ManualTestContext& context,
    const CxParamRegressionTask& task)
{
    const ResultRefView& result = context.current_result_ref;
    CxParamEvalRecord record;
    record.candidate_id = "manual_seed";
    record.case_id = task.case_id;
    record.tool = task.tool;
    record.executed = true;
    if (task.tool == "Findcircle")
    {
        record.points = result.valid_points_count > 0
            ? result.valid_points_count
            : result.points_count;
        record.fit_available = result.fit_radius > 0.0f ||
            result.status == "geometry_result_available";
        record.mean_distance = result.avgdist;
    }
    else
    {
        record.points = result.valid_line_points_count > 0
            ? result.valid_line_points_count
            : (result.line_points_count > 0 ? result.line_points_count : result.valid_points_count);
        record.fit_available = result.line_result_status == "geometry_result_available" ||
            result.status == "geometry_result_available" ||
            (result.line_x0 != result.line_x1 || result.line_y0 != result.line_y1);
        record.mean_distance = result.line_avgdist;
    }
    record.support_score = record.points > 0 ? 1.0 : 0.0;
    record.failure_stage = record.fit_available ? "" : "pending_probe_or_no_fit";
    record.classification = record.fit_available ? "manual_seed_geometry_available" : "manual_seed_needs_probe";
    const fs::path case_dir = ManualGaugeCaseDir(context);
    record.result_summary_path = (case_dir / "result_summary.json").string();
    record.tool_display_path = (case_dir / "tool_display.png").string();
    record.replay_package_path = (case_dir / "replay_package.json").string();
    return record;
}

static CxParamAccuracyStats BuildManualSeedAccuracyStats(
    const CxParamRegressionTask& task,
    const CxParamEvalRecord& record,
    const ManualGaugeState& gauge)
{
    CxParamAccuracyStats stats;
    stats.candidate_id = "manual_seed";
    stats.tool = task.tool;
    stats.total_cases = 1;
    stats.executed_cases = record.executed ? 1 : 0;
    stats.timeout_cases = record.timeout ? 1 : 0;
    stats.geometry_pass = record.fit_available ? 1 : 0;
    stats.evidence_pass = record.points > 0 ? 1 : 0;
    stats.human_accept = ManualGaugeAcceptedForParamRegression(gauge) ? 1 : 0;
    stats.geometry_pass_rate = static_cast<double>(stats.geometry_pass);
    stats.evidence_pass_rate = static_cast<double>(stats.evidence_pass);
    stats.human_accept_rate = static_cast<double>(stats.human_accept);
    stats.avg_support_score = record.support_score;
    stats.avg_mean_distance = record.mean_distance;
    stats.avg_fit_offset = record.fit_offset;
    stats.stability_score = record.fit_available ? 0.5 : 0.0;
    stats.risk_score = record.fit_available ? 0.4 : 0.8;
    return stats;
}

static bool InitializeParamRegressionFromGauge(
    ManualTestContext& context,
    std::string& reason)
{
    ManualParamRegressionState& state = context.param_regression;
    if (!ManualGaugeAcceptedForParamRegression(context.current_gauge))
    {
        state.initialized = false;
        state.status = "blocked";
        state.reason = "Manual gauge must be accepted before parameter regression.";
        reason = state.reason;
        return false;
    }

    state.task = BuildParamRegressionTaskFromManualGauge(context);
    state.range_set = MakeConservativeRangeSet(state.task.tool);
    state.range_set.max_candidates = state.max_candidates;
    state.range_set.max_case_seconds = state.max_case_seconds;
    state.range_set.max_total_seconds = state.max_total_seconds;
    state.candidates = GenerateBasicParamCandidates(state.range_set, state.max_candidates);
    state.records.clear();
    CxParamEvalRecord manual_seed = BuildManualSeedEvalRecord(context, state.task);
    state.records.push_back(manual_seed);
    state.accuracy_stats.clear();
    state.accuracy_stats.push_back(
        BuildManualSeedAccuracyStats(state.task, manual_seed, context.current_gauge));
    state.output_dir = (ManualGaugeCaseDir(context) / "param_regression").string();
    state.initialized = true;
    state.status = "ready";
    state.reason =
        "Manual gauge accepted. Phase 1 can export parameter range, candidates, and evidence reports.";
    reason.clear();
    return true;
}

static CxParamCandidate CandidateFromManualGauge(
    const ManualGaugeState& gauge,
    const std::string& id,
    const std::string& source)
{
    CxParamCandidate c;
    c.candidate_id = id;
    c.source = source;
    c.method = gauge.method;
    c.threshold = gauge.threshold;
    c.gap = gauge.gap;
    c.linegap = gauge.linegap;
    c.wgap = gauge.wgap;
    c.hgap = gauge.hgap;
    c.filterprofile = gauge.filterprofile;
    c.samplerate = 1;
    c.predicted_quality = source == "manual_seed" ? 0.65 : 0.55;
    c.predicted_risk = source == "manual_seed" ? 0.30 : 0.45;
    c.predicted_failure_class = "pending_probe";
    c.selected_for_probe = true;
    return c;
}

static void AddMlpackRankPlaceholderCandidates(ManualParamRegressionState& state)
{
    (void)state;
}

static void AddEnsmallenOptPlaceholderCandidates(ManualParamRegressionState& state)
{
    (void)state;
}

static void RefreshParamRegressionExportedFiles(ManualParamRegressionState& state)
{
    state.exported_files.clear();
    const fs::path root(state.output_dir);
    const char* names[] = {
        "param_regression_task.json",
        "param_range_report.json",
        "param_range_report.csv",
        "param_range_report.md",
        "param_candidates.json",
        "param_candidates.csv",
        "param_eval_records.jsonl",
        "hit_distribution.json",
        "hit_distribution.csv",
        "param_hit_distribution_report.md",
        "param_accuracy_matrix.json",
        "param_accuracy_matrix.csv",
        "param_accuracy_matrix.md",
        "param_candidate_distribution.md",
        "param_optimization_trace.json",
        "param_stability_report.md",
        "param_recommendation_report.md",
        "param_profile_promotion_gate.md",
        "param_profile_candidate.cxsc",
        "manual_acceptance_checklist.md"
    };
    for (const char* name : names)
    {
        const fs::path path = root / name;
        if (fs::exists(path))
            state.exported_files.push_back(path.string());
    }
}

static bool ExportParamRegressionManualAcceptanceChecklist(
    const ManualTestContext& context,
    const fs::path& path,
    std::string& reason)
{
    const ManualParamRegressionState& reg = context.param_regression;
    const ManualGaugeState& gauge = context.current_gauge;
    fs::create_directories(path.parent_path());
    std::ofstream file(path);
    if (!file.is_open())
    {
        reason = "failed to write manual acceptance checklist";
        return false;
    }

    file << "# Parameter Regression Manual Acceptance Checklist\n\n";
    file << "## Current Context\n\n";
    file << "- Case: `" << gauge.case_id << "`\n";
    file << "- Image: `" << gauge.image_id << "`\n";
    file << "- Target: `" << gauge.target_id << "`\n";
    file << "- Tool: `" << gauge.tool << "`\n";
    file << "- Gauge Review Status: `" << gauge.review_status << "`\n";
    file << "- Gauge Accepted: `" << (ManualGaugeAcceptedForParamRegression(gauge) ? "yes" : "no") << "`\n";
    file << "- Output Dir: `" << reg.output_dir << "`\n\n";

    file << "## Checklist\n\n";
    file << "- [ ] Manual gauge position/direction/width or circle ring is correct.\n";
    file << "- [ ] `Apply Gauge To Globals` has been used before probe/replay.\n";
    file << "- [ ] Candidate table contains manual seed and selected probe candidates.\n";
    file << "- [ ] Candidate parameter values are visible and editable in UI.\n";
    file << "- [ ] `param_candidates.json/csv` matches UI candidate table.\n";
    file << "- [ ] `param_eval_records.jsonl` contains manual seed evidence or selected probe result after probe.\n";
    file << "- [ ] `param_hit_distribution_report.md` is present for evidence review.\n";
    file << "- [ ] `param_accuracy_matrix.md/json/csv` is present for stability review.\n";
    file << "- [ ] `param_profile_promotion_gate.md` says promotion is disabled unless mini-regression passes.\n";
    file << "- [ ] `param_profile_candidate.cxsc` is diagnostic-only, not baseline.\n\n";

    file << "## Candidate Snapshot\n\n";
    file << "| Index | Candidate | Source | Selected | Method | Threshold | Gap | LineGap | WGap | HGap | Filter | Risk |\n";
    file << "|---:|---|---|---|---:|---:|---:|---:|---:|---:|---:|---:|\n";
    for (std::size_t i = 0; i < reg.candidates.size(); ++i)
    {
        const auto& c = reg.candidates[i];
        file << "| " << i << " | " << c.candidate_id << " | " << c.source << " | "
             << (c.selected_for_probe ? "yes" : "no") << " | " << c.method << " | "
             << c.threshold << " | " << c.gap << " | " << c.linegap << " | "
             << c.wgap << " | " << c.hgap << " | " << c.filterprofile << " | "
             << c.predicted_risk << " |\n";
    }
    reason.clear();
    return true;
}

static void ApplyCxParserExtDebugResultToManualConsole(
    ManualTestContext& context,
    const CxScriptSemanticBridgeResult& result)
{
    context.cxparser_ext_debug_ok = result.ok;
    context.cxparser_ext_debug_status = result.status;
    context.cxparser_ext_debug_reason = result.reason;
    context.debug_parser_output = result.raw_log;
    context.cxparser_ext_line_views = result.line_views;
    context.cxparser_ext_statement_views = result.statement_views;
    context.cxparser_ext_object_assignments = result.object_assignments;

    if (!result.ok)
    {
        context.debug_status = "CXPARSER_EXT_DEBUG_FAILED";
        context.debug_reason = result.reason;
        return;
    }

    context.debug_status = "CXPARSER_EXT_DEBUG_OK";
    context.debug_reason = result.status;

    for (const CxScriptObjectAssignmentView& assignment :
         result.object_assignments)
    {
        UpsertGlobalVariableView(
            context,
            assignment.lhs_type,
            assignment.lhs_variable,
            assignment.returned_object_ref,
            assignment.line_no,
            "cxparser_ext_debug_object_assignment");

        RuntimeObjectView& object = EnsureRuntimeObject(
            context,
            assignment.lhs_variable,
            assignment.lhs_type,
            assignment.line_no);

        object.exists_in_parser = true;
        object.type = assignment.lhs_type;
        object.runtime_state = "cxparser_ext_debug_assigned";
        object.last_runtime_status = assignment.status;
        object.last_method = assignment.method_name;
        object.last_update_line = assignment.line_no;
        object.display_summary =
            assignment.lhs_variable +
            " <= " +
            assignment.source_object +
            "." +
            assignment.method_name +
            "()";
        object.visualizable = false;
        object.visual_source = "cxparser_ext_debug";
        object.stale = false;
    }
}
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

    // å½“å‰ find_circle_direct_test.cxsc ä¸­ m_isetcircle å‚ä¸Ž if åˆ¤æ–­ã€‚
    // replay è°ƒè¯•æ¨¡å¼ä¸‹å¿…é¡»é»˜è®¤ä»Ž 0 å¼€å§‹ï¼Œå¦åˆ™ç¬¬äºŒæ¬¡è¿è¡Œä¼šè·³è¿‡ setcircleã€‚
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

    ResetCxDebugRuntimeLog(context, "ResetDebugRuntimeForReplay");
    AppendCxDebugEvent(
        context,
        "runtime_reset",
        0,
        "",
        "",
        "",
        context.debug_status,
        context.debug_reason,
        "runtime reset for replay");
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
    bool seenOpenBrace = false;

    for (int i = openBraceIndex; i < static_cast<int>(context.line_views.size()); ++i)
    {
        const std::string s = TrimLine(context.line_views[static_cast<std::size_t>(i)].statement);

        for (char ch : s)
        {
            if (ch == '{')
            {
                ++depth;
                seenOpenBrace = true;
            }
            else if (ch == '}' && seenOpenBrace)
            {
                --depth;
                if (depth == 0)
                    return i;
            }
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
    const std::string ifStatement =
        TrimLine(context.line_views[static_cast<std::size_t>(ifIndex)].statement);

    if (ifStatement.find('{') != std::string::npos)
    {
        const int close = FindMatchingBraceLine(context, ifIndex);
        if (close >= 0)
            return FindNextNonEmptyLine(context, close + 1);
    }

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

static void MarkDebugRunFinishedIfAtEnd(ManualTestContext& context)
{
    const int next = FindNextNonEmptyLine(context, context.current_line);

    if (context.current_line >= static_cast<int>(context.line_views.size()) ||
        next >= static_cast<int>(context.line_views.size()))
    {
        context.current_line = static_cast<int>(context.line_views.size());
        context.run_state = "runtime_finished";
        context.debug_status = "PENDING";
        context.debug_reason =
            "script finished; global.current_status remains PENDING; judge/rule not executed";

        AppendCxDebugEvent(
            context,
            "debug_run_finished",
            context.current_line,
            "",
            "",
            "",
            context.debug_status,
            context.debug_reason,
            "script reached end");
    }
}


static void MarkLineAsStructural(ManualTestContext& context,
    int lineIndex,
    const std::string& reason)
{
    if (lineIndex < 0 ||
        lineIndex >= static_cast<int>(context.line_views.size()))
    {
        return;
    }

    ScriptLineView& line =
        context.line_views[static_cast<std::size_t>(lineIndex)];

    line.status = "structural";
    line.reason = reason;
    line.timestamp = CurrentTimestamp();

    AppendCxDebugEvent(
        context,
        "structural_line",
        line.line_no,
        line.statement,
        line.object,
        line.method,
        line.status,
        line.reason,
        "structural line marked by debugger");
}

static void MarkIfBlockBracesStructural(ManualTestContext& context,
    int ifLineIndex,
    bool markCloseBrace)
{
    const int next = FindNextNonEmptyLine(context, ifLineIndex + 1);

    if (next >= static_cast<int>(context.line_views.size()))
        return;

    if (!IsBraceOpenLine(context.line_views[static_cast<std::size_t>(next)].statement))
        return;

    MarkLineAsStructural(context, next, "open brace skipped by if debugger");

    if (markCloseBrace)
    {
        const int close = FindMatchingBraceLine(context, next);

        if (close >= 0)
        {
            MarkLineAsStructural(context, close, "close brace skipped by if debugger");
        }
    }
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

static std::string StripCxScriptQuotes(std::string value)
{
    value = TrimLine(value);
    if (!value.empty() && value.back() == ';')
        value.pop_back();
    value = TrimLine(value);
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
        value = value.substr(1, value.size() - 2);
    return value;
}

static bool ReadRuntimeVariableValue(const ManualTestContext& context,
    const std::string& name,
    std::string& value)
{
    const std::string key = TrimLine(name);

    for (const auto& variable : context.variable_views)
    {
        if (variable.name == key)
        {
            value = variable.value;
            return true;
        }
    }

    for (const auto& variable : context.global_variable_views)
    {
        if (variable.name == key)
        {
            value = variable.value;
            return true;
        }
    }

    return false;
}

static bool ReadRuntimeNumber(ManualTestContext& context,
    const std::string& token,
    double& value)
{
    const std::string key = TrimLine(token);

    int intValue = 0;
    if (ReadRuntimeInt(context, key, intValue))
    {
        value = static_cast<double>(intValue);
        return true;
    }

    std::string variableValue;
    if (ReadRuntimeVariableValue(context, key, variableValue))
    {
        char* end = nullptr;
        const double parsed = std::strtod(variableValue.c_str(), &end);
        if (end != variableValue.c_str())
        {
            value = parsed;
            return true;
        }
    }

    char* end = nullptr;
    const double parsed = std::strtod(key.c_str(), &end);
    if (end == key.c_str() || *end != '\0')
        return false;

    value = parsed;
    return true;
}

static bool ReadRuntimeString(const ManualTestContext& context,
    const std::string& token,
    std::string& value)
{
    const std::string key = TrimLine(token);

    if (key.size() >= 2 && key.front() == '"' && key.back() == '"')
    {
        value = StripCxScriptQuotes(key);
        return true;
    }

    return ReadRuntimeVariableValue(context, key, value);
}

static bool EvalSimpleCondition(ManualTestContext& context,
    const std::string& condition,
    bool& value)
{
    const std::string s = TrimLine(condition);

    const char* ops[] = {"==", "!=", "<=", ">=", "<", ">"};
    std::string opText;
    std::size_t op = std::string::npos;

    for (const char* candidate : ops)
    {
        op = s.find(candidate);
        if (op != std::string::npos)
        {
            opText = candidate;
            break;
        }
    }

    if (op == std::string::npos || opText.empty())
        return false;

    const std::string lhs = TrimLine(s.substr(0, op));
    const std::string rhs = TrimLine(s.substr(op + opText.size()));

    if (lhs.empty() || rhs.empty())
        return false;

    if (lhs.find('"') != std::string::npos ||
        rhs.find('"') != std::string::npos)
    {
        std::string lv;
        std::string rv;
        if (!ReadRuntimeString(context, lhs, lv) ||
            !ReadRuntimeString(context, rhs, rv))
            return false;

        if (opText == "==")
            value = (lv == rv);
        else if (opText == "!=")
            value = (lv != rv);
        else
            return false;

        return true;
    }

    double lv = 0.0;
    double rv = 0.0;
    if (!ReadRuntimeNumber(context, lhs, lv) ||
        !ReadRuntimeNumber(context, rhs, rv))
        return false;

    if (opText == "==")
        value = (lv == rv);
    else if (opText == "!=")
        value = (lv != rv);
    else if (opText == "<")
        value = (lv < rv);
    else if (opText == ">")
        value = (lv > rv);
    else if (opText == "<=")
        value = (lv <= rv);
    else if (opText == ">=")
        value = (lv >= rv);
    else
        return false;

    return true;
}

static bool TryExecuteIntDeclarationAssignment(ManualTestContext& context,
    int lineIndex,
    const std::string& statement)
{
    std::string s = TrimLine(statement);

    if (s.rfind("int ", 0) != 0)
        return false;

    if (!s.empty() && s.back() == ';')
        s.pop_back();

    const std::size_t eq = s.find('=');

    if (eq == std::string::npos)
        return false;

    std::string lhs = TrimLine(s.substr(4, eq - 4));
    std::string rhs = TrimLine(s.substr(eq + 1));

    if (lhs.empty())
        return false;

    int v = 0;
    if (!ReadRuntimeInt(context, rhs, v))
    {
        char* end = nullptr;
        const long parsed = std::strtol(rhs.c_str(), &end, 10);
        if (end == rhs.c_str() || *end != '\0')
        {
            ScriptLineView& line =
                context.line_views[static_cast<std::size_t>(lineIndex)];
            line.status = "BLOCKED";
            line.reason = "int initializer unresolved: " + rhs;
            line.return_variable = lhs;
            line.timestamp = CurrentTimestamp();

            context.run_state = "blocked";
            context.debug_status = "BLOCKED";
            context.debug_reason = line.reason;
            return true;
        }

        v = static_cast<int>(parsed);
    }

    context.runtime_int_vars[lhs] = v;

    ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
    line.status = "runtime_executed";
    line.reason =
        "int variable initialized | " + lhs + "=" + std::to_string(v);
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
    context.debug_reason = line.reason;

    AppendCxDebugEvent(
        context,
        "int_variable_initialized",
        line.line_no,
        statement,
        lhs,
        "int_init",
        line.status,
        line.reason,
        lhs + "=" + std::to_string(v));

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

    // 1. è°ƒè¯•æŽ§åˆ¶å˜é‡ï¼šm_isetcircle = 1;
    if (lhs == "m_isetcircle")
    {
        const int v = std::atoi(rhs.c_str());

        context.runtime_int_vars[lhs] = v;

        line.status = "runtime_executed";
        line.reason = "assignment executed";

        line.return_variable = lhs;
        line.timestamp = CurrentTimestamp();
        AppendCxDebugEvent(
            context,
            "global_current_status_set",
            line.line_no,
            statement,
            lhs,
            "assignment",
            line.status,
            line.reason,
            "current_status=" + rhs);
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
        MarkDebugRunFinishedIfAtEnd(context);
        return true;
    }

    // 2. å…¨å±€çŠ¶æ€å˜é‡ï¼šglobal.current_status = "PENDING";
    if (lhs == "global.current_status")
    {
        // åŽ»æŽ‰å­—ç¬¦ä¸²ä¸¤ä¾§å¼•å·
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

        // åŒæ­¥åˆ° Local Variables / Variable Snapshotï¼Œæ–¹ä¾¿ç•Œé¢ç»Ÿä¸€è§‚å¯Ÿ
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
        MarkDebugRunFinishedIfAtEnd(context);
        context.run_state = "runtime_step";

        // æ³¨æ„ï¼šè¿™é‡Œä¸èƒ½å› ä¸ºèµ‹å€¼æˆåŠŸå°± PASSã€‚
        context.debug_status = "PENDING";
        context.debug_reason = line.reason;

        return true;
    }

    if (lhs.rfind("global.", 0) == 0)
    {
        const bool isStringValue =
            rhs.size() >= 2 && rhs.front() == '"' && rhs.back() == '"';

        if (isStringValue)
        {
            const std::string stringValue = StripCxScriptQuotes(rhs);

            UpsertGlobalVariableView(
                context,
                "string",
                lhs,
                stringValue,
                line.line_no,
                "runtime_value");

            UpsertVariableView(
                context,
                "string",
                lhs,
                stringValue,
                line.line_no,
                "runtime_value");

            line.status = "runtime_executed";
            line.reason = "global string assignment executed | " + lhs + "=" + stringValue;
            line.return_variable = lhs;
            line.timestamp = CurrentTimestamp();

            context.current_line = FindNextNonEmptyLine(context, lineIndex + 1);
            context.run_state = "runtime_step";
            context.debug_status = "PENDING";
            context.debug_reason = line.reason;
            MarkDebugRunFinishedIfAtEnd(context);
            return true;
        }

        double numericValue = 0.0;
        if (ReadRuntimeNumber(context, rhs, numericValue))
        {
            const int intValue = static_cast<int>(numericValue);
            const bool integerLike =
                std::fabs(numericValue - static_cast<double>(intValue)) < 0.000001;

            if (integerLike)
                context.runtime_int_vars[lhs] = intValue;

            std::ostringstream valueStream;
            valueStream << numericValue;
            const std::string valueText = integerLike
                ? std::to_string(intValue)
                : valueStream.str();

            UpsertGlobalVariableView(
                context,
                integerLike ? "int" : "double",
                lhs,
                valueText,
                line.line_no,
                "runtime_value");

            UpsertVariableView(
                context,
                integerLike ? "int" : "double",
                lhs,
                valueText,
                line.line_no,
                "runtime_value");

            line.status = "runtime_executed";
            line.reason = "global numeric assignment executed | " + lhs + "=" + valueText;
            line.return_variable = lhs;
            line.timestamp = CurrentTimestamp();

            context.current_line = FindNextNonEmptyLine(context, lineIndex + 1);
            context.run_state = "runtime_step";
            context.debug_status = "PENDING";
            context.debug_reason = line.reason;
            MarkDebugRunFinishedIfAtEnd(context);
            return true;
        }
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
        type == "FormfitGauge" || type == "CxOverlay" || type == "CircleRingGauge") return "cximage";
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
    else if (type == "Findline")
    {
        runtime.lines[name] = std::make_unique<Findline>();
        object.exists_in_parser = true;
        object.runtime_state = "runtime_object_created";
        object.last_runtime_status = "runtime_executed";
        object.display_summary = "Findline runtime object created";
        object.visualizable = false;
    }



    object.exists_in_parser = true;
    object.runtime_state = "declared";
    object.last_runtime_status = "runtime_executed";
    object.last_method = "declare";
    object.display_summary = "declared only; no visual geometry";
    object.last_update_line = context.line_views[static_cast<std::size_t>(lineIndex)].line_no;

    // å…³é”®ï¼šå£°æ˜Ž Findcircle ä¸èƒ½ç”»åœ†ã€‚
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
static void RefreshFindcircleDisplaySnapshot(ManualTestContext& context,
    RuntimeObjectView& object)
{
    if (object.type != "Findcircle")
        return;

    object.has_circle_roi_outer_polyline = false;
    object.has_circle_roi_inner_polyline = false;
    object.circle_roi_outer_xy.clear();
    object.circle_roi_inner_xy.clear();
    object.circle_roi_segment_count = 0;

    if (object.has_circle)
    {
        const CxCirclePolylineSnapshot outer = BuildCxCirclePolylineSnapshot(
            object.circle_cx,
            object.circle_cy,
            object.circle_radius);

        object.has_circle_roi_outer_polyline = outer.valid;
        if (outer.valid)
        {
            object.circle_roi_outer_xy = outer.xy;
            object.circle_roi_segment_count = outer.segment_count;
        }

        if (object.circle_inner > 0.0f)
        {
            const CxCirclePolylineSnapshot inner = BuildCxCirclePolylineSnapshot(
                object.circle_cx,
                object.circle_cy,
                object.circle_inner);

            object.has_circle_roi_inner_polyline = inner.valid;
            if (inner.valid)
                object.circle_roi_inner_xy = inner.xy;
        }
    }

    object.has_fit_circle_polyline = false;
    object.fit_circle_xy.clear();
    object.fit_circle_segment_count = 0;

    if (object.has_fit_result)
    {
        const CxCirclePolylineSnapshot fit = BuildCxCirclePolylineSnapshot(
            object.fit_cx,
            object.fit_cy,
            object.fit_radius);

        object.has_fit_circle_polyline = fit.valid;
        if (fit.valid)
        {
            object.fit_circle_xy = fit.xy;
            object.fit_circle_segment_count = fit.segment_count;
        }
    }

    ++object.display_version;
    ++context.runtime_overlay_version;
}
static void RefreshFindcircleMeasureGeometrySnapshot(
    RuntimeObjectView& object,
    Findcircle& circle)
{
    const FindcircleMeasureGeometryDebug& dbg =
        circle.lastmeasuregeometrydebug();

    object.circle_measure_geometry_request_valid =
        dbg.request_valid;

    object.circle_measure_geometry_dirty =
        dbg.geometry_dirty;

    object.circle_measure_geometry_ready =
        dbg.geometry_ready;

    object.circle_measure_geometry_version =
        dbg.geometry_version;

    object.circle_measure_geometry_built_version =
        dbg.geometry_built_version;

    object.circle_scan_line_count =
        dbg.scan_line_count;

    object.circle_scan_line_length =
        dbg.scan_line_length;

    object.circle_process_width =
        dbg.process_width;

    object.circle_measure_image_ready =
        dbg.image_ready;

    object.circle_measure_image_width =
        dbg.image_width;

    object.circle_measure_image_height =
        dbg.image_height;

    object.circle_measure_image_channels =
        dbg.image_channels;

    object.circle_measure_backimage_ready =
        dbg.backimage_ready;

    object.circle_measure_findobject_ready =
        dbg.findobject_ready;

    object.circle_measure_source =
        dbg.measure_source;

    object.circle_measure_failure_stage =
        dbg.failure_stage;

    object.circle_measure_detail =
        dbg.detail;

    object.circle_scan_lines_processed =
        dbg.scan_lines_processed;

    object.circle_total_samples =
        dbg.total_samples;

    object.circle_elapsed_ms =
        dbg.elapsed_ms;

    object.circle_budget_max_scan_lines =
        dbg.budget_max_scan_lines;

    object.circle_budget_max_samples =
        dbg.budget_max_samples;

    object.circle_budget_max_elapsed_ms =
        dbg.budget_max_elapsed_ms;
}

static bool ResolveDebugInt(ManualTestContext& context,
    const std::string& token,
    int& value);

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

    int circleValues[4] = {};
    for (int index = 0; index < 4; ++index)
    {
        if (!ResolveDebugInt(
                context,
                call.args[static_cast<std::size_t>(index)],
                circleValues[index]))
        {
            ScriptLineView& line =
                context.line_views[static_cast<std::size_t>(lineIndex)];
            line.status = "BLOCKED";
            line.reason = "Findcircle.setcircle unresolved parameter: " +
                call.args[static_cast<std::size_t>(index)];
            line.timestamp = CurrentTimestamp();
            context.run_state = "blocked";
            context.debug_status = "BLOCKED";
            context.debug_reason = line.reason;
            return true;
        }
    }

    object.circle_cx = static_cast<float>(circleValues[0]);
    object.circle_cy = static_cast<float>(circleValues[1]);
    object.circle_inner = static_cast<float>(circleValues[2]);
    object.circle_radius = static_cast<float>(circleValues[3]);

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

    RefreshFindcircleMeasureGeometrySnapshot(object, *circleIt->second);

    object.has_circle = true;
    RefreshFindcircleDisplaySnapshot(context, object);
    object.visualizable = true;
    object.exists_in_parser = true;
    object.stale = false;
    object.visual_source = "runtime_object";
    object.last_method = "setcircle";
    object.last_runtime_status = "runtime_executed";
    object.runtime_state = "runtime_param_set";
    object.last_update_line = context.line_views[static_cast<std::size_t>(lineIndex)].line_no;

    std::ostringstream summary;
    summary << "Findcircle.setcircle executed"
            << " | script_circle=("
            << object.circle_cx << ", "
            << object.circle_cy << ", "
            << object.circle_inner << ", "
            << object.circle_radius << ")"
            << " | native_perimeter=("
            << perimeterX << ", "
            << perimeterY << ")"
            << " | request_cache=updated";
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
    AppendCxDebugEvent(
        context,
        "findcircle_setcircle",
        line.line_no,
        statement,
        call.object,
        call.method,
        line.status,
        line.reason,
        object.display_summary);

    AppendCxDebugRuntimeObjectsSnapshot(context, "runtime_objects_after_setcircle");
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
        call.method == "setfitmeasuregap" ||
        call.method == "setcirclegap" ||
        call.method == "setlinesamplerate" ||
        call.method == "setgamarate" ||
        call.method == "setfindsetting" ||
        call.method == "setselectedgenum" ||
        call.method == "setfilter";

    if (!isCircleParamMethod)
        return false;

    if (!RuntimeObjectIsType(context, call.object, "Findcircle"))
    {
        /*
         * setmethod / setthre / setlinegap are shared by Findcircle and Findline.
         * Do not let Findcircle handler consume Findline calls.
         */
        return false;
    }

    DebugCximageRuntime& runtime = CxRuntime(context);

    auto circleIt = runtime.circles.find(call.object);
    if (circleIt == runtime.circles.end() || !circleIt->second)
        return false;

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

    if (call.method == "setfilter")
    {
        ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
        if (call.args.size() < 3)
        {
            line.status = "BLOCKED";
            line.reason = "Findcircle.setfilter requires borw, min, max";
            line.timestamp = CurrentTimestamp();
            context.run_state = "blocked";
            context.debug_status = "BLOCKED";
            context.debug_reason = line.reason;
            return true;
        }

        int borw = 0;
        int minArea = 0;
        int maxArea = 0;
        if (!ResolveDebugInt(context, call.args[0], borw) ||
            !ResolveDebugInt(context, call.args[1], minArea) ||
            !ResolveDebugInt(context, call.args[2], maxArea))
        {
            line.status = "BLOCKED";
            line.reason = "Findcircle.setfilter unresolved parameter";
            line.timestamp = CurrentTimestamp();
            context.run_state = "blocked";
            context.debug_status = "BLOCKED";
            context.debug_reason = line.reason;
            return true;
        }

        circleIt->second->setfilter(borw, minArea, maxArea);
        RuntimeObjectView& object = EnsureRuntimeObject(
            context, call.object, "Findcircle", line.line_no);
        RefreshFindcircleMeasureGeometrySnapshot(object, *circleIt->second);
        object.exists_in_parser = true;
        object.last_method = call.method;
        object.last_runtime_status = "runtime_executed";
        object.runtime_state = "runtime_param_set";
        object.last_update_line = line.line_no;
        object.display_summary = "setfilter(" + call.params + ")";
        object.stale = false;
        line.status = "runtime_executed";
        line.reason = "Findcircle.setfilter executed";
        line.timestamp = CurrentTimestamp();
        context.current_line = FindNextNonEmptyLine(context, lineIndex + 1);
        context.run_state = "runtime_step";
        context.debug_status = "PENDING";
        context.debug_reason = line.reason;
        return true;
    }

    if (call.method == "setlinesamplerate")
    {
        const std::string token = TrimLine(call.args[0]);
        char* end = nullptr;
        const double sampleRate = std::strtod(token.c_str(), &end);
        if (end == token.c_str() || *end != '\0')
        {
            ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
            line.status = "BLOCKED";
            line.reason = "Findcircle.setlinesamplerate unresolved parameter: " + token;
            line.timestamp = CurrentTimestamp();
            context.run_state = "blocked";
            context.debug_status = "BLOCKED";
            context.debug_reason = line.reason;
            return true;
        }
        circleIt->second->setlinesamplerate(sampleRate);
        RuntimeObjectView& object = EnsureRuntimeObject(
            context, call.object, "Findcircle",
            context.line_views[static_cast<std::size_t>(lineIndex)].line_no);
        RefreshFindcircleMeasureGeometrySnapshot(object, *circleIt->second);
        object.exists_in_parser = true;
        object.last_method = call.method;
        object.last_runtime_status = "runtime_executed";
        object.runtime_state = "runtime_param_set";
        object.last_update_line = context.line_views[static_cast<std::size_t>(lineIndex)].line_no;
        object.display_summary = "setlinesamplerate(" + token + ")";
        object.stale = false;
        ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
        line.status = "runtime_executed";
        line.reason = "Findcircle.setlinesamplerate executed";
        line.timestamp = CurrentTimestamp();
        context.current_line = FindNextNonEmptyLine(context, lineIndex + 1);
        context.run_state = "runtime_step";
        context.debug_status = "PENDING";
        context.debug_reason = line.reason;
        return true;
    }

    int value = 0;
    if (!ResolveDebugInt(context, call.args[0], value))
    {
        ScriptLineView& line =
            context.line_views[static_cast<std::size_t>(lineIndex)];
        line.status = "BLOCKED";
        line.reason = "Findcircle." + call.method +
            " unresolved parameter: " + call.args[0];
        line.timestamp = CurrentTimestamp();
        context.run_state = "blocked";
        context.debug_status = "BLOCKED";
        context.debug_reason = line.reason;
        return true;
    }

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
    else if (call.method == "setcirclegap")
        circleIt->second->setcirclegap(value);
    else if (call.method == "setgamarate")
        circleIt->second->setgamarate(value);
    else if (call.method == "setfindsetting")
        circleIt->second->setfindsetting(value);
    else if (call.method == "setselectedgenum")
        circleIt->second->setselectedgenum(value);
    else if (call.method == "setlinesamplerate")
        circleIt->second->setlinesamplerate(
            static_cast<double>(value));

    RefreshFindcircleMeasureGeometrySnapshot(
        EnsureRuntimeObject(
            context,
            call.object,
            "Findcircle",
            context.line_views[static_cast<std::size_t>(lineIndex)].line_no),
        *circleIt->second);

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

    RefreshFindcircleMeasureGeometrySnapshot(object, circle);
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
            file << "      \"valid_points_count\": " << object.valid_points_count << ",\n";
            file << "      \"has_line_roi\": " << (object.has_line_roi ? "true" : "false") << ",\n";
            file << "      \"line_roi\": [" << object.line_x0 << ", " << object.line_y0 << ", "
                << object.line_x1 << ", " << object.line_y1 << "],\n";
            file << "      \"has_line_scan_box\": " << (object.has_line_scan_box ? "true" : "false") << ",\n";
            file << "      \"line_scan_half_width\": " << object.line_scan_half_width << ",\n";
            file << "      \"line_pointsw_count\": " << object.line_pointsw_count << ",\n";
            file << "      \"line_pointsh_count\": " << object.line_pointsh_count << ",\n";
            file << "      \"line_measure_points_count\": " << object.line_measure_points_count << ",\n";
            file << "      \"valid_line_points_count\": " << object.valid_line_points_count << ",\n";
            file << "      \"has_fit_line\": " << (object.has_fit_line ? "true" : "false") << ",\n";
            file << "      \"fit_line\": [" << object.fit_line_x0 << ", " << object.fit_line_y0 << ", "
                << object.fit_line_x1 << ", " << object.fit_line_y1 << "],\n";
            file << "      \"line_avgdist\": " << object.line_avgdist << ",\n";
            file << "      \"line_fit_mode\": \"" << EscapeJsonString(object.line_fit_mode) << "\",\n";
            file << "      \"line_fit_status\": \"" << EscapeJsonString(object.line_fit_status) << "\",\n";
            file << "      \"line_measure_status\": \"" << EscapeJsonString(object.line_measure_status) << "\"\n";
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

    if (!RuntimeObjectIsType(context, call.object, "Findcircle"))
    {
        /*
         * Do not let Findcircle runtime bridge handle Findline.measure().
         */
        return false;
    }

    DebugCximageRuntime& runtime = CxRuntime(context);

    auto circleIt = runtime.circles.find(call.object);
    if (circleIt == runtime.circles.end() || !circleIt->second)
        return false;

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
            FillFindcircleResultView(object, *circleIt->second, "measure");
        }
        else if (call.method == "fitcircle")
        {
            circleIt->second->fitcircle();
            FillFindcircleResultView(object, *circleIt->second, "fitcircle");
        }
        else if (call.method == "FitResultMeasure")
        {
            if (!circleIt->second->canfitresultmeasure())
            {
                object.exists_in_parser = true;
                object.type = "Findcircle";
                object.last_method = "FitResultMeasure";
                object.last_runtime_status = "PENDING_BINDING";
                object.runtime_state = "fitresultmeasure_skipped";
                object.last_update_line = context.line_views[static_cast<std::size_t>(lineIndex)].line_no;
                object.visualizable = true;
                object.stale = false;

                object.display_summary =
                    "Findcircle.FitResultMeasure skipped | reason=fitcircle result is not valid";

                ScriptLineView& skipLine = context.line_views[static_cast<std::size_t>(lineIndex)];
                skipLine.status = "PENDING_BINDING";
                skipLine.reason = object.display_summary;
                skipLine.timestamp = CurrentTimestamp();

                context.current_line = FindNextNonEmptyLine(context, lineIndex + 1);
                context.run_state = "runtime_step";
                context.debug_status = "PENDING";
                context.debug_reason = skipLine.reason;

                RefreshFindcircleDisplaySnapshot(context, object);
                RefreshFindcircleMeasureGeometrySnapshot(object, *circleIt->second);

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
                object.display_summary = diagnostics.str();

                return true;
            }

            circleIt->second->FitResultMeasure(image);
            FillFindcircleResultView(object, *circleIt->second, "FitResultMeasure");
            object.has_result_measure =
                object.has_fit_result || object.has_measure_points;
        }

        RefreshFindcircleDisplaySnapshot(context, object);

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

    object.last_update_line = context.line_views[static_cast<std::size_t>(lineIndex)].line_no;

    ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
    line.status = "runtime_executed";
    line.reason = "Findcircle." + call.method +
        " executed by direct runtime bridge | " + object.display_summary;
    line.timestamp = CurrentTimestamp();
    AppendCxDebugEvent(
        context,
        "findcircle_runtime_method",
        line.line_no,
        statement,
        call.object,
        call.method,
        line.status,
        line.reason,
        object.display_summary);

    AppendCxDebugRuntimeObjectsSnapshot(
        context,
        "runtime_objects_after_" + call.method);
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
        AppendCxDebugEvent(
            context,
            "get_result_pending_binding",
            line.line_no,
            statement,
            sourceObjectName,
            "get_result",
            line.status,
            line.reason,
            "get_result did not bind");
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
        context.current_result_ref.result_type = "PendingGeometryResult";
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

        if (sourceObject->type == "Findline")
        {
            line.reason = sourceObject->line_result_reason.empty()
                ? "get_result requires a valid Findline fit result"
                : sourceObject->line_result_reason;
        }
        else
        {
            line.reason = "get_result requires a valid fit result; no result fabricated";
        }

        AppendCxDebugEvent(
            context,
            "get_result_pending_binding",
            line.line_no,
            statement,
            sourceObjectName,
            "get_result",
            line.status,
            line.reason,
            "get_result did not bind");
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
        context.current_result_ref.result_type = sourceObject->type == "Findline" ?
            "FindlineResult" : "FindcircleResult";
        context.current_result_ref.status = "PENDING_BINDING";
        context.current_result_ref.reason = line.reason;
        context.current_result_ref.line_no = line.line_no;

        if (sourceObject->type == "Findline")
        {
            context.current_result_ref.line_result_status =
                sourceObject->line_result_status;
            context.current_result_ref.line_result_reason =
                sourceObject->line_result_reason;
            context.current_result_ref.line_measure_status =
                sourceObject->line_measure_status;
            context.current_result_ref.line_measure_hint =
                sourceObject->line_measure_hint;
            context.current_result_ref.line_measure_failure_hint =
                sourceObject->line_measure_failure_hint;
            context.current_result_ref.line_filter_min_exceeds_component_p90 =
                sourceObject->line_filter_min_exceeds_component_p90;
        }

        // æ³¨æ„ï¼šè¿™é‡Œä¸ BLOCKEDï¼Œä¸ä¼ªé€  PASSï¼Œå…è®¸è„šæœ¬ç»§ç»­åˆ° global.current_statusã€‚
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
    context.current_result_ref.result_type = sourceObject->type == "Findline" ?
        "FindlineResult" : "FindcircleResult";
    context.current_result_ref.status = "geometry_result_available";
    context.current_result_ref.reason = "bound to runtime object geometry result";
    context.current_result_ref.fit_cx = sourceObject->fit_cx;
    context.current_result_ref.fit_cy = sourceObject->fit_cy;
    context.current_result_ref.fit_radius = sourceObject->fit_radius;
    context.current_result_ref.avgdist = sourceObject->fit_avgdist;
    context.current_result_ref.points_count = sourceObject->measure_points_count;
    context.current_result_ref.valid_points_count = sourceObject->valid_points_count;
    if (sourceObject->type == "Findline")
    {
        context.current_result_ref.line_x0 = sourceObject->fit_line_x0;
        context.current_result_ref.line_y0 = sourceObject->fit_line_y0;
        context.current_result_ref.line_x1 = sourceObject->fit_line_x1;
        context.current_result_ref.line_y1 = sourceObject->fit_line_y1;
        context.current_result_ref.line_avgdist = sourceObject->line_avgdist;
        context.current_result_ref.line_points_count = sourceObject->line_measure_points_count;
        context.current_result_ref.valid_line_points_count = sourceObject->valid_line_points_count;

        context.current_result_ref.line_measure_source =
            sourceObject->line_measure_source;

        context.current_result_ref.line_measure_fallback_used =
            sourceObject->line_measure_fallback_used;

        context.current_result_ref.line_measure_status =
            sourceObject->line_measure_status;
        context.current_result_ref.line_measure_hint =
            sourceObject->line_measure_hint;
    }
    context.current_result_ref.line_no = line.line_no;

    line.status = "runtime_executed";
    line.reason = lhs + " bound to " + refValue;
    AppendCxDebugEvent(
        context,
        "get_result_bound",
        line.line_no,
        statement,
        sourceObjectName,
        "get_result",
        line.status,
        line.reason,
        context.geometry_summary);

    AppendCxDebugRuntimeObjectsSnapshot(context, "runtime_objects_after_get_result");
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
static bool ResolveDebugInt(ManualTestContext& context, const std::string& token, int& value)
{
    const std::string key = TrimLine(token);
    const auto found = context.runtime_int_vars.find(key);
    if (found != context.runtime_int_vars.end()) { value = found->second; return true; }
    char* end = nullptr; const long parsed = std::strtol(key.c_str(), &end, 10);
    if (end == key.c_str() || *end != '\0') return false;
    value = static_cast<int>(parsed); return true;
}

static const char* FindlineModeName(int mode)
{
    static const char* names[] = {"Unspecified", "LeastSquares", "MinimumZone", "Ransac", "SingleEdge", "EdgePairCenter", "HorizontalVerticalPriority", "WeightedMeasurementPoints"};
    return mode >= 0 && mode < 8 ? names[mode] : "Unspecified";
}

static void AppendPointsShapeToXY(PointsShape& points, std::vector<float>& outXY)
{
    for (int i = 0; i < points.size(); ++i)
    {
        const double x = points.getx(i);
        const double y = points.gety(i);
        if (!std::isfinite(x) || !std::isfinite(y))
            continue;
        outXY.push_back(static_cast<float>(x));
        outXY.push_back(static_cast<float>(y));
    }
}

static void RefreshFindlineDisplaySnapshot(ManualTestContext& context,
                                           RuntimeObjectView& object,
                                           Findline& lineTool)
{
    if (object.type != "Findline")
    {
        object.has_line_scan_box = false;
        return;
    }

    FindlineDisplaySnapshot snapshot;

    if (!lineTool.getdisplaysnapshot(snapshot))
    {
        object.has_line_roi = false;
        object.has_line_scan_box = false;
        object.line_display_source = "Findline::getdisplaysnapshot unavailable";
        return;
    }

    object.has_line_roi = snapshot.has_line_roi;
    object.line_x0 = snapshot.x0;
    object.line_y0 = snapshot.y0;
    object.line_x1 = snapshot.x1;
    object.line_y1 = snapshot.y1;
    object.line_scale = snapshot.scale;

    object.line_tool_wgap = snapshot.wgap;
    object.line_tool_hgap = snapshot.hgap;
    object.linegap = snapshot.linegap;

    object.has_line_scan_box = snapshot.has_scan_box;
    object.line_scan_half_width = snapshot.scan_half_width;
    object.line_scan_box_xy = snapshot.scan_box_xy;
    object.line_display_source = snapshot.source;

    ++object.display_version;
    ++context.runtime_overlay_version;
}



static std::string BuildFindlineMeasureHint(const RuntimeObjectView& object)
{
    if (object.valid_line_points_count > 0)
        return "";

    if (!object.line_measure_roi_intersects_image)
        return "Findline ROI does not intersect image.";

    if (object.line_measure_findobject_called &&
        object.line_measure_cc_selected_accepted == 0 &&
        object.line_measure_cc_selected_total > 0 &&
        object.line_measure_effective_filter_min > object.line_measure_cc_selected_area_p90)
    {
        return "Findline original Measure produced no points because FindObject accepted no connected components. effective_filter_min is higher than selected component P90. Try Stage25 filter profile: m_line.setfilterprofile(1).";
    }

    if (object.line_measure_binary_foreground_pixels > 0 &&
        object.line_measure_cc_selected_total == 0)
    {
        return "Binary foreground exists but no selected connected components. Check filter_borw / foreground polarity.";
    }

    if (object.line_measure_findobject_called &&
        object.line_measure_cc_selected_total > 0 &&
        object.line_measure_cc_selected_accepted == 0)
    {
        return "FindObject was called but accepted no selected connected components. Check filter_min/filter_max/filter_borw and polarity.";
    }

    if (object.line_measure_binary_foreground_pixels == 0)
        return "Findline binary foreground is empty. Check threshold, method polarity, and gamma.";

    return "Findline original Measure produced no valid points. Check ROI, scan width, threshold, polarity, and filter settings.";
}
static void RefreshFindlineMeasureSnapshot(RuntimeObjectView& object,
    Findline& lineTool)
{
    object.line_measure_points_xy.clear();

    PointsShape& pw = lineTool.getresultpointsw();
    PointsShape& ph = lineTool.getresultpointsh();

    object.line_pointsw_count = pw.size();
    object.line_pointsh_count = ph.size();

    AppendPointsShapeToXY(pw, object.line_measure_points_xy);
    AppendPointsShapeToXY(ph, object.line_measure_points_xy);

    object.line_measure_points_count =
        object.line_pointsw_count + object.line_pointsh_count;

    object.valid_line_points_count =
        static_cast<int>(object.line_measure_points_xy.size() / 2);

    object.has_line_measure_points =
        !object.line_measure_points_xy.empty();

    object.measure_points_count = object.line_measure_points_count;
    object.valid_points_count = object.valid_line_points_count;
    object.has_measure_points = object.has_line_measure_points;

    object.line_seek_points_xy.clear();
    lineTool.exportmeasuredebugpoints(object.line_seek_points_xy);

    object.line_seek_points_count =
        static_cast<int>(object.line_seek_points_xy.size() / 2);

    object.has_line_seek_points =
        !object.line_seek_points_xy.empty();

    const FindlineMeasureProfileStats& stats =
        lineTool.lastmeasureprofilestats();

    object.line_profile_point_count = stats.point_count;
    object.line_edgeband_count = stats.edgeband_count;
    object.line_chain_length = stats.chain_length;

    if (object.valid_line_points_count > 0)
    {
        object.line_measure_failure_stage = "result_points_available";
    }
    else if (object.line_edgeband_count <= 0)
    {
        object.line_measure_failure_stage = "no_edge_band_candidates";
    }
    else if (object.line_chain_length <= 0)
    {
        object.line_measure_failure_stage = "no_valid_edge_chain";
    }
    else
    {
        object.line_measure_failure_stage = "chain_not_converted_to_measure_points";
    }

    const FindlineMeasureInputDebug& input =
        lineTool.lastmeasureinputdebug();

    object.line_measure_image_ready = input.image_mat_ready;
    object.line_measure_image_width = input.image_width;
    object.line_measure_image_height = input.image_height;
    object.line_measure_image_channels = input.image_channels;
    object.line_measure_image_type = input.image_type;

    object.line_measure_roi_intersects_image =
        input.roi_intersects_image;

    object.line_measure_roi_fully_inside_image =
        input.roi_fully_inside_image;

    object.line_measure_method = input.method;
    object.line_measure_threshold = input.threshold;
    object.line_measure_linegap = input.linegap;
    object.line_measure_wgap = input.wgap;
    object.line_measure_hgap = input.hgap;

    object.line_orientation = input.line_orientation;
    object.line_dx = input.line_dx;
    object.line_dy = input.line_dy;
    object.line_length = input.line_length;
    object.requested_tool_half_width = input.requested_tool_half_width;
    object.effective_tool_half_width = input.effective_tool_half_width;

    object.line_measure_backimage_ready =
        input.backimage_ready;

    object.line_measure_findobject_ready =
        input.findobject_ready;

    object.line_measure_objfilterset =
        input.objfilterset;

    object.line_measure_filter_borw =
        input.filter_borw;

    object.line_measure_filter_min =
        input.filter_min;

    object.line_measure_filter_max =
        input.filter_max;

    object.line_measure_filter_profile =
        input.filter_profile;

    object.line_measure_filter_explicit =
        input.filter_explicit;

    object.line_measure_effective_filter_borw =
        input.effective_filter_borw;

    object.line_measure_effective_filter_min =
        input.effective_filter_min;

    object.line_measure_effective_filter_max =
        input.effective_filter_max;

    object.line_measure_findobject_called =
        input.findobject_measure_called;

    object.line_measure_findobject_skipped =
        input.findobject_measure_skipped;

    object.line_measure_binary_foreground_pixels =
        input.binary_foreground_pixels;

    object.line_measure_binary_roi_width =
        input.binary_roi_width;

    object.line_measure_binary_roi_height =
        input.binary_roi_height;

    object.line_measure_result_empty_reason =
        input.result_empty_reason;

    object.line_findobject_component_total =
        input.findobject_component_total;

    object.line_findobject_component_accepted =
        input.findobject_component_accepted;

    object.line_findobject_component_rejected_by_min =
        input.findobject_component_rejected_by_min;

    object.line_findobject_component_rejected_by_max =
        input.findobject_component_rejected_by_max;

    object.line_findobject_component_rejected_by_borw =
        input.findobject_component_rejected_by_borw;

    object.line_findobject_area_min_observed =
        input.findobject_area_min_observed;

    object.line_findobject_area_max_observed =
        input.findobject_area_max_observed;

    object.line_findobject_area_mean_observed =
        input.findobject_area_mean_observed;

    object.line_findobject_area_min =
        input.findobject_area_min_observed;

    object.line_findobject_area_max =
        input.findobject_area_max_observed;

    object.line_findobject_area_median =
        input.findobject_area_median_observed;

    object.line_findobject_area_p90 =
        input.findobject_area_p90_observed;

    object.line_measure_cc_selected_foreground =
        input.cc_selected_foreground;

    object.line_measure_cc_white_total = input.cc_white.component_total;
    object.line_measure_cc_white_accepted = input.cc_white.accepted_by_area;
    object.line_measure_cc_white_rejected_min = input.cc_white.rejected_by_min;
    object.line_measure_cc_white_area_median = input.cc_white.area_median;
    object.line_measure_cc_white_area_p90 = input.cc_white.area_p90;

    object.line_measure_cc_black_total = input.cc_black.component_total;
    object.line_measure_cc_black_accepted = input.cc_black.accepted_by_area;
    object.line_measure_cc_black_rejected_min = input.cc_black.rejected_by_min;
    object.line_measure_cc_black_area_median = input.cc_black.area_median;
    object.line_measure_cc_black_area_p90 = input.cc_black.area_p90;

    object.line_measure_cc_selected_total = input.cc_selected.component_total;
    object.line_measure_cc_selected_accepted = input.cc_selected.accepted_by_area;
    object.line_measure_cc_selected_rejected_min = input.cc_selected.rejected_by_min;
    object.line_measure_cc_selected_area_median = input.cc_selected.area_median;
    object.line_measure_cc_selected_area_p90 = input.cc_selected.area_p90;

    object.line_measure_profile_count = input.profile_count;
    object.line_measure_sampled_pixel_count = input.sampled_pixel_count;

    object.line_measure_gray_min = input.gray_min;
    object.line_measure_gray_max = input.gray_max;
    object.line_measure_gray_mean = input.gray_mean;
    object.line_measure_max_gradient = input.max_gradient;

    object.line_measure_image_source = input.image_source;
    object.line_measure_input_failure_stage = input.failure_stage;
    object.line_measure_input_detail = input.detail;

    object.line_measure_fallback_allowed = input.fallback_allowed;
    object.line_measure_fallback_used = input.fallback_used;
    object.line_measure_source = input.measure_source;

    if (object.line_measure_source.empty())
    {
        if (object.valid_line_points_count > 0)
        {
            object.line_measure_source = "unknown_source_with_result";
        }
        else
        {
            object.line_measure_source = "original_measure_pipeline_no_result";
        }
    }

    object.line_measure_original_failure_stage =
        input.original_failure_stage;

    object.line_measure_original_detail =
        input.original_detail;

    object.line_measure_original_point_count =
        input.original_point_count;

    object.line_measure_original_edgeband_count =
        input.original_edgeband_count;

    object.line_measure_original_chain_length =
        input.original_chain_length;

    if (!input.failure_stage.empty())
    {
        object.line_measure_failure_stage = input.failure_stage;
    }

    object.line_measure_hint = BuildFindlineMeasureHint(object);
    object.line_filter_min_exceeds_component_p90 =
        object.line_measure_failure_stage == "findobject_filter_result_empty" &&
        object.line_measure_effective_filter_min > 0 &&
        object.line_measure_cc_selected_area_p90 > 0.0 &&
        object.line_measure_effective_filter_min > object.line_measure_cc_selected_area_p90;
    object.line_measure_failure_hint = object.line_filter_min_exceeds_component_p90 ?
        object.line_measure_hint : std::string();

    object.line_measure_geometry_request_valid =
        input.measure_geometry_request_valid;

    object.line_measure_geometry_dirty =
        input.measure_geometry_dirty;

    object.line_measure_geometry_ready =
        input.measure_geometry_ready;

    object.line_measure_geometry_version =
        input.measure_geometry_version;

    object.line_measure_geometry_built_version =
        input.measure_geometry_built_version;

    object.line_measure_geometry_half_width =
        input.measure_geometry_half_width;

    object.line_original_scan_w_count =
        input.original_scan_w_count;

    object.line_original_scan_h_count =
        input.original_scan_h_count;

    object.line_original_scan_w_length =
        input.original_scan_w_length;

    object.line_original_scan_h_length =
        input.original_scan_h_length;

    object.line_original_process_width =
        input.original_process_width;

    std::ostringstream status;
    status << "source=" << object.line_measure_source
           << ", fallback_used="
           << (object.line_measure_fallback_used ? "true" : "false")
           << ", image_ready="
           << (object.line_measure_image_ready ? "true" : "false")
           << ", scan_w=" << object.line_original_scan_w_count
           << ", scan_h=" << object.line_original_scan_h_count
           << ", scan_w_len=" << object.line_original_scan_w_length
           << ", scan_h_len=" << object.line_original_scan_h_length
           << ", process_w=" << object.line_original_process_width
           << ", image=" << object.line_measure_image_width
           << "x" << object.line_measure_image_height
           << "x" << object.line_measure_image_channels
           << ", roi_intersects="
           << (object.line_measure_roi_intersects_image ? "true" : "false")
           << ", threshold=" << object.line_measure_threshold
           << ", max_gradient=" << object.line_measure_max_gradient
           << ", original_points=" << object.line_measure_original_point_count
           << ", original_edgebands=" << object.line_measure_original_edgeband_count
           << ", original_chain=" << object.line_measure_original_chain_length
           << ", pointsw=" << object.line_pointsw_count
           << ", pointsh=" << object.line_pointsh_count
           << ", valid_xy=" << object.valid_line_points_count
           << ", seek_points=" << object.line_seek_points_count
           << ", edgebands=" << object.line_edgeband_count
           << ", chain=" << object.line_chain_length
           << ", failure_stage=" << object.line_measure_failure_stage
           << ", backimage_ready="
           << (object.line_measure_backimage_ready ? "true" : "false")
           << ", findobject_ready="
           << (object.line_measure_findobject_ready ? "true" : "false")
           << ", objfilterset="
           << object.line_measure_objfilterset
           << ", filter_borw="
           << object.line_measure_filter_borw
           << ", filter_min="
           << object.line_measure_filter_min
           << ", filter_max="
           << object.line_measure_filter_max
           << ", filter_profile="
           << object.line_measure_filter_profile
           << ", effective_filter_min="
           << object.line_measure_effective_filter_min
           << ", cc_selected_total="
           << object.line_measure_cc_selected_total
           << ", cc_selected_accepted="
           << object.line_measure_cc_selected_accepted
           << ", cc_selected_area_p90="
           << object.line_measure_cc_selected_area_p90
           << ", findobject_called="
           << (object.line_measure_findobject_called ? "true" : "false")
           << ", binary_foreground="
           << object.line_measure_binary_foreground_pixels;

    if (!object.line_measure_result_empty_reason.empty())
    {
        status << ", empty_reason="
               << object.line_measure_result_empty_reason;
    }

    object.line_measure_status = status.str();
}

static bool ApplyRuntimeFindlineWHgap(
    ManualTestContext& context,
    const std::string& objectName,
    int wgap,
    int hgap,
    int updateLineNo,
    const char* updateSource,
    std::string& outReason)
{
    if (wgap <= 0 || hgap <= 0)
    {
        std::ostringstream ss;
        ss << "Findline.SetWHgap rejected invalid value"
           << " | wgap=" << wgap
           << " | hgap=" << hgap;
        outReason = ss.str();
        return false;
    }

    DebugCximageRuntime& runtime = CxRuntime(context);
    auto it = runtime.lines.find(objectName);
    if (it == runtime.lines.end() || it->second == nullptr)
    {
        outReason = "Findline runtime object not found: " + objectName;
        return false;
    }

    Findline& lineTool = *it->second;
    lineTool.SetWHgap(wgap, hgap);

    RuntimeObjectView& object = EnsureRuntimeObject(
        context,
        objectName,
        "Findline",
        updateLineNo);

    object.exists_in_parser = true;
    object.type = "Findline";
    object.last_method = "SetWHgap";
    object.last_runtime_status = "runtime_executed";
    object.runtime_state = "line_param_updated_measure_pending";
    object.last_update_line = updateLineNo;
    object.visualizable = true;
    object.visual_source = "runtime_object";
    object.stale = false;
    object.line_tool_wgap = wgap;
    object.line_tool_hgap = hgap;

    RefreshFindlineDisplaySnapshot(context, object, lineTool);
    RefreshFindlineMeasureSnapshot(object, lineTool);

    std::ostringstream ss;
    ss << "Findline.SetWHgap applied"
       << " | source="
       << (updateSource != nullptr ? updateSource : "unknown")
       << " | wgap=" << wgap
       << " | hgap=" << hgap;

    if (object.has_line_scan_box)
    {
        ss << " | scan_half_width=" << object.line_scan_half_width
           << " | measure_pending=true";
    }
    else
    {
        ss << " | line_roi_pending=true";
    }

    object.display_summary = ss.str();
    outReason = object.display_summary;
    return true;
}

static bool TryExecuteFindlineSetline(ManualTestContext& context,
    int lineIndex,
    const std::string& statement)
{
    const ParsedMethodCall call = ParseMethodCall(statement);
    if (!call.valid || call.method != "setline")
        return false;

    DebugCximageRuntime& runtime = CxRuntime(context);
    auto it = runtime.lines.find(call.object);
    if (it == runtime.lines.end() || !it->second)
        return false;

    ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
    if (call.args.size() < 5)
    {
        line.status = "BLOCKED";
        line.reason = "Findline.setline requires 5 parameters";
        line.timestamp = CurrentTimestamp();
        context.run_state = "blocked";
        context.debug_status = "BLOCKED";
        context.debug_reason = line.reason;
        return true;
    }

    int values[5] = {};
    for (int i = 0; i < 5; ++i)
    {
        if (!ResolveDebugInt(context, call.args[static_cast<std::size_t>(i)], values[i]))
        {
            line.status = "BLOCKED";
            line.reason = "Findline.setline unresolved parameter: " + call.args[static_cast<std::size_t>(i)];
            line.timestamp = CurrentTimestamp();
            context.run_state = "blocked";
            context.debug_status = "BLOCKED";
            context.debug_reason = line.reason;
            return true;
        }
    }

    it->second->setline(values[0], values[1], values[2], values[3], values[4]);

    RuntimeObjectView& object = EnsureRuntimeObject(
        context,
        call.object,
        "Findline",
        line.line_no);

    object.exists_in_parser = true;
    object.type = "Findline";
    object.last_method = "setline";
    object.last_runtime_status = "runtime_executed";
    object.runtime_state = "runtime_param_set";
    object.last_update_line = line.line_no;

    object.has_line_roi = true;
    object.line_x0 = static_cast<float>(values[0]);
    object.line_y0 = static_cast<float>(values[1]);
    object.line_x1 = static_cast<float>(values[2]);
    object.line_y1 = static_cast<float>(values[3]);
    object.line_scale = static_cast<float>(values[4]);

    RefreshFindlineDisplaySnapshot(context, object, *it->second);

    object.visualizable = true;
    object.visual_source = "runtime_object";
    object.stale = false;

    std::ostringstream summary;
    summary << "Findline.setline executed"
            << " | line_roi=("
            << object.line_x0 << "," << object.line_y0
            << ")->(" << object.line_x1 << "," << object.line_y1 << ")"
            << " | scale=" << object.line_scale
            << " | scan_half_width=" << object.line_scan_half_width;

    object.display_summary = summary.str();

    line.status = "runtime_executed";
    line.reason = object.display_summary;
    line.timestamp = CurrentTimestamp();

    context.current_line = FindNextNonEmptyLine(context, lineIndex + 1);
    context.run_state = "runtime_step";
    context.debug_status = "PENDING";
    context.debug_reason = line.reason;
    context.runtime_current_status = "PENDING";
    return true;
}

static bool TryExecuteFindlineParamMethod(ManualTestContext& context,
    int lineIndex,
    const std::string& statement)
{
    const ParsedMethodCall call = ParseMethodCall(statement);
    if (!call.valid)
        return false;

    const bool isFindlineParamMethod =
        call.method == "setmethod" ||
        call.method == "setthre" ||
        call.method == "setlinegap" ||
        call.method == "setfitmode" ||
        call.method == "SetWHgap" ||
        call.method == "setwhgap" ||
        call.method == "setmeasurefallback" ||
        call.method == "setgamarate" ||
        call.method == "setobjfilter" ||
        call.method == "setfilter" ||
        call.method == "setfilterprofile";

    if (!isFindlineParamMethod)
        return false;

    if (!RuntimeObjectIsType(context, call.object, "Findline"))
        return false;

    DebugCximageRuntime& runtime = CxRuntime(context);
    auto it = runtime.lines.find(call.object);
    if (it == runtime.lines.end() || !it->second)
        return false;

    ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
    if (call.args.empty())
    {
        line.status = "BLOCKED";
        line.reason = call.method + " requires one parameter";
        line.timestamp = CurrentTimestamp();
        context.run_state = "blocked";
        context.debug_status = "BLOCKED";
        context.debug_reason = line.reason;
        return true;
    }

    if (call.method == "SetWHgap" || call.method == "setwhgap")
    {
        if (call.args.size() < 2)
        {
            line.status = "BLOCKED";
            line.reason = "Findline.SetWHgap requires wgap and hgap";
            line.timestamp = CurrentTimestamp();

            context.run_state = "blocked";
            context.debug_status = "BLOCKED";
            context.debug_reason = line.reason;
            return true;
        }

        int wgap = 0;
        int hgap = 0;

        if (!ResolveDebugInt(context, call.args[0], wgap) ||
            !ResolveDebugInt(context, call.args[1], hgap))
        {
            line.status = "BLOCKED";
            line.reason = "Findline.SetWHgap failed to resolve wgap/hgap";
            line.timestamp = CurrentTimestamp();

            context.run_state = "blocked";
            context.debug_status = "BLOCKED";
            context.debug_reason = line.reason;
            return true;
        }

        if (wgap <= 0 || hgap <= 0)
        {
            line.status = "BLOCKED";

            std::ostringstream reason;
            reason << "Findline.SetWHgap blocked"
                   << " | invalid wgap=" << wgap
                   << " | invalid hgap=" << hgap;

            line.reason = reason.str();
            line.timestamp = CurrentTimestamp();

            context.run_state = "blocked";
            context.debug_status = "BLOCKED";
            context.debug_reason = line.reason;
            return true;
        }

        std::string applyReason;
        if (!ApplyRuntimeFindlineWHgap(
                context,
                call.object,
                wgap,
                hgap,
                line.line_no,
                "script",
                applyReason))
        {
            line.status = "BLOCKED";
            line.reason = applyReason;
            line.timestamp = CurrentTimestamp();
            context.run_state = "blocked";
            context.debug_status = "BLOCKED";
            context.debug_reason = line.reason;
            return true;
        }

        line.status = "runtime_executed";
        line.reason = applyReason;
        line.timestamp = CurrentTimestamp();

        context.current_line = FindNextNonEmptyLine(context, lineIndex + 1);
        context.run_state = "runtime_step";
        context.debug_status = "PENDING";
        context.debug_reason = line.reason;

        AppendCxDebugEvent(
            context,
            "findline_param_method",
            line.line_no,
            statement,
            call.object,
            call.method,
            line.status,
            line.reason,
            applyReason);

        AppendCxDebugRuntimeObjectsSnapshot(
            context,
            "runtime_objects_after_findline_setwhgap");

        return true;
    }

    if (call.method == "setfilter")
    {
        if (call.args.size() < 3)
        {
            line.status = "BLOCKED";
            line.reason = "Findline.setfilter requires borw, min, max";
            line.timestamp = CurrentTimestamp();

            context.run_state = "blocked";
            context.debug_status = "BLOCKED";
            context.debug_reason = line.reason;
            return true;
        }

        int borw = 0;
        int minArea = 0;
        int maxArea = 0;

        if (!ResolveDebugInt(context, call.args[0], borw) ||
            !ResolveDebugInt(context, call.args[1], minArea) ||
            !ResolveDebugInt(context, call.args[2], maxArea))
        {
            line.status = "BLOCKED";
            line.reason = "Findline.setfilter failed to resolve parameters";
            line.timestamp = CurrentTimestamp();

            context.run_state = "blocked";
            context.debug_status = "BLOCKED";
            context.debug_reason = line.reason;
            return true;
        }

        it->second->setfilter(borw, minArea, maxArea);

        RuntimeObjectView& object = EnsureRuntimeObject(
            context,
            call.object,
            "Findline",
            line.line_no);

        object.exists_in_parser = true;
        object.type = "Findline";
        object.last_method = "setfilter";
        object.last_runtime_status = "runtime_executed";
        object.runtime_state = "line_param_updated_measure_pending";
        object.last_update_line = line.line_no;
        object.visualizable = true;
        object.visual_source = "runtime_object";
        object.stale = false;

        RefreshFindlineDisplaySnapshot(context, object, *it->second);
        RefreshFindlineMeasureSnapshot(object, *it->second);

        std::ostringstream summary;
        summary << "Findline.setfilter executed"
                << " | borw=" << borw
                << " | min=" << minArea
                << " | max=" << maxArea
                << " | measure_pending=true";

        object.display_summary = summary.str();

        line.status = "runtime_executed";
        line.reason = object.display_summary;
        line.timestamp = CurrentTimestamp();

        context.current_line =
            FindNextNonEmptyLine(context, lineIndex + 1);

        context.run_state = "runtime_step";
        context.debug_status = "PENDING";
        context.debug_reason = line.reason;

        return true;
    }

    if (call.method == "setfilterprofile")
    {
        int profile = 0;
        if (!ResolveDebugInt(context, call.args[0], profile))
        {
            line.status = "BLOCKED";
            line.reason = "Findline.setfilterprofile unresolved parameter";
            line.timestamp = CurrentTimestamp();
            context.run_state = "blocked";
            context.debug_status = "BLOCKED";
            context.debug_reason = line.reason;
            return true;
        }

        it->second->setfilterprofile(profile);

        RuntimeObjectView& object = EnsureRuntimeObject(
            context,
            call.object,
            "Findline",
            line.line_no);

        object.exists_in_parser = true;
        object.type = "Findline";
        object.last_method = "setfilterprofile";
        object.last_runtime_status = "runtime_executed";
        object.runtime_state = "line_param_updated_measure_pending";
        object.last_update_line = line.line_no;

        object.visualizable = true;
        object.visual_source = "runtime_object";
        object.stale = false;

        RefreshFindlineDisplaySnapshot(context, object, *it->second);
        RefreshFindlineMeasureSnapshot(object, *it->second);

        std::ostringstream summary;
        summary << "Findline.setfilterprofile executed"
                << " | profile=" << profile
                << " | measure_pending=true";

        object.display_summary = summary.str();

        line.status = "runtime_executed";
        line.reason = object.display_summary;
        line.timestamp = CurrentTimestamp();

        context.current_line =
            FindNextNonEmptyLine(context, lineIndex + 1);

        context.run_state = "runtime_step";
        context.debug_status = "PENDING";
        context.debug_reason = line.reason;

        return true;
    }

    int value = 0;
    if (!ResolveDebugInt(context, call.args[0], value))
    {
        line.status = "BLOCKED";
        line.reason = call.method + " unresolved parameter";
        line.timestamp = CurrentTimestamp();
        context.run_state = "blocked";
        context.debug_status = "BLOCKED";
        context.debug_reason = line.reason;
        return true;
    }

    bool updatedLineGap = false;
    int updatedLineGapValue = 0;

    if (call.method == "setmethod")
        it->second->setmethod(value);
    else if (call.method == "setthre")
        it->second->setthre(value);
    else if (call.method == "setlinegap")
    {
        it->second->setlinegap(value);
        updatedLineGap = true;
        updatedLineGapValue = value;
    }
    else if (call.method == "setfitmode")
        it->second->setfitmode(value);
    else if (call.method == "setmeasurefallback")
        it->second->setmeasurefallback(value);
    else if (call.method == "setgamarate")
        it->second->setgamarate(value);
    else if (call.method == "setobjfilter")
        it->second->setobjfilter(value);

    RuntimeObjectView& object = EnsureRuntimeObject(
        context,
        call.object,
        "Findline",
        line.line_no);

    object.exists_in_parser = true;
    object.type = "Findline";
    object.last_method = call.method;
    object.last_runtime_status = "runtime_executed";
    object.runtime_state = "runtime_param_set";
    object.last_update_line = line.line_no;
    object.visualizable = true;
    object.visual_source = "runtime_object";
    object.stale = false;

    if (call.method == "setfitmode")
        object.line_fit_mode = FindlineModeName(value);

    if (call.method == "setmeasurefallback")
    {
        object.line_measure_fallback_allowed = value > 0;
        object.line_measure_fallback_used = false;
        object.line_measure_source.clear();
    }

    if (updatedLineGap)
    {
        RefreshFindlineDisplaySnapshot(context, object, *it->second);
    }

    object.display_summary = "Findline." + call.method + "(" + call.params + ")";

    line.status = "runtime_executed";
    line.reason = "Findline." + call.method + " executed";
    line.timestamp = CurrentTimestamp();

    context.current_line = FindNextNonEmptyLine(context, lineIndex + 1);
    context.run_state = "runtime_step";
    context.debug_status = "PENDING";
    context.debug_reason = line.reason;
    context.runtime_current_status = "PENDING";
    return true;
}

static bool TryExecuteFindlineRuntimeMethod(ManualTestContext& context, int lineIndex, const std::string& statement)
{
    const ParsedMethodCall call = ParseMethodCall(statement);
    if (!call.valid)
        return false;

    const bool isFindlineRuntimeMethod =
        call.method == "measure" ||
        call.method == "fitline" ||
        call.method == "FitLine";

    if (!isFindlineRuntimeMethod)
        return false;

    if (!RuntimeObjectIsType(context, call.object, "Findline"))
        return false;

    DebugCximageRuntime& runtime = CxRuntime(context);
    auto it = runtime.lines.find(call.object);
    if (it == runtime.lines.end() || !it->second)
        return false;

    Findline& tool = *it->second;
    RuntimeObjectView& object = EnsureRuntimeObject(context, call.object, "Findline", context.line_views[static_cast<std::size_t>(lineIndex)].line_no);
    ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];

    if (call.method == "measure")
    {
        if (call.args.empty())
        {
            line.status = "BLOCKED";
            line.reason = "Findline.measure requires image";
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
            line.status = "BLOCKED";
            line.reason = "Findline image object missing: " + imageName;
            line.timestamp = CurrentTimestamp();
            context.run_state = "blocked";
            context.debug_status = "BLOCKED";
            context.debug_reason = line.reason;
            return true;
        }

        Image* image = imageIt->second.get();
        tool.measure(static_cast<void*>(image));

        RefreshFindlineMeasureSnapshot(object, tool);
        RefreshFindlineDisplaySnapshot(context, object, tool);

        object.exists_in_parser = true;
        object.type = "Findline";
        object.last_method = "measure";
        object.last_runtime_status = "runtime_executed";
        object.runtime_state = "line_measure_points_available";
        object.last_update_line = line.line_no;
        object.visualizable = true;
        object.visual_source = "runtime_object";
        object.stale = false;

        std::ostringstream summary;
        summary << "Findline.measure executed"
                << " | line_roi=("
                << object.line_x0 << "," << object.line_y0
                << ")->(" << object.line_x1 << "," << object.line_y1 << ")"
                << " | scan_half_width=" << object.line_scan_half_width
                << " | " << object.line_measure_status;

        if (object.line_measure_points_count == 0)
            summary << " | no measure points returned by Findline tool";

        object.display_summary = summary.str();

        line.status = "runtime_executed";
        line.reason = object.display_summary;
    }
    else
    {
        tool.fitline();
        RefreshFindlineMeasureSnapshot(object, tool);
        RefreshFindlineDisplaySnapshot(context, object, tool);

        object.exists_in_parser = true;
        object.type = "Findline";
        object.last_method = call.method;
        object.last_update_line = line.line_no;
        object.visualizable = true;
        object.visual_source = "runtime_object";
        object.stale = false;
        object.line_fit_status = tool.getfitstatus();
        object.line_fit_mode = FindlineModeName(tool.getfitmodevalue());
        object.has_fit_line = tool.hasfitresult();

        if (object.has_fit_line)
        {
            object.fit_line_x0 = static_cast<float>(tool.getresultx0());
            object.fit_line_y0 = static_cast<float>(tool.getresulty0());
            object.fit_line_x1 = static_cast<float>(tool.getresultx1());
            object.fit_line_y1 = static_cast<float>(tool.getresulty1());
            object.line_avgdist = static_cast<float>(tool.getavgdist());
            object.valid_line_points_count = tool.getvalidpointcount();
            object.valid_points_count = object.valid_line_points_count;
            object.runtime_state = "geometry_result_available";
            object.last_runtime_status = "runtime_executed";
        }
        else
        {
            object.runtime_state = "fitline_pending_binding";
            object.last_runtime_status = "PENDING_BINDING";

            if (object.valid_line_points_count < 2)
            {
                object.line_result_status = "NO_VALID_MEASURE_POINTS";
                object.line_result_reason =
                    "Findline.fitline requires at least two valid measure points; "
                    + object.line_measure_status;
            }
            else
            {
                object.line_result_status = "FITLINE_PENDING_BINDING";
                object.line_result_reason = tool.getfitstatus();
            }
        }

        RefreshFindlineDisplaySnapshot(context, object, tool);

        std::ostringstream summary;
        summary << "Findline." << call.method << " executed"
                << " | result_status=" << object.line_result_status
                << " | reason=" << object.line_result_reason
                << " | fit_mode=" << object.line_fit_mode
                << " | fit_status=" << object.line_fit_status;
        if (object.has_fit_line)
            summary << " | fit=(" << object.fit_line_x0 << "," << object.fit_line_y0 << ")->(" << object.fit_line_x1 << "," << object.fit_line_y1 << ") | avgdist=" << object.line_avgdist;
        object.display_summary = summary.str();

        line.status = object.has_fit_line ? "runtime_executed" : "PENDING_BINDING";
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
     * æ³¨æ„ï¼š
     * measure / fitcircle / FitResultMeasure æ˜¯çœŸå®žç®—æ³•è¿è¡Œè¡Œã€‚
     * å½“å‰ debug shim ä¸èƒ½ä¼ªé€ ç®—æ³•ç»“æžœã€‚
     * ä½†å®ƒä¹Ÿä¸èƒ½ BLOCKEDï¼Œå¦åˆ™åŽç»­ fitcircle / setfitmeasuregap / FitResultMeasure æ— æ³•ç»§ç»­è¡Œçº§åˆ†æžã€‚
     *
     * å› æ­¤è¿™é‡Œæ ‡è®°ä¸º runtime_deferred + PENDINGï¼Œ
     * å…è®¸è°ƒè¯•å™¨ç»§ç»­ä¸‹ä¸€è¡Œã€‚
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
    context.cxparser_ext_line_views.clear();
    context.cxparser_ext_statement_views.clear();
    context.cxparser_ext_object_assignments.clear();
    context.cxparser_ext_debug_ok = false;
    context.cxparser_ext_debug_status.clear();
    context.cxparser_ext_debug_reason.clear();
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

static void DebugScriptLineEnd(ManualTestContext& context, int lineIndex, const std::string& status)
{
    if (lineIndex >= 0 && lineIndex < static_cast<int>(context.line_views.size()))
    {
        ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
        std::cout << "[DEBUG SCRIPT LINE] line=" << line.line_no << " end status=" << status << "\n" << std::flush;
    }
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

    try
    {

    const int lineIndex = context.current_line;
    ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
    const std::string statement = TrimLine(line.statement);
    
    std::cout << "[DEBUG SCRIPT LINE] line=" << line.line_no << " begin: " << statement << "\n" << std::flush;

    AppendCxDebugEvent(
        context,
        "step_enter",
        line.line_no,
        statement,
        line.object,
        line.method,
        line.status,
        line.reason,
        "enter debug step");

    if (statement.empty())
    {
        line.status = "skipped_empty";
        line.reason = "empty line";
        context.current_line = FindNextNonEmptyLine(context, lineIndex + 1);
        DebugScriptLineEnd(context, lineIndex, "skipped_empty");
        return;
    }

    if (IsBraceOpenLine(statement) || IsBraceCloseLine(statement))
    {
        line.status = "structural";
        line.reason = "brace skipped by debugger";

        line.timestamp = CurrentTimestamp();

        AppendCxDebugEvent(
            context,
            "structural_brace",
            line.line_no,
            statement,
            "",
            "brace",
            line.status,
            line.reason,
            "brace skipped");

        context.current_line = FindNextNonEmptyLine(context, lineIndex + 1);
        context.run_state = "runtime_step";
        context.debug_status = "PENDING";
        context.debug_reason = line.reason;

        DebugScriptLineEnd(context, lineIndex, "structural");
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

        /*
         * if ä¸º true æ—¶ï¼Œè°ƒè¯•å™¨ä¼šè·³è¿‡ â€œ{â€ ç›´æŽ¥è¿›å…¥ block ç¬¬ä¸€æ¡è¯­å¥ï¼›
         * if ä¸º false æ—¶ï¼Œä¼šè·³è¿‡æ•´ä¸ª blockã€‚
         * å› æ­¤è¿™é‡Œå¿…é¡»ä¸»åŠ¨æŠŠè¢«è·³è¿‡çš„å¤§æ‹¬å·æ ‡æˆ structuralï¼Œ
         * å¦åˆ™ snapshot é‡Œ â€œ{â€ ä¼šæ®‹ç•™ source_analyzedã€‚
         */
        MarkIfBlockBracesStructural(context, lineIndex, !conditionValue);

        context.current_line = conditionValue ?
            FindIfBodyStartLine(context, lineIndex) :
            FindIfAfterBlockLine(context, lineIndex);

        context.run_state = "runtime_step";
        context.debug_status = "PENDING";
        context.debug_reason = line.reason;

        AppendCxDebugEvent(
            context,
            "if_control",
            line.line_no,
            statement,
            "",
            "if",
            line.status,
            line.reason,
            condition);

        return;
    }

    if (TryExecuteCurrentStatusAssignment(context, lineIndex, statement))
    {
        DebugScriptLineEnd(context, lineIndex, "current_status_assignment");
        return;
    }

    if (TryExecuteSimpleAssignment(context, lineIndex, statement))
    {
        DebugScriptLineEnd(context, lineIndex, "simple_assignment");
        return;
    }

    if (TryExecuteDeclaration(context, lineIndex, statement))
        return;

    if (TryExecuteImageCopyFromMat(context, lineIndex, statement))
        return;

    if (TryExecuteFindlineSetline(context, lineIndex, statement))
        return;

    if (TryExecuteFindlineParamMethod(context, lineIndex, statement))
        return;

    if (TryExecuteFindlineRuntimeMethod(context, lineIndex, statement))
        return;

    if (TryExecuteFindcircleSetcircle(context, lineIndex, statement))
        return;

    if (TryExecuteFindcircleParamMethod(context, lineIndex, statement))
        return;

    /*
     * Findcircle çš„ measure / fitcircle / FitResultMeasure
     * å½“å‰å¿…é¡»ä¼˜å…ˆèµ°çœŸå®ž direct runtime bridgeã€‚
     */
    if (TryExecuteFindcircleRuntimeMethod(context, lineIndex, statement))
    {
        DebugScriptLineEnd(context, lineIndex, "runtime_executed");
        return;
    }
    if (TryExecuteIntDeclarationAssignment(context, lineIndex, statement))
    {
        DebugScriptLineEnd(context, lineIndex, "executed");
        return;
    }
    if (TryExecuteGetResultBinding(context, lineIndex, statement))
    {
        DebugScriptLineEnd(context, lineIndex, "get_result_bound");
        return;
    }
    if (TryHandleFindcircleGetResult(context, lineIndex, statement))
    {
        DebugScriptLineEnd(context, lineIndex, "findcircle_get_result");
        return;
    }

    /*
     * å…¶å®ƒæ¨¡å—æš‚æ—¶æ‰èµ° deferredã€‚
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
            // ç†è®ºä¸Šå·²ç»è¢« TryExecutePendingRuntimeMethod å¤„ç†ã€‚
            // è¿™é‡ŒåšäºŒæ¬¡ä¿æŠ¤ã€‚
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
    catch (const std::exception& ex)
    {
        const int lineIndex = context.current_line;
        ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
        line.status = "BLOCKED";
        line.reason = std::string("runtime exception: ") + ex.what();
        line.timestamp = CurrentTimestamp();

        context.run_state = "blocked";
        context.debug_status = "BLOCKED";
        context.debug_reason = line.reason;

        AppendCxDebugEvent(
            context,
            "runtime_exception",
            line.line_no,
            line.statement,
            "",
            "",
            line.status,
            line.reason,
            "");
    }
    catch (...)
    {
        const int lineIndex = context.current_line;
        ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
        line.status = "BLOCKED";
        line.reason = "runtime exception: unknown";
        line.timestamp = CurrentTimestamp();

        context.run_state = "blocked";
        context.debug_status = "BLOCKED";
        context.debug_reason = line.reason;

        AppendCxDebugEvent(
            context,
            "runtime_exception",
            line.line_no,
            line.statement,
            "",
            "",
            line.status,
            line.reason,
            "");
    }
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

static float DistanceSquared(float x1, float y1, float x2, float y2)
{
    const float dx = x2 - x1;
    const float dy = y2 - y1;
    return dx * dx + dy * dy;
}

static GaugeHandleType HitTestGaugeHandleLine(
    const ManualGaugeState& gauge,
    float mouse_x,
    float mouse_y,
    float handle_radius)
{
    if (!gauge.has_line_gauge)
        return GaugeHandleType::None;

    const float r2 = handle_radius * handle_radius;

    if (DistanceSquared((float)gauge.line_x0, (float)gauge.line_y0, mouse_x, mouse_y) <= r2)
        return GaugeHandleType::LineP0;

    if (DistanceSquared((float)gauge.line_x1, (float)gauge.line_y1, mouse_x, mouse_y) <= r2)
        return GaugeHandleType::LineP1;

    const float center_x = ((float)gauge.line_x0 + (float)gauge.line_x1) * 0.5f;
    const float center_y = ((float)gauge.line_y0 + (float)gauge.line_y1) * 0.5f;
    if (DistanceSquared(center_x, center_y, mouse_x, mouse_y) <= r2)
        return GaugeHandleType::LineCenter;

    const float dx = (float)gauge.line_x1 - (float)gauge.line_x0;
    const float dy = (float)gauge.line_y1 - (float)gauge.line_y0;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len > 1.0f)
    {
        const float nx = -dy / len;
        const float ny = dx / len;

        const float width_plus_x = center_x + nx * (float)gauge.tool_half_width;
        const float width_plus_y = center_y + ny * (float)gauge.tool_half_width;
        if (DistanceSquared(width_plus_x, width_plus_y, mouse_x, mouse_y) <= r2)
            return GaugeHandleType::LineWidthPlus;

        const float width_minus_x = center_x - nx * (float)gauge.tool_half_width;
        const float width_minus_y = center_y - ny * (float)gauge.tool_half_width;
        if (DistanceSquared(width_minus_x, width_minus_y, mouse_x, mouse_y) <= r2)
            return GaugeHandleType::LineWidthMinus;
    }

    return GaugeHandleType::None;
}

static GaugeHandleType HitTestGaugeHandleCircle(
    const ManualGaugeState& gauge,
    float mouse_x,
    float mouse_y,
    float handle_radius)
{
    if (!gauge.has_circle_gauge)
        return GaugeHandleType::None;

    const float r2 = handle_radius * handle_radius;

    if (DistanceSquared((float)gauge.circle_cx, (float)gauge.circle_cy, mouse_x, mouse_y) <= r2)
        return GaugeHandleType::CircleCenter;

    int effective_radius = gauge.radius;
    if (effective_radius <= 0)
        effective_radius = gauge.gap > 0 ? gauge.gap : 50;

    const float px = (float)gauge.circle_cx + (float)effective_radius;
    const float py = (float)gauge.circle_cy;
    if (DistanceSquared(px, py, mouse_x, mouse_y) <= r2)
        return GaugeHandleType::CircleRadius;

    if (gauge.inner_radius > 0)
    {
        const float ix = (float)gauge.circle_cx + (float)gauge.inner_radius;
        const float iy = (float)gauge.circle_cy;
        if (DistanceSquared(ix, iy, mouse_x, mouse_y) <= r2)
            return GaugeHandleType::CircleInner;
    }

    if (gauge.outer_radius > 0)
    {
        const float ox = (float)gauge.circle_cx + (float)gauge.outer_radius;
        const float oy = (float)gauge.circle_cy;
        if (DistanceSquared(ox, oy, mouse_x, mouse_y) <= r2)
            return GaugeHandleType::CircleOuter;
    }

    return GaugeHandleType::None;
}

void ViewController::initManualStateTestConsole()
{
  m_manualTest.image_file_path =
    "D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/01.jpg";
  static constexpr const char* kDefaultCxImageCatalogPath =
      "cxparser/cxscript/module/cximage/catalog/cximage_catalog.cxsc";

  m_manualTest.catalog_path = kDefaultCxImageCatalogPath;
  m_manualSnippets.clear();

  m_manualSnippets = {
    {"Parser Run 1", "Image and shape visibility test.",
     "aimage1.Show(1);\nashape0.Show(1);\n", "builtin", true, "", "", false, false, false},
    {"Parser Run 2", "Pattern model setup fragment.",
     "amatch0.setmatchrect(50,50,2200,1900);\n", "builtin", true, "", "", false, false, false},
    {"Parser Run 3", "Image ROI threshold fragment.",
     "aimage1.roieasythre(255);\naimage1.Show(1);\n", "builtin", true, "", "", false, false, false},
    {"Parser Run 4", "Point and line inspection fragment.",
     "apoints0.Show(1);\nafindline.Show(1);\n", "builtin", true, "", "", false, false, false},
    {"Parser Run 5", "Manual runtime call fragment.",
     "arun.testrun();\n", "builtin", true, "", "", false, false, false},
    {"Parser Run 6", "Empty integration observation fragment.",
     "# enter one manual integration statement\n", "builtin", true, "", "", false, false, false},
    {"Custom Manual Text", "Start with an empty manual editor.",
     "", "manual", true, "", "", false, false, false},
    {"CxParserExt Debug Object Assignment Smoke", "A-line debug layer smoke: Class obj = source.method();",
     "", "cxparser_ext/cxscript/debug_smoke/object_assignment_smoke.cxsc", true, "cxparser_ext_debug", "DEBUG_LAYER_SMOKE", false, false, false},
    {"CxParserExt Debug Return Object Smoke", "A-line debug layer smoke: returned object assignment with input ref.",
     "", "cxparser_ext/cxscript/debug_smoke/return_object_trace_smoke.cxsc", true, "cxparser_ext_debug", "DEBUG_LAYER_SMOKE", false, false, false}
  };

  CxScriptCatalogRuntime catalog;
  std::string catalog_reason;
  if (LoadCxScriptCatalogFile(kDefaultCxImageCatalogPath, catalog, catalog_reason))
  {
    m_manualTest.catalog_loaded = true;
    m_manualTest.catalog_entries.clear();
    m_manualTest.catalog_entries.reserve(catalog.scripts.size());
    for (const auto& entry : catalog.scripts)
    {
      m_manualTest.catalog_entries.push_back(entry);
    }

    for (const auto& script : catalog.scripts)
    {
      if (!script.manual_visible)
        continue;

      if (!script.frozen)
        continue;

      if (script.expected_result != "ok" &&
          script.expected_result != "ng_expected")
        continue;

      ScriptSnippet snippet;
      snippet.name = script.label;
      snippet.description = "CxScript Catalog: " + script.script_id;
      snippet.text = "";
      snippet.source_path = script.path;
      snippet.runnable = true;
      snippet.parameter_policy_id = script.parameter_policy_id;
      snippet.parameter_role = script.parameter_role;
      snippet.is_product_default = (script.parameter_role == "PRODUCT_LEGACY_DEFAULT");
      snippet.is_stage25_default = (script.parameter_role == "STAGE25_RECOMMENDED_TEMPLATE");
      snippet.recommended = (script.expected_result == "ok");

      snippet.script_id = script.script_id;
      snippet.expected_result = script.expected_result;
      snippet.expected_result_status = script.expected_status;
      snippet.expected_policy_guard = script.expected_policy_guard;
      snippet.contract_path = script.contract_path;
      snippet.label = script.label;
      snippet.category = (script.expected_result == "ok") ? "Frozen / OK" : "Frozen / NG Expected";
      snippet.failure_hint = "[" + script.expected_result + "] " + script.tool + " / " + script.parameter_policy_id + " / " + script.expected_policy_guard;
      snippet.expects_measure_points = (script.expected_result == "ok");
      snippet.expects_fit_line = (script.expected_result == "ok");
      snippet.expected_filter_failure = (script.expected_result == "ng_expected");

      m_manualSnippets.push_back(snippet);
    }
  }
  else
  {
    m_manualTest.catalog_loaded = false;
    m_manualTest.debug_status = "CATALOG_LOAD_EMPTY";
    m_manualTest.debug_reason = "Failed to load cxscript catalog: " + catalog_reason;
  }

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
        "cxparser/cxscript/module/" + relative, true, "", "", false, false, false});
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

    if (entry.type == "CircleRingGauge" && entry.exists_in_parser && !entry.stale)
    {
        CircleRingGauge* gauge = static_cast<CircleRingGauge*>(m_parserDebugBridge.QueryClassObject("CircleRingGauge", entry.name));
        if (gauge != nullptr)
        {
            entry.has_ring_gauge = true;
            entry.ring_outer_radius = gauge->outer_radius();
            entry.ring_inner_radius = gauge->inner_radius();
            entry.ring_thickness = gauge->thickness();
            entry.ring_center_distance = gauge->center_distance();
            entry.ring_concentric_ok = gauge->concentric_ok() != 0;
            entry.ring_inside_ok = gauge->inside_ok() != 0;
            entry.ring_thickness_ok = gauge->thickness_ok() != 0;
            entry.ring_score = gauge->m_score;
            entry.ring_status = gauge->m_status;
            entry.ring_reason = gauge->m_reason;
            entry.ring_result_ref = gauge->m_result_ref;
        }
    }

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

  SyncRuntimeObjectsToShapeElements();
}


static void DrawCxParserExtLineViewsPanel(const ManualTestContext& context)
{
  if (!ImGui::CollapsingHeader("CxParserExt Line Views##manual_console"))
    return;

  ImGui::Text("status: %s | ok: %s",
              context.cxparser_ext_debug_status.c_str(),
              context.cxparser_ext_debug_ok ? "true" : "false");
  if (!context.cxparser_ext_debug_reason.empty())
    ImGui::TextWrapped("reason: %s",
                       context.cxparser_ext_debug_reason.c_str());

  for (const CxScriptLineView& line : context.cxparser_ext_line_views)
  {
    ImGui::Text("%d | %s | %s",
                line.line_no,
                line.statement_type.c_str(),
                line.status.c_str());
    ImGui::TextWrapped("%s", line.source_line.c_str());
    if (!line.reason.empty())
      ImGui::TextWrapped("reason: %s", line.reason.c_str());
    ImGui::Separator();
  }
}

static void DrawCxParserExtStatementViewsPanel(const ManualTestContext& context)
{
  if (!ImGui::CollapsingHeader("CxParserExt Statement Views##manual_console"))
    return;

  for (const CxScriptStatementView& stmt : context.cxparser_ext_statement_views)
  {
    ImGui::Text("#%d line=%d type=%s",
                stmt.statement_id,
                stmt.line_no,
                stmt.statement_type.c_str());
    if (!stmt.lhs_variable.empty())
      ImGui::Text("lhs: %s : %s",
                  stmt.lhs_variable.c_str(),
                  stmt.lhs_type.c_str());
    if (!stmt.source_object.empty())
      ImGui::Text("call: %s.%s()",
                  stmt.source_object.c_str(),
                  stmt.method_name.c_str());
    if (!stmt.returned_object_ref.empty())
      ImGui::Text("return ref: %s",
                  stmt.returned_object_ref.c_str());
    ImGui::Text("status: %s", stmt.status.c_str());
    if (!stmt.reason.empty())
      ImGui::TextWrapped("reason: %s", stmt.reason.c_str());
    ImGui::Separator();
  }
}

static void DrawCxParserExtObjectAssignmentsPanel(const ManualTestContext& context)
{
  if (!ImGui::CollapsingHeader("CxParserExt Object Assignments##manual_console"))
    return;

  for (const CxScriptObjectAssignmentView& item :
       context.cxparser_ext_object_assignments)
  {
    ImGui::Text("%s : %s",
                item.lhs_variable.c_str(),
                item.lhs_type.c_str());
    ImGui::Text("from: %s.%s()",
                item.source_object.c_str(),
                item.method_name.c_str());
    ImGui::Text("ref: %s", item.returned_object_ref.c_str());
    ImGui::Text("line %d: %s",
                item.line_no,
                item.source_line.c_str());
    ImGui::Text("status: %s", item.status.c_str());
    if (!item.reason.empty())
      ImGui::TextWrapped("reason: %s", item.reason.c_str());
    ImGui::Separator();
  }
}

static const char* UiTextOrDash(const std::string& value)
{
  return value.empty() ? "(none)" : value.c_str();
}

static std::string InferCurrentTemplateTool(const ManualTestContext& context)
{
  if (!context.current_gauge.tool.empty())
    return context.current_gauge.tool;
  for (const ScriptObjectView& object : context.object_views)
  {
    if (object.type == "Findcircle" || object.type == "Findline" ||
        object.type == "fastmatch" || object.type == "CircleRingGauge")
      return object.type;
  }
  return "pending";
}

static std::string InferCurrentTemplatePath(const ManualTestContext& context)
{
  if (!context.loaded_script_path.empty())
    return context.loaded_script_path;
  if (!context.bound_state_script_path.empty())
    return context.bound_state_script_path;
  if (!context.script_file_path.empty())
    return context.script_file_path;
  return "(none)";
}

static int CountSelectedParamCandidates(const ManualParamRegressionState& state)
{
  int count = 0;
  for (const auto& c : state.candidates)
    if (c.selected_for_probe) ++count;
  return count;
}

static bool IsFindlineFindcircleContext(const ManualTestContext& context)
{
  const ManualGaugeState& gauge = context.current_gauge;
  if (gauge.tool == "Findline" || gauge.tool == "Findcircle")
    return true;
  if (gauge.has_line_gauge || gauge.has_circle_gauge)
    return true;
  for (const ScriptObjectView& object : context.object_views)
  {
    if (object.type == "Findline" || object.type == "Findcircle")
      return true;
  }
  for (const RuntimeObjectView& object : context.runtime_objects)
  {
    if (object.type == "Findline" || object.type == "Findcircle")
      return true;
  }
  return false;
}

static void DrawKeyParameterUnavailableNotice(const ManualTestContext& context)
{
  if (!ImGui::CollapsingHeader("关键参数 UI / 参数整定图", ImGuiTreeNodeFlags_DefaultOpen))
    return;
  ImGui::TextColored(ImVec4(1.0f, 0.68f, 0.25f, 1.0f),
                     "Current image/tool is not suitable for Findline/Findcircle key-parameter tuning.");
  ImGui::TextWrapped(
    "Select or create a Line/Circle annotation tool, or load a script containing Findline/Findcircle. "
    "After selecting a Line/Circle element, the center/boundary handles sync ManualGaugeState and the key-parameter UI will appear.");
  ImGui::Text("current gauge tool=%s line_gauge=%s circle_gauge=%s",
              context.current_gauge.tool.c_str(),
              context.current_gauge.has_line_gauge ? "yes" : "no",
              context.current_gauge.has_circle_gauge ? "yes" : "no");
}

static void DrawCxScriptWorkbenchOverview(ManualTestContext& context)
{
  if (!ImGui::CollapsingHeader("CxScript Workbench / 人工验收总览", ImGuiTreeNodeFlags_DefaultOpen))
    return;

  const ManualGaugeState& gauge = context.current_gauge;
  const ManualParamRegressionState& reg = context.param_regression;
  const bool gaugeAccepted = ManualGaugeAcceptedForParamRegression(gauge);
  const bool keyParamSuitable = IsFindlineFindcircleContext(context);

  ImGui::TextWrapped(
    "This overview mirrors the design map: Evidence/Annotation -> cxparser script template -> Key Parameters -> Param Regression -> Conclusion/Evidence.");
  ImGui::Separator();

  if (ImGui::BeginTable("cxscript_workbench_map", 3,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
  {
    ImGui::TableSetupColumn("证据链 / 标注工具集");
    ImGui::TableSetupColumn("cxparser script 基础模板");
    ImGui::TableSetupColumn("关键参数 / 结论");
    ImGui::TableHeadersRow();

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text("Image");
    ImGui::BulletText("image_file: %s", UiTextOrDash(context.image_file_path));
    ImGui::BulletText("global.matInput: %s", context.global_variable_views.empty() ? "pending" : context.global_variable_views.front().status.c_str());
    ImGui::BulletText("annotation elements: %d", context.manual_elements_count);
    ImGui::BulletText("source preview: %s", context.source_preview_enabled ? "on" : "off");
    ImGui::BulletText("gauge annotation: %s",
                      (ManualGaugeCaseDir(context) / "gauge_annotation.json").string().c_str());

    ImGui::TableSetColumnIndex(1);
    ImGui::Text("Template");
    ImGui::BulletText("script: %s", InferCurrentTemplatePath(context).c_str());
    ImGui::BulletText("tool: %s", InferCurrentTemplateTool(context).c_str());
    ImGui::BulletText("editor source: %s", context.editor_source.c_str());
    ImGui::BulletText("editor dirty: %s", context.editor_dirty ? "yes" : "no");
    ImGui::BulletText("catalog loaded: %s (%d entries)",
                      context.catalog_loaded ? "yes" : "no",
                      static_cast<int>(context.catalog_entries.size()));
    ImGui::BulletText("semantic lines: %d", static_cast<int>(context.line_views.size()));

    ImGui::TableSetColumnIndex(2);
    ImGui::Text("Status");
    ImGui::BulletText("gauge: %s", gaugeAccepted ? "manual_accepted" : gauge.review_status.c_str());
    ImGui::BulletText("param regression: %s", reg.status.c_str());
    ImGui::BulletText("key parameter UI: %s", keyParamSuitable ? "visible" : "waiting for Findline/Findcircle");
    ImGui::BulletText("candidates: %d selected: %d",
                      static_cast<int>(reg.candidates.size()),
                      CountSelectedParamCandidates(reg));
    ImGui::BulletText("result: %s", context.current_result_ref.status.c_str());
    ImGui::BulletText("debug: %s | %s",
                      context.debug_status.c_str(),
                      context.debug_reason.c_str());

    ImGui::EndTable();
  }

  ImGui::Separator();
  if (ImGui::BeginTable("cxscript_design_flow", 6,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
  {
    ImGui::TableSetupColumn("1 Evidence");
    ImGui::TableSetupColumn("2 Template");
    ImGui::TableSetupColumn("3 Gauge");
    ImGui::TableSetupColumn("4 Params");
    ImGui::TableSetupColumn("5 Conclusion");
    ImGui::TableSetupColumn("6 Export");
    ImGui::TableHeadersRow();
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextWrapped("%s", context.image_file_path.empty() ? "image pending" : "image selected");
    ImGui::TableSetColumnIndex(1);
    ImGui::TextWrapped("%s", context.editor_text.empty() ? "script pending" : "script loaded");
    ImGui::TableSetColumnIndex(2);
    ImGui::TextWrapped("%s", gaugeAccepted ? "accepted" : "needs review");
    ImGui::TableSetColumnIndex(3);
    ImGui::TextWrapped("%s", reg.initialized ? "candidate table ready" : "initialize after gauge");
    ImGui::TableSetColumnIndex(4);
    ImGui::TextWrapped("%s", context.current_result_ref.status.empty() ? "no result" : context.current_result_ref.status.c_str());
    ImGui::TableSetColumnIndex(5);
    ImGui::TextWrapped("%d files indexed", static_cast<int>(reg.exported_files.size()));
    ImGui::EndTable();
  }
}

static void DrawEvidenceCaseListPanel(ManualTestContext& context)
{
  if (!ImGui::CollapsingHeader("Evidence Case List", ImGuiTreeNodeFlags_DefaultOpen))
    return;

  ImGui::TextWrapped("Evidence cases organized by case/image/target/tool/gauge_status/probe_status/contract_status/review_status");

  if (context.evidence_items.empty())
  {
    ImGui::TextDisabled("No evidence cases loaded.");
    ImGui::Text("Loading from catalog entries...");

    for (const auto& entry : context.catalog_entries)
    {
      bool isVisible = entry.manual_visible && entry.frozen &&
          (entry.expected_result == "ok" || entry.expected_result == "ng_expected");
      if (!isVisible) continue;

      ManualEvidenceItem item;
      item.case_id = entry.script_id;
      item.tool = entry.tool;
      item.script_id = entry.script_id;
      item.gauge_status = "unannotated";
      item.probe_status = "pending";
      item.contract_status = "pending";
      item.review_status = "unreviewed";
      context.evidence_items.push_back(item);
    }
  }

  if (!context.evidence_items.empty())
  {
    ImGui::BeginChild("evidence_case_list", ImVec2(-1, 200), true);

    if (ImGui::BeginTable("evidence_case_table", 10,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
    {
      ImGui::TableSetupColumn("Case", ImGuiTableColumnFlags_WidthFixed, 120.0f);
      ImGui::TableSetupColumn("Tool", ImGuiTableColumnFlags_WidthFixed, 80.0f);
      ImGui::TableSetupColumn("Image", ImGuiTableColumnFlags_WidthFixed, 100.0f);
      ImGui::TableSetupColumn("Target", ImGuiTableColumnFlags_WidthFixed, 100.0f);
      ImGui::TableSetupColumn("Gauge", ImGuiTableColumnFlags_WidthFixed, 90.0f);
      ImGui::TableSetupColumn("Probe", ImGuiTableColumnFlags_WidthFixed, 80.0f);
      ImGui::TableSetupColumn("Contract", ImGuiTableColumnFlags_WidthFixed, 80.0f);
      ImGui::TableSetupColumn("Review", ImGuiTableColumnFlags_WidthFixed, 90.0f);
      ImGui::TableSetupColumn("Script", ImGuiTableColumnFlags_WidthFixed, 100.0f);
      ImGui::TableSetupColumn("Param", ImGuiTableColumnFlags_WidthFixed, 100.0f);
      ImGui::TableHeadersRow();

      for (const auto& item : context.evidence_items)
      {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(item.case_id.c_str());
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(item.tool.c_str());
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted(item.image_id.empty() ? "-" : item.image_id.c_str());
        ImGui::TableSetColumnIndex(3);
        ImGui::TextUnformatted(item.target_id.empty() ? "-" : item.target_id.c_str());
        ImGui::TableSetColumnIndex(4);
        ImGui::TextUnformatted(item.gauge_status.c_str());
        ImGui::TableSetColumnIndex(5);
        ImGui::TextUnformatted(item.probe_status.c_str());
        ImGui::TableSetColumnIndex(6);
        ImGui::TextUnformatted(item.contract_status.c_str());
        ImGui::TableSetColumnIndex(7);
        ImGui::TextUnformatted(item.review_status.c_str());
        ImGui::TableSetColumnIndex(8);
        ImGui::TextUnformatted(item.script_id.empty() ? "-" : item.script_id.c_str());
        ImGui::TableSetColumnIndex(9);
        ImGui::TextUnformatted(item.parameter_profile_id.empty() ? "-" : item.parameter_profile_id.c_str());
      }

      ImGui::EndTable();
    }

    ImGui::EndChild();
  }
}

static void DrawCxScriptTemplateSummaryPanel(const ManualTestContext& context)
{
  if (!ImGui::CollapsingHeader("cxparser script 基础模板 / 当前模板摘要", ImGuiTreeNodeFlags_DefaultOpen))
    return;

  ImGui::Text("Current script: %s", InferCurrentTemplatePath(context).c_str());
  ImGui::Text("Tool: %s | Source: %s | Dirty: %s",
              InferCurrentTemplateTool(context).c_str(),
              context.editor_source.c_str(),
              context.editor_dirty ? "yes" : "no");
  ImGui::Text("Active case: %s | purpose: %s",
              UiTextOrDash(context.active_script_case_name),
              UiTextOrDash(context.active_script_case_purpose));
  ImGui::Separator();

  if (ImGui::BeginTable("template_object_method_summary", 4,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable,
                        ImVec2(-1, 120)))
  {
    ImGui::TableSetupColumn("Object");
    ImGui::TableSetupColumn("Type");
    ImGui::TableSetupColumn("Declared Line");
    ImGui::TableSetupColumn("Runtime State");
    ImGui::TableHeadersRow();
    for (const auto& object : context.object_views)
    {
      std::string runtime_state = "not_created";
      for (const auto& runtime : context.runtime_objects)
      {
        if (runtime.name == object.name)
        {
          runtime_state = runtime.exists_in_parser ? runtime.runtime_state : "stale";
          break;
        }
      }
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(object.name.c_str());
      ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(object.type.c_str());
      ImGui::TableSetColumnIndex(2); ImGui::Text("%d", object.declared_line);
      ImGui::TableSetColumnIndex(3); ImGui::TextUnformatted(runtime_state.c_str());
    }
    ImGui::EndTable();
  }
}

static void DrawKeyParameterSummaryPanel(const ManualTestContext& context)
{
  if (!ImGui::CollapsingHeader("关键参数表 / Key Parameter Summary", ImGuiTreeNodeFlags_DefaultOpen))
    return;

  const ManualGaugeState& g = context.current_gauge;
  if (ImGui::BeginTable("key_parameter_summary", 4,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
  {
    ImGui::TableSetupColumn("Group");
    ImGui::TableSetupColumn("Parameter");
    ImGui::TableSetupColumn("Value");
    ImGui::TableSetupColumn("Source");
    ImGui::TableHeadersRow();
    auto row = [&](const char* group, const char* name, int value, const char* source) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(group);
      ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(name);
      ImGui::TableSetColumnIndex(2); ImGui::Text("%d", value);
      ImGui::TableSetColumnIndex(3); ImGui::TextUnformatted(source);
    };

    row("line", "x0", g.line_x0, g.source.c_str());
    row("line", "y0", g.line_y0, g.source.c_str());
    row("line", "x1", g.line_x1, g.source.c_str());
    row("line", "y1", g.line_y1, g.source.c_str());
    row("line", "tool_half_width", g.tool_half_width, g.source.c_str());
    row("line", "wgap", g.wgap, g.source.c_str());
    row("line", "hgap", g.hgap, g.source.c_str());
    row("common", "linegap", g.linegap, g.source.c_str());
    row("common", "threshold", g.threshold, g.source.c_str());
    row("common", "method", g.method, g.source.c_str());
    row("common", "filterprofile", g.filterprofile, g.source.c_str());
    row("circle", "cx", g.circle_cx, g.source.c_str());
    row("circle", "cy", g.circle_cy, g.source.c_str());
    row("circle", "px", g.circle_px, g.source.c_str());
    row("circle", "py", g.circle_py, g.source.c_str());
    row("circle", "gap", g.gap, g.source.c_str());
    ImGui::EndTable();
  }
}

static void DrawConclusionSummaryPanel(const ManualTestContext& context)
{
  if (!ImGui::CollapsingHeader("结论 UI / Result Conclusion", ImGuiTreeNodeFlags_DefaultOpen))
    return;

  const ResultRefView& r = context.current_result_ref;
  ImGui::Text("result_ref: %s = %s", UiTextOrDash(r.name), UiTextOrDash(r.value));
  ImGui::Text("status: %s | reason: %s", UiTextOrDash(r.status), UiTextOrDash(r.reason));

  if (ImGui::BeginTable("conclusion_summary_table", 4,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
  {
    ImGui::TableSetupColumn("Metric");
    ImGui::TableSetupColumn("Findline");
    ImGui::TableSetupColumn("Findcircle");
    ImGui::TableSetupColumn("Evidence");
    ImGui::TableHeadersRow();
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("points");
    ImGui::TableSetColumnIndex(1); ImGui::Text("%d", r.valid_line_points_count > 0 ? r.valid_line_points_count : r.line_points_count);
    ImGui::TableSetColumnIndex(2); ImGui::Text("%d", r.valid_points_count > 0 ? r.valid_points_count : r.points_count);
    ImGui::TableSetColumnIndex(3); ImGui::TextUnformatted(context.image_overlay_summary.empty() ? "pending" : context.image_overlay_summary.c_str());
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("fit");
    ImGui::TableSetColumnIndex(1); ImGui::Text("(%.1f,%.1f)-(%.1f,%.1f)", r.line_x0, r.line_y0, r.line_x1, r.line_y1);
    ImGui::TableSetColumnIndex(2); ImGui::Text("(%.1f,%.1f) r=%.1f", r.fit_cx, r.fit_cy, r.fit_radius);
    ImGui::TableSetColumnIndex(3); ImGui::Text("avgdist line=%.2f circle=%.2f", r.line_avgdist, r.avgdist);
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("failure hint");
    ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(r.line_measure_failure_hint.empty() ? r.line_result_reason.c_str() : r.line_measure_failure_hint.c_str());
    ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(r.reason.c_str());
    ImGui::TableSetColumnIndex(3); ImGui::TextUnformatted(context.debug_reason.c_str());
    ImGui::EndTable();
  }
}

static void SyncKeyParameterUiToGauge(ManualTestContext& context)
{
  ManualGaugeState& g = context.current_gauge;
  ManualParamRegressionState& ui = context.param_regression;
  g.threshold = ui.contrast_percent;
  g.linegap = ui.measure_order;
  g.filterprofile = ui.enable_filter ? 1 : 0;
  if (g.has_line_gauge || g.tool != "Findcircle")
  {
    g.wgap = std::max(1, ui.sample_points);
    g.hgap = std::max(1, ui.valid_length_percent);
  }
  if (g.has_circle_gauge || g.tool == "Findcircle")
  {
    g.gap = std::max(1, ui.sample_points);
  }
  g.method = ui.edge_mode;
  g.dirty = true;
}

static void ResetKeyParameterUiDefaults(ManualTestContext& context)
{
  ManualParamRegressionState& ui = context.param_regression;
  ui.edge_mode = 0;
  ui.contrast_percent = 20;
  ui.valid_length_percent = 50;
  ui.interference_length_percent = 20;
  ui.roughness = 8;
  ui.burr_filter_percent = 0;
  ui.measure_order = 3;
  ui.black_index = 50;
  ui.sample_points = 8;
  ui.catch_method = 0;
  ui.enable_fast_measure = true;
  ui.enable_filter = true;
  SyncKeyParameterUiToGauge(context);
}

static void DrawKeyParameterControlPanel(ManualTestContext& context)
{
  ManualGaugeState& gauge = context.current_gauge;

  ImGui::TextUnformatted("Tool: ");
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "%s", gauge.tool.c_str());
  ImGui::Separator();

  ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
  if (ImGui::CollapsingHeader("Geometry"))
  {
      ImGui::PushID("geometry");
      if (gauge.tool == "Findcircle" || gauge.has_circle_gauge)
      {
          ImGui::SetNextItemWidth(120.0f); ImGui::InputInt("cx", &gauge.circle_cx);
          ImGui::SameLine(); ImGui::SetNextItemWidth(120.0f); ImGui::InputInt("cy", &gauge.circle_cy);
          ImGui::SameLine(); ImGui::SetNextItemWidth(120.0f); ImGui::InputInt("radius", &gauge.radius);

          ImGui::SetNextItemWidth(120.0f); ImGui::InputInt("inner_radius", &gauge.inner_radius);
          ImGui::SameLine(); ImGui::SetNextItemWidth(120.0f); ImGui::InputInt("outer_radius", &gauge.outer_radius);
          ImGui::SameLine(); ImGui::SetNextItemWidth(120.0f); ImGui::InputInt("gap", &gauge.gap);
      }
      else
      {
          ImGui::SetNextItemWidth(120.0f); ImGui::InputInt("x0", &gauge.line_x0);
          ImGui::SameLine(); ImGui::SetNextItemWidth(120.0f); ImGui::InputInt("y0", &gauge.line_y0);
          ImGui::SameLine(); ImGui::SetNextItemWidth(120.0f); ImGui::InputInt("half_width", &gauge.tool_half_width);

          ImGui::SetNextItemWidth(120.0f); ImGui::InputInt("x1", &gauge.line_x1);
          ImGui::SameLine(); ImGui::SetNextItemWidth(120.0f); ImGui::InputInt("y1", &gauge.line_y1);
      }
      ImGui::PopID();
  }

  ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
  if (ImGui::CollapsingHeader("Edge Params"))
  {
      ImGui::PushID("edge_params");

      ImGui::TextUnformatted("threshold");
      ImGui::SameLine(80.0f);
      ImGui::SetNextItemWidth(180.0f); ImGui::SliderInt("##threshold", &gauge.threshold, 0, 255);
      ImGui::SameLine(); ImGui::SetNextItemWidth(70.0f); ImGui::InputInt("##t_val", &gauge.threshold);
      gauge.threshold = std::max(0, std::min(255, gauge.threshold));

      ImGui::TextUnformatted("method");
      ImGui::SameLine(80.0f);
      const char* methods[] = {"0", "1", "2", "3"};
      ImGui::SetNextItemWidth(100.0f); ImGui::Combo("##method", &gauge.method, methods, IM_ARRAYSIZE(methods));

      ImGui::TextUnformatted("linegap");
      ImGui::SameLine(80.0f);
      ImGui::SetNextItemWidth(180.0f); ImGui::SliderInt("##linegap", &gauge.linegap, 0, 50);
      ImGui::SameLine(); ImGui::SetNextItemWidth(70.0f); ImGui::InputInt("##lg_val", &gauge.linegap);
      gauge.linegap = std::max(0, std::min(50, gauge.linegap));

      if (gauge.tool == "Findcircle" || gauge.has_circle_gauge)
      {
          ImGui::TextUnformatted("gap");
          ImGui::SameLine(80.0f);
          ImGui::SetNextItemWidth(180.0f); ImGui::SliderInt("##gap", &gauge.gap, 0, 200);
          ImGui::SameLine(); ImGui::SetNextItemWidth(70.0f); ImGui::InputInt("##gap_val", &gauge.gap);
          gauge.gap = std::max(0, std::min(200, gauge.gap));
      }
      else
      {
          ImGui::TextUnformatted("wgap");
          ImGui::SameLine(80.0f);
          ImGui::SetNextItemWidth(180.0f); ImGui::SliderInt("##wgap", &gauge.wgap, 0, 50);
          ImGui::SameLine(); ImGui::SetNextItemWidth(70.0f); ImGui::InputInt("##wg_val", &gauge.wgap);
          gauge.wgap = std::max(0, std::min(50, gauge.wgap));

          ImGui::TextUnformatted("hgap");
          ImGui::SameLine(80.0f);
          ImGui::SetNextItemWidth(180.0f); ImGui::SliderInt("##hgap", &gauge.hgap, 0, 50);
          ImGui::SameLine(); ImGui::SetNextItemWidth(70.0f); ImGui::InputInt("##hg_val", &gauge.hgap);
          gauge.hgap = std::max(0, std::min(50, gauge.hgap));

          ImGui::TextUnformatted("filterprofile");
          ImGui::SameLine(80.0f);
          ImGui::SetNextItemWidth(180.0f); ImGui::SliderInt("##fp", &gauge.filterprofile, 0, 10);
          ImGui::SameLine(); ImGui::SetNextItemWidth(70.0f); ImGui::InputInt("##fp_val", &gauge.filterprofile);
          gauge.filterprofile = std::max(0, std::min(10, gauge.filterprofile));
      }
      ImGui::PopID();
  }

  ImGui::Separator();
  ImGui::TextUnformatted("Actions");

  const float btnWidth = (ImGui::GetContentRegionAvail().x - 30.0f) / 3.0f;

  ImGui::PushID("actions");
  if (ImGui::Button("Apply To Gauge", ImVec2(btnWidth, 0)))
  {
    SyncKeyParameterUiToGauge(context);
    gauge.dirty = true;
    gauge.review_status = "editing";
  }
  ImGui::SameLine();
  if (ImGui::Button("Apply To Globals", ImVec2(btnWidth, 0)))
  {
    ApplyManualGaugeToGlobals(context);
  }
  ImGui::SameLine();
  if (ImGui::Button("Run Script", ImVec2(btnWidth, 0)))
  {
    context.run_state = "running";
  }

  if (ImGui::Button("Save Candidate", ImVec2(btnWidth, 0)))
  {
    SyncKeyParameterUiToGauge(context);
  }
  ImGui::SameLine();
  if (ImGui::Button("Reset", ImVec2(btnWidth, 0)))
  {
    ResetKeyParameterUiDefaults(context);
    SyncKeyParameterUiToGauge(context);
  }
  ImGui::PopID();
}

static void DrawParamTuningScatterPanel(ManualTestContext& context)
{
  if (!ImGui::CollapsingHeader("参数整定图 / Parameter Tuning Map", ImGuiTreeNodeFlags_DefaultOpen))
    return;

  ManualParamRegressionState& reg = context.param_regression;
  const char* tabs[] = {"Tuning", "Reading", "Test", "LastTest", "Search"};
  for (int i = 0; i < IM_ARRAYSIZE(tabs); ++i)
  {
    if (i > 0) ImGui::SameLine();
    if (ImGui::Selectable(tabs[i], reg.tuning_tab == i, 0, ImVec2(74, 0)))
      reg.tuning_tab = i;
  }

  ImGui::TextWrapped(
    "Scatter view uses current parameter candidates. X=threshold, Y=predicted quality/risk score. Selected candidates are highlighted.");

  const ImVec2 plotSize(520.0f, 260.0f);
  ImVec2 p0 = ImGui::GetCursorScreenPos();
  ImVec2 p1(p0.x + plotSize.x, p0.y + plotSize.y);
  ImDrawList* draw = ImGui::GetWindowDrawList();
  draw->AddRectFilled(p0, p1, IM_COL32(18, 18, 18, 255));
  draw->AddRect(p0, p1, IM_COL32(230, 230, 230, 255));

  for (int gx = 0; gx <= 10; ++gx)
  {
    const float x = p0.x + plotSize.x * gx / 10.0f;
    draw->AddLine(ImVec2(x, p0.y), ImVec2(x, p1.y), IM_COL32(80, 80, 80, 180));
  }
  for (int gy = 0; gy <= 10; ++gy)
  {
    const float y = p0.y + plotSize.y * gy / 10.0f;
    draw->AddLine(ImVec2(p0.x, y), ImVec2(p1.x, y), IM_COL32(80, 80, 80, 180));
  }

  auto mapX = [&](int threshold) {
    const float t = static_cast<float>(std::max(0, std::min(100, threshold))) / 100.0f;
    return p0.x + 36.0f + t * (plotSize.x - 54.0f);
  };
  auto mapY = [&](double quality, double risk) {
    double score = quality > 0.0 ? quality : (1.0 - risk);
    score = std::max(0.0, std::min(1.0, score));
    return p1.y - 28.0f - static_cast<float>(score) * (plotSize.y - 52.0f);
  };

  const ImU32 colors[] = {
    IM_COL32(255, 90, 90, 255),
    IM_COL32(80, 210, 255, 255),
    IM_COL32(120, 255, 120, 255),
    IM_COL32(255, 230, 80, 255),
    IM_COL32(255, 150, 255, 255)
  };

  for (std::size_t i = 0; i < reg.candidates.size(); ++i)
  {
    const CxParamCandidate& c = reg.candidates[i];
    const float x = mapX(c.threshold);
    const float y = mapY(c.predicted_quality, c.predicted_risk);
    const float radius = c.selected_for_probe ? 5.5f : 3.5f;
    draw->AddCircleFilled(ImVec2(x, y), radius, colors[i % IM_ARRAYSIZE(colors)]);
    if (static_cast<int>(i) == reg.selected_candidate_index)
      draw->AddCircle(ImVec2(x, y), radius + 4.0f, IM_COL32(255, 255, 255, 255), 16, 2.0f);
  }

  draw->AddText(ImVec2(p0.x + 8.0f, p0.y + 6.0f), IM_COL32(240, 240, 240, 255), "Quality / Risk");
  draw->AddText(ImVec2(p1.x - 110.0f, p1.y - 22.0f), IM_COL32(240, 240, 240, 255), "Threshold");
  ImGui::Dummy(plotSize);

  ImGui::SameLine();
  ImGui::BeginGroup();
  ImGui::TextUnformatted("After Parameter");
  ImGui::SetNextItemWidth(90.0f);
  ImGui::SliderInt("Edge", &reg.edge_mode, 0, 2);
  ImGui::SetNextItemWidth(90.0f);
  ImGui::SliderInt("Scale", &reg.contrast_percent, 0, 100);
  ImGui::SetNextItemWidth(90.0f);
  ImGui::SliderInt("X", &reg.valid_length_percent, 0, 100);
  ImGui::SetNextItemWidth(90.0f);
  ImGui::SliderInt("Y", &reg.interference_length_percent, 0, 100);
  ImGui::RadioButton("Visual", reg.tuning_tab == 0);
  ImGui::RadioButton("Param", reg.tuning_tab == 1);
  ImGui::RadioButton("Grid", reg.tuning_tab == 2);
  if (ImGui::Button("Reset Transform"))
    ResetKeyParameterUiDefaults(context);
  if (ImGui::Button("Animate"))
  {
    context.debug_action = "Tuning Map Animate";
    context.debug_status = "PENDING";
    context.debug_reason = "visual placeholder; no runtime execution";
  }
  if (ImGui::Button("Show Source"))
    context.source_preview_enabled = true;
  if (ImGui::Button("What?"))
  {
    context.debug_action = "Tuning Map Help";
    context.debug_status = "PENDING";
    context.debug_reason = "Tuning map plots candidates. White ring means focused candidate.";
  }
  ImGui::EndGroup();

  ImGui::Text("Candidates=%d Selected=%d Focus=%d",
              static_cast<int>(reg.candidates.size()),
              CountSelectedParamCandidates(reg),
              reg.selected_candidate_index);
}

void ViewController::EnsureCxScriptWorkbenchAssetsLoaded()
{
    if (m_manualTest.workbench_assets_loaded)
        return;

    static constexpr const char* kDefaultImageManifestPath =
        "D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/test_images/stage25_image_manifest.json";

    m_manualTest.manifest_path = kDefaultImageManifestPath;

    CxScriptCatalogRuntime catalog;
    std::string catalog_reason;
    if (LoadCxScriptCatalogFile(m_manualTest.catalog_path, catalog, catalog_reason))
    {
        m_manualTest.catalog_loaded = true;
        m_manualTest.catalog_entries.clear();
        for (const auto& entry : catalog.scripts)
        {
            m_manualTest.catalog_entries.push_back(entry);
        }
    }

    std::ifstream manifestFile(kDefaultImageManifestPath);
    if (manifestFile.is_open())
    {
        std::string jsonContent((std::istreambuf_iterator<char>(manifestFile)),
                                std::istreambuf_iterator<char>());
        manifestFile.close();

        m_manualTest.image_manifest_entries.clear();
        m_manualTest.image_manifest_items.clear();

        size_t pos = 0;
        while ((pos = jsonContent.find("{", pos)) != std::string::npos)
        {
            size_t objEnd = jsonContent.find("}", pos);
            if (objEnd == std::string::npos) break;

            std::string objStr = jsonContent.substr(pos, objEnd - pos + 1);

            ManifestImageItem item;

            size_t idPos = objStr.find("image_id");
            if (idPos != std::string::npos)
            {
                idPos += 9;
                size_t colon = objStr.find(":", idPos);
                if (colon != std::string::npos)
                {
                    colon++;
                    size_t quote = objStr.find("\"", colon);
                    if (quote != std::string::npos)
                    {
                        quote++;
                        size_t endQuote = objStr.find("\"", quote);
                        if (endQuote != std::string::npos)
                            item.image_id = objStr.substr(quote, endQuote - quote);
                    }
                }
            }

            size_t pathPos = objStr.find("image_path");
            if (pathPos != std::string::npos)
            {
                pathPos += 11;
                size_t colon = objStr.find(":", pathPos);
                if (colon != std::string::npos)
                {
                    colon++;
                    size_t quote = objStr.find("\"", colon);
                    if (quote != std::string::npos)
                    {
                        quote++;
                        size_t endQuote = objStr.find("\"", quote);
                        if (endQuote != std::string::npos)
                            item.image_path = objStr.substr(quote, endQuote - quote);
                    }
                }
            }

            if (!item.image_id.empty())
            {
                m_manualTest.image_manifest_entries.push_back(item.image_id);
                m_manualTest.image_manifest_items.push_back(item);
            }

            pos = objEnd + 1;
        }

        m_manualTest.manifest_loaded = true;
        m_manualTest.manifest_load_reason = "loaded";
    }
    else
    {
        m_manualTest.manifest_loaded = false;
        m_manualTest.manifest_load_reason = "failed to open " + std::string(kDefaultImageManifestPath);
    }

    m_manualTest.evidence_items.clear();
    for (const auto& entry : m_manualTest.catalog_entries)
    {
        if (!entry.manual_visible || !entry.frozen)
            continue;

        ManualEvidenceItem item;
        item.case_id = entry.script_id;
        item.tool = entry.tool;
        item.image_id = "";
        item.target_id = "";
        item.level = "";
        item.script_id = entry.script_id;
        item.gauge_status = "pending";
        item.probe_status = "pending";
        item.contract_status = "pending";
        item.review_status = "pending";
        m_manualTest.evidence_items.push_back(item);
    }

    m_manualTest.workbench_assets_loaded = true;
}

void ViewController::EnsureEvidenceChainThumbnailsLoaded()
{
    if (m_manualTest.evidence_chain_thumbs.empty())
    {
        for (const auto& entry : m_manualTest.catalog_entries)
        {
            if (!entry.manual_visible || !entry.frozen)
                continue;

            EvidenceChainThumb thumb;
            thumb.case_id = entry.script_id;
            thumb.script_id = entry.script_id;
            thumb.script_path = entry.path;
            thumb.image_id = "";
            thumb.image_path = "";
            thumb.target_id = "";
            thumb.tool = entry.tool;

            thumb.parameter_summary = "threshold=20 method=2 linegap=6";
            thumb.status = "pending";
            thumb.reason = "";

            thumb.texture_id = 0;
            thumb.texture_w = 0;
            thumb.texture_h = 0;
            thumb.texture_loaded = false;

            m_manualTest.evidence_chain_thumbs.push_back(thumb);
        }
    }

    for (auto& thumb : m_manualTest.evidence_chain_thumbs)
    {
        if (thumb.texture_loaded || thumb.image_path.empty())
            continue;

        cv::Mat image = cv::imread(thumb.image_path);
        if (image.empty())
        {
            thumb.texture_loaded = false;
            continue;
        }

        cv::Mat small;
        cv::resize(image, small, cv::Size(72, 72));

        thumb.texture_id = CreateTextureFromMat0(small);
        thumb.texture_w = small.cols;
        thumb.texture_h = small.rows;
        thumb.texture_loaded = true;
    }
}

void ViewController::SelectEvidenceChainThumb(int index)
{
    if (index < 0 || index >= static_cast<int>(m_manualTest.evidence_chain_thumbs.size()))
        return;

    m_manualTest.selected_evidence_thumb = index;

    const EvidenceChainThumb& thumb = m_manualTest.evidence_chain_thumbs[index];

    m_manualTest.current_gauge.case_id = thumb.case_id;
    m_manualTest.current_gauge.image_id = thumb.image_id;
    m_manualTest.current_gauge.target_id = thumb.target_id;
    m_manualTest.current_gauge.tool = thumb.tool;

    for (const auto& entry : m_manualTest.catalog_entries)
    {
        if (entry.script_id == thumb.script_id)
        {
            std::string text;
            if (ReadTextFile(ResolveWorkspaceFile(entry.path).generic_string(), text))
                m_manualTest.editor_text = text;
            m_manualTest.active_script_case_name = entry.label;
            m_manualTest.active_script_case_path = entry.path;
            m_manualTest.loaded_script_path = entry.path;
            break;
        }
    }

    ApplyManualGaugeToGlobals(m_manualTest);
}

void ViewController::DrawEvidenceChainThumbnailRail()
{
    DrawScriptEvidenceThumbnailRailByGroup();
}

void ViewController::RebuildScriptEvidenceGroups()
{
    m_manualTest.script_evidence_groups.clear();

    std::map<std::string, int> groupByScript;

    for (const auto& entry : m_manualTest.catalog_entries)
    {
        std::string scriptId = entry.script_id.empty() ? entry.label : entry.script_id;
        if (scriptId.empty())
            continue;

        ScriptEvidenceGroup group;
        group.script_id = scriptId;
        group.script_path = entry.path;
        group.label = entry.label;

        groupByScript[group.script_id] =
            static_cast<int>(m_manualTest.script_evidence_groups.size());

        m_manualTest.script_evidence_groups.push_back(group);
    }

    for (const auto& item : m_manualTest.evidence_items)
    {
        std::string scriptId = item.script_id;
        if (scriptId.empty())
            continue;

        auto it = groupByScript.find(scriptId);
        if (it == groupByScript.end())
            continue;

        ScriptEvidenceThumb thumb;
        thumb.case_id = item.case_id;
        thumb.script_id = item.script_id;
        thumb.image_id = item.image_id;
        thumb.target_id = item.target_id;
        thumb.tool = item.tool;
        thumb.status = item.contract_status;
        thumb.reason = "";

        thumb.image_path = ResolveImagePathFromManifest(item.image_id);

        thumb.parameter_summary = "";
        if (!item.image_id.empty())
            thumb.parameter_summary += "img=" + item.image_id;
        if (!item.target_id.empty())
            thumb.parameter_summary += " target=" + item.target_id;

        m_manualTest.script_evidence_groups[it->second].thumbs.push_back(thumb);
    }

    m_manualTest.script_evidence_groups_dirty = false;
}

std::string ViewController::ResolveImagePathFromManifest(const std::string& imageId) const
{
    for (const auto& item : m_manualTest.image_manifest_items)
    {
        if (item.image_id == imageId)
            return item.image_path;
    }
    return "";
}

void ViewController::EnsureScriptEvidenceThumbTexture(ScriptEvidenceThumb& thumb)
{
    if (thumb.texture_loaded || thumb.texture_failed)
        return;

    if (thumb.image_path.empty())
    {
        thumb.texture_failed = true;
        thumb.reason = "image_path empty";
        return;
    }

    cv::Mat img = cv::imread(thumb.image_path, cv::IMREAD_COLOR);
    if (img.empty())
    {
        thumb.texture_failed = true;
        thumb.reason = "image load failed";
        return;
    }

    cv::Mat resized;
    double scale = std::min(64.0 / img.cols, 64.0 / img.rows);
    cv::resize(img, resized, cv::Size(), scale, scale, cv::INTER_AREA);

    thumb.texture_id = CreateTextureFromMat0(resized);
    thumb.texture_w = resized.cols;
    thumb.texture_h = resized.rows;
    thumb.texture_loaded = (thumb.texture_id != 0);
    thumb.texture_failed = !thumb.texture_loaded;
}

void ViewController::SelectScriptEvidenceThumb(int groupIndex, int thumbIndex)
{
    if (groupIndex < 0 || groupIndex >= static_cast<int>(m_manualTest.script_evidence_groups.size()))
        return;

    auto& group = m_manualTest.script_evidence_groups[groupIndex];

    if (thumbIndex < 0 || thumbIndex >= static_cast<int>(group.thumbs.size()))
        return;

    auto& thumb = group.thumbs[thumbIndex];

    m_manualTest.selected_evidence_group = groupIndex;
    m_manualTest.selected_evidence_thumb = thumbIndex;

    for (const auto& entry : m_manualTest.catalog_entries)
    {
        if (entry.script_id == thumb.script_id)
        {
            std::string text;
            if (ReadTextFile(ResolveWorkspaceFile(entry.path).generic_string(), text))
                m_manualTest.editor_text = text;
            m_manualTest.active_script_case_name = entry.label;
            m_manualTest.active_script_case_path = entry.path;
            m_manualTest.loaded_script_path = entry.path;
            break;
        }
    }

    if (!thumb.image_path.empty())
    {
        cv::Mat img = cv::imread(thumb.image_path, cv::IMREAD_COLOR);
        if (!img.empty())
        {
            UpdateImageViewImage(img);
            m_parserDebugBridge.SetGlobalMatInput(img);
            m_manualTest.image_file_path = thumb.image_path;
        }
    }

    m_manualTest.active_case_id = thumb.case_id;
    m_manualTest.active_image_id = thumb.image_id;
    m_manualTest.active_target_id = thumb.target_id;

    m_manualTest.current_gauge.case_id = thumb.case_id;
    m_manualTest.current_gauge.image_id = thumb.image_id;
    m_manualTest.current_gauge.target_id = thumb.target_id;
    m_manualTest.current_gauge.tool = thumb.tool;

    ApplyManualGaugeToGlobals(m_manualTest);
}

void ViewController::DrawScriptEvidenceThumbnailRailByGroup()
{
    if (m_manualTest.script_evidence_groups_dirty)
        RebuildScriptEvidenceGroups();

    ImGui::BeginChild("##script_evidence_thumb_rail",
                      ImVec2(86.0f, 0.0f),
                      true);

    for (int g = 0; g < static_cast<int>(m_manualTest.script_evidence_groups.size()); ++g)
    {
        ScriptEvidenceGroup& group = m_manualTest.script_evidence_groups[g];

        if (group.thumbs.empty())
            continue;

        ImGui::PushID(g);

        ImGui::TextDisabled("%s", group.script_id.c_str());
        ImGui::Separator();

        for (int i = 0; i < static_cast<int>(group.thumbs.size()); ++i)
        {
            ScriptEvidenceThumb& thumb = group.thumbs[i];

            ImGui::PushID(i);

            EnsureScriptEvidenceThumbTexture(thumb);

            const bool selected =
                m_manualTest.selected_evidence_group == g &&
                m_manualTest.selected_evidence_thumb == i;

            if (selected)
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.55f, 0.85f, 1.0f));

            bool clicked = false;

            if (thumb.texture_loaded)
            {
                ImTextureID texId = (ImTextureID)(uint64_t)thumb.texture_id;
                clicked = ImGui::ImageButton("##thumb", texId, ImVec2(64, 64));
            }
            else
            {
                clicked = ImGui::Button("NO IMG##thumb", ImVec2(64, 64));
            }

            if (selected)
                ImGui::PopStyleColor();

            if (clicked)
            {
                SelectScriptEvidenceThumb(g, i);
            }

            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::Text("case: %s", thumb.case_id.c_str());
                ImGui::Text("script: %s", thumb.script_id.c_str());
                ImGui::Text("image: %s", thumb.image_id.c_str());
                ImGui::Text("target: %s", thumb.target_id.c_str());
                ImGui::Text("tool: %s", thumb.tool.c_str());
                ImGui::TextWrapped("params: %s", thumb.parameter_summary.c_str());
                ImGui::TextWrapped("path: %s", thumb.image_path.c_str());
                if (thumb.texture_failed)
                    ImGui::TextColored(ImVec4(1, 0.4f, 0.3f, 1), "failed: %s", thumb.reason.c_str());
                ImGui::EndTooltip();
            }

            ImGui::PopID();
        }

        ImGui::Spacing();
        ImGui::Separator();

        ImGui::PopID();
    }

    if (m_manualTest.script_evidence_groups.empty())
    {
        ImGui::TextDisabled("No evidence chain");
    }

    ImGui::EndChild();
}

void ViewController::DrawScriptEditorBlock(ManualTestContext& context)
{
    ImGui::PushID("script_editor_block");

    ImGui::Text("Script Editor");
    if (InputTextMultilineString("##manual_script_editor",
                                 context.editor_text,
                                 ImVec2(-1.0f, 140.0f)))
    {
        context.editor_dirty = true;
        if (context.editor_source.empty())
            context.editor_source = "manual";
    }
    ImGui::Text("editor_dirty: %s", context.editor_dirty ? "true" : "false");
    ImGui::Text("editor_source: %s", context.editor_source.c_str());
    ImGui::TextWrapped("loaded_script_path: %s",
                       context.loaded_script_path.empty() ? "(none)" :
                       context.loaded_script_path.c_str());

    ImGui::PopID();
}

void ViewController::DrawScriptDebugCompilerBlock(ManualTestContext& context)
{
    ImGui::PushID("script_debug_compiler_block");

    ImGui::Text("Script Debug Compiler");
    ImGui::Text("run_state: %s", context.run_state.c_str());

    const auto syncGeometryResult = [&]()
    {
        for (const ScriptVariableView& variable : context.global_variable_views)
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

    if (ImGui::Button("Compile##script_debug"))
    {
        ResetCxDebugRuntimeLog(context, "Compile");
        AnalyzeScript(context);
        ResetDebugRuntimeForReplay(context);

        context.run_state = "compiled";
        context.debug_action = "Compile";
        context.debug_status = "PENDING";
        context.debug_reason = "source compiled for debug replay; runtime not executed";

        m_scriptResult = ScriptResult();
        m_scriptResult.source = context.editor_source;
        m_scriptResult.script_path = context.loaded_script_path;
        m_scriptResult.status = "PENDING";
        m_scriptResult.reason = "compiled for debug replay; no PASS without runtime result";
        m_scriptResult.runtime_fillback_status = "debug_replay_ready";
    }

    ImGui::SameLine();
    if (ImGui::Button("Run##script_debug"))
    {
        ResetCxDebugRuntimeLog(context, "Run");
        AnalyzeScript(context);
        ResetDebugRuntimeForReplay(context);

        context.run_state = "runtime_run";
        context.stop_requested = false;

        int guard = 0;
        const int maxSteps = static_cast<int>(context.line_views.size()) * 4 + 16;

        while (!context.stop_requested &&
            context.current_line < static_cast<int>(context.line_views.size()) &&
            guard++ < maxSteps)
        {
            DebugStepOnceWithSnapshot(context);

            if (context.run_state == "blocked")
                break;
        }
        MarkDebugRunFinishedIfAtEnd(context);
        m_scriptResult.status = context.debug_status;
        m_scriptResult.reason = context.debug_reason;
        m_scriptResult.runtime_fillback_status = "debug_run";
        syncGeometryResult();
    }

    ImGui::SameLine();
    if (ImGui::Button("Step##script_debug"))
    {
        if (context.run_state == "idle" ||
            context.run_state == "compiled" ||
            context.run_state == "ready")
        {
            if (context.runtime_objects.empty() &&
                context.runtime_int_vars.empty())
            {
                ResetDebugRuntimeForReplay(context);
            }
        }

        DebugStepOnceWithSnapshot(context);

        m_scriptResult.status = context.debug_status;
        m_scriptResult.reason = context.debug_reason;
        m_scriptResult.runtime_fillback_status = "debug_step";
        syncGeometryResult();
    }

    ImGui::SameLine();
    if (ImGui::Button("Continue##script_debug"))
    {
        context.stop_requested = false;
        context.run_state = "runtime_continue";

        int guard = 0;
        const int maxSteps = static_cast<int>(context.line_views.size()) * 4 + 16;

        while (!context.stop_requested &&
            context.current_line < static_cast<int>(context.line_views.size()) &&
            guard++ < maxSteps)
        {
            DebugStepOnceWithSnapshot(context);

            if (context.run_state == "blocked")
                break;
        }
        MarkDebugRunFinishedIfAtEnd(context);
        m_scriptResult.status = context.debug_status;
        m_scriptResult.reason = context.debug_reason;
        m_scriptResult.runtime_fillback_status = "debug_continue";
        syncGeometryResult();
    }

    ImGui::SameLine();
    if (ImGui::Button("Stop##script_debug"))
    {
        context.stop_requested = true;
        context.run_state = "stopped";
        context.debug_status = "PENDING";
        context.debug_reason = "debug run stopped by user";

        m_scriptResult.status = "PENDING";
        m_scriptResult.reason = context.debug_reason;
        m_scriptResult.runtime_fillback_status = "stopped";
    }

    ImGui::SameLine();
    if (ImGui::Button("Reset##script_debug"))
    {
        AnalyzeScript(context);
        ResetDebugRuntimeForReplay(context);

        m_scriptResult = ScriptResult();
        m_scriptResult.status = "PENDING";
        m_scriptResult.reason = "debug runtime reset";
        m_scriptResult.runtime_fillback_status = "reset";
    }

    if (ImGui::Button("Run File##script_debug"))
        m_scriptResult = RunCxScript(context.script_file_path);

    ImGui::SameLine();
    if (ImGui::Button("Run Bound State##script_debug"))
    {
        ResetCxDebugRuntimeLog(context, "Run Bound State");
        std::string boundScript;
        const bool boundReady = !context.bound_state_script_path.empty() &&
            ReadTextFile(ResolveWorkspaceFile(context.bound_state_script_path).generic_string(),
                         boundScript);
        if (boundReady)
        {
            context.editor_text = boundScript;
            context.loaded_script_path = context.bound_state_script_path;
            context.editor_source = "bound_state";
            context.analyzed_text.clear();
        }
        if (!boundReady)
        {
            context.run_state = "blocked";
            context.debug_status = "BLOCKED";
            context.debug_reason = "bound N0 script unavailable";
        }
        else
        {
            AnalyzeScript(context);
            ResetDebugRuntimeForReplay(context);
            context.stop_requested = false;
            int guard = 0;
            const int maxSteps = static_cast<int>(context.line_views.size()) * 4 + 16;
            while (!context.stop_requested &&
                   context.current_line < static_cast<int>(context.line_views.size()) &&
                   guard++ < maxSteps)
            {
                DebugStepOnceWithSnapshot(context);
                if (context.run_state == "blocked") break;
            }
            MarkDebugRunFinishedIfAtEnd(context);
        }
        m_scriptResult.status = context.run_state == "blocked" ?
            "BLOCKED" : "PENDING";
        m_scriptResult.reason = context.debug_reason;
        m_scriptResult.runtime_fillback_status = "bound_block_debug_steps";
        syncGeometryResult();
    }

    ImGui::SameLine();
    if (ImGui::Button("Clear Result##script_debug"))
    {
        m_scriptResult = ScriptResult();
        m_scriptResult.status = "PENDING";
        m_scriptResult.reason = "result cleared";
        m_scriptResult.runtime_fillback_status = "not_started";
        context.debug_action = "Clear Result";
        context.debug_status = "PENDING";
        context.debug_reason = m_scriptResult.reason;
        context.debug_parser_output.clear();
    }

    if (ImGui::Button("Save Debug Log Snapshot##script_debug"))
    {
        RefreshSnapshotFromCurrentResultRef(context);
        std::string savedPath;
        std::string reason;
        if (SaveCxDebugSnapshotText(context, savedPath, reason))
        {
            m_scriptResult.status = "PENDING";
            m_scriptResult.reason = "debug snapshot saved: " + savedPath;
            m_scriptResult.runtime_fillback_status = "debug_snapshot_saved";
        }
        else
        {
            m_scriptResult.status = "PENDING";
            m_scriptResult.reason = reason;
            m_scriptResult.runtime_fillback_status = "debug_snapshot_save_failed";
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Clear Debug Log##script_debug"))
    {
        ResetCxDebugRuntimeLog(context, "manual clear debug log");

        m_scriptResult.status = "PENDING";
        m_scriptResult.reason = "debug runtime log cleared";
        m_scriptResult.runtime_fillback_status = "debug_log_cleared";
    }

    ImGui::SameLine();
    if (ImGui::Button("Run CxParserExt Debug##script_debug"))
    {
        const std::string scriptPath = !context.loaded_script_path.empty() ?
            context.loaded_script_path : context.active_script_case_path;
        CxScriptSemanticBridgeResult debugResult;
        if (scriptPath.empty())
        {
            debugResult.ok = false;
            debugResult.status = "missing_script_path";
            debugResult.reason =
                "CxParserExt debug in-process requires a script file path";
        }
        else
        {
            const fs::path resolvedScript = ResolveWorkspaceFile(scriptPath);
            m_parserDebugBridge.RunCxParserExtDebugInProcess(
                resolvedScript.generic_string(),
                debugResult);
        }
        ApplyCxParserExtDebugResultToManualConsole(context, debugResult);
    }

    ImGui::PopID();
}

void ViewController::drawManualStateTestConsole()
{
  if (!m_showManualStateTestConsole) return;

  EnsureCxScriptWorkbenchAssetsLoaded();

  ImGui::SetNextWindowPos(ImVec2(8, 8), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(820, 980), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Script Debug Console / Manual State Test Console",
                    &m_showManualStateTestConsole,
                    ImGuiWindowFlags_NoCollapse))
  {
    ImGui::End();
    return;
  }

  m_manualTest.source_preview_enabled = m_showSourcePreviewOverlay;
  m_manualTest.manual_elements_count =
    static_cast<int>(m_annotationLayer.Elements().size());

  DrawScriptEditorBlock(m_manualTest);
  ImGui::Separator();

  DrawScriptDebugCompilerBlock(m_manualTest);
  ImGui::Separator();

  if (ImGui::Button("Load Catalog"))
  {
      CxScriptCatalogRuntime catalog;
      std::string catalog_reason;
      m_manualTest.catalog_entries.clear();
      if (LoadCxScriptCatalogFile(m_manualTest.catalog_path, catalog, catalog_reason))
      {
          m_manualTest.catalog_loaded = true;
          for (const auto& entry : catalog.scripts)
              m_manualTest.catalog_entries.push_back(entry);
      }
      else
      {
          m_manualTest.catalog_loaded = false;
      }
  }
  ImGui::SameLine();
  if (ImGui::Button("Load Manifest"))
  {
      std::ifstream manifestFile(m_manualTest.manifest_path);
      if (manifestFile.is_open())
      {
          std::string jsonContent((std::istreambuf_iterator<char>(manifestFile)),
                                  std::istreambuf_iterator<char>());
          manifestFile.close();

          m_manualTest.image_manifest_entries.clear();
          size_t pos = 0;
          while ((pos = jsonContent.find("image_id", pos)) != std::string::npos)
          {
              pos += 9;
              size_t colon = jsonContent.find(":", pos);
              if (colon == std::string::npos) break;
              colon++;
              size_t quote = jsonContent.find("\"", colon);
              if (quote == std::string::npos) break;
              quote++;
              size_t endQuote = jsonContent.find("\"", quote);
              if (endQuote == std::string::npos) break;
              m_manualTest.image_manifest_entries.push_back(jsonContent.substr(quote, endQuote - quote));
              pos = endQuote;
          }
          m_manualTest.manifest_loaded = true;
          m_manualTest.manifest_load_reason = "loaded";
      }
      else
      {
          m_manualTest.manifest_loaded = false;
          m_manualTest.manifest_load_reason = "failed to open " + m_manualTest.manifest_path;
      }
  }
  ImGui::SameLine();
  if (ImGui::Button("Reload Chain"))
  {
      m_manualTest.evidence_items.clear();
      for (const auto& entry : m_manualTest.catalog_entries)
      {
          if (!entry.manual_visible || !entry.frozen) continue;
          ManualEvidenceItem item;
          item.case_id = entry.script_id;
          item.tool = entry.tool;
          item.image_id = "";
          item.target_id = "";
          item.script_id = entry.script_id;
          item.gauge_status = "pending";
          item.probe_status = "pending";
          item.contract_status = "pending";
          item.review_status = "pending";
          m_manualTest.evidence_items.push_back(item);
      }
  }

  ImGui::SameLine(); ImGui::Text("|"); ImGui::SameLine();

  if (ImGui::Button("Run##manual_control"))
  {
      m_manualTest.run_state = "running";
      m_scriptResult = ScriptResult();
      m_scriptResult.source = "manual";
      m_scriptResult.script_path = m_manualTest.loaded_script_path;
      m_scriptResult.status = "RUNNING";
      m_scriptResult.reason = "executing current script";
  }
  ImGui::SameLine();
  if (ImGui::Button("Step##manual_control"))
  {
      m_manualTest.debug_action = "step";
  }
  ImGui::SameLine();
  if (ImGui::Button("Reset##manual_control"))
  {
      m_scriptResult = ScriptResult();
      m_manualTest.run_state = "idle";
      m_manualTest.current_gauge.has_line_gauge = false;
      m_manualTest.current_gauge.has_circle_gauge = false;
  }
  ImGui::SameLine();
  if (ImGui::Button("Save Snapshot"))
  {
      m_manualTest.debug_action = "save_snapshot";
  }

  ImGui::SameLine(); ImGui::Text("|"); ImGui::SameLine();

  if (ImGui::Button("Accept Gauge"))
  {
      m_manualTest.current_gauge.accepted = true;
      m_manualTest.current_gauge.dirty = false;
      m_manualTest.current_gauge.review_status = "manual_accepted";
  }
  ImGui::SameLine();
  if (ImGui::Button("Save Annotation"))
  {
      std::string caseId = m_manualTest.current_gauge.case_id.empty() ? "unnamed" : m_manualTest.current_gauge.case_id;
      std::filesystem::path dir = "cxscript_runs/manual_gauge_workbench/" + caseId;
      std::filesystem::create_directories(dir);
      std::filesystem::path filePath = dir / "gauge_annotation.json";

      std::ofstream ofs(filePath);
      if (ofs.is_open())
      {
          ofs << "{\n";
          ofs << "  \"case_id\": \"" << m_manualTest.current_gauge.case_id << "\",\n";
          ofs << "  \"image_id\": \"" << m_manualTest.current_gauge.image_id << "\",\n";
          ofs << "  \"target_id\": \"" << m_manualTest.current_gauge.target_id << "\",\n";
          ofs << "  \"tool\": \"" << m_manualTest.current_gauge.tool << "\",\n";
          ofs << "  \"source\": \"" << m_manualTest.current_gauge.source << "\",\n";
          ofs << "  \"review_status\": \"" << m_manualTest.current_gauge.review_status << "\",\n";
          ofs << "  \"line\": {\n";
          ofs << "    \"x0\": " << m_manualTest.current_gauge.line_x0 << ",\n";
          ofs << "    \"y0\": " << m_manualTest.current_gauge.line_y0 << ",\n";
          ofs << "    \"x1\": " << m_manualTest.current_gauge.line_x1 << ",\n";
          ofs << "    \"y1\": " << m_manualTest.current_gauge.line_y1 << ",\n";
          ofs << "    \"tool_half_width\": " << m_manualTest.current_gauge.tool_half_width << "\n";
          ofs << "  },\n";
          ofs << "  \"circle\": {\n";
          ofs << "    \"cx\": " << m_manualTest.current_gauge.circle_cx << ",\n";
          ofs << "    \"cy\": " << m_manualTest.current_gauge.circle_cy << ",\n";
          ofs << "    \"px\": " << m_manualTest.current_gauge.circle_px << ",\n";
          ofs << "    \"py\": " << m_manualTest.current_gauge.circle_py << ",\n";
          ofs << "    \"radius\": " << m_manualTest.current_gauge.radius << ",\n";
          ofs << "    \"inner_radius\": " << m_manualTest.current_gauge.inner_radius << ",\n";
          ofs << "    \"outer_radius\": " << m_manualTest.current_gauge.outer_radius << "\n";
          ofs << "  },\n";
          ofs << "  \"params\": {\n";
          ofs << "    \"threshold\": " << m_manualTest.current_gauge.threshold << ",\n";
          ofs << "    \"gap\": " << m_manualTest.current_gauge.gap << ",\n";
          ofs << "    \"linegap\": " << m_manualTest.current_gauge.linegap << ",\n";
          ofs << "    \"wgap\": " << m_manualTest.current_gauge.wgap << ",\n";
          ofs << "    \"hgap\": " << m_manualTest.current_gauge.hgap << ",\n";
          ofs << "    \"filterprofile\": " << m_manualTest.current_gauge.filterprofile << ",\n";
          ofs << "    \"method\": " << m_manualTest.current_gauge.method << "\n";
          ofs << "  }\n";
          ofs << "}\n";
          ofs.close();
      }
  }
  ImGui::Separator();

  ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
  if (ImGui::CollapsingHeader("Compact Flow"))
  {
      ImGui::Text("Current: %s", m_manualTest.runtime_current_status.c_str());
      ImGui::Text("Node: %s", m_manualTest.runtime_current_node.c_str());
      ImGui::Text("Case: %s", m_manualTest.active_script_case_name.c_str());
      ImGui::Text("Script: %s", m_manualTest.active_script_case_path.c_str());

      ImGui::SameLine();
      if (ImGui::Button("N0 Load")) {}
      ImGui::SameLine();
      if (ImGui::Button("N1 Run")) {}
      ImGui::SameLine();
      if (ImGui::Button("N2 Review")) {}
      ImGui::SameLine();
      if (ImGui::Button("N3 Contract")) {}
  }
  ImGui::Separator();

  DrawCxScriptWorkbenchOverview(m_manualTest);
  DrawCxScriptTemplateSummaryPanel(m_manualTest);
  ImGui::Separator();

  ImGui::SetNextItemOpen(false, ImGuiCond_FirstUseEver);
  if (ImGui::CollapsingHeader("Input Source"))
  {
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

    if (ImGui::Button("Export Gauge Frame Probe"))
    {
      std::string scriptPath = !m_manualTest.loaded_script_path.empty()
        ? m_manualTest.loaded_script_path
        : m_manualTest.script_file_path;
      fs::path resolvedScript = ResolveWorkspaceFile(scriptPath);
      if (!fs::exists(resolvedScript) && fs::exists(fs::path(scriptPath)))
        resolvedScript = fs::path(scriptPath);

      GaugeFrameProbeOptions options;
      options.enabled = true;
      options.image_path = m_manualTest.image_file_path;
      options.script_path = resolvedScript;
      const std::string caseName = resolvedScript.stem().string().empty()
        ? "manual_frame_probe"
        : resolvedScript.stem().string();
      options.case_name = caseName;
      options.out_root = fs::path("D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxscript_runs/gauge_frame_probe_manual") / caseName;

      GaugeFrameProbeResult probeResult;
      const bool exported = RunGaugeFrameProbe(options, probeResult);
      m_scriptResult.status = exported ? "PASS" : "BLOCKED";
      std::ostringstream reason;
      reason << (exported ? "Gauge frame probe exported" : "Gauge frame probe export failed")
             << " | reason=" << probeResult.reason
             << " | frame_black=" << probeResult.frame_black_path.string()
             << " | frame_on_image=" << probeResult.frame_on_image_path.string()
             << " | frame_geometry=" << probeResult.frame_geometry_path.string()
             << " | frame_report=" << probeResult.frame_report_path.string();
      m_scriptResult.reason = reason.str();
      m_scriptResult.runtime_fillback_status = exported ? "gauge_frame_probe_exported" : "gauge_frame_probe_failed";
    }
    ImGui::Separator();

    ImGui::SetNextItemOpen(false, ImGuiCond_FirstUseEver);
    if (ImGui::CollapsingHeader("Manual Gauge State / Apply To Globals"))
    {
      ManualGaugeState& gauge = m_manualTest.current_gauge;
      ImGui::TextWrapped(
        "This reuses the existing ManualStateTestConsole gauge/script/replay chain. The current ManualGaugeState is the single source for Apply To Globals, probe, annotation, and manifest candidate.");

      ImGui::Separator();
      ImGui::Text("Phase 1 anchor cases only:");
      ImGui::BulletText("L0_basic_findcircle_basic_dot_ok");
      ImGui::BulletText("L1_line_high_contrast_001_plate_top_edge_ok");
      ImGui::BulletText("L1_line_high_contrast_002_metal_part_lower_right_edge_ok");

      ImGui::Separator();
      InputTextString("Gauge case_id", gauge.case_id);
      InputTextString("Gauge image_id", gauge.image_id);
      InputTextString("Gauge target_id", gauge.target_id);
      InputTextString("Gauge tool", gauge.tool);
      InputTextString("Gauge source", gauge.source);
      InputTextString("Gauge review_status", gauge.review_status);

      if (ImGui::Button("Load Gauge From Current Globals"))
      {
        auto readInt = [&](const std::string& name, int& value) {
          const auto it = m_manualTest.runtime_int_vars.find(name);
          if (it != m_manualTest.runtime_int_vars.end())
          {
            value = it->second;
            return true;
          }
          return false;
        };

        gauge.has_line_gauge =
          readInt("global.roi_x0", gauge.line_x0) |
          readInt("global.roi_y0", gauge.line_y0) |
          readInt("global.roi_x1", gauge.line_x1) |
          readInt("global.roi_y1", gauge.line_y1);
        readInt("global.tool_half_width", gauge.tool_half_width);
        readInt("global.wgap", gauge.wgap);
        readInt("global.hgap", gauge.hgap);
        readInt("global.linegap", gauge.linegap);
        readInt("global.threshold", gauge.threshold);
        readInt("global.filterprofile", gauge.filterprofile);
        readInt("global.method", gauge.method);

        gauge.has_circle_gauge =
          readInt("global.circle_cx", gauge.circle_cx) |
          readInt("global.circle_cy", gauge.circle_cy) |
          readInt("global.circle_px", gauge.circle_px) |
          readInt("global.circle_py", gauge.circle_py);
        readInt("global.gap", gauge.gap);

        gauge.source = "runtime_globals";
        gauge.review_status = "editing";
        gauge.accepted = false;
        gauge.dirty = false;
        m_manualTest.debug_action = "Load Gauge From Current Globals";
        m_manualTest.debug_status = "PENDING";
        m_manualTest.debug_reason = "ManualGaugeState loaded from runtime_int_vars";
      }

      ImGui::SameLine();
      if (ImGui::Button("Reload Gauge Annotation"))
      {
        std::string reason;
        const fs::path path = ManualGaugeCaseDir(m_manualTest) / "gauge_annotation.json";
        const bool ok = LoadManualGaugeAnnotation(m_manualTest, path, reason);
        m_manualTest.debug_action = "Reload Gauge Annotation";
        m_manualTest.debug_status = ok ? "PENDING" : "BLOCKED";
        m_manualTest.debug_reason = ok ? ("reloaded: " + path.string()) : reason;
      }

      ImGui::SameLine();
      if (ImGui::Button("Accept Current Gauge"))
      {
        gauge.accepted = true;
        gauge.review_status = "manual_accepted";
        gauge.source = "manual";
        gauge.dirty = false;
      }

      ImGui::Separator();
      ImGui::Checkbox("Has Line Gauge", &gauge.has_line_gauge);
      ImGui::InputInt("line_x0", &gauge.line_x0);
      ImGui::InputInt("line_y0", &gauge.line_y0);
      ImGui::InputInt("line_x1", &gauge.line_x1);
      ImGui::InputInt("line_y1", &gauge.line_y1);
      ImGui::InputInt("tool_half_width", &gauge.tool_half_width);
      ImGui::InputInt("wgap", &gauge.wgap);
      ImGui::InputInt("hgap", &gauge.hgap);
      ImGui::InputInt("linegap", &gauge.linegap);
      ImGui::InputInt("threshold", &gauge.threshold);
      ImGui::InputInt("filterprofile", &gauge.filterprofile);
      ImGui::InputInt("method", &gauge.method);

      ImGui::Separator();
      ImGui::Checkbox("Has Circle Gauge", &gauge.has_circle_gauge);
      ImGui::InputInt("circle_cx", &gauge.circle_cx);
      ImGui::InputInt("circle_cy", &gauge.circle_cy);
      ImGui::InputInt("circle_px", &gauge.circle_px);
      ImGui::InputInt("circle_py", &gauge.circle_py);
      ImGui::InputInt("gap", &gauge.gap);

      ImGui::Separator();
      if (ImGui::Button("Apply Gauge To Globals"))
      {
        const bool ok = ApplyManualGaugeToGlobals(m_manualTest);
        if (ok)
        {
          if (gauge.has_line_gauge)
          {
            m_parserDebugBridge.SetGlobalInt("roi_x0", gauge.line_x0);
            m_parserDebugBridge.SetGlobalInt("roi_y0", gauge.line_y0);
            m_parserDebugBridge.SetGlobalInt("roi_x1", gauge.line_x1);
            m_parserDebugBridge.SetGlobalInt("roi_y1", gauge.line_y1);
            m_parserDebugBridge.SetGlobalInt("tool_half_width", gauge.tool_half_width);
            m_parserDebugBridge.SetGlobalInt("wgap", gauge.wgap);
            m_parserDebugBridge.SetGlobalInt("hgap", gauge.hgap);
            m_parserDebugBridge.SetGlobalInt("linegap", gauge.linegap);
            m_parserDebugBridge.SetGlobalInt("threshold", gauge.threshold);
            m_parserDebugBridge.SetGlobalInt("filterprofile", gauge.filterprofile);
            m_parserDebugBridge.SetGlobalInt("method", gauge.method);
          }
          if (gauge.has_circle_gauge)
          {
            m_parserDebugBridge.SetGlobalInt("circle_cx", gauge.circle_cx);
            m_parserDebugBridge.SetGlobalInt("circle_cy", gauge.circle_cy);
            m_parserDebugBridge.SetGlobalInt("circle_px", gauge.circle_px);
            m_parserDebugBridge.SetGlobalInt("circle_py", gauge.circle_py);
            m_parserDebugBridge.SetGlobalInt("gap", gauge.gap);
            m_parserDebugBridge.SetGlobalInt("linegap", gauge.linegap);
            m_parserDebugBridge.SetGlobalInt("threshold", gauge.threshold);
            m_parserDebugBridge.SetGlobalInt("method", gauge.method);
          }
          m_parserDebugBridge.SetGlobalString("gauge_source", gauge.source);
          m_parserDebugBridge.SetGlobalString("gauge_review_status", gauge.review_status);
        }
        if (!ok)
        {
          m_manualTest.debug_action = "Apply Gauge To Globals";
          m_manualTest.debug_status = "BLOCKED";
          m_manualTest.debug_reason = "No active line/circle gauge in ManualGaugeState";
        }
      }

      ImGui::SameLine();
      if (ImGui::Button("Save Gauge Annotation"))
      {
        std::string reason;
        const fs::path path = ManualGaugeCaseDir(m_manualTest) / "gauge_annotation.json";
        const bool ok = SaveManualGaugeAnnotation(m_manualTest, path, reason);
        m_manualTest.debug_action = "Save Gauge Annotation";
        m_manualTest.debug_status = ok ? "PENDING" : "BLOCKED";
        m_manualTest.debug_reason = ok ? ("saved: " + path.string()) : reason;
      }

      ImGui::SameLine();
      if (ImGui::Button("Export Manifest Candidate"))
      {
        std::string reason;
        const fs::path path = ManualGaugeCaseDir(m_manualTest) / "manifest_candidate.json";
        const bool ok = ExportManualGaugeManifestCandidate(m_manualTest, path, reason);
        m_manualTest.debug_action = "Export Manifest Candidate";
        m_manualTest.debug_status = ok ? "PENDING" : "BLOCKED";
        m_manualTest.debug_reason = ok ? ("saved: " + path.string()) : reason;
      }

      if (ImGui::Button("Run Probe With Accepted Gauge"))
      {
        if (!gauge.accepted)
        {
          m_manualTest.debug_action = "Run Probe With Accepted Gauge";
          m_manualTest.debug_status = "BLOCKED";
          m_manualTest.debug_reason = "Gauge must be accepted before probe";
        }
        else
        {
          ApplyManualGaugeToGlobals(m_manualTest);
          m_manualTest.debug_action = "Run Probe With Accepted Gauge";
          m_manualTest.debug_status = "PENDING";
          m_manualTest.debug_reason = "Gauge globals applied; use existing Run/Frame Probe buttons for execution";
        }
      }

      ImGui::Text("Gauge accepted: %s | dirty: %s",
                  gauge.accepted ? "true" : "false",
                  gauge.dirty ? "true" : "false");
      ImGui::TextWrapped("Last action: %s | %s",
                         m_manualTest.debug_action.c_str(),
                         m_manualTest.debug_reason.c_str());
    }

    ImGui::Separator();
    ImGui::SetNextItemOpen(false, ImGuiCond_FirstUseEver);
    if (ImGui::CollapsingHeader("Parameter Regression Panel"))
    {
      ManualParamRegressionState& reg = m_manualTest.param_regression;
      const ManualGaugeState& gauge = m_manualTest.current_gauge;
      const bool gauge_accepted = ManualGaugeAcceptedForParamRegression(gauge);

      ImGui::TextWrapped(
        "Phase 1: parameter regression is gated by accepted ManualGaugeState. "
        "This panel only exports range/candidate/evidence tables; it does not auto-run batches or promote profiles.");
      ImGui::Separator();
      ImGui::Text("Current Context");
      ImGui::Text("case=%s image=%s target=%s tool=%s",
                  gauge.case_id.c_str(),
                  gauge.image_id.c_str(),
                  gauge.target_id.c_str(),
                  gauge.tool.c_str());
      ImGui::Text("gauge_review_status=%s accepted=%s",
                  gauge.review_status.c_str(),
                  gauge_accepted ? "yes" : "no");
      if (!gauge_accepted)
      {
        ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.2f, 1.0f),
                           "Blocked: accept the current gauge before creating parameter candidates.");
      }

      ImGui::Separator();
      ImGui::Text("Regression Limits");
      ImGui::InputInt("Max Candidates", &reg.max_candidates);
      ImGui::InputInt("Max Case Seconds", &reg.max_case_seconds);
      ImGui::InputInt("Max Total Seconds", &reg.max_total_seconds);
      if (reg.max_candidates < 1) reg.max_candidates = 1;
      if (reg.max_candidates > 64) reg.max_candidates = 64;
      if (reg.max_case_seconds < 1) reg.max_case_seconds = 1;
      if (reg.max_total_seconds < reg.max_case_seconds)
        reg.max_total_seconds = reg.max_case_seconds;

      ImGui::BeginDisabled(!gauge_accepted);
      if (ImGui::Button("Initialize From Accepted Gauge"))
      {
        std::string reason;
        const bool ok = InitializeParamRegressionFromGauge(m_manualTest, reason);
        m_manualTest.debug_action = "Initialize Param Regression";
        m_manualTest.debug_status = ok ? "PENDING" : "BLOCKED";
        m_manualTest.debug_reason = ok ? m_manualTest.param_regression.reason : reason;
      }
      ImGui::SameLine();
      if (ImGui::Button("Regenerate Basic Candidates"))
      {
        std::string reason;
        const bool ok = InitializeParamRegressionFromGauge(m_manualTest, reason);
        m_manualTest.debug_action = "Regenerate Param Candidates";
        m_manualTest.debug_status = ok ? "PENDING" : "BLOCKED";
        m_manualTest.debug_reason = ok ? "candidate table regenerated" : reason;
      }

      if (ImGui::Button("Add Candidate"))
      {
        if (!reg.initialized)
        {
          std::string reason;
          InitializeParamRegressionFromGauge(m_manualTest, reason);
        }
        CxParamCandidate c = CandidateFromManualGauge(
            gauge,
            "manual_added_" + std::to_string(static_cast<int>(reg.candidates.size())),
            "manual_added");
        reg.candidates.push_back(c);
        m_manualTest.debug_action = "Add Param Candidate";
        m_manualTest.debug_status = "PENDING";
        m_manualTest.debug_reason = "manual candidate added from current accepted gauge";
      }
      ImGui::SameLine();
      if (ImGui::Button("Clone Current"))
      {
        if (!reg.initialized)
        {
          std::string reason;
          InitializeParamRegressionFromGauge(m_manualTest, reason);
        }
        if (!reg.candidates.empty())
        {
          CxParamCandidate c = reg.candidates.front();
          c.candidate_id = "clone_" + std::to_string(static_cast<int>(reg.candidates.size()));
          c.source = "manual_clone";
          c.selected_for_probe = true;
          reg.candidates.push_back(c);
          m_manualTest.debug_action = "Clone Param Candidate";
          m_manualTest.debug_status = "PENDING";
          m_manualTest.debug_reason = "candidate cloned for manual edit/review";
        }
      }
      ImGui::SameLine();
      if (ImGui::Button("Load From mlpack Rank"))
      {
        if (!reg.initialized)
        {
          std::string reason;
          InitializeParamRegressionFromGauge(m_manualTest, reason);
        }
        AddMlpackRankPlaceholderCandidates(reg);
        m_manualTest.debug_action = "Load From mlpack Rank";
        m_manualTest.debug_status = "PENDING";
        m_manualTest.debug_reason = "rule-based mlpack rank placeholder candidate appended";
      }

      if (ImGui::Button("Load From ensmallen Opt"))
      {
        if (!reg.initialized)
        {
          std::string reason;
          InitializeParamRegressionFromGauge(m_manualTest, reason);
        }
        AddEnsmallenOptPlaceholderCandidates(reg);
        m_manualTest.debug_action = "Load From ensmallen Opt";
        m_manualTest.debug_status = "PENDING";
        m_manualTest.debug_reason = "bounded ensmallen placeholder candidate appended";
      }
      ImGui::SameLine();
      if (ImGui::Button("Run Selected Probe"))
      {
        m_manualTest.debug_action = "Run Selected Probe";
        m_manualTest.debug_status = "BLOCKED";
        m_manualTest.debug_reason =
            "Phase 1 prepares selected probe assets only. Use exported candidates with suite/headless short probe.";
      }
      ImGui::SameLine();
      if (ImGui::Button("Run Top N"))
      {
        m_manualTest.debug_action = "Run Top N";
        m_manualTest.debug_status = "BLOCKED";
        m_manualTest.debug_reason =
            "Automatic batch execution is intentionally disabled until probe timeout controls are wired.";
      }

      if (ImGui::Button("Export Param Regression Reports"))
      {
        std::string reason;
        if (!reg.initialized)
        {
          InitializeParamRegressionFromGauge(m_manualTest, reason);
        }
        bool ok = reg.initialized;
        if (ok)
        {
          ok = ExportParamRegressionReports(
              reg.output_dir,
              reg.task,
              reg.range_set,
              reg.candidates,
              reg.records,
              reg.accuracy_stats,
              reason);
        }
        if (ok)
        {
          std::string checklist_reason;
          ExportParamRegressionManualAcceptanceChecklist(
              m_manualTest,
              fs::path(reg.output_dir) / "manual_acceptance_checklist.md",
              checklist_reason);
          RefreshParamRegressionExportedFiles(reg);
          reg.last_export_status = "PASS";
          reg.last_export_reason = "reports exported for manual acceptance";
        }
        else
        {
          reg.last_export_status = "FAIL";
          reg.last_export_reason = reason;
        }
        m_manualTest.debug_action = "Export Param Regression Reports";
        m_manualTest.debug_status = ok ? "PENDING" : "BLOCKED";
        m_manualTest.debug_reason = ok ? ("exported: " + reg.output_dir) : reason;
      }
      ImGui::SameLine();
      if (ImGui::Button("Save As Diagnostic Profile"))
      {
        std::string reason;
        if (!reg.initialized)
        {
          InitializeParamRegressionFromGauge(m_manualTest, reason);
        }
        bool ok = reg.initialized;
        if (ok)
        {
          ok = ExportParamRegressionReports(
              reg.output_dir,
              reg.task,
              reg.range_set,
              reg.candidates,
              reg.records,
              reg.accuracy_stats,
              reason);
        }
        if (ok)
        {
          std::string checklist_reason;
          ExportParamRegressionManualAcceptanceChecklist(
              m_manualTest,
              fs::path(reg.output_dir) / "manual_acceptance_checklist.md",
              checklist_reason);
          RefreshParamRegressionExportedFiles(reg);
          reg.last_export_status = "PASS";
          reg.last_export_reason = "diagnostic profile candidate exported";
        }
        else
        {
          reg.last_export_status = "FAIL";
          reg.last_export_reason = reason;
        }
        m_manualTest.debug_action = "Save Diagnostic Profile Candidate";
        m_manualTest.debug_status = ok ? "PENDING" : "BLOCKED";
        m_manualTest.debug_reason = ok
            ? ("diagnostic profile candidate saved: " + (fs::path(reg.output_dir) / "param_profile_candidate.cxsc").string())
            : reason;
      }
      ImGui::SameLine();
      if (ImGui::Button("Promote To Profile Candidate"))
      {
        m_manualTest.debug_action = "Promote To Profile Candidate";
        m_manualTest.debug_status = "BLOCKED";
        m_manualTest.debug_reason =
            "Promotion gate is closed in phase 1: mini-regression and human accept matrix are required.";
      }
      ImGui::EndDisabled();

      ImGui::Text("status=%s reason=%s", reg.status.c_str(), reg.reason.c_str());
      ImGui::Text("output_dir=%s", reg.output_dir.c_str());

      ImGui::Separator();
      ImGui::Text("Parameter Range Editor");
      ImGui::BeginDisabled(!gauge_accepted || !reg.initialized);
      if (ImGui::BeginTable("param_range_table", 7,
                            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
      {
        ImGui::TableSetupColumn("Parameter");
        ImGui::TableSetupColumn("Min");
        ImGui::TableSetupColumn("Max");
        ImGui::TableSetupColumn("Step");
        ImGui::TableSetupColumn("Values");
        ImGui::TableSetupColumn("Role");
        ImGui::TableSetupColumn("Enabled");
        ImGui::TableHeadersRow();
        for (std::size_t ri = 0; ri < reg.range_set.ranges.size(); ++ri)
        {
          CxParamRange& range = reg.range_set.ranges[ri];
          ImGui::PushID(static_cast<int>(ri));
          std::ostringstream values;
          for (std::size_t i = 0; i < range.discrete_values.size(); ++i)
          {
            if (i) values << ",";
            values << range.discrete_values[i];
          }
          ImGui::TableNextRow();
          ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(range.name.c_str());
          ImGui::TableSetColumnIndex(1); ImGui::Text("%.1f", range.min_value);
          ImGui::TableSetColumnIndex(2); ImGui::Text("%.1f", range.max_value);
          ImGui::TableSetColumnIndex(3); ImGui::Text("%.1f", range.step);
          ImGui::TableSetColumnIndex(4); ImGui::TextUnformatted(values.str().c_str());
          ImGui::TableSetColumnIndex(5);
          ImGui::SetNextItemWidth(120.0f);
          InputTextString("##range_role", range.role);
          ImGui::TableSetColumnIndex(6);
          ImGui::Checkbox("##range_enabled", &range.enabled);
          ImGui::PopID();
        }
        ImGui::EndTable();
      }
      ImGui::EndDisabled();

      ImGui::Separator();
      ImGui::Text("Candidate Table");
      ImGui::TextWrapped("Edit values directly, then export reports. Selected candidates are marked for short-probe planning; automatic batch execution remains disabled.");
      ImGui::BeginDisabled(!gauge_accepted || !reg.initialized);
      if (!reg.candidates.empty())
      {
        if (reg.selected_candidate_index < 0) reg.selected_candidate_index = 0;
        if (reg.selected_candidate_index >= static_cast<int>(reg.candidates.size()))
          reg.selected_candidate_index = static_cast<int>(reg.candidates.size()) - 1;
      }
      if (ImGui::BeginTable("param_candidate_table", 14,
                            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollX,
                            ImVec2(-1, 230)))
      {
        ImGui::TableSetupColumn("#");
        ImGui::TableSetupColumn("Candidate");
        ImGui::TableSetupColumn("Source");
        ImGui::TableSetupColumn("method");
        ImGui::TableSetupColumn("threshold");
        ImGui::TableSetupColumn("gap");
        ImGui::TableSetupColumn("linegap");
        ImGui::TableSetupColumn("wgap");
        ImGui::TableSetupColumn("hgap");
        ImGui::TableSetupColumn("filter");
        ImGui::TableSetupColumn("quality");
        ImGui::TableSetupColumn("risk");
        ImGui::TableSetupColumn("selected");
        ImGui::TableSetupColumn("focus");
        ImGui::TableHeadersRow();
        for (std::size_t i = 0; i < reg.candidates.size(); ++i)
        {
          CxParamCandidate& c = reg.candidates[i];
          ImGui::PushID(static_cast<int>(i));
          ImGui::TableNextRow();
          ImGui::TableSetColumnIndex(0); ImGui::Text("%d", static_cast<int>(i));
          ImGui::TableSetColumnIndex(1);
          ImGui::SetNextItemWidth(150.0f);
          InputTextString("##candidate_id", c.candidate_id);
          ImGui::TableSetColumnIndex(2);
          ImGui::SetNextItemWidth(110.0f);
          InputTextString("##source", c.source);
          ImGui::TableSetColumnIndex(3);
          ImGui::SetNextItemWidth(70.0f);
          ImGui::InputInt("##method", &c.method);
          ImGui::TableSetColumnIndex(4);
          ImGui::SetNextItemWidth(80.0f);
          ImGui::InputInt("##threshold", &c.threshold);
          ImGui::TableSetColumnIndex(5);
          ImGui::SetNextItemWidth(70.0f);
          ImGui::InputInt("##gap", &c.gap);
          ImGui::TableSetColumnIndex(6);
          ImGui::SetNextItemWidth(80.0f);
          ImGui::InputInt("##linegap", &c.linegap);
          ImGui::TableSetColumnIndex(7);
          ImGui::SetNextItemWidth(70.0f);
          ImGui::InputInt("##wgap", &c.wgap);
          ImGui::TableSetColumnIndex(8);
          ImGui::SetNextItemWidth(70.0f);
          ImGui::InputInt("##hgap", &c.hgap);
          ImGui::TableSetColumnIndex(9);
          ImGui::SetNextItemWidth(70.0f);
          ImGui::InputInt("##filter", &c.filterprofile);
          ImGui::TableSetColumnIndex(10);
          ImGui::SetNextItemWidth(80.0f);
          ImGui::InputDouble("##quality", &c.predicted_quality, 0.01, 0.1, "%.2f");
          ImGui::TableSetColumnIndex(11);
          ImGui::SetNextItemWidth(80.0f);
          ImGui::InputDouble("##risk", &c.predicted_risk, 0.01, 0.1, "%.2f");
          ImGui::TableSetColumnIndex(12);
          ImGui::Checkbox("##selected", &c.selected_for_probe);
          ImGui::TableSetColumnIndex(13);
          if (ImGui::SmallButton(reg.selected_candidate_index == static_cast<int>(i) ? "focused" : "focus"))
            reg.selected_candidate_index = static_cast<int>(i);
          ImGui::PopID();
        }
        ImGui::EndTable();
      }
      ImGui::EndDisabled();

      if (!reg.candidates.empty())
      {
        const CxParamCandidate& focused = reg.candidates[static_cast<std::size_t>(reg.selected_candidate_index)];
        ImGui::Text("Focused candidate: %s | source=%s | selected=%s | threshold=%d linegap=%d gap=%d",
                    focused.candidate_id.c_str(),
                    focused.source.c_str(),
                    focused.selected_for_probe ? "yes" : "no",
                    focused.threshold,
                    focused.linegap,
                    focused.gap);
      }

      ImGui::Separator();
      ImGui::Text("Hit Distribution View");
      ImGui::TextWrapped("Phase 1 exports a placeholder hit distribution report. Probe bins will be filled after selected candidates are run against accepted gauge anchors.");

      ImGui::Text("Result / Evidence Records");
      if (ImGui::BeginTable("param_eval_record_table", 9,
                            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollX,
                            ImVec2(-1, 120)))
      {
        ImGui::TableSetupColumn("Candidate");
        ImGui::TableSetupColumn("Executed");
        ImGui::TableSetupColumn("Timeout");
        ImGui::TableSetupColumn("Points");
        ImGui::TableSetupColumn("Fit");
        ImGui::TableSetupColumn("Support");
        ImGui::TableSetupColumn("MeanDist");
        ImGui::TableSetupColumn("FailureStage");
        ImGui::TableSetupColumn("Classification");
        ImGui::TableHeadersRow();
        for (const auto& r : reg.records)
        {
          ImGui::TableNextRow();
          ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(r.candidate_id.c_str());
          ImGui::TableSetColumnIndex(1); ImGui::Text("%s", r.executed ? "yes" : "no");
          ImGui::TableSetColumnIndex(2); ImGui::Text("%s", r.timeout ? "yes" : "no");
          ImGui::TableSetColumnIndex(3); ImGui::Text("%d", r.points);
          ImGui::TableSetColumnIndex(4); ImGui::Text("%s", r.fit_available ? "yes" : "no");
          ImGui::TableSetColumnIndex(5); ImGui::Text("%.2f", r.support_score);
          ImGui::TableSetColumnIndex(6); ImGui::Text("%.2f", r.mean_distance);
          ImGui::TableSetColumnIndex(7); ImGui::TextUnformatted(r.failure_stage.c_str());
          ImGui::TableSetColumnIndex(8); ImGui::TextUnformatted(r.classification.c_str());
        }
        ImGui::EndTable();
      }

      ImGui::Text("Accuracy / Stability Matrix");
      if (ImGui::BeginTable("param_accuracy_table", 6,
                            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
      {
        ImGui::TableSetupColumn("Candidate");
        ImGui::TableSetupColumn("Geometry");
        ImGui::TableSetupColumn("Evidence");
        ImGui::TableSetupColumn("Human");
        ImGui::TableSetupColumn("Stability");
        ImGui::TableSetupColumn("Risk");
        ImGui::TableHeadersRow();
        for (const auto& s : reg.accuracy_stats)
        {
          ImGui::TableNextRow();
          ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(s.candidate_id.c_str());
          ImGui::TableSetColumnIndex(1); ImGui::Text("%.2f", s.geometry_pass_rate);
          ImGui::TableSetColumnIndex(2); ImGui::Text("%.2f", s.evidence_pass_rate);
          ImGui::TableSetColumnIndex(3); ImGui::Text("%.2f", s.human_accept_rate);
          ImGui::TableSetColumnIndex(4); ImGui::Text("%.2f", s.stability_score);
          ImGui::TableSetColumnIndex(5); ImGui::Text("%.2f", s.risk_score);
        }
        ImGui::EndTable();
      }

      ImGui::Separator();
      ImGui::Text("Recommendation & Promotion Gate");
      ImGui::TextColored(ImVec4(0.7f, 0.85f, 1.0f, 1.0f),
                         "Recommendation: inspect exported candidate evidence. Promotion remains disabled in Phase 1.");
      ImGui::TextWrapped("Last export: %s | %s",
                         reg.last_export_status.c_str(),
                         reg.last_export_reason.c_str());

      if (ImGui::Button("Refresh Exported File Index"))
      {
        RefreshParamRegressionExportedFiles(reg);
        m_manualTest.debug_action = "Refresh Param Export Index";
        m_manualTest.debug_status = "PENDING";
        m_manualTest.debug_reason = "exported file index refreshed";
      }
      ImGui::SameLine();
      if (ImGui::Button("Write Manual Acceptance Checklist"))
      {
        std::string reason;
        if (reg.output_dir.empty())
          reg.output_dir = (ManualGaugeCaseDir(m_manualTest) / "param_regression").string();
        const bool ok = ExportParamRegressionManualAcceptanceChecklist(
            m_manualTest,
            fs::path(reg.output_dir) / "manual_acceptance_checklist.md",
            reason);
        if (ok)
          RefreshParamRegressionExportedFiles(reg);
        m_manualTest.debug_action = "Write Manual Acceptance Checklist";
        m_manualTest.debug_status = ok ? "PENDING" : "BLOCKED";
        m_manualTest.debug_reason = ok ? "manual acceptance checklist written" : reason;
      }

      ImGui::Text("Exported Evidence Files");
      if (ImGui::BeginChild("param_exported_files", ImVec2(-1, 120), true))
      {
        if (reg.exported_files.empty())
        {
          ImGui::TextUnformatted("No exported files indexed yet. Click Export Param Regression Reports.");
        }
        for (const auto& file_path : reg.exported_files)
        {
          ImGui::BulletText("%s", file_path.c_str());
        }
      }
      ImGui::EndChild();
    }

    ImGui::Separator();
    ImGui::SetNextItemOpen(false, ImGuiCond_FirstUseEver);
    if (ImGui::CollapsingHeader("Evidence Replay"))
    {
      static std::string replay_package_path;
      static std::string replay_case_id;
      static std::string replay_image_path;
      static std::string replay_script_path;
      static std::string replay_script_snapshot_path;
      static bool replay_loaded = false;

      ImGui::Text("Load replay package to manually reproduce a test case");
      ImGui::SetNextItemWidth(420.0f);
      InputTextString("Replay package path", replay_package_path);
      ImGui::SameLine();
      if (ImGui::Button("Load Replay"))
      {
        std::ifstream pkgFile(replay_package_path);
        if (!pkgFile.is_open())
        {
          m_scriptResult.status = "FAIL";
          m_scriptResult.reason = "replay_package.json not found";
          replay_loaded = false;
        }
        else
        {
          std::string jsonContent((std::istreambuf_iterator<char>(pkgFile)),
                                  std::istreambuf_iterator<char>());

          auto findField = [&](const std::string& field) -> std::string {
            size_t pos = jsonContent.find("\"" + field + "\":");
            if (pos == std::string::npos) return "";
            pos = jsonContent.find(":", pos);
            pos = jsonContent.find_first_not_of(" \t:", pos);
            if (jsonContent[pos] == '"')
            {
              pos++;
              size_t end = jsonContent.find("\"", pos);
              return jsonContent.substr(pos, end - pos);
            }
            else
            {
              size_t end = jsonContent.find_first_of(",}\n", pos);
              return jsonContent.substr(pos, end - pos);
            }
          };

          replay_case_id = findField("case_id");
          replay_image_path = findField("path");
          replay_script_path = findField("path");
          replay_script_snapshot_path = findField("snapshot_path");

          std::filesystem::path pkgDir = std::filesystem::path(replay_package_path).parent_path();
          if (!replay_script_snapshot_path.empty())
          {
            std::filesystem::path snapshotFull = pkgDir / replay_script_snapshot_path;
            if (std::filesystem::exists(snapshotFull))
            {
              if (ReadTextFile(snapshotFull.string(), m_manualTest.editor_text))
              {
                m_manualTest.editor_source = "replay";
                m_manualTest.loaded_script_path = snapshotFull.string();
                m_manualTest.script_file_path = snapshotFull.string();
                m_manualTest.editor_dirty = false;
              }
            }
          }

          if (!replay_image_path.empty())
          {
            cv::Mat image = cv::imread(replay_image_path);
            if (!image.empty())
            {
              UpdateImageViewImage(image);
              m_parserDebugBridge.SetGlobalMatInput(image);
              m_manualTest.image_file_path = replay_image_path;
            }
          }

          replay_loaded = true;
          m_scriptResult.status = "PENDING";
          m_scriptResult.reason = "Replay package loaded: " + replay_case_id;
        }
      }

      if (replay_loaded)
      {
        ImGui::Separator();
        ImGui::Text("Case ID: %s", replay_case_id.c_str());
        ImGui::Text("Image: %s", replay_image_path.c_str());
        ImGui::Text("Script: %s", replay_script_path.c_str());
        ImGui::Text("Snapshot: %s", replay_script_snapshot_path.c_str());

        if (ImGui::Button("Initialize Replay Globals"))
        {
          m_parserDebugBridge.ClearGlobalInputs();
          m_manualTest.debug_action = "Initialize Replay Globals";
          m_scriptResult.status = "PENDING";
          m_scriptResult.reason = "Replay globals initialized from replay_package.json";
        }
      }
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
  }

  ImGui::Separator();
  ImGui::Columns(2, "manual_console_columns", true);
  ImGui::SetNextItemOpen(false, ImGuiCond_FirstUseEver);
  if (ImGui::CollapsingHeader("Builtin Parser Snippets"))
  {
    for (std::size_t i = 0; i < m_manualSnippets.size(); ++i)
    {
      ScriptSnippet& snippet = m_manualSnippets[i];
      if (snippet.source_path != "builtin" && snippet.source_path != "manual")
        continue;

      ImGui::PushID(static_cast<int>(i));
      if (ImGui::Selectable(snippet.name.c_str()))
      {
        m_manualTest.active_script_case_name.clear();
        m_manualTest.active_script_case_path.clear();
        m_manualTest.active_script_case_purpose.clear();
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
  }

  ImGui::Separator();
  ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
  if (ImGui::CollapsingHeader("Evidence Chain"))
  {
      if (!m_manualTest.evidence_items.empty())
      {
          for (const auto& item : m_manualTest.evidence_items)
          {
              ImGui::PushID(item.case_id.c_str());
              if (ImGui::Selectable(item.case_id.c_str()))
              {
                  m_manualTest.current_gauge.case_id = item.case_id;
                  m_manualTest.current_gauge.tool = item.tool;
                  m_manualTest.current_gauge.image_id = item.image_id;
                  m_manualTest.current_gauge.target_id = item.target_id;

                  for (const auto& entry : m_manualTest.catalog_entries)
                  {
                      if (entry.script_id == item.script_id)
                      {
                          std::string text;
                          if (ReadTextFile(ResolveWorkspaceFile(entry.path).generic_string(), text))
                              m_manualTest.editor_text = text;
                          m_manualTest.active_script_case_name = entry.label;
                          m_manualTest.active_script_case_path = entry.path;
                          m_manualTest.loaded_script_path = entry.path;
                          break;
                      }
                  }
              }
              ImGui::Text("  tool: %s | image: %s | target: %s",
                  item.tool.c_str(),
                  item.image_id.c_str(),
                  item.target_id.c_str());
              ImGui::Text("  gauge: %s | probe: %s | contract: %s",
                  item.gauge_status.c_str(),
                  item.probe_status.c_str(),
                  item.contract_status.c_str());
              ImGui::Separator();
              ImGui::PopID();
          }
      }
      else
      {
          ImGui::TextDisabled("Empty Chain");
      }
  }

  ImGui::Separator();
  ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
  if (ImGui::CollapsingHeader("Runnable Scripts"))
  {
    ImGui::Text("Catalog: %s", m_manualTest.catalog_path.c_str());
    ImGui::Text("Status: %s", m_manualTest.catalog_loaded ? "LOADED" : "NOT_LOADED");

    if (!m_manualTest.catalog_entries.empty())
    {
      for (const auto& entry : m_manualTest.catalog_entries)
      {
        bool isVisible = entry.manual_visible && entry.frozen &&
            (entry.expected_result == "ok" || entry.expected_result == "ng_expected");
        if (!isVisible) continue;

        const bool selected =
            m_manualTest.loaded_script_path == entry.path;

        ImGui::PushID(entry.script_id.c_str());
        std::string displayLabel = "[" + entry.expected_result + "] " + entry.label;
        if (ImGui::Selectable(displayLabel.c_str(), selected))
        {
          std::string text;
          if (ReadTextFile(ResolveWorkspaceFile(entry.path).generic_string(), text))
          {
            m_manualTest.editor_text = text;
          }
          m_manualTest.active_script_case_name = entry.label;
          m_manualTest.active_script_case_path = entry.path;
          m_manualTest.active_script_case_purpose = "CxScript Catalog: " + entry.script_id;
          m_manualTest.editor_source = "catalog";
          m_manualTest.loaded_script_path = entry.path;
          m_manualTest.editor_dirty = false;
          m_manualTest.analyzed_text.clear();
          m_manualTest.current_line = 0;
        }
        ImGui::PopID();
      }
    }
    else
    {
      ImGui::TextDisabled("No catalog entries loaded.");
    }
  }

  ImGui::Separator();
  ImGui::SetNextItemOpen(false, ImGuiCond_FirstUseEver);
  if (ImGui::CollapsingHeader("Image Manifest"))
  {
      ImGui::Text("Manifest: %s", m_manualTest.manifest_path.c_str());
      ImGui::Text("Status: %s", m_manualTest.manifest_loaded ? "LOADED" : "NOT_LOADED");
      if (!m_manualTest.manifest_load_reason.empty())
          ImGui::Text("Reason: %s", m_manualTest.manifest_load_reason.c_str());

      if (!m_manualTest.image_manifest_entries.empty())
      {
          for (const auto& imageId : m_manualTest.image_manifest_entries)
          {
              ImGui::PushID(imageId.c_str());
              if (ImGui::Selectable(imageId.c_str()))
              {
                  m_manualTest.current_gauge.image_id = imageId;
              }
              ImGui::PopID();
          }
      }
      else
      {
          ImGui::TextDisabled("No images loaded.");
      }
  }

  ImGui::Separator();
  ImGui::SetNextItemOpen(false, ImGuiCond_FirstUseEver);
  if (ImGui::CollapsingHeader("CxScript Catalog Review"))
  {
    ImGui::Text("Catalog Path: %s", m_manualTest.catalog_path.c_str());
    ImGui::Text("Loaded: %s", m_manualTest.catalog_loaded ? "yes" : "no");
    ImGui::Text("Total entries: %d", static_cast<int>(m_manualTest.catalog_entries.size()));

    if (!m_manualTest.catalog_entries.empty())
    {
      ImGui::Separator();
      ImGui::Text("Manual-visible scripts (visible in normal list):");
      ImGui::BeginChild("visible_catalog_list", ImVec2(-1, 150), true);
      for (const auto& entry : m_manualTest.catalog_entries)
      {
        bool isVisible = entry.manual_visible && entry.frozen &&
            (entry.expected_result == "ok" || entry.expected_result == "ng_expected");
        if (!isVisible) continue;

        ImGui::Text("[%s] %s", entry.expected_result.c_str(), entry.label.c_str());
        ImGui::Text("  ScriptId: %s", entry.script_id.c_str());
        ImGui::Text("  Tool: %s | Policy: %s", entry.tool.c_str(), entry.parameter_policy_id.c_str());
        if (entry.contract_path.empty())
        {
          ImGui::TextColored(ImVec4(1, 0.2f, 0.2f, 1), "  Contract: (MISSING)");
        }
        else
        {
          ImGui::Text("  Contract: %s", entry.contract_path.c_str());
        }
        ImGui::Text("  Path: %s", entry.path.c_str());
        ImGui::Separator();
      }
      ImGui::EndChild();

      ImGui::Separator();
      ImGui::Text("Hidden scripts (with reason):");
      ImGui::BeginChild("hidden_catalog_list", ImVec2(-1, 150), true);
      for (const auto& entry : m_manualTest.catalog_entries)
      {
        bool isVisible = entry.manual_visible && entry.frozen &&
            (entry.expected_result == "ok" || entry.expected_result == "ng_expected");
        if (isVisible) continue;

        std::string reason;
        if (!entry.manual_visible)
          reason = "manual_visible=0";
        else if (!entry.frozen)
          reason = "frozen=0";
        else if (entry.expected_result != "ok" && entry.expected_result != "ng_expected")
          reason = "expected_result is not ok/ng_expected";
        else if (entry.path.find("/diagnostic/") != std::string::npos)
          reason = "diagnostic path hidden from normal list";
        else if (entry.path.find("/draft/") != std::string::npos)
          reason = "draft path hidden";
        else if (entry.path.find("/deprecated/") != std::string::npos)
          reason = "deprecated path hidden";
        else
          reason = "other";

        ImGui::Text("[%s] %s", reason.c_str(), entry.label.c_str());
        ImGui::Text("  ScriptId: %s", entry.script_id.c_str());
        ImGui::Text("  Tool: %s | Expected: %s", entry.tool.c_str(), entry.expected_result.c_str());
        ImGui::Text("  Path: %s", entry.path.c_str());
        ImGui::Separator();
      }
      ImGui::EndChild();
    }
    else
    {
      ImGui::TextDisabled("No catalog entries loaded.");
    }
  }

  ImGui::Separator();
  ImGui::SetNextItemOpen(false, ImGuiCond_FirstUseEver);
  if (ImGui::CollapsingHeader("Advanced Diagnostic Scripts"))
  {
    bool hasAdvanced = false;
    for (const auto& entry : m_manualTest.catalog_entries)
    {
      if (entry.advanced_visible && entry.expected_result == "diagnostic")
      {
        hasAdvanced = true;
        break;
      }
    }

    if (hasAdvanced)
    {
      for (const auto& entry : m_manualTest.catalog_entries)
      {
        if (!entry.advanced_visible || entry.expected_result != "diagnostic")
          continue;

        ImGui::PushID(entry.script_id.c_str());
        if (ImGui::Selectable(entry.label.c_str()))
        {
          std::string text;
          if (ReadTextFile(ResolveWorkspaceFile(entry.path).generic_string(), text))
          {
            m_manualTest.editor_text = text;
            m_manualTest.editor_source = "diagnostic_script";
            m_manualTest.loaded_script_path = entry.path;
            m_manualTest.editor_dirty = false;
            m_manualTest.analyzed_text.clear();
            m_manualTest.current_line = 0;
          }
        }
        ImGui::TextWrapped("Tool: %s | Contract: %s", entry.tool.c_str(),
            entry.contract_path.empty() ? "(none)" : entry.contract_path.c_str());
        ImGui::PopID();
      }
    }
    else
    {
      ImGui::TextDisabled("No advanced diagnostic scripts available.");
    }
  }

  ImGui::Separator();
  ImGui::SetNextItemOpen(false, ImGuiCond_FirstUseEver);
  if (ImGui::CollapsingHeader("Legacy Recursive Scan Debug"))
  {
    static bool showLegacyRecursiveScanDebug = false;
    ImGui::Checkbox(
        "Enable legacy recursive script scan",
        &showLegacyRecursiveScanDebug);

    if (showLegacyRecursiveScanDebug)
    {
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
    }
    else
    {
      ImGui::TextDisabled("Legacy recursive scan is disabled.");
      ImGui::TextDisabled("Normal Script Catalog uses cximage_catalog.cxsc as single source.");
    }
  }

  AnalyzeScript(m_manualTest);

  if (m_manualTest.current_line >= static_cast<int>(m_manualTest.line_views.size()))
    m_manualTest.current_line = m_manualTest.line_views.empty() ? 0 :
      static_cast<int>(m_manualTest.line_views.size()) - 1;

  ImGui::Separator();
  ImGui::SetNextItemOpen(false, ImGuiCond_FirstUseEver);
  if (ImGui::CollapsingHeader("Current Flow Block"))
  {
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
  }


  ImGui::Separator();
  ImGui::SetNextItemOpen(false, ImGuiCond_FirstUseEver);
  if (ImGui::CollapsingHeader("Last Debug Result##manual_console"))
  {
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

  }

  if (m_manualTest.current_line >= static_cast<int>(m_manualTest.line_views.size()))
    m_manualTest.current_line = m_manualTest.line_views.empty() ? 0 :
      static_cast<int>(m_manualTest.line_views.size()) - 1;
  ImGui::Separator();
  DrawCxParserExtLineViewsPanel(m_manualTest);
  DrawCxParserExtStatementViewsPanel(m_manualTest);
  DrawCxParserExtObjectAssignmentsPanel(m_manualTest);

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
  ImGui::SetNextItemOpen(false, ImGuiCond_FirstUseEver);
  if (ImGui::CollapsingHeader("Advanced Debug Panels"))
  {
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
  for (RuntimeObjectView& object : m_manualTest.runtime_objects)
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
    else if (object.type == "Findline")
    {
      ImGui::Text("fit_mode=%s | fit_status=%s",
                  object.line_fit_mode.empty() ? "not_selected" : object.line_fit_mode.c_str(),
                  object.line_fit_status.empty() ? "not_executed" : object.line_fit_status.c_str());
      ImGui::Text("points=%d | valid_points=%d | avgdist=%.3f",
                  object.line_measure_points_count,
                  object.valid_line_points_count,
                  object.line_avgdist);
      ImGui::Text("filter_profile=%d | raw_filter_min=%d | effective_filter_min=%d",
                  object.line_measure_filter_profile,
                  object.line_measure_filter_min,
                  object.line_measure_effective_filter_min);
      ImGui::Text("cc_selected total=%d accepted=%d p90=%.3f",
                  object.line_measure_cc_selected_total,
                  object.line_measure_cc_selected_accepted,
                  object.line_measure_cc_selected_area_p90);
      if (!object.line_measure_hint.empty())
        ImGui::TextWrapped("measure_hint: %s", object.line_measure_hint.c_str());
      if (object.has_fit_line)
        ImGui::Text("fit=(%.3f, %.3f)->(%.3f, %.3f)",
                    object.fit_line_x0, object.fit_line_y0,
                    object.fit_line_x1, object.fit_line_y1);
      ImGui::PushID(object.name.c_str());
      ImGui::SeparatorText("Findline Params");
      int editWgap = object.line_tool_wgap > 0 ?
        object.line_tool_wgap : std::max(1, object.line_measure_wgap);
      int editHgap = object.line_tool_hgap > 0 ?
        object.line_tool_hgap : std::max(1, object.line_measure_hgap);
      ImGui::InputInt("wgap##findline", &editWgap);
      ImGui::InputInt("hgap##findline", &editHgap);
      if (ImGui::Button("Apply WHgap##findline"))
      {
        std::string updateReason;
        const bool updated = ApplyRuntimeFindlineWHgap(
            m_manualTest,
            object.name,
            editWgap,
            editHgap,
            0,
            "manual_console",
            updateReason);
        if (!updated)
        {
          object.last_runtime_status = "BLOCKED";
          object.runtime_state = "line_param_update_failed";
          object.display_summary = updateReason;
        }
        m_scriptResult.status = updated ? "PENDING" : "BLOCKED";
        m_scriptResult.reason = updateReason;
      }
      ImGui::PopID();
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
      RefreshSnapshotFromCurrentResultRef(m_manualTest);
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

  }

  ImGui::End();
}

bool UpdateRuntimeFindlineSetlineFromUi(
    ManualTestContext& context,
    const std::string& objectName,
    float x0,
    float y0,
    float x1,
    float y1,
    float scale,
    std::string& outReason)
{
    outReason.clear();

    DebugCximageRuntime& runtime = CxRuntime(context);

    auto it = runtime.lines.find(objectName);
    if (it == runtime.lines.end() || !it->second)
    {
        outReason = "Findline runtime object not found: " + objectName;
        return false;
    }

    const int ix0 = static_cast<int>(std::lround(x0));
    const int iy0 = static_cast<int>(std::lround(y0));
    const int ix1 = static_cast<int>(std::lround(x1));
    const int iy1 = static_cast<int>(std::lround(y1));
    const int iscale = static_cast<int>(std::max(1.0f, scale));

    it->second->setline(ix0, iy0, ix1, iy1, iscale);

    RuntimeObjectView* object =
        FindRuntimeObjectByName(context, objectName);

    if (object == nullptr)
    {
        outReason = "RuntimeObjectView not found: " + objectName;
        return false;
    }

    object->exists_in_parser = true;
    object->type = "Findline";
    object->last_method = "ui_drag_setline";
    object->last_runtime_status = "runtime_executed";
    object->runtime_state = "runtime_param_set";
    object->visualizable = true;
    object->visual_source = "runtime_object";
    object->stale = false;

    RefreshFindlineDisplaySnapshot(context, *object, *it->second);

    std::ostringstream ss;
    ss << "Findline UI drag updated setline"
       << " | line_roi=("
       << object->line_x0 << "," << object->line_y0
       << ")->("
       << object->line_x1 << "," << object->line_y1
       << ")"
       << " | scan_half_width=" << object->line_scan_half_width
       << " | source=" << object->line_display_source;

    object->display_summary = ss.str();
    outReason = object->display_summary;

    return true;
}

static bool SaveCxScriptOverlayImage(
    const ManualTestContext& context,
    const cv::Mat& sourceImage,
    const fs::path& outputPath,
    std::string& outReason)
{
    try
    {
        cv::Mat canvas;
        sourceImage.copyTo(canvas);

        if (canvas.channels() == 1)
        {
            cv::cvtColor(canvas, canvas, cv::COLOR_GRAY2BGR);
        }

        for (std::size_t idx = 0; idx < context.runtime_objects.size(); ++idx)
        {
            const RuntimeObjectView& object = context.runtime_objects[idx];

            if (object.type == "Findline")
            {
                if (object.has_line_roi)
                {
                    cv::line(
                        canvas,
                        cv::Point((int)object.line_x0, (int)object.line_y0),
                        cv::Point((int)object.line_x1, (int)object.line_y1),
                        cv::Scalar(0, 255, 0),
                        2);
                }

                if (object.has_line_scan_box)
                {
                    std::vector<cv::Point> box;
                    for (std::size_t i = 0; i + 1 < object.line_scan_box_xy.size(); i += 2)
                    {
                        box.emplace_back(
                            (int)object.line_scan_box_xy[i],
                            (int)object.line_scan_box_xy[i + 1]);
                    }

                    if (box.size() >= 4)
                    {
                        cv::polylines(
                            canvas,
                            box,
                            true,
                            cv::Scalar(0, 180, 0),
                            1);
                    }
                }

                for (std::size_t i = 0; i + 1 < object.line_measure_points_xy.size(); i += 2)
                {
                    cv::circle(
                        canvas,
                        cv::Point(
                            (int)object.line_measure_points_xy[i],
                            (int)object.line_measure_points_xy[i + 1]),
                        3,
                        cv::Scalar(0, 0, 255),
                        -1);
                }

                if (object.has_fit_line)
                {
                    cv::line(
                        canvas,
                        cv::Point((int)object.fit_line_x0, (int)object.fit_line_y0),
                        cv::Point((int)object.fit_line_x1, (int)object.fit_line_y1),
                        cv::Scalar(0, 255, 255),
                        2);
                }

                cv::putText(
                    canvas,
                    object.name + " | valid=" + std::to_string(object.valid_line_points_count) + " | fit=" + (object.has_fit_line ? "true" : "false"),
                    cv::Point(20, 30 + (int)idx * 30),
                    cv::FONT_HERSHEY_SIMPLEX,
                    0.6,
                    cv::Scalar(0, 255, 255),
                    2);
            }
            else if (object.type == "Findcircle")
            {
                if (object.has_circle)
                {
                    cv::circle(
                        canvas,
                        cv::Point((int)object.circle_cx, (int)object.circle_cy),
                        (int)object.circle_radius,
                        cv::Scalar(0, 255, 0),
                        2);

                    cv::circle(
                        canvas,
                        cv::Point((int)object.circle_cx, (int)object.circle_cy),
                        (int)object.circle_inner,
                        cv::Scalar(0, 180, 0),
                        1);
                }

                if (object.has_measure_points)
                {
                    for (std::size_t i = 0; i + 1 < object.measure_points_xy.size(); i += 2)
                    {
                        cv::circle(
                            canvas,
                            cv::Point(
                                (int)object.measure_points_xy[i],
                                (int)object.measure_points_xy[i + 1]),
                            3,
                            cv::Scalar(0, 0, 255),
                            -1);
                    }
                }

                if (object.has_fit_result)
                {
                    cv::circle(
                        canvas,
                        cv::Point((int)object.fit_cx, (int)object.fit_cy),
                        (int)object.fit_radius,
                        cv::Scalar(0, 255, 255),
                        2);
                }

                cv::putText(
                    canvas,
                    object.name + " | valid=" + std::to_string(object.valid_points_count) + " | fit=" + (object.has_fit_result ? "true" : "false"),
                    cv::Point(20, 30 + (int)idx * 30),
                    cv::FONT_HERSHEY_SIMPLEX,
                    0.6,
                    cv::Scalar(0, 255, 255),
                    2);
            }
        }

        fs::create_directories(outputPath.parent_path());

        if (!cv::imwrite(outputPath.string(), canvas))
        {
            outReason = "failed to write overlay image: " + outputPath.string();
            return false;
        }

        return true;
    }
    catch (const std::exception& e)
    {
        outReason = "SaveCxScriptOverlayImage exception: " + std::string(e.what());
        return false;
    }
}

static bool SaveCxScriptHeadlessSummaryJson(
    const ManualTestContext& context,
    const CxScriptHeadlessOptions& options,
    const CxScriptHeadlessResult& result,
    const fs::path& outputPath,
    std::string& outReason)
{
    try
    {
        fs::create_directories(outputPath.parent_path());

        std::ofstream file(outputPath.string(), std::ios::out | std::ios::trunc);

        if (!file.is_open())
        {
            outReason = "failed to open summary json file: " + outputPath.string();
            return false;
        }

        file << "{\n";
        file << "  \"ok\": " << (result.ok ? "true" : "false") << ",\n";
        file << "  \"case_name\": \"" << CxDebugJsonEscape(options.case_name) << "\",\n";
        file << "  \"image_path\": \"" << CxDebugJsonEscape(options.image_path) << "\",\n";
        file << "  \"script_path\": \"" << CxDebugJsonEscape(options.script_path) << "\",\n";
        file << "  \"run_state\": \"" << CxDebugJsonEscape(context.run_state) << "\",\n";
        file << "  \"debug_status\": \"" << CxDebugJsonEscape(context.debug_status) << "\",\n";
        file << "  \"debug_reason\": \"" << CxDebugJsonEscape(context.debug_reason) << "\",\n";
        file << "  \"snapshot_path\": \"" << CxDebugJsonEscape(result.snapshot_path) << "\",\n";
        file << "  \"overlay_path\": \"" << CxDebugJsonEscape(result.overlay_path) << "\",\n";

        const bool hasFitLine =
            context.current_result_ref.result_type == "FindlineResult" &&
            context.current_result_ref.status == "geometry_result_available";
        const bool hasFitCircle =
            context.current_result_ref.result_type == "FindcircleResult" &&
            context.current_result_ref.status == "geometry_result_available";
        std::string policyGuard = "GEOMETRY_RESULT_UNAVAILABLE";
        if (hasFitLine)
            policyGuard = "MEASURE_AND_FIT_AVAILABLE";
        else if (hasFitCircle)
            policyGuard = "CIRCLE_MEASURE_AND_FIT_AVAILABLE";
        if (options.contract_context_enabled && !options.policy_guard.empty())
            policyGuard = options.policy_guard;

        const int summaryPointsCount = options.contract_context_enabled
            ? options.points_count
            : context.current_result_ref.points_count;
        const int summaryValidPointsCount = options.contract_context_enabled
            ? options.valid_points_count
            : context.current_result_ref.valid_points_count;
        const bool summaryHasFitLine = options.contract_context_enabled
            ? options.has_fit_line != 0
            : hasFitLine;
        const bool summaryHasFitCircle = options.contract_context_enabled
            ? options.has_fit_circle != 0
            : hasFitCircle;
        const double summaryCircleRadius = options.contract_context_enabled
            ? options.circle_radius
            : context.current_result_ref.fit_radius;
        const double summaryAvgdist = options.contract_context_enabled
            ? options.avgdist
            : context.current_result_ref.avgdist;
        const double summaryLocalSupport = options.contract_context_enabled
            ? options.local_support
            : 0.0;
        const double summaryLocalMeanDistance = options.contract_context_enabled
            ? options.local_mean_distance
            : context.current_result_ref.line_avgdist;
        const double summaryFitOffset = options.contract_context_enabled
            ? options.fit_offset
            : 0.0;
        const std::string summaryResultStatus = options.contract_context_enabled
            ? options.result_status
            : context.current_result_ref.status;
        const std::string summaryFailureStage = options.contract_context_enabled
            ? options.failure_stage
            : context.current_result_ref.line_measure_failure_hint;

        std::string contractPass = "0";
        std::string contractStatus;
        std::string contractConclusion;
        const auto contractPassIt = context.runtime_int_vars.find("global.contract_pass");
        if (contractPassIt != context.runtime_int_vars.end())
            contractPass = std::to_string(contractPassIt->second);
        ReadRuntimeVariableValue(context, "global.contract_status", contractStatus);
        ReadRuntimeVariableValue(context, "global.contract_conclusion", contractConclusion);

        file << "  \"tool\": \"" << CxDebugJsonEscape(options.stage25_tool) << "\",\n";
        file << "  \"stage25_image_id\": \"" << CxDebugJsonEscape(options.stage25_image_id) << "\",\n";
        file << "  \"stage25_target_id\": \"" << CxDebugJsonEscape(options.stage25_target_id) << "\",\n";
        file << "  \"stage25_level\": \"" << CxDebugJsonEscape(options.stage25_level) << "\",\n";
        file << "  \"roi_injected\": " << ((options.circle_cx != 0 || options.circle_cy != 0 || options.roi_x0 != 0 || options.roi_y0 != 0) ? "true" : "false") << ",\n";
        file << "  \"points_count\": " << summaryPointsCount << ",\n";
        file << "  \"valid_points_count\": " << summaryValidPointsCount << ",\n";
        file << "  \"has_fit_line\": " << (summaryHasFitLine ? "true" : "false") << ",\n";
        file << "  \"has_fit_circle\": " << (summaryHasFitCircle ? "true" : "false") << ",\n";
        file << "  \"circle_radius\": " << summaryCircleRadius << ",\n";
        file << "  \"avgdist\": " << summaryAvgdist << ",\n";
        file << "  \"local_support\": " << summaryLocalSupport << ",\n";
        file << "  \"local_mean_distance\": " << summaryLocalMeanDistance << ",\n";
        file << "  \"fit_offset\": " << summaryFitOffset << ",\n";
        file << "  \"policy_guard\": \"" << CxDebugJsonEscape(policyGuard) << "\",\n";
        file << "  \"result_status\": \"" << CxDebugJsonEscape(summaryResultStatus) << "\",\n";
        file << "  \"failure_stage\": \"" << CxDebugJsonEscape(summaryFailureStage) << "\",\n";
        file << "  \"result_overlay_path\": \"" << CxDebugJsonEscape(options.contract_context_enabled ? options.result_overlay_path : result.overlay_path) << "\",\n";
        file << "  \"evidence_overlay_path\": \"" << CxDebugJsonEscape(options.evidence_overlay_path) << "\",\n";
        file << "  \"tool_display_path\": \"" << CxDebugJsonEscape(options.tool_display_path) << "\",\n";
        file << "  \"contract_pass\": " << contractPass << ",\n";
        file << "  \"contract_status\": \"" << CxDebugJsonEscape(contractStatus) << "\",\n";
        file << "  \"contract_conclusion\": \"" << CxDebugJsonEscape(contractConclusion) << "\",\n";

        file << "  \"roi_x0\": " << options.roi_x0 << ",\n";
        file << "  \"roi_y0\": " << options.roi_y0 << ",\n";
        file << "  \"roi_x1\": " << options.roi_x1 << ",\n";
        file << "  \"roi_y1\": " << options.roi_y1 << ",\n";
        file << "  \"circle_cx\": " << options.circle_cx << ",\n";
        file << "  \"circle_cy\": " << options.circle_cy << ",\n";
        file << "  \"circle_px\": " << options.circle_px << ",\n";
        file << "  \"circle_py\": " << options.circle_py << ",\n";

        file << "  \"fit_line_x0\": " << context.current_result_ref.line_x0 << ",\n";
        file << "  \"fit_line_y0\": " << context.current_result_ref.line_y0 << ",\n";
        file << "  \"fit_line_x1\": " << context.current_result_ref.line_x1 << ",\n";
        file << "  \"fit_line_y1\": " << context.current_result_ref.line_y1 << ",\n";
        file << "  \"circle_center_x\": " << context.current_result_ref.fit_cx << ",\n";
        file << "  \"circle_center_y\": " << context.current_result_ref.fit_cy << ",\n";

        file << "  \"measure_points_xy\": [";
        bool firstPoint = true;
        for (const auto& object : context.runtime_objects)
        {
            if (object.type == "Findline" && !object.line_measure_points_xy.empty())
            {
                for (size_t i = 0; i + 1 < object.line_measure_points_xy.size(); i += 2)
                {
                    if (!firstPoint) file << ", ";
                    file << "[" << object.line_measure_points_xy[i] << "," << object.line_measure_points_xy[i + 1] << "]";
                    firstPoint = false;
                }
            }
            else if (object.type == "Findcircle" && !object.measure_points_xy.empty())
            {
                for (size_t i = 0; i + 1 < object.measure_points_xy.size(); i += 2)
                {
                    if (!firstPoint) file << ", ";
                    file << "[" << object.measure_points_xy[i] << "," << object.measure_points_xy[i + 1] << "]";
                    firstPoint = false;
                }
            }
        }
        file << "],\n";

        file << "  \"current_result_ref\": {\n";
        file << "    \"name\": \"" << CxDebugJsonEscape(context.current_result_ref.name) << "\",\n";
        file << "    \"source_object\": \"" << CxDebugJsonEscape(context.current_result_ref.source_object) << "\",\n";
        file << "    \"result_type\": \"" << CxDebugJsonEscape(context.current_result_ref.result_type) << "\",\n";
        file << "    \"status\": \"" << CxDebugJsonEscape(context.current_result_ref.status) << "\",\n";
        file << "    \"reason\": \"" << CxDebugJsonEscape(context.current_result_ref.reason) << "\",\n";
        file << "    \"line_measure_hint\": \"" << CxDebugJsonEscape(context.current_result_ref.line_measure_hint) << "\",\n";
        file << "    \"line_filter_min_exceeds_component_p90\": " << (context.current_result_ref.line_filter_min_exceeds_component_p90 ? "true" : "false") << ",\n";
        file << "    \"line_measure_failure_hint\": \"" << CxDebugJsonEscape(context.current_result_ref.line_measure_failure_hint) << "\",\n";
        file << "    \"points_count\": " << context.current_result_ref.points_count << ",\n";
        file << "    \"valid_points_count\": " << context.current_result_ref.valid_points_count << ",\n";
        file << "    \"has_fit_line\": " << (context.current_result_ref.result_type == "FindlineResult" && context.current_result_ref.status == "geometry_result_available" ? "true" : "false") << ",\n";
        file << "    \"has_fit_circle\": " << (context.current_result_ref.result_type == "FindcircleResult" && context.current_result_ref.status == "geometry_result_available" ? "true" : "false") << ",\n";
        file << "    \"circle_radius\": " << context.current_result_ref.fit_radius << ",\n";
        file << "    \"avgdist\": " << context.current_result_ref.avgdist << "\n";
        file << "  },\n";

        file << "  \"runtime_objects\": [\n";

        for (std::size_t i = 0; i < context.runtime_objects.size(); ++i)
        {
            const RuntimeObjectView& object = context.runtime_objects[i];

            file << "    {\n";
            file << "      \"name\": \"" << CxDebugJsonEscape(object.name) << "\",\n";
            file << "      \"type\": \"" << CxDebugJsonEscape(object.type) << "\",\n";
            file << "      \"runtime_state\": \"" << CxDebugJsonEscape(object.runtime_state) << "\"";

            if (object.type == "Findline")
            {
                file << ",\n";
                file << "      \"line_roi\": [" << object.line_x0 << "," << object.line_y0 << "," << object.line_x1 << "," << object.line_y1 << "],\n";
                file << "      \"has_line_scan_box\": " << (object.has_line_scan_box ? "true" : "false") << ",\n";
                file << "      \"line_scan_half_width\": " << object.line_scan_half_width << ",\n";
                file << "      \"line_orientation\": \"" << CxDebugJsonEscape(object.line_orientation) << "\",\n";
                file << "      \"line_dx\": " << object.line_dx << ",\n";
                file << "      \"line_dy\": " << object.line_dy << ",\n";
                file << "      \"line_length\": " << object.line_length << ",\n";
                file << "      \"requested_tool_half_width\": " << object.requested_tool_half_width << ",\n";
                file << "      \"effective_tool_half_width\": " << object.effective_tool_half_width << ",\n";
                file << "      \"valid_line_points_count\": " << object.valid_line_points_count << ",\n";
                file << "      \"has_fit_line\": " << (object.has_fit_line ? "true" : "false") << ",\n";
                file << "      \"line_avgdist\": " << object.line_avgdist << ",\n";
                file << "      \"line_measure_method\": " << object.line_measure_method << ",\n";
                file << "      \"line_measure_threshold\": " << object.line_measure_threshold << ",\n";
                file << "      \"line_measure_linegap\": " << object.line_measure_linegap << ",\n";
                file << "      \"line_measure_wgap\": " << object.line_measure_wgap << ",\n";
                file << "      \"line_measure_hgap\": " << object.line_measure_hgap << ",\n";
                file << "      \"line_measure_max_gradient\": " << object.line_measure_max_gradient << ",\n";
                file << "      \"line_measure_binary_foreground_pixels\": " << object.line_measure_binary_foreground_pixels << ",\n";
                file << "      \"line_measure_findobject_called\": " << (object.line_measure_findobject_called ? "true" : "false") << ",\n";
                file << "      \"line_measure_filter_min\": " << object.line_measure_filter_min << ",\n";
                file << "      \"line_measure_filter_max\": " << object.line_measure_filter_max << ",\n";
                file << "      \"line_measure_filter_profile\": " << object.line_measure_filter_profile << ",\n";
                file << "      \"line_measure_effective_filter_borw\": " << object.line_measure_effective_filter_borw << ",\n";
                file << "      \"line_measure_effective_filter_min\": " << object.line_measure_effective_filter_min << ",\n";
                file << "      \"line_measure_effective_filter_max\": " << object.line_measure_effective_filter_max << ",\n";
                file << "      \"line_measure_source\": \"" << CxDebugJsonEscape(object.line_measure_source) << "\",\n";
                file << "      \"line_measure_failure_stage\": \"" << CxDebugJsonEscape(object.line_measure_failure_stage) << "\",\n";
                file << "      \"line_measure_fallback_used\": " << (object.line_measure_fallback_used ? "true" : "false") << ",\n";
                file << "      \"line_measure_status\": \"" << CxDebugJsonEscape(object.line_measure_status) << "\",\n";
                file << "      \"line_measure_hint\": \"" << CxDebugJsonEscape(object.line_measure_hint) << "\",\n";
                file << "      \"line_filter_min_exceeds_component_p90\": " << (object.line_filter_min_exceeds_component_p90 ? "true" : "false") << ",\n";
                file << "      \"line_measure_failure_hint\": \"" << CxDebugJsonEscape(object.line_measure_failure_hint) << "\",\n";
                file << "      \"line_fit_status\": \"" << CxDebugJsonEscape(object.line_fit_status) << "\",\n";
                file << "      \"line_findobject_component_total\": " << object.line_findobject_component_total << ",\n";
                file << "      \"line_findobject_component_accepted\": " << object.line_findobject_component_accepted << ",\n";
                file << "      \"line_findobject_component_rejected_by_min\": " << object.line_findobject_component_rejected_by_min << ",\n";
                file << "      \"line_findobject_component_rejected_by_max\": " << object.line_findobject_component_rejected_by_max << ",\n";
                file << "      \"line_findobject_area_mean_observed\": " << object.line_findobject_area_mean_observed << ",\n";
                file << "      \"line_findobject_area_min\": " << object.line_findobject_area_min << ",\n";
                file << "      \"line_findobject_area_max\": " << object.line_findobject_area_max << ",\n";
                file << "      \"line_findobject_area_median\": " << object.line_findobject_area_median << ",\n";
                file << "      \"line_findobject_area_p90\": " << object.line_findobject_area_p90 << ",\n";
                file << "      \"line_measure_cc_selected_foreground\": \"" << CxDebugJsonEscape(object.line_measure_cc_selected_foreground) << "\",\n";
                file << "      \"line_measure_cc_white_total\": " << object.line_measure_cc_white_total << ",\n";
                file << "      \"line_measure_cc_white_accepted\": " << object.line_measure_cc_white_accepted << ",\n";
                file << "      \"line_measure_cc_white_rejected_min\": " << object.line_measure_cc_white_rejected_min << ",\n";
                file << "      \"line_measure_cc_white_area_median\": " << object.line_measure_cc_white_area_median << ",\n";
                file << "      \"line_measure_cc_white_area_p90\": " << object.line_measure_cc_white_area_p90 << ",\n";
                file << "      \"line_measure_cc_black_total\": " << object.line_measure_cc_black_total << ",\n";
                file << "      \"line_measure_cc_black_accepted\": " << object.line_measure_cc_black_accepted << ",\n";
                file << "      \"line_measure_cc_black_rejected_min\": " << object.line_measure_cc_black_rejected_min << ",\n";
                file << "      \"line_measure_cc_black_area_median\": " << object.line_measure_cc_black_area_median << ",\n";
                file << "      \"line_measure_cc_black_area_p90\": " << object.line_measure_cc_black_area_p90 << ",\n";
                file << "      \"line_measure_cc_selected_total\": " << object.line_measure_cc_selected_total << ",\n";
                file << "      \"line_measure_cc_selected_accepted\": " << object.line_measure_cc_selected_accepted << ",\n";
                file << "      \"line_measure_cc_selected_rejected_min\": " << object.line_measure_cc_selected_rejected_min << ",\n";
                file << "      \"line_measure_cc_selected_area_median\": " << object.line_measure_cc_selected_area_median << ",\n";
                file << "      \"line_measure_cc_selected_area_p90\": " << object.line_measure_cc_selected_area_p90;
            }
            else if (object.type == "Findcircle")
            {
                file << ",\n";
                file << "      \"circle_roi\": [" << object.circle_cx << "," << object.circle_cy << "," << object.circle_inner << "," << object.circle_radius << "],\n";
                file << "      \"has_circle\": " << (object.has_circle ? "true" : "false") << ",\n";
                file << "      \"measure_points_count\": " << object.measure_points_count << ",\n";
                file << "      \"valid_points_count\": " << object.valid_points_count << ",\n";
                file << "      \"has_fit_result\": " << (object.has_fit_result ? "true" : "false") << ",\n";
                file << "      \"fit_circle_center_x\": " << object.fit_cx << ",\n";
                file << "      \"fit_circle_center_y\": " << object.fit_cy << ",\n";
                file << "      \"fit_circle_radius\": " << object.fit_radius << ",\n";
                file << "      \"circle_avgdist\": " << object.fit_avgdist << ",\n";
                file << "      \"has_result_measure\": " << (object.has_result_measure ? "true" : "false") << ",\n";
                file << "      \"circle_measure_source\": \"" << CxDebugJsonEscape(object.circle_measure_source) << "\",\n";
                file << "      \"circle_measure_failure_stage\": \"" << CxDebugJsonEscape(object.circle_measure_failure_stage) << "\",\n";
                file << "      \"circle_scan_lines_processed\": " << object.circle_scan_lines_processed << ",\n";
                file << "      \"circle_total_samples\": " << object.circle_total_samples << ",\n";
                file << "      \"circle_elapsed_ms\": " << object.circle_elapsed_ms << ",\n";
                file << "      \"circle_budget_max_scan_lines\": " << object.circle_budget_max_scan_lines << ",\n";
                file << "      \"circle_budget_max_samples\": " << object.circle_budget_max_samples << ",\n";
                file << "      \"circle_budget_max_elapsed_ms\": " << object.circle_budget_max_elapsed_ms;
            }

            file << "\n    }";

            if (i < context.runtime_objects.size() - 1)
            {
                file << ",";
            }

            file << "\n";
        }

        file << "  ]\n";
        file << "}\n";

        return true;
    }
    catch (const std::exception& e)
    {
        outReason = "SaveCxScriptHeadlessSummaryJson exception: " + std::string(e.what());
        return false;
    }
}

bool ParseCxScriptHeadlessArgs(int argc, char** argv, CxScriptHeadlessOptions& options)
{
    options.enabled = false;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];

        if (arg == "--cxscript-headless")
        {
            options.enabled = true;
        }
        else if (arg == "--image" && i + 1 < argc)
        {
            options.image_path = argv[++i];
        }
        else if (arg == "--script" && i + 1 < argc)
        {
            options.script_path = argv[++i];
        }
        else if (arg == "--out" && i + 1 < argc)
        {
            options.output_dir = argv[++i];
        }
        else if (arg == "--case-name" && i + 1 < argc)
        {
            options.case_name = argv[++i];
        }
        else if (arg == "--snapshot" && i + 1 < argc)
        {
            options.snapshot_path = argv[++i];
        }
        else if (arg == "--overlay" && i + 1 < argc)
        {
            options.overlay_path = argv[++i];
        }
        else if (arg == "--summary" && i + 1 < argc)
        {
            options.summary_path = argv[++i];
        }
        else if (arg == "--max-steps" && i + 1 < argc)
        {
            try
            {
                options.max_steps = std::stoi(argv[++i]);
            }
            catch (...)
            {
                options.max_steps = 10000;
            }
        }
        else if (arg == "--no-overlay")
        {
            options.save_overlay = false;
        }
    }

    return true;
}

bool RunCxScriptHeadless(
    const CxScriptHeadlessOptions& options,
    CxScriptHeadlessResult& result)
{
    struct HeadlessScopeSentinel
    {
        ~HeadlessScopeSentinel()
        {
            std::cout << "[DEBUG HEADLESS] function scope destroyed\n" << std::flush;
        }
    } headlessScopeSentinel;

    std::cout << "[DEBUG HEADLESS] function begin\n" << std::flush;

    result.ok = false;
    result.exit_code = 1;

    if (!options.enabled)
    {
        result.reason = "headless mode not enabled";
        return false;
    }

    if (options.image_path.empty())
    {
        result.reason = "image path is empty";
        result.exit_code = 3;
        return false;
    }

    if (options.script_path.empty())
    {
        result.reason = "script path is empty";
        result.exit_code = 3;
        return false;
    }

    if (options.output_dir.empty())
    {
        result.reason = "output directory is empty";
        result.exit_code = 3;
        return false;
    }

    try
    {
        fs::create_directories(options.output_dir);
    }
    catch (...)
    {
        result.reason = "failed to create output directory: " + options.output_dir;
        result.exit_code = 3;
        return false;
    }

    cv::Mat mat = cv::imread(options.image_path, cv::IMREAD_COLOR);

    if (mat.empty())
    {
        result.reason = "failed to load image: " + options.image_path;
        result.exit_code = 3;
        return false;
    }

    const fs::path resolvedScriptPath = ResolveWorkspaceFile(options.script_path);
    std::ifstream scriptFile(resolvedScriptPath);

    if (!scriptFile.is_open())
    {
        result.reason = "failed to open script: " + options.script_path + " (resolved: " + resolvedScriptPath.generic_string() + ")";
        result.exit_code = 3;
        return false;
    }

    std::string scriptText((std::istreambuf_iterator<char>(scriptFile)),
                           std::istreambuf_iterator<char>());

    static ManualTestContext context;
    context.editor_text = scriptText;
    context.loaded_script_path = options.script_path;
    context.script_file_path = options.script_path;
    context.editor_source = "headless";
    context.active_script_case_name = options.case_name.empty()
        ? fs::path(options.script_path).stem().string()
        : options.case_name;
    context.active_script_case_path = options.script_path;
    context.active_script_case_purpose = "headless cxscript image test";

    for (auto& var : context.global_variable_views)
    {
        if (var.name == "global.matInput")
        {
            var.image_path = options.image_path;
            break;
        }
    }

    AnalyzeScript(context);

    ResetDebugRuntimeForReplay(context);

    const auto injectString = [&](const std::string& name, const std::string& value)
    {
        UpsertGlobalVariableView(context, "string", name, value, 0, "runtime_initialized");
        UpsertVariableView(context, "string", name, value, 0, "runtime_initialized");
    };

    const auto injectInt = [&](const std::string& name, int value)
    {
        context.runtime_int_vars[name] = value;
        UpsertGlobalVariableView(context, "int", name, std::to_string(value), 0, "runtime_initialized");
        UpsertVariableView(context, "int", name, std::to_string(value), 0, "runtime_initialized");
    };

    const auto injectDouble = [&](const std::string& name, double value)
    {
        std::ostringstream valueStream;
        valueStream << value;
        UpsertGlobalVariableView(context, "double", name, valueStream.str(), 0, "runtime_initialized");
        UpsertVariableView(context, "double", name, valueStream.str(), 0, "runtime_initialized");
    };

    if (!options.stage25_image_id.empty())
    {
        injectString("global.stage25_image_id", options.stage25_image_id);
    }
    if (!options.stage25_level.empty())
    {
        injectString("global.stage25_level", options.stage25_level);
    }
    if (!options.stage25_target_id.empty())
    {
        injectString("global.stage25_target_id", options.stage25_target_id);
    }
    if (!options.stage25_tool.empty())
    {
        injectString("global.stage25_tool", options.stage25_tool);
    }

    injectInt("global.roi_x0", options.roi_x0);
    injectInt("global.roi_y0", options.roi_y0);
    injectInt("global.roi_x1", options.roi_x1);
    injectInt("global.roi_y1", options.roi_y1);

    injectInt("global.circle_cx", options.circle_cx);
    injectInt("global.circle_cy", options.circle_cy);
    injectInt("global.circle_px", options.circle_px);
    injectInt("global.circle_py", options.circle_py);

    injectInt("global.tool_half_width", options.tool_half_width);
    injectInt("global.wgap", options.wgap);
    injectInt("global.hgap", options.hgap);
    injectInt("global.gap", options.gap);
    injectInt("global.linegap", options.linegap);
    injectInt("global.threshold", options.threshold);
    injectInt("global.method", options.method);
    injectInt("global.filterprofile", options.filterprofile);

    if (options.contract_context_enabled)
    {
        injectInt("global.headless_ok", options.contract_headless_ok);
        injectInt("global.contract_pass", options.contract_pass_initial);
        injectInt("global.points_count", options.points_count);
        injectInt("global.valid_points_count", options.valid_points_count);
        injectInt("global.has_fit_line", options.has_fit_line);
        injectInt("global.has_fit_circle", options.has_fit_circle);
        injectDouble("global.local_support", options.local_support);
        injectDouble("global.local_mean_distance", options.local_mean_distance);
        injectDouble("global.fit_offset", options.fit_offset);
        injectDouble("global.circle_radius", options.circle_radius);
        injectDouble("global.avgdist", options.avgdist);
        injectString("global.policy_guard", options.policy_guard);
        injectInt("global.policy_guard_match", options.policy_guard_match);
        injectString("global.result_status", options.result_status);
        injectString("global.failure_stage", options.failure_stage);
        injectString("global.result_overlay_path", options.result_overlay_path);
        injectString("global.evidence_overlay_path", options.evidence_overlay_path);
        injectString("global.tool_display_path", options.tool_display_path);
        injectString("global.contract_status", "");
        injectString("global.contract_conclusion", "");
    }

    context.run_state = "runtime_step";
    context.current_line = 0;

    int steps = 0;
    const auto startTime = std::chrono::steady_clock::now();
    const auto maxDuration = std::chrono::seconds(std::max(1, options.timeout_sec));
    int lastLine = context.current_line;
    std::string lastRunState = context.run_state;
    int noProgressSteps = 0;

    std::cout << "[DEBUG HEADLESS] Starting script execution loop\n";

    while (context.run_state != "runtime_finished" &&
           context.run_state != "finished" &&
           context.run_state != "blocked" &&
           context.current_line < static_cast<int>(context.line_views.size()) &&
           steps < options.max_steps)
    {
        const auto elapsed = std::chrono::steady_clock::now() - startTime;
        if (elapsed > maxDuration)
        {
            result.reason = "execution timeout after " + std::to_string(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count()) + " seconds";
            result.exit_code = 6;
            return false;
        }

        DebugStepOnceWithSnapshot(context);
        ++steps;

        if (context.current_line == lastLine &&
            context.run_state == lastRunState)
        {
            ++noProgressSteps;
        }
        else
        {
            noProgressSteps = 0;
            lastLine = context.current_line;
            lastRunState = context.run_state;
        }

        if (noProgressSteps >= 8)
        {
            result.reason =
                "script execution made no line progress for " +
                std::to_string(noProgressSteps) +
                " steps at line " +
                std::to_string(context.current_line);
            result.exit_code = 7;
            context.run_state = "blocked";
            context.debug_status = "BLOCKED";
            context.debug_reason = result.reason;
            return false;
        }

        if (steps % 10 == 0 || steps < 50)
        {
            std::cout << "[DEBUG HEADLESS] Step " << steps << ", run_state=" << context.run_state 
                      << ", current_line=" << context.current_line 
                      << ", line_views.size=" << context.line_views.size() << "\n";
        }

        if (steps > 0 && steps % 500 == 0)
        {
            std::cout << "[DEBUG HEADLESS] Loop conditions: "
                      << "run_state!=" << context.run_state << " "
                      << "current_line=" << context.current_line << " "
                      << "line_views.size=" << context.line_views.size() << " "
                      << "steps=" << steps << "/" << options.max_steps << "\n";
        }
    }

    std::cout << "[DEBUG HEADLESS] Script execution loop ended, steps=" << steps 
              << ", run_state=" << context.run_state 
              << ", current_line=" << context.current_line
              << ", line_views.size=" << context.line_views.size() << "\n" << std::flush;

    if (steps >= options.max_steps)
    {
        result.reason = "max steps exceeded";
        result.exit_code = 4;
        return false;
    }

    std::cout << "[DEBUG HEADLESS] MarkDebugRunFinishedIfAtEnd begin\n" << std::flush;
    MarkDebugRunFinishedIfAtEnd(context);
    std::cout << "[DEBUG HEADLESS] MarkDebugRunFinishedIfAtEnd end\n" << std::flush;

    std::cout << "[DEBUG HEADLESS] snapshot begin\n" << std::flush;
    fs::path snapshotPath = options.snapshot_path.empty()
        ? fs::path(options.output_dir) / "snapshot.txt"
        : fs::path(options.snapshot_path);

    std::string snapshotReason;

    if (!SaveCxDebugSnapshotText(context, snapshotPath, snapshotReason))
    {
        result.reason = "failed to save snapshot: " + snapshotReason;
        result.exit_code = 5;
        return false;
    }

    result.snapshot_path = snapshotPath.string();
    std::cout << "[DEBUG HEADLESS] snapshot end, path=" << snapshotPath.string() << "\n" << std::flush;

    std::cout << "[DEBUG HEADLESS] overlay begin\n" << std::flush;
    if (options.save_overlay)
    {
        fs::path overlayPath = options.overlay_path.empty()
            ? fs::path(options.output_dir) / "result_overlay.png"
            : fs::path(options.overlay_path);

        std::string overlayReason;

        if (!SaveCxScriptOverlayImage(context, mat, overlayPath, overlayReason))
        {
            result.reason = "failed to save overlay: " + overlayReason;
            result.exit_code = 5;
            return false;
        }

        result.overlay_path = overlayPath.string();
    }
    std::cout << "[DEBUG HEADLESS] overlay end\n" << std::flush;

    std::cout << "[DEBUG HEADLESS] Setting result state\n" << std::flush;
    result.run_state = context.run_state;
    result.debug_status = context.debug_status;
    result.debug_reason = context.debug_reason;
    result.current_result_name = context.current_result_ref.name;
    result.current_result_status = context.current_result_ref.status;
    result.current_result_reason = context.current_result_ref.reason;
    result.ok = true;
    result.exit_code = (context.run_state == "blocked") ? 2 : 0;
    std::cout << "[DEBUG HEADLESS] summary begin\n" << std::flush;
    fs::path summaryPath = options.summary_path.empty()
        ? fs::path(options.output_dir) / "result_summary.json"
        : fs::path(options.summary_path);

    std::string summaryReason;

    if (!SaveCxScriptHeadlessSummaryJson(context, options, result, summaryPath, summaryReason))
    {
        result.reason = "failed to save summary: " + summaryReason;
        result.exit_code = 5;
        return false;
    }

    result.summary_path = summaryPath.string();
    std::cout << "[DEBUG HEADLESS] summary end, path=" << summaryPath.string() << "\n" << std::flush;

    std::cout << "[DEBUG HEADLESS] Evidence analysis: " << (options.enable_evidence_analysis ? "enabled" : "disabled") << "\n" << std::flush;
    if (options.enable_evidence_analysis)
    {
        CxImageEvidenceOptions evidenceOptions;
        evidenceOptions.enabled = true;
        evidenceOptions.profile_half_width = 40;
        evidenceOptions.min_gradient = 6.0;
        evidenceOptions.min_gradient_ratio = 0.35;
        evidenceOptions.max_profiles = 200;
        evidenceOptions.nearest_point_support_px = 3.0;
        evidenceOptions.line_distance_support_px = 3.0;
        evidenceOptions.save_profile_debug = false;

        std::string evidenceReason;
        AnalyzeCxScriptImageEvidence(mat, context, evidenceOptions, fs::path(options.output_dir), evidenceReason);
    }

    std::cout << "[DEBUG HEADLESS] Before return - run_state=" << context.run_state << "\n" << std::flush;
    std::cout << "[DEBUG HEADLESS] Returning true\n" << std::flush;
    return true;

}

void ViewController::drawKeyParameterControlsWindow()
{
  ImGui::SetNextWindowPos(ImVec2(840, 8), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(520, 520), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Key Parameter Controls", nullptr, ImGuiWindowFlags_NoCollapse))
  {
    ImGui::End();
    return;
  }

  if (IsFindlineFindcircleContext(m_manualTest))
  {
    DrawKeyParameterControlPanel(m_manualTest);
  }
  else
  {
    DrawKeyParameterUnavailableNotice(m_manualTest);
  }

  ImGui::End();
}

void ViewController::drawParameterTuningAndConclusionWindow()
{
  ImGui::SetNextWindowPos(ImVec2(840, 540), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(760, 430), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Parameter Tuning Map / Result Conclusion", nullptr, ImGuiWindowFlags_NoCollapse))
  {
    ImGui::End();
    return;
  }

  if (IsFindlineFindcircleContext(m_manualTest))
  {
    DrawParamTuningScatterPanel(m_manualTest);
  }
  DrawConclusionSummaryPanel(m_manualTest);

  ImGui::End();
}

void ViewController::drawEvidenceAlbumWindow()
{
  ImGui::SetNextWindowPos(ImVec2(8, 1000), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(820, 420), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Evidence Album / Case Chain", nullptr, ImGuiWindowFlags_NoCollapse))
  {
    ImGui::End();
    return;
  }

  DrawEvidenceCaseListPanel(m_manualTest);

  ImGui::End();
}

void ViewController::drawAnnotationToolWindow()
{
  ImGui::SetNextWindowPos(ImVec2(1610, 8), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(310, 960), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Annotation Tool Palette / Tool Inspector", nullptr, ImGuiWindowFlags_NoCollapse))
  {
    ImGui::End();
    return;
  }

  ImGui::Text("Manifest path");
  ImGui::SetNextItemWidth(280.0f);
  InputTextString("##manifest_path", m_annotationManifestPath);
  if (ImGui::Button("Load Tool Manifest"))
  {
    m_annotationStatus = "manifest loading...";
  }

  ImGui::Separator();
  ImGui::Text("Tool Palette");
  ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "Annotation UI v2 button palette");

  auto drawAnnotationToolButton = [this](const char* label, ImageToolMode mode)
  {
    ImGui::PushID(label);

    const bool active = m_imageToolEnabled && m_imageToolMode == mode;

    if (active)
    {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.50f, 0.85f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.60f, 0.95f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.12f, 0.40f, 0.75f, 1.0f));
    }

    if (ImGui::Button(label, ImVec2(-1.0f, 26.0f)))
    {
      if (mode == ImageToolMode::PointerPan)
      {
        m_imageToolEnabled = false;
        m_imageToolMode = ImageToolMode::PointerPan;
        CancelAnnotationCreate();
        m_annotationStatus = "Pointer / Pan active";
      }
      else if (active)
      {
        m_imageToolEnabled = false;
        m_imageToolMode = ImageToolMode::PointerPan;
        CancelAnnotationCreate();
        m_annotationStatus = std::string(label) + " disabled";
      }
      else
      {
        m_imageToolEnabled = true;
        m_imageToolMode = mode;
        CancelAnnotationCreate();
        m_annotationStatus = std::string(label) + " enabled";
      }
    }

    if (active)
      ImGui::PopStyleColor(3);

    ImGui::PopID();
  };

  drawAnnotationToolButton("Pointer / Pan", ImageToolMode::PointerPan);
  drawAnnotationToolButton("Point", ImageToolMode::PointCreate);
  drawAnnotationToolButton("Line", ImageToolMode::LineCreate);
  drawAnnotationToolButton("Rect", ImageToolMode::RectCreate);
  drawAnnotationToolButton("Circle", ImageToolMode::CircleCreate);
  drawAnnotationToolButton("Polyline", ImageToolMode::PolylineCreate);
  drawAnnotationToolButton("Auto Boundary / EdgeSam", ImageToolMode::AutoBoundary);

  ImGui::Separator();
  ImGui::Text("Tool enabled: %s", m_imageToolEnabled ? "YES" : "NO");
  ImGui::Text("Active tool: %s", ImageToolModeName(m_imageToolMode));

  ImGui::Separator();
  ImGui::Text("Element List");
  ImGui::BeginChild("annotation_elements", ImVec2(-1, 150), true);
  int elemIndex = 0;
  for (const auto& elem : m_annotationLayer.Elements())
  {
    ImGui::PushID(elemIndex++);
    const char* kindStr = "unknown";
    if (elem.kind == OverlayKind::Point) kindStr = "Point";
    else if (elem.kind == OverlayKind::Line) kindStr = "Line";
    else if (elem.kind == OverlayKind::Rect) kindStr = "Rect";
    else if (elem.kind == OverlayKind::Circle) kindStr = "Circle";
    else if (elem.kind == OverlayKind::Polyline) kindStr = "Polyline";
    ImGui::Text("%s | id=%d", kindStr, elem.id);
    ImGui::PopID();
  }
  if (m_annotationLayer.Elements().empty())
  {
    ImGui::TextDisabled("No annotation elements");
  }
  ImGui::EndChild();

  ImGui::Separator();
  ImGui::Text("Session path");
  ImGui::SetNextItemWidth(280.0f);
  InputTextString("##session_path", m_annotationSessionPath);

  if (ImGui::Button("Save Elements"))
  {
    m_annotationStatus = "saving...";
  }
  ImGui::SameLine();
  if (ImGui::Button("Load Elements"))
  {
    m_annotationStatus = "loading...";
  }
  ImGui::SameLine();
  if (ImGui::Button("Clear Elements"))
  {
    m_annotationLayer.Clear();
    m_annotationStatus = "cleared";
  }

  ImGui::Text("Status: %s", m_annotationStatus.c_str());

  ImGui::End();
}

