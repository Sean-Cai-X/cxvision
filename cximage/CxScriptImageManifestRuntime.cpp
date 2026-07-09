#include "CxScriptImageManifestRuntime.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <opencv2/opencv.hpp>

namespace
{
    bool SkipWhitespace(const std::string& text, size_t& pos)
    {
        while (pos < text.size() && std::isspace(text[pos]))
            ++pos;
        return pos < text.size();
    }

    bool ExpectChar(const std::string& text, size_t& pos, char ch)
    {
        SkipWhitespace(text, pos);
        if (pos >= text.size() || text[pos] != ch)
            return false;
        ++pos;
        return true;
    }

    std::string ParseString(const std::string& text, size_t& pos)
    {
        SkipWhitespace(text, pos);
        if (!ExpectChar(text, pos, '"'))
            return "";

        std::string result;
        bool escaped = false;
        while (pos < text.size())
        {
            const char ch = text[pos++];
            if (escaped)
            {
                result += ch;
                escaped = false;
            }
            else if (ch == '\\')
            {
                escaped = true;
            }
            else if (ch == '"')
            {
                break;
            }
            else
            {
                result += ch;
            }
        }
        return result;
    }

    int ParseInt(const std::string& text, size_t& pos)
    {
        SkipWhitespace(text, pos);
        const auto start = pos;
        if (pos < text.size() && text[pos] == '-')
            ++pos;
        while (pos < text.size() && std::isdigit(text[pos]))
            ++pos;
        if (pos == start)
            return 0;
        try
        {
            return std::stoi(text.substr(start, pos - start));
        }
        catch (...)
        {
            return 0;
        }
    }

    size_t FindArrayStart(const std::string& text, size_t pos, const std::string& arrayName)
    {
        const std::string pattern = "\"" + arrayName + "\"";
        const auto keyPos = text.find(pattern, pos);
        if (keyPos == std::string::npos)
            return std::string::npos;

        const auto colon = text.find(":", keyPos + pattern.size());
        if (colon == std::string::npos)
            return std::string::npos;

        SkipWhitespace(text, pos = colon + 1);
        if (pos >= text.size() || text[pos] != '[')
            return std::string::npos;

        return pos;
    }

    size_t FindObjectStart(const std::string& text, size_t pos, const std::string& keyName)
    {
        const std::string pattern = "\"" + keyName + "\"";
        const auto keyPos = text.find(pattern, pos);
        if (keyPos == std::string::npos)
            return std::string::npos;

        const auto colon = text.find(":", keyPos + pattern.size());
        if (colon == std::string::npos)
            return std::string::npos;

        SkipWhitespace(text, pos = colon + 1);
        if (pos >= text.size() || text[pos] != '{')
            return std::string::npos;

        return pos;
    }

    size_t FindNextArrayElement(const std::string& text, size_t pos)
    {
        SkipWhitespace(text, pos);
        if (pos >= text.size())
            return std::string::npos;

        if (text[pos] == ']')
            return std::string::npos;

        if (text[pos] == ',')
            ++pos;

        SkipWhitespace(text, pos);
        if (pos >= text.size())
            return std::string::npos;

        return pos;
    }

    size_t FindObjectEnd(const std::string& text, size_t start)
    {
        int depth = 1;
        bool inString = false;
        bool escaped = false;

        for (size_t i = start + 1; i < text.size(); ++i)
        {
            const char ch = text[i];
            if (inString)
            {
                if (escaped)
                {
                    escaped = false;
                }
                else if (ch == '\\')
                {
                    escaped = true;
                }
                else if (ch == '"')
                {
                    inString = false;
                }
                continue;
            }

            if (ch == '"')
            {
                inString = true;
                continue;
            }

            if (ch == '{')
                ++depth;
            else if (ch == '}')
            {
                --depth;
                if (depth == 0)
                    return i;
            }
        }
        return std::string::npos;
    }

    bool ParseImageObject(
        const std::string& text,
        size_t pos,
        CxScriptImageManifestEntry& entry)
    {
        const auto end = FindObjectEnd(text, pos);
        if (end == std::string::npos)
            return false;

        SkipWhitespace(text, pos);
        if (pos < end && text[pos] == '{')
            ++pos;

        while (pos < end)
        {
            const std::string key = ParseString(text, pos);
            if (key.empty())
                break;

            ExpectChar(text, pos, ':');

            if (key == "image_id")
                entry.image_id = ParseString(text, pos);
            else if (key == "level")
                entry.level = ParseString(text, pos);
            else if (key == "path")
                entry.path = ParseString(text, pos);
            else if (key == "source")
                entry.source_path = ParseString(text, pos);
            else if (key == "width")
                entry.width = ParseInt(text, pos);
            else if (key == "height")
                entry.height = ParseInt(text, pos);
            else if (key == "tool_targets")
            {
                SkipWhitespace(text, pos);
                if (text[pos] == '[')
                {
                    ++pos;
                    while (true)
                    {
                        const auto elemPos = FindNextArrayElement(text, pos);
                        if (elemPos == std::string::npos)
                            break;

                        if (text[elemPos] == '{')
                        {
                            CxScriptImageTargetRoi target;
                            const auto targetEnd = FindObjectEnd(text, elemPos);
                            if (targetEnd != std::string::npos)
                            {
                                size_t tp = elemPos + 1;
                                SkipWhitespace(text, tp);
                                while (tp < targetEnd)
                                {
                                    const std::string tkey = ParseString(text, tp);
                                    if (tkey.empty())
                                        break;
                                    ExpectChar(text, tp, ':');

                                    if (tkey == "tool")
                                        target.tool = ParseString(text, tp);
                                    else if (tkey == "roi_name")
                                        target.target_id = ParseString(text, tp);
                                    else if (tkey == "x0")
                                        target.x0 = ParseInt(text, tp);
                                    else if (tkey == "y0")
                                        target.y0 = ParseInt(text, tp);
                                    else if (tkey == "x1")
                                        target.x1 = ParseInt(text, tp);
                                    else if (tkey == "y1")
                                        target.y1 = ParseInt(text, tp);
                                    else if (tkey == "cx")
                                        target.cx = ParseInt(text, tp);
                                    else if (tkey == "cy")
                                        target.cy = ParseInt(text, tp);
                                    else if (tkey == "px")
                                        target.px = ParseInt(text, tp);
                                    else if (tkey == "py")
                                        target.py = ParseInt(text, tp);
                                    else if (tkey == "wgap")
                                        target.wgap = ParseInt(text, tp);
                                    else if (tkey == "hgap")
                                        target.hgap = ParseInt(text, tp);
                                    else if (tkey == "gap")
                                        target.gap = ParseInt(text, tp);
                                    else if (tkey == "linegap")
                                        target.linegap = ParseInt(text, tp);
                                    else if (tkey == "tool_half_width")
                                        target.tool_half_width = ParseInt(text, tp);
                                    else if (tkey == "threshold")
                                        target.threshold = ParseInt(text, tp);
                                    else if (tkey == "method")
                                        target.method = ParseInt(text, tp);
                                    else if (tkey == "expected_edge")
                                        target.expected_edge = ParseString(text, tp);
                                    else if (tkey == "edge_polarity_hint")
                                        target.edge_polarity_hint = ParseString(text, tp);
                                    else if (tkey == "comment")
                                        target.comment = ParseString(text, tp);
                                    else
                                    {
                                        SkipWhitespace(text, tp);
                                        if (tp < targetEnd && text[tp] == '"')
                                            ParseString(text, tp);
                                        else
                                        {
                                            while (tp < targetEnd && text[tp] != ',' && text[tp] != '}')
                                                ++tp;
                                        }
                                    }

                                    if (!ExpectChar(text, tp, ','))
                                        break;
                                }
                                entry.targets.push_back(target);
                            }
                            pos = targetEnd + 1;
                        }
                        else
                        {
                            break;
                        }
                    }
                }
            }
            else
            {
                SkipWhitespace(text, pos);
                if (pos < end && text[pos] == '"')
                    ParseString(text, pos);
                else
                {
                    while (pos < end && text[pos] != ',' && text[pos] != '}')
                        ++pos;
                }
            }

            if (!ExpectChar(text, pos, ','))
                break;
        }

        entry.raw_not_cropped = true;
        entry.raw_not_enhanced = true;
        entry.raw_not_rotated = true;

        return true;
    }
}

bool LoadStage25ImageManifestJson(
    const std::string& manifest_path,
    CxScriptImageManifestRuntime& out_manifest,
    std::string& out_reason)
{
    std::filesystem::path path(manifest_path);
    if (!std::filesystem::exists(path))
    {
        out_reason = "Manifest file not found: " + manifest_path;
        return false;
    }

    std::ifstream file(path);
    if (!file.is_open())
    {
        out_reason = "Cannot open manifest file: " + manifest_path;
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    const std::string text = buffer.str();

    out_manifest = CxScriptImageManifestRuntime{};
    out_manifest.manifest_path = manifest_path;

    size_t pos = 0;
    SkipWhitespace(text, pos);
    if (!ExpectChar(text, pos, '{'))
    {
        out_reason = "Invalid JSON format: expected '{'";
        return false;
    }

    while (pos < text.size())
    {
        const std::string key = ParseString(text, pos);
        if (key.empty())
            break;

        ExpectChar(text, pos, ':');

        if (key == "schema_version")
            out_manifest.schema_version = ParseString(text, pos);
        else if (key == "purpose")
            out_manifest.purpose = ParseString(text, pos);
        else if (key == "selection_policy")
            out_manifest.selection_policy = ParseString(text, pos);
        else if (key == "images")
        {
            SkipWhitespace(text, pos);
            if (text[pos] == '[')
            {
                ++pos;
                while (true)
                {
                    const auto elemPos = FindNextArrayElement(text, pos);
                    if (elemPos == std::string::npos)
                        break;

                    if (text[elemPos] == '{')
                    {
                        CxScriptImageManifestEntry entry;
                        if (ParseImageObject(text, elemPos, entry))
                        {
                            out_manifest.images.push_back(entry);
                            pos = FindObjectEnd(text, elemPos) + 1;

                            if (entry.level == "L0_basic")
                                out_manifest.l0_count++;
                            else if (entry.level.find("L1") == 0)
                                out_manifest.l1_count++;
                            else if (entry.level.find("L2") == 0)
                                out_manifest.l2_count++;
                            else if (entry.level.find("L3") == 0)
                                out_manifest.l3_count++;
                        }
                        else
                        {
                            break;
                        }
                    }
                    else
                    {
                        break;
                    }
                }
            }
        }
        else
        {
            SkipWhitespace(text, pos);
            if (pos < text.size() && text[pos] == '"')
                ParseString(text, pos);
            else if (pos < text.size() && text[pos] == '{')
            {
                const auto objEnd = FindObjectEnd(text, pos);
                if (objEnd != std::string::npos)
                    pos = objEnd + 1;
                else
                    break;
            }
            else if (pos < text.size() && text[pos] == '[')
            {
                int depth = 1;
                for (size_t i = pos + 1; i < text.size() && depth > 0; ++i)
                {
                    if (text[i] == '[') ++depth;
                    else if (text[i] == ']') --depth;
                    pos = i;
                }
                ++pos;
            }
            else
            {
                while (pos < text.size() && text[pos] != ',' && text[pos] != '}')
                    ++pos;
            }
        }

        if (!ExpectChar(text, pos, ','))
            break;
    }

    out_manifest.total_images = static_cast<int>(out_manifest.images.size());

    return true;
}

namespace
{
    void AddImageManifestIssue(
        CxScriptImageManifestValidationResult& result,
        const std::string& severity,
        const std::string& image_id,
        const std::string& target_id,
        const std::string& message)
    {
        CxScriptImageManifestValidationIssue issue;
        issue.severity = severity;
        issue.image_id = image_id;
        issue.target_id = target_id;
        issue.message = message;
        result.issues.push_back(issue);

        if (severity == "error")
            result.ok = false;
    }

    bool IsPointInsideImage(int x, int y, int w, int h)
    {
        return x >= 0 && y >= 0 && x < w && y < h;
    }

    void ValidateTargetRoi(
        const CxScriptImageManifestEntry& image,
        const CxScriptImageTargetRoi& target,
        CxScriptImageManifestValidationResult& result)
    {
        if (target.tool == "Findline")
        {
            if (target.x0 == target.x1 && target.y0 == target.y1)
            {
                AddImageManifestIssue(
                    result,
                    "error",
                    image.image_id,
                    target.target_id,
                    "Findline ROI start/end points are identical");
            }

            if (!IsPointInsideImage(target.x0, target.y0, image.width, image.height) ||
                !IsPointInsideImage(target.x1, target.y1, image.width, image.height))
            {
                AddImageManifestIssue(
                    result,
                    "error",
                    image.image_id,
                    target.target_id,
                    "Findline ROI is outside image");
            }
        }

        if (target.tool == "Findcircle")
        {
            if (!IsPointInsideImage(target.cx, target.cy, image.width, image.height) ||
                !IsPointInsideImage(target.px, target.py, image.width, image.height))
            {
                AddImageManifestIssue(
                    result,
                    "error",
                    image.image_id,
                    target.target_id,
                    "Findcircle ROI is outside image");
            }

            const int dx = target.px - target.cx;
            const int dy = target.py - target.cy;
            if (dx * dx + dy * dy <= 4)
            {
                AddImageManifestIssue(
                    result,
                    "error",
                    image.image_id,
                    target.target_id,
                    "Findcircle radius is too small");
            }
        }
    }
}

CxScriptImageManifestValidationResult ValidateStage25ImageManifest(
    const CxScriptImageManifestRuntime& manifest)
{
    CxScriptImageManifestValidationResult result;

    if (manifest.total_images != 13)
    {
        AddImageManifestIssue(
            result,
            "error",
            "",
            "",
            "Expected 13 images, found " + std::to_string(manifest.total_images));
    }

    if (manifest.l0_count != 1)
    {
        AddImageManifestIssue(
            result,
            "error",
            "",
            "",
            "Expected L0=1, found " + std::to_string(manifest.l0_count));
    }

    if (manifest.l1_count != 4)
    {
        AddImageManifestIssue(
            result,
            "error",
            "",
            "",
            "Expected L1=4, found " + std::to_string(manifest.l1_count));
    }

    if (manifest.l2_count != 4)
    {
        AddImageManifestIssue(
            result,
            "error",
            "",
            "",
            "Expected L2=4, found " + std::to_string(manifest.l2_count));
    }

    if (manifest.l3_count != 4)
    {
        AddImageManifestIssue(
            result,
            "error",
            "",
            "",
            "Expected L3=4, found " + std::to_string(manifest.l3_count));
    }

    for (const auto& image : manifest.images)
    {
        if (!std::filesystem::exists(image.path))
        {
            AddImageManifestIssue(
                result,
                "error",
                image.image_id,
                "",
                "Image path does not exist: " + image.path);
            continue;
        }

        cv::Mat img = cv::imread(image.path);
        if (img.empty())
        {
            AddImageManifestIssue(
                result,
                "error",
                image.image_id,
                "",
                "Cannot read image: " + image.path);
            continue;
        }

        if (img.cols != image.width || img.rows != image.height)
        {
            AddImageManifestIssue(
                result,
                "error",
                image.image_id,
                "",
                "Image size mismatch: manifest (" + std::to_string(image.width) + "x" +
                    std::to_string(image.height) + "), actual (" +
                    std::to_string(img.cols) + "x" + std::to_string(img.rows) + ")");
        }

        if (!image.raw_not_cropped)
        {
            AddImageManifestIssue(
                result,
                "error",
                image.image_id,
                "",
                "raw_not_cropped must be true");
        }

        if (!image.raw_not_enhanced)
        {
            AddImageManifestIssue(
                result,
                "error",
                image.image_id,
                "",
                "raw_not_enhanced must be true");
        }

        if (!image.raw_not_rotated)
        {
            AddImageManifestIssue(
                result,
                "error",
                image.image_id,
                "",
                "raw_not_rotated must be true");
        }

        for (const auto& target : image.targets)
        {
            ValidateTargetRoi(image, target, result);
        }
    }

    return result;
}

const CxScriptImageManifestEntry* FindImageById(
    const CxScriptImageManifestRuntime& manifest,
    const std::string& image_id)
{
    for (const auto& image : manifest.images)
    {
        if (image.image_id == image_id)
            return &image;
    }
    return nullptr;
}

const CxScriptImageTargetRoi* FindTargetRoiByImageAndTargetId(
    const CxScriptImageManifestRuntime& manifest,
    const std::string& image_id,
    const std::string& target_id)
{
    const CxScriptImageManifestEntry* image = FindImageById(manifest, image_id);
    if (!image)
        return nullptr;

    for (const auto& target : image->targets)
    {
        if (target.target_id == target_id)
            return &target;
    }
    return nullptr;
}
