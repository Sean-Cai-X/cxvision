#pragma once

#include "FindSegmentationBackend.h"

#include "Image.h"

#include <string>

class FindSegmentation
{
public:
    FindSegmentation();

    void setbackend(const char* backend);
    void setmodel(const char* model_path);
    void setdevice(const char* device);

    void setthreshold(double threshold);
    void setpromptrect(int x0, int y0, int x1, int y1);
    void setpoint(int x, int y);
    void setmode(int mode);

    void segment(void* image);
    void extractboundary();
    void buildoverlay(void* image);

    const char* get_result();
    const char* get_mask_ref();
    const char* get_contour_ref();
    const char* get_overlay_ref();

    int status_code();
    int get_contour_count();
    double get_primary_area();

public:
    std::string m_status;
    std::string m_reason;
    std::string m_result_ref;
    std::string m_mask_ref;
    std::string m_contour_ref;
    std::string m_overlay_ref;

private:
    std::string m_backend = "opencv_smoke";
    std::string m_model_path;
    std::string m_device = "auto";

    double m_threshold = 0.5;
    int m_mode = 0;

    bool m_has_rect = false;
    int m_x0 = 0;
    int m_y0 = 0;
    int m_x1 = 0;
    int m_y1 = 0;

    bool m_has_point = false;
    int m_px = 0;
    int m_py = 0;

    FindSegmentationResult m_result;
};