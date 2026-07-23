#include "CxScriptSuiteReportWriter.h"
#include <fstream>
#include <iomanip>
#include <map>

void CxScriptSuiteReportWriter::WriteSuiteRunReport(
    const std::filesystem::path& outRoot,
    const std::vector<CxScriptSuiteCaseResult>& caseResults)
{
    const std::filesystem::path reportsDir = outRoot / "reports";
    std::filesystem::create_directories(reportsDir);

    const std::filesystem::path reportPath = reportsDir / "suite_run_report.md";
    std::ofstream file(reportPath);

    if (!file.is_open())
        return;

    file << "# Suite Run Report\n\n";
    file << "| Case | Level | Image | Tool | Script | Expected | Points | Fit | PolicyGuard | ContractPass | Conclusion |\n";
    file << "|------|-------|-------|------|--------|----------|--------|-----|-------------|--------------|------------|\n";

    for (const auto& result : caseResults)
    {
        file << "| " << result.case_id << " | ";
        file << result.level << " | ";
        file << result.image_id << " | ";
        file << result.tool << " | ";
        file << result.script_id << " | ";
        file << result.expected_result << " | ";
        file << result.valid_points_count << " | ";
        file << (result.has_fit_line || result.has_fit_circle ? "yes" : "no") << " | ";
        file << result.actual_policy_guard << " | ";
        file << (result.contract_pass ? "yes" : "no") << " | ";
        file << result.conclusion << " |\n";
    }

    file << "\n## Summary\n\n";
    int passed = 0, failed = 0, executed = 0;
    for (const auto& r : caseResults)
    {
        if (r.headless_ok) executed++;
        if (r.contract_pass) passed++;
        else if (r.headless_ok) failed++;
    }
    file << "- Total cases: " << caseResults.size() << "\n";
    file << "- Executed: " << executed << "\n";
    file << "- Contract pass: " << passed << "\n";
    file << "- Contract fail: " << failed << "\n";
}

void CxScriptSuiteReportWriter::WriteSuiteRunReportJson(
    const std::filesystem::path& outRoot,
    const std::vector<CxScriptSuiteCaseResult>& caseResults)
{
    const std::filesystem::path reportsDir = outRoot / "reports";
    std::filesystem::create_directories(reportsDir);

    const std::filesystem::path reportPath = reportsDir / "suite_run_report.json";
    std::ofstream file(reportPath);

    if (!file.is_open())
        return;

    file << "{\n";
    file << "  \"total_cases\": " << caseResults.size() << ",\n";
    int passed = 0, failed = 0, executed = 0;
    for (const auto& r : caseResults)
    {
        if (r.headless_ok) executed++;
        if (r.contract_pass) passed++;
        else if (r.headless_ok) failed++;
    }
    file << "  \"executed_cases\": " << executed << ",\n";
    file << "  \"contract_pass\": " << passed << ",\n";
    file << "  \"contract_fail\": " << failed << ",\n";
    file << "  \"cases\": [\n";

    for (size_t i = 0; i < caseResults.size(); ++i)
    {
        const auto& r = caseResults[i];
        file << "    {\n";
        file << "      \"case_id\": \"" << r.case_id << "\",\n";
        file << "      \"script_id\": \"" << r.script_id << "\",\n";
        file << "      \"image_id\": \"" << r.image_id << "\",\n";
        file << "      \"level\": \"" << r.level << "\",\n";
        file << "      \"tool\": \"" << r.tool << "\",\n";
        file << "      \"expected_result\": \"" << r.expected_result << "\",\n";
        file << "      \"actual_policy_guard\": \"" << r.actual_policy_guard << "\",\n";
        file << "      \"contract_pass\": " << (r.contract_pass ? "true" : "false") << ",\n";
        file << "      \"conclusion\": \"" << r.conclusion << "\"\n";
        file << "    }";
        if (i < caseResults.size() - 1) file << ",";
        file << "\n";
    }

    file << "  ]\n";
    file << "}\n";
}

void CxScriptSuiteReportWriter::WriteImageManifestContractReport(
    const std::filesystem::path& outRoot,
    const CxScriptImageManifestRuntime& manifest,
    const CxScriptImageManifestValidationResult& validation)
{
    const std::filesystem::path reportsDir = outRoot / "reports";
    std::filesystem::create_directories(reportsDir);

    const std::filesystem::path reportPath = reportsDir / "image_manifest_contract_report.md";
    std::ofstream file(reportPath);

    if (!file.is_open())
        return;

    file << "# Stage25 Image Manifest Contract Report\n\n";
    file << "- Manifest: " << manifest.manifest_path << "\n";
    file << "- Schema: " << manifest.schema_version << "\n";
    file << "- Images: " << manifest.total_images << "\n";
    file << "- L0: " << manifest.l0_count << "\n";
    file << "- L1: " << manifest.l1_count << "\n";
    file << "- L2: " << manifest.l2_count << "\n";
    file << "- L3: " << manifest.l3_count << "\n";
    file << "- Raw not cropped: required\n";
    file << "- Raw not enhanced: required\n";
    file << "- Raw not rotated: required\n\n";

    file << "## Validation Status: " << (validation.ok ? "PASS" : "FAIL") << "\n\n";

    if (!validation.issues.empty())
    {
        file << "### Issues\n\n";
        file << "| Severity | Image | Target | Message |\n";
        file << "|----------|-------|--------|----------|\n";
        for (const auto& issue : validation.issues)
        {
            file << "| " << issue.severity << " | ";
            file << issue.image_id << " | ";
            file << issue.target_id << " | ";
            file << issue.message << " |\n";
        }
        file << "\n";
    }

    file << "## Image List\n\n";
    file << "| Image | Level | Path | Size | Status |\n";
    file << "|-------|-------|------|------|--------|\n";
    for (const auto& image : manifest.images)
    {
        bool exists = std::filesystem::exists(image.path);
        file << "| " << image.image_id << " | ";
        file << image.level << " | ";
        file << image.path << " | ";
        file << image.width << "x" << image.height << " | ";
        file << (exists ? "OK" : "MISSING") << " |\n";
    }
}

void CxScriptSuiteReportWriter::WriteImageManifestContractReportJson(
    const std::filesystem::path& outRoot,
    const CxScriptImageManifestRuntime& manifest,
    const CxScriptImageManifestValidationResult& validation)
{
    const std::filesystem::path reportsDir = outRoot / "reports";
    std::filesystem::create_directories(reportsDir);

    const std::filesystem::path reportPath = reportsDir / "image_manifest_contract_report.json";
    std::ofstream file(reportPath);

    if (!file.is_open())
        return;

    file << "{\n";
    file << "  \"manifest_path\": \"" << manifest.manifest_path << "\",\n";
    file << "  \"schema_version\": \"" << manifest.schema_version << "\",\n";
    file << "  \"total_images\": " << manifest.total_images << ",\n";
    file << "  \"l0_count\": " << manifest.l0_count << ",\n";
    file << "  \"l1_count\": " << manifest.l1_count << ",\n";
    file << "  \"l2_count\": " << manifest.l2_count << ",\n";
    file << "  \"l3_count\": " << manifest.l3_count << ",\n";
    file << "  \"validation_ok\": " << (validation.ok ? "true" : "false") << ",\n";
    file << "  \"issues\": [\n";

    for (size_t i = 0; i < validation.issues.size(); ++i)
    {
        const auto& issue = validation.issues[i];
        file << "    {\n";
        file << "      \"severity\": \"" << issue.severity << "\",\n";
        file << "      \"image_id\": \"" << issue.image_id << "\",\n";
        file << "      \"target_id\": \"" << issue.target_id << "\",\n";
        file << "      \"message\": \"" << issue.message << "\"\n";
        file << "    }";
        if (i < validation.issues.size() - 1) file << ",";
        file << "\n";
    }

    file << "  ],\n";
    file << "  \"images\": [\n";

    for (size_t i = 0; i < manifest.images.size(); ++i)
    {
        const auto& image = manifest.images[i];
        file << "    {\n";
        file << "      \"image_id\": \"" << image.image_id << "\",\n";
        file << "      \"level\": \"" << image.level << "\",\n";
        file << "      \"path\": \"" << image.path << "\",\n";
        file << "      \"width\": " << image.width << ",\n";
        file << "      \"height\": " << image.height << "\n";
        file << "    }";
        if (i < manifest.images.size() - 1) file << ",";
        file << "\n";
    }

    file << "  ]\n";
    file << "}\n";
}

void CxScriptSuiteReportWriter::WriteBestDetectionGallery(
    const std::filesystem::path& outRoot,
    const std::vector<CxScriptSuiteCaseResult>& caseResults)
{
    const std::filesystem::path reportsDir = outRoot / "reports";
    std::filesystem::create_directories(reportsDir);

    const std::filesystem::path reportPath = reportsDir / "best_detection_gallery.md";
    std::ofstream file(reportPath);

    if (!file.is_open())
        return;

    file << "# Best Detection Gallery\n\n";
    file << "| Level | Image | Tool | Script | Expected | Conclusion | Support | MeanDist | FitOffset |\n";
    file << "|-------|-------|------|--------|----------|------------|---------|----------|-----------|\n";

    for (const auto& result : caseResults)
    {
        if (!result.contract_pass)
            continue;

        file << "| " << result.level << " | ";
        file << result.image_id << " | ";
        file << result.tool << " | ";
        file << result.script_id << " | ";
        file << result.expected_result << " | ";
        file << result.conclusion << " | ";
        file << std::fixed << std::setprecision(2) << result.local_support << " | ";
        file << std::fixed << std::setprecision(2) << result.local_mean_distance << " | ";
        file << std::fixed << std::setprecision(2) << result.fit_offset << " |\n";
    }
}

void CxScriptSuiteReportWriter::WriteToolDisplayIndex(
    const std::filesystem::path& outRoot,
    const std::vector<CxScriptSuiteCaseResult>& caseResults)
{
    const std::filesystem::path reportsDir = outRoot / "reports";
    std::filesystem::create_directories(reportsDir);

    const std::filesystem::path reportPath = reportsDir / "tool_display_index.md";
    std::ofstream file(reportPath);

    if (!file.is_open())
        return;

    file << "# Tool Display Index\n\n";
    file << "| Level | Image | Tool | Script | ResultOverlay | EvidenceOverlay | ToolDisplay |\n";
    file << "|-------|-------|------|--------|---------------|-----------------|-------------|\n";

    for (const auto& result : caseResults)
    {
        file << "| " << result.level << " | ";
        file << result.image_id << " | ";
        file << result.tool << " | ";
        file << result.script_id << " | ";
        file << (!result.result_overlay_path.empty() ? "yes" : "no") << " | ";
        file << (!result.evidence_overlay_path.empty() ? "yes" : "no") << " | ";
        file << (!result.tool_display_path.empty() ? "yes" : "no") << " |\n";
    }
}

void CxScriptSuiteReportWriter::WriteFindlineAlgorithmIterationReport(
    const std::filesystem::path& outRoot,
    const std::vector<CxScriptSuiteCaseResult>& caseResults)
{
    const std::filesystem::path reportsDir = outRoot / "reports";
    std::filesystem::create_directories(reportsDir);

    const std::filesystem::path reportPath = reportsDir / "findline_algorithm_iteration_report.md";
    std::ofstream file(reportPath);

    if (!file.is_open())
        return;

    file << "# Findline Algorithm Iteration Report\n\n";
    file << "| Level | Image | Target | Script | Points | FitLine | Support | MeanDist | FitOffset | Contract | Status | Conclusion | ToolDisplay |\n";
    file << "|-------|-------|--------|--------|--------|---------|---------|----------|-----------|----------|--------|------------|-------------|\n";

    int passed = 0, failed = 0;
    for (const auto& result : caseResults)
    {
        if (result.tool != "FindLine")
            continue;

        file << "| " << result.level << " | ";
        file << result.image_id << " | ";
        file << result.target_id << " | ";
        file << result.script_id << " | ";
        file << result.valid_points_count << " | ";
        file << (result.has_fit_line ? "yes" : "no") << " | ";
        file << std::fixed << std::setprecision(2) << result.local_support << " | ";
        file << std::fixed << std::setprecision(2) << result.local_mean_distance << " | ";
        file << std::fixed << std::setprecision(2) << result.fit_offset << " | ";
        file << (result.contract_pass ? "yes" : "no") << " | ";
        file << result.contract_status << " | ";
        file << result.conclusion << " | ";
        file << (!result.tool_display_path.empty() ? "yes" : "no") << " |\n";

        if (result.contract_pass) passed++;
        else failed++;
    }

    file << "\n## Summary\n\n";
    file << "- Total Findline cases: " << (passed + failed) << "\n";
    file << "- Contract pass: " << passed << "\n";
    file << "- Contract fail: " << failed << "\n";
}

void CxScriptSuiteReportWriter::WriteFindcircleAlgorithmIterationReport(
    const std::filesystem::path& outRoot,
    const std::vector<CxScriptSuiteCaseResult>& caseResults)
{
    const std::filesystem::path reportsDir = outRoot / "reports";
    std::filesystem::create_directories(reportsDir);

    const std::filesystem::path reportPath = reportsDir / "findcircle_algorithm_iteration_report.md";
    std::ofstream file(reportPath);

    if (!file.is_open())
        return;

    file << "# Findcircle Algorithm Iteration Report\n\n";
    file << "| Level | Image | Target | Script | Points | FitCircle | Radius | AvgDist | Contract | Status | Conclusion | ToolDisplay |\n";
    file << "|-------|-------|--------|--------|--------|-----------|--------|---------|----------|--------|------------|-------------|\n";

    int passed = 0, failed = 0;
    for (const auto& result : caseResults)
    {
        if (result.tool != "Findcircle")
            continue;

        file << "| " << result.level << " | ";
        file << result.image_id << " | ";
        file << result.target_id << " | ";
        file << result.script_id << " | ";
        file << result.valid_points_count << " | ";
        file << (result.has_fit_circle ? "yes" : "no") << " | ";
        file << std::fixed << std::setprecision(2) << result.circle_radius << " | ";
        file << std::fixed << std::setprecision(2) << result.avgdist << " | ";
        file << (result.contract_pass ? "yes" : "no") << " | ";
        file << result.contract_status << " | ";
        file << result.conclusion << " | ";
        file << (!result.tool_display_path.empty() ? "yes" : "no") << " |\n";

        if (result.contract_pass) passed++;
        else failed++;
    }

    file << "\n## Summary\n\n";
    file << "- Total Findcircle cases: " << (passed + failed) << "\n";
    file << "- Contract pass: " << passed << "\n";
    file << "- Contract fail: " << failed << "\n";
}

void CxScriptSuiteReportWriter::WriteFailureClassificationReport(
    const std::filesystem::path& outRoot,
    const std::vector<CxScriptSuiteCaseResult>& caseResults)
{
    const std::filesystem::path reportsDir = outRoot / "reports";
    std::filesystem::create_directories(reportsDir);

    const std::filesystem::path reportPath = reportsDir / "failure_classification_report.md";
    std::ofstream file(reportPath);

    if (!file.is_open())
        return;

    file << "# Failure Classification Report\n\n";
    file << "## Failure Classification\n\n";
    file << "| Tool | FailureClass | Count | Images | Suggested Next Action |\n";
    file << "|------|--------------|-------|--------|-----------------------|\n";

    std::map<std::string, std::vector<std::string>> failureMap;
    std::map<std::string, std::string> suggestions = {
        {"findline_fail_filter_reject", "优先调整 filter_profile / threshold / polarity，不改 fitline"},
        {"findline_fail_fit_degenerate", "检查 valid point 去重、共线判断、最小点数策略"},
        {"findline_fail_low_support", "检查 ROI、采样方向、line normal、局部 evidence"},
        {"findcircle_fail_insufficient_points", "检查 gap / linegap / threshold / circle ROI 半径"},
        {"findcircle_fail_no_fit_circle", "检查拟合方法、异常点、圆弧覆盖比例"},
        {"findcircle_fail_high_residual", "检查拟合方法、异常点、圆弧覆盖比例"}
    };

    for (const auto& result : caseResults)
    {
        if (result.contract_pass)
            continue;

        std::string key = result.tool + ":" + result.contract_status;
        failureMap[key].push_back(result.image_id);
    }

    for (const auto& [key, images] : failureMap)
    {
        size_t colon = key.find(':');
        std::string tool = key.substr(0, colon);
        std::string status = key.substr(colon + 1);

        std::string imagesList;
        for (size_t i = 0; i < images.size(); ++i)
        {
            if (i > 0) imagesList += ", ";
            imagesList += images[i];
        }

        std::string suggestion = "";
        auto it = suggestions.find(status);
        if (it != suggestions.end())
            suggestion = it->second;

        file << "| " << tool << " | ";
        file << status << " | ";
        file << images.size() << " | ";
        file << imagesList << " | ";
        file << suggestion << " |\n";
    }

    file << "\n## Detailed Failure Analysis\n\n";

    for (const auto& result : caseResults)
    {
        if (result.contract_pass)
            continue;

        file << "### " << result.case_id << "\n\n";
        file << "- **Image**: " << result.image_id << "\n";
        file << "- **Target**: " << result.target_id << "\n";
        file << "- **Tool**: " << result.tool << "\n";
        file << "- **Script**: " << result.script_id << "\n";
        file << "- **Level**: " << result.level << "\n";
        file << "- **Failure Status**: " << result.contract_status << "\n";
        file << "- **Conclusion**: " << result.conclusion << "\n";
        file << "- **Valid Points**: " << result.valid_points_count << "\n";

        if (result.tool == "FindLine")
        {
            file << "- **Has Fit Line**: " << (result.has_fit_line ? "yes" : "no") << "\n";
            file << "- **Local Support**: " << std::fixed << std::setprecision(2) << result.local_support << "\n";
            file << "- **Mean Distance**: " << std::fixed << std::setprecision(2) << result.local_mean_distance << "\n";
        }
        else if (result.tool == "Findcircle")
        {
            file << "- **Has Fit Circle**: " << (result.has_fit_circle ? "yes" : "no") << "\n";
            file << "- **Circle Radius**: " << std::fixed << std::setprecision(2) << result.circle_radius << "\n";
            file << "- **Avg Dist**: " << std::fixed << std::setprecision(2) << result.avgdist << "\n";
        }

        file << "\n";
    }
}