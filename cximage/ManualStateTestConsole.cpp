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
    m_manualTest.run_state = "ready";
    m_manualTest.debug_status = "script_loaded";
    m_manualTest.debug_reason = "loaded exact bound script: " + scriptPath;
    reason = m_manualTest.debug_reason;
    return true;
}

void ViewController::RefreshRuntimeObjectTable(const std::string& lastMethod,
    const std::string& runtimeStatus)
{
    (void)lastMethod;
    (void)runtimeStatus;

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
}

void ViewController::drawManualStateTestConsole()
{
    ImGui::SetNextWindowPos(ImVec2(8, 8), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(820, 980), ImGuiCond_FirstUseEver);
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

        if (active) ImGui::PopStyleColor(3);
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
        ImGui::TextDisabled("No annotation elements");
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
