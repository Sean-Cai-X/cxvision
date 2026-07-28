#include "CxEvidenceSelfTestRuntime.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace
{
std::string JsonEscape(const std::string& text)
{
    std::string out;
    for (char c : text)
    {
        switch (c)
        {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': break;
        case '\t': out += "\\t"; break;
        default: out += c; break;
        }
    }
    return out;
}
}

void AddEvidenceSelfTestStep(
    CxEvidenceSelfTestResult& result,
    const std::string& code,
    const std::string& status,
    const std::string& reason)
{
    CxEvidenceSelfTestStepResult step;
    step.code = code;
    step.status = status;
    step.reason = reason;
    result.steps.push_back(step);
}

bool WriteEvidenceSelfTestSummaryJson(
    const CxEvidenceSelfTestResult& result,
    const std::string& outPath,
    std::string& reason)
{
    reason.clear();

    std::filesystem::create_directories(
        std::filesystem::path(outPath).parent_path());

    std::ofstream file(outPath, std::ios::binary);
    if (!file.is_open())
    {
        reason = "failed to open selftest summary json: " + outPath;
        return false;
    }

    file << "{\n";
    file << "  \"run_id\": \"" << JsonEscape(result.run_id) << "\",\n";
    file << "  \"case_id\": \"" << JsonEscape(result.case_id) << "\",\n";
    file << "  \"executed\": " << (result.executed ? "true" : "false") << ",\n";
    file << "  \"final_code\": \"" << JsonEscape(result.final_code) << "\",\n";
    file << "  \"final_status\": \"" << JsonEscape(result.final_status) << "\",\n";
    file << "  \"final_reason\": \"" << JsonEscape(result.final_reason) << "\",\n";

    file << "  \"script_id\": \"" << JsonEscape(result.script_id) << "\",\n";
    file << "  \"script_path\": \"" << JsonEscape(result.script_path) << "\",\n";
    file << "  \"image_id\": \"" << JsonEscape(result.image_id) << "\",\n";
    file << "  \"image_path\": \"" << JsonEscape(result.image_path) << "\",\n";
    file << "  \"target_id\": \"" << JsonEscape(result.target_id) << "\",\n";
    file << "  \"tool\": \"" << JsonEscape(result.tool) << "\",\n";
    file << "  \"parameter_summary\": \"" << JsonEscape(result.parameter_summary) << "\",\n";

    file << "  \"runtime_object_count\": " << result.runtime_object_count << ",\n";
    file << "  \"shape_element_count\": " << result.shape_element_count << ",\n";
    file << "  \"gauge_shape_count\": " << result.gauge_shape_count << ",\n";
    file << "  \"result_shape_count\": " << result.result_shape_count << ",\n";
    file << "  \"shape_element_count_before\": " << result.shape_element_count_before << ",\n";
    file << "  \"shape_element_count_after\": " << result.shape_element_count_after << ",\n";
    file << "  \"projected_shape_count\": " << result.projected_shape_count << ",\n";

    file << "  \"parser_binding_ok\": " << (result.parser_binding_ok ? "true" : "false") << ",\n";
    file << "  \"runtime_executed\": " << (result.runtime_executed ? "true" : "false") << ",\n";
    file << "  \"runtime_object_ok\": " << (result.runtime_object_ok ? "true" : "false") << ",\n";
    file << "  \"gauge_projection_ok\": " << (result.gauge_projection_ok ? "true" : "false") << ",\n";
    file << "  \"shape_projection_ok\": " << (result.shape_projection_ok ? "true" : "false") << ",\n";
    file << "  \"result_projection_ok\": " << (result.result_projection_ok ? "true" : "false") << ",\n";

    file << "  \"runtime_object_type\": \"" << JsonEscape(result.runtime_object_type) << "\",\n";
    file << "  \"runtime_object_name\": \"" << JsonEscape(result.runtime_object_name) << "\",\n";
    file << "  \"runtime_status\": \"" << JsonEscape(result.runtime_status) << "\",\n";
    file << "  \"result_ref\": \"" << JsonEscape(result.result_ref) << "\",\n";
    file << "  \"evidence_ref\": \"" << JsonEscape(result.evidence_ref) << "\",\n";
    file << "  \"gauge_status\": \"" << JsonEscape(result.gauge_status) << "\",\n";
    file << "  \"projection_reason\": \"" << JsonEscape(result.projection_reason) << "\",\n";

    file << "  \"algorithm_status\": \"" << JsonEscape(result.algorithm_status) << "\",\n";
    file << "  \"algorithm_reason\": \"" << JsonEscape(result.algorithm_reason) << "\",\n";
    file << "  \"measure_points_count\": " << result.measure_points_count << ",\n";
    file << "  \"valid_points_count\": " << result.valid_points_count << ",\n";
    file << "  \"has_measure_points\": " << (result.has_measure_points ? "true" : "false") << ",\n";
    file << "  \"has_fit_result\": " << (result.has_fit_result ? "true" : "false") << ",\n";
    file << "  \"avgdist\": " << result.avgdist << ",\n";
    file << "  \"fit_cx\": " << result.fit_cx << ",\n";
    file << "  \"fit_cy\": " << result.fit_cy << ",\n";
    file << "  \"fit_radius\": " << result.fit_radius << ",\n";
    file << "  \"fit_radius_x\": " << result.fit_radius_x << ",\n";
    file << "  \"fit_radius_y\": " << result.fit_radius_y << ",\n";
    file << "  \"fit_angle_deg\": " << result.fit_angle_deg << ",\n";
    file << "  \"fastmatch_model_point_count\": " << result.fastmatch_model_point_count << ",\n";
    file << "  \"fastmatch_learn_a_count\": " << result.fastmatch_learn_a_count << ",\n";
    file << "  \"fastmatch_learn_b_count\": " << result.fastmatch_learn_b_count << ",\n";
    file << "  \"fastmatch_learn_a2_count\": " << result.fastmatch_learn_a2_count << ",\n";
    file << "  \"fastmatch_learn_b2_count\": " << result.fastmatch_learn_b2_count << ",\n";
    file << "  \"fastmatch_learn_status_code\": " << result.fastmatch_learn_status_code << ",\n";
    file << "  \"fastmatch_pattern_a_count\": " << result.fastmatch_pattern_a_count << ",\n";
    file << "  \"fastmatch_pattern_b_count\": " << result.fastmatch_pattern_b_count << ",\n";
    file << "  \"fastmatch_candidate_count\": " << result.fastmatch_candidate_count << ",\n";
    file << "  \"fastmatch_best_score\": " << result.fastmatch_best_score << ",\n";

    file << "  \"steps\": [\n";
    for (std::size_t i = 0; i < result.steps.size(); ++i)
    {
        const auto& step = result.steps[i];
        file << "    {\n";
        file << "      \"code\": \"" << JsonEscape(step.code) << "\",\n";
        file << "      \"status\": \"" << JsonEscape(step.status) << "\",\n";
        file << "      \"reason\": \"" << JsonEscape(step.reason) << "\"\n";
        file << "    }";
        if (i + 1 < result.steps.size())
            file << ",";
        file << "\n";
    }
    file << "  ]\n";
    file << "}\n";

    return true;
}

bool WriteEvidenceSelfTestReportMd(
    const CxEvidenceSelfTestResult& result,
    const std::string& outPath,
    std::string& reason)
{
    reason.clear();

    std::filesystem::create_directories(
        std::filesystem::path(outPath).parent_path());

    std::ofstream file(outPath, std::ios::binary);
    if (!file.is_open())
    {
        reason = "failed to open selftest report md: " + outPath;
        return false;
    }

    file << "# Evidence Chain Self Test\n\n";
    file << "- run_id: " << result.run_id << "\n";
    file << "- case_id: " << result.case_id << "\n";
    file << "- script: " << result.script_path << "\n";
    file << "- image: " << result.image_path << "\n";
    file << "- target: " << result.target_id << "\n";
    file << "- tool: " << result.tool << "\n";
    file << "- param: " << result.parameter_summary << "\n";
    file << "- final: " << result.final_code << " / " << result.final_status << "\n\n";

    file << "## Algorithm Diagnostics\n\n";
    file << "- algorithm_status: " << result.algorithm_status << "\n";
    file << "- algorithm_reason: " << result.algorithm_reason << "\n";
    file << "- measure_points_count: " << result.measure_points_count << "\n";
    file << "- valid_points_count: " << result.valid_points_count << "\n";
    file << "- has_measure_points: " << (result.has_measure_points ? "true" : "false") << "\n";
    file << "- has_fit_result: " << (result.has_fit_result ? "true" : "false") << "\n";
    file << "- avgdist: " << result.avgdist << "\n";
    if (result.tool == "FastMatch" || result.runtime_object_type == "FastMatch")
    {
        file << "- fastmatch_model_point_count: " << result.fastmatch_model_point_count << "\n";
        file << "- fastmatch_learn_a_count: " << result.fastmatch_learn_a_count << "\n";
        file << "- fastmatch_learn_b_count: " << result.fastmatch_learn_b_count << "\n";
        file << "- fastmatch_learn_a2_count: " << result.fastmatch_learn_a2_count << "\n";
        file << "- fastmatch_learn_b2_count: " << result.fastmatch_learn_b2_count << "\n";
        file << "- fastmatch_learn_status_code: " << result.fastmatch_learn_status_code << "\n";
        file << "- fastmatch_pattern_a_count: " << result.fastmatch_pattern_a_count << "\n";
        file << "- fastmatch_pattern_b_count: " << result.fastmatch_pattern_b_count << "\n";
        file << "- fastmatch_candidate_count: " << result.fastmatch_candidate_count << "\n";
        file << "- fastmatch_best_score: " << result.fastmatch_best_score << "\n";
    }
    file << "- fit: center=(" << result.fit_cx << "," << result.fit_cy
         << "), r=" << result.fit_radius
         << ", rx=" << result.fit_radius_x
         << ", ry=" << result.fit_radius_y
         << ", angle=" << result.fit_angle_deg << "\n\n";

    file << "| Step | Status | Reason |\n";
    file << "|---|---|---|\n";
    for (const auto& step : result.steps)
    {
        file << "| " << step.code
             << " | " << step.status
             << " | " << step.reason
             << " |\n";
    }

    return true;
}

bool WriteEvidenceSelfTestBatchSummaryJson(
    const CxEvidenceSelfTestBatchResult& result,
    const std::string& outPath,
    std::string& reason)
{
    reason.clear();

    std::filesystem::create_directories(
        std::filesystem::path(outPath).parent_path());

    std::ofstream file(outPath, std::ios::binary);
    if (!file.is_open())
    {
        reason = "failed to open batch selftest summary json: " + outPath;
        return false;
    }

    file << "{\n";
    file << "  \"run_id\": \"" << JsonEscape(result.run_id) << "\",\n";
    file << "  \"total_cases\": " << result.total_cases << ",\n";
    file << "  \"executed_cases\": " << result.executed_cases << ",\n";
    file << "  \"pass_count\": " << result.pass_count << ",\n";
    file << "  \"pending_count\": " << result.pending_count << ",\n";
    file << "  \"fail_count\": " << result.fail_count << ",\n";
    file << "  \"final_code\": \"" << JsonEscape(result.final_code) << "\",\n";
    file << "  \"final_status\": \"" << JsonEscape(result.final_status) << "\",\n";
    file << "  \"final_reason\": \"" << JsonEscape(result.final_reason) << "\",\n";

    file << "  \"cases\": [\n";
    for (std::size_t i = 0; i < result.case_results.size(); ++i)
    {
        const auto& caseResult = result.case_results[i];
        file << "    {\n";
        file << "      \"case_id\": \"" << JsonEscape(caseResult.case_id) << "\",\n";
        file << "      \"script_path\": \"" << JsonEscape(caseResult.script_path) << "\",\n";
        file << "      \"image_path\": \"" << JsonEscape(caseResult.image_path) << "\",\n";
        file << "      \"target_id\": \"" << JsonEscape(caseResult.target_id) << "\",\n";
        file << "      \"tool\": \"" << JsonEscape(caseResult.tool) << "\",\n";
        file << "      \"final_code\": \"" << JsonEscape(caseResult.final_code) << "\",\n";
        file << "      \"final_status\": \"" << JsonEscape(caseResult.final_status) << "\",\n";
        file << "      \"final_reason\": \"" << JsonEscape(caseResult.final_reason) << "\",\n";
        file << "      \"runtime_object_count\": " << caseResult.runtime_object_count << ",\n";
        file << "      \"shape_element_count\": " << caseResult.shape_element_count << ",\n";
        file << "      \"gauge_shape_count\": " << caseResult.gauge_shape_count << ",\n";
        file << "      \"result_shape_count\": " << caseResult.result_shape_count << ",\n";
        file << "      \"algorithm_status\": \"" << JsonEscape(caseResult.algorithm_status) << "\",\n";
        file << "      \"algorithm_reason\": \"" << JsonEscape(caseResult.algorithm_reason) << "\",\n";
        file << "      \"measure_points_count\": " << caseResult.measure_points_count << ",\n";
        file << "      \"valid_points_count\": " << caseResult.valid_points_count << ",\n";
        file << "      \"has_fit_result\": " << (caseResult.has_fit_result ? "true" : "false") << "\n";
        file << "    }";
        if (i + 1 < result.case_results.size())
            file << ",";
        file << "\n";
    }
    file << "  ]\n";
    file << "}\n";

    return true;
}

bool WriteEvidenceSelfTestBatchReportMd(
    const CxEvidenceSelfTestBatchResult& result,
    const std::string& outPath,
    std::string& reason)
{
    reason.clear();

    std::filesystem::create_directories(
        std::filesystem::path(outPath).parent_path());

    std::ofstream file(outPath, std::ios::binary);
    if (!file.is_open())
    {
        reason = "failed to open batch selftest report md: " + outPath;
        return false;
    }

    file << "# Evidence Chain Batch Self Test\n\n";
    file << "- run_id: " << result.run_id << "\n";
    file << "- total_cases: " << result.total_cases << "\n";
    file << "- executed_cases: " << result.executed_cases << "\n";
    file << "- pass_count: " << result.pass_count << "\n";
    file << "- pending_count: " << result.pending_count << "\n";
    file << "- fail_count: " << result.fail_count << "\n";
    file << "- final: " << result.final_code << " / " << result.final_status << "\n";
    file << "- reason: " << result.final_reason << "\n\n";

    file << "| Case | Script | Image | Target | Tool | RuntimeObject | GaugeShapes | ResultShapes | Points | Fit | Algorithm | Final | Reason |\n";
    file << "|---|---|---|---|---|---:|---:|---:|---:|---|---|---|---|\n";
    for (const auto& caseResult : result.case_results)
    {
        file << "| " << caseResult.case_id
             << " | " << caseResult.script_path
             << " | " << caseResult.image_path
             << " | " << caseResult.target_id
             << " | " << caseResult.tool
             << " | " << caseResult.runtime_object_count
             << " | " << caseResult.gauge_shape_count
             << " | " << caseResult.result_shape_count
             << " | " << caseResult.valid_points_count
             << " | " << (caseResult.has_fit_result ? "true" : "false")
             << " | " << caseResult.algorithm_status
             << " | " << caseResult.final_code
             << " | " << caseResult.final_reason
             << " |\n";
    }

    return true;
}
