#include "pch.h"
#include "ManualConsoleEvidenceChain.h"
#include "ManualConsoleUtils.h"
#include "ManualConsoleGauge.h"
#include "ManualStateTestConsole.h"
#include "CxScriptCatalogRuntime.h"
#include "CxScriptCasePackageWriter.h"
#include "CxUnifiedLog.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <functional>
#include <sstream>
#include <vector>
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
    if (lowered == "findsegmentation")
        return "FindSegmentation";
    if (lowered == "torchtask" || lowered == "torch")
        return "TorchTask";
    return typeOrTool;
}

static std::string BuildEvidenceClassificationKeyLocal(
    const std::string& tool,
    const std::string& scriptId,
    const std::string& scriptPath,
    const std::string& label,
    const std::string& status,
    const std::string& reason,
    const std::string& parameterSummary)
{
    std::string key =
        tool + " " + scriptId + " " + scriptPath + " " + label + " " +
        status + " " + reason + " " + parameterSummary;
    std::transform(key.begin(), key.end(), key.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return key;
}

static std::string BuildEvidenceCategoryOverrideKeyLocal(
    const ScriptEvidenceThumb& thumb)
{
    std::ostringstream oss;
    oss << "case=" << thumb.case_id
        << "|script_id=" << thumb.script_id
        << "|script_path=" << thumb.script_path
        << "|source_script=" << thumb.source_evidence_script_path
        << "|image=" << thumb.image_id
        << "|target=" << thumb.target_id
        << "|candidate=" << thumb.candidate_id;
    return oss.str();
}

static std::vector<std::string> BuildEvidenceCategoryOverrideLookupKeysLocal(
    const ScriptEvidenceThumb& thumb)
{
    std::vector<std::string> keys;
    keys.push_back(BuildEvidenceCategoryOverrideKeyLocal(thumb));

    if (!thumb.case_id.empty() && !thumb.candidate_id.empty())
    {
        std::ostringstream oss;
        oss << "case=" << thumb.case_id
            << "|candidate=" << thumb.candidate_id;
        keys.push_back(oss.str());
    }

    if (!thumb.case_id.empty())
    {
        std::ostringstream oss;
        oss << "case=" << thumb.case_id;
        keys.push_back(oss.str());
    }

    if (!thumb.image_id.empty() && !thumb.target_id.empty())
    {
        std::ostringstream oss;
        oss << "image=" << thumb.image_id
            << "|target=" << thumb.target_id;
        keys.push_back(oss.str());
    }

    return keys;
}

static std::string ResolveEvidenceCategoryOverrideLocal(
    const ManualTestContext& context,
    const ScriptEvidenceThumb& thumb)
{
    for (const std::string& key :
         BuildEvidenceCategoryOverrideLookupKeysLocal(thumb))
    {
        const auto it = context.evidence_category_overrides.find(key);
        if (it != context.evidence_category_overrides.end())
            return it->second;
    }

    /*
     * Evidence rows come from several sources: locked suite handoff,
     * saved candidates, catalog rows and working revisions.  Not every row
     * carries the same stable key, so after exact lookup we allow a curated
     * case-level/image-target override to match rows whose script/reason/path
     * still contains the locked evidence identity.  This keeps To Verify from
     * being polluted by stale candidate rows after an automatic propagation
     * pass has already moved the case to Process Validation.
     */
    const std::string haystack =
        thumb.case_id + " " +
        thumb.script_id + " " +
        thumb.script_path + " " +
        thumb.source_evidence_script_path + " " +
        thumb.image_id + " " +
        thumb.target_id + " " +
        thumb.reason + " " +
        thumb.parameter_summary;

    for (const auto& entry : context.evidence_category_overrides)
    {
        const std::string& key = entry.first;
        if (key.rfind("case=", 0) == 0)
        {
            const std::string caseId = key.substr(5);
            if (!caseId.empty() &&
                (thumb.case_id == caseId ||
                 haystack.find(caseId) != std::string::npos))
            {
                return entry.second;
            }
            continue;
        }

        if (key.rfind("image=", 0) == 0)
        {
            const std::size_t targetPos = key.find("|target=");
            if (targetPos == std::string::npos)
                continue;
            const std::string imageId = key.substr(6, targetPos - 6);
            const std::string targetId = key.substr(targetPos + 8);
            if (!imageId.empty() && !targetId.empty() &&
                thumb.image_id == imageId &&
                thumb.target_id == targetId)
            {
                return entry.second;
            }
        }
    }

    return thumb.evidence_category_override;
}

static bool HasCuratedFindGeometryCategoryOverridesLocal(
    const ManualTestContext& context)
{
    for (const auto& entry : context.evidence_category_overrides)
    {
        if (entry.first.rfind("case=", 0) == 0 ||
            entry.first.rfind("image=", 0) == 0)
        {
            return true;
        }
    }
    return false;
}

static std::pair<int, std::string> ClassifyEvidenceMajorBucketLocal(
    const ManualTestContext& context,
    const ScriptEvidenceThumb& thumb,
    const std::string& groupLabel)
{
    const std::string categoryOverride =
        ResolveEvidenceCategoryOverrideLocal(context, thumb);
    const bool hasCuratedFindGeometry =
        HasCuratedFindGeometryCategoryOverridesLocal(context);
    const bool isManualGuidanceRow =
        thumb.status == "pending_human_guidance" ||
        groupLabel.find("Needs Human Setting") != std::string::npos ||
        groupLabel.find("needs human setting") != std::string::npos;

    /*
     * To Verify is an operator work queue.  Once a curated propagation pass
     * exists, only the explicit manual guidance queue rows may occupy this
     * bucket; duplicate handoff/catalog/candidate rows for the same case are
     * evidence context and belong to Process Validation.  This makes the UI
     * count traceable to manual_guidance_queue.tsv instead of incidental
     * duplicate rows.
     */
    if (categoryOverride == "To Verify" &&
        hasCuratedFindGeometry &&
        !isManualGuidanceRow)
    {
        return {3, "Process Validation"};
    }

    if (categoryOverride == "Verified")
        return {0, "Verified"};
    if (categoryOverride == "To Verify")
        return {1, "To Verify"};
    if (categoryOverride == "Defect")
        return {2, "Defect"};
    if (categoryOverride == "Process Validation")
        return {3, "Process Validation"};
    if (categoryOverride == "Torch / Model Validation")
        return {4, "Torch / Model Validation"};

    const std::string key = BuildEvidenceClassificationKeyLocal(
        thumb.tool,
        thumb.script_id,
        thumb.script_path,
        groupLabel,
        thumb.status,
        thumb.reason,
        thumb.parameter_summary);

    const bool isTorchLayer =
        key.find("torch") != std::string::npos ||
        key.find("find_segmentation") != std::string::npos ||
        key.find("findsegmentation") != std::string::npos ||
        key.find("edgesam") != std::string::npos ||
        key.find("segmentation") != std::string::npos ||
        key.find("detection") != std::string::npos ||
        key.find("model") != std::string::npos;

    if (isTorchLayer)
        return {4, "Torch / Model Validation"};

    const std::string normalizedTool =
        NormalizeEvidenceToolTypeLocal(thumb.tool);
    const bool isCuratedFindGeometryTool =
        normalizedTool == "FindLine" || normalizedTool == "FindCircle" ||
        key.find("findline") != std::string::npos ||
        key.find("find_line") != std::string::npos ||
        key.find("findcircle") != std::string::npos ||
        key.find("find_circle") != std::string::npos;

    if (isCuratedFindGeometryTool && hasCuratedFindGeometry)
    {
        return {3, "Process Validation"};
    }

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
}

static std::filesystem::path EvidenceCategoryOverridesPathLocal()
{
    return ResolveCxVisionRunPath(
        "cxscript_runs/evidence_chain/evidence_category_overrides.tsv");
}

static std::string EscapeEvidenceOverrideFieldLocal(const std::string& text)
{
    std::string out;
    out.reserve(text.size());
    for (char ch : text)
    {
        switch (ch)
        {
        case '\\': out += "\\\\"; break;
        case '\t': out += "\\t"; break;
        case '\n': out += "\\n"; break;
        case '\r': break;
        default: out += ch; break;
        }
    }
    return out;
}

static std::string UnescapeEvidenceOverrideFieldLocal(const std::string& text)
{
    std::string out;
    out.reserve(text.size());
    bool escaped = false;
    for (char ch : text)
    {
        if (escaped)
        {
            if (ch == 't')
                out += '\t';
            else if (ch == 'n')
                out += '\n';
            else
                out += ch;
            escaped = false;
            continue;
        }
        if (ch == '\\')
        {
            escaped = true;
            continue;
        }
        out += ch;
    }
    if (escaped)
        out += '\\';
    return out;
}

static bool LoadEvidenceCategoryOverridesLocal(ManualTestContext& context)
{
    std::string text;
    const std::filesystem::path path = EvidenceCategoryOverridesPathLocal();
    if (!ReadTextFile(path.string(), text))
        return false;

    std::istringstream input(text);
    std::string line;
    while (std::getline(input, line))
    {
        if (line.empty() || line[0] == '#')
            continue;
        const std::size_t tab = line.find('\t');
        if (tab == std::string::npos)
            continue;
        // std::getline removes '\n' but preserves the '\r' in CRLF files.
        // Trim both fields so exact category comparisons do not receive
        // "To Verify\r" / "Process Validation\r" and silently miss every
        // curated override on Windows.
        const std::string key = TrimLine(
            UnescapeEvidenceOverrideFieldLocal(line.substr(0, tab)));
        const std::string category = TrimLine(
            UnescapeEvidenceOverrideFieldLocal(line.substr(tab + 1)));
        if (!key.empty() && !category.empty())
            context.evidence_category_overrides[key] = category;
    }
    return true;
}

static bool SaveEvidenceCategoryOverridesLocal(
    const ManualTestContext& context,
    std::string& reason)
{
    const std::filesystem::path path = EvidenceCategoryOverridesPathLocal();
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec)
    {
        reason = "failed to create Evidence category override directory: " +
            path.parent_path().string() + " reason=" + ec.message();
        return false;
    }

    std::vector<std::pair<std::string, std::string>> entries(
        context.evidence_category_overrides.begin(),
        context.evidence_category_overrides.end());
    std::stable_sort(
        entries.begin(),
        entries.end(),
        [](const auto& left, const auto& right)
        {
            return left.first < right.first;
        });

    std::ostringstream out;
    out << "# Evidence Chain manual category overrides\n";
    out << "# key<TAB>category\n";
    for (const auto& entry : entries)
    {
        out << EscapeEvidenceOverrideFieldLocal(entry.first)
            << '\t'
            << EscapeEvidenceOverrideFieldLocal(entry.second)
            << '\n';
    }

    if (!WriteTextFile(path, out.str()))
    {
        reason = "failed to write Evidence category overrides: " + path.string();
        return false;
    }
    reason = "Evidence category overrides saved: " + path.string();
    return true;
}

static std::string InferEvidenceChainToolBucketLocal(
    const std::string& tool,
    const std::string& scriptId,
    const std::string& scriptPath,
    const std::string& label,
    const std::string& status = {},
    const std::string& reason = {},
    const std::string& parameterSummary = {})
{
    const std::string normalizedTool = NormalizeEvidenceToolTypeLocal(tool);
    const std::string key = BuildEvidenceClassificationKeyLocal(
        tool,
        scriptId,
        scriptPath,
        label,
        status,
        reason,
        parameterSummary);

    if (normalizedTool == "FindSegmentation" ||
        key.find("find_segmentation") != std::string::npos ||
        key.find("findsegmentation") != std::string::npos ||
        key.find("edgesam") != std::string::npos)
    {
        return "FindSegmentation Prompt / EdgeSam";
    }

    if (normalizedTool == "TorchTask" ||
        key.find("torch") != std::string::npos ||
        key.find("deeplab") != std::string::npos ||
        key.find("yolo") != std::string::npos)
    {
        if (key.find("detect") != std::string::npos ||
            key.find("yolo") != std::string::npos)
        {
            return "Torch Detection - Model Unverified";
        }
        if (key.find("segment") != std::string::npos ||
            key.find("mask") != std::string::npos ||
            key.find("deeplab") != std::string::npos)
        {
            return "Torch Segmentation - Runtime Smoke";
        }
        return "Torch / Model Validation";
    }

    return normalizedTool.empty() ? (label.empty() ? "Other" : label) : normalizedTool;
}

static std::string ResolveEvidenceImagePathByIdFromDiskLocal(
    const std::string& imageId)
{
    if (imageId.empty() || imageId.rfind("fallback_image_", 0) == 0)
        return {};

    const std::filesystem::path root =
        ResolveCxVisionRunPath("test_images");
    std::error_code ec;
    if (!std::filesystem::is_directory(root, ec))
        return {};

    static const char* exts[] = {
        ".jpg", ".jpeg", ".png", ".bmp", ".tif", ".tiff"
    };

    std::filesystem::recursive_directory_iterator it(
        root,
        std::filesystem::directory_options::skip_permission_denied,
        ec);
    const std::filesystem::recursive_directory_iterator end;
    for (; !ec && it != end; it.increment(ec))
    {
        if (!it->is_regular_file(ec))
            continue;
        const std::filesystem::path path = it->path();
        const std::string stem = path.stem().string();
        if (stem != imageId)
            continue;
        const std::string ext = path.extension().string();
        std::string lowerExt = ext;
        std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        for (const char* allowed : exts)
        {
            if (lowerExt == allowed)
                return path.string();
        }
    }
    return {};
}

static bool SelectEvidenceImageFileFromDialogLocal(
    std::string& outPath,
    std::string& reason)
{
    outPath.clear();
    reason =
        "file dialog is disabled in this build; use Bind Current Image View "
        "or Use First Manifest Image";
    return false;
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

static std::string EnsureFindLineSelectedEdgeStatementLocal(
    const std::string& tool,
    const std::string& scriptText)
{
    if (NormalizeEvidenceToolTypeLocal(tool) != "FindLine")
        return scriptText;
    if (scriptText.find("setselectedgenum") != std::string::npos)
        return scriptText;
    if (scriptText.find("FindLine") == std::string::npos &&
        scriptText.find("Findline") == std::string::npos)
    {
        return scriptText;
    }

    const std::string statement =
        "m_line.setselectedgenum(global_findline_selected_edge);\n";
    std::string migrated = scriptText;
    std::size_t insertAt = migrated.find("m_line.measure");
    if (insertAt == std::string::npos)
        insertAt = migrated.find("m_line.measureRobust");
    if (insertAt == std::string::npos)
    {
        if (!migrated.empty() && migrated.back() != '\n')
            migrated += "\n";
        migrated += statement;
        return migrated;
    }

    migrated.insert(insertAt, statement);
    return migrated;
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

static bool ReadJsonBoolFieldLocal(
    const std::string& text,
    const std::string& key,
    bool& outValue)
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

    if (text.compare(begin, 4, "true") == 0)
    {
        outValue = true;
        return true;
    }
    if (text.compare(begin, 5, "false") == 0)
    {
        outValue = false;
        return true;
    }
    return false;
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

static bool ApplyCandidateRuntimeGlobalsLocal(
    ManualTestContext& context,
    const std::string& runtimeGlobalsPath,
    std::string& reason)
{
    reason.clear();
    std::string text;
    if (!ReadTextFile(runtimeGlobalsPath, text))
    {
        reason = "failed to read candidate runtime globals: " + runtimeGlobalsPath;
        return false;
    }

    const std::size_t globalsKey = text.find("\"globals\"");
    const std::size_t objectBegin =
        globalsKey == std::string::npos ? std::string::npos : text.find('{', globalsKey);
    const std::size_t objectEnd =
        objectBegin == std::string::npos ? std::string::npos : text.find('}', objectBegin);
    if (objectBegin == std::string::npos || objectEnd == std::string::npos)
    {
        reason = "candidate runtime_globals.json has no globals object";
        return false;
    }

    std::size_t pos = objectBegin + 1;
    int applied = 0;
    while (pos < objectEnd)
    {
        const std::size_t keyBegin = text.find('"', pos);
        if (keyBegin == std::string::npos || keyBegin >= objectEnd)
            break;
        const std::size_t keyEnd = text.find('"', keyBegin + 1);
        if (keyEnd == std::string::npos || keyEnd >= objectEnd)
            break;
        const std::string key = text.substr(keyBegin + 1, keyEnd - keyBegin - 1);
        const std::size_t colon = text.find(':', keyEnd + 1);
        if (colon == std::string::npos || colon >= objectEnd)
            break;

        std::size_t valueBegin = colon + 1;
        while (valueBegin < objectEnd &&
               std::isspace(static_cast<unsigned char>(text[valueBegin])))
            ++valueBegin;
        std::size_t valueEnd = valueBegin;
        if (valueEnd < objectEnd && (text[valueEnd] == '-' || text[valueEnd] == '+'))
            ++valueEnd;
        while (valueEnd < objectEnd &&
               std::isdigit(static_cast<unsigned char>(text[valueEnd])))
            ++valueEnd;

        if (key.rfind("global_", 0) == 0 && valueEnd > valueBegin)
        {
            try
            {
                context.runtime_int_vars[key] =
                    std::stoi(text.substr(valueBegin, valueEnd - valueBegin));
                ++applied;
            }
            catch (...)
            {
                reason = "invalid candidate runtime global: " + key;
                return false;
            }
        }
        pos = valueEnd > valueBegin ? valueEnd : colon + 1;
    }

    if (applied == 0)
    {
        reason = "candidate runtime_globals.json contains no global_* values";
        return false;
    }

    auto getRuntimeInt = [&](const std::string& key, int fallback) -> int
    {
        const auto found = context.runtime_int_vars.find(key);
        return found == context.runtime_int_vars.end() ? fallback : found->second;
    };

    context.findline_scan_edge_count =
        std::max(1, std::min(16, getRuntimeInt("global_findline_edge_count", 4)));
    context.findline_selected_scan_edge =
        std::max(0,
                 std::min(
                     getRuntimeInt("global_findline_selected_edge", 0),
                     context.findline_scan_edge_count));
    context.findline_edge_params.resize(
        static_cast<std::size_t>(context.findline_scan_edge_count + 1));
    for (int edge = 1; edge <= context.findline_scan_edge_count; ++edge)
    {
        ManualFindLineEdgeParamState& params =
            context.findline_edge_params[static_cast<std::size_t>(edge)];
        const std::string prefix =
            "global_findline_edge" + std::to_string(edge) + "_";
        params.initialized = true;
        params.threshold = getRuntimeInt(prefix + "threshold",
                                         getRuntimeInt("global_threshold", 20));
        params.method = getRuntimeInt(prefix + "method",
                                      getRuntimeInt("global_method", 0));
        params.linegap = getRuntimeInt(prefix + "linegap",
                                       getRuntimeInt("global_linegap", 6));
        params.wgap = getRuntimeInt(prefix + "wgap",
                                    getRuntimeInt("global_wgap", 8));
        params.hgap = getRuntimeInt(prefix + "hgap",
                                    getRuntimeInt("global_hgap", 32));
        params.filterprofile =
            getRuntimeInt(prefix + "filterprofile",
                          getRuntimeInt("global_filterprofile", 0));
    }
    return true;
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
        gauge.inner_radius = getInt("global_circle_inner_radius", 0);
        gauge.outer_radius = getInt("global_circle_outer_radius", gauge.radius);
        if (gauge.outer_radius <= 0)
            gauge.outer_radius = gauge.radius;
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
        << " circle_inner_radius=" << getInt("global_circle_inner_radius", 0)
        << " circle_outer_radius=" << getInt("global_circle_outer_radius", 0)
        << " circle_ring_width=" << getInt("global_circle_ring_width", 0)
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
            findGroup(tool + " / Needs Human Setting");
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

static std::filesystem::path ManualGuidanceQueuePathLocal()
{
    return ResolveCxVisionRunPath(
        "cxscript_runs/evidence_chain/manual_guidance_queue.tsv");
}

static std::vector<std::string> SplitEvidenceTsvRowLocal(
    const std::string& line)
{
    std::vector<std::string> cells;
    std::string current;
    for (char ch : line)
    {
        if (ch == '\t')
        {
            cells.push_back(current);
            current.clear();
            continue;
        }
        if (ch != '\r')
            current += ch;
    }
    cells.push_back(current);
    return cells;
}

static std::string BuildManualGuidanceGlobalsSummaryLocal(
    const std::string& candidateDir)
{
    if (candidateDir.empty())
        return {};

    std::string text;
    const std::filesystem::path globalsPath =
        std::filesystem::path(candidateDir) / "globals.txt";
    if (!ReadTextFile(globalsPath.string(), text))
        return {};

    std::ostringstream summary;
    std::istringstream input(text);
    std::string line;
    bool first = true;
    while (std::getline(input, line))
    {
        line = TrimLine(line);
        if (line.rfind("global_", 0) != 0 ||
            line.find('=') == std::string::npos)
        {
            continue;
        }
        if (!first)
            summary << ' ';
        summary << line;
        first = false;
    }
    return summary.str();
}

static void AppendManualGuidanceQueueLocal(
    ManualTestContext& context,
    const std::function<std::string(const std::string&)>& resolveImagePath,
    const std::function<ScriptEvidenceGroup&(const std::string&)>& findGroup)
{
    const std::filesystem::path queuePath = ManualGuidanceQueuePathLocal();
    std::string text;
    if (!ReadTextFile(queuePath.string(), text))
        return;

    std::istringstream input(text);
    std::string line;
    bool headerSeen = false;
    while (std::getline(input, line))
    {
        if (line.empty() || line[0] == '#')
            continue;

        const std::vector<std::string> cells = SplitEvidenceTsvRowLocal(line);
        if (!headerSeen)
        {
            headerSeen = true;
            if (!cells.empty() && cells[0] == "case_id")
                continue;
        }

        if (cells.size() < 9)
            continue;

        ScriptEvidenceThumb thumb;
        thumb.case_id = cells[0];
        thumb.tool = NormalizeEvidenceToolTypeLocal(cells[1]);
        thumb.image_id = cells[2];
        thumb.target_id = cells[3];
        thumb.candidate_id = cells[4];
        thumb.status = "pending_human_guidance";
        thumb.reason = cells[5];
        thumb.parameter_summary = cells[6];
        thumb.script_path = cells[7];
        thumb.script_id = thumb.case_id;
        thumb.image_path = resolveImagePath(thumb.image_id);
        thumb.thumbnail_path = cells[8];
        thumb.evidence_category_override = "To Verify";
        if (cells.size() > 10)
            thumb.candidate_dir = cells[10];

        // A manual-guidance algorithm package is evidence, not automatically
        // a restorable GUI working revision.  Only advertise saved state when
        // the complete transactional restore package exists.  The current
        // propagation cases contain globals.txt/result assets but no working
        // script/gauge snapshot, so they must open the declared frozen script
        // and restore its locked parameters instead of aborting on a missing
        // script_snapshot_path.
        if (!thumb.candidate_dir.empty())
        {
            const std::filesystem::path candidateRoot(thumb.candidate_dir);
            const std::filesystem::path scriptSnapshot =
                candidateRoot / "script_snapshot.cxsc";
            const std::filesystem::path runtimeGlobals =
                candidateRoot / "runtime_globals.json";
            const std::filesystem::path gaugeAnnotation =
                candidateRoot / "gauge_annotation.json";
            if (std::filesystem::is_regular_file(scriptSnapshot) &&
                std::filesystem::is_regular_file(runtimeGlobals) &&
                std::filesystem::is_regular_file(gaugeAnnotation))
            {
                thumb.working_script_snapshot_path = scriptSnapshot.string();
                thumb.runtime_globals_path = runtimeGlobals.string();
                thumb.gauge_annotation_path = gaugeAnnotation.string();
                thumb.has_saved_state = true;
            }

            const std::string lockedGlobals =
                BuildManualGuidanceGlobalsSummaryLocal(thumb.candidate_dir);
            if (!lockedGlobals.empty())
            {
                if (!thumb.parameter_summary.empty())
                    thumb.parameter_summary += "; ";
                thumb.parameter_summary += lockedGlobals;
            }
        }

        // The queue's historical source_script column contains the runtime
        // result summary for these generated cases.  The editable source is
        // the explicit cxscript path above; keep that identity traceable.
        thumb.source_evidence_script_path = thumb.script_path;

        if (thumb.image_path.empty())
            thumb.image_path = ResolveOriginalImagePathFromEvidencePacketLocal(
                thumb.parameter_summary);

        PopulateEditableObjectBindingForThumbLocal(thumb);

        ScriptEvidenceGroup& group =
            findGroup(thumb.tool + " / Needs Human Setting");
        bool exists = false;
        for (auto& existing : group.thumbs)
        {
            if (existing.case_id == thumb.case_id &&
                existing.target_id == thumb.target_id)
            {
                /*
                 * The algorithm-review handoff is loaded before the manual
                 * guidance queue and may already have inserted the same case
                 * as pending_algorithm_review.  The queue is the curated
                 * operator work list, so it must replace the earlier row
                 * instead of being dropped by de-duplication; otherwise the
                 * To Verify bucket becomes 0 even though the queue contains
                 * the three expected human-guided cases.
                 */
                existing = thumb;
                exists = true;
                break;
            }
        }
        if (!exists)
            group.thumbs.push_back(std::move(thumb));
    }
}

static void WriteEvidenceChainLoadedElementsDebugLocal(
    const ManualTestContext& context)
{
    auto escapeTsv = [](std::string value) -> std::string
    {
        for (char& ch : value)
        {
            if (ch == '\t' || ch == '\r' || ch == '\n')
                ch = ' ';
        }
        return value;
    };

    int totalThumbs = 0;
    int guidanceThumbs = 0;
    int toVerifyOverrideThumbs = 0;
    int processOverrideThumbs = 0;
    int displayToVerifyThumbs = 0;
    int displayProcessThumbs = 0;

    std::ostringstream rows;
    rows << "group_label\tcase_id\timage_id\ttarget_id\ttool\tcandidate_id\t"
         << "status\toverride\tdisplay_major\tdisplay_priority\tis_candidate\t"
         << "has_saved_state\tscript_id\tscript_path\tthumbnail_path\treason\n";

    for (const auto& group : context.script_evidence_groups)
    {
        for (const auto& thumb : group.thumbs)
        {
            ++totalThumbs;
            const std::string resolvedOverride =
                ResolveEvidenceCategoryOverrideLocal(context, thumb);
            if (thumb.status == "pending_human_guidance")
                ++guidanceThumbs;
            if (resolvedOverride == "To Verify")
                ++toVerifyOverrideThumbs;
            if (resolvedOverride == "Process Validation")
                ++processOverrideThumbs;
            const auto displayMajor =
                ClassifyEvidenceMajorBucketLocal(context, thumb, group.label);
            if (displayMajor.second == "To Verify")
                ++displayToVerifyThumbs;
            if (displayMajor.second == "Process Validation")
                ++displayProcessThumbs;

            rows << escapeTsv(group.label) << '\t'
                 << escapeTsv(thumb.case_id) << '\t'
                 << escapeTsv(thumb.image_id) << '\t'
                 << escapeTsv(thumb.target_id) << '\t'
                 << escapeTsv(thumb.tool) << '\t'
                 << escapeTsv(thumb.candidate_id) << '\t'
                 << escapeTsv(thumb.status) << '\t'
                 << escapeTsv(resolvedOverride) << '\t'
                 << escapeTsv(displayMajor.second) << '\t'
                 << displayMajor.first << '\t'
                 << (thumb.is_candidate ? "1" : "0") << '\t'
                 << (thumb.has_saved_state ? "1" : "0") << '\t'
                 << escapeTsv(thumb.script_id) << '\t'
                 << escapeTsv(thumb.script_path) << '\t'
                 << escapeTsv(thumb.thumbnail_path) << '\t'
                 << escapeTsv(thumb.reason) << '\n';
        }
    }

    const std::filesystem::path detailPath = ResolveCxVisionRunPath(
        "cxscript_runs/evidence_chain/"
        "evidence_chain_loaded_elements_debug.tsv");
    WriteTextFile(detailPath, rows.str());

    std::ostringstream summary;
    summary << "{\n"
            << "  \"groups\": " << context.script_evidence_groups.size() << ",\n"
            << "  \"thumbs\": " << totalThumbs << ",\n"
            << "  \"pending_human_guidance\": " << guidanceThumbs << ",\n"
            << "  \"to_verify_override\": " << toVerifyOverrideThumbs << ",\n"
            << "  \"process_validation_override\": " << processOverrideThumbs << ",\n"
            << "  \"display_to_verify\": " << displayToVerifyThumbs << ",\n"
            << "  \"display_process_validation\": " << displayProcessThumbs << ",\n"
            << "  \"manual_guidance_queue\": \""
            << ManualGuidanceQueuePathLocal().string() << "\",\n"
            << "  \"category_overrides\": \""
            << EvidenceCategoryOverridesPathLocal().string() << "\"\n"
            << "}\n";
    const std::filesystem::path summaryPath = ResolveCxVisionRunPath(
        "cxscript_runs/evidence_chain/"
        "evidence_chain_loaded_summary_debug.json");
    WriteTextFile(summaryPath, summary.str());

    CXLOG_INFO(
        "EvidenceChain",
        "evidence_chain_ui_loaded_elements",
        "snapshot_written",
        "groups=" + std::to_string(context.script_evidence_groups.size()) +
        " thumbs=" + std::to_string(totalThumbs) +
        " pending_human_guidance=" + std::to_string(guidanceThumbs) +
        " to_verify_override=" + std::to_string(toVerifyOverrideThumbs) +
        " process_validation_override=" +
        std::to_string(processOverrideThumbs) +
        " display_to_verify=" + std::to_string(displayToVerifyThumbs) +
        " display_process_validation=" +
        std::to_string(displayProcessThumbs) +
        " detail=" + detailPath.string() +
        " summary=" + summaryPath.string());
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

static void AppendSavedEvidenceCandidatesLocal(
    ManualTestContext& context,
    const std::function<ScriptEvidenceGroup&(const std::string&)>& findGroup)
{
    // New writes use the stable project run root.  Do not read the process
    // working-directory root here: when the GUI is started from build/Release
    // it pulls stale build-local candidate packages into the manual review
    // list and can re-trigger old parser/image binding states.
    std::vector<std::filesystem::path> roots;
    roots.push_back(ResolveCxVisionRunPath("cxscript_runs/evidence_candidates"));
    // Builds prior to the stable project run-root change wrote GUI candidates
    // below <repo>/cxscript_runs.  Resolve that location independently of the
    // process working directory so an upgraded binary can still restore the
    // user's latest saved working revision.
    const std::filesystem::path repoLocalRoot =
        (ResolveCaseDirectory(".") / "cxscript_runs/evidence_candidates")
            .lexically_normal();
    if (repoLocalRoot != roots.front().lexically_normal())
        roots.push_back(repoLocalRoot);
    std::error_code ec;

    std::vector<std::filesystem::path> bindings;
    for (const auto& root : roots)
    {
        ec.clear();
        if (!std::filesystem::is_directory(root, ec))
            continue;

        std::filesystem::recursive_directory_iterator it(
            root,
            std::filesystem::directory_options::skip_permission_denied,
            ec);
        const std::filesystem::recursive_directory_iterator end;
        for (; !ec && it != end; it.increment(ec))
        {
            if (it->is_regular_file(ec) &&
                it->path().filename() == "evidence_binding.json")
                bindings.push_back(it->path());
        }
    }

    std::stable_sort(
        bindings.begin(),
        bindings.end(),
        [](const std::filesystem::path& left,
           const std::filesystem::path& right)
        {
            std::error_code leftEc;
            std::error_code rightEc;
            const auto leftTime = std::filesystem::last_write_time(left, leftEc);
            const auto rightTime = std::filesystem::last_write_time(right, rightEc);
            if (!leftEc && !rightEc && leftTime != rightTime)
                return leftTime < rightTime;
            return left.string() < right.string();
        });

    auto bindWorkingRevisionToOriginal =
        [&](const ScriptEvidenceThumb& candidate)
    {
        const std::filesystem::path resolvedSource =
            ResolveWorkspaceFile(candidate.source_evidence_script_path)
                .lexically_normal();
        for (auto& group : context.script_evidence_groups)
        {
            for (auto& original : group.thumbs)
            {
                if (original.is_candidate)
                    continue;
                const bool sameCase = !candidate.case_id.empty() &&
                    original.case_id == candidate.case_id;
                const bool sameScript =
                    !candidate.source_evidence_script_path.empty() &&
                    ResolveWorkspaceFile(original.script_path).lexically_normal() ==
                        resolvedSource;
                const bool identityMatches = !candidate.case_id.empty()
                    ? (sameCase &&
                       (candidate.source_evidence_script_path.empty() || sameScript))
                    : sameScript;
                if (!identityMatches)
                    continue;

                original.candidate_id = candidate.candidate_id;
                original.candidate_dir = candidate.candidate_dir;
                original.evidence_binding_path =
                    candidate.evidence_binding_path;
                original.parameter_snapshot_path =
                    candidate.parameter_snapshot_path;
                original.runtime_globals_path =
                    candidate.runtime_globals_path;
                original.gauge_annotation_path =
                    candidate.gauge_annotation_path;
                original.working_script_snapshot_path =
                    candidate.script_path;
                original.has_saved_state = true;
                original.source_evidence_script_path =
                    candidate.source_evidence_script_path;
                original.parameter_summary = candidate.parameter_summary;
                original.status = candidate.status;
                original.reason =
                    "active working revision=" + candidate.candidate_id +
                    "; restored from candidate binding; " +
                    candidate.reason;
                return;
            }
        }
    };

    for (const auto& bindingPath : bindings)
    {

        std::string binding;
        if (!ReadTextFile(bindingPath.string(), binding))
            continue;

        const std::string candidateId =
            ReadJsonStringFieldLocal(binding, "candidate_id");
        const std::string caseId = ReadJsonStringFieldLocal(binding, "case_id");
        const std::string originalScriptId =
            ReadJsonStringFieldLocal(binding, "script_id");
        const std::string scriptSnapshot =
            ReadJsonStringFieldLocal(binding, "script_snapshot_path");
        const std::string imagePath =
            ReadJsonStringFieldLocal(binding, "image_path");
        if (candidateId.empty() || caseId.empty() || scriptSnapshot.empty() ||
            imagePath.empty())
            continue;

        bool gaugeAccepted = false;
        std::string gaugeReviewStatus;
        {
            const std::string gaugePath =
                ReadJsonStringFieldLocal(binding, "gauge_annotation_path");
            std::string gaugeText;
            if (!gaugePath.empty() && ReadTextFile(gaugePath, gaugeText))
            {
                ReadJsonBoolFieldLocal(gaugeText, "accepted", gaugeAccepted);
                gaugeReviewStatus =
                    ReadJsonStringFieldLocal(gaugeText, "review_status");
            }
        }
        const bool humanConfirmed =
            gaugeAccepted && gaugeReviewStatus == "manual_accepted";

        ScriptEvidenceThumb thumb;
        thumb.is_candidate = true;
        thumb.candidate_id = candidateId;
        thumb.candidate_dir = bindingPath.parent_path().string();
        thumb.evidence_binding_path = bindingPath.string();
        thumb.parameter_snapshot_path =
            ReadJsonStringFieldLocal(binding, "parameter_snapshot_path");
        thumb.runtime_globals_path =
            ReadJsonStringFieldLocal(binding, "runtime_globals_path");
        thumb.gauge_annotation_path =
            ReadJsonStringFieldLocal(binding, "gauge_annotation_path");
        thumb.working_script_snapshot_path = scriptSnapshot;
        thumb.has_saved_state = true;
        thumb.source_evidence_script_path =
            ReadJsonStringFieldLocal(binding, "source_evidence_script_path");
        if (thumb.source_evidence_script_path.empty())
            thumb.source_evidence_script_path =
                ReadJsonStringFieldLocal(binding, "script_path");
        thumb.case_id = caseId;
        thumb.script_id =
            (originalScriptId.empty() ? std::string("candidate") : originalScriptId) +
            " [" + candidateId + "]";
        thumb.script_path = scriptSnapshot;
        thumb.image_id = ReadJsonStringFieldLocal(binding, "image_id");
        thumb.image_path = imagePath;
        thumb.thumbnail_path = imagePath;
        thumb.target_id = ReadJsonStringFieldLocal(binding, "target_id");
        thumb.tool = ReadJsonStringFieldLocal(binding, "tool");
        thumb.parameter_summary =
            ReadJsonStringFieldLocal(binding, "parameter_summary");
        thumb.status = humanConfirmed
            ? "manual_confirmed_candidate"
            : "pending_human_review";
        thumb.reason =
            std::string(humanConfirmed
                ? "manual accepted evidence candidate"
                : "saved evidence candidate pending manual acceptance") +
            "; candidate_id=" + candidateId +
            "; candidate_dir=" + thumb.candidate_dir +
            "; gauge_accepted=" + (gaugeAccepted ? "true" : "false") +
            "; gauge_review_status=" + gaugeReviewStatus;

        std::string analysis;
        const std::filesystem::path analysisPath =
            bindingPath.parent_path() / "analysis_state.json";
        if (ReadTextFile(analysisPath.string(), analysis))
        {
            thumb.primary_object_type =
                ReadJsonStringFieldLocal(analysis, "primary_object_type");
            thumb.primary_object_name =
                ReadJsonStringFieldLocal(analysis, "primary_object_name");
            thumb.primary_object_status =
                ReadJsonStringFieldLocal(analysis, "primary_object_status");
        }
        PopulateEditableObjectBindingForThumbLocal(thumb);
        bindWorkingRevisionToOriginal(thumb);

        const std::string candidateTool = NormalizeEvidenceToolTypeLocal(thumb.tool);
        ScriptEvidenceGroup& group = findGroup(
            (candidateTool.empty() ? std::string("Unknown") : candidateTool) +
            (humanConfirmed
                ? " / Human Confirmed Candidates"
                : " / Saved Pending Candidates"));
        bool exists = false;
        for (const auto& existing : group.thumbs)
        {
            if (existing.is_candidate &&
                existing.candidate_id == thumb.candidate_id &&
                existing.case_id == thumb.case_id)
            {
                exists = true;
                break;
            }
        }
        if (!exists)
            group.thumbs.push_back(std::move(thumb));
    }
}

void ViewController::EnsureCxScriptWorkbenchAssetsLoaded()
{
  if (m_manualTest.script_evidence_groups_dirty == false)
    return;

  LoadEvidenceCategoryOverridesLocal(m_manualTest);

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
    group.label = InferEvidenceChainToolBucketLocal(
        tool,
        scriptId,
        scriptPath,
        tool.empty() ? (scriptId.empty() ? "unknown" : scriptId) : tool);
    m_manualTest.script_evidence_groups.push_back(group);
    return m_manualTest.script_evidence_groups.back();
  };

  for (const auto& item : m_manualTest.evidence_items)
  {
    if (item.script_id.empty())
      continue;

    const std::string scriptPath = ResolveCatalogScriptPathById(item.script_id);
    const std::string groupLabel = InferEvidenceChainToolBucketLocal(
        item.tool,
        item.script_id,
        scriptPath,
        item.tool,
        item.probe_status.empty() ? item.contract_status : item.probe_status,
        item.review_status,
        item.parameter_profile_id);
    ScriptEvidenceGroup& group =
        findOrCreateGroup(item.script_id, scriptPath, groupLabel);

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

    const std::string normalizedTool =
        NormalizeEvidenceToolTypeLocal(entry.tool);
    const bool isSmokeEvidence =
        entry.expected_result == "smoke" &&
        (normalizedTool == "TorchTask" ||
         entry.path.find("/torch/") != std::string::npos ||
         entry.path.find("\\torch\\") != std::string::npos);
    bool isVisible = entry.manual_visible && entry.frozen &&
        (entry.expected_result == "ok" ||
         entry.expected_result == "ng_expected" ||
         isSmokeEvidence);
    if (!isVisible) continue;

    if (hasThumbForScript(entry.script_id, entry.path))
      continue;

    const std::string groupLabel = InferEvidenceChainToolBucketLocal(
        entry.tool,
        entry.script_id,
        entry.path,
        entry.label,
        entry.expected_status,
        entry.expected_policy_guard,
        entry.parameter_policy_id);
    ScriptEvidenceGroup& group =
        findOrCreateGroup(entry.script_id, entry.path, groupLabel);

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

  AppendManualGuidanceQueueLocal(
      m_manualTest,
      [this](const std::string& imageId) -> std::string
      {
        return ResolveImagePathFromManifest(imageId);
      },
      [&](const std::string& label) -> ScriptEvidenceGroup&
      {
        return findOrCreateGroup("", "", label);
      });

  AppendSavedEvidenceCandidatesLocal(
      m_manualTest,
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
      group.label = InferEvidenceChainToolBucketLocal(
          item.type,
          item.name,
          item.path,
          item.type.empty() ? "script" : item.type,
          item.status,
          item.description,
          "");

      ScriptEvidenceThumb thumb;
      thumb.script_id = item.name;
      thumb.script_path = item.path;
      thumb.tool = group.label;
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

      if (thumb.parameter_summary.empty() ||
          thumb.parameter_summary.find('=') == std::string::npos)
      {
        thumb.parameter_summary =
            BuildDefaultEvidenceParamSummaryForScript(item.path);
      }

      AssignFallbackImageToThumb(thumb, fallbackImages, fallbackImageIndex++);
      PopulateEditableObjectBindingForThumbLocal(thumb);

      group.thumbs.push_back(thumb);
      m_manualTest.script_evidence_groups.push_back(group);
    }
  }

  WriteEvidenceChainLoadedElementsDebugLocal(m_manualTest);

  m_manualTest.script_evidence_groups_dirty = false;
  m_manualTest.script_evidence_row_refs_dirty = true;
}

bool ViewController::WriteEvidenceChainCatalogSemanticSelfTest(
    const std::string& outDir,
    std::string& reason)
{
  reason.clear();

  EnsureCxScriptWorkbenchAssetsLoaded();

  struct ExpectedBucket
  {
    const char* label;
    const char* semantic_status;
  };

  const ExpectedBucket expected[] = {
      {"Torch / Model Validation", "FLOW_SMOKE_ONLY"},
      {"Torch Detection - Model Unverified", "DETECTION_NON_EMPTY_RESULT_UNVERIFIED"},
      {"Torch Segmentation - Runtime Smoke", "RUNTIME_SMOKE_ONLY"},
      {"FindSegmentation Prompt / EdgeSam", "PROMPT_EDGESAM_PENDING_BINDING"}};

  struct BucketSummary
  {
    std::string label;
    std::string semantic_status;
    int group_count = 0;
    int script_count = 0;
    std::vector<std::string> scripts;
  };

  std::vector<BucketSummary> summaries;
  for (const ExpectedBucket& e : expected)
  {
    BucketSummary s;
    s.label = e.label;
    s.semantic_status = e.semantic_status;
    summaries.push_back(s);
  }

  auto findSummary = [&](const std::string& label) -> BucketSummary*
  {
    for (auto& summary : summaries)
    {
      if (summary.label == label)
        return &summary;
    }
    return nullptr;
  };

  for (const ScriptEvidenceGroup& group : m_manualTest.script_evidence_groups)
  {
    BucketSummary* summary = findSummary(group.label);
    if (summary == nullptr)
      continue;

    ++summary->group_count;
    summary->script_count += static_cast<int>(group.thumbs.size());
    for (const ScriptEvidenceThumb& thumb : group.thumbs)
    {
      std::string script = thumb.script_id.empty()
          ? thumb.script_path
          : thumb.script_id;
      if (script.empty())
        script = "(unnamed)";
      if (std::find(summary->scripts.begin(), summary->scripts.end(), script) ==
          summary->scripts.end())
      {
        summary->scripts.push_back(script);
      }
    }
  }

  bool allPresent = true;
  for (const BucketSummary& summary : summaries)
  {
    if (summary.script_count <= 0)
      allPresent = false;
  }

  const std::filesystem::path root(outDir.empty()
      ? "cxscript_runs/evidence_selftest/catalog_semantics"
      : outDir);
  std::filesystem::create_directories(root);

  const std::filesystem::path jsonPath =
      root / "evidence_chain_catalog_semantics.json";
  const std::filesystem::path mdPath =
      root / "evidence_chain_catalog_semantics.md";

  auto escapeJson = [](const std::string& text) -> std::string
  {
    std::string out;
    for (char ch : text)
    {
      switch (ch)
      {
      case '\\': out += "\\\\"; break;
      case '"': out += "\\\""; break;
      case '\n': out += "\\n"; break;
      case '\r': break;
      case '\t': out += "\\t"; break;
      default: out += ch; break;
      }
    }
    return out;
  };

  {
    std::ofstream file(jsonPath, std::ios::binary);
    if (!file.is_open())
    {
      reason = "failed to write evidence chain catalog semantics json";
      return false;
    }

    file << "{\n";
    file << "  \"final_code\": \""
         << (allPresent ? "ASSET_PREFLIGHT_PASS" : "ASSET_PREFLIGHT_FAIL")
         << "\",\n";
    file << "  \"final_status\": \""
         << (allPresent ? "PASS" : "FAIL")
         << "\",\n";
    file << "  \"reason\": \"Torch evidence chain classification semantic check\",\n";
    file << "  \"manual_ui_panel_scope\": \"handled_by_other_thread\",\n";
    file << "  \"model_semantic_quality\": \"NOT_CLAIMED\",\n";
    file << "  \"detection_non_empty_result\": \"UNVERIFIED\",\n";
    file << "  \"findsegmentation_prompt_edgesam\": \"PENDING_BINDING\",\n";
    file << "  \"buckets\": [\n";
    for (std::size_t i = 0; i < summaries.size(); ++i)
    {
      const BucketSummary& summary = summaries[i];
      file << "    {\n";
      file << "      \"label\": \"" << escapeJson(summary.label) << "\",\n";
      file << "      \"semantic_status\": \"" << escapeJson(summary.semantic_status) << "\",\n";
      file << "      \"group_count\": " << summary.group_count << ",\n";
      file << "      \"script_count\": " << summary.script_count << ",\n";
      file << "      \"present\": " << (summary.script_count > 0 ? "true" : "false") << ",\n";
      file << "      \"scripts\": [";
      for (std::size_t si = 0; si < summary.scripts.size(); ++si)
      {
        if (si > 0)
          file << ", ";
        file << "\"" << escapeJson(summary.scripts[si]) << "\"";
      }
      file << "]\n";
      file << "    }";
      if (i + 1 < summaries.size())
        file << ",";
      file << "\n";
    }
    file << "  ]\n";
    file << "}\n";
  }

  {
    std::ofstream file(mdPath, std::ios::binary);
    if (!file.is_open())
    {
      reason = "failed to write evidence chain catalog semantics md";
      return false;
    }

    file << "# Evidence Chain Catalog Semantic Self Test\n\n";
    file << "- conclusion: `"
         << (allPresent ? "ASSET_PREFLIGHT_PASS" : "ASSET_PREFLIGHT_FAIL")
         << "`\n";
    file << "- manual_ui_panel_scope: `handled_by_other_thread`\n";
    file << "- model_semantic_quality: `NOT_CLAIMED`\n";
    file << "- detection_non_empty_result: `UNVERIFIED`\n";
    file << "- findsegmentation_prompt_edgesam: `PENDING_BINDING`\n\n";

    file << "| Bucket | Scripts | Semantic Status | Example Scripts |\n";
    file << "|---|---:|---|---|\n";
    for (const BucketSummary& summary : summaries)
    {
      file << "| " << summary.label
           << " | " << summary.script_count
           << " | " << summary.semantic_status
           << " | ";
      for (std::size_t i = 0; i < summary.scripts.size() && i < 4; ++i)
      {
        if (i > 0)
          file << "<br>";
        file << "`" << summary.scripts[i] << "`";
      }
      if (summary.scripts.empty())
        file << "`MISSING`";
      file << " |\n";
    }
  }

  reason = allPresent
      ? "evidence chain catalog semantic buckets are present: " +
        jsonPath.string()
      : "one or more Torch/FindSegmentation evidence chain buckets are missing: " +
        jsonPath.string();

  return allPresent;
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
  if ((thumb.texture_loaded && !thumb.texture_placeholder) ||
      thumb.texture_failed)
    return;

  if (m_manualTest.script_evidence_thumb_load_count_this_frame >=
      m_manualTest.script_evidence_thumb_load_budget_per_frame)
  {
    return;
  }

  if (thumb.image_path.empty() && !thumb.image_id.empty())
    thumb.image_path = ResolveImagePathFromManifest(thumb.image_id);
  if (thumb.image_path.empty() && !thumb.image_id.empty())
    thumb.image_path = ResolveEvidenceImagePathByIdFromDiskLocal(thumb.image_id);

  std::vector<std::string> previewCandidates;
  auto addPreviewCandidate = [&](const std::string& path)
  {
    if (path.empty())
      return;
    if (std::find(previewCandidates.begin(), previewCandidates.end(), path) ==
        previewCandidates.end())
    {
      previewCandidates.push_back(path);
    }
  };
  addPreviewCandidate(thumb.thumbnail_path);
  addPreviewCandidate(thumb.image_path);
  if (!thumb.image_id.empty())
    addPreviewCandidate(ResolveImagePathFromManifest(thumb.image_id));
  if (!thumb.image_id.empty())
    addPreviewCandidate(ResolveEvidenceImagePathByIdFromDiskLocal(thumb.image_id));

  if (previewCandidates.empty())
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
    thumb.texture_placeholder = thumb.texture_loaded;
    thumb.reason = "placeholder thumbnail generated; image path is empty";
    ++m_manualTest.script_evidence_thumb_load_count_this_frame;
    return;
  }

  cv::Mat image;
  std::string loadedPreviewPath;
  std::string lastFailure;
  for (const std::string& previewPath : previewCandidates)
  {
    if (!std::filesystem::exists(previewPath))
    {
      lastFailure = "thumbnail image not found: " + previewPath;
      continue;
    }

    image = cv::imread(previewPath, cv::IMREAD_COLOR);
    if (!image.empty())
    {
      loadedPreviewPath = previewPath;
      break;
    }
    lastFailure = "thumbnail image read failed: " + previewPath;
  }

  if (image.empty())
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
    thumb.texture_placeholder = thumb.texture_loaded;
    thumb.reason = lastFailure.empty()
        ? "thumbnail image unavailable"
        : lastFailure;
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
  thumb.texture_placeholder = false;
  if (thumb.texture_failed)
    thumb.reason = "failed to create thumbnail texture";
  else if (thumb.thumbnail_path != loadedPreviewPath)
    thumb.reason = "thumbnail loaded from image fallback: " + loadedPreviewPath;
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

    const bool isDeprecatedScript = IsDeprecatedCxScriptPath(scriptPath);

    if (thumb.script_id.empty() && scriptPath.empty())
    {
        reason = "evidence thumb has neither script_id nor script_path";
        return false;
    }

    out.valid = true;
    out.group_index = groupIndex;
    out.thumb_index = thumbIndex;

    out.case_id = thumb.case_id;

    out.candidate_id = thumb.candidate_id;
    out.candidate_dir = thumb.candidate_dir;
    out.evidence_binding_path = thumb.evidence_binding_path;
    out.parameter_snapshot_path = thumb.parameter_snapshot_path;
    out.runtime_globals_path = thumb.runtime_globals_path;
    out.gauge_annotation_path = thumb.gauge_annotation_path;
    out.working_script_snapshot_path = thumb.working_script_snapshot_path;
    out.is_candidate = thumb.is_candidate;
    out.has_saved_state = thumb.has_saved_state;
    out.source_evidence_script_path = thumb.source_evidence_script_path.empty()
        ? scriptPath
        : thumb.source_evidence_script_path;

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
    if (isDeprecatedScript)
    {
        if (out.status.empty())
            out.status = "legacy_script";
        if (out.reason.empty())
        {
            out.reason =
                "deprecated cxscript selected for viewing only; run/bind gates may reject it: " +
                scriptPath;
        }
    }
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

    CXLOG_INFO(
        "EvidenceChain",
        "evidence_selection_begin",
        "staging",
        "script_id=" + snapshot.script_id +
        " case_id=" + snapshot.case_id +
        " image_id=" + snapshot.image_id +
        " candidate=" + (snapshot.is_candidate ? "true" : "false") +
        " has_saved_state=" +
        (snapshot.has_saved_state ? "true" : "false") +
        " working_script=" + snapshot.working_script_snapshot_path +
        " load_image=" + (loadImageToView ? "true" : "false"));

    auto abortSelection = [&](const std::string& stage,
                              const std::string& message) -> bool
    {
        reason = message;
        CXLOG_ERROR(
            "EvidenceChain",
            "evidence_selection_abort",
            stage,
            "script_id=" + snapshot.script_id +
            " case_id=" + snapshot.case_id +
            " candidate_id=" + snapshot.candidate_id +
            " reason=" + message);
        return false;
    };

    CxEvidenceSelectionSnapshot resolved = snapshot;
    const bool loadWorkingRevision =
        !resolved.is_candidate && resolved.has_saved_state;
    if (loadWorkingRevision && resolved.working_script_snapshot_path.empty())
    {
        return abortSelection(
            "working_script_missing",
            "Evidence working revision is missing script_snapshot_path");
    }
    const std::string effectiveScriptPath = loadWorkingRevision
        ? resolved.working_script_snapshot_path
        : resolved.script_path;
    std::string scriptText;
    std::filesystem::path resolvedScriptPath;
    if (!effectiveScriptPath.empty())
    {
        resolvedScriptPath = ResolveWorkspaceFile(effectiveScriptPath);
        if (!ReadTextFile(resolvedScriptPath.string(), scriptText))
        {
            return abortSelection(
                "script_read",
                "failed to read evidence script: " +
                    resolvedScriptPath.string());
        }

        scriptText = EnsureFindLineSelectedEdgeStatementLocal(
            resolved.tool,
            scriptText);

        // The candidate/working script is the authoritative source for its
        // object declarations.  Do not reuse the baseline Evidence object's
        // declaration list after loading a saved script snapshot.
        if (resolved.is_candidate || loadWorkingRevision)
            resolved.editable_objects.clear();
        if (resolved.editable_objects.empty())
            AnalyzeEditableObjectsFromCxScriptLocal(
                scriptText,
                resolved.editable_objects);

        if (resolved.primary_object_name.empty() ||
            resolved.primary_object_status.empty() ||
            resolved.primary_object_status == "none" ||
            resolved.primary_object_status == "unresolved")
        {
            ResolvePrimaryEditableObjectLocal(
                resolved.tool,
                resolved.target_id,
                resolved.parameter_summary,
                resolved.editable_objects,
                resolved.primary_object_type,
                resolved.primary_object_name,
                resolved.primary_object_status);
        }
    }

    cv::Mat stagedImage;
    std::filesystem::path resolvedImagePath;
    if (loadImageToView)
    {
        std::string imagePathForLoad = snapshot.image_path;
        if (imagePathForLoad.empty() && !snapshot.image_id.empty())
            imagePathForLoad = ResolveImagePathFromManifest(snapshot.image_id);
        if (imagePathForLoad.empty() && !snapshot.image_id.empty())
            imagePathForLoad =
                ResolveEvidenceImagePathByIdFromDiskLocal(snapshot.image_id);

        if (imagePathForLoad.empty())
        {
            return abortSelection(
                "image_path",
                "selected evidence has empty image_path");
        }

        resolved.image_path = imagePathForLoad;
        resolvedImagePath = ResolveWorkspaceFile(imagePathForLoad);
        if (!std::filesystem::is_regular_file(resolvedImagePath))
        {
            return abortSelection(
                "image_file",
                "image file not found: " + resolvedImagePath.string());
        }

        stagedImage = cv::imread(resolvedImagePath.string(), cv::IMREAD_COLOR);
        if (stagedImage.empty())
        {
            return abortSelection(
                "image_decode",
                "failed to read image: " + resolvedImagePath.string());
        }
    }

    // Evidence selection is a transaction.  Build the complete Workbench
    // state in a value copy and publish it only after script, parameters,
    // gauge and optional image have all passed validation.
    ManualTestContext staged = m_manualTest;
    staged.current_evidence_selection = resolved;
    staged.selected_evidence_group = resolved.group_index;
    staged.selected_evidence_thumb = resolved.thumb_index;
    staged.active_case_id = resolved.case_id;
    staged.active_image_id = resolved.image_id;
    staged.active_target_id = resolved.target_id;
    staged.key_parameter_edit_revision = 0;
    staged.last_key_parameter_edit_summary =
        "evidence selection baseline: " + resolved.script_id;

    if (!resolved.image_path.empty())
        staged.image_file_path = resolved.image_path;

    if (!effectiveScriptPath.empty())
    {
        staged.editor_text = scriptText;
        staged.loaded_script_path = resolvedScriptPath.string();
        staged.script_file_path = resolvedScriptPath.string();
        staged.editor_source = resolved.is_candidate
            ? "evidence_candidate"
            : (loadWorkingRevision
                ? "evidence_working_revision"
                : "evidence");
        staged.editor_dirty = false;

        SeedDefaultManualGlobals(staged, effectiveScriptPath);
        staged.current_gauge.case_id = resolved.case_id;
        staged.current_gauge.image_id = resolved.image_id;
        staged.current_gauge.target_id = resolved.target_id;
        if (!resolved.tool.empty() && staged.current_gauge.tool.empty())
            staged.current_gauge.tool =
                NormalizeEvidenceToolTypeLocal(resolved.tool);
        staged.current_gauge.primary_object_type =
            resolved.primary_object_type;
        staged.current_gauge.primary_object_name =
            resolved.primary_object_name;
        staged.current_gauge.primary_object_status =
            resolved.primary_object_status.empty()
                ? "unresolved"
                : resolved.primary_object_status;
    }

    std::string parameterSource = "tool_defaults";
    if (resolved.is_candidate || loadWorkingRevision)
    {
        if (resolved.runtime_globals_path.empty() ||
            resolved.gauge_annotation_path.empty())
        {
            return abortSelection(
                "working_assets",
                "candidate is missing runtime_globals_path or gauge_annotation_path");
        }

        std::string restoreReason;
        if (!ApplyCandidateRuntimeGlobalsLocal(
                staged,
                resolved.runtime_globals_path,
                restoreReason))
        {
            return abortSelection("runtime_globals_restore", restoreReason);
        }
        if (!LoadManualGaugeWorkingCopyFromPath(
                staged,
                resolved.gauge_annotation_path,
                restoreReason))
        {
            return abortSelection(
                "gauge_restore",
                "failed to restore candidate gauge: " + restoreReason);
        }

        staged.current_gauge.source = resolved.is_candidate
            ? "evidence_candidate"
            : "evidence_working_revision";
        staged.current_gauge.case_id = resolved.case_id;
        staged.current_gauge.image_id = resolved.image_id;
        staged.current_gauge.target_id = resolved.target_id;
        if (staged.current_gauge.primary_object_type.empty())
            staged.current_gauge.primary_object_type =
                resolved.primary_object_type;
        if (staged.current_gauge.primary_object_name.empty())
            staged.current_gauge.primary_object_name =
                resolved.primary_object_name;
        if (staged.current_gauge.primary_object_status.empty() ||
            staged.current_gauge.primary_object_status == "unresolved")
        {
            staged.current_gauge.primary_object_status =
                resolved.primary_object_status.empty()
                    ? "restored_from_candidate"
                    : resolved.primary_object_status;
        }
        staged.current_gauge.dirty = false;
        staged.current_evidence_selection.primary_object_type =
            staged.current_gauge.primary_object_type;
        staged.current_evidence_selection.primary_object_name =
            staged.current_gauge.primary_object_name;
        staged.current_evidence_selection.primary_object_status =
            staged.current_gauge.primary_object_status;
        parameterSource = resolved.is_candidate
            ? "candidate_snapshot"
            : "active_working_revision";
    }
    else
    {
        std::string lockedParamReason;
        if (EvidenceSnapshotHasLockedParamSummaryLocal(
                resolved,
                lockedParamReason))
        {
            if (!ApplyEvidenceParameterSummaryToRuntimeGlobals(
                    staged,
                    resolved.parameter_summary,
                    lockedParamReason))
            {
                reason = "failed to apply evidence locked parameters: " +
                    lockedParamReason;
                return false;
            }
            SyncEvidenceLockedGlobalsToManualGaugeLocal(
                staged,
                resolved.script_path,
                "evidence_locked",
                resolved.primary_object_type,
                resolved.primary_object_name,
                resolved.primary_object_status);
            staged.current_evidence_selection.primary_object_type =
                staged.current_gauge.primary_object_type;
            staged.current_evidence_selection.primary_object_name =
                staged.current_gauge.primary_object_name;
            staged.current_evidence_selection.primary_object_status =
                staged.current_gauge.primary_object_status;
            parameterSource = "evidence_parameter_snapshot";
        }
    }

    staged.debug_action = "Apply Evidence Selection";
    staged.debug_status = loadImageToView
        ? "EVIDENCE_SELECTION_READY_WITH_IMAGE"
        : "EVIDENCE_SELECTION_READY";
    staged.debug_reason =
        "script=" + resolved.script_id +
        " image=" + resolved.image_id +
        " target=" + resolved.target_id +
        " parameter_source=" + parameterSource +
        ((resolved.is_candidate || loadWorkingRevision)
            ? " candidate_id=" + resolved.candidate_id
            : " baseline_evidence=true");

    m_manualTest = std::move(staged);

    if (loadImageToView)
    {
        UpdateImageViewImage(stagedImage);
        m_manualTest.image_file_path = resolvedImagePath.string();
        m_scriptResult.image_ref = resolvedImagePath.string();
        m_scriptResult.reason = "image loaded from evidence selection transaction";
        m_annotationStatus = "image loaded from evidence selection transaction";
        m_imageViewZoom = 1.0f;
        m_imageViewPanX = 0.0f;
        m_imageViewPanY = 0.0f;
    }

    if (resolved.is_candidate || loadWorkingRevision)
    {
        AppendEvidenceCandidateStateProbe(
            m_manualTest,
            resolved.candidate_dir,
            resolved.candidate_id,
            resolved.is_candidate
                ? "candidate_reload_complete"
                : "working_revision_reload_complete",
            "ready",
            m_manualTest.debug_reason);
    }

    reason = m_manualTest.debug_reason;
    CXLOG_INFO(
        "EvidenceChain",
        "evidence_selection_commit",
        "ready",
        m_manualTest.debug_reason +
        " primary_object=" +
        m_manualTest.current_evidence_selection.primary_object_type + " " +
        m_manualTest.current_evidence_selection.primary_object_name +
        " gauge={threshold=" +
        std::to_string(m_manualTest.current_gauge.threshold) +
        ",method=" + std::to_string(m_manualTest.current_gauge.method) +
        ",linegap=" + std::to_string(m_manualTest.current_gauge.linegap) +
        ",wgap=" + std::to_string(m_manualTest.current_gauge.wgap) +
        ",hgap=" + std::to_string(m_manualTest.current_gauge.hgap) +
        ",filterprofile=" +
        std::to_string(m_manualTest.current_gauge.filterprofile) + "}");
    return true;
}

void ViewController::ResetEvidenceThumbTexture(ScriptEvidenceThumb& thumb)
{
    thumb.texture_id = 0;
    thumb.texture_w = 0;
    thumb.texture_h = 0;
    thumb.texture_loaded = false;
    thumb.texture_failed = false;
    thumb.texture_placeholder = false;
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

    const float rowHeight = 128.0f;
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
        return ClassifyEvidenceMajorBucketLocal(
            m_manualTest,
            thumb,
            group.label);
    };

    auto classifyTool = [&](const ScriptEvidenceThumb& thumb,
                            const ScriptEvidenceGroup& group) -> std::pair<int, std::string>
    {
        const std::string key = toLower(
            thumb.tool + " " + thumb.script_id + " " + thumb.script_path + " " +
            thumb.status + " " + thumb.reason + " " + thumb.parameter_summary + " " +
            group.label);

        if (key.find("find_segmentation") != std::string::npos ||
            key.find("findsegmentation") != std::string::npos ||
            key.find("edgesam") != std::string::npos)
            return {6, "FindSegmentation Prompt / EdgeSam"};

        if (key.find("torch") != std::string::npos ||
            key.find("deeplab") != std::string::npos ||
            key.find("yolo") != std::string::npos)
        {
            if (key.find("detect") != std::string::npos ||
                key.find("yolo") != std::string::npos)
                return {7, "Torch Detection - Model Unverified"};
            if (key.find("segment") != std::string::npos ||
                key.find("mask") != std::string::npos ||
                key.find("deeplab") != std::string::npos)
                return {8, "Torch Segmentation - Runtime Smoke"};
            return {9, "Torch / Model Validation"};
        }

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
            return {6, "FindSegmentation Prompt / EdgeSam"};
        if (exactTool == "TorchTask")
            return {9, "Torch / Model Validation"};

        if (key.find("findline") != std::string::npos ||
            key.find("find_line") != std::string::npos)
            return {0, "FindLine"};

        if (key.find("findcircle") != std::string::npos ||
            key.find("find_circle") != std::string::npos)
            return {1, "FindCircle"};

        if (key.find("findobject") != std::string::npos ||
            key.find("find_object") != std::string::npos)
            return {2, "FindObject"};

        if (key.find("findellipse") != std::string::npos ||
            key.find("find_ellipse") != std::string::npos)
            return {3, "FindEllipse"};

        if (key.find("findrect") != std::string::npos ||
            key.find("find_rect") != std::string::npos)
            return {4, "FindRect"};

        if (key.find("fastmatch") != std::string::npos)
            return {5, "FastMatch"};

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
        { "Process Validation", 3, {} },
        { "Torch / Model Validation", 4, {} }
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

    static int classificationDebugDumpBudget = 3;
    if (classificationDebugDumpBudget > 0)
    {
        --classificationDebugDumpBudget;

        auto escapeTsv = [](std::string value) -> std::string
        {
            for (char& ch : value)
            {
                if (ch == '\t' || ch == '\r' || ch == '\n')
                    ch = ' ';
            }
            return value;
        };

        std::ostringstream debug;
        debug << "major\ttool\tgroup_label\tcase_id\timage_id\ttarget_id\t"
              << "candidate_id\tstatus\toverride\tscript_id\treason\n";

        for (const auto& major : categories)
        {
            for (const auto& tool : major.tools)
            {
                for (const ScriptEvidenceRowRef& ref : tool.rows)
                {
                    if (ref.group_index < 0 ||
                        ref.group_index >=
                            static_cast<int>(m_manualTest.script_evidence_groups.size()))
                        continue;
                    const ScriptEvidenceGroup& group =
                        m_manualTest.script_evidence_groups[ref.group_index];
                    if (ref.thumb_index < 0 ||
                        ref.thumb_index >= static_cast<int>(group.thumbs.size()))
                        continue;
                    const ScriptEvidenceThumb& thumb =
                        group.thumbs[ref.thumb_index];
                    debug << escapeTsv(major.label) << '\t'
                          << escapeTsv(tool.label) << '\t'
                          << escapeTsv(group.label) << '\t'
                          << escapeTsv(thumb.case_id) << '\t'
                          << escapeTsv(thumb.image_id) << '\t'
                          << escapeTsv(thumb.target_id) << '\t'
                          << escapeTsv(thumb.candidate_id) << '\t'
                          << escapeTsv(thumb.status) << '\t'
                          << escapeTsv(ResolveEvidenceCategoryOverrideLocal(
                                 m_manualTest,
                                 thumb)) << '\t'
                          << escapeTsv(thumb.script_id) << '\t'
                          << escapeTsv(thumb.reason) << '\n';
                }
            }
        }

        WriteTextFile(
            ResolveCxVisionRunPath(
                "cxscript_runs/evidence_chain/"
                "evidence_chain_ui_classification_debug.tsv"),
            debug.str());
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
        << " circle_inner_radius=" << getInt("global_circle_inner_radius", 0)
        << " circle_outer_radius=" << getInt("global_circle_outer_radius", 0)
        << " circle_ring_width=" << getInt("global_circle_ring_width", 0)
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

    auto finishRow = [&]()
    {
        ImGui::EndChild();
        if (selected)
            ImGui::PopStyleColor();
        ImGui::PopID();
    };

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
    auto textEllipsized = [](const char* label,
                             const std::string& value,
                             int maxChars)
    {
        std::string shown = value.empty() ? "-" : value;
        if (maxChars > 3 && static_cast<int>(shown.size()) > maxChars)
            shown = shown.substr(0, static_cast<std::size_t>(maxChars - 3)) + "...";
        ImGui::Text("%s%s", label, shown.c_str());
    };

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
        textEllipsized("path: ", thumb.script_path, 82);
        ImGui::Text("tool: %s | status: %s",
                    thumb.tool.empty() ? "-" : thumb.tool.c_str(),
                    thumb.status.empty() ? "-" : thumb.status.c_str());
        ImGui::Text("image: %s | target: %s",
                    thumb.image_id.empty() ? "-" : thumb.image_id.c_str(),
                    thumb.target_id.empty() ? "-" : thumb.target_id.c_str());
        textEllipsized("param: ", thumb.parameter_summary, 82);
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
        finishRow();
        return;
    }
    else if (rowClicked)
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
        finishRow();
        return;
    }

    if (rowRightClicked)
    {
        ImGui::OpenPopup("evidence_row_context");
    }

    bool rowStateReplaced = false;
    if (ImGui::BeginPopup("evidence_row_context"))
    {
        ImGui::TextUnformatted(thumb.script_id.c_str());
        ImGui::Separator();

        if (ImGui::BeginMenu("Move To Category"))
        {
            auto moveToCategory = [&](const char* category,
                                      const char* status,
                                      const char* reason)
            {
                thumb.evidence_category_override = category;
                m_manualTest.evidence_category_overrides[
                    BuildEvidenceCategoryOverrideKeyLocal(thumb)] = category;
                thumb.status = status;
                thumb.reason = reason;
                m_manualTest.script_evidence_row_refs_dirty = true;
                std::string saveReason;
                if (SaveEvidenceCategoryOverridesLocal(m_manualTest, saveReason))
                {
                    m_manualTest.debug_status = "EVIDENCE_CATEGORY_SAVED";
                    m_manualTest.debug_reason =
                        thumb.script_id + " -> " +
                        thumb.evidence_category_override + "; " + saveReason;
                }
                else
                {
                    m_manualTest.debug_status = "EVIDENCE_CATEGORY_SAVE_FAIL";
                    m_manualTest.debug_reason = saveReason;
                }
            };

            if (ImGui::MenuItem("Verified"))
                moveToCategory("Verified", "verified", "manual category: verified");
            if (ImGui::MenuItem("To Verify"))
                moveToCategory("To Verify", "pending_human_review", "manual category: to verify");
            if (ImGui::MenuItem("Defect"))
                moveToCategory("Defect", "defect", "manual category: defect");
            if (ImGui::MenuItem("Process Validation"))
                moveToCategory("Process Validation", "process_validation", "manual category: process validation");
            if (ImGui::MenuItem("Torch / Model Validation"))
                moveToCategory("Torch / Model Validation", "model_validation", "manual category: model validation");

            ImGui::EndMenu();
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Load This Image To Image View"))
        {
            const std::string thumbImagePathBeforeLoad = thumb.image_path;
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
                    "loaded from evidence row: " + thumbImagePathBeforeLoad;
            }
            rowStateReplaced = true;
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
                const std::string boundScriptId = thumb.script_id;
                const std::string boundImagePath = thumb.image_path;

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
                        boundScriptId + " -> " + boundImagePath;
                }
                rowStateReplaced = true;
            }
        }

        if (ImGui::MenuItem("Select Image File..."))
        {
            std::string selectedPath;
            std::string dialogReason;
            if (!SelectEvidenceImageFileFromDialogLocal(selectedPath, dialogReason))
            {
                m_manualTest.debug_status = "EVIDENCE_IMAGE_SELECT_CANCEL";
                m_manualTest.debug_reason = dialogReason;
            }
            else
            {
                const std::string selectedScriptId = thumb.script_id;
                thumb.image_path = selectedPath;
                thumb.thumbnail_path = selectedPath;
                const std::filesystem::path selectedFs(selectedPath);
                thumb.image_id = selectedFs.stem().string();
                ResetEvidenceThumbTexture(thumb);
                thumb.reason = "bound from selected image file";

                std::string reason;
                if (!RefreshEvidenceSelectionFromThumb(
                        groupIndex,
                        thumbIndex,
                        true,
                        reason))
                {
                    m_manualTest.debug_status = "EVIDENCE_IMAGE_SELECT_FAIL";
                    m_manualTest.debug_reason = reason;
                }
                else
                {
                    m_manualTest.debug_status = "EVIDENCE_IMAGE_SELECTED";
                    m_manualTest.debug_reason =
                        selectedScriptId + " -> " + selectedPath;
                }
                rowStateReplaced = true;
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
                const std::string boundImagePath = thumb.image_path;
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
                        "bound first manifest image: " + boundImagePath;
                }
                rowStateReplaced = true;
            }
        }

        if (ImGui::MenuItem("Bind Current Runtime Params"))
        {
            thumb.parameter_summary = BuildCurrentRuntimeParamSummary(m_manualTest);
            thumb.reason = "parameter summary bound from runtime globals";
            const std::string boundParameterSummary = thumb.parameter_summary;

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
                m_manualTest.debug_reason = boundParameterSummary;
            }
            rowStateReplaced = true;
        }

        if (ImGui::MenuItem("Clear Image Binding"))
        {
            const std::string clearedScriptId = thumb.script_id;
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
                m_manualTest.debug_reason = "image binding cleared for " + clearedScriptId;
            }
            rowStateReplaced = true;
        }

        ImGui::EndPopup();
    }

    if (rowStateReplaced)
    {
        finishRow();
        return;
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

    finishRow();
}
