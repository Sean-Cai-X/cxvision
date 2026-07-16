#include "ViewController.h"
#include "Image.h"
#include "Findcircle.h"
#include "findline.h"
#include "FindSegmentation.h"
#include "CircleRingGauge.h"
#include "CxImageRuntimeOverlay.h"
#include "imagemanager.h"
#include "CxScriptImageEvidenceAnalyzer.h"
#include "CxScriptGeometryFrameProbe.h"
#include "FindlineParameterPolicy.h"
#include "CxScriptCatalogRuntime.h"
#include "CxParamProbeRunner.h"
#include "CxScriptRunTraceRuntime.h"
#include "ManualConsoleUtils.h"
#include "ManualConsoleRuntimeView.h"
#include "ManualConsoleCxScriptDebug.h"
#include "ManualConsoleFindcircleDebug.h"
#include "ManualConsoleFindlineDebug.h"
#include "ManualConsoleGauge.h"
#include "ManualConsoleParamRegressionPanel.h"
#include "ManualConsoleEvidenceChain.h"
#include "ManualConsoleScriptDebugPanel.h"
#include "CxScriptCasePackageWriter.h"
#include "CxScriptHeadlessRunner.h"

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

static fs::path ResolveCaseDirectory()
{
    return fs::path("docs") / "notes" / "cxscript_case";
}

static std::string CurrentTimestamp()
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

static std::string CxDebugJsonEscape(const std::string& text)
{
    std::string out;
    out.reserve(text.size());
    for (char ch : text)
    {
        if (ch == '\\') out += "\\\\";
        else if (ch == '\"') out += "\\\"";
        else if (ch == '\n') out += "\\n";
        else if (ch == '\r') out += "\\r";
        else if (ch == '\t') out += "\\t";
        else out += ch;
    }
    return out;
}

static void CopyPointsToFloatXY(const PointsShape& points, std::vector<float>& out)
{
    out.clear();
    for (int i = 0; i < points.size(); ++i)
    {
        const double x = points.getx(i);
        const double y = points.gety(i);
        if (!std::isfinite(x) || !std::isfinite(y))
            continue;
        out.push_back(static_cast<float>(x));
        out.push_back(static_cast<float>(y));
    }
}

static void FillRuntimeObjectFromFindcircle(
    RuntimeObjectView& object,
    const std::string& name,
    Findcircle& circle)
{
    object = RuntimeObjectView{};
    object.name = name;
    object.type = "Findcircle";
    object.exists_in_parser = true;
    object.last_runtime_status = "runtime_executed";
    object.runtime_state = "runtime_executed";
    object.visualizable = true;
    object.visual_source = "runtime_object";
    object.stale = false;

    object.has_circle = true;
    object.circle_cx = static_cast<float>(circle.getcirclecentx());
    object.circle_cy = static_cast<float>(circle.getcirclecenty());
    object.circle_px = static_cast<float>(circle.getcirclepax());
    object.circle_py = static_cast<float>(circle.getcirclepay());
    // Preserve the legacy snapshot fields until the diagnostic JSON schema is
    // migrated.  New GUI synchronization must use circle_px/circle_py.
    object.circle_inner = object.circle_px;
    object.circle_radius = object.circle_py;

    const PointsShape& points = circle.getresultpoints();
    CopyPointsToFloatXY(points, object.measure_points_xy);
    object.measure_points_count = points.size();
    object.valid_points_count =
        static_cast<int>(object.measure_points_xy.size() / 2);
    object.has_measure_points = !object.measure_points_xy.empty();

    object.has_fit_result = circle.hasfitresult();
    if (object.has_fit_result)
    {
        object.fit_cx = static_cast<float>(circle.getresultcentx());
        object.fit_cy = static_cast<float>(circle.getresultcenty());
        object.fit_radius = static_cast<float>(circle.getradius());
        object.fit_avgdist = static_cast<float>(circle.getavgdist());
        object.runtime_state = "geometry_result_available";
    }

    object.display_summary = BuildFindcircleGeometrySummary(object);
}

static void FillRuntimeObjectFromFindline(
    RuntimeObjectView& object,
    const std::string& name,
    Findline& line)
{
    object = RuntimeObjectView{};
    object.name = name;
    object.type = "Findline";
    object.exists_in_parser = true;
    object.last_runtime_status = "runtime_executed";
    object.runtime_state = "runtime_executed";
    object.visualizable = true;
    object.visual_source = "runtime_object";
    object.stale = false;

    FindlineDisplaySnapshot snapshot;
    if (line.getdisplaysnapshot(snapshot))
    {
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
        object.effective_tool_half_width = snapshot.scan_half_width;
        object.requested_tool_half_width = snapshot.scan_half_width;
    }

    const PointsShape& w_points = line.getresultpointsw();
    const PointsShape& h_points = line.getresultpointsh();
    object.line_pointsw_count = w_points.size();
    object.line_pointsh_count = h_points.size();
    CopyPointsToFloatXY(w_points, object.line_measure_points_xy);
    std::vector<float> h_xy;
    CopyPointsToFloatXY(h_points, h_xy);
    object.line_measure_points_xy.insert(
        object.line_measure_points_xy.end(), h_xy.begin(), h_xy.end());
    object.line_measure_points_count =
        static_cast<int>(object.line_measure_points_xy.size() / 2);
    object.valid_line_points_count = line.getvalidpointcount();
    object.valid_points_count = object.valid_line_points_count;
    object.has_line_measure_points = !object.line_measure_points_xy.empty();

    object.has_fit_line = line.hasfitresult();
    if (object.has_fit_line)
    {
        object.fit_line_x0 = static_cast<float>(line.getresultx0());
        object.fit_line_y0 = static_cast<float>(line.getresulty0());
        object.fit_line_x1 = static_cast<float>(line.getresultx1());
        object.fit_line_y1 = static_cast<float>(line.getresulty1());
        object.line_avgdist = static_cast<float>(line.getavgdist());
        object.runtime_state = "geometry_result_available";
    }

    object.display_summary = BuildFindlineGeometrySummary(object);
}

static void SeedDefaultManualGlobals(
    ManualTestContext& context,
    const std::string& scriptPath)
{
    auto set = [&](const char* name, int value) {
        context.runtime_int_vars[name] = value;
    };

    set("global_threshold", 20);
    set("global_method", 0);
    set("global_linegap", 6);
    set("global_wgap", 8);
    set("global_hgap", 32);
    set("global_tool_half_width", 32);

    const bool isCircleScript =
        scriptPath.find("find_circle") != std::string::npos ||
        scriptPath.find("findcircle") != std::string::npos;
    const bool isLineScript =
        scriptPath.find("find_line") != std::string::npos ||
        scriptPath.find("findline") != std::string::npos;
    const bool isVerticalLineScript =
        scriptPath.find("find_line_vertical") != std::string::npos ||
        scriptPath.find("findline_vertical") != std::string::npos;

    if (isVerticalLineScript)
    {
        set("global_roi_x0", 380);
        set("global_roi_y0", 120);
        set("global_roi_x1", 380);
        set("global_roi_y1", 820);
        set("global_wgap", 32);
        set("global_hgap", 8);
    }
    else
    {
        set("global_roi_x0", 120);
        set("global_roi_y0", 240);
        set("global_roi_x1", 980);
        set("global_roi_y1", 240);
    }

    if (isCircleScript)
    {
        set("global_circle_cx", 765);
        set("global_circle_cy", 471);
        set("global_circle_px", 1200);
        set("global_circle_py", 471);
        set("global_gap", 5);
        set("global_linegap", 3);
    }

    // The Script Editor and Gauge Workbench must start from the same value
    // snapshot.  Previously only runtime_int_vars were seeded, leaving the
    // visible/editable gauge at its zero-initialized geometry.
    ManualGaugeState gauge;
    gauge.source = "script_default";
    gauge.review_status = "editing";
    gauge.threshold = context.runtime_int_vars["global_threshold"];
    gauge.method = context.runtime_int_vars["global_method"];
    gauge.linegap = context.runtime_int_vars["global_linegap"];

    if (isCircleScript)
    {
        gauge.tool = "Findcircle";
        gauge.has_circle_gauge = true;
        gauge.circle_cx = context.runtime_int_vars["global_circle_cx"];
        gauge.circle_cy = context.runtime_int_vars["global_circle_cy"];
        gauge.circle_px = context.runtime_int_vars["global_circle_px"];
        gauge.circle_py = context.runtime_int_vars["global_circle_py"];
        gauge.gap = context.runtime_int_vars["global_gap"];
        gauge.radius = static_cast<int>(std::lround(std::hypot(
            static_cast<double>(gauge.circle_px - gauge.circle_cx),
            static_cast<double>(gauge.circle_py - gauge.circle_cy))));
    }
    else if (isLineScript)
    {
        gauge.tool = "Findline";
        gauge.has_line_gauge = true;
        gauge.line_x0 = context.runtime_int_vars["global_roi_x0"];
        gauge.line_y0 = context.runtime_int_vars["global_roi_y0"];
        gauge.line_x1 = context.runtime_int_vars["global_roi_x1"];
        gauge.line_y1 = context.runtime_int_vars["global_roi_y1"];
        gauge.tool_half_width =
            context.runtime_int_vars["global_tool_half_width"];
        gauge.wgap = context.runtime_int_vars["global_wgap"];
        gauge.hgap = context.runtime_int_vars["global_hgap"];
    }

    if (gauge.has_circle_gauge || gauge.has_line_gauge)
        context.current_gauge = gauge;
}
}

bool ViewController::QueryParserObjectExists(const std::string& type, const std::string& name)
{
    return m_parserDebugBridge.QueryObjectExists(type, name);
}

bool ViewController::QueryParserDouble(const std::string& name, double& value)
{
    return m_parserDebugBridge.QueryDouble(name, value);
}

void ViewController::initManualStateTestConsole()
{
    m_manualTest.analyzed_text.clear();
    m_manualTest.editor_text.clear();
    m_manualTest.editor_dirty = false;
    m_manualTest.current_line = 0;
    m_manualTest.run_state = "ready";
    m_manualTest.debug_status = "PENDING";
    m_manualTest.debug_reason = "not executed";
    m_manualTest.runtime_current_status = "PENDING";
    m_manualTest.show_image = true;
    m_manualTest.case_directory = ResolveCaseDirectory().string();
    m_manualTest.catalog_path = "cxparser/cxscript/module/cximage/cxscript_catalog.cxsc";
    m_manualTest.manifest_path = "cxparser/cxscript/module/cximage/image_manifest.cxsc";
}

bool ViewController::LoadBoundStateToManualConsole(
    const std::string& nodeId,
    const std::string& scriptPath,
    std::string& reason)
{
    m_manualTest.active_script_case_name = nodeId;
    m_manualTest.active_script_case_path = scriptPath;
    if (scriptPath.empty())
    {
        reason = "semantic node has no bound script path";
        m_manualTest.debug_status = "script_load_failed";
        m_manualTest.debug_reason = reason;
        return false;
    }

    const std::filesystem::path resolved = ResolveWorkspaceFile(scriptPath);
    std::string text;
    if (!ReadTextFile(resolved.string(), text))
    {
        reason = "cannot read bound catalog script: " + resolved.string();
        m_manualTest.editor_text.clear();
        m_manualTest.loaded_script_path.clear();
        m_manualTest.script_file_path.clear();
        m_manualTest.editor_dirty = false;
        m_manualTest.analyzed_text.clear();
        m_manualTest.debug_status = "script_load_failed";
        m_manualTest.debug_reason = reason;
        return false;
    }

    m_manualTest.editor_text = text;
    m_manualTest.loaded_script_path = resolved.string();
    m_manualTest.script_file_path = resolved.string();
    m_manualTest.editor_source = "semantic_flow";
    m_manualTest.editor_dirty = false;
    m_manualTest.analyzed_text.clear();
    m_manualTest.current_line = 0;
    SeedDefaultManualGlobals(m_manualTest, scriptPath);
    m_manualTest.run_state = "ready";
    m_manualTest.debug_status = "script_loaded";
    m_manualTest.debug_reason = "loaded exact bound script: " + scriptPath;
    reason = m_manualTest.debug_reason;
    return true;
}

void ViewController::RefreshRuntimeObjectTable(const std::string& lastMethod,
    const std::string& runtimeStatus)
{
    m_manualTest.runtime_objects.clear();

    for (const std::string& name :
         m_parserDebugBridge.ListClassObjectNames("Findcircle"))
    {
        Findcircle* circle = static_cast<Findcircle*>(
            m_parserDebugBridge.QueryClassObject("Findcircle", name));
        if (circle == nullptr)
            continue;

        RuntimeObjectView object;
        FillRuntimeObjectFromFindcircle(object, name, *circle);
        object.last_method = lastMethod;
        object.last_runtime_status = runtimeStatus;
        m_manualTest.runtime_objects.push_back(object);
    }

    for (const std::string& name :
         m_parserDebugBridge.ListClassObjectNames("Findline"))
    {
        Findline* line = static_cast<Findline*>(
            m_parserDebugBridge.QueryClassObject("Findline", name));
        if (line == nullptr)
            continue;

        RuntimeObjectView object;
        FillRuntimeObjectFromFindline(object, name, *line);
        object.last_method = lastMethod;
        object.last_runtime_status = runtimeStatus;
        m_manualTest.runtime_objects.push_back(object);
    }

    for (const std::string& name :
         m_parserDebugBridge.ListClassObjectNames("FindSegmentation"))
    {
        FindSegmentation* seg = static_cast<FindSegmentation*>(
            m_parserDebugBridge.QueryClassObject("FindSegmentation", name));
        if (seg == nullptr)
            continue;

        RuntimeObjectView object;
        object.name = name;
        object.type = "FindSegmentation";
        object.exists_in_parser = true;
        object.last_method = lastMethod;
        object.last_runtime_status = runtimeStatus;
        object.runtime_state = seg->m_status;
        object.stale = false;
        object.visualizable = seg->get_contour_count() > 0;
        object.visual_source = object.visualizable ? "segmentation_boundary" : "segmentation_prompt";
        object.has_measure_points = seg->get_contour_count() > 0;
        object.measure_points_count = seg->get_contour_count();
        object.valid_points_count = seg->get_contour_count();
        object.display_summary =
            "FindSegmentation " + name +
            " status=" + seg->m_status +
            " contours=" + std::to_string(seg->get_contour_count()) +
            " area=" + std::to_string(seg->get_primary_area());
        m_manualTest.runtime_objects.push_back(object);
    }

    for (RuntimeObjectView& object : m_manualTest.runtime_objects)
    {
        if (object.stale)
            continue;

        object.display_summary = BuildGeometrySummary(object);
    }

    m_manualTest.geometry_summary = "";
    m_manualTest.image_overlay_summary = "";

    for (const RuntimeObjectView& object : m_manualTest.runtime_objects)
    {
        if (object.visualizable && !object.stale)
        {
            m_manualTest.geometry_summary += BuildGeometrySummary(object) + "\n";
            m_manualTest.image_overlay_summary += BuildOverlaySummary(m_manualTest, object) + "\n";
        }
    }

    SyncRuntimeObjectsToShapeElements();

    m_manualTest.current_result_ref = ResultRefView();
    m_scriptResult.result_ref.clear();
    m_scriptResult.overlay_ref.clear();
    for (const RuntimeObjectView& object : m_manualTest.runtime_objects)
    {
        if (!object.visualizable || object.stale)
            continue;

        m_manualTest.current_result_ref.source_object = object.name;
        m_manualTest.current_result_ref.value = "runtime_object:" + object.name;
        m_manualTest.current_result_ref.status = "runtime_object_available";

        if (object.type == "Findcircle")
        {
            m_manualTest.current_result_ref.name = "global_circle_ref";
            m_manualTest.current_result_ref.result_type = "FindcircleResult";
            m_manualTest.current_result_ref.fit_cx = object.fit_cx;
            m_manualTest.current_result_ref.fit_cy = object.fit_cy;
            m_manualTest.current_result_ref.fit_radius = object.fit_radius;
            m_manualTest.current_result_ref.avgdist = object.fit_avgdist;
            m_manualTest.current_result_ref.points_count = object.measure_points_count;
            m_manualTest.current_result_ref.valid_points_count = object.valid_points_count;
        }
        else if (object.type == "Findline")
        {
            m_manualTest.current_result_ref.name = "global_line_ref";
            m_manualTest.current_result_ref.result_type = "FindlineResult";
            m_manualTest.current_result_ref.line_x0 = object.fit_line_x0;
            m_manualTest.current_result_ref.line_y0 = object.fit_line_y0;
            m_manualTest.current_result_ref.line_x1 = object.fit_line_x1;
            m_manualTest.current_result_ref.line_y1 = object.fit_line_y1;
            m_manualTest.current_result_ref.line_avgdist = object.line_avgdist;
            m_manualTest.current_result_ref.line_points_count = object.line_measure_points_count;
            m_manualTest.current_result_ref.valid_line_points_count = object.valid_points_count;
        }

        m_scriptResult.result_ref = m_manualTest.current_result_ref.value;
        m_scriptResult.overlay_ref = "shape_owner:" + object.type + ":" + object.name;
        break;
    }
}

void ViewController::drawManualStateTestConsole()
{
    ImGui::SetNextWindowPos(ImVec2(8, 8), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(520, 380), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Manual State Test Console", nullptr, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        return;
    }

    ImGui::Text("Script: %s", m_manualTest.loaded_script_path.c_str());
    ImGui::Text("Image: %s", m_manualTest.image_file_path.c_str());
    ImGui::Text("Run State: %s", m_manualTest.run_state.c_str());
    ImGui::Text("Debug Status: %s", m_manualTest.debug_status.c_str());
    ImGui::Text("Debug Reason: %s", m_manualTest.debug_reason.c_str());

    ImGui::Separator();

    DrawScriptEditorBlock(m_manualTest);

    ImGui::Separator();

    DrawScriptDebugCompilerBlock(m_manualTest);

    ImGui::Separator();

    DrawCxParserExtLineViewsPanel(m_manualTest);
    DrawCxParserExtStatementViewsPanel(m_manualTest);
    DrawCxParserExtObjectAssignmentsPanel(m_manualTest);

    ImGui::End();
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
        std::string reason;
        CxAnnotationToolManifestSnapshot snapshot;
        if (!m_parserOwner.ParseAnnotationToolManifest(m_annotationManifestPath, snapshot, reason))
        {
            m_annotationStatus = "parse failed: " + reason;
        }
        else if (!m_annotationLayer.ApplyToolManifestSnapshot(snapshot, reason))
        {
            m_annotationStatus = "apply failed: " + reason;
        }
        else
        {
            m_annotationStatus = reason;
        }
    }

    ImGui::Separator();
    ImGui::Text("Tool Palette");
    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "Annotation UI v2 manifest button palette");

    auto toolModeFromDefinition = [](const AnnotationToolDefinition& tool)
    {
        if (tool.kind == OverlayKind::Point)
            return ImageToolMode::PointCreate;
        if (tool.kind == OverlayKind::Line)
            return ImageToolMode::LineCreate;
        if (tool.kind == OverlayKind::Rect)
            return ImageToolMode::RectCreate;
        if (tool.kind == OverlayKind::Circle)
            return ImageToolMode::CircleCreate;
        if (tool.kind == OverlayKind::Ellipse)
            return ImageToolMode::EllipseCreate;
        if (tool.kind == OverlayKind::Polyline ||
            tool.kind == OverlayKind::BoundaryPolyline)
            return ImageToolMode::PolylineCreate;
        if (tool.kind == OverlayKind::AutoBoundaryRequest ||
            tool.action == "auto_segmentation")
            return ImageToolMode::AutoBoundary;
        return ImageToolMode::PointerPan;
    };

    auto drawPointerPanButton = [this]()
    {
        ImGui::PushID("Pointer / Pan");
        const bool active = !m_imageToolEnabled ||
                            m_imageToolMode == ImageToolMode::PointerPan ||
                            m_annotationLayer.ActiveToolIndex() < 0;
        if (active)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.50f, 0.85f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.60f, 0.95f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.12f, 0.40f, 0.75f, 1.0f));
        }

        if (ImGui::Button("Pointer / Pan", ImVec2(-1.0f, 26.0f)))
        {
            m_imageToolEnabled = false;
            m_imageToolMode = ImageToolMode::PointerPan;
            CancelAnnotationCreate();
            m_annotationLayer.SetActiveToolIndex(-1);
            m_annotationStatus = "Pointer / Pan active";
        }

        if (active) ImGui::PopStyleColor(3);
        ImGui::PopID();
    };

    auto drawManifestToolButton = [this, &toolModeFromDefinition](int toolIndex, const AnnotationToolDefinition& tool)
    {
        ImGui::PushID(tool.name.c_str());
        const bool active = m_imageToolEnabled &&
                            m_annotationLayer.ActiveToolIndex() == toolIndex;
        if (active)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.50f, 0.85f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.60f, 0.95f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.12f, 0.40f, 0.75f, 1.0f));
        }

        const std::string label = tool.label.empty() ? tool.name : tool.label;
        if (ImGui::Button(label.c_str(), ImVec2(-1.0f, 26.0f)))
        {
            if (active)
            {
                m_imageToolEnabled = false;
                m_imageToolMode = ImageToolMode::PointerPan;
                CancelAnnotationCreate();
                m_annotationLayer.SetActiveToolIndex(-1);
                m_annotationStatus = label + " disabled";
            }
            else
            {
                m_imageToolEnabled = true;
                m_imageToolMode = toolModeFromDefinition(tool);
                CancelAnnotationCreate();
                m_annotationLayer.SetActiveToolIndex(toolIndex);
                m_annotationStatus =
                    "enabled tool_id=" + tool.name +
                    " shape=" + tool.shape_type +
                    " role=" + tool.role +
                    " action=" + tool.action;
            }
        }

        if (active) ImGui::PopStyleColor(3);
        ImGui::PopID();
    };

    drawPointerPanButton();
    for (int i = 0; i < static_cast<int>(m_annotationLayer.Tools().size()); ++i)
    {
        const AnnotationToolDefinition& tool = m_annotationLayer.Tools()[i];
        if (!tool.manual_visible) continue;
        drawManifestToolButton(i, tool);
    }

    ImGui::Separator();
    ImGui::Text("Tool enabled: %s", m_imageToolEnabled ? "YES" : "NO");
    ImGui::Text("Active mode: %s", ImageToolModeName(m_imageToolMode));
    const AnnotationToolDefinition* activeTool = m_annotationLayer.ActiveTool();
    if (activeTool != nullptr)
    {
        ImGui::TextWrapped("Active tool id: %s | shape=%s | role=%s | action=%s",
                           activeTool->name.c_str(),
                           activeTool->shape_type.c_str(),
                           activeTool->role.c_str(),
                           activeTool->action.c_str());
    }
    else
    {
        ImGui::TextDisabled("Active tool id: (none)");
    }
    ImGui::Text("ShapeElements: %d",
                static_cast<int>(m_annotationLayer.ShapeElements().size()));
    ImGui::TextWrapped("Last pointer: %s | %s | %s",
                       m_lastPointerResult.phase.c_str(),
                       m_lastPointerResult.status.c_str(),
                       m_lastPointerResult.reason.c_str());

    ImGui::Separator();
    ImGui::Text("Element List (ShapeElements)");
    ImGui::BeginChild("annotation_elements", ImVec2(-1, 150), true);
    int elemIndex = 0;
    for (const auto& elem : m_annotationLayer.ShapeElements())
    {
        ImGui::PushID(elemIndex++);
        ImGui::Text("%s | id=%d | tool=%s | role=%s",
                    elem.stable_ref.c_str(),
                    elem.id,
                    elem.tool_id.c_str(),
                    elem.semantic_role.c_str());
        ImGui::PopID();
    }
    if (m_annotationLayer.ShapeElements().empty())
        ImGui::TextDisabled("No shape elements");
    ImGui::EndChild();

    ImGui::Separator();
    ImGui::Text("Session path");
    ImGui::SetNextItemWidth(280.0f);
    InputTextString("##session_path", m_annotationSessionPath);

    if (ImGui::Button("Save Elements"))
        m_annotationStatus = "saving...";
    ImGui::SameLine();
    if (ImGui::Button("Load Elements"))
        m_annotationStatus = "loading...";
    ImGui::SameLine();
    if (ImGui::Button("Clear Elements"))
    {
        m_annotationLayer.Clear();
        m_annotationStatus = "cleared";
    }

    ImGui::Text("Status: %s", m_annotationStatus.c_str());
    ImGui::End();
}
