#include "FindSegmentationOpenCvSmokeBackend.h"

bool FindSegmentationOpenCvSmokeBackend::Run(
    const FindSegmentationInput& input,
    FindSegmentationResult& output,
    std::string& reason)
{
    if (input.image.empty())
    {
        reason = "input image is empty";
        return false;
    }

    cv::Mat bgr = input.image;
    cv::Mat gray;
    if (bgr.channels() == 3)
        cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);
    else
        gray = bgr.clone();

    cv::Rect roi(0, 0, gray.cols, gray.rows);
    if (input.has_rect)
    {
        roi = input.rect & cv::Rect(0, 0, gray.cols, gray.rows);
        if (roi.width <= 0 || roi.height <= 0)
        {
            reason = "prompt rect outside image";
            return false;
        }
    }

    cv::Mat work = cv::Mat::zeros(gray.size(), CV_8UC1);
    cv::Mat roiGray = gray(roi);

    cv::Mat blurred;
    cv::GaussianBlur(roiGray, blurred, cv::Size(3, 3), 0);

    cv::Mat edges;
    cv::Canny(blurred, edges, 50, 150);

    edges.copyTo(work(roi));

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(work, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    output.mask = cv::Mat::zeros(gray.size(), CV_8UC1);
    output.contours.clear();

    double bestArea = 0.0;
    int bestIndex = -1;

    for (int i = 0; i < static_cast<int>(contours.size()); ++i)
    {
        double area = std::abs(cv::contourArea(contours[i]));
        if (area < 10.0)
            continue;

        FindSegmentationContour c;
        c.points = contours[i];
        c.area = area;
        c.perimeter = cv::arcLength(contours[i], true);

        if (area > bestArea)
        {
            bestArea = area;
            bestIndex = static_cast<int>(output.contours.size());
        }

        output.contours.push_back(c);
    }

    if (bestIndex >= 0)
    {
        std::vector<std::vector<cv::Point>> drawContours;
        drawContours.push_back(output.contours[bestIndex].points);
        cv::drawContours(output.mask, drawContours, 0, cv::Scalar(255), cv::FILLED);
        output.primary_area = output.contours[bestIndex].area;
    }

    output.overlay = bgr.clone();
    for (const auto& c : output.contours)
    {
        std::vector<std::vector<cv::Point>> one;
        one.push_back(c.points);
        cv::drawContours(output.overlay, one, 0, cv::Scalar(0, 255, 255), 2);
    }

    output.ok = !output.contours.empty();
    output.backend = "opencv_smoke";
    output.backend_status = "smoke_ready";
    output.status = output.ok ? "boundary_available" : "no_boundary";
    output.reason = output.ok ? "opencv smoke boundary generated" : "no contour found";
    output.mask_width = output.mask.cols;
    output.mask_height = output.mask.rows;
    output.contour_count = static_cast<int>(output.contours.size());

    reason = output.reason;
    return output.ok;
}