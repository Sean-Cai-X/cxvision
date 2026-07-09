#include "CxScriptSuiteRunner.h"
#include "CxScriptSuiteRuntime.h"
#include "CxScriptCatalogRuntime.h"
#include "CxScriptImageManifestRuntime.h"
#include "CxScriptSuiteReportWriter.h"
#include "CxScriptToolDisplayExporter.h"
#include "CxScriptBestCaseSelector.h"
#include "CxParameterProfileRuntime.h"
#include "CxScriptEvidenceChainRuntime.h"
#include "CxScriptReviewGateRuntime.h"
#include "CxScriptRunTraceRuntime.h"
#include "CxAlgorithmTraceSink.h"
#include "ManualStateTestConsole.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <opencv2/opencv.hpp>

class ScopedSuiteTimer
{
public:
    explicit ScopedSuiteTimer(const std::string& name)
        : name_(name),
          begin_(std::chrono::steady_clock::now())
    {
        std::cout << "[phase-begin] " << name_ << "\n" << std::flush;
    }

    ~ScopedSuiteTimer()
    {
        auto end = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            end - begin_).count();

        std::cout << "[phase-end] " << name_
                  << " elapsed_ms=" << ms << "\n" << std::flush;
    }

    long long elapsed_ms() const
    {
        auto end = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            end - begin_).count();
    }

private:
    std::string name_;
    std::chrono::steady_clock::time_point begin_;
};

const CxScriptCatalogEntry* FindCatalogScriptById(
    const CxScriptCatalogRuntime& catalog,
    const std::string& script_id)
{
    for (const auto& script : catalog.scripts)
    {
        if (script.script_id == script_id)
            return &script;
    }
    return nullptr;
}

namespace
{
    bool StopForHumanReviewIfNeeded(
        const CxScriptSuiteRunOptions& options,
        CxScriptSuiteCaseResult& caseResult,
        const std::string& stage,
        const std::string& suggestedAction);

    bool WriteHumanReviewJson(
        const std::filesystem::path& path,
        const CxScriptHumanReview& review,
        std::string& reason);

    void AppendNodeTrace(
        const std::filesystem::path& caseDir,
        const std::string& node_id,
        const std::string& phase,
        const std::string& status,
        const std::string& message = "",
        int elapsed_ms = 0,
        int valid_points = 0)
    {
        auto path = caseDir / "evidence_node_trace.jsonl";
        std::ofstream file(path, std::ios::app);
        if (!file.is_open())
            return;

        std::time_t now = std::time(nullptr);
        char time_buf[64];
        std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%dT%H:%M:%S", std::localtime(&now));

        file << "{\"node_id\":\"" << node_id << "\","
             << "\"phase\":\"" << phase << "\","
             << "\"status\":\"" << status << "\","
             << "\"timestamp\":\"" << time_buf << "\","
             << "\"phase_elapsed_ms\":" << elapsed_ms << ","
             << "\"valid_points\":" << valid_points;

        if (!message.empty())
            file << ",\"message\":\"" << message << "\"";

        file << "}\n";
    }

    std::string TrimJsonScalar(std::string value)
    {
        const size_t comment = value.find(',');
        if (comment != std::string::npos)
            value = value.substr(0, comment);

        size_t start = value.find_first_not_of(" \t\r\n\"");
        size_t end = value.find_last_not_of(" \t\r\n\"");
        if (start == std::string::npos || end == std::string::npos || end < start)
            return "";

        return value.substr(start, end - start + 1);
    }

    bool JsonLineHasKey(const std::string& line, const std::string& key)
    {
        return line.find("\"" + key + "\"") != std::string::npos;
    }

    std::string JsonLineValue(const std::string& line)
    {
        const auto colon = line.find(":");
        if (colon == std::string::npos)
            return "";
        return TrimJsonScalar(line.substr(colon + 1));
    }

    bool ParseJsonBoolValue(const std::string& value)
    {
        return value == "true" || value == "1";
    }

    bool JsonLineHasAnyKey(const std::string& line, const std::vector<std::string>& keys)
    {
        for (const auto& key : keys)
        {
            if (JsonLineHasKey(line, key))
                return true;
        }
        return false;
    }

    void LoadSuiteCaseMetricsFromSummary(
        const std::string& summary_path,
        CxScriptSuiteCaseResult& out)
    {
        if (summary_path.empty())
            return;

        std::ifstream file(summary_path);
        if (!file.is_open())
            return;

        std::cout << "[DEBUG] Loading summary from: " << summary_path << "\n";

        std::string line;
        std::string jsonContent;
        while (std::getline(file, line))
        {
            jsonContent += line + "\n";
        }

        size_t pointsArrayStart = jsonContent.find("\"measure_points_xy\": [");
        if (pointsArrayStart != std::string::npos)
        {
            pointsArrayStart += std::string("\"measure_points_xy\": [").size();
            size_t pointsArrayEnd = jsonContent.find("]", pointsArrayStart);
            if (pointsArrayEnd != std::string::npos)
            {
                std::string pointsArray = jsonContent.substr(pointsArrayStart, pointsArrayEnd - pointsArrayStart);
                size_t pos = 0;
                while ((pos = pointsArray.find("[", pos)) != std::string::npos)
                {
                    size_t endPos = pointsArray.find("]", pos);
                    if (endPos != std::string::npos)
                    {
                        std::string pointPair = pointsArray.substr(pos + 1, endPos - pos - 1);
                        size_t commaPos = pointPair.find(",");
                        if (commaPos != std::string::npos)
                        {
                            try
                            {
                                double x = std::stod(pointPair.substr(0, commaPos));
                                double y = std::stod(pointPair.substr(commaPos + 1));
                                out.measure_points_xy.emplace_back(x, y);
                            }
                            catch (...) {}
                        }
                    }
                    pos = endPos + 1;
                }
            }
        }

        std::istringstream contentStream(jsonContent);
        while (std::getline(contentStream, line))
        {
            if (JsonLineHasAnyKey(line, {"valid_points_count", "valid_line_points_count", "valid_circle_points_count"}))
            {
                try { out.valid_points_count = std::stoi(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasAnyKey(line, {"points_count", "measure_points_count", "line_measure_points_count", "circle_measure_points_count"}))
            {
                try { out.points_count = std::stoi(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasAnyKey(line, {"has_fit_line", "line_has_fit", "fit_line_available"}))
            {
                out.has_fit_line = ParseJsonBoolValue(JsonLineValue(line));
            }
            else if (JsonLineHasAnyKey(line, {"has_fit_circle", "circle_has_fit", "fit_circle_available"}))
            {
                out.has_fit_circle = ParseJsonBoolValue(JsonLineValue(line));
            }
            else if (JsonLineHasKey(line, "local_support"))
            {
                try { out.local_support = std::stod(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "local_mean_distance"))
            {
                try { out.local_mean_distance = std::stod(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "fit_offset"))
            {
                try { out.fit_offset = std::stod(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasAnyKey(line, {"actual_policy_guard", "line_policy_guard_status", "circle_policy_guard_status", "policy_guard_status", "policy_guard"}))
            {
                out.actual_policy_guard = JsonLineValue(line);
            }
            else if (JsonLineHasKey(line, "result_status"))
            {
                out.result_status = JsonLineValue(line);
            }
            else if (JsonLineHasKey(line, "failure_stage"))
            {
                out.failure_stage = JsonLineValue(line);
            }
            else if (JsonLineHasKey(line, "contract_pass"))
            {
                out.contract_pass = ParseJsonBoolValue(JsonLineValue(line));
            }
            else if (JsonLineHasKey(line, "contract_status"))
            {
                out.contract_status = JsonLineValue(line);
            }
            else if (JsonLineHasKey(line, "contract_conclusion"))
            {
                out.contract_conclusion = JsonLineValue(line);
                out.conclusion = JsonLineValue(line);
            }
            else if (JsonLineHasAnyKey(line, {"policy_guard", "actual_policy_guard"}))
            {
                out.policy_guard = JsonLineValue(line);
                out.actual_policy_guard = JsonLineValue(line);
            }
            else if (JsonLineHasAnyKey(line, {"circle_radius", "fit_circle_radius"}))
            {
                try { out.circle_radius = std::stod(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasAnyKey(line, {"avgdist", "circle_avgdist", "line_avgdist"}))
            {
                try { out.avgdist = std::stod(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "roi_x0"))
            {
                try { out.roi_x0 = std::stoi(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "roi_y0"))
            {
                try { out.roi_y0 = std::stoi(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "roi_x1"))
            {
                try { out.roi_x1 = std::stoi(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "roi_y1"))
            {
                try { out.roi_y1 = std::stoi(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "circle_cx"))
            {
                try { out.circle_cx = std::stoi(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "circle_cy"))
            {
                try { out.circle_cy = std::stoi(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "circle_px"))
            {
                try { out.circle_px = std::stoi(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "circle_py"))
            {
                try { out.circle_py = std::stoi(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "fit_line_x0"))
            {
                try { out.fit_line_x0 = std::stod(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "fit_line_y0"))
            {
                try { out.fit_line_y0 = std::stod(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "fit_line_x1"))
            {
                try { out.fit_line_x1 = std::stod(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "fit_line_y1"))
            {
                try { out.fit_line_y1 = std::stod(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "circle_center_x"))
            {
                try { out.circle_center_x = std::stod(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "circle_center_y"))
            {
                try { out.circle_center_y = std::stod(JsonLineValue(line)); }
                catch (...) {}
            }
        }

        std::cout << "[DEBUG] Loaded metrics: points=" << out.points_count 
                  << ", valid=" << out.valid_points_count
                  << ", has_fit_circle=" << out.has_fit_circle
                  << ", circle_radius=" << out.circle_radius << "\n";
    }

    struct ResolvedEvidenceCase
    {
        const CxScriptCatalogEntry* script = nullptr;
        const CxScriptImageManifestEntry* image = nullptr;
        const CxScriptImageTargetRoi* target = nullptr;
        const CxParameterProfile* profile = nullptr;
        std::string contract_path;
    };

    bool ResolveEvidenceCase(
        const CxScriptSuiteCase& suite_case,
        const CxScriptCatalogRuntime& catalog,
        const CxScriptImageManifestRuntime& manifest,
        const CxParameterProfileRuntime& profiles,
        ResolvedEvidenceCase& out,
        std::string& reason)
    {
        out.script = FindCatalogScriptById(catalog, suite_case.script_id);
        if (!out.script)
        {
            reason = "MISSING_SCRIPT: " + suite_case.script_id;
            return false;
        }

        out.image = FindImageById(manifest, suite_case.image_id);
        if (!out.image)
        {
            reason = "MISSING_IMAGE: " + suite_case.image_id;
            return false;
        }

        if (!suite_case.target_id.empty())
        {
            out.target = FindTargetRoiByImageAndTargetId(manifest, suite_case.image_id, suite_case.target_id);
            if (!out.target)
            {
                reason = "MISSING_TARGET: " + suite_case.target_id + " for image " + suite_case.image_id;
                return false;
            }
        }

        if (!suite_case.parameter_profile_id.empty())
        {
            out.profile = profiles.FindProfile(suite_case.parameter_profile_id);
            if (!out.profile)
            {
                reason = "MISSING_PARAMETER_PROFILE: " + suite_case.parameter_profile_id;
                return false;
            }
        }

        out.contract_path = out.script->contract_path;

        if (out.target)
        {
            if (out.target->tool == "Findcircle")
            {
                if (out.target->cx == 0 || out.target->cy == 0 || out.target->px == 0 || out.target->py == 0)
                {
                    reason = "INVALID_ROI: circle ROI coordinates are zero";
                    return false;
                }
            }
            else if (out.target->tool == "Findline")
            {
                if (out.target->x0 == 0 && out.target->y0 == 0 && out.target->x1 == 0 && out.target->y1 == 0)
                {
                    reason = "INVALID_ROI: line ROI coordinates are zero";
                    return false;
                }
            }
        }

        reason = "OK";
        return true;
    }

    void InjectParameterGlobals(
        CxScriptHeadlessOptions& headless,
        const CxParameterProfile& profile)
    {
        headless.method = profile.method;
        headless.threshold = profile.threshold;
        headless.gap = profile.gap;
        headless.linegap = profile.linegap;
        headless.wgap = profile.wgap;
        headless.hgap = profile.hgap;
        headless.filterprofile = profile.filterprofile;
    }

    void WriteCaseTrace(
        const std::filesystem::path& caseDir,
        const CxScriptSuiteCase& suite_case,
        const ResolvedEvidenceCase& resolved)
    {
        std::filesystem::path tracePath = caseDir / "case_trace.txt";
        bool exists = std::filesystem::exists(tracePath);
        std::ofstream traceFile(tracePath, exists ? std::ios::app : std::ios::out);
        if (traceFile.is_open())
        {
            std::time_t now_c = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            std::tm now_tm;
            localtime_s(&now_tm, &now_c);
            char time_buf[64];
            std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &now_tm);

            traceFile << "Case Trace\n";
            traceFile << "==========\n\n";
            traceFile << "started_at: " << time_buf << "\n";
            traceFile << "case_id: " << suite_case.case_id << "\n";
            traceFile << "image_id: " << suite_case.image_id << "\n";
            traceFile << "target_id: " << suite_case.target_id << "\n";
            traceFile << "script_id: " << suite_case.script_id << "\n";
            traceFile << "parameter_profile_id: " << suite_case.parameter_profile_id << "\n";
            traceFile << "expected_result: " << suite_case.expected_result << "\n";
            traceFile << "expected_policy_guard: " << suite_case.expected_policy_guard << "\n\n";

            if (resolved.image)
                traceFile << "Image: " << resolved.image->path << "\n";
            if (resolved.script)
                traceFile << "Script: " << resolved.script->path << "\n";
            if (resolved.contract_path.empty())
                traceFile << "Contract: NOT FOUND\n";
            else
                traceFile << "Contract: " << resolved.contract_path << "\n";
            if (resolved.profile)
            {
                traceFile << "\nParameter Profile:\n";
                traceFile << "  profile_id: " << resolved.profile->profile_id << "\n";
                traceFile << "  method: " << resolved.profile->method << "\n";
                traceFile << "  threshold: " << resolved.profile->threshold << "\n";
                traceFile << "  gap: " << resolved.profile->gap << "\n";
                traceFile << "  linegap: " << resolved.profile->linegap << "\n";
                traceFile << "  wgap: " << resolved.profile->wgap << "\n";
                traceFile << "  hgap: " << resolved.profile->hgap << "\n";
                traceFile << "  filterprofile: " << resolved.profile->filterprofile << "\n";
            }
            if (resolved.target)
            {
                traceFile << "\nTarget ROI:\n";
                traceFile << "  tool: " << resolved.target->tool << "\n";
                traceFile << "  x0,y0,x1,y1: " << resolved.target->x0 << "," << resolved.target->y0 << "," << resolved.target->x1 << "," << resolved.target->y1 << "\n";
                traceFile << "  cx,cy,px,py: " << resolved.target->cx << "," << resolved.target->cy << "," << resolved.target->px << "," << resolved.target->py << "\n";
            }

            traceFile << "\nPhase Timeline:\n";
            traceFile << "---------------\n";
        }
    }

    void AppendPhaseTrace(
        const std::filesystem::path& caseDir,
        const std::string& phase,
        const std::string& event_type,
        const std::string& message,
        long long elapsed_ms)
    {
        std::ofstream traceFile(caseDir / "case_trace.txt", std::ios::app);
        if (traceFile.is_open())
        {
            std::time_t now_c = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            std::tm now_tm;
            localtime_s(&now_tm, &now_c);
            char time_buf[64];
            std::strftime(time_buf, sizeof(time_buf), "%H:%M:%S", &now_tm);

            if (event_type == "begin")
            {
                traceFile << "[" << time_buf << "] " << phase << " BEGIN\n";
            }
            else if (event_type == "end")
            {
                traceFile << "[" << time_buf << "] " << phase << " END   elapsed_ms=" << elapsed_ms << "\n";
            }
            else if (event_type == "progress")
            {
                traceFile << "[" << time_buf << "] " << phase << " PROGRESS " << message << "\n";
            }
            else if (event_type == "abort")
            {
                traceFile << "[" << time_buf << "] " << phase << " ABORT  " << message << " elapsed_ms=" << elapsed_ms << "\n";
            }
            else
            {
                traceFile << "[" << time_buf << "] " << phase << " " << event_type << " " << message << "\n";
            }
        }
    }

    void ExportRoiPreview(
        const std::string& imagePath,
        const std::filesystem::path& caseDir,
        const CxScriptSuiteCase& suite_case,
        const ResolvedEvidenceCase& resolved)
    {
        cv::Mat img = cv::imread(imagePath);
        if (img.empty())
            return;

        cv::Mat preview = img.clone();

        if (resolved.target)
        {
            if (resolved.target->tool == "Findcircle")
            {
                const double roiRadius = std::hypot(
                    static_cast<double>(resolved.target->px - resolved.target->cx),
                    static_cast<double>(resolved.target->py - resolved.target->cy));

                if (roiRadius > 1.0)
                {
                    cv::circle(
                        preview,
                        cv::Point(resolved.target->cx, resolved.target->cy),
                        static_cast<int>(std::round(roiRadius)),
                        cv::Scalar(0, 255, 0),
                        2,
                        cv::LINE_AA);
                }
            }
            else if (resolved.target->tool == "Findline")
            {
                cv::line(
                    preview,
                    cv::Point(resolved.target->x0, resolved.target->y0),
                    cv::Point(resolved.target->x1, resolved.target->y1),
                    cv::Scalar(0, 255, 0),
                    2,
                    cv::LINE_AA);
            }
        }

        int y = 30;
        cv::putText(preview, "image_id: " + suite_case.image_id, {20, y},
            cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 2);
        y += 25;
        cv::putText(preview, "target_id: " + suite_case.target_id, {20, y},
            cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 2);
        y += 25;
        cv::putText(preview, "tool: " + (resolved.script ? resolved.script->tool : "unknown"), {20, y},
            cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 2);
        y += 25;
        cv::putText(preview, "param: " + suite_case.parameter_profile_id, {20, y},
            cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 2);
        y += 25;
        cv::putText(preview, "script: " + suite_case.script_id, {20, y},
            cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 2);

        cv::imwrite((caseDir / "roi_preview.png").string(), preview);
    }

    void WriteEvidencePacket(
        const std::filesystem::path& caseDir,
        const CxScriptSuiteCase& suite_case,
        const ResolvedEvidenceCase& resolved,
        const CxScriptSuiteCaseResult& result)
    {
        std::ofstream packetFile(caseDir / "evidence_packet.json");
        if (!packetFile.is_open())
            return;

        packetFile << "{\n";
        packetFile << "  \"evidence_id\": \"" << suite_case.case_id << "\",\n";
        packetFile << "  \"case_id\": \"" << suite_case.case_id << "\",\n";
        packetFile << "\n";
        packetFile << "  \"image\": {\n";
        packetFile << "    \"image_id\": \"" << suite_case.image_id << "\",\n";
        if (resolved.image)
            packetFile << "    \"path\": \"" << resolved.image->path << "\",\n";
        else
            packetFile << "    \"path\": \"\",\n";
        packetFile << "    \"level\": \"" << suite_case.level << "\"\n";
        packetFile << "  },\n";
        packetFile << "\n";
        packetFile << "  \"target\": {\n";
        packetFile << "    \"target_id\": \"" << suite_case.target_id << "\",\n";
        if (resolved.target)
        {
            packetFile << "    \"tool\": \"" << resolved.target->tool << "\",\n";
            packetFile << "    \"cx\": " << resolved.target->cx << ",\n";
            packetFile << "    \"cy\": " << resolved.target->cy << ",\n";
            packetFile << "    \"px\": " << resolved.target->px << ",\n";
            packetFile << "    \"py\": " << resolved.target->py << ",\n";
            packetFile << "    \"roi_valid\": true\n";
        }
        else
        {
            packetFile << "    \"tool\": \"\",\n";
            packetFile << "    \"cx\": 0,\n";
            packetFile << "    \"cy\": 0,\n";
            packetFile << "    \"px\": 0,\n";
            packetFile << "    \"py\": 0,\n";
            packetFile << "    \"roi_valid\": false\n";
        }
        packetFile << "  },\n";
        packetFile << "\n";
        packetFile << "  \"script\": {\n";
        packetFile << "    \"script_id\": \"" << suite_case.script_id << "\",\n";
        if (resolved.script)
            packetFile << "    \"path\": \"" << resolved.script->path << "\",\n";
        else
            packetFile << "    \"path\": \"\",\n";
        packetFile << "    \"expected_result\": \"" << suite_case.expected_result << "\"\n";
        packetFile << "  },\n";
        packetFile << "\n";
        packetFile << "  \"parameter\": {\n";
        packetFile << "    \"profile_id\": \"" << suite_case.parameter_profile_id << "\",\n";
        if (resolved.profile)
        {
            packetFile << "    \"method\": " << resolved.profile->method << ",\n";
            packetFile << "    \"threshold\": " << resolved.profile->threshold << ",\n";
            packetFile << "    \"gap\": " << resolved.profile->gap << ",\n";
            packetFile << "    \"linegap\": " << resolved.profile->linegap << ",\n";
            packetFile << "    \"wgap\": " << resolved.profile->wgap << ",\n";
            packetFile << "    \"hgap\": " << resolved.profile->hgap << ",\n";
            packetFile << "    \"filterprofile\": " << resolved.profile->filterprofile << "\n";
        }
        else
        {
            packetFile << "    \"method\": 0,\n";
            packetFile << "    \"threshold\": 0,\n";
            packetFile << "    \"gap\": 0,\n";
            packetFile << "    \"linegap\": 0,\n";
            packetFile << "    \"wgap\": 0,\n";
            packetFile << "    \"hgap\": 0,\n";
            packetFile << "    \"filterprofile\": 0\n";
        }
        packetFile << "  },\n";
        packetFile << "\n";
        packetFile << "  \"contract\": {\n";
        packetFile << "    \"path\": \"" << resolved.contract_path << "\",\n";
        packetFile << "    \"expected_policy_guard\": \"" << suite_case.expected_policy_guard << "\"\n";
        packetFile << "  },\n";
        packetFile << "\n";
        packetFile << "  \"result\": {\n";
        packetFile << "    \"valid_points_count\": " << result.valid_points_count << ",\n";
        packetFile << "    \"has_fit_circle\": " << (result.has_fit_circle ? "true" : "false") << ",\n";
        packetFile << "    \"has_fit_line\": " << (result.has_fit_line ? "true" : "false") << ",\n";
        packetFile << "    \"contract_pass\": " << (result.contract_pass ? "true" : "false") << ",\n";
        packetFile << "    \"failure_stage\": \"" << result.failure_stage << "\",\n";
        packetFile << "    \"conclusion\": \"" << result.conclusion << "\"\n";
        packetFile << "  }\n";
        packetFile << "}\n";
    }

    void CopyScriptSnapshot(
        const std::filesystem::path& scriptPath,
        const std::filesystem::path& caseDir)
    {
        std::filesystem::path snapshotPath = caseDir / "script_snapshot.cxsc";
        try
        {
            if (std::filesystem::exists(scriptPath))
            {
                std::filesystem::copy_file(
                    scriptPath,
                    snapshotPath,
                    std::filesystem::copy_options::overwrite_existing);
            }
        }
        catch (...) {}
    }

    void WriteHumanReview(
        const std::string& case_id,
        const std::string& decision,
        const std::string& note,
        const std::string& next_action,
        const std::filesystem::path& caseDir)
    {
        std::ofstream file(caseDir / "human_review.json");
        if (!file.is_open())
            return;

        std::time_t now_c = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm now_tm;
        localtime_s(&now_tm, &now_c);
        char time_buf[64];
        std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%dT%H:%M:%S", &now_tm);

        file << "{\n";
        file << "  \"review_id\": \"" << case_id << "_promotion\",\n";
        file << "  \"case_id\": \"" << case_id << "\",\n";
        file << "  \"decision\": \"" << decision << "\",\n";
        file << "  \"reviewer\": \"manual\",\n";
        file << "  \"timestamp\": \"" << time_buf << "\",\n";
        file << "  \"note\": \"" << note << "\",\n";
        file << "  \"next_action\": \"" << next_action << "\"\n";
        file << "}\n";
    }

    void WriteFinalCaseSummary(
        const CxScriptSuiteCaseResult& r,
        const std::filesystem::path& caseDir)
    {
        std::ofstream file(caseDir / "result_summary.json");
        if (!file.is_open())
            return;

        file << "{\n";
        file << "  \"case_id\": \"" << r.case_id << "\",\n";
        file << "  \"evidence_id\": \"" << r.evidence_id << "\",\n";
        file << "  \"image_id\": \"" << r.image_id << "\",\n";
        file << "  \"target_id\": \"" << r.target_id << "\",\n";
        file << "  \"script_id\": \"" << r.script_id << "\",\n";
        file << "  \"parameter_profile_id\": \"" << r.parameter_profile_id << "\",\n";
        file << "\n";
        file << "  \"headless_ok\": " << (r.headless_ok ? "true" : "false") << ",\n";
        file << "  \"contract_pass\": " << (r.contract_pass ? "1" : "0") << ",\n";
        file << "  \"contract_status\": \"" << r.contract_status << "\",\n";
        file << "  \"contract_conclusion\": \"" << r.contract_conclusion << "\",\n";
        file << "\n";
        file << "  \"valid_points_count\": " << r.valid_points_count << ",\n";
        file << "  \"has_fit_line\": " << (r.has_fit_line ? "true" : "false") << ",\n";
        file << "  \"has_fit_circle\": " << (r.has_fit_circle ? "true" : "false") << ",\n";
        file << "  \"circle_radius\": " << r.circle_radius << ",\n";
        file << "  \"avgdist\": " << r.avgdist << ",\n";
        file << "\n";
        file << "  \"policy_guard\": \"" << r.policy_guard << "\",\n";
        file << "  \"actual_policy_guard\": \"" << r.actual_policy_guard << "\",\n";
        file << "  \"result_status\": \"" << r.result_status << "\",\n";
        file << "  \"failure_stage\": \"" << r.failure_stage << "\",\n";
        file << "  \"conclusion\": \"" << r.conclusion << "\",\n";
        file << "\n";
        file << "  \"snapshot_path\": \"" << r.snapshot_path << "\",\n";
        file << "  \"summary_path\": \"" << r.summary_path << "\",\n";
        file << "  \"result_overlay_path\": \"" << r.result_overlay_path << "\",\n";
        file << "  \"evidence_overlay_path\": \"" << r.evidence_overlay_path << "\",\n";
        file << "  \"tool_display_path\": \"" << r.tool_display_path << "\",\n";
        file << "\n";
        file << "  \"roi_x0\": " << r.roi_x0 << ",\n";
        file << "  \"roi_y0\": " << r.roi_y0 << ",\n";
        file << "  \"roi_x1\": " << r.roi_x1 << ",\n";
        file << "  \"roi_y1\": " << r.roi_y1 << ",\n";
        file << "  \"circle_cx\": " << r.circle_cx << ",\n";
        file << "  \"circle_cy\": " << r.circle_cy << ",\n";
        file << "  \"circle_px\": " << r.circle_px << ",\n";
        file << "  \"circle_py\": " << r.circle_py << "\n";
        file << "}\n";
    }

    void WriteReplayPackage(
        const std::filesystem::path& caseDir,
        const CxScriptSuiteCase& suite_case,
        const ResolvedEvidenceCase& resolved,
        const CxScriptSuiteCaseResult& result)
    {
        std::ofstream file(caseDir / "replay_package.json");
        if (!file.is_open())
            return;

        file << "{\n";
        file << "  \"replay_version\": \"stage25-replay-v1\",\n";

        std::time_t now_c = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm now_tm;
        localtime_s(&now_tm, &now_c);
        std::stringstream ss;
        ss << std::put_time(&now_tm, "%Y-%m-%dT%H:%M:%S");
        file << "  \"created_at\": \"" << ss.str() << "\",\n";

        file << "\n";
        file << "  \"case_id\": \"" << suite_case.case_id << "\",\n";
        file << "  \"evidence_id\": \"" << suite_case.case_id << "\",\n";

        file << "\n";
        file << "  \"image\": {\n";
        file << "    \"image_id\": \"" << suite_case.image_id << "\",\n";
        if (resolved.image)
            file << "    \"path\": \"" << resolved.image->path << "\"\n";
        else
            file << "    \"path\": \"\"\n";
        file << "  },\n";

        file << "\n";
        file << "  \"target\": {\n";
        file << "    \"target_id\": \"" << suite_case.target_id << "\",\n";
        if (resolved.target)
        {
            file << "    \"tool\": \"" << resolved.target->tool << "\",\n";
            file << "    \"circle_cx\": " << resolved.target->cx << ",\n";
            file << "    \"circle_cy\": " << resolved.target->cy << ",\n";
            file << "    \"circle_px\": " << resolved.target->px << ",\n";
            file << "    \"circle_py\": " << resolved.target->py << ",\n";
            file << "    \"roi_x0\": " << resolved.target->x0 << ",\n";
            file << "    \"roi_y0\": " << resolved.target->y0 << ",\n";
            file << "    \"roi_x1\": " << resolved.target->x1 << ",\n";
            file << "    \"roi_y1\": " << resolved.target->y1 << ",\n";
            file << "    \"tool_half_width\": " << resolved.target->tool_half_width << "\n";
        }
        else
        {
            file << "    \"tool\": \"\",\n";
            file << "    \"circle_cx\": 0,\n";
            file << "    \"circle_cy\": 0,\n";
            file << "    \"circle_px\": 0,\n";
            file << "    \"circle_py\": 0,\n";
            file << "    \"roi_x0\": 0,\n";
            file << "    \"roi_y0\": 0,\n";
            file << "    \"roi_x1\": 0,\n";
            file << "    \"roi_y1\": 0,\n";
            file << "    \"tool_half_width\": 0\n";
        }
        file << "  },\n";

        file << "\n";
        file << "  \"script\": {\n";
        file << "    \"script_id\": \"" << suite_case.script_id << "\",\n";
        if (resolved.script)
            file << "    \"path\": \"" << resolved.script->path << "\",\n";
        else
            file << "    \"path\": \"\",\n";
        file << "    \"snapshot_path\": \"script_snapshot.cxsc\"\n";
        file << "  },\n";

        file << "\n";
        file << "  \"parameter\": {\n";
        file << "    \"profile_id\": \"" << suite_case.parameter_profile_id << "\",\n";
        if (resolved.profile)
        {
            file << "    \"method\": " << resolved.profile->method << ",\n";
            file << "    \"threshold\": " << resolved.profile->threshold << ",\n";
            file << "    \"gap\": " << resolved.profile->gap << ",\n";
            file << "    \"linegap\": " << resolved.profile->linegap << ",\n";
            file << "    \"wgap\": " << resolved.profile->wgap << ",\n";
            file << "    \"hgap\": " << resolved.profile->hgap << ",\n";
            file << "    \"filterprofile\": " << resolved.profile->filterprofile << "\n";
        }
        else
        {
            file << "    \"method\": 0,\n";
            file << "    \"threshold\": 0,\n";
            file << "    \"gap\": 0,\n";
            file << "    \"linegap\": 0,\n";
            file << "    \"wgap\": 0,\n";
            file << "    \"hgap\": 0,\n";
            file << "    \"filterprofile\": 0\n";
        }
        file << "  },\n";

        file << "\n";
        file << "  \"globals\": {\n";
        if (resolved.target)
        {
            file << "    \"global.circle_cx\": " << resolved.target->cx << ",\n";
            file << "    \"global.circle_cy\": " << resolved.target->cy << ",\n";
            file << "    \"global.circle_px\": " << resolved.target->px << ",\n";
            file << "    \"global.circle_py\": " << resolved.target->py << ",\n";
            file << "    \"global.roi_x0\": " << resolved.target->x0 << ",\n";
            file << "    \"global.roi_y0\": " << resolved.target->y0 << ",\n";
            file << "    \"global.roi_x1\": " << resolved.target->x1 << ",\n";
            file << "    \"global.roi_y1\": " << resolved.target->y1 << ",\n";
            file << "    \"global.wgap\": " << resolved.target->wgap << ",\n";
            file << "    \"global.hgap\": " << resolved.target->hgap << ",\n";
            file << "    \"global.gap\": " << resolved.target->gap << ",\n";
            file << "    \"global.linegap\": " << resolved.target->linegap << ",\n";
            file << "    \"global.tool_half_width\": " << resolved.target->tool_half_width << ",\n";
        }
        if (resolved.profile)
        {
            file << "    \"global.method\": " << resolved.profile->method << ",\n";
            file << "    \"global.threshold\": " << resolved.profile->threshold << ",\n";
            file << "    \"global.gap\": " << resolved.profile->gap << ",\n";
            file << "    \"global.linegap\": " << resolved.profile->linegap << ",\n";
            file << "    \"global.wgap\": " << resolved.profile->wgap << ",\n";
            file << "    \"global.hgap\": " << resolved.profile->hgap << ",\n";
            file << "    \"global.filterprofile\": " << resolved.profile->filterprofile << "\n";
        }
        else
        {
            file << "    \"global.method\": 0,\n";
            file << "    \"global.threshold\": 0,\n";
            file << "    \"global.gap\": 0,\n";
            file << "    \"global.linegap\": 0,\n";
            file << "    \"global.wgap\": 0,\n";
            file << "    \"global.hgap\": 0,\n";
            file << "    \"global.filterprofile\": 0\n";
        }
        file << "  },\n";

        file << "\n";
        file << "  \"last_status\": {\n";
        file << "    \"phase\": \"evidence_resolved\",\n";
        file << "    \"status\": \"completed\",\n";
        file << "    \"message\": \"Replay package generated before headless\",\n";
        file << "    \"elapsed_ms\": 0\n";
        file << "  },\n";

        file << "\n";
        file << "  \"artifacts\": {\n";
        file << "    \"roi_preview\": \"roi_preview.png\",\n";
        file << "    \"live_status\": \"live_status.json\",\n";
        file << "    \"run_trace\": \"run_trace.jsonl\",\n";
        file << "    \"cxparser_ext_trace\": \"cxparser_ext_trace.json\"\n";
        file << "  }\n";
        file << "}\n";
    }

    void EvaluateSuiteCaseContract(CxScriptSuiteCaseResult& r)
    {
        if (!r.headless_ok)
        {
            r.contract_pass = false;
            r.conclusion = "Headless execution failed";
            return;
        }

        if (r.contract_path.empty())
        {
            if (r.expected_result == "diagnostic")
            {
                r.contract_pass = true;
                r.conclusion = "Diagnostic script executed";
            }
            else
            {
                r.contract_pass = false;
                r.contract_status = "missing_contract";
                r.conclusion = "Missing cxscript contract; C++ does not judge OK/NG";
            }
            return;
        }

        const std::filesystem::path contractDir =
            std::filesystem::path(r.case_dir) / "contract";
        std::filesystem::create_directories(contractDir);

        CxScriptHeadlessOptions contractHeadless;
        contractHeadless.enabled = true;
        contractHeadless.image_path = r.image_path;
        contractHeadless.script_path = r.contract_path;
        contractHeadless.output_dir = contractDir.string();
        contractHeadless.case_name = r.case_id + "_contract";
        contractHeadless.save_overlay = false;
        contractHeadless.summary_path =
            (contractDir / "contract_summary.json").string();
        contractHeadless.snapshot_path =
            (contractDir / "contract_snapshot.txt").string();

        contractHeadless.stage25_image_id = r.image_id;
        contractHeadless.stage25_level = r.level;
        contractHeadless.stage25_target_id = r.target_id;
        contractHeadless.stage25_tool = r.tool;

        contractHeadless.contract_context_enabled = true;
        contractHeadless.contract_headless_ok = r.headless_ok ? 1 : 0;
        contractHeadless.contract_pass_initial = 0;
        contractHeadless.points_count = r.points_count;
        contractHeadless.valid_points_count = r.valid_points_count;
        contractHeadless.has_fit_line = r.has_fit_line ? 1 : 0;
        contractHeadless.has_fit_circle = r.has_fit_circle ? 1 : 0;
        contractHeadless.local_support = r.local_support;
        contractHeadless.local_mean_distance = r.local_mean_distance;
        contractHeadless.fit_offset = r.fit_offset;
        contractHeadless.circle_radius = r.circle_radius;
        contractHeadless.avgdist = r.avgdist;
        contractHeadless.policy_guard = r.actual_policy_guard;
        contractHeadless.result_status = r.result_status;
        contractHeadless.failure_stage = r.failure_stage;
        contractHeadless.result_overlay_path = r.result_overlay_path;
        contractHeadless.evidence_overlay_path = r.evidence_overlay_path;
        contractHeadless.tool_display_path = r.tool_display_path;

        CxScriptHeadlessResult contractResult;
        RunCxScriptHeadless(contractHeadless, contractResult);

        if (!contractResult.ok)
        {
            r.contract_pass = false;
            r.conclusion = "Contract script failed: " + contractResult.reason;
            return;
        }

        if (contractResult.summary_path.empty())
        {
            r.contract_pass = false;
            r.conclusion = "Contract summary file not found";
            return;
        }

        std::ifstream contractFile(contractResult.summary_path);
        if (contractFile.is_open())
        {
            std::string line;
            while (std::getline(contractFile, line))
            {
                if (JsonLineHasKey(line, "contract_pass"))
                {
                    r.contract_pass = ParseJsonBoolValue(JsonLineValue(line));
                }
                else if (JsonLineHasKey(line, "contract_status"))
                {
                    r.contract_status = JsonLineValue(line);
                }
                else if (JsonLineHasKey(line, "contract_conclusion"))
                {
                    r.contract_conclusion = JsonLineValue(line);
                    r.conclusion = JsonLineValue(line);
                }
            }
        }
    }

    void RunSingleSuiteCase(
        const CxScriptSuiteCase& suiteCase,
        const CxScriptCatalogEntry& script,
        const CxScriptImageManifestEntry& image,
        const CxScriptImageManifestRuntime& manifest,
        const CxParameterProfileRuntime& profiles,
        const ResolvedEvidenceCase& resolved,
        const std::filesystem::path& outRoot,
        const CxScriptSuiteRunOptions& options,
        CxScriptSuiteCaseResult& out)
    {
        out.case_id = suiteCase.case_id;
        out.evidence_id = suiteCase.case_id;
        out.script_id = suiteCase.script_id;
        out.script_path = script.path;
        out.image_id = image.image_id;
        out.image_path = image.path;
        out.level = image.level;
        out.target_id = suiteCase.target_id;
        out.tool = script.tool;
        out.parameter_profile_id = suiteCase.parameter_profile_id;
        out.contract_id = script.contract_path;

        out.expected_result =
            !suiteCase.expected_result.empty()
                ? suiteCase.expected_result
                : script.expected_result;

        out.expected_policy_guard =
            !suiteCase.expected_policy_guard.empty()
                ? suiteCase.expected_policy_guard
                : script.expected_policy_guard;

        out.contract_path = resolved.contract_path;

        const std::filesystem::path caseDir =
            outRoot /
            "cases" /
            image.level /
            image.image_id /
            script.script_id /
            suiteCase.case_id;

        std::filesystem::create_directories(caseDir);
        out.case_dir = caseDir.string();

        CxScriptRunTraceRuntime trace;
        std::string traceReason;

        if (options.trace_run)
        {
            std::string sessionId = suiteCase.case_id;
            trace.open(caseDir, sessionId, suiteCase.case_id, traceReason);
            trace.set_case_context(
                suiteCase.case_id,
                suiteCase.image_id,
                suiteCase.target_id,
                suiteCase.script_id,
                suiteCase.parameter_profile_id);
        }

        if (options.trace_run)
            trace.event("resolve_evidence", "end", "Evidence resolved");

        {
            ScopedSuiteTimer timer("roi_preview:" + suiteCase.case_id);
            WriteCaseTrace(caseDir, suiteCase, resolved);
            AppendPhaseTrace(caseDir, "roi_preview", "begin", "", 0);
            if (options.trace_run)
                trace.event("roi_preview", "begin", "Generating ROI preview");
            ExportRoiPreview(image.path, caseDir, suiteCase, resolved);
            out.roi_preview_path = (caseDir / "roi_preview.png").string();
            if (options.trace_run)
                trace.event("roi_preview", "end", "ROI preview generated");
            AppendPhaseTrace(caseDir, "roi_preview", "end", "", timer.elapsed_ms());
        }

        if (StopForHumanReviewIfNeeded(options, out, "roi", "accept_roi or reject_roi"))
        {
            WriteEvidencePacket(caseDir, suiteCase, resolved, out);
            if (options.dump_replay_package)
            {
                CopyScriptSnapshot(script.path, caseDir);
                WriteReplayPackage(caseDir, suiteCase, resolved, out);
            }
            return;
        }

        {
            ScopedSuiteTimer timer("headless:" + suiteCase.case_id);
            AppendPhaseTrace(caseDir, "headless", "begin", "", 0);
            if (options.trace_run)
                trace.event("headless", "begin", "Running cxscript headless");

            if (options.trace_run)
            {
                CxAlgorithmTraceScope::SetCallback(
                    [&](const CxAlgorithmTraceEvent& e)
                    {
                        trace.event(
                            "algorithm." + e.tool + "." + e.phase,
                            e.status,
                            e.message + " scan=" + std::to_string(e.scan_index) +
                            " samples=" + std::to_string(e.sample_count) +
                            " valid=" + std::to_string(e.valid_points) +
                            " algorithm_elapsed_ms=" + std::to_string(e.elapsed_ms));
                    });
            }

            CxScriptHeadlessOptions headless;
            headless.enabled = true;
            headless.image_path = image.path;
            headless.script_path = script.path;
            headless.output_dir = caseDir.string();
            headless.case_name = suiteCase.case_id;
            headless.save_overlay = options.save_overlay;
            headless.enable_evidence_analysis = false;

            headless.stage25_image_id = image.image_id;
            headless.stage25_level = image.level;
            headless.stage25_target_id = suiteCase.target_id;
            headless.stage25_tool = script.tool;

            if (resolved.target)
            {
                headless.roi_x0 = resolved.target->x0;
                headless.roi_y0 = resolved.target->y0;
                headless.roi_x1 = resolved.target->x1;
                headless.roi_y1 = resolved.target->y1;
                headless.circle_cx = resolved.target->cx;
                headless.circle_cy = resolved.target->cy;
                headless.circle_px = resolved.target->px;
                headless.circle_py = resolved.target->py;
                headless.wgap = resolved.target->wgap;
                headless.hgap = resolved.target->hgap;
                headless.gap = resolved.target->gap;
                headless.linegap = resolved.target->linegap;
                headless.tool_half_width = resolved.target->tool_half_width;

                out.roi_x0 = resolved.target->x0;
                out.roi_y0 = resolved.target->y0;
                out.roi_x1 = resolved.target->x1;
                out.roi_y1 = resolved.target->y1;
                out.circle_cx = resolved.target->cx;
                out.circle_cy = resolved.target->cy;
                out.circle_px = resolved.target->px;
                out.circle_py = resolved.target->py;
            }

            if (resolved.profile)
            {
                InjectParameterGlobals(headless, *resolved.profile);
            }

            CxScriptHeadlessResult headlessResult;
            RunCxScriptHeadless(headless, headlessResult);

            out.headless_ok = headlessResult.ok;
            out.case_dir = caseDir.string();
            out.snapshot_path = headlessResult.snapshot_path;
            out.summary_path = headlessResult.summary_path;
            out.result_overlay_path = headlessResult.overlay_path;

            std::cout << "[DEBUG] Loading metrics from summary\n";
            LoadSuiteCaseMetricsFromSummary(out.summary_path, out);
            std::cout << "[DEBUG] Metrics loaded, headless_ok=" << (out.headless_ok ? "true" : "false") << "\n";

            if (!out.headless_ok)
            {
                out.contract_pass = false;
                out.actual_policy_guard = "HEADLESS_FAILED";
                out.failure_stage = "headless_execution_failed";
                out.conclusion = "Headless execution failed: " + headlessResult.reason;

                if (options.trace_run)
                    trace.event("headless", "error", "Headless execution failed: " + headlessResult.reason);

                if (options.export_tool_display)
                {
                    out.tool_display_path =
                        CxScriptToolDisplayExporter::ExportToolDisplay(
                            image.path,
                            out.result_overlay_path,
                            out.evidence_overlay_path,
                            caseDir / "tool_display.png",
                            out);
                }

                WriteEvidencePacket(caseDir, suiteCase, resolved, out);
                WriteFinalCaseSummary(out, caseDir);
                if (options.dump_replay_package)
                {
                    CopyScriptSnapshot(script.path, caseDir);
                    WriteReplayPackage(caseDir, suiteCase, resolved, out);
                }
                if (options.trace_run)
                    trace.close();
                return;
            }

            if (options.trace_run)
                trace.event("headless", "end", "Headless finished");

            CxAlgorithmTraceScope::Clear();
            AppendPhaseTrace(caseDir, "headless", "end", "", timer.elapsed_ms());

            std::cout << "[DEBUG] headless completed, tool_display starting\n";
        }

        {
            ScopedSuiteTimer timer("tool_display:" + suiteCase.case_id);
            AppendPhaseTrace(caseDir, "tool_display", "begin", "", 0);
            AppendNodeTrace(caseDir, "N110_tool_display", "tool_display", "begin", "Exporting tool display");
            std::cout << "[DEBUG] tool_display begin\n";
            if (options.trace_run)
                trace.event("tool_display", "begin", "Exporting tool display");

            if (options.export_tool_display)
            {
                out.tool_display_path =
                    CxScriptToolDisplayExporter::ExportToolDisplay(
                        image.path,
                        out.result_overlay_path,
                        out.evidence_overlay_path,
                        caseDir / "tool_display.png",
                        out);
            }

            if (options.trace_run)
                trace.event("tool_display", "end", "Tool display exported");
            AppendPhaseTrace(caseDir, "tool_display", "end", "", timer.elapsed_ms());
            AppendNodeTrace(caseDir, "N110_tool_display", "tool_display", "end", "Tool display exported", timer.elapsed_ms());
        }

        if (StopForHumanReviewIfNeeded(options, out, "result", "accept_result or reject_overlay or derive_profile"))
        {
            WriteEvidencePacket(caseDir, suiteCase, resolved, out);
            WriteFinalCaseSummary(out, caseDir);
            if (options.dump_replay_package)
            {
                CopyScriptSnapshot(script.path, caseDir);
                WriteReplayPackage(caseDir, suiteCase, resolved, out);
            }
            if (options.trace_run)
                trace.close();
            return;
        }

        {
            ScopedSuiteTimer timer("contract:" + suiteCase.case_id);
            AppendPhaseTrace(caseDir, "contract", "begin", "", 0);
            AppendNodeTrace(caseDir, "N120_contract", "contract", "begin", "Running contract cxscript");
            if (options.trace_run)
                trace.event("contract", "begin", "Running contract cxscript");
            EvaluateSuiteCaseContract(out);
            if (options.trace_run)
                trace.event("contract", "end", "Contract finished");
            AppendPhaseTrace(caseDir, "contract", "end", "", timer.elapsed_ms());
            AppendNodeTrace(caseDir, "N120_contract", "contract", "end", 
                "Contract finished, pass=" + std::to_string(out.contract_pass ? 1 : 0), 
                timer.elapsed_ms());
        }

        if (StopForHumanReviewIfNeeded(options, out, "contract", "accept_contract or reject_contract"))
        {
            WriteEvidencePacket(caseDir, suiteCase, resolved, out);
            WriteFinalCaseSummary(out, caseDir);
            if (options.dump_replay_package)
            {
                CopyScriptSnapshot(script.path, caseDir);
                WriteReplayPackage(caseDir, suiteCase, resolved, out);
            }
            if (options.trace_run)
                trace.close();
            return;
        }

        WriteEvidencePacket(caseDir, suiteCase, resolved, out);
        WriteFinalCaseSummary(out, caseDir);

        if (options.dump_replay_package)
        {
            CopyScriptSnapshot(script.path, caseDir);
            WriteReplayPackage(caseDir, suiteCase, resolved, out);
        }

        AppendPhaseTrace(caseDir, "promotion", "begin", "", 0);
        if (options.trace_run)
            trace.event("promotion", "begin", "Promotion stage");

        if (StopForHumanReviewIfNeeded(options, out, "promotion", "accept_to_regression or derive_diagnostic_profile"))
        {
            AppendPhaseTrace(caseDir, "promotion", "end", "", 0);
            if (options.trace_run)
                trace.close();
            return;
        }

        AppendPhaseTrace(caseDir, "promotion", "end", "", 0);

        if (out.contract_pass && out.headless_ok)
        {
            WriteHumanReview(
                suiteCase.case_id,
                "accept",
                suiteCase.case_id + " baseline accepted. ROI, measure points, fit circle, and evidence summary are consistent.",
                "promote_to_baseline",
                caseDir);
        }

        if (options.trace_run)
        {
            trace.event("promotion", "end", "Promotion finished");
            trace.close();
        }
    }

    int CountExecuted(const std::vector<CxScriptSuiteCaseResult>& cases)
    {
        int count = 0;
        for (const auto& c : cases)
            if (c.headless_ok)
                ++count;
        return count;
    }

    int CountContractPass(const std::vector<CxScriptSuiteCaseResult>& cases)
    {
        int count = 0;
        for (const auto& c : cases)
            if (c.contract_pass)
                ++count;
        return count;
    }

    bool StopForHumanReviewIfNeeded(
        const CxScriptSuiteRunOptions& options,
        CxScriptSuiteCaseResult& caseResult,
        const std::string& stage,
        const std::string& suggestedAction)
    {
        if (!options.require_human_review)
            return false;

        if (!options.review_stage.empty() &&
            options.review_stage != stage)
            return false;

        if (!options.resume_review_id.empty())
        {
            CxScriptHumanReview review;
            std::string reason;
            std::filesystem::path reviewPath =
                std::filesystem::path(caseResult.case_dir) / "human_review.json";

            if (LoadHumanReviewJson(reviewPath, review, reason))
            {
                if (review.review_id == options.resume_review_id &&
                    (options.review_decision.empty() || review.decision == options.review_decision))
                {
                    return false;
                }
            }

            if (!options.review_decision.empty())
            {
                CxScriptHumanReview manualReview;
                manualReview.review_id = options.resume_review_id;
                manualReview.decision = options.review_decision;
                manualReview.reviewer = "command_line";
                manualReview.note = "Review decision from command line";
                manualReview.next_action = "continue";

                std::string writeReason;
                WriteHumanReviewJson(reviewPath, manualReview, writeReason);
                return false;
            }
        }

        CxScriptReviewRequest request;
        request.review_id = caseResult.case_id + "_" + stage;
        request.case_id = caseResult.case_id;
        request.evidence_id = caseResult.evidence_id;
        request.stage = stage;
        request.image_id = caseResult.image_id;
        request.target_id = caseResult.target_id;
        request.script_id = caseResult.script_id;
        request.parameter_profile_id = caseResult.parameter_profile_id;
        request.contract_id = caseResult.contract_id;

        request.roi_preview_path = caseResult.roi_preview_path;
        request.tool_display_path = caseResult.tool_display_path;
        request.result_summary_path = caseResult.summary_path;
        request.evidence_packet_path = caseResult.evidence_packet_path;
        request.contract_result_path = caseResult.contract_result_path;
        request.suggested_action = suggestedAction;

        if (stage == "roi")
            request.reason = "ROI preview generated. Please confirm whether the green ROI covers the intended target.";
        else if (stage == "result")
            request.reason = "Tool display generated. Please confirm whether the result matches the expected output.";
        else if (stage == "contract")
            request.reason = "Contract evaluation completed. Please confirm whether the contract result is consistent with the visual evidence.";
        else if (stage == "promotion")
            request.reason = "Case execution completed. Please decide whether to promote this case to the formal regression suite.";
        else
            request.reason = "Review required at stage: " + stage;

        std::string reason;
        std::filesystem::path requestPath =
            std::filesystem::path(caseResult.case_dir) / "review_request.json";
        WriteReviewRequestJson(requestPath, request, reason);

        std::cout << "[review-required]\n";
        std::cout << "review_id=" << request.review_id << "\n";
        std::cout << "stage=" << stage << "\n";
        std::cout << "roi_preview=" << request.roi_preview_path << "\n";
        std::cout << "tool_display=" << request.tool_display_path << "\n";
        std::cout << "review_request=" << requestPath.string() << "\n";
        std::cout << std::flush;

        caseResult.stopped_for_review = true;
        caseResult.review_stage = stage;
        return true;
    }

    bool WriteHumanReviewJson(
        const std::filesystem::path& path,
        const CxScriptHumanReview& review,
        std::string& reason)
    {
        try
        {
            std::ofstream file(path);
            if (!file.is_open())
            {
                reason = "Cannot open human review file: " + path.string();
                return false;
            }

            file << "{\n";
            file << "  \"review_id\": \"" << review.review_id << "\",\n";
            file << "  \"decision\": \"" << review.decision << "\",\n";
            file << "  \"reviewer\": \"" << review.reviewer << "\",\n";
            file << "  \"note\": \"" << review.note << "\",\n";
            file << "  \"next_action\": \"" << review.next_action << "\"";

            if (!review.suggested_profile_id.empty())
            {
                file << ",\n";
                file << "  \"suggested_profile_id\": \"" << review.suggested_profile_id << "\"";
            }

            file << "\n";
            file << "}\n";

            return true;
        }
        catch (const std::exception& e)
        {
            reason = "Failed to write human review: " + std::string(e.what());
            return false;
        }
    }
}

bool RunCxScriptSuite(
    const CxScriptSuiteRunOptions& options,
    CxScriptSuiteRunResult& result)
{
    result = CxScriptSuiteRunResult{};

    CxScriptSuiteRuntime suite;
    std::string reason;

    if (!LoadCxScriptSuiteFile(options.suite_path, suite, reason))
    {
        result.reason = reason;
        return false;
    }

    CxScriptCatalogRuntime catalog;
    const std::string catalogPath =
        !options.catalog_path_override.empty()
            ? options.catalog_path_override
            : suite.catalog_path;

    if (!LoadCxScriptCatalogFile(catalogPath, catalog, reason))
    {
        result.reason = reason;
        return false;
    }

    CxScriptImageManifestRuntime imageManifest;

    if (!LoadStage25ImageManifestJson(
            options.image_manifest_path,
            imageManifest,
            reason))
    {
        result.reason = reason;
        return false;
    }

    auto validation = ValidateStage25ImageManifest(imageManifest);

    const std::filesystem::path outRoot =
        !options.out_root_override.empty()
            ? std::filesystem::path(options.out_root_override)
            : std::filesystem::path(suite.output_root);

    std::filesystem::create_directories(outRoot);

    CxScriptSuiteReportWriter::WriteImageManifestContractReport(
        outRoot,
        imageManifest,
        validation);

    if (!validation.ok)
    {
        result.ok = false;
        result.reason = "image manifest validation failed";
        result.report_root = outRoot.string();
        return false;
    }

    CxParameterProfileRuntime parameterProfiles;
    if (!options.parameter_profile_path.empty())
    {
        std::string loadReason;
        if (LoadCxParameterProfileFile(options.parameter_profile_path, parameterProfiles, loadReason))
        {
            std::cout << "[DEBUG] Loaded " << parameterProfiles.profiles.size() << " parameter profiles\n";
        }
        else
        {
            std::cerr << "[WARN] Failed to load parameter profiles: " << loadReason << "\n";
        }
    }

    for (const auto& suiteCase : suite.cases)
    {
        ResolvedEvidenceCase resolved;
        std::string resolveReason;

        {
            ScopedSuiteTimer timer("resolve_evidence:" + suiteCase.case_id);
            if (!ResolveEvidenceCase(suiteCase, catalog, imageManifest, parameterProfiles, resolved, resolveReason))
            {
                CxScriptSuiteCaseResult failCase;
                failCase.case_id = suiteCase.case_id;
                failCase.script_id = suiteCase.script_id;
                failCase.image_id = suiteCase.image_id;
                failCase.headless_ok = false;
                failCase.contract_pass = false;
                failCase.failure_stage = "evidence_resolution_failed";
                failCase.conclusion = resolveReason;

                const std::filesystem::path caseDir =
                    outRoot /
                    "cases" /
                    suiteCase.level /
                    suiteCase.image_id /
                    suiteCase.script_id /
                    suiteCase.case_id;
                std::filesystem::create_directories(caseDir);

                WriteCaseTrace(caseDir, suiteCase, resolved);
                WriteEvidencePacket(caseDir, suiteCase, resolved, failCase);

                if (resolved.image)
                    ExportRoiPreview(resolved.image->path, caseDir, suiteCase, resolved);

                result.case_results.push_back(failCase);
                continue;
            }
        }

        if (options.dry_run)
        {
            CxScriptSuiteCaseResult dryCase;
            dryCase.case_id = suiteCase.case_id;
            dryCase.script_id = suiteCase.script_id;
            dryCase.image_id = suiteCase.image_id;
            dryCase.target_id = suiteCase.target_id;
            dryCase.parameter_profile_id = suiteCase.parameter_profile_id;
            dryCase.headless_ok = false;
            dryCase.contract_pass = false;
            dryCase.failure_stage = "dry_run_completed";
            dryCase.conclusion = "Dry run: all evidence chain components resolved successfully";

            std::cout << "[dry-run] case_id=" << suiteCase.case_id
                      << " script_found=" << (resolved.script ? "true" : "false")
                      << " image_found=" << (resolved.image ? "true" : "false")
                      << " target_found=" << (resolved.target ? "true" : "false")
                      << " parameter_found=" << (resolved.profile ? "true" : "false")
                      << " contract_found=" << (!resolved.contract_path.empty() ? "true" : "false")
                      << "\n";

            result.case_results.push_back(dryCase);
            continue;
        }

        CxScriptSuiteCaseResult caseResult;

        if (options.preview_only)
        {
            const std::filesystem::path caseDir =
                outRoot /
                "cases" /
                suiteCase.level /
                suiteCase.image_id /
                suiteCase.script_id /
                suiteCase.case_id;
            std::filesystem::create_directories(caseDir);

            caseResult.case_id = suiteCase.case_id;
            caseResult.evidence_id = suiteCase.case_id;
            caseResult.script_id = suiteCase.script_id;
            caseResult.image_id = suiteCase.image_id;
            caseResult.target_id = suiteCase.target_id;
            caseResult.parameter_profile_id = suiteCase.parameter_profile_id;
            caseResult.contract_id = resolved.script ? resolved.script->contract_path : "";
            caseResult.case_dir = caseDir.string();

            CxScriptRunTraceRuntime trace;
            std::string traceReason;
            if (options.trace_run)
            {
                trace.open(caseDir, suiteCase.case_id, suiteCase.case_id, traceReason);
                trace.set_case_context(
                    suiteCase.case_id,
                    suiteCase.image_id,
                    suiteCase.target_id,
                    suiteCase.script_id,
                    suiteCase.parameter_profile_id);
                trace.event("resolve_evidence", "end", "Evidence resolved");
            }

            {
                ScopedSuiteTimer timer("roi_preview:" + suiteCase.case_id);
                if (options.trace_run)
                    trace.event("roi_preview", "begin", "Generating ROI preview");
                WriteCaseTrace(caseDir, suiteCase, resolved);
                ExportRoiPreview(resolved.image->path, caseDir, suiteCase, resolved);
                caseResult.roi_preview_path = (caseDir / "roi_preview.png").string();
                if (options.trace_run)
                    trace.event("roi_preview", "end", "ROI preview generated");
            }

            WriteEvidencePacket(caseDir, suiteCase, resolved, caseResult);

            if (options.dump_replay_package)
            {
                CopyScriptSnapshot(resolved.script ? resolved.script->path : "", caseDir);
                WriteReplayPackage(caseDir, suiteCase, resolved, caseResult);
            }

            if (options.trace_run)
                trace.close();

            if (StopForHumanReviewIfNeeded(options, caseResult, "roi", "accept_roi or reject_roi"))
            {
                caseResult.stopped_for_review = true;
                caseResult.review_stage = "roi";
            }

            result.case_results.push_back(caseResult);
            continue;
        }

        RunSingleSuiteCase(
            suiteCase,
            *resolved.script,
            *resolved.image,
            imageManifest,
            parameterProfiles,
            resolved,
            outRoot,
            options,
            caseResult);

        result.case_results.push_back(caseResult);
    }

    if (options.export_best_examples)
    {
        CxScriptBestCaseSelector::SelectAndExportBestExamples(
            outRoot,
            result.case_results);
    }

    CxScriptSuiteReportWriter::WriteSuiteRunReport(
        outRoot,
        result.case_results);

    CxScriptSuiteReportWriter::WriteBestDetectionGallery(
        outRoot,
        result.case_results);

    CxScriptSuiteReportWriter::WriteFindlineAlgorithmIterationReport(
        outRoot,
        result.case_results);

    CxScriptSuiteReportWriter::WriteFindcircleAlgorithmIterationReport(
        outRoot,
        result.case_results);

    CxScriptSuiteReportWriter::WriteFailureClassificationReport(
        outRoot,
        result.case_results);

    result.total_cases = static_cast<int>(result.case_results.size());
    result.executed_cases = CountExecuted(result.case_results);
    result.contract_pass = CountContractPass(result.case_results);
    result.contract_fail = result.executed_cases - result.contract_pass;
    result.report_root = outRoot.string();

    bool stoppedForReview = false;
    for (const auto& cr : result.case_results)
    {
        if (cr.stopped_for_review)
        {
            stoppedForReview = true;
            break;
        }
    }

    if (stoppedForReview)
    {
        result.ok = false;
        result.reason = "REVIEW_REQUIRED";
        return false;
    }

    result.ok = result.contract_fail == 0;
    result.reason = result.ok ? "suite passed" : "suite has contract failures";

    return result.ok;
}
