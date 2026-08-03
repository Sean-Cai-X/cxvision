#include "FindSegmentation.h"
#include "FindSegmentationEdgeSamBackend.h"
#include "FindSegmentationOpenCvSmokeBackend.h"
#include "ImageAnnotationLayer.h"
#include "PolylineShape.h"
#include "RectShape.h"
#include "CircleShape.h"

#include <iostream>
#include <memory>

FindSegmentation::FindSegmentation()
{
}

void FindSegmentation::setbackend(const char* backend)
{
    if (backend != nullptr)
        m_backend = backend;
}

void FindSegmentation::setmodel(const char* model_path)
{
    if (model_path != nullptr)
        m_model_path = model_path;
}

void FindSegmentation::setdevice(const char* device)
{
    if (device != nullptr)
        m_device = device;
}

void FindSegmentation::setthreshold(double threshold)
{
    m_threshold = threshold;
}

void FindSegmentation::setpromptrect(int x0, int y0, int x1, int y1)
{
    m_x0 = x0;
    m_y0 = y0;
    m_x1 = x1;
    m_y1 = y1;
    m_has_rect = true;
}

void FindSegmentation::setpromptrectxyxy(int y1, int x1, int y0, int x0)
{
    setpromptrect(x0, y0, x1, y1);
}

void FindSegmentation::setpoint(int x, int y)
{
    m_px = x;
    m_py = y;
    m_has_point = true;
}

void FindSegmentation::setmode(int mode)
{
    m_mode = mode;
}

void FindSegmentation::segment(void* image)
{
    std::cout << "[FindSegmentation] segment begin image=" << image << "\n" << std::flush;

    if (image == nullptr)
    {
        m_status = "failed";
        m_reason = "segment image pointer is null";
        m_result.ok = false;
        std::cout << "[FindSegmentation] segment end status=" << m_status << " reason=" << m_reason << "\n" << std::flush;
        return;
    }

    Image* img = static_cast<Image*>(image);
    cv::Mat mat = img->getmat();

    if (mat.empty())
    {
        m_status = "failed";
        m_reason = "segment input mat is empty";
        m_result.ok = false;
        std::cout << "[FindSegmentation] segment end status=" << m_status << " reason=" << m_reason << "\n" << std::flush;
        return;
    }

    std::cout << "[FindSegmentation] backend=" << m_backend << "\n" << std::flush;

    FindSegmentationInput input;
    input.image = mat;
    input.model_path = m_model_path;
    input.device = m_device;
    input.backend = m_backend;
    input.threshold = m_threshold;
    input.mode = m_mode;

    if (m_has_rect)
    {
        input.has_rect = true;
        input.rect = cv::Rect(m_x0, m_y0, m_x1 - m_x0, m_y1 - m_y0);
        std::cout << "[FindSegmentation] prompt_rect state="
                  << m_x0 << "," << m_y0 << ","
                  << m_x1 << "," << m_y1
                  << " input_rect="
                  << input.rect.x << "," << input.rect.y << ","
                  << input.rect.width << "," << input.rect.height
                  << " image=" << mat.cols << "x" << mat.rows
                  << "\n" << std::flush;
    }

    if (m_has_point)
    {
        input.has_point = true;
        input.point = cv::Point(m_px, m_py);
    }

    std::string reason;

    if (m_backend == "edgesam" || m_backend == "libtorch_segmentation")
    {
        FindSegmentationEdgeSamBackend backend;
        backend.Run(input, m_result, reason);
    }
    else
    {
        FindSegmentationOpenCvSmokeBackend backend;
        backend.Run(input, m_result, reason);
    }

    m_status = m_result.status;
    m_reason = m_result.reason;

    m_result_ref = m_result.result_ref.empty() ? "segmentation:" + m_backend : m_result.result_ref;
    m_mask_ref = m_result.mask_ref.empty() ? "mask:" + m_backend : m_result.mask_ref;
    m_contour_ref = m_result.contour_ref.empty() ? "contour:" + m_backend : m_result.contour_ref;
    m_overlay_ref = m_result.overlay_ref.empty() ? "overlay:" + m_backend : m_result.overlay_ref;

    std::cout << "[FindSegmentation] segment end status=" << m_status << "\n" << std::flush;
}

void FindSegmentation::extractboundary()
{
}

void FindSegmentation::buildoverlay(void* image)
{
    if (image == nullptr)
        return;

    if (!m_result.overlay_ref.empty())
    {
        m_overlay_ref = m_result.overlay_ref;
        return;
    }

    if (!m_result.overlay.empty())
    {
        m_overlay_ref = "overlay:" + m_backend + ":generated";
    }
}

const char* FindSegmentation::get_result()
{
    return m_result_ref.c_str();
}

const char* FindSegmentation::get_mask_ref()
{
    return m_mask_ref.c_str();
}

const char* FindSegmentation::get_contour_ref()
{
    return m_contour_ref.c_str();
}

const char* FindSegmentation::get_overlay_ref()
{
    return m_overlay_ref.c_str();
}

int FindSegmentation::status_code()
{
    return m_result.ok ? 1 : 0;
}

int FindSegmentation::get_contour_count()
{
    return m_result.contour_count;
}

double FindSegmentation::get_primary_area()
{
    return m_result.primary_area;
}

const std::string& FindSegmentation::backend() const
{
    return m_backend;
}

const std::string& FindSegmentation::model_path() const
{
    return m_model_path;
}

const std::string& FindSegmentation::device() const
{
    return m_device;
}

const FindSegmentationResult& FindSegmentation::result() const
{
    return m_result;
}

void FindSegmentation::PublishDisplayShapes(
    ICxShapeSink& sink,
    const std::string& owner_ref) const
{
    if (m_has_rect)
    {
        auto rect = std::make_unique<RectShape>();
        rect->setRect(m_x0, m_y0, m_x1, m_y1);
        sink.UpsertShape(
            owner_ref + ".prompt_rect",
            "FindSegmentation",
            owner_ref,
            "setpromptrect",
            "roi",
            true,
            false,
            std::move(rect));
    }

    if (m_has_point)
    {
        auto point = std::make_unique<PointsShape>();
        point->addpoint(gp_Pnt(m_px, m_py, 0.0));
        sink.UpsertShape(
            owner_ref + ".prompt_point",
            "FindSegmentation",
            owner_ref,
            "setpoint",
            "prompt_point",
            true,
            false,
            std::move(point));
    }

    if (m_result.contours.empty())
        return;

    const FindSegmentationContour* best = nullptr;
    for (const FindSegmentationContour& contour : m_result.contours)
    {
        if (best == nullptr || contour.area > best->area)
            best = &contour;
    }

    if (best == nullptr || best->points.empty())
        return;

    auto polyline = std::make_unique<PolylineShape>();
    for (const cv::Point& p : best->points)
        polyline->addPoint(p.x, p.y);
    polyline->close(true);
    sink.UpsertShape(
        owner_ref + ".boundary_polyline",
        "FindSegmentation",
        owner_ref,
        "boundary",
        "boundary",
        false,
        true,
        std::move(polyline));

    cv::Rect bbox = cv::boundingRect(best->points);
    auto rect = std::make_unique<RectShape>();
    rect->setRect(
        bbox.x,
        bbox.y,
        bbox.x + bbox.width,
        bbox.y + bbox.height);
    sink.UpsertShape(
        owner_ref + ".boundary_bbox",
        "FindSegmentation",
        owner_ref,
        "boundary_bbox",
        "boundary_bbox",
        false,
        true,
        std::move(rect));
}
