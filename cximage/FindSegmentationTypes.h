#pragma once

#include <opencv2/opencv.hpp>

#include <string>
#include <vector>

struct FindSegmentationContour
{
    std::vector<cv::Point> points;
    double area = 0.0;
    double perimeter = 0.0;
};

struct FindSegmentationResult
{
    bool ok = false;

    std::string backend = "opencv_smoke";
    std::string backend_status = "not_run";
    std::string status = "not_run";
    std::string reason;

    cv::Mat mask;
    cv::Mat overlay;

    std::vector<FindSegmentationContour> contours;

    int mask_width = 0;
    int mask_height = 0;
    int contour_count = 0;
    double primary_area = 0.0;

    std::string result_ref;
    std::string mask_ref;
    std::string contour_ref;
    std::string overlay_ref;
};

struct FindSegmentationInputSnapshot
{
    std::string backend = "opencv_smoke";
    std::string model_path;
    std::string device = "auto";

    double threshold = 0.5;
    int mode = 0;

    int image_width = 0;
    int image_height = 0;

    bool has_rect = false;
    int rect_x = 0;
    int rect_y = 0;
    int rect_width = 0;
    int rect_height = 0;

    bool has_point = false;
    int point_x = 0;
    int point_y = 0;
};

struct FindSegmentationBackendDiagnosticSnapshot
{
    std::string backend = "opencv_smoke";
    std::string backend_status = "not_run";
    std::string status = "not_run";
    std::string reason;

    bool image_ready = false;
    bool prompt_rect_ready = false;
    bool prompt_point_ready = false;
    bool mask_ready = false;
    bool overlay_ready = false;

    int mask_width = 0;
    int mask_height = 0;
    int contour_count = 0;
    double primary_area = 0.0;
};
