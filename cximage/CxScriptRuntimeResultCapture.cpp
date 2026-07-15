#include "pch.h"
#include "CxScriptRuntimeResultCapture.h"
#include "Findline.h"
#include "Findcircle.h"
#include "ParserClass.h"
#include "ImageAnnotationLayer.h"
#include "shapebase.h"

void CopyShapeElementsToSnapshots(
    const ImageAnnotationLayer& layer,
    std::vector<CxShapeElementSnapshot>& snapshots)
{
    snapshots.clear();
    for (const auto& elem : layer.ShapeElements())
    {
        CxShapeElementSnapshot snap;
        snap.stable_ref = elem.stable_ref;
        snap.owner_type = elem.owner_type;
        snap.owner_ref = elem.owner_ref;
        snap.semantic_role = elem.semantic_role;
        snap.editable = elem.editable;
        snap.result_element = elem.result_element;

        if (elem.shape)
        {
            CxShapeGeometrySnapshot geo;
            if (elem.shape->snapshot(geo))
            {
                snap.shape_kind = CxShapeKindName(geo.kind);
                snap.center_x = geo.center.x;
                snap.center_y = geo.center.y;
                snap.radius = geo.radius;
                snap.inner_radius = geo.inner_radius;
                snap.half_width = geo.half_width;
                snap.radius_x = geo.radius_x;
                snap.radius_y = geo.radius_y;
                snap.angle_deg = geo.angle;
                snap.closed = geo.closed;

                for (const auto& pt : geo.points)
                {
                    snap.points.push_back(pt.x);
                    snap.points.push_back(pt.y);
                }
            }
        }

        snapshots.push_back(snap);
    }
}

bool CaptureFindlineResult(
    Findline& tool,
    const std::string& object_name,
    CxScriptToolResultCapture& output)
{
    output.type = "Findline";
    output.name = object_name;
    output.owner_ref = object_name;

    output.valid_points_count = tool.getvalidpointcount();
    output.has_fit_line = tool.hasfitresult();
    output.avgdist = tool.getavgdist();
    const FindlineMeasureInputDebug& debug = tool.lastmeasureinputdebug();
    output.object_prefilter_requested = (debug.objfilterset & 0x01) != 0;
    output.object_prefilter_applied = debug.findobject_measure_called;
    output.object_filter_borw = debug.effective_filter_borw;
    output.object_filter_min = debug.effective_filter_min;
    output.object_filter_max = debug.effective_filter_max;
    output.budget_exceeded = tool.budgetexceeded();
    output.failure_stage = output.has_fit_line
        ? std::string()
        : tool.getfailurestage();

    if (output.has_fit_line)
    {
        output.fit_line_x0 = tool.getresultx0();
        output.fit_line_y0 = tool.getresulty0();
        output.fit_line_x1 = tool.getresultx1();
        output.fit_line_y1 = tool.getresulty1();
    }

    ImageAnnotationLayer layer;
    tool.PublishDisplayShapes(layer, output.owner_ref);
    CopyShapeElementsToSnapshots(layer, output.shapes);

    return true;
}

bool CaptureFindcircleResult(
    Findcircle& tool,
    const std::string& object_name,
    CxScriptToolResultCapture& output)
{
    output.type = "Findcircle";
    output.name = object_name;
    output.owner_ref = object_name;

    output.valid_points_count = tool.getvalidpointcount();
    output.has_fit_circle = tool.hasfitresult();
    output.circle_cx = tool.getresultcentx();
    output.circle_cy = tool.getresultcenty();
    output.circle_radius = tool.getradius();
    output.avgdist = tool.getavgdist();
    output.object_prefilter_requested = (tool.getfindsetting() & 0x01) != 0;
    output.object_prefilter_applied = tool.getdebugprefilterused() != 0;
    output.object_filter_borw = tool.getfilterborw();
    output.object_filter_min = tool.getfiltermin();
    output.object_filter_max = tool.getfiltermax();
    output.fit_filter_input_count = tool.getfitfilterinputcount();
    output.fit_filter_kept_count = tool.getfitfilterkeptcount();
    output.fit_filter_rejected_count = tool.getfitfilterrejectedcount();
    output.fit_filter_sigma = tool.getfitfiltersigma();
    output.fit_filter_threshold = tool.getfitfilterthreshold();
    output.budget_exceeded = tool.budgetexceeded();
    output.failure_stage = output.has_fit_circle
        ? std::string()
        : tool.getfailurestage();

    ImageAnnotationLayer layer;
    tool.PublishDisplayShapes(layer, output.owner_ref);
    CopyShapeElementsToSnapshots(layer, output.shapes);

    return true;
}

static void MergeToolCapture(
    const CxScriptToolResultCapture& tool,
    CxScriptExecutionCapture& capture)
{
    capture.valid_points_count += tool.valid_points_count;
    capture.has_fit_line = capture.has_fit_line || tool.has_fit_line;
    capture.has_fit_circle = capture.has_fit_circle || tool.has_fit_circle;
    capture.budget_exceeded = capture.budget_exceeded || tool.budget_exceeded;
    capture.avgdist = tool.avgdist;
    capture.object_prefilter_requested = tool.object_prefilter_requested;
    capture.object_prefilter_applied = tool.object_prefilter_applied;
    capture.object_filter_borw = tool.object_filter_borw;
    capture.object_filter_min = tool.object_filter_min;
    capture.object_filter_max = tool.object_filter_max;
    capture.fit_filter_input_count = tool.fit_filter_input_count;
    capture.fit_filter_kept_count = tool.fit_filter_kept_count;
    capture.fit_filter_rejected_count = tool.fit_filter_rejected_count;
    capture.fit_filter_sigma = tool.fit_filter_sigma;
    capture.fit_filter_threshold = tool.fit_filter_threshold;

    if (capture.failure_stage.empty() && !tool.failure_stage.empty())
        capture.failure_stage = tool.failure_stage;

    if (capture.reason.empty() && !tool.reason.empty())
        capture.reason = tool.reason;

    if (tool.has_fit_circle)
    {
        capture.circle_radius = tool.circle_radius;
    }

    for (const auto& shape : tool.shapes)
    {
        capture.shapes.push_back(shape);

        if (shape.semantic_role == "roi")
            capture.rendered_roi_count++;

        if (shape.semantic_role == "scan")
            capture.rendered_scan_count++;

        if (shape.semantic_role == "measure_points")
        {
            capture.rendered_measure_points_count +=
                static_cast<int>(shape.points.size() / 2);
        }

        if (shape.semantic_role == "result")
            capture.rendered_result_count++;
    }
}

bool CaptureRuntimeToolResults(
    mu::CxParserRuntime& runtime,
    CxScriptExecutionCapture& capture,
    std::string& reason)
{
    bool supported_object_found = false;

    const int findline_count = runtime.GetClassObjSum("Findline");

    for (int i = 0; i < findline_count; ++i)
    {
        Findline* tool = static_cast<Findline*>(
            runtime.GetClassObj("Findline", i));

        if (tool == nullptr)
            continue;

        supported_object_found = true;

        CxScriptToolResultCapture tool_capture;

        const std::string object_name =
            runtime.GetClassObjName("Findline", i);

        try
        {
            if (!CaptureFindlineResult(*tool, object_name, tool_capture))
            {
                reason = "failed to capture Findline: " + object_name;
                return false;
            }
        }
        catch (...)
        {
            reason = "CaptureFindlineResult crashed for: " + object_name;
            return false;
        }

        capture.scan_line_count += tool->getscanlinecount();
        capture.sample_count += tool->getsamplecount();

        MergeToolCapture(tool_capture, capture);
    }

    const int findcircle_count = runtime.GetClassObjSum("Findcircle");

    for (int i = 0; i < findcircle_count; ++i)
    {
        Findcircle* tool = static_cast<Findcircle*>(
            runtime.GetClassObj("Findcircle", i));

        if (tool == nullptr)
            continue;

        supported_object_found = true;

        CxScriptToolResultCapture tool_capture;

        const std::string object_name =
            runtime.GetClassObjName("Findcircle", i);

        try
        {
            if (!CaptureFindcircleResult(*tool, object_name, tool_capture))
            {
                reason = "failed to capture Findcircle: " + object_name;
                return false;
            }
        }
        catch (...)
        {
            reason = "CaptureFindcircleResult crashed for: " + object_name;
            return false;
        }

        capture.scan_line_count += tool->getscanlinecount();
        capture.sample_count += tool->getsamplecount();

        MergeToolCapture(tool_capture, capture);
    }

    if (!supported_object_found)
    {
        reason = "no supported Findline/Findcircle runtime object found";
        return false;
    }

    reason.clear();
    return true;
}
