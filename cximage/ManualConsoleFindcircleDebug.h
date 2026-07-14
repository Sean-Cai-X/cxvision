#ifndef CXIMAGE_MANUAL_CONSOLE_FINDCIRCLE_DEBUG_H
#define CXIMAGE_MANUAL_CONSOLE_FINDCIRCLE_DEBUG_H

#include <string>
#include "ViewController.h"
#include "Findcircle.h"

void RefreshFindcircleDisplaySnapshot(ManualTestContext& context,
    RuntimeObjectView& object);

void RefreshFindcircleMeasureGeometrySnapshot(
    RuntimeObjectView& object,
    Findcircle& circle);

bool ResolveDebugIntToken(ManualTestContext& context, const std::string& token, int& value);

bool TryExecuteFindcircleSetcircle(ManualTestContext& context,
    int lineIndex,
    const std::string& statement);

bool TryExecuteFindcircleParamMethod(ManualTestContext& context,
    int lineIndex,
    const std::string& statement);

void FillFindcircleResultView(RuntimeObjectView& object,
    Findcircle& circle,
    const std::string& methodName);

bool SaveFindcircleDebugSnapshotJson(const ManualTestContext& context,
    std::string& outPath,
    std::string& outReason);

bool TryExecuteFindcircleRuntimeMethod(ManualTestContext& context,
    int lineIndex,
    const std::string& statement);

bool TryHandleFindcircleGetResult(ManualTestContext& context,
    int lineIndex,
    const std::string& statement);

#endif
