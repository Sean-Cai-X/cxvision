#include "pch.h"
#include "ManualConsoleGauge.h"
#include "ManualConsoleUtils.h"
#include "ManualConsoleCxScriptDebug.h"
#include "CxScriptHeadlessRuntime.h"

#include <sstream>
#include <fstream>
#include <cmath>
#include <regex>
#include <algorithm>

namespace
{
std::string GaugeJsonEscape(const std::string& value)
{
    std::string out;
    out.reserve(value.size());
    for (char ch : value)
    {
        if (ch == '\\' || ch == '"')
            out.push_back('\\');
        if (ch == '\n')
            out += "\\n";
        else if (ch != '\r')
            out.push_back(ch);
    }
    return out;
}

std::string SafePathComponent(const std::string& value)
{
    std::string out;
    out.reserve(value.size());
    for (unsigned char ch : value)
    {
        if (std::isalnum(ch) || ch == '-' || ch == '_')
            out.push_back(static_cast<char>(ch));
        else
            out.push_back('_');
    }
    while (out.find("..") != std::string::npos)
        out.replace(out.find(".."), 2, "__");
    return out;
}

bool ExtractJsonString(const std::string& source, const char* key, std::string& value)
{
    const std::regex pattern(std::string("\\\"") + key + "\\\"\\s*:\\s*\\\"([^\\\"]*)\\\"");
    std::smatch match;
    if (!std::regex_search(source, match, pattern))
        return false;
    value = match[1].str();
    return true;
}

bool ExtractJsonInt(const std::string& source, const char* key, int& value)
{
    const std::regex pattern(std::string("\\\"") + key + "\\\"\\s*:\\s*(-?[0-9]+)");
    std::smatch match;
    if (!std::regex_search(source, match, pattern))
        return false;
    value = std::stoi(match[1].str());
    return true;
}

bool ExtractJsonBool(const std::string& source, const char* key, bool& value)
{
    const std::regex pattern(std::string("\\\"") + key + "\\\"\\s*:\\s*(true|false)");
    std::smatch match;
    if (!std::regex_search(source, match, pattern))
        return false;
    value = match[1].str() == "true";
    return true;
}

bool AtomicReplaceFile(
    const std::filesystem::path& temporary,
    const std::filesystem::path& destination,
    std::string& reason)
{
#ifdef _WIN32
    if (!MoveFileExW(
            temporary.c_str(),
            destination.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        reason = "cannot atomically replace file: " + destination.string();
        return false;
    }
#else
    std::error_code ec;
    std::filesystem::rename(temporary, destination, ec);
    if (ec)
    {
        reason = "cannot atomically replace file: " + ec.message();
        return false;
    }
#endif
    return true;
}
}

void InjectManualGaugeInt(ManualTestContext& context, const std::string& key, int value)
{
    context.runtime_int_vars[key] = value;
    UpsertGlobalVariableView(
        context,
        "int",
        key,
        std::to_string(value),
        0,
        "manual_gauge_applied");
}

bool ValidateManualGaugeGeometryForEditing(
    const ManualGaugeState& gauge,
    std::string& reason)
{
    if (gauge.tool == "FindLine")
    {
        if (!gauge.has_line_gauge)
            reason = "line gauge is unavailable";
        else if (gauge.line_x0 == gauge.line_x1 && gauge.line_y0 == gauge.line_y1)
            reason = "line gauge length is zero";
        else if (gauge.tool_half_width <= 0)
            reason = "tool_half_width must be positive";
        else
            reason.clear();
        return reason.empty();
    }
    if (gauge.tool == "FindCircle")
    {
        if (!gauge.has_circle_gauge)
            reason = "circle gauge is unavailable";
        else if (gauge.circle_px == gauge.circle_cx && gauge.circle_py == gauge.circle_cy)
            reason = "circle radius is zero";
        else
            reason.clear();
        return reason.empty();
    }
    if (gauge.tool == "FindEllipse")
    {
        if (!gauge.has_ellipse_gauge)
            reason = "ellipse gauge is unavailable";
        else if (gauge.ellipse_x0 == gauge.ellipse_x1 ||
                 gauge.ellipse_y0 == gauge.ellipse_y1)
            reason = "ellipse gauge radius is zero";
        else
            reason.clear();
        return reason.empty();
    }
    reason = "unsupported gauge tool";
    return false;
}

bool ValidateManualGaugeGeometry(const ManualGaugeState& gauge, std::string& reason)
{
    if (!gauge.accepted)
    {
        reason = "gauge is not accepted";
        return false;
    }
    if (gauge.review_status != "manual_accepted")
    {
        reason = "review_status is not manual_accepted";
        return false;
    }
    if (gauge.dirty)
    {
        reason = "gauge was edited after acceptance";
        return false;
    }
    return ValidateManualGaugeGeometryForEditing(gauge, reason);
}

void NormalizeManualGaugeGeometry(ManualGaugeState& gauge)
{
    if (!gauge.has_circle_gauge)
        return;
    const double dx = static_cast<double>(gauge.circle_px - gauge.circle_cx);
    const double dy = static_cast<double>(gauge.circle_py - gauge.circle_cy);
    gauge.radius = static_cast<int>(std::lround(std::sqrt(dx * dx + dy * dy)));
}

bool ApplyManualGaugeToGlobals(ManualTestContext& context, const std::string& objectName)
{
    (void)objectName;
    return ApplyManualGaugeToGlobals(context);
}

bool ApplyManualGaugeToGlobals(ManualTestContext& context)
{
    ManualGaugeState& gauge = context.current_gauge;
    std::string reason;
    if (!ValidateManualGaugeGeometryForEditing(gauge, reason))
    {
        context.debug_status = "gauge_apply_failed";
        context.debug_reason = reason;
        return false;
    }
    if (gauge.tool == "FindLine")
    {
        InjectManualGaugeInt(context, "global_roi_x0", gauge.line_x0);
        InjectManualGaugeInt(context, "global_roi_y0", gauge.line_y0);
        InjectManualGaugeInt(context, "global_roi_x1", gauge.line_x1);
        InjectManualGaugeInt(context, "global_roi_y1", gauge.line_y1);
        InjectManualGaugeInt(context, "global_tool_half_width", gauge.tool_half_width);
        InjectManualGaugeInt(context, "global_wgap", gauge.wgap);
        InjectManualGaugeInt(context, "global_hgap", gauge.hgap);
        InjectManualGaugeInt(context, "global_linegap", gauge.linegap);
        InjectManualGaugeInt(context, "global_threshold", gauge.threshold);
        InjectManualGaugeInt(context, "global_filterprofile", gauge.filterprofile);
        InjectManualGaugeInt(context, "global_method", gauge.method);

        context.findline_scan_edge_count =
            std::max(1, std::min(16, context.findline_scan_edge_count));
        context.findline_selected_scan_edge =
            std::max(0, std::min(context.findline_selected_scan_edge,
                                 context.findline_scan_edge_count));
        if (context.findline_edge_params.size() <
            static_cast<std::size_t>(context.findline_scan_edge_count + 1))
        {
            context.findline_edge_params.resize(
                static_cast<std::size_t>(context.findline_scan_edge_count + 1));
        }

        InjectManualGaugeInt(
            context,
            "global_findline_edge_count",
            context.findline_scan_edge_count);
        InjectManualGaugeInt(
            context,
            "global_findline_selected_edge",
            context.findline_selected_scan_edge);
        context.findline_best_fit_edge =
            std::max(0, std::min(context.findline_best_fit_edge,
                                 context.findline_scan_edge_count));
        context.findline_recommended_fit_edge =
            std::max(0, std::min(context.findline_recommended_fit_edge,
                                 context.findline_scan_edge_count));
        context.findline_relation_edge =
            std::max(0, std::min(context.findline_relation_edge,
                                 context.findline_scan_edge_count));
        context.findline_attach_edge =
            std::max(0, std::min(context.findline_attach_edge,
                                 context.findline_scan_edge_count));
        InjectManualGaugeInt(
            context,
            "global_findline_best_edge",
            context.findline_best_fit_edge);
        InjectManualGaugeInt(
            context,
            "global_findline_recommended_edge",
            context.findline_recommended_fit_edge);
        InjectManualGaugeInt(
            context,
            "global_findline_relation_edge",
            context.findline_relation_edge);
        InjectManualGaugeInt(
            context,
            "global_findline_attach_edge",
            context.findline_attach_edge);

        for (int edge = 1; edge <= context.findline_scan_edge_count; ++edge)
        {
            ManualFindLineEdgeParamState& params =
                context.findline_edge_params[static_cast<std::size_t>(edge)];
            if (!params.initialized)
            {
                params.initialized = true;
                params.threshold = gauge.threshold;
                params.method = gauge.method;
                params.linegap = gauge.linegap;
                params.wgap = gauge.wgap;
                params.hgap = gauge.hgap;
                params.filterprofile = gauge.filterprofile;
            }
            const std::string prefix =
                "global_findline_edge" + std::to_string(edge) + "_";
            InjectManualGaugeInt(context, prefix + "threshold", params.threshold);
            InjectManualGaugeInt(context, prefix + "method", params.method);
            InjectManualGaugeInt(context, prefix + "linegap", params.linegap);
            InjectManualGaugeInt(context, prefix + "wgap", params.wgap);
            InjectManualGaugeInt(context, prefix + "hgap", params.hgap);
            InjectManualGaugeInt(context, prefix + "filterprofile", params.filterprofile);
        }

        if (context.findline_selected_scan_edge > 0)
        {
            const ManualFindLineEdgeParamState& selected =
                context.findline_edge_params[
                    static_cast<std::size_t>(context.findline_selected_scan_edge)];
            InjectManualGaugeInt(context, "global_findline_selected_threshold", selected.threshold);
            InjectManualGaugeInt(context, "global_findline_selected_method", selected.method);
            InjectManualGaugeInt(context, "global_findline_selected_linegap", selected.linegap);
            InjectManualGaugeInt(context, "global_findline_selected_wgap", selected.wgap);
            InjectManualGaugeInt(context, "global_findline_selected_hgap", selected.hgap);
            InjectManualGaugeInt(context, "global_findline_selected_filterprofile", selected.filterprofile);
        }
    }
    else if (gauge.tool == "FindCircle")
    {
        NormalizeManualGaugeGeometry(gauge);
        int effectiveOuterRadius = gauge.outer_radius > 0
            ? gauge.outer_radius
            : gauge.radius;
        if (effectiveOuterRadius <= 0)
            effectiveOuterRadius = gauge.radius;
        if (effectiveOuterRadius <= 0)
            effectiveOuterRadius = 1;

        int effectiveInnerRadius = gauge.inner_radius > 0
            ? gauge.inner_radius
            : 0;
        if (effectiveInnerRadius >= effectiveOuterRadius)
            effectiveInnerRadius = std::max(0, effectiveOuterRadius - 1);

        const double dx = static_cast<double>(gauge.circle_px - gauge.circle_cx);
        const double dy = static_cast<double>(gauge.circle_py - gauge.circle_cy);
        const double len = std::sqrt(dx * dx + dy * dy);
        const double ux = len > 1.0 ? dx / len : 1.0;
        const double uy = len > 1.0 ? dy / len : 0.0;
        gauge.radius = effectiveOuterRadius;
        gauge.circle_px = gauge.circle_cx +
            static_cast<int>(std::lround(ux * effectiveOuterRadius));
        gauge.circle_py = gauge.circle_cy +
            static_cast<int>(std::lround(uy * effectiveOuterRadius));
        gauge.inner_radius = effectiveInnerRadius;
        gauge.outer_radius = effectiveOuterRadius;

        InjectManualGaugeInt(context, "global_circle_cx", gauge.circle_cx);
        InjectManualGaugeInt(context, "global_circle_cy", gauge.circle_cy);
        InjectManualGaugeInt(context, "global_circle_px", gauge.circle_px);
        InjectManualGaugeInt(context, "global_circle_py", gauge.circle_py);
        InjectManualGaugeInt(context, "global_circle_inner_radius", gauge.inner_radius);
        InjectManualGaugeInt(context, "global_circle_outer_radius", gauge.outer_radius);
        InjectManualGaugeInt(
            context,
            "global_circle_ring_width",
            std::max(0, gauge.outer_radius - gauge.inner_radius));
        gauge.circle_arc_start_deg = std::max(-359, std::min(360, gauge.circle_arc_start_deg));
        gauge.circle_arc_end_deg = std::max(-359, std::min(360, gauge.circle_arc_end_deg));
        InjectManualGaugeInt(context, "global_findcircle_arc_enabled", gauge.circle_arc_enabled ? 1 : 0);
        InjectManualGaugeInt(context, "global_findcircle_arc_start_deg", gauge.circle_arc_start_deg);
        InjectManualGaugeInt(context, "global_findcircle_arc_end_deg", gauge.circle_arc_end_deg);
        InjectManualGaugeInt(context, "global_gap", gauge.gap);
        InjectManualGaugeInt(context, "global_linegap", gauge.linegap);
        InjectManualGaugeInt(context, "global_threshold", gauge.threshold);
        InjectManualGaugeInt(context, "global_method", gauge.method);

        context.findcircle_scan_edge_count =
            std::max(1, std::min(32, context.findcircle_scan_edge_count));
        context.findcircle_selected_scan_edge =
            std::max(0, std::min(context.findcircle_selected_scan_edge,
                                 context.findcircle_scan_edge_count));
        if (context.findcircle_edge_params.size() <
            static_cast<std::size_t>(context.findcircle_scan_edge_count + 1))
        {
            context.findcircle_edge_params.resize(
                static_cast<std::size_t>(context.findcircle_scan_edge_count + 1));
        }

        InjectManualGaugeInt(
            context,
            "global_findcircle_edge_count",
            context.findcircle_scan_edge_count);
        InjectManualGaugeInt(
            context,
            "global_findcircle_selected_edge",
            context.findcircle_selected_scan_edge);
        context.findcircle_best_fit_edge =
            std::max(0, std::min(context.findcircle_best_fit_edge,
                                 context.findcircle_scan_edge_count));
        context.findcircle_recommended_fit_edge =
            std::max(0, std::min(context.findcircle_recommended_fit_edge,
                                 context.findcircle_scan_edge_count));
        context.findcircle_relation_edge =
            std::max(0, std::min(context.findcircle_relation_edge,
                                 context.findcircle_scan_edge_count));
        context.findcircle_attach_edge =
            std::max(0, std::min(context.findcircle_attach_edge,
                                 context.findcircle_scan_edge_count));
        InjectManualGaugeInt(
            context,
            "global_findcircle_best_edge",
            context.findcircle_best_fit_edge);
        InjectManualGaugeInt(
            context,
            "global_findcircle_recommended_edge",
            context.findcircle_recommended_fit_edge);
        InjectManualGaugeInt(
            context,
            "global_findcircle_relation_edge",
            context.findcircle_relation_edge);
        InjectManualGaugeInt(
            context,
            "global_findcircle_attach_edge",
            context.findcircle_attach_edge);

        for (int edge = 1; edge <= context.findcircle_scan_edge_count; ++edge)
        {
            ManualFindCircleEdgeParamState& params =
                context.findcircle_edge_params[static_cast<std::size_t>(edge)];
            if (!params.initialized)
            {
                params.initialized = true;
                params.threshold = gauge.threshold;
                params.method = gauge.method;
                params.linegap = gauge.linegap;
                params.gap = gauge.gap;
            }
            const std::string prefix =
                "global_findcircle_edge" + std::to_string(edge) + "_";
            InjectManualGaugeInt(context, prefix + "threshold", params.threshold);
            InjectManualGaugeInt(context, prefix + "method", params.method);
            InjectManualGaugeInt(context, prefix + "linegap", params.linegap);
            InjectManualGaugeInt(context, prefix + "gap", params.gap);
        }

        if (context.findcircle_selected_scan_edge > 0)
        {
            const ManualFindCircleEdgeParamState& selected =
                context.findcircle_edge_params[
                    static_cast<std::size_t>(context.findcircle_selected_scan_edge)];
            InjectManualGaugeInt(context, "global_findcircle_selected_threshold", selected.threshold);
            InjectManualGaugeInt(context, "global_findcircle_selected_method", selected.method);
            InjectManualGaugeInt(context, "global_findcircle_selected_linegap", selected.linegap);
            InjectManualGaugeInt(context, "global_findcircle_selected_gap", selected.gap);
        }
    }
    else if (gauge.tool == "FindEllipse")
    {
        InjectManualGaugeInt(context, "global_ellipse_x0", gauge.ellipse_x0);
        InjectManualGaugeInt(context, "global_ellipse_y0", gauge.ellipse_y0);
        InjectManualGaugeInt(context, "global_ellipse_x1", gauge.ellipse_x1);
        InjectManualGaugeInt(context, "global_ellipse_y1", gauge.ellipse_y1);
        InjectManualGaugeInt(context, "global_gap", gauge.gap);
        InjectManualGaugeInt(context, "global_linegap", gauge.linegap);
        InjectManualGaugeInt(context, "global_threshold", gauge.threshold);
        InjectManualGaugeInt(context, "global_method", gauge.method);
    }
    else
    {
        context.debug_status = "gauge_apply_failed";
        context.debug_reason = "unsupported gauge tool: " + gauge.tool;
        return false;
    }
    gauge.review_status = "applied_to_globals";
    gauge.accepted = false;
    context.debug_status = "gauge_applied";
    context.debug_reason.clear();
    return true;
}

std::filesystem::path ManualGaugeCaseDir(const ManualTestContext& context)
{
    std::filesystem::path out;
    std::string reason;
    if (!ResolveManualGaugeCaseDir(context, out, reason))
        return {};
    return out;
}

bool ResolveManualGaugeCaseDir(
    const ManualTestContext& context,
    std::filesystem::path& out,
    std::string& reason)
{
    const std::string case_id = context.current_gauge.case_id.empty()
        ? context.active_case_id
        : context.current_gauge.case_id;
    const std::string safe_case_id = SafePathComponent(case_id);
    if (safe_case_id.empty())
    {
        reason = "case_id is empty";
        return false;
    }
    if (context.manual_gauge_output_root.empty())
    {
        reason = "manual_gauge_output_root is empty";
        return false;
    }
    std::error_code ec;
    std::filesystem::path root = std::filesystem::absolute(context.manual_gauge_output_root, ec);
    if (ec)
    {
        reason = "cannot resolve manual gauge output root: " + ec.message();
        return false;
    }
    out = (root / safe_case_id).lexically_normal();
    const std::filesystem::path relative = out.lexically_relative(root.lexically_normal());
    if (relative.empty() || relative.native().find(L"..") == 0)
    {
        reason = "resolved case directory escapes output root";
        return false;
    }
    reason.clear();
    return true;
}

bool SaveManualGaugeAnnotation(
    ManualTestContext& context,
    const std::string& objectName,
    const std::string& gaugeName,
    std::string& outPath,
    std::string& outReason)
{
    (void)objectName;
    (void)gaugeName;
    ManualGaugeState& gauge = context.current_gauge;
    NormalizeManualGaugeGeometry(gauge);
    if (!ValidateManualGaugeGeometry(gauge, outReason))
        return false;

    std::filesystem::path dir;
    if (!ResolveManualGaugeCaseDir(context, dir, outReason))
        return false;
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec)
    {
        outReason = "cannot create gauge case directory: " + ec.message();
        return false;
    }

    const std::filesystem::path destination = dir / "gauge_annotation.json";
    const std::filesystem::path temporary = destination.string() + ".tmp";
    std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
    if (!file.is_open())
    {
        outReason = "cannot open temporary gauge annotation";
        return false;
    }
    file << "{\n"
         << "  \"schema_version\": 1,\n"
         << "  \"case_id\": \"" << GaugeJsonEscape(gauge.case_id) << "\",\n"
         << "  \"image_id\": \"" << GaugeJsonEscape(gauge.image_id) << "\",\n"
         << "  \"target_id\": \"" << GaugeJsonEscape(gauge.target_id) << "\",\n"
         << "  \"tool\": \"" << GaugeJsonEscape(gauge.tool) << "\",\n"
         << "  \"source\": \"" << GaugeJsonEscape(gauge.source) << "\",\n"
         << "  \"review_status\": \"" << GaugeJsonEscape(gauge.review_status) << "\",\n"
         << "  \"accepted\": " << (gauge.accepted ? "true" : "false") << ",\n"
         << "  \"has_line_gauge\": " << (gauge.has_line_gauge ? "true" : "false") << ",\n"
         << "  \"line_x0\": " << gauge.line_x0 << ",\n"
         << "  \"line_y0\": " << gauge.line_y0 << ",\n"
         << "  \"line_x1\": " << gauge.line_x1 << ",\n"
         << "  \"line_y1\": " << gauge.line_y1 << ",\n"
         << "  \"tool_half_width\": " << gauge.tool_half_width << ",\n"
         << "  \"has_circle_gauge\": " << (gauge.has_circle_gauge ? "true" : "false") << ",\n"
         << "  \"circle_cx\": " << gauge.circle_cx << ",\n"
         << "  \"circle_cy\": " << gauge.circle_cy << ",\n"
         << "  \"circle_px\": " << gauge.circle_px << ",\n"
         << "  \"circle_py\": " << gauge.circle_py << ",\n"
         << "  \"radius\": " << gauge.radius << ",\n"
         << "  \"inner_radius\": " << gauge.inner_radius << ",\n"
         << "  \"outer_radius\": " << gauge.outer_radius << ",\n"
         << "  \"circle_arc_enabled\": " << (gauge.circle_arc_enabled ? "true" : "false") << ",\n"
         << "  \"circle_arc_start_deg\": " << gauge.circle_arc_start_deg << ",\n"
         << "  \"circle_arc_end_deg\": " << gauge.circle_arc_end_deg << ",\n"
         << "  \"has_ellipse_gauge\": " << (gauge.has_ellipse_gauge ? "true" : "false") << ",\n"
         << "  \"ellipse_x0\": " << gauge.ellipse_x0 << ",\n"
         << "  \"ellipse_y0\": " << gauge.ellipse_y0 << ",\n"
         << "  \"ellipse_x1\": " << gauge.ellipse_x1 << ",\n"
         << "  \"ellipse_y1\": " << gauge.ellipse_y1 << ",\n"
         << "  \"wgap\": " << gauge.wgap << ",\n"
         << "  \"hgap\": " << gauge.hgap << ",\n"
         << "  \"gap\": " << gauge.gap << ",\n"
         << "  \"linegap\": " << gauge.linegap << ",\n"
         << "  \"threshold\": " << gauge.threshold << ",\n"
         << "  \"filterprofile\": " << gauge.filterprofile << ",\n"
         << "  \"method\": " << gauge.method << "\n"
         << "}\n";
    file.flush();
    const bool write_ok = file.good();
    file.close();
    if (!write_ok)
    {
        outReason = "failed while writing gauge annotation";
        std::filesystem::remove(temporary, ec);
        return false;
    }
    if (!AtomicReplaceFile(temporary, destination, outReason))
    {
        std::filesystem::remove(temporary, ec);
        return false;
    }
    outPath = destination.string();
    outReason.clear();
    return true;
}

bool LoadManualGaugeAnnotation(
    ManualTestContext& context,
    const std::string& objectName,
    const std::string& gaugeName,
    std::string& outPath,
    std::string& outReason)
{
    (void)objectName;
    (void)gaugeName;
    std::filesystem::path dir;
    if (!ResolveManualGaugeCaseDir(context, dir, outReason))
        return false;
    const std::filesystem::path source_path = dir / "gauge_annotation.json";
    if (!LoadManualGaugeAnnotationFromPath(context, source_path, outReason))
        return false;
    outPath = source_path.string();
    return true;
}

static bool LoadManualGaugeAnnotationFromPathImpl(
    ManualTestContext& context,
    const std::filesystem::path& source_path,
    bool requireManualAcceptance,
    std::string& outReason)
{
    std::ifstream file(source_path, std::ios::binary);
    if (!file.is_open())
    {
        outReason = "cannot open gauge annotation: " + source_path.string();
        return false;
    }
    const std::string source{
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()};
    ManualGaugeState loaded;
    int schema_version = 0;
    if (!ExtractJsonInt(source, "schema_version", schema_version) || schema_version != 1 ||
        !ExtractJsonString(source, "case_id", loaded.case_id) ||
        !ExtractJsonString(source, "image_id", loaded.image_id) ||
        !ExtractJsonString(source, "target_id", loaded.target_id) ||
        !ExtractJsonString(source, "tool", loaded.tool) ||
        !ExtractJsonString(source, "source", loaded.source) ||
        !ExtractJsonString(source, "review_status", loaded.review_status) ||
        !ExtractJsonBool(source, "accepted", loaded.accepted) ||
        !ExtractJsonBool(source, "has_line_gauge", loaded.has_line_gauge) ||
        !ExtractJsonBool(source, "has_circle_gauge", loaded.has_circle_gauge))
    {
        outReason = "gauge annotation schema is incomplete";
        return false;
    }
    // Backward compatible for annotations saved before FindEllipse gauge fields.
    ExtractJsonBool(source, "has_ellipse_gauge", loaded.has_ellipse_gauge);
    ExtractJsonBool(source, "circle_arc_enabled", loaded.circle_arc_enabled);
    const char* integer_keys[] = {
        "line_x0", "line_y0", "line_x1", "line_y1", "tool_half_width",
        "circle_cx", "circle_cy", "circle_px", "circle_py", "radius",
        "inner_radius", "outer_radius", "circle_arc_start_deg", "circle_arc_end_deg", "ellipse_x0", "ellipse_y0",
        "ellipse_x1", "ellipse_y1", "wgap", "hgap", "gap", "linegap",
        "threshold", "filterprofile", "method"
    };
    int* integer_values[] = {
        &loaded.line_x0, &loaded.line_y0, &loaded.line_x1, &loaded.line_y1,
        &loaded.tool_half_width, &loaded.circle_cx, &loaded.circle_cy,
        &loaded.circle_px, &loaded.circle_py, &loaded.radius,
        &loaded.inner_radius, &loaded.outer_radius, &loaded.circle_arc_start_deg, &loaded.circle_arc_end_deg, &loaded.ellipse_x0,
        &loaded.ellipse_y0, &loaded.ellipse_x1, &loaded.ellipse_y1,
        &loaded.wgap, &loaded.hgap, &loaded.gap, &loaded.linegap,
        &loaded.threshold, &loaded.filterprofile, &loaded.method
    };
    for (std::size_t i = 0; i < std::size(integer_keys); ++i)
    {
        if (!ExtractJsonInt(source, integer_keys[i], *integer_values[i]))
        {
            if (std::string(integer_keys[i]).find("ellipse_") == 0 &&
                !loaded.has_ellipse_gauge)
            {
                continue;
            }
            if (std::string(integer_keys[i]).find("circle_arc_") == 0)
            {
                // Saved annotations from before the sector contract are full
                // circle by definition; retain the struct defaults.
                continue;
            }
            outReason = std::string("missing gauge field: ") + integer_keys[i];
            return false;
        }
    }
    loaded.dirty = false;
    NormalizeManualGaugeGeometry(loaded);
    const bool valid = requireManualAcceptance
        ? ValidateManualGaugeGeometry(loaded, outReason)
        : ValidateManualGaugeGeometryForEditing(loaded, outReason);
    if (!valid)
        return false;
    context.current_gauge = loaded;
    outReason.clear();
    return true;
}

bool LoadManualGaugeAnnotationFromPath(
    ManualTestContext& context,
    const std::filesystem::path& source_path,
    std::string& outReason)
{
    return LoadManualGaugeAnnotationFromPathImpl(
        context,
        source_path,
        true,
        outReason);
}

bool LoadManualGaugeWorkingCopyFromPath(
    ManualTestContext& context,
    const std::filesystem::path& source_path,
    std::string& outReason)
{
    return LoadManualGaugeAnnotationFromPathImpl(
        context,
        source_path,
        false,
        outReason);
}

bool ExportManualGaugeManifestCandidate(
    ManualTestContext& context,
    const std::string& objectName,
    const std::string& gaugeName,
    std::string& outPath,
    std::string& outReason)
{
    (void)objectName;
    (void)gaugeName;
    ManualGaugeState gauge = context.current_gauge;
    NormalizeManualGaugeGeometry(gauge);
    if (!ValidateManualGaugeGeometry(gauge, outReason))
        return false;
    std::filesystem::path dir;
    if (!ResolveManualGaugeCaseDir(context, dir, outReason))
        return false;
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec)
    {
        outReason = "cannot create gauge case directory: " + ec.message();
        return false;
    }
    const std::filesystem::path destination = dir / "gauge_manifest_candidate.cxsc";
    const std::filesystem::path temporary = destination.string() + ".tmp";
    std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
    if (!file.is_open())
    {
        outReason = "cannot open temporary manifest candidate";
        return false;
    }
    file << "// candidate only; pending manual review; do not promote automatically\n"
         << "Stage25Manifest m;\n"
         << "m.reset();\n"
         << "m.setname(\"manual_gauge_candidate_" << GaugeJsonEscape(gauge.case_id) << "\");\n"
         << "m.addimage(\"" << GaugeJsonEscape(gauge.image_id) << "\", \"manual_candidate\", \""
         << GaugeJsonEscape(context.image_file_path) << "\");\n";
    if (gauge.tool == "FindLine")
        file << "m.image_addfindlinetarget(\"" << GaugeJsonEscape(gauge.target_id) << "\", "
             << gauge.line_x0 << ", " << gauge.line_y0 << ", " << gauge.line_x1 << ", "
             << gauge.line_y1 << ", " << gauge.wgap << ", " << gauge.hgap << ");\n";
    else if (gauge.tool == "FindCircle")
        file << "m.image_addfindcircletarget(\"" << GaugeJsonEscape(gauge.target_id) << "\", "
             << gauge.circle_cx << ", " << gauge.circle_cy << ", " << gauge.circle_px << ", "
             << gauge.circle_py << ", " << gauge.gap << ", " << gauge.linegap << ");\n";
    else if (gauge.tool == "FindEllipse")
    {
        const int cx = (gauge.ellipse_x0 + gauge.ellipse_x1) / 2;
        const int cy = (gauge.ellipse_y0 + gauge.ellipse_y1) / 2;
        const int rx = std::abs(gauge.ellipse_x1 - gauge.ellipse_x0) / 2;
        const int ry = std::abs(gauge.ellipse_y1 - gauge.ellipse_y0) / 2;
        file << "m.image_addfindellipsetarget(\"" << GaugeJsonEscape(gauge.target_id) << "\", "
             << cx << ", " << cy << ", " << rx << ", " << ry << ", 0);\n";
    }
    file.flush();
    const bool write_ok = file.good();
    file.close();
    if (!write_ok)
    {
        outReason = "failed while writing manifest candidate";
        std::filesystem::remove(temporary, ec);
        return false;
    }
    if (!AtomicReplaceFile(temporary, destination, outReason))
    {
        std::filesystem::remove(temporary, ec);
        return false;
    }
    outPath = destination.string();
    outReason.clear();
    return true;
}

bool ManualGaugeAcceptedForParamRegression(const ManualGaugeState& gauge)
{
    std::string reason;
    return ValidateManualGaugeGeometry(gauge, reason);
}

bool ValidateParamRegressionPrerequisites(
    const ManualTestContext& context,
    std::string& reason)
{
    if (!ValidateManualGaugeGeometry(context.current_gauge, reason))
        return false;
    const ManualGaugeState& gauge = context.current_gauge;
    if (gauge.case_id.empty() || gauge.image_id.empty() || gauge.target_id.empty())
    {
        reason = "case_id, image_id and target_id are required";
        return false;
    }
    if (context.image_file_path.empty() || !std::filesystem::is_regular_file(context.image_file_path))
    {
        reason = "image_file_path is unavailable";
        return false;
    }
    const std::string script_path = !context.loaded_script_path.empty()
        ? context.loaded_script_path
        : context.active_script_case_path;
    if (script_path.empty() || !std::filesystem::is_regular_file(script_path))
    {
        reason = "script path is unavailable";
        return false;
    }
    std::filesystem::path case_dir;
    if (!ResolveManualGaugeCaseDir(context, case_dir, reason))
        return false;
    if (!std::filesystem::is_regular_file(case_dir / "gauge_annotation.json"))
    {
        reason = "gauge_annotation.json has not been saved";
        return false;
    }
    reason.clear();
    return true;
}
