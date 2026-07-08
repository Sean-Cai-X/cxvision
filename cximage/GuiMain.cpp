#include "Main.h"
#include "ManualStateTestConsole.h"
#include "CxScriptGeometryFrameProbe.h"
#include "CxScriptSuiteRunner.h"

#if defined(CXVISION_ENABLE_LEGACY_STAGE25_CPP)
#include "CxScriptStage25Runner.h"
#endif

#include <iostream>

int main(int argc, char** argv)
{
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
    }

    if (suiteOptions.enabled)
    {
        CxScriptSuiteRunResult result;
        const bool ok = RunCxScriptSuite(suiteOptions, result);

        std::cout << "suite_run_ok=" << (ok ? "true" : "false") << "\n";
        std::cout << "reason=" << result.reason << "\n";
        std::cout << "total_cases=" << result.total_cases << "\n";
        std::cout << "executed_cases=" << result.executed_cases << "\n";
        std::cout << "contract_pass=" << result.contract_pass << "\n";
        std::cout << "contract_fail=" << result.contract_fail << "\n";
        std::cout << "report_root=" << result.report_root << "\n";

        return ok ? 0 : 1;
    }

    return glfw_occ_main();
}