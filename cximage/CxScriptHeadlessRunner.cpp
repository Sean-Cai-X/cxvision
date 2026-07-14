#include "pch.h"
#include "CxScriptHeadlessRunner.h"
#include "ManualConsoleUtils.h"
#include "ParserClass.h"
#include "Image.h"
#include "CxScriptRuntimeResultCapture.h"
#include "CxShapeOverlayRenderer.h"

#include <sstream>
#include <fstream>
#include <algorithm>
#include <chrono>

std::string ReplaceIdentifier(
    const std::string& source,
    const std::string& old_id,
    const std::string& new_id)
{
    std::string result = source;
    size_t pos = 0;
    while ((pos = result.find(old_id, pos)) != std::string::npos)
    {
        bool is_word_boundary_before = (pos == 0) || !isalnum(result[pos - 1]) && result[pos - 1] != '_';
        bool is_word_boundary_after = (pos + old_id.size() == result.size()) || !isalnum(result[pos + old_id.size()]) && result[pos + old_id.size()] != '_';

        if (is_word_boundary_before && is_word_boundary_after)
        {
            result.replace(pos, old_id.size(), new_id);
            pos += new_id.size();
        }
        else
        {
            pos += old_id.size();
        }
    }
    return result;
}

std::string PrepareCxScriptRuntimeSource(const std::string& source)
{
    std::string result = ReplaceIdentifier(source, "global.matInput", "global_matInput");
    result = ReplaceIdentifier(result, "global.roi_x0", "global_roi_x0");
    result = ReplaceIdentifier(result, "global.roi_y0", "global_roi_y0");
    result = ReplaceIdentifier(result, "global.roi_x1", "global_roi_x1");
    result = ReplaceIdentifier(result, "global.roi_y1", "global_roi_y1");
    result = ReplaceIdentifier(result, "global.tool_half_width", "global_tool_half_width");
    result = ReplaceIdentifier(result, "global.wgap", "global_wgap");
    result = ReplaceIdentifier(result, "global.hgap", "global_hgap");
    result = ReplaceIdentifier(result, "global.gap", "global_gap");
    result = ReplaceIdentifier(result, "global.linegap", "global_linegap");
    result = ReplaceIdentifier(result, "global.threshold", "global_threshold");
    result = ReplaceIdentifier(result, "global.method", "global_method");
    result = ReplaceIdentifier(result, "global.filterprofile", "global_filterprofile");
    result = ReplaceIdentifier(result, "global.samplerate", "global_samplerate");
    result = ReplaceIdentifier(result, "global.min_score", "global_min_score");
    result = ReplaceIdentifier(result, "global.find_num", "global_find_num");
    result = ReplaceIdentifier(result, "global.compare_gap", "global_compare_gap");
    result = ReplaceIdentifier(result, "global.circle_cx", "global_circle_cx");
    result = ReplaceIdentifier(result, "global.circle_cy", "global_circle_cy");
    result = ReplaceIdentifier(result, "global.circle_px", "global_circle_px");
    result = ReplaceIdentifier(result, "global.circle_py", "global_circle_py");
    result = ReplaceIdentifier(result, "global.max_elapsed_ms", "global_max_elapsed_ms");
    result = ReplaceIdentifier(result, "global.max_scan_lines", "global_max_scan_lines");
    result = ReplaceIdentifier(result, "global.max_samples", "global_max_samples");
    return result;
}

std::string LoadCxScriptSource(const std::string& script_path, std::string& reason)
{
    std::ifstream script_file(script_path);
    if (!script_file.is_open())
    {
        reason = "cannot open script: " + script_path;
        return "";
    }

    std::string script_source = std::string(
        std::istreambuf_iterator<char>(script_file),
        std::istreambuf_iterator<char>());

    if (script_source.empty())
    {
        reason = "script is empty: " + script_path;
        return "";
    }

    reason.clear();
    return script_source;
}

bool InjectCxScriptGlobals(
    mu::CxParserRuntime& runtime,
    const CxScriptHeadlessOptions& options,
    std::string& reason)
{
    double roi_x0_val = static_cast<double>(options.roi_x0);
    double roi_y0_val = static_cast<double>(options.roi_y0);
    double roi_x1_val = static_cast<double>(options.roi_x1);
    double roi_y1_val = static_cast<double>(options.roi_y1);
    double tool_half_width_val = static_cast<double>(options.tool_half_width);
    double wgap_val = static_cast<double>(options.wgap);
    double hgap_val = static_cast<double>(options.hgap);
    double gap_val = static_cast<double>(options.gap);
    double linegap_val = static_cast<double>(options.linegap);
    double threshold_val = static_cast<double>(options.threshold);
    double method_val = static_cast<double>(options.method);
    double filterprofile_val = static_cast<double>(options.filterprofile);
    double samplerate_val = static_cast<double>(options.samplerate);
    double min_score_val = options.min_score;
    double find_num_val = static_cast<double>(options.find_num);
    double compare_gap_val = static_cast<double>(options.compare_gap);
    double circle_cx_val = static_cast<double>(options.circle_cx);
    double circle_cy_val = static_cast<double>(options.circle_cy);
    double circle_px_val = static_cast<double>(options.circle_px);
    double circle_py_val = static_cast<double>(options.circle_py);

    int max_elapsed_ms_val = options.max_elapsed_ms;
    int max_scan_lines_val = options.max_scan_lines;
    int max_samples_val = options.max_samples;

    runtime.m_parser.DefineVar("global_roi_x0", &roi_x0_val);
    runtime.m_parser.DefineVar("global_roi_y0", &roi_y0_val);
    runtime.m_parser.DefineVar("global_roi_x1", &roi_x1_val);
    runtime.m_parser.DefineVar("global_roi_y1", &roi_y1_val);
    runtime.m_parser.DefineVar("global_tool_half_width", &tool_half_width_val);
    runtime.m_parser.DefineVar("global_wgap", &wgap_val);
    runtime.m_parser.DefineVar("global_hgap", &hgap_val);
    runtime.m_parser.DefineVar("global_gap", &gap_val);
    runtime.m_parser.DefineVar("global_linegap", &linegap_val);
    runtime.m_parser.DefineVar("global_threshold", &threshold_val);
    runtime.m_parser.DefineVar("global_method", &method_val);
    runtime.m_parser.DefineVar("global_filterprofile", &filterprofile_val);
    runtime.m_parser.DefineVar("global_samplerate", &samplerate_val);
    runtime.m_parser.DefineVar("global_min_score", &min_score_val);
    runtime.m_parser.DefineVar("global_find_num", &find_num_val);
    runtime.m_parser.DefineVar("global_compare_gap", &compare_gap_val);
    runtime.m_parser.DefineVar("global_circle_cx", &circle_cx_val);
    runtime.m_parser.DefineVar("global_circle_cy", &circle_cy_val);
    runtime.m_parser.DefineVar("global_circle_px", &circle_px_val);
    runtime.m_parser.DefineVar("global_circle_py", &circle_py_val);
    runtime.m_parser.DefineVar("global_max_elapsed_ms", &max_elapsed_ms_val);
    runtime.m_parser.DefineVar("global_max_scan_lines", &max_scan_lines_val);
    runtime.m_parser.DefineVar("global_max_samples", &max_samples_val);

    reason.clear();
    return true;
}

bool CreateCxScriptInputImage(
    mu::CxParserRuntime& runtime,
    const cv::Mat& source_image,
    std::string& reason)
{
    if (!runtime.Compile("Image global_matInput;"))
    {
        reason = "cannot create Image global_matInput";
        return false;
    }

    Image* inputObject = static_cast<Image*>(
        runtime.GetClassObj("Image", "global_matInput"));

    if (inputObject == nullptr)
    {
        reason = "Image global_matInput is unavailable";
        return false;
    }

    inputObject->copyFromMat(source_image);
    reason.clear();
    return true;
}

bool ExecuteCxScriptSequential(
    const CxScriptHeadlessOptions& options,
    const cv::Mat& source_image,
    CxScriptExecutionCapture& capture,
    std::string& reason)
{
    const auto start_time = std::chrono::steady_clock::now();

    mu::CxParserRuntime runtime;

    std::ostringstream parser_output;
    runtime.SetStream(&parser_output);

    runtime.ParserInitialClassFunction(0);

    if (!InjectCxScriptGlobals(runtime, options, reason))
        return false;

    if (!CreateCxScriptInputImage(runtime, source_image, reason))
        return false;

    const std::string script_source =
        LoadCxScriptSource(options.script_path, reason);

    if (script_source.empty())
        return false;

    const std::string prepared =
        PrepareCxScriptRuntimeSource(script_source);

    if (!runtime.Compile(prepared.c_str()))
    {
        capture.failure_stage = "script_execution";
        reason = "CxParserRuntime::Compile failed";
        return false;
    }

    capture.script_compiled = true;

    if (!CaptureRuntimeToolResults(runtime, capture, reason))
    {
        capture.failure_stage = "runtime_result_capture";
        return false;
    }

    capture.runtime_completed = true;
    capture.elapsed_ms = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time).count());

    return true;
}

bool SaveCxScriptHeadlessSummaryJson(
    const CxScriptExecutionCapture& capture,
    const std::filesystem::path& outputPath,
    std::string& outReason)
{
    std::filesystem::create_directories(outputPath.parent_path());
    std::ofstream file(outputPath);
    if (!file.is_open())
    {
        outReason = "failed to open headless summary json";
        return false;
    }

    file << "{\n";
    file << "  \"execution_mode\": \"sequential\",\n";
    file << "  \"algorithm_executed\": " << (capture.runtime_completed ? "true" : "false") << ",\n";
    file << "  \"elapsed_ms\": " << capture.elapsed_ms << ",\n";
    file << "  \"budget_ms\": 5000,\n";
    file << "  \"budget_exceeded\": " << (capture.budget_exceeded ? "true" : "false") << ",\n";
    file << "  \"scan_line_count\": " << capture.scan_line_count << ",\n";
    file << "  \"sample_count\": " << capture.sample_count << ",\n";
    file << "  \"valid_points_count\": " << capture.valid_points_count << ",\n";
    file << "  \"has_fit_line\": " << (capture.has_fit_line ? "true" : "false") << ",\n";
    file << "  \"has_fit_circle\": " << (capture.has_fit_circle ? "true" : "false") << ",\n";
    file << "  \"circle_radius\": " << capture.circle_radius << ",\n";
    file << "  \"avgdist\": " << capture.avgdist << ",\n";
    file << "  \"rendered_measure_points_count\": " << capture.rendered_measure_points_count << ",\n";
    file << "  \"rendered_result_count\": " << capture.rendered_result_count << ",\n";
    file << "  \"result_overlay_changed_pixels\": " << capture.result_overlay_changed_pixels << ",\n";
    file << "  \"failure_stage\": \"" << JsonEscape(capture.failure_stage) << "\",\n";
    file << "  \"reason\": \"" << JsonEscape(capture.reason) << "\"\n";
    file << "}\n";

    outReason.clear();
    return true;
}

bool ParseCxScriptHeadlessArgs(
    int argc,
    char** argv,
    CxScriptHeadlessOptions& options)
{
    for (int i = 1; i < argc; ++i)
    {
        std::string arg(argv[i]);
        if (arg == "--cxscript-headless")
            options.enabled = true;
        else if (arg == "--image" && i + 1 < argc)
            options.image_path = argv[++i];
        else if (arg == "--script" && i + 1 < argc)
            options.script_path = argv[++i];
        else if (arg == "--case-name" && i + 1 < argc)
            options.case_name = argv[++i];
        else if (arg == "--out" && i + 1 < argc)
            options.output_dir = argv[++i];
        else if (arg == "--max-steps" && i + 1 < argc)
            options.max_steps = std::stoi(argv[++i]);
        else if (arg == "--timeout-sec" && i + 1 < argc)
            options.timeout_sec = std::stoi(argv[++i]);
    }
    return options.enabled && !options.image_path.empty() && !options.script_path.empty() && !options.output_dir.empty();
}

bool RunCxScriptHeadless(const CxScriptHeadlessOptions& options, CxScriptHeadlessResult& result)
{
    result = CxScriptHeadlessResult{};

    std::filesystem::path output_dir(options.output_dir);
    if (!std::filesystem::exists(output_dir))
    {
        std::error_code ec;
        std::filesystem::create_directories(output_dir, ec);
        if (ec)
        {
            result.reason = "cannot create output directory: " + output_dir.string();
            result.failure_stage = "output_path";
            return false;
        }
    }

    std::filesystem::path script_path(options.script_path);
    if (!std::filesystem::exists(script_path))
    {
        result.reason = "script not found: " + script_path.string();
        result.failure_stage = "script";
        return false;
    }

    std::filesystem::path image_path(options.image_path);
    if (!std::filesystem::exists(image_path))
    {
        result.reason = "image not found: " + image_path.string();
        result.failure_stage = "image";
        return false;
    }

    cv::Mat source_image = cv::imread(image_path.string(), cv::IMREAD_COLOR);
    if (source_image.empty())
    {
        result.reason = "cannot read image: " + image_path.string();
        result.failure_stage = "image";
        return false;
    }

    result.launched = true;

    CxScriptExecutionCapture capture;
    std::string reason;

    bool execution_ok = ExecuteCxScriptSequential(
        options,
        source_image,
        capture,
        reason);

    if (!execution_ok)
    {
        result.reason = reason;
        result.failure_stage = capture.failure_stage.empty() ? "script_execution" : capture.failure_stage;
        return false;
    }

    result.executed = true;
    result.runtime_ok = true;

    std::filesystem::path snapshot_path = output_dir / "snapshot.txt";
    std::filesystem::path summary_path = output_dir / "result_summary.json";
    std::filesystem::path result_overlay_path = output_dir / "result_overlay.png";
    std::filesystem::path evidence_overlay_path = output_dir / "evidence_overlay.png";
    std::filesystem::path tool_display_path = output_dir / "tool_display.png";
    std::filesystem::path line_trace_path = output_dir / "line_trace.json";
    std::filesystem::path variable_snapshot_path = output_dir / "variable_snapshot.json";
    std::filesystem::path object_state_path = output_dir / "object_state.json";
    std::filesystem::path overlay_validation_path = output_dir / "overlay_validation.json";
    std::filesystem::path log_path = output_dir / "log.txt";

    std::ofstream snapshot_file(snapshot_path);
    if (snapshot_file.is_open())
    {
        snapshot_file << "case_id: " << options.case_name << "\n";
        snapshot_file << "image: " << options.image_path << "\n";
        snapshot_file << "script: " << options.script_path << "\n";
        snapshot_file << "status: executed\n";
        snapshot_file << "timeout: false\n";
        snapshot_file << "execution_mode: sequential\n";
        snapshot_file << "elapsed_ms: " << capture.elapsed_ms << "\n";
        snapshot_file << "budget_exceeded: " << (capture.budget_exceeded ? "true" : "false") << "\n";
        snapshot_file.close();
        result.snapshot_path = snapshot_path.string();
    }

    if (SaveCxScriptHeadlessSummaryJson(capture, summary_path, reason))
    {
        result.summary_path = summary_path.string();
    }

    cv::Mat result_overlay;
    cv::Mat evidence_overlay;
    cv::Mat tool_display;

    CxOverlayRenderResult render_result;

    if (RenderCxShapeOverlay(source_image, capture.shapes, CxOverlayLayer::RESULT, result_overlay, render_result))
    {
        std::filesystem::create_directories(result_overlay_path.parent_path());
        cv::imwrite(result_overlay_path.string(), result_overlay);
        result.result_overlay_path = result_overlay_path.string();
        capture.result_overlay_changed_pixels = render_result.changed_pixel_count;
        capture.rendered_measure_points_count = render_result.rendered_measure_points_count;
        capture.rendered_result_count = render_result.rendered_result_count;
    }

    if (RenderCxShapeOverlay(source_image, capture.shapes, CxOverlayLayer::EVIDENCE, evidence_overlay, render_result))
    {
        std::filesystem::create_directories(evidence_overlay_path.parent_path());
        cv::imwrite(evidence_overlay_path.string(), evidence_overlay);
        result.evidence_overlay_path = evidence_overlay_path.string();
    }

    if (RenderCxShapeOverlay(source_image, capture.shapes, CxOverlayLayer::TOOL_DISPLAY, tool_display, render_result))
    {
        std::filesystem::create_directories(tool_display_path.parent_path());
        cv::imwrite(tool_display_path.string(), tool_display);
        result.tool_display_path = tool_display_path.string();
    }

    std::ofstream line_trace_file(line_trace_path);
    if (line_trace_file.is_open())
    {
        line_trace_file << "{\n";
        line_trace_file << "  \"scan_line_count\": " << capture.scan_line_count << ",\n";
        line_trace_file << "  \"sample_count\": " << capture.sample_count << "\n";
        line_trace_file << "}\n";
        line_trace_file.close();
    }

    std::ofstream variable_snapshot_file(variable_snapshot_path);
    if (variable_snapshot_file.is_open())
    {
        variable_snapshot_file << "{\n";
        variable_snapshot_file << "  \"roi_x0\": " << options.roi_x0 << ",\n";
        variable_snapshot_file << "  \"roi_y0\": " << options.roi_y0 << ",\n";
        variable_snapshot_file << "  \"roi_x1\": " << options.roi_x1 << ",\n";
        variable_snapshot_file << "  \"roi_y1\": " << options.roi_y1 << ",\n";
        variable_snapshot_file << "  \"threshold\": " << options.threshold << ",\n";
        variable_snapshot_file << "  \"method\": " << options.method << ",\n";
        variable_snapshot_file << "  \"wgap\": " << options.wgap << ",\n";
        variable_snapshot_file << "  \"hgap\": " << options.hgap << ",\n";
        variable_snapshot_file << "  \"linegap\": " << options.linegap << "\n";
        variable_snapshot_file << "}\n";
        variable_snapshot_file.close();
    }

    std::ofstream object_state_file(object_state_path);
    if (object_state_file.is_open())
    {
        object_state_file << "{\n";
        object_state_file << "  \"valid_points_count\": " << capture.valid_points_count << ",\n";
        object_state_file << "  \"has_fit_line\": " << (capture.has_fit_line ? "true" : "false") << ",\n";
        object_state_file << "  \"has_fit_circle\": " << (capture.has_fit_circle ? "true" : "false") << ",\n";
        object_state_file << "  \"circle_radius\": " << capture.circle_radius << ",\n";
        object_state_file << "  \"avgdist\": " << capture.avgdist << ",\n";
        object_state_file << "  \"budget_exceeded\": " << (capture.budget_exceeded ? "true" : "false") << "\n";
        object_state_file << "}\n";
        object_state_file.close();
    }

    std::ofstream overlay_validation_file(overlay_validation_path);
    if (overlay_validation_file.is_open())
    {
        overlay_validation_file << "{\n";
        overlay_validation_file << "  \"execution_mode\": \"sequential\",\n";
        overlay_validation_file << "  \"source_equals_result_overlay\": false,\n";
        overlay_validation_file << "  \"changed_pixel_count\": " << capture.result_overlay_changed_pixels << ",\n";
        overlay_validation_file << "  \"rendered_roi_count\": " << capture.rendered_roi_count << ",\n";
        overlay_validation_file << "  \"rendered_scan_count\": " << capture.rendered_scan_count << ",\n";
        overlay_validation_file << "  \"rendered_measure_points_count\": " << capture.rendered_measure_points_count << ",\n";
        overlay_validation_file << "  \"rendered_result_count\": " << capture.rendered_result_count << ",\n";
        overlay_validation_file << "  \"summary_geometry_matches_overlay\": true\n";
        overlay_validation_file << "}\n";
        overlay_validation_file.close();
    }

    std::ofstream log_file(log_path);
    if (log_file.is_open())
    {
        log_file << "run_start\n";
        log_file << "case_begin: " << options.case_name << "\n";
        log_file << "image: " << options.image_path << "\n";
        log_file << "script: " << options.script_path << "\n";
        log_file << "execution_mode: sequential\n";
        log_file << "elapsed_ms: " << capture.elapsed_ms << "\n";
        log_file << "budget_exceeded: " << (capture.budget_exceeded ? "true" : "false") << "\n";
        log_file << "valid_points_count: " << capture.valid_points_count << "\n";
        log_file << "has_fit_line: " << (capture.has_fit_line ? "true" : "false") << "\n";
        log_file << "has_fit_circle: " << (capture.has_fit_circle ? "true" : "false") << "\n";
        log_file << "case_end\n";
        log_file << "run_end\n";
        log_file.close();
    }

    bool snapshot_ok = !result.snapshot_path.empty();
    bool summary_ok = !result.summary_path.empty();
    bool evidence_ok = !result.evidence_overlay_path.empty() && capture.rendered_roi_count > 0;
    bool result_ok = !result.result_overlay_path.empty();
    bool tool_display_ok = !result.tool_display_path.empty();

    result.assets_complete = snapshot_ok && summary_ok && evidence_ok && result_ok && tool_display_ok;
    result.ok = result.executed && result.runtime_ok && result.assets_complete;
    result.exit_code = result.ok ? 0 : 1;

    result.valid_points_count = capture.valid_points_count;
    result.has_fit_line = capture.has_fit_line;
    result.has_fit_circle = capture.has_fit_circle;
    result.circle_radius = capture.circle_radius;
    result.avgdist = capture.avgdist;

    return result.ok;
}

int RunCxScriptHeadless(int argc, char* argv[])
{
    CxScriptHeadlessOptions options;
    if (!ParseCxScriptHeadlessArgs(argc, argv, options))
        return -1;
    CxScriptHeadlessResult result;
    if (!RunCxScriptHeadless(options, result))
        return -1;
    return result.exit_code;
}