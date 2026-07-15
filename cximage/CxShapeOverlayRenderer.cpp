#include "pch.h"
#include "CxShapeOverlayRenderer.h"
#include "CxScriptHeadlessRuntime.h"

void DrawRoiRectangle(cv::Mat& img, const CxShapeElementSnapshot& shape, const cv::Scalar& color)
{
    if (shape.points.size() >= 4)
    {
        cv::Point pt1(static_cast<int>(shape.points[0]), static_cast<int>(shape.points[1]));
        cv::Point pt2(static_cast<int>(shape.points[2]), static_cast<int>(shape.points[3]));
        cv::rectangle(img, pt1, pt2, color, 2);
    }
}

void DrawRoiPolyline(cv::Mat& img, const CxShapeElementSnapshot& shape, const cv::Scalar& color)
{
    if (shape.points.size() < 4)
        return;

    std::vector<cv::Point> pts;
    for (size_t i = 0; i + 1 < shape.points.size(); i += 2)
        pts.emplace_back(static_cast<int>(shape.points[i]), static_cast<int>(shape.points[i + 1]));

    cv::polylines(img, pts, true, color, 2);
}

void DrawRoiCircle(cv::Mat& img, const CxShapeElementSnapshot& shape, const cv::Scalar& color)
{
    cv::Point center(static_cast<int>(shape.center_x), static_cast<int>(shape.center_y));
    int radius = static_cast<int>(shape.radius > 0 ? shape.radius : shape.radius_x);
    cv::circle(img, center, radius, color, 2);
}

void DrawMeasurePoints(cv::Mat& img, const CxShapeElementSnapshot& shape, const cv::Scalar& color)
{
    for (size_t i = 0; i + 1 < shape.points.size(); i += 2)
    {
        cv::Point pt(static_cast<int>(shape.points[i]), static_cast<int>(shape.points[i + 1]));
        cv::circle(img, pt, 2, color, -1);
    }
}

void DrawFitLine(cv::Mat& img, const CxShapeElementSnapshot& shape, const cv::Scalar& color)
{
    if (shape.points.size() >= 4)
    {
        cv::Point pt1(static_cast<int>(shape.points[0]), static_cast<int>(shape.points[1]));
        cv::Point pt2(static_cast<int>(shape.points[2]), static_cast<int>(shape.points[3]));
        cv::line(img, pt1, pt2, color, 3);
        cv::circle(img, pt1, 4, color, -1);
        cv::circle(img, pt2, 4, color, -1);
    }
}

void DrawFitCircle(cv::Mat& img, const CxShapeElementSnapshot& shape, const cv::Scalar& color)
{
    cv::Point center(static_cast<int>(shape.center_x), static_cast<int>(shape.center_y));
    int radius = static_cast<int>(shape.radius_x);
    cv::circle(img, center, radius, color, 3);

    int cross_size = 8;
    cv::line(img, cv::Point(center.x - cross_size, center.y), cv::Point(center.x + cross_size, center.y), color, 2);
    cv::line(img, cv::Point(center.x, center.y - cross_size), cv::Point(center.x, center.y + cross_size), color, 2);
}

bool RenderCxShapeOverlay(
    const cv::Mat& source,
    const std::vector<CxShapeElementSnapshot>& shapes,
    CxOverlayLayer layer,
    cv::Mat& output,
    CxOverlayRenderResult& result)
{
    result = CxOverlayRenderResult{};

    if (source.empty())
    {
        result.reason = "source image is empty";
        return false;
    }

    output = source.clone();
    cv::Mat original = source.clone();

    cv::Scalar roi_color(0, 255, 255);
    cv::Scalar scan_color(0, 255, 0);
    cv::Scalar measure_color(0, 0, 255);
    cv::Scalar result_color(0, 255, 255);

    if (layer == CxOverlayLayer::RESULT)
    {
        roi_color = cv::Scalar(100, 150, 150);
        scan_color = cv::Scalar(100, 180, 100);
        measure_color = cv::Scalar(100, 50, 200);
        result_color = cv::Scalar(0, 200, 255);
    }

    for (const auto& shape : shapes)
    {
        if (shape.shape_kind.empty())
            continue;

        bool should_render = false;

        if (layer == CxOverlayLayer::EVIDENCE)
        {
            if (shape.semantic_role == "roi" || shape.semantic_role == "scan" || shape.semantic_role == "measure_points")
                should_render = true;
        }
        else if (layer == CxOverlayLayer::RESULT)
        {
            if (shape.semantic_role == "result" || shape.semantic_role == "measure_points")
                should_render = true;
            if (shape.semantic_role == "roi" || shape.semantic_role == "scan")
                should_render = true;
        }
        else if (layer == CxOverlayLayer::TOOL_DISPLAY)
        {
            should_render = true;
        }

        if (!should_render)
            continue;

        result.rendered_element_count++;

        if (shape.semantic_role == "roi")
        {
            result.rendered_roi_count++;
            if (shape.shape_kind == "CircleShape")
                DrawRoiCircle(output, shape, roi_color);
            else if (shape.shape_kind == "PolylineShape")
                DrawRoiPolyline(output, shape, roi_color);
            else if (shape.shape_kind == "RectShape")
                DrawRoiRectangle(output, shape, roi_color);
        }
        else if (shape.semantic_role == "scan")
        {
            result.rendered_scan_count++;
            if (shape.shape_kind == "CircleShape")
                DrawRoiCircle(output, shape, scan_color);
            else
                DrawRoiPolyline(output, shape, scan_color);
        }
        else if (shape.semantic_role == "measure_points")
        {
            result.rendered_measure_points_count += static_cast<int>(shape.points.size()) / 2;
            DrawMeasurePoints(output, shape, measure_color);
        }
        else if (shape.semantic_role == "result")
        {
            result.rendered_result_count++;
            if (shape.shape_kind == "CircleShape")
                DrawFitCircle(output, shape, result_color);
            else if (shape.shape_kind == "LineShape")
                DrawFitLine(output, shape, result_color);
        }
        else if (shape.semantic_role == "learn_roi" || shape.semantic_role == "search_roi")
        {
            result.rendered_roi_count++;
            DrawRoiRectangle(output, shape, roi_color);
        }
        else if (shape.semantic_role == "expected_gt")
        {
            result.rendered_result_count++;
            cv::Scalar gt_color(255, 100, 100);
            DrawRoiRectangle(output, shape, gt_color);
        }
    }

    cv::Mat diff;
    cv::absdiff(output, original, diff);
    // countNonZero requires a single-channel matrix.  The overlays are BGR,
    // so flatten only for counting while preserving the rendered image.
    result.changed_pixel_count = cv::countNonZero(diff.reshape(1));

    if (result.changed_pixel_count == 0 && !shapes.empty())
    {
        cv::putText(output, "NO FIT RESULT", cv::Point(20, 40),
            cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 255), 2);
    }

    result.ok = true;
    return true;
}
