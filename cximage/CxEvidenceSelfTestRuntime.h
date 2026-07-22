#pragma once

#include <string>
#include <vector>

struct CxEvidenceSelfTestRequest
{
    std::string run_id;
    std::string case_id;

    int group_index = -1;
    int thumb_index = -1;

    std::string script_id;
    std::string image_id;
    std::string target_id;

    std::string out_dir;

    bool require_image = true;
    bool require_params = true;
    bool require_runtime_object = true;
    bool require_gauge_projection = true;
    bool require_result_projection = true;
};

struct CxEvidenceSelfTestStepResult
{
    std::string code;
    std::string status;
    std::string reason;

    std::string script_path;
    std::string image_path;
    std::string target_id;
    std::string parameter_summary;

    double elapsed_ms = 0.0;
};

struct CxEvidenceSelfTestResult
{
    std::string run_id;
    std::string case_id;

    bool executed = false;
    bool passed_to_human_review = false;

    std::string final_code;
    std::string final_status;
    std::string final_reason;

    std::string script_id;
    std::string script_path;

    std::string image_id;
    std::string image_path;

    std::string target_id;
    std::string tool;
    std::string parameter_summary;

    int runtime_object_count = 0;
    int shape_element_count = 0;
    int gauge_shape_count = 0;
    int result_shape_count = 0;

    std::vector<CxEvidenceSelfTestStepResult> steps;
};

void AddEvidenceSelfTestStep(
    CxEvidenceSelfTestResult& result,
    const std::string& code,
    const std::string& status,
    const std::string& reason);

bool WriteEvidenceSelfTestSummaryJson(
    const CxEvidenceSelfTestResult& result,
    const std::string& outPath,
    std::string& reason);

bool WriteEvidenceSelfTestReportMd(
    const CxEvidenceSelfTestResult& result,
    const std::string& outPath,
    std::string& reason);