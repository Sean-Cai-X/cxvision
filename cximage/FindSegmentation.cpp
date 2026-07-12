#include "FindSegmentation.h"
#include "FindSegmentationEdgeSamBackend.h"
#include "FindSegmentationOpenCvSmokeBackend.h"

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
    }

    if (m_has_point)
    {
        input.has_point = true;
        input.point = cv::Point(m_px, m_py);
    }

    std::string reason;

    if (m_backend == "edgesam")
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

    m_result_ref = "segmentation:" + m_backend;
    m_mask_ref = "mask:" + m_backend;
    m_contour_ref = "contour:" + m_backend;
    m_overlay_ref = "overlay:" + m_backend;

    std::cout << "[FindSegmentation] segment end status=" << m_status << "\n" << std::flush;
}

void FindSegmentation::extractboundary()
{
}

void FindSegmentation::buildoverlay(void* image)
{
    if (image == nullptr)
        return;

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