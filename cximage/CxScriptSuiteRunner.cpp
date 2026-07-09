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
    std::string TrimJsonScalar(std::string value)
    {
        const size_t comment = value.find(',');
        if (comment != std::string::npos)
            value = value.substr(0, comment);

        size_t start = value.find_first_not_of(" \t\r\n\"");
        size_t end = value.find_last_not_of(" \t\r\n\"");
        if (start == std::string::npos || end == std::string::npos || end < start)
            return "";

        return value.substr(start, end - start + 1);
    }

    bool JsonLineHasKey(const std::string& line, const std::string& key)
    {
        return line.find("\"" + key + "\"") != std::string::npos;
    }

    std::string JsonLineValue(const std::string& line)
    {
        const auto colon = line.find(":");
        if (colon == std::string::npos)
            return "";
        return TrimJsonScalar(line.substr(colon + 1));
    }

    bool ParseJsonBoolValue(const std::string& value)
    {
        return value == "true" || value == "1";
    }

    bool JsonLineHasAnyKey(const std::string& line, const std::vector<std::string>& keys)
    {
        for (const auto& key : keys)
        {
            if (JsonLineHasKey(line, key))
                return true;
        }
        return false;
    }

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
        std::string jsonContent;
        while (std::getline(file, line))
        {
            jsonContent += line + "\n";
        }

        size_t pointsArrayStart = jsonContent.find("\"measure_points_xy\": [");
        if (pointsArrayStart != std::string::npos)
        {
            pointsArrayStart += std::string("\"measure_points_xy\": [").size();
            size_t pointsArrayEnd = jsonContent.find("]", pointsArrayStart);
            if (pointsArrayEnd != std::string::npos)
            {
                std::string pointsArray = jsonContent.substr(pointsArrayStart, pointsArrayEnd - pointsArrayStart);
                size_t pos = 0;
                while ((pos = pointsArray.find("[", pos)) != std::string::npos)
                {
                    size_t endPos = pointsArray.find("]", pos);
                    if (endPos != std::string::npos)
                    {
                        std::string pointPair = pointsArray.substr(pos + 1, endPos - pos - 1);
                        size_t commaPos = pointPair.find(",");
                        if (commaPos != std::string::npos)
                        {
                            try
                            {
                                double x = std::stod(pointPair.substr(0, commaPos));
                                double y = std::stod(pointPair.substr(commaPos + 1));
                                out.measure_points_xy.emplace_back(x, y);
                            }
                            catch (...) {}
                        }
                    }
                    pos = endPos + 1;
                }
            }
        }

        std::istringstream contentStream(jsonContent);
        while (std::getline(contentStream, line))
        {
            if (JsonLineHasAnyKey(line, {"valid_points_count", "valid_line_points_count", "valid_circle_points_count"}))
            {
                try { out.valid_points_count = std::stoi(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasAnyKey(line, {"points_count", "measure_points_count", "line_measure_points_count", "circle_measure_points_count"}))
            {
                try { out.points_count = std::stoi(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasAnyKey(line, {"has_fit_line", "line_has_fit", "fit_line_available"}))
            {
                out.has_fit_line = ParseJsonBoolValue(JsonLineValue(line));
            }
            else if (JsonLineHasAnyKey(line, {"has_fit_circle", "circle_has_fit", "fit_circle_available"}))
            {
                out.has_fit_circle = ParseJsonBoolValue(JsonLineValue(line));
            }
            else if (JsonLineHasKey(line, "local_support"))
            {
                try { out.local_support = std::stod(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "local_mean_distance"))
            {
                try { out.local_mean_distance = std::stod(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "fit_offset"))
            {
                try { out.fit_offset = std::stod(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasAnyKey(line, {"actual_policy_guard", "line_policy_guard_status", "circle_policy_guard_status", "policy_guard_status", "policy_guard"}))
            {
                out.actual_policy_guard = JsonLineValue(line);
            }
            else if (JsonLineHasKey(line, "result_status"))
            {
                out.result_status = JsonLineValue(line);
            }
            else if (JsonLineHasKey(line, "failure_stage"))
            {
                out.failure_stage = JsonLineValue(line);
            }
            else if (JsonLineHasKey(line, "contract_pass"))
            {
                out.contract_pass = ParseJsonBoolValue(JsonLineValue(line));
            }
            else if (JsonLineHasKey(line, "contract_status"))
            {
                out.contract_status = JsonLineValue(line);
            }
            else if (JsonLineHasKey(line, "contract_conclusion"))
            {
                out.conclusion = JsonLineValue(line);
            }
            else if (JsonLineHasAnyKey(line, {"circle_radius", "fit_circle_radius"}))
            {
                try { out.circle_radius = std::stod(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasAnyKey(line, {"avgdist", "circle_avgdist", "line_avgdist"}))
            {
                try { out.avgdist = std::stod(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "roi_x0"))
            {
                try { out.roi_x0 = std::stoi(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "roi_y0"))
            {
                try { out.roi_y0 = std::stoi(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "roi_x1"))
            {
                try { out.roi_x1 = std::stoi(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "roi_y1"))
            {
                try { out.roi_y1 = std::stoi(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "circle_cx"))
            {
                try { out.circle_cx = std::stoi(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "circle_cy"))
            {
                try { out.circle_cy = std::stoi(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "circle_px"))
            {
                try { out.circle_px = std::stoi(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "circle_py"))
            {
                try { out.circle_py = std::stoi(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "fit_line_x0"))
            {
                try { out.fit_line_x0 = std::stod(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "fit_line_y0"))
            {
                try { out.fit_line_y0 = std::stod(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "fit_line_x1"))
            {
                try { out.fit_line_x1 = std::stod(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "fit_line_y1"))
            {
                try { out.fit_line_y1 = std::stod(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "circle_center_x"))
            {
                try { out.circle_center_x = std::stod(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "circle_center_y"))
            {
                try { out.circle_center_y = std::stod(JsonLineValue(line)); }
                catch (...) {}
            }
        }
    }

    void EvaluateSuiteCaseContract(CxScriptSuiteCaseResult& r)
    {
        if (!r.headless_ok)
        {
            r.contract_pass = false;
            r.conclusion = "Headless execution failed";
            return;
        }

        if (r.contract_path.empty())
        {
            if (r.expected_result == "diagnostic")
            {
                r.contract_pass = true;
                r.conclusion = "Diagnostic script executed";
            }
            else
            {
                r.contract_pass = false;
                r.contract_status = "missing_contract";
                r.conclusion = "Missing cxscript contract; C++ does not judge OK/NG";
            }
            return;
        }

        const std::filesystem::path contractDir =
            std::filesystem::path(r.case_dir) / "contract";
        std::filesystem::create_directories(contractDir);

        CxScriptHeadlessOptions contractHeadless;
        contractHeadless.enabled = true;
        contractHeadless.image_path = r.image_path;
        contractHeadless.script_path = r.contract_path;
        contractHeadless.output_dir = contractDir.string();
        contractHeadless.case_name = r.case_id + "_contract";
        contractHeadless.save_overlay = false;
        contractHeadless.summary_path =
            (contractDir / "contract_summary.json").string();
        contractHeadless.snapshot_path =
            (contractDir / "contract_snapshot.txt").string();

        contractHeadless.stage25_image_id = r.image_id;
        contractHeadless.stage25_level = r.level;
        contractHeadless.stage25_target_id = r.target_id;
        contractHeadless.stage25_tool = r.tool;

        contractHeadless.contract_context_enabled = true;
        contractHeadless.contract_headless_ok = r.headless_ok ? 1 : 0;
        contractHeadless.contract_pass_initial = 0;
        contractHeadless.points_count = r.points_count;
        contractHeadless.valid_points_count = r.valid_points_count;
        contractHeadless.has_fit_line = r.has_fit_line ? 1 : 0;
        contractHeadless.has_fit_circle = r.has_fit_circle ? 1 : 0;
        contractHeadless.local_support = r.local_support;
        contractHeadless.local_mean_distance = r.local_mean_distance;
        contractHeadless.fit_offset = r.fit_offset;
        contractHeadless.circle_radius = r.circle_radius;
        contractHeadless.avgdist = r.avgdist;
        contractHeadless.policy_guard = r.actual_policy_guard;
        contractHeadless.result_status = r.result_status;
        contractHeadless.failure_stage = r.failure_stage;
        contractHeadless.result_overlay_path = r.result_overlay_path;
        contractHeadless.evidence_overlay_path = r.evidence_overlay_path;
        contractHeadless.tool_display_path = r.tool_display_path;

        CxScriptHeadlessResult contractResult;
        RunCxScriptHeadless(contractHeadless, contractResult);

        if (!contractResult.ok)
        {
            r.contract_pass = false;
            r.conclusion = "Contract script failed: " + contractResult.reason;
            return;
        }

        if (contractResult.summary_path.empty())
        {
            r.contract_pass = false;
            r.conclusion = "Contract summary file not found";
            return;
        }

        std::ifstream contractFile(contractResult.summary_path);
        if (contractFile.is_open())
        {
            std::string line;
            while (std::getline(contractFile, line))
            {
                if (JsonLineHasKey(line, "contract_pass"))
                {
                    r.contract_pass = ParseJsonBoolValue(JsonLineValue(line));
                }
                else if (JsonLineHasKey(line, "contract_status"))
                {
                    r.contract_status = JsonLineValue(line);
                }
                else if (JsonLineHasKey(line, "contract_conclusion"))
                {
                    r.conclusion = JsonLineValue(line);
                }
            }
        }

        if (r.tool == "Findcircle")
        {
            if (r.valid_points_count < 3 || !r.has_fit_circle || r.circle_radius <= 0)
            {
                r.contract_pass = false;
                r.contract_status = "findcircle_ok_failed";
                if (r.valid_points_count < 3)
                    r.conclusion = "Findcircle OK requires valid_points_count >= 3";
                else if (!r.has_fit_circle)
                    r.conclusion = "Findcircle OK requires has_fit_circle == true";
                else if (r.circle_radius <= 0)
                    r.conclusion = "Findcircle OK requires circle_radius > 0";
                return;
            }
        }
        else if (r.tool == "Findline")
        {
            if (r.valid_points_count < 3 || !r.has_fit_line)
            {
                r.contract_pass = false;
                r.contract_status = "findline_ok_failed";
                if (r.valid_points_count < 3)
                    r.conclusion = "Findline OK requires valid_points_count >= 3";
                else if (!r.has_fit_line)
                    r.conclusion = "Findline OK requires has_fit_line == true";
                return;
            }
        }

        if (r.contract_status.empty())
            r.contract_status = r.contract_pass ? "contract_passed" : "contract_failed";
        if (r.conclusion.empty())
            r.conclusion = r.contract_pass ? "Contract script passed" : "Contract script failed";
    }

    void RunSingleSuiteCase(
        const CxScriptSuiteCase& suiteCase,
        const CxScriptCatalogEntry& script,
        const CxScriptImageManifestEntry& image,
        const CxScriptImageManifestRuntime& manifest,
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
        out.target_id = suiteCase.target_id;
        out.tool = script.tool;
        out.expected_result =
            !suiteCase.expected_result.empty()
                ? suiteCase.expected_result
                : script.expected_result;

        out.expected_policy_guard =
            !suiteCase.expected_policy_guard.empty()
                ? suiteCase.expected_policy_guard
                : script.expected_policy_guard;

        out.contract_path = script.contract_path;

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

        headless.stage25_image_id = image.image_id;
        headless.stage25_level = image.level;
        headless.stage25_target_id = suiteCase.target_id;
        headless.stage25_tool = script.tool;

        if (!suiteCase.target_id.empty())
        {
            const CxScriptImageTargetRoi* targetRoi =
                FindTargetRoiByImageAndTargetId(manifest, image.image_id, suiteCase.target_id);

            if (targetRoi)
            {
                headless.roi_x0 = targetRoi->x0;
                headless.roi_y0 = targetRoi->y0;
                headless.roi_x1 = targetRoi->x1;
                headless.roi_y1 = targetRoi->y1;
                headless.circle_cx = targetRoi->cx;
                headless.circle_cy = targetRoi->cy;
                headless.circle_px = targetRoi->px;
                headless.circle_py = targetRoi->py;
                headless.wgap = targetRoi->wgap;
                headless.hgap = targetRoi->hgap;
                headless.gap = targetRoi->gap;
                headless.linegap = targetRoi->linegap;
                headless.tool_half_width = targetRoi->tool_half_width;
                headless.threshold = targetRoi->threshold;
                headless.method = targetRoi->method;

                out.roi_x0 = targetRoi->x0;
                out.roi_y0 = targetRoi->y0;
                out.roi_x1 = targetRoi->x1;
                out.roi_y1 = targetRoi->y1;
                out.circle_cx = targetRoi->cx;
                out.circle_cy = targetRoi->cy;
                out.circle_px = targetRoi->px;
                out.circle_py = targetRoi->py;

                std::cout << "[DEBUG] Found target: " << suiteCase.target_id 
                          << " for image: " << image.image_id 
                          << " cx=" << targetRoi->cx << " cy=" << targetRoi->cy 
                          << " px=" << targetRoi->px << " py=" << targetRoi->py << "\n";
            }
            else
            {
                std::cout << "[DEBUG] Target NOT found: " << suiteCase.target_id 
                          << " for image: " << image.image_id << "\n";
                const CxScriptImageManifestEntry* img = FindImageById(manifest, image.image_id);
                if (img)
                {
                    std::cout << "[DEBUG] Image has " << img->targets.size() << " targets:\n";
                    for (const auto& t : img->targets)
                    {
                        std::cout << "[DEBUG]   target_id=" << t.target_id << "\n";
                    }
                }
            }
        }

        CxScriptHeadlessResult headlessResult;
        RunCxScriptHeadless(headless, headlessResult);

        out.headless_ok = headlessResult.ok;
        out.case_dir = caseDir.string();
        out.snapshot_path = headlessResult.snapshot_path;
        out.summary_path = headlessResult.summary_path;
        out.result_overlay_path = headlessResult.overlay_path;

        LoadSuiteCaseMetricsFromSummary(out.summary_path, out);

        if (!out.headless_ok)
        {
            out.contract_pass = false;
            out.actual_policy_guard = "HEADLESS_FAILED";
            out.failure_stage = "headless_execution_failed";
            out.conclusion = "Headless execution failed: " + headlessResult.reason;

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
            return;
        }

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

        EvaluateSuiteCaseContract(out);
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
            imageManifest,
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

    CxScriptSuiteReportWriter::WriteFindlineAlgorithmIterationReport(
        outRoot,
        result.case_results);

    CxScriptSuiteReportWriter::WriteFindcircleAlgorithmIterationReport(
        outRoot,
        result.case_results);

    CxScriptSuiteReportWriter::WriteFailureClassificationReport(
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
