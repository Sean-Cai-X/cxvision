#include "Main.h"
#include "ManualStateTestConsole.h"
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

    return glfw_occ_main();
}