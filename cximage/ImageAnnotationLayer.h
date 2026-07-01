#ifndef CXIMAGE_IMAGE_ANNOTATION_LAYER_H
#define CXIMAGE_IMAGE_ANNOTATION_LAYER_H

#include <string>
#include <vector>

struct OverlayImagePoint
{
  float x = 0.0f;
  float y = 0.0f;
};

enum class OverlayKind
{
  Point,
  Line,
  Rect,
  Circle,
  Polyline
};

enum class ImageToolMode
{
  PointerPan,
  PointCreate,
  LineCreate,
  RectCreate,
  CircleCreate,
  PolylineCreate,
  AttachToScript
};

struct OverlayElement
{
  int id = 0;
  std::string ref;
  OverlayKind kind = OverlayKind::Point;
  std::string role;
  std::string source;
  std::string module_hint;
  std::vector<OverlayImagePoint> image_points;
  float radius = 0.0f;
  bool selected = false;
  bool visible = true;
  bool editable = true;
  std::string label;
  std::string evidence_ref;
  std::string generated_statement;
  std::string result_ref;
  std::string issue_entry_ref;
};

struct AnnotationToolDefinition
{
  std::string name;
  OverlayKind kind = OverlayKind::Point;
  std::string role;
  std::string source;
  std::string action;
  std::string module_hint;
  std::string description;
};

class ImageAnnotationLayer
{
public:
  bool LoadManifest(const std::string& path, std::string& reason);
  bool SaveElements(const std::string& path, const std::string& imageRef,
                    std::string& reason) const;
  bool LoadElements(const std::string& path, std::string& imageRef,
                    std::string& reason);

  OverlayElement& Create(OverlayKind kind, const std::string& role,
                         const std::string& source,
                         const std::string& moduleHint);
  void Clear();
  void Select(int index);

  std::vector<AnnotationToolDefinition>& Tools() { return myTools; }
  const std::vector<AnnotationToolDefinition>& Tools() const { return myTools; }
  std::vector<OverlayElement>& Elements() { return myElements; }
  const std::vector<OverlayElement>& Elements() const { return myElements; }

  int SelectedIndex() const { return mySelectedIndex; }
  OverlayElement* Selected();
  const OverlayElement* Selected() const;
  int ActiveToolIndex() const { return myActiveToolIndex; }
  void SetActiveToolIndex(int index) { myActiveToolIndex = index; }
  const AnnotationToolDefinition* ActiveTool() const;
  std::string SelectedRef(OverlayKind kind) const;

  static const char* KindName(OverlayKind kind);
  static bool ParseKind(const std::string& text, OverlayKind& kind);

private:
  std::string MakeRef(OverlayKind kind, int id) const;

  std::vector<AnnotationToolDefinition> myTools;
  std::vector<OverlayElement> myElements;
  int myNextId = 1;
  int mySelectedIndex = -1;
  int myActiveToolIndex = -1;
};

#endif