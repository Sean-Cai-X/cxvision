#ifndef CXIMAGE_MANUAL_CONSOLE_GAUGE_H
#define CXIMAGE_MANUAL_CONSOLE_GAUGE_H

#include <string>
#include "ViewController.h"

void InjectManualGaugeInt(ManualTestContext& context, const std::string& key, int value);

void DrawGaugeHandle(
    const ImVec2& canvas_pos,
    float x,
    float y,
    float scale,
    const ImVec4& color,
    const char* label,
    bool selected);

void DrawGaugeHandlesLine(
    const RuntimeObjectView& object,
    const ImVec2& canvas_pos,
    float scale,
    const char* objectName,
    const char* gaugeName);

void DrawGaugeHandlesCircle(
    const RuntimeObjectView& object,
    const ImVec2& canvas_pos,
    float scale,
    const char* objectName,
    const char* gaugeName);

void DrawGaugeHandles(
    const RuntimeObjectView& object,
    const ImVec2& canvas_pos,
    float scale,
    const char* objectName,
    const char* gaugeName);

void ApplyManualGaugeToGlobals(ManualTestContext& context, const std::string& objectName);
void ApplyManualGaugeToGlobals(ManualTestContext& context);

std::filesystem::path ManualGaugeCaseDir(const ManualTestContext& context);

bool SaveManualGaugeAnnotation(
    ManualTestContext& context,
    const std::string& objectName,
    const std::string& gaugeName,
    std::string& outPath,
    std::string& outReason);

bool LoadManualGaugeAnnotation(
    ManualTestContext& context,
    const std::string& objectName,
    const std::string& gaugeName,
    std::string& outPath,
    std::string& outReason);

bool ExportManualGaugeManifestCandidate(
    ManualTestContext& context,
    const std::string& objectName,
    const std::string& gaugeName,
    std::string& outPath,
    std::string& outReason);

bool ManualGaugeAcceptedForParamRegression(const ManualGaugeState& gauge);

#endif
