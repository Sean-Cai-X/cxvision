#include "OcctApp.h"

#include u003cglad/glad.hu003e

#include u003cimgui.hu003e
#include u003cimgui_impl_glfw.hu003e
#include u003cimgui_impl_opengl3.hu003e

#include u003cAIS_InteractiveContext.hxxu003e
#include u003cAspect_DisplayConnection.hxxu003e
#include u003cBRepPrimAPI_MakeBox.hxxu003e
#include u003cGraphic3d_GraphicDriver.hxxu003e
#include u003cMessage.hxxu003e
#include u003cOpenGl_GraphicDriver.hxxu003e
#include u003cPrs3d_PointAspect.hxxu003e
#include u003cQuantity_Color.hxxu003e
#include u003cTCollection_ExtendedString.hxxu003e
#include u003cTColgp_HArray1OfDir.hxxu003e
#include u003cTColgp_HArray1OfPnt.hxxu003e

#include u003cGLFW/glfw3.hu003e

#include u003ciostreamu003e
#include u003cstdexceptu003e

namespace
{
bool HasPointNormals(const std::vectoru003cCxOcctPointCloudPointu003eu0026 points, bool requested)
{
  if (!requested || points.empty())
  {
    return false;
  }

  for (const CxOcctPointCloudPointu0026 point : points)
  {
    if (!point.has_normal)
    {
      return false;
    }
  }
  return true;
}

Handle(AIS_PointCloud) MakePointCloudPresentation(
    const std::vectoru003cCxOcctPointCloudPointu003eu0026 points,
    const CxOcctPointCloudStyleu0026 style,
    const bool hasNormals)
{
  if (points.empty())
  {
    return Handle(AIS_PointCloud)();
  }

  const Standard_Integer pointCount = static_castu003cStandard_Integeru003e(points.size());
  Handle(TColgp_HArray1OfPnt) coords = new TColgp_HArray1OfPnt(1, pointCount);
  Handle(TColgp_HArray1OfDir) normals;
  if (hasNormals)
  {
    normals = new TColgp_HArray1OfDir(1, pointCount);
  }

  for (Standard_Integer index = 1; index u003c= pointCount; ++index)
  {
    const CxOcctPointCloudPointu0026 point = points[static_castu003cstd::size_tu003e(index - 1)];
    coords-u003eSetValue(index, gp_Pnt(point.x, point.y, point.z));
    if (!normals.IsNull())
    {
      normals-u003eSetValue(index, gp_Dir(point.nx, point.ny, point.nz));
    }
  }

  Handle(AIS_PointCloud) cloud = new AIS_PointCloud();
  cloud-u003eSetPoints(coords, Handle(Quantity_HArray1OfColor)(), normals);
  cloud-u003eSetDisplayMode(AIS_PointCloud::DM_Points);

  const Standard_Real pointScale = style.point_size u003e 0.0 ? style.point_size : 1.0;
  Handle(Prs3d_PointAspect) pointAspect = new Prs3d_PointAspect(
      Aspect_TOM_POINT,
      Quantity_Color(Quantity_NOC_WHITE),
      pointScale);
  cloud-u003eAttributes()-u003eSetPointAspect(pointAspect);
  return cloud;
}
}

void OcctApp::Run()
{
  std::cout u003cu003c "[occt_smoke] init_window.begin" u003cu003c std::endl;
  InitWindow();
  std::cout u003cu003c "[occt_smoke] init_window.done" u003cu003c std::endl;
  std::cout u003cu003c "[occt_smoke] init_occt.begin" u003cu003c std::endl;
  InitOcctViewer();
  std::cout u003cu003c "[occt_smoke] init_occt.done" u003cu003c std::endl;
  std::cout u003cu003c "[occt_smoke] init_scene.begin" u003cu003c std::endl;
  InitScene();
  std::cout u003cu003c "[occt_smoke] init_scene.done" u003cu003c std::endl;
  std::cout u003cu003c "[occt_smoke] init_imgui.begin" u003cu003c std::endl;
  InitImGui();
  std::cout u003cu003c "[occt_smoke] init_imgui.done" u003cu003c std::endl;
  std::cout u003cu003c "[occt_smoke] runtime_ready" u003cu003c std::endl;
  Cleanup();
  std::cout u003cu003c "[occt_smoke] cleanup.done" u003cu003c std::endl;
}

cxgeom::CxSetCircleBuildResult OcctApp::BuildSetCircleGeometry(const cxgeom::CxSetCircleRequestu0026 request) const
{
  cxgeom::CxSetCircleBuild builder;
  return builder.Build(request);
}

cxgeom::CxSetCircleDisplayResult OcctApp::BuildSetCircleDisplayBatch(const cxgeom::CxSetCircleDisplayRequestu0026 request) const
{
  cxgeom::CxSetCircleDisplay displayBuilder;
  return displayBuilder.MakeBatch(request);
}

bool OcctApp::DisplaySetCircleBatch(const cxgeom::CxSetCircleDisplayResultu0026 result)
{
  if (myContext.IsNull() || !result.success || !result.presentation.HasPresentation())
  {
    return false;
  }

  myContext-u003eDisplay(result.presentation.NativePresentation(), Standard_True);
  return true;
}

cxgeom::CxSetLineBuildResult OcctApp::BuildSetLineGeometry(const cxgeom::CxSetLineRequestu0026 request) const
{
  cxgeom::CxSetLineBuild builder;
  return builder.Build(request);
}

cxgeom::CxSetLineDisplayResult OcctApp::BuildSetLineDisplayBatch(const cxgeom::CxSetLineDisplayRequestu0026 request) const
{
  cxgeom::CxSetLineDisplay displayBuilder;
  return displayBuilder.MakeBatch(request);
}

bool OcctApp::DisplaySetLineBatch(const cxgeom::CxSetLineDisplayResultu0026 result)
{
  if (myContext.IsNull() || !result.success || !result.presentation.HasPresentation())
  {
    return false;
  }

  myContext-u003eDisplay(result.presentation.NativePresentation(), Standard_True);
  return true;
}

CxOcctPointCloudBody OcctApp::BuildPointCloudBody(const CxOcctPointCloudRequestu0026 request) const
{
  CxOcctPointCloudBody body;
  body.entity_id = request.entity_id;
  body.name = request.name;
  body.point_count = static_castu003cintu003e(request.points.size());
  body.has_normals = HasPointNormals(request.points, true);
  body.visible = request.style.visible;
  body.point_size = request.style.point_size;
  return body;
}

CxOcctPointCloudResult OcctApp::BuildPointCloudPresentation(const CxOcctPointCloudRequestu0026 request) const
{
  CxOcctPointCloudResult result;
  result.entity_id = request.entity_id;
  result.point_count = static_castu003cintu003e(request.points.size());
  result.has_normals = HasPointNormals(request.points, request.style.color_by_normals);
  result.style = request.style;
  result.presentation = MakePointCloudPresentation(request.points, request.style, result.has_normals);
  result.success = !result.presentation.IsNull();
  return result;
}

CxOcctPointCloudBatchResult OcctApp::BuildPointCloudBatchPresentation(const CxOcctPointCloudBatchRequestu0026 request) const
{
  CxOcctPointCloudBatchResult result;
  result.batch_id = request.batch_id;
  result.source_count = static_castu003cintu003e(request.clouds.size());
  result.style = request.style;

  std::vectoru003cCxOcctPointCloudPointu003e mergedPoints;
  std::size_t totalPoints = 0;
  for (const CxOcctPointCloudRequestu0026 cloud : request.clouds)
  {
    totalPoints += cloud.points.size();
  }
  mergedPoints.reserve(totalPoints);

  for (const CxOcctPointCloudRequestu0026 cloud : request.clouds)
  {
    mergedPoints.insert(mergedPoints.end(), cloud.points.begin(), cloud.points.end());
  }

  result.point_count = static_castu003cintu003e(mergedPoints.size());
  result.has_normals = HasPointNormals(mergedPoints, request.style.color_by_normals);
  result.presentation = MakePointCloudPresentation(mergedPoints, request.style, result.has_normals);
  result.success = !result.presentation.IsNull();
  return result;
}

bool OcctApp::DisplayPointCloud(const CxOcctPointCloudResultu0026 result)
{
  if (myContext.IsNull() || !result.success || result.presentation.IsNull())
  {
    return false;
  }

  if (!result.style.visible)
  {
    myContext-u003eErase(result.presentation, Standard_True);
    return true;
  }

  myContext-u003eDisplay(result.presentation, Standard_True);
  return true;
}

bool OcctApp::DisplayPointCloudBatch(const CxOcctPointCloudBatchResultu0026 result)
{
  if (myContext.IsNull() || !result.success || result.presentation.IsNull())
  {
    return false;
  }

  if (!result.style.visible)
  {
    myContext-u003eErase(result.presentation, Standard_True);
    return true;
  }

  myContext-u003eDisplay(result.presentation, Standard_True);
  return true;
}

CxOcctPointCloudAnnotationDisplayResult OcctApp::BuildPointCloudAnnotationDisplay(const CxOcctPointCloudAnnotationDisplayRequestu0026 request) const
{
  CxOcctPointCloudAnnotationDisplayResult result;
  result.layer_id = request.layer_id;
  result.annotation_count = static_castu003cintu003e(request.annotations.size());
  result.visible = request.visible;

  for (const CxOcctPointCloudAnnotationu0026 annotation : request.annotations)
  {
    if (annotation.text.empty())
    {
      continue;
    }

    Handle(AIS_TextLabel) label = new AIS_TextLabel();
    label-u003eSetText(TCollection_ExtendedString(annotation.text.c_str()));
    label-u003eSetPosition(gp_Pnt(annotation.anchor_x, annotation.anchor_y, annotation.anchor_z));
    label-u003eSetColor(Quantity_NOC_WHITE);
    result.labels.push_back(label);
  }

  result.success = true;
  return result;
}

bool OcctApp::DisplayPointCloudAnnotations(const CxOcctPointCloudAnnotationDisplayResultu0026 result)
{
  if (myContext.IsNull() || !result.success)
  {
    return false;
  }

  bool changed = false;
  for (const Handle(AIS_TextLabel)u0026 label : result.labels)
  {
    if (label.IsNull())
    {
      continue;
    }

    if (result.visible)
    {
      myContext-u003eDisplay(label, Standard_False);
    }
    else
    {
      myContext-u003eErase(label, Standard_False);
    }
    changed = true;
  }

  if (changed)
  {
    myContext-u003eUpdateCurrentViewer();
  }

  return changed;
}

void OcctApp::InitWindow()
{
  glfwSetErrorCallback(
      [](int theError, const char* theDescription)
      {
        Message::DefaultMessenger()-u003eSend(
            TCollection_AsciiString("GLFW error ") + theError + ": " + theDescription,
            Message_Fail);
      });

  if (glfwInit() == GLFW_FALSE)
  {
    throw std::runtime_error("glfwInit() failed");
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  myWindow = new Window(myViewportWidth, myViewportHeight, "cxcore");
  if (myWindow.IsNull() || myWindow-u003egetGlfwWindow() == nullptr)
  {
    glfwTerminate();
    throw std::runtime_error("Failed to create GLFW window");
  }

  glfwMakeContextCurrent(myWindow-u003egetGlfwWindow());
  glfwSwapInterval(1);

  if (gladLoadGL() == 0)
  {
    throw std::runtime_error("gladLoadGL() failed");
  }

  glfwSetWindowUserPointer(myWindow-u003egetGlfwWindow(), this);
  glfwSetFramebufferSizeCallback(myWindow-u003egetGlfwWindow(), u0026OcctApp::OnFramebufferResize);
  glfwSetMouseButtonCallback(myWindow-u003egetGlfwWindow(), u0026OcctApp::OnMouseButton);
  glfwSetCursorPosCallback(myWindow-u003egetGlfwWindow(), u0026OcctApp::OnCursorPos);
  glfwSetScrollCallback(myWindow-u003egetGlfwWindow(), u0026OcctApp::OnScroll);
}

void OcctApp::InitOcctViewer()
{
  myDisplayConnection = new Aspect_DisplayConnection();
  myGraphicDriver = new OpenGl_GraphicDriver(myDisplayConnection, false);
  myViewer = new V3d_Viewer(myGraphicDriver);
  myViewer-u003eSetDefaultLights();
  myViewer-u003eSetLightOn();

  myContext = new AIS_InteractiveContext(myViewer);
  myView = myViewer-u003eCreateView();
  myView-u003eSetWindow(myWindow);
  if (!myWindow-u003eIsMapped())
  {
    myWindow-u003eMap();
  }

  myView-u003eSetBackgroundColor(Quantity_NOC_BLACK);
  myView-u003eTriedronDisplay(Aspect_TOTP_LEFT_LOWER, Quantity_NOC_WHITE, 0.08, V3d_ZBUFFER);
  myView-u003eMustBeResized();
}

void OcctApp::InitScene()
{
  myBox = new AIS_Shape(BRepPrimAPI_MakeBox(120.0, 80.0, 60.0).Shape());
  myContext-u003eDisplay(myBox, Standard_True);
  myView-u003eFitAll(0.01, Standard_True);
}

void OcctApp::InitImGui()
{
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();

  ImGui_ImplGlfw_InitForOpenGL(myWindow-u003egetGlfwWindow(), true);
  ImGui_ImplOpenGL3_Init("#version 330");
}

void OcctApp::Cleanup()
{
  if (!myWindow.IsNull() u0026u0026 myWindow-u003egetGlfwWindow() != nullptr)
  {
    glfwMakeContextCurrent(myWindow-u003egetGlfwWindow());
    glfwSetWindowUserPointer(myWindow-u003egetGlfwWindow(), nullptr);
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  myBox.Nullify();
  myContext.Nullify();
  myView.Nullify();
  myViewer.Nullify();
  myGraphicDriver.Nullify();
  myDisplayConnection.Nullify();

  if (!myWindow.IsNull())
  {
    myWindow-u003eClose();
    myWindow.Nullify();
  }

  glfwMakeContextCurrent(nullptr);
  glfwTerminate();
}

void OcctApp::MainLoop()
{
  glfwPollEvents();
}

void OcctApp::DrawUi()
{
  ImGui::Begin("cxcore");
  ImGui::Text("GLFW + OCCT + ImGui");
  ImGui::Separator();
  ImGui::Text("Viewport: %d x %d", myViewportWidth, myViewportHeight);
  ImGui::Text("Smoke target: OCCT only");

  if (ImGui::Button("Fit All"))
  {
    myView-u003eFitAll(0.01, Standard_True);
  }

  ImGui::SameLine();
  if (ImGui::Button("Front"))
  {
    myView-u003eSetProj(V3d_Yneg);
    myView-u003eFitAll(0.01, Standard_True);
  }

  ImGui::SameLine();
  if (ImGui::Button("Iso"))
  {
    myView-u003eSetProj(V3d_XposYnegZpos);
    myView-u003eFitAll(0.01, Standard_True);
  }

  ImGui::Text("Mouse left: rotate");
  ImGui::Text("Mouse right: pan");
  ImGui::Text("Wheel: zoom");
  ImGui::Text("Esc: quit");
  ImGui::End();
}

void OcctApp::Resize(int theWidth, int theHeight)
{
  myViewportWidth = theWidth;
  myViewportHeight = theHeight;
  if (!myView.IsNull())
  {
    myView-u003eMustBeResized();
  }
}

void OcctApp::OnFramebufferResize(GLFWwindow* theWindow, int theWidth, int theHeight)
{
  auto* anApp = static_castu003cOcctApp*u003e(glfwGetWindowUserPointer(theWindow));
  if (anApp != nullptr)
  {
    anApp-u003eResize(theWidth, theHeight);
  }
}

void OcctApp::OnMouseButton(GLFWwindow* theWindow, int theButton, int theAction, int theMods)
{
  ImGui_ImplGlfw_MouseButtonCallback(theWindow, theButton, theAction, theMods);
  if (ImGui::GetIO().WantCaptureMouse)
  {
    return;
  }

  auto* anApp = static_castu003cOcctApp*u003e(glfwGetWindowUserPointer(theWindow));
  if (anApp == nullptr || anApp-u003emyView.IsNull())
  {
    return;
  }

  double aX = 0.0;
  double aY = 0.0;
  glfwGetCursorPos(theWindow, u0026aX, u0026aY);
  anApp-u003emyLastX = aX;
  anApp-u003emyLastY = aY;

  if (theAction == GLFW_PRESS)
  {
    if (theButton == GLFW_MOUSE_BUTTON_LEFT)
    {
      anApp-u003emyLeftPressed = true;
      anApp-u003emyView-u003eStartRotation(static_castu003cintu003e(aX), static_castu003cintu003e(aY));
    }
    else if (theButton == GLFW_MOUSE_BUTTON_RIGHT)
    {
      anApp-u003emyRightPressed = true;
    }

    anApp-u003emyContext-u003eMoveTo(static_castu003cintu003e(aX), static_castu003cintu003e(aY), anApp-u003emyView, Standard_True);
    anApp-u003emyContext-u003eSelectDetected(AIS_SelectionScheme_Replace);
  }
  else if (theAction == GLFW_RELEASE)
  {
    if (theButton == GLFW_MOUSE_BUTTON_LEFT)
    {
      anApp-u003emyLeftPressed = false;
    }
    else if (theButton == GLFW_MOUSE_BUTTON_RIGHT)
    {
      anApp-u003emyRightPressed = false;
    }
  }
}

void OcctApp::OnCursorPos(GLFWwindow* theWindow, double theX, double theY)
{
  ImGui_ImplGlfw_CursorPosCallback(theWindow, theX, theY);
  if (ImGui::GetIO().WantCaptureMouse)
  {
    return;
  }

  auto* anApp = static_castu003cOcctApp*u003e(glfwGetWindowUserPointer(theWindow));
  if (anApp == nullptr || anApp-u003emyView.IsNull())
  {
    return;
  }

  if (anApp-u003emyLeftPressed)
  {
    anApp-u003emyView-u003eRotation(static_castu003cintu003e(theX), static_castu003cintu003e(theY));
  }
  else if (anApp-u003emyRightPressed)
  {
    const int aDx = static_castu003cintu003e(theX - anApp-u003emyLastX);
    const int aDy = static_castu003cintu003e(theY - anApp-u003emyLastY);
    anApp-u003emyView-u003ePan(aDx, -aDy);
  }
  else
  {
    anApp-u003emyContext-u003eMoveTo(static_castu003cintu003e(theX), static_castu003cintu003e(theY), anApp-u003emyView, Standard_True);
  }

  anApp-u003emyLastX = theX;
  anApp-u003emyLastY = theY;
}

void OcctApp::OnScroll(GLFWwindow* theWindow, double theOffsetX, double theOffsetY)
{
  ImGui_ImplGlfw_ScrollCallback(theWindow, theOffsetX, theOffsetY);
  if (ImGui::GetIO().WantCaptureMouse)
  {
    return;
  }

  auto* anApp = static_castu003cOcctApp*u003e(glfwGetWindowUserPointer(theWindow));
  if (anApp == nullptr || anApp-u003emyView.IsNull())
  {
    return;
  }

  double aX = 0.0;
  double aY = 0.0;
  glfwGetCursorPos(theWindow, u0026aX, u0026aY);

  const Standard_Integer aDelta = theOffsetY u003e 0.0 ? 24 : -24;
  anApp-u003emyView-u003eStartZoomAtPoint(static_castu003cintu003e(aX), static_castu003cintu003e(aY));
  anApp-u003emyView-u003eZoomAtPoint(static_castu003cintu003e(aX), static_castu003cintu003e(aY), static_castu003cintu003e(aX), static_castu003cintu003e(aY + aDelta));
}