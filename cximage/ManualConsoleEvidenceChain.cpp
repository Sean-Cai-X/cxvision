#include "pch.h"
#include "ManualConsoleEvidenceChain.h"
#include "ManualConsoleUtils.h"
#include "ManualStateTestConsole.h"

void ViewController::EnsureCxScriptWorkbenchAssetsLoaded()
{
  if (m_manualTest.script_evidence_groups_dirty == false)
    return;

  m_manualTest.script_evidence_groups.clear();

  for (const auto& entry : m_manualTest.catalog_entries)
  {
    bool isVisible = entry.manual_visible && entry.frozen &&
        (entry.expected_result == "ok" || entry.expected_result == "ng_expected");
    if (!isVisible) continue;

    bool found = false;
    for (auto& group : m_manualTest.script_evidence_groups)
    {
      if (group.label == entry.tool)
      {
        ScriptEvidenceThumb thumb;
        thumb.script_id = entry.script_id;
        thumb.script_path = entry.path;
        thumb.tool = entry.tool;
        group.thumbs.push_back(thumb);
        found = true;
        break;
      }
    }

    if (!found)
    {
      ScriptEvidenceGroup group;
      group.label = entry.tool;
      ScriptEvidenceThumb thumb;
      thumb.script_id = entry.script_id;
      thumb.script_path = entry.path;
      thumb.tool = entry.tool;
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
  m_manualTest.script_evidence_groups_dirty = true;
  EnsureCxScriptWorkbenchAssetsLoaded();
}

std::string ViewController::ResolveImagePathFromManifest(const std::string& imageId) const
{
  for (const auto& item : m_manualTest.evidence_items)
  {
    if (item.image_id == imageId && !item.image_path.empty())
      return item.image_path;
  }
  return "";
}

void ViewController::EnsureScriptEvidenceThumbTexture(ScriptEvidenceThumb& thumb)
{
  (void)thumb;
}

void ViewController::SelectScriptEvidenceThumb(int groupIndex, int thumbIndex)
{
  if (groupIndex < 0 || groupIndex >= static_cast<int>(m_manualTest.script_evidence_groups.size()))
    return;

  const ScriptEvidenceGroup& group = m_manualTest.script_evidence_groups[groupIndex];
  if (thumbIndex < 0 || thumbIndex >= static_cast<int>(group.thumbs.size()))
    return;

  const ScriptEvidenceThumb& thumb = group.thumbs[thumbIndex];

  for (const auto& entry : m_manualTest.catalog_entries)
  {
    if (entry.script_id == thumb.script_id)
    {
      m_manualTest.editor_source = "catalog";
      ReadTextFile(entry.path, m_manualTest.editor_text);
      m_manualTest.editor_dirty = false;
      m_manualTest.debug_action = "Load Script";
      m_manualTest.debug_status = "PENDING";
      m_manualTest.debug_reason = "Loaded from catalog: " + thumb.script_id;
      break;
    }
  }
}

void ViewController::DrawScriptEvidenceThumbnailRailByGroup()
{
  if (m_manualTest.script_evidence_groups.empty())
    return;

  ImGui::BeginChild("script_evidence_by_group", ImVec2(-1, 120), true);

  for (std::size_t gi = 0; gi < m_manualTest.script_evidence_groups.size(); ++gi)
  {
    const ScriptEvidenceGroup& group = m_manualTest.script_evidence_groups[gi];

    ImGui::TextUnformatted(group.label.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("(%d)", static_cast<int>(group.thumbs.size()));

    const float thumbWidth = 56.0f;
    const float thumbHeight = 56.0f;
    const float spacing = 6.0f;

    for (std::size_t ii = 0; ii < group.thumbs.size(); ++ii)
    {
      if (ii > 0) ImGui::SameLine();
      const ScriptEvidenceThumb& thumb = group.thumbs[ii];

      ImGui::PushID(gi * 1000 + ii);
      if (ImGui::ImageButton(
              ("thumb_group_" + std::to_string(gi) + "_" + std::to_string(ii)).c_str(),
              static_cast<ImU64>(thumb.texture_id ? thumb.texture_id : 1),
              ImVec2(thumbWidth, thumbHeight)))
      {
        SelectScriptEvidenceThumb(static_cast<int>(gi), static_cast<int>(ii));
      }
      ImGui::SetTooltip("%s\n%s",
                        thumb.script_id.c_str(),
                        thumb.tool.c_str());
      ImGui::PopID();
    }

    ImGui::Separator();
  }

  ImGui::EndChild();
}
