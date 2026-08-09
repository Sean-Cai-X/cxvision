#pragma once

#include "metrology_analytics/CxMetrologyAnalyticsSmoke.h"

#include <filesystem>
#include <string>

namespace cxvision::metrology_analytics
{

struct CxManualConsoleAnalyticsSmokePanelContract
{
    std::string window_title = "Analytics Smoke / Metrology Bridge";
    std::string tab_label = "Analytics Smoke";
    bool triggers_ci = false;
    bool has_reference_replay_section = true;
    bool has_s4_bridge_section = true;
    int minimum_expected_cases = 23;
};

struct ManualConsoleAnalyticsSmokeUiState
{
    bool initialized = false;
    bool run_requested = false;
    std::filesystem::path output_root =
        "D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxscript_runs/metrology_analytics";
    std::string status = "NOT_RUN";
    std::string reason = "Analytics smoke has not been run from Manual Console.";
    std::string last_output_dir;
    std::string last_summary_path;
    std::string last_report_path;
    std::string last_bridge_status = "S3_S4_BRIDGE_DRAFT_ONLY_PENDING_S4_REVIEW";
    CxMetrologyAnalyticsSmokeResult last_result;
    int selected_case = -1;
};

CxManualConsoleAnalyticsSmokePanelContract
ManualConsoleAnalyticsSmokePanelContract();

void DrawManualConsoleAnalyticsSmokePanel(
    ManualConsoleAnalyticsSmokeUiState& state);

} // namespace cxvision::metrology_analytics

