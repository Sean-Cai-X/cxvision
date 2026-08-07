#include "pch.h"
#include "CxScriptGlobalValueSet.h"
#include "CxScriptHeadlessRuntime.h"
#include "ParserClass.h"

#include <fstream>
#include <sstream>
#include <string>

bool LoadHeadlessGlobalDeclarations(
    const std::string& init_script_path,
    CxScriptGlobalValueSet& values,
    std::string& reason)
{
    std::ifstream script_file(init_script_path);
    if (!script_file.is_open())
    {
        reason = "cannot open headless globals script: " + init_script_path;
        return false;
    }

    std::string line;
    int line_num = 0;
    while (std::getline(script_file, line))
    {
        line_num++;

        const size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos)
            continue;

        if (line.substr(first, 2) == "//")
            continue;

        static const char* numeric_types[] = { "int ", "double ", "float " };
        bool is_numeric = false;
        size_t type_length = 0;
        for (const char* type : numeric_types)
        {
            const size_t len = std::strlen(type);
            if (line.compare(first, len, type) == 0)
            {
                is_numeric = true;
                type_length = len;
                break;
            }
        }

        if (!is_numeric)
        {
            if (line.compare(first, 6, "Image ") == 0)
                continue;

            if (line.find("=") != std::string::npos)
                continue;

            reason = "headless global declaration unsupported at line " +
                std::to_string(line_num) + ": " + line;
            return false;
        }

        size_t name_begin = first + type_length;
        size_t name_end = name_begin;
        while (name_end < line.size() &&
               (std::isalnum(static_cast<unsigned char>(line[name_end])) || line[name_end] == '_'))
        {
            ++name_end;
        }

        if (name_end == name_begin)
            continue;

        const std::string name = line.substr(name_begin, name_end - name_begin);

        if (name.find("global.") != std::string::npos)
        {
            reason = "headless global declaration uses dot notation at line " +
                std::to_string(line_num) + ": " + name;
            return false;
        }

        if (values.numbers.count(name) > 0)
        {
            reason = "duplicate headless global declaration at line " +
                std::to_string(line_num) + ": " + name;
            return false;
        }

        values.numbers[name] = 0.0;
    }

    if (values.numbers.empty())
    {
        reason = "no global_* declarations found in: " + init_script_path;
        return false;
    }

    return true;
}

bool ApplyGlobalOverrides(
    CxScriptGlobalValueSet& values,
    const std::map<std::string, double>& overrides,
    std::string& reason)
{
    for (const auto& [name, value] : overrides)
    {
        auto it = values.numbers.find(name);
        if (it == values.numbers.end())
        {
            reason = "global override not declared in headless_globals.cxsc: " + name;
            return false;
        }
        it->second = value;
    }
    return true;
}

bool BindGlobalValueSetToParser(
    mu::CxParserRuntime& runtime,
    CxScriptGlobalValueSet& values,
    std::string& reason)
{
    for (auto& [name, value] : values.numbers)
    {
        runtime.m_parser.DefineVar(name, &value);
    }
    (void)reason;
    return true;
}

std::map<std::string, double> BuildHeadlessGlobalOverrides(
    const CxScriptHeadlessOptions& options)
{
    std::map<std::string, double> overrides;

    overrides["global_roi_x0"] = static_cast<double>(options.roi_x0);
    overrides["global_roi_y0"] = static_cast<double>(options.roi_y0);
    overrides["global_roi_x1"] = static_cast<double>(options.roi_x1);
    overrides["global_roi_y1"] = static_cast<double>(options.roi_y1);

    overrides["global_circle_cx"] = static_cast<double>(options.circle_cx);
    overrides["global_circle_cy"] = static_cast<double>(options.circle_cy);
    overrides["global_circle_px"] = static_cast<double>(options.circle_px);
    overrides["global_circle_py"] = static_cast<double>(options.circle_py);
    overrides["global_findcircle_arc_enabled"] = static_cast<double>(options.findcircle_arc_enabled);
    overrides["global_findcircle_arc_start_deg"] = static_cast<double>(options.findcircle_arc_start_deg);
    overrides["global_findcircle_arc_end_deg"] = static_cast<double>(options.findcircle_arc_end_deg);

    overrides["global_ellipse_x0"] = static_cast<double>(options.ellipse_x0);
    overrides["global_ellipse_y0"] = static_cast<double>(options.ellipse_y0);
    overrides["global_ellipse_x1"] = static_cast<double>(options.ellipse_x1);
    overrides["global_ellipse_y1"] = static_cast<double>(options.ellipse_y1);

    overrides["global_tool_half_width"] = static_cast<double>(options.tool_half_width);
    overrides["global_wgap"] = static_cast<double>(options.wgap);
    overrides["global_hgap"] = static_cast<double>(options.hgap);
    overrides["global_gap"] = static_cast<double>(options.gap);
    overrides["global_linegap"] = static_cast<double>(options.linegap);
    overrides["global_threshold"] = static_cast<double>(options.threshold);
    overrides["global_method"] = static_cast<double>(options.method);
    overrides["global_filterprofile"] = static_cast<double>(options.filterprofile);
    overrides["global_findsetting"] = 0.0;
    overrides["global_objfilter"] = 1.0;
    overrides["global_findline_objfilter"] = 1.0;
    overrides["global_findcircle_findsetting"] = 0.0;
    overrides["global_findellipse_findsetting"] = 1.0;
    overrides["global_findrect_findsetting"] = 0.0;
    overrides["global_findline_point_consistency_enabled"] = 0.0;
    overrides["global_findline_point_consistency_range"] = 0.0;
    overrides["global_findcircle_point_consistency_enabled"] = 0.0;
    overrides["global_findcircle_point_consistency_range"] = 0.0;
    overrides["global_samplerate"] = static_cast<double>(options.samplerate);
    overrides["global_min_score"] = options.min_score;
    overrides["global_min_score_percent"] = 0.0;
    overrides["global_find_num"] = static_cast<double>(options.find_num);
    overrides["global_compare_gap"] = static_cast<double>(options.compare_gap);
    overrides["global_match_step_x"] = 10.0;
    overrides["global_match_step_y"] = 10.0;
    overrides["global_match_thre"] = 10.0;
    overrides["global_fastmatch_action"] = 3.0;
    overrides["global_strategy_id"] = static_cast<double>(options.strategy_id);
    overrides["global_algorithm_executed"] = static_cast<double>(options.algorithm_executed);

    overrides["global_learn_roi_x"] = static_cast<double>(options.learn_roi_x);
    overrides["global_learn_roi_y"] = static_cast<double>(options.learn_roi_y);
    overrides["global_learn_roi_w"] = static_cast<double>(options.learn_roi_w);
    overrides["global_learn_roi_h"] = static_cast<double>(options.learn_roi_h);
    overrides["global_search_roi_x"] = static_cast<double>(options.search_roi_x);
    overrides["global_search_roi_y"] = static_cast<double>(options.search_roi_y);
    overrides["global_search_roi_w"] = static_cast<double>(options.search_roi_w);
    overrides["global_search_roi_h"] = static_cast<double>(options.search_roi_h);
    overrides["global_expected_rect_x"] = static_cast<double>(options.expected_rect_x);
    overrides["global_expected_rect_y"] = static_cast<double>(options.expected_rect_y);
    overrides["global_expected_rect_w"] = static_cast<double>(options.expected_rect_w);
    overrides["global_expected_rect_h"] = static_cast<double>(options.expected_rect_h);
    overrides["global_learn_a_count"] = 0.0;
    overrides["global_learn_b_count"] = 0.0;
    overrides["global_learn_a2_count"] = 0.0;
    overrides["global_learn_b2_count"] = 0.0;
    overrides["global_learn_status_code"] = 0.0;
    overrides["global_match_count"] = 0.0;
    overrides["global_best_score"] = 0.0;
    overrides["global_model_point_count"] = 0.0;

    overrides["global_max_elapsed_ms"] = static_cast<double>(options.max_elapsed_ms);
    overrides["global_max_scan_lines"] = static_cast<double>(options.max_scan_lines);
    overrides["global_max_samples"] = static_cast<double>(options.max_samples);

    return overrides;
}

namespace
{
bool IsValidGlobalName(const std::string& name)
{
    if (name.empty())
        return false;
    if (name.size() < 8 || name.compare(0, 7, "global_") != 0)
        return false;
    for (size_t i = 7; i < name.size(); ++i)
    {
        char c = name[i];
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_')
            return false;
    }
    return true;
}

std::string Trim(const std::string& str)
{
    const size_t first = str.find_first_not_of(" \t");
    if (first == std::string::npos)
        return "";
    const size_t last = str.find_last_not_of(" \t");
    return str.substr(first, last - first + 1);
}
}

bool LoadHeadlessGlobalValuesFile(
    const std::string& values_path,
    std::map<std::string, double>& overrides,
    std::string& reason)
{
    std::ifstream file(values_path);
    if (!file.is_open())
    {
        reason = "cannot open headless globals values file: " + values_path;
        return false;
    }

    std::string line;
    int line_num = 0;
    while (std::getline(file, line))
    {
        line_num++;

        const size_t comment_pos = line.find("//");
        if (comment_pos != std::string::npos)
            line = line.substr(0, comment_pos);

        std::string trimmed = Trim(line);
        if (trimmed.empty())
            continue;

        const size_t eq_pos = trimmed.find('=');
        if (eq_pos == std::string::npos)
        {
            reason = "invalid headless globals value at line " +
                std::to_string(line_num) + ": " + line;
            return false;
        }

        std::string name_part = Trim(trimmed.substr(0, eq_pos));
        std::string value_part = Trim(trimmed.substr(eq_pos + 1));

        if (name_part.empty())
        {
            reason = "invalid headless globals value at line " +
                std::to_string(line_num) + ": " + line;
            return false;
        }

        if (name_part.find('.') != std::string::npos)
        {
            reason = "invalid headless globals value at line " +
                std::to_string(line_num) + ": dot notation not allowed";
            return false;
        }

        if (!IsValidGlobalName(name_part))
        {
            reason = "invalid headless globals value at line " +
                std::to_string(line_num) + ": " + name_part;
            return false;
        }

        if (value_part.empty())
        {
            reason = "invalid headless globals value at line " +
                std::to_string(line_num) + ": " + line;
            return false;
        }

        if (value_part.back() == ';')
            value_part = value_part.substr(0, value_part.size() - 1);

        try
        {
            size_t consumed = 0;
            double value = std::stod(value_part, &consumed);
            if (consumed != value_part.size())
            {
                reason = "invalid headless globals value at line " +
                    std::to_string(line_num) + ": " + line;
                return false;
            }

            if (overrides.count(name_part) > 0)
            {
                reason = "duplicate headless globals value at line " +
                    std::to_string(line_num) + ": " + name_part;
                return false;
            }

            overrides[name_part] = value;
        }
        catch (const std::exception&)
        {
            reason = "invalid headless globals value at line " +
                std::to_string(line_num) + ": " + line;
            return false;
        }
    }

    return true;
}
