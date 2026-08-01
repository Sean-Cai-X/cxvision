#include "CxEvidenceSelfTestRuntime.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#include <opencv2/imgcodecs.hpp>

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

bool StartsWith(const std::string& text, const std::string& prefix)
{
    return text.size() >= prefix.size() &&
           text.compare(0, prefix.size(), prefix) == 0;
}

bool LooksLikeNonFileArtifactRef(const std::string& ref)
{
    return StartsWith(ref, "overlay:") ||
           StartsWith(ref, "mask:") ||
           StartsWith(ref, "contour:") ||
           StartsWith(ref, "result:") ||
           StartsWith(ref, "evidence:");
}

std::string ToLowerAscii(std::string text)
{
    for (char& c : text)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

bool LooksLikeImagePath(const std::string& path)
{
    const std::string lower = ToLowerAscii(path);
    return lower.size() >= 4 &&
           (lower.rfind(".png") == lower.size() - 4 ||
            lower.rfind(".jpg") == lower.size() - 4 ||
            lower.rfind(".bmp") == lower.size() - 4 ||
            (lower.size() >= 5 && lower.rfind(".jpeg") == lower.size() - 5));
}

struct CxArtifactAuditItem
{
    std::string role;
    std::string ref;
    bool empty = true;
    bool non_file_ref = false;
    bool exists = false;
    bool is_file = false;
    std::uintmax_t size_bytes = 0;
    bool image_read_ok = false;
    int width = 0;
    int height = 0;
    std::string status;
    std::string reason;
};

CxArtifactAuditItem AuditArtifactRef(
    const std::string& role,
    const std::string& ref)
{
    CxArtifactAuditItem item;
    item.role = role;
    item.ref = ref;
    item.empty = ref.empty();
    if (item.empty)
    {
        item.status = "EMPTY";
        item.reason = "artifact ref is empty";
        return item;
    }

    item.non_file_ref = LooksLikeNonFileArtifactRef(ref);
    if (item.non_file_ref)
    {
        item.status = "NON_FILE_REF";
        item.reason = "artifact ref is a runtime reference, not a filesystem path";
        return item;
    }

    std::error_code ec;
    const std::filesystem::path path(ref);
    item.exists = std::filesystem::exists(path, ec);
    item.is_file = item.exists && std::filesystem::is_regular_file(path, ec);
    if (item.is_file)
        item.size_bytes = std::filesystem::file_size(path, ec);

    if (!item.exists)
    {
        item.status = "MISSING";
        item.reason = "file does not exist";
        return item;
    }
    if (!item.is_file)
    {
        item.status = "NOT_FILE";
        item.reason = "path exists but is not a regular file";
        return item;
    }

    if (LooksLikeImagePath(ref))
    {
        const cv::Mat image = cv::imread(ref, cv::IMREAD_UNCHANGED);
        item.image_read_ok = !image.empty();
        if (item.image_read_ok)
        {
            item.width = image.cols;
            item.height = image.rows;
            item.status = "FILE_IMAGE_READ_OK";
            item.reason = "image artifact exists and can be opened";
        }
        else
        {
            item.status = "FILE_IMAGE_READ_FAIL";
            item.reason = "image artifact exists but cv::imread failed";
        }
        return item;
    }

    item.status = item.size_bytes > 0 ? "FILE_OK" : "FILE_EMPTY";
    item.reason = item.size_bytes > 0
        ? "file artifact exists"
        : "file artifact exists but has zero bytes";
    return item;
}

bool IsTorchEvidenceCase(const CxEvidenceSelfTestResult& result)
{
    const std::string tool = ToLowerAscii(result.tool);
    const std::string runtime = ToLowerAscii(result.runtime_object_type);
    const std::string path = ToLowerAscii(result.script_path);
    return tool.find("torch") != std::string::npos ||
           runtime.find("torch") != std::string::npos ||
           tool.find("findsegmentation") != std::string::npos ||
           runtime.find("findsegmentation") != std::string::npos ||
           path.find("/torch/") != std::string::npos ||
           path.find("\\torch\\") != std::string::npos ||
           path.find("find_segmentation") != std::string::npos ||
           path.find("edgesam") != std::string::npos ||
           path.find("libtorch") != std::string::npos;
}

bool IsArtifactOkForSmoke(const CxArtifactAuditItem& item)
{
    if (item.empty)
        return false;
    if (item.non_file_ref)
        return true;
    if (!item.exists || !item.is_file || item.size_bytes == 0)
        return false;
    if (LooksLikeImagePath(item.ref) && !item.image_read_ok)
        return false;
    return true;
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
    file << "  \"primary_object_type\": \"" << JsonEscape(result.primary_object_type) << "\",\n";
    file << "  \"primary_object_name\": \"" << JsonEscape(result.primary_object_name) << "\",\n";
    file << "  \"primary_object_status\": \"" << JsonEscape(result.primary_object_status) << "\",\n";
    file << "  \"editable_object_count\": " << result.editable_object_count << ",\n";

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
    file << "  \"torch_result_ref\": \"" << JsonEscape(result.torch_result_ref) << "\",\n";
    file << "  \"torch_evidence_ref\": \"" << JsonEscape(result.torch_evidence_ref) << "\",\n";
    file << "  \"torch_primary_visual_ref\": \"" << JsonEscape(result.torch_primary_visual_ref) << "\",\n";
    file << "  \"torch_mask_ref\": \"" << JsonEscape(result.torch_mask_ref) << "\",\n";
    file << "  \"torch_overlay_ref\": \"" << JsonEscape(result.torch_overlay_ref) << "\",\n";
    file << "  \"torch_actual_device\": \"" << JsonEscape(result.torch_actual_device) << "\",\n";
    file << "  \"torch_ok\": " << result.torch_ok << ",\n";
    file << "  \"torch_error_code\": " << result.torch_error_code << ",\n";
    file << "  \"torch_result_count\": " << result.torch_result_count << ",\n";
    file << "  \"torch_mask_available\": " << result.torch_mask_available << ",\n";
    file << "  \"torch_infer_ms\": " << result.torch_infer_ms << ",\n";
    file << "  \"torch_train_ms\": " << result.torch_train_ms << ",\n";
    file << "  \"torch_total_ms\": " << result.torch_total_ms << ",\n";
    file << "  \"torch_trainer_lifecycle_summary\": \"" << JsonEscape(result.torch_trainer_lifecycle_summary) << "\",\n";
    file << "  \"torch_unified_mainline_summary\": \"" << JsonEscape(result.torch_unified_mainline_summary) << "\",\n";
    file << "  \"segmentation_backend_status\": \"" << JsonEscape(result.segmentation_backend_status) << "\",\n";
    file << "  \"segmentation_result_ref\": \"" << JsonEscape(result.segmentation_result_ref) << "\",\n";
    file << "  \"segmentation_mask_ref\": \"" << JsonEscape(result.segmentation_mask_ref) << "\",\n";
    file << "  \"segmentation_contour_ref\": \"" << JsonEscape(result.segmentation_contour_ref) << "\",\n";
    file << "  \"segmentation_overlay_ref\": \"" << JsonEscape(result.segmentation_overlay_ref) << "\",\n";
    file << "  \"segmentation_contour_count\": " << result.segmentation_contour_count << ",\n";
    file << "  \"segmentation_primary_area\": " << result.segmentation_primary_area << ",\n";

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
    file << "- primary_object: " << result.primary_object_type << " "
         << result.primary_object_name << " | "
         << result.primary_object_status << "\n";
    file << "- editable_object_count: " << result.editable_object_count << "\n";
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
    if (result.tool == "TorchTask" || result.runtime_object_type == "TorchTask")
    {
        file << "- torch_ok: " << result.torch_ok << "\n";
        file << "- torch_status: " << result.algorithm_status << "\n";
        file << "- torch_actual_device: " << result.torch_actual_device << "\n";
        file << "- torch_result_ref: " << result.torch_result_ref << "\n";
        file << "- torch_evidence_ref: " << result.torch_evidence_ref << "\n";
        file << "- torch_primary_visual_ref: " << result.torch_primary_visual_ref << "\n";
        file << "- torch_mask_ref: " << result.torch_mask_ref << "\n";
        file << "- torch_overlay_ref: " << result.torch_overlay_ref << "\n";
        file << "- torch_result_count: " << result.torch_result_count << "\n";
        file << "- torch_mask_available: " << result.torch_mask_available << "\n";
        file << "- torch_infer_ms: " << result.torch_infer_ms << "\n";
        file << "- torch_train_ms: " << result.torch_train_ms << "\n";
        file << "- torch_total_ms: " << result.torch_total_ms << "\n";
        file << "- torch_trainer_lifecycle_summary: "
             << result.torch_trainer_lifecycle_summary << "\n";
        file << "- torch_unified_mainline_summary: "
             << result.torch_unified_mainline_summary << "\n";
        file << "- model_semantic_quality: NOT_CLAIMED\n";
    }
    if (result.tool == "FindSegmentation" ||
        result.runtime_object_type == "FindSegmentation")
    {
        file << "- segmentation_backend_status: "
             << result.segmentation_backend_status << "\n";
        file << "- segmentation_result_ref: "
             << result.segmentation_result_ref << "\n";
        file << "- segmentation_mask_ref: "
             << result.segmentation_mask_ref << "\n";
        file << "- segmentation_contour_ref: "
             << result.segmentation_contour_ref << "\n";
        file << "- segmentation_overlay_ref: "
             << result.segmentation_overlay_ref << "\n";
        file << "- segmentation_contour_count: "
             << result.segmentation_contour_count << "\n";
        file << "- segmentation_primary_area: "
             << result.segmentation_primary_area << "\n";
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
        file << "      \"primary_object_type\": \"" << JsonEscape(caseResult.primary_object_type) << "\",\n";
        file << "      \"primary_object_name\": \"" << JsonEscape(caseResult.primary_object_name) << "\",\n";
        file << "      \"primary_object_status\": \"" << JsonEscape(caseResult.primary_object_status) << "\",\n";
        file << "      \"editable_object_count\": " << caseResult.editable_object_count << ",\n";
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
        file << "      \"has_fit_result\": " << (caseResult.has_fit_result ? "true" : "false") << ",\n";
        file << "      \"torch_result_ref\": \"" << JsonEscape(caseResult.torch_result_ref) << "\",\n";
        file << "      \"torch_evidence_ref\": \"" << JsonEscape(caseResult.torch_evidence_ref) << "\",\n";
        file << "      \"torch_primary_visual_ref\": \"" << JsonEscape(caseResult.torch_primary_visual_ref) << "\",\n";
        file << "      \"torch_mask_ref\": \"" << JsonEscape(caseResult.torch_mask_ref) << "\",\n";
        file << "      \"torch_overlay_ref\": \"" << JsonEscape(caseResult.torch_overlay_ref) << "\",\n";
        file << "      \"torch_actual_device\": \"" << JsonEscape(caseResult.torch_actual_device) << "\",\n";
        file << "      \"torch_ok\": " << caseResult.torch_ok << ",\n";
        file << "      \"torch_result_count\": " << caseResult.torch_result_count << ",\n";
        file << "      \"torch_mask_available\": " << caseResult.torch_mask_available << ",\n";
        file << "      \"segmentation_result_ref\": \"" << JsonEscape(caseResult.segmentation_result_ref) << "\",\n";
        file << "      \"segmentation_mask_ref\": \"" << JsonEscape(caseResult.segmentation_mask_ref) << "\",\n";
        file << "      \"segmentation_contour_ref\": \"" << JsonEscape(caseResult.segmentation_contour_ref) << "\",\n";
        file << "      \"segmentation_overlay_ref\": \"" << JsonEscape(caseResult.segmentation_overlay_ref) << "\",\n";
        file << "      \"segmentation_contour_count\": " << caseResult.segmentation_contour_count << ",\n";
        file << "      \"segmentation_primary_area\": " << caseResult.segmentation_primary_area << "\n";
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

    file << "| Case | Script | Image | Target | Tool | PrimaryObject | EditableObjects | RuntimeObject | GaugeShapes | ResultShapes | Points | Fit | Algorithm | Final | Reason |\n";
    file << "|---|---|---|---|---|---|---:|---:|---:|---:|---:|---|---|---|---|\n";
    for (const auto& caseResult : result.case_results)
    {
        file << "| " << caseResult.case_id
             << " | " << caseResult.script_path
             << " | " << caseResult.image_path
             << " | " << caseResult.target_id
             << " | " << caseResult.tool
             << " | " << caseResult.primary_object_type << " "
             << caseResult.primary_object_name << " / "
             << caseResult.primary_object_status
             << " | " << caseResult.editable_object_count
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

bool WriteTorchArtifactAuditReport(
    const CxEvidenceSelfTestBatchResult& result,
    const std::string& outDir,
    std::string& reason)
{
    reason.clear();

    std::filesystem::create_directories(outDir);
    const std::string jsonPath =
        (std::filesystem::path(outDir) / "torch_artifact_audit.json").string();
    const std::string mdPath =
        (std::filesystem::path(outDir) / "torch_artifact_audit.md").string();

    struct CaseAudit
    {
        const CxEvidenceSelfTestResult* result = nullptr;
        std::vector<CxArtifactAuditItem> artifacts;
        bool smoke_ok = false;
        std::string detection_non_empty_result;
        std::string mask_artifact_available;
        std::string segmentation_attach_available;
        std::string reason;
    };

    std::vector<CaseAudit> audits;
    for (const CxEvidenceSelfTestResult& caseResult : result.case_results)
    {
        if (!IsTorchEvidenceCase(caseResult))
            continue;

        CaseAudit audit;
        audit.result = &caseResult;
        audit.detection_non_empty_result =
            caseResult.torch_result_count > 0
                ? "OBSERVED_NOT_SEMANTIC_ACCEPTED"
                : "UNVERIFIED_EMPTY";
        audit.mask_artifact_available =
            (caseResult.torch_mask_available != 0 ||
             !caseResult.torch_mask_ref.empty() ||
             !caseResult.segmentation_mask_ref.empty())
                ? "PRESENT_OR_REFERENCED"
                : "UNVERIFIED_EMPTY";
        audit.segmentation_attach_available =
            (caseResult.segmentation_contour_count > 0 &&
             !caseResult.segmentation_contour_ref.empty())
                ? "AVAILABLE"
                : "PENDING_OR_EMPTY";

        if (caseResult.runtime_object_type == "TorchTask" ||
            caseResult.tool == "TorchTask")
        {
            audit.artifacts.push_back(
                AuditArtifactRef("torch_result_ref",
                                 caseResult.torch_result_ref.empty()
                                     ? caseResult.result_ref
                                     : caseResult.torch_result_ref));
            audit.artifacts.push_back(
                AuditArtifactRef("torch_evidence_ref",
                                 caseResult.torch_evidence_ref.empty()
                                     ? caseResult.evidence_ref
                                     : caseResult.torch_evidence_ref));
            audit.artifacts.push_back(
                AuditArtifactRef("torch_primary_visual_ref",
                                 caseResult.torch_primary_visual_ref));
            audit.artifacts.push_back(
                AuditArtifactRef("torch_mask_ref",
                                 caseResult.torch_mask_ref));
            audit.artifacts.push_back(
                AuditArtifactRef("torch_overlay_ref",
                                 caseResult.torch_overlay_ref));

            bool requiredOk = caseResult.torch_ok != 0;
            requiredOk = requiredOk && IsArtifactOkForSmoke(audit.artifacts[0]);
            requiredOk = requiredOk && IsArtifactOkForSmoke(audit.artifacts[1]);
            if (!caseResult.torch_primary_visual_ref.empty())
                requiredOk = requiredOk && IsArtifactOkForSmoke(audit.artifacts[2]);
            if (caseResult.torch_mask_available != 0 ||
                !caseResult.torch_mask_ref.empty())
                requiredOk = requiredOk && IsArtifactOkForSmoke(audit.artifacts[3]);
            if (!caseResult.torch_overlay_ref.empty())
                requiredOk = requiredOk && IsArtifactOkForSmoke(audit.artifacts[4]);

            audit.smoke_ok = requiredOk;
            audit.reason = audit.smoke_ok
                ? "TorchTask runtime artifact chain is readable; model semantic quality is NOT_CLAIMED."
                : "TorchTask runtime artifact chain has missing or unreadable required artifacts.";
        }
        else if (caseResult.runtime_object_type == "FindSegmentation" ||
                 caseResult.tool.find("FindSegmentation") != std::string::npos)
        {
            audit.artifacts.push_back(
                AuditArtifactRef("segmentation_result_ref",
                                 caseResult.segmentation_result_ref.empty()
                                     ? caseResult.result_ref
                                     : caseResult.segmentation_result_ref));
            audit.artifacts.push_back(
                AuditArtifactRef("segmentation_mask_ref",
                                 caseResult.segmentation_mask_ref));
            audit.artifacts.push_back(
                AuditArtifactRef("segmentation_contour_ref",
                                 caseResult.segmentation_contour_ref));
            audit.artifacts.push_back(
                AuditArtifactRef("segmentation_overlay_ref",
                                 caseResult.segmentation_overlay_ref));

            bool requiredOk = caseResult.segmentation_contour_count > 0 &&
                              caseResult.segmentation_primary_area > 0.0;
            requiredOk = requiredOk && IsArtifactOkForSmoke(audit.artifacts[0]);
            requiredOk = requiredOk && IsArtifactOkForSmoke(audit.artifacts[1]);
            requiredOk = requiredOk && IsArtifactOkForSmoke(audit.artifacts[2]);
            if (!caseResult.segmentation_overlay_ref.empty())
                requiredOk = requiredOk && IsArtifactOkForSmoke(audit.artifacts[3]);

            audit.smoke_ok = requiredOk;
            audit.reason = audit.smoke_ok
                ? "FindSegmentation libtorch/smoke attach chain is readable; boundary attach is available."
                : "FindSegmentation attach chain has missing artifacts or no contour/area evidence.";
        }
        else
        {
            audit.smoke_ok = false;
            audit.reason = "Torch-related case type is not covered by artifact audit.";
        }

        audits.push_back(audit);
    }

    int smokePass = 0;
    for (const CaseAudit& audit : audits)
    {
        if (audit.smoke_ok)
            ++smokePass;
    }

    std::ofstream json(jsonPath, std::ios::binary);
    if (!json.is_open())
    {
        reason = "failed to open torch artifact audit json: " + jsonPath;
        return false;
    }

    json << "{\n";
    json << "  \"run_id\": \"" << JsonEscape(result.run_id) << "\",\n";
    json << "  \"audit_scope\": \"torch_artifact_chain_smoke\",\n";
    json << "  \"model_semantic_quality\": \"NOT_CLAIMED\",\n";
    json << "  \"total_cases\": " << audits.size() << ",\n";
    json << "  \"smoke_artifact_pass_count\": " << smokePass << ",\n";
    json << "  \"smoke_artifact_fail_count\": " << (audits.size() - smokePass) << ",\n";
    json << "  \"conclusion\": \""
         << (smokePass == static_cast<int>(audits.size())
                 ? "TORCH_ARTIFACT_AUDIT_PASS"
                 : "TORCH_ARTIFACT_AUDIT_FAIL")
         << "\",\n";
    json << "  \"cases\": [\n";
    for (std::size_t i = 0; i < audits.size(); ++i)
    {
        const CaseAudit& audit = audits[i];
        const CxEvidenceSelfTestResult& caseResult = *audit.result;
        json << "    {\n";
        json << "      \"case_id\": \"" << JsonEscape(caseResult.case_id) << "\",\n";
        json << "      \"script_path\": \"" << JsonEscape(caseResult.script_path) << "\",\n";
        json << "      \"tool\": \"" << JsonEscape(caseResult.tool) << "\",\n";
        json << "      \"runtime_object_type\": \"" << JsonEscape(caseResult.runtime_object_type) << "\",\n";
        json << "      \"algorithm_status\": \"" << JsonEscape(caseResult.algorithm_status) << "\",\n";
        json << "      \"smoke_artifact_ok\": " << (audit.smoke_ok ? "true" : "false") << ",\n";
        json << "      \"reason\": \"" << JsonEscape(audit.reason) << "\",\n";
        json << "      \"model_semantic_quality\": \"NOT_CLAIMED\",\n";
        json << "      \"detection_non_empty_result\": \""
             << JsonEscape(audit.detection_non_empty_result) << "\",\n";
        json << "      \"mask_artifact_available\": \""
             << JsonEscape(audit.mask_artifact_available) << "\",\n";
        json << "      \"segmentation_attach_available\": \""
             << JsonEscape(audit.segmentation_attach_available) << "\",\n";
        json << "      \"torch_actual_device\": \""
             << JsonEscape(caseResult.torch_actual_device) << "\",\n";
        json << "      \"torch_result_count\": " << caseResult.torch_result_count << ",\n";
        json << "      \"torch_mask_available\": " << caseResult.torch_mask_available << ",\n";
        json << "      \"segmentation_contour_count\": " << caseResult.segmentation_contour_count << ",\n";
        json << "      \"segmentation_primary_area\": " << caseResult.segmentation_primary_area << ",\n";
        json << "      \"artifacts\": [\n";
        for (std::size_t j = 0; j < audit.artifacts.size(); ++j)
        {
            const CxArtifactAuditItem& item = audit.artifacts[j];
            json << "        {\n";
            json << "          \"role\": \"" << JsonEscape(item.role) << "\",\n";
            json << "          \"ref\": \"" << JsonEscape(item.ref) << "\",\n";
            json << "          \"empty\": " << (item.empty ? "true" : "false") << ",\n";
            json << "          \"non_file_ref\": " << (item.non_file_ref ? "true" : "false") << ",\n";
            json << "          \"exists\": " << (item.exists ? "true" : "false") << ",\n";
            json << "          \"is_file\": " << (item.is_file ? "true" : "false") << ",\n";
            json << "          \"size_bytes\": " << item.size_bytes << ",\n";
            json << "          \"image_read_ok\": " << (item.image_read_ok ? "true" : "false") << ",\n";
            json << "          \"width\": " << item.width << ",\n";
            json << "          \"height\": " << item.height << ",\n";
            json << "          \"status\": \"" << JsonEscape(item.status) << "\",\n";
            json << "          \"reason\": \"" << JsonEscape(item.reason) << "\"\n";
            json << "        }";
            if (j + 1 < audit.artifacts.size())
                json << ",";
            json << "\n";
        }
        json << "      ]\n";
        json << "    }";
        if (i + 1 < audits.size())
            json << ",";
        json << "\n";
    }
    json << "  ]\n";
    json << "}\n";

    std::ofstream md(mdPath, std::ios::binary);
    if (!md.is_open())
    {
        reason = "failed to open torch artifact audit md: " + mdPath;
        return false;
    }

    md << "# Torch Artifact Audit\n\n";
    md << "- run_id: " << result.run_id << "\n";
    md << "- audit_scope: torch_artifact_chain_smoke\n";
    md << "- model_semantic_quality: NOT_CLAIMED\n";
    md << "- total_cases: " << audits.size() << "\n";
    md << "- smoke_artifact_pass_count: " << smokePass << "\n";
    md << "- smoke_artifact_fail_count: " << (audits.size() - smokePass) << "\n\n";

    md << "| Case | Tool | Runtime | Status | ArtifactSmoke | Detection | Mask | Attach | Reason |\n";
    md << "|---|---|---|---|---|---|---|---|---|\n";
    for (const CaseAudit& audit : audits)
    {
        const CxEvidenceSelfTestResult& caseResult = *audit.result;
        md << "| " << caseResult.case_id
           << " | " << caseResult.tool
           << " | " << caseResult.runtime_object_type
           << " | " << caseResult.algorithm_status
           << " | " << (audit.smoke_ok ? "PASS" : "FAIL")
           << " | " << audit.detection_non_empty_result
           << " | " << audit.mask_artifact_available
           << " | " << audit.segmentation_attach_available
           << " | " << audit.reason
           << " |\n";
    }

    md << "\n## Artifact Details\n\n";
    md << "| Case | Role | Ref | Status | Exists | Size | ImageRead | WxH | Reason |\n";
    md << "|---|---|---|---|---|---:|---|---|---|\n";
    for (const CaseAudit& audit : audits)
    {
        const CxEvidenceSelfTestResult& caseResult = *audit.result;
        for (const CxArtifactAuditItem& item : audit.artifacts)
        {
            md << "| " << caseResult.case_id
               << " | " << item.role
               << " | " << item.ref
               << " | " << item.status
               << " | " << (item.exists ? "true" : "false")
               << " | " << item.size_bytes
               << " | " << (item.image_read_ok ? "true" : "false")
               << " | " << item.width << "x" << item.height
               << " | " << item.reason
               << " |\n";
        }
    }

    reason = "torch artifact audit written";
    return true;
}
