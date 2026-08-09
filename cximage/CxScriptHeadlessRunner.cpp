#include "pch.h"
#include "CxScriptHeadlessRunner.h"
#include "ImageManager.h"
#include "ManualConsoleUtils.h"
#include "ParserClass.h"
#include "Image.h"
#include "CxScriptRuntimeResultCapture.h"
#include "CxShapeOverlayRenderer.h"
#include "CxScriptRuntimeCaptureSmoke.h"
#include "CxScriptGlobalValueSet.h"
#include "CxScriptCasePackageWriter.h"
#include "measurement_semantics/CxMeasurementSemanticEvidenceWriter.h"

#include <sstream>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstring>
#include <iterator>
#include <limits>
#include <map>
#include <vector>
#include <cmath>
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
    CxScriptGlobalValueSet global_value_set;

    if (!LoadHeadlessGlobalDeclarations(kHeadlessGlobalInitScript, global_value_set, reason))
        return false;

    if (!BindGlobalValueSetToParser(runtime, global_value_set, reason))
        return false;

    const std::string global_init_source =
        LoadCxScriptSource(kHeadlessGlobalInitScript, reason);
    if (global_init_source.empty())
        return false;

    const std::string prepared_global_init =
        BuildCxScriptGlobalInitRuntimeSource(global_init_source);
    if (!runtime.Compile(prepared_global_init.c_str()))
    {
        reason = "cannot compile headless global init script: " +
            std::string(kHeadlessGlobalInitScript);
        return false;
    }

    std::map<std::string, double> option_globals = BuildHeadlessGlobalOverrides(options);
    if (!ApplyGlobalOverrides(global_value_set, option_globals, reason))
        return false;

    if (!options.globals_path.empty())
    {
        std::map<std::string, double> file_globals;
        if (!LoadHeadlessGlobalValuesFile(options.globals_path, file_globals, reason))
            return false;
        if (!ApplyGlobalOverrides(global_value_set, file_globals, reason))
            return false;
    }

    if (!ApplyGlobalOverrides(global_value_set, options.cli_global_overrides, reason))
        return false;

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
            { "global_runtime_valid_points_count", static_cast<double>(options.runtime_valid_points_count) },
            { "global_runtime_has_fit_line", static_cast<double>(options.runtime_has_fit_line) },
            { "global_runtime_has_fit_circle", static_cast<double>(options.runtime_has_fit_circle) },
            { "global_runtime_global_valid_points_count_mismatch", static_cast<double>(options.runtime_global_valid_points_count_mismatch) },
            { "global_runtime_global_has_fit_line_mismatch", static_cast<double>(options.runtime_global_has_fit_line_mismatch) },
            { "global_runtime_global_has_fit_circle_mismatch", static_cast<double>(options.runtime_global_has_fit_circle_mismatch) },
            { "global_runtime_global_result_mismatch", static_cast<double>(options.runtime_global_result_mismatch) },
            { "global_rendered_measure_points_count", static_cast<double>(options.contract_rendered_measure_points_count) },
            { "global_rendered_result_count", static_cast<double>(options.contract_rendered_result_count) },
            { "global_result_overlay_changed_pixels", static_cast<double>(options.contract_result_overlay_changed_pixels) },
            { "global_policy_guard_match", static_cast<double>(options.policy_guard_match) },
            { "global_circle_radius", options.circle_radius },
            { "global_avgdist", options.avgdist },
        };

        if (!ApplyGlobalOverrides(global_value_set, contract_globals, reason))
            return false;
    }

    values.swap(global_value_set.numbers);

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

bool InjectCxScriptRuntimeStrings(
    mu::CxParserRuntime& runtime,
    const CxScriptHeadlessOptions& options,
    std::string& reason)
{
    try
    {
        const std::string case_id = options.case_name.empty()
            ? options.case_id
            : options.case_name;

        // Read-only values owned by the current serial Headless request.
        // TorchTask therefore uses the same image, case, and artifact root as
        // the surrounding Headless evidence package.
        const char separator = '|';
        const std::string request_context = case_id + separator +
            options.image_path + separator + options.output_dir;
        runtime.DefineStringConstant("global_torch_request_context", request_context);
    }
    catch (const std::exception& e)
    {
        reason = "cannot inject headless torch runtime strings: " +
            std::string(e.what());
        return false;
    }
    catch (...)
    {
        reason = "cannot inject headless torch runtime strings";
        return false;
    }

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

    if (!InjectCxScriptRuntimeStrings(runtime, options, reason))
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

struct CxCircleMeasurePointRadiusStats
{
    int count = 0;
    int inside_inner_count = 0;
    int outside_outer_count = 0;
    double min_radius = 0.0;
    double avg_radius = 0.0;
    double max_radius = 0.0;
};

CxCircleMeasurePointRadiusStats ComputeCircleMeasurePointRadiusStats(
    const std::vector<CxShapeElementSnapshot>& shapes,
    double cx,
    double cy,
    double inner_radius,
    double outer_radius)
{
    CxCircleMeasurePointRadiusStats stats;
    bool has_shape_center = false;
    bool has_shape_inner = false;
    bool has_shape_outer = false;

    for (const auto& shape : shapes)
    {
        if (shape.owner_type != "FindCircle")
            continue;

        if (shape.stable_ref.find(".roi_circle") != std::string::npos &&
            shape.radius > 0.0)
        {
            cx = shape.center_x;
            cy = shape.center_y;
            if (!has_shape_outer && outer_radius <= 0.0)
                outer_radius = shape.radius;
            has_shape_center = true;
        }
        else if (shape.stable_ref.find(".inner_scan_circle") != std::string::npos &&
                 shape.radius > 0.0)
        {
            inner_radius = shape.radius;
            has_shape_inner = true;
        }
        else if (shape.stable_ref.find(".outer_scan_circle") != std::string::npos &&
                 shape.radius > 0.0)
        {
            outer_radius = shape.radius;
            has_shape_outer = true;
        }
    }

    if (!has_shape_center && (cx == 0.0 && cy == 0.0))
        return stats;

    double min_radius = std::numeric_limits<double>::max();
    double max_radius = 0.0;
    double sum_radius = 0.0;

    for (const auto& shape : shapes)
    {
        if (shape.owner_type != "FindCircle" ||
            shape.semantic_role != "measure_points")
        {
            continue;
        }

        for (size_t i = 0; i + 1 < shape.points.size(); i += 2)
        {
            const double dx = shape.points[i] - cx;
            const double dy = shape.points[i + 1] - cy;
            const double radius = std::sqrt(dx * dx + dy * dy);

            min_radius = std::min(min_radius, radius);
            max_radius = std::max(max_radius, radius);
            sum_radius += radius;
            ++stats.count;

            if (inner_radius > 0.0 && radius < inner_radius - 0.5)
                ++stats.inside_inner_count;
            if (outer_radius > 0.0 && radius > outer_radius + 0.5)
                ++stats.outside_outer_count;
        }
    }

    if (stats.count > 0)
    {
        stats.min_radius = min_radius;
        stats.avg_radius = sum_radius / static_cast<double>(stats.count);
        stats.max_radius = max_radius;
    }

    return stats;
}

CxScriptResultPackage BuildCxScriptResultPackage(
    const CxScriptExecutionCapture& capture)
{
    CxScriptResultPackage pkg;

    pkg.runtime_globals_after = capture.runtime_globals;
    pkg.shapes = capture.shapes;

    pkg.status = capture.runtime_completed ? "executed" : "not_executed";
    pkg.failure_stage = capture.failure_stage;
    pkg.reason = capture.reason;

    pkg.metrics["elapsed_ms"] = capture.elapsed_ms;
    pkg.metrics["budget_ms"] = capture.budget_ms;
    pkg.metrics["max_steps"] = capture.max_steps;
    pkg.metrics["max_scan_lines"] = capture.max_scan_lines;
    pkg.metrics["max_samples"] = capture.max_samples;
    pkg.metrics["scan_line_count"] = capture.scan_line_count;
    pkg.metrics["sample_count"] = capture.sample_count;
    pkg.metrics["tool_effective_method"] = capture.tool_method;
    pkg.metrics["tool_effective_threshold"] = capture.tool_threshold;
    pkg.metrics["tool_effective_wgap"] = capture.tool_wgap;
    pkg.metrics["tool_effective_hgap"] = capture.tool_hgap;
    pkg.metrics["tool_effective_linegap"] = capture.tool_linegap;
    pkg.metrics["scan_rows_examined"] = capture.scan_rows_examined;
    pkg.metrics["scan_rows_with_foreground"] = capture.scan_rows_with_foreground;
    pkg.metrics["scan_runs_total"] = capture.scan_runs_total;
    pkg.metrics["scan_runs_within_length_limit"] = capture.scan_runs_within_length_limit;
    pkg.metrics["scan_runs_over_length_limit"] = capture.scan_runs_over_length_limit;
    pkg.metrics["scan_runs_rejected_by_selection"] = capture.scan_runs_rejected_by_selection;
    pkg.metrics["scan_runs_rejected_near_endpoint"] = capture.scan_runs_rejected_near_endpoint;
    pkg.metrics["scan_points_emitted"] = capture.scan_points_emitted;
    pkg.metrics["findline_point_consistency_enabled"] = capture.findline_point_consistency_enabled;
    pkg.metrics["findline_point_consistency_range"] = capture.findline_point_consistency_range;
    pkg.metrics["findline_point_consistency_input_points"] = capture.findline_point_consistency_input_points;
    pkg.metrics["findline_point_consistency_output_points"] = capture.findline_point_consistency_output_points;
    pkg.metrics["findline_point_consistency_removed_points"] = capture.findline_point_consistency_removed_points;
    pkg.metrics["findcircle_point_consistency_enabled"] = capture.circle_point_consistency_enabled;
    pkg.metrics["findcircle_point_consistency_range"] = capture.circle_point_consistency_range;
    pkg.metrics["findcircle_point_consistency_input_points"] = capture.circle_point_consistency_input_points;
    pkg.metrics["findcircle_point_consistency_output_points"] = capture.circle_point_consistency_output_points;
    pkg.metrics["findcircle_point_consistency_removed_points"] = capture.circle_point_consistency_removed_points;
    pkg.metrics["findline_selected_edge_index"] = capture.findline_selected_edge_index;
    pkg.metrics["findline_evaluated_edge_count"] = capture.findline_evaluated_edge_count;
    pkg.metrics["findline_best_edge_index"] = capture.findline_best_edge_index;
    pkg.metrics["findline_best_edge_score"] = capture.findline_best_edge_score;
    pkg.metrics["strategy_id"] = capture.strategy_id;
    pkg.metrics["selected_method"] = capture.selected_method;
    pkg.metrics["selected_threshold"] = capture.selected_threshold;
    pkg.metrics["selected_wgap"] = capture.selected_wgap;
    pkg.metrics["selected_hgap"] = capture.selected_hgap;
    pkg.metrics["selected_linegap"] = capture.selected_linegap;
    pkg.metrics["selected_filterprofile"] = capture.selected_filterprofile;
    // Keep injection and script echo separate.  A missing script echo must
    // never be reported as if the CLI/manifest failed to inject its value.
    const auto readRuntimeGlobal = [&capture](const char* name) -> double
    {
        const auto it = capture.runtime_globals.find(name);
        return it == capture.runtime_globals.end() ? 0.0 : it->second;
    };
    const auto hasRuntimeGlobal = [&capture](const char* name) -> bool
    {
        return capture.runtime_globals.find(name) != capture.runtime_globals.end();
    };
    pkg.metrics["injected_threshold"] = readRuntimeGlobal("global_threshold");
    pkg.metrics["injected_method"] = readRuntimeGlobal("global_method");
    pkg.metrics["injected_wgap"] = readRuntimeGlobal("global_wgap");
    pkg.metrics["injected_hgap"] = readRuntimeGlobal("global_hgap");
    pkg.metrics["injected_linegap"] = readRuntimeGlobal("global_linegap");
    pkg.metrics["injected_filterprofile"] = readRuntimeGlobal("global_filterprofile");
    pkg.metrics["script_selected_threshold"] = capture.selected_threshold;
    pkg.metrics["script_selected_method"] = capture.selected_method;
    pkg.metrics["valid_points_count"] = capture.valid_points_count;
    pkg.metrics["runtime_valid_points_count"] = capture.valid_points_count;
    pkg.metrics["global_valid_points_count"] = readRuntimeGlobal("global_valid_points_count");
    pkg.metrics["runtime_has_fit_line"] = capture.has_fit_line ? 1.0 : 0.0;
    pkg.metrics["global_has_fit_line"] = readRuntimeGlobal("global_has_fit_line");
    pkg.metrics["runtime_has_fit_circle"] = capture.has_fit_circle ? 1.0 : 0.0;
    pkg.metrics["global_has_fit_circle"] = readRuntimeGlobal("global_has_fit_circle");
    pkg.metrics["runtime_global_valid_points_count_mismatch"] =
        hasRuntimeGlobal("global_valid_points_count") &&
        static_cast<int>(readRuntimeGlobal("global_valid_points_count")) != capture.valid_points_count
            ? 1.0 : 0.0;
    pkg.metrics["runtime_global_has_fit_line_mismatch"] =
        hasRuntimeGlobal("global_has_fit_line") &&
        ((readRuntimeGlobal("global_has_fit_line") != 0.0) != capture.has_fit_line)
            ? 1.0 : 0.0;
    pkg.metrics["runtime_global_has_fit_circle_mismatch"] =
        hasRuntimeGlobal("global_has_fit_circle") &&
        ((readRuntimeGlobal("global_has_fit_circle") != 0.0) != capture.has_fit_circle)
            ? 1.0 : 0.0;
    pkg.metrics["circle_radius"] = capture.circle_radius;
    pkg.metrics["avgdist"] = capture.avgdist;
    const double circle_gauge_cx = readRuntimeGlobal("global_circle_cx");
    const double circle_gauge_cy = readRuntimeGlobal("global_circle_cy");
    const double circle_inner_radius = readRuntimeGlobal("global_circle_inner_radius");
    const double circle_outer_radius = readRuntimeGlobal("global_circle_outer_radius");
    const CxCircleMeasurePointRadiusStats circle_radius_stats =
        ComputeCircleMeasurePointRadiusStats(
            capture.shapes,
            circle_gauge_cx,
            circle_gauge_cy,
            circle_inner_radius,
            circle_outer_radius);
    pkg.metrics["circle_measure_point_count_for_radius_check"] =
        circle_radius_stats.count;
    pkg.metrics["circle_measure_point_radius_min"] =
        circle_radius_stats.min_radius;
    pkg.metrics["circle_measure_point_radius_avg"] =
        circle_radius_stats.avg_radius;
    pkg.metrics["circle_measure_point_radius_max"] =
        circle_radius_stats.max_radius;
    pkg.metrics["circle_measure_points_inside_inner_count"] =
        circle_radius_stats.inside_inner_count;
    pkg.metrics["circle_measure_points_outside_outer_count"] =
        circle_radius_stats.outside_outer_count;
    pkg.metrics["result_rect_count"] = capture.result_rect_count;
    pkg.metrics["top1_rect_x"] = capture.top1_rect_x;
    pkg.metrics["top1_rect_y"] = capture.top1_rect_y;
    pkg.metrics["top1_rect_w"] = capture.top1_rect_w;
    pkg.metrics["top1_rect_h"] = capture.top1_rect_h;
    pkg.metrics["model_point_count"] = capture.model_point_count;
    pkg.metrics["fastmatch_learn_a_count"] = capture.fastmatch_learn_a_count;
    pkg.metrics["fastmatch_learn_b_count"] = capture.fastmatch_learn_b_count;
    pkg.metrics["fastmatch_learn_a2_count"] = capture.fastmatch_learn_a2_count;
    pkg.metrics["fastmatch_learn_b2_count"] = capture.fastmatch_learn_b2_count;
    pkg.metrics["fastmatch_learn_status_code"] = capture.fastmatch_learn_status_code;
    pkg.metrics["candidate_count"] = capture.candidate_count;
    pkg.metrics["best_score"] = capture.best_score;
    pkg.metrics["rendered_measure_points_count"] = capture.rendered_measure_points_count;
    pkg.metrics["rendered_result_count"] = capture.rendered_result_count;
    pkg.metrics["result_overlay_changed_pixels"] = capture.result_overlay_changed_pixels;

    pkg.metrics["ellipse_cx"] = capture.ellipse_cx;
    pkg.metrics["ellipse_cy"] = capture.ellipse_cy;
    pkg.metrics["ellipse_radius_x"] = capture.ellipse_radius_x;
    pkg.metrics["ellipse_radius_y"] = capture.ellipse_radius_y;
    pkg.metrics["ellipse_angle_deg"] = capture.ellipse_angle_deg;
    pkg.metrics["ellipse_selected_edge_index"] = capture.ellipse_selected_edge_index;
    pkg.metrics["ellipse_scan_candidate_lines"] = capture.ellipse_scan_candidate_lines;
    pkg.metrics["ellipse_scan_total_candidates"] = capture.ellipse_scan_total_candidates;
    pkg.metrics["ellipse_scan_accepted_points_before_gate"] = capture.ellipse_scan_accepted_points_before_gate;
    pkg.metrics["ellipse_accepted_min_boundary_ratio"] = capture.ellipse_accepted_min_boundary_ratio;
    pkg.metrics["ellipse_accepted_max_boundary_ratio"] = capture.ellipse_accepted_max_boundary_ratio;
    pkg.metrics["ellipse_accepted_avg_boundary_ratio"] = capture.ellipse_accepted_avg_boundary_ratio;
    pkg.metrics["ellipse_scan_lines_cross_outside_ellipse_count"] = capture.ellipse_scan_lines_cross_outside_ellipse_count;
    pkg.metrics["ellipse_scan_endpoint_norm_min"] = capture.ellipse_scan_endpoint_norm_min;
    pkg.metrics["ellipse_scan_endpoint_norm_avg"] = capture.ellipse_scan_endpoint_norm_avg;
    pkg.metrics["ellipse_scan_endpoint_norm_max"] = capture.ellipse_scan_endpoint_norm_max;
    pkg.metrics["ellipse_accepted_points_outside_ellipse_count"] = capture.ellipse_accepted_points_outside_ellipse_count;
    pkg.metrics["ellipse_accepted_point_norm_min"] = capture.ellipse_accepted_point_norm_min;
    pkg.metrics["ellipse_accepted_point_norm_avg"] = capture.ellipse_accepted_point_norm_avg;
    pkg.metrics["ellipse_accepted_point_norm_max"] = capture.ellipse_accepted_point_norm_max;
    pkg.metrics["ellipse_rejected_boundary_band_candidate_count"] =
        capture.ellipse_rejected_boundary_band_candidate_count;
    pkg.metrics["ellipse_rejected_boundary_band_norm_min"] =
        capture.ellipse_rejected_boundary_band_norm_min;
    pkg.metrics["ellipse_rejected_boundary_band_norm_avg"] =
        capture.ellipse_rejected_boundary_band_norm_avg;
    pkg.metrics["ellipse_rejected_boundary_band_norm_max"] =
        capture.ellipse_rejected_boundary_band_norm_max;
    pkg.metrics["ellipse_point_consistency_enabled"] =
        capture.ellipse_point_consistency_enabled;
    pkg.metrics["ellipse_point_consistency_range"] =
        capture.ellipse_point_consistency_range;
    pkg.metrics["ellipse_point_consistency_input_points"] =
        capture.ellipse_point_consistency_input_points;
    pkg.metrics["ellipse_point_consistency_output_points"] =
        capture.ellipse_point_consistency_output_points;
    pkg.metrics["ellipse_point_consistency_removed_points"] =
        capture.ellipse_point_consistency_removed_points;

    pkg.metrics["fastmatch_model_width"] = capture.fastmatch_model_width;
    pkg.metrics["fastmatch_model_height"] = capture.fastmatch_model_height;
    pkg.metrics["fastmatch_pattern_a_count"] = capture.fastmatch_pattern_a_count;
    pkg.metrics["fastmatch_pattern_b_count"] = capture.fastmatch_pattern_b_count;
    pkg.metrics["fastmatch_pattern_a_x"] = capture.fastmatch_pattern_a_x;
    pkg.metrics["fastmatch_pattern_a_y"] = capture.fastmatch_pattern_a_y;
    pkg.metrics["fastmatch_pattern_a_width"] = capture.fastmatch_pattern_a_width;
    pkg.metrics["fastmatch_pattern_a_height"] = capture.fastmatch_pattern_a_height;
    pkg.metrics["fastmatch_pattern_b_x"] = capture.fastmatch_pattern_b_x;
    pkg.metrics["fastmatch_pattern_b_y"] = capture.fastmatch_pattern_b_y;
    pkg.metrics["fastmatch_pattern_b_width"] = capture.fastmatch_pattern_b_width;
    pkg.metrics["fastmatch_pattern_b_height"] = capture.fastmatch_pattern_b_height;
    pkg.metrics["fastmatch_match_call_count"] = capture.fastmatch_match_call_count;
    pkg.metrics["fastmatch_match_ab_call_count"] = capture.fastmatch_match_ab_call_count;
    pkg.metrics["fastmatch_match_sample_ab_call_count"] = capture.fastmatch_match_sample_ab_call_count;
    pkg.metrics["fastmatch_match_last_stage"] = capture.fastmatch_match_last_stage;
    pkg.metrics["fastmatch_match_image_width"] = capture.fastmatch_match_image_width;
    pkg.metrics["fastmatch_match_image_height"] = capture.fastmatch_match_image_height;
    pkg.metrics["fastmatch_learn_rect_x0"] = capture.fastmatch_learn_rect_x0;
    pkg.metrics["fastmatch_learn_rect_y0"] = capture.fastmatch_learn_rect_y0;
    pkg.metrics["fastmatch_learn_rect_x1"] = capture.fastmatch_learn_rect_x1;
    pkg.metrics["fastmatch_learn_rect_y1"] = capture.fastmatch_learn_rect_y1;
    pkg.metrics["fastmatch_match_rect_x0"] = capture.fastmatch_match_rect_x0;
    pkg.metrics["fastmatch_match_rect_y0"] = capture.fastmatch_match_rect_y0;
    pkg.metrics["fastmatch_match_rect_x1"] = capture.fastmatch_match_rect_x1;
    pkg.metrics["fastmatch_match_rect_y1"] = capture.fastmatch_match_rect_y1;
    pkg.metrics["fastmatch_raw_probe_count"] = capture.fastmatch_raw_probe_count;
    pkg.metrics["fastmatch_raw_threshold_hit_count"] = capture.fastmatch_raw_threshold_hit_count;
    pkg.metrics["fastmatch_result_to_list_count"] = capture.fastmatch_result_to_list_count;
    pkg.metrics["fastmatch_candidate_insert_count"] = capture.fastmatch_candidate_insert_count;
    pkg.metrics["fastmatch_candidate_replace_count"] = capture.fastmatch_candidate_replace_count;
    pkg.metrics["fastmatch_candidate_reject_count"] = capture.fastmatch_candidate_reject_count;

    pkg.metrics["object_filter_borw"] = capture.object_filter_borw;
    pkg.metrics["findobject_strategy_id"] = capture.object_filter_strategy_id;
    pkg.metrics["object_filter_min"] = capture.object_filter_min;
    pkg.metrics["object_filter_max"] = capture.object_filter_max;
    pkg.metrics["findobject_component_count"] = capture.object_component_count;
    pkg.metrics["findobject_component_accepted_count"] = capture.object_component_accepted_count;
    pkg.metrics["findobject_component_rejected_count"] = capture.object_component_rejected_count;
    pkg.metrics["findobject_component_max_area"] = capture.object_component_max_area;
    pkg.metrics["findobject_component_max_width"] = capture.object_component_max_width;
    pkg.metrics["findobject_component_max_height"] = capture.object_component_max_height;
    pkg.metrics["findobject_foreground_before"] = capture.object_foreground_before;
    pkg.metrics["findobject_foreground_after"] = capture.object_foreground_after;
    pkg.metrics["findobject_white_component_count"] = capture.object_white_component_count;
    pkg.metrics["findobject_white_accepted_count"] = capture.object_white_accepted_count;
    pkg.metrics["findobject_white_rejected_count"] = capture.object_white_rejected_count;
    pkg.metrics["findobject_black_component_count"] = capture.object_black_component_count;
    pkg.metrics["findobject_black_accepted_count"] = capture.object_black_accepted_count;
    pkg.metrics["findobject_black_rejected_count"] = capture.object_black_rejected_count;
    pkg.metrics["fit_filter_input_count"] = capture.fit_filter_input_count;
    pkg.metrics["fit_filter_kept_count"] = capture.fit_filter_kept_count;
    pkg.metrics["fit_filter_rejected_count"] = capture.fit_filter_rejected_count;
    pkg.metrics["fit_filter_sigma"] = capture.fit_filter_sigma;
    pkg.metrics["fit_filter_threshold"] = capture.fit_filter_threshold;
    pkg.metrics["findrect_top_points"] = capture.findrect_top_points;
    pkg.metrics["findrect_bottom_points"] = capture.findrect_bottom_points;
    pkg.metrics["findrect_left_points"] = capture.findrect_left_points;
    pkg.metrics["findrect_right_points"] = capture.findrect_right_points;
    pkg.metrics["findrect_coarse_score"] = capture.findrect_coarse_score;
    pkg.metrics["findrect_refine_score"] = capture.findrect_refine_score;

    pkg.metrics["segmentation_status_code"] = capture.segmentation_status_code;
    pkg.metrics["segmentation_contour_count"] = capture.segmentation_contour_count;
    pkg.metrics["segmentation_primary_area"] = capture.segmentation_primary_area;
    pkg.metrics["torch_ok"] = capture.torch_ok;
    pkg.metrics["torch_error_code"] = capture.torch_error_code;
    pkg.metrics["torch_train_ms"] = capture.torch_train_ms;
    pkg.metrics["torch_infer_ms"] = capture.torch_infer_ms;
    pkg.metrics["torch_total_ms"] = capture.torch_total_ms;
    pkg.metrics["torch_result_count"] = capture.torch_result_count;

    pkg.facts["execution_mode"] = "sequential";
    pkg.facts["algorithm_executed"] = capture.runtime_completed ? "true" : "false";
    pkg.facts["budget_exceeded"] = capture.budget_exceeded ? "true" : "false";
    pkg.facts["has_fit_line"] = capture.has_fit_line ? "true" : "false";
    pkg.facts["has_fit_circle"] = capture.has_fit_circle ? "true" : "false";
    pkg.facts["has_fit_ellipse"] = capture.has_fit_ellipse ? "true" : "false";
    pkg.facts["has_result_rect"] = capture.has_result_rect ? "true" : "false";
    pkg.facts["has_result_box"] = capture.has_result_box ? "true" : "false";
    pkg.facts["has_best_result"] = capture.has_best_result ? "true" : "false";
    pkg.facts["failure_stage"] = capture.failure_stage;
    pkg.facts["reason"] = capture.reason;
    pkg.facts["runtime_result_source"] = "runtime_capture";
    pkg.facts["global_result_echo_available"] =
        (hasRuntimeGlobal("global_valid_points_count") ||
         hasRuntimeGlobal("global_has_fit_line") ||
         hasRuntimeGlobal("global_has_fit_circle"))
            ? "true" : "false";
    pkg.facts["runtime_global_result_mismatch"] =
        (pkg.metrics["runtime_global_valid_points_count_mismatch"] != 0.0 ||
         pkg.metrics["runtime_global_has_fit_line_mismatch"] != 0.0 ||
         pkg.metrics["runtime_global_has_fit_circle_mismatch"] != 0.0)
            ? "true" : "false";

    pkg.facts["ellipse_candidate_policy"] = capture.ellipse_candidate_policy;
    pkg.facts["ellipse_scan_geometry_policy"] = capture.ellipse_scan_geometry_policy;
    pkg.facts["object_prefilter_requested"] = capture.object_prefilter_requested ? "true" : "false";
    pkg.facts["object_prefilter_applied"] = capture.object_prefilter_applied ? "true" : "false";
    pkg.facts["findobject_algorithm_branch"] = capture.object_algorithm_branch;
    pkg.facts["findobject_strategy_semantics"] =
        "0=auto_by_filter,1=measure_region_growth,2=measure_fast_region_growth,3=connected_components,4=peak_local_bfs_diagnostic";
    pkg.facts["script_selected_threshold_matches_injected"] =
        capture.selected_threshold == static_cast<int>(readRuntimeGlobal("global_threshold"))
            ? "true" : "false";
    pkg.facts["findrect_seed_valid"] = capture.findrect_seed_valid ? "true" : "false";
    pkg.facts["findrect_top_valid"] = capture.findrect_top_valid ? "true" : "false";
    pkg.facts["findrect_bottom_valid"] = capture.findrect_bottom_valid ? "true" : "false";
    pkg.facts["findrect_left_valid"] = capture.findrect_left_valid ? "true" : "false";
    pkg.facts["findrect_right_valid"] = capture.findrect_right_valid ? "true" : "false";
    pkg.facts["segmentation_result_ref"] = capture.segmentation_result_ref;
    pkg.facts["segmentation_mask_ref"] = capture.segmentation_mask_ref;
    pkg.facts["segmentation_contour_ref"] = capture.segmentation_contour_ref;
    pkg.facts["segmentation_overlay_ref"] = capture.segmentation_overlay_ref;
    pkg.facts["torch_ok"] = capture.torch_ok != 0 ? "true" : "false";
    pkg.facts["torch_status"] = capture.torch_status;
    pkg.facts["torch_failure_stage"] = capture.torch_failure_stage;
    pkg.facts["torch_reason"] = capture.torch_reason;
    pkg.facts["torch_evidence_ref"] = capture.torch_evidence_ref;
    pkg.facts["torch_primary_visual_ref"] = capture.torch_primary_visual_ref;
    pkg.facts["torch_trainer_lifecycle_summary"] = capture.torch_trainer_lifecycle_summary;
    pkg.facts["torch_unified_mainline_summary"] = capture.torch_unified_mainline_summary;

    if (capture.contract_context)
    {
        pkg.facts["contract_pass"] = capture.contract_pass ? "true" : "false";
        pkg.facts["contract_status"] = capture.contract_status;
        pkg.facts["contract_conclusion"] = capture.contract_conclusion;
    }

    return pkg;
}

std::string FindObjectStrategyName(int strategy_id)
{
    switch (strategy_id)
    {
    case 0:
        return "auto_by_filter";
    case 1:
        return "measure_region_growth";
    case 2:
        return "measure_fast_region_growth";
    case 3:
        return "connected_components";
    case 4:
        return "peak_local_bfs_diagnostic";
    default:
        return "unknown";
    }
}

std::string ClassifyFindLineFindObjectBoundary(
    const CxScriptExecutionCapture& capture)
{
    if (capture.has_fit_line)
        return "findline_fit_available";

    if (capture.object_prefilter_applied &&
        capture.object_foreground_after > 0 &&
        capture.scan_rows_with_foreground == 0 &&
        capture.scan_runs_total == 0 &&
        capture.valid_points_count == 0)
    {
        return "findline_fail_prefilter_foreground_not_visible_to_scan_rows";
    }

    if (capture.scan_rows_with_foreground > 0 &&
        capture.scan_runs_total == 0 &&
        capture.valid_points_count == 0)
    {
        return "findline_fail_binary_saturated_or_no_segment_boundary";
    }

    if (capture.object_prefilter_requested && !capture.object_prefilter_applied)
        return "findline_fail_findobject_prefilter_not_applied";

    if (capture.object_prefilter_applied &&
        capture.object_component_count > 0 &&
        capture.object_component_accepted_count == 0)
    {
        return "findline_fail_findobject_component_rejected";
    }

    if (capture.scan_runs_total > 0 &&
        capture.scan_points_emitted == 0)
    {
        return "findline_fail_scan_runs_rejected";
    }

    if (capture.valid_points_count > 0 && !capture.has_fit_line)
        return "findline_fail_fit_degenerate";

    if (!capture.failure_stage.empty())
        return capture.failure_stage;

    return "findline_fail_unknown";
}

bool SaveFindObjectBranchEvidenceJson(
    const CxScriptExecutionCapture& capture,
    const CxScriptHeadlessOptions& options,
    const std::filesystem::path& outputPath,
    std::string& outReason)
{
    std::error_code ec;
    std::filesystem::create_directories(outputPath.parent_path(), ec);
    if (ec)
    {
        outReason = "failed to create findobject branch evidence directory: " + ec.message();
        return false;
    }

    std::ofstream file(outputPath, std::ios::binary | std::ios::trunc);
    if (!file.is_open())
    {
        outReason = "failed to open findobject branch evidence json";
        return false;
    }

    const std::string boundary = ClassifyFindLineFindObjectBoundary(capture);

    file << "{\n";
    file << "  \"case_id\": \"" << JsonEscape(options.case_name) << "\",\n";
    file << "  \"script\": \"" << JsonEscape(options.script_path) << "\",\n";
    file << "  \"image\": \"" << JsonEscape(options.image_path) << "\",\n";
    file << "  \"runtime_result_source\": \"runtime_capture\",\n";
    file << "  \"strategy\": {\n";
    file << "    \"id\": " << capture.object_filter_strategy_id << ",\n";
    file << "    \"name\": \"" << JsonEscape(FindObjectStrategyName(capture.object_filter_strategy_id)) << "\",\n";
    file << "    \"algorithm_branch\": \"" << JsonEscape(capture.object_algorithm_branch) << "\",\n";
    file << "    \"semantics\": \"0=auto_by_filter,1=measure_region_growth,2=measure_fast_region_growth,3=connected_components,4=peak_local_bfs_diagnostic\"\n";
    file << "  },\n";
    file << "  \"input\": {\n";
    file << "    \"method\": " << capture.tool_method << ",\n";
    file << "    \"threshold\": " << capture.tool_threshold << ",\n";
    file << "    \"wgap\": " << capture.tool_wgap << ",\n";
    file << "    \"hgap\": " << capture.tool_hgap << ",\n";
    file << "    \"linegap\": " << capture.tool_linegap << "\n";
    file << "  },\n";
    file << "  \"findobject\": {\n";
    file << "    \"requested\": " << (capture.object_prefilter_requested ? "true" : "false") << ",\n";
    file << "    \"applied\": " << (capture.object_prefilter_applied ? "true" : "false") << ",\n";
    file << "    \"borw\": " << capture.object_filter_borw << ",\n";
    file << "    \"filter_min\": " << capture.object_filter_min << ",\n";
    file << "    \"filter_max\": " << capture.object_filter_max << ",\n";
    file << "    \"foreground_before\": " << capture.object_foreground_before << ",\n";
    file << "    \"foreground_after\": " << capture.object_foreground_after << ",\n";
    file << "    \"component_count\": " << capture.object_component_count << ",\n";
    file << "    \"accepted_count\": " << capture.object_component_accepted_count << ",\n";
    file << "    \"rejected_count\": " << capture.object_component_rejected_count << ",\n";
    file << "    \"max_area\": " << capture.object_component_max_area << ",\n";
    file << "    \"white_component_count\": " << capture.object_white_component_count << ",\n";
    file << "    \"white_accepted_count\": " << capture.object_white_accepted_count << ",\n";
    file << "    \"black_component_count\": " << capture.object_black_component_count << ",\n";
    file << "    \"black_accepted_count\": " << capture.object_black_accepted_count << "\n";
    file << "  },\n";
    file << "  \"findline_scan\": {\n";
    file << "    \"rows_examined\": " << capture.scan_rows_examined << ",\n";
    file << "    \"rows_with_foreground\": " << capture.scan_rows_with_foreground << ",\n";
    file << "    \"runs_total\": " << capture.scan_runs_total << ",\n";
    file << "    \"runs_within_length_limit\": " << capture.scan_runs_within_length_limit << ",\n";
    file << "    \"runs_over_length_limit\": " << capture.scan_runs_over_length_limit << ",\n";
    file << "    \"runs_rejected_by_selection\": " << capture.scan_runs_rejected_by_selection << ",\n";
    file << "    \"runs_rejected_near_endpoint\": " << capture.scan_runs_rejected_near_endpoint << ",\n";
    file << "    \"points_emitted\": " << capture.scan_points_emitted << "\n";
    file << "  },\n";
    file << "  \"result\": {\n";
    file << "    \"has_fit_line\": " << (capture.has_fit_line ? "true" : "false") << ",\n";
    file << "    \"valid_points_count\": " << capture.valid_points_count << ",\n";
    file << "    \"avgdist\": " << capture.avgdist << ",\n";
    file << "    \"rendered_measure_points_count\": " << capture.rendered_measure_points_count << ",\n";
    file << "    \"rendered_result_count\": " << capture.rendered_result_count << ",\n";
    file << "    \"failure_stage\": \"" << JsonEscape(capture.failure_stage) << "\",\n";
    file << "    \"boundary_classification\": \"" << JsonEscape(boundary) << "\"\n";
    file << "  }\n";
    file << "}\n";

    file.flush();
    if (!file.good())
    {
        outReason = "failed while writing findobject branch evidence json";
        return false;
    }

    outReason.clear();
    return true;
}

void WriteJsonNumberMap(
    std::ofstream& file,
    const std::string& key,
    const std::map<std::string, double>& values,
    bool trailing_comma)
{
    file << "  \"" << key << "\": {\n";
    for (auto it = values.begin(); it != values.end(); ++it)
    {
        const auto next = std::next(it);
        file << "    \"" << JsonEscape(it->first) << "\": " << it->second
             << (next == values.end() ? "" : ",") << "\n";
    }
    file << "  }" << (trailing_comma ? "," : "") << "\n";
}

void WriteJsonStringMap(
    std::ofstream& file,
    const std::string& key,
    const std::map<std::string, std::string>& values,
    bool trailing_comma)
{
    file << "  \"" << key << "\": {\n";
    for (auto it = values.begin(); it != values.end(); ++it)
    {
        const auto next = std::next(it);
        file << "    \"" << JsonEscape(it->first) << "\": \""
             << JsonEscape(it->second) << "\""
             << (next == values.end() ? "" : ",") << "\n";
    }
    file << "  }" << (trailing_comma ? "," : "") << "\n";
}

void WriteJsonShapeSnapshots(
    std::ofstream& file,
    const std::vector<CxShapeElementSnapshot>& shapes,
    bool trailing_comma)
{
    file << "  \"shapes\": [";
    if (shapes.empty())
    {
        file << "]" << (trailing_comma ? "," : "") << "\n";
        return;
    }

    for (auto it = shapes.begin(); it != shapes.end(); ++it)
    {
        const auto next = std::next(it);
        const CxShapeElementSnapshot& s = *it;
        file << "\n    {\n";
        file << "      \"stable_ref\": \"" << JsonEscape(s.stable_ref) << "\",\n";
        file << "      \"owner_type\": \"" << JsonEscape(s.owner_type) << "\",\n";
        file << "      \"owner_ref\": \"" << JsonEscape(s.owner_ref) << "\",\n";
        file << "      \"semantic_role\": \"" << JsonEscape(s.semantic_role) << "\",\n";
        file << "      \"shape_kind\": \"" << JsonEscape(s.shape_kind) << "\",\n";
        file << "      \"editable\": " << (s.editable ? "true" : "false") << ",\n";
        file << "      \"result_element\": " << (s.result_element ? "true" : "false") << ",\n";
        file << "      \"center_x\": " << s.center_x << ",\n";
        file << "      \"center_y\": " << s.center_y << ",\n";
        file << "      \"radius\": " << s.radius << ",\n";
        file << "      \"radius_x\": " << s.radius_x << ",\n";
        file << "      \"radius_y\": " << s.radius_y << ",\n";
        file << "      \"angle_deg\": " << s.angle_deg << ",\n";
        file << "      \"points\": [";
        if (s.points.empty())
        {
            file << "]";
        }
        else
        {
            for (auto pit = s.points.begin(); pit != s.points.end(); ++pit)
            {
                const auto pnext = std::next(pit);
                file << *pit << (pnext == s.points.end() ? "" : ",");
            }
            file << "]";
        }
        file << "\n    }" << (next == shapes.end() ? "" : ",");
    }
    file << "\n  ]" << (trailing_comma ? "," : "") << "\n";
}

void WriteJsonFindLineScanDiagnostics(
    std::ofstream& file,
    const std::vector<CxFindLineScanDiagnosticSnapshot>& diagnostics,
    bool trailing_comma)
{
    file << "  \"findline_scan_diagnostics\": [";
    if (diagnostics.empty())
    {
        file << "]" << (trailing_comma ? "," : "") << "\n";
        return;
    }

    for (auto it = diagnostics.begin(); it != diagnostics.end(); ++it)
    {
        const auto next = std::next(it);
        const CxFindLineScanDiagnosticSnapshot& d = *it;
        file << "\n    {\n";
        file << "      \"scan_index\": " << d.scan_index << ",\n";
        file << "      \"scan_type\": " << d.scan_type << ",\n";
        file << "      \"scan_line\": {"
             << "\"x0\": " << d.x0 << ", "
             << "\"y0\": " << d.y0 << ", "
             << "\"x1\": " << d.x1 << ", "
             << "\"y1\": " << d.y1 << "},\n";
        file << "      \"candidate_count\": " << d.candidate_count << ",\n";
        file << "      \"accepted\": " << (d.accepted ? "true" : "false") << ",\n";
        file << "      \"accepted_point\": {"
             << "\"x\": " << d.accepted_x << ", "
             << "\"y\": " << d.accepted_y << "},\n";
        file << "      \"reject_reason\": \""
             << JsonEscape(d.reject_reason) << "\"\n";
        file << "    }" << (next == diagnostics.end() ? "" : ",");
    }
    file << "\n  ]" << (trailing_comma ? "," : "") << "\n";
}

void WriteJsonFindLineEdgeEvaluations(
    std::ofstream& file,
    const std::vector<CxFindLineEdgeEvaluationSnapshot>& evaluations,
    bool trailing_comma)
{
    file << "  \"findline_edge_evaluations\": [";
    if (evaluations.empty())
    {
        file << "]" << (trailing_comma ? "," : "") << "\n";
        return;
    }

    for (auto it = evaluations.begin(); it != evaluations.end(); ++it)
    {
        const auto next = std::next(it);
        const CxFindLineEdgeEvaluationSnapshot& e = *it;
        file << "\n    {\n";
        file << "      \"edge_index\": " << e.edge_index << ",\n";
        file << "      \"candidate_scan_rows\": " << e.candidate_scan_rows << ",\n";
        file << "      \"accepted_points\": " << e.accepted_points << ",\n";
        file << "      \"rejected_by_selection\": " << e.rejected_by_selection << ",\n";
        file << "      \"rejected_near_endpoint\": " << e.rejected_near_endpoint << ",\n";
        file << "      \"over_length_runs\": " << e.over_length_runs << ",\n";
        file << "      \"coverage\": " << e.coverage << ",\n";
        file << "      \"score\": " << e.score << ",\n";
        file << "      \"selected\": " << (e.selected ? "true" : "false") << ",\n";
        file << "      \"fit_possible\": " << (e.fit_possible ? "true" : "false") << "\n";
        file << "    }" << (next == evaluations.end() ? "" : ",");
    }
    file << "\n  ]" << (trailing_comma ? "," : "") << "\n";
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

    CxScriptResultPackage pkg = BuildCxScriptResultPackage(capture);

    file << "{\n";

    file << "  \"execution_mode\": \"sequential\",\n";

    WriteJsonStringMap(file, "facts", pkg.facts, true);
    WriteJsonNumberMap(file, "metrics", pkg.metrics, true);
    WriteJsonNumberMap(file, "runtime_globals", capture.runtime_globals, true);
    WriteJsonShapeSnapshots(file, pkg.shapes, true);
    WriteJsonFindLineEdgeEvaluations(
        file,
        capture.findline_edge_evaluations,
        true);
    WriteJsonFindLineScanDiagnostics(
        file,
        capture.findline_scan_diagnostics,
        false);

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
        else if (arg == "--globals" && i + 1 < argc)
            options.globals_path = argv[++i];
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
        else if (arg == "--findcircle-arc-enabled" && i + 1 < argc)
            options.findcircle_arc_enabled = std::stoi(argv[++i]);
        else if (arg == "--findcircle-arc-start-deg" && i + 1 < argc)
            options.findcircle_arc_start_deg = std::stoi(argv[++i]);
        else if (arg == "--findcircle-arc-end-deg" && i + 1 < argc)
            options.findcircle_arc_end_deg = std::stoi(argv[++i]);
        else if (arg == "--ellipse-x0" && i + 1 < argc)
            options.ellipse_x0 = std::stoi(argv[++i]);
        else if (arg == "--ellipse-y0" && i + 1 < argc)
            options.ellipse_y0 = std::stoi(argv[++i]);
        else if (arg == "--ellipse-x1" && i + 1 < argc)
            options.ellipse_x1 = std::stoi(argv[++i]);
        else if (arg == "--ellipse-y1" && i + 1 < argc)
            options.ellipse_y1 = std::stoi(argv[++i]);
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
        else if (arg == "--save-evidence-candidate")
            options.save_evidence_candidate = true;
        else if (arg == "--evidence-candidate-root" && i + 1 < argc)
            options.evidence_candidate_root = argv[++i];
        else if (arg == "--evidence-candidate-id" && i + 1 < argc)
            options.evidence_candidate_id = argv[++i];
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
        if (options.save_evidence_candidate)
        {
            ManualTestContext candidateContext;
            candidateContext.active_case_id =
                options.case_name.empty() ? options.case_id : options.case_name;
            candidateContext.active_image_id =
                options.image_id.empty() ? options.stage25_image_id : options.image_id;
            candidateContext.active_target_id =
                options.target_id.empty() ? options.stage25_target_id : options.target_id;
            candidateContext.image_file_path = options.image_path;
            candidateContext.loaded_script_path = options.script_path;
            candidateContext.script_file_path = options.script_path;
            candidateContext.editor_source = "headless";
            ReadTextFile(options.script_path, candidateContext.editor_text);
            candidateContext.debug_status = "HEADLESS_EXECUTION_FAIL";
            candidateContext.debug_reason = result.reason;
            candidateContext.current_result_ref.status = "runtime_result_failed";
            candidateContext.current_result_ref.reason = result.reason;

            auto setGlobal = [&](const std::string& name, int value)
            {
                candidateContext.runtime_int_vars[name] = value;
            };
            setGlobal("global_roi_x0", options.roi_x0);
            setGlobal("global_roi_y0", options.roi_y0);
            setGlobal("global_roi_x1", options.roi_x1);
            setGlobal("global_roi_y1", options.roi_y1);
            setGlobal("global_circle_cx", options.circle_cx);
            setGlobal("global_circle_cy", options.circle_cy);
            setGlobal("global_circle_px", options.circle_px);
            setGlobal("global_circle_py", options.circle_py);
            setGlobal("global_circle_inner_radius", 0);
            setGlobal("global_circle_outer_radius", 0);
            setGlobal("global_circle_ring_width", 0);
            setGlobal("global_findcircle_arc_enabled", options.findcircle_arc_enabled);
            setGlobal("global_findcircle_arc_start_deg", options.findcircle_arc_start_deg);
            setGlobal("global_findcircle_arc_end_deg", options.findcircle_arc_end_deg);
            setGlobal("global_ellipse_x0", options.ellipse_x0);
            setGlobal("global_ellipse_y0", options.ellipse_y0);
            setGlobal("global_ellipse_x1", options.ellipse_x1);
            setGlobal("global_ellipse_y1", options.ellipse_y1);
            setGlobal("global_tool_half_width", options.tool_half_width);
            setGlobal("global_wgap", options.wgap);
            setGlobal("global_hgap", options.hgap);
            setGlobal("global_gap", options.gap);
            setGlobal("global_linegap", options.linegap);
            setGlobal("global_threshold", options.threshold);
            setGlobal("global_method", options.method);
            setGlobal("global_filterprofile", options.filterprofile);
            setGlobal("global_max_elapsed_ms", options.max_elapsed_ms);
            setGlobal("global_max_scan_lines", options.max_scan_lines);
            setGlobal("global_max_samples", options.max_samples);

            ManualGaugeState& gauge = candidateContext.current_gauge;
            gauge.case_id = candidateContext.active_case_id;
            gauge.image_id = candidateContext.active_image_id;
            gauge.target_id = candidateContext.active_target_id;
            gauge.source = "headless";
            gauge.review_status = "pending_human_review";
            gauge.tool = options.stage25_tool.empty() ? "FindLine" : options.stage25_tool;
            gauge.has_line_gauge = true;
            gauge.line_x0 = options.roi_x0;
            gauge.line_y0 = options.roi_y0;
            gauge.line_x1 = options.roi_x1;
            gauge.line_y1 = options.roi_y1;
            gauge.tool_half_width = options.tool_half_width;
            gauge.threshold = options.threshold;
            gauge.method = options.method;
            gauge.linegap = options.linegap;
            gauge.wgap = options.wgap;
            gauge.hgap = options.hgap;
            gauge.gap = options.gap;
            gauge.filterprofile = options.filterprofile;

            CxEvidenceCandidateSaveOptions candidateOptions;
            candidateOptions.root_dir = options.evidence_candidate_root.empty()
                ? "cxscript_runs/evidence_candidates"
                : options.evidence_candidate_root;
            candidateOptions.candidate_id = options.evidence_candidate_id;
            candidateOptions.mode = "headless_failed";
            candidateOptions.add_to_evidence_chain = false;
            CxEvidenceCandidateSaveResult candidateResult;
            SaveEvidenceCandidatePackage(
                candidateContext,
                candidateOptions,
                candidateResult);
        }
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
    std::filesystem::path findobject_branch_evidence_path = output_dir / "findobject_branch_evidence.json";
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

    std::string measurement_semantic_reason;
    if (!WriteMeasurementSemanticSidecars(
            capture,
            effective_options,
            output_dir,
            measurement_semantic_reason))
    {
        if (result.reason.empty())
            result.reason = measurement_semantic_reason;
    }

    std::string branch_evidence_reason;
    if (!SaveFindObjectBranchEvidenceJson(
            capture,
            options,
            findobject_branch_evidence_path,
            branch_evidence_reason))
    {
        if (result.reason.empty())
            result.reason = branch_evidence_reason;
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
        object_state_file << "  \"ellipse_cx\": " << capture.ellipse_cx << ",\n";
        object_state_file << "  \"ellipse_cy\": " << capture.ellipse_cy << ",\n";
        object_state_file << "  \"ellipse_radius_x\": " << capture.ellipse_radius_x << ",\n";
        object_state_file << "  \"ellipse_radius_y\": " << capture.ellipse_radius_y << ",\n";
        object_state_file << "  \"ellipse_angle_deg\": " << capture.ellipse_angle_deg << ",\n";
        object_state_file << "  \"ellipse_selected_edge_index\": " << capture.ellipse_selected_edge_index << ",\n";
        object_state_file << "  \"ellipse_point_consistency_enabled\": " << capture.ellipse_point_consistency_enabled << ",\n";
        object_state_file << "  \"ellipse_point_consistency_range\": " << capture.ellipse_point_consistency_range << ",\n";
        object_state_file << "  \"ellipse_point_consistency_input_points\": " << capture.ellipse_point_consistency_input_points << ",\n";
        object_state_file << "  \"ellipse_point_consistency_output_points\": " << capture.ellipse_point_consistency_output_points << ",\n";
        object_state_file << "  \"ellipse_point_consistency_removed_points\": " << capture.ellipse_point_consistency_removed_points << ",\n";
        object_state_file << "  \"avgdist\": " << capture.avgdist << ",\n";
        object_state_file << "  \"result_rect_count\": " << capture.result_rect_count << ",\n";
        object_state_file << "  \"top1_rect_x\": " << capture.top1_rect_x << ",\n";
        object_state_file << "  \"top1_rect_y\": " << capture.top1_rect_y << ",\n";
        object_state_file << "  \"top1_rect_w\": " << capture.top1_rect_w << ",\n";
        object_state_file << "  \"top1_rect_h\": " << capture.top1_rect_h << ",\n";
        object_state_file << "  \"model_point_count\": " << capture.model_point_count << ",\n";
        object_state_file << "  \"fastmatch_learn_a_count\": " << capture.fastmatch_learn_a_count << ",\n";
        object_state_file << "  \"fastmatch_learn_b_count\": " << capture.fastmatch_learn_b_count << ",\n";
        object_state_file << "  \"fastmatch_learn_a2_count\": " << capture.fastmatch_learn_a2_count << ",\n";
        object_state_file << "  \"fastmatch_learn_b2_count\": " << capture.fastmatch_learn_b2_count << ",\n";
        object_state_file << "  \"fastmatch_learn_status_code\": " << capture.fastmatch_learn_status_code << ",\n";
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
        object_state_file << "  \"fastmatch_learn_rect_x0\": " << capture.fastmatch_learn_rect_x0 << ",\n";
        object_state_file << "  \"fastmatch_learn_rect_y0\": " << capture.fastmatch_learn_rect_y0 << ",\n";
        object_state_file << "  \"fastmatch_learn_rect_x1\": " << capture.fastmatch_learn_rect_x1 << ",\n";
        object_state_file << "  \"fastmatch_learn_rect_y1\": " << capture.fastmatch_learn_rect_y1 << ",\n";
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
        object_state_file << "  \"findobject_strategy_id\": " << capture.object_filter_strategy_id << ",\n";
        object_state_file << "  \"object_filter_borw\": " << capture.object_filter_borw << ",\n";
        object_state_file << "  \"object_filter_min\": " << capture.object_filter_min << ",\n";
        object_state_file << "  \"object_filter_max\": " << capture.object_filter_max << ",\n";
        object_state_file << "  \"tool_effective_method\": " << capture.tool_method << ",\n";
        object_state_file << "  \"tool_effective_threshold\": " << capture.tool_threshold << ",\n";
        object_state_file << "  \"scan_rows_examined\": " << capture.scan_rows_examined << ",\n";
        object_state_file << "  \"scan_rows_with_foreground\": " << capture.scan_rows_with_foreground << ",\n";
        object_state_file << "  \"scan_runs_total\": " << capture.scan_runs_total << ",\n";
        object_state_file << "  \"scan_runs_within_length_limit\": " << capture.scan_runs_within_length_limit << ",\n";
        object_state_file << "  \"scan_runs_over_length_limit\": " << capture.scan_runs_over_length_limit << ",\n";
        object_state_file << "  \"scan_runs_rejected_by_selection\": " << capture.scan_runs_rejected_by_selection << ",\n";
        object_state_file << "  \"scan_runs_rejected_near_endpoint\": " << capture.scan_runs_rejected_near_endpoint << ",\n";
        object_state_file << "  \"scan_points_emitted\": " << capture.scan_points_emitted << ",\n";
        object_state_file << "  \"findline_point_consistency_enabled\": " << capture.findline_point_consistency_enabled << ",\n";
        object_state_file << "  \"findline_point_consistency_range\": " << capture.findline_point_consistency_range << ",\n";
        object_state_file << "  \"findline_point_consistency_input_points\": " << capture.findline_point_consistency_input_points << ",\n";
        object_state_file << "  \"findline_point_consistency_output_points\": " << capture.findline_point_consistency_output_points << ",\n";
        object_state_file << "  \"findline_point_consistency_removed_points\": " << capture.findline_point_consistency_removed_points << ",\n";
        object_state_file << "  \"findcircle_point_consistency_enabled\": " << capture.circle_point_consistency_enabled << ",\n";
        object_state_file << "  \"findcircle_point_consistency_range\": " << capture.circle_point_consistency_range << ",\n";
        object_state_file << "  \"findcircle_point_consistency_input_points\": " << capture.circle_point_consistency_input_points << ",\n";
        object_state_file << "  \"findcircle_point_consistency_output_points\": " << capture.circle_point_consistency_output_points << ",\n";
        object_state_file << "  \"findcircle_point_consistency_removed_points\": " << capture.circle_point_consistency_removed_points << ",\n";
        object_state_file << "  \"findobject_foreground_before\": " << capture.object_foreground_before << ",\n";
        object_state_file << "  \"findobject_foreground_after\": " << capture.object_foreground_after << ",\n";
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
        object_state_file << "  \"torch_ok\": " << capture.torch_ok << ",\n";
        object_state_file << "  \"torch_error_code\": " << capture.torch_error_code << ",\n";
        object_state_file << "  \"torch_train_ms\": " << capture.torch_train_ms << ",\n";
        object_state_file << "  \"torch_infer_ms\": " << capture.torch_infer_ms << ",\n";
        object_state_file << "  \"torch_total_ms\": " << capture.torch_total_ms << ",\n";
        object_state_file << "  \"torch_result_count\": " << capture.torch_result_count << ",\n";
        object_state_file << "  \"torch_status\": \"" << JsonEscape(capture.torch_status) << "\",\n";
        object_state_file << "  \"torch_evidence_ref\": \"" << JsonEscape(capture.torch_evidence_ref) << "\",\n";
        object_state_file << "  \"torch_primary_visual_ref\": \"" << JsonEscape(capture.torch_primary_visual_ref) << "\",\n";
        object_state_file << "  \"torch_trainer_lifecycle_summary\": \"" << JsonEscape(capture.torch_trainer_lifecycle_summary) << "\",\n";
        object_state_file << "  \"torch_unified_mainline_summary\": \"" << JsonEscape(capture.torch_unified_mainline_summary) << "\",\n";
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
            capture.rendered_measure_points_count > 0 ||
            (capture.has_result_rect && capture.rendered_result_count > 0);
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
        log_file << "tool_effective_method: " << capture.tool_method << "\n";
        log_file << "tool_effective_threshold: " << capture.tool_threshold << "\n";
        log_file << "scan_rows_examined: " << capture.scan_rows_examined << "\n";
        log_file << "scan_rows_with_foreground: " << capture.scan_rows_with_foreground << "\n";
        log_file << "scan_runs_total: " << capture.scan_runs_total << "\n";
        log_file << "scan_runs_within_length_limit: " << capture.scan_runs_within_length_limit << "\n";
        log_file << "scan_runs_over_length_limit: " << capture.scan_runs_over_length_limit << "\n";
        log_file << "scan_runs_rejected_by_selection: " << capture.scan_runs_rejected_by_selection << "\n";
        log_file << "scan_runs_rejected_near_endpoint: " << capture.scan_runs_rejected_near_endpoint << "\n";
        log_file << "scan_points_emitted: " << capture.scan_points_emitted << "\n";
        log_file << "findline_point_consistency_enabled: " << capture.findline_point_consistency_enabled << "\n";
        log_file << "findline_point_consistency_range: " << capture.findline_point_consistency_range << "\n";
        log_file << "findline_point_consistency_input_points: " << capture.findline_point_consistency_input_points << "\n";
        log_file << "findline_point_consistency_output_points: " << capture.findline_point_consistency_output_points << "\n";
        log_file << "findline_point_consistency_removed_points: " << capture.findline_point_consistency_removed_points << "\n";
        log_file << "findcircle_point_consistency_enabled: " << capture.circle_point_consistency_enabled << "\n";
        log_file << "findcircle_point_consistency_range: " << capture.circle_point_consistency_range << "\n";
        log_file << "findcircle_point_consistency_input_points: " << capture.circle_point_consistency_input_points << "\n";
        log_file << "findcircle_point_consistency_output_points: " << capture.circle_point_consistency_output_points << "\n";
        log_file << "findcircle_point_consistency_removed_points: " << capture.circle_point_consistency_removed_points << "\n";
        log_file << "findobject_foreground_before: " << capture.object_foreground_before << "\n";
        log_file << "findobject_foreground_after: " << capture.object_foreground_after << "\n";
        log_file << "valid_points_count: " << capture.valid_points_count << "\n";
        log_file << "has_fit_line: " << (capture.has_fit_line ? "true" : "false") << "\n";
        log_file << "has_fit_circle: " << (capture.has_fit_circle ? "true" : "false") << "\n";
        log_file << "has_fit_ellipse: " << (capture.has_fit_ellipse ? "true" : "false") << "\n";
        log_file << "has_result_rect: " << (capture.has_result_rect ? "true" : "false") << "\n";
        log_file << "model_point_count: " << capture.model_point_count << "\n";
        log_file << "fastmatch_learn_a_count: " << capture.fastmatch_learn_a_count << "\n";
        log_file << "fastmatch_learn_b_count: " << capture.fastmatch_learn_b_count << "\n";
        log_file << "fastmatch_learn_a2_count: " << capture.fastmatch_learn_a2_count << "\n";
        log_file << "fastmatch_learn_b2_count: " << capture.fastmatch_learn_b2_count << "\n";
        log_file << "fastmatch_learn_status_code: " << capture.fastmatch_learn_status_code << "\n";
        log_file << "fastmatch_learn_rect: "
                 << capture.fastmatch_learn_rect_x0 << ","
                 << capture.fastmatch_learn_rect_y0 << ","
                 << capture.fastmatch_learn_rect_x1 << ","
                 << capture.fastmatch_learn_rect_y1 << "\n";
        log_file << "fastmatch_match_rect: "
                 << capture.fastmatch_match_rect_x0 << ","
                 << capture.fastmatch_match_rect_y0 << ","
                 << capture.fastmatch_match_rect_x1 << ","
                 << capture.fastmatch_match_rect_y1 << "\n";
        log_file << "candidate_count: " << capture.candidate_count << "\n";
        log_file << "best_score: " << capture.best_score << "\n";
        log_file << "torch_ok: " << capture.torch_ok << "\n";
        log_file << "torch_status: " << capture.torch_status << "\n";
        log_file << "torch_train_ms: " << capture.torch_train_ms << "\n";
        log_file << "torch_infer_ms: " << capture.torch_infer_ms << "\n";
        log_file << "torch_total_ms: " << capture.torch_total_ms << "\n";
        log_file << "torch_trainer_lifecycle_summary: " << capture.torch_trainer_lifecycle_summary << "\n";
        log_file << "torch_unified_mainline_summary: " << capture.torch_unified_mainline_summary << "\n";
        log_file << "case_end\n";
        log_file << "run_end\n";
        log_file.close();
    }

    bool snapshot_ok = !result.snapshot_path.empty();
    bool summary_ok = !result.summary_path.empty();
    const bool torch_task_ok = capture.torch_ok != 0;
    const bool segmentation_result_ok =
        capture.segmentation_status_code != 0 ||
        capture.segmentation_contour_count > 0 ||
        capture.rendered_result_count > 0;
    bool evidence_ok = options.contract_context_enabled ||
        (!result.evidence_overlay_path.empty() &&
            (capture.rendered_roi_count > 0 ||
             torch_task_ok ||
             segmentation_result_ok));
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
    result.fastmatch_learn_a_count = capture.fastmatch_learn_a_count;
    result.fastmatch_learn_b_count = capture.fastmatch_learn_b_count;
    result.fastmatch_learn_a2_count = capture.fastmatch_learn_a2_count;
    result.fastmatch_learn_b2_count = capture.fastmatch_learn_b2_count;
    result.fastmatch_learn_status_code = capture.fastmatch_learn_status_code;
    result.candidate_count = capture.candidate_count;
    result.best_score = capture.best_score;
    result.has_result_box = capture.has_result_box;
    result.has_best_result = capture.has_best_result;
    result.circle_radius = capture.circle_radius;
    result.avgdist = capture.avgdist;

    if (options.save_evidence_candidate)
    {
        ManualTestContext candidateContext;
        candidateContext.active_case_id =
            options.case_name.empty() ? options.case_id : options.case_name;
        candidateContext.active_image_id =
            options.image_id.empty() ? options.stage25_image_id : options.image_id;
        candidateContext.active_target_id =
            options.target_id.empty() ? options.stage25_target_id : options.target_id;
        candidateContext.image_file_path = options.image_path;
        candidateContext.loaded_script_path = options.script_path;
        candidateContext.script_file_path = options.script_path;
        candidateContext.editor_source = "headless";
        ReadTextFile(options.script_path, candidateContext.editor_text);
        candidateContext.debug_status =
            result.ok ? "HEADLESS_EXECUTION_PASS" : "HEADLESS_EXECUTION_PARTIAL";
        candidateContext.debug_reason = result.reason;
        candidateContext.current_result_ref.status =
            result.ok ? "runtime_result_available" : "runtime_result_incomplete";
        candidateContext.current_result_ref.reason = result.reason;

        auto setGlobal = [&](const std::string& name, int value)
        {
            candidateContext.runtime_int_vars[name] = value;
        };
        setGlobal("global_roi_x0", options.roi_x0);
        setGlobal("global_roi_y0", options.roi_y0);
        setGlobal("global_roi_x1", options.roi_x1);
        setGlobal("global_roi_y1", options.roi_y1);
        setGlobal("global_circle_cx", options.circle_cx);
        setGlobal("global_circle_cy", options.circle_cy);
        setGlobal("global_circle_px", options.circle_px);
        setGlobal("global_circle_py", options.circle_py);
        setGlobal("global_circle_inner_radius", 0);
        setGlobal("global_circle_outer_radius", 0);
        setGlobal("global_circle_ring_width", 0);
        setGlobal("global_findcircle_arc_enabled", options.findcircle_arc_enabled);
        setGlobal("global_findcircle_arc_start_deg", options.findcircle_arc_start_deg);
        setGlobal("global_findcircle_arc_end_deg", options.findcircle_arc_end_deg);
        setGlobal("global_ellipse_x0", options.ellipse_x0);
        setGlobal("global_ellipse_y0", options.ellipse_y0);
        setGlobal("global_ellipse_x1", options.ellipse_x1);
        setGlobal("global_ellipse_y1", options.ellipse_y1);
        setGlobal("global_tool_half_width", options.tool_half_width);
        setGlobal("global_wgap", options.wgap);
        setGlobal("global_hgap", options.hgap);
        setGlobal("global_gap", options.gap);
        setGlobal("global_linegap", options.linegap);
        setGlobal("global_threshold", options.threshold);
        setGlobal("global_method", options.method);
        setGlobal("global_filterprofile", options.filterprofile);
        setGlobal("global_find_num", options.find_num);
        setGlobal("global_compare_gap", options.compare_gap);
        setGlobal("global_max_elapsed_ms", options.max_elapsed_ms);
        setGlobal("global_max_scan_lines", options.max_scan_lines);
        setGlobal("global_max_samples", options.max_samples);

        ManualGaugeState& gauge = candidateContext.current_gauge;
        gauge.case_id = candidateContext.active_case_id;
        gauge.image_id = candidateContext.active_image_id;
        gauge.target_id = candidateContext.active_target_id;
        gauge.source = "headless";
        gauge.review_status = "pending_human_review";
        gauge.threshold = options.threshold;
        gauge.method = options.method;
        gauge.linegap = options.linegap;
        gauge.wgap = options.wgap;
        gauge.hgap = options.hgap;
        gauge.gap = options.gap;
        gauge.tool_half_width = options.tool_half_width;
        gauge.filterprofile = options.filterprofile;

        std::string lowerScript = options.script_path;
        std::transform(lowerScript.begin(), lowerScript.end(), lowerScript.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        const std::string tool = !options.stage25_tool.empty()
            ? options.stage25_tool
            : (lowerScript.find("circle") != std::string::npos ? "FindCircle" :
               lowerScript.find("ellipse") != std::string::npos ? "FindEllipse" :
               lowerScript.find("rect") != std::string::npos ? "FindRect" :
               lowerScript.find("fastmatch") != std::string::npos ? "FastMatch" :
               "FindLine");
        gauge.tool = tool;
        if (tool == "FindCircle")
        {
            gauge.has_circle_gauge = true;
            gauge.circle_cx = options.circle_cx;
            gauge.circle_cy = options.circle_cy;
            gauge.circle_px = options.circle_px;
            gauge.circle_py = options.circle_py;
        }
        else if (tool == "FindEllipse")
        {
            gauge.has_ellipse_gauge = true;
            gauge.ellipse_x0 = options.ellipse_x0;
            gauge.ellipse_y0 = options.ellipse_y0;
            gauge.ellipse_x1 = options.ellipse_x1;
            gauge.ellipse_y1 = options.ellipse_y1;
        }
        else
        {
            gauge.has_line_gauge = true;
            gauge.line_x0 = options.roi_x0;
            gauge.line_y0 = options.roi_y0;
            gauge.line_x1 = options.roi_x1;
            gauge.line_y1 = options.roi_y1;
        }

        CxEvidenceCandidateSaveOptions candidateOptions;
        candidateOptions.root_dir = options.evidence_candidate_root.empty()
            ? "cxscript_runs/evidence_candidates"
            : options.evidence_candidate_root;
        candidateOptions.candidate_id = options.evidence_candidate_id;
        candidateOptions.mode = "headless_result";
        candidateOptions.request_run = false;
        candidateOptions.add_to_evidence_chain = false;
        candidateOptions.linked_result_summary_path = result.summary_path;
        candidateOptions.linked_result_overlay_path = result.result_overlay_path;
        candidateOptions.linked_evidence_overlay_path = result.evidence_overlay_path;
        candidateOptions.linked_tool_display_path = result.tool_display_path;

        CxEvidenceCandidateSaveResult candidateResult;
        if (!SaveEvidenceCandidatePackage(
                candidateContext,
                candidateOptions,
                candidateResult) &&
            result.reason.empty())
        {
            result.reason = candidateResult.reason;
        }
    }

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
