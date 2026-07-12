#pragma once

#include "FindSegmentationTypes.h"

#include <opencv2/opencv.hpp>

#include <string>

struct FindSegmentationInput
{
    cv::Mat image;

    std::string model_path;
    std::string device = "auto";
    std::string backend = "opencv_smoke";

    double threshold = 0.5;
    int mode = 0;

    bool has_rect = false;
    cv::Rect rect;

    bool has_point = false;
    cv::Point point;
};

class IFindSegmentationBackend
{
public:
    virtual ~IFindSegmentationBackend() = default;

    virtual bool Run(
        const FindSegmentationInput& input,
        FindSegmentationResult& output,
        std::string& reason) = 0;
};