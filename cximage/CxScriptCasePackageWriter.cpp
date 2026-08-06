#include "pch.h"
#include "CxScriptCasePackageWriter.h"
#include "CxUnifiedLog.h"
#include "ManualConsoleUtils.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <fstream>
#include <sstream>

namespace
{
std::string SafePathComponentForCandidate(const std::string& value)
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

std::filesystem::path CanonicalEvidenceCandidateRoot(
    const std::string& configuredRoot,
    bool* outCanonicalized = nullptr)
{
    std::filesystem::path root = ResolveCxVisionRunPath(configuredRoot);
    const std::string generic = root.generic_string();
    const bool isBuildReleaseEvidenceRoot =
        generic.find("/Release/cxscript_runs/evidence_candidates") != std::string::npos ||
        generic.find("/Debug/cxscript_runs/evidence_candidates") != std::string::npos ||
        generic.find("/RelWithDebInfo/cxscript_runs/evidence_candidates") != std::string::npos ||
        generic.find("/MinSizeRel/cxscript_runs/evidence_candidates") != std::string::npos;

    if (isBuildReleaseEvidenceRoot)
    {
        if (outCanonicalized)
            *outCanonicalized = true;
        return ResolveCxVisionRunPath("cxscript_runs/evidence_candidates");
    }

    if (outCanonicalized)
        *outCanonicalized = false;
    return root;
}

void AppendEvidenceCandidateStateProbeImpl(
    const ManualTestContext& context,
    const std::string& candidateDir,
    const std::string& candidateId,
    const std::string& phase,
    const std::string& status,
    const std::string& reason)
{
    if (candidateDir.empty())
        return;

    const ManualGaugeState& g = context.current_gauge;
    auto globalValue = [&](const char* key) -> int
    {
        const auto it = context.runtime_int_vars.find(key);
        return it == context.runtime_int_vars.end() ? -2147483647 : it->second;
    };

    std::ostringstream line;
    line << "{"
         << "\"candidate_id\":\"" << JsonEscape(candidateId) << "\","
         << "\"phase\":\"" << JsonEscape(phase) << "\","
         << "\"status\":\"" << JsonEscape(status) << "\","
         << "\"reason\":\"" << JsonEscape(reason) << "\","
         << "\"selected_is_candidate\":"
         << (context.current_evidence_selection.is_candidate ? "true" : "false") << ","
         << "\"selected_candidate_id\":\""
         << JsonEscape(context.current_evidence_selection.candidate_id) << "\","
         << "\"script_path\":\"" << JsonEscape(context.loaded_script_path) << "\","
         << "\"gauge_source\":\"" << JsonEscape(g.source) << "\","
         << "\"gauge_dirty\":" << (g.dirty ? "true" : "false") << ","
         << "\"key_parameter_edit_revision\":"
         << context.key_parameter_edit_revision << ","
         << "\"last_key_parameter_edit_summary\":\""
         << JsonEscape(context.last_key_parameter_edit_summary) << "\","
         << "\"gauge\":{"
         << "\"x0\":" << g.line_x0 << ",\"y0\":" << g.line_y0
         << ",\"x1\":" << g.line_x1 << ",\"y1\":" << g.line_y1
         << ",\"half_width\":" << g.tool_half_width
         << ",\"threshold\":" << g.threshold
         << ",\"method\":" << g.method
         << ",\"linegap\":" << g.linegap
         << ",\"wgap\":" << g.wgap
         << ",\"hgap\":" << g.hgap
         << ",\"filterprofile\":" << g.filterprofile << "},"
         << "\"globals\":{"
         << "\"global_roi_x0\":" << globalValue("global_roi_x0")
         << ",\"global_roi_y0\":" << globalValue("global_roi_y0")
         << ",\"global_roi_x1\":" << globalValue("global_roi_x1")
         << ",\"global_roi_y1\":" << globalValue("global_roi_y1")
         << ",\"global_tool_half_width\":" << globalValue("global_tool_half_width")
         << ",\"global_threshold\":" << globalValue("global_threshold")
         << ",\"global_method\":" << globalValue("global_method")
         << ",\"global_linegap\":" << globalValue("global_linegap")
         << ",\"global_wgap\":" << globalValue("global_wgap")
         << ",\"global_hgap\":" << globalValue("global_hgap")
         << ",\"global_filterprofile\":" << globalValue("global_filterprofile")
         << ",\"global_findline_edge_count\":" << globalValue("global_findline_edge_count")
         << ",\"global_findline_selected_edge\":" << globalValue("global_findline_selected_edge")
         << ",\"global_findline_best_edge\":" << globalValue("global_findline_best_edge")
         << ",\"global_findline_recommended_edge\":" << globalValue("global_findline_recommended_edge")
         << ",\"global_findline_relation_edge\":" << globalValue("global_findline_relation_edge")
         << ",\"global_findline_attach_edge\":" << globalValue("global_findline_attach_edge")
         << "}}\n";

    const std::filesystem::path path =
        std::filesystem::path(candidateDir) / "candidate_state_probe.jsonl";
    std::ofstream file(path, std::ios::binary | std::ios::app);
    if (file.is_open())
        file << line.str();

    CXLOG_INFO(
        "EvidenceCandidate",
        phase.c_str(),
        status,
        "candidate_id=" + candidateId +
        " gauge={threshold=" + std::to_string(g.threshold) +
        ",method=" + std::to_string(g.method) +
        ",linegap=" + std::to_string(g.linegap) +
        ",wgap=" + std::to_string(g.wgap) +
        ",hgap=" + std::to_string(g.hgap) +
        ",filterprofile=" + std::to_string(g.filterprofile) +
        "} globals={threshold=" + std::to_string(globalValue("global_threshold")) +
        ",method=" + std::to_string(globalValue("global_method")) +
        ",linegap=" + std::to_string(globalValue("global_linegap")) +
        ",wgap=" + std::to_string(globalValue("global_wgap")) +
        ",hgap=" + std::to_string(globalValue("global_hgap")) +
        ",filterprofile=" + std::to_string(globalValue("global_filterprofile")) +
        ",edge_count=" + std::to_string(globalValue("global_findline_edge_count")) +
        ",selected_edge=" + std::to_string(globalValue("global_findline_selected_edge")) +
        ",best_edge=" + std::to_string(globalValue("global_findline_best_edge")) +
        ",recommended_edge=" + std::to_string(globalValue("global_findline_recommended_edge")) +
        ",relation_edge=" + std::to_string(globalValue("global_findline_relation_edge")) +
        ",attach_edge=" + std::to_string(globalValue("global_findline_attach_edge")) +
        "} reason=" + reason);
}

std::string GenerateCandidateId()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm local_time = {};
#if defined(_WIN32)
    localtime_s(&local_time, &t);
#else
    localtime_r(&t, &local_time);
#endif
    char buffer[32] = {};
    std::strftime(buffer, sizeof(buffer), "candidate_%Y%m%d_%H%M%S", &local_time);
    return buffer;
}

std::string EvidenceToolGroupLabel(const std::string& tool)
{
    if (tool == "FindLine" || tool == "Findline")
        return "FindLine";
    if (tool == "FindCircle" || tool == "Findcircle")
        return "FindCircle";
    if (tool == "FindEllipse" || tool == "Findellipse")
        return "FindEllipse";
    if (tool == "FindRect")
        return "FindRect";
    if (tool == "FastMatch" || tool == "fastmatch")
        return "FastMatch";
    if (tool == "FindSegmentation")
        return "FindSegmentation";
    return tool.empty() ? "Module" : tool;
}

std::string CurrentScriptText(const ManualTestContext& context)
{
    if (!context.editor_text.empty())
        return context.editor_text;

    if (!context.loaded_script_path.empty())
    {
        std::string text;
        if (ReadTextFile(context.loaded_script_path, text))
            return text;
    }
    if (!context.script_file_path.empty())
    {
        std::string text;
        if (ReadTextFile(context.script_file_path, text))
            return text;
    }
    return {};
}

std::string EffectiveScriptPath(const ManualTestContext& context)
{
    if (!context.loaded_script_path.empty())
        return context.loaded_script_path;
    if (!context.script_file_path.empty())
        return context.script_file_path;
    if (context.current_evidence_selection.valid)
        return context.current_evidence_selection.script_path;
    return {};
}

std::string EffectiveScriptId(const ManualTestContext& context)
{
    if (context.current_evidence_selection.valid &&
        !context.current_evidence_selection.script_id.empty())
        return context.current_evidence_selection.script_id;
    const std::string scriptPath = EffectiveScriptPath(context);
    if (!scriptPath.empty())
        return std::filesystem::path(scriptPath).filename().string();
    return context.active_script_case_name;
}

std::string EffectiveCaseId(const ManualTestContext& context)
{
    if (!context.active_case_id.empty())
        return context.active_case_id;
    if (context.current_evidence_selection.valid &&
        !context.current_evidence_selection.case_id.empty())
        return context.current_evidence_selection.case_id;
    if (!context.current_gauge.case_id.empty())
        return context.current_gauge.case_id;
    if (!context.active_script_case_name.empty())
        return context.active_script_case_name;
    return "manual_case";
}

std::string EffectiveImagePath(const ManualTestContext& context)
{
    if (!context.image_file_path.empty())
        return context.image_file_path;
    if (context.current_evidence_selection.valid)
        return context.current_evidence_selection.image_path;
    return {};
}

std::string EffectiveImageId(const ManualTestContext& context)
{
    if (!context.active_image_id.empty())
        return context.active_image_id;
    if (context.current_evidence_selection.valid)
        return context.current_evidence_selection.image_id;
    if (!context.current_gauge.image_id.empty())
        return context.current_gauge.image_id;
    return {};
}

std::string EffectiveTargetId(const ManualTestContext& context)
{
    if (!context.active_target_id.empty())
        return context.active_target_id;
    if (context.current_evidence_selection.valid)
        return context.current_evidence_selection.target_id;
    if (!context.current_gauge.target_id.empty())
        return context.current_gauge.target_id;
    return {};
}

std::string EffectiveTool(const ManualTestContext& context)
{
    if (!context.current_gauge.tool.empty())
        return context.current_gauge.tool;
    if (context.current_evidence_selection.valid)
        return context.current_evidence_selection.tool;
    return {};
}

std::string BuildParameterSummaryFromGlobals(const ManualTestContext& context)
{
    std::vector<std::pair<std::string, int>> vars(
        context.runtime_int_vars.begin(),
        context.runtime_int_vars.end());
    std::sort(vars.begin(), vars.end());

    std::ostringstream oss;
    bool first = true;
    for (const auto& kv : vars)
    {
        if (kv.first.rfind("global_", 0) != 0)
            continue;
        if (!first)
            oss << " ";
        oss << kv.first.substr(7) << "=" << kv.second;
        first = false;
    }
    return oss.str();
}

std::string EffectiveParameterSummary(const ManualTestContext& context)
{
    const std::string fromGlobals = BuildParameterSummaryFromGlobals(context);
    if (!fromGlobals.empty())
        return fromGlobals;
    if (context.current_evidence_selection.valid)
        return context.current_evidence_selection.parameter_summary;
    return {};
}

bool WriteGaugeSnapshotJson(
    const ManualGaugeState& gauge,
    const std::filesystem::path& path,
    std::string& reason)
{
    std::ostringstream json;
    json << "{\n"
         << "  \"schema_version\": 1,\n"
         << "  \"case_id\": \"" << JsonEscape(gauge.case_id) << "\",\n"
         << "  \"image_id\": \"" << JsonEscape(gauge.image_id) << "\",\n"
         << "  \"target_id\": \"" << JsonEscape(gauge.target_id) << "\",\n"
         << "  \"tool\": \"" << JsonEscape(gauge.tool) << "\",\n"
         << "  \"source\": \"" << JsonEscape(gauge.source) << "\",\n"
         << "  \"review_status\": \"" << JsonEscape(gauge.review_status) << "\",\n"
         << "  \"dirty\": " << (gauge.dirty ? "true" : "false") << ",\n"
         << "  \"accepted\": " << (gauge.accepted ? "true" : "false") << ",\n"
         << "  \"primary_object_type\": \"" << JsonEscape(gauge.primary_object_type) << "\",\n"
         << "  \"primary_object_name\": \"" << JsonEscape(gauge.primary_object_name) << "\",\n"
         << "  \"primary_object_status\": \"" << JsonEscape(gauge.primary_object_status) << "\",\n"
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
         << "  \"circle_arc_enabled\": "
         << (gauge.circle_arc_enabled ? "true" : "false") << ",\n"
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
    if (!WriteTextFile(path, json.str()))
    {
        reason = "failed to write gauge_annotation.json";
        return false;
    }
    return true;
}

bool WriteRuntimeGlobalsJson(
    const ManualTestContext& context,
    const std::filesystem::path& path,
    std::string& reason)
{
    std::vector<std::pair<std::string, int>> vars(
        context.runtime_int_vars.begin(),
        context.runtime_int_vars.end());
    std::sort(vars.begin(), vars.end());

    std::ostringstream json;
    json << "{\n"
         << "  \"schema_version\": 1,\n"
         << "  \"globals\": {";
    bool first = true;
    for (const auto& kv : vars)
    {
        if (!first)
            json << ",";
        json << "\n    \"" << JsonEscape(kv.first) << "\": " << kv.second;
        first = false;
    }
    if (!first)
        json << "\n  ";
    json << "}\n}\n";
    if (!WriteTextFile(path, json.str()))
    {
        reason = "failed to write runtime_globals.json";
        return false;
    }
    return true;
}

bool WriteLineTraceJson(
    const ManualTestContext& context,
    const std::filesystem::path& path,
    std::string& reason)
{
    std::ostringstream json;
    json << "{\n"
         << "  \"schema_version\": 1,\n"
         << "  \"lines\": [";
    bool first = true;
    for (const auto& line : context.line_views)
    {
        if (!first)
            json << ",";
        json << "\n    {"
             << "\"line_no\": " << line.line_no << ", "
             << "\"statement\": \"" << JsonEscape(line.statement) << "\", "
             << "\"status\": \"" << JsonEscape(line.status) << "\""
             << "}";
        first = false;
    }
    if (!first)
        json << "\n  ";
    json << "]\n}\n";
    if (!WriteTextFile(path, json.str()))
    {
        reason = "failed to write line_trace.json";
        return false;
    }
    return true;
}

void AppendCandidateToEvidenceChain(
    ManualTestContext& context,
    const CxEvidenceCandidateSaveResult& result,
    const std::string& caseId,
    const std::string& scriptId,
    const std::string& scriptPath,
    const std::string& sourceEvidenceScriptPath,
    const std::string& imageId,
    const std::string& imagePath,
    const std::string& targetId,
    const std::string& tool,
    const std::string& parameterSummary)
{
    auto bindWorkingRevision = [&](ScriptEvidenceThumb& target)
    {
        target.candidate_id = result.candidate_id;
        target.candidate_dir = result.candidate_dir;
        target.evidence_binding_path = result.evidence_binding_path;
        target.parameter_snapshot_path = result.parameter_snapshot_path;
        target.runtime_globals_path =
            (std::filesystem::path(result.candidate_dir) /
             "runtime_globals.json").string();
        target.gauge_annotation_path = result.gauge_annotation_path;
        target.working_script_snapshot_path = scriptPath;
        target.has_saved_state = true;
        target.source_evidence_script_path = sourceEvidenceScriptPath;
        target.parameter_summary = parameterSummary;
        target.primary_object_type = context.current_gauge.primary_object_type;
        target.primary_object_name = context.current_gauge.primary_object_name;
        target.primary_object_status = context.current_gauge.primary_object_status;
        target.status = "pending_human_review";
        target.reason =
            "active working revision=" + result.candidate_id +
            "; baseline source remains unchanged";
    };

    bool workingRevisionBound = false;
    const int selectedGroup = context.selected_evidence_group;
    const int selectedThumb = context.selected_evidence_thumb;
    if (selectedGroup >= 0 &&
        selectedGroup < static_cast<int>(context.script_evidence_groups.size()))
    {
        ScriptEvidenceGroup& selected =
            context.script_evidence_groups[selectedGroup];
        if (selectedThumb >= 0 &&
            selectedThumb < static_cast<int>(selected.thumbs.size()) &&
            !selected.thumbs[selectedThumb].is_candidate)
        {
            bindWorkingRevision(selected.thumbs[selectedThumb]);
            workingRevisionBound = true;
        }
    }

    if (!workingRevisionBound)
    {
        const std::filesystem::path resolvedSource =
            ResolveWorkspaceFile(sourceEvidenceScriptPath).lexically_normal();
        for (auto& group : context.script_evidence_groups)
        {
            for (auto& existing : group.thumbs)
            {
                if (existing.is_candidate)
                    continue;
                const bool sameCase = !caseId.empty() &&
                    existing.case_id == caseId;
                const bool sameScript = !sourceEvidenceScriptPath.empty() &&
                    ResolveWorkspaceFile(existing.script_path).lexically_normal() ==
                        resolvedSource;
                const bool identityMatches = !caseId.empty()
                    ? (sameCase &&
                       (sourceEvidenceScriptPath.empty() || sameScript))
                    : sameScript;
                if (!identityMatches)
                    continue;
                bindWorkingRevision(existing);
                workingRevisionBound = true;
                break;
            }
            if (workingRevisionBound)
                break;
        }
    }

    if (!context.current_evidence_selection.is_candidate)
    {
        context.current_evidence_selection.candidate_id = result.candidate_id;
        context.current_evidence_selection.candidate_dir = result.candidate_dir;
        context.current_evidence_selection.evidence_binding_path =
            result.evidence_binding_path;
        context.current_evidence_selection.parameter_snapshot_path =
            result.parameter_snapshot_path;
        context.current_evidence_selection.runtime_globals_path =
            (std::filesystem::path(result.candidate_dir) /
             "runtime_globals.json").string();
        context.current_evidence_selection.gauge_annotation_path =
            result.gauge_annotation_path;
        context.current_evidence_selection.working_script_snapshot_path =
            scriptPath;
        context.current_evidence_selection.has_saved_state = true;
        context.current_evidence_selection.source_evidence_script_path =
            sourceEvidenceScriptPath;
        context.current_evidence_selection.parameter_summary = parameterSummary;
        context.current_evidence_selection.parameter_profile_id =
            parameterSummary;
        context.current_evidence_selection.primary_object_type =
            context.current_gauge.primary_object_type;
        context.current_evidence_selection.primary_object_name =
            context.current_gauge.primary_object_name;
        context.current_evidence_selection.primary_object_status =
            context.current_gauge.primary_object_status;
        context.current_evidence_selection.status = "pending_human_review";
        context.current_evidence_selection.reason =
            "active working revision=" + result.candidate_id;
    }

    ScriptEvidenceThumb thumb;
    thumb.candidate_id = result.candidate_id;
    thumb.candidate_dir = result.candidate_dir;
    thumb.evidence_binding_path = result.evidence_binding_path;
    thumb.parameter_snapshot_path = result.parameter_snapshot_path;
    thumb.runtime_globals_path =
        (std::filesystem::path(result.candidate_dir) / "runtime_globals.json").string();
    thumb.gauge_annotation_path = result.gauge_annotation_path;
    thumb.working_script_snapshot_path = result.script_snapshot_path;
    thumb.is_candidate = true;
    thumb.has_saved_state = true;
    thumb.source_evidence_script_path = sourceEvidenceScriptPath;
    thumb.case_id = caseId;
    thumb.script_id =
        (scriptId.empty() ? std::string("candidate") : scriptId) +
        " [" + result.candidate_id + "]";
    thumb.script_path = result.script_snapshot_path;
    thumb.image_id = imageId;
    thumb.image_path = imagePath;
    thumb.thumbnail_path = imagePath;
    thumb.target_id = targetId;
    thumb.tool = tool;
    thumb.parameter_summary = parameterSummary;
    thumb.primary_object_type = context.current_gauge.primary_object_type;
    thumb.primary_object_name = context.current_gauge.primary_object_name;
    thumb.primary_object_status = context.current_gauge.primary_object_status;
    thumb.status = "pending_human_review";
    thumb.reason =
        "evidence candidate saved; candidate_id=" + result.candidate_id +
        "; candidate_dir=" + result.candidate_dir;
    thumb.primary_object_type = context.current_gauge.primary_object_type;
    thumb.primary_object_name = context.current_gauge.primary_object_name;
    thumb.primary_object_status =
        context.current_gauge.primary_object_status.empty()
            ? "unresolved"
            : context.current_gauge.primary_object_status;

    const std::string groupLabel = EvidenceToolGroupLabel(tool);
    ScriptEvidenceGroup* groupPtr = nullptr;
    for (auto& group : context.script_evidence_groups)
    {
        if (group.label == groupLabel)
        {
            groupPtr = &group;
            break;
        }
    }
    if (groupPtr == nullptr)
    {
        ScriptEvidenceGroup group;
        group.label = groupLabel;
        context.script_evidence_groups.push_back(group);
        groupPtr = &context.script_evidence_groups.back();
    }
    groupPtr->thumbs.push_back(thumb);

    // Saving creates a new immutable Evidence row, but does not switch the
    // active Workbench away from its baseline.  A Candidate is restored only
    // when the user or Headless explicitly selects that Candidate row.
    context.script_evidence_row_refs_dirty = true;
}
}

void AppendEvidenceCandidateStateProbe(
    const ManualTestContext& context,
    const std::string& candidateDir,
    const std::string& candidateId,
    const std::string& phase,
    const std::string& status,
    const std::string& reason)
{
    AppendEvidenceCandidateStateProbeImpl(
        context, candidateDir, candidateId, phase, status, reason);
}

bool SaveEvidenceCandidatePackage(
    ManualTestContext& context,
    const CxEvidenceCandidateSaveOptions& options,
    CxEvidenceCandidateSaveResult& result)
{
    result = CxEvidenceCandidateSaveResult{};

    const std::string caseId = options.case_id_override.empty()
        ? EffectiveCaseId(context)
        : options.case_id_override;
    const std::string safeCaseId = SafePathComponentForCandidate(caseId);
    if (safeCaseId.empty())
    {
        result.reason = "case_id is empty";
        return false;
    }

    const std::string candidateId = SafePathComponentForCandidate(
        options.candidate_id.empty() ? GenerateCandidateId() : options.candidate_id);
    if (candidateId.empty())
    {
        result.reason = "candidate_id is empty";
        return false;
    }

    const std::string scriptText = CurrentScriptText(context);
    if (scriptText.empty())
    {
        result.reason = "script text is empty";
        return false;
    }

    const std::string imagePath = EffectiveImagePath(context);
    if (imagePath.empty())
    {
        result.reason = "image_path is empty";
        return false;
    }

    const std::string scriptPath = EffectiveScriptPath(context);
    const std::string sourceEvidenceScriptPath =
        context.current_evidence_selection.source_evidence_script_path.empty()
            ? scriptPath
            : context.current_evidence_selection.source_evidence_script_path;
    const std::string scriptId = EffectiveScriptId(context);
    const std::string imageId = EffectiveImageId(context);
    const std::string targetId = EffectiveTargetId(context);
    const std::string tool = EffectiveTool(context);
    const std::string parameterSummary = EffectiveParameterSummary(context);

    std::error_code ec;
    bool rootCanonicalized = false;
    const std::filesystem::path root =
        CanonicalEvidenceCandidateRoot(options.root_dir, &rootCanonicalized);
    if (rootCanonicalized)
    {
        CXLOG_INFO(
            "EvidenceCandidate",
            "candidate_root_canonicalized",
            "available",
            "configured_root=" + options.root_dir +
            " canonical_root=" + root.string());
    }

    const std::filesystem::path candidateDir =
        (root / safeCaseId / candidateId).lexically_normal();
    std::filesystem::create_directories(candidateDir, ec);
    if (ec)
    {
        result.reason = "cannot create evidence candidate dir: " + ec.message();
        return false;
    }

    std::string bindingProbeReason = "candidate values captured after ApplyManualGaugeToGlobals";
    const bool filterProfileBound =
        scriptText.find("global_filterprofile") != std::string::npos &&
        (scriptText.find("setfilterprofile(filterprofile)") != std::string::npos ||
         scriptText.find("setfilterprofile(global_filterprofile)") != std::string::npos);
    if (!filterProfileBound &&
        scriptText.find("setfilterprofile(") != std::string::npos)
    {
        bindingProbeReason +=
            "; SCRIPT_BINDING_CONFLICT: setfilterprofile uses a literal or non-global value";
    }
    AppendEvidenceCandidateStateProbe(
        context,
        candidateDir.string(),
        candidateId,
        "candidate_save_input",
        filterProfileBound ? "captured" : "captured_with_binding_warning",
        bindingProbeReason);

    const std::filesystem::path scriptSnapshotPath =
        candidateDir / "script_snapshot.cxsc";
    const std::filesystem::path parameterSnapshotPath =
        candidateDir / "parameter_snapshot.cxsc";
    const std::filesystem::path gaugeAnnotationPath =
        candidateDir / "gauge_annotation.json";
    const std::filesystem::path runtimeGlobalsPath =
        candidateDir / "runtime_globals.json";
    const std::filesystem::path analysisStatePath =
        candidateDir / "analysis_state.json";
    const std::filesystem::path resultSummaryPath =
        candidateDir / "result_summary.json";
    const std::filesystem::path evidenceBindingPath =
        candidateDir / "evidence_binding.json";
    const std::filesystem::path lineTracePath =
        candidateDir / "line_trace.json";
    const std::filesystem::path logPath =
        candidateDir / "log.txt";

    const bool preserveInputs = options.preserve_input_snapshots;

    if (!preserveInputs && !WriteTextFile(scriptSnapshotPath, scriptText))
    {
        result.reason = "failed to write script_snapshot.cxsc";
        return false;
    }

    std::ostringstream params;
    params << "// Evidence candidate parameter snapshot\n"
           << "// candidate_id: " << candidateId << "\n"
           << "// source: runtime globals from current analysis session\n";
    std::vector<std::pair<std::string, int>> vars(
        context.runtime_int_vars.begin(),
        context.runtime_int_vars.end());
    std::sort(vars.begin(), vars.end());
    for (const auto& kv : vars)
    {
        if (kv.first.rfind("global_", 0) == 0)
            params << "int " << kv.first << " = " << kv.second << ";\n";
    }
    if (!preserveInputs && !WriteTextFile(parameterSnapshotPath, params.str()))
    {
        result.reason = "failed to write parameter_snapshot.cxsc";
        return false;
    }

    std::string writeReason;
    if (!preserveInputs &&
        !WriteGaugeSnapshotJson(context.current_gauge, gaugeAnnotationPath, writeReason))
    {
        result.reason = writeReason;
        return false;
    }

    if (!preserveInputs &&
        !WriteRuntimeGlobalsJson(context, runtimeGlobalsPath, writeReason))
    {
        result.reason = writeReason;
        return false;
    }

    if (!WriteLineTraceJson(context, lineTracePath, writeReason))
    {
        result.reason = writeReason;
        return false;
    }

    std::ostringstream analysisJson;
    analysisJson << "{\n"
                 << "  \"schema_version\": 1,\n"
                 << "  \"candidate_id\": \"" << JsonEscape(candidateId) << "\",\n"
                 << "  \"mode\": \"" << JsonEscape(options.mode) << "\",\n"
                 << "  \"request_run\": " << (options.request_run ? "true" : "false") << ",\n"
                 << "  \"editor_dirty\": " << (context.editor_dirty ? "true" : "false") << ",\n"
                 << "  \"editor_source\": \"" << JsonEscape(context.editor_source) << "\",\n"
                 << "  \"loaded_script_path\": \"" << JsonEscape(context.loaded_script_path) << "\",\n"
                 << "  \"debug_action\": \"" << JsonEscape(context.debug_action) << "\",\n"
                 << "  \"debug_status\": \"" << JsonEscape(context.debug_status) << "\",\n"
                 << "  \"debug_reason\": \"" << JsonEscape(context.debug_reason) << "\",\n"
                 << "  \"run_state\": \"" << JsonEscape(context.run_state) << "\",\n"
                 << "  \"primary_object_type\": \"" << JsonEscape(context.current_gauge.primary_object_type) << "\",\n"
                 << "  \"primary_object_name\": \"" << JsonEscape(context.current_gauge.primary_object_name) << "\",\n"
                 << "  \"primary_object_status\": \"" << JsonEscape(context.current_gauge.primary_object_status) << "\"\n"
                 << "}\n";
    if (!WriteTextFile(analysisStatePath, analysisJson.str()))
    {
        result.reason = "failed to write analysis_state.json";
        return false;
    }

    const bool linkedResultAvailable =
        !options.linked_result_summary_path.empty();
    const bool linkedOverlayAvailable =
        !options.linked_result_overlay_path.empty() &&
        !options.linked_evidence_overlay_path.empty() &&
        !options.linked_tool_display_path.empty();

    std::ostringstream summaryJson;
    summaryJson << "{\n"
                << "  \"schema_version\": 1,\n"
                << "  \"candidate_id\": \"" << JsonEscape(candidateId) << "\",\n"
                << "  \"runtime_result\": \""
                << (linkedResultAvailable
                        ? "linked_runtime_result"
                        : (options.request_run ? "run_requested" : "pending"))
                << "\",\n"
                << "  \"status\": \"" << JsonEscape(context.current_result_ref.status) << "\",\n"
                << "  \"reason\": \"" << JsonEscape(context.current_result_ref.reason) << "\",\n"
                << "  \"debug_status\": \"" << JsonEscape(context.debug_status) << "\",\n"
                << "  \"debug_reason\": \"" << JsonEscape(context.debug_reason) << "\",\n"
                << "  \"linked_result_summary_path\": \"" << JsonEscape(options.linked_result_summary_path) << "\",\n"
                << "  \"linked_result_overlay_path\": \"" << JsonEscape(options.linked_result_overlay_path) << "\",\n"
                << "  \"linked_evidence_overlay_path\": \"" << JsonEscape(options.linked_evidence_overlay_path) << "\",\n"
                << "  \"linked_tool_display_path\": \"" << JsonEscape(options.linked_tool_display_path) << "\",\n"
                << "  \"asset_missing\": " << (linkedResultAvailable && linkedOverlayAvailable ? "false" : "true") << ",\n"
                << "  \"missing_assets\": "
                << (linkedResultAvailable && linkedOverlayAvailable
                        ? "[]"
                        : "[\"result_overlay.png\", \"evidence_overlay.png\", \"tool_display.png\", \"object_state.json\", \"variable_snapshot.json\"]")
                << "\n"
                << "}\n";
    if (!WriteTextFile(resultSummaryPath, summaryJson.str()))
    {
        result.reason = "failed to write result_summary.json";
        return false;
    }

    std::ostringstream bindingJson;
    bindingJson << "{\n"
                << "  \"schema_version\": 1,\n"
                << "  \"evidence_status\": \"to_review\",\n"
                << "  \"review_status\": \"pending_human_review\",\n"
                << "  \"candidate_id\": \"" << JsonEscape(candidateId) << "\",\n"
                << "  \"case_id\": \"" << JsonEscape(caseId) << "\",\n"
                << "  \"script_id\": \"" << JsonEscape(scriptId) << "\",\n"
                << "  \"script_path\": \"" << JsonEscape(scriptPath) << "\",\n"
                << "  \"script_snapshot_path\": \"" << JsonEscape(scriptSnapshotPath.string()) << "\",\n"
                << "  \"source_evidence_script_path\": \""
                << JsonEscape(ResolveWorkspaceFile(sourceEvidenceScriptPath).string())
                << "\",\n"
                << "  \"image_id\": \"" << JsonEscape(imageId) << "\",\n"
                << "  \"image_path\": \"" << JsonEscape(imagePath) << "\",\n"
                << "  \"target_id\": \"" << JsonEscape(targetId) << "\",\n"
                << "  \"tool\": \"" << JsonEscape(tool) << "\",\n"
                << "  \"parameter_summary\": \"" << JsonEscape(parameterSummary) << "\",\n"
                << "  \"parameter_snapshot_path\": \"" << JsonEscape(parameterSnapshotPath.string()) << "\",\n"
                << "  \"gauge_annotation_path\": \"" << JsonEscape(gaugeAnnotationPath.string()) << "\",\n"
                << "  \"analysis_state_path\": \"" << JsonEscape(analysisStatePath.string()) << "\",\n"
                << "  \"result_summary_path\": \"" << JsonEscape(resultSummaryPath.string()) << "\",\n"
                << "  \"linked_result_summary_path\": \"" << JsonEscape(options.linked_result_summary_path) << "\",\n"
                << "  \"linked_result_overlay_path\": \"" << JsonEscape(options.linked_result_overlay_path) << "\",\n"
                << "  \"linked_evidence_overlay_path\": \"" << JsonEscape(options.linked_evidence_overlay_path) << "\",\n"
                << "  \"linked_tool_display_path\": \"" << JsonEscape(options.linked_tool_display_path) << "\",\n"
                << "  \"runtime_globals_path\": \"" << JsonEscape(runtimeGlobalsPath.string()) << "\",\n"
                << "  \"line_trace_path\": \"" << JsonEscape(lineTracePath.string()) << "\",\n"
                << "  \"source\": \"manual_or_headless_analysis_save\",\n"
                << "  \"promotion_allowed\": false\n"
                << "}\n";
    if (!preserveInputs && !WriteTextFile(evidenceBindingPath, bindingJson.str()))
    {
        result.reason = "failed to write evidence_binding.json";
        return false;
    }

    std::ostringstream log;
    log << "Evidence candidate saved\n"
        << "candidate_id=" << candidateId << "\n"
        << "case_id=" << caseId << "\n"
        << "script=" << scriptPath << "\n"
        << "image=" << imagePath << "\n"
        << "tool=" << tool << "\n"
        << "mode=" << options.mode << "\n";
    WriteTextFile(logPath, log.str());

    result.ok = true;
    result.candidate_id = candidateId;
    result.candidate_dir = candidateDir.string();
    result.evidence_binding_path = evidenceBindingPath.string();
    result.script_snapshot_path = scriptSnapshotPath.string();
    result.parameter_snapshot_path = parameterSnapshotPath.string();
    result.gauge_annotation_path = gaugeAnnotationPath.string();
    result.analysis_state_path = analysisStatePath.string();
    result.result_summary_path = resultSummaryPath.string();

    AppendEvidenceCandidateStateProbe(
        context,
        result.candidate_dir,
        result.candidate_id,
        "candidate_package_written",
        "written",
        "script_snapshot, parameter_snapshot, runtime_globals, gauge_annotation and evidence_binding written");

    if (options.add_to_evidence_chain)
    {
        AppendCandidateToEvidenceChain(
            context,
            result,
            caseId,
            scriptId,
            scriptSnapshotPath.string(),
            ResolveWorkspaceFile(sourceEvidenceScriptPath).string(),
            imageId,
            imagePath,
            targetId,
            tool,
            parameterSummary);
        AppendEvidenceCandidateStateProbe(
            context,
            result.candidate_dir,
            result.candidate_id,
            "working_revision_bound",
            "available",
            "original Evidence row now points to this candidate package; baseline source script remains unchanged");
        AppendEvidenceCandidateStateProbe(
            context,
            result.candidate_dir,
            result.candidate_id,
            "candidate_row_added",
            "available",
            "saved candidate was added to Evidence Chain; current Workbench values remain unchanged");
    }

    context.editor_dirty = false;
    context.debug_action = options.request_run
        ? "Save And Run Evidence Candidate"
        : "Save Evidence Candidate";
    context.debug_status = options.request_run
        ? "EVIDENCE_CANDIDATE_RUN_REQUESTED"
        : "EVIDENCE_CANDIDATE_SAVED";
    context.debug_reason = result.candidate_dir;
    // Only an initial save owns the pending-candidate slot.  A post-run update
    // must not replace it or make later ordinary Run actions update a stale
    // candidate package.
    if (options.add_to_evidence_chain || options.request_run)
    {
        context.last_evidence_candidate_id = result.candidate_id;
        context.last_evidence_candidate_dir = result.candidate_dir;
        context.last_evidence_candidate_reason = result.reason;
    }
    if (options.request_run)
        context.run_state = "running";

    return true;
}

bool SaveCasePackage(
    ManualTestContext& context,
    const std::string& caseName,
    const std::string& outputDir,
    std::string& outPath,
    std::string& outReason)
{
    CxEvidenceCandidateSaveOptions options;
    options.root_dir = outputDir.empty() ? "cxscript_runs/evidence_candidates" : outputDir;
    options.candidate_id = caseName.empty() ? std::string() : caseName;
    options.mode = "legacy_case_package";
    options.request_run = false;
    options.add_to_evidence_chain = false;

    CxEvidenceCandidateSaveResult result;
    if (!SaveEvidenceCandidatePackage(context, options, result))
    {
        outPath.clear();
        outReason = result.reason;
        return false;
    }
    outPath = result.candidate_dir;
    outReason.clear();
    return true;
}
