#ifndef CXIMAGE_CXSCRIPT_HEADLESS_RUNTIME_H
#define CXIMAGE_CXSCRIPT_HEADLESS_RUNTIME_H

#include <string>

struct CxScriptHeadlessOptions
{
    bool enabled = false;

    std::string image_path;
    std::string script_path;
    std::string output_dir;

    std::string case_name;
    std::string snapshot_path;
    std::string overlay_path;
    std::string summary_path;
    std::string runtime_log_path;

    bool save_overlay = true;
    int max_steps = 10000;
    int timeout_sec = 30;

    std::string stage25_image_id;
    std::string stage25_level;
    std::string stage25_target_id;
    std::string stage25_tool;

    int roi_x0 = 0;
    int roi_y0 = 0;
    int roi_x1 = 0;
    int roi_y1 = 0;

    int circle_cx = 0;
    int circle_cy = 0;
    int circle_px = 0;
    int circle_py = 0;

    int tool_half_width = 20;
    int wgap = 32;
    int hgap = 8;
    int gap = 5;
    int linegap = 6;
    int threshold = 20;
    int method = 2;
    int filterprofile = 0;
    int samplerate = 1;
    double min_score = 0.0;
    int find_num = 1;
    int compare_gap = 0;

    bool contract_context_enabled = false;
    bool enable_evidence_analysis = true;
    int contract_headless_ok = 0;
    int contract_pass_initial = 0;
    int points_count = 0;
    int valid_points_count = 0;
    int has_fit_line = 0;
    int has_fit_circle = 0;
    double local_support = 0.0;
    double local_mean_distance = 0.0;
    double fit_offset = 0.0;
    double circle_radius = 0.0;
    double avgdist = 0.0;
    std::string policy_guard;
    int policy_guard_match = 0;
    std::string result_status;
    std::string failure_stage;
    std::string result_overlay_path;
    std::string evidence_overlay_path;
    std::string tool_display_path;

    double double_method = 0.0;
    double double_threshold = 0.0;
    double double_gap = 0.0;
    double double_linegap = 0.0;
    double double_wgap = 0.0;
    double double_hgap = 0.0;
};

struct CxScriptHeadlessResult
{
    bool ok = false;
    bool launched = false;
    bool executed = false;
    bool runtime_ok = false;
    bool timed_out = false;
    bool assets_complete = false;

    int exit_code = 0;

    std::string failure_stage;
    std::string reason;

    std::string snapshot_path;
    std::string overlay_path;
    std::string summary_path;
    std::string result_overlay_path;
    std::string evidence_overlay_path;
    std::string tool_display_path;
    std::string runtime_log_path;

    std::string run_state;
    std::string debug_status;
    std::string debug_reason;

    std::string current_result_name;
    std::string current_result_status;
    std::string current_result_reason;

    int points_count = 0;
    int valid_points_count = 0;
    bool has_fit_line = false;
    bool has_fit_circle = false;

    int model_point_count = 0;
    int candidate_count = 0;
    double best_score = 0.0;

    double local_support = 0.0;
    double local_mean_distance = 0.0;
    double fit_offset = 0.0;
    double circle_radius = 0.0;
    double avgdist = 0.0;
};

bool RunCxScriptHeadless(
    const CxScriptHeadlessOptions& options,
    CxScriptHeadlessResult& result);

#endif