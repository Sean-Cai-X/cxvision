#ifndef CXIMAGE_MANUAL_CONSOLE_FINDLINE_DEBUG_H
#define CXIMAGE_MANUAL_CONSOLE_FINDLINE_DEBUG_H

#include <string>
#include "ViewController.h"

const char* FindlineModeName(int mode);

void AppendPointsShapeToXY(PointsShape& points, std::vector<float>& outXY);

void RefreshFindlineDisplaySnapshot(ManualTestContext& context,
                                   RuntimeObjectView& object,
                                   Findline& lineTool);

std::string BuildFindlineMeasureHint(const RuntimeObjectView& object);

void RefreshFindlineMeasureSnapshot(RuntimeObjectView& object,
    Findline& lineTool);

bool ApplyRuntimeFindlineWHgap(
    ManualTestContext& context,
    const std::string& objectName,
    int wgap,
    int hgap,
    int updateLineNo,
    const char* updateSource,
    std::string& outReason);

bool ResolveDebugIntValue(ManualTestContext& context, const std::string& token, int& value);

bool TryExecuteFindlineSetline(ManualTestContext& context,
    int lineIndex,
    const std::string& statement);

bool TryExecuteFindlineParamMethod(ManualTestContext& context,
    int lineIndex,
    const std::string& statement);

bool TryExecuteFindlineRuntimeMethod(ManualTestContext& context, int lineIndex, const std::string& statement);

bool TryExecutePendingRuntimeMethod(ManualTestContext& context,
    int lineIndex,
    const std::string& statement);

bool TryExecuteGetResultBinding(ManualTestContext& context,
    int lineIndex,
    const std::string& statement);

bool TryExecuteImageCopyFromMat(ManualTestContext& context,
    int lineIndex,
    const std::string& statement);

#endif
