#pragma once

#include <string>
#include <vector>
#include "CxParamRegressionRuntime.h"
#include "CxScriptHeadlessRuntime.h"

struct CxParamProbeRequest
{
    CxParamRegressionTask task;
    CxParamCandidate candidate;
    std::string image_path;
    std::string target_id;
    std::string script_path;
    std::string contract_path;
    std::string out_dir;
    int timeout_seconds = 10;
};

struct CxParamProbeResult
{
    bool launched = false;
    bool executed = false;
    bool runtime_ok = false;
    bool assets_complete = false;
    bool probe_ok = false;
    bool support_available = false;
    bool timeout = false;
    int exit_code = -1;
    std::string result_summary_path;
    std::string result_overlay_path;
    std::string evidence_overlay_path;
    std::string tool_display_path;
    std::string snapshot_path;
    std::string failure_stage;
    std::string reason;

    int candidate_points = 0;
    bool fit_available = false;
    double support_score = 0.0;
    double mean_distance = 0.0;
};

bool RunSingleParamProbe(
    const CxParamProbeRequest& request,
    CxParamProbeResult& result);

void InjectCandidateGlobals(
    CxScriptHeadlessOptions& options,
    const CxParamCandidate& candidate);
