#include "ViewController.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <sstream>

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

std::string Coordinate(float value)
{
  return std::to_string(static_cast<int>(std::lround(value)));
}

std::string GenerateElementStatement(const OverlayElement& element)
{
  if (element.image_points.empty()) return std::string();
  const OverlayImagePoint& first = element.image_points[0];
  if (element.kind == OverlayKind::Point)
    return "picked_point0.set(" + Coordinate(first.x) + ", " +
      Coordinate(first.y) + ");";
  if (element.kind == OverlayKind::Circle)
    return "afindcircle0.setcircle(" + Coordinate(first.x) + ", " +
      Coordinate(first.y) + ", 0, " + Coordinate(element.radius) + ");";
  if (element.kind == OverlayKind::Polyline)
  {
    std::ostringstream statements;
    for (const OverlayImagePoint& point : element.image_points)
      statements << "polyline0.addpoint(" << Coordinate(point.x) << ", "
                 << Coordinate(point.y) << ");\n";
    return statements.str();
  }
  if (element.image_points.size() < 2) return std::string();
  const OverlayImagePoint& second = element.image_points[1];
  if (element.kind == OverlayKind::Line)
    return "afindline0.setline(" + Coordinate(first.x) + ", " +
      Coordinate(first.y) + ", " + Coordinate(second.x) + ", " +
      Coordinate(second.y) + ");";
  if (element.kind == OverlayKind::Rect)
    return "amatch0.setmatchrect(" +
      Coordinate(std::min(first.x, second.x)) + ", " +
      Coordinate(std::min(first.y, second.y)) + ", " +
      Coordinate(std::fabs(second.x - first.x)) + ", " +
      Coordinate(std::fabs(second.y - first.y)) + ");";
  return std::string();
}

bool ParseNumericParameters(const std::string& text,
                            std::vector<float>& values)
{
  std::istringstream input(text);
  std::string token;
  while (std::getline(input, token, ','))
  {
    try { values.push_back(std::stof(token)); }
    catch (...) { values.clear(); return false; }
  }
  return !values.empty();
}

void InsertStatement(std::string& editor, const std::string& statement)
{
  if (statement.empty()) return;
  if (!editor.empty() && editor.back() != '\n') editor.push_back('\n');
  editor += statement;
  if (editor.empty() || editor.back() != '\n') editor.push_back('\n');
}

void ReplaceEditorLine(std::string& editor, int lineIndex,
                       const std::string& statement)
{
  if (statement.empty() || lineIndex < 0) return;
  std::istringstream input(editor);
  std::vector<std::string> lines;
  std::string line;
  while (std::getline(input, line)) lines.push_back(line);
  if (lineIndex >= static_cast<int>(lines.size())) return;
  lines[static_cast<std::size_t>(lineIndex)] = statement;
  std::ostringstream output;
  for (const std::string& item : lines) output << item << '\n';
  editor = output.str();
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
    element.source = "manual_element";
    m_scriptResult.status = "PENDING";
    m_scriptResult.reason = "manual_element created; parser runtime unchanged";
    m_scriptResult.overlay_ref = "overlay:" + element.ref;
    m_scriptResult.evidence_ref = element.evidence_ref;
    m_scriptResult.result_ref = element.result_ref;
    m_scriptResult.issue_entry_ref = element.issue_entry_ref;
    element.generated_statement = GenerateElementStatement(element);
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
    else if ((tool->kind == OverlayKind::Line || tool->kind == OverlayKind::Rect ||
              tool->kind == OverlayKind::Circle) && tool->action != "connect_refs")
    {
      if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
      {
        m_annotationDragging = true;
        m_annotationDragKind = tool->kind;
        m_annotationDragStart = {imagePoint.x, imagePoint.y};
      }
      if (m_annotationDragging && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
      {
        OverlayElement& element = m_annotationLayer.Create(
          tool->kind, tool->role, tool->source, tool->module_hint);
        element.image_points.push_back(m_annotationDragStart);
        if (tool->kind == OverlayKind::Circle)
        {
          const float dx = imagePoint.x - m_annotationDragStart.x;
          const float dy = imagePoint.y - m_annotationDragStart.y;
          element.radius = std::sqrt(dx * dx + dy * dy);
        }
        else element.image_points.push_back({imagePoint.x, imagePoint.y});
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

  if (m_annotationDragging && insideImage)
  {
    const ImVec2 start = ImageToScreen(m_annotationDragStart.x,
                                       m_annotationDragStart.y);
    const ImVec2 end = ImageToScreen(imagePoint.x, imagePoint.y);
    const ImU32 previewColor = IM_COL32(80, 220, 255, 220);
    if (m_annotationDragKind == OverlayKind::Line)
      drawList->AddLine(start, end, previewColor, 2.0f);
    else if (m_annotationDragKind == OverlayKind::Rect)
      drawList->AddRect(ImVec2(std::min(start.x, end.x), std::min(start.y, end.y)),
                        ImVec2(std::max(start.x, end.x), std::max(start.y, end.y)),
                        previewColor, 0.0f, 0, 2.0f);
    else if (m_annotationDragKind == OverlayKind::Circle)
    {
      const float dx = end.x - start.x;
      const float dy = end.y - start.y;
      drawList->AddCircle(start, std::sqrt(dx * dx + dy * dy),
                          previewColor, 48, 2.0f);
    }
  }

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
      drawList->AddLine(ImVec2(point.x - 8.0f, point.y),
                        ImVec2(point.x + 8.0f, point.y), color, thickness);
      drawList->AddLine(ImVec2(point.x, point.y - 8.0f),
                        ImVec2(point.x, point.y + 8.0f), color, thickness);
      drawList->AddCircleFilled(point, 3.0f, color);
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
    const ImVec2 sourceLabel = ImageToScreen(element.image_points[0].x,
                                             element.image_points[0].y);
    drawList->AddText(ImVec2(sourceLabel.x + 6.0f, sourceLabel.y + 6.0f),
                      color, element.source.empty() ? "manual_element" :
                                                    element.source.c_str());
  }

  for (const RuntimeObjectView& runtime : m_manualTest.runtime_objects)
  {
    if (!runtime.exists_in_parser || runtime.stale || !runtime.has_circle ||
        runtime.type != "Findcircle") continue;
    const ImVec2 center = ImageToScreen(runtime.circle_cx, runtime.circle_cy);
    const float scaleX = m_annotationImageWidth / m_imageViewImage.cols;
    const float scaleY = m_annotationImageHeight / m_imageViewImage.rows;
    const float radius = std::fabs(runtime.circle_radius) *
                         (scaleX + scaleY) * 0.5f;
    drawList->AddCircle(center, radius, IM_COL32(40, 255, 120, 255), 64, 3.0f);
    drawList->AddText(ImVec2(center.x + 8.0f, center.y + 8.0f),
                      IM_COL32(40, 255, 120, 255), "runtime_object");
  }

  if (m_showSourcePreviewOverlay &&
      !m_manualTest.line_views.empty() && m_manualTest.current_line >= 0 &&
      m_manualTest.current_line < static_cast<int>(m_manualTest.line_views.size()))
  {
    const ScriptLineView& line = m_manualTest.line_views[
      static_cast<std::size_t>(m_manualTest.current_line)];
    std::vector<float> values;
    if (ParseNumericParameters(line.params, values))
    {
      const ImU32 scriptColor = IM_COL32(255, 170, 40, 130);
      if (line.method == "setcircle" && values.size() >= 4)
      {
        const ImVec2 center = ImageToScreen(values[0], values[1]);
        const float scaleX = m_annotationImageWidth / m_imageViewImage.cols;
        const float scaleY = m_annotationImageHeight / m_imageViewImage.rows;
        const float radius = std::fabs(values[3]) * (scaleX + scaleY) * 0.5f;
        const int segments = 48;
        for (int segment = 0; segment < segments; segment += 2)
        {
          const float a0 = 6.2831853f * segment / segments;
          const float a1 = 6.2831853f * (segment + 1) / segments;
          drawList->AddLine(ImVec2(center.x + std::cos(a0) * radius,
                                   center.y + std::sin(a0) * radius),
                            ImVec2(center.x + std::cos(a1) * radius,
                                   center.y + std::sin(a1) * radius),
                            scriptColor, 2.0f);
        }
        drawList->AddText(ImVec2(center.x + 8.0f, center.y + 8.0f),
                          scriptColor, "source_preview / not_executed");
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
  auto selectTool = [this](OverlayKind kind)
  {
    for (int i = 0; i < static_cast<int>(m_annotationLayer.Tools().size()); ++i)
    {
      const AnnotationToolDefinition& candidate = m_annotationLayer.Tools()[i];
      if (candidate.kind == kind && candidate.action != "connect_refs")
      {
        m_annotationLayer.SetActiveToolIndex(i);
        m_attachToScriptMode = false;
        if (kind == OverlayKind::Point) m_imageToolMode = ImageToolMode::PointCreate;
        else if (kind == OverlayKind::Line) m_imageToolMode = ImageToolMode::LineCreate;
        else if (kind == OverlayKind::Rect) m_imageToolMode = ImageToolMode::RectCreate;
        else if (kind == OverlayKind::Circle) m_imageToolMode = ImageToolMode::CircleCreate;
        else m_imageToolMode = ImageToolMode::PolylineCreate;
        if (kind != OverlayKind::Polyline) m_activePolylineElement = -1;
        return;
      }
    }
  };
  const bool pointerMode = m_annotationLayer.ActiveToolIndex() < 0 &&
                           !m_attachToScriptMode;
  if (ImGui::Selectable("Pointer / Pan", pointerMode))
  {
    m_annotationLayer.SetActiveToolIndex(-1);
    m_attachToScriptMode = false;
    m_imageToolMode = ImageToolMode::PointerPan;
    m_activePolylineElement = -1;
  }
  if (ImGui::Selectable("Point", m_annotationLayer.ActiveTool() != nullptr &&
                        m_annotationLayer.ActiveTool()->kind == OverlayKind::Point))
    selectTool(OverlayKind::Point);
  if (ImGui::Selectable("Line", m_annotationLayer.ActiveTool() != nullptr &&
                        m_annotationLayer.ActiveTool()->kind == OverlayKind::Line &&
                        m_annotationLayer.ActiveTool()->action != "connect_refs"))
    selectTool(OverlayKind::Line);
  if (ImGui::Selectable("Rect", m_annotationLayer.ActiveTool() != nullptr &&
                        m_annotationLayer.ActiveTool()->kind == OverlayKind::Rect))
    selectTool(OverlayKind::Rect);
  if (ImGui::Selectable("Circle", m_annotationLayer.ActiveTool() != nullptr &&
                        m_annotationLayer.ActiveTool()->kind == OverlayKind::Circle))
    selectTool(OverlayKind::Circle);
  if (ImGui::Selectable("Polyline", m_annotationLayer.ActiveTool() != nullptr &&
                        m_annotationLayer.ActiveTool()->kind == OverlayKind::Polyline))
    selectTool(OverlayKind::Polyline);
  if (ImGui::Selectable("Attach To Script", m_attachToScriptMode))
  {
    m_annotationLayer.SetActiveToolIndex(-1);
    m_attachToScriptMode = true;
    m_imageToolMode = ImageToolMode::AttachToScript;
    m_activePolylineElement = -1;
  }
  ImGui::Separator();
  ImGui::TextDisabled("Manifest tool definitions");
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
  if (ImGui::IsKeyPressed(ImGuiKey_Escape))
  {
    m_activePolylineElement = -1;
    m_annotationDragging = false;
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
    selected->generated_statement = GenerateElementStatement(*selected);
    ImGui::TextWrapped("generated_statement: %s",
                       selected->generated_statement.empty() ? "(none)" :
                       selected->generated_statement.c_str());
    ImGui::TextWrapped("evidence_ref: %s", selected->evidence_ref.c_str());
    ImGui::TextWrapped("result_ref: %s", selected->result_ref.empty() ? "(none)" : selected->result_ref.c_str());
    ImGui::TextWrapped("issue_entry_ref: %s", selected->issue_entry_ref.empty() ? "(none)" : selected->issue_entry_ref.c_str());
    ImGui::Checkbox("visible##inspector", &selected->visible);
    ImGui::SameLine();
    ImGui::Checkbox("editable", &selected->editable);
    if (selected->kind == OverlayKind::Circle && !selected->image_points.empty())
    {
      ImGui::Text("cx: %.3f", selected->image_points[0].x);
      ImGui::Text("cy: %.3f", selected->image_points[0].y);
      ImGui::Text("r: %.3f", selected->radius);
    }
    else ImGui::Text("radius: %.3f", selected->radius);
    for (std::size_t i = 0; i < selected->image_points.size(); ++i)
      ImGui::BulletText("point[%d] image=(%.2f, %.2f)", static_cast<int>(i),
                        selected->image_points[i].x,
                        selected->image_points[i].y);
    if (ImGui::Button("Copy Statement"))
      ImGui::SetClipboardText(selected->generated_statement.c_str());
    ImGui::SameLine();
    if (ImGui::Button("Insert Statement To Editor"))
    {
      InsertStatement(m_manualTest.editor_text, selected->generated_statement);
      m_manualTest.editor_dirty = true;
      m_manualTest.analyzed_text.clear();
      m_annotationStatus = "statement inserted into Script Editor";
    }
    ImGui::SameLine();
    if (ImGui::Button("Replace Current Line"))
    {
      ReplaceEditorLine(m_manualTest.editor_text, m_manualTest.current_line,
                        selected->generated_statement);
      m_manualTest.editor_dirty = true;
      m_manualTest.analyzed_text.clear();
      m_annotationStatus = "current script line replaced";
    }
    if (selected->kind == OverlayKind::Circle)
    {
      if (ImGui::Button("Apply To Script"))
      {
        const bool replace = !m_manualTest.line_views.empty() &&
          m_manualTest.current_line >= 0 &&
          m_manualTest.current_line < static_cast<int>(m_manualTest.line_views.size()) &&
          m_manualTest.line_views[static_cast<std::size_t>(m_manualTest.current_line)].method ==
            "setcircle";
        if (replace)
          ReplaceEditorLine(m_manualTest.editor_text, m_manualTest.current_line,
                            selected->generated_statement);
        else InsertStatement(m_manualTest.editor_text, selected->generated_statement);
        m_manualTest.editor_dirty = true;
        m_manualTest.analyzed_text.clear();
        m_annotationStatus = "manual_element applied to script only";
      }
      ImGui::SameLine();
      if (ImGui::Button("Apply To Parser"))
      {
        if (!QueryParserObjectExists("Findcircle", "afindcircle0"))
          m_imageparser.Compile("Findcircle afindcircle0;");
        const bool applied = m_imageparser.Compile(
          selected->generated_statement.c_str());
        RefreshRuntimeObjectTable("setcircle",
          applied ? "runtime_executed" : "BLOCKED");
        m_scriptResult.status = applied ? "PENDING" : "BLOCKED";
        m_scriptResult.reason = applied ?
          "manual_element applied to parser; runtime objects refreshed" :
          "parser rejected manual circle statement";
        m_annotationStatus = m_scriptResult.reason;
      }
    }
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
  if (ImGui::Button("Clear Elements"))
  {
    m_annotationLayer.Clear();
    m_activePolylineElement = -1;
    m_annotationDragging = false;
  }

  ImGui::End();
}