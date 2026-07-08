#include "CxScriptToolDisplayExporter.h"
#include <opencv2/opencv.hpp>

std::string CxScriptToolDisplayExporter::ExportToolDisplay(
    const std::string& original_path,
    const std::string& result_overlay_path,
    const std::string& evidence_overlay_path,
    const std::filesystem::path& output_path,
    const CxScriptSuiteCaseResult& result)
{
    cv::Mat original = cv::imread(original_path);
    cv::Mat resultOverlay = cv::imread(result_overlay_path);
    cv::Mat evidenceOverlay = cv::imread(evidence_overlay_path);

    if (original.empty())
        return "";

    if (resultOverlay.empty())
        resultOverlay = original.clone();

    if (evidenceOverlay.empty())
        evidenceOverlay = original.clone();

    cv::Mat resultView = resultOverlay.clone();

    if (result.tool == "Findline" && (result.roi_x0 != 0 || result.roi_y0 != 0 || result.roi_x1 != 0 || result.roi_y1 != 0))
    {
        cv::line(
            resultView,
            cv::Point(result.roi_x0, result.roi_y0),
            cv::Point(result.roi_x1, result.roi_y1),
            cv::Scalar(0, 255, 0),
            2,
            cv::LINE_AA);
    }

    if (result.tool == "Findcircle" && result.circle_radius > 0)
    {
        double centerX = result.circle_center_x;
        double centerY = result.circle_center_y;
        if (centerX == 0 && centerY == 0)
        {
            centerX = result.circle_cx;
            centerY = result.circle_cy;
        }
        cv::circle(
            resultView,
            cv::Point(static_cast<int>(centerX), static_cast<int>(centerY)),
            static_cast<int>(result.circle_radius),
            cv::Scalar(0, 255, 255),
            2,
            cv::LINE_AA);
    }

    if (result.headless_ok)
    {
        cv::putText(
            resultView,
            result.contract_pass ? "PASS" : "FAIL",
            {20, 40},
            cv::FONT_HERSHEY_SIMPLEX,
            1.0,
            result.contract_pass ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 0, 255),
            2,
            cv::LINE_AA);
    }
    else
    {
        cv::putText(
            resultView,
            "HEADLESS_FAILED",
            {20, 40},
            cv::FONT_HERSHEY_SIMPLEX,
            1.0,
            cv::Scalar(0, 0, 255),
            2,
            cv::LINE_AA);
    }

    const int targetW = 480;

    auto resizeKeep = [](const cv::Mat& src, int width)
    {
        cv::Mat dst;
        const double scale = static_cast<double>(width) / src.cols;
        cv::resize(src, dst, cv::Size(width, static_cast<int>(src.rows * scale)));
        return dst;
    };

    cv::Mat a = resizeKeep(original, targetW);
    cv::Mat b = resizeKeep(resultView, targetW);
    cv::Mat c = resizeKeep(evidenceOverlay, targetW);

    const int h = std::max({a.rows, b.rows, c.rows});

    auto padToHeight = [](const cv::Mat& src, int height)
    {
        if (src.rows == height)
            return src;

        cv::Mat dst(height, src.cols, src.type(), cv::Scalar(20, 20, 20));
        src.copyTo(dst(cv::Rect(0, 0, src.cols, src.rows)));
        return dst;
    };

    a = padToHeight(a, h);
    b = padToHeight(b, h);
    c = padToHeight(c, h);

    cv::putText(a, "Original", {20, 30}, cv::FONT_HERSHEY_SIMPLEX, 0.8, {0,255,0}, 2);
    cv::putText(b, "Result Overlay", {20, 30}, cv::FONT_HERSHEY_SIMPLEX, 0.8, {0,255,0}, 2);
    cv::putText(c, "Evidence Overlay", {20, 30}, cv::FONT_HERSHEY_SIMPLEX, 0.8, {0,255,0}, 2);

    cv::Mat row;
    cv::hconcat(std::vector<cv::Mat>{a, b, c}, row);

    std::filesystem::create_directories(output_path.parent_path());
    cv::imwrite(output_path.string(), row);

    return output_path.string();
}