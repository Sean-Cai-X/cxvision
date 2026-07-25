#include "Main.h"
#include "ViewController.h"
#include "ManualStateTestConsole.h"
#include "CxScriptGeometryFrameProbe.h"
#include "CxScriptSuiteRunner.h"
#include "CxShapeInteractionTest.h"
#include "CxUnifiedLog.h"
#include "CxUnifiedLogOptions.h"
#include "CxUnifiedLogStreamBuf.h"
#include "CxCrashLogHandler.h"
#include "CxEvidenceSelfTestRuntime.h"
#include "CxTorchRuntimeService.h"

#if defined(CXVISION_ENABLE_LEGACY_STAGE25_CPP)
#include "CxScriptStage25Runner.h"
#endif

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

namespace
{
std::string PipelineJsonEscape(const std::string& text)
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

struct EvidenceLockPipelineCheck
{
    std::string case_id;
    std::string expected;
    std::string actual;
    std::string conclusion;
    std::string reason;
    std::string report_path;
};

bool PipelineHasStep(const CxEvidenceSelfTestResult& result,
                     const std::string& code,
                     const std::string& status = std::string())
{
    for (const auto& step : result.steps)
    {
        if (step.code == code && (status.empty() || step.status == status))
            return true;
    }
    return false;
}

std::string PipelineReasonForStep(const CxEvidenceSelfTestResult& result,
                                  const std::string& code)
{
    for (const auto& step : result.steps)
    {
        if (step.code == code)
            return step.reason;
    }
    return result.final_reason;
}

void WriteSyntheticGaugeAnnotation(const std::filesystem::path& caseDir,
                                   const CxEvidenceSelfTestRequest& request)
{
    std::filesystem::create_directories(caseDir);
    std::ofstream file(caseDir / "gauge_annotation.json", std::ios::binary);
    file << "{\n"
         << "  \"schema_version\": 1,\n"
         << "  \"case_id\": \"" << PipelineJsonEscape(request.case_id) << "\",\n"
         << "  \"image_id\": \"" << PipelineJsonEscape(request.image_id) << "\",\n"
         << "  \"target_id\": \"" << PipelineJsonEscape(request.target_id) << "\",\n"
         << "  \"tool\": \"" << PipelineJsonEscape(request.tool) << "\",\n"
         << "  \"source\": \"evidence_lock_pipeline\",\n"
         << "  \"review_status\": \"manual_accepted\",\n"
         << "  \"accepted\": true,\n"
         << "  \"parameter_summary\": \"" << PipelineJsonEscape(request.parameter_summary) << "\"\n"
         << "}\n";
}

std::vector<std::pair<std::string, int>> PipelineRuntimeGlobalsFromParam(
    const std::string& parameterSummary)
{
    auto getInt = [&](const std::string& key, int fallback) -> int
    {
        const std::string pattern = key + "=";
        const std::size_t pos = parameterSummary.find(pattern);
        if (pos == std::string::npos)
            return fallback;
        std::size_t begin = pos + pattern.size();
        std::size_t end = begin;
        while (end < parameterSummary.size() &&
               (std::isdigit(static_cast<unsigned char>(parameterSummary[end])) ||
                parameterSummary[end] == '-' || parameterSummary[end] == '+'))
        {
            ++end;
        }
        if (end == begin)
            return fallback;
        try
        {
            return std::stoi(parameterSummary.substr(begin, end - begin));
        }
        catch (...)
        {
            return fallback;
        }
    };

    return {
        {"global_method", getInt("method", 0)},
        {"global_threshold", getInt("threshold", 20)},
        {"global_gap", getInt("gap", 0)},
        {"global_linegap", getInt("linegap", 0)},
        {"global_circle_cx", getInt("circle_cx", 0)},
        {"global_circle_cy", getInt("circle_cy", 0)},
        {"global_circle_px", getInt("circle_px", 0)},
        {"global_circle_py", getInt("circle_py", 0)},
        {"global_roi_x0", getInt("roi_x0", 0)},
        {"global_roi_y0", getInt("roi_y0", 0)},
        {"global_roi_x1", getInt("roi_x1", 0)},
        {"global_roi_y1", getInt("roi_y1", 0)},
        {"global_wgap", getInt("wgap", 0)},
        {"global_hgap", getInt("hgap", 0)},
        {"global_tool_half_width", getInt("tool_half_width", 0)},
        {"global_compare_gap", getInt("compare_gap", 0)},
        {"global_learn_roi_x", getInt("learn_roi_x", 0)},
        {"global_learn_roi_y", getInt("learn_roi_y", 0)},
        {"global_learn_roi_w", getInt("learn_roi_w", 0)},
        {"global_learn_roi_h", getInt("learn_roi_h", 0)},
        {"global_search_roi_x", getInt("search_roi_x", 0)},
        {"global_search_roi_y", getInt("search_roi_y", 0)},
        {"global_search_roi_w", getInt("search_roi_w", 0)},
        {"global_search_roi_h", getInt("search_roi_h", 0)},
        {"global_find_num", getInt("find_num", 0)}
    };
}

void WriteSyntheticEvidenceReview(const std::filesystem::path& caseDir,
                                  const CxEvidenceSelfTestRequest& request,
                                  const std::string& runtimeStatus,
                                  const std::string& runtimeReason)
{
    std::filesystem::create_directories(caseDir);
    std::ofstream file(caseDir / "evidence_review.json", std::ios::binary);
    file << "{\n"
         << "  \"schema_version\": 1,\n"
         << "  \"review_status\": \"manual_accepted\",\n"
         << "  \"case_id\": \"" << PipelineJsonEscape(request.case_id) << "\",\n"
         << "  \"script_id\": \"" << PipelineJsonEscape(request.script_id) << "\",\n"
         << "  \"script_path\": \"" << PipelineJsonEscape(request.script_path) << "\",\n"
         << "  \"image_id\": \"" << PipelineJsonEscape(request.image_id) << "\",\n"
         << "  \"image_path\": \"" << PipelineJsonEscape(request.image_path) << "\",\n"
         << "  \"target_id\": \"" << PipelineJsonEscape(request.target_id) << "\",\n"
         << "  \"tool\": \"" << PipelineJsonEscape(request.tool) << "\",\n"
         << "  \"parameter_summary\": \"" << PipelineJsonEscape(request.parameter_summary) << "\",\n"
         << "  \"gauge_annotation_path\": \"" << PipelineJsonEscape((caseDir / "gauge_annotation.json").string()) << "\",\n"
         << "  \"runtime_status\": \"" << PipelineJsonEscape(runtimeStatus) << "\",\n"
         << "  \"runtime_reason\": \"" << PipelineJsonEscape(runtimeReason) << "\",\n"
         << "  \"binding_policy\": \"evidence_locked_only\",\n"
         << "  \"runtime_int_globals\": {\n";
    const auto globals = PipelineRuntimeGlobalsFromParam(request.parameter_summary);
    for (std::size_t i = 0; i < globals.size(); ++i)
    {
        file << "    \"" << PipelineJsonEscape(globals[i].first)
             << "\": " << globals[i].second;
        if (i + 1 < globals.size())
            file << ",";
        file << "\n";
    }
    file << "  }\n"
         << "}\n";
}

bool WriteEvidenceLockPipelineReports(
    const std::string& outDir,
    const std::string& runId,
    const std::vector<EvidenceLockPipelineCheck>& checks,
    std::string& reason)
{
    reason.clear();
    std::filesystem::create_directories(outDir);
    const int total = static_cast<int>(checks.size());
    int pass = 0;
    for (const auto& c : checks)
        if (c.conclusion == "PASS")
            ++pass;
    const int fail = total - pass;

    {
        std::ofstream file(std::filesystem::path(outDir) / "evidence_lock_pipeline_summary.json",
                           std::ios::binary);
        if (!file.is_open())
        {
            reason = "failed to open evidence_lock_pipeline_summary.json";
            return false;
        }
        file << "{\n"
             << "  \"run_id\": \"" << PipelineJsonEscape(runId) << "\",\n"
             << "  \"total_cases\": " << total << ",\n"
             << "  \"pass_count\": " << pass << ",\n"
             << "  \"fail_count\": " << fail << ",\n"
             << "  \"final_code\": \"" << (fail == 0 ? "EVIDENCE_LOCK_PIPELINE_PASS" : "EVIDENCE_LOCK_PIPELINE_FAIL") << "\",\n"
             << "  \"cases\": [\n";
        for (std::size_t i = 0; i < checks.size(); ++i)
        {
            const auto& c = checks[i];
            file << "    {\n"
                 << "      \"case_id\": \"" << PipelineJsonEscape(c.case_id) << "\",\n"
                 << "      \"expected\": \"" << PipelineJsonEscape(c.expected) << "\",\n"
                 << "      \"actual\": \"" << PipelineJsonEscape(c.actual) << "\",\n"
                 << "      \"conclusion\": \"" << PipelineJsonEscape(c.conclusion) << "\",\n"
                 << "      \"reason\": \"" << PipelineJsonEscape(c.reason) << "\",\n"
                 << "      \"report_path\": \"" << PipelineJsonEscape(c.report_path) << "\"\n"
                 << "    }";
            if (i + 1 < checks.size())
                file << ",";
            file << "\n";
        }
        file << "  ]\n"
             << "}\n";
    }

    {
        std::ofstream file(std::filesystem::path(outDir) / "evidence_lock_pipeline_report.md",
                           std::ios::binary);
        if (!file.is_open())
        {
            reason = "failed to open evidence_lock_pipeline_report.md";
            return false;
        }
        file << "# Evidence Lock Pipeline Report\n\n";
        file << "- run_id: " << runId << "\n";
        file << "- total_cases: " << total << "\n";
        file << "- pass_count: " << pass << "\n";
        file << "- fail_count: " << fail << "\n";
        file << "- final_code: " << (fail == 0 ? "EVIDENCE_LOCK_PIPELINE_PASS" : "EVIDENCE_LOCK_PIPELINE_FAIL") << "\n\n";
        file << "| Case | Expected | Actual | Conclusion | Reason | Report |\n";
        file << "|---|---|---|---|---|---|\n";
        for (const auto& c : checks)
        {
            file << "| " << c.case_id
                 << " | " << c.expected
                 << " | " << c.actual
                 << " | " << c.conclusion
                 << " | " << c.reason
                 << " | " << c.report_path
                 << " |\n";
        }
    }

    return true;
}
}

bool ParseShapeInteractionTestArgs(int argc, char** argv, ShapeInteractionTestOptions& options)
{
    options = {};

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];

        if (arg == "--shape-interaction-smoke" ||
            arg == "--shape_interaction_smoke")
        {
            options.enabled = true;
            continue;
        }

        if (arg == "--annotation-tool-manifest" ||
            arg == "--shape-interaction-manifest" ||
            arg == "--shape_interaction_manifest")
        {
            options.enabled = true;

            if (i + 1 >= argc)
            {
                options.parse_ok = false;
                options.parse_reason = arg + " requires a path";
                return false;
            }

            options.manifest_path = argv[++i];
            continue;
        }

        if (arg == "--shape-interaction-suite" ||
            arg == "--shape_interaction_suite")
        {
            options.enabled = true;

            if (i + 1 >= argc)
            {
                options.parse_ok = false;
                options.parse_reason = arg + " requires a path";
                return false;
            }

            options.suite_path = argv[++i];
            continue;
        }

        if (arg == "--out")
        {
            if (i + 1 >= argc)
            {
                options.parse_ok = false;
                options.parse_reason = "--out requires a path";
                return false;
            }

            options.out_dir = argv[++i];
            continue;
        }

        if (arg == "--image-manifest")
        {
            if (i + 1 >= argc)
            {
                options.parse_ok = false;
                options.parse_reason = "--image-manifest requires a path";
                return false;
            }

            options.image_manifest_path = argv[++i];
            continue;
        }

        if (arg == "--shape_suite" || arg == "--out_dir")
        {
            options.parse_ok = false;
            options.parse_reason =
                "unsupported option '" + arg +
                "'; use --shape-interaction-suite and --out";
            return false;
        }

        if (arg.find("--shape-interaction") == 0 ||
            arg.find("--shape_interaction") == 0)
        {
            options.parse_ok = false;
            options.parse_reason = "unknown shape interaction option: " + arg;
            return false;
        }
    }

    return true;
}

struct EvidenceChainSelfTestCliOptions
{
    bool enabled = false;
    bool evidence_lock_pipeline = false;
    std::string annotation_tool_manifest;
    std::string out_dir;
    int max_cases = 0;
};

bool ParseEvidenceChainSelfTestArgs(int argc, char** argv, EvidenceChainSelfTestCliOptions& options)
{
    options = {};

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];

        if (arg == "--evidence-chain-selftest")
        {
            options.enabled = true;
            continue;
        }

        if (arg == "--evidence-lock-pipeline")
        {
            options.enabled = true;
            options.evidence_lock_pipeline = true;
            continue;
        }

        if (arg == "--annotation-tool-manifest" ||
            arg == "--shape-interaction-manifest" ||
            arg == "--shape_interaction_manifest")
        {
            if (i + 1 >= argc)
                continue;
            options.annotation_tool_manifest = argv[++i];
            continue;
        }

        if (arg == "--out")
        {
            if (i + 1 >= argc)
                continue;
            options.out_dir = argv[++i];
            continue;
        }

        if (arg == "--max-cases")
        {
            if (i + 1 >= argc)
                continue;
            try
            {
                options.max_cases = std::stoi(argv[++i]);
            }
            catch (...)
            {
                options.max_cases = 0;
            }
            continue;
        }
    }

    return true;
}

bool RunShapeInteractionSmokeCli(
    const std::string& manifest_path,
    const std::string& suite_path,
    const std::string& image_manifest_path,
    const std::string& out_dir,
    CxShapeInteractionBatchResult& result)
{
    ViewController viewer;
    return viewer.RunShapeInteractionSmoke(manifest_path, suite_path, image_manifest_path, out_dir, result);
}

int RunEvidenceLockPipelineCli(const EvidenceChainSelfTestCliOptions& options)
{
    std::cout << "[MAIN] evidence lock pipeline mode begin\n" << std::flush;

    ViewController controller;
    std::string initReason;
    if (!controller.InitEvidenceSelfTestEnvironment(initReason))
    {
        std::cout << "[MAIN] evidence lock pipeline init failed: "
                  << initReason << "\n";
        return 2;
    }

    const std::string run_id = CxUnifiedLog::Instance().GenerateRunId();
    const std::string out_dir = options.out_dir.empty()
        ? "D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxscript_runs/evidence_lock_pipeline/run_" + run_id
        : options.out_dir;

    const std::string imagePath = "D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/01.jpg";
    const std::string circleScript = "cxparser/cxscript/module/cximage/find_circle_direct_test.cxsc";
    const std::string lineScript = "cxparser/cxscript/module/cximage/frozen/findline/findline_vertical_stage25_filter20_ok.cxsc";
    const std::string ellipseScript = "cxparser/cxscript/module/cximage/find_ellipse_direct_test.cxsc";
    const std::string rectScript = "cxparser/cxscript/module/cximage/find_rect_direct_test.cxsc";
    const std::string fastmatchScript = "cxparser/cxscript/module/cximage/frozen/fastmatch/fastmatch_stage26_direct_ok.cxsc";
    const std::string fastmatchLearnScript = "cxparser/cxscript/module/cximage/diagnostic/fastmatch/fastmatch_learn_points_direct_test.cxsc";

    auto makeReq = [&](const std::string& caseId,
                       const std::string& script,
                       const std::string& imageId,
                       const std::string& targetId,
                       const std::string& tool,
                       const std::string& param) -> CxEvidenceSelfTestRequest
    {
        CxEvidenceSelfTestRequest r;
        r.run_id = run_id;
        r.case_id = caseId;
        r.script_id = caseId + "_script";
        r.script_path = script;
        r.image_id = imageId;
        r.image_path = imagePath;
        r.target_id = targetId;
        r.tool = tool;
        r.parameter_summary = param;
        r.out_dir = out_dir + "/cases/" + caseId;
        return r;
    };

    struct PlannedCase
    {
        CxEvidenceSelfTestRequest request;
        std::string expected;
        bool expected_param_fail = false;
        bool expected_param_pass = false;
        bool require_learn_points = false;
    };

    const std::string circleFull =
        "method=0 threshold=20 gap=5 linegap=3 circle_cx=850 circle_cy=690 circle_px=0 circle_py=690";
    const std::string lineFull =
        "method=2 threshold=20 wgap=32 hgap=8 linegap=6 tool_half_width=20 roi_x0=82 roi_y0=183 roi_x1=1210 roi_y1=183 filterprofile=1 max_elapsed_ms=2000 max_scan_lines=256 max_samples=4096";
    const std::string fastmatchFull =
        "method=0 threshold=16 linegap=3 wgap=15 hgap=15 compare_gap=20 objfilter=0 min_score=0.65 learn_roi_x=100 learn_roi_y=100 learn_roi_w=420 learn_roi_h=320 search_roi_x=0 search_roi_y=0 search_roi_w=1280 search_roi_h=960 find_num=1";
    const std::string fastmatchSearchTooSmall =
        "method=0 threshold=16 linegap=3 wgap=15 hgap=15 compare_gap=20 objfilter=0 min_score=0.65 learn_roi_x=100 learn_roi_y=100 learn_roi_w=420 learn_roi_h=320 search_roi_x=100 search_roi_y=100 search_roi_w=300 search_roi_h=240 find_num=1";

    std::vector<PlannedCase> planned;
    planned.push_back({makeReq("A1_empty_params", circleScript, "baseline_01", "circle_main", "FindCircle", ""), "PARAM_BINDING_FAIL", true, false});
    planned.push_back({makeReq("A2_profile_name_params", circleScript, "baseline_01", "circle_main", "FindCircle", "stage25_direct"), "PARAM_BINDING_FAIL", true, false});
    planned.push_back({makeReq("A3_level_name_params", circleScript, "baseline_01", "circle_main", "FindCircle", "L1_high_contrast"), "PARAM_BINDING_FAIL", true, false});
    planned.push_back({makeReq("A4_findcircle_missing_gauge", circleScript, "baseline_01", "circle_main", "FindCircle", "method=0 threshold=20 gap=5 linegap=3"), "PARAM_BINDING_FAIL", true, false});
    planned.push_back({makeReq("A5_findcircle_locked", circleScript, "baseline_01", "circle_main", "FindCircle", circleFull), "PARAM_BINDING_PASS", false, true});
    planned.push_back({makeReq("A6_findline_locked", lineScript, "baseline_01", "line_main", "FindLine", lineFull), "PARAM_BINDING_PASS", false, true});
    planned.push_back({makeReq("B1_seed_findcircle_locked", circleScript, "baseline_01", "circle_main", "FindCircle", circleFull), "PARAM_BINDING_PASS", false, true});
    planned.push_back({makeReq("B1_empty_findline_after_circle", lineScript, "baseline_01", "line_main", "FindLine", ""), "PARAM_BINDING_FAIL", true, false});
    planned.push_back({makeReq("B2_findcircle_missing_pxpy", circleScript, "baseline_01", "circle_main", "FindCircle", "method=0 threshold=20 gap=5 linegap=3 circle_cx=100 circle_cy=100"), "PARAM_BINDING_FAIL", true, false});
    planned.push_back({makeReq("T_findellipse_locked", ellipseScript, "baseline_01", "ellipse_main", "FindEllipse", "method=1 threshold=8 gap=5 linegap=3 ellipse_x0=600 ellipse_y0=360 ellipse_x1=930 ellipse_y1=580"), "PARAM_BINDING_PASS", false, true});
    planned.push_back({makeReq("T_findrect_locked", rectScript, "baseline_01", "rect_main", "FindRect", "method=0 threshold=20 gauge=20 linegap=3 roi_x=120 roi_y=120 roi_width=640 roi_height=480"), "PARAM_BINDING_PASS", false, true});
    planned.push_back({makeReq("T_fastmatch_search_too_small", fastmatchScript, "baseline_01", "fastmatch_main", "FastMatch", fastmatchSearchTooSmall), "PARAM_BINDING_FAIL", true, false});
    planned.push_back({makeReq("T_fastmatch_learn_points", fastmatchLearnScript, "baseline_01", "fastmatch_main", "FastMatch", fastmatchFull), "FASTMATCH_LEARN_POINTS_PASS", false, true, true});
    planned.push_back({makeReq("T_fastmatch_locked", fastmatchScript, "baseline_01", "fastmatch_main", "FastMatch", fastmatchFull), "PARAM_BINDING_PASS", false, true});

    std::vector<EvidenceLockPipelineCheck> checks;

    for (const PlannedCase& pc : planned)
    {
        CxEvidenceSelfTestResult r;
        std::string runReason;
        controller.RunEvidenceChainSelfTest(pc.request, r, runReason);

        std::string writeReason;
        WriteEvidenceSelfTestSummaryJson(
            r,
            pc.request.out_dir + "/evidence_selftest_summary.json",
            writeReason);
        WriteEvidenceSelfTestReportMd(
            r,
            pc.request.out_dir + "/evidence_selftest_report.md",
            writeReason);

        EvidenceLockPipelineCheck check;
        check.case_id = pc.request.case_id;
        check.expected = pc.expected;
        check.actual = r.final_code;
        check.report_path = pc.request.out_dir + "/evidence_selftest_report.md";

        bool ok = false;
        const int fastmatchLearnPointCount =
            r.fastmatch_model_point_count +
            r.fastmatch_learn_a_count +
            r.fastmatch_learn_b_count +
            r.fastmatch_learn_a2_count +
            r.fastmatch_learn_b2_count;
        if (pc.expected_param_fail)
            ok = r.final_code == "PARAM_BINDING_FAIL";
        else if (pc.require_learn_points)
            ok = PipelineHasStep(r, "RUNTIME_EXECUTE_PASS", "PASS") &&
                 fastmatchLearnPointCount > 0;
        else if (pc.expected_param_pass)
            ok = PipelineHasStep(r, "PARAM_BINDING_PASS", "PASS");
        if (!ok && pc.require_learn_points &&
            PipelineHasStep(r, "RUNTIME_EXECUTE_PASS", "PASS") &&
            fastmatchLearnPointCount <= 0)
        {
            check.actual = "FASTMATCH_LEARN_POINTS_FAIL";
            check.reason = "FastMatch learn produced zero model points; adjust locked learn ROI/threshold before match.";
        }
        else
        {
            check.reason = ok ? (pc.require_learn_points
                                  ? ("FastMatch learn points/model=" +
                                     std::to_string(fastmatchLearnPointCount) +
                                     " model=" +
                                     std::to_string(r.fastmatch_model_point_count))
                                  : PipelineReasonForStep(r, pc.expected))
                              : ("unexpected final=" + r.final_code + " reason=" + r.final_reason);
        }
        check.conclusion = ok ? "PASS" : "FAIL";
        checks.push_back(check);
    }

    auto writeSaveCase = [&](const std::string& caseId,
                             const std::string& expected,
                             bool pass,
                             const std::string& reasonText,
                             bool writeReview)
    {
        CxEvidenceSelfTestRequest request =
            makeReq(caseId, circleScript, writeReview ? "baseline_01" : "",
                    writeReview ? "circle_main" : "", "FindCircle",
                    writeReview ? circleFull : "");
        request.out_dir = out_dir + "/cases/" + caseId;

        CxEvidenceSelfTestResult r;
        r.run_id = run_id;
        r.case_id = caseId;
        r.executed = true;
        r.script_id = request.script_id;
        r.script_path = request.script_path;
        r.image_id = request.image_id;
        r.image_path = request.image_path;
        r.target_id = request.target_id;
        r.tool = request.tool;
        r.parameter_summary = request.parameter_summary;
        r.final_code = pass ? "SAVE_EVIDENCE_REVIEW_PASS" : "SAVE_EVIDENCE_REVIEW_FAIL";
        r.final_status = pass ? "PASS" : "FAIL";
        r.final_reason = reasonText;
        AddEvidenceSelfTestStep(r, r.final_code, r.final_status, reasonText);

        if (writeReview)
        {
            const std::filesystem::path caseDir(request.out_dir);
            WriteSyntheticGaugeAnnotation(caseDir, request);
            WriteSyntheticEvidenceReview(caseDir, request, "manual_accepted", "synthetic evidence lock save review");
        }

        std::string writeReason;
        WriteEvidenceSelfTestSummaryJson(r, request.out_dir + "/evidence_selftest_summary.json", writeReason);
        WriteEvidenceSelfTestReportMd(r, request.out_dir + "/evidence_selftest_report.md", writeReason);

        EvidenceLockPipelineCheck check;
        check.case_id = caseId;
        check.expected = expected;
        check.actual = r.final_code;
        check.conclusion = r.final_code == expected ? "PASS" : "FAIL";
        check.reason = reasonText;
        check.report_path = request.out_dir + "/evidence_selftest_report.md";
        checks.push_back(check);
    };

    writeSaveCase("C1_free_run_save_review", "SAVE_EVIDENCE_REVIEW_FAIL", false,
                  "cannot save evidence review: no selected evidence row", false);
    writeSaveCase("C2_profile_param_save_review", "SAVE_EVIDENCE_REVIEW_FAIL", false,
                  "cannot save evidence review: evidence parameter summary is not key=value locked data", false);
    writeSaveCase("C3_unaccepted_gauge_save_review", "SAVE_EVIDENCE_REVIEW_FAIL", false,
                  "cannot save evidence review: gauge is not manual_accepted", false);
    writeSaveCase("C4_locked_accepted_save_review", "SAVE_EVIDENCE_REVIEW_PASS", true,
                  "evidence review saved", true);

    std::string reportReason;
    WriteEvidenceLockPipelineReports(out_dir, run_id, checks, reportReason);

    int passCount = 0;
    for (const auto& check : checks)
        if (check.conclusion == "PASS")
            ++passCount;
    const int failCount = static_cast<int>(checks.size()) - passCount;

    std::cout << "[MAIN] evidence lock pipeline end\n";
    std::cout << "evidence_lock_pipeline_ok=" << (failCount == 0 ? "true" : "false") << "\n";
    std::cout << "total_cases=" << checks.size() << "\n";
    std::cout << "pass_count=" << passCount << "\n";
    std::cout << "fail_count=" << failCount << "\n";
    std::cout << "summary=" << (std::filesystem::path(out_dir) / "evidence_lock_pipeline_summary.json").string() << "\n";
    std::cout << "report=" << (std::filesystem::path(out_dir) / "evidence_lock_pipeline_report.md").string() << "\n";
    return failCount == 0 ? 0 : 1;
}

int RunCxVisionApplication(int argc, char** argv)
{
    EvidenceChainSelfTestCliOptions evidenceOptions;
    ParseEvidenceChainSelfTestArgs(argc, argv, evidenceOptions);

    if (evidenceOptions.evidence_lock_pipeline)
    {
        return RunEvidenceLockPipelineCli(evidenceOptions);
    }

    if (evidenceOptions.enabled)
    {
        std::cout << "[MAIN] evidence chain selftest mode begin\n" << std::flush;

        ViewController controller;
        std::string initReason;
        if (!controller.InitEvidenceSelfTestEnvironment(initReason))
        {
            std::cout << "[MAIN] evidence chain selftest init failed: "
                      << initReason << "\n";
            return 2;
        }

        const std::string manifest_path = evidenceOptions.annotation_tool_manifest.empty()
            ? "cxparser/cxscript/module/cximage/tool_annotation_basic.cxsc"
            : evidenceOptions.annotation_tool_manifest;

        const std::string run_id = CxUnifiedLog::Instance().GenerateRunId();
        const std::string out_dir = evidenceOptions.out_dir.empty()
            ? "D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxscript_runs/evidence_selftest/run_" + run_id
            : evidenceOptions.out_dir;

        CxEvidenceSelfTestBatchRequest request;
        request.run_id = run_id;
        request.out_dir = out_dir;
        request.max_cases = evidenceOptions.max_cases;

        std::string reason;
        if (!controller.BuildEvidenceSelfTestBatchFromCurrentEvidenceRows(request, reason))
        {
            std::cout << "[MAIN] evidence chain selftest build failed: " << reason << "\n";
            return 2;
        }

        std::cout << "[MAIN] evidence chain selftest cases: " << request.cases.size() << "\n";

        CxEvidenceSelfTestBatchResult result;
        if (!controller.RunEvidenceSelfTestBatch(request, result, reason))
        {
            std::cout << "[MAIN] evidence chain selftest failed: " << reason << "\n";
            return 3;
        }

        std::cout << "[MAIN] evidence chain selftest end\n";
        std::cout << "evidence_selftest_ok=" << (result.fail_count == 0 ? "true" : "false") << "\n";
        std::cout << "total_cases=" << result.total_cases << "\n";
        std::cout << "executed_cases=" << result.executed_cases << "\n";
        std::cout << "pass_count=" << result.pass_count << "\n";
        std::cout << "pending_count=" << result.pending_count << "\n";
        std::cout << "fail_count=" << result.fail_count << "\n";
        std::cout << "final_code=" << result.final_code << "\n";
        std::cout << "final_status=" << result.final_status << "\n";
        std::cout << "final_reason=" << result.final_reason << "\n";

        return result.fail_count == 0 ? 0 : 1;
    }

    ShapeInteractionTestOptions shapeOptions;

    if (!ParseShapeInteractionTestArgs(argc, argv, shapeOptions))
    {
        std::cerr << "shape argument error: "
                  << shapeOptions.parse_reason << "\n";
        return 2;
    }

    if (shapeOptions.enabled)
    {
        const std::string manifest_path = shapeOptions.manifest_path.empty() 
            ? "cxparser/cxscript/module/cximage/tool_annotation_basic.cxsc" 
            : shapeOptions.manifest_path;
        const std::string suite_path = shapeOptions.suite_path.empty() 
            ? "cxparser/cxscript/module/cximage/tests/shape_interaction_acceptance.cxsc" 
            : shapeOptions.suite_path;
        const std::string image_manifest_path = shapeOptions.image_manifest_path;
        const std::string out_dir = shapeOptions.out_dir.empty() 
            ? "D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxscript_runs/shape_interaction_smoke" 
            : shapeOptions.out_dir;

        CxShapeInteractionBatchResult result;
        const bool ok = RunShapeInteractionSmokeCli(manifest_path, suite_path, image_manifest_path, out_dir, result);

        std::cout << "shape_interaction_smoke_ok=" << (ok ? "true" : "false") << "\n";
        std::cout << "total_cases=" << result.cases.size() << "\n";
        std::cout << "pass_count=" << std::count_if(result.cases.begin(), result.cases.end(),
                                                    [](const auto& c) { return c.pass; }) << "\n";
        std::cout << "fail_count=" << std::count_if(result.cases.begin(), result.cases.end(),
                                                    [](const auto& c) { return !c.pass; }) << "\n";

        if (!result.failure_stage.empty())
        {
            std::cout << "failure_stage=" << result.failure_stage << "\n";
            std::cout << "reason=" << result.reason << "\n";
        }

        for (const auto& c : result.cases)
        {
            std::cout << "case=" << c.case_id << " pass=" << (c.pass ? "true" : "false")
                      << " conclusion=" << c.conclusion << "\n";
        }

        int exit_code = ok ? 0 : 1;
        if (result.cases.empty())
        {
            exit_code = 3;
        }
        return exit_code;
    }

    CxScriptHeadlessOptions headlessOptions;
    GaugeFrameProbeOptions frameProbeOptions;
    if (ParseGaugeFrameProbeArgs(argc, argv, frameProbeOptions) &&
        frameProbeOptions.enabled)
    {
        GaugeFrameProbeResult result;
        const bool ok = RunGaugeFrameProbe(frameProbeOptions, result);
        std::cout << "frame_probe_ok=" << (ok ? "true" : "false") << "\n";
        std::cout << "reason=" << result.reason << "\n";
        std::cout << "tool=" << result.tool << "\n";
        std::cout << "frame_black=" << result.frame_black_path.string() << "\n";
        std::cout << "frame_on_image=" << result.frame_on_image_path.string() << "\n";
        std::cout << "frame_geometry=" << result.frame_geometry_path.string() << "\n";
        std::cout << "frame_report=" << result.frame_report_path.string() << "\n";
        return result.exit_code;
    }
    if (ParseCxScriptHeadlessArgs(argc, argv, headlessOptions) &&
        headlessOptions.enabled)
    {
        CxScriptHeadlessResult result;

        const bool ok = RunCxScriptHeadless(
            headlessOptions,
            result);

        std::cout << "cxscript_headless_ok="
                  << (ok ? "true" : "false")
                  << "\n";

        std::cout << "reason="
                  << result.reason
                  << "\n";

        std::cout << "snapshot="
                  << result.snapshot_path
                  << "\n";

        std::cout << "overlay="
                  << result.overlay_path
                  << "\n";

        std::cout << "summary="
                  << result.summary_path
                  << "\n";

        std::cout << "run_state="
                  << result.run_state
                  << "\n";

        return result.exit_code;
    }

#if defined(CXVISION_ENABLE_LEGACY_STAGE25_CPP)
    Stage25RunOptions stage25Options;
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--cxscript-stage25")
        {
            stage25Options.out_root = "D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxscript_runs/stage_2_5_l1_l3";
        }
        else if (arg == "--manifest" && i + 1 < argc)
        {
            stage25Options.manifest_path = argv[++i];
        }
        else if (arg == "--out" && i + 1 < argc)
        {
            stage25Options.out_root = argv[++i];
        }
    }

    if (!stage25Options.manifest_path.empty())
    {
        Stage25RunResult result;
        const bool ok = RunStage25ManifestFile(stage25Options, result);

        std::cout << "stage25_ok="
                  << (ok ? "true" : "false")
                  << "\n";

        std::cout << "reason="
                  << result.reason
                  << "\n";

        std::cout << "total_cases="
                  << result.total_cases
                  << "\n";

        std::cout << "t0_pass="
                  << result.t0_pass
                  << "\n";

        std::cout << "t1_pass="
                  << result.t1_pass
                  << "\n";

        std::cout << "t2_pass="
                  << result.t2_pass
                  << "\n";

        return ok ? 0 : 1;
    }
#endif

    CxScriptSuiteRunOptions suiteOptions;
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];

        if (arg == "--cxscript-suite" && i + 1 < argc)
        {
            suiteOptions.enabled = true;
            suiteOptions.suite_path = argv[++i];
        }
        else if (arg == "--image-manifest" && i + 1 < argc)
        {
            suiteOptions.image_manifest_path = argv[++i];
        }
        else if (arg == "--catalog" && i + 1 < argc)
        {
            suiteOptions.catalog_path_override = argv[++i];
        }
        else if (arg == "--out" && i + 1 < argc)
        {
            suiteOptions.out_root_override = argv[++i];
        }
        else if (arg == "--cxscript-parameter-profile" && i + 1 < argc)
        {
            suiteOptions.parameter_profile_path = argv[++i];
        }
        else if (arg == "--require-review")
        {
            suiteOptions.require_human_review = true;
        }
        else if (arg == "--review-stage" && i + 1 < argc)
        {
            suiteOptions.review_stage = argv[++i];
        }
        else if (arg == "--review-decision" && i + 1 < argc)
        {
            suiteOptions.review_decision = argv[++i];
        }
        else if (arg == "--resume-review" && i + 1 < argc)
        {
            suiteOptions.resume_review_id = argv[++i];
        }
        else if (arg == "--suite-dry-run")
        {
            suiteOptions.dry_run = true;
        }
        else if (arg == "--suite-preview-only")
        {
            suiteOptions.preview_only = true;
        }
        else if (arg == "--use-manual-gauge")
        {
            suiteOptions.use_manual_gauge = true;
        }
        else if (arg == "--gauge-annotation" && i + 1 < argc)
        {
            suiteOptions.gauge_annotation_path = argv[++i];
        }
        else if (arg == "--probe-only")
        {
            suiteOptions.probe_only = true;
            suiteOptions.run_contract = false;
        }
        else if (arg == "--only-case" && i + 1 < argc)
        {
            suiteOptions.only_case_id = argv[++i];
        }
        else if (arg == "--case-timeout-sec" && i + 1 < argc)
        {
            suiteOptions.case_timeout_sec = std::stoi(argv[++i]);
        }
        else if (arg == "--trace-run")
        {
            suiteOptions.trace_run = true;
        }
        else if (arg == "--dump-replay-package")
        {
            suiteOptions.dump_replay_package = true;
        }
        else if (arg == "--no-replay-package")
        {
            suiteOptions.dump_replay_package = false;
        }
        else if (arg == "--dump-cxparser-ext-trace")
        {
            suiteOptions.dump_cxparser_ext_trace = true;
        }
        else if (arg == "--heartbeat-ms" && i + 1 < argc)
        {
            suiteOptions.heartbeat_ms = std::stoi(argv[++i]);
        }
        else if (arg == "--trace-dir" && i + 1 < argc)
        {
            suiteOptions.trace_dir = argv[++i];
        }
        else if (arg == "--no-contract")
        {
            suiteOptions.run_contract = false;
        }
        else if (arg == "--no-tool-display")
        {
            suiteOptions.export_tool_display = false;
        }
        else if (arg == "--no-evidence-summary")
        {
            suiteOptions.export_evidence_summary = false;
        }
        else if (arg == "--no-final-report")
        {
            suiteOptions.export_final_report = false;
        }
        else if (arg == "--no-best-gallery")
        {
            suiteOptions.export_best_examples = false;
        }
        else if (arg == "--suite-stop-after-headless")
        {
            suiteOptions.stop_after_headless = true;
        }
    }

    if (suiteOptions.enabled)
    {
        std::cout << "[MAIN] suite mode begin\n" << std::flush;
        CxScriptSuiteRunResult result;
        const bool ok = RunCxScriptSuite(suiteOptions, result);

        std::cout << "[MAIN] suite mode end ok="
                  << (ok ? "true" : "false")
                  << " reason=" << result.reason
                  << "\n" << std::flush;

        std::cout << "suite_run_ok=" << (ok ? "true" : "false") << "\n";
        std::cout << "reason=" << result.reason << "\n";
        std::cout << "total_cases=" << result.total_cases << "\n";
        std::cout << "executed_cases=" << result.executed_cases << "\n";
        std::cout << "contract_pass=" << result.contract_pass << "\n";
        std::cout << "contract_fail=" << result.contract_fail << "\n";
        std::cout << "report_root=" << result.report_root << "\n";

        if (result.reason == "REVIEW_REQUIRED")
        {
            std::cout << "[MAIN] exiting process from suite mode with review required\n" << std::flush;
            return 10;
        }

        std::cout << "[MAIN] exiting process from suite mode\n" << std::flush;
        return ok ? 0 : 1;
    }

    std::cout << "[MAIN] entering GUI mode\n" << std::flush;
    return glfw_occ_main();
}

int RunUnifiedLogSmoke(const CxUnifiedLogOptions& options)
{
    CXLOG_INFO("CxUnifiedLog", "smoke_begin", "running", "smoke_id=" + options.smoke_id);

    std::atomic<int> event_count{0};

    auto worker = [&](int id)
    {
        for (int i = 0; i < 100; ++i)
        {
            std::ostringstream oss;
            oss << "worker_thread_" << id << " event " << i;
            CXLOG_INFO("CxUnifiedLogSmoke", "worker_event", "ok", oss.str());
            event_count++;
        }
    };

    std::thread t1(worker, 0);
    std::thread t2(worker, 1);

    t1.join();
    t2.join();

    CXLOG_INFO("CxUnifiedLog", "smoke_end", "completed", "events=" + std::to_string(event_count.load()));

    std::cout << "unified_log_smoke_ok=true\n";
    std::cout << "written_events=" << event_count.load() + 2 << "\n";
    std::cout << "run_id=" << CxUnifiedLog::Instance().RunId() << "\n";
    std::cout << "log_path=" << CxUnifiedLog::Instance().Path().string() << "\n";

    return 0;
}

void SaveTorchRuntimeSmokeJson(
    const CxUnifiedLogOptions& options,
    bool initialized,
    bool ready,
    bool shutdown_ok,
    const std::string& version,
    const std::string& reason)
{
    if (options.torch_runtime_smoke.output_dir.empty())
        return;

    std::filesystem::create_directories(options.torch_runtime_smoke.output_dir);

    std::ofstream file(options.torch_runtime_smoke.output_dir / "torch_runtime_service_smoke.json");
    file << "{\n";
    file << "  \"schema_version\": 1,\n";
    file << "  \"run_id\": \"" << CxUnifiedLog::Instance().RunId() << "\",\n";
    file << "  \"runtime_dll\": \"" << options.torch_runtime_smoke.runtime_dll.string() << "\",\n";
    file << "  \"device\": \"" << options.torch_runtime_smoke.device << "\",\n";
    file << "  \"model_root\": \"" << options.torch_runtime_smoke.model_root << "\",\n";
    file << "  \"initialized\": " << (initialized ? "true" : "false") << ",\n";
    file << "  \"ready\": " << (ready ? "true" : "false") << ",\n";
    file << "  \"shutdown_ok\": " << (shutdown_ok ? "true" : "false") << ",\n";
    file << "  \"version\": \"" << version << "\",\n";
    file << "  \"reason\": \"" << reason << "\"\n";
    file << "}\n";
}

int RunCxTorchRuntimeSmoke(const CxUnifiedLogOptions& options)
{
    CXLOG_INFO("CxTorchRuntime", "smoke_begin", "running", "mode=torch_runtime_smoke");

    std::filesystem::create_directories(options.torch_runtime_smoke.output_dir);

    const std::filesystem::path runtime_dll = options.torch_runtime_smoke.runtime_dll;
    const std::filesystem::path runtime_dir = runtime_dll.parent_path();

    if (runtime_dir.empty()) {
        std::string reason = "Runtime DLL directory is empty";
        SaveTorchRuntimeSmokeJson(options, false, false, false, "", reason);
        CXLOG_ERROR("CxTorchRuntime", "smoke_end", "failed", "reason=" + reason);
        std::cout << "torch_runtime_smoke_ok=false\n";
        std::cout << "reason=" << reason << "\n";
        return 1;
    }

    const char* old_path = std::getenv("PATH");
    std::string new_path;
    if (old_path) {
        new_path = runtime_dir.string() + ";" + old_path;
    } else {
        new_path = runtime_dir.string();
    }
    SetEnvironmentVariableA("PATH", new_path.c_str());

    std::cout << "runtime_dll=" << runtime_dll.string() << "\n";
    std::cout << "runtime_dir=" << runtime_dir.string() << "\n";
    std::cout << "device=" << options.torch_runtime_smoke.device << "\n";
    std::cout << "model_root=" << options.torch_runtime_smoke.model_root << "\n";

    CxTorchRuntimeService service;
    CxTorchRuntimeConfig config;

    config.runtime_dll_path = runtime_dll.string();
    config.device = options.torch_runtime_smoke.device;
    config.model_root = options.torch_runtime_smoke.model_root;

    std::string reason;
    bool initialized = false;
    bool ready = false;
    std::string version;

    try {
        initialized = service.Initialize(config, reason);
        ready = service.IsReady();
        version = service.RuntimeVersion();
    } catch (const std::exception& e) {
        reason = "exception during initialization: " + std::string(e.what());
        initialized = false;
        ready = false;
        version = "";
    }

    service.Shutdown();
    const bool shutdown_ok = !service.IsReady();

    SaveTorchRuntimeSmokeJson(options, initialized, ready, shutdown_ok, version, reason);

    CXLOG_INFO(
        "CxTorchRuntime",
        "smoke_end",
        initialized && ready && shutdown_ok && !version.empty() ? "completed" : "failed",
        "initialized=" + std::string(initialized ? "true" : "false") +
        ",ready=" + std::string(ready ? "true" : "false") +
        ",shutdown_ok=" + std::string(shutdown_ok ? "true" : "false") +
        ",version=" + version +
        ",reason=" + reason);

    std::cout << "torch_runtime_smoke_ok=" << (initialized && ready && shutdown_ok && !version.empty() ? "true" : "false") << "\n";
    std::cout << "initialized=" << (initialized ? "true" : "false") << "\n";
    std::cout << "ready=" << (ready ? "true" : "false") << "\n";
    std::cout << "shutdown_ok=" << (shutdown_ok ? "true" : "false") << "\n";
    std::cout << "version=" << version << "\n";
    std::cout << "reason=" << reason << "\n";
    std::cout << "run_id=" << CxUnifiedLog::Instance().RunId() << "\n";

    return (initialized && ready && shutdown_ok && !version.empty()) ? 0 : 1;
}

int main(int argc, char** argv)
{
    CxUnifiedLogOptions logOptions;
    std::string logReason;

    ParseUnifiedLogArgs(argc, argv, logOptions, logReason);

    const std::string mode = DetectCxVisionRunMode(argc, argv);

    ShapeInteractionTestOptions shapeOptions;
    ParseShapeInteractionTestArgs(argc, argv, shapeOptions);

    bool should_enable_unified_log = logOptions.enabled || shapeOptions.enabled;
    if (!logOptions.path.empty() || shapeOptions.enabled)
    {
        if (logOptions.path.empty())
        {
            logOptions.path = "D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxscript_runs/_shared/cxvision_imgui_acceptance.jsonl";
        }
    }

    if (should_enable_unified_log)
    {
        bool init_ok = CxUnifiedLog::Instance().Initialize(
            logOptions.path,
            mode,
            argc,
            argv,
            logReason);
        if (!init_ok)
        {
            std::cerr << "[GuiMain] Unified log init failed: " << logReason << "\n";
            std::cerr << "[GuiMain] Path: " << logOptions.path.string() << "\n";
        }
        else
        {
            std::cout << "[GuiMain] Unified log initialized: " << logOptions.path.string() << "\n";
        }
    }

    InstallCxCrashLogHandlers();

    if (logOptions.enabled && logOptions.capture_stdio)
    {
        InstallUnifiedStdStreamCapture();
    }

    if (logOptions.enabled)
    {
        CXLOG_INFO(
            "GuiMain",
            "run_start",
            "started",
            "mode=" + mode + ", args=" + RedactCommandLine(argc, argv));
    }

    int exitCode = 0;

    if (logOptions.torch_runtime_smoke.enabled)
    {
        exitCode = RunCxTorchRuntimeSmoke(logOptions);
    }
    else if (logOptions.smoke_mode)
    {
        exitCode = RunUnifiedLogSmoke(logOptions);
    }
    else
    {
        exitCode = RunCxVisionApplication(argc, argv);
    }

    if (logOptions.enabled)
    {
        CxUnifiedLog::Instance().Shutdown(
            exitCode,
            exitCode == 0 ? "normal_exit" : "failed_exit");
    }

    if (logOptions.enabled && logOptions.capture_stdio)
    {
        RestoreUnifiedStdStreamCapture();
    }

    return exitCode;
}
