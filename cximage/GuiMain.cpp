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

#if defined(CXVISION_ENABLE_LEGACY_STAGE25_CPP)
#include "CxScriptStage25Runner.h"
#endif

#include <iostream>

bool ParseShapeInteractionTestArgs(int argc, char** argv, ShapeInteractionTestOptions& options)
{
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--shape-interaction-smoke")
        {
            options.enabled = true;
        }
        else if (arg == "--annotation-tool-manifest" && i + 1 < argc)
        {
            options.manifest_path = argv[++i];
        }
        else if (arg == "--shape-interaction-suite" && i + 1 < argc)
        {
            options.suite_path = argv[++i];
        }
        else if (arg == "--out" && i + 1 < argc)
        {
            options.out_dir = argv[++i];
        }
    }
    return true;
}

bool RunShapeInteractionSmokeCli(
    const std::string& manifest_path,
    const std::string& suite_path,
    const std::string& out_dir,
    CxShapeInteractionBatchResult& result)
{
    ViewController viewer;
    return viewer.RunShapeInteractionSmoke(manifest_path, suite_path, out_dir, result);
}

int RunCxVisionApplication(int argc, char** argv)
{
    ShapeInteractionTestOptions shapeOptions;
    ParseShapeInteractionTestArgs(argc, argv, shapeOptions);
    if (shapeOptions.enabled)
    {
        CxShapeInteractionBatchResult result;
        const bool ok = RunShapeInteractionSmokeCli(
            shapeOptions.manifest_path.empty() ? "cxparser/cxscript/module/cximage/tool_annotation_basic.cxsc" : shapeOptions.manifest_path,
            shapeOptions.suite_path.empty() ? "cxparser/cxscript/module/cximage/tests/shape_interaction_acceptance.cxsc" : shapeOptions.suite_path,
            shapeOptions.out_dir.empty() ? "D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxscript_runs/shape_interaction_smoke" : shapeOptions.out_dir,
            result);

        std::cout << "shape_interaction_smoke_ok=" << (ok ? "true" : "false") << "\n";
        std::cout << "total_cases=" << result.cases.size() << "\n";
        std::cout << "pass_count=" << std::count_if(result.cases.begin(), result.cases.end(),
                                                    [](const auto& c) { return c.pass; }) << "\n";
        std::cout << "fail_count=" << std::count_if(result.cases.begin(), result.cases.end(),
                                                    [](const auto& c) { return !c.pass; }) << "\n";

        for (const auto& c : result.cases)
        {
            std::cout << "case=" << c.case_id << " pass=" << (c.pass ? "true" : "false")
                      << " conclusion=" << c.conclusion << "\n";
        }

        return ok ? 0 : 1;
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

int main(int argc, char** argv)
{
    CxUnifiedLogOptions logOptions;
    std::string logReason;

    ParseUnifiedLogArgs(argc, argv, logOptions, logReason);

    const std::string mode = DetectCxVisionRunMode(argc, argv);

    if (logOptions.enabled)
    {
        CxUnifiedLog::Instance().Initialize(
            logOptions.path,
            mode,
            argc,
            argv,
            logReason);
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

    if (logOptions.smoke_mode)
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