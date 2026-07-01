#include "ViewController.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <filesystem>

namespace
{
namespace fs = std::filesystem;

int AnnotationStringResize(ImGuiInputTextCallbackData* data)
{
  if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
  {
    std::string* value = static_cast<std::string*>(data->UserData);
    value->resize(static_cast<std::size_t>(data->BufTextLen));
    data->Buf = value->data();
  }
  return 0;
}

bool AnnotationInputText(const char* label, std::string& value)
{
  if (value.capacity() < 256) value.reserve(256);
  return ImGui::InputText(label, value.data(), value.capacity() + 1,
                          ImGuiInputTextFlags_CallbackResize,
                          AnnotationStringResize, &value);
}

OverlayImagePoint ElementCenter(const OverlayElement& element)
{
  OverlayImagePoint center;
  if (element.image_points.empty()) return center;
  for (const OverlayImagePoint& point : element.image_points)
  {
    center.x += point.x;
    center.y += point.y;
  }
  center.x /= static_cast<float>(element.image_points.size());
  center.y /= static_cast<float>(element.image_points.size());
  return center;
}
}

void ViewController::initImageEvidenceLayer()
{
  const fs::path repositoryRoot = fs::path(__FILE__).parent_path().parent_path();
  m_annotationManifestPath =
    (repositoryRoot / "cxparser" / "cxscript" / "module" / "cximage" /
     "tool_annotation_basic.cxsc").generic_string();
  m_annotationSessionPath =
    (repositoryRoot / "cxparser" / "cxscript" / "annotations" /
     "session_001.cxann").generic_string();
  m_annotationLayer.LoadManifest(m_annotationManifestPath, m_annotationStatus);
}

ImVec2 ViewController::ImageToScreen(float ix, float iy) const
{
  if (m_imageViewImage.empty()) return ImVec2(m_annotationImagePosX,
                                               m_annotationImagePosY);
  return ImVec2(m_annotationImagePosX +
                  ix * m_annotationImageWidth / m_imageViewImage.cols,
                m_annotationImagePosY +
                  iy * m_annotationImageHeight / m_imageViewImage.rows);
}

ImVec2 ViewController::ScreenToImage(float sx, float sy) const
{
  if (m_imageViewImage.empty() || m_annotationImageWidth <= 0.0f ||
      m_annotationImageHeight <= 0.0f)
    return ImVec2(-1.0f, -1.0f);
  return ImVec2((sx - m_annotationImagePosX) * m_imageViewImage.cols /
                  m_annotationImageWidth,
                (sy - m_annotationImagePosY) * m_imageViewImage.rows /
                  m_annotationImageHeight);
}

void ViewController::drawImageEvidenceOnCanvas(bool canvasHovered,
                                                bool canvasActive,
                                                ImDrawList* drawList)
{
  const AnnotationToolDefinition* tool = m_annotationLayer.ActiveTool();
  ImGuiIO& io = ImGui::GetIO();
  const ImVec2 imagePoint = ScreenToImage(io.MousePos.x, io.MousePos.y);
  const bool insideImage = imagePoint.x >= 0.0f && imagePoint.y >= 0.0f &&
    imagePoint.x < m_imageViewImage.cols && imagePoint.y < m_imageViewImage.rows;

  auto finalizeElement = [this](OverlayElement& element)
  {
    m_scriptResult.status = "PENDING";
    m_scriptResult.reason = "manual annotation created; runtime not executed";
    m_scriptResult.overlay_ref = "overlay:" + element.ref;
    m_scriptResult.evidence_ref = element.evidence_ref;
    m_scriptResult.result_ref = element.result_ref;
    m_scriptResult.issue_entry_ref = element.issue_entry_ref;
    m_annotationStatus = "created " + element.ref;
  };

  if (tool != nullptr && canvasHovered && insideImage)
  {
    if (tool->kind == OverlayKind::Point && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
      OverlayElement& element = m_annotationLayer.Create(
        tool->kind, tool->role, tool->source, tool->module_hint);
      element.image_points.push_back({imagePoint.x, imagePoint.y});
      finalizeElement(element);
    }
    else if (tool->kind == OverlayKind::Circle &&
             ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
      OverlayElement& element = m_annotationLayer.Create(
        tool->kind, tool->role, tool->source, tool->module_hint);
      element.image_points.push_back({imagePoint.x, imagePoint.y});
      element.radius = 30.0f;
      finalizeElement(element);
    }
    else if (tool->kind == OverlayKind::Polyline &&
             ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
      if (m_activePolylineElement < 0 ||
          m_activePolylineElement >= static_cast<int>(m_annotationLayer.Elements().size()))
      {
        OverlayElement& created = m_annotationLayer.Create(
          tool->kind, tool->role, tool->source, tool->module_hint);
        m_activePolylineElement = m_annotationLayer.SelectedIndex();
        created.image_points.push_back({imagePoint.x, imagePoint.y});
        finalizeElement(created);
      }
      else
      {
        OverlayElement& element = m_annotationLayer.Elements()[m_activePolylineElement];
        element.image_points.push_back({imagePoint.x, imagePoint.y});
        m_annotationLayer.Select(m_activePolylineElement);
        finalizeElement(element);
      }
    }
    else if ((tool->kind == OverlayKind::Line || tool->kind == OverlayKind::Rect) &&
             tool->action != "connect_refs")
    {
      if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
      {
        m_annotationDragging = true;
        m_annotationDragStart = {imagePoint.x, imagePoint.y};
      }
      if (m_annotationDragging && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
      {
        OverlayElement& element = m_annotationLayer.Create(
          tool->kind, tool->role, tool->source, tool->module_hint);
        element.image_points.push_back(m_annotationDragStart);
        element.image_points.push_back({imagePoint.x, imagePoint.y});
        m_annotationDragging = false;
        finalizeElement(element);
      }
    }
    else if (tool->action == "connect_refs" &&
             ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
             m_annotationLayer.Elements().size() >= 2)
    {
      const std::size_t count = m_annotationLayer.Elements().size();
      const OverlayImagePoint first = ElementCenter(m_annotationLayer.Elements()[count - 2]);
      const OverlayImagePoint second = ElementCenter(m_annotationLayer.Elements()[count - 1]);
      OverlayElement& element = m_annotationLayer.Create(
        OverlayKind::Line, tool->role, tool->source, tool->module_hint);
      element.image_points.push_back(first);
      element.image_points.push_back(second);
      finalizeElement(element);
    }
  }
  if (!canvasActive && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    m_annotationDragging = false;

  for (const OverlayElement& element : m_annotationLayer.Elements())
  {
    if (!element.visible || element.image_points.empty()) continue;
    const ImU32 color = element.selected ? IM_COL32(255, 220, 40, 255) :
                                          IM_COL32(255, 80, 180, 255);
    const float thickness = element.selected ? 3.0f : 2.0f;
    if (element.kind == OverlayKind::Point)
    {
      const ImVec2 point = ImageToScreen(element.image_points[0].x,
                                         element.image_points[0].y);
      drawList->AddCircleFilled(point, 4.0f, color);
      drawList->AddCircle(point, 8.0f, color, 0, thickness);
    }
    else if ((element.kind == OverlayKind::Line || element.kind == OverlayKind::Rect) &&
             element.image_points.size() >= 2)
    {
      const ImVec2 first = ImageToScreen(element.image_points[0].x,
                                         element.image_points[0].y);
      const ImVec2 second = ImageToScreen(element.image_points[1].x,
                                          element.image_points[1].y);
      if (element.kind == OverlayKind::Rect)
      {
        const ImVec2 minimum(std::min(first.x, second.x), std::min(first.y, second.y));
        const ImVec2 maximum(std::max(first.x, second.x), std::max(first.y, second.y));
        drawList->AddRect(minimum, maximum, color, 0.0f, 0, thickness);
      }
      else
        drawList->AddLine(first, second, color, thickness);
    }
    else if (element.kind == OverlayKind::Circle)
    {
      const ImVec2 center = ImageToScreen(element.image_points[0].x,
                                          element.image_points[0].y);
      const float scaleX = m_annotationImageWidth / m_imageViewImage.cols;
      const float scaleY = m_annotationImageHeight / m_imageViewImage.rows;
      drawList->AddCircle(center, element.radius * (scaleX + scaleY) * 0.5f,
                          color, 48, thickness);
    }
    else if (element.kind == OverlayKind::Polyline)
    {
      for (std::size_t i = 1; i < element.image_points.size(); ++i)
      {
        const ImVec2 first = ImageToScreen(element.image_points[i - 1].x,
                                           element.image_points[i - 1].y);
        const ImVec2 second = ImageToScreen(element.image_points[i].x,
                                            element.image_points[i].y);
        drawList->AddLine(first, second, color, thickness);
      }
    }
  }
}

void ViewController::drawImageEvidencePanels()
{
  ImGui::SetNextWindowPos(ImVec2(350, 40), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(620, 720), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Image Evidence / Annotation Tools"))
  {
    ImGui::End();
    return;
  }

  ImGui::Text("Annotation Tool Manifest");
  AnnotationInputText("Manifest path", m_annotationManifestPath);
  if (ImGui::Button("Load Tool Manifest"))
    m_annotationLayer.LoadManifest(m_annotationManifestPath, m_annotationStatus);
  ImGui::SameLine();
  ImGui::TextWrapped("%s", m_annotationStatus.c_str());
  ImGui::TextDisabled(
    "CxScript direct logic: AnnotationTool declaration -> field assignments -> "
    "register_annotation_tool(object)");
  ImGui::TextDisabled(
    "cximage=pending_binding  torch=pending_binding  "
    "mlpack=pending_binding  ensmallen=pending_binding");

  ImGui::Separator();
  ImGui::Columns(2, "annotation_columns", true);
  ImGui::Text("Tool Palette");
  for (int i = 0; i < static_cast<int>(m_annotationLayer.Tools().size()); ++i)
  {
    const AnnotationToolDefinition& tool = m_annotationLayer.Tools()[i];
    ImGui::PushID(i);
    if (ImGui::Selectable(tool.name.c_str(), m_annotationLayer.ActiveToolIndex() == i))
    {
      m_annotationLayer.SetActiveToolIndex(i);
      if (tool.kind != OverlayKind::Polyline) m_activePolylineElement = -1;
    }
    ImGui::TextWrapped("%s | %s | %s", ImageAnnotationLayer::KindName(tool.kind),
                       tool.role.c_str(), tool.action.c_str());
    ImGui::TextWrapped("source=%s modules=%s", tool.source.c_str(),
                       tool.module_hint.c_str());
    ImGui::PopID();
  }
  if (ImGui::Button("Finish Polyline")) m_activePolylineElement = -1;
  ImGui::SameLine();
  if (ImGui::Button("Pointer / Pan"))
  {
    m_annotationLayer.SetActiveToolIndex(-1);
    m_activePolylineElement = -1;
  }

  ImGui::NextColumn();
  ImGui::Text("Element List");
  for (int i = 0; i < static_cast<int>(m_annotationLayer.Elements().size()); ++i)
  {
    OverlayElement& element = m_annotationLayer.Elements()[i];
    ImGui::PushID(1000 + i);
    if (ImGui::Selectable(element.ref.c_str(), element.selected))
    {
      m_annotationLayer.Select(i);
      m_scriptResult.overlay_ref = "overlay:" + element.ref;
      m_scriptResult.evidence_ref = element.evidence_ref;
      m_scriptResult.result_ref = element.result_ref;
      m_scriptResult.issue_entry_ref = element.issue_entry_ref;
    }
    ImGui::SameLine();
    ImGui::Checkbox("visible", &element.visible);
    ImGui::TextWrapped("%s | role=%s | source=%s | modules=%s",
                       ImageAnnotationLayer::KindName(element.kind),
                       element.role.c_str(), element.source.c_str(),
                       element.module_hint.c_str());
    ImGui::PopID();
  }
  ImGui::Columns(1);

  ImGui::Separator();
  ImGui::Text("Element Inspector");
  OverlayElement* selected = m_annotationLayer.Selected();
  if (selected == nullptr)
  {
    ImGui::TextDisabled("No element selected");
  }
  else
  {
    ImGui::Text("ref: %s", selected->ref.c_str());
    ImGui::Text("kind: %s", ImageAnnotationLayer::KindName(selected->kind));
    AnnotationInputText("role", selected->role);
    ImGui::Text("source: %s", selected->source.c_str());
    AnnotationInputText("module_hint", selected->module_hint);
    AnnotationInputText("label", selected->label);
    ImGui::TextWrapped("evidence_ref: %s", selected->evidence_ref.c_str());
    ImGui::TextWrapped("result_ref: %s", selected->result_ref.empty() ? "(none)" : selected->result_ref.c_str());
    ImGui::TextWrapped("issue_entry_ref: %s", selected->issue_entry_ref.empty() ? "(none)" : selected->issue_entry_ref.c_str());
    ImGui::Checkbox("visible##inspector", &selected->visible);
    ImGui::SameLine();
    ImGui::Checkbox("editable", &selected->editable);
    ImGui::Text("radius: %.3f", selected->radius);
    for (std::size_t i = 0; i < selected->image_points.size(); ++i)
      ImGui::BulletText("point[%d] image=(%.2f, %.2f)", static_cast<int>(i),
                        selected->image_points[i].x,
                        selected->image_points[i].y);
  }

  ImGui::Separator();
  AnnotationInputText("Session path", m_annotationSessionPath);
  if (ImGui::Button("Save Elements"))
    m_annotationLayer.SaveElements(m_annotationSessionPath,
                                   m_scriptResult.image_ref,
                                   m_annotationStatus);
  ImGui::SameLine();
  if (ImGui::Button("Load Elements"))
  {
    std::string imageRef;
    if (m_annotationLayer.LoadElements(m_annotationSessionPath,
                                       imageRef, m_annotationStatus) &&
        !imageRef.empty())
      m_scriptResult.image_ref = imageRef;
  }
  ImGui::SameLine();
  if (ImGui::Button("Clear Elements")) m_annotationLayer.Clear();

  ImGui::End();
}