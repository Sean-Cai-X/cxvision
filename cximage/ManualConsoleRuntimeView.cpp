#include "ManualConsoleRuntimeView.h"

#include <sstream>

RuntimeObjectView* FindRuntimeObjectByName(ManualTestContext& context,
    const std::string& name)
{
    for (RuntimeObjectView& object : context.runtime_objects)
    {
        if (object.name == name)
            return &object;
    }

    return nullptr;
}

const RuntimeObjectView* FindRuntimeObjectByName(const ManualTestContext& context,
    const std::string& name)
{
    for (const RuntimeObjectView& object : context.runtime_objects)
    {
        if (object.name == name)
            return &object;
    }

    return nullptr;
}

RuntimeObjectView* FindRuntimeObject(ManualTestContext& context,
    const std::string& name)
{
    for (RuntimeObjectView& object : context.runtime_objects)
    {
        if (object.name == name)
            return &object;
    }

    return nullptr;
}

bool RuntimeObjectIsType(ManualTestContext& context,
    const std::string& objectName,
    const std::string& expectedType)
{
    RuntimeObjectView* object = FindRuntimeObjectByName(context, objectName);

    if (object == nullptr)
        return false;

    return object->type == expectedType;
}

RuntimeObjectView& EnsureRuntimeObject(ManualTestContext& context,
    const std::string& name,
    const std::string& type,
    int declaredLine)
{
    if (RuntimeObjectView* existing = FindRuntimeObject(context, name))
        return *existing;

    RuntimeObjectView object;
    object.name = name;
    object.type = type;
    object.declared_line = declaredLine;
    object.exists_in_parser = true;
    object.runtime_state = "declared";
    object.last_runtime_status = "runtime_executed";
    object.last_method = "declare";
    object.last_update_line = declaredLine;
    object.display_summary = "declared";
    object.visualizable = false;
    object.visual_source = "runtime_object";
    object.stale = false;

    context.runtime_objects.push_back(object);
    return context.runtime_objects.back();
}

std::string BuildFindcircleGeometrySummary(const RuntimeObjectView& object)
{
    std::ostringstream ss;

    ss << "geometry: object=" << object.name;

    if (object.has_circle)
    {
        ss << " | roi_circle=("
           << object.circle_cx << "," << object.circle_cy
           << ", inner=" << object.circle_inner
           << ", r=" << object.circle_radius << ")";
    }
    else
    {
        ss << " | roi_circle=(none)";
    }

    ss << " | measure_points_count=" << object.measure_points_count
       << " | valid_points_count=" << object.valid_points_count;

    if (object.has_fit_result)
    {
        ss << " | fit_circle=("
           << object.fit_cx << "," << object.fit_cy
           << ", r=" << object.fit_radius << ")"
           << " | avgdist=" << object.fit_avgdist;
    }
    else
    {
        ss << " | fit_circle=(none)";
    }

    ss << " | has_result_measure="
       << (object.has_result_measure ? "true" : "false")
       << " | roi_outer_polyline=" << (object.has_circle_roi_outer_polyline ? "true" : "false")
       << " | roi_inner_polyline=" << (object.has_circle_roi_inner_polyline ? "true" : "false")
       << " | roi_segments=" << object.circle_roi_segment_count
       << " | fit_circle_polyline=" << (object.has_fit_circle_polyline ? "true" : "false")
       << " | fit_segments=" << object.fit_circle_segment_count
       << " | display_version=" << object.display_version
       << " | circle_geometry_ready="
       << (object.circle_measure_geometry_ready ? "true" : "false")
       << " | circle_dirty="
       << (object.circle_measure_geometry_dirty ? "true" : "false")
       << " | circle_scan_lines="
       << object.circle_scan_line_count
       << " | circle_scan_len="
       << object.circle_scan_line_length
       << " | circle_process_w="
       << object.circle_process_width
       << " | circle_measure_source="
       << object.circle_measure_source
       << " | circle_failure_stage="
       << object.circle_measure_failure_stage;

    return ss.str();
}

std::string BuildFindlineGeometrySummary(const RuntimeObjectView& object)
{
    std::ostringstream ss;

    ss << "geometry: object=" << object.name;

    if (object.has_line_roi)
    {
        ss << " | line_roi=("
           << object.line_x0 << "," << object.line_y0
           << ")->(" << object.line_x1 << "," << object.line_y1 << ")";
    }
    else
    {
        ss << " | line_roi=(none)";
    }

    ss << " | scan_half_width=" << object.line_scan_half_width
       << " | linegap=" << object.linegap
       << " | line_scan_box_xy=[" << object.line_scan_box_xy[0] << "," << object.line_scan_box_xy[1] << ","
       << object.line_scan_box_xy[2] << "," << object.line_scan_box_xy[3] << ","
       << object.line_scan_box_xy[4] << "," << object.line_scan_box_xy[5] << ","
       << object.line_scan_box_xy[6] << "," << object.line_scan_box_xy[7] << "]"
       << " | measure_points_count=" << object.line_measure_points_count
       << " | valid_points_count=" << object.valid_line_points_count
       << " | pointsw=" << object.line_pointsw_count
       << " | pointsh=" << object.line_pointsh_count
       << " | seek_points=" << object.line_seek_points_count
       << " | edgebands=" << object.line_edgeband_count
       << " | chain=" << object.line_chain_length
       << " | measure_failure_stage=" << object.line_measure_failure_stage
       << " | measure_detail=" << object.line_measure_failure_hint
       << " | result_status=" << object.line_result_status
       << " | result_reason=" << object.line_result_reason
       << " | image_ready=" << (object.line_measure_image_ready ? "true" : "false")
       << " | image_size=" << object.line_measure_image_width
       << "x" << object.line_measure_image_height
       << "x" << object.line_measure_image_channels
       << " | roi_intersects_image="
       << (object.line_measure_roi_intersects_image ? "true" : "false")
       << " | threshold=" << object.line_measure_threshold
       << " | max_gradient=" << object.line_measure_max_gradient
       << " | profile_count=" << object.line_measure_profile_count
       << " | sampled_pixels=" << object.line_measure_sampled_pixel_count
       << " | measure_source=" << object.line_measure_source
       << " | fallback_used="
       << (object.line_measure_fallback_used ? "true" : "false")
       << " | original_points="
       << object.line_measure_original_point_count
       << " | original_edgebands="
       << object.line_measure_original_edgeband_count
       << " | original_chain="
       << object.line_measure_original_chain_length
       << " | fit_mode=" << object.line_fit_mode
       << " | fit_status=" << object.line_fit_status
       << " | has_line_scan_box=" << (object.has_line_scan_box ? "true" : "false")
       << " | display_version=" << object.display_version
       << " | geometry_request_valid=" << (object.line_measure_geometry_request_valid ? "true" : "false")
       << " | geometry_ready=" << (object.line_measure_geometry_ready ? "true" : "false")
       << " | geometry_dirty=" << (object.line_measure_geometry_dirty ? "true" : "false")
       << " | geometry_half_width=" << object.line_measure_geometry_half_width
       << " | scan_w=" << object.line_original_scan_w_count
       << " | scan_h=" << object.line_original_scan_h_count
       << " | scan_w_len=" << object.line_original_scan_w_length
       << " | scan_h_len=" << object.line_original_scan_h_length
       << " | process_w=" << object.line_original_process_width
       << " | backimage_ready="
       << (object.line_measure_backimage_ready ? "true" : "false")
       << " | findobject_ready="
       << (object.line_measure_findobject_ready ? "true" : "false")
       << " | findobject_called="
       << (object.line_measure_findobject_called ? "true" : "false")
       << " | binary_foreground="
       << object.line_measure_binary_foreground_pixels
       << " | filter_profile=" << object.line_measure_filter_profile
       << " | effective_filter_borw="
       << object.line_measure_effective_filter_borw
       << " | effective_filter_min="
       << object.line_measure_effective_filter_min
       << " | effective_filter_max="
       << object.line_measure_effective_filter_max
       << " | component_total="
       << object.line_findobject_component_total
       << " | component_accepted="
       << object.line_findobject_component_accepted
       << " | component_rejected_min="
       << object.line_findobject_component_rejected_by_min
       << " | component_rejected_max="
       << object.line_findobject_component_rejected_by_max
       << " | component_rejected_borw="
       << object.line_findobject_component_rejected_by_borw
       << " | result_empty_reason="
       << object.line_measure_result_empty_reason;

    return ss.str();
}

std::string BuildFindSegmentationGeometrySummary(const RuntimeObjectView& object)
{
    std::ostringstream ss;

    ss << "segmentation: object=" << object.name
       << " | backend=" << object.segmentation_backend
       << " | backend_status=" << object.segmentation_backend_status
       << " | status_code=" << object.segmentation_status_code
       << " | runtime_state=" << object.runtime_state
       << " | device=" << object.segmentation_device
       << " | contour_count=" << object.segmentation_contour_count
       << " | primary_area=" << object.segmentation_primary_area
       << " | result_ref=" << object.segmentation_result_ref
       << " | mask_ref=" << object.segmentation_mask_ref
       << " | contour_ref=" << object.segmentation_contour_ref
       << " | overlay_ref=" << object.segmentation_overlay_ref
       << " | libtorch_contract="
       << (object.segmentation_has_libtorch_contract ? "true" : "false")
       << " | real_mask_attach="
       << (object.segmentation_real_mask_attach_ready ? "true" : "false");

    if (!object.segmentation_reason.empty())
        ss << " | reason=" << object.segmentation_reason;

    return ss.str();
}

std::string BuildFindellipseGeometrySummary(const RuntimeObjectView& object)
{
    std::ostringstream ss;

    ss << "geometry: object=" << object.name;

    if (object.has_ellipse_roi)
    {
        ss << " | roi_ellipse=(cx=" << object.ellipse_cx
           << ", cy=" << object.ellipse_cy
           << ", rx=" << object.ellipse_rx
           << ", ry=" << object.ellipse_ry << ")";
    }
    else
    {
        ss << " | roi_ellipse=(none)";
    }

    ss << " | measure_points_count=" << object.measure_points_count
       << " | valid_points_count=" << object.valid_points_count
       << " | fit_ellipse="
       << (object.has_fit_ellipse ? "true" : "false");

    if (object.has_fit_ellipse)
    {
        ss << " | fit_ellipse_result=(cx=" << object.fit_ellipse_cx
           << ", cy=" << object.fit_ellipse_cy
           << ", rx=" << object.fit_ellipse_rx
           << ", ry=" << object.fit_ellipse_ry
           << ", angle=" << object.fit_ellipse_angle_deg << ")"
           << " | avgdist=" << object.fit_ellipse_avgdist;
    }

    ss
       << " | result_status=" << object.ellipse_result_status
       << " | result_reason=" << object.ellipse_result_reason;

    return ss.str();
}

std::string BuildGeometrySummary(const RuntimeObjectView& object)
{
    if (object.type == "Findline")
        return BuildFindlineGeometrySummary(object);
    if (object.type == "Findellipse")
        return BuildFindellipseGeometrySummary(object);
    if (object.type == "FindSegmentation")
        return BuildFindSegmentationGeometrySummary(object);
    if (object.type == "fastmatch")
    {
        std::ostringstream ss;
        ss << "geometry: object=" << object.name
           << " | tool=fastmatch"
           << " | model_points=" << object.fastmatch_model_point_count
           << " | learnA=" << object.fastmatch_learn_a_count
           << " | learnB=" << object.fastmatch_learn_b_count
           << " | learnA2=" << object.fastmatch_learn_a2_count
           << " | learnB2=" << object.fastmatch_learn_b2_count
           << " | patternA=" << object.fastmatch_pattern_a_count
           << " | patternB=" << object.fastmatch_pattern_b_count
           << " | candidates=" << object.fastmatch_candidate_count
           << " | best_score=" << object.fastmatch_best_score
           << " | result_ref="
           << (object.fastmatch_result_ref.empty() ? "(none)" : object.fastmatch_result_ref)
           << " | status="
           << (object.fastmatch_status.empty() ? "(none)" : object.fastmatch_status);
        return ss.str();
    }

    return BuildFindcircleGeometrySummary(object);
}

std::string BuildFindcircleOverlaySummary(const ManualTestContext& context,
    const RuntimeObjectView& object)
{
    std::ostringstream ss;

    ss << "image overlay:"
       << " green_roi_circle=" << (object.has_circle ? "true" : "false")
       << " | green_roi_polyline=" << (object.has_circle_roi_outer_polyline ? "true" : "false")
       << " | red_measure_points=" << object.valid_points_count
       << " | yellow_fit_circle=" << (object.has_fit_result ? "true" : "false")
       << " | yellow_fit_polyline=" << (object.has_fit_circle_polyline ? "true" : "false")
       << " | source_preview_enabled=" << (context.source_preview_enabled ? "true" : "false")
       << " | manual_elements_count=" << context.manual_elements_count;

    return ss.str();
}

std::string BuildFindlineOverlaySummary(const ManualTestContext& context,
    const RuntimeObjectView& object)
{
    std::ostringstream ss;

    ss << "image overlay:"
       << " green_line_roi=" << (object.has_line_roi ? "true" : "false")
       << " | green_line_scan_box=" << (object.has_line_scan_box ? "true" : "false")
       << " | blue_seek_points=" << object.line_seek_points_count
       << " | red_measure_points=" << object.valid_line_points_count
       << " | yellow_fit_line=" << (object.has_fit_line ? "true" : "false")
       << " | fitline_pending="
       << ((object.runtime_state == "fitline_pending_binding" ||
            object.runtime_state == "fitline_pending_implementation")
               ? "true"
               : "false")
       << " | source_preview_enabled=false"
       << " | manual_elements_count=0";

    return ss.str();
}

std::string BuildFindSegmentationOverlaySummary(const ManualTestContext& context,
    const RuntimeObjectView& object)
{
    std::ostringstream ss;

    ss << "image overlay:"
       << " prompt_roi=true"
       << " | boundary_polyline="
       << (object.segmentation_contour_count > 0 ? "true" : "false")
       << " | boundary_bbox="
       << (object.segmentation_contour_count > 0 ? "true" : "false")
       << " | editable_roi=true"
       << " | editable_result=false"
       << " | stale=" << (object.stale ? "true" : "false")
       << " | source_preview_enabled="
       << (context.source_preview_enabled ? "true" : "false")
       << " | manual_elements_count=" << context.manual_elements_count;

    return ss.str();
}

std::string BuildFindellipseOverlaySummary(const ManualTestContext& context,
    const RuntimeObjectView& object)
{
    std::ostringstream ss;

    ss << "image overlay:"
       << " green_roi_ellipse=" << (object.has_ellipse_roi ? "true" : "false")
       << " | red_measure_points=" << object.valid_points_count
       << " | yellow_fit_ellipse=" << (object.has_fit_ellipse ? "true" : "false")
       << " | fitellipse_pending="
       << (object.has_fit_ellipse ? "false" : "true")
       << " | source_preview_enabled="
       << (context.source_preview_enabled ? "true" : "false")
       << " | manual_elements_count=" << context.manual_elements_count;

    return ss.str();
}

std::string BuildOverlaySummary(const ManualTestContext& context,
    const RuntimeObjectView& object)
{
    if (object.type == "Findline")
        return BuildFindlineOverlaySummary(context, object);
    if (object.type == "Findellipse")
        return BuildFindellipseOverlaySummary(context, object);
    if (object.type == "FindSegmentation")
        return BuildFindSegmentationOverlaySummary(context, object);
    if (object.type == "fastmatch")
    {
        std::ostringstream ss;
        ss << "image overlay:"
           << " learn_roi=true"
           << " | search_roi=true"
           << " | model_points=" << object.fastmatch_model_point_count
           << " | learnA=" << object.fastmatch_learn_a_count
           << " | learnB=" << object.fastmatch_learn_b_count
           << " | learnA2=" << object.fastmatch_learn_a2_count
           << " | learnB2=" << object.fastmatch_learn_b2_count
           << " | patternA=" << object.fastmatch_pattern_a_count
           << " | patternB=" << object.fastmatch_pattern_b_count
           << " | candidate_boxes=" << object.fastmatch_candidate_count
           << " | stale=" << (object.stale ? "true" : "false")
           << " | source_preview_enabled="
           << (context.source_preview_enabled ? "true" : "false")
           << " | manual_elements_count=" << context.manual_elements_count;
        return ss.str();
    }

    return BuildFindcircleOverlaySummary(context, object);
}

void UpdateFindcircleDebugSnapshot(ManualTestContext& context,
    const RuntimeObjectView& object,
    int lineNo,
    const std::string& statement)
{
    context.geometry_summary = BuildGeometrySummary(object);
    context.image_overlay_summary = BuildOverlaySummary(context, object);

    std::ostringstream ss;

    ss << object.type << " Debug Snapshot Summary\n"
        << "script_path: " << context.loaded_script_path << "\n"
        << "flow_block_id: cximage_find_circle_explore.N0\n"
        << "line: " << lineNo << "\n"
        << "statement: " << statement << "\n"
        << "object: " << object.name << "\n"
        << "runtime_state: " << object.runtime_state << "\n"
        << "last_method: " << object.last_method << "\n"
        << context.geometry_summary << "\n"
        << context.image_overlay_summary << "\n";

    if (!context.current_result_ref.name.empty())
    {
        ss << "result_ref: "
            << context.current_result_ref.name
            << " = "
            << context.current_result_ref.value
            << " | status="
            << context.current_result_ref.status
            << "\n";
    }

    context.findcircle_debug_snapshot_summary = ss.str();
}

void RefreshSnapshotFromCurrentResultRef(ManualTestContext& context)
{
    if (context.current_result_ref.source_object.empty())
        return;

    RuntimeObjectView* object =
        FindRuntimeObjectByName(context, context.current_result_ref.source_object);

    if (object == nullptr)
        return;

    UpdateFindcircleDebugSnapshot(
        context,
        *object,
        context.current_result_ref.line_no,
        context.current_result_ref.name + " = " + context.current_result_ref.value);
}

std::string BuildDebugCursorText(const ManualTestContext& context)
{
    if (context.run_state == "runtime_finished" ||
        context.current_line >= static_cast<int>(context.line_views.size()))
    {
        return "END";
    }

    if (context.current_line >= 0 &&
        context.current_line < static_cast<int>(context.line_views.size()))
    {
        const ScriptLineView& line =
            context.line_views[static_cast<std::size_t>(context.current_line)];

        std::ostringstream ss;
        ss << "line_no=" << line.line_no
            << ", index=" << context.current_line;
        return ss.str();
    }

    return "INVALID";
}

int LastExecutedLineNo(const ManualTestContext& context)
{
    int last = 0;

    for (const ScriptLineView& line : context.line_views)
    {
        if (line.status == "runtime_executed" ||
            line.status == "runtime_deferred" ||
            line.status == "control_true" ||
            line.status == "control_false" ||
            line.status == "structural")
        {
            last = line.line_no;
        }
    }

    return last;
}

std::string ModuleForType(const std::string& type)
{
    if (type.rfind("Torch", 0) == 0) return "torch";
    if (type.rfind("Mlpack", 0) == 0) return "mlpack";
    if (type.rfind("Ensmallen", 0) == 0) return "ensmallen";
    if (type == "Image" || type.rfind("Find", 0) == 0 || type == "fastmatch" ||
        type == "FormfitGauge" || type == "CxOverlay" || type == "CircleRingGauge") return "cximage";
    return "cxscript";
}

bool IsObjectType(const std::string& type)
{
    return ModuleForType(type) != "cxscript";
}

std::string ModuleForStatement(const std::string& statement)
{
    if (statement.find("torch.") != std::string::npos || statement.find("Torch") != std::string::npos) return "torch";
    if (statement.find("mlpack.") != std::string::npos || statement.find("Mlpack") != std::string::npos) return "mlpack";
    if (statement.find("ensmallen.") != std::string::npos || statement.find("Ensmallen") != std::string::npos) return "ensmallen";
    if (statement.find("cximage.") != std::string::npos || statement.find("Image") != std::string::npos ||
        statement.find("Find") != std::string::npos || statement.find("fastmatch") != std::string::npos) return "cximage";
    return "cxscript";
}
