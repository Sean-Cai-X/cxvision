#include "CxScriptSuiteRunner.h"
#include "CxScriptSuiteRuntime.h"
#include "CxScriptCatalogRuntime.h"
#include "CxScriptImageManifestRuntime.h"
#include "CxScriptSuiteReportWriter.h"
#include "CxScriptToolDisplayExporter.h"
#include "CxScriptBestCaseSelector.h"
#include "ManualStateTestConsole.h"
#include <filesystem>
#include <fstream>
#include <sstream>

const CxScriptCatalogEntry* FindCatalogScriptById(
    const CxScriptCatalogRuntime& catalog,
    const std::string& script_id)
{
    for (const auto& script : catalog.scripts)
    {
        if (script.script_id == script_id)
            return &script;
    }
    return nullptr;
}

namespace
{
    void LoadSuiteCaseMetricsFromSummary(
        const std::string& summary_path,
        CxScriptSuiteCaseResult& out)
    {
        if (summary_path.empty())
            return;

        std::ifstream file(summary_path);
        if (!file.is_open())
            return;

        std::string line;
        while (std::getline(file, line))
        {
            if (line.find("points_count") != std::string::npos)
            {
                const auto colon = line.find(":");
                if (colon != std::string::npos)
                {
                    try { out.points_count = std::stoi(line.substr(colon + 1)); }
                    catch (...) {}
                }
            }
            else if (line.find("valid_points_count") != std::string::npos)
            {
                const auto colon = line.find(":");
                if (colon != std::string::npos)
                {
                    try { out.valid_points_count = std::stoi(line.substr(colon + 1)); }
                    catch (...) {}
                }
            }
            else if (line.find("has_fit_line") != std::string::npos)
            {
                const auto colon = line.find(":");
                if (colon != std::string::npos)
                {
                    const std::string val = line.substr(colon + 1);
                    out.has_fit_line = (val == "true" || val == "1");
                }
            }
            else if (line.find("has_fit_circle") != std::string::npos)
            {
                const auto colon = line.find(":");
                if (colon != std::string::npos)
                {
                    const std::string val = line.substr(colon + 1);
                    out.has_fit_circle = (val == "true" || val == "1");
                }
            }
            else if (line.find("local_support") != std::string::npos)
            {
                const auto colon = line.find(":");
                if (colon != std::string::npos)
                {
                    try { out.local_support = std::stod(line.substr(colon + 1)); }
                    catch (...) {}
                }
            }
            else if (line.find("local_mean_distance") != std::string::npos)
            {
                const auto colon = line.find(":");
                if (colon != std::string::npos)
                {
                    try { out.local_mean_distance = std::stod(line.substr(colon + 1)); }
                    catch (...) {}
                }
            }
            else if (line.find("fit_offset") != std::string::npos)
            {
                const auto colon = line.find(":");
                if (colon != std::string::npos)
                {
                    try { out.fit_offset = std::stod(line.substr(colon + 1)); }
                    catch (...) {}
                }
            }
            else if (line.find("policy_guard") != std::string::npos)
            {
                const auto colon = line.find(":");
                if (colon != std::string::npos)
                {
                    out.actual_policy_guard = line.substr(colon + 1);
                    size_t start = out.actual_policy_guard.find_first_not_of(" \t\"");
                    size_t end = out.actual_policy_guard.find_last_not_of(" \t\"");
                    if (start != std::string::npos && end != std::string::npos)
                        out.actual_policy_guard = out.actual_policy_guard.substr(start, end - start + 1);
                }
            }
            else if (line.find("result_status") != std::string::npos)
            {
                const auto colon = line.find(":");
                if (colon != std::string::npos)
                {
                    out.result_status = line.substr(colon + 1);
                    size_t start = out.result_status.find_first_not_of(" \t\"");
                    size_t end = out.result_status.find_last_not_of(" \t\"");
                    if (start != std::string::npos && end != std::string::npos)
                        out.result_status = out.result_status.substr(start, end - start + 1);
                }
            }
            else if (line.find("failure_stage") != std::string::npos)
            {
                const auto colon = line.find(":");
                if (colon != std::string::npos)
                {
                    out.failure_stage = line.substr(colon + 1);
                    size_t start = out.failure_stage.find_first_not_of(" \t\"");
                    size_t end = out.failure_stage.find_last_not_of(" \t\"");
                    if (start != std::string::npos && end != std::string::npos)
                        out.failure_stage = out.failure_stage.substr(start, end - start + 1);
                }
            }
        }
    }

    void EvaluateSuiteCaseContract(CxScriptSuiteCaseResult& r)
    {
        if (r.expected_result == "ok")
        {
            const bool hasEnoughGeometry =
                (r.tool == "Findline" && r.has_fit_line && r.valid_points_count >= 2) ||
                (r.tool == "Findcircle" && r.has_fit_circle && r.valid_points_count >= 3) ||
                (r.tool == "CircleRingGauge" && r.result_status == "ring_gauge_ok");

            r.contract_pass =
                r.headless_ok &&
                hasEnoughGeometry &&
                r.actual_policy_guard == r.expected_policy_guard;

            r.conclusion = r.contract_pass
                ? "OK script passed: expected geometry result available"
                : "OK script failed: expected geometry was not available";

            return;
        }

        if (r.expected_result == "ng_expected")
        {
            r.contract_pass =
                r.headless_ok &&
                r.actual_policy_guard == r.expected_policy_guard;

            r.conclusion = r.contract_pass
                ? "NG expected script passed: expected failure baseline confirmed"
                : "NG expected script failed: failure did not match expected contract";

            return;
        }

        if (r.expected_result == "diagnostic")
        {
            r.contract_pass = r.headless_ok;
            r.conclusion = "Diagnostic script executed";
            return;
        }

        r.contract_pass = false;
        r.conclusion = "Unknown expected_result contract";
    }

    void RunSingleSuiteCase(
        const CxScriptSuiteCase& suiteCase,
        const CxScriptCatalogEntry& script,
        const CxScriptImageManifestEntry& image,
        const std::filesystem::path& outRoot,
        const CxScriptSuiteRunOptions& options,
        CxScriptSuiteCaseResult& out)
    {
        out.case_id = suiteCase.case_id;
        out.script_id = suiteCase.script_id;
        out.script_path = script.path;
        out.image_id = image.image_id;
        out.image_path = image.path;
        out.level = image.level;
        out.tool = script.tool;
        out.expected_result =
            !suiteCase.expected_result.empty()
                ? suiteCase.expected_result
                : script.expected_result;

        out.expected_policy_guard =
            !suiteCase.expected_policy_guard.empty()
                ? suiteCase.expected_policy_guard
                : script.expected_policy_guard;

        const std::filesystem::path caseDir =
            outRoot /
            "cases" /
            image.level /
            image.image_id /
            script.script_id /
            suiteCase.case_id;

        std::filesystem::create_directories(caseDir);

        CxScriptHeadlessOptions headless;
        headless.enabled = true;
        headless.image_path = image.path;
        headless.script_path = script.path;
        headless.output_dir = caseDir.string();
        headless.case_name = suiteCase.case_id;
        headless.save_overlay = options.save_overlay;

        CxScriptHeadlessResult headlessResult;
        RunCxScriptHeadless(headless, headlessResult);

        out.headless_ok = headlessResult.ok;
        out.case_dir = caseDir.string();
        out.snapshot_path = headlessResult.snapshot_path;
        out.summary_path = headlessResult.summary_path;
        out.result_overlay_path = headlessResult.overlay_path;

        LoadSuiteCaseMetricsFromSummary(out.summary_path, out);

        EvaluateSuiteCaseContract(out);

        if (options.export_tool_display)
        {
            out.tool_display_path =
                CxScriptToolDisplayExporter::ExportToolDisplay(
                    image.path,
                    out.result_overlay_path,
                    out.evidence_overlay_path,
                    caseDir / "tool_display.png",
                    out);
        }
    }

    int CountExecuted(const std::vector<CxScriptSuiteCaseResult>& cases)
    {
        int count = 0;
        for (const auto& c : cases)
            if (c.headless_ok)
                ++count;
        return count;
    }

    int CountContractPass(const std::vector<CxScriptSuiteCaseResult>& cases)
    {
        int count = 0;
        for (const auto& c : cases)
            if (c.contract_pass)
                ++count;
        return count;
    }
}

bool RunCxScriptSuite(
    const CxScriptSuiteRunOptions& options,
    CxScriptSuiteRunResult& result)
{
    result = CxScriptSuiteRunResult{};

    CxScriptSuiteRuntime suite;
    std::string reason;

    if (!LoadCxScriptSuiteFile(options.suite_path, suite, reason))
    {
        result.reason = reason;
        return false;
    }

    CxScriptCatalogRuntime catalog;
    const std::string catalogPath =
        !options.catalog_path_override.empty()
            ? options.catalog_path_override
            : suite.catalog_path;

    if (!LoadCxScriptCatalogFile(catalogPath, catalog, reason))
    {
        result.reason = reason;
        return false;
    }

    CxScriptImageManifestRuntime imageManifest;

    if (!LoadStage25ImageManifestJson(
            options.image_manifest_path,
            imageManifest,
            reason))
    {
        result.reason = reason;
        return false;
    }

    auto validation = ValidateStage25ImageManifest(imageManifest);

    const std::filesystem::path outRoot =
        !options.out_root_override.empty()
            ? std::filesystem::path(options.out_root_override)
            : std::filesystem::path(suite.output_root);

    std::filesystem::create_directories(outRoot);

    CxScriptSuiteReportWriter::WriteImageManifestContractReport(
        outRoot,
        imageManifest,
        validation);

    if (!validation.ok)
    {
        result.ok = false;
        result.reason = "image manifest validation failed";
        result.report_root = outRoot.string();
        return false;
    }

    for (const auto& suiteCase : suite.cases)
    {
        const CxScriptCatalogEntry* script =
            FindCatalogScriptById(catalog, suiteCase.script_id);

        if (!script)
        {
            CxScriptSuiteCaseResult missingCase;
            missingCase.case_id = suiteCase.case_id;
            missingCase.script_id = suiteCase.script_id;
            missingCase.headless_ok = false;
            missingCase.contract_pass = false;
            missingCase.conclusion = "Script not found in catalog: " + suiteCase.script_id;
            result.case_results.push_back(missingCase);
            continue;
        }

        const CxScriptImageManifestEntry* image =
            FindImageById(imageManifest, suiteCase.image_id);

        if (!image)
        {
            CxScriptSuiteCaseResult missingCase;
            missingCase.case_id = suiteCase.case_id;
            missingCase.script_id = suiteCase.script_id;
            missingCase.image_id = suiteCase.image_id;
            missingCase.headless_ok = false;
            missingCase.contract_pass = false;
            missingCase.conclusion = "Image not found in manifest: " + suiteCase.image_id;
            result.case_results.push_back(missingCase);
            continue;
        }

        CxScriptSuiteCaseResult caseResult;

        RunSingleSuiteCase(
            suiteCase,
            *script,
            *image,
            outRoot,
            options,
            caseResult);

        result.case_results.push_back(caseResult);
    }

    if (options.export_best_examples)
    {
        CxScriptBestCaseSelector::SelectAndExportBestExamples(
            outRoot,
            result.case_results);
    }

    CxScriptSuiteReportWriter::WriteSuiteRunReport(
        outRoot,
        result.case_results);

    CxScriptSuiteReportWriter::WriteBestDetectionGallery(
        outRoot,
        result.case_results);

    result.total_cases = static_cast<int>(result.case_results.size());
    result.executed_cases = CountExecuted(result.case_results);
    result.contract_pass = CountContractPass(result.case_results);
    result.contract_fail = result.executed_cases - result.contract_pass;
    result.report_root = outRoot.string();

    result.ok = result.contract_fail == 0;
    result.reason = result.ok ? "suite passed" : "suite has contract failures";

    return result.ok;
}