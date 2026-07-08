#include "Main.h"
#include "ManualStateTestConsole.h"
#include "CxScriptStage25Runner.h"
#include "CxScriptGeometryFrameProbe.h"
#include "CxScriptSuiteRuntime.h"
#include "CxScriptCatalogRuntime.h"
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

    std::string suite_path;
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--cxscript-suite")
        {
            if (i + 1 < argc)
            {
                suite_path = argv[++i];
            }
        }
    }

    if (!suite_path.empty())
    {
        CxScriptSuiteRuntime suite;
        std::string suite_reason;

        if (!LoadCxScriptSuiteFile(suite_path, suite, suite_reason))
        {
            std::cout << "suite_load_ok=false\n";
            std::cout << "reason=" << suite_reason << "\n";
            return 1;
        }

        std::cout << "suite_load_ok=true\n";
        std::cout << "suite_id=" << suite.suite_id << "\n";
        std::cout << "suite_name=" << suite.name << "\n";
        std::cout << "catalog_path=" << suite.catalog_path << "\n";
        std::cout << "output_root=" << suite.output_root << "\n";
        std::cout << "case_count=" << suite.cases.size() << "\n";

        for (const auto& case_entry : suite.cases)
        {
            std::cout << "case_id=" << case_entry.case_id << "\n";
            std::cout << "  script_id=" << case_entry.script_id << "\n";
            std::cout << "  expected=" << case_entry.expected_result << "\n";
            std::cout << "  policy_guard=" << case_entry.expected_policy_guard << "\n";
            std::cout << "  image=" << case_entry.image_id << "\n";
            std::cout << "  level=" << case_entry.level << "\n";
        }

        if (!suite.catalog_path.empty())
        {
            CxScriptCatalogRuntime catalog;
            std::string catalog_reason;

            if (LoadCxScriptCatalogFile(suite.catalog_path, catalog, catalog_reason))
            {
                std::cout << "catalog_load_ok=true\n";
                std::cout << "catalog_name=" << catalog.name << "\n";
                std::cout << "catalog_version=" << catalog.version << "\n";
                std::cout << "catalog_script_count=" << catalog.scripts.size() << "\n";
            }
            else
            {
                std::cout << "catalog_load_ok=false\n";
                std::cout << "catalog_reason=" << catalog_reason << "\n";
            }
        }

        return 0;
    }

    return glfw_occ_main();
}