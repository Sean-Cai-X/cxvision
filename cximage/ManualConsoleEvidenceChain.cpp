#include "pch.h"
#include "ManualConsoleEvidenceChain.h"
#include "ManualConsoleUtils.h"
#include "ManualStateTestConsole.h"
#include "CxScriptCatalogRuntime.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <functional>
#include <sstream>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

static std::string NormalizeEvidenceToolTypeLocal(const std::string& typeOrTool)
{
    std::string lowered = typeOrTool;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

    if (lowered == "findline")
        return "FindLine";
    if (lowered == "findcircle")
        return "FindCircle";
    if (lowered == "findellipse")
        return "FindEllipse";
    if (lowered == "findrect")
        return "FindRect";
    if (lowered == "fastmatch" || lowered == "cfastmatch")
        return "FastMatch";
    if (typeOrTool == "FindSegmentation")
        return "FindSegmentation";
    return typeOrTool;
}

static bool IsEvidenceEditableToolTypeLocal(const std::string& type)
{
    const std::string normalized = NormalizeEvidenceToolTypeLocal(type);
    return normalized == "FindLine" ||
           normalized == "FindCircle" ||
           normalized == "FindEllipse" ||
           normalized == "FindRect" ||
           normalized == "FastMatch" ||
           normalized == "FindSegmentation";
}

static std::string StripCxScriptLineCommentLocal(const std::string& line)
{
    const std::size_t comment = line.find("//");
    if (comment == std::string::npos)
        return line;
    return line.substr(0, comment);
}

static void AnalyzeEditableObjectsFromCxScriptLocal(
    const std::string& scriptText,
    std::vector<CxEvidenceEditableObjectRef>& outObjects)
{
    outObjects.clear();

    std::istringstream input(scriptText);
    std::string raw;
    int lineNo = 1;
    while (std::getline(input, raw))
    {
        const std::string statement = TrimLine(StripCxScriptLineCommentLocal(raw));
        if (statement.empty() ||
            statement.find('(') != std::string::npos ||
            statement.find('=') != std::string::npos)
        {
            ++lineNo;
            continue;
        }

        std::istringstream tokens(statement);
        std::string type;
        std::string name;
        tokens >> type >> name;
        if (type.empty() || name.empty())
        {
            ++lineNo;
            continue;
        }

        const std::size_t suffix = name.find_first_of(";");
        if (suffix != std::string::npos)
            name.erase(suffix);

        type = NormalizeEvidenceToolTypeLocal(type);
        if (IsEvidenceEditableToolTypeLocal(type) && !name.empty())
        {
            CxEvidenceEditableObjectRef ref;
            ref.type = type;
            ref.name = name;
            ref.declared_line = lineNo;
            outObjects.push_back(ref);
        }

        ++lineNo;
    }
}

static std::string ReadKeyValueFromEvidenceParamSummaryLocal(
    const std::string& summary,
    const std::string& key)
{
    std::istringstream tokens(summary);
    std::string token;
    const std::string prefix = key + "=";
    while (tokens >> token)
    {
        if (token.rfind(prefix, 0) == 0)
            return token.substr(prefix.size());
    }
    return {};
}

static bool ReadJsonIntFieldLocal(
    const std::string& text,
    const std::string& key,
    int& outValue)
{
    const std::string pattern = "\"" + key + "\"";
    const std::size_t keyPos = text.find(pattern);
    if (keyPos == std::string::npos)
        return false;
    const std::size_t colon = text.find(':', keyPos + pattern.size());
    if (colon == std::string::npos)
        return false;

    std::size_t begin = colon + 1;
    while (begin < text.size() &&
           std::isspace(static_cast<unsigned char>(text[begin])))
    {
        ++begin;
    }

    std::size_t end = begin;
    while (end < text.size() &&
           (std::isdigit(static_cast<unsigned char>(text[end])) ||
            text[end] == '-' ||
            text[end] == '+'))
    {
        ++end;
    }

    if (end == begin)
        return false;

    try
    {
        outValue = std::stoi(text.substr(begin, end - begin));
        return true;
    }
    catch (...)
    {
        return false;
    }
}

static std::string ReadJsonStringFieldLocal(
    const std::string& text,
    const std::string& key)
{
    const std::string pattern = "\"" + key + "\"";
    const std::size_t keyPos = text.find(pattern);
    if (keyPos == std::string::npos)
        return {};
    const std::size_t colon = text.find(':', keyPos + pattern.size());
    if (colon == std::string::npos)
        return {};

    std::size_t begin = colon + 1;
    while (begin < text.size() &&
           std::isspace(static_cast<unsigned char>(text[begin])))
    {
        ++begin;
    }
    if (begin >= text.size() || text[begin] != '"')
        return {};
    ++begin;

    std::string value;
    bool escaped = false;
    for (std::size_t i = begin; i < text.size(); ++i)
    {
        const char ch = text[i];
        if (escaped)
        {
            value.push_back(ch);
            escaped = false;
            continue;
        }
        if (ch == '\\')
        {
            escaped = true;
            continue;
        }
        if (ch == '"')
            return value;
        value.push_back(ch);
    }

    return {};
}

static std::string ResolveEvidencePacketPathFromSummaryLocal(
    const std::string& runtimeSummary)
{
    if (runtimeSummary.empty() || runtimeSummary == "-")
        return {};

    try
    {
        std::filesystem::path path(runtimeSummary);
        path = path.parent_path() / "evidence_packet.json";
        return path.string();
    }
    catch (...)
    {
        return {};
    }
}

static std::string ResolveOriginalImagePathFromEvidencePacketLocal(
    const std::string& runtimeSummary)
{
    const std::string evidencePacket =
        ResolveEvidencePacketPathFromSummaryLocal(runtimeSummary);
    if (evidencePacket.empty())
        return {};

    std::string text;
    if (!ReadTextFile(evidencePacket, text))
        return {};

    return ReadJsonStringFieldLocal(text, "path");
}

static void ResolvePrimaryEditableObjectLocal(
    const std::string& tool,
    const std::string& targetId,
    const std::string& parameterSummary,
    const std::vector<CxEvidenceEditableObjectRef>& objects,
    std::string& outType,
    std::string& outName,
    std::string& outStatus)
{
    outType.clear();
    outName.clear();
    outStatus = "none";

    if (objects.empty())
    {
        outStatus = "no_editable_object_declared";
        return;
    }

    std::string explicitName =
        ReadKeyValueFromEvidenceParamSummaryLocal(parameterSummary, "primary_object_ref");
    if (explicitName.empty())
        explicitName =
            ReadKeyValueFromEvidenceParamSummaryLocal(parameterSummary, "primary_object");
    if (explicitName.empty())
        explicitName =
            ReadKeyValueFromEvidenceParamSummaryLocal(parameterSummary, "object_ref");

    auto bindObject = [&](const CxEvidenceEditableObjectRef& ref,
                          const char* status)
    {
        outType = ref.type;
        outName = ref.name;
        outStatus = status;
    };

    if (!explicitName.empty())
    {
        for (const auto& ref : objects)
        {
            if (ref.name == explicitName)
            {
                bindObject(ref, "explicit_object_ref");
                return;
            }
        }
        outStatus = "explicit_object_ref_not_found";
        return;
    }

    if (!targetId.empty() && targetId != "-")
    {
        for (const auto& ref : objects)
        {
            if (targetId.find(ref.name) != std::string::npos ||
                ref.name.find(targetId) != std::string::npos)
            {
                bindObject(ref, "target_id_matched_object");
                return;
            }
        }
    }

    const std::string normalizedTool = NormalizeEvidenceToolTypeLocal(tool);
    std::vector<const CxEvidenceEditableObjectRef*> toolMatches;
    for (const auto& ref : objects)
    {
        if (ref.type == normalizedTool)
            toolMatches.push_back(&ref);
    }

    if (toolMatches.size() == 1)
    {
        bindObject(*toolMatches.front(), "single_matching_tool_object");
        return;
    }

    if (objects.size() == 1)
    {
        bindObject(objects.front(), "single_editable_object");
        return;
    }

    outStatus = toolMatches.size() > 1
        ? "needs_object_selection"
        : "needs_object_selection_no_tool_match";
}

static bool EvidenceSnapshotHasLockedParamSummaryLocal(
    const CxEvidenceSelectionSnapshot& snapshot,
    std::string& reason)
{
    if (!snapshot.valid)
    {
        reason = "invalid evidence snapshot";
        return false;
    }
    if (snapshot.parameter_summary.empty() || snapshot.parameter_summary == "-")
    {
        reason = "evidence parameter summary is empty";
        return false;
    }
    if (snapshot.parameter_summary.find('=') == std::string::npos)
    {
        reason = "evidence parameter summary is not key=value locked data: " +
                 snapshot.parameter_summary;
        return false;
    }
    reason.clear();
    return true;
}

static void SyncEvidenceLockedGlobalsToManualGaugeLocal(
    ManualTestContext& context,
    const std::string& scriptPath,
    const std::string& source,
    const std::string& primaryObjectType,
    const std::string& primaryObjectName,
    const std::string& primaryObjectStatus)
{
    auto getInt = [&](const std::string& key, int fallback) -> int
    {
        const auto it = context.runtime_int_vars.find(key);
        return it == context.runtime_int_vars.end() ? fallback : it->second;
    };

    const bool isCircleScript =
        scriptPath.find("find_circle") != std::string::npos ||
        scriptPath.find("findcircle") != std::string::npos ||
        scriptPath.find("FindCircle") != std::string::npos;
    const bool isLineScript =
        scriptPath.find("find_line") != std::string::npos ||
        scriptPath.find("findline") != std::string::npos ||
        scriptPath.find("FindLine") != std::string::npos;
    const bool isEllipseScript =
        scriptPath.find("find_ellipse") != std::string::npos ||
        scriptPath.find("findellipse") != std::string::npos ||
        scriptPath.find("FindEllipse") != std::string::npos;

    ManualGaugeState gauge;
    gauge.case_id = context.active_case_id;
    gauge.image_id = context.active_image_id;
    gauge.target_id = context.active_target_id;
    gauge.source = source;
    gauge.primary_object_type = primaryObjectType;
    gauge.primary_object_name = primaryObjectName;
    gauge.primary_object_status = primaryObjectStatus;
    gauge.review_status = "editing";
    gauge.threshold = getInt("global_threshold", 20);
    gauge.method = getInt("global_method", 0);
    gauge.linegap = getInt("global_linegap", 3);
    gauge.wgap = getInt("global_wgap", 32);
    gauge.hgap = getInt("global_hgap", 8);
    gauge.gap = getInt("global_gap", 5);
    gauge.tool_half_width = getInt("global_tool_half_width", 32);
    gauge.filterprofile = getInt("global_filterprofile", 1);

    const std::string primaryType =
        NormalizeEvidenceToolTypeLocal(primaryObjectType);

    if (primaryType == "FindCircle" ||
        (primaryType.empty() && isCircleScript))
    {
        gauge.tool = "FindCircle";
        gauge.has_circle_gauge = true;
        gauge.circle_cx = getInt("global_circle_cx", 0);
        gauge.circle_cy = getInt("global_circle_cy", 0);
        gauge.circle_px = getInt("global_circle_px", gauge.circle_cx);
        gauge.circle_py = getInt("global_circle_py", gauge.circle_cy);
        gauge.radius = static_cast<int>(std::lround(std::hypot(
            static_cast<double>(gauge.circle_px - gauge.circle_cx),
            static_cast<double>(gauge.circle_py - gauge.circle_cy))));
    }
    else if (primaryType == "FindEllipse" ||
             (primaryType.empty() && isEllipseScript))
    {
        gauge.tool = "FindEllipse";
        gauge.has_ellipse_gauge = true;
        gauge.ellipse_x0 = getInt("global_ellipse_x0", 0);
        gauge.ellipse_y0 = getInt("global_ellipse_y0", 0);
        gauge.ellipse_x1 = getInt("global_ellipse_x1", 0);
        gauge.ellipse_y1 = getInt("global_ellipse_y1", 0);
    }
    else if (primaryType == "FindLine" ||
             (primaryType.empty() && isLineScript))
    {
        gauge.tool = "FindLine";
        gauge.has_line_gauge = true;
        gauge.line_x0 = getInt("global_roi_x0", 0);
        gauge.line_y0 = getInt("global_roi_y0", 0);
        gauge.line_x1 = getInt("global_roi_x1", 0);
        gauge.line_y1 = getInt("global_roi_y1", 0);
    }

    if (gauge.has_circle_gauge || gauge.has_line_gauge ||
        gauge.has_ellipse_gauge)
    {
        context.current_gauge = gauge;
    }
}

static std::vector<std::string> BuildEvidenceFallbackImageCandidates(
    const ManualTestContext& context)
{
    std::vector<std::string> candidates;

    auto addCandidate = [&](const std::string& path)
    {
        if (path.empty())
            return;
        if (!std::filesystem::exists(path))
            return;
        if (std::find(candidates.begin(), candidates.end(), path) != candidates.end())
            return;
        candidates.push_back(path);
    };

    addCandidate(context.image_file_path);

    for (const auto& variable : context.global_variable_views)
    {
        addCandidate(variable.image_path);
    }

    for (const auto& item : context.image_manifest_items)
    {
        addCandidate(item.image_path);
    }

    addCandidate("D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/01.jpg");

    return candidates;
}

static bool IsDeprecatedCxScriptPath(const std::string& path)
{
    return path.find("/deprecated/") != std::string::npos ||
           path.find("\\deprecated\\") != std::string::npos;
}

static bool IsAllowedEvidenceFallbackScript(const std::string& path)
{
    if (path.empty())
        return false;

    if (IsDeprecatedCxScriptPath(path))
        return false;

    // Evidence Chain is allowed to create placeholders for current direct,
    // frozen, headless and diagnostic assets.  Deprecated scripts remain
    // runnable from the legacy catalog only; they must not become semantic
    // evidence bindings by accident.
    return path.find("/headless/") != std::string::npos ||
           path.find("\\headless\\") != std::string::npos ||
           path.find("/frozen/") != std::string::npos ||
           path.find("\\frozen\\") != std::string::npos ||
           path.find("/diagnostic/") != std::string::npos ||
           path.find("\\diagnostic\\") != std::string::npos ||
           path.find("_direct") != std::string::npos ||
           path.find("_smoke") != std::string::npos;
}

static void AssignFallbackImageToThumb(
    ScriptEvidenceThumb& thumb,
    const std::vector<std::string>& candidates,
    std::size_t index)
{
    if (!thumb.image_path.empty())
        return;

    if (candidates.empty())
        return;

    const std::string& path = candidates[index % candidates.size()];
    thumb.image_path = path;

    if (thumb.image_id.empty())
        thumb.image_id = "fallback_image_" + std::to_string(index % candidates.size());

    if (thumb.reason.empty())
        thumb.reason = "fallback image bound for evidence placeholder";
}

static std::string BuildDefaultEvidenceParamSummaryForScript(
    const std::string& scriptPath)
{
    ManualTestContext temp;
    SeedDefaultManualGlobals(temp, scriptPath);

    auto getInt = [&](const std::string& key, int fallback) -> int
    {
        auto it = temp.runtime_int_vars.find(key);
        return it == temp.runtime_int_vars.end() ? fallback : it->second;
    };

    std::ostringstream oss;
    oss << "method=" << getInt("global_method", 0)
        << " threshold=" << getInt("global_threshold", 20)
        << " wgap=" << getInt("global_wgap", 0)
        << " hgap=" << getInt("global_hgap", 0)
        << " gap=" << getInt("global_gap", 0)
        << " linegap=" << getInt("global_linegap", 0)
        << " filterprofile=" << getInt("global_filterprofile", 1)
        << " tool_half_width=" << getInt("global_tool_half_width", 0)
        << " roi_x0=" << getInt("global_roi_x0", 0)
        << " roi_y0=" << getInt("global_roi_y0", 0)
        << " roi_x1=" << getInt("global_roi_x1", 0)
        << " roi_y1=" << getInt("global_roi_y1", 0)
        << " roi_x=" << getInt("global_roi_x", 0)
        << " roi_y=" << getInt("global_roi_y", 0)
        << " roi_width=" << getInt("global_roi_width", 0)
        << " roi_height=" << getInt("global_roi_height", 0)
        << " circle_cx=" << getInt("global_circle_cx", 0)
        << " circle_cy=" << getInt("global_circle_cy", 0)
        << " circle_px=" << getInt("global_circle_px", 0)
        << " circle_py=" << getInt("global_circle_py", 0)
        << " ellipse_x0=" << getInt("global_ellipse_x0", 0)
        << " ellipse_y0=" << getInt("global_ellipse_y0", 0)
        << " ellipse_x1=" << getInt("global_ellipse_x1", 0)
        << " ellipse_y1=" << getInt("global_ellipse_y1", 0)
        << " learn_roi_x=" << getInt("global_learn_roi_x", 120)
        << " learn_roi_y=" << getInt("global_learn_roi_y", 120)
        << " learn_roi_w=" << getInt("global_learn_roi_w", 120)
        << " learn_roi_h=" << getInt("global_learn_roi_h", 90)
        << " search_roi_x=" << getInt("global_search_roi_x", 60)
        << " search_roi_y=" << getInt("global_search_roi_y", 60)
        << " search_roi_w=" << getInt("global_search_roi_w", 640)
        << " search_roi_h=" << getInt("global_search_roi_h", 480)
        << " compare_gap=" << getInt("global_compare_gap", 0)
        << " objfilter=" << getInt("global_objfilter", 0)
        << " find_num=" << getInt("global_find_num", 1)
        << " max_elapsed_ms=" << getInt("global_max_elapsed_ms", 2000)
        << " max_scan_lines=" << getInt("global_max_scan_lines", 2000)
        << " max_samples=" << getInt("global_max_samples", 200000);

    return oss.str();
}

static void PopulateEditableObjectBindingForThumbLocal(
    ScriptEvidenceThumb& thumb)
{
    thumb.primary_object_type.clear();
    thumb.primary_object_name.clear();
    thumb.primary_object_status.clear();

    if (thumb.script_path.empty())
    {
        thumb.primary_object_status = "script_path_empty";
        return;
    }

    std::string scriptText;
    if (!ReadTextFile(thumb.script_path, scriptText))
    {
        thumb.primary_object_status = "script_read_failed";
        return;
    }

    std::vector<CxEvidenceEditableObjectRef> objects;
    AnalyzeEditableObjectsFromCxScriptLocal(scriptText, objects);
    ResolvePrimaryEditableObjectLocal(
        thumb.tool,
        thumb.target_id,
        thumb.parameter_summary,
        objects,
        thumb.primary_object_type,
        thumb.primary_object_name,
        thumb.primary_object_status);
}

static std::string StripMarkdownCellDecorLocal(std::string value)
{
    value = TrimLine(value);
    if (value.size() >= 2 && value.front() == '`' && value.back() == '`')
        value = value.substr(1, value.size() - 2);
    return TrimLine(value);
}

static std::vector<std::string> SplitMarkdownTableRowLocal(
    const std::string& line)
{
    std::vector<std::string> cells;
    std::string current;
    bool inBacktick = false;
    for (char ch : line)
    {
        if (ch == '`')
            inBacktick = !inBacktick;
        if (ch == '|' && !inBacktick)
        {
            cells.push_back(StripMarkdownCellDecorLocal(current));
            current.clear();
            continue;
        }
        current.push_back(ch);
    }
    cells.push_back(StripMarkdownCellDecorLocal(current));

    if (!cells.empty() && cells.front().empty())
        cells.erase(cells.begin());
    if (!cells.empty() && cells.back().empty())
        cells.pop_back();
    return cells;
}

static std::string BuildManualReviewParamSummaryLocal(
    const std::string& tool,
    const std::string& failureClass,
    const std::string& runtimeSummary,
    const std::string& extraEvidence)
{
    auto appendInt = [](std::ostringstream& oss,
                        const std::string& text,
                        const std::string& summaryKey,
                        const std::string& paramKey,
                        std::string& missing) -> void
    {
        int value = 0;
        if (ReadJsonIntFieldLocal(text, summaryKey, value))
        {
            oss << " " << paramKey << "=" << value;
            return;
        }

        if (!missing.empty())
            missing += ",";
        missing += summaryKey;
    };

    std::string summaryText;
    ReadTextFile(runtimeSummary, summaryText);

    const std::string normalizedTool = NormalizeEvidenceToolTypeLocal(tool);
    std::string missing;

    std::ostringstream oss;
    oss << "evidence_locked=1"
        << " tool=" << normalizedTool;

    if (normalizedTool == "FindLine")
    {
        appendInt(oss, summaryText, "roi_x0", "roi_x0", missing);
        appendInt(oss, summaryText, "roi_y0", "roi_y0", missing);
        appendInt(oss, summaryText, "roi_x1", "roi_x1", missing);
        appendInt(oss, summaryText, "roi_y1", "roi_y1", missing);
        appendInt(oss, summaryText, "effective_tool_half_width", "tool_half_width", missing);
    }
    else if (normalizedTool == "FindCircle")
    {
        appendInt(oss, summaryText, "circle_cx", "circle_cx", missing);
        appendInt(oss, summaryText, "circle_cy", "circle_cy", missing);
        appendInt(oss, summaryText, "circle_px", "circle_px", missing);
        appendInt(oss, summaryText, "circle_py", "circle_py", missing);
    }
    else if (normalizedTool == "FindEllipse")
    {
        appendInt(oss, summaryText, "ellipse_x0", "ellipse_x0", missing);
        appendInt(oss, summaryText, "ellipse_y0", "ellipse_y0", missing);
        appendInt(oss, summaryText, "ellipse_x1", "ellipse_x1", missing);
        appendInt(oss, summaryText, "ellipse_y1", "ellipse_y1", missing);
    }
    else if (normalizedTool == "FindRect")
    {
        appendInt(oss, summaryText, "roi_x", "roi_x", missing);
        appendInt(oss, summaryText, "roi_y", "roi_y", missing);
        appendInt(oss, summaryText, "roi_width", "roi_width", missing);
        appendInt(oss, summaryText, "roi_height", "roi_height", missing);
    }

    appendInt(oss, summaryText, "effective_gap", "gap", missing);
    appendInt(oss, summaryText, "effective_linegap", "linegap", missing);
    appendInt(oss, summaryText, "effective_threshold", "threshold", missing);
    appendInt(oss, summaryText, "effective_filterprofile", "filterprofile", missing);
    appendInt(oss, summaryText, "effective_method", "method", missing);
    appendInt(oss, summaryText, "effective_wgap", "wgap", missing);
    appendInt(oss, summaryText, "effective_hgap", "hgap", missing);

    oss << " max_elapsed_ms=2000"
        << " max_scan_lines=2000"
        << " max_samples=200000"
        << " review_status=pending_algorithm_review"
        << " failure_class=" << failureClass
        << " result_summary_path=" << runtimeSummary;
    if (!missing.empty())
        oss << " locked_param_missing=" << missing;
    if (!extraEvidence.empty() && extraEvidence != "-")
        oss << " extra_evidence=" << extraEvidence;
    return oss.str();
}

static bool IsManualReviewHandoffCaseRowLocal(
    const std::vector<std::string>& cells)
{
    if (cells.size() < 11)
        return false;
    if (cells[0] == "Case" || cells[0].find("---") != std::string::npos)
        return false;
    return !cells[0].empty() &&
           !cells[1].empty() &&
           !cells[5].empty() &&
           !cells[10].empty();
}

static void AppendManualAlgorithmReviewHandoffLocal(
    ManualTestContext& context,
    const std::string& handoffPath,
    const std::function<std::string(const std::string&)>& resolveImagePath,
    const std::function<ScriptEvidenceGroup&(const std::string&)>& findGroup)
{
    std::string text;
    if (!ReadTextFile(handoffPath, text))
        return;

    std::istringstream input(text);
    std::string line;
    while (std::getline(input, line))
    {
        if (line.find('|') == std::string::npos)
            continue;

        const std::vector<std::string> cells =
            SplitMarkdownTableRowLocal(line);
        if (!IsManualReviewHandoffCaseRowLocal(cells))
            continue;

        const std::string caseId = cells[0];
        const std::string tool = NormalizeEvidenceToolTypeLocal(cells[1]);
        const std::string imageId = cells[2];
        const std::string targetId = cells[3];
        const std::string failureClass = cells[4];
        const std::string runtimeSummary = cells[5];
        const std::string toolDisplay = cells[6];
        const std::string resultOverlay = cells[7];
        const std::string evidenceOverlay = cells[8];
        const std::string roiPreview = cells[9];
        const std::string scriptSnapshot = cells[10];
        const std::string extraEvidence =
            cells.size() > 11 ? cells[11] : std::string();

        ScriptEvidenceThumb thumb;
        thumb.case_id = caseId;
        thumb.script_id = caseId;
        thumb.script_path = scriptSnapshot;
        thumb.image_id = imageId;
        thumb.image_path =
            ResolveOriginalImagePathFromEvidencePacketLocal(runtimeSummary);
        if (thumb.image_path.empty())
            thumb.image_path = resolveImagePath(imageId);
        thumb.thumbnail_path = roiPreview.empty() ? toolDisplay : roiPreview;
        thumb.target_id = targetId;
        thumb.tool = tool;
        thumb.parameter_summary =
            BuildManualReviewParamSummaryLocal(
                tool,
                failureClass,
                runtimeSummary,
                extraEvidence);
        thumb.status = "pending_algorithm_review";
        thumb.reason =
            "manual algorithm review from handoff; failure_class=" +
            failureClass +
            "; result_summary=" + runtimeSummary +
            "; tool_display=" + toolDisplay +
            "; result_overlay=" + resultOverlay +
            "; evidence_overlay=" + evidenceOverlay +
            "; roi_preview=" + roiPreview +
            "; source_image=" + thumb.image_path +
            "; handoff=" + handoffPath;

        PopulateEditableObjectBindingForThumbLocal(thumb);

        ScriptEvidenceGroup& group =
            findGroup(tool + " / Review Queue");
        bool exists = false;
        for (const auto& existing : group.thumbs)
        {
            if (existing.case_id == thumb.case_id)
            {
                exists = true;
                break;
            }
        }
        if (!exists)
            group.thumbs.push_back(thumb);
    }

    context.debug_status = "MANUAL_ALGORITHM_REVIEW_HANDOFF_LOADED";
    context.debug_reason = handoffPath;
}

static void EnsureStructuredCxImageCatalogEntriesLoaded(ManualTestContext& context)
{
    if (!context.catalog_entries.empty())
        return;

    const char* catalogPath =
        "cxparser/cxscript/module/cximage/catalog/cximage_catalog.cxsc";

    CxScriptCatalogRuntime catalog;
    std::string reason;
    if (!LoadCxScriptCatalogFile(catalogPath, catalog, reason))
    {
        context.catalog_loaded = false;
        context.catalog_path = catalogPath;
        return;
    }

    context.catalog_entries = catalog.scripts;
    context.catalog_loaded = true;
    context.catalog_path = catalogPath;
}

void ViewController::EnsureCxScriptWorkbenchAssetsLoaded()
{
  if (m_manualTest.script_evidence_groups_dirty == false)
    return;

  EnsureStructuredCxImageCatalogEntriesLoaded(m_manualTest);

  m_manualTest.script_evidence_groups.clear();

  const std::vector<std::string> fallbackImages =
      BuildEvidenceFallbackImageCandidates(m_manualTest);

  std::size_t fallbackImageIndex = 0;

  auto hasThumbForScript = [&](const std::string& scriptId,
                               const std::string& scriptPath) -> bool
  {
    for (const auto& group : m_manualTest.script_evidence_groups)
    {
      for (const auto& thumb : group.thumbs)
      {
        if (!scriptId.empty() && thumb.script_id == scriptId)
          return true;
        if (scriptId.empty() && !scriptPath.empty() &&
            thumb.script_path == scriptPath)
          return true;
      }
    }
    return false;
  };

  auto findOrCreateGroup = [&](const std::string& scriptId,
                               const std::string& scriptPath,
                               const std::string& tool) -> ScriptEvidenceGroup&
  {
    for (auto& group : m_manualTest.script_evidence_groups)
    {
      if (!scriptId.empty() && group.script_id == scriptId)
        return group;
      if (scriptId.empty() && group.label == tool)
        return group;
    }

    ScriptEvidenceGroup group;
    group.script_id = scriptId;
    group.script_path = scriptPath;
    group.label = tool.empty() ? (scriptId.empty() ? "unknown" : scriptId) : tool;
    m_manualTest.script_evidence_groups.push_back(group);
    return m_manualTest.script_evidence_groups.back();
  };

  for (const auto& item : m_manualTest.evidence_items)
  {
    if (item.script_id.empty())
      continue;

    const std::string scriptPath = ResolveCatalogScriptPathById(item.script_id);
    ScriptEvidenceGroup& group =
        findOrCreateGroup(item.script_id, scriptPath, item.tool);

    ScriptEvidenceThumb thumb;
    thumb.case_id = item.case_id;
    thumb.script_id = item.script_id;
    thumb.script_path = scriptPath;
    thumb.image_id = item.image_id;
    thumb.image_path = item.image_path;
    thumb.target_id = item.target_id;
    thumb.tool = item.tool;
    thumb.parameter_summary = item.parameter_profile_id;
    if (thumb.parameter_summary.empty())
    {
      for (const auto& entry : m_manualTest.catalog_entries)
      {
        if (entry.script_id == item.script_id)
        {
          thumb.parameter_summary = entry.parameter_policy_id;
          break;
        }
      }
    }
    if (thumb.parameter_summary.empty() ||
        thumb.parameter_summary.find('=') == std::string::npos)
    {
      thumb.parameter_summary =
          BuildDefaultEvidenceParamSummaryForScript(scriptPath);
    }
    thumb.status = item.probe_status.empty() ? item.contract_status : item.probe_status;
    thumb.reason = item.review_status;

    AssignFallbackImageToThumb(thumb, fallbackImages, fallbackImageIndex++);
    PopulateEditableObjectBindingForThumbLocal(thumb);

    group.thumbs.push_back(thumb);
  }

  for (const auto& entry : m_manualTest.catalog_entries)
  {
    if (!IsAllowedEvidenceFallbackScript(entry.path))
      continue;

    bool isVisible = entry.manual_visible && entry.frozen &&
        (entry.expected_result == "ok" || entry.expected_result == "ng_expected");
    if (!isVisible) continue;

    if (hasThumbForScript(entry.script_id, entry.path))
      continue;

    ScriptEvidenceGroup& group =
        findOrCreateGroup(entry.script_id, entry.path, entry.tool);

    ScriptEvidenceThumb thumb;
    thumb.script_id = entry.script_id;
    thumb.script_path = entry.path;
    thumb.tool = entry.tool;
    thumb.parameter_summary =
        BuildDefaultEvidenceParamSummaryForScript(entry.path);
    thumb.reason = entry.parameter_policy_id.empty()
        ? "catalog fallback default params"
        : "catalog fallback default params from policy " +
          entry.parameter_policy_id;

    for (const auto& img : m_manualTest.image_manifest_items)
    {
      if (!img.image_path.empty())
      {
        thumb.image_id = img.image_id;
        thumb.image_path = img.image_path;
        break;
      }
    }

    AssignFallbackImageToThumb(thumb, fallbackImages, fallbackImageIndex++);
    PopulateEditableObjectBindingForThumbLocal(thumb);

    group.thumbs.push_back(thumb);
  }

  const std::string algorithmReviewHandoff =
      "D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxscript_runs/suite/"
      "run_20260730_findline_findcircle_algorithm_boundary_v13/reports/"
      "manual_algorithm_review_handoff.md";
  AppendManualAlgorithmReviewHandoffLocal(
      m_manualTest,
      algorithmReviewHandoff,
      [this](const std::string& imageId) -> std::string
      {
        return ResolveImagePathFromManifest(imageId);
      },
      [&](const std::string& label) -> ScriptEvidenceGroup&
      {
        return findOrCreateGroup("", "", label);
      });

  std::stable_sort(
      m_manualTest.script_evidence_groups.begin(),
      m_manualTest.script_evidence_groups.end(),
      [](const ScriptEvidenceGroup& left, const ScriptEvidenceGroup& right)
      {
        auto priority = [](const ScriptEvidenceGroup& group) -> int
        {
          const std::string key = group.label + " " + group.script_id + " " + group.script_path;
          if (key.find("Findline") != std::string::npos ||
              key.find("findline") != std::string::npos ||
              key.find("find_line") != std::string::npos)
            return 0;
          if (key.find("Findcircle") != std::string::npos ||
              key.find("findcircle") != std::string::npos ||
              key.find("find_circle") != std::string::npos)
            return 1;
          if (key.find("FindEllipse") != std::string::npos ||
              key.find("findellipse") != std::string::npos ||
              key.find("find_ellipse") != std::string::npos)
            return 2;
          if (key.find("FindRect") != std::string::npos ||
              key.find("findrect") != std::string::npos ||
              key.find("find_rect") != std::string::npos)
            return 3;
          if (key.find("fastmatch") != std::string::npos ||
              key.find("FastMatch") != std::string::npos)
            return 4;
          if (key.find("FindSegmentation") != std::string::npos ||
              key.find("find_segmentation") != std::string::npos)
            return 5;
          return 10;
        };

        const int lp = priority(left);
        const int rp = priority(right);
        if (lp != rp)
          return lp < rp;
        return left.script_id < right.script_id;
      });

  // Keep baseline Catalog entries alongside Suite/review evidence.  The old
  // empty-only condition made a review handoff hide the entire Verified list.
  {
    for (const auto& item : m_scriptCatalog)
    {
      if (!IsAllowedEvidenceFallbackScript(item.path))
        continue;

      const bool isDirectLike =
          item.name.find("direct_test") != std::string::npos ||
          item.name.find("_direct") != std::string::npos ||
          item.name.find("_smoke") != std::string::npos ||
          item.path.find("/headless/") != std::string::npos ||
          item.path.find("\\headless\\") != std::string::npos;

      if (!m_showAllScripts && !isDirectLike)
        continue;

      if (hasThumbForScript(item.name, item.path))
        continue;

      ScriptEvidenceGroup group;
      group.script_id = item.name;
      group.script_path = item.path;
      group.label = item.type.empty() ? "script" : item.type;

      ScriptEvidenceThumb thumb;
      thumb.script_id = item.name;
      thumb.script_path = item.path;
      thumb.tool = item.type;
      thumb.status = item.status;
      thumb.reason = item.description;

      for (const auto& img : m_manualTest.image_manifest_items)
      {
        if (!img.image_path.empty())
        {
          thumb.image_id = img.image_id;
          thumb.image_path = img.image_path;
          thumb.parameter_summary = img.level;
          break;
        }
      }

      if (thumb.image_path.empty() && !m_manualTest.image_file_path.empty())
      {
        thumb.image_id = m_manualTest.active_image_id.empty()
            ? "current_image"
            : m_manualTest.active_image_id;
        thumb.image_path = m_manualTest.image_file_path;
      }

      AssignFallbackImageToThumb(thumb, fallbackImages, fallbackImageIndex++);
      PopulateEditableObjectBindingForThumbLocal(thumb);

      group.thumbs.push_back(thumb);
      m_manualTest.script_evidence_groups.push_back(group);
    }
  }

  m_manualTest.script_evidence_groups_dirty = false;
  m_manualTest.script_evidence_row_refs_dirty = true;
}

void ViewController::EnsureEvidenceChainThumbnailsLoaded()
{
  if (m_manualTest.workbench_assets_loaded)
    return;

  for (const auto& group : m_manualTest.script_evidence_groups)
  {
    for (const auto& thumb : group.thumbs)
    {
      EvidenceChainThumb ect;
      ect.script_id = thumb.script_id;
      ect.tool = thumb.tool;
      m_manualTest.evidence_chain_thumbs.push_back(ect);
    }
  }

  m_manualTest.workbench_assets_loaded = true;
}

void ViewController::SelectEvidenceChainThumb(int index)
{
  if (index < 0 || index >= static_cast<int>(m_manualTest.evidence_chain_thumbs.size()))
    return;

  m_manualTest.selected_evidence_thumb = index;
  const EvidenceChainThumb& ect = m_manualTest.evidence_chain_thumbs[index];

  for (const auto& entry : m_manualTest.catalog_entries)
  {
    if (entry.script_id == ect.script_id)
    {
      m_manualTest.editor_source = "catalog";
      ReadTextFile(entry.path, m_manualTest.editor_text);
      m_manualTest.editor_dirty = false;
      m_manualTest.debug_action = "Load Script";
      m_manualTest.debug_status = "PENDING";
      m_manualTest.debug_reason = "Loaded from catalog: " + ect.script_id;
      break;
    }
  }
}

void ViewController::DrawEvidenceChainThumbnailRail()
{
  if (m_manualTest.evidence_chain_thumbs.empty())
    return;

  ImGui::BeginChild("evidence_chain_rail", ImVec2(-1, 80), true);

  const float thumbWidth = 64.0f;
  const float thumbHeight = 64.0f;
  const float spacing = 8.0f;
  const int visibleCount = static_cast<int>(
      ImGui::GetContentRegionAvail().x / (thumbWidth + spacing));

  const int startIndex = std::max(0,
      m_manualTest.selected_evidence_thumb - visibleCount / 2);

  for (int i = startIndex;
       i < static_cast<int>(m_manualTest.evidence_chain_thumbs.size()) &&
           i < startIndex + visibleCount + 1;
       ++i)
  {
    if (i > startIndex) ImGui::SameLine();
    const EvidenceChainThumb& ect = m_manualTest.evidence_chain_thumbs[i];
    const bool isSelected = i == m_manualTest.selected_evidence_thumb;

    ImGui::PushID(i);
    if (isSelected)
      ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(40, 100, 200, 255));

    if (ImGui::ImageButton(
            ("thumb_" + std::to_string(i)).c_str(),
            static_cast<ImU64>(ect.texture_id ? ect.texture_id : 1),
            ImVec2(thumbWidth, thumbHeight)))
    {
      SelectEvidenceChainThumb(i);
    }

    if (isSelected)
      ImGui::PopStyleColor();

    ImGui::SetTooltip("%s\n%s", ect.script_id.c_str(), ect.tool.c_str());
    ImGui::PopID();
  }

  ImGui::EndChild();
}

void ViewController::RebuildScriptEvidenceGroups()
{
  m_manualTest.script_evidence_groups.clear();
  m_manualTest.selected_evidence_group = -1;
  m_manualTest.selected_evidence_thumb = -1;
  m_manualTest.current_evidence_selection = CxEvidenceSelectionSnapshot{};
  m_manualTest.script_evidence_groups_dirty = true;
  m_manualTest.script_evidence_row_refs_dirty = true;
  EnsureCxScriptWorkbenchAssetsLoaded();
}

void ViewController::RebuildScriptEvidenceRowRefs()
{
    m_manualTest.script_evidence_row_refs.clear();

    for (std::size_t gi = 0; gi < m_manualTest.script_evidence_groups.size(); ++gi)
    {
        ScriptEvidenceRowRef header;
        header.group_index = static_cast<int>(gi);
        header.thumb_index = -1;
        header.is_group_header = true;
        header.label = m_manualTest.script_evidence_groups[gi].label;
        m_manualTest.script_evidence_row_refs.push_back(header);

        for (std::size_t ti = 0; ti < m_manualTest.script_evidence_groups[gi].thumbs.size(); ++ti)
        {
            ScriptEvidenceRowRef row;
            row.group_index = static_cast<int>(gi);
            row.thumb_index = static_cast<int>(ti);
            row.is_group_header = false;
            row.label = m_manualTest.script_evidence_groups[gi].thumbs[ti].script_id;
            m_manualTest.script_evidence_row_refs.push_back(row);
        }
    }

    m_manualTest.script_evidence_row_refs_dirty = false;
}

std::string ViewController::ResolveImagePathFromManifest(const std::string& imageId) const
{
  for (const auto& item : m_manualTest.evidence_items)
  {
    if (item.image_id == imageId && !item.image_path.empty())
      return item.image_path;
  }
  for (const auto& item : m_manualTest.image_manifest_items)
  {
    if (item.image_id == imageId && !item.image_path.empty())
      return item.image_path;
  }
  return "";
}

std::string ViewController::ResolveCatalogScriptPathById(const std::string& scriptId) const
{
  for (const auto& entry : m_manualTest.catalog_entries)
  {
    if (entry.script_id == scriptId)
      return entry.path;
  }
  return "";
}

std::string ViewController::ResolveCatalogScriptLabelById(const std::string& scriptId) const
{
  for (const auto& entry : m_manualTest.catalog_entries)
  {
    if (entry.script_id == scriptId)
      return entry.label.empty() ? entry.script_id : entry.label;
  }
  return scriptId;
}

void ViewController::EnsureScriptEvidenceThumbTexture(ScriptEvidenceThumb& thumb)
{
  if (thumb.texture_loaded || thumb.texture_failed)
    return;

  if (m_manualTest.script_evidence_thumb_load_count_this_frame >=
      m_manualTest.script_evidence_thumb_load_budget_per_frame)
  {
    return;
  }

  if (thumb.image_path.empty() && !thumb.image_id.empty())
    thumb.image_path = ResolveImagePathFromManifest(thumb.image_id);

  const std::string previewPath = thumb.thumbnail_path.empty()
      ? thumb.image_path
      : thumb.thumbnail_path;

  if (previewPath.empty())
  {
    cv::Mat placeholder(60, 80, CV_8UC3, cv::Scalar(90, 120, 150));
    cv::putText(
        placeholder,
        "NO IMG",
        cv::Point(12, 36),
        cv::FONT_HERSHEY_SIMPLEX,
        0.4,
        cv::Scalar(255, 255, 255),
        1,
        cv::LINE_AA);

    thumb.texture_id = CreateTextureFromMat0(placeholder);
    thumb.texture_w = placeholder.cols;
    thumb.texture_h = placeholder.rows;
    thumb.texture_loaded = thumb.texture_id != 0;
    thumb.texture_failed = !thumb.texture_loaded;
    thumb.reason = "placeholder thumbnail generated; image path is empty";
    ++m_manualTest.script_evidence_thumb_load_count_this_frame;
    return;
  }

  if (!std::filesystem::exists(previewPath))
  {
    thumb.texture_failed = true;
    thumb.reason = "thumbnail image not found: " + previewPath;
    ++m_manualTest.script_evidence_thumb_load_count_this_frame;
    return;
  }

  cv::Mat image = cv::imread(previewPath, cv::IMREAD_COLOR);
  if (image.empty())
  {
    thumb.texture_failed = true;
    thumb.reason = "thumbnail image read failed: " + previewPath;
    ++m_manualTest.script_evidence_thumb_load_count_this_frame;
    return;
  }

  cv::Mat preview;
  const int maxSide = 80;
  const int srcMaxSide = std::max(image.cols, image.rows);
  const double scale = srcMaxSide > 0
      ? static_cast<double>(maxSide) / static_cast<double>(srcMaxSide)
      : 1.0;
  if (scale > 0.0 && scale < 1.0)
    cv::resize(image, preview, cv::Size(), scale, scale, cv::INTER_AREA);
  else
    preview = image;

  thumb.texture_id = CreateTextureFromMat0(preview);
  thumb.texture_w = preview.cols;
  thumb.texture_h = preview.rows;
  thumb.texture_loaded = thumb.texture_id != 0;
  thumb.texture_failed = !thumb.texture_loaded;
  if (thumb.texture_failed)
    thumb.reason = "failed to create thumbnail texture";
  ++m_manualTest.script_evidence_thumb_load_count_this_frame;
}

bool ViewController::ActivateScriptEvidenceThumb(
    const ScriptEvidenceThumb& thumb,
    bool loadImageToView,
    std::string& reason)
{
    reason.clear();

    int groupIndex = m_manualTest.selected_evidence_group;
    int thumbIndex = m_manualTest.selected_evidence_thumb;

    bool indexMatches = false;
    if (groupIndex >= 0 &&
        groupIndex < static_cast<int>(m_manualTest.script_evidence_groups.size()))
    {
        const ScriptEvidenceGroup& group =
            m_manualTest.script_evidence_groups[groupIndex];

        if (thumbIndex >= 0 &&
            thumbIndex < static_cast<int>(group.thumbs.size()))
        {
            const ScriptEvidenceThumb& selectedThumb = group.thumbs[thumbIndex];
            indexMatches =
                selectedThumb.script_id == thumb.script_id &&
                selectedThumb.script_path == thumb.script_path;
        }
    }

    if (!indexMatches)
    {
        groupIndex = -1;
        thumbIndex = -1;
    }

    CxEvidenceSelectionSnapshot snapshot;
    if (!BuildEvidenceSnapshotFromThumb(
            groupIndex,
            thumbIndex,
            thumb,
            snapshot,
            reason))
    {
        return false;
    }

    return ApplyEvidenceSelectionSnapshotToManualContext(
        snapshot,
        loadImageToView,
        reason);
}

bool ViewController::BuildEvidenceSnapshotFromThumb(
    int groupIndex,
    int thumbIndex,
    const ScriptEvidenceThumb& thumb,
    CxEvidenceSelectionSnapshot& out,
    std::string& reason) const
{
    reason.clear();
    out = CxEvidenceSelectionSnapshot{};

    std::string scriptPath = thumb.script_path;
    if (scriptPath.empty())
        scriptPath = ResolveCatalogScriptPathById(thumb.script_id);

    if (IsDeprecatedCxScriptPath(scriptPath))
    {
        reason = "deprecated cxscript cannot be used as Evidence binding: " +
                 scriptPath;
        return false;
    }

    if (thumb.script_id.empty() && scriptPath.empty())
    {
        reason = "evidence thumb has neither script_id nor script_path";
        return false;
    }

    out.valid = true;
    out.group_index = groupIndex;
    out.thumb_index = thumbIndex;

    out.case_id = thumb.case_id;

    out.script_id = thumb.script_id.empty() ? scriptPath : thumb.script_id;
    out.script_path = scriptPath;

    out.image_id = thumb.image_id;
    out.image_path = thumb.image_path;

    out.target_id = thumb.target_id;
    out.tool = thumb.tool;

    out.parameter_summary = thumb.parameter_summary;
    out.parameter_profile_id = thumb.parameter_summary;

    out.status = thumb.status;
    out.reason = thumb.reason;
    out.source = "evidence_thumb";
    out.primary_object_type = thumb.primary_object_type;
    out.primary_object_name = thumb.primary_object_name;
    out.primary_object_status = thumb.primary_object_status;

    if (!scriptPath.empty())
    {
        std::string scriptText;
        if (ReadTextFile(scriptPath, scriptText))
        {
            AnalyzeEditableObjectsFromCxScriptLocal(
                scriptText,
                out.editable_objects);
            if (out.primary_object_status.empty() ||
                out.primary_object_status == "script_read_failed" ||
                out.primary_object_status == "script_path_empty")
            {
                ResolvePrimaryEditableObjectLocal(
                    out.tool,
                    out.target_id,
                    out.parameter_summary,
                    out.editable_objects,
                    out.primary_object_type,
                    out.primary_object_name,
                    out.primary_object_status);
            }
        }
    }

    return true;
}

bool ViewController::GetSelectedEvidenceSnapshot(
    CxEvidenceSelectionSnapshot& out,
    std::string& reason) const
{
    reason.clear();
    out = CxEvidenceSelectionSnapshot{};

    const int groupIndex = m_manualTest.selected_evidence_group;
    const int thumbIndex = m_manualTest.selected_evidence_thumb;

    if (groupIndex < 0 ||
        groupIndex >= static_cast<int>(m_manualTest.script_evidence_groups.size()))
    {
        reason = "no evidence group selected";
        return false;
    }

    const ScriptEvidenceGroup& group =
        m_manualTest.script_evidence_groups[groupIndex];

    if (thumbIndex < 0 ||
        thumbIndex >= static_cast<int>(group.thumbs.size()))
    {
        reason = "no evidence thumb selected";
        return false;
    }

    return BuildEvidenceSnapshotFromThumb(
        groupIndex,
        thumbIndex,
        group.thumbs[thumbIndex],
        out,
        reason);
}

bool ViewController::ApplyEvidenceSelectionSnapshotToManualContext(
    const CxEvidenceSelectionSnapshot& snapshot,
    bool loadImageToView,
    std::string& reason)
{
    reason.clear();

    if (!snapshot.valid)
    {
        reason = "invalid evidence selection snapshot";
        return false;
    }

    m_manualTest.current_evidence_selection = snapshot;

    m_manualTest.selected_evidence_group = snapshot.group_index;
    m_manualTest.selected_evidence_thumb = snapshot.thumb_index;

    m_manualTest.active_case_id = snapshot.case_id;
    m_manualTest.active_image_id = snapshot.image_id;
    m_manualTest.active_target_id = snapshot.target_id;

    if (!snapshot.image_path.empty())
        m_manualTest.image_file_path = snapshot.image_path;

    if (!snapshot.script_path.empty())
    {
        std::string text;
        if (!ReadTextFile(snapshot.script_path, text))
        {
            reason = "failed to read evidence script: " + snapshot.script_path;
            return false;
        }

        m_manualTest.editor_text = text;
        m_manualTest.loaded_script_path = snapshot.script_path;
        m_manualTest.script_file_path = snapshot.script_path;
        m_manualTest.editor_source = "evidence";
        m_manualTest.editor_dirty = false;
        SeedDefaultManualGlobals(m_manualTest, snapshot.script_path);
        m_manualTest.current_gauge.primary_object_type =
            snapshot.primary_object_type;
        m_manualTest.current_gauge.primary_object_name =
            snapshot.primary_object_name;
        m_manualTest.current_gauge.primary_object_status =
            snapshot.primary_object_status.empty()
                ? "unresolved"
                : snapshot.primary_object_status;
    }

    std::string lockedParamReason;
    if (EvidenceSnapshotHasLockedParamSummaryLocal(snapshot, lockedParamReason))
    {
        if (!ApplyEvidenceParameterSummaryToRuntimeGlobals(
                snapshot.parameter_summary,
                lockedParamReason))
        {
            reason = "failed to apply evidence locked parameters: " +
                     lockedParamReason;
            return false;
        }
        SyncEvidenceLockedGlobalsToManualGaugeLocal(
            m_manualTest,
            snapshot.script_path,
            "evidence_locked",
            snapshot.primary_object_type,
            snapshot.primary_object_name,
            snapshot.primary_object_status);
    }

    m_manualTest.debug_action = "Apply Evidence Selection";
    m_manualTest.debug_status = "PENDING";
    m_manualTest.debug_reason =
        "script=" + snapshot.script_id +
        " image=" + snapshot.image_id +
        " target=" + snapshot.target_id +
        " param=" + snapshot.parameter_summary +
        (lockedParamReason.empty()
            ? " | evidence params locked"
            : " | evidence params not locked: " + lockedParamReason);

    if (loadImageToView)
    {
        if (snapshot.image_path.empty())
        {
            reason = "selected evidence has empty image_path";
            return false;
        }

        if (!LoadImageIntoImageView(snapshot.image_path, reason))
            return false;

        m_manualTest.debug_status = "IMAGE_VIEW_LOADED";
        m_manualTest.debug_reason =
            "loaded image from evidence snapshot: " + snapshot.image_path;
    }

    return true;
}

void ViewController::ResetEvidenceThumbTexture(ScriptEvidenceThumb& thumb)
{
    thumb.texture_id = 0;
    thumb.texture_w = 0;
    thumb.texture_h = 0;
    thumb.texture_loaded = false;
    thumb.texture_failed = false;
}

bool ViewController::RefreshEvidenceSelectionFromThumb(
    int groupIndex,
    int thumbIndex,
    bool loadImageToView,
    std::string& reason)
{
    reason.clear();

    if (groupIndex < 0 ||
        groupIndex >= static_cast<int>(m_manualTest.script_evidence_groups.size()))
    {
        reason = "invalid evidence group index";
        return false;
    }

    ScriptEvidenceGroup& group =
        m_manualTest.script_evidence_groups[groupIndex];

    if (thumbIndex < 0 ||
        thumbIndex >= static_cast<int>(group.thumbs.size()))
    {
        reason = "invalid evidence thumb index";
        return false;
    }

    CxEvidenceSelectionSnapshot snapshot;
    if (!BuildEvidenceSnapshotFromThumb(
            groupIndex,
            thumbIndex,
            group.thumbs[thumbIndex],
            snapshot,
            reason))
    {
        return false;
    }

    return ApplyEvidenceSelectionSnapshotToManualContext(
        snapshot,
        loadImageToView,
        reason);
}

void ViewController::SelectScriptEvidenceThumb(int groupIndex, int thumbIndex)
{
    if (groupIndex < 0 ||
        groupIndex >= static_cast<int>(m_manualTest.script_evidence_groups.size()))
        return;

    ScriptEvidenceGroup& group = m_manualTest.script_evidence_groups[groupIndex];
    if (thumbIndex < 0 || thumbIndex >= static_cast<int>(group.thumbs.size()))
        return;

    CxEvidenceSelectionSnapshot snapshot;
    std::string reason;

    if (!BuildEvidenceSnapshotFromThumb(
            groupIndex,
            thumbIndex,
            group.thumbs[thumbIndex],
            snapshot,
            reason))
    {
        m_manualTest.debug_status = "EVIDENCE_SELECT_FAIL";
        m_manualTest.debug_reason = reason;
        return;
    }

    if (!ApplyEvidenceSelectionSnapshotToManualContext(
            snapshot,
            false,
            reason))
    {
        m_manualTest.debug_status = "EVIDENCE_APPLY_FAIL";
        m_manualTest.debug_reason = reason;
        return;
    }
}

void ViewController::DrawScriptEvidenceThumbnailRailByGroup()
{
    if (m_manualTest.script_evidence_groups.empty())
    {
        ImGui::TextColored(
            ImVec4(1.0f, 0.75f, 0.2f, 1.0f),
            "No trace binding thumbnails.");
        return;
    }

    m_manualTest.script_evidence_thumb_load_count_this_frame = 0;

    const float rowHeight = 92.0f;
    const float availableHeight = ImGui::GetContentRegionAvail().y;
    const float targetHeight = rowHeight * 4.0f + 72.0f;
    const float listHeight = std::max(
        220.0f,
        std::min(availableHeight > 0.0f ? availableHeight : targetHeight,
                 targetHeight));

    struct EvidenceCategory
    {
        std::string label;
        int priority = 100;
        std::vector<ScriptEvidenceRowRef> rows;
    };

    struct EvidenceMajorCategory
    {
        std::string label;
        int priority = 100;
        std::vector<EvidenceCategory> tools;
    };

    auto toLower = [](std::string value) -> std::string
    {
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return value;
    };

    auto classifyMajor = [&](const ScriptEvidenceThumb& thumb,
                             const ScriptEvidenceGroup& group) -> std::pair<int, std::string>
    {
        const std::string key = toLower(
            thumb.status + " " +
            thumb.reason + " " +
            thumb.parameter_summary + " " +
            thumb.script_id + " " +
            thumb.script_path + " " +
            group.label);

        if (key.find("pending_algorithm_review") != std::string::npos ||
            key.find("pending_human_review") != std::string::npos ||
            key.find("primary_object_selection_pending") != std::string::npos ||
            key.find("pending") != std::string::npos ||
            key.find("待验证") != std::string::npos ||
            key.find("待测试") != std::string::npos)
            return {1, "To Verify"};

        if (key.find("fail") != std::string::npos ||
            key.find("error") != std::string::npos ||
            key.find("defect") != std::string::npos ||
            key.find("blocked") != std::string::npos ||
            key.find("有缺陷") != std::string::npos)
            return {2, "Defect"};

        if (key.find("diagnostic") != std::string::npos ||
            key.find("legacy") != std::string::npos ||
            key.find("ng_expected") != std::string::npos ||
            key.find("smoke") != std::string::npos ||
            key.find("process") != std::string::npos ||
            key.find("过程") != std::string::npos)
            return {3, "Process Validation"};

        return {0, "Verified"};
    };

    auto classifyTool = [](const ScriptEvidenceThumb& thumb,
                           const ScriptEvidenceGroup& group) -> std::pair<int, std::string>
    {
        const std::string exactTool = NormalizeEvidenceToolTypeLocal(thumb.tool);
        if (exactTool == "FindLine")
            return {0, "FindLine"};
        if (exactTool == "FindCircle")
            return {1, "FindCircle"};
        if (exactTool == "FindObject")
            return {2, "FindObject"};
        if (exactTool == "FindEllipse")
            return {3, "FindEllipse"};
        if (exactTool == "FindRect")
            return {4, "FindRect"};
        if (exactTool == "FastMatch")
            return {5, "FastMatch"};
        if (exactTool == "FindSegmentation")
            return {6, "FindSegmentation"};

        const std::string key =
            thumb.tool + " " + thumb.script_id + " " + thumb.script_path + " " + group.label;

        if (key.find("FindLine") != std::string::npos ||
            key.find("Findline") != std::string::npos ||
            key.find("findline") != std::string::npos ||
            key.find("find_line") != std::string::npos)
            return {0, "FindLine"};

        if (key.find("FindCircle") != std::string::npos ||
            key.find("Findcircle") != std::string::npos ||
            key.find("findcircle") != std::string::npos ||
            key.find("find_circle") != std::string::npos)
            return {1, "FindCircle"};

        if (key.find("FindObject") != std::string::npos ||
            key.find("findobject") != std::string::npos ||
            key.find("find_object") != std::string::npos)
            return {2, "FindObject"};

        if (key.find("FindEllipse") != std::string::npos ||
            key.find("FindEllipse") != std::string::npos ||
            key.find("findellipse") != std::string::npos ||
            key.find("find_ellipse") != std::string::npos)
            return {3, "FindEllipse"};

        if (key.find("FindRect") != std::string::npos ||
            key.find("findrect") != std::string::npos ||
            key.find("find_rect") != std::string::npos)
            return {4, "FindRect"};

        if (key.find("FastMatch") != std::string::npos ||
            key.find("fastmatch") != std::string::npos)
            return {5, "FastMatch"};

        if (key.find("FindSegmentation") != std::string::npos ||
            key.find("find_segmentation") != std::string::npos ||
            key.find("findsegmentation") != std::string::npos)
            return {6, "FindSegmentation"};

        if (key.find("integration") != std::string::npos)
            return {20, "Integration"};

        return {30, group.label.empty() ? "Other" : group.label};
    };

    // These are navigation buckets, not data-dependent labels.  Keep all four
    // visible even when a bucket has no evidence yet, so the operator always
    // sees the same Evidence Chain workflow.
    std::vector<EvidenceMajorCategory> categories = {
        { "Verified", 0, {} },
        { "To Verify", 1, {} },
        { "Defect", 2, {} },
        { "Process Validation", 3, {} }
    };

    auto findOrCreateMajor =
        [&](int priority, const std::string& label) -> EvidenceMajorCategory&
        {
            for (auto& category : categories)
            {
                if (category.priority == priority && category.label == label)
                    return category;
            }

            EvidenceMajorCategory category;
            category.priority = priority;
            category.label = label;
            categories.push_back(category);
            return categories.back();
        };

    auto findOrCreateTool =
        [](EvidenceMajorCategory& major,
           int priority,
           const std::string& label) -> EvidenceCategory&
        {
            for (auto& tool : major.tools)
            {
                if (tool.priority == priority && tool.label == label)
                    return tool;
            }

            EvidenceCategory tool;
            tool.priority = priority;
            tool.label = label;
            major.tools.push_back(tool);
            return major.tools.back();
        };

    for (std::size_t gi = 0; gi < m_manualTest.script_evidence_groups.size(); ++gi)
    {
        ScriptEvidenceGroup& group = m_manualTest.script_evidence_groups[gi];
        for (std::size_t ti = 0; ti < group.thumbs.size(); ++ti)
        {
            ScriptEvidenceThumb& thumb = group.thumbs[ti];
            const auto majorClass = classifyMajor(thumb, group);
            const auto toolClass = classifyTool(thumb, group);

            ScriptEvidenceRowRef row;
            row.group_index = static_cast<int>(gi);
            row.thumb_index = static_cast<int>(ti);
            row.is_group_header = false;
            row.label = thumb.script_id;

            EvidenceMajorCategory& major =
                findOrCreateMajor(majorClass.first, majorClass.second);
            findOrCreateTool(major, toolClass.first, toolClass.second)
                .rows.push_back(row);
        }
    }

    std::stable_sort(
        categories.begin(),
        categories.end(),
        [](const EvidenceMajorCategory& left, const EvidenceMajorCategory& right)
        {
            if (left.priority != right.priority)
                return left.priority < right.priority;
            return left.label < right.label;
        });
    for (auto& major : categories)
    {
        std::stable_sort(
            major.tools.begin(),
            major.tools.end(),
            [](const EvidenceCategory& left, const EvidenceCategory& right)
            {
                if (left.priority != right.priority)
                    return left.priority < right.priority;
                return left.label < right.label;
            });
    }

    ImGui::BeginChild("script_evidence_by_group", ImVec2(-1, listHeight), true);

    for (std::size_t ci = 0; ci < categories.size(); ++ci)
    {
        EvidenceMajorCategory& major = categories[ci];

        int majorCount = 0;
        for (const auto& tool : major.tools)
            majorCount += static_cast<int>(tool.rows.size());

        ImGui::PushID(static_cast<int>(ci));

        std::string header =
            major.label + " (" + std::to_string(majorCount) + ")";
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_None;
        // The category level is the primary navigation level.  Open it when
        // the panel first appears; the user can still collapse it afterwards.
        flags |= ImGuiTreeNodeFlags_DefaultOpen;

        if (ImGui::CollapsingHeader(header.c_str(), flags))
        {
            if (major.tools.empty())
            {
                ImGui::TextDisabled("No evidence entries.");
            }

            for (std::size_t ti = 0; ti < major.tools.size(); ++ti)
            {
                EvidenceCategory& tool = major.tools[ti];
                if (tool.rows.empty())
                    continue;

                ImGui::PushID(static_cast<int>(ti));
                const std::string toolHeader =
                    tool.label + " (" + std::to_string(tool.rows.size()) + ")";
                ImGuiTreeNodeFlags toolFlags =
                    ImGuiTreeNodeFlags_DefaultOpen |
                    ImGuiTreeNodeFlags_OpenOnArrow;
                if (ImGui::TreeNodeEx(toolHeader.c_str(), toolFlags))
                {
                    for (const ScriptEvidenceRowRef& ref : tool.rows)
                    {
                        if (ref.group_index < 0 ||
                            ref.group_index >=
                                static_cast<int>(m_manualTest.script_evidence_groups.size()))
                            continue;

                        ScriptEvidenceGroup& group =
                            m_manualTest.script_evidence_groups[ref.group_index];

                        if (ref.thumb_index < 0 ||
                            ref.thumb_index >= static_cast<int>(group.thumbs.size()))
                            continue;

                        ScriptEvidenceThumb& thumb = group.thumbs[ref.thumb_index];

                        EnsureScriptEvidenceThumbTexture(thumb);

                        DrawOneScriptEvidenceRow(
                            ref.group_index,
                            ref.thumb_index,
                            thumb,
                            rowHeight);
                    }
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
        }

        ImGui::PopID();
    }

    ImGui::EndChild();
}

static std::string BuildCurrentRuntimeParamSummary(
    const ManualTestContext& context)
{
    auto getInt = [&](const std::string& key, int fallback) -> int
    {
        auto it = context.runtime_int_vars.find(key);
        return it == context.runtime_int_vars.end() ? fallback : it->second;
    };

    std::ostringstream oss;
    oss << "method=" << getInt("global_method", 0)
        << " threshold=" << getInt("global_threshold", 20)
        << " wgap=" << getInt("global_wgap", 0)
        << " hgap=" << getInt("global_hgap", 0)
        << " gap=" << getInt("global_gap", 0)
        << " linegap=" << getInt("global_linegap", 0)
        << " tool_half_width=" << getInt("global_tool_half_width", 0)
        << " roi_x0=" << getInt("global_roi_x0", 0)
        << " roi_y0=" << getInt("global_roi_y0", 0)
        << " roi_x1=" << getInt("global_roi_x1", 0)
        << " roi_y1=" << getInt("global_roi_y1", 0)
        << " roi_x=" << getInt("global_roi_x", 0)
        << " roi_y=" << getInt("global_roi_y", 0)
        << " roi_width=" << getInt("global_roi_width", 0)
        << " roi_height=" << getInt("global_roi_height", 0)
        << " circle_cx=" << getInt("global_circle_cx", 0)
        << " circle_cy=" << getInt("global_circle_cy", 0)
        << " circle_px=" << getInt("global_circle_px", 0)
        << " circle_py=" << getInt("global_circle_py", 0)
        << " ellipse_x0=" << getInt("global_ellipse_x0", 0)
        << " ellipse_y0=" << getInt("global_ellipse_y0", 0)
        << " ellipse_x1=" << getInt("global_ellipse_x1", 0)
        << " ellipse_y1=" << getInt("global_ellipse_y1", 0)
        << " learn_roi_x=" << getInt("global_learn_roi_x", 0)
        << " learn_roi_y=" << getInt("global_learn_roi_y", 0)
        << " learn_roi_w=" << getInt("global_learn_roi_w", 0)
        << " learn_roi_h=" << getInt("global_learn_roi_h", 0)
        << " search_roi_x=" << getInt("global_search_roi_x", 0)
        << " search_roi_y=" << getInt("global_search_roi_y", 0)
        << " search_roi_w=" << getInt("global_search_roi_w", 0)
        << " search_roi_h=" << getInt("global_search_roi_h", 0);

    return oss.str();
}

void ViewController::DrawOneScriptEvidenceRow(
    int groupIndex,
    int thumbIndex,
    ScriptEvidenceThumb& thumb,
    float rowHeight)
{
    ImGui::PushID(groupIndex * 1000 + thumbIndex);

    const bool selected =
        m_manualTest.selected_evidence_group == groupIndex &&
        m_manualTest.selected_evidence_thumb == thumbIndex;

    if (selected)
        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(45, 80, 115, 180));

    ImGui::BeginChild(
        "evidence_row",
        ImVec2(-1, rowHeight),
        true,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    const ImVec2 rowMin = ImGui::GetCursorScreenPos();
    const ImVec2 rowSize(ImGui::GetContentRegionAvail().x, rowHeight - 6.0f);

    ImGui::InvisibleButton(
        "evidence_row_hit",
        rowSize,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

    const bool rowHovered = ImGui::IsItemHovered();
    const bool rowClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    const bool rowDoubleClicked =
        rowHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
    const bool rowRightClicked = ImGui::IsItemClicked(ImGuiMouseButton_Right);

    ImGui::SetCursorScreenPos(rowMin);

    const float imageColWidth = 96.0f;

    if (ImGui::BeginTable(
            "evidence_row_table",
            2,
            ImGuiTableFlags_SizingStretchProp,
            ImVec2(-1, rowHeight - 8.0f)))
    {
        ImGui::TableSetupColumn("Text", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Image", ImGuiTableColumnFlags_WidthFixed, imageColWidth);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);

        ImGui::TextUnformatted(thumb.script_id.empty() ? "(no script)" : thumb.script_id.c_str());
        ImGui::TextDisabled("path: %.90s", thumb.script_path.empty() ? "-" : thumb.script_path.c_str());
        ImGui::Text("tool: %s | status: %s",
                    thumb.tool.empty() ? "-" : thumb.tool.c_str(),
                    thumb.status.empty() ? "-" : thumb.status.c_str());
        ImGui::Text("image: %s | target: %s",
                    thumb.image_id.empty() ? "-" : thumb.image_id.c_str(),
                    thumb.target_id.empty() ? "-" : thumb.target_id.c_str());
        ImGui::Text("param: %s",
                    thumb.parameter_summary.empty() ? "-" : thumb.parameter_summary.c_str());
        ImGui::Text("primary: %s %s | %s",
                    thumb.primary_object_type.empty() ? "-" : thumb.primary_object_type.c_str(),
                    thumb.primary_object_name.empty() ? "-" : thumb.primary_object_name.c_str(),
                    thumb.primary_object_status.empty() ? "-" : thumb.primary_object_status.c_str());

        ImGui::TableSetColumnIndex(1);

        const ImVec2 thumbSize(80.0f, 60.0f);

        if (thumb.texture_id != 0)
        {
            ImGui::Image(
                static_cast<ImU64>(thumb.texture_id),
                thumbSize);
        }
        else
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 p0 = ImGui::GetCursorScreenPos();
            ImVec2 p1(p0.x + thumbSize.x, p0.y + thumbSize.y);
            drawList->AddRectFilled(p0, p1, IM_COL32(90, 130, 170, 220));
            drawList->AddText(ImVec2(p0.x + 18, p0.y + 28), IM_COL32(255, 255, 255, 255), "NO IMG");
            ImGui::Dummy(thumbSize);
        }

        ImGui::EndTable();
    }

    if (rowClicked)
    {
        std::string reason;
        if (!RefreshEvidenceSelectionFromThumb(
                groupIndex,
                thumbIndex,
                false,
                reason))
        {
            m_manualTest.debug_status = "EVIDENCE_SELECT_FAIL";
            m_manualTest.debug_reason = reason;
        }
    }

    if (rowDoubleClicked)
    {
        std::string reason;
        if (!RefreshEvidenceSelectionFromThumb(
                groupIndex,
                thumbIndex,
                true,
                reason))
        {
            m_manualTest.debug_status = "LOAD_IMAGE_FAIL";
            m_manualTest.debug_reason = reason;
        }
    }

    if (rowRightClicked)
    {
        ImGui::OpenPopup("evidence_row_context");
    }

    if (ImGui::BeginPopup("evidence_row_context"))
    {
        ImGui::TextUnformatted(thumb.script_id.c_str());
        ImGui::Separator();

        if (ImGui::MenuItem("Load This Image To Image View"))
        {
            std::string reason;
            if (!RefreshEvidenceSelectionFromThumb(
                    groupIndex,
                    thumbIndex,
                    true,
                    reason))
            {
                m_manualTest.debug_status = "LOAD_IMAGE_FAIL";
                m_manualTest.debug_reason = reason;
            }
            else
            {
                m_manualTest.debug_status = "IMAGE_VIEW_LOADED";
                m_manualTest.debug_reason =
                    "loaded from evidence row: " + thumb.image_path;
            }
        }

        if (ImGui::MenuItem("Bind Current Image View"))
        {
            if (m_manualTest.image_file_path.empty())
            {
                m_manualTest.debug_status = "EVIDENCE_IMAGE_BIND_FAIL";
                m_manualTest.debug_reason = "current Image View image path is empty";
            }
            else
            {
                thumb.image_path = m_manualTest.image_file_path;
                thumb.image_id = m_manualTest.active_image_id.empty()
                    ? "current_image"
                    : m_manualTest.active_image_id;

                ResetEvidenceThumbTexture(thumb);

                thumb.reason = "bound from current Image View";

                std::string reason;
                if (!RefreshEvidenceSelectionFromThumb(
                        groupIndex,
                        thumbIndex,
                        false,
                        reason))
                {
                    m_manualTest.debug_status = "EVIDENCE_IMAGE_BIND_FAIL";
                    m_manualTest.debug_reason = reason;
                }
                else
                {
                    m_manualTest.debug_status = "EVIDENCE_IMAGE_BOUND";
                    m_manualTest.debug_reason =
                        thumb.script_id + " -> " + thumb.image_path;
                }
            }
        }

        if (ImGui::MenuItem("Use First Manifest Image"))
        {
            bool bound = false;

            for (const auto& item : m_manualTest.image_manifest_items)
            {
                if (!item.image_path.empty())
                {
                    thumb.image_path = item.image_path;
                    thumb.image_id = item.image_id;
                    ResetEvidenceThumbTexture(thumb);
                    thumb.reason = "bound from manifest image";
                    bound = true;
                    break;
                }
            }

            if (!bound)
            {
                m_manualTest.debug_status = "EVIDENCE_IMAGE_BIND_FAIL";
                m_manualTest.debug_reason = "image manifest has no usable image";
            }
            else
            {
                std::string reason;
                if (!RefreshEvidenceSelectionFromThumb(
                        groupIndex,
                        thumbIndex,
                        false,
                        reason))
                {
                    m_manualTest.debug_status = "EVIDENCE_IMAGE_BIND_FAIL";
                    m_manualTest.debug_reason = reason;
                }
                else
                {
                    m_manualTest.debug_status = "EVIDENCE_IMAGE_BOUND";
                    m_manualTest.debug_reason =
                        "bound first manifest image: " + thumb.image_path;
                }
            }
        }

        if (ImGui::MenuItem("Bind Current Runtime Params"))
        {
            thumb.parameter_summary = BuildCurrentRuntimeParamSummary(m_manualTest);
            thumb.reason = "parameter summary bound from runtime globals";

            std::string reason;
            if (!RefreshEvidenceSelectionFromThumb(
                    groupIndex,
                    thumbIndex,
                    false,
                    reason))
            {
                m_manualTest.debug_status = "EVIDENCE_PARAM_BIND_FAIL";
                m_manualTest.debug_reason = reason;
            }
            else
            {
                m_manualTest.debug_status = "EVIDENCE_PARAM_BOUND";
                m_manualTest.debug_reason = thumb.parameter_summary;
            }
        }

        if (ImGui::MenuItem("Clear Image Binding"))
        {
            thumb.image_path.clear();
            thumb.image_id.clear();
            ResetEvidenceThumbTexture(thumb);
            thumb.reason = "image binding cleared";

            std::string reason;
            if (!RefreshEvidenceSelectionFromThumb(
                    groupIndex,
                    thumbIndex,
                    false,
                    reason))
            {
                m_manualTest.debug_status = "EVIDENCE_IMAGE_CLEAR_FAIL";
                m_manualTest.debug_reason = reason;
            }
            else
            {
                m_manualTest.debug_status = "EVIDENCE_IMAGE_CLEARED";
                m_manualTest.debug_reason = "image binding cleared for " + thumb.script_id;
            }
        }

        ImGui::EndPopup();
    }

    if (rowHovered)
    {
        ImGui::SetTooltip(
            "Click: select | Double-click: load image | Right-click: menu\n"
            "script: %s\nimage: %s\npath: %s\nreason: %s",
            thumb.script_id.c_str(),
            thumb.image_id.c_str(),
            thumb.image_path.c_str(),
            thumb.reason.c_str());
    }

    ImGui::EndChild();

    if (selected)
        ImGui::PopStyleColor();

    ImGui::PopID();
}
