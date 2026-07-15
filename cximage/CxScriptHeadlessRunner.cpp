#include "pch.h"
#include "CxScriptHeadlessRunner.h"
#include "ManualConsoleUtils.h"
#include "ParserClass.h"
#include "Image.h"
#include "CxScriptRuntimeResultCapture.h"
#include "CxShapeOverlayRenderer.h"
#include "CxScriptRuntimeCaptureSmoke.h"

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

std::string PrepareCxScriptRuntimeSource(const std::string& source, bool contract_context)
{
    std::istringstream input(source);
    std::ostringstream normalized;
    std::string line;
    while (std::getline(input, line))
    {
        if (contract_context &&
            (line.find("global.contract_status") != std::string::npos ||
             line.find("global.contract_conclusion") != std::string::npos ||
             line.find("global_contract_status") != std::string::npos ||
             line.find("global_contract_conclusion") != std::string::npos))
        {
            continue;
        }
        const size_t first = line.find_first_not_of(" \t");
        if (first != std::string::npos)
        {
            static const char* declaration_types[] = { "int ", "double ", "float " };
            for (const char* type : declaration_types)
            {
                const size_t length = std::strlen(type);
                if (line.compare(first, length, type) == 0)
                {
                    line.erase(first, length);
                    break;
                }
            }
        }
        normalized << line << '\n';
    }

    std::string runtime_source = normalized.str();
    for (size_t i = 0; i < runtime_source.size(); ++i)
    {
        if (runtime_source[i] != '&')
            continue;

        size_t previous = i;
        while (previous > 0 && std::isspace(static_cast<unsigned char>(runtime_source[previous - 1])))
            --previous;
        size_t next = i + 1;
        while (next < runtime_source.size() && std::isspace(static_cast<unsigned char>(runtime_source[next])))
            ++next;

        const bool argument_position =
            previous > 0 && (runtime_source[previous - 1] == '(' || runtime_source[previous - 1] == ',');
        const bool object_identifier =
            next < runtime_source.size() &&
            (std::isalpha(static_cast<unsigned char>(runtime_source[next])) || runtime_source[next] == '_');
        if (argument_position && object_identifier)
        {
            runtime_source.erase(i, 1);
            --i;
        }
    }

    std::string result = ReplaceIdentifier(runtime_source, "global.matInput", "global_matInput");
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
    result = ReplaceIdentifier(result, "global.line_ref", "global_line_ref");
    result = ReplaceIdentifier(result, "global.circle_ref", "global_circle_ref");
    result = ReplaceIdentifier(result, "global.current_status", "global_current_status");
    result = ReplaceIdentifier(result, "global.contract_pass", "global_contract_pass");
    result = ReplaceIdentifier(result, "global.headless_ok", "global_headless_ok");
    result = ReplaceIdentifier(result, "global.algorithm_executed", "global_algorithm_executed");
    result = ReplaceIdentifier(result, "global.budget_exceeded", "global_budget_exceeded");
    result = ReplaceIdentifier(result, "global.valid_points_count", "global_valid_points_count");
    result = ReplaceIdentifier(result, "global.has_fit_line", "global_has_fit_line");
    result = ReplaceIdentifier(result, "global.has_fit_circle", "global_has_fit_circle");
    result = ReplaceIdentifier(result, "global.rendered_measure_points_count", "global_rendered_measure_points_count");
    result = ReplaceIdentifier(result, "global.rendered_result_count", "global_rendered_result_count");
    result = ReplaceIdentifier(result, "global.result_overlay_changed_pixels", "global_result_overlay_changed_pixels");
    result = ReplaceIdentifier(result, "global.policy_guard_match", "global_policy_guard_match");
    result = ReplaceIdentifier(result, "global.circle_radius", "global_contract_circle_radius");
    result = ReplaceIdentifier(result, "global.avgdist", "global_contract_avgdist");
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

void DefineCxScriptLocalVariables(
    mu::CxParserRuntime& runtime,
    const std::string& source,
    std::map<std::string, double>& storage)
{
    std::istringstream input(source);
    std::string line;
    while (std::getline(input, line))
    {
        const size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos)
            continue;

        static const char* declaration_types[] = { "int ", "double ", "float " };
        size_t name_begin = std::string::npos;
        for (const char* type : declaration_types)
        {
            const size_t length = std::strlen(type);
            if (line.compare(first, length, type) == 0)
            {
                name_begin = first + length;
                break;
            }
        }
        if (name_begin == std::string::npos)
            continue;

        size_t name_end = name_begin;
        while (name_end < line.size() &&
               (std::isalnum(static_cast<unsigned char>(line[name_end])) || line[name_end] == '_'))
        {
            ++name_end;
        }
        if (name_end == name_begin)
            continue;

        const std::string name = line.substr(name_begin, name_end - name_begin);
        auto inserted = storage.emplace(name, 0.0);
        if (inserted.second)
            runtime.m_parser.DefineVar(name, &inserted.first->second);
    }
}

struct CxScriptInjectedGlobals
{
    double roi_x0 = 0.0;
    double roi_y0 = 0.0;
    double roi_x1 = 0.0;
    double roi_y1 = 0.0;
    double tool_half_width = 0.0;
    double wgap = 0.0;
    double hgap = 0.0;
    double gap = 0.0;
    double linegap = 0.0;
    double threshold = 0.0;
    double method = 0.0;
    double filterprofile = 0.0;
    double samplerate = 0.0;
    double min_score = 0.0;
    double find_num = 0.0;
    double compare_gap = 0.0;
    double circle_cx = 0.0;
    double circle_cy = 0.0;
    double circle_px = 0.0;
    double circle_py = 0.0;
    double max_elapsed_ms = 0.0;
    double max_scan_lines = 0.0;
    double max_samples = 0.0;
    double line_ref = 0.0;
    double circle_ref = 0.0;
    double contract_pass = 0.0;
    double headless_ok = 0.0;
    double algorithm_executed = 0.0;
    double budget_exceeded = 0.0;
    double valid_points_count = 0.0;
    double has_fit_line = 0.0;
    double has_fit_circle = 0.0;
    double rendered_measure_points_count = 0.0;
    double rendered_result_count = 0.0;
    double result_overlay_changed_pixels = 0.0;
    double policy_guard_match = 0.0;
    double contract_circle_radius = 0.0;
    double contract_avgdist = 0.0;
};

bool InjectCxScriptGlobals(
    mu::CxParserRuntime& runtime,
    const CxScriptHeadlessOptions& options,
    CxScriptInjectedGlobals& values,
    std::string& reason)
{
    values.roi_x0 = static_cast<double>(options.roi_x0);
    values.roi_y0 = static_cast<double>(options.roi_y0);
    values.roi_x1 = static_cast<double>(options.roi_x1);
    values.roi_y1 = static_cast<double>(options.roi_y1);
    values.tool_half_width = static_cast<double>(options.tool_half_width);
    values.wgap = static_cast<double>(options.wgap);
    values.hgap = static_cast<double>(options.hgap);
    values.gap = static_cast<double>(options.gap);
    values.linegap = static_cast<double>(options.linegap);
    values.threshold = static_cast<double>(options.threshold);
    values.method = static_cast<double>(options.method);
    values.filterprofile = static_cast<double>(options.filterprofile);
    values.samplerate = static_cast<double>(options.samplerate);
    values.min_score = options.min_score;
    values.find_num = static_cast<double>(options.find_num);
    values.compare_gap = static_cast<double>(options.compare_gap);
    values.circle_cx = static_cast<double>(options.circle_cx);
    values.circle_cy = static_cast<double>(options.circle_cy);
    values.circle_px = static_cast<double>(options.circle_px);
    values.circle_py = static_cast<double>(options.circle_py);
    values.max_elapsed_ms = static_cast<double>(options.max_elapsed_ms);
    values.max_scan_lines = static_cast<double>(options.max_scan_lines);
    values.max_samples = static_cast<double>(options.max_samples);

    runtime.m_parser.DefineVar("global_roi_x0", &values.roi_x0);
    runtime.m_parser.DefineVar("global_roi_y0", &values.roi_y0);
    runtime.m_parser.DefineVar("global_roi_x1", &values.roi_x1);
    runtime.m_parser.DefineVar("global_roi_y1", &values.roi_y1);
    runtime.m_parser.DefineVar("global_tool_half_width", &values.tool_half_width);
    runtime.m_parser.DefineVar("global_wgap", &values.wgap);
    runtime.m_parser.DefineVar("global_hgap", &values.hgap);
    runtime.m_parser.DefineVar("global_gap", &values.gap);
    runtime.m_parser.DefineVar("global_linegap", &values.linegap);
    runtime.m_parser.DefineVar("global_threshold", &values.threshold);
    runtime.m_parser.DefineVar("global_method", &values.method);
    runtime.m_parser.DefineVar("global_filterprofile", &values.filterprofile);
    runtime.m_parser.DefineVar("global_samplerate", &values.samplerate);
    runtime.m_parser.DefineVar("global_min_score", &values.min_score);
    runtime.m_parser.DefineVar("global_find_num", &values.find_num);
    runtime.m_parser.DefineVar("global_compare_gap", &values.compare_gap);
    runtime.m_parser.DefineVar("global_circle_cx", &values.circle_cx);
    runtime.m_parser.DefineVar("global_circle_cy", &values.circle_cy);
    runtime.m_parser.DefineVar("global_circle_px", &values.circle_px);
    runtime.m_parser.DefineVar("global_circle_py", &values.circle_py);
    runtime.m_parser.DefineVar("global_max_elapsed_ms", &values.max_elapsed_ms);
    runtime.m_parser.DefineVar("global_max_scan_lines", &values.max_scan_lines);
    runtime.m_parser.DefineVar("global_max_samples", &values.max_samples);
    runtime.m_parser.DefineVar("global_line_ref", &values.line_ref);
    runtime.m_parser.DefineVar("global_circle_ref", &values.circle_ref);

    if (options.contract_context_enabled)
    {
        values.contract_pass = static_cast<double>(options.contract_pass_initial);
        values.headless_ok = static_cast<double>(options.contract_headless_ok);
        values.algorithm_executed = static_cast<double>(options.contract_algorithm_executed);
        values.budget_exceeded = static_cast<double>(options.contract_budget_exceeded);
        values.valid_points_count = static_cast<double>(options.valid_points_count);
        values.has_fit_line = static_cast<double>(options.has_fit_line);
        values.has_fit_circle = static_cast<double>(options.has_fit_circle);
        values.rendered_measure_points_count = static_cast<double>(options.contract_rendered_measure_points_count);
        values.rendered_result_count = static_cast<double>(options.contract_rendered_result_count);
        values.result_overlay_changed_pixels = static_cast<double>(options.contract_result_overlay_changed_pixels);
        values.policy_guard_match = static_cast<double>(options.policy_guard_match);
        values.contract_circle_radius = options.circle_radius;
        values.contract_avgdist = options.avgdist;

        runtime.m_parser.DefineVar("global_contract_pass", &values.contract_pass);
        runtime.m_parser.DefineVar("global_headless_ok", &values.headless_ok);
        runtime.m_parser.DefineVar("global_algorithm_executed", &values.algorithm_executed);
        runtime.m_parser.DefineVar("global_budget_exceeded", &values.budget_exceeded);
        runtime.m_parser.DefineVar("global_valid_points_count", &values.valid_points_count);
        runtime.m_parser.DefineVar("global_has_fit_line", &values.has_fit_line);
        runtime.m_parser.DefineVar("global_has_fit_circle", &values.has_fit_circle);
        runtime.m_parser.DefineVar("global_rendered_measure_points_count", &values.rendered_measure_points_count);
        runtime.m_parser.DefineVar("global_rendered_result_count", &values.rendered_result_count);
        runtime.m_parser.DefineVar("global_result_overlay_changed_pixels", &values.result_overlay_changed_pixels);
        runtime.m_parser.DefineVar("global_policy_guard_match", &values.policy_guard_match);
        runtime.m_parser.DefineVar("global_contract_circle_radius", &values.contract_circle_radius);
        runtime.m_parser.DefineVar("global_contract_avgdist", &values.contract_avgdist);
    }

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
    runtime.SetVarFactory();

    CxScriptInjectedGlobals injected_globals;
    if (!InjectCxScriptGlobals(runtime, options, injected_globals, reason))
        return false;

    if (!CreateCxScriptInputImage(runtime, source_image, reason))
        return false;

    const std::string script_source =
        LoadCxScriptSource(options.script_path, reason);

    if (script_source.empty())
        return false;

    std::map<std::string, double> script_locals;
    DefineCxScriptLocalVariables(runtime, script_source, script_locals);

    const std::string prepared =
        PrepareCxScriptRuntimeSource(script_source, options.contract_context_enabled);

    if (!runtime.CompileCollectedScript(prepared, reason))
    {
        const std::string parser_diagnostic = parser_output.str();
        if (!parser_diagnostic.empty())
            reason += " | parser: " + parser_diagnostic;
        capture.failure_stage = "script_compile";
        return false;
    }

    capture.script_compiled = true;

    if (!runtime.RunCollectedScript(reason))
    {
        const std::string parser_diagnostic = parser_output.str();
        if (!parser_diagnostic.empty())
            reason += " | parser: " + parser_diagnostic;
        capture.failure_stage = "script_execution";
        return false;
    }

    if (options.contract_context_enabled)
    {
        capture.contract_context = true;
        capture.contract_pass = injected_globals.contract_pass != 0.0;
        capture.contract_status = capture.contract_pass ? "contract_passed" : "contract_failed";
        capture.contract_conclusion = capture.contract_pass
            ? "CxScript contract conditions passed"
            : "CxScript contract conditions failed";
    }
    else try
    {
        if (!CaptureRuntimeToolResults(runtime, capture, reason))
        {
            capture.failure_stage = "runtime_result_capture";
            return false;
        }
    }
    catch (...)
    {
        reason = "CaptureRuntimeToolResults crashed";
        capture.failure_stage = "runtime_result_capture_crash";
        return false;
    }

    if (options.runtime_capture_smoke)
    {
        if (!ValidateCxScriptRuntimeCaptureSmoke(runtime, capture, reason))
        {
            capture.failure_stage = "runtime_capture_smoke";
            return false;
        }
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
    file << "  \"reason\": \"" << JsonEscape(capture.reason) << "\""
         << (capture.contract_context ? "," : "") << "\n";
    if (capture.contract_context)
    {
        file << "  \"contract_pass\": " << (capture.contract_pass ? "true" : "false") << ",\n";
        file << "  \"contract_status\": \"" << JsonEscape(capture.contract_status) << "\",\n";
        file << "  \"contract_conclusion\": \"" << JsonEscape(capture.contract_conclusion) << "\"\n";
    }
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
        else if (arg == "--runtime-capture-smoke")
            options.runtime_capture_smoke = true;
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

    cv::Mat result_overlay;
    cv::Mat evidence_overlay;
    cv::Mat tool_display;

    CxOverlayRenderResult render_result;

    if (RenderCxShapeOverlay(source_image, capture.shapes, CxOverlayLayer::RESULT, result_overlay, render_result))
    {
        std::filesystem::create_directories(result_overlay_path.parent_path());
        capture.result_overlay_changed_pixels = render_result.changed_pixel_count;
        capture.rendered_measure_points_count = render_result.rendered_measure_points_count;
        capture.rendered_result_count = render_result.rendered_result_count;
        if (cv::imwrite(result_overlay_path.string(), result_overlay))
            result.result_overlay_path = result_overlay_path.string();
    }

    if (RenderCxShapeOverlay(source_image, capture.shapes, CxOverlayLayer::EVIDENCE, evidence_overlay, render_result))
    {
        std::filesystem::create_directories(evidence_overlay_path.parent_path());
        if (cv::imwrite(evidence_overlay_path.string(), evidence_overlay))
            result.evidence_overlay_path = evidence_overlay_path.string();
    }

    if (RenderCxShapeOverlay(source_image, capture.shapes, CxOverlayLayer::TOOL_DISPLAY, tool_display, render_result))
    {
        std::filesystem::create_directories(tool_display_path.parent_path());
        if (cv::imwrite(tool_display_path.string(), tool_display))
            result.tool_display_path = tool_display_path.string();
    }

    // Rendering facts are part of the contract input, so persist the summary
    // only after all overlay passes have updated the capture.
    if (SaveCxScriptHeadlessSummaryJson(capture, summary_path, reason))
    {
        result.summary_path = summary_path.string();
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

    if (options.runtime_capture_smoke)
    {
        std::filesystem::path smoke_path = output_dir / "runtime_capture_smoke.json";
        std::ofstream smoke_file(smoke_path);
        if (smoke_file.is_open())
        {
            smoke_file << "{\n";
            smoke_file << "  \"pass\": " << (capture.smoke_pass ? "true" : "false") << ",\n";
            smoke_file << "  \"execution_mode\": \"sequential\",\n";
            smoke_file << "  \"algorithm_scope\": \"geometry_capture_only\",\n";
            smoke_file << "  \"findline_object_name\": \"" << capture.smoke_findline_object_name << "\",\n";
            smoke_file << "  \"findline_roi\": " << (capture.smoke_findline_roi ? "true" : "false") << ",\n";
            smoke_file << "  \"findline_scan\": " << (capture.smoke_findline_scan ? "true" : "false") << ",\n";
            smoke_file << "  \"findcircle_object_name\": \"" << capture.smoke_findcircle_object_name << "\",\n";
            smoke_file << "  \"findcircle_roi_shape_kind\": \"" << capture.smoke_findcircle_roi_shape_kind << "\",\n";
            smoke_file << "  \"findcircle_roi_radius\": " << capture.smoke_findcircle_roi_radius << ",\n";
            smoke_file << "  \"findcircle_outer_scan_radius\": " << capture.smoke_findcircle_outer_scan_radius << ",\n";
            smoke_file << "  \"reason\": \"" << capture.reason << "\"\n";
            smoke_file << "}\n";
            smoke_file.close();
        }
    }

    std::ofstream overlay_validation_file(overlay_validation_path);
    if (overlay_validation_file.is_open())
    {
        const bool source_equals_result_overlay =
            capture.result_overlay_changed_pixels == 0;
        const bool fit_geometry_matches_overlay =
            (!capture.has_fit_line && !capture.has_fit_circle) ||
            capture.rendered_result_count > 0;
        const bool point_geometry_matches_overlay =
            capture.valid_points_count <= 0 ||
            capture.rendered_measure_points_count > 0;
        const bool summary_geometry_matches_overlay =
            fit_geometry_matches_overlay && point_geometry_matches_overlay;

        overlay_validation_file << "{\n";
        overlay_validation_file << "  \"execution_mode\": \"sequential\",\n";
        overlay_validation_file << "  \"source_equals_result_overlay\": "
                                << (source_equals_result_overlay ? "true" : "false") << ",\n";
        overlay_validation_file << "  \"changed_pixel_count\": " << capture.result_overlay_changed_pixels << ",\n";
        overlay_validation_file << "  \"rendered_roi_count\": " << capture.rendered_roi_count << ",\n";
        overlay_validation_file << "  \"rendered_scan_count\": " << capture.rendered_scan_count << ",\n";
        overlay_validation_file << "  \"rendered_measure_points_count\": " << capture.rendered_measure_points_count << ",\n";
        overlay_validation_file << "  \"rendered_result_count\": " << capture.rendered_result_count << ",\n";
        overlay_validation_file << "  \"summary_geometry_matches_overlay\": "
                                << (summary_geometry_matches_overlay ? "true" : "false") << "\n";
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
    bool evidence_ok = options.contract_context_enabled ||
        (!result.evidence_overlay_path.empty() && capture.rendered_roi_count > 0);
    bool result_ok = !result.result_overlay_path.empty();
    bool tool_display_ok = !result.tool_display_path.empty();

    result.assets_complete = options.contract_context_enabled
        ? (snapshot_ok && summary_ok)
        : (snapshot_ok && summary_ok && evidence_ok && result_ok && tool_display_ok);
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
