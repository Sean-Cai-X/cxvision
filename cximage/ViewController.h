#ifndef _ViewController_Header
#define _ViewController_Header

#include "Window.h"

#include <AIS_InteractiveContext.hxx>
#include <AIS_ViewController.hxx>
#include <V3d_View.hxx>

#include <AIS_Manipulator.hxx>

#include <map>
#include <Standard_Handle.hxx>
#include <string>
#include <vector>
#include <imgui.h>

#include "View.h"
#include "Image.h"

#include "Shape.h"
#include "Findline.h"

#include "FastMatch.h"
#include "imagemanager.h"
#include "ParserClass.h"
#include "ParserDebugBridge.h"
#include "muParser.h"
#include "ManualStateTestConsole.h"
#include "ImageAnnotationLayer.h"
#include "SemanticFlowGraph.h"

//! OCCT view controller hosting interaction, image tools, and shape display.
class ViewController : protected AIS_ViewController
{
  struct ScriptCatalogEntry
  {
    std::string name, path, type, status, description;
  };

  struct ScriptResult
  {
    std::string source, script_path, status, reason, runtime_fillback_status;
    std::string image_ref, overlay_ref, result_ref, evidence_ref, issue_entry_ref;
    double elapsed_ms = 0.0;
    std::vector<std::string> log_lines;
  };
public:
  //! Construct the controller with default interaction state.
  ViewController();

  //! Release controller resources.
  ~ViewController();

  //! Run the application event loop.
  void run();

  void LoadBoundStateToManualConsole(const std::string& nodeId,
                                         const std::string& scriptPath);

private:
  void initImGui();
  void initScriptCatalog();
  bool GetSelectedCatalogScript(std::string& outPath,
                                std::string& outName) const;
  void HandleSemanticFlowAction(const SemanticFlowAction& action);
  void drawScriptAcceptancePanels();
  void drawManualStateTestConsole();
  void initManualStateTestConsole();
  void initImageEvidenceLayer();
  void drawImageEvidencePanels();
  void drawImageEvidenceOnCanvas(bool canvasHovered, bool canvasActive,
                                 ImDrawList* drawList);
  ImVec2 ImageToScreen(float ix, float iy) const;
  ImVec2 ScreenToImage(float sx, float sy) const;
  ScriptResult RunCxScript(const std::string& theScriptPath);
  void RefreshRuntimeObjectTable(const std::string& lastMethod,
                                 const std::string& runtimeStatus);
  bool QueryParserObjectExists(const std::string& type,
                               const std::string& name);
  Image* QueryParserImage(const std::string& name);
  bool QueryParserDouble(const std::string& name, double& value);
  bool SetParserDouble(const std::string& name, double value);
  void FitAll();
  double GetScale();
  void ViewerUpDate();
  void DisplayShape(const Handle(AIS_InteractiveObject)& AShape,
                    Standard_Boolean isShow = Standard_True);
  void RemoveAllShapes(Standard_Boolean isUpDate = Standard_False);

  //! Create the GLFW host window.
  void initWindow(int theWidth, int theHeight, const char* theTitle);

  //! Create 3D Viewer.
  void initViewer(int theWidth, int theHeight);

  //! Populate the 3D viewer with demo content.
  void initDemoScene();

  //! Application event loop.
  void mainloop();

  //! Clean up runtime resources.
  void cleanup();

//! @name GLWF callbacks
private:
  //! Handle window resize.
  void onResize(int theWidth, int theHeight);

  //! Handle mouse wheel input.
  void onMouseScroll(double theOffsetX, double theOffsetY);

  //! Handle mouse button changes.
  void onMouseButton(int theButton, int theAction, int theMods);

  //! Handle mouse motion.
  void onMouseMove(int thePosX, int thePosY);


  bool isManipulating = false;
  Standard_Integer startX, startY;

  bool isGDragging = false;
  bool isDragging = false;
  bool ismove = false;
  Graphic3d_Vec2i dragStart;
  gp_Pnt initialShapePosition;
  Handle(AIS_Shape) selectedShape;
  Handle(AIS_TextLabel) selectedTextLabel;
//! @name GLWF callbacks (static functions)
private:

  //! GLFW callback redirecting messages into Message::DefaultMessenger().
  static void errorCallback (int theError, const char* theDescription);

  //! Wrapper for glfwGetWindowUserPointer() returning this class instance.
  static ViewController* toView (GLFWwindow* theWin);

  //! Window resize callback.
  static void onResizeCallback (GLFWwindow* theWin, int theWidth, int theHeight)
  { toView(theWin)->onResize (theWidth, theHeight); }

  //! Frame-buffer resize callback.
  static void onFBResizeCallback (GLFWwindow* theWin, int theWidth, int theHeight)
  { toView(theWin)->onResize (theWidth, theHeight); }

  //! Mouse scroll callback.
  static void onMouseScrollCallback (GLFWwindow* theWin, double theOffsetX, double theOffsetY)
  { toView(theWin)->onMouseScroll (theOffsetX, theOffsetY); }

  //! Mouse click callback.
  static void onMouseButtonCallback (GLFWwindow* theWin, int theButton, int theAction, int theMods)
  { toView(theWin)->onMouseButton (theButton, theAction, theMods); }

  //! Mouse move callback.
  static void onMouseMoveCallback (GLFWwindow* theWin, double thePosX, double thePosY)
  { toView(theWin)->onMouseMove ((int )thePosX, (int )thePosY); }

private:

  enum class RuntimeLineDragHandle
  {
      None = 0,
      StartPoint,
      EndPoint,
      Body
  };

  RuntimeLineDragHandle m_runtimeLineDragHandle = RuntimeLineDragHandle::None;
  std::string m_runtimeLineDragObject;
  float m_runtimeLineDragStartMouseX = 0.0f;
  float m_runtimeLineDragStartMouseY = 0.0f;
  float m_runtimeLineDragStartX0 = 0.0f;
  float m_runtimeLineDragStartY0 = 0.0f;
  float m_runtimeLineDragStartX1 = 0.0f;
  float m_runtimeLineDragStartY1 = 0.0f;

  Handle(Window) myOcctWindow;
  Handle(V3d_CustomView) m_myView;
  Handle(AIS_InteractiveContext) myContext;
  Handle(AIS_Shape) aBox;

public:
    std::string view_Name;
    std::string focuEleUid;
    int st_Width;
    int st_Height;

    std::map<std::string, Handle(AIS_InteractiveObject)> occ_Shapes;
    V3d_TypeOfOrientation viewPort;
private:
    //! Convert screen coordinates into the current view space.
    gp_Pnt ConvertClickToPoint(Standard_Real theX, Standard_Real theY, Handle(V3d_View) theView);
    gp_Pnt m_mousePt;
    gp_Pnt GetViewCenter();

protected:
    enum CurrentAction3d
    {
        CurAction3d_Nothing,
        CurAction3d_DynamicPanning,
        CurAction3d_DynamicZooming,
        CurAction3d_DynamicRotation,
    };
    Standard_Integer m_x_max;
    Standard_Integer m_y_max;
    float m_scale;
    CurrentAction3d m_current_mode;
    bool leftMouseBtn = false;
    bool midMouseBtn = false;
    gp_Pnt mouseDownPT;
    gp_Pnt m_Pt;

    unsigned int texture_id0 = 0;
    cv::Mat s_img0;

    bool m_imageshow = false;

    bool m_planerotate = false;

    int m_iruntimes = 0;

    bool m_ipickpoints = false;

    bool m_ilinescan = false;

    bool m_iattachline = false;


    static bool opencvSW;
    static bool opencvblur;
    static bool opencvreset;
    static bool gpublur;
    static bool irunedge;
    static bool ihedge;
    static bool iwedge;
    static bool ib2wedge;
    static bool iw2bedge;
    static bool ipythre;
    static bool iotsuThreshold;

    static int ivalue1;
    static int ivalue2;
    static int ivalue3;
    static int ivalue4;
    static int ivalue5;
    static int ivalue6;
    static int ivalue7;
    static int ivalue8;
    static int ivalue9;
    static int ivalue10;
    static int ivalue11;
    static int ivalue12;

    //! Create a GPU texture from a cv::Mat image.
    unsigned int CreateTextureFromMat0(const cv::Mat& mat);
    void UpdateImageViewImage(const cv::Mat& image);
    void SetTexturedtoBoxFace(const cv::Mat& image);
    void SetTexturedtoPlane(const cv::Mat& image);
    Handle(Image_PixMap) ConvertCvMatToOcctImage(const cv::Mat& mat);
    vector<float> createGaussianKernel(int kernelSize, double sigma);
    void Imgui_OpenCV_Ini0();

    void Imgui_GPU_NLM_main0(cv::Mat& src_host, cv::Mat& dst_host);
    void Imgui_GPU_NLM_main(cv::Mat& src_host, cv::Mat& dst_host);
    void Imgui_GPU_Gauss_main(cv::Mat& src_host, cv::Mat& dst_host);
    void Imgui_imageDenoising(cv::Mat& src_host, cv::Mat& dst_host);
    void Imgui_OpenCV_Window0(bool* p_open);

    void AdjustModelBoundingBoxToImageSize(const Handle(V3d_View)& myView, const Standard_CString imagePath);

    Handle(Graphic3d_Texture2D) CreateTextureFromImage(const Handle(Image_PixMap)& image);
    void SetBackgroundInView(Handle(V3d_View)& view, const cv::Mat& image);

    Image m_occtimage;

    Shape* m_shapex;

    int m_ibtntimes = 0;
    gp_Pnt m_point0;
    gp_Pnt m_point1;

    PointsShape* m_apoints;
    PointsShape* m_bpoints;

    Findline* m_afindline;

    Shape m_shape0;

    fastmatch m_Match;

    void drawline();
    void ReduceView();
    void HomeView();
private:
    void setCurrentShape(Shape* pshape);
    Shape* indexOf(const string& shapeName);
    Shape* indexAt(const gp_Pnt& pos);
    string uniqueName(const string& name);

    mu::CxParserRuntime m_imageparser;
    ParserDebugBridge m_parserDebugBridge;

    std::ostringstream m_os;
    std::ostringstream m_createcodeos;

    void clearos();
    void clearcreateos();
    void clearparserobject();
    void resetparser();
    void SetParserValue(const string& codestr, double dvalue);
    double GetParserValue(const string& codestr);
    string initialparser();
    string getoutputstring() { return m_os.str(); }

    string m_strcode;

    double m_dmovx = 0;
    double m_dmovy = 0;
    double m_dangle = 0;
    double m_dzoomx = 1;
    double m_dzoomy = 1;

    double m_dscalex = 1;
    double m_dscaley = 1;

    gp_Pnt m_mousePressPos;

    gp_Pnt m_mousePressOffset;
    Shape* m_pmousepressshape = nullptr;
    bool m_resizeHandlePressed;

    float m_current_window_posx;
    float m_current_window_posy;
    float m_imguiw;
    float m_imguih;
    std::vector<ScriptCatalogEntry> m_scriptCatalog;
    int m_selectedScript = -1;
    ScriptResult m_scriptResult;
    bool m_scriptRunRequested = false;
    bool m_showAllScripts = false;
    bool m_showLegacyGpuWork = false;
    bool m_detachablePanels = false;
    bool m_showManualStateTestConsole = true;
    ManualTestContext m_manualTest;
    ImageAnnotationLayer m_annotationLayer;
    std::vector<ScriptSnippet> m_manualSnippets;
    std::vector<ScriptSnippet> m_directTestModules;
    std::vector<DirectCapability> m_directCapabilities;
    SemanticFlowGraph m_semanticFlowGraph;
    bool m_renderImageInOcctBackground = false;
    bool m_showTestPoints = false;
    bool m_showTestRectangle = false;
    bool m_showTestScanLine = false;
    unsigned int m_imageViewTexture = 0;
    cv::Mat m_imageViewImage;
    float m_imageViewZoom = 1.0f;
    float m_imageViewPanX = 0.0f;
    float m_imageViewPanY = 0.0f;
    float m_annotationImagePosX = 0.0f;
    float m_annotationImagePosY = 0.0f;
    float m_annotationImageWidth = 1.0f;
    float m_annotationImageHeight = 1.0f;
    bool m_annotationDragging = false;
    OverlayKind m_annotationDragKind = OverlayKind::Point;
    OverlayImagePoint m_annotationDragStart;
    int m_activePolylineElement = -1;
    bool m_attachToScriptMode = false;
    bool m_showSourcePreviewOverlay = false;
    ImageToolMode m_imageToolMode = ImageToolMode::PointerPan;
    std::string m_annotationManifestPath;
    std::string m_annotationSessionPath;
    std::string m_annotationStatus;
};

#endif // _ViewController_Header

