#include "Main.h"
#include "ManualStateTestConsole.h"
#include "CxScriptStage25Runner.h"
#include <iostream>

int main(int argc, char** argv)
{
    CxScriptHeadlessOptions headlessOptions;

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

    return glfw_occ_main();
}