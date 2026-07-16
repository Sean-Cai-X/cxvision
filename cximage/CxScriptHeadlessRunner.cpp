#include "pch.h"
#include "CxScriptHeadlessRunner.h"
#include "ImageManager.h"
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
#include <cctype>
#include <cstring>
#include <iterator>
#include <map>
#include <vector>
#ifdef _WIN32
#include <Windows.h>
#endif

static const char* kHeadlessGlobalInitScript =
    "cxparser/cxscript/module/cximage/headless/headless_globals.cxsc";

std::string PrepareCxScriptRuntimeSource(const std::string& source, bool contract_context)
{
    std::istringstream input(source);
    std::ostringstream normalized;
    std::string line;
    while (std::getline(input, line))
    {
        if (contract_context &&
            (line.find("global_contract_status") != std::string::npos ||
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

    return runtime_source;
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

using CxScriptGlobalStorage = std::map<std::string, double>;

std::vector<std::string> ExtractCxScriptGlobalDeclarations(const std::string& source)
{
    std::vector<std::string> names;
    std::istringstream input(source);
    std::string line;
    while (std::getline(input, line))
    {
        const size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos)
            continue;

        static const char* declaration_types[] = { "int ", "double ", "float " };
        bool declaration = false;
        for (const char* type : declaration_types)
        {
            const size_t length = std::strlen(type);
            if (line.compare(first, length, type) == 0)
            {
                declaration = true;
                break;
            }
        }
        if (!declaration)
            continue;

        size_t name_begin = line.find("global_", first);
        if (name_begin == std::string::npos)
            continue;

        size_t name_end = name_begin;
        while (name_end < line.size() &&
               (std::isalnum(static_cast<unsigned char>(line[name_end])) || line[name_end] == '_'))
        {
            ++name_end;
        }
        if (name_end > name_begin)
            names.emplace_back(line.substr(name_begin, name_end - name_begin));
    }
    return names;
}

std::string BuildCxScriptGlobalInitRuntimeSource(const std::string& source)
{
    std::ostringstream runtime_source;
    std::istringstream input(source);
    std::string line;
    while (std::getline(input, line))
    {
        const size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos)
            continue;

        static const char* declaration_types[] = { "int ", "double ", "float " };
        bool declaration = false;
        for (const char* type : declaration_types)
        {
            const size_t length = std::strlen(type);
            if (line.compare(first, length, type) == 0)
            {
                declaration = true;
                break;
            }
        }
        if (!declaration)
            runtime_source << line << '\n';
    }

    std::string prepared = runtime_source.str();
    if (prepared.find_first_not_of(" \t\r\n") == std::string::npos)
        prepared = "global_strategy_id = global_strategy_id;\n";
    return PrepareCxScriptRuntimeSource(prepared, false);
}

bool SetCxScriptGlobalValue(
    CxScriptGlobalStorage& storage,
    const std::string& name,
    double value,
    std::string& reason)
{
    auto found = storage.find(name);
    if (found == storage.end())
    {
        reason = "headless global init missing declaration: " + name;
        return false;
    }
    found->second = value;
    return true;
}

double GetCxScriptGlobalValue(
    const CxScriptGlobalStorage& storage,
    const std::string& name,
    double fallback = 0.0)
{
    auto found = storage.find(name);
    return found == storage.end() ? fallback : found->second;
}

bool InjectCxScriptGlobals(
    mu::CxParserRuntime& runtime,
    const CxScriptHeadlessOptions& options,
    CxScriptGlobalStorage& values,
    std::string& reason)
{
    const std::string global_init_source =
        LoadCxScriptSource(kHeadlessGlobalInitScript, reason);
    if (global_init_source.empty())
        return false;

    const std::vector<std::string> global_names =
        ExtractCxScriptGlobalDeclarations(global_init_source);
    if (global_names.empty())
    {
        reason = "headless global init has no global_ declarations: " +
            std::string(kHeadlessGlobalInitScript);
        return false;
    }

    for (const std::string& name : global_names)
    {
        auto inserted = values.emplace(name, 0.0);
        if (inserted.second)
            runtime.m_parser.DefineVar(name, &inserted.first->second);
    }

    const std::string prepared_global_init =
        BuildCxScriptGlobalInitRuntimeSource(global_init_source);
    if (!runtime.Compile(prepared_global_init.c_str()))
    {
        reason = "cannot compile headless global init script: " +
            std::string(kHeadlessGlobalInitScript);
        return false;
    }

    const std::map<std::string, double> option_globals = {
        { "global_roi_x0", static_cast<double>(options.roi_x0) },
        { "global_roi_y0", static_cast<double>(options.roi_y0) },
        { "global_roi_x1", static_cast<double>(options.roi_x1) },
        { "global_roi_y1", static_cast<double>(options.roi_y1) },
        { "global_tool_half_width", static_cast<double>(options.tool_half_width) },
        { "global_wgap", static_cast<double>(options.wgap) },
        { "global_hgap", static_cast<double>(options.hgap) },
        { "global_gap", static_cast<double>(options.gap) },
        { "global_linegap", static_cast<double>(options.linegap) },
        { "global_threshold", static_cast<double>(options.threshold) },
        { "global_method", static_cast<double>(options.method) },
        { "global_filterprofile", static_cast<double>(options.filterprofile) },
        { "global_samplerate", static_cast<double>(options.samplerate) },
        { "global_min_score", options.min_score },
        { "global_find_num", static_cast<double>(options.find_num) },
        { "global_compare_gap", static_cast<double>(options.compare_gap) },
        { "global_learn_roi_x", static_cast<double>(options.learn_roi_x) },
        { "global_learn_roi_y", static_cast<double>(options.learn_roi_y) },
        { "global_learn_roi_w", static_cast<double>(options.learn_roi_w) },
        { "global_learn_roi_h", static_cast<double>(options.learn_roi_h) },
        { "global_search_roi_x", static_cast<double>(options.search_roi_x) },
        { "global_search_roi_y", static_cast<double>(options.search_roi_y) },
        { "global_search_roi_w", static_cast<double>(options.search_roi_w) },
        { "global_search_roi_h", static_cast<double>(options.search_roi_h) },
        { "global_expected_rect_x", static_cast<double>(options.expected_rect_x) },
        { "global_expected_rect_y", static_cast<double>(options.expected_rect_y) },
        { "global_expected_rect_w", static_cast<double>(options.expected_rect_w) },
        { "global_expected_rect_h", static_cast<double>(options.expected_rect_h) },
        { "global_circle_cx", static_cast<double>(options.circle_cx) },
        { "global_circle_cy", static_cast<double>(options.circle_cy) },
        { "global_circle_px", static_cast<double>(options.circle_px) },
        { "global_circle_py", static_cast<double>(options.circle_py) },
        { "global_max_elapsed_ms", static_cast<double>(options.max_elapsed_ms) },
        { "global_max_scan_lines", static_cast<double>(options.max_scan_lines) },
        { "global_max_samples", static_cast<double>(options.max_samples) },
        { "global_strategy_id", static_cast<double>(options.strategy_id) },
        { "global_algorithm_executed", static_cast<double>(options.algorithm_executed) },
        { "global_selected_method", static_cast<double>(options.method) },
        { "global_selected_threshold", static_cast<double>(options.threshold) },
        { "global_selected_wgap", static_cast<double>(options.wgap) },
        { "global_selected_hgap", static_cast<double>(options.hgap) },
        { "global_selected_linegap", static_cast<double>(options.linegap) },
        { "global_selected_filterprofile", static_cast<double>(options.filterprofile) },
    };

    for (const auto& item : option_globals)
    {
        if (!SetCxScriptGlobalValue(values, item.first, item.second, reason))
            return false;
    }

    if (options.contract_context_enabled)
    {
        const std::map<std::string, double> contract_globals = {
            { "global_contract_pass", static_cast<double>(options.contract_pass_initial) },
            { "global_headless_ok", static_cast<double>(options.contract_headless_ok) },
            { "global_algorithm_executed", static_cast<double>(options.contract_algorithm_executed) },
            { "global_budget_exceeded", static_cast<double>(options.contract_budget_exceeded) },
            { "global_valid_points_count", static_cast<double>(options.valid_points_count) },
            { "global_has_fit_line", static_cast<double>(options.has_fit_line) },
            { "global_has_fit_circle", static_cast<double>(options.has_fit_circle) },
            { "global_rendered_measure_points_count", static_cast<double>(options.contract_rendered_measure_points_count) },
            { "global_rendered_result_count", static_cast<double>(options.contract_rendered_result_count) },
            { "global_result_overlay_changed_pixels", static_cast<double>(options.contract_result_overlay_changed_pixels) },
            { "global_policy_guard_match", static_cast<double>(options.policy_guard_match) },
            { "global_circle_radius", options.circle_radius },
            { "global_avgdist", options.avgdist },
        };

        for (const auto& item : contract_globals)
        {
            if (!SetCxScriptGlobalValue(values, item.first, item.second, reason))
                return false;
        }
    }

    reason.clear();
    return true;
}

bool InjectCxScriptInputImage(
    mu::CxParserRuntime& runtime,
    const cv::Mat& source_image,
    const std::string& object_name,
    std::string& reason)
{
    Image* inputObject = static_cast<Image*>(
        runtime.GetClassObj("Image", object_name));

    if (inputObject == nullptr)
    {
        reason = "Image " + object_name +
            " is unavailable; declare it in " + std::string(kHeadlessGlobalInitScript);
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

    if (!ImageManager::EnsureAlgorithmRuntimeResources(source_image.cols, source_image.rows))
    {
        capture.failure_stage = "cximage_runtime_resources";
        reason = "failed to initialize shared cximage algorithm runtime resources";
        return false;
    }

    mu::CxParserRuntime runtime;

    std::ostringstream parser_output;
    runtime.SetStream(&parser_output);

    runtime.ParserInitialClassFunction(0);
    runtime.SetVarFactory();

    CxScriptGlobalStorage global_values;
    if (!InjectCxScriptGlobals(runtime, options, global_values, reason))
        return false;

    if (!InjectCxScriptInputImage(runtime, source_image, "global_matInput", reason))
        return false;

    if (!options.template_image_path.empty())
    {
        cv::Mat template_image = cv::imread(options.template_image_path, cv::IMREAD_COLOR);
        if (template_image.empty())
        {
            reason = "cannot read template image: " + options.template_image_path;
            capture.failure_stage = "template_image";
            return false;
        }

        if (!InjectCxScriptInputImage(runtime, template_image, "global_templateInput", reason))
            return false;
    }

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

    capture.strategy_id = static_cast<int>(GetCxScriptGlobalValue(global_values, "global_strategy_id"));
    capture.selected_method = static_cast<int>(GetCxScriptGlobalValue(global_values, "global_selected_method"));
    capture.selected_threshold = static_cast<int>(GetCxScriptGlobalValue(global_values, "global_selected_threshold"));
    capture.selected_wgap = static_cast<int>(GetCxScriptGlobalValue(global_values, "global_selected_wgap"));
    capture.selected_hgap = static_cast<int>(GetCxScriptGlobalValue(global_values, "global_selected_hgap"));
    capture.selected_linegap = static_cast<int>(GetCxScriptGlobalValue(global_values, "global_selected_linegap"));
    capture.selected_filterprofile = static_cast<int>(GetCxScriptGlobalValue(global_values, "global_selected_filterprofile"));

    if (options.contract_context_enabled)
    {
        capture.contract_context = true;
        capture.contract_pass = GetCxScriptGlobalValue(global_values, "global_contract_pass") != 0.0;
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

    capture.runtime_globals = global_values;

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
    std::error_code ec;
    std::filesystem::create_directories(outputPath.parent_path(), ec);
    if (ec)
    {
        outReason = "failed to create headless summary directory: " + ec.message();
        return false;
    }
    const std::filesystem::path temporary = outputPath.string() + ".tmp";
    std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
    if (!file.is_open())
    {
        outReason = "failed to open headless summary json";
        return false;
    }

    file << "{\n";
    file << "  \"execution_mode\": \"sequential\",\n";
    file << "  \"algorithm_executed\": " << (capture.runtime_completed ? "true" : "false") << ",\n";
    file << "  \"elapsed_ms\": " << capture.elapsed_ms << ",\n";
    file << "  \"budget_ms\": " << capture.budget_ms << ",\n";
    file << "  \"max_steps\": " << capture.max_steps << ",\n";
    file << "  \"max_scan_lines\": " << capture.max_scan_lines << ",\n";
    file << "  \"max_samples\": " << capture.max_samples << ",\n";
    file << "  \"strategy_id\": " << capture.strategy_id << ",\n";
    file << "  \"selected_method\": " << capture.selected_method << ",\n";
    file << "  \"selected_threshold\": " << capture.selected_threshold << ",\n";
    file << "  \"selected_wgap\": " << capture.selected_wgap << ",\n";
    file << "  \"selected_hgap\": " << capture.selected_hgap << ",\n";
    file << "  \"selected_linegap\": " << capture.selected_linegap << ",\n";
    file << "  \"selected_filterprofile\": " << capture.selected_filterprofile << ",\n";
    file << "  \"budget_exceeded\": " << (capture.budget_exceeded ? "true" : "false") << ",\n";
    file << "  \"scan_line_count\": " << capture.scan_line_count << ",\n";
    file << "  \"sample_count\": " << capture.sample_count << ",\n";
    file << "  \"valid_points_count\": " << capture.valid_points_count << ",\n";
    file << "  \"has_fit_line\": " << (capture.has_fit_line ? "true" : "false") << ",\n";
    file << "  \"has_fit_circle\": " << (capture.has_fit_circle ? "true" : "false") << ",\n";
    file << "  \"has_fit_ellipse\": " << (capture.has_fit_ellipse ? "true" : "false") << ",\n";
    file << "  \"has_result_rect\": " << (capture.has_result_rect ? "true" : "false") << ",\n";
    file << "  \"circle_radius\": " << capture.circle_radius << ",\n";
    file << "  \"avgdist\": " << capture.avgdist << ",\n";
    file << "  \"result_rect_count\": " << capture.result_rect_count << ",\n";
    file << "  \"model_point_count\": " << capture.model_point_count << ",\n";
    file << "  \"fastmatch_model_width\": " << capture.fastmatch_model_width << ",\n";
    file << "  \"fastmatch_model_height\": " << capture.fastmatch_model_height << ",\n";
    file << "  \"fastmatch_pattern_a_count\": " << capture.fastmatch_pattern_a_count << ",\n";
    file << "  \"fastmatch_pattern_b_count\": " << capture.fastmatch_pattern_b_count << ",\n";
    file << "  \"fastmatch_pattern_a_x\": " << capture.fastmatch_pattern_a_x << ",\n";
    file << "  \"fastmatch_pattern_a_y\": " << capture.fastmatch_pattern_a_y << ",\n";
    file << "  \"fastmatch_pattern_a_width\": " << capture.fastmatch_pattern_a_width << ",\n";
    file << "  \"fastmatch_pattern_a_height\": " << capture.fastmatch_pattern_a_height << ",\n";
    file << "  \"fastmatch_pattern_b_x\": " << capture.fastmatch_pattern_b_x << ",\n";
    file << "  \"fastmatch_pattern_b_y\": " << capture.fastmatch_pattern_b_y << ",\n";
    file << "  \"fastmatch_pattern_b_width\": " << capture.fastmatch_pattern_b_width << ",\n";
    file << "  \"fastmatch_pattern_b_height\": " << capture.fastmatch_pattern_b_height << ",\n";
    file << "  \"candidate_count\": " << capture.candidate_count << ",\n";
    file << "  \"best_score\": " << capture.best_score << ",\n";
    file << "  \"has_result_box\": " << (capture.has_result_box ? "true" : "false") << ",\n";
    file << "  \"has_best_result\": " << (capture.has_best_result ? "true" : "false") << ",\n";
    file << "  \"fastmatch_match_call_count\": " << capture.fastmatch_match_call_count << ",\n";
    file << "  \"fastmatch_match_ab_call_count\": " << capture.fastmatch_match_ab_call_count << ",\n";
    file << "  \"fastmatch_match_sample_ab_call_count\": " << capture.fastmatch_match_sample_ab_call_count << ",\n";
    file << "  \"fastmatch_match_last_stage\": " << capture.fastmatch_match_last_stage << ",\n";
    file << "  \"fastmatch_match_image_width\": " << capture.fastmatch_match_image_width << ",\n";
    file << "  \"fastmatch_match_image_height\": " << capture.fastmatch_match_image_height << ",\n";
    file << "  \"fastmatch_match_rect_x0\": " << capture.fastmatch_match_rect_x0 << ",\n";
    file << "  \"fastmatch_match_rect_y0\": " << capture.fastmatch_match_rect_y0 << ",\n";
    file << "  \"fastmatch_match_rect_x1\": " << capture.fastmatch_match_rect_x1 << ",\n";
    file << "  \"fastmatch_match_rect_y1\": " << capture.fastmatch_match_rect_y1 << ",\n";
    file << "  \"fastmatch_raw_probe_count\": " << capture.fastmatch_raw_probe_count << ",\n";
    file << "  \"fastmatch_raw_threshold_hit_count\": " << capture.fastmatch_raw_threshold_hit_count << ",\n";
    file << "  \"fastmatch_result_to_list_count\": " << capture.fastmatch_result_to_list_count << ",\n";
    file << "  \"fastmatch_candidate_insert_count\": " << capture.fastmatch_candidate_insert_count << ",\n";
    file << "  \"fastmatch_candidate_replace_count\": " << capture.fastmatch_candidate_replace_count << ",\n";
    file << "  \"fastmatch_candidate_reject_count\": " << capture.fastmatch_candidate_reject_count << ",\n";
    file << "  \"object_prefilter_requested\": " << (capture.object_prefilter_requested ? "true" : "false") << ",\n";
    file << "  \"object_prefilter_applied\": " << (capture.object_prefilter_applied ? "true" : "false") << ",\n";
    file << "  \"object_filter_borw\": " << capture.object_filter_borw << ",\n";
    file << "  \"object_filter_min\": " << capture.object_filter_min << ",\n";
    file << "  \"object_filter_max\": " << capture.object_filter_max << ",\n";
    file << "  \"fit_filter_input_count\": " << capture.fit_filter_input_count << ",\n";
    file << "  \"fit_filter_kept_count\": " << capture.fit_filter_kept_count << ",\n";
    file << "  \"fit_filter_rejected_count\": " << capture.fit_filter_rejected_count << ",\n";
    file << "  \"fit_filter_sigma\": " << capture.fit_filter_sigma << ",\n";
    file << "  \"fit_filter_threshold\": " << capture.fit_filter_threshold << ",\n";
    file << "  \"findrect_seed_valid\": " << (capture.findrect_seed_valid ? "true" : "false") << ",\n";
    file << "  \"findrect_top_valid\": " << (capture.findrect_top_valid ? "true" : "false") << ",\n";
    file << "  \"findrect_bottom_valid\": " << (capture.findrect_bottom_valid ? "true" : "false") << ",\n";
    file << "  \"findrect_left_valid\": " << (capture.findrect_left_valid ? "true" : "false") << ",\n";
    file << "  \"findrect_right_valid\": " << (capture.findrect_right_valid ? "true" : "false") << ",\n";
    file << "  \"findrect_top_points\": " << capture.findrect_top_points << ",\n";
    file << "  \"findrect_bottom_points\": " << capture.findrect_bottom_points << ",\n";
    file << "  \"findrect_left_points\": " << capture.findrect_left_points << ",\n";
    file << "  \"findrect_right_points\": " << capture.findrect_right_points << ",\n";
    file << "  \"findrect_coarse_score\": " << capture.findrect_coarse_score << ",\n";
    file << "  \"findrect_refine_score\": " << capture.findrect_refine_score << ",\n";
    file << "  \"segmentation_status_code\": " << capture.segmentation_status_code << ",\n";
    file << "  \"segmentation_contour_count\": " << capture.segmentation_contour_count << ",\n";
    file << "  \"segmentation_primary_area\": " << capture.segmentation_primary_area << ",\n";
    file << "  \"segmentation_result_ref\": \"" << JsonEscape(capture.segmentation_result_ref) << "\",\n";
    file << "  \"segmentation_mask_ref\": \"" << JsonEscape(capture.segmentation_mask_ref) << "\",\n";
    file << "  \"segmentation_contour_ref\": \"" << JsonEscape(capture.segmentation_contour_ref) << "\",\n";
    file << "  \"segmentation_overlay_ref\": \"" << JsonEscape(capture.segmentation_overlay_ref) << "\",\n";
    file << "  \"rendered_measure_points_count\": " << capture.rendered_measure_points_count << ",\n";
    file << "  \"rendered_result_count\": " << capture.rendered_result_count << ",\n";
    file << "  \"result_overlay_changed_pixels\": " << capture.result_overlay_changed_pixels << ",\n";
    file << "  \"runtime_globals\": {\n";
    for (auto it = capture.runtime_globals.begin(); it != capture.runtime_globals.end(); ++it)
    {
        const auto next = std::next(it);
        file << "    \"" << JsonEscape(it->first) << "\": " << it->second
             << (next == capture.runtime_globals.end() ? "" : ",") << "\n";
    }
    file << "  },\n";
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

    file.flush();
    const bool write_ok = file.good();
    file.close();
    if (!write_ok)
    {
        std::filesystem::remove(temporary, ec);
        outReason = "failed while writing headless summary json";
        return false;
    }
#ifdef _WIN32
    if (!MoveFileExW(
            temporary.c_str(), outputPath.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        std::filesystem::remove(temporary, ec);
        outReason = "failed to atomically replace headless summary json";
        return false;
    }
#else
    std::filesystem::rename(temporary, outputPath, ec);
    if (ec)
    {
        std::filesystem::remove(temporary, ec);
        outReason = "failed to atomically replace headless summary json";
        return false;
    }
#endif

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
        else if (arg == "--template-image" && i + 1 < argc)
            options.template_image_path = argv[++i];
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
        else if (arg == "--roi-x0" && i + 1 < argc)
            options.roi_x0 = std::stoi(argv[++i]);
        else if (arg == "--roi-y0" && i + 1 < argc)
            options.roi_y0 = std::stoi(argv[++i]);
        else if (arg == "--roi-x1" && i + 1 < argc)
            options.roi_x1 = std::stoi(argv[++i]);
        else if (arg == "--roi-y1" && i + 1 < argc)
            options.roi_y1 = std::stoi(argv[++i]);
        else if (arg == "--circle-cx" && i + 1 < argc)
            options.circle_cx = std::stoi(argv[++i]);
        else if (arg == "--circle-cy" && i + 1 < argc)
            options.circle_cy = std::stoi(argv[++i]);
        else if (arg == "--circle-px" && i + 1 < argc)
            options.circle_px = std::stoi(argv[++i]);
        else if (arg == "--circle-py" && i + 1 < argc)
            options.circle_py = std::stoi(argv[++i]);
        else if (arg == "--tool-half-width" && i + 1 < argc)
            options.tool_half_width = std::stoi(argv[++i]);
        else if (arg == "--wgap" && i + 1 < argc)
            options.wgap = std::stoi(argv[++i]);
        else if (arg == "--hgap" && i + 1 < argc)
            options.hgap = std::stoi(argv[++i]);
        else if (arg == "--gap" && i + 1 < argc)
            options.gap = std::stoi(argv[++i]);
        else if (arg == "--linegap" && i + 1 < argc)
            options.linegap = std::stoi(argv[++i]);
        else if (arg == "--threshold" && i + 1 < argc)
            options.threshold = std::stoi(argv[++i]);
        else if (arg == "--method" && i + 1 < argc)
            options.method = std::stoi(argv[++i]);
        else if (arg == "--filterprofile" && i + 1 < argc)
            options.filterprofile = std::stoi(argv[++i]);
        else if (arg == "--samplerate" && i + 1 < argc)
            options.samplerate = std::stoi(argv[++i]);
        else if (arg == "--min-score" && i + 1 < argc)
            options.min_score = std::stod(argv[++i]);
        else if (arg == "--find-num" && i + 1 < argc)
            options.find_num = std::stoi(argv[++i]);
        else if (arg == "--compare-gap" && i + 1 < argc)
            options.compare_gap = std::stoi(argv[++i]);
        else if (arg == "--strategy-id" && i + 1 < argc)
            options.strategy_id = std::stoi(argv[++i]);
        else if (arg == "--max-elapsed-ms" && i + 1 < argc)
            options.max_elapsed_ms = std::stoi(argv[++i]);
        else if (arg == "--max-scan-lines" && i + 1 < argc)
            options.max_scan_lines = std::stoi(argv[++i]);
        else if (arg == "--max-samples" && i + 1 < argc)
            options.max_samples = std::stoi(argv[++i]);
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

    if (!options.template_image_path.empty())
    {
        std::filesystem::path template_path(options.template_image_path);
        if (!std::filesystem::exists(template_path))
        {
            result.reason = "template image not found: " + template_path.string();
            result.failure_stage = "template_image";
            return false;
        }
    }

    CxScriptExecutionCapture capture;
    const int timeout_ms = std::max(1, options.timeout_sec) * 1000;
    capture.budget_ms = options.max_elapsed_ms > 0
        ? std::min(options.max_elapsed_ms, timeout_ms)
        : timeout_ms;
    capture.max_steps = options.max_steps;
    capture.max_scan_lines = options.max_scan_lines;
    capture.max_samples = options.max_samples;
    std::string reason;

    CxScriptHeadlessOptions effective_options = options;
    effective_options.max_elapsed_ms = capture.budget_ms;
    bool execution_ok = ExecuteCxScriptSequential(
        effective_options,
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
        if (!options.template_image_path.empty())
            snapshot_file << "template_image: " << options.template_image_path << "\n";
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
    else
    {
        result.failure_stage = "summary_export";
        result.reason = reason;
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
        variable_snapshot_file << "  \"strategy_id\": " << capture.strategy_id << ",\n";
        variable_snapshot_file << "  \"threshold\": " << options.threshold << ",\n";
        variable_snapshot_file << "  \"method\": " << options.method << ",\n";
        variable_snapshot_file << "  \"wgap\": " << options.wgap << ",\n";
        variable_snapshot_file << "  \"hgap\": " << options.hgap << ",\n";
        variable_snapshot_file << "  \"linegap\": " << options.linegap << ",\n";
        variable_snapshot_file << "  \"selected_threshold\": " << capture.selected_threshold << ",\n";
        variable_snapshot_file << "  \"selected_method\": " << capture.selected_method << ",\n";
        variable_snapshot_file << "  \"selected_wgap\": " << capture.selected_wgap << ",\n";
        variable_snapshot_file << "  \"selected_hgap\": " << capture.selected_hgap << ",\n";
        variable_snapshot_file << "  \"selected_linegap\": " << capture.selected_linegap << ",\n";
        variable_snapshot_file << "  \"selected_filterprofile\": " << capture.selected_filterprofile << "\n";
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
        object_state_file << "  \"has_fit_ellipse\": " << (capture.has_fit_ellipse ? "true" : "false") << ",\n";
        object_state_file << "  \"has_result_rect\": " << (capture.has_result_rect ? "true" : "false") << ",\n";
        object_state_file << "  \"circle_radius\": " << capture.circle_radius << ",\n";
        object_state_file << "  \"avgdist\": " << capture.avgdist << ",\n";
        object_state_file << "  \"result_rect_count\": " << capture.result_rect_count << ",\n";
        object_state_file << "  \"model_point_count\": " << capture.model_point_count << ",\n";
        object_state_file << "  \"fastmatch_model_width\": " << capture.fastmatch_model_width << ",\n";
        object_state_file << "  \"fastmatch_model_height\": " << capture.fastmatch_model_height << ",\n";
        object_state_file << "  \"fastmatch_pattern_a_count\": " << capture.fastmatch_pattern_a_count << ",\n";
        object_state_file << "  \"fastmatch_pattern_b_count\": " << capture.fastmatch_pattern_b_count << ",\n";
        object_state_file << "  \"fastmatch_pattern_a_x\": " << capture.fastmatch_pattern_a_x << ",\n";
        object_state_file << "  \"fastmatch_pattern_a_y\": " << capture.fastmatch_pattern_a_y << ",\n";
        object_state_file << "  \"fastmatch_pattern_a_width\": " << capture.fastmatch_pattern_a_width << ",\n";
        object_state_file << "  \"fastmatch_pattern_a_height\": " << capture.fastmatch_pattern_a_height << ",\n";
        object_state_file << "  \"fastmatch_pattern_b_x\": " << capture.fastmatch_pattern_b_x << ",\n";
        object_state_file << "  \"fastmatch_pattern_b_y\": " << capture.fastmatch_pattern_b_y << ",\n";
        object_state_file << "  \"fastmatch_pattern_b_width\": " << capture.fastmatch_pattern_b_width << ",\n";
        object_state_file << "  \"fastmatch_pattern_b_height\": " << capture.fastmatch_pattern_b_height << ",\n";
        object_state_file << "  \"candidate_count\": " << capture.candidate_count << ",\n";
        object_state_file << "  \"best_score\": " << capture.best_score << ",\n";
        object_state_file << "  \"has_result_box\": " << (capture.has_result_box ? "true" : "false") << ",\n";
        object_state_file << "  \"has_best_result\": " << (capture.has_best_result ? "true" : "false") << ",\n";
        object_state_file << "  \"fastmatch_match_call_count\": " << capture.fastmatch_match_call_count << ",\n";
        object_state_file << "  \"fastmatch_match_ab_call_count\": " << capture.fastmatch_match_ab_call_count << ",\n";
        object_state_file << "  \"fastmatch_match_sample_ab_call_count\": " << capture.fastmatch_match_sample_ab_call_count << ",\n";
        object_state_file << "  \"fastmatch_match_last_stage\": " << capture.fastmatch_match_last_stage << ",\n";
        object_state_file << "  \"fastmatch_match_image_width\": " << capture.fastmatch_match_image_width << ",\n";
        object_state_file << "  \"fastmatch_match_image_height\": " << capture.fastmatch_match_image_height << ",\n";
        object_state_file << "  \"fastmatch_match_rect_x0\": " << capture.fastmatch_match_rect_x0 << ",\n";
        object_state_file << "  \"fastmatch_match_rect_y0\": " << capture.fastmatch_match_rect_y0 << ",\n";
        object_state_file << "  \"fastmatch_match_rect_x1\": " << capture.fastmatch_match_rect_x1 << ",\n";
        object_state_file << "  \"fastmatch_match_rect_y1\": " << capture.fastmatch_match_rect_y1 << ",\n";
        object_state_file << "  \"fastmatch_raw_probe_count\": " << capture.fastmatch_raw_probe_count << ",\n";
        object_state_file << "  \"fastmatch_raw_threshold_hit_count\": " << capture.fastmatch_raw_threshold_hit_count << ",\n";
        object_state_file << "  \"fastmatch_result_to_list_count\": " << capture.fastmatch_result_to_list_count << ",\n";
        object_state_file << "  \"fastmatch_candidate_insert_count\": " << capture.fastmatch_candidate_insert_count << ",\n";
        object_state_file << "  \"fastmatch_candidate_replace_count\": " << capture.fastmatch_candidate_replace_count << ",\n";
        object_state_file << "  \"fastmatch_candidate_reject_count\": " << capture.fastmatch_candidate_reject_count << ",\n";
        object_state_file << "  \"object_prefilter_requested\": " << (capture.object_prefilter_requested ? "true" : "false") << ",\n";
        object_state_file << "  \"object_prefilter_applied\": " << (capture.object_prefilter_applied ? "true" : "false") << ",\n";
        object_state_file << "  \"object_filter_borw\": " << capture.object_filter_borw << ",\n";
        object_state_file << "  \"object_filter_min\": " << capture.object_filter_min << ",\n";
        object_state_file << "  \"object_filter_max\": " << capture.object_filter_max << ",\n";
        object_state_file << "  \"fit_filter_input_count\": " << capture.fit_filter_input_count << ",\n";
        object_state_file << "  \"fit_filter_kept_count\": " << capture.fit_filter_kept_count << ",\n";
        object_state_file << "  \"fit_filter_rejected_count\": " << capture.fit_filter_rejected_count << ",\n";
        object_state_file << "  \"fit_filter_sigma\": " << capture.fit_filter_sigma << ",\n";
        object_state_file << "  \"fit_filter_threshold\": " << capture.fit_filter_threshold << ",\n";
        object_state_file << "  \"findrect_seed_valid\": " << (capture.findrect_seed_valid ? "true" : "false") << ",\n";
        object_state_file << "  \"findrect_top_valid\": " << (capture.findrect_top_valid ? "true" : "false") << ",\n";
        object_state_file << "  \"findrect_bottom_valid\": " << (capture.findrect_bottom_valid ? "true" : "false") << ",\n";
        object_state_file << "  \"findrect_left_valid\": " << (capture.findrect_left_valid ? "true" : "false") << ",\n";
        object_state_file << "  \"findrect_right_valid\": " << (capture.findrect_right_valid ? "true" : "false") << ",\n";
        object_state_file << "  \"findrect_top_points\": " << capture.findrect_top_points << ",\n";
        object_state_file << "  \"findrect_bottom_points\": " << capture.findrect_bottom_points << ",\n";
        object_state_file << "  \"findrect_left_points\": " << capture.findrect_left_points << ",\n";
        object_state_file << "  \"findrect_right_points\": " << capture.findrect_right_points << ",\n";
        object_state_file << "  \"findrect_coarse_score\": " << capture.findrect_coarse_score << ",\n";
        object_state_file << "  \"findrect_refine_score\": " << capture.findrect_refine_score << ",\n";
        object_state_file << "  \"segmentation_status_code\": " << capture.segmentation_status_code << ",\n";
        object_state_file << "  \"segmentation_contour_count\": " << capture.segmentation_contour_count << ",\n";
        object_state_file << "  \"segmentation_primary_area\": " << capture.segmentation_primary_area << ",\n";
        object_state_file << "  \"segmentation_result_ref\": \"" << JsonEscape(capture.segmentation_result_ref) << "\",\n";
        object_state_file << "  \"segmentation_mask_ref\": \"" << JsonEscape(capture.segmentation_mask_ref) << "\",\n";
        object_state_file << "  \"segmentation_contour_ref\": \"" << JsonEscape(capture.segmentation_contour_ref) << "\",\n";
        object_state_file << "  \"segmentation_overlay_ref\": \"" << JsonEscape(capture.segmentation_overlay_ref) << "\",\n";
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
        log_file << "has_fit_ellipse: " << (capture.has_fit_ellipse ? "true" : "false") << "\n";
        log_file << "has_result_rect: " << (capture.has_result_rect ? "true" : "false") << "\n";
        log_file << "model_point_count: " << capture.model_point_count << "\n";
        log_file << "candidate_count: " << capture.candidate_count << "\n";
        log_file << "best_score: " << capture.best_score << "\n";
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
    result.has_fit_ellipse = capture.has_fit_ellipse;
    result.has_result_rect = capture.has_result_rect;
    result.model_point_count = capture.model_point_count;
    result.candidate_count = capture.candidate_count;
    result.best_score = capture.best_score;
    result.has_result_box = capture.has_result_box;
    result.has_best_result = capture.has_best_result;
    result.circle_radius = capture.circle_radius;
    result.avgdist = capture.avgdist;

    return result.ok;
}

bool RunCxScriptHeadlessCapture(
    const CxScriptHeadlessOptions& options,
    CxScriptExecutionCapture& capture,
    std::string& reason)
{
    capture = CxScriptExecutionCapture{};

    std::filesystem::path script_path(options.script_path);
    if (!std::filesystem::exists(script_path))
    {
        reason = "script not found: " + script_path.string();
        capture.failure_stage = "script";
        return false;
    }

    std::filesystem::path image_path(options.image_path);
    if (!std::filesystem::exists(image_path))
    {
        reason = "image not found: " + image_path.string();
        capture.failure_stage = "image";
        return false;
    }

    cv::Mat source_image = cv::imread(image_path.string(), cv::IMREAD_COLOR);
    if (source_image.empty())
    {
        reason = "cannot read image: " + image_path.string();
        capture.failure_stage = "image";
        return false;
    }

    if (!options.template_image_path.empty())
    {
        std::filesystem::path template_path(options.template_image_path);
        if (!std::filesystem::exists(template_path))
        {
            reason = "template image not found: " + template_path.string();
            capture.failure_stage = "template_image";
            return false;
        }
    }

    const int timeout_ms = std::max(1, options.timeout_sec) * 1000;
    capture.budget_ms = options.max_elapsed_ms > 0
        ? std::min(options.max_elapsed_ms, timeout_ms)
        : timeout_ms;
    capture.max_steps = options.max_steps;
    capture.max_scan_lines = options.max_scan_lines;
    capture.max_samples = options.max_samples;

    CxScriptHeadlessOptions effective_options = options;
    effective_options.max_elapsed_ms = capture.budget_ms;
    const bool execution_ok = ExecuteCxScriptSequential(
        effective_options,
        source_image,
        capture,
        reason);

    if (!execution_ok && capture.failure_stage.empty())
        capture.failure_stage = "script_execution";

    return execution_ok;
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
