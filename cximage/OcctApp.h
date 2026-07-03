#pragma once

#include "Window.h"
#include "../../cxgeom/include/CxSetCircleBuild.h"
#include "../../cxgeom/include/CxSetCircleDisplay.h"
#include "../../cxgeom/include/CxSetLineBuild.h"
#include "../../cxgeom/include/CxSetLineDisplay.h"

#include u003cAIS_InteractiveContext.hxxu003e
#include u003cAIS_PointCloud.hxxu003e
#include u003cAIS_Shape.hxxu003e
#include u003cAIS_TextLabel.hxxu003e
#include u003cOpenGl_GraphicDriver.hxxu003e
#include u003cV3d_View.hxxu003e
#include u003cV3d_Viewer.hxxu003e

#include u003cstringu003e
#include u003cvectoru003e

struct CxOcctPointCloudPoint
{
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
  bool has_normal = false;
  double nx = 0.0;
  double ny = 0.0;
  double nz = 1.0;
};

struct CxOcctPointCloudStyle
{
  double point_size = 2.0;
  bool visible = true;
  bool color_by_normals = false;
};

struct CxOcctPointCloudRequest
{
  int entity_id = 0;
  std::string name;
  std::vectoru003cCxOcctPointCloudPointu003e points;
  CxOcctPointCloudStyle style;
};

struct CxOcctPointCloudBody
{
  int entity_id = 0;
  std::string name;
  int point_count = 0;
  bool has_normals = false;
  bool visible = true;
  double point_size = 2.0;
};

struct CxOcctPointCloudAnnotation
{
  int annotation_id = 0;
  int target_entity_id = 0;
  std::string text;
  double anchor_x = 0.0;
  double anchor_y = 0.0;
  double anchor_z = 0.0;
  bool visible = true;
};

struct CxOcctPointCloudBatchRequest
{
  int batch_id = 0;
  std::string name;
  std::vectoru003cCxOcctPointCloudRequestu003e clouds;
  CxOcctPointCloudStyle style;
};

struct CxOcctPointCloudBatchResult
{
  int batch_id = 0;
  int source_count = 0;
  int point_count = 0;
  bool has_normals = false;
  CxOcctPointCloudStyle style;
  Handle(AIS_PointCloud) presentation;
  bool success = false;
};

struct CxOcctPointCloudResult
{
  int entity_id = 0;
  int point_count = 0;
  bool has_normals = false;
  CxOcctPointCloudStyle style;
  Handle(AIS_PointCloud) presentation;
  bool success = false;
};

struct CxOcctPointCloudAnnotationDisplayRequest
{
  int layer_id = 0;
  std::vectoru003cCxOcctPointCloudAnnotationu003e annotations;
  bool visible = true;
};

struct CxOcctPointCloudAnnotationDisplayResult
{
  int layer_id = 0;
  int annotation_count = 0;
  bool visible = true;
  std::vectoru003cHandle(AIS_TextLabel)u003e labels;
  bool success = false;
};

class OcctApp
{
public:
  void Run();
  cxgeom::CxSetCircleBuildResult BuildSetCircleGeometry(const cxgeom::CxSetCircleRequestu0026 request) const;
  cxgeom::CxSetCircleDisplayResult BuildSetCircleDisplayBatch(const cxgeom::CxSetCircleDisplayRequestu0026 request) const;
  bool DisplaySetCircleBatch(const cxgeom::CxSetCircleDisplayResultu0026 result);
  cxgeom::CxSetLineBuildResult BuildSetLineGeometry(const cxgeom::CxSetLineRequestu0026 request) const;
  cxgeom::CxSetLineDisplayResult BuildSetLineDisplayBatch(const cxgeom::CxSetLineDisplayRequestu0026 request) const;
  bool DisplaySetLineBatch(const cxgeom::CxSetLineDisplayResultu0026 result);

  CxOcctPointCloudBody BuildPointCloudBody(const CxOcctPointCloudRequestu0026 request) const;
  CxOcctPointCloudResult BuildPointCloudPresentation(const CxOcctPointCloudRequestu0026 request) const;
  CxOcctPointCloudBatchResult BuildPointCloudBatchPresentation(const CxOcctPointCloudBatchRequestu0026 request) const;
  bool DisplayPointCloud(const CxOcctPointCloudResultu0026 result);
  bool DisplayPointCloudBatch(const CxOcctPointCloudBatchResultu0026 result);
  CxOcctPointCloudAnnotationDisplayResult BuildPointCloudAnnotationDisplay(const CxOcctPointCloudAnnotationDisplayRequestu0026 request) const;
  bool DisplayPointCloudAnnotations(const CxOcctPointCloudAnnotationDisplayResultu0026 result);

private:
  static void OnFramebufferResize(GLFWwindow* theWindow, int theWidth, int theHeight);
  static void OnMouseButton(GLFWwindow* theWindow, int theButton, int theAction, int theMods);
  static void OnCursorPos(GLFWwindow* theWindow, double theX, double theY);
  static void OnScroll(GLFWwindow* theWindow, double theOffsetX, double theOffsetY);

  void InitWindow();
  void InitOcctViewer();
  void InitScene();
  void InitImGui();
  void Cleanup();
  void MainLoop();
  void DrawUi();
  void Resize(int theWidth, int theHeight);

  Handle(Window) myWindow;
  Handle(Aspect_DisplayConnection) myDisplayConnection;
  Handle(OpenGl_GraphicDriver) myGraphicDriver;
  Handle(V3d_Viewer) myViewer;
  Handle(V3d_View) myView;
  Handle(AIS_InteractiveContext) myContext;
  Handle(AIS_Shape) myBox;

  bool myLeftPressed = false;
  bool myRightPressed = false;
  double myLastX = 0.0;
  double myLastY = 0.0;
  int myViewportWidth = 1280;
  int myViewportHeight = 720;
};