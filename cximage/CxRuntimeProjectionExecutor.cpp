#include "CxRuntimeProjectionExecutor.h"
#include "Findline.h"
#include "Findcircle.h"
#include "Findellipse.h"
#include "FindRect.h"
#include "FastMatch.h"
#include "Image.h"
#include "shapebase.h"
#include <filesystem>

namespace fs = std::filesystem;

namespace {

bool LoadProjectionImage(const std::string& path, Image& image, CxRuntimeProjectionResult& result)
{
    if (path.empty())
    {
        result.failure_stage = "image_load";
        result.reason = "image path is empty";
        return false;
    }

    if (!fs::exists(path))
    {
        result.failure_stage = "image_load";
        result.reason = "image not found: " + path;
        return false;
    }

    image.load(path.c_str());

    if (image.getWidth() == 0 || image.getHeight() == 0)
    {
        result.failure_stage = "image_decode";
        result.reason = "image exists but decode failed: " + path;
        return false;
    }

    return true;
}

void CountRoles(const ImageAnnotationLayer& layer, const std::string& owner_ref, CxRuntimeProjectionResult& result)
{
    result.role_counts.clear();
    for (const auto& elem : layer.ShapeElements())
    {
        if (elem.owner_ref != owner_ref)
            continue;
        result.role_counts[elem.semantic_role]++;
    }
}

void CapturePublishedShapes(const ImageAnnotationLayer& layer,
                            const std::string& owner_ref,
                            CxRuntimeProjectionResult& result)
{
    result.published_shapes.clear();
    for (const auto& elem : layer.ShapeElements())
    {
        if (elem.owner_ref != owner_ref)
            continue;

        CxShapeElementSnapshot snap;
        snap.stable_ref = elem.stable_ref;
        snap.owner_type = elem.owner_type;
        snap.owner_ref = elem.owner_ref;
        snap.semantic_role = elem.semantic_role;
        snap.editable = elem.editable;
        snap.result_element = elem.result_element;

        if (elem.shape)
        {
            CxShapeGeometrySnapshot geometry;
            if (elem.shape->snapshot(geometry))
            {
                snap.shape_kind = CxShapeKindName(geometry.kind);
                snap.center_x = geometry.center.x;
                snap.center_y = geometry.center.y;
                snap.radius = geometry.radius;
                snap.inner_radius = geometry.inner_radius;
                snap.half_width = geometry.half_width;
                snap.radius_x = geometry.radius_x;
                snap.radius_y = geometry.radius_y;
                snap.angle_deg = geometry.angle;
                snap.closed = geometry.closed;
                for (const auto& point : geometry.points)
                {
                    snap.points.push_back(point.x);
                    snap.points.push_back(point.y);
                }

                if (geometry.kind == CxShapeKind::Polyline && geometry.points.size() == 4)
                {
                    for (const auto& point : geometry.points)
                    {
                        snap.center_x += point.x;
                        snap.center_y += point.y;
                    }
                    snap.center_x /= 4.0;
                    snap.center_y /= 4.0;
                    const double dx01 = geometry.points[1].x - geometry.points[0].x;
                    const double dy01 = geometry.points[1].y - geometry.points[0].y;
                    const double dx12 = geometry.points[2].x - geometry.points[1].x;
                    const double dy12 = geometry.points[2].y - geometry.points[1].y;
                    snap.radius_x = std::hypot(dx01, dy01) / 2.0;
                    snap.radius_y = std::hypot(dx12, dy12) / 2.0;
                    snap.angle_deg = std::atan2(dy01, dx01) * 180.0 / M_PI;
                }
            }
        }
        result.published_shapes.push_back(std::move(snap));
    }
}

bool HasElementsForOwner(const ImageAnnotationLayer& layer, const std::string& owner_ref)
{
    for (const auto& elem : layer.ShapeElements())
    {
        if (elem.owner_ref == owner_ref)
            return true;
    }
    return false;
}

bool Fail(CxRuntimeProjectionResult& result, const std::string& stage, const std::string& reason)
{
    result.failure_stage = stage;
    result.reason = reason;
    return false;
}

bool ExecuteFindline(const CxRuntimeProjectionRequest& request,
                     ImageAnnotationLayer& layer,
                     CxRuntimeProjectionResult& result)
{
    result.owner_type = "Findline";
    result.owner_ref = request.owner_ref;

    if (request.owner_ref.empty())
        return Fail(result, "request_validation", "owner_ref is empty");

    if (request.roi_x0 == request.roi_x1 && request.roi_y0 == request.roi_y1)
        return Fail(result, "request_validation", "Findline axis length is zero");

    Findline tool;
    tool.SetWHgap(request.wgap, request.hgap);
    tool.setlinegap(request.linegap);
    tool.setline(
        static_cast<int>(request.roi_x0),
        static_cast<int>(request.roi_y0),
        static_cast<int>(request.roi_x1),
        static_cast<int>(request.roi_y1),
        request.tool_half_width);
    tool.setmethod(request.method);
    tool.setthre(request.threshold);

    if (request.filter_profile > 0)
        tool.setfilterprofile(request.filter_profile);

    if (request.require_algorithm_execution)
    {
        Image image;
        if (!LoadProjectionImage(request.image_path, image, result))
            return false;

        tool.measure(&image);
        result.executed = true;

        tool.fitline();

        result.valid_points_count = tool.getvalidpointcount();
        result.has_fit_line = tool.hasfitresult();
        result.fit_residual = tool.getavgdist();

        result.algorithm_ok =
            result.valid_points_count >= 2 &&
            result.has_fit_line;

        if (!result.algorithm_ok)
        {
            result.failure_stage =
                result.valid_points_count < 2
                    ? "measure_points"
                    : "fitline";

            result.reason =
                "Findline result unavailable: valid_points=" +
                std::to_string(result.valid_points_count) +
                ", has_fit_line=" +
                std::string(result.has_fit_line ? "true" : "false");
        }
    }

    const size_t before = layer.ShapeElements().size();
    tool.PublishDisplayShapes(layer, request.owner_ref);
    const size_t after = layer.ShapeElements().size();

    CapturePublishedShapes(layer, request.owner_ref, result);
    CountRoles(layer, request.owner_ref, result);

    result.publish_ok =
        after > before ||
        HasElementsForOwner(layer, request.owner_ref);

    return true;
}

bool ExecuteFindcircle(const CxRuntimeProjectionRequest& request,
                       ImageAnnotationLayer& layer,
                       CxRuntimeProjectionResult& result)
{
    result.owner_type = "Findcircle";
    result.owner_ref = request.owner_ref;

    if (request.owner_ref.empty())
        return Fail(result, "request_validation", "owner_ref is empty");

    const double dx = request.circle_px - request.circle_cx;
    const double dy = request.circle_py - request.circle_cy;

    if (std::hypot(dx, dy) < 2.0)
        return Fail(result, "request_validation", "Findcircle radius is too small");

    Findcircle tool;
    tool.setcircle(
        static_cast<int>(request.circle_cx),
        static_cast<int>(request.circle_cy),
        static_cast<int>(request.circle_px),
        static_cast<int>(request.circle_py));
    tool.Setgap(request.gap);
    tool.setlinegap(request.linegap);
    tool.setthre(request.threshold);
    tool.setmethod(request.method);

    if (request.require_algorithm_execution)
    {
        Image image;
        if (!LoadProjectionImage(request.image_path, image, result))
            return false;

        tool.measure(&image);
        result.executed = true;

        tool.fitcircle();

        result.valid_points_count = tool.getvalidpointcount();
        result.has_fit_circle = tool.hasfitresult();
        result.circle_radius = tool.getradius();
        result.fit_residual = tool.getavgdist();
        result.avgdist = result.fit_residual;

        result.algorithm_ok =
            result.valid_points_count >= 3 &&
            result.has_fit_circle &&
            result.circle_radius > 0.0;

        if (!result.algorithm_ok)
        {
            result.failure_stage =
                result.valid_points_count < 3
                    ? "measure_points"
                    : "fitcircle";

            result.reason =
                "Findcircle result unavailable: valid_points=" +
                std::to_string(result.valid_points_count) +
                ", has_fit_circle=" +
                std::string(result.has_fit_circle ? "true" : "false") +
                ", radius=" + std::to_string(result.circle_radius);
        }
    }

    const size_t before = layer.ShapeElements().size();
    tool.PublishDisplayShapes(layer, request.owner_ref);
    const size_t after = layer.ShapeElements().size();

    CapturePublishedShapes(layer, request.owner_ref, result);
    CountRoles(layer, request.owner_ref, result);

    result.publish_ok =
        after > before ||
        HasElementsForOwner(layer, request.owner_ref);

    return true;
}

bool ExecuteFindellipse(const CxRuntimeProjectionRequest& request,
                        ImageAnnotationLayer& layer,
                        CxRuntimeProjectionResult& result)
{
    result.owner_type = "Findellipse";
    result.owner_ref = request.owner_ref;

    if (request.owner_ref.empty())
        return Fail(result, "request_validation", "owner_ref is empty");

    if (!request.has_ellipse_roi)
        return Fail(result, "request_validation", "Findellipse requires has_ellipse_roi");

    if (request.ellipse_rx <= 1.0 || request.ellipse_ry <= 1.0)
        return Fail(result, "request_validation", "Findellipse radius is too small");

    if (std::abs(request.ellipse_angle_deg) > 0.001)
        return Fail(result,
                    "unsupported_rotated_ellipse_roi",
                    "Findellipse rotated ROI is not bound yet");

    Findellipse tool;
    tool.setellipse(
        static_cast<int>(request.ellipse_cx - request.ellipse_rx),
        static_cast<int>(request.ellipse_cy - request.ellipse_ry),
        static_cast<int>(request.ellipse_cx + request.ellipse_rx),
        static_cast<int>(request.ellipse_cy + request.ellipse_ry));
    tool.Setgap(request.gap);
    tool.setlinegap(request.linegap);
    tool.setthre(request.threshold);
    tool.setmethod(request.method);

    if (request.require_algorithm_execution)
    {
        Image image;
        if (!LoadProjectionImage(request.image_path, image, result))
            return false;

        tool.measure(&image);
        result.executed = true;

        FindellipseDisplaySnapshot snapshot;
        if (!tool.getdisplaysnapshot(snapshot))
        {
            result.failure_stage = "display_snapshot";
            result.reason = "Findellipse display snapshot unavailable";
            return false;
        }

        result.valid_points_count = snapshot.measure_points_count;
        result.has_fit_ellipse = false;
        result.algorithm_ok = false;
        result.failure_stage = "fitellipse_binding";
        result.reason =
            "Findellipse measure points are available, "
            "but fitted ellipse result is not bound";
    }

    const size_t before = layer.ShapeElements().size();
    tool.PublishDisplayShapes(layer, request.owner_ref);
    const size_t after = layer.ShapeElements().size();

    CapturePublishedShapes(layer, request.owner_ref, result);
    CountRoles(layer, request.owner_ref, result);

    result.publish_ok =
        after > before ||
        HasElementsForOwner(layer, request.owner_ref);

    return true;
}

bool ExecuteFindRect(const CxRuntimeProjectionRequest& request,
                     ImageAnnotationLayer& layer,
                     CxRuntimeProjectionResult& result)
{
    result.owner_type = "FindRect";
    result.owner_ref = request.owner_ref;

    if (request.owner_ref.empty())
        return Fail(result, "request_validation", "owner_ref is empty");

    FindRect tool;

    if (request.has_rotated_rect_roi)
    {
        if (request.rect_width < 2 || request.rect_height < 2)
            return Fail(result, "request_validation", "FindRect rotated rect width or height is too small");

        tool.setrotatedrect(
            request.rect_cx,
            request.rect_cy,
            request.rect_width,
            request.rect_height,
            request.rect_angle_deg);
    }
    else
    {
        const int x0 = static_cast<int>(std::min(request.roi_x0, request.roi_x1));
        const int y0 = static_cast<int>(std::min(request.roi_y0, request.roi_y1));
        const int x1 = static_cast<int>(std::max(request.roi_x0, request.roi_x1));
        const int y1 = static_cast<int>(std::max(request.roi_y0, request.roi_y1));

        const int width = x1 - x0;
        const int height = y1 - y0;

        if (width < 2 || height < 2)
            return Fail(result, "request_validation", "FindRect width or height is too small");

        tool.setrect(x0, y0, width, height);
    }
    tool.setlinegap(request.linegap);
    tool.setthre(request.threshold);

    if (request.require_algorithm_execution)
    {
        Image image;
        if (!LoadProjectionImage(request.image_path, image, result))
            return false;

        tool.measure(&image);
        result.executed = true;

        result.valid_points_count = tool.getresultobjsnum();
        result.has_result_rect = tool.hasresult();
        result.algorithm_ok = tool.hasresult();

        if (!result.algorithm_ok)
        {
            result.failure_stage = "findrect_result";
            result.reason = "FindRect result unavailable: has_result=" +
                std::string(result.has_result_rect ? "true" : "false");
        }
    }

    const size_t before = layer.ShapeElements().size();
    tool.PublishDisplayShapes(layer, request.owner_ref);
    const size_t after = layer.ShapeElements().size();

    CapturePublishedShapes(layer, request.owner_ref, result);
    CountRoles(layer, request.owner_ref, result);

    result.publish_ok =
        after > before ||
        HasElementsForOwner(layer, request.owner_ref);

    return true;
}

bool ExecuteFastMatch(const CxRuntimeProjectionRequest& request,
                      ImageAnnotationLayer& layer,
                      CxRuntimeProjectionResult& result)
{
    result.owner_type = "FastMatch";
    result.owner_ref = request.owner_ref;

    if (request.owner_ref.empty())
        return Fail(result, "request_validation", "owner_ref is empty");

    if (!request.has_learn_roi && !request.has_search_roi)
        return Fail(result, "request_validation", "FastMatch requires at least one of learn_roi or search_roi");

    fastmatch matcher;

    if (request.has_learn_roi)
    {
        const int lx0 = static_cast<int>(request.learn_roi.x);
        const int ly0 = static_cast<int>(request.learn_roi.y);
        const int lwidth = static_cast<int>(request.learn_roi.width);
        const int lheight = static_cast<int>(request.learn_roi.height);

        if (lwidth < 2 || lheight < 2)
            return Fail(result, "request_validation", "FastMatch learn ROI width or height is too small");

        matcher.setrect(lx0, ly0, lwidth, lheight);
    }

    if (request.has_search_roi)
    {
        const int sx0 = static_cast<int>(request.search_roi.x);
        const int sy0 = static_cast<int>(request.search_roi.y);
        const int swidth = static_cast<int>(request.search_roi.width);
        const int sheight = static_cast<int>(request.search_roi.height);

        if (swidth < 2 || sheight < 2)
            return Fail(result, "request_validation", "FastMatch search ROI width or height is too small");

        matcher.setmatchrect(sx0, sy0, swidth, sheight);
    }

    if (request.has_expected_rect)
    {
        matcher.setexpectedrect(
            request.expected_rect.x,
            request.expected_rect.y,
            request.expected_rect.x + request.expected_rect.width,
            request.expected_rect.y + request.expected_rect.height);
    }

    if (request.require_algorithm_execution)
    {
        Image image;
        if (!LoadProjectionImage(request.image_path, image, result))
            return false;

        matcher.learn(&image);
        matcher.match(&image);

        result.valid_points_count = matcher.getmodelpointcount();
        result.model_point_count = matcher.getmodelpointcount();
        result.candidate_count = matcher.getresultcandidatecount();
        result.best_score = matcher.getresultbestscore();
        result.algorithm_ok = result.model_point_count > 0;
        result.executed = true;

        if (!result.algorithm_ok)
        {
            result.failure_stage = "fastmatch_model";
            result.reason = "FastMatch model unavailable: model_points=" +
                std::to_string(result.model_point_count);
        }
    }

    const size_t before = layer.ShapeElements().size();
    matcher.PublishDisplayShapes(layer, request.owner_ref);
    const size_t after = layer.ShapeElements().size();

    CapturePublishedShapes(layer, request.owner_ref, result);
    CountRoles(layer, request.owner_ref, result);

    result.publish_ok =
        after > before ||
        HasElementsForOwner(layer, request.owner_ref);

    return true;
}

}

CxRuntimeProjectionExecutor::CxRuntimeProjectionExecutor()
{
    Register("findline_gauge", ExecuteFindline);
    Register("findcircle_gauge", ExecuteFindcircle);
    Register("findellipse_gauge", ExecuteFindellipse);
    Register("findrect_gauge", ExecuteFindRect);
    Register("fastmatch", ExecuteFastMatch);
}

void CxRuntimeProjectionExecutor::Register(const std::string& tool_id, const ProjectionHandler& handler)
{
    m_handlers[tool_id] = handler;
}

bool CxRuntimeProjectionExecutor::Execute(const CxRuntimeProjectionRequest& request,
                                          ImageAnnotationLayer& output_layer,
                                          CxRuntimeProjectionResult& result)
{
    auto it = m_handlers.find(request.tool_id);
    if (it == m_handlers.end())
    {
        result.failure_stage = "tool_not_found";
        result.reason = "no projection handler registered for tool: " + request.tool_id;
        return false;
    }

    return it->second(request, output_layer, result);
}
