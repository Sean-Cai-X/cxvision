#include "pch.h"
#include "CxParamProbeRunner.h"
#include "CxScriptHeadlessRuntime.h"
#include <filesystem>

namespace fs = std::filesystem;

void InjectCandidateGlobals(
    CxScriptHeadlessOptions& options,
    const CxParamCandidate& candidate)
{
    options.method = candidate.method;
    options.threshold = candidate.threshold;
    options.gap = candidate.gap;
    options.linegap = candidate.linegap;
    options.wgap = candidate.wgap;
    options.hgap = candidate.hgap;
    options.filterprofile = candidate.filterprofile;
    options.samplerate = candidate.samplerate;
    options.min_score = candidate.min_score;
    options.find_num = candidate.find_num;
    options.compare_gap = candidate.compare_gap;
}

bool RunSingleParamProbe(
    const CxParamProbeRequest& request,
    CxParamProbeResult& result)
{
    result = {};

    if (request.script_path.empty())
    {
        result.reason = "script_path is empty";
        return false;
    }
    if (request.image_path.empty())
    {
        result.reason = "image_path is empty";
        return false;
    }
    if (request.out_dir.empty())
    {
        result.reason = "out_dir is empty";
        return false;
    }

    fs::create_directories(request.out_dir);

    CxScriptHeadlessOptions options;
    options.enabled = true;
    options.image_path = request.image_path;
    options.script_path = request.script_path;
    options.output_dir = request.out_dir;
    options.case_name = request.candidate.candidate_id;
    options.timeout_sec = request.timeout_seconds;
    options.stage25_tool = request.task.tool;
    options.stage25_target_id = request.target_id;

    InjectCandidateGlobals(options, request.candidate);

    CxScriptHeadlessResult headless_result;
    const bool launched = RunCxScriptHeadless(options, headless_result);

    result.launched = launched;
    result.executed = headless_result.ok;
    result.exit_code = headless_result.exit_code;
    result.reason = headless_result.reason;
    result.failure_stage = options.failure_stage;

    if (!headless_result.snapshot_path.empty())
    {
        result.snapshot_path = headless_result.snapshot_path;
    }
    if (!headless_result.overlay_path.empty())
    {
        result.result_overlay_path = headless_result.overlay_path;
    }
    if (!options.result_overlay_path.empty())
    {
        result.result_overlay_path = options.result_overlay_path;
    }
    if (!options.evidence_overlay_path.empty())
    {
        result.evidence_overlay_path = options.evidence_overlay_path;
    }
    if (!options.tool_display_path.empty())
    {
        result.tool_display_path = options.tool_display_path;
    }
    if (!headless_result.summary_path.empty())
    {
        result.result_summary_path = headless_result.summary_path;
    }

    result.candidate_points = options.points_count;
    result.fit_available = options.has_fit_line != 0 || options.has_fit_circle != 0;
    result.support_score = options.local_support;
    result.mean_distance = options.local_mean_distance;

    return launched;
}