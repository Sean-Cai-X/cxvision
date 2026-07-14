#ifndef CXIMAGE_CXSCRIPT_HEADLESS_RUNNER_H
#define CXIMAGE_CXSCRIPT_HEADLESS_RUNNER_H

#include <string>
#include <opencv2/opencv.hpp>
#include "ViewController.h"
#include "CxScriptHeadlessRuntime.h"

bool SaveCxScriptOverlayImage(
    const ManualTestContext& context,
    const cv::Mat& sourceImage,
    const std::filesystem::path& outputPath,
    std::string& outReason);

bool SaveCxScriptHeadlessSummaryJson(
    const ManualTestContext& context,
    const std::filesystem::path& outputPath,
    std::string& outReason);

bool ParseCxScriptHeadlessArgs(
    int argc,
    char** argv,
    CxScriptHeadlessOptions& options);

bool RunCxScriptHeadless(const CxScriptHeadlessOptions& options, CxScriptHeadlessResult& result);

int RunCxScriptHeadless(int argc, char* argv[]);

#endif
