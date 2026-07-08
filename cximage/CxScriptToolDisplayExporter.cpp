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

    const int targetW = 480;

    auto resizeKeep = [](const cv::Mat& src, int width)
    {
        cv::Mat dst;
        const double scale = static_cast<double>(width) / src.cols;
        cv::resize(src, dst, cv::Size(width, static_cast<int>(src.rows * scale)));
        return dst;
    };

    cv::Mat a = resizeKeep(original, targetW);
    cv::Mat b = resizeKeep(resultOverlay, targetW);
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