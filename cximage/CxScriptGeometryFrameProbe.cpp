#include "CxScriptGeometryFrameProbe.h"
#include "CxScriptGeometryFrameOverlay.h"
#include "Findline.h"
#include "Findcircle.h"
#include <opencv2/opencv.hpp>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kTol = 1.5;

std::string Trim(std::string s)
{
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    const auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

std::vector<std::string> SplitParams(const std::string& text)
{
    std::vector<std::string> out;
    std::stringstream ss(text);
    std::string item;
    while (std::getline(ss, item, ',')) out.push_back(Trim(item));
    return out;
}

int ResolveInt(const std::map<std::string, int>& vars, const std::string& token, int fallback = 0)
{
    const std::string t = Trim(token);
    const auto it = vars.find(t);
    if (it != vars.end()) return it->second;
    try { return std::stoi(t); } catch (...) { return fallback; }
}

struct ParsedProbeScript
{
    std::string tool;
    std::map<std::string, int> vars;
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0, tool_half_width = 0;
    int wgap = 0, hgap = 0, linegap = 0;
    int cx = 0, cy = 0, px = 0, py = 0, gap = 0;
};

bool ReadText(const std::filesystem::path& path, std::string& text)
{
    std::ifstream file(path);
    if (!file.is_open()) return false;
    std::stringstream ss;
    ss << file.rdbuf();
    text = ss.str();
    return true;
}

bool ParseProbeScript(const std::filesystem::path& script, ParsedProbeScript& out, std::string& reason)
{
    std::string text;
    if (!ReadText(script, text)) { reason = "cannot read script: " + script.string(); return false; }
    std::stringstream ss(text);
    std::string line;
    std::regex intDecl(R"(^\s*int\s+([A-Za-z_][A-Za-z0-9_]*)\s*=\s*([^;]+)\s*;)");
    std::regex methodCall(R"((m_[A-Za-z0-9_]+)\.([A-Za-z_][A-Za-z0-9_]*)\((.*)\)\s*;)");
    while (std::getline(ss, line))
    {
        const std::string t = Trim(line);
        if (t.empty() || t.rfind("//", 0) == 0 || t.rfind("/*", 0) == 0 || t.rfind("*", 0) == 0) continue;
        std::smatch m;
        if (std::regex_search(t, m, intDecl))
        {
            out.vars[m[1].str()] = ResolveInt(out.vars, m[2].str(), 0);
            continue;
        }
        if (std::regex_search(t, m, methodCall))
        {
            const std::string method = m[2].str();
            const auto params = SplitParams(m[3].str());
            if (method == "SetWHgap" && params.size() >= 2)
            {
                out.wgap = ResolveInt(out.vars, params[0]);
                out.hgap = ResolveInt(out.vars, params[1]);
            }
            else if (method == "setlinegap" && params.size() >= 1)
            {
                out.linegap = ResolveInt(out.vars, params[0]);
            }
            else if (method == "setline" && params.size() >= 5)
            {
                out.tool = "Findline";
                out.x0 = ResolveInt(out.vars, params[0]);
                out.y0 = ResolveInt(out.vars, params[1]);
                out.x1 = ResolveInt(out.vars, params[2]);
                out.y1 = ResolveInt(out.vars, params[3]);
                out.tool_half_width = ResolveInt(out.vars, params[4]);
            }
            else if (method == "Setgap" && params.size() >= 1)
            {
                out.gap = ResolveInt(out.vars, params[0]);
            }
            else if (method == "setcircle" && params.size() >= 4)
            {
                out.tool = "Findcircle";
                out.cx = ResolveInt(out.vars, params[0]);
                out.cy = ResolveInt(out.vars, params[1]);
                out.px = ResolveInt(out.vars, params[2]);
                out.py = ResolveInt(out.vars, params[3]);
            }
        }
    }
    if (out.tool.empty()) { reason = "script does not contain setline or setcircle frame call"; return false; }
    return true;
}

bool RectIntersectsImage(const std::vector<GaugePoint2d>& rect, int w, int h)
{
    double minx = 1e30, miny = 1e30, maxx = -1e30, maxy = -1e30;
    for (const auto& p : rect) { minx = std::min(minx, p.x); miny = std::min(miny, p.y); maxx = std::max(maxx, p.x); maxy = std::max(maxy, p.y); }
    return maxx >= 0.0 && maxy >= 0.0 && minx <= (double)(w - 1) && miny <= (double)(h - 1);
}

bool RectInsideImage(const std::vector<GaugePoint2d>& rect, int w, int h)
{
    for (const auto& p : rect)
        if (p.x < 0.0 || p.y < 0.0 || p.x > (double)(w - 1) || p.y > (double)(h - 1)) return false;
    return true;
}

bool PointClose(const GaugePoint2d& a, const GaugePoint2d& b)
{
    return std::hypot(a.x - b.x, a.y - b.y) <= kTol;
}

bool PointSetCloseIgnoringOrder(const std::vector<GaugePoint2d>& a, const std::vector<GaugePoint2d>& b)
{
    if (a.size() != b.size()) return false;
    std::vector<bool> used(b.size(), false);
    for (const auto& pa : a)
    {
        bool found = false;
        for (std::size_t i = 0; i < b.size(); ++i)
        {
            if (!used[i] && PointClose(pa, b[i])) { used[i] = true; found = true; break; }
        }
        if (!found) return false;
    }
    return true;
}

GaugeLineFrameProbe BuildLineFrame(const ParsedProbeScript& s, int image_w, int image_h)
{
    GaugeLineFrameProbe p;
    p.image_width = image_w;
    p.image_height = image_h;
    p.x0 = s.x0; p.y0 = s.y0; p.x1 = s.x1; p.y1 = s.y1;
    p.tool_half_width = s.tool_half_width;
    p.linegap = s.linegap;
    p.wgap = s.wgap;
    p.hgap = s.hgap;
    const double dx = p.x1 - p.x0;
    const double dy = p.y1 - p.y0;
    p.line_length = std::hypot(dx, dy);
    if (p.line_length > 1e-9)
    {
        p.unit_dx = dx / p.line_length;
        p.unit_dy = dy / p.line_length;
        p.normal_x = -p.unit_dy;
        p.normal_y = p.unit_dx;
    }
    p.rect = {
        {p.x0 + p.normal_x * p.tool_half_width, p.y0 + p.normal_y * p.tool_half_width},
        {p.x0 - p.normal_x * p.tool_half_width, p.y0 - p.normal_y * p.tool_half_width},
        {p.x1 - p.normal_x * p.tool_half_width, p.y1 - p.normal_y * p.tool_half_width},
        {p.x1 + p.normal_x * p.tool_half_width, p.y1 + p.normal_y * p.tool_half_width}
    };
    p.scan_line_length = 2.0 * p.tool_half_width;
    p.scan_line_count = p.linegap > 0 ? std::max(1, (int)std::floor(p.line_length / p.linegap)) : 0;
    p.roi_intersects_image = RectIntersectsImage(p.rect, image_w, image_h);
    p.roi_fully_inside_image = RectInsideImage(p.rect, image_w, image_h);

    Findline runtime;
    if (s.wgap > 0 || s.hgap > 0) runtime.SetWHgap(std::max(1, s.wgap), std::max(1, s.hgap));
    if (s.linegap > 0) runtime.setlinegap(s.linegap);
    runtime.setline(s.x0, s.y0, s.x1, s.y1, s.tool_half_width);
    FindlineDisplaySnapshot snap;
    p.runtime_has_scan_box = runtime.getdisplaysnapshot(snap) && snap.has_scan_box;
    if (p.runtime_has_scan_box)
    {
        for (int i = 0; i < 4; ++i)
            p.runtime_rect.push_back({snap.scan_box_xy[i * 2], snap.scan_box_xy[i * 2 + 1]});
        if (PointSetCloseIgnoringOrder(p.rect, p.runtime_rect))
        {
            p.frame_compare_status = "FRAME_MATCH";
            p.frame_compare_reason = "probe rect matches runtime line_scan_box_xy, corner order ignored";
        }
        else
        {
            p.frame_compare_status = "FRAME_GEOMETRY_MISMATCH";
            p.frame_compare_reason = "probe rect does not match runtime line_scan_box_xy";
        }
    }
    else
    {
        p.frame_compare_status = "RUNTIME_FRAME_UNAVAILABLE";
        p.frame_compare_reason = "Findline runtime did not expose scan box";
    }
    if (!p.roi_intersects_image)
    {
        p.frame_compare_status = "ROI_OUT_OF_IMAGE";
        p.frame_compare_reason = "line gauge frame does not intersect image";
    }
    return p;
}

GaugeCircleFrameProbe BuildCircleFrame(const ParsedProbeScript& s, int image_w, int image_h)
{
    GaugeCircleFrameProbe p;
    p.image_width = image_w; p.image_height = image_h;
    p.cx = s.cx; p.cy = s.cy; p.px = s.px; p.py = s.py;
    p.gap = s.gap; p.linegap = s.linegap;
    p.radius = std::hypot(p.px - p.cx, p.py - p.cy);
    p.scan_line_count = p.gap > 0 ? std::max(1, 360 / p.gap) : 0;
    p.circle_intersects_image = (p.cx + p.radius >= 0.0 && p.cy + p.radius >= 0.0 && p.cx - p.radius <= image_w - 1 && p.cy - p.radius <= image_h - 1);
    p.circle_fully_inside_image = (p.cx - p.radius >= 0.0 && p.cy - p.radius >= 0.0 && p.cx + p.radius <= image_w - 1 && p.cy + p.radius <= image_h - 1);

    Findcircle runtime;
    if (s.gap > 0) runtime.Setgap(s.gap);
    if (s.linegap > 0) runtime.setlinegap(s.linegap);
    runtime.setcircle(s.cx, s.cy, s.px, s.py);
    const double rr = std::hypot((double)runtime.getcirclepax() - runtime.getcirclecentx(), (double)runtime.getcirclepay() - runtime.getcirclecenty());
    if (std::abs(runtime.getcirclecentx() - p.cx) <= kTol &&
        std::abs(runtime.getcirclecenty() - p.cy) <= kTol &&
        std::abs(runtime.getcirclepax() - p.px) <= kTol &&
        std::abs(runtime.getcirclepay() - p.py) <= kTol &&
        std::abs(rr - p.radius) <= kTol)
    {
        p.frame_compare_status = "FRAME_MATCH";
        p.frame_compare_reason = "probe circle matches runtime circle geometry";
    }
    else
    {
        p.frame_compare_status = "FRAME_GEOMETRY_MISMATCH";
        p.frame_compare_reason = "probe circle does not match runtime circle center/pass/radius";
    }
    if (!p.circle_intersects_image)
    {
        p.frame_compare_status = "ROI_OUT_OF_IMAGE";
        p.frame_compare_reason = "circle gauge frame does not intersect image";
    }
    return p;
}

void WriteLineJson(const GaugeLineFrameProbe& p, const std::filesystem::path& path)
{
    std::ofstream f(path);
    f << "{\n";
    f << "  \"tool\": \"Findline\",\n";
    f << "  \"image_width\": " << p.image_width << ",\n";
    f << "  \"image_height\": " << p.image_height << ",\n";
    f << "  \"x0\": " << p.x0 << ", \"y0\": " << p.y0 << ",\n";
    f << "  \"x1\": " << p.x1 << ", \"y1\": " << p.y1 << ",\n";
    f << "  \"tool_half_width\": " << p.tool_half_width << ",\n";
    f << "  \"line_length\": " << p.line_length << ",\n";
    f << "  \"unit_direction\": [" << p.unit_dx << ", " << p.unit_dy << "],\n";
    f << "  \"unit_normal\": [" << p.normal_x << ", " << p.normal_y << "],\n";
    f << "  \"rect\": [\n";
    for (std::size_t i = 0; i < p.rect.size(); ++i)
        f << "    [" << p.rect[i].x << ", " << p.rect[i].y << "]" << (i + 1 < p.rect.size() ? "," : "") << "\n";
    f << "  ],\n";
    f << "  \"runtime_rect\": [\n";
    for (std::size_t i = 0; i < p.runtime_rect.size(); ++i)
        f << "    [" << p.runtime_rect[i].x << ", " << p.runtime_rect[i].y << "]" << (i + 1 < p.runtime_rect.size() ? "," : "") << "\n";
    f << "  ],\n";
    f << "  \"roi_intersects_image\": " << (p.roi_intersects_image ? "true" : "false") << ",\n";
    f << "  \"roi_fully_inside_image\": " << (p.roi_fully_inside_image ? "true" : "false") << ",\n";
    f << "  \"linegap\": " << p.linegap << ",\n";
    f << "  \"wgap\": " << p.wgap << ",\n";
    f << "  \"hgap\": " << p.hgap << ",\n";
    f << "  \"scan_line_count\": " << p.scan_line_count << ",\n";
    f << "  \"scan_line_direction\": \"normal_to_center_line\",\n";
    f << "  \"scan_line_length\": " << p.scan_line_length << ",\n";
    f << "  \"coordinate_rule\": \"image coordinate: x right, y down\",\n";
    f << "  \"orientation_rule\": \"line rectangle = center line +/- normal * half_width\",\n";
    f << "  \"frame_compare_status\": \"" << p.frame_compare_status << "\",\n";
    f << "  \"frame_compare_reason\": \"" << p.frame_compare_reason << "\"\n";
    f << "}\n";
}

void WriteCircleJson(const GaugeCircleFrameProbe& p, const std::filesystem::path& path)
{
    std::ofstream f(path);
    f << "{\n";
    f << "  \"tool\": \"Findcircle\",\n";
    f << "  \"image_width\": " << p.image_width << ",\n";
    f << "  \"image_height\": " << p.image_height << ",\n";
    f << "  \"cx\": " << p.cx << ", \"cy\": " << p.cy << ",\n";
    f << "  \"px\": " << p.px << ", \"py\": " << p.py << ",\n";
    f << "  \"radius\": " << p.radius << ",\n";
    f << "  \"gap\": " << p.gap << ",\n";
    f << "  \"linegap\": " << p.linegap << ",\n";
    f << "  \"circle_intersects_image\": " << (p.circle_intersects_image ? "true" : "false") << ",\n";
    f << "  \"circle_fully_inside_image\": " << (p.circle_fully_inside_image ? "true" : "false") << ",\n";
    f << "  \"scan_line_count\": " << p.scan_line_count << ",\n";
    f << "  \"scan_line_direction\": \"radial\",\n";
    f << "  \"coordinate_rule\": \"image coordinate: x right, y down\",\n";
    f << "  \"orientation_rule\": \"circle = center + radius from pass point\",\n";
    f << "  \"frame_compare_status\": \"" << p.frame_compare_status << "\",\n";
    f << "  \"frame_compare_reason\": \"" << p.frame_compare_reason << "\"\n";
    f << "}\n";
}

void WriteReport(const std::string& tool, const std::filesystem::path& script, const std::filesystem::path& image,
                 const std::string& status, const std::string& reason, const std::filesystem::path& path)
{
    std::ofstream f(path);
    f << "# Gauge Frame Probe Report\n\n";
    f << "## Case Info\n\n";
    f << "- Tool: " << tool << "\n";
    f << "- Script: " << script.string() << "\n";
    f << "- Image: " << image.string() << "\n";
    f << "- Probe mode: G0/G1 Geometry Frame Probe\n\n";
    f << "## Runtime Compare\n\n";
    f << "- frame compare status: " << status << "\n";
    f << "- frame compare reason: " << reason << "\n\n";
    f << "## Decision\n\n";
    if (status == "FRAME_MATCH") f << "PASS\n";
    else if (status == "ROI_OUT_OF_IMAGE") f << "WARNING\n";
    else f << "FAIL\n";
    f << "\n## Notes\n\nManual review should inspect frame_black.png and frame_on_image.png before any measure/fit test.\n";
}
}

bool ParseGaugeFrameProbeArgs(int argc, char** argv, GaugeFrameProbeOptions& options)
{
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--cxscript-frame-probe") options.enabled = true;
        else if (arg == "--image" && i + 1 < argc) options.image_path = argv[++i];
        else if (arg == "--script" && i + 1 < argc) options.script_path = argv[++i];
        else if (arg == "--out" && i + 1 < argc) options.out_root = argv[++i];
        else if (arg == "--case-name" && i + 1 < argc) options.case_name = argv[++i];
    }
    return options.enabled;
}

bool RunGaugeFrameProbe(const GaugeFrameProbeOptions& options, GaugeFrameProbeResult& result)
{
    result.exit_code = 1;
    if (options.script_path.empty()) { result.reason = "--script is required"; return false; }
    if (options.image_path.empty()) { result.reason = "--image is required"; return false; }
    const std::filesystem::path out = options.out_root.empty() ? std::filesystem::path("gauge_frame_probe_out") : options.out_root;
    std::filesystem::create_directories(out);

    cv::Mat image = cv::imread(options.image_path.string(), cv::IMREAD_COLOR);
    if (image.empty()) { result.reason = "cannot load image: " + options.image_path.string(); return false; }

    ParsedProbeScript script;
    if (!ParseProbeScript(options.script_path, script, result.reason)) return false;
    result.tool = script.tool;
    result.frame_black_path = out / "frame_black.png";
    result.frame_on_image_path = out / "frame_on_image.png";
    result.frame_geometry_path = out / "frame_geometry.json";
    result.frame_report_path = out / "frame_geometry_report.md";
    result.snapshot_path = out / "snapshot.txt";

    std::string imageReason;
    if (script.tool == "Findline")
    {
        GaugeLineFrameProbe probe = BuildLineFrame(script, image.cols, image.rows);
        WriteLineJson(probe, result.frame_geometry_path);
        WriteReport(probe.tool, options.script_path, options.image_path, probe.frame_compare_status, probe.frame_compare_reason, result.frame_report_path);
        if (!SaveLineFrameProbeImages(probe, image, result.frame_black_path, result.frame_on_image_path, imageReason))
        { result.reason = imageReason; return false; }
        std::ofstream s(result.snapshot_path);
        s << "tool: Findline\nscript_path: " << options.script_path.string() << "\nframe_compare_status: " << probe.frame_compare_status << "\nscan_line_count: " << probe.scan_line_count << "\nscan_line_length: " << probe.scan_line_length << "\n";
        result.ok = (probe.frame_compare_status == "FRAME_MATCH");
    }
    else
    {
        GaugeCircleFrameProbe probe = BuildCircleFrame(script, image.cols, image.rows);
        WriteCircleJson(probe, result.frame_geometry_path);
        WriteReport(probe.tool, options.script_path, options.image_path, probe.frame_compare_status, probe.frame_compare_reason, result.frame_report_path);
        if (!SaveCircleFrameProbeImages(probe, image, result.frame_black_path, result.frame_on_image_path, imageReason))
        { result.reason = imageReason; return false; }
        std::ofstream s(result.snapshot_path);
        s << "tool: Findcircle\nscript_path: " << options.script_path.string() << "\nframe_compare_status: " << probe.frame_compare_status << "\nradius: " << probe.radius << "\nscan_line_count: " << probe.scan_line_count << "\n";
        result.ok = (probe.frame_compare_status == "FRAME_MATCH");
    }

    result.exit_code = result.ok ? 0 : 2;
    result.reason = result.ok ? "" : "frame probe did not pass";
    return result.ok;
}
