#include "pch.h"
#include "ManualConsoleGauge.h"
#include "ManualConsoleUtils.h"
#include "ManualConsoleCxScriptDebug.h"
#include "CxScriptHeadlessRuntime.h"

#include <sstream>
#include <fstream>

void InjectManualGaugeInt(ManualTestContext& context, const std::string& key, int value)
{
    context.runtime_int_vars[key] = value;
    UpsertGlobalVariableView(
        context,
        "int",
        key,
        std::to_string(value),
        0,
        "manual_gauge_applied");
}

void DrawGaugeHandle(
    const ImVec2& canvas_pos,
    float x,
    float y,
    float scale,
    const ImVec4& color,
    const char* label,
    bool selected)
{
    const float r = selected ? 8.0f * scale : 6.0f * scale;
    float fx = canvas_pos.x + x * scale;
    float fy = canvas_pos.y + y * scale;
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddCircleFilled(ImVec2(fx, fy), r, IM_COL32(
        static_cast<int>(color.x * 255),
        static_cast<int>(color.y * 255),
        static_cast<int>(color.z * 255),
        static_cast<int>(color.w * 255)));
    if (label)
        draw_list->AddText(ImVec2(fx + r + 4, fy - 8), IM_COL32(255, 255, 255, 255), label);
}

void DrawGaugeHandlesLine(
    const RuntimeObjectView& object,
    const ImVec2& canvas_pos,
    float scale,
    const char* objectName,
    const char* gaugeName)
{
    (void)object;
    (void)canvas_pos;
    (void)scale;
    (void)objectName;
    (void)gaugeName;
}

void DrawGaugeHandlesCircle(
    const RuntimeObjectView& object,
    const ImVec2& canvas_pos,
    float scale,
    const char* objectName,
    const char* gaugeName)
{
    (void)object;
    (void)canvas_pos;
    (void)scale;
    (void)objectName;
    (void)gaugeName;
}

void DrawGaugeHandles(
    const RuntimeObjectView& object,
    const ImVec2& canvas_pos,
    float scale,
    const char* objectName,
    const char* gaugeName)
{
    if (object.type == "Findline")
        DrawGaugeHandlesLine(object, canvas_pos, scale, objectName, gaugeName);
    else if (object.type == "Findcircle")
        DrawGaugeHandlesCircle(object, canvas_pos, scale, objectName, gaugeName);
}

void ApplyManualGaugeToGlobals(ManualTestContext& context, const std::string& objectName)
{
    (void)context;
    (void)objectName;
}

void ApplyManualGaugeToGlobals(ManualTestContext& context)
{
    (void)context;
}

std::filesystem::path ManualGaugeCaseDir(const ManualTestContext& context)
{
    (void)context;
    return std::filesystem::path();
}

bool SaveManualGaugeAnnotation(
    ManualTestContext& context,
    const std::string& objectName,
    const std::string& gaugeName,
    std::string& outPath,
    std::string& outReason)
{
    (void)context;
    (void)objectName;
    (void)gaugeName;
    (void)outPath;
    (void)outReason;
    return true;
}

bool LoadManualGaugeAnnotation(
    ManualTestContext& context,
    const std::string& objectName,
    const std::string& gaugeName,
    std::string& outPath,
    std::string& outReason)
{
    (void)context;
    (void)objectName;
    (void)gaugeName;
    (void)outPath;
    (void)outReason;
    return true;
}

bool ExportManualGaugeManifestCandidate(
    ManualTestContext& context,
    const std::string& objectName,
    const std::string& gaugeName,
    std::string& outPath,
    std::string& outReason)
{
    (void)context;
    (void)objectName;
    (void)gaugeName;
    (void)outPath;
    (void)outReason;
    return true;
}

bool ManualGaugeAcceptedForParamRegression(const ManualGaugeState& gauge)
{
    (void)gauge;
    return false;
}
