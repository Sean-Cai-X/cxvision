#include "pch.h"
#include "ManualConsoleEvidenceChain.h"
#include "ManualConsoleUtils.h"
#include "ManualConsoleGauge.h"
#include "ManualStateTestConsole.h"
#include "CxScriptCatalogRuntime.h"
#include "CxScriptCasePackageWriter.h"
#include "CxScriptEvidenceChainRuntime.h"
#include "CxUnifiedLog.h"
#include "CircleShape.h"
#include "EllipseShape.h"
#include "LineGaugeShape.h"
#include "PolylineShape.h"
#include "RectShape.h"

#include <glad/glad.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <sstream>
#include <unordered_map>
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
    if (lowered == "findobject" || lowered == "find_object")
        return "FindObject";
    if (lowered == "fastmatch" || lowered == "cfastmatch")
        return "FastMatch";
    if (lowered == "gridpatternclasstool" || lowered == "gridpatternclass")
        return "GridPatternClassTool";
    if (lowered == "regionpatterntool" || lowered == "regionpattern")
        return "RegionPatternTool";
    if (lowered == "findsegmentation" ||
        lowered.find("findsegmentation") != std::string::npos ||
        lowered.find("find_segmentation") != std::string::npos)
        return "FindSegmentation";
    if (lowered == "torchtask" || lowered == "torch" ||
        lowered.find("torch") != std::string::npos)
        return "TorchTask";
    return typeOrTool;
}

static std::string EvidenceSourceFileNameLocal(const std::string& pathOrId)
{
    if (pathOrId.empty())
        return {};

    std::string fileName = std::filesystem::path(pathOrId).filename().string();
    if (fileName.empty())
        fileName = pathOrId;

    const std::size_t candidateSuffix = fileName.find(" [");
    if (candidateSuffix != std::string::npos)
        fileName.erase(candidateSuffix);

    std::transform(fileName.begin(), fileName.end(), fileName.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return fileName;
}

static bool SameEvidenceSourceScriptLocal(
    const std::string& candidateSourcePath,
    const ScriptEvidenceThumb& original)
{
    if (candidateSourcePath.empty())
        return false;

    const std::filesystem::path candidateSource =
        ResolveWorkspaceFile(candidateSourcePath).lexically_normal();
    const std::filesystem::path originalSource =
        ResolveWorkspaceFile(original.script_path).lexically_normal();
    if (candidateSource == originalSource)
        return true;

    const std::string candidateFile =
        EvidenceSourceFileNameLocal(candidateSourcePath);
    if (candidateFile.empty())
        return false;
    return candidateFile == EvidenceSourceFileNameLocal(original.script_path) ||
           candidateFile == EvidenceSourceFileNameLocal(original.script_id);
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

static std::string PreferEvidenceCategoryOverrideLocal(
    const std::string& current,
    const std::string& candidate)
{
    if (candidate.empty())
        return current;
    if (current.empty())
        return candidate;
    if (current == candidate)
        return current;
    if (candidate == "Verified" || current == "Verified")
        return "Verified";
    if (candidate == "Defect" || current == "Defect")
        return "Defect";
    if (candidate == "To Verify" || current == "To Verify")
        return "To Verify";
    return current;
}

static void StoreEvidenceCategoryOverrideLocal(
    ManualTestContext& context,
    const ScriptEvidenceThumb& thumb,
    const std::string& category)
{
    for (const std::string& key :
         BuildEvidenceCategoryOverrideLookupKeysLocal(thumb))
    {
        if (!key.empty())
            context.evidence_category_overrides[key] = category;
    }
}

static std::string ResolveEvidenceCategoryOverrideLocal(
    const ManualTestContext& context,
    const ScriptEvidenceThumb& thumb)
{
    std::string resolved;
    for (const std::string& key :
         BuildEvidenceCategoryOverrideLookupKeysLocal(thumb))
    {
        const auto it = context.evidence_category_overrides.find(key);
        if (it != context.evidence_category_overrides.end())
            resolved = PreferEvidenceCategoryOverrideLocal(resolved, it->second);
    }

    
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
                resolved = PreferEvidenceCategoryOverrideLocal(
                    resolved,
                    entry.second);
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
                resolved = PreferEvidenceCategoryOverrideLocal(
                    resolved,
                    entry.second);
            }
        }
    }

    return resolved.empty() ? thumb.evidence_category_override : resolved;
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

static bool IsTorchEvidenceCandidateRowLocal(
    const ScriptEvidenceThumb& thumb,
    const std::string& groupLabel)
{
    const std::string normalizedTool =
        NormalizeEvidenceToolTypeLocal(thumb.tool);

    return normalizedTool == "TorchTask" &&
           thumb.evidence_category_override == "Torch Evidence Candidates";
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
    if (categoryOverride == "Torch Evidence Candidates")
        return {5, "Torch Evidence Candidates"};
    if (categoryOverride == "Torch / Model Validation")
        return {6, "Torch / Model Validation"};
    if (!categoryOverride.empty())
        return {4, categoryOverride};

    const std::string key = BuildEvidenceClassificationKeyLocal(
        thumb.tool,
        thumb.script_id,
        thumb.script_path,
        groupLabel,
        thumb.status,
        thumb.reason,
        thumb.parameter_summary);

    if (IsTorchEvidenceCandidateRowLocal(thumb, groupLabel))
        return {5, "Torch Evidence Candidates"};

    const std::string normalizedTool =
        NormalizeEvidenceToolTypeLocal(thumb.tool);
    const bool isExplicitCxImageTool =
        normalizedTool == "FindLine" ||
        normalizedTool == "FindCircle" ||
        normalizedTool == "FindObject" ||
        normalizedTool == "FindEllipse" ||
        normalizedTool == "FindRect" ||
        normalizedTool == "RegionPatternTool" ||
        normalizedTool == "GridPatternClassTool" ||
        normalizedTool == "FastMatch";
    const bool isTorchLayer =
        !isExplicitCxImageTool &&
        (key.find("torch") != std::string::npos ||
         key.find("find_segmentation") != std::string::npos ||
         key.find("findsegmentation") != std::string::npos ||
         key.find("edgesam") != std::string::npos ||
         key.find("segmentation") != std::string::npos ||
         key.find("detection") != std::string::npos ||
         key.find("model") != std::string::npos);

    if (isTorchLayer)
        return {6, "Torch / Model Validation"};

    if (normalizedTool == "TorchTask")
        return {6, "Torch / Model Validation"};

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

struct HDReferenceImageBindingLocal
{
    std::string script_id;
    std::string image_id;
    std::string image_path;
};

static bool ResolveHDReferenceImageBindingLocal(
    const std::string& scriptId,
    HDReferenceImageBindingLocal& out)
{
    auto normalizeKey = [](const std::string& value) -> std::string
    {
        if (value.empty())
            return {};
        std::string normalized = value;
        std::replace(normalized.begin(), normalized.end(), '\\', '/');
        const std::size_t slash = normalized.find_last_of('/');
        if (slash != std::string::npos)
            normalized = normalized.substr(slash + 1);
        const std::string suffix = ".cxsc";
        if (normalized.size() > suffix.size() &&
            normalized.compare(normalized.size() - suffix.size(),
                               suffix.size(),
                               suffix) == 0)
        {
            normalized.resize(normalized.size() - suffix.size());
        }
        return normalized;
    };

    auto splitTsv = [](const std::string& line) -> std::vector<std::string>
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
    };

    static bool loaded = false;
    static std::vector<HDReferenceImageBindingLocal> bindings;
    if (!loaded)
    {
        loaded = true;
        const std::filesystem::path bindingPath = ResolveWorkspaceFile(
            "cxparser/cxscript/module/cximage/evidence/hd_reference_image_bindings.tsv");
        std::string text;
        if (ReadTextFile(bindingPath.string(), text))
        {
            std::istringstream input(text);
            std::string line;
            bool headerSeen = false;
            while (std::getline(input, line))
            {
                line = TrimLine(line);
                if (line.empty() || line[0] == '#')
                    continue;
                const std::vector<std::string> cells = splitTsv(line);
                if (!headerSeen)
                {
                    headerSeen = true;
                    if (!cells.empty() && cells[0] == "script_id")
                        continue;
                }
                if (cells.size() < 3)
                    continue;
                HDReferenceImageBindingLocal binding;
                binding.script_id = TrimLine(cells[0]);
                binding.image_id = TrimLine(cells[1]);
                binding.image_path = TrimLine(cells[2]);
                if (!binding.script_id.empty() &&
                    !binding.image_id.empty() &&
                    !binding.image_path.empty())
                {
                    bindings.push_back(std::move(binding));
                }
            }
        }
    }

    const std::string key = normalizeKey(scriptId);
    for (const auto& binding : bindings)
    {
        if (key == binding.script_id)
        {
            out = binding;
            return true;
        }
    }
    return false;
}

static void ApplyHDReferenceImageBindingLocal(
    ScriptEvidenceThumb& thumb)
{
    HDReferenceImageBindingLocal binding{};
    if (!ResolveHDReferenceImageBindingLocal(thumb.script_id, binding) &&
        !ResolveHDReferenceImageBindingLocal(thumb.script_path, binding))
    {
        return;
    }

    thumb.image_id = binding.image_id;
    thumb.image_path = binding.image_path;
    thumb.thumbnail_path = binding.image_path;
    if (thumb.case_id.empty())
        thumb.case_id = binding.script_id;
    if (thumb.status.empty())
        thumb.status = "hd_reference_ready";
    if (thumb.reason.empty())
        thumb.reason = "HD reference image binding";
    else
        thumb.reason += "; HD reference image binding";
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
           normalized == "FindObject" ||
           normalized == "FastMatch" ||
           normalized == "GridPatternClassTool" ||
           normalized == "RegionPatternTool" ||
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

static bool ValidateCandidateGaugeAnnotationLocal(
    const std::filesystem::path& gaugePath,
    const std::string& bindingTool,
    std::string& reason)
{
    std::string text;
    if (gaugePath.empty() || !ReadTextFile(gaugePath.string(), text))
    {
        reason = "gauge_annotation.json is missing";
        return false;
    }

    std::string tool = NormalizeEvidenceToolTypeLocal(
        ReadJsonStringFieldLocal(text, "tool"));
    if (tool.empty())
        tool = NormalizeEvidenceToolTypeLocal(bindingTool);

    if (tool == "FindObject")
    {
        int x0 = 0;
        int y0 = 0;
        int x1 = 0;
        int y1 = 0;
        if (!ReadJsonIntFieldLocal(text, "findobject_x0", x0) ||
            !ReadJsonIntFieldLocal(text, "findobject_y0", y0) ||
            !ReadJsonIntFieldLocal(text, "findobject_x1", x1) ||
            !ReadJsonIntFieldLocal(text, "findobject_y1", y1))
        {
            reason = "FindObject ROI gauge fields are incomplete";
            return false;
        }
        if (x0 == x1 || y0 == y1)
        {
            reason = "FindObject ROI must have positive width and height";
            return false;
        }
        reason.clear();
        return true;
    }

    if (tool == "FindLine" || tool == "FindRect")
    {
        bool hasLineGauge = false;
        int x0 = 0;
        int y0 = 0;
        int x1 = 0;
        int y1 = 0;
        int halfWidth = 0;
        if (!ReadJsonBoolFieldLocal(text, "has_line_gauge", hasLineGauge) ||
            !hasLineGauge ||
            !ReadJsonIntFieldLocal(text, "line_x0", x0) ||
            !ReadJsonIntFieldLocal(text, "line_y0", y0) ||
            !ReadJsonIntFieldLocal(text, "line_x1", x1) ||
            !ReadJsonIntFieldLocal(text, "line_y1", y1) ||
            !ReadJsonIntFieldLocal(text, "tool_half_width", halfWidth))
        {
            reason = "FindLine/FindRect gauge fields are incomplete";
            return false;
        }
        if (x0 == x1 && y0 == y1)
        {
            reason = "line gauge length is zero";
            return false;
        }
        if (halfWidth <= 0)
        {
            reason = "tool_half_width must be positive";
            return false;
        }
        reason.clear();
        return true;
    }

    if (tool == "FindCircle")
    {
        bool hasCircleGauge = false;
        int cx = 0;
        int cy = 0;
        int px = 0;
        int py = 0;
        if (!ReadJsonBoolFieldLocal(text, "has_circle_gauge", hasCircleGauge) ||
            !hasCircleGauge ||
            !ReadJsonIntFieldLocal(text, "circle_cx", cx) ||
            !ReadJsonIntFieldLocal(text, "circle_cy", cy) ||
            !ReadJsonIntFieldLocal(text, "circle_px", px) ||
            !ReadJsonIntFieldLocal(text, "circle_py", py))
        {
            reason = "FindCircle gauge fields are incomplete";
            return false;
        }
        if (cx == px && cy == py)
        {
            reason = "circle radius is zero";
            return false;
        }
        reason.clear();
        return true;
    }

    if (tool == "FindEllipse")
    {
        bool hasEllipseGauge = false;
        int x0 = 0;
        int y0 = 0;
        int x1 = 0;
        int y1 = 0;
        if (!ReadJsonBoolFieldLocal(text, "has_ellipse_gauge", hasEllipseGauge) ||
            !hasEllipseGauge ||
            !ReadJsonIntFieldLocal(text, "ellipse_x0", x0) ||
            !ReadJsonIntFieldLocal(text, "ellipse_y0", y0) ||
            !ReadJsonIntFieldLocal(text, "ellipse_x1", x1) ||
            !ReadJsonIntFieldLocal(text, "ellipse_y1", y1))
        {
            reason = "FindEllipse gauge fields are incomplete";
            return false;
        }
        if (x0 == x1 || y0 == y1)
        {
            reason = "ellipse gauge radius is zero";
            return false;
        }
        reason.clear();
        return true;
    }

    reason.clear();
    return true;
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
        std::max(-1,
                 std::min(
                     getRuntimeInt("global_findline_selected_edge", 0),
                     context.findline_scan_edge_count));
    context.findline_best_fit_edge =
        std::max(0,
                 std::min(
                     getRuntimeInt("global_findline_best_edge", 0),
                     context.findline_scan_edge_count));
    context.findline_recommended_fit_edge =
        std::max(0,
                 std::min(
                     getRuntimeInt("global_findline_recommended_edge", 0),
                     context.findline_scan_edge_count));
    context.findline_relation_edge =
        std::max(0,
                 std::min(
                     getRuntimeInt("global_findline_relation_edge", 0),
                     context.findline_scan_edge_count));
    context.findline_attach_edge =
        std::max(0,
                 std::min(
                     getRuntimeInt("global_findline_attach_edge", 0),
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

    context.findcircle_scan_edge_count =
        std::max(1, std::min(32, getRuntimeInt("global_findcircle_edge_count", 4)));
    context.findcircle_selected_scan_edge =
        std::max(-1,
                 std::min(
                     getRuntimeInt("global_findcircle_selected_edge", 0),
                     context.findcircle_scan_edge_count));
    context.findcircle_best_fit_edge =
        std::max(0,
                 std::min(
                     getRuntimeInt("global_findcircle_best_edge", 0),
                     context.findcircle_scan_edge_count));
    context.findcircle_recommended_fit_edge =
        std::max(0,
                 std::min(
                     getRuntimeInt("global_findcircle_recommended_edge", 0),
                     context.findcircle_scan_edge_count));
    context.findcircle_relation_edge =
        std::max(0,
                 std::min(
                     getRuntimeInt("global_findcircle_relation_edge", 0),
                     context.findcircle_scan_edge_count));
    context.findcircle_attach_edge =
        std::max(0,
                 std::min(
                     getRuntimeInt("global_findcircle_attach_edge", 0),
                     context.findcircle_scan_edge_count));
    context.findcircle_edge_params.resize(
        static_cast<std::size_t>(context.findcircle_scan_edge_count + 1));
    const int sharedCircleThreshold = getRuntimeInt("global_threshold", 20);
    const int sharedCircleMethod = getRuntimeInt("global_method", 0);
    const int sharedCircleLinegap = getRuntimeInt("global_linegap", 3);
    const int sharedCircleGap = getRuntimeInt("global_gap", 6);
    for (int edge = 1; edge <= context.findcircle_scan_edge_count; ++edge)
    {
        ManualFindCircleEdgeParamState& params =
            context.findcircle_edge_params[static_cast<std::size_t>(edge)];
        params.initialized = true;
        params.threshold = sharedCircleThreshold;
        params.method = sharedCircleMethod;
        params.linegap = sharedCircleLinegap;
        params.gap = sharedCircleGap;
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

static bool EvidenceParamSummaryHasKeyLocal(
    const std::string& summary,
    const std::string& key)
{
    std::istringstream iss(summary);
    std::string token;
    const std::string needle = key + "=";
    while (iss >> token)
    {
        if (!token.empty() && token.back() == ';')
            token.pop_back();
        if (token.rfind(needle, 0) == 0)
            return true;
    }
    return false;
}

static bool EvidenceSnapshotLooksLikeToolLocal(
    const CxEvidenceSelectionSnapshot& snapshot,
    const std::string& normalizedTool)
{
    if (NormalizeEvidenceToolTypeLocal(snapshot.tool) == normalizedTool)
        return true;

    std::string haystack =
        snapshot.script_id + " " + snapshot.script_path + " " +
        snapshot.source_evidence_script_path;
    std::transform(haystack.begin(), haystack.end(), haystack.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

    if (normalizedTool == "FindEllipse")
    {
        return haystack.find("findellipse") != std::string::npos ||
            haystack.find("find_ellipse") != std::string::npos;
    }
    return false;
}

static bool MigrateLegacyEvidenceSelectionSnapshotLocal(
    CxEvidenceSelectionSnapshot& snapshot)
{
    const bool looksLikeSavedCandidate =
        snapshot.is_candidate || snapshot.has_saved_state ||
        !snapshot.candidate_id.empty() || !snapshot.candidate_dir.empty() ||
        !snapshot.runtime_globals_path.empty() ||
        !snapshot.gauge_annotation_path.empty();
    if (!looksLikeSavedCandidate)
        return false;

    if (!EvidenceSnapshotLooksLikeToolLocal(snapshot, "FindEllipse"))
        return false;

    if (EvidenceParamSummaryHasKeyLocal(
            snapshot.parameter_summary,
            "ellipse_inner_scale_percent") ||
        EvidenceParamSummaryHasKeyLocal(
            snapshot.parameter_summary,
            "inner_scale_percent"))
    {
        return false;
    }

    if (snapshot.parameter_summary.empty() ||
        snapshot.parameter_summary == "-")
    {
        snapshot.parameter_summary = "ellipse_inner_scale_percent=0";
    }
    else
    {
        snapshot.parameter_summary += "; ellipse_inner_scale_percent=0";
    }
    snapshot.parameter_profile_id = snapshot.parameter_summary;
    if (!snapshot.reason.empty())
        snapshot.reason += " | ";
    snapshot.reason +=
        "migrated legacy FindEllipse candidate default ellipse_inner_scale_percent=0";
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
    const bool isFindObjectScript =
        scriptPath.find("find_object") != std::string::npos ||
        scriptPath.find("findobject") != std::string::npos ||
        scriptPath.find("FindObject") != std::string::npos;
    const bool isFindSegmentationScript =
        scriptPath.find("find_segmentation") != std::string::npos ||
        scriptPath.find("findsegmentation") != std::string::npos ||
        scriptPath.find("FindSegmentation") != std::string::npos;
    const bool isFastMatchScript =
        scriptPath.find("fastmatch") != std::string::npos ||
        scriptPath.find("FastMatch") != std::string::npos ||
        scriptPath.find("CFastMatch") != std::string::npos;

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
        gauge.ellipse_inner_scale_percent = std::max(
            0,
            std::min(
                99,
                getInt(
                    "global_findellipse_inner_scale_percent",
                    getInt("ellipse_inner_scale_percent",
                           getInt("inner_scale_percent", 0)))));
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
    else if (primaryType == "FindObject" ||
             (primaryType.empty() && isFindObjectScript))
    {
        gauge.tool = "FindObject";
        gauge.primary_object_type = "FindObject";
        if (gauge.primary_object_name.empty())
            gauge.primary_object_name = primaryObjectName.empty() ? "m_object" : primaryObjectName;
        gauge.has_findobject_roi = true;
        gauge.findobject_x0 = getInt("global_roi_x0", getInt("global_roi_x", 0));
        gauge.findobject_y0 = getInt("global_roi_y0", getInt("global_roi_y", 0));
        const int roiW = getInt("global_roi_width", 0);
        const int roiH = getInt("global_roi_height", 0);
        gauge.findobject_x1 = getInt("global_roi_x1", gauge.findobject_x0 + roiW);
        gauge.findobject_y1 = getInt("global_roi_y1", gauge.findobject_y0 + roiH);
        gauge.findobject_foreground_mode = getInt("global_method", gauge.method);
        gauge.findobject_threshold = getInt("global_threshold", gauge.threshold);
        gauge.findobject_min_area = getInt("global_object_min_area", 10);
    }
    else if (primaryType == "FindSegmentation" ||
             (primaryType.empty() && isFindSegmentationScript))
    {
        gauge.tool = "FindSegmentation";
        gauge.primary_object_type = "FindSegmentation";
        if (gauge.primary_object_name.empty())
            gauge.primary_object_name =
                primaryObjectName.empty() ? "m_seg" : primaryObjectName;
        if (gauge.primary_object_status.empty())
            gauge.primary_object_status = "evidence_findsegmentation_selected";
        gauge.has_segmentation_prompt_rect = true;
        gauge.segmentation_prompt_x0 = getInt("global_roi_x0", getInt("global_roi_x", 120));
        gauge.segmentation_prompt_y0 = getInt("global_roi_y0", getInt("global_roi_y", 120));
        const int roiW = getInt("global_roi_width", 860);
        const int roiH = getInt("global_roi_height", 700);
        gauge.segmentation_prompt_x1 = getInt("global_roi_x1", gauge.segmentation_prompt_x0 + roiW);
        gauge.segmentation_prompt_y1 = getInt("global_roi_y1", gauge.segmentation_prompt_y0 + roiH);
        gauge.segmentation_mode = getInt("global_segmentation_mode", gauge.method);
        gauge.segmentation_threshold_percent = getInt(
            "global_segmentation_threshold_percent",
            gauge.segmentation_threshold_percent);
        gauge.has_segmentation_positive_point =
            getInt("global_segmentation_positive_enabled", 0) != 0;
        gauge.segmentation_positive_x = getInt(
            "global_segmentation_positive_x", gauge.segmentation_positive_x);
        gauge.segmentation_positive_y = getInt(
            "global_segmentation_positive_y", gauge.segmentation_positive_y);
        gauge.has_segmentation_negative_point =
            getInt("global_segmentation_negative_enabled", 0) != 0;
        gauge.segmentation_negative_x = getInt(
            "global_segmentation_negative_x", gauge.segmentation_negative_x);
        gauge.segmentation_negative_y = getInt(
            "global_segmentation_negative_y", gauge.segmentation_negative_y);
    }
    else if (primaryType == "FastMatch" ||
             (primaryType.empty() && isFastMatchScript))
    {
        gauge.tool = "FastMatch";
        gauge.primary_object_type = "FastMatch";
        if (gauge.primary_object_name.empty())
            gauge.primary_object_name = "m_match";
        if (gauge.primary_object_status.empty())
            gauge.primary_object_status = "evidence_fastmatch_selected";
    }

    if (gauge.has_circle_gauge || gauge.has_line_gauge ||
        gauge.has_ellipse_gauge || gauge.has_findobject_roi ||
        gauge.has_segmentation_prompt_rect || gauge.tool == "FastMatch")
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

    for (const auto& item : context.image_manifest_items)
    {
        addCandidate(item.image_path);
    }

    const std::filesystem::path testImageRoot =
        ResolveCxVisionRunPath("test_images");
    std::error_code ec;
    if (std::filesystem::is_directory(testImageRoot, ec))
    {
        std::filesystem::recursive_directory_iterator it(
            testImageRoot,
            std::filesystem::directory_options::skip_permission_denied,
            ec);
        const std::filesystem::recursive_directory_iterator end;
        for (; !ec && it != end; it.increment(ec))
        {
            if (!it->is_regular_file(ec))
                continue;
            std::string ext = it->path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(),
                [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" ||
                ext == ".bmp" || ext == ".tif" || ext == ".tiff")
            {
                addCandidate(it->path().string());
                if (candidates.size() >= 64)
                    break;
            }
        }
    }

    std::stable_sort(candidates.begin(), candidates.end());
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
    std::unordered_map<std::string, std::size_t>& nextIndexByPool)
{
    if (!thumb.image_path.empty())
        return;

    if (candidates.empty())
        return;

    auto toLower = [](std::string value) -> std::string
    {
        std::transform(
            value.begin(),
            value.end(),
            value.begin(),
            [](unsigned char ch)
            {
                return static_cast<char>(std::tolower(ch));
            });
        return value;
    };

    const std::string identity = toLower(
        thumb.tool + " " + thumb.script_id + " " + thumb.script_path);
    std::string poolKey = "other";

    std::vector<const std::string*> preferred;
    auto collectPreferred = [&](const char* token)
    {
        for (const std::string& candidate : candidates)
        {
            const std::string lowerCandidate = toLower(candidate);
            if (lowerCandidate.find(token) != std::string::npos)
                preferred.push_back(&candidate);
        }
    };

    if (identity.find("findellipse") != std::string::npos ||
        identity.find("find_ellipse") != std::string::npos)
    {
        poolKey = "ellipse";
        collectPreferred("ellipse");
    }
    else if (identity.find("findcircle") != std::string::npos ||
             identity.find("find_circle") != std::string::npos)
    {
        poolKey = "circle";
        collectPreferred("circle");
    }
    else if (identity.find("findline") != std::string::npos ||
             identity.find("find_line") != std::string::npos)
    {
        poolKey = "line";
        collectPreferred("line");
    }
    else if (identity.find("findrect") != std::string::npos ||
             identity.find("find_rect") != std::string::npos)
    {
        poolKey = "rect";
        collectPreferred("rect");
    }
    else if (identity.find("fastmatch") != std::string::npos)
    {
        poolKey = "fastmatch";
        collectPreferred("match");
    }

    std::stable_sort(
        preferred.begin(),
        preferred.end(),
        [](const std::string* left, const std::string* right)
        {
            return *left < *right;
        });

    const std::size_t index = nextIndexByPool[poolKey]++;
    const std::string& path = preferred.empty()
        ? candidates[index % candidates.size()]
        : *preferred[index % preferred.size()];
    thumb.image_path = path;
    thumb.thumbnail_path = path;

    if (thumb.image_id.empty())
        thumb.image_id = std::filesystem::path(path).stem().string();

    if (thumb.reason.empty())
        thumb.reason = "fallback image bound for evidence placeholder";
}

static std::string ResolveEvidenceImagePathFromContextLocal(
    const ManualTestContext& context,
    const std::string& imageId)
{
    if (imageId.empty())
        return {};

    for (const auto& item : context.evidence_items)
    {
        if (item.image_id == imageId && !item.image_path.empty())
            return item.image_path;
    }

    for (const auto& item : context.image_manifest_items)
    {
        if (item.image_id == imageId && !item.image_path.empty())
            return item.image_path;
    }

    if (imageId.rfind("fallback_image_", 0) == 0)
    {
        const std::string suffix =
            imageId.substr(std::string("fallback_image_").size());
        char* endPtr = nullptr;
        const long parsed = std::strtol(suffix.c_str(), &endPtr, 10);
        if (endPtr != suffix.c_str() && endPtr != nullptr && *endPtr == '\0' &&
            parsed >= 0)
        {
            const std::vector<std::string> candidates =
                BuildEvidenceFallbackImageCandidates(context);
            if (!candidates.empty())
            {
                return candidates[
                    static_cast<std::size_t>(parsed) % candidates.size()];
            }
        }
    }

    return ResolveEvidenceImagePathByIdFromDiskLocal(imageId);
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
        << " grid_normalized_width=" << getInt("global_grid_normalized_width", 48)
        << " grid_normalized_height=" << getInt("global_grid_normalized_height", 48)
        << " grid_rows=" << getInt("global_grid_rows", 12)
        << " grid_cols=" << getInt("global_grid_cols", 12)
        << " grid_levels=" << getInt("global_grid_levels", 3)
        << " grid_orientation_bins=" << getInt("global_grid_orientation_bins", 8)
        << " grid_foreground_threshold=" << getInt("global_grid_foreground_threshold", -1)
        << " grid_active_foreground_percent=" << getInt("global_grid_active_foreground_percent", 5)
        << " grid_active_edge_percent=" << getInt("global_grid_active_edge_percent", 3)
        << " grid_max_overlays=" << getInt("global_grid_max_overlays", 96)
        << " grid_fusion_mode=" << getInt("global_grid_fusion_mode", 2)
        << " region_roi_x=" << getInt("global_region_roi_x", 120)
        << " region_roi_y=" << getInt("global_region_roi_y", 120)
        << " region_roi_w=" << getInt("global_region_roi_w", 120)
        << " region_roi_h=" << getInt("global_region_roi_h", 90)
        << " region_normalized_width=" << getInt("global_region_normalized_width", 32)
        << " region_normalized_height=" << getInt("global_region_normalized_height", 32)
        << " region_pooling_rows=" << getInt("global_region_pooling_rows", 4)
        << " region_pooling_cols=" << getInt("global_region_pooling_cols", 4)
        << " region_use_binary=" << getInt("global_region_use_binary", 0)
        << " region_threshold=" << getInt("global_region_threshold", 128)
        << " region_foreground_dark=" << getInt("global_region_foreground_dark", 1)
        << " region_max_overlays=" << getInt("global_region_max_overlays", 64)
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
    else if (normalizedTool == "FindObject")
    {
        appendInt(oss, summaryText, "roi_x0", "roi_x0", missing);
        appendInt(oss, summaryText, "roi_y0", "roi_y0", missing);
        appendInt(oss, summaryText, "roi_x1", "roi_x1", missing);
        appendInt(oss, summaryText, "roi_y1", "roi_y1", missing);
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

static int AppendManualAlgorithmReviewHandoffsFromRunFoldersLocal(
    ManualTestContext& context,
    const std::function<std::string(const std::string&)>& resolveImagePath,
    const std::function<ScriptEvidenceGroup&(const std::string&)>& findGroup,
    std::string& reason)
{
    reason.clear();
    const std::filesystem::path root =
        ResolveCxVisionRunPath("cxscript_runs");
    std::error_code ec;
    if (!std::filesystem::is_directory(root, ec))
    {
        reason = "cxscript_runs folder not found: " + root.string();
        return 0;
    }

    std::vector<std::filesystem::path> handoffs;
    std::filesystem::recursive_directory_iterator it(
        root,
        std::filesystem::directory_options::skip_permission_denied,
        ec);
    const std::filesystem::recursive_directory_iterator end;
    for (; !ec && it != end; it.increment(ec))
    {
        if (!it->is_regular_file(ec))
            continue;
        if (it->path().filename() == "manual_algorithm_review_handoff.md")
            handoffs.push_back(it->path());
    }

    std::stable_sort(
        handoffs.begin(),
        handoffs.end(),
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

    int before = 0;
    for (const auto& group : context.script_evidence_groups)
        before += static_cast<int>(group.thumbs.size());

    for (const auto& handoff : handoffs)
    {
        AppendManualAlgorithmReviewHandoffLocal(
            context,
            handoff.string(),
            resolveImagePath,
            findGroup);
    }

    int after = 0;
    for (const auto& group : context.script_evidence_groups)
        after += static_cast<int>(group.thumbs.size());

    const int appended = std::max(0, after - before);
    std::ostringstream oss;
    oss << "manual algorithm review handoffs scanned="
        << handoffs.size()
        << " appended_cases=" << appended;
    reason = oss.str();
    return appended;
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
         << "has_saved_state\tscript_id\tscript_path\timage_path\t"
         << "thumbnail_path\tdataset_images\tannotations\treason\n";

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
                 << escapeTsv(thumb.image_path) << '\t'
                 << escapeTsv(thumb.thumbnail_path) << '\t'
                 << thumb.dataset_images.size() << '\t'
                 << thumb.annotations.size() << '\t'
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

static bool EvidenceThumbLooksLikeFindEllipseLocal(
    const ScriptEvidenceThumb& thumb,
    const ScriptEvidenceGroup& group);

static void AppendSavedEvidenceCandidatesLocal(
    ManualTestContext& context,
    const std::function<ScriptEvidenceGroup&(const std::string&)>& findGroup)
{
    std::vector<std::filesystem::path> roots;
    roots.push_back(ResolveCxVisionRunPath("cxscript_runs/evidence_candidates"));
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
        -> bool
    {
        for (auto& group : context.script_evidence_groups)
        {
            for (auto& original : group.thumbs)
            {
                if (original.is_candidate)
                    continue;
                const bool sameCase = !candidate.case_id.empty() &&
                    original.case_id == candidate.case_id;
                const bool sameScript = SameEvidenceSourceScriptLocal(
                    candidate.source_evidence_script_path,
                    original);
                const bool sameImage =
                    candidate.image_id.empty() || original.image_id.empty() ||
                    original.image_id == candidate.image_id;
                const bool sameTarget =
                    candidate.target_id.empty() || original.target_id.empty() ||
                    original.target_id == candidate.target_id;
                const bool identityMatches =
                    sameCase || (sameScript && sameImage && sameTarget);
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
                const std::string originalTool =
                    NormalizeEvidenceToolTypeLocal(original.tool);
                const std::string candidateTool =
                    NormalizeEvidenceToolTypeLocal(candidate.tool);
                if (!candidateTool.empty() &&
                    (originalTool.empty() ||
                     originalTool == "module" ||
                     originalTool == "unknown"))
                {
                    original.tool = candidateTool;
                }
                original.parameter_summary = candidate.parameter_summary;
                original.status = candidate.status;
                original.reason =
                    "active working revision=" + candidate.candidate_id +
                    "; restored from candidate binding; " +
                    candidate.reason;
                CXLOG_INFO(
                    "EvidenceChain",
                    "working_revision_rebound",
                    "restored",
                    "case_id=" + candidate.case_id +
                        " candidate_id=" + candidate.candidate_id +
                        " match=" + (sameCase ? "case_id" : "script_image_target") +
                        " source=" + candidate.source_evidence_script_path +
                        " original_script=" + original.script_path +
                        " image_id=" + original.image_id +
                        " target_id=" + original.target_id);
                return true;
            }
        }
        return false;
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

        const std::string bindingTool =
            NormalizeEvidenceToolTypeLocal(ReadJsonStringFieldLocal(binding, "tool"));

        bool gaugeAccepted = false;
        std::string gaugeReviewStatus;
        bool candidateGaugeValid = false;
        std::string candidateGaugeInvalidReason;
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
            candidateGaugeValid = ValidateCandidateGaugeAnnotationLocal(
                gaugePath,
                bindingTool,
                candidateGaugeInvalidReason);
        }
        const bool humanConfirmed =
            candidateGaugeValid &&
            gaugeAccepted &&
            gaugeReviewStatus == "manual_accepted";

        ScriptEvidenceThumb thumb;
        thumb.is_candidate = candidateGaugeValid;
        thumb.candidate_id = candidateId;
        thumb.candidate_dir = bindingPath.parent_path().string();
        thumb.evidence_binding_path = bindingPath.string();
        thumb.parameter_snapshot_path =
            ReadJsonStringFieldLocal(binding, "parameter_snapshot_path");
        thumb.runtime_globals_path =
            candidateGaugeValid
                ? ReadJsonStringFieldLocal(binding, "runtime_globals_path")
                : std::string();
        thumb.gauge_annotation_path =
            candidateGaugeValid
                ? ReadJsonStringFieldLocal(binding, "gauge_annotation_path")
                : std::string();
        thumb.working_script_snapshot_path =
            candidateGaugeValid ? scriptSnapshot : std::string();
        thumb.has_saved_state = candidateGaugeValid;
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
        thumb.tool = bindingTool;
        thumb.parameter_summary =
            ReadJsonStringFieldLocal(binding, "parameter_summary");
        if (!candidateGaugeValid)
        {
            thumb.status = "invalid_saved_candidate";
            thumb.evidence_category_override = "Defect";
            thumb.reason =
                "saved evidence candidate is not restorable: " +
                candidateGaugeInvalidReason +
                "; candidate_id=" + candidateId +
                "; candidate_dir=" + thumb.candidate_dir;
            CXLOG_WARN(
                "EvidenceChain",
                "candidate_package_invalid",
                "invalid_gauge",
                "case_id=" + caseId +
                    " candidate_id=" + candidateId +
                    " tool=" + thumb.tool +
                    " reason=" + candidateGaugeInvalidReason);
        }
        else
        {
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
        }

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
        const bool reboundToOriginal =
            candidateGaugeValid && bindWorkingRevisionToOriginal(thumb);

        if (reboundToOriginal &&
            EvidenceThumbLooksLikeFindEllipseLocal(thumb, ScriptEvidenceGroup{}))
        {
            continue;
        }

        const std::string candidateTool = NormalizeEvidenceToolTypeLocal(thumb.tool);
        ScriptEvidenceGroup& group = findGroup(
            (candidateTool.empty() ? std::string("Unknown") : candidateTool) +
            (!candidateGaugeValid
                ? " / Invalid Saved Candidates"
                : humanConfirmed
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

static bool LooksLikeCxScriptPathLocal(const std::string& value)
{
    if (value.empty())
        return false;
    if (value.find('/') != std::string::npos ||
        value.find('\\') != std::string::npos)
        return true;

    std::string lower = value;
    std::transform(lower.begin(), lower.end(), lower.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return lower.size() >= 5 &&
        lower.compare(lower.size() - 5, 5, ".cxsc") == 0;
}

static std::string DeriveEvidenceScriptIdLocal(const std::string& scriptValue)
{
    if (scriptValue.empty())
        return {};
    if (!LooksLikeCxScriptPathLocal(scriptValue))
        return scriptValue;

    std::filesystem::path path(scriptValue);
    std::string id = path.stem().string();
    return id.empty() ? scriptValue : id;
}

static std::string ResolveEvidenceChainScriptPathLocal(
    const ManualTestContext& context,
    const std::string& scriptValue)
{
    if (scriptValue.empty())
        return {};
    if (LooksLikeCxScriptPathLocal(scriptValue))
        return scriptValue;

    for (const auto& entry : context.catalog_entries)
    {
        if (entry.script_id == scriptValue)
            return entry.path;
    }
    return {};
}

static std::string ResolveEvidenceChainImagePathLocal(
    const ManualTestContext& context,
    const std::string& imageId)
{
    if (imageId.empty())
        return {};
    for (const auto& item : context.image_manifest_items)
    {
        if (item.image_id == imageId && !item.image_path.empty())
            return item.image_path;
    }
    return ResolveEvidenceImagePathFromContextLocal(context, imageId);
}

static bool HasEvidenceChainThumbIdentityLocal(
    const ManualTestContext& context,
    const ScriptEvidenceThumb& candidate)
{
    for (const auto& group : context.script_evidence_groups)
    {
        for (const auto& thumb : group.thumbs)
        {
            const bool sameCase =
                !candidate.case_id.empty() &&
                thumb.case_id == candidate.case_id;
            const bool sameScript =
                !candidate.script_id.empty() &&
                thumb.script_id == candidate.script_id;
            const bool samePath =
                !candidate.script_path.empty() &&
                thumb.script_path == candidate.script_path;
            const bool sameImageTarget =
                candidate.image_id == thumb.image_id &&
                candidate.target_id == thumb.target_id;

            if (sameCase && (sameScript || samePath || sameImageTarget))
                return true;
        }
    }
    return false;
}

static std::string EvidenceThumbCaseNameLocal(const ScriptEvidenceThumb& thumb)
{
    std::string value;
    if (!thumb.case_id.empty())
        value = thumb.case_id;
    else if (!thumb.source_case_id.empty())
        value = thumb.source_case_id;
    else if (!thumb.script_id.empty())
        value = thumb.script_id;
    else if (!thumb.script_path.empty())
        value = std::filesystem::path(thumb.script_path).stem().string();

    const std::size_t candidateSuffix = value.find(" [");
    if (candidateSuffix != std::string::npos)
        value.erase(candidateSuffix);
    return value;
}

static std::string LowerEvidenceKeyLocal(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

static bool EvidenceThumbLooksLikeFindEllipseLocal(
    const ScriptEvidenceThumb& thumb,
    const ScriptEvidenceGroup& group)
{
    const std::string normalizedTool =
        NormalizeEvidenceToolTypeLocal(thumb.tool.empty() ? group.label : thumb.tool);
    if (normalizedTool == "FindEllipse")
        return true;

    const std::string key = LowerEvidenceKeyLocal(
        thumb.case_id + " " +
        thumb.source_case_id + " " +
        thumb.script_id + " " +
        thumb.script_path + " " +
        thumb.source_evidence_script_path + " " +
        thumb.reason + " " +
        group.label);
    return key.find("findellipse") != std::string::npos ||
        key.find("find_ellipse") != std::string::npos;
}

static std::string FindEllipseCaseDedupeKeyLocal(
    const ScriptEvidenceThumb& thumb)
{
    std::string caseName = EvidenceThumbCaseNameLocal(thumb);
    if (caseName.empty())
        caseName = thumb.case_id;
    if (caseName.empty())
        caseName = thumb.source_case_id;
    if (caseName.empty())
        caseName = thumb.script_id;
    if (caseName.empty())
        caseName = thumb.script_path;
    if (caseName.empty())
        caseName = thumb.source_evidence_script_path;
    if (caseName.empty())
        caseName = thumb.image_id + "|" + thumb.target_id;
    return LowerEvidenceKeyLocal(caseName);
}

static bool FindEllipseThumbIsVerifiedLocal(const ScriptEvidenceThumb& thumb)
{
    const std::string key = LowerEvidenceKeyLocal(
        thumb.evidence_category_override + " " + thumb.status + " " +
        thumb.reason);
    return key.find("verified") != std::string::npos;
}

static int FindEllipseThumbRestoreScoreLocal(const ScriptEvidenceThumb& thumb)
{
    int score = 0;
    if (thumb.has_saved_state)
        score += 1000;
    if (thumb.is_candidate)
        score += 300;
    if (!thumb.runtime_globals_path.empty())
        score += 250;
    if (!thumb.gauge_annotation_path.empty())
        score += 250;
    if (!thumb.working_script_snapshot_path.empty())
        score += 250;
    if (!thumb.candidate_dir.empty())
        score += 150;
    if (!thumb.evidence_binding_path.empty())
        score += 120;
    if (!thumb.parameter_snapshot_path.empty())
        score += 80;
    if (FindEllipseThumbIsVerifiedLocal(thumb))
        score += 70;
    if (thumb.parameter_summary.find('=') != std::string::npos)
        score += 40;
    if (!thumb.image_path.empty())
        score += 20;
    if (!thumb.script_path.empty())
        score += 10;
    return score;
}

static void MergeFindEllipseThumbPayloadLocal(
    ScriptEvidenceThumb& dst,
    const ScriptEvidenceThumb& src)
{
    auto copyIfEmpty = [](std::string& target, const std::string& value)
    {
        if (target.empty() && !value.empty())
            target = value;
    };

    copyIfEmpty(dst.candidate_id, src.candidate_id);
    copyIfEmpty(dst.candidate_dir, src.candidate_dir);
    copyIfEmpty(dst.evidence_binding_path, src.evidence_binding_path);
    copyIfEmpty(dst.parameter_snapshot_path, src.parameter_snapshot_path);
    copyIfEmpty(dst.runtime_globals_path, src.runtime_globals_path);
    copyIfEmpty(dst.gauge_annotation_path, src.gauge_annotation_path);
    copyIfEmpty(dst.working_script_snapshot_path,
                src.working_script_snapshot_path);
    copyIfEmpty(dst.source_evidence_script_path,
                src.source_evidence_script_path);
    copyIfEmpty(dst.case_id, src.case_id);
    copyIfEmpty(dst.script_id, src.script_id);
    copyIfEmpty(dst.script_path, src.script_path);
    copyIfEmpty(dst.image_id, src.image_id);
    copyIfEmpty(dst.image_path, src.image_path);
    copyIfEmpty(dst.thumbnail_path, src.thumbnail_path);
    copyIfEmpty(dst.target_id, src.target_id);
    copyIfEmpty(dst.tool, src.tool);
    copyIfEmpty(dst.parameter_summary, src.parameter_summary);
    copyIfEmpty(dst.evidence_output_root, src.evidence_output_root);
    copyIfEmpty(dst.contract_id, src.contract_id);
    copyIfEmpty(dst.expected_result, src.expected_result);
    copyIfEmpty(dst.expected_policy_guard, src.expected_policy_guard);
    copyIfEmpty(dst.evidence_level, src.evidence_level);
    copyIfEmpty(dst.evidence_case_role, src.evidence_case_role);
    copyIfEmpty(dst.source_case_id, src.source_case_id);
    copyIfEmpty(dst.primary_object_type, src.primary_object_type);
    copyIfEmpty(dst.primary_object_name, src.primary_object_name);
    copyIfEmpty(dst.primary_object_status, src.primary_object_status);

    if (dst.dataset_images.empty() && !src.dataset_images.empty())
        dst.dataset_images = src.dataset_images;
    if (dst.annotations.empty() && !src.annotations.empty())
        dst.annotations = src.annotations;

    dst.has_saved_state = dst.has_saved_state || src.has_saved_state;
    dst.is_candidate = dst.is_candidate || src.is_candidate;
    dst.manual_review_required = dst.manual_review_required ||
        src.manual_review_required;
    dst.promotion_candidate = dst.promotion_candidate ||
        src.promotion_candidate;

    if (FindEllipseThumbIsVerifiedLocal(src))
    {
        dst.evidence_category_override = "Verified";
        dst.status = "verified";
        if (dst.reason.empty() ||
            dst.reason.find("manual category:") != std::string::npos)
        {
            dst.reason = src.reason.empty()
                ? "manual category: verified"
                : src.reason;
        }
    }
    else
    {
        copyIfEmpty(dst.evidence_category_override,
                    src.evidence_category_override);
        copyIfEmpty(dst.status, src.status);
        copyIfEmpty(dst.reason, src.reason);
    }
}

static void PruneFindEllipseDuplicateCasesByNameLocal(ManualTestContext& context)
{
    std::vector<std::string> caseOrder;
    std::unordered_map<std::string, ScriptEvidenceThumb> bestThumbByCase;
    std::unordered_map<std::string, int> bestScoreByCase;

    for (const ScriptEvidenceGroup& group : context.script_evidence_groups)
    {
        for (const ScriptEvidenceThumb& thumb : group.thumbs)
        {
            if (!EvidenceThumbLooksLikeFindEllipseLocal(thumb, group))
                continue;

            const std::string caseKey = FindEllipseCaseDedupeKeyLocal(thumb);
            if (caseKey.empty())
                continue;

            auto found = bestThumbByCase.find(caseKey);
            if (found == bestThumbByCase.end())
            {
                if (caseOrder.size() >= 3)
                    continue;
                caseOrder.push_back(caseKey);
                bestThumbByCase[caseKey] = thumb;
                bestScoreByCase[caseKey] =
                    FindEllipseThumbRestoreScoreLocal(thumb);
                continue;
            }

            const int score = FindEllipseThumbRestoreScoreLocal(thumb);
            if (score > bestScoreByCase[caseKey])
            {
                ScriptEvidenceThumb merged = thumb;
                MergeFindEllipseThumbPayloadLocal(merged, found->second);
                found->second = std::move(merged);
                bestScoreByCase[caseKey] = score;
            }
            else
            {
                MergeFindEllipseThumbPayloadLocal(found->second, thumb);
            }
        }
    }

    std::unordered_map<std::string, bool> emitted;
    for (ScriptEvidenceGroup& group : context.script_evidence_groups)
    {
        std::vector<ScriptEvidenceThumb> kept;
        kept.reserve(group.thumbs.size());

        for (ScriptEvidenceThumb& thumb : group.thumbs)
        {
            if (!EvidenceThumbLooksLikeFindEllipseLocal(thumb, group))
            {
                kept.push_back(std::move(thumb));
                continue;
            }

            const std::string caseKey = FindEllipseCaseDedupeKeyLocal(thumb);
            auto best = bestThumbByCase.find(caseKey);
            if (best == bestThumbByCase.end() || emitted[caseKey])
                continue;

            kept.push_back(best->second);
            emitted[caseKey] = true;
        }

        group.thumbs.swap(kept);
    }
}


static int AppendCxScriptEvidenceChainFilesLocal(
    ManualTestContext& context,
    const std::function<ScriptEvidenceGroup&(const std::string&)>& findGroup,
    const std::vector<std::string>& fallbackImages,
    std::unordered_map<std::string, std::size_t>& fallbackImageIndexByPool,
    std::string& reason)
{
    reason.clear();
    const std::filesystem::path root = ResolveWorkspaceFile(
        "cxparser/cxscript/module/cximage/evidence");
    std::error_code ec;
    if (!std::filesystem::is_directory(root, ec))
    {
        reason = "cxscript evidence chain root not found: " + root.string();
        return 0;
    }

    std::vector<std::filesystem::path> files;
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
        if (path.extension() == ".cxsc")
            files.push_back(path);
    }

    std::stable_sort(
        files.begin(),
        files.end(),
        [](const std::filesystem::path& left,
           const std::filesystem::path& right)
        {
            return left.string() < right.string();
        });

    int appended = 0;
    int loadedFiles = 0;
    std::vector<std::string> errors;
    for (const auto& file : files)
    {
        CxScriptEvidenceChainRuntime chain;
        std::string loadReason;
        if (!LoadCxScriptEvidenceChainFile(file.string(), chain, loadReason))
        {
            errors.push_back(file.filename().string() + ": " + loadReason);
            continue;
        }
        ++loadedFiles;

        for (const CxScriptEvidenceCase& c : chain.cases)
        {
            const auto existingCase = std::find_if(
                context.evidence_items.begin(),
                context.evidence_items.end(),
                [&](const ManualEvidenceItem& item)
                {
                    return item.case_id == c.evidence_id;
                });
            if (existingCase == context.evidence_items.end())
            {
                ManualEvidenceItem item;
                item.case_id = c.evidence_id;
                item.level = c.level;
                item.image_id = c.image_id;
                item.target_id = c.target_id;
                item.tool = NormalizeEvidenceToolTypeLocal(c.tool);
                item.script_id = DeriveEvidenceScriptIdLocal(c.script_id);
                item.parameter_profile_id = c.parameter_profile_id;
                item.gauge_status =
                    c.annotations.empty() ? "unannotated" : "annotated";
                item.probe_status = "pending";
                item.contract_status =
                    c.contract_id.empty() ? "missing" : "pending";
                item.review_status = c.manual_review_required
                    ? "pending_human_review"
                    : "unreviewed";
                item.image_path =
                    ResolveEvidenceChainImagePathLocal(context, c.image_id);
                item.source_evidence_chain_path = file.string();
                context.evidence_items.push_back(std::move(item));
            }
            ScriptEvidenceThumb thumb;
            thumb.case_id = c.evidence_id;
            thumb.script_id = DeriveEvidenceScriptIdLocal(c.script_id);
            thumb.script_path =
                ResolveEvidenceChainScriptPathLocal(context, c.script_id);
            thumb.source_evidence_script_path = file.string();
            thumb.image_id = c.image_id;
            thumb.image_path =
                ResolveEvidenceChainImagePathLocal(context, c.image_id);
            thumb.thumbnail_path = thumb.image_path;
            thumb.target_id = c.target_id;
            thumb.tool = NormalizeEvidenceToolTypeLocal(c.tool);
            thumb.parameter_summary = c.parameter_profile_id;
            thumb.evidence_output_root = chain.output_root;
            thumb.contract_id = c.contract_id;
            thumb.expected_result = c.expected_result;
            thumb.expected_policy_guard = c.expected_policy_guard;
            thumb.evidence_level = c.level;
            thumb.evidence_case_role = c.case_role;
            thumb.source_case_id = c.source_case_id;
            thumb.manual_review_required = c.manual_review_required;
            thumb.promotion_candidate = c.promotion_candidate;
            thumb.evidence_category_override = c.display_category;
            thumb.status = c.manual_review_required
                ? "pending_human_review"
                : "ready";
            thumb.reason =
                "cxscript evidence chain: " + chain.chain_id +
                "; role=" + c.case_role +
                "; expected=" + c.expected_result +
                "; policy=" + c.expected_policy_guard;

            for (const CxScriptEvidenceDatasetImage& image : c.dataset_images)
            {
                CxEvidenceDatasetImageBinding binding;
                binding.image_id = image.image_id;
                binding.image_path = image.image_path;
                binding.split = image.split;
                binding.label = image.label;
                binding.source = image.source;
                thumb.dataset_images.push_back(std::move(binding));
            }

            for (const CxScriptEvidenceAnnotation& annotation : c.annotations)
            {
                CxEvidenceAnnotationBinding binding;
                binding.image_id = annotation.image_id;
                binding.shape_kind = annotation.shape_kind;
                binding.semantic_role = annotation.semantic_role;
                binding.owner_binding = annotation.owner_binding;
                binding.label = annotation.label;
                binding.class_id = annotation.class_id;
                binding.x0 = annotation.x0;
                binding.y0 = annotation.y0;
                binding.x1 = annotation.x1;
                binding.y1 = annotation.y1;
                binding.normalized = annotation.normalized;
                thumb.annotations.push_back(std::move(binding));
            }

            if (thumb.parameter_summary.empty() ||
                thumb.parameter_summary.find('=') == std::string::npos)
            {
                thumb.parameter_summary =
                    BuildDefaultEvidenceParamSummaryForScript(thumb.script_path);
            }

            ApplyHDReferenceImageBindingLocal(thumb);
            AssignFallbackImageToThumb(
                thumb,
                fallbackImages,
                fallbackImageIndexByPool);
            PopulateEditableObjectBindingForThumbLocal(thumb);

            if (HasEvidenceChainThumbIdentityLocal(context, thumb))
                continue;

            const std::string groupLabel = c.display_group.empty()
                ? InferEvidenceChainToolBucketLocal(
                    thumb.tool,
                    thumb.script_id,
                    thumb.script_path,
                    thumb.tool.empty() ? chain.chain_name : thumb.tool,
                    thumb.status,
                    thumb.reason,
                    thumb.parameter_summary)
                : c.display_group;
            findGroup(groupLabel).thumbs.push_back(std::move(thumb));
            ++appended;
        }
    }

    std::ostringstream oss;
    oss << "cxscript evidence chains loaded files=" << loadedFiles
        << " appended_cases=" << appended;
    if (!errors.empty())
        oss << " skipped=" << errors.size() << " first_error=" << errors.front();
    reason = oss.str();
    return appended;
}

void ViewController::EnsureCxScriptWorkbenchAssetsLoaded()
{
  if (m_manualTest.script_evidence_groups_dirty == false)
    return;

  LoadEvidenceCategoryOverridesLocal(m_manualTest);

  EnsureStructuredCxImageCatalogEntriesLoaded(m_manualTest);
  m_manualTest.evidence_items.erase(
      std::remove_if(
          m_manualTest.evidence_items.begin(),
          m_manualTest.evidence_items.end(),
          [](const ManualEvidenceItem& item)
          {
            return !item.source_evidence_chain_path.empty();
          }),
      m_manualTest.evidence_items.end());

  for (auto& group : m_manualTest.script_evidence_groups)
  {
    for (auto& thumb : group.thumbs)
      ResetEvidenceThumbTexture(thumb);
  }
  m_manualTest.script_evidence_groups.clear();

  const std::vector<std::string> fallbackImages =
      BuildEvidenceFallbackImageCandidates(m_manualTest);

  std::unordered_map<std::string, std::size_t> fallbackImageIndexByPool;

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

    AssignFallbackImageToThumb(
        thumb,
        fallbackImages,
        fallbackImageIndexByPool);
    PopulateEditableObjectBindingForThumbLocal(thumb);

    group.thumbs.push_back(thumb);
  }

  {
    std::string evidenceChainReason;
    AppendCxScriptEvidenceChainFilesLocal(
        m_manualTest,
        [&](const std::string& label) -> ScriptEvidenceGroup&
        {
          return findOrCreateGroup("", "", label);
        },
        fallbackImages,
        fallbackImageIndexByPool,
        evidenceChainReason);
    if (!evidenceChainReason.empty())
    {
      if (!m_manualTest.debug_reason.empty() &&
          m_manualTest.debug_reason != "not started")
      {
        m_manualTest.debug_reason += "; ";
      }
      else
      {
        m_manualTest.debug_reason.clear();
      }
      m_manualTest.debug_reason += evidenceChainReason;
    }
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
    const bool isDescriptorEvidence =
        normalizedTool == "RegionPatternTool" ||
        normalizedTool == "GridPatternClassTool" ||
        entry.expected_result == "descriptor_available" ||
        entry.expected_result == "grid_feature_available";
    bool isVisible = entry.manual_visible && entry.frozen &&
        (entry.expected_result == "ok" ||
         entry.expected_result == "ng_expected" ||
         isSmokeEvidence ||
         isDescriptorEvidence);
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

    ApplyHDReferenceImageBindingLocal(thumb);
    AssignFallbackImageToThumb(
        thumb,
        fallbackImages,
        fallbackImageIndexByPool);
    PopulateEditableObjectBindingForThumbLocal(thumb);

    group.thumbs.push_back(thumb);
  }

  {
    std::string handoffReason;
    AppendManualAlgorithmReviewHandoffsFromRunFoldersLocal(
        m_manualTest,
        [this](const std::string& imageId) -> std::string
        {
          return ResolveImagePathFromManifest(imageId);
        },
        [&](const std::string& label) -> ScriptEvidenceGroup&
        {
          return findOrCreateGroup("", "", label);
        },
        handoffReason);
    if (!handoffReason.empty())
    {
      if (!m_manualTest.debug_reason.empty() &&
          m_manualTest.debug_reason != "not started")
      {
        m_manualTest.debug_reason += "; ";
      }
      else
      {
        m_manualTest.debug_reason.clear();
      }
      m_manualTest.debug_reason += handoffReason;
    }
  }

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

  {
    for (const auto& item : m_scriptCatalog)
    {
      if (!IsAllowedEvidenceFallbackScript(item.path))
        continue;

      const bool isDirectLike =
          item.name.find("direct_test") != std::string::npos ||
          item.name.find("_direct") != std::string::npos ||
          item.name.find("_smoke") != std::string::npos ||
          item.type == "GridPatternClassTool" ||
          item.type == "RegionPatternTool" ||
          item.path.find("grid_pattern_class_evidence") != std::string::npos ||
          item.path.find("region_pattern_evidence") != std::string::npos ||
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

      ApplyHDReferenceImageBindingLocal(thumb);

      if (thumb.parameter_summary.empty() ||
          thumb.parameter_summary.find('=') == std::string::npos)
      {
        thumb.parameter_summary =
            BuildDefaultEvidenceParamSummaryForScript(item.path);
      }

      AssignFallbackImageToThumb(
          thumb,
          fallbackImages,
          fallbackImageIndexByPool);
      PopulateEditableObjectBindingForThumbLocal(thumb);

      group.thumbs.push_back(thumb);
      m_manualTest.script_evidence_groups.push_back(group);
    }
  }

  AppendSavedEvidenceCandidatesLocal(
      m_manualTest,
      [&](const std::string& label) -> ScriptEvidenceGroup&
      {
        return findOrCreateGroup("", "", label);
      });

  PruneFindEllipseDuplicateCasesByNameLocal(m_manualTest);

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

  ++m_manualTest.script_evidence_groups_revision;
  if (m_manualTest.script_evidence_groups_debug_revision !=
      m_manualTest.script_evidence_groups_revision)
  {
    WriteEvidenceChainLoadedElementsDebugLocal(m_manualTest);
    m_manualTest.script_evidence_groups_debug_revision =
        m_manualTest.script_evidence_groups_revision;
  }

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
  for (auto& group : m_manualTest.script_evidence_groups)
  {
    for (auto& thumb : group.thumbs)
      ResetEvidenceThumbTexture(thumb);
  }
  m_manualTest.script_evidence_groups.clear();
  m_manualTest.selected_evidence_group = -1;
  m_manualTest.selected_evidence_thumb = -1;
  m_manualTest.current_evidence_selection = CxEvidenceSelectionSnapshot{};
  m_manualTest.script_evidence_groups_dirty = true;
  m_manualTest.script_evidence_row_refs_dirty = true;
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
  if (thumb.texture_loaded && !thumb.texture_placeholder)
    return;

  if (thumb.texture_failed &&
      thumb.thumbnail_path.empty() &&
      thumb.image_path.empty() &&
      thumb.image_id.empty())
  {
    return;
  }

  if (m_manualTest.script_evidence_thumb_load_count_this_frame >=
      m_manualTest.script_evidence_thumb_load_budget_per_frame)
  {
    return;
  }

  if (thumb.image_path.empty() && !thumb.image_id.empty())
    thumb.image_path = ResolveImagePathFromManifest(thumb.image_id);
  if (thumb.image_path.empty() && !thumb.image_id.empty())
    thumb.image_path =
        ResolveEvidenceImagePathFromContextLocal(m_manualTest, thumb.image_id);

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
    addPreviewCandidate(
        ResolveEvidenceImagePathFromContextLocal(m_manualTest, thumb.image_id));

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
    const std::filesystem::path resolvedPreview =
        ResolveWorkspaceFile(previewPath).lexically_normal();
    if (!std::filesystem::exists(resolvedPreview))
    {
      lastFailure = "thumbnail image not found: " + resolvedPreview.string();
      continue;
    }

    image = cv::imread(resolvedPreview.string(), cv::IMREAD_COLOR);
    if (!image.empty())
    {
      loadedPreviewPath = resolvedPreview.string();
      break;
    }
    lastFailure = "thumbnail image read failed: " + resolvedPreview.string();
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
  if (thumb.texture_loaded)
    thumb.thumbnail_path = loadedPreviewPath;
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
    if (out.image_path.empty() && !out.image_id.empty())
        out.image_path =
            ResolveEvidenceImagePathFromContextLocal(m_manualTest, out.image_id);

    out.target_id = thumb.target_id;
    out.tool = thumb.tool;

    out.parameter_summary = thumb.parameter_summary;
    out.parameter_profile_id = thumb.parameter_summary;

    out.evidence_output_root = thumb.evidence_output_root;
    out.contract_id = thumb.contract_id;
    out.expected_result = thumb.expected_result;
    out.expected_policy_guard = thumb.expected_policy_guard;
    out.evidence_level = thumb.evidence_level;
    out.evidence_case_role = thumb.evidence_case_role;
    out.source_case_id = thumb.source_case_id;
    out.manual_review_required = thumb.manual_review_required;
    out.promotion_candidate = thumb.promotion_candidate;

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
    out.dataset_images = thumb.dataset_images;
    out.annotations = thumb.annotations;

    if (MigrateLegacyEvidenceSelectionSnapshotLocal(out))
    {
        CXLOG_INFO(
            "EvidenceChain",
            "legacy_candidate_schema_migrated",
            "default_parameter_added",
            "script_id=" + out.script_id +
                " case_id=" + out.case_id +
                " candidate_id=" + out.candidate_id +
                " parameter_summary=" + out.parameter_summary);
    }

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

static bool IsEvidenceSelectionImageSetLocal(
    const CxEvidenceSelectionSnapshot& sel);

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
        " image_path=" + snapshot.image_path +
        " candidate=" + (snapshot.is_candidate ? "true" : "false") +
        " has_saved_state=" +
        (snapshot.has_saved_state ? "true" : "false") +
        " dataset_images=" +
        std::to_string(snapshot.dataset_images.size()) +
        " annotations=" +
        std::to_string(snapshot.annotations.size()) +
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
                ResolveEvidenceImagePathFromContextLocal(
                    m_manualTest,
                    snapshot.image_id);

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

    ManualTestContext staged = m_manualTest;
    // Selecting an Evidence row is an input-context switch, not a runtime
    // result replay.  Clear the previous parser runtime object snapshot and
    // any deferred runtime shape sync before projecting the new input Gauge;
    // otherwise the old tool (notably FastMatch) can republish stale result
    // shapes after the new FindLine/FindCircle Gauge is shown.
    staged.runtime_objects.clear();

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
        if (!resolved.tool.empty())
        {
            const std::string normalizedEvidenceTool =
                NormalizeEvidenceToolTypeLocal(resolved.tool);
            if (normalizedEvidenceTool == "TorchTask")
            {
                // A Torch evidence row does not own a legacy line/circle
                // gauge.  Reset the previous geometry context so Key
                // Parameter Controls follows the selected Torch case instead
                // of silently retaining the last FindLine selection.
                ManualGaugeState torchContext;
                torchContext.case_id = resolved.case_id;
                torchContext.image_id = resolved.image_id;
                torchContext.target_id = resolved.target_id;
                torchContext.tool = "TorchTask";
                torchContext.source = "evidence";
                torchContext.review_status = "editing";
                staged.current_gauge = torchContext;
            }
            else
            {
                staged.current_gauge.tool = normalizedEvidenceTool;
            }
        }
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

        CXLOG_INFO(
            "EvidenceChain",
            "candidate_parameter_state_restored",
            "RESTORED",
            "case_id=" + resolved.case_id +
                " candidate_id=" + resolved.candidate_id +
                " circle_arc_enabled=" +
                std::to_string(staged.current_gauge.circle_arc_enabled ? 1 : 0) +
                " circle_arc_start_deg=" +
                std::to_string(staged.current_gauge.circle_arc_start_deg) +
                " circle_arc_end_deg=" +
                std::to_string(staged.current_gauge.circle_arc_end_deg) +
                " findcircle_selected_edge=" +
                std::to_string(staged.findcircle_selected_scan_edge) +
                " findcircle_edge_count=" +
                std::to_string(staged.findcircle_scan_edge_count) +
                " findline_selected_edge=" +
                std::to_string(staged.findline_selected_scan_edge) +
                " findline_edge_count=" +
                std::to_string(staged.findline_scan_edge_count));

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
                return abortSelection(
                    "parameter_summary_apply",
                    "failed to apply evidence locked parameters: " +
                        lockedParamReason);
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
        " image_path=" + resolved.image_path +
        " target=" + resolved.target_id +
        " parameter_source=" + parameterSource +
        ((resolved.is_candidate || loadWorkingRevision)
            ? " candidate_id=" + resolved.candidate_id
            : " baseline_evidence=true");

    const bool shouldSyncTrainingImageSet =
        IsEvidenceSelectionImageSetLocal(resolved) ||
        !resolved.dataset_images.empty();

    m_manualTest = std::move(staged);
    m_runtimeShapeSyncPending = false;
    m_runtimeShapeSyncReason.clear();
    m_runtimeShapeSyncDeferCount = 0;
    m_scriptResult.result_ref.clear();
    m_scriptResult.overlay_ref.clear();
    m_scriptResult.evidence_ref.clear();
    m_scriptResult.issue_entry_ref.clear();
    m_scriptResult.runtime_fillback_status =
        "evidence_selection_cleared_previous_runtime";

    if (shouldSyncTrainingImageSet)
    {
        CXLOG_INFO(
            "TorchTrainingImageSet",
            "evidence_image_set_detected",
            "sync_begin",
            "script_id=" + resolved.script_id +
            " case_id=" + resolved.case_id +
            " image_id=" + resolved.image_id);
        SyncTorchTrainingImageSetFromEvidenceSelection();
        CXLOG_INFO(
            "TorchTrainingImageSet",
            "evidence_image_set_synced",
            "sync_done",
            "script_id=" + resolved.script_id +
            " case_id=" + resolved.case_id +
            " image_count=" +
            std::to_string(m_manualTest.torch_training_images.size()) +
            " reason=" + m_manualTest.torch_training_image_reason);
    }
    else
    {
        CXLOG_INFO(
            "EvidenceChain",
            "evidence_single_image_selection",
            "no_training_set_sync",
            "script_id=" + resolved.script_id +
            " case_id=" + resolved.case_id +
            " image_id=" + resolved.image_id);
    }

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
        CXLOG_INFO(
            "ImageView",
            "evidence_image_loaded",
            "loaded",
            "script_id=" + resolved.script_id +
            " case_id=" + resolved.case_id +
            " image_id=" + resolved.image_id +
            " image_path=" + resolvedImagePath.string());

        std::string previewReason;
        const bool previewOk =
            ProjectCurrentGaugeToImageViewPreview(previewReason);
        if (previewOk)
        {
            m_annotationStatus =
                "evidence image and input gauge loaded: " + previewReason;
            CXLOG_INFO(
                "EvidenceChain",
                "evidence_input_gauge_preview",
                "projected",
                "script_id=" + resolved.script_id +
                " case_id=" + resolved.case_id +
                " image_id=" + resolved.image_id +
                " reason=" + previewReason);
        }
        else
        {
            CXLOG_INFO(
                "EvidenceChain",
                "evidence_input_gauge_preview",
                "skipped",
                "script_id=" + resolved.script_id +
                " case_id=" + resolved.case_id +
                " image_id=" + resolved.image_id +
                " reason=" + previewReason);
        }
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
    if (thumb.texture_id != 0)
    {
        GLuint texture = static_cast<GLuint>(thumb.texture_id);
        glDeleteTextures(1, &texture);
    }
    thumb.texture_id = 0;
    thumb.texture_w = 0;
    thumb.texture_h = 0;
    thumb.texture_loaded = false;
    thumb.texture_failed = false;
    thumb.texture_placeholder = false;
}

void ViewController::EnsureTorchTrainingImageTexture(TorchTrainingImageItem& item)
{
    if ((item.texture_loaded && !item.texture_placeholder) ||
        item.texture_failed)
    {
        return;
    }

    if (m_manualTest.script_evidence_thumb_load_count_this_frame >=
        m_manualTest.script_evidence_thumb_load_budget_per_frame)
    {
        return;
    }

    cv::Mat image;
    std::filesystem::path resolved;
    if (!item.image_path.empty())
    {
        resolved = ResolveWorkspaceFile(item.image_path);
        if (std::filesystem::is_regular_file(resolved))
            image = cv::imread(resolved.string(), cv::IMREAD_COLOR);
    }

    if (image.empty())
    {
        cv::Mat placeholder(72, 72, CV_8UC3, cv::Scalar(70, 105, 135));
        cv::putText(
            placeholder,
            "NO",
            cv::Point(17, 32),
            cv::FONT_HERSHEY_SIMPLEX,
            0.5,
            cv::Scalar(255, 255, 255),
            1,
            cv::LINE_AA);
        cv::putText(
            placeholder,
            "IMG",
            cv::Point(12, 52),
            cv::FONT_HERSHEY_SIMPLEX,
            0.45,
            cv::Scalar(255, 255, 255),
            1,
            cv::LINE_AA);
        item.texture_id = CreateTextureFromMat0(placeholder);
        item.texture_w = placeholder.cols;
        item.texture_h = placeholder.rows;
        item.texture_loaded = item.texture_id != 0;
        item.texture_failed = !item.texture_loaded;
        item.texture_placeholder = item.texture_loaded;
        item.status = "image_unavailable";
        ++m_manualTest.script_evidence_thumb_load_count_this_frame;
        return;
    }

    cv::Mat preview;
    const int maxSide = 96;
    const int srcMaxSide = std::max(image.cols, image.rows);
    const double scale = srcMaxSide > 0
        ? static_cast<double>(maxSide) / static_cast<double>(srcMaxSide)
        : 1.0;
    if (scale > 0.0 && scale < 1.0)
        cv::resize(image, preview, cv::Size(), scale, scale, cv::INTER_AREA);
    else
        preview = image;

    item.texture_id = CreateTextureFromMat0(preview);
    item.texture_w = preview.cols;
    item.texture_h = preview.rows;
    item.texture_loaded = item.texture_id != 0;
    item.texture_failed = !item.texture_loaded;
    item.texture_placeholder = false;
    item.status = item.texture_loaded ? "ready" : "texture_failed";
    ++m_manualTest.script_evidence_thumb_load_count_this_frame;
}

void ViewController::AddTorchTrainingImageFromPath(
    const std::string& imagePath,
    const std::string& imageId,
    const std::string& split,
    const std::string& label,
    const std::string& source)
{
    if (imagePath.empty())
    {
        m_manualTest.torch_training_image_status = "ADD_IMAGE_FAIL";
        m_manualTest.torch_training_image_reason = "image path is empty";
        return;
    }

    std::filesystem::path resolved = ResolveWorkspaceFile(imagePath);
    const std::string normalized = resolved.lexically_normal().string();

    for (std::size_t i = 0; i < m_manualTest.torch_training_images.size(); ++i)
    {
        TorchTrainingImageItem& existing = m_manualTest.torch_training_images[i];
        const std::string existingPath =
            ResolveWorkspaceFile(existing.image_path).lexically_normal().string();
        if (existingPath == normalized && existing.split == split)
        {
            existing.image_id = imageId.empty() ? existing.image_id : imageId;
            existing.case_id = m_manualTest.active_case_id;
            existing.target_id = m_manualTest.active_target_id;
            existing.label = label.empty() ? existing.label : label;
            existing.source = source.empty() ? existing.source : source;
            existing.status = std::filesystem::is_regular_file(resolved)
                ? "ready"
                : "image_unavailable";
            m_manualTest.selected_torch_training_image = static_cast<int>(i);
            m_manualTest.torch_training_image_status = "IMAGE_SET_UPDATED";
            m_manualTest.torch_training_image_reason =
                "updated existing " + split + " image: " + normalized;
            return;
        }
    }

    TorchTrainingImageItem item;
    item.image_id = imageId;
    item.image_path = normalized;
    item.case_id = m_manualTest.active_case_id;
    item.target_id = m_manualTest.active_target_id;
    item.split = split.empty() ? "train" : split;
    item.label = label.empty() ? "unlabeled" : label;
    item.source = source.empty() ? "manual" : source;
    item.status = std::filesystem::is_regular_file(resolved)
        ? "ready"
        : "image_unavailable";

    m_manualTest.torch_training_images.push_back(item);
    m_manualTest.selected_torch_training_image =
        static_cast<int>(m_manualTest.torch_training_images.size() - 1);
    m_manualTest.torch_training_image_status = "IMAGE_SET_ADDED";
    m_manualTest.torch_training_image_reason =
        "added " + item.split + " image: " + normalized;
}

static void ClearTorchTrainingImageSetForEvidenceSyncLocal(
    ManualTestContext& context,
    const std::string& reason)
{
    context.torch_training_images.clear();
    context.selected_torch_training_image = -1;
    context.torch_training_new_image_path.clear();
    context.torch_training_image_status = "IMAGE_SET_CLEARED";
    context.torch_training_image_reason = reason;
}

static void ApplyEvidenceAnnotationsToTorchTrainingItemLocal(
    TorchTrainingImageItem& item,
    const std::vector<CxEvidenceAnnotationBinding>& annotations)
{
    item.annotation_shapes.clear();

    int imageW = 0;
    int imageH = 0;
    cv::Mat image = cv::imread(item.image_path, cv::IMREAD_UNCHANGED);
    if (!image.empty())
    {
        imageW = image.cols;
        imageH = image.rows;
    }

    int shapeIndex = 0;
    for (const CxEvidenceAnnotationBinding& annotation : annotations)
    {
        if (!annotation.image_id.empty() &&
            !item.image_id.empty() &&
            annotation.image_id != item.image_id)
        {
            continue;
        }

        double x0 = annotation.x0;
        double y0 = annotation.y0;
        double x1 = annotation.x1;
        double y1 = annotation.y1;
        if (annotation.normalized && imageW > 0 && imageH > 0)
        {
            x0 *= static_cast<double>(imageW);
            x1 *= static_cast<double>(imageW);
            y0 *= static_cast<double>(imageH);
            y1 *= static_cast<double>(imageH);
        }

        if (x1 < x0)
            std::swap(x0, x1);
        if (y1 < y0)
            std::swap(y0, y1);

        TorchTrainingAnnotationShapeSnapshot snap;
        snap.stable_ref =
            item.image_id + "_bbox_" + std::to_string(shapeIndex + 1);
        snap.tool_id = "TorchTask";
        snap.owner_type = "TorchDataset";
        snap.owner_ref = item.image_id;
        snap.owner_binding = annotation.owner_binding.empty()
            ? "label_bbox"
            : annotation.owner_binding;
        snap.semantic_role =
            annotation.semantic_role +
            "_class_" + std::to_string(annotation.class_id);
        snap.shape_kind = annotation.shape_kind.empty()
            ? "RectShape"
            : annotation.shape_kind;
        snap.center_x = (x0 + x1) * 0.5;
        snap.center_y = (y0 + y1) * 0.5;
        snap.radius_x = std::max(0.0, (x1 - x0) * 0.5);
        snap.radius_y = std::max(0.0, (y1 - y0) * 0.5);
        snap.points_xy = { x0, y0, x1, y0, x1, y1, x0, y1 };
        snap.closed = true;
        snap.editable = true;
        snap.visible = true;
        snap.result_element = false;
        item.annotation_shapes.push_back(std::move(snap));
        ++shapeIndex;
    }

    item.annotation_shape_count =
        static_cast<int>(item.annotation_shapes.size());
    item.annotation_overlay_count = item.annotation_shape_count;
    item.annotation_status =
        item.annotation_shape_count > 0 ? "reviewed" : "unlabeled";
    item.annotation_reason =
        item.annotation_shape_count > 0
            ? "loaded from evidence dataset annotations"
            : "no evidence annotation bound for image";
}

static std::unique_ptr<ShapeBase> CreateTorchTrainingShapeFromSnapshotLocal(
    const TorchTrainingAnnotationShapeSnapshot& snap)
{
    std::vector<CxShapePoint> points;
    for (std::size_t i = 1; i < snap.points_xy.size(); i += 2)
        points.push_back({ snap.points_xy[i - 1], snap.points_xy[i] });

    if (snap.shape_kind == "PointsShape")
    {
        auto shape = std::make_unique<PointsShape>();
        for (const auto& p : points)
        {
            gp_Pnt gp(p.x, p.y, 0.0);
            shape->addpoint(gp);
        }
        return shape;
    }

    if (snap.shape_kind == "LineShape" && points.size() >= 2)
    {
        auto shape = std::make_unique<LineShape>();
        shape->setline(
            static_cast<int>(std::lround(points[0].x)),
            static_cast<int>(std::lround(points[0].y)),
            static_cast<int>(std::lround(points[1].x)),
            static_cast<int>(std::lround(points[1].y)));
        return shape;
    }

    if (snap.shape_kind == "LineGaugeShape" && points.size() >= 2)
    {
        return std::make_unique<LineGaugeShape>(
            points[0].x,
            points[0].y,
            points[1].x,
            points[1].y,
            snap.half_width > 0.0 ? snap.half_width : 20.0);
    }

    if (snap.shape_kind == "RectShape")
    {
        if (points.size() >= 4)
        {
            double minX = points[0].x;
            double minY = points[0].y;
            double maxX = points[0].x;
            double maxY = points[0].y;
            for (const auto& p : points)
            {
                minX = std::min(minX, p.x);
                minY = std::min(minY, p.y);
                maxX = std::max(maxX, p.x);
                maxY = std::max(maxY, p.y);
            }
            return std::make_unique<RectShape>(minX, minY, maxX, maxY);
        }
        if (snap.radius_x > 0.0 && snap.radius_y > 0.0)
        {
            return std::make_unique<RectShape>(
                snap.center_x - snap.radius_x,
                snap.center_y - snap.radius_y,
                snap.center_x + snap.radius_x,
                snap.center_y + snap.radius_y);
        }
    }

    if (snap.shape_kind == "CircleShape")
    {
        return std::make_unique<CircleShape>(
            snap.center_x,
            snap.center_y,
            snap.radius > 0.0 ? snap.radius : 20.0,
            snap.inner_radius);
    }

    if (snap.shape_kind == "EllipseShape")
    {
        return std::make_unique<EllipseShape>(
            snap.center_x,
            snap.center_y,
            snap.radius_x > 0.0 ? snap.radius_x : 30.0,
            snap.radius_y > 0.0 ? snap.radius_y : 20.0);
    }

    if (snap.shape_kind == "PolylineShape")
    {
        auto shape = std::make_unique<PolylineShape>();
        for (const auto& p : points)
            shape->addPoint(p.x, p.y);
        shape->close(snap.closed);
        return shape;
    }

    return nullptr;
}

static std::string NormalizeEvidenceImageSetKeyLocal(std::string key)
{
    std::replace(key.begin(), key.end(), '\\', '/');
    const std::size_t slash = key.find_last_of('/');
    if (slash != std::string::npos)
        key = key.substr(slash + 1);
    const std::string suffix = ".cxsc";
    if (key.size() > suffix.size() &&
        key.compare(key.size() - suffix.size(), suffix.size(), suffix) == 0)
    {
        key.resize(key.size() - suffix.size());
    }
    std::transform(key.begin(), key.end(), key.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return key;
}

static bool IsEvidenceSelectionImageSetLocal(
    const CxEvidenceSelectionSnapshot& sel)
{
    std::vector<std::string> keys = {
        sel.script_id,
        sel.script_path,
        sel.case_id,
        sel.source_evidence_script_path
    };

    for (std::string key : keys)
    {
        key = NormalizeEvidenceImageSetKeyLocal(key);
        if (key == "hd_fruit_classification_reference_direct" ||
            key.rfind("torch_resnet18_baseline_", 0) == 0 ||
            key.rfind("torch_resnet50_baseline_", 0) == 0 ||
            key == "hd_juice_bottle_anomaly_reference_direct" ||
            key == "hd_dongle_ocr_reference_direct" ||
            key == "hd_pill_semantic_segmentation_reference_direct" ||
            key == "hd_pill_bag_instance_segmentation_reference_direct" ||
            key == "hd_pill_bag_detection_reference_direct" ||
            key == "hd_screws_oriented_detection_reference_direct")
        {
            return true;
        }
    }

    return false;
}

void ViewController::CaptureCurrentTorchTrainingAnnotationState()
{
    const int selected = m_manualTest.selected_torch_training_image;
    if (selected < 0 ||
        selected >= static_cast<int>(m_manualTest.torch_training_images.size()))
    {
        return;
    }

    TorchTrainingImageItem& item =
        m_manualTest.torch_training_images[static_cast<std::size_t>(selected)];
    if (m_manualTest.image_file_path.empty() || item.image_path.empty())
        return;

    const std::string currentImage =
        ResolveWorkspaceFile(m_manualTest.image_file_path)
            .lexically_normal()
            .string();
    const std::string selectedImage =
        ResolveWorkspaceFile(item.image_path)
            .lexically_normal()
            .string();
    if (currentImage != selectedImage)
        return;

    std::vector<TorchTrainingAnnotationShapeSnapshot> capturedShapes;
    const int capturedOverlayCount =
        static_cast<int>(m_annotationLayer.Elements().size());

    for (const auto& element : m_annotationLayer.ShapeElements())
    {
        if (!element.shape || element.runtime_bound)
            continue;

        CxShapeGeometrySnapshot geo;
        if (!element.shape->snapshot(geo))
            continue;

        TorchTrainingAnnotationShapeSnapshot snap;
        snap.stable_ref = element.stable_ref.empty() ? element.ref : element.stable_ref;
        snap.tool_id = element.tool_id;
        snap.owner_type = element.owner_type;
        snap.owner_ref = element.owner_ref;
        snap.owner_binding = element.owner_binding;
        snap.semantic_role = element.semantic_role;
        snap.shape_kind = CxShapeKindName(geo.kind);
        snap.center_x = geo.center.x;
        snap.center_y = geo.center.y;
        snap.radius = geo.radius;
        snap.inner_radius = geo.inner_radius;
        snap.radius_x = geo.radius_x;
        snap.radius_y = geo.radius_y;
        snap.angle = geo.angle;
        snap.half_width = geo.half_width;
        snap.closed = geo.closed;
        snap.editable = element.editable;
        snap.visible = element.visible;
        snap.result_element = element.result_element;

        for (const auto& p : geo.points)
        {
            snap.points_xy.push_back(p.x);
            snap.points_xy.push_back(p.y);
        }

        capturedShapes.push_back(std::move(snap));
    }

    auto sameShape = [](
        const TorchTrainingAnnotationShapeSnapshot& a,
        const TorchTrainingAnnotationShapeSnapshot& b) -> bool
    {
        return a.stable_ref == b.stable_ref &&
            a.tool_id == b.tool_id &&
            a.owner_type == b.owner_type &&
            a.owner_ref == b.owner_ref &&
            a.owner_binding == b.owner_binding &&
            a.semantic_role == b.semantic_role &&
            a.shape_kind == b.shape_kind &&
            a.center_x == b.center_x && a.center_y == b.center_y &&
            a.radius == b.radius && a.inner_radius == b.inner_radius &&
            a.radius_x == b.radius_x && a.radius_y == b.radius_y &&
            a.angle == b.angle && a.half_width == b.half_width &&
            a.closed == b.closed && a.editable == b.editable &&
            a.visible == b.visible && a.result_element == b.result_element &&
            a.points_xy == b.points_xy;
    };
    bool changed = item.annotation_overlay_count != capturedOverlayCount ||
        item.annotation_shapes.size() != capturedShapes.size();
    if (!changed)
    {
        for (std::size_t i = 0; i < capturedShapes.size(); ++i)
        {
            if (!sameShape(item.annotation_shapes[i], capturedShapes[i]))
            {
                changed = true;
                break;
            }
        }
    }
    if (!changed)
        return;

    item.annotation_shapes = std::move(capturedShapes);
    item.annotation_overlay_count = capturedOverlayCount;
    item.annotation_shape_count = static_cast<int>(item.annotation_shapes.size());
    item.annotation_status =
        item.annotation_shape_count > 0 ? "editing" : "unlabeled";
    CXLOG_INFO(
        "TorchTrainingImageSet",
        "training_image_annotation_captured",
        "captured",
        "index=" + std::to_string(selected) +
        " image_id=" + item.image_id +
        " image_path=" + item.image_path +
        " shapes=" + std::to_string(item.annotation_shape_count) +
        " overlays=" + std::to_string(item.annotation_overlay_count));
}

void ViewController::RestoreTorchTrainingAnnotationState(
    const TorchTrainingImageItem& item)
{
    m_annotationLayer.Clear();
    m_annotationLayer.ClearShapeElements();

    int restored = 0;
    for (std::size_t i = 0; i < item.annotation_shapes.size(); ++i)
    {
        const auto& snap = item.annotation_shapes[i];
        std::unique_ptr<ShapeBase> shape =
            CreateTorchTrainingShapeFromSnapshotLocal(snap);
        if (!shape)
            continue;

        const std::string stableRef =
            snap.stable_ref.empty()
                ? ("torch_training_annotation_" + std::to_string(i + 1))
                : snap.stable_ref;
        m_annotationLayer.UpsertShape(
            stableRef,
            snap.owner_type.empty() ? "TorchTrainingImage" : snap.owner_type,
            snap.owner_ref.empty() ? item.image_id : snap.owner_ref,
            snap.owner_binding.empty() ? "manual_annotation" : snap.owner_binding,
            snap.semantic_role.empty() ? "annotation" : snap.semantic_role,
            snap.editable,
            snap.result_element,
            std::move(shape));
        ++restored;
    }

    m_annotationStatus =
        "torch dataset image annotation restored: shapes=" +
        std::to_string(restored) + " label=" + item.label;
    CXLOG_INFO(
        "TorchTrainingImageSet",
        "training_image_annotation_restored",
        "restored",
        "image_id=" + item.image_id +
        " image_path=" + item.image_path +
        " shapes=" + std::to_string(restored) +
        " label=" + item.label);
}

bool ViewController::LoadTorchTrainingImageIntoAnnotationView(
    int itemIndex,
    std::string& reason)
{
    reason.clear();
    if (itemIndex < 0 ||
        itemIndex >= static_cast<int>(m_manualTest.torch_training_images.size()))
    {
        reason = "torch training image index out of range";
        return false;
    }

    CaptureCurrentTorchTrainingAnnotationState();

    TorchTrainingImageItem& item =
        m_manualTest.torch_training_images[static_cast<std::size_t>(itemIndex)];
    if (!LoadImageIntoImageView(item.image_path, reason))
    {
        CXLOG_ERROR(
            "TorchTrainingImageSet",
            "training_image_load_image_view",
            "load_failed",
            "index=" + std::to_string(itemIndex) +
            " image_id=" + item.image_id +
            " image_path=" + item.image_path +
            " reason=" + reason);
        return false;
    }

    m_manualTest.selected_torch_training_image = itemIndex;
    RestoreTorchTrainingAnnotationState(item);

    m_imageToolEnabled = true;
    m_imageToolMode = ImageToolMode::PointerPan;
    CancelAnnotationCreate();
    m_annotationLayer.SetActiveToolIndex(-1);
    m_manualTest.torch_training_image_status = "ANNOTATION_READY";
    m_manualTest.torch_training_image_reason =
        "loaded dataset image into Image View and enabled annotation: " +
        item.image_path +
        " shapes=" + std::to_string(item.annotation_shape_count);
    CXLOG_INFO(
        "TorchTrainingImageSet",
        "training_image_load_image_view",
        "loaded",
        "index=" + std::to_string(itemIndex) +
        " image_id=" + item.image_id +
        " image_path=" + item.image_path +
        " label=" + item.label +
        " shapes=" + std::to_string(item.annotation_shape_count));
    return true;
}

void ViewController::SyncTorchTrainingImageSetFromEvidenceSelection()
{
    CxEvidenceSelectionSnapshot sel =
        m_manualTest.current_evidence_selection;
    if (!sel.valid)
        return;

    if (sel.dataset_images.empty())
    {
        for (const ScriptEvidenceGroup& group : m_manualTest.script_evidence_groups)
        {
            for (const ScriptEvidenceThumb& thumb : group.thumbs)
            {
                if (thumb.dataset_images.empty())
                    continue;

                const bool sameCase =
                    !sel.case_id.empty() && thumb.case_id == sel.case_id;
                const bool sameScript =
                    !sel.script_id.empty() && thumb.script_id == sel.script_id;
                const bool sameImageAndTool =
                    !sel.image_id.empty() &&
                    !sel.tool.empty() &&
                    thumb.image_id == sel.image_id &&
                    NormalizeEvidenceToolTypeLocal(thumb.tool) ==
                        NormalizeEvidenceToolTypeLocal(sel.tool);

                if (!sameCase && !sameScript && !sameImageAndTool)
                    continue;

                sel.dataset_images = thumb.dataset_images;
                sel.annotations = thumb.annotations;
                CXLOG_INFO(
                    "TorchTrainingImageSet",
                    "evidence_dataset_resolved_from_loaded_thumb",
                    "resolved",
                    "case_id=" + sel.case_id +
                    " script_id=" + sel.script_id +
                    " matched_case=" + thumb.case_id +
                    " matched_script=" + thumb.script_id +
                    " dataset_images=" +
                    std::to_string(sel.dataset_images.size()) +
                    " annotations=" + std::to_string(sel.annotations.size()));
                break;
            }
            if (!sel.dataset_images.empty())
                break;
        }
    }

    ClearTorchTrainingImageSetForEvidenceSyncLocal(
        m_manualTest,
        "rebuild image set from evidence case: " +
            (sel.case_id.empty() ? sel.script_id : sel.case_id));

    if (!sel.dataset_images.empty())
    {
        int added = 0;
        int preferredImageIndex = -1;
        for (const CxEvidenceDatasetImageBinding& binding : sel.dataset_images)
        {
            std::string imagePath = binding.image_path;
            if (imagePath.empty() && !binding.image_id.empty())
                imagePath = ResolveImagePathFromManifest(binding.image_id);
            if (imagePath.empty() && !binding.image_id.empty())
                imagePath =
                    ResolveEvidenceImagePathFromContextLocal(
                        m_manualTest,
                        binding.image_id);
            if (imagePath.empty())
                continue;

            AddTorchTrainingImageFromPath(
                imagePath,
                binding.image_id,
                binding.split,
                binding.label,
                binding.source.empty() ? "evidence_dataset" : binding.source);

            const int selected = m_manualTest.selected_torch_training_image;
            if (selected >= 0 &&
                selected <
                    static_cast<int>(m_manualTest.torch_training_images.size()))
            {
                TorchTrainingImageItem& item =
                    m_manualTest.torch_training_images[
                        static_cast<std::size_t>(selected)];
                item.case_id = sel.case_id;
                item.target_id = sel.target_id;
                ApplyEvidenceAnnotationsToTorchTrainingItemLocal(
                    item,
                    sel.annotations);
                const std::string itemPath =
                    ResolveWorkspaceFile(item.image_path)
                        .lexically_normal()
                        .string();
                const std::string evidencePath =
                    ResolveWorkspaceFile(sel.image_path)
                        .lexically_normal()
                        .string();
                if ((!sel.image_id.empty() && item.image_id == sel.image_id) ||
                    (!evidencePath.empty() && itemPath == evidencePath))
                {
                    preferredImageIndex = selected;
                }
            }
            ++added;
        }

        const int referenceSetCount =
            AddHDReferenceImageSetForCurrentSelection();
        added += referenceSetCount;

        if (preferredImageIndex >= 0)
            m_manualTest.selected_torch_training_image = preferredImageIndex;
        else if (!m_manualTest.torch_training_images.empty())
            m_manualTest.selected_torch_training_image = 0;

        m_manualTest.torch_training_image_status =
            added > 0 ? "EVIDENCE_DATASET_BOUND" : "EVIDENCE_DATASET_EMPTY";
        m_manualTest.torch_training_image_reason =
            "synced evidence dataset images for " + sel.case_id +
            " count=" + std::to_string(added) +
            " annotations=" + std::to_string(sel.annotations.size());
        CXLOG_INFO(
            "TorchTrainingImageSet",
            "evidence_dataset_sync_complete",
            m_manualTest.torch_training_image_status,
            m_manualTest.torch_training_image_reason);
        return;
    }

    std::string imagePath = sel.image_path;
    if (imagePath.empty() && !sel.image_id.empty())
        imagePath = ResolveImagePathFromManifest(sel.image_id);
    if (imagePath.empty() && !sel.image_id.empty())
        imagePath =
            ResolveEvidenceImagePathFromContextLocal(m_manualTest, sel.image_id);
    if (imagePath.empty())
        return;

    std::string label = "unlabeled";
    std::string key = sel.status + " " + sel.reason + " " +
        sel.parameter_summary + " " + sel.tool + " " + sel.script_id;
    std::transform(key.begin(), key.end(), key.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (key.find("anomaly") != std::string::npos ||
        key.find("defect") != std::string::npos ||
        key.find("fail") != std::string::npos ||
        key.find("error") != std::string::npos)
    {
        label = "anomaly";
    }
    else if (key.find("good") != std::string::npos ||
             key.find("ok") != std::string::npos ||
             key.find("success") != std::string::npos ||
             key.find("pass") != std::string::npos)
    {
        label = "good";
    }

    AddTorchTrainingImageFromPath(
        imagePath,
        sel.image_id,
        "train",
        label,
        "evidence");
    const int referenceSetCount =
        AddHDReferenceImageSetForCurrentSelection();
    if (referenceSetCount <= 0)
    {
        m_manualTest.torch_training_image_reason =
            "synced from evidence case: " + sel.case_id;
    }
    CXLOG_INFO(
        "TorchTrainingImageSet",
        "evidence_dataset_sync_fallback_single_image",
        m_manualTest.torch_training_image_status,
        "case_id=" + sel.case_id +
        " script_id=" + sel.script_id +
        " dataset_images=0 annotations=0 reason=" +
        m_manualTest.torch_training_image_reason);
}

static ImU32 TorchDatasetLabelColorLocal(const std::string& label)
{
    if (label == "good")
        return IM_COL32(45, 190, 95, 245);
    if (label == "anomaly")
        return IM_COL32(235, 85, 75, 245);
    if (label == "pending")
        return IM_COL32(235, 180, 55, 245);
    return IM_COL32(95, 135, 180, 245);
}

int ViewController::AddHDReferenceImageSetForCurrentSelection()
{
    const CxEvidenceSelectionSnapshot& sel =
        m_manualTest.current_evidence_selection;
    std::string key = sel.script_id;
    if (key.empty())
        key = sel.case_id;
    std::replace(key.begin(), key.end(), '\\', '/');
    const std::size_t slash = key.find_last_of('/');
    if (slash != std::string::npos)
        key = key.substr(slash + 1);
    const std::string suffix = ".cxsc";
    if (key.size() > suffix.size() &&
        key.compare(key.size() - suffix.size(), suffix.size(), suffix) == 0)
    {
        key.resize(key.size() - suffix.size());
    }
    if (key.rfind("torch_resnet18_baseline_", 0) == 0 ||
        key.rfind("torch_resnet50_baseline_", 0) == 0)
    {
        key = "hd_fruit_classification_reference_direct";
    }

    struct DirBinding
    {
        const char* script_id;
        const char* split;
        const char* label;
        const char* dir;
        int max_count;
    };

    static const DirBinding kDirs[] = {
        {"hd_fruit_classification_reference_direct", "train", "apple_braeburn", "D:/Codex-WorkDir/Sean_WorkDir/images/fruit/apple_braeburn", 16},
        {"hd_fruit_classification_reference_direct", "train", "apple_golden_delicious", "D:/Codex-WorkDir/Sean_WorkDir/images/fruit/apple_golden_delicious", 16},
        {"hd_fruit_classification_reference_direct", "train", "apple_topaz", "D:/Codex-WorkDir/Sean_WorkDir/images/fruit/apple_topaz", 16},
        {"hd_fruit_classification_reference_direct", "train", "peach", "D:/Codex-WorkDir/Sean_WorkDir/images/fruit/peach", 16},
        {"hd_fruit_classification_reference_direct", "train", "pear", "D:/Codex-WorkDir/Sean_WorkDir/images/fruit/pear", 16},

        {"hd_juice_bottle_anomaly_reference_direct", "train", "good", "D:/Codex-WorkDir/Sean_WorkDir/images/juice_bottle/good", 64},
        {"hd_juice_bottle_anomaly_reference_direct", "test", "anomaly", "D:/Codex-WorkDir/Sean_WorkDir/images/juice_bottle/logical_anomaly", 64},
        {"hd_juice_bottle_anomaly_reference_direct", "test", "anomaly", "D:/Codex-WorkDir/Sean_WorkDir/images/juice_bottle/structural_anomaly", 64},

        {"hd_dongle_ocr_reference_direct", "train", "unlabeled", "D:/Codex-WorkDir/Sean_WorkDir/images/dongle", 64},

        {"hd_pill_semantic_segmentation_reference_direct", "train", "anomaly", "D:/Codex-WorkDir/Sean_WorkDir/images/pill/ginseng/contamination", 32},
        {"hd_pill_semantic_segmentation_reference_direct", "train", "anomaly", "D:/Codex-WorkDir/Sean_WorkDir/images/pill/ginseng/crack", 32},
        {"hd_pill_semantic_segmentation_reference_direct", "train", "anomaly", "D:/Codex-WorkDir/Sean_WorkDir/images/pill/magnesium/contamination", 32},
        {"hd_pill_semantic_segmentation_reference_direct", "train", "anomaly", "D:/Codex-WorkDir/Sean_WorkDir/images/pill/magnesium/crack", 32},
        {"hd_pill_semantic_segmentation_reference_direct", "train", "anomaly", "D:/Codex-WorkDir/Sean_WorkDir/images/pill/mint/contamination", 32},
        {"hd_pill_semantic_segmentation_reference_direct", "train", "anomaly", "D:/Codex-WorkDir/Sean_WorkDir/images/pill/mint/crack", 32},

        {"hd_pill_bag_instance_segmentation_reference_direct", "train", "unlabeled", "D:/Codex-WorkDir/Sean_WorkDir/images/pill_bag", 64},
        {"hd_pill_bag_detection_reference_direct", "train", "unlabeled", "D:/Codex-WorkDir/Sean_WorkDir/images/pill_bag", 64},
        {"hd_screws_oriented_detection_reference_direct", "train", "unlabeled", "D:/Codex-WorkDir/Sean_WorkDir/images/screws", 64}
    };

    auto isImageFile = [](const std::filesystem::path& path) -> bool
    {
        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
               ext == ".bmp" || ext == ".tif" || ext == ".tiff";
    };

    int added = 0;
    std::error_code ec;
    for (const DirBinding& binding : kDirs)
    {
        if (key != binding.script_id)
            continue;

        const std::filesystem::path dir =
            ResolveWorkspaceFile(binding.dir).lexically_normal();
        if (!std::filesystem::is_directory(dir, ec))
            continue;

        std::vector<std::filesystem::path> files;
        for (std::filesystem::directory_iterator it(dir, ec), end;
             !ec && it != end;
             it.increment(ec))
        {
            if (it->is_regular_file(ec) && isImageFile(it->path()))
                files.push_back(it->path());
        }
        std::sort(files.begin(), files.end());

        int count = 0;
        for (const auto& path : files)
        {
            if (count >= binding.max_count)
                break;
            AddTorchTrainingImageFromPath(
                path.string(),
                path.stem().string(),
                binding.split,
                binding.label,
                "hd_reference_set");
            ++count;
            ++added;
        }
    }

    if (added > 0)
    {
        m_manualTest.torch_training_image_status =
            "HD_REFERENCE_IMAGE_SET_LOADED";
        m_manualTest.torch_training_image_reason =
            "loaded reference image set for " + key +
            " count=" + std::to_string(added);
    }

    return added;
}

void ViewController::DrawTorchTrainingImageRail(const char* split, const char* label)
{
    ImGui::Text("%s", label);
    ImGui::BeginChild(
        (std::string("torch_dataset_rail_") + split).c_str(),
        ImVec2(-1, 112.0f),
        true,
        ImGuiWindowFlags_HorizontalScrollbar);

    ImGui::TextUnformatted("<");
    ImGui::SameLine();

    bool any = false;
    const ImVec2 thumbSize(76.0f, 76.0f);
    for (std::size_t i = 0; i < m_manualTest.torch_training_images.size(); ++i)
    {
        TorchTrainingImageItem& item = m_manualTest.torch_training_images[i];
        if (item.split != split)
            continue;
        any = true;
        EnsureTorchTrainingImageTexture(item);

        ImGui::PushID(static_cast<int>(i));
        const bool selected =
            m_manualTest.selected_torch_training_image == static_cast<int>(i);
        if (selected)
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(30, 140, 210, 255));

        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        bool clicked = false;
        if (item.texture_id != 0)
        {
            clicked = ImGui::ImageButton(
                "torch_dataset_thumb",
                static_cast<ImU64>(item.texture_id),
                thumbSize);
        }
        else
        {
            clicked = ImGui::Button("NO IMG", thumbSize);
        }
        const bool itemHovered = ImGui::IsItemHovered();

        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImVec2 p1(p0.x + thumbSize.x, p0.y + thumbSize.y);
        draw->AddRect(p0, p1,
                      selected ? IM_COL32(0, 190, 255, 255) : IM_COL32(0, 120, 180, 180),
                      6.0f,
                      0,
                      selected ? 3.0f : 1.5f);
        const std::string badge = item.label.empty() ? "unlabeled" : item.label;
        const ImVec2 badgeMin(p0.x + 3.0f, p1.y - 20.0f);
        const ImVec2 badgeMax(p1.x - 3.0f, p1.y - 3.0f);
        draw->AddRectFilled(badgeMin, badgeMax, TorchDatasetLabelColorLocal(badge), 4.0f);
        draw->AddText(ImVec2(badgeMin.x + 4.0f, badgeMin.y + 2.0f),
                      IM_COL32(255, 255, 255, 255),
                      badge.c_str());
        if (item.annotation_shape_count > 0)
        {
            const std::string shapeBadge =
                "S" + std::to_string(item.annotation_shape_count);
            draw->AddRectFilled(
                ImVec2(p1.x - 28.0f, p0.y + 3.0f),
                ImVec2(p1.x - 3.0f, p0.y + 19.0f),
                IM_COL32(80, 170, 255, 220),
                4.0f);
            draw->AddText(
                ImVec2(p1.x - 24.0f, p0.y + 4.0f),
                IM_COL32(255, 255, 255, 255),
                shapeBadge.c_str());
        }

        if (clicked)
        {
            CXLOG_INFO(
                "TorchTrainingImageSet",
                "training_thumb_click",
                "ui_event",
                "index=" + std::to_string(i) +
                " split=" + item.split +
                " label=" + item.label +
                " image_id=" + item.image_id +
                " image_path=" + item.image_path);
            std::string reason;
            if (!LoadTorchTrainingImageIntoAnnotationView(
                    static_cast<int>(i),
                    reason))
            {
                m_manualTest.debug_status = "TORCH_DATASET_IMAGE_LOAD_FAIL";
                m_manualTest.debug_reason = reason;
            }
            else
            {
                m_manualTest.debug_status = "TORCH_DATASET_IMAGE_LOADED";
                m_manualTest.debug_reason = item.image_path;
            }
        }
        if (itemHovered)
        {
            ImGui::SetTooltip(
                "%s\nsplit=%s label=%s annotation=%s shapes=%d\ncase=%s\nsource=%s",
                item.image_path.c_str(),
                item.split.c_str(),
                item.label.c_str(),
                item.annotation_status.c_str(),
                item.annotation_shape_count,
                item.case_id.c_str(),
                item.source.c_str());
        }

        if (selected)
            ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PopID();
    }

    if (!any)
    {
        ImGui::TextDisabled("No %s images yet.", split);
        ImGui::SameLine();
    }
    ImGui::TextUnformatted(">");
    ImGui::EndChild();
}

bool ViewController::ExportTorchTrainingLabelPackage(
    std::string& packagePath,
    std::string& reason)
{
    packagePath.clear();
    reason.clear();

    // This package is deliberately value-only.  It is evidence for the next
    // label-adapter stage, not a claim that the current Tiny Smoke consumed a
    // dataset or produced semantic masks.
    std::string runId = CxUnifiedLog::Instance().RunId();
    if (runId.empty())
        runId = "ui_session";
    const std::filesystem::path outputDir = ResolveCxVisionRunPath(
        "cxscript_runs/manual_torch_dataset") / runId;
    std::error_code ec;
    std::filesystem::create_directories(outputDir, ec);
    if (ec)
    {
        reason = "cannot create Torch training label package directory: " +
            outputDir.string() + " reason=" + ec.message();
        return false;
    }

    int imageCount = 0;
    int imageMissingCount = 0;
    int shapeCount = 0;
    int closedRegionCount = 0;
    int bboxCandidateCount = 0;
    std::ostringstream json;
    json << "{\n";
    json << "  \"schema\": \"cxvision.torch.training_label_package.v1\",\n";
    json << "  \"status\": \"TO_VERIFY\",\n";
    json << "  \"training_mode\": \"label_package_export_only\",\n";
    json << "  \"dataset_consumed_by_current_runtime\": false,\n";
    json << "  \"segmentation_mask_export_ready\": false,\n";
    json << "  \"segmentation_mask_export_reason\": \"shape snapshots are exported; raster mask adapter is pending\",\n";
    json << "  \"run_id\": \"" << JsonEscape(runId) << "\",\n";
    json << "  \"evidence_case_id\": \""
         << JsonEscape(m_manualTest.current_evidence_selection.case_id) << "\",\n";
    json << "  \"images\": [\n";

    for (std::size_t imageIndex = 0;
         imageIndex < m_manualTest.torch_training_images.size();
         ++imageIndex)
    {
        const TorchTrainingImageItem& item =
            m_manualTest.torch_training_images[imageIndex];
        const bool imageExists = !item.image_path.empty() &&
            std::filesystem::exists(std::filesystem::path(item.image_path));
        ++imageCount;
        if (!imageExists)
            ++imageMissingCount;
        if (imageIndex != 0)
            json << ",\n";
        json << "    {\n";
        json << "      \"image_id\": \"" << JsonEscape(item.image_id) << "\",\n";
        json << "      \"image_path\": \"" << JsonEscape(item.image_path) << "\",\n";
        json << "      \"image_exists\": " << (imageExists ? "true" : "false") << ",\n";
        json << "      \"case_id\": \"" << JsonEscape(item.case_id) << "\",\n";
        json << "      \"target_id\": \"" << JsonEscape(item.target_id) << "\",\n";
        json << "      \"source\": \"" << JsonEscape(item.source) << "\",\n";
        json << "      \"split\": \"" << JsonEscape(item.split) << "\",\n";
        json << "      \"label\": \"" << JsonEscape(item.label) << "\",\n";
        json << "      \"annotation_status\": \"" << JsonEscape(item.annotation_status) << "\",\n";
        json << "      \"shapes\": [";
        for (std::size_t shapeIndex = 0;
             shapeIndex < item.annotation_shapes.size();
             ++shapeIndex)
        {
            const TorchTrainingAnnotationShapeSnapshot& shape =
                item.annotation_shapes[shapeIndex];
            ++shapeCount;
            const bool closedRegion = shape.closed &&
                (shape.shape_kind == "RectShape" ||
                 shape.shape_kind == "CircleShape" ||
                 shape.shape_kind == "EllipseShape" ||
                 shape.shape_kind == "PolylineShape");
            if (closedRegion)
                ++closedRegionCount;
            if (shape.shape_kind == "RectShape" ||
                shape.semantic_role.find("bbox") != std::string::npos)
                ++bboxCandidateCount;
            if (shapeIndex != 0)
                json << ",";
            json << "\n        {\"stable_ref\":\"" << JsonEscape(shape.stable_ref)
                 << "\",\"shape_kind\":\"" << JsonEscape(shape.shape_kind)
                 << "\",\"semantic_role\":\"" << JsonEscape(shape.semantic_role)
                 << "\",\"owner_binding\":\"" << JsonEscape(shape.owner_binding)
                 << "\",\"closed\":" << (shape.closed ? "true" : "false")
                 << ",\"center_x\":" << shape.center_x
                 << ",\"center_y\":" << shape.center_y
                 << ",\"radius_x\":" << shape.radius_x
                 << ",\"radius_y\":" << shape.radius_y
                 << ",\"radius\":" << shape.radius
                 << ",\"angle\":" << shape.angle
                 << ",\"points_xy\":[";
            for (std::size_t pointIndex = 0;
                 pointIndex < shape.points_xy.size();
                 ++pointIndex)
            {
                if (pointIndex != 0)
                    json << ",";
                json << shape.points_xy[pointIndex];
            }
            json << "]}";
        }
        if (!item.annotation_shapes.empty())
            json << "\n      ";
        json << "]\n    }";
    }
    json << "\n  ],\n";
    json << "  \"summary\": {\n";
    json << "    \"image_count\": " << imageCount << ",\n";
    json << "    \"image_missing_count\": " << imageMissingCount << ",\n";
    json << "    \"shape_count\": " << shapeCount << ",\n";
    json << "    \"closed_region_count\": " << closedRegionCount << ",\n";
    json << "    \"bbox_candidate_count\": " << bboxCandidateCount << "\n";
    json << "  }\n}";

    const std::filesystem::path path = outputDir / "torch_training_label_package.json";
    if (!WriteTextFile(path, json.str()))
    {
        reason = "failed to write Torch training label package: " + path.string();
        return false;
    }

    packagePath = path.string();
    reason = "TO_VERIFY: exported " + std::to_string(imageCount) +
        " images and " + std::to_string(shapeCount) +
        " shape snapshots; segmentation mask adapter remains pending";
    m_manualTest.torch_training_image_status = "LABEL_PACKAGE_EXPORTED_TO_VERIFY";
    m_manualTest.torch_training_image_reason = reason + " path=" + packagePath;
    CXLOG_INFO(
        "TorchTrainingImageSet",
        "training_label_package_exported",
        "TO_VERIFY",
        "path=" + packagePath +
            " images=" + std::to_string(imageCount) +
            " shapes=" + std::to_string(shapeCount) +
            " closed_regions=" + std::to_string(closedRegionCount) +
            " bbox_candidates=" + std::to_string(bboxCandidateCount));
    return true;
}

bool ViewController::RunTorchTrainingLabelPackageSmoke(
    const std::string& preferredScriptId,
    const std::string& requestedOutDir,
    std::string& packagePath,
    std::string& reason)
{
    packagePath.clear();
    reason.clear();

    const std::string runId = CxUnifiedLog::Instance().RunId().empty()
        ? "ui_session"
        : CxUnifiedLog::Instance().RunId();
    const std::filesystem::path outDir = requestedOutDir.empty()
        ? ResolveCxVisionRunPath("cxscript_runs/torch_training_label_package_smoke") / runId
        : std::filesystem::path(requestedOutDir);
    std::error_code ec;
    std::filesystem::create_directories(outDir, ec);
    if (ec)
    {
        reason = "cannot create smoke output directory: " + outDir.string() +
            " reason=" + ec.message();
        return false;
    }

    CxEvidenceSelfTestBatchRequest batch;
    batch.run_id = runId;
    batch.out_dir = outDir.string();
    batch.tool_filter = "TorchTask";
    std::string batchReason;
    if (!BuildEvidenceSelfTestBatchFromCurrentEvidenceRows(batch, batchReason))
    {
        reason = "cannot resolve Torch evidence rows: " + batchReason;
        return false;
    }

    const CxEvidenceSelfTestRequest* selectedRequest = nullptr;
    for (const CxEvidenceSelfTestRequest& item : batch.cases)
    {
        if (!preferredScriptId.empty() && item.script_id == preferredScriptId)
        {
            selectedRequest = &item;
            break;
        }
    }
    if (selectedRequest == nullptr)
    {
        for (const CxEvidenceSelfTestRequest& item : batch.cases)
        {
            std::string key = item.script_id + " " + item.script_path + " " + item.tool;
            std::transform(key.begin(), key.end(), key.begin(),
                [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            if (key.find("torch") != std::string::npos && !item.image_path.empty())
            {
                selectedRequest = &item;
                break;
            }
        }
    }
    if (selectedRequest == nullptr)
    {
        reason = "no Torch evidence case with an image is available for label-package smoke";
        return false;
    }

    CXLOG_INFO(
        "TorchTrainingImageSet",
        "training_label_package_smoke_begin",
        "running",
        "script_id=" + selectedRequest->script_id +
            " case_id=" + selectedRequest->case_id);

    CxEvidenceSelectionSnapshot snapshot;
    std::string stageReason;
    if (!ResolveEvidenceSelfTestSnapshot(*selectedRequest, snapshot, stageReason) ||
        !ApplyEvidenceSelectionSnapshotToManualContext(snapshot, false, stageReason))
    {
        reason = "Torch evidence selection/load failed: " + stageReason;
        return false;
    }
    // The command-line smoke intentionally has no GLFW/OpenGL Image View.
    // Reuse the same loaded-image state path used by Evidence selftests;
    // actual image-view drawing/pointer behavior remains covered by GUI L2.
    if (!LoadImageForEvidenceSelfTest(snapshot.image_path, stageReason))
    {
        reason = "headless Image View state load failed: " + stageReason;
        return false;
    }
    // ApplyEvidenceSelectionSnapshotToManualContext() already performs the
    // dataset sync for image-set evidence.  Rebuilding it here would clear
    // and recreate the same rail a second time, which is both redundant and
    // inconsistent with the UI selection transaction.
    CXLOG_INFO(
        "TorchTrainingImageSet",
        "training_label_package_smoke_dataset_reused",
        "ready",
        "case_id=" + snapshot.case_id +
            " image_count=" +
            std::to_string(m_manualTest.torch_training_images.size()));
    if (m_manualTest.torch_training_images.empty())
    {
        reason = "Torch evidence selection produced no Training Image Set items";
        return false;
    }

    int imageIndex = m_manualTest.selected_torch_training_image;
    if (imageIndex < 0 ||
        imageIndex >= static_cast<int>(m_manualTest.torch_training_images.size()))
    {
        imageIndex = 0;
    }
    m_manualTest.selected_torch_training_image = imageIndex;
    RestoreTorchTrainingAnnotationState(
        m_manualTest.torch_training_images[static_cast<std::size_t>(imageIndex)]);
    m_manualTest.torch_training_image_status = "HEADLESS_ANNOTATION_READY";
    m_manualTest.torch_training_image_reason =
        "headless state simulation: selected training image and restored annotations";
    CXLOG_INFO(
        "TorchTrainingImageSet",
        "training_label_package_smoke_image_state_loaded",
        "HEADLESS_ANNOTATION_READY",
        "index=" + std::to_string(imageIndex) +
            " image_path=" +
            m_manualTest.torch_training_images[
                static_cast<std::size_t>(imageIndex)].image_path);

    TorchTrainingImageItem& item =
        m_manualTest.torch_training_images[static_cast<std::size_t>(imageIndex)];
    cv::Mat image = cv::imread(item.image_path, cv::IMREAD_UNCHANGED);
    if (image.empty())
    {
        reason = "selected Training Image Set image cannot be opened: " + item.image_path;
        return false;
    }

    const double x0 = std::max(0.0, static_cast<double>(image.cols) * 0.20);
    const double y0 = std::max(0.0, static_cast<double>(image.rows) * 0.20);
    const double x1 = std::max(x0 + 2.0, static_cast<double>(image.cols) * 0.60);
    const double y1 = std::max(y0 + 2.0, static_cast<double>(image.rows) * 0.60);
    const std::string stableRef = "torch_label_package_smoke_bbox";
    m_annotationLayer.UpsertShape(
        stableRef,
        "TorchDataset",
        item.image_id,
        "label_bbox",
        "smoke_annotation",
        true,
        false,
        std::make_unique<RectShape>(x0, y0, x1, y1));
    item.label = "anomaly";
    CaptureCurrentTorchTrainingAnnotationState();

    bool shapeCaptured = false;
    for (const TorchTrainingAnnotationShapeSnapshot& shape : item.annotation_shapes)
    {
        if (shape.stable_ref == stableRef && shape.shape_kind == "RectShape")
        {
            shapeCaptured = true;
            break;
        }
    }
    if (!shapeCaptured)
    {
        reason = "AnnotationLayer -> Training Image Set shape snapshot failed";
        return false;
    }

    std::string exportReason;
    if (!ExportTorchTrainingLabelPackage(packagePath, exportReason))
    {
        reason = "label package export failed: " + exportReason;
        return false;
    }

    std::ifstream packageFile(packagePath, std::ios::binary);
    const std::string packageText(
        (std::istreambuf_iterator<char>(packageFile)),
        std::istreambuf_iterator<char>());
    const bool schemaOk = packageText.find(
        "cxvision.torch.training_label_package.v1") != std::string::npos;
    const bool toVerifyOk = packageText.find("\"status\": \"TO_VERIFY\"") !=
        std::string::npos;
    const bool imageOk = packageText.find(JsonEscape(item.image_path)) !=
        std::string::npos;
    const bool labelOk = packageText.find("\"label\": \"anomaly\"") !=
        std::string::npos;
    const bool shapeOk = packageText.find(stableRef) != std::string::npos &&
        packageText.find("\"shape_kind\":\"RectShape\"") != std::string::npos;
    const bool pass = schemaOk && toVerifyOk && imageOk && labelOk && shapeOk;

    std::ostringstream report;
    report << "{\n"
           << "  \"schema\": \"cxvision.torch.training_label_package_smoke.v1\",\n"
           << "  \"conclusion\": \""
           << (pass ? "LABEL_PACKAGE_EXPORT_PASS_TO_VERIFY" : "LABEL_PACKAGE_EXPORT_FAIL")
           << "\",\n"
           << "  \"script_id\": \"" << JsonEscape(selectedRequest->script_id) << "\",\n"
           << "  \"case_id\": \"" << JsonEscape(selectedRequest->case_id) << "\",\n"
           << "  \"image_path\": \"" << JsonEscape(item.image_path) << "\",\n"
           << "  \"label\": \"" << JsonEscape(item.label) << "\",\n"
           << "  \"shape_ref\": \"" << stableRef << "\",\n"
           << "  \"package_path\": \"" << JsonEscape(packagePath) << "\",\n"
           << "  \"checks\": {\n"
           << "    \"schema\": " << (schemaOk ? "true" : "false") << ",\n"
           << "    \"to_verify_status\": " << (toVerifyOk ? "true" : "false") << ",\n"
           << "    \"image\": " << (imageOk ? "true" : "false") << ",\n"
           << "    \"label\": " << (labelOk ? "true" : "false") << ",\n"
           << "    \"shape\": " << (shapeOk ? "true" : "false") << "\n"
           << "  }\n"
           << "}\n";
    std::string writeReason;
    const std::filesystem::path reportPath = outDir /
        "torch_training_label_package_smoke.json";
    if (!WriteTextFile(reportPath, report.str()))
    {
        reason = "cannot write smoke report: " + reportPath.string();
        return false;
    }

    reason = std::string(pass
        ? "LABEL_PACKAGE_EXPORT_PASS_TO_VERIFY"
        : "LABEL_PACKAGE_EXPORT_FAIL") +
        " package=" + packagePath +
        " report=" + reportPath.string();
    CXLOG_INFO(
        "TorchTrainingImageSet",
        "training_label_package_smoke_end",
        pass ? "LABEL_PACKAGE_EXPORT_PASS_TO_VERIFY" : "FAIL",
        reason);
    return pass;
}

void ViewController::drawTorchTrainingImageSetWindow()
{
    ImGui::SetNextWindowPos(ImVec2(1380, 740), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(620, 420), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Torch Training Image Set", nullptr, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        return;
    }

    ImGui::TextWrapped(
        "Training/validation/test image rails for Torch evidence review. "
        "Click a thumbnail to load it into Image View. Labels are operator evidence, not model quality PASS.");
    ImGui::Separator();

    ImGui::Text("active_case: %s", m_manualTest.active_case_id.empty() ? "-" : m_manualTest.active_case_id.c_str());
    ImGui::Text("selected_evidence: %s", m_manualTest.current_evidence_selection.case_id.empty()
        ? "-"
        : m_manualTest.current_evidence_selection.case_id.c_str());
    ImGui::Text("status: %s", m_manualTest.torch_training_image_status.c_str());
    ImGui::TextWrapped("reason: %s", m_manualTest.torch_training_image_reason.c_str());

    if (!m_annotationLayer.HasActiveDrag() &&
        m_manualTest.selected_torch_training_image >= 0 &&
        m_manualTest.selected_torch_training_image <
            static_cast<int>(m_manualTest.torch_training_images.size()))
    {
        const TorchTrainingImageItem& selectedItem =
            m_manualTest.torch_training_images[
                static_cast<std::size_t>(
                    m_manualTest.selected_torch_training_image)];
        if (!m_manualTest.image_file_path.empty() &&
            !selectedItem.image_path.empty())
        {
            const std::string currentImage =
                ResolveWorkspaceFile(m_manualTest.image_file_path)
                    .lexically_normal()
                    .string();
            const std::string selectedImage =
                ResolveWorkspaceFile(selectedItem.image_path)
                    .lexically_normal()
                    .string();
            if (currentImage == selectedImage)
                CaptureCurrentTorchTrainingAnnotationState();
        }
    }

    if (ImGui::Button("Sync Selected Evidence Case"))
    {
        CXLOG_INFO(
            "TorchTrainingImageSet",
            "sync_selected_evidence_button",
            "ui_event",
            "case_id=" + m_manualTest.active_case_id +
            " script_id=" +
            m_manualTest.current_evidence_selection.script_id);
        SyncTorchTrainingImageSetFromEvidenceSelection();
    }
    ImGui::SameLine();
    if (ImGui::Button("Add Current As Train"))
    {
        CXLOG_INFO(
            "TorchTrainingImageSet",
            "add_current_as_train_button",
            "ui_event",
            "image_path=" + m_manualTest.image_file_path +
            " image_id=" + m_manualTest.active_image_id);
        AddTorchTrainingImageFromPath(m_manualTest.image_file_path, m_manualTest.active_image_id, "train", "unlabeled", "current_image");
    }
    ImGui::SameLine();
    if (ImGui::Button("Add Current As Val"))
    {
        CXLOG_INFO(
            "TorchTrainingImageSet",
            "add_current_as_val_button",
            "ui_event",
            "image_path=" + m_manualTest.image_file_path +
            " image_id=" + m_manualTest.active_image_id);
        AddTorchTrainingImageFromPath(m_manualTest.image_file_path, m_manualTest.active_image_id, "val", "unlabeled", "current_image");
    }
    ImGui::SameLine();
    if (ImGui::Button("Add Current As Test"))
    {
        CXLOG_INFO(
            "TorchTrainingImageSet",
            "add_current_as_test_button",
            "ui_event",
            "image_path=" + m_manualTest.image_file_path +
            " image_id=" + m_manualTest.active_image_id);
        AddTorchTrainingImageFromPath(m_manualTest.image_file_path, m_manualTest.active_image_id, "test", "unlabeled", "current_image");
    }

    ImGui::SetNextItemWidth(-1.0f);
    InputTextString("Incremental image path", m_manualTest.torch_training_new_image_path);
    if (ImGui::Button("Add Path As Train"))
    {
        CXLOG_INFO(
            "TorchTrainingImageSet",
            "add_path_as_train_button",
            "ui_event",
            "image_path=" + m_manualTest.torch_training_new_image_path);
        AddTorchTrainingImageFromPath(m_manualTest.torch_training_new_image_path, "", "train", "unlabeled", "manual_path");
    }
    ImGui::SameLine();
    if (ImGui::Button("Add Manifest Images"))
    {
        CXLOG_INFO(
            "TorchTrainingImageSet",
            "add_manifest_images_button",
            "ui_event",
            "case_id=" + m_manualTest.active_case_id +
            " script_id=" +
            m_manualTest.current_evidence_selection.script_id);
        ClearTorchTrainingImageSetForEvidenceSyncLocal(
            m_manualTest,
            "manual rebuild from selected evidence manifest images");
        int count = AddHDReferenceImageSetForCurrentSelection();
        if (count == 0)
        {
            for (const ManifestImageItem& item : m_manualTest.image_manifest_items)
            {
                if (!item.image_path.empty())
                {
                    AddTorchTrainingImageFromPath(item.image_path, item.image_id, "train", "unlabeled", "manifest");
                    ++count;
                }
            }
            m_manualTest.torch_training_image_status = "MANIFEST_IMAGES_ADDED";
            m_manualTest.torch_training_image_reason =
                "incrementally added manifest images: " + std::to_string(count);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Export Label Package (To Verify)"))
    {
        CaptureCurrentTorchTrainingAnnotationState();
        std::string packagePath;
        std::string exportReason;
        if (!ExportTorchTrainingLabelPackage(packagePath, exportReason))
        {
            m_manualTest.debug_status = "TORCH_LABEL_PACKAGE_EXPORT_FAIL";
            m_manualTest.debug_reason = exportReason;
            CXLOG_ERROR(
                "TorchTrainingImageSet",
                "training_label_package_export_failed",
                "FAIL",
                exportReason);
        }
        else
        {
            m_manualTest.debug_status = "TORCH_LABEL_PACKAGE_TO_VERIFY";
            m_manualTest.debug_reason = exportReason;
        }
    }

    if (m_manualTest.selected_torch_training_image >= 0 &&
        m_manualTest.selected_torch_training_image <
            static_cast<int>(m_manualTest.torch_training_images.size()))
    {
        TorchTrainingImageItem& item =
            m_manualTest.torch_training_images[m_manualTest.selected_torch_training_image];
        ImGui::Separator();
        ImGui::Text("Selected image: %s", item.image_id.empty() ? "-" : item.image_id.c_str());
        ImGui::TextWrapped("%s", item.image_path.c_str());
        ImGui::Text(
            "Annotation: %s | shapes=%d | overlays=%d",
            item.annotation_status.c_str(),
            item.annotation_shape_count,
            item.annotation_overlay_count);

        if (ImGui::Button("label: good"))
        {
            item.label = "good";
            item.annotation_status = "editing";
            CXLOG_INFO(
                "TorchTrainingImageSet",
                "training_image_label_changed",
                "ui_event",
                "label=good image_id=" + item.image_id +
                " image_path=" + item.image_path);
        }
        ImGui::SameLine();
        if (ImGui::Button("label: anomaly"))
        {
            item.label = "anomaly";
            item.annotation_status = "editing";
            CXLOG_INFO(
                "TorchTrainingImageSet",
                "training_image_label_changed",
                "ui_event",
                "label=anomaly image_id=" + item.image_id +
                " image_path=" + item.image_path);
        }
        ImGui::SameLine();
        if (ImGui::Button("label: unlabeled"))
        {
            item.label = "unlabeled";
            item.annotation_status = "unlabeled";
            CXLOG_INFO(
                "TorchTrainingImageSet",
                "training_image_label_changed",
                "ui_event",
                "label=unlabeled image_id=" + item.image_id +
                " image_path=" + item.image_path);
        }
        ImGui::SameLine();
        if (ImGui::Button("label: pending"))
        {
            item.label = "pending";
            item.annotation_status = "pending";
            CXLOG_INFO(
                "TorchTrainingImageSet",
                "training_image_label_changed",
                "ui_event",
                "label=pending image_id=" + item.image_id +
                " image_path=" + item.image_path);
        }

        if (ImGui::Button("move train"))
        {
            item.split = "train";
            CXLOG_INFO("TorchTrainingImageSet", "training_image_split_changed", "ui_event", "split=train image_id=" + item.image_id + " image_path=" + item.image_path);
        }
        ImGui::SameLine();
        if (ImGui::Button("move val"))
        {
            item.split = "val";
            CXLOG_INFO("TorchTrainingImageSet", "training_image_split_changed", "ui_event", "split=val image_id=" + item.image_id + " image_path=" + item.image_path);
        }
        ImGui::SameLine();
        if (ImGui::Button("move test"))
        {
            item.split = "test";
            CXLOG_INFO("TorchTrainingImageSet", "training_image_split_changed", "ui_event", "split=test image_id=" + item.image_id + " image_path=" + item.image_path);
        }
    }

    m_manualTest.script_evidence_thumb_load_count_this_frame = 0;
    DrawTorchTrainingImageRail("train", "Training Set / 训练集");
    DrawTorchTrainingImageRail("val", "Validation Set / 验证集");
    DrawTorchTrainingImageRail("test", "Test Set / 测试集");

    ImGui::End();
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

        if (!thumb.evidence_category_override.empty() && !group.label.empty())
            return {0, group.label};

        if (IsTorchEvidenceCandidateRowLocal(thumb, group.label))
        {
            const std::string label = group.label.empty()
                ? std::string("Torch Evidence Candidate Case")
                : group.label;
            return {0, label};
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
        if (exactTool == "RegionPatternTool")
            return {5, "RegionPattern"};
        if (exactTool == "GridPatternClassTool")
            return {6, "GridPattern"};
        if (exactTool == "FastMatch")
            return {7, "FastMatch"};
        if (exactTool == "FindSegmentation")
            return {8, "FindSegmentation Prompt / EdgeSam"};
        if (exactTool == "TorchTask")
            return {9, "Torch / Model Validation"};

        // Only infer Torch/model ownership from free text when the row has no
        // explicit tool type.  Every candidate parameter snapshot contains
        // shared global_torch_* fields, which must not move FindEllipse and
        // other cximage tools into the Torch navigation bucket.
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

        if (key.find("regionpattern") != std::string::npos ||
            key.find("region_pattern") != std::string::npos)
            return {5, "RegionPattern"};

        if (key.find("gridpattern") != std::string::npos ||
            key.find("grid_pattern") != std::string::npos)
            return {6, "GridPattern"};

        if (key.find("fastmatch") != std::string::npos)
            return {7, "FastMatch"};

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
        { "Torch Evidence Candidates", 5, {} },
        { "Torch / Model Validation", 6, {} }
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
    bool rowDoubleClicked =
        rowHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
    const bool rowRightClicked = ImGui::IsItemClicked(ImGuiMouseButton_Right);

    ImGui::SetCursorScreenPos(rowMin);

    const float imageColWidth = 96.0f;
    bool imageDoubleClicked = false;
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
            imageDoubleClicked =
                ImGui::IsItemHovered() &&
                ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
        }
        else
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 p0 = ImGui::GetCursorScreenPos();
            ImVec2 p1(p0.x + thumbSize.x, p0.y + thumbSize.y);
            drawList->AddRectFilled(p0, p1, IM_COL32(90, 130, 170, 220));
            drawList->AddText(ImVec2(p0.x + 18, p0.y + 28), IM_COL32(255, 255, 255, 255), "NO IMG");
            ImGui::Dummy(thumbSize);
            imageDoubleClicked =
                ImGui::IsItemHovered() &&
                ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
        }

        ImGui::EndTable();
    }

    // The table and its Image item are rendered after the row-sized invisible
    // button.  Depending on the ImGui overlap rules, the second click can be
    // owned by the table item and the invisible button never reports a native
    // double click.  Detect the complete visual row bounds as the canonical
    // click surface and keep a small, explicit double-click window.
    const ImVec2 rowMax(rowMin.x + rowSize.x, rowMin.y + rowSize.y);
    const bool rowBoundsHovered =
        ImGui::IsMouseHoveringRect(rowMin, rowMax, false);
    const bool rowBoundsClicked =
        rowBoundsHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    if (rowBoundsClicked)
    {
        const double now = ImGui::GetTime();
        const bool sameRow =
            m_manualTest.last_evidence_click_group == groupIndex &&
            m_manualTest.last_evidence_click_thumb == thumbIndex;
        const double elapsed = now - m_manualTest.last_evidence_click_time;
        if (sameRow && elapsed >= 0.0 && elapsed <= 0.55)
            rowDoubleClicked = true;

        m_manualTest.last_evidence_click_group = groupIndex;
        m_manualTest.last_evidence_click_thumb = thumbIndex;
        m_manualTest.last_evidence_click_time = now;
    }

    if (rowDoubleClicked || imageDoubleClicked)
    {
        CXLOG_INFO(
            "EvidenceChain",
            "evidence_thumb_double_click",
            "ui_event",
            "group_index=" + std::to_string(groupIndex) +
            " thumb_index=" + std::to_string(thumbIndex) +
            " script_id=" + thumb.script_id +
            " image_id=" + thumb.image_id +
            " image_path=" + thumb.image_path +
            " tool=" + thumb.tool);
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
    else if (rowClicked || rowBoundsClicked)
    {
        CXLOG_INFO(
            "EvidenceChain",
            "evidence_thumb_click",
            "ui_event",
            "group_index=" + std::to_string(groupIndex) +
            " thumb_index=" + std::to_string(thumbIndex) +
            " script_id=" + thumb.script_id +
            " image_id=" + thumb.image_id +
            " tool=" + thumb.tool);
        std::string reason;
        if (!RefreshEvidenceSelectionFromThumb(
                groupIndex,
                thumbIndex,
                true,
                reason))
        {
            m_manualTest.debug_status = "EVIDENCE_SELECT_LOAD_FAIL";
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
                StoreEvidenceCategoryOverrideLocal(
                    m_manualTest,
                    thumb,
                    category);
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
            if (ImGui::MenuItem("Torch Evidence Candidates"))
                moveToCategory("Torch Evidence Candidates", "torch_evidence_candidate", "manual category: torch evidence candidate");
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
            "Click: select and load image | Right-click: menu\n"
            "script: %s\nimage: %s\npath: %s\nreason: %s",
            thumb.script_id.c_str(),
            thumb.image_id.c_str(),
            thumb.image_path.c_str(),
            thumb.reason.c_str());
    }

    finishRow();
}