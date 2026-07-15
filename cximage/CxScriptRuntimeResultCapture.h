#ifndef CXIMAGE_CXSCRIPT_RUNTIME_RESULT_CAPTURE_H
#define CXIMAGE_CXSCRIPT_RUNTIME_RESULT_CAPTURE_H

#include "CxScriptHeadlessRuntime.h"

namespace mu
{
    class CxParserRuntime;
}

class Findline;
class Findcircle;

struct CxScriptToolResultCapture
{
    std::string type;
    std::string name;
    std::string owner_ref;

    bool algorithm_executed = false;
    bool measure_completed = false;
    bool fit_completed = false;
    bool budget_exceeded = false;

    int elapsed_ms = 0;
    int scan_line_count = 0;
    int sample_count = 0;

    int valid_points_count = 0;
    bool has_fit_line = false;
    bool has_fit_circle = false;

    double avgdist = 0.0;

    double fit_line_x0 = 0.0;
    double fit_line_y0 = 0.0;
    double fit_line_x1 = 0.0;
    double fit_line_y1 = 0.0;

    double circle_cx = 0.0;
    double circle_cy = 0.0;
    double circle_radius = 0.0;

    bool object_prefilter_requested = false;
    bool object_prefilter_applied = false;
    int object_filter_borw = 0;
    int object_filter_min = 0;
    int object_filter_max = 0;
    int fit_filter_input_count = 0;
    int fit_filter_kept_count = 0;
    int fit_filter_rejected_count = 0;
    double fit_filter_sigma = 0.0;
    double fit_filter_threshold = 0.0;

    std::string failure_stage;
    std::string reason;

    std::vector<CxShapeElementSnapshot> shapes;
};

bool CaptureRuntimeToolResults(
    mu::CxParserRuntime& runtime,
    CxScriptExecutionCapture& capture,
    std::string& reason);

bool CaptureFindlineResult(
    class Findline& tool,
    const std::string& object_name,
    CxScriptToolResultCapture& output);

bool CaptureFindcircleResult(
    class Findcircle& tool,
    const std::string& object_name,
    CxScriptToolResultCapture& output);

#endif
