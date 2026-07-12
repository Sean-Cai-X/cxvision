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