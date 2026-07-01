#include "ImageAnnotationLayer.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace
{
namespace fs = std::filesystem;

std::string Trim(const std::string& text)
{
  const std::size_t first = text.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return std::string();
  const std::size_t last = text.find_last_not_of(" \t\r\n");
  return text.substr(first, last - first + 1);
}

bool SplitKeyValue(const std::string& line, std::string& key, std::string& value)
{
  const std::size_t separator = line.find(':');
  if (separator == std::string::npos) return false;
  key = Trim(line.substr(0, separator));
  value = Trim(line.substr(separator + 1));
  return !key.empty();
}

std::string UnquoteStatementValue(std::string value)
{
  value = Trim(value);
  if (!value.empty() && value.back() == ';') value.pop_back();
  value = Trim(value);
  if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
    value = value.substr(1, value.size() - 2);
  return value;
}
}

const char* ImageAnnotationLayer::KindName(OverlayKind kind)
{
  switch (kind)
  {
    case OverlayKind::Point: return "Point";
    case OverlayKind::Line: return "Line";
    case OverlayKind::Rect: return "Rect";
    case OverlayKind::Circle: return "Circle";
    case OverlayKind::Polyline: return "Polyline";
  }
  return "Point";
}

bool ImageAnnotationLayer::ParseKind(const std::string& text, OverlayKind& kind)
{
  if (text == "Point") kind = OverlayKind::Point;
  else if (text == "Line") kind = OverlayKind::Line;
  else if (text == "Rect") kind = OverlayKind::Rect;
  else if (text == "Circle") kind = OverlayKind::Circle;
  else if (text == "Polyline") kind = OverlayKind::Polyline;
  else return false;
  return true;
}

bool ImageAnnotationLayer::LoadManifest(const std::string& path,
                                        std::string& reason)
{
  std::ifstream stream{fs::path(path)};
  if (!stream)
  {
    reason = "annotation tool CxScript not found";
    return false;
  }
  myTools.clear();
  AnnotationToolDefinition current;
  std::string currentObject;
  std::string line;
  while (std::getline(stream, line))
  {
    line = Trim(line);
    if (line.empty() || line[0] == '#' || line.rfind("//", 0) == 0) continue;

    const std::string declarationPrefix = "AnnotationTool ";
    if (line.rfind(declarationPrefix, 0) == 0)
    {
      current = AnnotationToolDefinition();
      currentObject = Trim(line.substr(declarationPrefix.size()));
      if (!currentObject.empty() && currentObject.back() == ';')
        currentObject.pop_back();
      continue;
    }

    const std::string registerPrefix = "register_annotation_tool(";
    if (line.rfind(registerPrefix, 0) == 0)
    {
      const std::size_t close = line.find(')', registerPrefix.size());
      const std::string object = close == std::string::npos ? std::string() :
        Trim(line.substr(registerPrefix.size(), close - registerPrefix.size()));
      if (!currentObject.empty() && object == currentObject && !current.name.empty())
        myTools.push_back(current);
      current = AnnotationToolDefinition();
      currentObject.clear();
      continue;
    }

    if (currentObject.empty()) continue;
    const std::string assignmentPrefix = currentObject + ".";
    if (line.rfind(assignmentPrefix, 0) != 0) continue;
    const std::size_t equals = line.find('=', assignmentPrefix.size());
    if (equals == std::string::npos) continue;
    const std::string field = Trim(line.substr(
      assignmentPrefix.size(), equals - assignmentPrefix.size()));
    std::string value = UnquoteStatementValue(line.substr(equals + 1));
    if (field == "name") current.name = value;
    else if (field == "kind")
    {
      const std::string kindPrefix = "OverlayKind::";
      if (value.rfind(kindPrefix, 0) == 0) value = value.substr(kindPrefix.size());
      ParseKind(value, current.kind);
    }
    else if (field == "role") current.role = value;
    else if (field == "source") current.source = value;
    else if (field == "action") current.action = value;
    else if (field == "module_hint") current.module_hint = value;
    else if (field == "description") current.description = value;
  }
  reason = myTools.empty() ? "CxScript contains no registered annotation tools" :
                             "CxScript annotation tools loaded";
  return !myTools.empty();
}

std::string ImageAnnotationLayer::MakeRef(OverlayKind kind, int id) const
{
  const char* prefix = "point";
  if (kind == OverlayKind::Line) prefix = "line";
  else if (kind == OverlayKind::Rect) prefix = "roi_rect";
  else if (kind == OverlayKind::Circle) prefix = "circle";
  else if (kind == OverlayKind::Polyline) prefix = "polyline";
  std::ostringstream stream;
  stream << prefix << '_' << std::setw(3) << std::setfill('0') << id;
  return stream.str();
}

OverlayElement& ImageAnnotationLayer::Create(OverlayKind kind,
                                              const std::string& role,
                                              const std::string& source,
                                              const std::string& moduleHint)
{
  OverlayElement element;
  element.id = myNextId++;
  element.kind = kind;
  element.ref = MakeRef(kind, element.id);
  element.role = role;
  element.source = source;
  element.module_hint = moduleHint;
  element.evidence_ref = "evidence:" + element.ref;
  myElements.push_back(element);
  Select(static_cast<int>(myElements.size()) - 1);
  return myElements.back();
}

void ImageAnnotationLayer::Clear()
{
  myElements.clear();
  mySelectedIndex = -1;
  myNextId = 1;
}

void ImageAnnotationLayer::Select(int index)
{
  mySelectedIndex = index >= 0 && index < static_cast<int>(myElements.size()) ? index : -1;
  for (int i = 0; i < static_cast<int>(myElements.size()); ++i)
    myElements[i].selected = i == mySelectedIndex;
}

OverlayElement* ImageAnnotationLayer::Selected()
{
  return mySelectedIndex >= 0 && mySelectedIndex < static_cast<int>(myElements.size()) ?
    &myElements[mySelectedIndex] : nullptr;
}

const OverlayElement* ImageAnnotationLayer::Selected() const
{
  return mySelectedIndex >= 0 && mySelectedIndex < static_cast<int>(myElements.size()) ?
    &myElements[mySelectedIndex] : nullptr;
}

const AnnotationToolDefinition* ImageAnnotationLayer::ActiveTool() const
{
  return myActiveToolIndex >= 0 && myActiveToolIndex < static_cast<int>(myTools.size()) ?
    &myTools[myActiveToolIndex] : nullptr;
}

std::string ImageAnnotationLayer::SelectedRef(OverlayKind kind) const
{
  const OverlayElement* selected = Selected();
  if (selected != nullptr && selected->kind == kind) return selected->ref;
  for (auto it = myElements.rbegin(); it != myElements.rend(); ++it)
    if (it->kind == kind) return it->ref;
  return std::string();
}

bool ImageAnnotationLayer::SaveElements(const std::string& path,
                                         const std::string& imageRef,
                                         std::string& reason) const
{
  const fs::path output(path);
  if (output.has_parent_path()) fs::create_directories(output.parent_path());
  std::ofstream stream(output);
  if (!stream)
  {
    reason = "unable to open annotation output";
    return false;
  }
  stream << "image_ref: " << imageRef << '\n';
  for (const OverlayElement& element : myElements)
  {
    stream << "element: " << element.ref << '\n';
    stream << "kind: " << KindName(element.kind) << '\n';
    stream << "role: " << element.role << '\n';
    stream << "source: " << element.source << '\n';
    stream << "module_hint: " << element.module_hint << '\n';
    stream << "radius: " << element.radius << '\n';
    stream << "label: " << element.label << '\n';
    stream << "points:";
    for (const OverlayImagePoint& point : element.image_points)
      stream << ' ' << point.x << ',' << point.y;
    stream << "\nend\n";
  }
  reason = "annotation elements saved";
  return true;
}

bool ImageAnnotationLayer::LoadElements(const std::string& path,
                                         std::string& imageRef,
                                         std::string& reason)
{
  std::ifstream stream{fs::path(path)};
  if (!stream)
  {
    reason = "annotation file not found";
    return false;
  }
  Clear();
  OverlayElement* current = nullptr;
  std::string line;
  while (std::getline(stream, line))
  {
    std::string key;
    std::string value;
    if (!SplitKeyValue(line, key, value)) continue;
    if (key == "image_ref") imageRef = value;
    else if (key == "element")
    {
      OverlayElement& created = Create(OverlayKind::Point, "", "manual", "cximage");
      created.ref = value;
      created.evidence_ref = "evidence:" + value;
      current = &created;
    }
    else if (current == nullptr) continue;
    else if (key == "kind") ParseKind(value, current->kind);
    else if (key == "role") current->role = value;
    else if (key == "source") current->source = value;
    else if (key == "module_hint") current->module_hint = value;
    else if (key == "radius") current->radius = std::stof(value);
    else if (key == "label") current->label = value;
    else if (key == "points")
    {
      std::istringstream points(value);
      std::string token;
      while (points >> token)
      {
        const std::size_t comma = token.find(',');
        if (comma == std::string::npos) continue;
        current->image_points.push_back({std::stof(token.substr(0, comma)),
                                         std::stof(token.substr(comma + 1))});
      }
    }
  }
  Select(myElements.empty() ? -1 : static_cast<int>(myElements.size()) - 1);
  reason = "annotation elements loaded";
  return true;
}