#include "pch.h"
#include "CxScriptRuntimeResultCapture.h"
#include "Findline.h"
#include "Findcircle.h"
#include "Findellipse.h"
#include "FindRect.h"
#include "FindSegmentation.h"
#include "FastMatch.h"
#include "ParserClass.h"
#include "ImageAnnotationLayer.h"
#include "shapebase.h"

#include <unordered_set>

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

bool CaptureFindellipseResult(
    Findellipse& tool,
    const std::string& object_name,
    CxScriptToolResultCapture& output)
{
    output.type = "Findellipse";
    output.name = object_name;
    output.owner_ref = object_name;

    FindellipseDisplaySnapshot snapshot;
    const bool has_snapshot = tool.getdisplaysnapshot(snapshot);
    output.valid_points_count = snapshot.measure_points_count;
    output.has_fit_ellipse = tool.hasfitresult() != 0.0;
    output.ellipse_cx = tool.getresultcentx();
    output.ellipse_cy = tool.getresultcenty();
    output.ellipse_radius_x = tool.getresultradiusx();
    output.ellipse_radius_y = tool.getresultradiusy();
    output.ellipse_angle_deg = tool.getresultangle();
    output.avgdist = tool.getavgdist();
    output.failure_stage = output.has_fit_ellipse
        ? std::string()
        : (snapshot.measure_points_count > 0 ? "fitellipse" : "measure_points");

    output.ellipse_scan_candidate_lines = snapshot.scan_candidate_lines;
    output.ellipse_scan_total_candidates = snapshot.scan_total_candidates;
    output.ellipse_scan_accepted_points_before_gate = snapshot.scan_accepted_points_before_gate;
    output.ellipse_accepted_min_boundary_ratio = snapshot.accepted_min_boundary_ratio;
    output.ellipse_accepted_max_boundary_ratio = snapshot.accepted_max_boundary_ratio;
    output.ellipse_accepted_avg_boundary_ratio = snapshot.accepted_avg_boundary_ratio;
    output.ellipse_candidate_policy = snapshot.candidate_policy;

    output.ellipse_scan_lines_outside_roi_count = snapshot.scan_lines_outside_roi_count;
    output.ellipse_scan_lines_cross_outside_ellipse_count = snapshot.scan_lines_cross_outside_ellipse_count;
    output.ellipse_scan_endpoint_norm_min = snapshot.scan_endpoint_norm_min;
    output.ellipse_scan_endpoint_norm_avg = snapshot.scan_endpoint_norm_avg;
    output.ellipse_scan_endpoint_norm_max = snapshot.scan_endpoint_norm_max;
    output.ellipse_accepted_points_outside_ellipse_count = snapshot.accepted_points_outside_ellipse_count;
    output.ellipse_accepted_point_norm_min = snapshot.accepted_point_norm_min;
    output.ellipse_accepted_point_norm_avg = snapshot.accepted_point_norm_avg;
    output.ellipse_accepted_point_norm_max = snapshot.accepted_point_norm_max;
    output.ellipse_rejected_boundary_band_candidate_count =
        snapshot.rejected_boundary_band_candidate_count;
    output.ellipse_rejected_boundary_band_norm_min =
        snapshot.rejected_boundary_band_norm_min;
    output.ellipse_rejected_boundary_band_norm_avg =
        snapshot.rejected_boundary_band_norm_avg;
    output.ellipse_rejected_boundary_band_norm_max =
        snapshot.rejected_boundary_band_norm_max;
    output.ellipse_scan_geometry_policy = snapshot.scan_geometry_policy;

    if (!snapshot.measure_failure_stage.empty())
        output.failure_stage = snapshot.measure_failure_stage;

    if (!has_snapshot)
        output.reason = "Findellipse display snapshot is empty";
    else if (!snapshot.measure_failure_stage.empty())
    {
        output.reason = snapshot.measure_failure_reason;
        if (snapshot.scan_candidate_lines > 0)
        {
            output.reason += " candidate_lines=" + std::to_string(snapshot.scan_candidate_lines);
            output.reason += " total_candidates=" + std::to_string(snapshot.scan_total_candidates);
            output.reason += " accepted_before_gate=" + std::to_string(snapshot.scan_accepted_points_before_gate);
            output.reason += " boundary_ratio=" + std::to_string(snapshot.accepted_min_boundary_ratio);
            output.reason += "/" + std::to_string(snapshot.accepted_avg_boundary_ratio);
            output.reason += "/" + std::to_string(snapshot.accepted_max_boundary_ratio);
            output.reason += " candidate_policy=" + snapshot.candidate_policy;
        }
        if (!snapshot.scan_geometry_policy.empty())
        {
            output.reason += " scan_lines=" + std::to_string(snapshot.scan_line_count);
            output.reason += " scan_len=" + std::to_string(snapshot.scan_line_length);
            output.reason += " scan_outside_lines=" + std::to_string(snapshot.scan_lines_cross_outside_ellipse_count);
            output.reason += " endpoint_norm=" + std::to_string(snapshot.scan_endpoint_norm_min);
            output.reason += "/" + std::to_string(snapshot.scan_endpoint_norm_avg);
            output.reason += "/" + std::to_string(snapshot.scan_endpoint_norm_max);
            output.reason += " accepted_outside=" + std::to_string(snapshot.accepted_points_outside_ellipse_count);
            output.reason += " accepted_norm=" + std::to_string(snapshot.accepted_point_norm_min);
            output.reason += "/" + std::to_string(snapshot.accepted_point_norm_avg);
            output.reason += "/" + std::to_string(snapshot.accepted_point_norm_max);
            output.reason += " rejected_boundary_band=" +
                std::to_string(snapshot.rejected_boundary_band_candidate_count);
            output.reason += " rejected_norm=" +
                std::to_string(snapshot.rejected_boundary_band_norm_min);
            output.reason += "/" + std::to_string(snapshot.rejected_boundary_band_norm_avg);
            output.reason += "/" + std::to_string(snapshot.rejected_boundary_band_norm_max);
            output.reason += " scan_policy=" + snapshot.scan_geometry_policy;
        }
    }
    else if (!output.has_fit_ellipse && snapshot.measure_points_count > 0)
        output.reason = "Findellipse produced measure points, but fitellipse result is unavailable.";
    else if (!output.has_fit_ellipse)
        output.reason = "Findellipse produced zero measure points.";

    ImageAnnotationLayer layer;
    tool.PublishDisplayShapes(layer, output.owner_ref);
    CopyShapeElementsToSnapshots(layer, output.shapes);

    return true;
}

bool CaptureFindRectResult(
    FindRect& tool,
    const std::string& object_name,
    CxScriptToolResultCapture& output)
{
    output.type = "FindRect";
    output.name = object_name;
    output.owner_ref = object_name;

    output.result_rect_count = tool.getresultobjsnum();
    output.has_result_rect = tool.hasresult();
    output.valid_points_count = output.result_rect_count;
    output.findrect_seed_valid = tool.getdebugseedvalid() != 0;
    output.findrect_top_valid = tool.getdebugtopvalid() != 0;
    output.findrect_bottom_valid = tool.getdebugbottomvalid() != 0;
    output.findrect_left_valid = tool.getdebugleftvalid() != 0;
    output.findrect_right_valid = tool.getdebugrightvalid() != 0;
    output.findrect_top_points = tool.getdebugtoppoints();
    output.findrect_bottom_points = tool.getdebugbottompoints();
    output.findrect_left_points = tool.getdebugleftpoints();
    output.findrect_right_points = tool.getdebugrightpoints();
    output.findrect_coarse_score = tool.getdebugcoarsescore();
    output.findrect_refine_score = tool.getdebugrefinescore();
    output.failure_stage = output.has_result_rect
        ? std::string()
        : tool.getfailurestage();
    if (output.failure_stage.empty() && !output.has_result_rect)
        output.failure_stage = "result_rect";

    if (!output.has_result_rect)
        output.reason = "FindRect result unavailable";

    ImageAnnotationLayer layer;
    tool.PublishDisplayShapes(layer, output.owner_ref);
    CopyShapeElementsToSnapshots(layer, output.shapes);

    return true;
}

bool CaptureFastMatchResult(
    fastmatch& tool,
    const std::string& object_name,
    CxScriptToolResultCapture& output)
{
    output.type = "FastMatch";
    output.name = object_name;
    output.owner_ref = object_name;

    output.model_point_count = tool.getmodelpointcount();
    output.fastmatch_model_width = tool.getmodelwidth();
    output.fastmatch_model_height = tool.getmodelheight();
    output.fastmatch_pattern_a_count = tool.getpatternapointcount();
    output.fastmatch_pattern_b_count = tool.getpatternbpointcount();
    output.fastmatch_pattern_a_x = tool.getpatternax();
    output.fastmatch_pattern_a_y = tool.getpatternay();
    output.fastmatch_pattern_a_width = tool.getpatternawidth();
    output.fastmatch_pattern_a_height = tool.getpatternaheight();
    output.fastmatch_pattern_b_x = tool.getpatternbx();
    output.fastmatch_pattern_b_y = tool.getpatternby();
    output.fastmatch_pattern_b_width = tool.getpatternbwidth();
    output.fastmatch_pattern_b_height = tool.getpatternbheight();
    output.candidate_count = tool.getresultcandidatecount();
    output.best_score = tool.getresultbestscore();
    output.has_best_result = tool.getresultbestindex() >= 0;
    output.has_result_box = tool.getresultnum(0) > 0.0;
    output.valid_points_count = output.candidate_count;
    output.fastmatch_match_call_count = tool.getmatchcallcount();
    output.fastmatch_match_ab_call_count = tool.getmatchabcallcount();
    output.fastmatch_match_sample_ab_call_count = tool.getmatchsampleabcallcount();
    output.fastmatch_match_last_stage = tool.getmatchlaststage();
    output.fastmatch_match_image_width = tool.getmatchimagewidth();
    output.fastmatch_match_image_height = tool.getmatchimageheight();
    output.fastmatch_match_rect_x0 = tool.getmatchrectx0();
    output.fastmatch_match_rect_y0 = tool.getmatchrecty0();
    output.fastmatch_match_rect_x1 = tool.getmatchrectx1();
    output.fastmatch_match_rect_y1 = tool.getmatchrecty1();
    output.fastmatch_raw_probe_count = tool.getrawmatchprobecount();
    output.fastmatch_raw_threshold_hit_count = tool.getrawmatchthresholdhitcount();
    output.fastmatch_result_to_list_count = tool.getresulttolistcallcount();
    output.fastmatch_candidate_insert_count = tool.getresultcandidateinsertcount();
    output.fastmatch_candidate_replace_count = tool.getresultcandidatereplacecount();
    output.fastmatch_candidate_reject_count = tool.getresultcandidaterejectcount();

    if (output.model_point_count <= 0)
        output.failure_stage = "model_points";
    else if (output.candidate_count <= 0)
        output.failure_stage = "match_candidates";

    if (!output.failure_stage.empty())
        output.reason = "FastMatch result unavailable";

    ImageAnnotationLayer layer;
    tool.PublishDisplayShapes(layer, output.owner_ref);
    CopyShapeElementsToSnapshots(layer, output.shapes);

    return true;
}

bool CaptureFindSegmentationResult(
    FindSegmentation& tool,
    const std::string& object_name,
    CxScriptToolResultCapture& output)
{
    output.type = "FindSegmentation";
    output.name = object_name;
    output.owner_ref = object_name;

    output.segmentation_status_code = tool.status_code();
    output.segmentation_contour_count = tool.get_contour_count();
    output.segmentation_primary_area = tool.get_primary_area();
    output.segmentation_result_ref = tool.get_result();
    output.segmentation_mask_ref = tool.get_mask_ref();
    output.segmentation_contour_ref = tool.get_contour_ref();
    output.segmentation_overlay_ref = tool.get_overlay_ref();

    output.valid_points_count = output.segmentation_contour_count;
    output.has_result_rect = output.segmentation_contour_count > 0;
    output.result_rect_count = output.segmentation_contour_count;
    output.avgdist = output.segmentation_primary_area;
    output.failure_stage = output.segmentation_contour_count > 0 ? std::string() : "boundary_contours";
    if (!output.failure_stage.empty())
        output.reason = tool.m_reason.empty() ? "FindSegmentation boundary unavailable" : tool.m_reason;

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
    capture.has_fit_ellipse = capture.has_fit_ellipse || tool.has_fit_ellipse;
    capture.has_result_rect = capture.has_result_rect || tool.has_result_rect;
    capture.budget_exceeded = capture.budget_exceeded || tool.budget_exceeded;
    capture.avgdist = tool.avgdist;
    if (tool.has_fit_ellipse)
    {
        capture.ellipse_cx = tool.ellipse_cx;
        capture.ellipse_cy = tool.ellipse_cy;
        capture.ellipse_radius_x = tool.ellipse_radius_x;
        capture.ellipse_radius_y = tool.ellipse_radius_y;
        capture.ellipse_angle_deg = tool.ellipse_angle_deg;
    }
    capture.ellipse_scan_candidate_lines = tool.ellipse_scan_candidate_lines;
    capture.ellipse_scan_total_candidates = tool.ellipse_scan_total_candidates;
    capture.ellipse_scan_accepted_points_before_gate = tool.ellipse_scan_accepted_points_before_gate;
    capture.ellipse_accepted_min_boundary_ratio = tool.ellipse_accepted_min_boundary_ratio;
    capture.ellipse_accepted_max_boundary_ratio = tool.ellipse_accepted_max_boundary_ratio;
    capture.ellipse_accepted_avg_boundary_ratio = tool.ellipse_accepted_avg_boundary_ratio;
    capture.ellipse_candidate_policy = tool.ellipse_candidate_policy;
    capture.ellipse_scan_lines_cross_outside_ellipse_count = tool.ellipse_scan_lines_cross_outside_ellipse_count;
    capture.ellipse_scan_endpoint_norm_min = tool.ellipse_scan_endpoint_norm_min;
    capture.ellipse_scan_endpoint_norm_avg = tool.ellipse_scan_endpoint_norm_avg;
    capture.ellipse_scan_endpoint_norm_max = tool.ellipse_scan_endpoint_norm_max;
    capture.ellipse_accepted_points_outside_ellipse_count = tool.ellipse_accepted_points_outside_ellipse_count;
    capture.ellipse_accepted_point_norm_min = tool.ellipse_accepted_point_norm_min;
    capture.ellipse_accepted_point_norm_avg = tool.ellipse_accepted_point_norm_avg;
    capture.ellipse_accepted_point_norm_max = tool.ellipse_accepted_point_norm_max;
    capture.ellipse_rejected_boundary_band_candidate_count =
        tool.ellipse_rejected_boundary_band_candidate_count;
    capture.ellipse_rejected_boundary_band_norm_min =
        tool.ellipse_rejected_boundary_band_norm_min;
    capture.ellipse_rejected_boundary_band_norm_avg =
        tool.ellipse_rejected_boundary_band_norm_avg;
    capture.ellipse_rejected_boundary_band_norm_max =
        tool.ellipse_rejected_boundary_band_norm_max;
    capture.ellipse_scan_geometry_policy = tool.ellipse_scan_geometry_policy;

    capture.result_rect_count += tool.result_rect_count;
    capture.model_point_count += tool.model_point_count;
    if (tool.fastmatch_model_width > 0)
        capture.fastmatch_model_width = tool.fastmatch_model_width;
    if (tool.fastmatch_model_height > 0)
        capture.fastmatch_model_height = tool.fastmatch_model_height;
    capture.fastmatch_pattern_a_count += tool.fastmatch_pattern_a_count;
    capture.fastmatch_pattern_b_count += tool.fastmatch_pattern_b_count;
    capture.fastmatch_pattern_a_x = tool.fastmatch_pattern_a_x;
    capture.fastmatch_pattern_a_y = tool.fastmatch_pattern_a_y;
    capture.fastmatch_pattern_a_width = tool.fastmatch_pattern_a_width;
    capture.fastmatch_pattern_a_height = tool.fastmatch_pattern_a_height;
    capture.fastmatch_pattern_b_x = tool.fastmatch_pattern_b_x;
    capture.fastmatch_pattern_b_y = tool.fastmatch_pattern_b_y;
    capture.fastmatch_pattern_b_width = tool.fastmatch_pattern_b_width;
    capture.fastmatch_pattern_b_height = tool.fastmatch_pattern_b_height;
    capture.candidate_count += tool.candidate_count;
    if (tool.best_score > capture.best_score)
        capture.best_score = tool.best_score;
    capture.has_result_box = capture.has_result_box || tool.has_result_box;
    capture.has_best_result = capture.has_best_result || tool.has_best_result;
    capture.fastmatch_match_call_count += tool.fastmatch_match_call_count;
    capture.fastmatch_match_ab_call_count += tool.fastmatch_match_ab_call_count;
    capture.fastmatch_match_sample_ab_call_count += tool.fastmatch_match_sample_ab_call_count;
    if (tool.fastmatch_match_last_stage > 0)
        capture.fastmatch_match_last_stage = tool.fastmatch_match_last_stage;
    capture.fastmatch_match_image_width = tool.fastmatch_match_image_width;
    capture.fastmatch_match_image_height = tool.fastmatch_match_image_height;
    capture.fastmatch_match_rect_x0 = tool.fastmatch_match_rect_x0;
    capture.fastmatch_match_rect_y0 = tool.fastmatch_match_rect_y0;
    capture.fastmatch_match_rect_x1 = tool.fastmatch_match_rect_x1;
    capture.fastmatch_match_rect_y1 = tool.fastmatch_match_rect_y1;
    capture.fastmatch_raw_probe_count += tool.fastmatch_raw_probe_count;
    capture.fastmatch_raw_threshold_hit_count += tool.fastmatch_raw_threshold_hit_count;
    capture.fastmatch_result_to_list_count += tool.fastmatch_result_to_list_count;
    capture.fastmatch_candidate_insert_count += tool.fastmatch_candidate_insert_count;
    capture.fastmatch_candidate_replace_count += tool.fastmatch_candidate_replace_count;
    capture.fastmatch_candidate_reject_count += tool.fastmatch_candidate_reject_count;
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
    capture.findrect_seed_valid = capture.findrect_seed_valid || tool.findrect_seed_valid;
    capture.findrect_top_valid = capture.findrect_top_valid || tool.findrect_top_valid;
    capture.findrect_bottom_valid = capture.findrect_bottom_valid || tool.findrect_bottom_valid;
    capture.findrect_left_valid = capture.findrect_left_valid || tool.findrect_left_valid;
    capture.findrect_right_valid = capture.findrect_right_valid || tool.findrect_right_valid;
    capture.findrect_top_points += tool.findrect_top_points;
    capture.findrect_bottom_points += tool.findrect_bottom_points;
    capture.findrect_left_points += tool.findrect_left_points;
    capture.findrect_right_points += tool.findrect_right_points;
    if (tool.findrect_coarse_score != 0.0)
        capture.findrect_coarse_score = tool.findrect_coarse_score;
    if (tool.findrect_refine_score != 0.0)
        capture.findrect_refine_score = tool.findrect_refine_score;
    if (tool.segmentation_status_code != 0)
        capture.segmentation_status_code = tool.segmentation_status_code;
    capture.segmentation_contour_count += tool.segmentation_contour_count;
    if (tool.segmentation_primary_area != 0.0)
        capture.segmentation_primary_area = tool.segmentation_primary_area;
    if (!tool.segmentation_result_ref.empty())
        capture.segmentation_result_ref = tool.segmentation_result_ref;
    if (!tool.segmentation_mask_ref.empty())
        capture.segmentation_mask_ref = tool.segmentation_mask_ref;
    if (!tool.segmentation_contour_ref.empty())
        capture.segmentation_contour_ref = tool.segmentation_contour_ref;
    if (!tool.segmentation_overlay_ref.empty())
        capture.segmentation_overlay_ref = tool.segmentation_overlay_ref;

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

        if (shape.semantic_role == "roi" ||
            shape.semantic_role == "learn_roi" ||
            shape.semantic_role == "search_roi")
        {
            capture.rendered_roi_count++;
        }

        if (shape.semantic_role == "scan")
            capture.rendered_scan_count++;

        if (shape.semantic_role == "measure_points")
        {
            capture.rendered_measure_points_count +=
                static_cast<int>(shape.points.size() / 2);
        }

        if (shape.semantic_role == "result" ||
            shape.semantic_role == "boundary" ||
            shape.semantic_role == "boundary_bbox")
            capture.rendered_result_count++;
    }
}

bool CaptureRuntimeToolResults(
    mu::CxParserRuntime& runtime,
    CxScriptExecutionCapture& capture,
    std::string& reason)
{
    bool supported_object_found = false;
    std::unordered_set<void*> captured_objects;

    const int findline_count = runtime.GetClassObjSum("Findline");

    for (int i = 0; i < findline_count; ++i)
    {
        Findline* tool = static_cast<Findline*>(
            runtime.GetClassObj("Findline", i));

        if (tool == nullptr)
            continue;
        if (!captured_objects.insert(tool).second)
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
        if (!captured_objects.insert(tool).second)
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

    const int findellipse_count = runtime.GetClassObjSum("Findellipse");

    for (int i = 0; i < findellipse_count; ++i)
    {
        Findellipse* tool = static_cast<Findellipse*>(
            runtime.GetClassObj("Findellipse", i));

        if (tool == nullptr)
            continue;
        if (!captured_objects.insert(tool).second)
            continue;

        supported_object_found = true;

        CxScriptToolResultCapture tool_capture;

        const std::string object_name =
            runtime.GetClassObjName("Findellipse", i);

        try
        {
            if (!CaptureFindellipseResult(*tool, object_name, tool_capture))
            {
                reason = "failed to capture Findellipse: " + object_name;
                return false;
            }
        }
        catch (...)
        {
            reason = "CaptureFindellipseResult crashed for: " + object_name;
            return false;
        }

        MergeToolCapture(tool_capture, capture);
    }

    const int findrect_count = runtime.GetClassObjSum("FindRect");

    for (int i = 0; i < findrect_count; ++i)
    {
        FindRect* tool = static_cast<FindRect*>(
            runtime.GetClassObj("FindRect", i));

        if (tool == nullptr)
            continue;
        if (!captured_objects.insert(tool).second)
            continue;

        supported_object_found = true;

        CxScriptToolResultCapture tool_capture;

        const std::string object_name =
            runtime.GetClassObjName("FindRect", i);

        try
        {
            if (!CaptureFindRectResult(*tool, object_name, tool_capture))
            {
                reason = "failed to capture FindRect: " + object_name;
                return false;
            }
        }
        catch (...)
        {
            reason = "CaptureFindRectResult crashed for: " + object_name;
            return false;
        }

        MergeToolCapture(tool_capture, capture);
    }

    const char* fastmatch_class_names[] = { "Match", "fastmatch" };
    for (const char* class_name : fastmatch_class_names)
    {
        const int fastmatch_count = runtime.GetClassObjSum(class_name);

        for (int i = 0; i < fastmatch_count; ++i)
        {
            fastmatch* tool = static_cast<fastmatch*>(
                runtime.GetClassObj(class_name, i));

            if (tool == nullptr)
                continue;
            if (!captured_objects.insert(tool).second)
                continue;

            supported_object_found = true;

            CxScriptToolResultCapture tool_capture;

            const std::string object_name =
                runtime.GetClassObjName(class_name, i);

            try
            {
                if (!CaptureFastMatchResult(*tool, object_name, tool_capture))
                {
                    reason = std::string("failed to capture ") + class_name + ": " + object_name;
                    return false;
                }
            }
            catch (...)
            {
                reason = std::string("CaptureFastMatchResult crashed for: ") + object_name;
                return false;
            }

            MergeToolCapture(tool_capture, capture);
        }
    }

    const int findseg_count = runtime.GetClassObjSum("FindSegmentation");

    for (int i = 0; i < findseg_count; ++i)
    {
        FindSegmentation* tool = static_cast<FindSegmentation*>(
            runtime.GetClassObj("FindSegmentation", i));

        if (tool == nullptr)
            continue;
        if (!captured_objects.insert(tool).second)
            continue;

        supported_object_found = true;

        CxScriptToolResultCapture tool_capture;

        const std::string object_name =
            runtime.GetClassObjName("FindSegmentation", i);

        try
        {
            if (!CaptureFindSegmentationResult(*tool, object_name, tool_capture))
            {
                reason = "failed to capture FindSegmentation: " + object_name;
                return false;
            }
        }
        catch (...)
        {
            reason = "CaptureFindSegmentationResult crashed for: " + object_name;
            return false;
        }

        MergeToolCapture(tool_capture, capture);
    }

    if (!supported_object_found)
    {
        reason = "no supported cximage runtime object found; expected one of Findline, Findcircle, Findellipse, FindRect, FindSegmentation, Match or fastmatch";
        return false;
    }

    reason.clear();
    return true;
}
