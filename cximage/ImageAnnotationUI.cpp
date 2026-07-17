#include "ViewController.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <sstream>

#include "CxAnnotationToolRuntime.h"
#include "CxUnifiedLog.h"
#include "Findcircle.h"
#include "Findellipse.h"
#include "FindRect.h"
#include "findline.h"
#include "shapebase.h"
#include "RectShape.h"
#include "CircleShape.h"
#include "EllipseShape.h"
#include "LineGaugeShape.h"
#include "ManualConsoleCxScriptDebug.h"
#include "PolylineShape.h"

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

void UpdateManualGaugeFromElement(ManualTestContext& context,
                                  const OverlayElement& element)
{
  ManualGaugeState& gauge = context.current_gauge;
  if (element.kind == OverlayKind::Line && element.image_points.size() >= 2)
  {
    gauge.tool = "Findline";
    gauge.has_line_gauge = true;
    gauge.line_x0 = static_cast<int>(std::lround(element.image_points[0].x));
    gauge.line_y0 = static_cast<int>(std::lround(element.image_points[0].y));
    gauge.line_x1 = static_cast<int>(std::lround(element.image_points[1].x));
    gauge.line_y1 = static_cast<int>(std::lround(element.image_points[1].y));
    gauge.source = "annotation_handle";
    gauge.review_status = "editing";
    gauge.accepted = false;
    gauge.dirty = true;
  }
  else if (element.kind == OverlayKind::Circle && !element.image_points.empty())
  {
    gauge.tool = "Findcircle";
    gauge.has_circle_gauge = true;
    gauge.circle_cx = static_cast<int>(std::lround(element.image_points[0].x));
    gauge.circle_cy = static_cast<int>(std::lround(element.image_points[0].y));
    gauge.circle_px = static_cast<int>(std::lround(element.image_points[0].x + element.radius));
    gauge.circle_py = static_cast<int>(std::lround(element.image_points[0].y));
    gauge.source = "annotation_handle";
    gauge.review_status = "editing";
    gauge.accepted = false;
    gauge.dirty = true;
  }
}

void UpdateManualGaugeFromShapeElement(
    ManualTestContext& context,
    const CxShapeElement& element)
{
  if (!element.shape || !element.editable)
    return;

  ManualGaugeState& gauge = context.current_gauge;
  if (element.owner_type == "Findline" &&
      element.shape->kind() == CxShapeKind::LineGauge)
  {
    const auto* line = dynamic_cast<const LineGaugeShape*>(element.shape.get());
    if (line == nullptr)
      return;

    gauge.tool = "Findline";
    gauge.has_line_gauge = true;
    gauge.has_circle_gauge = false;
    gauge.line_x0 = static_cast<int>(std::lround(line->x0()));
    gauge.line_y0 = static_cast<int>(std::lround(line->y0()));
    gauge.line_x1 = static_cast<int>(std::lround(line->x1()));
    gauge.line_y1 = static_cast<int>(std::lround(line->y1()));
    gauge.tool_half_width = std::max(
        1, static_cast<int>(std::lround(line->halfWidth())));
  }
  else if (element.owner_type == "Findcircle" &&
           element.shape->kind() == CxShapeKind::Circle)
  {
    CxShapePoint center;
    double radius = 0.0;
    double inner_radius = 0.0;
    if (!element.shape->exportCircle(center, radius, inner_radius))
      return;

    gauge.tool = "Findcircle";
    gauge.has_circle_gauge = true;
    gauge.has_line_gauge = false;
    gauge.circle_cx = static_cast<int>(std::lround(center.x));
    gauge.circle_cy = static_cast<int>(std::lround(center.y));
    gauge.radius = std::max(1, static_cast<int>(std::lround(radius)));
    gauge.circle_px = gauge.circle_cx + gauge.radius;
    gauge.circle_py = gauge.circle_cy;
    gauge.inner_radius = std::max(
        0, static_cast<int>(std::lround(inner_radius)));
  }
  else
  {
    return;
  }

  gauge.source = "shape_drag";
  gauge.review_status = "editing";
  gauge.accepted = false;
  gauge.dirty = true;
}

bool ExportShapeElementToRuntimeGlobals(
    ManualTestContext& context,
    const CxShapeElement& element,
    std::string& reason)
{
  if (!element.shape)
  {
    reason = "selected shape has no geometry";
    return false;
  }

  auto setInt = [&context](const std::string& name, double value)
  {
    context.runtime_int_vars[name] = static_cast<int>(std::lround(value));
  };

  if (element.shape->kind() == CxShapeKind::Circle)
  {
    CxShapePoint center;
    double radius = 0.0;
    double inner_radius = 0.0;
    if (!element.shape->exportCircle(center, radius, inner_radius) || radius <= 0.0)
    {
      reason = "selected circle cannot export center/radius";
      return false;
    }

    setInt("global_circle_cx", center.x);
    setInt("global_circle_cy", center.y);
    setInt("global_circle_px", center.x + radius);
    setInt("global_circle_py", center.y);
    setInt("global_seed_x", center.x);
    setInt("global_seed_y", center.y);
    UpdateManualGaugeFromShapeElement(context, element);
    reason = "circle exported to global_circle_* and global_seed_*";
    return true;
  }

  CxShapePoint p0;
  CxShapePoint p1;
  if (element.shape->exportLine(p0, p1))
  {
    setInt("global_roi_x0", p0.x);
    setInt("global_roi_y0", p0.y);
    setInt("global_roi_x1", p1.x);
    setInt("global_roi_y1", p1.y);
    UpdateManualGaugeFromShapeElement(context, element);
    reason = "line exported to global_roi_*";
    return true;
  }

  std::vector<CxShapePoint> points;
  bool closed = false;
  element.shape->exportPolyline(points, closed);
  if (points.empty())
    element.shape->exportPoints(points);

  if (points.empty())
  {
    reason = "selected shape has no exportable points";
    return false;
  }

  double min_x = points[0].x;
  double min_y = points[0].y;
  double max_x = points[0].x;
  double max_y = points[0].y;
  for (const CxShapePoint& p : points)
  {
    min_x = std::min(min_x, p.x);
    min_y = std::min(min_y, p.y);
    max_x = std::max(max_x, p.x);
    max_y = std::max(max_y, p.y);
  }

  setInt("global_roi_x0", min_x);
  setInt("global_roi_y0", min_y);
  setInt("global_roi_x1", max_x);
  setInt("global_roi_y1", max_y);
  setInt("global_seed_x", (min_x + max_x) * 0.5);
  setInt("global_seed_y", (min_y + max_y) * 0.5);

  if (points.size() == 1)
    reason = "point exported to global_seed_* and degenerate global_roi_*";
  else
    reason = "shape bounds exported to global_roi_* and center to global_seed_*";
  return true;
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

    std::string init_reason;
    if (!m_parserOwner.Initialize(init_reason))
    {
        m_annotationStatus = "parser initialize failed: " + init_reason;
        return;
    }

    CxAnnotationToolManifestSnapshot snapshot;
    if (!m_parserOwner.ParseAnnotationToolManifest(m_annotationManifestPath, snapshot, m_annotationStatus))
    {
        return;
    }

    if (!m_annotationLayer.ApplyToolManifestSnapshot(snapshot, m_annotationStatus))
    {
        return;
    }
}

bool ViewController::IsAnnotationCreateModeActive() const
{
    return m_imageToolEnabled &&
           m_imageToolMode != ImageToolMode::PointerPan;
}

bool ViewController::IsMouseInsideImageCanvas(const ImVec2& p) const
{
    return p.x >= m_imageCanvasMin.x &&
           p.x <= m_imageCanvasMax.x &&
           p.y >= m_imageCanvasMin.y &&
           p.y <= m_imageCanvasMax.y;
}

bool ViewController::HasActiveFindCircleGauge() const
{
    return m_manualTest.current_gauge.has_circle_gauge ||
           m_manualTest.current_gauge.tool == "Findcircle";
}

bool ViewController::HasEditableFindCircleGauge() const
{
    return m_manualTest.current_gauge.has_circle_gauge &&
           m_manualTest.current_gauge.circle_cx > 0 &&
           m_manualTest.current_gauge.circle_cy > 0 &&
           m_manualTest.current_gauge.radius > 0;
}

const char* ViewController::ImageToolModeName(ImageToolMode mode)
{
    switch (mode)
    {
    case ImageToolMode::PointerPan: return "Pointer / Pan";
    case ImageToolMode::PointCreate: return "Point";
    case ImageToolMode::LineCreate: return "Line";
    case ImageToolMode::RectCreate: return "Rect";
    case ImageToolMode::CircleCreate: return "Circle";
    case ImageToolMode::EllipseCreate: return "Ellipse";
    case ImageToolMode::PolylineCreate: return "Polyline";
    case ImageToolMode::AutoBoundary: return "Auto Boundary";
    case ImageToolMode::AttachToScript: return "Attach To Script";
    default: return "Unknown";
    }
}

void ViewController::CancelAnnotationCreate()
{
    m_annotationCreateActive = false;
    m_annotationDragging = false;
    m_activePolylineElement = -1;
    m_activePolylinePoints.clear();
}

void ViewController::ClampImagePointToImageBounds(double& x, double& y) const
{
    if (!m_imageViewImage.empty())
    {
        x = std::max(0.0, std::min(x, static_cast<double>(m_imageViewImage.cols)));
        y = std::max(0.0, std::min(y, static_cast<double>(m_imageViewImage.rows)));
    }
}

ImVec2 ViewController::ImageToScreen(float ix, float iy) const
{
  if (m_imageViewImage.empty()) return ImVec2(m_annotationImagePosX,
                                               m_annotationImagePosY);
  float sx = m_annotationImageWidth / static_cast<float>(m_imageViewImage.cols);
  float sy = m_annotationImageHeight / static_cast<float>(m_imageViewImage.rows);
  return ImVec2(m_annotationImagePosX + ix * sx,
                m_annotationImagePosY + iy * sy);
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

ImVec2 ViewController::ImageToScreenPoint(const OverlayImagePoint& p) const
{
  return ImageToScreen(p.x, p.y);
}

ImVec2 ViewController::ScreenToImagePoint(const ImVec2& p) const
{
  return ScreenToImage(p.x, p.y);
}

static double Distance2(double x0, double y0, double x1, double y1)
{
    const double dx = x1 - x0;
    const double dy = y1 - y0;
    return dx * dx + dy * dy;
}

static ImageToolMode ToolModeFromAnnotationTool(const AnnotationToolDefinition& tool)
{
    if (tool.kind == OverlayKind::Point)
        return ImageToolMode::PointCreate;
    if (tool.kind == OverlayKind::Line)
        return ImageToolMode::LineCreate;
    if (tool.kind == OverlayKind::Rect)
        return ImageToolMode::RectCreate;
    if (tool.kind == OverlayKind::Circle)
        return ImageToolMode::CircleCreate;
    if (tool.kind == OverlayKind::Ellipse)
        return ImageToolMode::EllipseCreate;
    if (tool.kind == OverlayKind::Polyline ||
        tool.kind == OverlayKind::BoundaryPolyline)
        return ImageToolMode::PolylineCreate;
    if (tool.kind == OverlayKind::AutoBoundaryRequest ||
        tool.action == "auto_segmentation")
        return ImageToolMode::AutoBoundary;
    return ImageToolMode::PointerPan;
}

bool ViewController::CommitDraftShapeFromTool(
    const AnnotationToolDefinition& tool,
    CxImagePointerResult& out)
{
    const double dx = m_annotationDragEnd.x - m_annotationDragStart.x;
    const double dy = m_annotationDragEnd.y - m_annotationDragStart.y;

    if (tool.shape_type == "LineShape")
    {
        const double dist = std::sqrt(dx * dx + dy * dy);
        if (dist < 2.0)
        {
            out.status = "draft_too_small";
            out.reason = "line length too small";
            return false;
        }
    }
    else if (tool.shape_type == "RectShape" || tool.shape_type == "AutoBoundary")
    {
        const double w = std::abs(dx);
        const double h = std::abs(dy);
        if (w < 2.0 || h < 2.0)
        {
            out.status = "draft_too_small";
            out.reason = "rect width/height too small";
            return false;
        }
    }
    else if (tool.shape_type == "CircleShape")
    {
        const double r = std::sqrt(dx * dx + dy * dy);
        if (r < 2.0)
        {
            out.status = "draft_too_small";
            out.reason = "circle radius too small";
            return false;
        }
    }
    else if (tool.shape_type == "EllipseShape")
    {
        const double rx = std::abs(dx);
        const double ry = std::abs(dy);
        if (rx < 2.0 || ry < 2.0)
        {
            out.status = "draft_too_small";
            out.reason = "ellipse radius too small";
            return false;
        }
    }

    std::vector<CxShapePoint> pts = {
        {m_annotationDragStart.x, m_annotationDragStart.y},
        {m_annotationDragEnd.x, m_annotationDragEnd.y}
    };
    auto shape = CreateInitialShapeForTool(tool, pts);
    if (!shape)
    {
        out.status = "failed";
        out.reason = "CreateInitialShapeForTool failed";
        return false;
    }

    CxShapeElement& element = m_annotationLayer.CreateFromTool(tool, std::move(shape));
    out.consumed = true;
    out.phase = "create_shape";
    out.status = "created";
    out.created_ref = element.stable_ref;
    out.reason = "shape created from annotation tool";
    CXLOG_INFO("ImageAnnotationUI", "annotation_shape_created", "created", "ref=" + out.created_ref);
    return true;
}

CxImagePointerResult ViewController::ProcessImageAnnotationPointerFrame(
    const CxImagePointerFrame& frame)
{
    CxImagePointerResult out;

    const bool hasActiveAnnotationState =
        m_annotationLayer.HasActiveDrag() || m_annotationDragging;
    const bool pointerInsideActiveCanvas =
        frame.canvas_hovered && frame.inside_image;
    const bool logPointerBegin =
        (pointerInsideActiveCanvas && frame.HasInteractionEvent()) ||
        (hasActiveAnnotationState &&
         (frame.HasInteractionEvent() || frame.HasDragMoveEvent()));

    if (logPointerBegin)
    {
        CXLOG_INFO("ImageAnnotationUI", "annotation_pointer_begin", "running",
            "x=" + std::to_string(frame.image_x) + ", y=" + std::to_string(frame.image_y) +
            ", inside=" + std::string(frame.inside_image ? "true" : "false"));
    }

    if (frame.escape_pressed)
    {
        m_annotationDragging = false;
        m_activePolylinePoints.clear();
        m_annotationLayer.CancelDrag();
        out.consumed = true;
        out.phase = "cancel";
        out.status = "cancelled";
        m_lastPointerResult = out;
        CXLOG_INFO("ImageAnnotationUI", "annotation_cancel", "cancelled", "reason=escape");
        return out;
    }

    if (m_annotationLayer.HasActiveDrag())
    {
        out.consumed = true;
        out.phase = "drag_existing";

        if (frame.left_down)
        {
            // Existing gauge/ROI handles must be allowed to move outside the
            // image bounds.  Findcircle radius handles and wide Findline
            // boxes often intentionally extend past the visible image while
            // the center or fitted result remains meaningful.  New shape
            // creation is still clamped in its draft path below.
            const bool ok = m_annotationLayer.UpdateDrag(frame.image_x, frame.image_y);
            out.status = ok ? "dragging" : "failed";
            out.reason = ok ? "drag updated" : "UpdateDrag failed";
            m_lastPointerResult = out;
            return out;
        }

        if (frame.left_released)
        {
            CxShapeCommitResult commit;
            const bool ok = m_annotationLayer.CommitEdit(commit);
            out.commit = commit;
            out.status = ok ? "committed" : "failed";
            out.reason = commit.reason;

            if (ok && commit.owner_type == "fastmatch" && commit.editable && !commit.owner_binding.empty())
            {
                void* toolObj = m_parserDebugBridge.QueryClassObject("fastmatch", commit.owner_ref);
                if (toolObj != nullptr)
                {
                    fastmatch* tool = static_cast<fastmatch*>(toolObj);
                    auto* layer = &m_annotationLayer;
                    const auto& elements = layer->ShapeElements();
                    for (const auto& elem : elements)
                    {
                        if (elem.stable_ref == commit.stable_ref && elem.shape != nullptr && elem.shape->kind() == CxShapeKind::Rect)
                        {
                            auto* rectShape = dynamic_cast<RectShape*>(elem.shape.get());
                            if (rectShape != nullptr)
                            {
                                const double x0 = rectShape->x0();
                                const double y0 = rectShape->y0();
                                const double x1 = rectShape->x1();
                                const double y1 = rectShape->y1();
                                std::string reason;
                                if (tool->ApplyDisplayShapeEdit(commit.owner_binding, commit.semantic_role,
                                                                x0, y0, x1, y1, reason))
                                {
                                    commit.runtime_writeback = true;
                                    out.reason = reason;
                                }
                                else
                                {
                                    out.reason = "FastMatch edit rejected: " + reason;
                                }
                                break;
                            }
                        }
                    }
                }
            }
            else if (ok && commit.owner_type == "Findcircle" && commit.editable &&
                     commit.owner_binding == "setcircle")
            {
                const CxShapeElement* edited =
                    m_annotationLayer.FindShapeByStableRef(commit.stable_ref);
                Findcircle* tool = static_cast<Findcircle*>(
                    m_parserDebugBridge.QueryClassObject("Findcircle", commit.owner_ref));
                if (edited != nullptr && edited->shape != nullptr && tool != nullptr)
                {
                    CxShapePoint center;
                    double radius = 0.0;
                    double inner_radius = 0.0;
                    if (edited->shape->exportCircle(center, radius, inner_radius) && radius > 0.0)
                    {
                        const int cx = static_cast<int>(std::lround(center.x));
                        const int cy = static_cast<int>(std::lround(center.y));
                        const int px = static_cast<int>(std::lround(center.x + radius));
                        const int py = cy;
                        tool->setcircle(cx, cy, px, py);
                        m_annotationLayer.ConfirmRuntimeWriteback(commit.stable_ref);
                        commit.runtime_writeback = true;
                        out.reason = "Findcircle ROI edit written back to setcircle";
                    }
                    else
                    {
                        out.reason = "Findcircle edit rejected: circle geometry export failed";
                    }
                }
                else
                {
                    out.reason = "Findcircle edit rejected: runtime object or shape unavailable";
                }
            }
            else if (ok && commit.owner_type == "Findline" && commit.editable &&
                     commit.owner_binding == "setline")
            {
                const CxShapeElement* edited =
                    m_annotationLayer.FindShapeByStableRef(commit.stable_ref);
                Findline* tool = static_cast<Findline*>(
                    m_parserDebugBridge.QueryClassObject("Findline", commit.owner_ref));
                if (edited != nullptr && edited->shape != nullptr && tool != nullptr)
                {
                    const LineGaugeShape* line =
                        dynamic_cast<const LineGaugeShape*>(edited->shape.get());
                    if (line != nullptr)
                    {
                        const int x0 = static_cast<int>(std::lround(line->x0()));
                        const int y0 = static_cast<int>(std::lround(line->y0()));
                        const int x1 = static_cast<int>(std::lround(line->x1()));
                        const int y1 = static_cast<int>(std::lround(line->y1()));
                        const int half_width = std::max(
                            1, static_cast<int>(std::lround(line->halfWidth())));
                        tool->setline(x0, y0, x1, y1, half_width);
                        m_annotationLayer.ConfirmRuntimeWriteback(commit.stable_ref);
                        commit.runtime_writeback = true;
                        out.reason = "Findline ROI edit written back to setline";
                    }
                    else
                    {
                        out.reason = "Findline edit rejected: line gauge geometry unavailable";
                    }
                }
                else
                {
                    out.reason = "Findline edit rejected: runtime object or shape unavailable";
                }
            }
            else if (ok && commit.owner_type == "Findellipse" && commit.editable &&
                     commit.owner_binding == "setellipse")
            {
                const CxShapeElement* edited =
                    m_annotationLayer.FindShapeByStableRef(commit.stable_ref);
                Findellipse* tool = static_cast<Findellipse*>(
                    m_parserDebugBridge.QueryClassObject("Findellipse", commit.owner_ref));
                if (edited != nullptr && edited->shape != nullptr && tool != nullptr)
                {
                    std::vector<CxShapePoint> points;
                    edited->shape->exportPoints(points);
                    if (points.size() >= 3)
                    {
                        const double cx = points[0].x;
                        const double cy = points[0].y;
                        const double rx = std::abs(points[1].x - cx);
                        const double ry = std::abs(points[2].y - cy);
                        if (rx >= 1.0 && ry >= 1.0)
                        {
                            tool->setellipse(
                                static_cast<int>(std::lround(cx - rx)),
                                static_cast<int>(std::lround(cy - ry)),
                                static_cast<int>(std::lround(cx + rx)),
                                static_cast<int>(std::lround(cy + ry)));
                            m_annotationLayer.ConfirmRuntimeWriteback(commit.stable_ref);
                            commit.runtime_writeback = true;
                            out.reason = "Findellipse ROI edit written back to setellipse";
                        }
                        else
                        {
                            out.reason = "Findellipse edit rejected: ellipse radius too small";
                        }
                    }
                    else
                    {
                        out.reason = "Findellipse edit rejected: ellipse geometry unavailable";
                    }
                }
                else
                {
                    out.reason = "Findellipse edit rejected: runtime object or shape unavailable";
                }
            }
            else if (ok && commit.owner_type == "FindRect" && commit.editable &&
                     (commit.owner_binding == "setrect" ||
                      commit.owner_binding == "setrotatedrect"))
            {
                const CxShapeElement* edited =
                    m_annotationLayer.FindShapeByStableRef(commit.stable_ref);
                FindRect* tool = static_cast<FindRect*>(
                    m_parserDebugBridge.QueryClassObject("FindRect", commit.owner_ref));
                if (edited != nullptr && edited->shape != nullptr && tool != nullptr)
                {
                    std::vector<CxShapePoint> points;
                    bool closed = false;
                    edited->shape->exportPolyline(points, closed);
                    if (points.empty())
                        edited->shape->exportPoints(points);

                    if (!points.empty())
                    {
                        double min_x = points[0].x;
                        double min_y = points[0].y;
                        double max_x = points[0].x;
                        double max_y = points[0].y;
                        for (const CxShapePoint& p : points)
                        {
                            min_x = std::min(min_x, p.x);
                            min_y = std::min(min_y, p.y);
                            max_x = std::max(max_x, p.x);
                            max_y = std::max(max_y, p.y);
                        }
                        const double w = max_x - min_x;
                        const double h = max_y - min_y;
                        if (w >= 2.0 && h >= 2.0)
                        {
                            tool->setrect(
                                static_cast<int>(std::lround(min_x)),
                                static_cast<int>(std::lround(min_y)),
                                static_cast<int>(std::lround(w)),
                                static_cast<int>(std::lround(h)));
                            m_annotationLayer.ConfirmRuntimeWriteback(commit.stable_ref);
                            commit.runtime_writeback = true;
                            out.reason = "FindRect ROI edit written back to setrect";
                        }
                        else
                        {
                            out.reason = "FindRect edit rejected: rect too small";
                        }
                    }
                    else
                    {
                        out.reason = "FindRect edit rejected: rect geometry unavailable";
                    }
                }
                else
                {
                    out.reason = "FindRect edit rejected: runtime object or shape unavailable";
                }
            }

            if (ok)
            {
                const CxShapeElement* edited =
                    m_annotationLayer.FindShapeByStableRef(commit.stable_ref);
                if (edited != nullptr)
                {
                    UpdateManualGaugeFromShapeElement(m_manualTest, *edited);
                    std::string exportReason;
                    ExportShapeElementToRuntimeGlobals(m_manualTest, *edited, exportReason);
                }
            }

            if (ok)
            {
                CXLOG_INFO("ImageAnnotationUI", "annotation_drag_commit", "committed", "reason=" + out.reason);
                std::string snapshotPath;
                std::string snapshotReason;
                if (!SaveCxDebugSnapshotText(m_manualTest, snapshotPath, snapshotReason))
                {
                    m_manualTest.debug_reason +=
                        " | debug snapshot save failed: " + snapshotReason;
                }
            }
            m_lastPointerResult = out;
            return out;
        }

        m_lastPointerResult = out;
        return out;
    }

    if (m_annotationDragging)
    {
        out.consumed = true;
        out.phase = "create_shape";

        if (frame.left_down)
        {
            double clamped_x = frame.image_x;
            double clamped_y = frame.image_y;
            ClampImagePointToImageBounds(clamped_x, clamped_y);
            m_annotationDragEnd = {(float)clamped_x, (float)clamped_y};
            out.status = "draft_updating";
            out.reason = "draft updating";
            m_lastPointerResult = out;
            return out;
        }

        if (frame.left_released)
        {
            const AnnotationToolDefinition* activeTool = m_annotationLayer.ActiveTool();
            if (activeTool)
            {
                CommitDraftShapeFromTool(*activeTool, out);
            }
            else
            {
                out.status = "failed";
                out.reason = "no active tool";
            }
            m_annotationDragging = false;
            m_lastPointerResult = out;
            return out;
        }

        m_lastPointerResult = out;
        return out;
    }

    if (!frame.canvas_hovered || !frame.inside_image)
    {
        out.phase = "pointer_reject";
        out.status = "ignored";
        out.reason = "outside image canvas";
        if (frame.left_released && hasActiveAnnotationState)
        {
            m_annotationDragging = false;
            m_annotationLayer.CancelDrag();
            out.status = "cancelled";
            out.reason = "outside image canvas release; annotation state cleared";
        }
        m_lastPointerResult = out;
        if (hasActiveAnnotationState && frame.HasInteractionEvent())
        {
            CXLOG_INFO(
                "ImageAnnotationUI",
                "annotation_pointer_reject",
                out.status,
                "reason=" + out.reason);
        }
        return out;
    }

    const AnnotationToolDefinition* activeTool = m_annotationLayer.ActiveTool();
    const bool hasActiveTool = activeTool != nullptr;
    const bool inCreateMode = IsAnnotationCreateModeActive();

    if (!inCreateMode)
    {
        if (frame.left_clicked)
        {
            const double imageTolerance = std::clamp(
                10.0 / std::max(0.05f, m_imageViewZoom),
                3.0,
                80.0);
            CxShapeHitResult hit =
                m_annotationLayer.HitTest(
                    frame.image_x, frame.image_y, imageTolerance);

            if (hit.hit)
            {
                m_annotationLayer.SelectShape(hit.element_index);
                const bool dragStarted =
                    m_annotationLayer.BeginDrag(
                        hit, frame.image_x, frame.image_y);

                const CxShapeElement* selected = m_annotationLayer.SelectedShape();
                out.consumed = true;
                out.phase = "select_existing";
                out.status = dragStarted ? "selected" : "failed";
                out.selected_ref = selected ? selected->stable_ref : "";
                out.reason = dragStarted
                    ? "shape selected and drag started"
                    : "shape selected but BeginDrag failed";
                if (dragStarted)
                    CXLOG_INFO("ImageAnnotationUI", "annotation_drag_begin", "running", "ref=" + out.selected_ref);
                m_lastPointerResult = out;
                return out;
            }
        }

        out.phase = "pointer_pan";
        out.status = "not_consumed";
        m_lastPointerResult = out;
        return out;
    }

    if (!hasActiveTool)
    {
        out.consumed = true;
        out.phase = "create_shape";
        out.status = "failed";
        out.reason = "annotation tool enabled but ActiveTool is null";
        m_lastPointerResult = out;
        CXLOG_INFO("ImageAnnotationUI", "annotation_tool_missing", "failed", "reason=" + out.reason);
        return out;
    }

    OverlayKind currentKind = activeTool->kind;

    if (currentKind == OverlayKind::Point)
    {
        if (frame.left_clicked)
        {
            if (hasActiveTool)
            {
                const AnnotationToolDefinition* tool = activeTool;
                if (!tool)
                {
                    out.consumed = true;
                    out.phase = "create_shape";
                    out.status = "failed";
                    out.reason = "active tool definition unavailable";
                    m_lastPointerResult = out;
                    return out;
                }

                std::vector<CxShapePoint> pts = {{frame.image_x, frame.image_y}};
                auto shape = CreateInitialShapeForTool(*tool, pts);
                if (shape)
                {
                    CxShapeElement& element = m_annotationLayer.CreateFromTool(*tool, std::move(shape));
                    out.consumed = true;
                    out.phase = "create_shape";
                    out.status = "created";
                    out.created_ref = element.stable_ref;
                    out.reason = "point created";
                    CXLOG_INFO("ImageAnnotationUI", "annotation_shape_created", "created", "ref=" + out.created_ref);
                    m_lastPointerResult = out;
                    return out;
                }

                out.consumed = true;
                out.phase = "create_shape";
                out.status = "failed";
                out.reason = "CreateInitialShapeForTool failed";
                m_lastPointerResult = out;
                return out;
            }
        }
    }
    else if (currentKind == OverlayKind::Polyline)
    {
        if (frame.left_clicked)
        {
            m_activePolylinePoints.push_back({frame.image_x, frame.image_y});
            out.consumed = true;
            out.phase = "create_shape";
            out.status = "point_added";
            out.reason = "polyline point added";
            m_lastPointerResult = out;
            return out;
        }

        if (frame.right_clicked || frame.enter_pressed)
        {
            const int required_points = frame.right_clicked ? 3 : 2;
            if (static_cast<int>(m_activePolylinePoints.size()) < required_points)
            {
                out.consumed = true;
                out.phase = "create_shape";
                out.status = "polyline_too_small";
                out.reason = frame.right_clicked
                    ? "closed polyline requires at least three points"
                    : "polyline requires at least two points";
                m_lastPointerResult = out;
                return out;
            }

            const AnnotationToolDefinition* tool = activeTool;
            if (tool)
            {
                auto shape = CreateInitialShapeForTool(*tool, m_activePolylinePoints);
                if (shape)
                {
                    if (shape->kind() == CxShapeKind::Polyline)
                    {
                        PolylineShape* polyline =
                            dynamic_cast<PolylineShape*>(shape.get());
                        if (polyline != nullptr)
                            polyline->close(frame.right_clicked);
                    }
                    CxShapeElement& element = m_annotationLayer.CreateFromTool(*tool, std::move(shape));
                out.consumed = true;
                out.phase = "create_shape";
                out.status = "created";
                out.created_ref = element.stable_ref;
                out.reason = frame.right_clicked
                    ? "polyline closed by right click"
                    : "polyline created";
                CXLOG_INFO("ImageAnnotationUI", "annotation_shape_created", "created", "ref=" + out.created_ref);
                m_activePolylinePoints.clear();
                m_lastPointerResult = out;
                return out;
                }
            }
            out.consumed = true;
            out.phase = "create_shape";
            out.status = "failed";
            out.reason = "active tool definition unavailable";
            m_activePolylinePoints.clear();
            m_lastPointerResult = out;
            return out;
        }
    }
    else if (currentKind == OverlayKind::Line)
    {
        if (!m_annotationDragging)
        {
            if (frame.left_clicked)
            {
                m_annotationDragging = true;
                m_annotationDragKind = OverlayKind::Line;
                m_annotationDragStart = {(float)frame.image_x, (float)frame.image_y};
                m_annotationDragEnd = {(float)frame.image_x, (float)frame.image_y};
                out.consumed = true;
                out.phase = "create_shape";
                out.status = "draft_started";
                out.reason = "line draft started";
                m_lastPointerResult = out;
                return out;
            }
            m_lastPointerResult = out;
            return out;
        }

        if (frame.left_down)
        {
            m_annotationDragEnd = {(float)frame.image_x, (float)frame.image_y};
            out.consumed = true;
            out.phase = "create_shape";
            out.status = "draft_updating";
            out.reason = "line draft updating";
            m_lastPointerResult = out;
            return out;
        }

        if (frame.left_released)
        {
            const AnnotationToolDefinition* tool = activeTool;
            CXLOG_INFO("ImageAnnotationUI", "annotation_line_release", "debug", 
                       "currentToolName=" + activeTool->name + 
                       ", tool=active" +
                       ", m_imageToolMode=" + std::to_string(static_cast<int>(m_imageToolMode)));
            if (tool)
            {
                CommitDraftShapeFromTool(*tool, out);
            }
            else
            {
                out.status = "failed";
                out.reason = "active tool definition unavailable";
            }
            m_annotationDragging = false;
            m_lastPointerResult = out;
            return out;
        }
    }
    else if (currentKind == OverlayKind::Rect)
    {
        if (!m_annotationDragging)
        {
            if (frame.left_clicked)
            {
                m_annotationDragging = true;
                m_annotationDragKind = OverlayKind::Rect;
                m_annotationDragStart = {(float)frame.image_x, (float)frame.image_y};
                m_annotationDragEnd = {(float)frame.image_x, (float)frame.image_y};
                out.consumed = true;
                out.phase = "create_shape";
                out.status = "draft_started";
                out.reason = "rect draft started";
                m_lastPointerResult = out;
                return out;
            }
            m_lastPointerResult = out;
            return out;
        }

        if (frame.left_down)
        {
            m_annotationDragEnd = {(float)frame.image_x, (float)frame.image_y};
            out.consumed = true;
            out.phase = "create_shape";
            out.status = "draft_updating";
            out.reason = "rect draft updating";
            m_lastPointerResult = out;
            return out;
        }

        if (frame.left_released)
        {
            const AnnotationToolDefinition* tool = activeTool;
            if (tool)
            {
                CommitDraftShapeFromTool(*tool, out);
            }
            else
            {
                out.status = "failed";
                out.reason = "active tool definition unavailable";
            }
            m_annotationDragging = false;
            m_lastPointerResult = out;
            return out;
        }
    }
    else if (currentKind == OverlayKind::AutoBoundaryRequest)
    {
        if (!m_annotationDragging)
        {
            if (frame.left_clicked)
            {
                m_annotationDragging = true;
                m_annotationDragKind = OverlayKind::AutoBoundaryRequest;
                m_annotationDragStart = {(float)frame.image_x, (float)frame.image_y};
                m_annotationDragEnd = {(float)frame.image_x, (float)frame.image_y};
                out.consumed = true;
                out.phase = "create_shape";
                out.status = "draft_started";
                out.reason = "auto boundary prompt draft started";
                m_lastPointerResult = out;
                return out;
            }
            m_lastPointerResult = out;
            return out;
        }

        if (frame.left_down)
        {
            m_annotationDragEnd = {(float)frame.image_x, (float)frame.image_y};
            out.consumed = true;
            out.phase = "create_shape";
            out.status = "draft_updating";
            out.reason = "auto boundary prompt draft updating";
            m_lastPointerResult = out;
            return out;
        }

        if (frame.left_released)
        {
            const AnnotationToolDefinition* tool = activeTool;
            if (tool)
            {
                CommitDraftShapeFromTool(*tool, out);
            }
            else
            {
                out.status = "failed";
                out.reason = "active tool definition unavailable";
            }
            m_annotationDragging = false;
            m_lastPointerResult = out;
            return out;
        }
    }
    else if (currentKind == OverlayKind::Circle)
    {
        if (!m_annotationDragging)
        {
            if (frame.left_clicked)
            {
                m_annotationDragging = true;
                m_annotationDragKind = OverlayKind::Circle;
                m_annotationDragStart = {(float)frame.image_x, (float)frame.image_y};
                m_annotationDragEnd = {(float)frame.image_x, (float)frame.image_y};
                out.consumed = true;
                out.phase = "create_shape";
                out.status = "draft_started";
                out.reason = "circle draft started";
                m_lastPointerResult = out;
                return out;
            }
            m_lastPointerResult = out;
            return out;
        }

        if (frame.left_down)
        {
            m_annotationDragEnd = {(float)frame.image_x, (float)frame.image_y};
            out.consumed = true;
            out.phase = "create_shape";
            out.status = "draft_updating";
            out.reason = "circle draft updating";
            m_lastPointerResult = out;
            return out;
        }

        if (frame.left_released)
        {
            const AnnotationToolDefinition* tool = activeTool;
            if (tool)
            {
                CommitDraftShapeFromTool(*tool, out);
            }
            else
            {
                out.status = "failed";
                out.reason = "active tool definition unavailable";
            }
            m_annotationDragging = false;
            m_lastPointerResult = out;
            return out;
        }
    }
    else if (currentKind == OverlayKind::Ellipse)
    {
        if (!m_annotationDragging)
        {
            if (frame.left_clicked)
            {
                m_annotationDragging = true;
                m_annotationDragKind = OverlayKind::Ellipse;
                m_annotationDragStart = {(float)frame.image_x, (float)frame.image_y};
                m_annotationDragEnd = {(float)frame.image_x, (float)frame.image_y};
                out.consumed = true;
                out.phase = "create_shape";
                out.status = "draft_started";
                out.reason = "ellipse draft started";
                m_lastPointerResult = out;
                return out;
            }
            m_lastPointerResult = out;
            return out;
        }

        if (frame.left_down)
        {
            m_annotationDragEnd = {(float)frame.image_x, (float)frame.image_y};
            out.consumed = true;
            out.phase = "create_shape";
            out.status = "draft_updating";
            out.reason = "ellipse draft updating";
            m_lastPointerResult = out;
            return out;
        }

        if (frame.left_released)
        {
            const AnnotationToolDefinition* tool = activeTool;
            if (tool)
            {
                CommitDraftShapeFromTool(*tool, out);
            }
            else
            {
                out.status = "failed";
                out.reason = "active tool definition unavailable";
            }
            m_annotationDragging = false;
            m_lastPointerResult = out;
            return out;
        }
    }

    m_lastPointerResult = out;
    return out;
}

void ViewController::drawImageEvidenceOnCanvas(bool canvasHovered,
                                                bool canvasActive,
                                                ImDrawList* drawList)
{
  ImGuiIO& io = ImGui::GetIO();

  ImVec2 mouseScreen = io.MousePos;
  bool mouseInImageCanvas = false;
  ImVec2 imagePoint(-1.0f, -1.0f);
  bool insideImage = false;

  if (!m_imageViewImage.empty())
  {
    imagePoint = ScreenToImage(mouseScreen.x, mouseScreen.y);
    insideImage = imagePoint.x >= 0.0f && imagePoint.y >= 0.0f &&
      imagePoint.x < m_imageViewImage.cols && imagePoint.y < m_imageViewImage.rows;

    mouseInImageCanvas =
      mouseScreen.x >= m_annotationImagePosX &&
      mouseScreen.x <= m_annotationImagePosX + m_annotationImageWidth &&
      mouseScreen.y >= m_annotationImagePosY &&
      mouseScreen.y <= m_annotationImagePosY + m_annotationImageHeight;

    m_imageCanvasMin = ImVec2(m_annotationImagePosX, m_annotationImagePosY);
    m_imageCanvasMax = ImVec2(
        m_annotationImagePosX + m_annotationImageWidth,
        m_annotationImagePosY + m_annotationImageHeight);
  }

  m_debugImageViewHovered = canvasHovered;
  m_debugMouseInImage = insideImage;
  m_debugMouseImageX = imagePoint.x;
  m_debugMouseImageY = imagePoint.y;
  m_debugAnnotationDragging = m_annotationDragging;

  CxImagePointerFrame pointerFrame;
  pointerFrame.canvas_hovered = canvasHovered;
  pointerFrame.inside_image = insideImage;
  pointerFrame.left_clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
  pointerFrame.left_down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
  pointerFrame.left_released = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
  pointerFrame.right_clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Right);
  pointerFrame.escape_pressed = ImGui::IsKeyPressed(ImGuiKey_Escape);
  pointerFrame.enter_pressed = ImGui::IsKeyPressed(ImGuiKey_Enter);
  pointerFrame.pointer_moved =
    io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f;
  pointerFrame.image_x = imagePoint.x;
  pointerFrame.image_y = imagePoint.y;

  CxImagePointerResult pointerResult = ProcessImageAnnotationPointerFrame(pointerFrame);

  if (pointerResult.consumed)
  {
    m_blockOccMouseInputThisFrame = true;
    m_blockImagePanThisFrame = true;
    m_annotationStatus = pointerResult.status + ": " + pointerResult.reason;
  }

  if (!canvasActive && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    m_annotationDragging = false;

  if (m_annotationDragging && insideImage)
  {
    const ImVec2 start = ImageToScreen(m_annotationDragStart.x,
                                       m_annotationDragStart.y);
    const ImVec2 end = ImageToScreen(m_annotationDragEnd.x,
                                     m_annotationDragEnd.y);
    const ImU32 previewColor = IM_COL32(80, 220, 255, 220);
    if (m_annotationDragKind == OverlayKind::Line)
      drawList->AddLine(start, end, previewColor, 2.0f);
    else if (m_annotationDragKind == OverlayKind::Rect ||
             m_annotationDragKind == OverlayKind::AutoBoundaryRequest)
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
    else if (m_annotationDragKind == OverlayKind::Ellipse)
    {
      const float cx = (start.x + end.x) * 0.5f;
      const float cy = (start.y + end.y) * 0.5f;
      const float rx = std::abs(end.x - start.x) * 0.5f;
      const float ry = std::abs(end.y - start.y) * 0.5f;
      drawList->AddEllipse(ImVec2(cx, cy), ImVec2(rx, ry), previewColor, 0.0f, 48, 2.0f);
    }
  }

  if (!m_annotationDragging && IsAnnotationCreateModeActive() &&
      !m_activePolylinePoints.empty())
  {
    const ImU32 previewColor = IM_COL32(80, 220, 255, 220);
    for (std::size_t i = 1; i < m_activePolylinePoints.size(); ++i)
    {
      const ImVec2 first = ImageToScreen(
          static_cast<float>(m_activePolylinePoints[i - 1].x),
          static_cast<float>(m_activePolylinePoints[i - 1].y));
      const ImVec2 second = ImageToScreen(
          static_cast<float>(m_activePolylinePoints[i].x),
          static_cast<float>(m_activePolylinePoints[i].y));
      drawList->AddLine(first, second, previewColor, 2.0f);
    }

    for (const CxShapePoint& p : m_activePolylinePoints)
    {
      const ImVec2 screen = ImageToScreen(
          static_cast<float>(p.x),
          static_cast<float>(p.y));
      drawList->AddCircleFilled(screen, 4.0f, previewColor);
      drawList->AddCircle(screen, 6.0f, IM_COL32(255, 255, 255, 220), 16, 1.0f);
    }

    if (insideImage)
    {
      const CxShapePoint& last = m_activePolylinePoints.back();
      const ImVec2 first = ImageToScreen(
          static_cast<float>(last.x),
          static_cast<float>(last.y));
      const ImVec2 second = ImageToScreen(imagePoint.x, imagePoint.y);
      drawList->AddLine(first, second, IM_COL32(80, 220, 255, 120), 1.0f);
    }
  }

  for (int elementIndex = 0;
       elementIndex < static_cast<int>(m_annotationLayer.Elements().size());
       ++elementIndex)
  {
    OverlayElement& element = m_annotationLayer.Elements()[elementIndex];
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
    else if (element.kind == OverlayKind::Circle && element.image_points.size() >= 2)
    {
      const ImVec2 center = ImageToScreen(element.image_points[0].x,
                                          element.image_points[0].y);
      const ImVec2 radiusPoint = ImageToScreen(element.image_points[1].x,
                                               element.image_points[1].y);
      const float dx = radiusPoint.x - center.x;
      const float dy = radiusPoint.y - center.y;
      const float radiusScreen = std::sqrt(dx * dx + dy * dy);
      drawList->AddCircle(center, radiusScreen, color, 96, thickness);
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
    else if (element.kind == OverlayKind::AutoBoundaryRequest && !element.image_points.empty())
    {
      const ImVec2 p = ImageToScreen(element.image_points[0].x,
                                     element.image_points[0].y);
      drawList->AddCircleFilled(p, 6.0f, IM_COL32(255, 160, 40, 255));
      drawList->AddCircle(p, 9.0f, IM_COL32(255, 255, 255, 255), 24, 2.0f);
      drawList->AddText(ImVec2(p.x + 10.0f, p.y - 8.0f),
                       IM_COL32(255, 220, 80, 255), "EdgeSam");
    }
    else if (element.kind == OverlayKind::BoundaryPolyline)
    {
      for (std::size_t i = 1; i < element.image_points.size(); ++i)
      {
        const ImVec2 first = ImageToScreen(element.image_points[i - 1].x,
                                           element.image_points[i - 1].y);
        const ImVec2 second = ImageToScreen(element.image_points[i].x,
                                            element.image_points[i].y);
        drawList->AddLine(first, second, IM_COL32(80, 255, 200, 255), thickness);
      }
    }
    const ImVec2 sourceLabel = ImageToScreen(element.image_points[0].x,
                                             element.image_points[0].y);
    drawList->AddText(ImVec2(sourceLabel.x + 6.0f, sourceLabel.y + 6.0f),
                      color, element.source.empty() ? "manual_element" :
                                                    element.source.c_str());

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
  ImGuiIO& io = ImGui::GetIO();
  if (m_detachablePanels)
  {
    ImGui::SetNextWindowPos(ImVec2(350, 40), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_Once);
  }
  else
  {
    const float width = std::max(420.0f, io.DisplaySize.x * 0.24f);
    const float height = std::max(520.0f, io.DisplaySize.y - 80.0f);
    const float x = std::max(20.0f, io.DisplaySize.x - width - 20.0f);
    ImGui::SetNextWindowPos(ImVec2(x, 40.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
  }
  if (!ImGui::Begin("Image Evidence / Annotation Tools"))
  {
    ImGui::End();
    return;
  }

  ImGui::Text("Annotation Tool Manifest");
  AnnotationInputText("Manifest path", m_annotationManifestPath);
  if (ImGui::Button("Load Tool Manifest"))
    {
        std::string reason;
        CxAnnotationToolManifestSnapshot snapshot;
        if (!m_parserOwner.ParseAnnotationToolManifest(m_annotationManifestPath, snapshot, reason))
        {
            m_annotationStatus = "parse failed: " + reason;
        }
        else if (!m_annotationLayer.ApplyToolManifestSnapshot(snapshot, reason))
        {
            m_annotationStatus = "apply failed: " + reason;
        }
        else
        {
            m_annotationStatus = reason;
        }
    }
  ImGui::SameLine();
  ImGui::TextWrapped("%s", m_annotationStatus.c_str());
  ImGui::TextDisabled(
    "CxScript direct logic: CxAnnotationTool_* declarations -> CxAnnotationToolRuntime");
  ImGui::TextDisabled(
    "cximage=pending_binding  torch=pending_binding  "
    "mlpack=pending_binding  ensmallen=pending_binding");

  ImGui::Separator();
  ImGui::Columns(2, "annotation_columns", true);
  ImGui::Text("Tool Palette");

  auto drawPointerPanButton = [this]()
  {
    ImGui::PushID("Pointer / Pan");

    const bool active = !m_imageToolEnabled ||
                        m_imageToolMode == ImageToolMode::PointerPan ||
                        m_annotationLayer.ActiveToolIndex() < 0;

    if (active)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.55f, 0.85f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.65f, 0.95f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.45f, 0.75f, 1.0f));
    }

    if (ImGui::Button("Pointer / Pan", ImVec2(-1.0f, 28.0f)))
    {
        m_imageToolEnabled = false;
        m_imageToolMode = ImageToolMode::PointerPan;
        CancelAnnotationCreate();
        m_annotationLayer.SetActiveToolIndex(-1);
        m_annotationStatus = "annotation tool disabled";
    }

    if (active)
        ImGui::PopStyleColor(3);

    ImGui::PopID();
  };

  auto drawManifestToolButton = [this](int toolIndex, const AnnotationToolDefinition& tool)
  {
    ImGui::PushID(tool.name.c_str());

    const bool active = m_imageToolEnabled &&
                        m_annotationLayer.ActiveToolIndex() == toolIndex;

    if (active)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.55f, 0.85f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.65f, 0.95f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.45f, 0.75f, 1.0f));
    }

    const std::string label = tool.label.empty() ? tool.name : tool.label;
    if (ImGui::Button(label.c_str(), ImVec2(-1.0f, 28.0f)))
    {
        if (active)
        {
            m_imageToolEnabled = false;
            m_imageToolMode = ImageToolMode::PointerPan;
            CancelAnnotationCreate();
            m_annotationLayer.SetActiveToolIndex(-1);
            m_annotationStatus = "annotation tool disabled";
        }
        else
        {
            m_imageToolEnabled = true;
            m_imageToolMode = ToolModeFromAnnotationTool(tool);
            CancelAnnotationCreate();
            m_annotationLayer.SetActiveToolIndex(toolIndex);
            m_annotationStatus =
                "enabled tool_id=" + tool.name +
                " shape=" + tool.shape_type +
                " role=" + tool.role +
                " action=" + tool.action;
        }
    }

    if (active)
        ImGui::PopStyleColor(3);

    ImGui::PopID();
  };

  ImGui::TextColored(ImVec4(1, 0.7f, 0.2f, 1),
                      "Annotation Tool Palette V2 ACTIVE");
  ImGui::TextUnformatted("Annotation Tools (from cxscript manifest)");
  drawPointerPanButton();
  for (int i = 0; i < static_cast<int>(m_annotationLayer.Tools().size()); ++i)
  {
    const AnnotationToolDefinition& tool = m_annotationLayer.Tools()[i];
    if (!tool.manual_visible) continue;
    drawManifestToolButton(i, tool);
  }

  ImGui::Separator();

  if (HasEditableFindCircleGauge())
  {
    ImGui::TextUnformatted("FindCircle Gauge");
    ImGui::TextDisabled("Move Circle Center");
    ImGui::TextDisabled("Radius R");
    ImGui::TextDisabled("Inner Rin");
    ImGui::TextDisabled("Outer Rout");
  }
  else
  {
    ImGui::TextDisabled("FindCircle Gauge: not available");
  }

  ImGui::Separator();
  ImGui::Text("Tool enabled: %s", m_imageToolEnabled ? "YES" : "NO");
  ImGui::Text("Active mode: %s", ImageToolModeName(m_imageToolMode));
  const AnnotationToolDefinition* activeTool = m_annotationLayer.ActiveTool();
  if (activeTool != nullptr)
  {
    ImGui::TextWrapped(
        "Active tool id: %s | shape=%s | role=%s | action=%s | index=%d",
        activeTool->name.c_str(),
        activeTool->shape_type.c_str(),
        activeTool->role.c_str(),
        activeTool->action.c_str(),
        m_annotationLayer.ActiveToolIndex());
  }
  else
  {
    ImGui::TextDisabled("Active tool id: (none) | index=%d",
                        m_annotationLayer.ActiveToolIndex());
  }
  ImGui::Text("ShapeElements: %d",
              static_cast<int>(m_annotationLayer.ShapeElements().size()));
  ImGui::TextWrapped("Last pointer: %s | %s | %s",
                     m_lastPointerResult.phase.c_str(),
                     m_lastPointerResult.status.c_str(),
                     m_lastPointerResult.reason.c_str());
  ImGui::Separator();
  ImGui::TextDisabled("Manifest tool definitions");
  for (int i = 0; i < static_cast<int>(m_annotationLayer.Tools().size()); ++i)
  {
    const AnnotationToolDefinition& tool = m_annotationLayer.Tools()[i];
    if (!tool.manual_visible) continue;
    ImGui::PushID(i);
    const bool active = m_annotationLayer.ActiveToolIndex() == i;
    ImGui::Text("%s%s", active ? "* " : "  ", tool.name.c_str());
    ImGui::TextWrapped("%s | %s | %s", ImageAnnotationLayer::KindName(tool.kind),
                       tool.role.c_str(), tool.action.c_str());
    ImGui::TextWrapped("source=%s modules=%s", tool.source.c_str(),
                       tool.module_hint.c_str());
    ImGui::PopID();
  }
  if (ImGui::Button("Cancel Polyline Draft")) { m_activePolylineElement = -1; m_activePolylinePoints.clear(); }
  if (ImGui::IsKeyPressed(ImGuiKey_Escape))
  {
    m_imageToolEnabled = false;
    m_imageToolMode = ImageToolMode::PointerPan;
    CancelAnnotationCreate();
    m_annotationLayer.SetActiveToolIndex(-1);
    m_annotationStatus = "annotation tool disabled (ESC)";
  }

  ImGui::NextColumn();
  ImGui::Text("Element List (ShapeElements)");
  for (int i = 0; i < static_cast<int>(m_annotationLayer.ShapeElements().size()); ++i)
  {
    CxShapeElement& element = m_annotationLayer.ShapeElements()[i];
    ImGui::PushID(2000 + i);
    if (ImGui::Selectable(element.ref.c_str(), element.selected))
    {
      m_annotationLayer.SelectShape(i);
      m_scriptResult.overlay_ref = "shape:" + element.ref;
    }
    ImGui::SameLine();
    ImGui::Checkbox("visible", &element.visible);
    char detailBuf[512] = "";
    snprintf(detailBuf, sizeof(detailBuf),
             "id=%d | tool=%s | role=%s | owner=%s:%s | editable=%d | result=%d | stale=%d",
             element.id,
             element.tool_id.c_str(),
             element.semantic_role.c_str(),
             element.owner_type.c_str(),
             element.owner_ref.c_str(),
             element.editable ? 1 : 0,
             element.result_element ? 1 : 0,
             element.stale ? 1 : 0);
    ImGui::TextWrapped("%s", detailBuf);
    ImGui::PopID();
  }

  ImGui::Separator();
  static bool showLegacyOverlayElements = false;
  if (ImGui::CollapsingHeader("Legacy Overlay Elements Debug", &showLegacyOverlayElements))
  {
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
      const char* kindName = ImageAnnotationLayer::KindName(element.kind);
      ImGui::TextWrapped("%s id=%d | status=%s | role=%s",
                         kindName, element.id,
                         element.status.c_str(), element.role.c_str());
      ImGui::PopID();
    }
  }

  ImGui::Columns(1);

  ImGui::Separator();
  ImGui::Text("Element Inspector");
  CxShapeElement* selectedShape = m_annotationLayer.SelectedShape();
  if (selectedShape != nullptr)
  {
    ImGui::Text("ref: %s", selectedShape->ref.c_str());
    ImGui::Text("tool_id: %s", selectedShape->tool_id.c_str());
    ImGui::Text("semantic_role: %s", selectedShape->semantic_role.c_str());
    ImGui::Text("owner_type: %s", selectedShape->owner_type.c_str());
    ImGui::Text("owner_ref: %s", selectedShape->owner_ref.c_str());
    ImGui::Text("editable: %s", selectedShape->editable ? "true" : "false");
    ImGui::Text("result_element: %s", selectedShape->result_element ? "true" : "false");
    ImGui::Text("stale: %s", selectedShape->stale ? "true" : "false");
    if (ImGui::Button("Bind Selected Shape To Globals"))
    {
      std::string reason;
      if (ExportShapeElementToRuntimeGlobals(m_manualTest, *selectedShape, reason))
      {
        m_manualTest.debug_status = "shape_bound_to_globals";
        m_manualTest.debug_reason = reason;
        m_annotationStatus = reason;
      }
      else
      {
        m_manualTest.debug_status = "shape_bind_failed";
        m_manualTest.debug_reason = reason;
        m_annotationStatus = reason;
      }
    }
  }
  else
  {
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
            m_parserDebugBridge.ApplyStatement("Findcircle afindcircle0;");
          const bool applied = m_parserDebugBridge.ApplyStatement(
            selected->generated_statement);
          m_scriptResult.status = applied ? "PENDING" : "BLOCKED";
          m_scriptResult.reason = applied ?
            "manual_element applied to parser; runtime objects refreshed" :
            "parser rejected manual circle statement";
          RefreshRuntimeObjectTable("setcircle",
            applied ? "runtime_executed" : "BLOCKED");
          m_annotationStatus = m_scriptResult.reason;
        }
      }
    }
  }

  ImGui::Separator();
  AnnotationInputText("Session path", m_annotationSessionPath);
  if (ImGui::Button("Save Elements"))
  {
      m_annotationStatus =
          "saving elements count=" + std::to_string(m_annotationLayer.Elements().size());
      m_annotationLayer.SaveElements(m_annotationSessionPath,
                                     m_scriptResult.image_ref,
                                     m_annotationStatus);
  }
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

bool ViewController::TestLoadAnnotationToolManifest(const std::string& path, std::string& reason)
{
    m_annotationManifestPath = path;

    std::string init_reason;
    if (!m_parserOwner.Initialize(init_reason))
    {
        reason = "parser initialize failed: " + init_reason;
        return false;
    }

    CxAnnotationToolManifestSnapshot snapshot;
    if (!m_parserOwner.ParseAnnotationToolManifest(path, snapshot, reason))
    {
        return false;
    }

    return m_annotationLayer.ApplyToolManifestSnapshot(snapshot, reason);
}

bool ViewController::TestApplyAnnotationToolManifestSnapshot(const CxAnnotationToolManifestSnapshot& snapshot, std::string& reason)
{
    return m_annotationLayer.ApplyToolManifestSnapshot(snapshot, reason);
}

bool ViewController::TestSetActiveAnnotationTool(const std::string& tool_id, std::string& reason)
{
    for (int i = 0; i < static_cast<int>(m_annotationLayer.Tools().size()); ++i)
    {
        if (m_annotationLayer.Tools()[i].name == tool_id)
        {
            m_annotationLayer.SetActiveToolIndex(i);
            m_imageToolEnabled = true;
            m_imageToolMode = ToolModeFromAnnotationTool(m_annotationLayer.Tools()[i]);
            CancelAnnotationCreate();
            return true;
        }
    }
    reason = "tool not found: " + tool_id;
    return false;
}

void ViewController::TestEnableAnnotationCreateMode()
{
    m_imageToolEnabled = true;
    m_imageToolMode = ImageToolMode::LineCreate;
}

void ViewController::TestSetActiveToolKind(OverlayKind kind)
{
    switch (kind)
    {
    case OverlayKind::Point: m_imageToolMode = ImageToolMode::PointCreate; break;
    case OverlayKind::Line: m_imageToolMode = ImageToolMode::LineCreate; break;
    case OverlayKind::Rect: m_imageToolMode = ImageToolMode::RectCreate; break;
    case OverlayKind::Circle: m_imageToolMode = ImageToolMode::CircleCreate; break;
    case OverlayKind::Ellipse: m_imageToolMode = ImageToolMode::EllipseCreate; break;
    case OverlayKind::Polyline: m_imageToolMode = ImageToolMode::PolylineCreate; break;
    default: m_imageToolMode = ImageToolMode::LineCreate; break;
    }
}

void ViewController::TestSetToolModePointerPan()
{
    m_imageToolEnabled = false;
    m_imageToolMode = ImageToolMode::PointerPan;
    m_annotationLayer.SetActiveToolIndex(-1);
    CancelAnnotationCreate();
}

std::size_t ViewController::TestShapeElementCount() const
{
    return m_annotationLayer.ShapeElements().size();
}

bool ViewController::TestGetLastPointerResult(CxImagePointerResult& out) const
{
    out = m_lastPointerResult;
    return true;
}

std::string ViewController::TestShapeKindByRef(const std::string& ref) const
{
    const auto& elements = m_annotationLayer.ShapeElements();
    for (const auto& elem : elements)
    {
        if (elem.stable_ref == ref && elem.shape)
        {
            switch (elem.shape->kind())
            {
            case CxShapeKind::Points: return "PointsShape";
            case CxShapeKind::Line: return "LineShape";
            case CxShapeKind::Rect: return "RectShape";
            case CxShapeKind::Circle: return "CircleShape";
            case CxShapeKind::Polyline: return "PolylineShape";
            case CxShapeKind::Ellipse: return "EllipseShape";
            case CxShapeKind::LineGauge: return "LineGaugeShape";
            default: return "Unknown";
            }
        }
    }
    return "";
}
