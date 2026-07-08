#pragma once

#include "CxScriptSuiteRuntime.h"
#include "CxScriptCatalogRuntime.h"
#include "CxScriptImageManifestRuntime.h"
#include <string>
#include <vector>

struct CxScriptSuiteRunOptions
{
    bool enabled = false;

    std::string suite_path;
    std::string image_manifest_path;
    std::string catalog_path_override;
    std::string out_root_override;

    bool save_overlay = true;
    bool export_tool_display = true;
    bool export_best_examples = true;
};

struct CxScriptSuiteCaseResult
{
    std::string case_id;
    std::string script_id;
    std::string script_path;

    std::string image_id;
    std::string image_path;
    std::string level;
    std::string target_id;
    std::string tool;

    std::string expected_result;
    std::string expected_policy_guard;
    std::string actual_policy_guard;

    bool headless_ok = false;
    bool contract_pass = false;

    int points_count = 0;
    int valid_points_count = 0;
    bool has_fit_line = false;
    bool has_fit_circle = false;

    double local_support = 0.0;
    double local_mean_distance = 0.0;
    double fit_offset = 0.0;

    std::string result_status;
    std::string failure_stage;
    std::string conclusion;

    std::string case_dir;
    std::string snapshot_path;
    std::string summary_path;
    std::string result_overlay_path;
    std::string evidence_overlay_path;
    std::string tool_display_path;
};

struct CxScriptSuiteRunResult
{
    bool ok = false;
    std::string reason;

    int total_cases = 0;
    int executed_cases = 0;
    int contract_pass = 0;
    int contract_fail = 0;

    std::string report_root;

    std::vector<CxScriptSuiteCaseResult> case_results;
};

bool RunCxScriptSuite(
    const CxScriptSuiteRunOptions& options,
    CxScriptSuiteRunResult& result);

const CxScriptCatalogEntry* FindCatalogScriptById(
    const CxScriptCatalogRuntime& catalog,
    const std::string& script_id);