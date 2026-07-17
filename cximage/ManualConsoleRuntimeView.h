#ifndef CXIMAGE_MANUAL_CONSOLE_RUNTIME_VIEW_H
#define CXIMAGE_MANUAL_CONSOLE_RUNTIME_VIEW_H

#include <string>
#include "ViewController.h"

RuntimeObjectView* FindRuntimeObjectByName(ManualTestContext& context,
    const std::string& name);

const RuntimeObjectView* FindRuntimeObjectByName(const ManualTestContext& context,
    const std::string& name);

RuntimeObjectView* FindRuntimeObject(ManualTestContext& context,
    const std::string& name);

bool RuntimeObjectIsType(ManualTestContext& context,
    const std::string& objectName,
    const std::string& expectedType);

RuntimeObjectView& EnsureRuntimeObject(ManualTestContext& context,
    const std::string& name,
    const std::string& type,
    int declaredLine);

std::string BuildFindcircleGeometrySummary(const RuntimeObjectView& object);

std::string BuildFindlineGeometrySummary(const RuntimeObjectView& object);

std::string BuildFindSegmentationGeometrySummary(const RuntimeObjectView& object);

std::string BuildGeometrySummary(const RuntimeObjectView& object);

std::string BuildFindcircleOverlaySummary(const ManualTestContext& context,
    const RuntimeObjectView& object);

std::string BuildFindlineOverlaySummary(const ManualTestContext& context,
    const RuntimeObjectView& object);

std::string BuildFindSegmentationOverlaySummary(const ManualTestContext& context,
    const RuntimeObjectView& object);

std::string BuildOverlaySummary(const ManualTestContext& context,
    const RuntimeObjectView& object);

void UpdateFindcircleDebugSnapshot(ManualTestContext& context,
    const RuntimeObjectView& object,
    int lineNo,
    const std::string& statement);

void RefreshSnapshotFromCurrentResultRef(ManualTestContext& context);

std::string BuildDebugCursorText(const ManualTestContext& context);

int LastExecutedLineNo(const ManualTestContext& context);

std::string ModuleForType(const std::string& type);

bool IsObjectType(const std::string& type);

std::string ModuleForStatement(const std::string& statement);

#endif
