#include "pch.h"
#include "ManualConsoleEvidenceChain.h"
#include "ManualConsoleUtils.h"
#include "ManualStateTestConsole.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <sstream>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

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
    const std::string& source)
{
    auto getInt = [&](const std::string& key, int fallback) -> int
    {
        const auto it = context.runtime_int_vars.find(key);
        return it == context.runtime_int_vars.end() ? fallback : it->second;
    };

    const bool isCircleScript =
        scriptPath.find("find_circle") != std::string::npos ||
        scriptPath.find("FindCircle") != std::string::npos;
    const bool isLineScript =
        scriptPath.find("find_line") != std::string::npos ||
        scriptPath.find("FindLine") != std::string::npos;
    const bool isEllipseScript =
        scriptPath.find("find_ellipse") != std::string::npos ||
        scriptPath.find("FindEllipse") != std::string::npos;

    ManualGaugeState gauge;
    gauge.case_id = context.active_case_id;
    gauge.image_id = context.active_image_id;
    gauge.target_id = context.active_target_id;
    gauge.source = source;
    gauge.review_status = "editing";
    gauge.threshold = getInt("global_threshold", 20);
    gauge.method = getInt("global_method", 0);
    gauge.linegap = getInt("global_linegap", 3);
    gauge.wgap = getInt("global_wgap", 32);
    gauge.hgap = getInt("global_hgap", 8);
    gauge.gap = getInt("global_gap", 5);
    gauge.tool_half_width = getInt("global_tool_half_width", 32);
    gauge.filterprofile = getInt("global_filterprofile", 1);

    if (isCircleScript)
    {
        gauge.tool = "Findcircle";
        gauge.has_circle_gauge = true;
        gauge.circle_cx = getInt("global_circle_cx", 0);
        gauge.circle_cy = getInt("global_circle_cy", 0);
        gauge.circle_px = getInt("global_circle_px", gauge.circle_cx);
        gauge.circle_py = getInt("global_circle_py", gauge.circle_cy);
        gauge.radius = static_cast<int>(std::lround(std::hypot(
            static_cast<double>(gauge.circle_px - gauge.circle_cx),
            static_cast<double>(gauge.circle_py - gauge.circle_cy))));
    }
    else if (isEllipseScript)
    {
        gauge.tool = "Findellipse";
        gauge.has_ellipse_gauge = true;
        gauge.ellipse_x0 = getInt("global_ellipse_x0", 0);
        gauge.ellipse_y0 = getInt("global_ellipse_y0", 0);
        gauge.ellipse_x1 = getInt("global_ellipse_x1", 0);
        gauge.ellipse_y1 = getInt("global_ellipse_y1", 0);
    }
    else if (isLineScript)
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

void ViewController::EnsureCxScriptWorkbenchAssetsLoaded()
{
  if (m_manualTest.script_evidence_groups_dirty == false)
    return;

  m_manualTest.script_evidence_groups.clear();

  const std::vector<std::string> fallbackImages =
      BuildEvidenceFallbackImageCandidates(m_manualTest);

  std::size_t fallbackImageIndex = 0;

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
    thumb.status = item.probe_status.empty() ? item.contract_status : item.probe_status;
    thumb.reason = item.review_status;

    AssignFallbackImageToThumb(thumb, fallbackImages, fallbackImageIndex++);

    group.thumbs.push_back(thumb);
  }

  if (!m_manualTest.script_evidence_groups.empty())
  {
    m_manualTest.script_evidence_groups_dirty = false;
    return;
  }

  for (const auto& entry : m_manualTest.catalog_entries)
  {
    if (!IsAllowedEvidenceFallbackScript(entry.path))
      continue;

    bool isVisible = entry.manual_visible && entry.frozen &&
        (entry.expected_result == "ok" || entry.expected_result == "ng_expected");
    if (!isVisible) continue;

    ScriptEvidenceGroup& group =
        findOrCreateGroup(entry.script_id, entry.path, entry.tool);

    ScriptEvidenceThumb thumb;
    thumb.script_id = entry.script_id;
    thumb.script_path = entry.path;
    thumb.tool = entry.tool;
    thumb.parameter_summary = entry.parameter_policy_id;

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

    group.thumbs.push_back(thumb);
  }

  if (m_manualTest.script_evidence_groups.empty())
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

      group.thumbs.push_back(thumb);
      m_manualTest.script_evidence_groups.push_back(group);
    }
  }

  m_manualTest.script_evidence_groups_dirty = false;
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

  if (thumb.image_path.empty())
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

  if (!std::filesystem::exists(thumb.image_path))
  {
    thumb.texture_failed = true;
    thumb.reason = "thumbnail image not found: " + thumb.image_path;
    ++m_manualTest.script_evidence_thumb_load_count_this_frame;
    return;
  }

  cv::Mat image = cv::imread(thumb.image_path, cv::IMREAD_COLOR);
  if (image.empty())
  {
    thumb.texture_failed = true;
    thumb.reason = "thumbnail image read failed: " + thumb.image_path;
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
            "evidence_locked");
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

    if (m_manualTest.script_evidence_row_refs_dirty ||
        m_manualTest.script_evidence_row_refs.empty())
    {
        RebuildScriptEvidenceRowRefs();
    }

    m_manualTest.script_evidence_thumb_load_count_this_frame = 0;

    const float headerHeight = 24.0f;
    const float rowHeight = 92.0f;
    const float minVisibleRows = 4.0f;
    const float reservedBelow = 70.0f;
    const float availableHeight = ImGui::GetContentRegionAvail().y - reservedBelow;
    const float listHeight = std::max(rowHeight * minVisibleRows, availableHeight);

    ImGui::BeginChild("script_evidence_by_group", ImVec2(-1, listHeight), true);

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(m_manualTest.script_evidence_row_refs.size()), rowHeight);

    while (clipper.Step())
    {
        for (int rowIndex = clipper.DisplayStart; rowIndex < clipper.DisplayEnd; ++rowIndex)
        {
            const ScriptEvidenceRowRef& ref = m_manualTest.script_evidence_row_refs[rowIndex];

            if (ref.group_index < 0 ||
                ref.group_index >= static_cast<int>(m_manualTest.script_evidence_groups.size()))
                continue;

            if (ref.is_group_header)
            {
                ImGui::Separator();
                ImGui::TextUnformatted(ref.label.empty() ? "Evidence Group" : ref.label.c_str());
                ImGui::Separator();
                continue;
            }

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
