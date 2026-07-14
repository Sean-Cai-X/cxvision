#include "pch.h"
#include "ManualConsoleFindlineDebug.h"
#include "ManualConsoleUtils.h"
#include "ManualConsoleRuntimeView.h"
#include "ManualConsoleCxScriptDebug.h"
#include "ManualConsoleFindcircleDebug.h"
#include "CxScriptRunTraceRuntime.h"

#include <sstream>
#include <cmath>

const char* FindlineModeName(int mode)
{
    static const char* names[] = {"Unspecified", "LeastSquares", "MinimumZone", "Ransac", "SingleEdge", "EdgePairCenter", "HorizontalVerticalPriority", "WeightedMeasurementPoints"};
    return mode >= 0 && mode < 8 ? names[mode] : "Unspecified";
}

void AppendPointsShapeToXY(PointsShape& points, std::vector<float>& outXY)
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

void RefreshFindlineDisplaySnapshot(ManualTestContext& context,
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

std::string BuildFindlineMeasureHint(const RuntimeObjectView& object)
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

void RefreshFindlineMeasureSnapshot(RuntimeObjectView& object,
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

bool ApplyRuntimeFindlineWHgap(
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

bool ResolveDebugIntValue(ManualTestContext& context, const std::string& token, int& value)
{
    const std::string key = TrimLine(token);
    const auto found = context.runtime_int_vars.find(key);
    if (found != context.runtime_int_vars.end()) { value = found->second; return true; }
    char* end = nullptr; const long parsed = std::strtol(key.c_str(), &end, 10);
    if (end == key.c_str() || *end != '\0') return false;
    value = static_cast<int>(parsed); return true;
}

bool TryExecuteFindlineSetline(ManualTestContext& context,
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
        if (!ResolveDebugIntValue(context, call.args[static_cast<std::size_t>(i)], values[i]))
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

bool TryExecuteFindlineParamMethod(ManualTestContext& context,
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

        if (!ResolveDebugIntValue(context, call.args[0], wgap) ||
            !ResolveDebugIntValue(context, call.args[1], hgap))
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

        if (!ResolveDebugIntValue(context, call.args[0], borw) ||
            !ResolveDebugIntValue(context, call.args[1], minArea) ||
            !ResolveDebugIntValue(context, call.args[2], maxArea))
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
        if (!ResolveDebugIntValue(context, call.args[0], profile))
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
    if (!ResolveDebugIntValue(context, call.args[0], value))
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

bool TryExecuteFindlineRuntimeMethod(ManualTestContext& context, int lineIndex, const std::string& statement)
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

bool TryExecutePendingRuntimeMethod(ManualTestContext& context,
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

bool TryExecuteGetResultBinding(ManualTestContext& context,
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

bool TryExecuteImageCopyFromMat(ManualTestContext& context,
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
