#include "ManualStateTestConsole.h"
#include "viewcontroller.h"
#include "ManualConsoleGauge.h"
#include "ManualConsoleParamRegressionPanel.h"
#include "LineGaugeShape.h"
#include <glad/glad.h>

#include "occtinclude.h"
#include <GLFW/glfw3.h>

#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <AIS_Shape.hxx>
#include <Aspect_Handle.hxx>
#include <Aspect_DisplayConnection.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <Message.hxx>
#include <Message_Messenger.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <AIS_Manipulator.hxx>
#include <AIS_InteractiveContext.hxx>
#include <V3d_View.hxx>
#include <opencv2/opencv.hpp>
#include <AIS_TexturedShape.hxx>
#include <Image_AlienPixMap.hxx>
#include <Geom_RectangularTrimmedSurface.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>

#include <iostream>
#include <filesystem>
#include <fstream>

#ifndef CXCORE_ENABLE_VIEWCONTROLLER_CUDA
#define CXCORE_ENABLE_VIEWCONTROLLER_CUDA 0
#endif
#if CXCORE_ENABLE_VIEWCONTROLLER_CUDA
#include <cuda_runtime.h>
#include "AI_kernels.h"


#include <cuda_gl_interop.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "GPU\imageDenoising.h"

#include <helper_functions.h>
#include <helper_cuda.h>
#endif

#include <math.h>
#include <vector>
#include <algorithm>

#include <string>

#include <opencv2/core/version.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/video/tracking.hpp>


#include "Sysctl.h"


#if CXCORE_ENABLE_VIEWCONTROLLER_CUDA
#include "CalibPolyModel.h"
#include "yolov8all_1_23.h"
#endif

namespace
{
  namespace fs = std::filesystem;

  fs::path findRepositoryRoot()
  {
    const fs::path candidates[] = { fs::path(__FILE__).parent_path().parent_path(), fs::current_path() };
    for (const fs::path& candidate : candidates)
    {
      fs::path current = candidate;
      while (!current.empty())
      {
        if (fs::exists(current / "cxparser" / "cxscript")) return current;
        const fs::path parent = current.parent_path();
        if (parent == current) break;
        current = parent;
      }
    }
    return fs::path();
  }

  bool pathIsWithin(const fs::path& child, const fs::path& parent)
  {
    const fs::path normalizedChild = fs::weakly_canonical(child);
    const fs::path normalizedParent = fs::weakly_canonical(parent);
    auto childIt = normalizedChild.begin();
    for (auto parentIt = normalizedParent.begin(); parentIt != normalizedParent.end(); ++parentIt, ++childIt)
    {
      if (childIt == normalizedChild.end() || *childIt != *parentIt) return false;
    }
    return true;
  }

  const char* emptyAsNone(const std::string& value)
  {
    return value.empty() ? "(none)" : value.c_str();
  }
  static Aspect_VKeyMouse mouseButtonFromGlfw (int theButton)
  {
    switch (theButton)
    {
      case GLFW_MOUSE_BUTTON_LEFT:   return Aspect_VKeyMouse_LeftButton;
      case GLFW_MOUSE_BUTTON_RIGHT:  return Aspect_VKeyMouse_RightButton;
      case GLFW_MOUSE_BUTTON_MIDDLE: return Aspect_VKeyMouse_MiddleButton;
    }
    return Aspect_VKeyMouse_NONE;
  }

  static Aspect_VKeyFlags keyFlagsFromGlfw (int theFlags)
  {
    Aspect_VKeyFlags aFlags = Aspect_VKeyFlags_NONE;
    if ((theFlags & GLFW_MOD_SHIFT) != 0)
    {
      aFlags |= Aspect_VKeyFlags_SHIFT;
    }
    if ((theFlags & GLFW_MOD_CONTROL) != 0)
    {
      aFlags |= Aspect_VKeyFlags_CTRL;
    }
    if ((theFlags & GLFW_MOD_ALT) != 0)
    {
      aFlags |= Aspect_VKeyFlags_ALT;
    }
    if ((theFlags & GLFW_MOD_SUPER) != 0)
    {
      aFlags |= Aspect_VKeyFlags_META;
    }
    return aFlags;
  }
}

bool ViewController::opencvSW = false;
bool ViewController::opencvblur = false;
bool ViewController::opencvreset = false;
bool ViewController::gpublur = false;

bool ViewController::irunedge = false;

bool ViewController::ipythre = false;
bool ViewController::iotsuThreshold = false;
bool ViewController::ihedge = false;
bool ViewController::iwedge = false;

bool ViewController::ib2wedge = false;
bool ViewController::iw2bedge = false;

int ViewController::ivalue1 = 0;
int ViewController::ivalue2 = 0;

int ViewController::ivalue3 = 4;
int ViewController::ivalue4 = 11;

int ViewController::ivalue5 = 11;
int ViewController::ivalue6 = 2;

int ViewController::ivalue7 = 0;
int ViewController::ivalue8 = 0;

int ViewController::ivalue9 = 0;
int ViewController::ivalue10 = 0;

int ViewController::ivalue11 = 0;
int ViewController::ivalue12 = 0;

std::string formatNumber(double dvalue)
{
    std::stringstream ss;
    ss << dvalue << ";";
    return ss.str();
}

ViewController::ViewController()
{
    mouseDownPT.SetX(0);
    mouseDownPT.SetY(0);

    m_Pt.SetX(0);
    m_Pt.SetY(0);
    m_scale = 1.0;
}

ViewController::~ViewController()
{
}

gp_Pnt ViewController::ConvertClickToPoint(Standard_Real theX, Standard_Real theY, Handle(V3d_View) theView)
{
    Standard_Real XEye, YEye, ZEye, XAt, YAt, ZAt;
    theView->Eye(XEye, YEye, ZEye);
    theView->At(XAt, YAt, ZAt);
    gp_Pnt EyePoint(XEye, YEye, ZEye);
    gp_Pnt AtPoint(XAt, YAt, ZAt);

    gp_Vec EyeVector(EyePoint, AtPoint);
    gp_Dir EyeDir(EyeVector);

    gp_Pln PlaneOfTheView = gp_Pln(AtPoint, EyeDir);
    Standard_Real X, Y, Z;
    theView->Convert(int(theX), int(theY), X, Y, Z);
    gp_Pnt ConvertedPoint(X, Y, Z);
    gp_Pnt ResultPoint;
    return ResultPoint;
}

ViewController* ViewController::toView(GLFWwindow* theWin)
{
  return static_cast<ViewController*>(glfwGetWindowUserPointer(theWin));
}

void ViewController::errorCallback(int theError, const char* theDescription)
{
  Message::DefaultMessenger()->Send(TCollection_AsciiString("Error") + theError + ": " + theDescription, Message_Fail);
}

GLuint gl_PBO, gl_Tex;
#if CXCORE_ENABLE_VIEWCONTROLLER_CUDA
struct cudaGraphicsResource* cuda_pbo_resource;
uchar4* h_Src;
#endif
int imageW, imageH;
GLuint shader;

#if CXCORE_ENABLE_VIEWCONTROLLER_CUDA
int g_Kernel = 0;
bool g_FPS = false;
bool g_Diag = false;
StopWatchInterface* timer = NULL;

const float noiseStep = 0.025f;
const float lerpStep = 0.025f;
static float knnNoise = 0.32f;
static float nlmNoise = 1.45f;
static float lerpC = 0.2f;

const int frameN = 24;
int frameCounter = 0;

#define BUFFER_DATA(i) ((char *)0 + i)

const int frameCheckNumber = 4;
int fpsCount = 0;
int fpsLimit = 1;
unsigned int frameCount = 0;
unsigned int g_TotalErrors = 0;

int* pArgc = NULL;
char** pArgv = NULL;

#define MAX_EPSILON_ERROR 5
#define REFRESH_DELAY 10
#define BUFFER_DATA(i) ((char *)0 + i)

void runImageFiltersx(TColor* d_dst, int imageW, int imageH, int g_Kernel, cudaTextureObject_t texImagex) {
    switch (g_Kernel) {
    case 0:
        cuda_Copy(d_dst, imageW, imageH, texImagex);
        break;

    case 1:
        if (!g_Diag) {
            cuda_KNN(d_dst, imageW, imageH, 1.0f / (knnNoise * knnNoise), lerpC,
                texImagex);
        }
        else {
            cuda_KNNdiag(d_dst, imageW, imageH, 1.0f / (knnNoise * knnNoise), lerpC,
                texImagex);
        }

        break;

    case 2:
        if (!g_Diag) {
            cuda_NLM(d_dst, imageW, imageH, 1.0f / (nlmNoise * nlmNoise), lerpC,
                texImagex);
        }
        else {
            cuda_NLMdiag(d_dst, imageW, imageH, 1.0f / (nlmNoise * nlmNoise), lerpC,
                texImagex);
        }

        break;

    case 3:
        if (!g_Diag) {
            cuda_NLM2(d_dst, imageW, imageH, 1.0f / (nlmNoise * nlmNoise), lerpC,
                texImagex);
        }
        else {
            cuda_NLM2diag(d_dst, imageW, imageH, 1.0f / (nlmNoise * nlmNoise),
                lerpC, texImagex);
        }

        break;
    }
    cudaDeviceSynchronize();
    getLastCudaError("Filtering kernel execution failed.\n");
}
void runImageFilters(TColor* d_dst) {
    switch (g_Kernel) {
    case 0:
        cuda_Copy(d_dst, imageW, imageH, texImage);
        break;

    case 1:
        if (!g_Diag) {
            cuda_KNN(d_dst, imageW, imageH, 1.0f / (knnNoise * knnNoise), lerpC,
                texImage);
        }
        else {
            cuda_KNNdiag(d_dst, imageW, imageH, 1.0f / (knnNoise * knnNoise), lerpC,
                texImage);
        }

        break;

    case 2:
        if (!g_Diag) {
            cuda_NLM(d_dst, imageW, imageH, 1.0f / (nlmNoise * nlmNoise), lerpC,
                texImage);
        }
        else {
            cuda_NLMdiag(d_dst, imageW, imageH, 1.0f / (nlmNoise * nlmNoise), lerpC,
                texImage);
        }

        break;

    case 3:
        if (!g_Diag) {
            cuda_NLM2(d_dst, imageW, imageH, 1.0f / (nlmNoise * nlmNoise), lerpC,
                texImage);
        }
        else {
            cuda_NLM2diag(d_dst, imageW, imageH, 1.0f / (nlmNoise * nlmNoise),
                lerpC, texImage);
        }

        break;
    }
    cudaDeviceSynchronize();
    getLastCudaError("Filtering kernel execution failed.\n");
}

void InitializeTextureAndPBO(int imageW, int imageH) {
    glGenBuffers(1, &gl_PBO);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, gl_PBO);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, imageW * imageH * sizeof(uchar4), NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    cudaGraphicsResource* cuda_pbo_resource;
    checkCudaErrors(cudaGraphicsGLRegisterBuffer(&cuda_pbo_resource, gl_PBO,
        cudaGraphicsMapFlagsWriteDiscard));

    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, imageW, imageH, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);

}
GLuint UpdateTextureWithCuda(int imageW, int imageH)
{
    TColor* d_dst = NULL;
    size_t num_bytes;

    checkCudaErrors(cudaGraphicsMapResources(1, &cuda_pbo_resource, 0));
    getLastCudaError("cudaGraphicsMapResources failed");

    checkCudaErrors(cudaGraphicsResourceGetMappedPointer(
        (void**)&d_dst, &num_bytes, cuda_pbo_resource));
    getLastCudaError("cudaGraphicsResourceGetMappedPointer failed");

    runImageFilters(d_dst);

    checkCudaErrors(cudaDeviceSynchronize());

    checkCudaErrors(cudaGraphicsUnmapResources(1, &cuda_pbo_resource, 0));

    glBindTexture(GL_TEXTURE_2D, texImage);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, imageW, imageH,
        GL_RGBA, GL_UNSIGNED_BYTE, BUFFER_DATA(0));
    glBindTexture(GL_TEXTURE_2D, 0);

    return texImage;
}

#define cudaCheckErrors(ans) { gpuAssert((ans), __FILE__, __LINE__); }
inline void gpuAssert(cudaError_t code, const char* file, int line, bool abort = true)
{
    if (code != cudaSuccess)
    {
        fprintf(stderr, "GPUassert: %s %s %d\n", cudaGetErrorString(code), file, line);
        if (abort) exit(code);
    }
}

GLuint CreateTextureCube(cv::Mat& src) {
    if (src.empty()) {
        std::cerr << "Input image is empty!" << std::endl;
        return 0;
    }

    cv::Mat src_rgba;
    if (src.channels() == 3) {
        cv::cvtColor(src, src_rgba, cv::COLOR_BGR2RGBA);
    }
    else if (src.channels() == 4) {
        src_rgba = src;
    }
    else {
        std::cerr << "Unsupported number of channels: " << src.channels() << std::endl;
        return 0;
    }

    GLuint gl_texture_id;
    glGenTextures(1, &gl_texture_id);
    glBindTexture(GL_TEXTURE_2D, gl_texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, src.cols, src.rows, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, src_rgba.data);
    glBindTexture(GL_TEXTURE_2D, 0);

    cudaGraphicsResource* cuda_tex_resource;
    cudaCheckErrors(cudaGraphicsGLRegisterImage(&cuda_tex_resource, gl_texture_id,
        GL_TEXTURE_2D, cudaGraphicsMapFlagsWriteDiscard));

    void* dev_ptr;
    size_t num_bytes;
    cudaCheckErrors(cudaGraphicsMapResources(1, &cuda_tex_resource, 0));
    cudaCheckErrors(cudaGraphicsSubResourceGetMappedArray((cudaArray**)&dev_ptr, cuda_tex_resource, 0, 0));

    cudaChannelFormatDesc channel_desc = cudaCreateChannelDesc<uchar4>();
    cudaMemcpyToArray((cudaArray*)dev_ptr, 0, 0, src_rgba.data,
        src.cols * src.rows * sizeof(uchar4),
        cudaMemcpyHostToDevice);

    cudaCheckErrors(cudaGraphicsUnmapResources(1, &cuda_tex_resource, 0));

    return gl_texture_id;
}
#else
GLuint CreateTextureCube(cv::Mat& src)
{
    if (src.empty()) {
        return 0;
    }

    cv::Mat src_rgba;
    if (src.channels() == 3) {
        cv::cvtColor(src, src_rgba, cv::COLOR_BGR2RGBA);
    } else if (src.channels() == 4) {
        src_rgba = src;
    } else {
        return 0;
    }

    GLuint gl_texture_id = 0;
    glGenTextures(1, &gl_texture_id);
    glBindTexture(GL_TEXTURE_2D, gl_texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, src.cols, src.rows, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, src_rgba.data);
    glBindTexture(GL_TEXTURE_2D, 0);
    return gl_texture_id;
}

GLuint UpdateTextureWithCuda(int, int)
{
    return 0;
}
#endif

void ViewController::run()
{
    int iw = 2048 / 2;
    int ih = 1536 / 2;
  initWindow (iw, ih, "glfw occt image ai");
  initViewer(iw,ih);
  initDemoScene();

#if CXCORE_ENABLE_VIEWCONTROLLER_CUDA
  InitializeTextureAndPBO(2048, 1536);
#endif
  Imgui_OpenCV_Ini0();
  initScriptCatalog();
  initManualStateTestConsole();
  initImageEvidenceLayer();
  m_semanticFlowGraph.Initialize(findRepositoryRoot().generic_string());
  mainloop();
  cleanup();
}

void ViewController::initImGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(myOcctWindow->getGlfwWindow(), true);
    ImGui_ImplOpenGL3_Init("#version 450");
}

void ViewController::initScriptCatalog()
{
  m_scriptCatalog.clear();
  m_selectedScript = -1;
  const fs::path root = findRepositoryRoot();
  if (root.empty())
  {
    m_scriptResult.status = "BLOCKED";
    m_scriptResult.reason = "cxparser/cxscript directory not found";
    m_scriptResult.runtime_fillback_status = "not_started";
    return;
  }

  const std::pair<const char*, const char*> scanRoots[] = {
    {"module", "cxparser/cxscript/module"},
    {"integration", "cxparser/cxscript/integration"}
  };
  for (const auto& scanRoot : scanRoots)
  {
    const fs::path directory = root / scanRoot.second;
    if (!fs::exists(directory)) continue;
    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(directory))
    {
      if (!entry.is_regular_file()) continue;
      const std::string extension = entry.path().extension().string();
      if (extension != ".cxs" && extension != ".cxsc") continue;
      ScriptCatalogEntry item;
      item.name = entry.path().filename().string();
      item.path = fs::relative(entry.path(), root).generic_string();
      item.type = scanRoot.first;
      item.status = "ready";
      const bool isDirectLike =
        item.name.find("direct_test") != std::string::npos ||
        item.name.find("_direct") != std::string::npos ||
        item.name.find("_smoke") != std::string::npos ||
        item.path.find("/headless/") != std::string::npos;
      item.description = isDirectLike
        ? "C/C++ statement-level direct CxScript case."
        : "CxScript " + item.type + " case.";
      m_scriptCatalog.push_back(item);
    }
  }
  std::sort(m_scriptCatalog.begin(), m_scriptCatalog.end(),
    [](const ScriptCatalogEntry& left, const ScriptCatalogEntry& right) { return left.path < right.path; });
  for (std::size_t i = 0; i < m_scriptCatalog.size(); ++i)
  {
    if (m_scriptCatalog[i].path == "cxparser/cxscript/module/cximage/headless/fastmatch_l1_direct.cxsc")
    {
      m_selectedScript = static_cast<int>(i);
      break;
    }
  }
  if (m_selectedScript < 0 && !m_scriptCatalog.empty()) m_selectedScript = 0;
  m_scriptResult.status = "PENDING";
  m_scriptResult.reason = "select a script and run it";
  m_scriptResult.runtime_fillback_status = "not_started";
}

bool ViewController::GetSelectedCatalogScript(std::string& outPath,
                                              std::string& outName) const
{
  outPath.clear();
  outName.clear();

  if (m_selectedScript < 0 ||
      m_selectedScript >= static_cast<int>(m_scriptCatalog.size()))
  {
    return false;
  }

  const ScriptCatalogEntry& entry =
    m_scriptCatalog[static_cast<std::size_t>(m_selectedScript)];

  outPath = entry.path;
  outName = entry.name;
  return !outPath.empty();
}

void ViewController::HandleSemanticFlowAction(const SemanticFlowAction& action)
{
  if (action.type == SemanticFlowActionType::None)
    return;

  if (action.type == SemanticFlowActionType::BindCatalogScriptToSelectedNode)
  {
    std::string scriptPath;
    std::string scriptName;

    if (!GetSelectedCatalogScript(scriptPath, scriptName))
    {
      m_scriptResult.source = "semantic_flow";
      m_scriptResult.status = "PENDING";
      m_scriptResult.reason = "no Script Catalog item selected";
      m_scriptResult.runtime_fillback_status = "semantic_bind_blocked";
      return;
    }

    std::string reason;
    if (!m_semanticFlowGraph.BindScriptToNode(action.node_index, scriptPath, scriptName, reason))
    {
      m_scriptResult.source = "semantic_flow";
      m_scriptResult.status = "PENDING";
      m_scriptResult.reason = reason;
      m_scriptResult.runtime_fillback_status = "semantic_bind_blocked";
      return;
    }

    m_scriptResult.source = "semantic_flow";
    m_scriptResult.status = "PENDING";
    m_scriptResult.script_path = scriptPath;
    m_scriptResult.reason = reason;
    m_scriptResult.runtime_fillback_status = "semantic_node_bound_from_catalog";
    return;
  }

  if (action.type == SemanticFlowActionType::LoadBoundScript)
  {
    std::string reason;
    const bool loaded = LoadBoundStateToManualConsole(
      action.node_id, action.script_path, reason);

    m_scriptResult.source = "semantic_flow";
    m_scriptResult.status = loaded ? "PENDING" : "FAIL";
    m_scriptResult.script_path = action.script_path;
    m_scriptResult.reason = reason;
    m_scriptResult.runtime_fillback_status = loaded
      ? "semantic_node_loaded"
      : "semantic_node_load_failed";
    return;
  }

  if (action.type == SemanticFlowActionType::RunBoundScript)
  {
    std::string loadReason;
    if (!LoadBoundStateToManualConsole(
          action.node_id, action.script_path, loadReason))
    {
      m_scriptResult.source = "semantic_flow";
      m_scriptResult.status = "FAIL";
      m_scriptResult.script_path = action.script_path;
      m_scriptResult.reason = loadReason;
      m_scriptResult.runtime_fillback_status = "semantic_node_load_failed";
      return;
    }

    m_scriptResult = RunCxScript(action.script_path);
    RefreshRuntimeObjectTable("Flow Run",
      m_scriptResult.status == "BLOCKED" ? "BLOCKED" : "runtime_executed");
    m_semanticFlowGraph.ApplyScriptResult(action.node_index,
                                          m_scriptResult.status,
                                          m_scriptResult.result_ref,
                                          m_scriptResult.evidence_ref,
                                          m_scriptResult.issue_entry_ref,
                                          m_scriptResult.reason);
    m_scriptResult.runtime_fillback_status = "semantic_bound_script_run_requested";
    return;
  }
}
ViewController::ScriptResult ViewController::RunCxScript(const std::string& theScriptPath)
{
  ScriptResult result;
  result.source = "file";
  result.script_path = theScriptPath;
  result.runtime_fillback_status = "pending_real_runtime_fillback";
  if (theScriptPath.empty())
  {
    result.status = "PENDING";
    result.reason = "no script selected";
    result.runtime_fillback_status = "not_started";
    return result;
  }
  const fs::path root = findRepositoryRoot();
  const fs::path scriptPath = root / fs::path(theScriptPath);
  if (root.empty() || !fs::exists(scriptPath))
  {
    result.status = "FAIL";
    result.reason = "script file not found";
    result.log_lines.push_back("RunCxScript rejected a missing script path.");
    return result;
  }
  const fs::path moduleRoot = root / "cxparser" / "cxscript" / "module";
  const fs::path integrationRoot = root / "cxparser" / "cxscript" / "integration";
  if (!pathIsWithin(scriptPath, moduleRoot) && !pathIsWithin(scriptPath, integrationRoot))
  {
    result.status = "BLOCKED";
    result.reason = "script path is outside the allowed cxscript runtime roots";
    result.log_lines.push_back("rag_script_cases is semantic_reference_only.");
    return result;
  }
  std::ifstream stream(scriptPath);
  const std::string scriptText((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
  if (scriptText.find("dev_analysis_gui_shell.exe") != std::string::npos)
  {
    const fs::path localShell = root / "cxparser" / "build" / "Release" / "dev_analysis_gui_shell.exe";
    const fs::path workspaceShell = root.parent_path().parent_path() /
      "cxparser" / "build" / "Release" / "dev_analysis_gui_shell.exe";
    if (!fs::exists(localShell) && !fs::exists(workspaceShell))
    {
      result.status = "BLOCKED";
      result.reason = "dev_analysis_gui_shell.exe missing";
      result.log_lines.push_back("External GUI capture dependency is unavailable.");
      result.log_lines.push_back("No runtime result package was produced.");
      return result;
    }
  }
  // Semantic Flow and Manual Console must execute with the same external
  // input snapshot.  The editor Run button already performs this binding;
  // bind it here as well for "Run Bound Script".
  for (const auto& input : m_manualTest.runtime_int_vars)
  {
    if (input.first.rfind("global_", 0) == 0)
      m_parserDebugBridge.SetGlobalInt(input.first, input.second);
  }
  bool imageBound = true;
  if (!m_imageViewImage.empty())
    imageBound = m_parserDebugBridge.SetGlobalMatInput(m_imageViewImage);
  else if (!s_img0.empty())
    imageBound = m_parserDebugBridge.SetGlobalMatInput(s_img0);

  const bool ran = imageBound && m_parserDebugBridge.RunScript(scriptText);
  result.status = ran ? "PENDING" : "BLOCKED";
  result.reason = ran ?
    "parser runtime executed; runtime objects require query; no PASS inferred" :
    (imageBound ? "parser runtime rejected script" :
                  "no Image View/default image available for global_matInput");
  result.runtime_fillback_status = ran ? "runtime_objects_queried" : "not_started";
  result.log_lines.push_back(ran ?
    "Script executed through ParserDebugBridge." :
    "ParserDebugBridge execution failed.");
  return result;
}

static bool SyncRuntimeObjectToManualGaugeState(
    ManualTestContext& context,
    const RuntimeObjectView& object)
{
    ManualGaugeState& gauge = context.current_gauge;

    if (object.type == "Findline" && object.has_line_roi)
    {
        gauge.tool = "Findline";
        gauge.source = "runtime_object";
        gauge.review_status = "editing";

        gauge.has_line_gauge = true;
        gauge.has_circle_gauge = false;

        gauge.line_x0 = static_cast<int>(std::round(object.line_x0));
        gauge.line_y0 = static_cast<int>(std::round(object.line_y0));
        gauge.line_x1 = static_cast<int>(std::round(object.line_x1));
        gauge.line_y1 = static_cast<int>(std::round(object.line_y1));

        int half_width = static_cast<int>(std::round(object.effective_tool_half_width));
        if (half_width <= 0)
            half_width = static_cast<int>(std::round(object.requested_tool_half_width));
        if (half_width <= 0)
            half_width = static_cast<int>(std::round(object.line_scan_half_width));
        if (half_width <= 0)
            half_width = 20;

        gauge.tool_half_width = half_width;
        gauge.wgap = object.line_tool_wgap;
        gauge.hgap = object.line_tool_hgap;
        gauge.linegap = object.linegap;

        gauge.dirty = false;
        gauge.accepted = false;
        return true;
    }

    if (object.type == "Findcircle" && object.has_circle)
    {
        gauge.tool = "Findcircle";
        gauge.source = "runtime_object";
        gauge.review_status = "editing";

        gauge.has_circle_gauge = true;
        gauge.has_line_gauge = false;

        gauge.circle_cx = static_cast<int>(std::round(object.circle_cx));
        gauge.circle_cy = static_cast<int>(std::round(object.circle_cy));

        const int px = static_cast<int>(std::round(object.circle_px));
        const int py = static_cast<int>(std::round(object.circle_py));
        int radius = static_cast<int>(std::lround(std::hypot(
            static_cast<double>(px - gauge.circle_cx),
            static_cast<double>(py - gauge.circle_cy))));
        if (radius <= 0)
            radius = 50;

        gauge.radius = radius;
        gauge.circle_px = px;
        gauge.circle_py = py;

        gauge.dirty = false;
        gauge.accepted = false;
        return true;
    }

    return false;
}

void ViewController::drawScriptAcceptancePanels()
{
  ImGuiIO& panelIo = ImGui::GetIO();
  const ImGuiCond layoutCondition = m_detachablePanels ? ImGuiCond_FirstUseEver : ImGuiCond_Always;
  const ImGuiWindowFlags panelFlags = m_detachablePanels ? ImGuiWindowFlags_None : ImGuiWindowFlags_NoMove;
  const ImVec2 displaySize = panelIo.DisplaySize;
  const float margin = 4.0f;
  const float contentHeight = std::max(300.0f, displaySize.y - margin * 2.0f);
  const float leftWidth = std::max(260.0f, displaySize.x * 0.28f);
  const float rightWidth = std::max(300.0f, displaySize.x * 0.30f);
  const float middleWidth = std::max(280.0f, displaySize.x - leftWidth - rightWidth - margin * 4.0f);
  const float catalogHeight = contentHeight * 0.55f;
  const float runHeight = 155.0f;
  const float middleX = margin * 2.0f + leftWidth;
  const float rightX = middleX + middleWidth + margin;

  ImGui::SetNextWindowPos(ImVec2(margin, margin), layoutCondition);
  ImGui::SetNextWindowSize(ImVec2(leftWidth, catalogHeight), layoutCondition);
  ImGui::Begin("Evidence Chain UI", nullptr, panelFlags);
  ImGui::TextUnformatted("证据链 UI");
  ImGui::TextDisabled("(图像集 / case / evidence / review)");
  ImGui::Separator();

  const char* sectionLabels[] = {
    "Image Set",
    "Case",
    "Evidence",
    "Review"
  };
  const char* sectionHints[] = {
    "图像集",
    "case",
    "evidence",
    "review"
  };
  const char* sectionIcons[] = {
    "[IMG]",
    "[DOC]",
    "[OK]",
    "[USR]"
  };
  const float buttonWidth = std::max(54.0f, (ImGui::GetContentRegionAvail().x - 18.0f) / 4.0f);
  for (int i = 0; i < 4; ++i)
  {
    if (i > 0) ImGui::SameLine();
    ImGui::PushID(i);
    if (m_evidenceChainUiSection == i)
      ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(70, 130, 190, 255));
    if (ImGui::Button((std::string(sectionIcons[i]) + "\n" + sectionLabels[i]).c_str(),
                      ImVec2(buttonWidth, 48.0f)))
    {
      m_evidenceChainUiSection = i;
      if (i == 2)
      {
        RebuildScriptEvidenceGroups();
        EnsureEvidenceChainThumbnailsLoaded();
      }
    }
    if (m_evidenceChainUiSection == i)
      ImGui::PopStyleColor();
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("%s", sectionHints[i]);
    ImGui::PopID();
  }

  ImGui::Separator();
  if (m_evidenceChainUiSection == 0)
  {
    ImGui::TextUnformatted("Image Set / 图像集");
    ImGui::Text("manifest: %s", m_manualTest.manifest_loaded ? "loaded" : "not loaded");
    ImGui::TextWrapped("%s", m_manualTest.manifest_path.empty() ? "(no manifest path)" : m_manualTest.manifest_path.c_str());
    if (!m_manualTest.image_manifest_items.empty())
    {
      ImGui::BeginChild("evidence_image_set_list", ImVec2(-1, 0), true);
      for (std::size_t i = 0; i < m_manualTest.image_manifest_items.size(); ++i)
      {
        const ManifestImageItem& item = m_manualTest.image_manifest_items[i];
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::Selectable(item.image_id.c_str(), m_manualTest.active_image_id == item.image_id))
        {
          m_manualTest.active_image_id = item.image_id;
          m_manualTest.image_file_path = item.image_path;
        }
        ImGui::TextDisabled("%s | %s", item.level.c_str(), item.status.c_str());
        ImGui::PopID();
      }
      ImGui::EndChild();
    }
    else
    {
      ImGui::TextDisabled("No image manifest entries loaded.");
    }
  }
  else if (m_evidenceChainUiSection == 1)
  {
    ImGui::TextUnformatted("Case / 用例");
    DrawEvidenceCaseListPanel(m_manualTest);
  }
  else if (m_evidenceChainUiSection == 2)
  {
    ImGui::TextUnformatted("Evidence / 脚本 + 参数 + 图片");
    EnsureCxScriptWorkbenchAssetsLoaded();
    DrawScriptEvidenceThumbnailRailByGroup();
    ImGui::Separator();
    ImGui::Checkbox("Show all catalog scripts", &m_showAllScripts);
    if (ImGui::CollapsingHeader("CxScript Templates", ImGuiTreeNodeFlags_DefaultOpen))
    {
      for (std::size_t i = 0; i < m_scriptCatalog.size(); ++i)
      {
        const ScriptCatalogEntry& item = m_scriptCatalog[i];
        const bool isDirectLike =
          item.name.find("direct_test") != std::string::npos ||
          item.name.find("_direct") != std::string::npos ||
          item.name.find("_smoke") != std::string::npos ||
          item.path.find("/headless/") != std::string::npos;
        if (!m_showAllScripts && !isDirectLike) continue;
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::Selectable(item.name.c_str(), m_selectedScript == static_cast<int>(i)))
        {
          m_selectedScript = static_cast<int>(i);
          m_manualTest.loaded_script_path = item.path;
        }
        ImGui::TextWrapped("path: %s", item.path.c_str());
        ImGui::Text("type: %s | status: %s", item.type.c_str(), item.status.c_str());
        ImGui::TextWrapped("description: %s", item.description.c_str());
        ImGui::Separator();
        ImGui::PopID();
      }
      if (m_scriptCatalog.empty()) ImGui::TextDisabled("No runnable scripts found.");
    }
  }
  else
  {
    ImGui::TextUnformatted("Review / 人工复核");
    ImGui::Text("active_case: %s", m_manualTest.active_case_id.empty() ? "(none)" : m_manualTest.active_case_id.c_str());
    ImGui::Text("active_image: %s", m_manualTest.active_image_id.empty() ? "(none)" : m_manualTest.active_image_id.c_str());
    ImGui::Text("active_target: %s", m_manualTest.active_target_id.empty() ? "(none)" : m_manualTest.active_target_id.c_str());
    ImGui::Text("gauge_review: %s", m_manualTest.current_gauge.review_status.c_str());
    if (ImGui::Button("Mark Review Accepted"))
    {
      m_manualTest.current_gauge.review_status = "manual_accepted";
      m_manualTest.current_gauge.accepted = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Mark Review Rejected"))
    {
      m_manualTest.current_gauge.review_status = "manual_rejected";
      m_manualTest.current_gauge.accepted = false;
    }
  }
  ImGui::End();

  ImGui::SetNextWindowPos(ImVec2(margin, margin + catalogHeight + margin), layoutCondition);
  ImGui::SetNextWindowSize(ImVec2(leftWidth, contentHeight - catalogHeight - margin), layoutCondition);
  ImGui::Begin("Capability Status", nullptr, panelFlags);
  ImGui::Text("cxscript:");
  ImGui::BulletText("ScriptCatalog: %s", m_scriptCatalog.empty() ? "blocked" : "available");
  ImGui::BulletText("RunCxScript: pending");
  ImGui::Text("cximage:");
  ImGui::BulletText("ImageView: available");
  ImGui::BulletText("OverlayView: pending");
  ImGui::Text("mlpack:");
  ImGui::BulletText("observe_handoff: pending");
  ImGui::Text("ensmallen:");
  ImGui::BulletText("semantic_lifecycle: listed_only / not_executable");
  ImGui::Text("torch:");
  ImGui::BulletText("listed_only / not_executable / pending");
  ImGui::Text("rag_script_cases:");
  ImGui::BulletText("semantic_reference_only / not_runtime_target");
  ImGui::End();

  ImGui::SetNextWindowPos(ImVec2(middleX, margin), layoutCondition);
  ImGui::SetNextWindowSize(ImVec2(middleWidth, contentHeight), layoutCondition);
  ImGui::Begin("Image View", nullptr, panelFlags);
  const bool parserImage = m_scriptResult.image_ref.rfind("runtime_object:", 0) == 0;
  const bool baseImageValid = m_imageViewTexture != 0 && !m_imageViewImage.empty();
  const char* baseImageSource = parserImage ? "parser_image" :
    (baseImageValid ? "view_image" : "none");
  ImGui::Text("Layer 0 Base Image");
  ImGui::Text("source: %s | name: %s | %s", baseImageSource,
    parserImage ? m_scriptResult.image_ref.c_str() :
      (baseImageValid ? m_manualTest.image_file_path.c_str() : "(none)"),
    baseImageValid ? "valid" : "invalid");
  int runtimeVisualCount = 0;
  std::string runtimeVisualNames;

  if (ImGui::Button("Reset view"))
  {
    m_imageViewZoom = 1.0f;
    m_imageViewPanX = 0.0f;
    m_imageViewPanY = 0.0f;
  }
  ImGui::SameLine();
  ImGui::Text("Zoom: %.0f%%  Move: middle-drag  Zoom: wheel", m_imageViewZoom * 100.0f);

  const ImVec2 canvasSize(
    std::max(120.0f, ImGui::GetContentRegionAvail().x),
    std::max(160.0f, ImGui::GetContentRegionAvail().y - ImGui::GetTextLineHeightWithSpacing() * 3.0f));
  ImGui::InvisibleButton("image_view_canvas", canvasSize,
                         ImGuiButtonFlags_MouseButtonLeft |
                         ImGuiButtonFlags_MouseButtonMiddle |
                         ImGuiButtonFlags_MouseButtonRight);
  ImGuiIO& io = ImGui::GetIO();
  const bool canvasHovered = ImGui::IsItemHovered();
  const bool canvasActive = ImGui::IsItemActive();
  const ImVec2 canvasMin = ImGui::GetItemRectMin();
  const ImVec2 canvasMax = ImGui::GetItemRectMax();
  ImDrawList* drawList = ImGui::GetWindowDrawList();
  drawList->AddRectFilled(canvasMin, canvasMax, IM_COL32(18, 18, 18, 255));
  drawList->AddRect(canvasMin, canvasMax, IM_COL32(90, 90, 90, 255));

  if (canvasHovered && io.MouseWheel != 0.0f && baseImageValid)
  {
    const float oldZoom = m_imageViewZoom;
    const float newZoom = std::clamp(oldZoom * (1.0f + io.MouseWheel * 0.1f), 0.05f, 20.0f);
    if (std::fabs(newZoom - oldZoom) > 0.0001f)
    {
      const float localX = io.MousePos.x - canvasMin.x - m_imageViewPanX;
      const float localY = io.MousePos.y - canvasMin.y - m_imageViewPanY;
      const float imageXAtMouse = localX / oldZoom;
      const float imageYAtMouse = localY / oldZoom;
      m_imageViewZoom = newZoom;
      m_imageViewPanX = (io.MousePos.x - canvasMin.x) - imageXAtMouse * newZoom;
      m_imageViewPanY = (io.MousePos.y - canvasMin.y) - imageYAtMouse * newZoom;
    }
  }
  if (canvasHovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
  {
    const ImVec2 delta = io.MouseDelta;
    m_imageViewPanX += delta.x;
    m_imageViewPanY += delta.y;
  }

  const ImVec2 imageSize = baseImageValid
      ? ImVec2(static_cast<float>(m_imageViewImage.cols) * m_imageViewZoom,
               static_cast<float>(m_imageViewImage.rows) * m_imageViewZoom)
      : ImVec2(0.0f, 0.0f);
  const ImVec2 imagePos(canvasMin.x + m_imageViewPanX,
                        canvasMin.y + m_imageViewPanY);
  const ImVec2 imageEnd(imagePos.x + imageSize.x,
                        imagePos.y + imageSize.y);
  m_annotationImagePosX = imagePos.x;
  m_annotationImagePosY = imagePos.y;
  m_annotationImageWidth = std::max(1.0f, imageSize.x);
  m_annotationImageHeight = std::max(1.0f, imageSize.y);
  drawList->PushClipRect(canvasMin, canvasMax, true);
  if (baseImageValid)
  {
    drawList->AddImage((ImTextureID)(uint64_t)m_imageViewTexture,
                       imagePos,
                       imageEnd,
                       ImVec2(0, 0),
                       ImVec2(1, 1));
  }
  else
  {
    drawList->AddText(ImVec2(canvasMin.x + 8.0f, canvasMin.y + 8.0f),
                      IM_COL32(200, 200, 200, 255),
                      "Image View: no image loaded");
  }
  const float sx = m_imageViewZoom;
  const float sy = m_imageViewZoom;

        auto ImageToScreen = [&](float x, float y) -> ImVec2
        {
            return ImVec2(imagePos.x + x * sx, imagePos.y + y * sy);
        };

        auto DrawImagePolylineClosed = [&](const std::vector<float>& xy, ImU32 color, float thickness)
        {
            const std::size_t count = xy.size() / 2;
            if (count < 2)
                return;

            for (std::size_t i = 0; i < count; ++i)
            {
                const std::size_t j = (i + 1) % count;
                drawList->AddLine(
                    ImageToScreen(xy[i * 2], xy[i * 2 + 1]),
                    ImageToScreen(xy[j * 2], xy[j * 2 + 1]),
                    color,
                    thickness);
            }
        };


        // Layer 2: Manual Element / Annotation Tool Layer.

        // Manual tools are not runtime result.
        drawImageEvidenceOnCanvas(canvasHovered, canvasActive, drawList);

        // Sync Runtime Object to ManualGaugeState if not already set
        if (!m_manualTest.current_gauge.has_line_gauge &&
            !m_manualTest.current_gauge.has_circle_gauge &&
            !m_manualTest.current_gauge.dirty)
        {
            const std::string activeName = m_manualTest.current_result_ref.source_object;

            if (!activeName.empty())
            {
                for (const RuntimeObjectView& object : m_manualTest.runtime_objects)
                {
                    if (object.name == activeName &&
                        object.visualizable &&
                        !object.stale &&
                        SyncRuntimeObjectToManualGaugeState(m_manualTest, object))
                    {
                        break;
                    }
                }
            }
            else
            {
                for (const RuntimeObjectView& object : m_manualTest.runtime_objects)
                {
                    if (!object.visualizable || object.stale)
                        continue;

                    if (SyncRuntimeObjectToManualGaugeState(m_manualTest, object))
                        break;
                }
            }
        }

        // Layer 3: Shape Elements from ImageAnnotationLayer
        {
            std::vector<const CxShapeElement*> shapeElements;
            m_annotationLayer.EnumerateVisibleShapes(shapeElements);

            for (const CxShapeElement* element : shapeElements)
            {
                DrawShapeElementOnImageView(*element, drawList);
            }
        }

    if (m_showTestPoints)
    {
      const ImU32 color = IM_COL32(255, 64, 64, 255);
      const ImVec2 points[] = {
        imagePos,
        ImVec2(imageEnd.x, imagePos.y),
        ImVec2(imagePos.x, imageEnd.y),
        imageEnd,
        ImVec2((imagePos.x + imageEnd.x) * 0.5f,
               (imagePos.y + imageEnd.y) * 0.5f)
      };
      for (const ImVec2& point : points)
      {
        drawList->AddLine(ImVec2(point.x - 6.0f, point.y),
                          ImVec2(point.x + 6.0f, point.y), color, 2.0f);
        drawList->AddLine(ImVec2(point.x, point.y - 6.0f),
                          ImVec2(point.x, point.y + 6.0f), color, 2.0f);
      }
    }
    if (m_showTestRectangle)
    {
      drawList->AddRect(
        ImVec2(imagePos.x + imageSize.x * 0.2f, imagePos.y + imageSize.y * 0.2f),
        ImVec2(imagePos.x + imageSize.x * 0.7f, imagePos.y + imageSize.y * 0.7f),
        IM_COL32(64, 255, 64, 255), 0.0f, 0, 2.0f);
    }
    if (m_showTestScanLine)
    {
      const float y = imagePos.y + imageSize.y * 0.5f;
      drawList->AddLine(ImVec2(imagePos.x, y), ImVec2(imageEnd.x, y),
                        IM_COL32(64, 160, 255, 255), 2.0f);
    }
    drawList->PopClipRect();

    const bool mouseOnImage = canvasHovered &&
      io.MousePos.x >= imagePos.x && io.MousePos.x < imageEnd.x &&
      io.MousePos.y >= imagePos.y && io.MousePos.y < imageEnd.y;
    if (mouseOnImage)
    {
      const int imageX = static_cast<int>(
        (io.MousePos.x - imagePos.x) * m_imageViewImage.cols / imageSize.x);
      const int imageY = static_cast<int>(
        (io.MousePos.y - imagePos.y) * m_imageViewImage.rows / imageSize.y);
      ImGui::SetTooltip("image coordinate: %d, %d", imageX, imageY);
    }
  ImGui::TextWrapped("image_ref: %s", emptyAsNone(m_scriptResult.image_ref));
  ImGui::TextWrapped("overlay_ref: %s", emptyAsNone(m_scriptResult.overlay_ref));
  ImGui::Text("result_attach_status: %s", m_scriptResult.result_ref.empty() ? "not_attached" : "attached");
  ImGui::End();

  ImGui::SetNextWindowPos(ImVec2(rightX, margin), layoutCondition);
  ImGui::SetNextWindowSize(ImVec2(rightWidth, runHeight), layoutCondition);
  ImGui::Begin("Run Control", nullptr, panelFlags);
  ImGui::Text("Options");
  ImGui::Checkbox("Detachable panels (inside host)", &m_detachablePanels);
  ImGui::SameLine();
  ImGui::Checkbox("Manual State Test Console", &m_showManualStateTestConsole);
  ImGui::SameLine();
  ImGui::Checkbox("Show legacy GPU work (debug)", &m_showLegacyGpuWork);
  if (m_selectedScript >= 0 && m_selectedScript < static_cast<int>(m_scriptCatalog.size()))
    ImGui::TextWrapped("Selected: %s", m_scriptCatalog[m_selectedScript].path.c_str());
  else ImGui::TextDisabled("Selected: none");
  if (ImGui::Button("Run Selected Script") && m_selectedScript >= 0)
  {
    m_scriptRunRequested = true;
    m_scriptResult = RunCxScript(m_scriptCatalog[m_selectedScript].path);
    RefreshRuntimeObjectTable("Run Selected Script",
      m_scriptResult.status == "BLOCKED" ? "BLOCKED" : "runtime_executed");
    m_scriptRunRequested = false;
  }
  ImGui::SameLine();
  if (ImGui::Button("Stop"))
  {
    m_scriptRunRequested = false;
    m_scriptResult.status = "PENDING";
    m_scriptResult.reason = "run stopped by user";
    m_scriptResult.runtime_fillback_status = "stopped";
    m_scriptResult.log_lines.push_back("Stop requested.");
  }
  ImGui::SameLine();
  if (ImGui::Button("Clear Result"))
  {
    m_scriptResult = ScriptResult();
    m_scriptResult.status = "PENDING";
    m_scriptResult.reason = "result cleared";
    m_scriptResult.runtime_fillback_status = "not_started";
  }
  ImGui::Text("Run state: %s", m_scriptRunRequested ? "running" : "idle");
  ImGui::End();

  ImGui::SetNextWindowPos(ImVec2(rightX, margin + runHeight + margin), layoutCondition);
  ImGui::SetNextWindowSize(ImVec2(rightWidth, contentHeight * 0.25f), layoutCondition);
  ImGui::Begin("Result / Log", nullptr, panelFlags);
  ImGui::TextWrapped("Source: %s", emptyAsNone(m_scriptResult.source));
  ImGui::TextWrapped("Script: %s", emptyAsNone(m_scriptResult.script_path));
  ImGui::Text("Status: %s", emptyAsNone(m_scriptResult.status));
  ImGui::TextWrapped("Reason: %s", emptyAsNone(m_scriptResult.reason));
  ImGui::TextWrapped("runtime_fillback_status: %s", emptyAsNone(m_scriptResult.runtime_fillback_status));
  ImGui::Text("elapsed_ms: %.3f", m_scriptResult.elapsed_ms);
  ImGui::TextWrapped("image_ref: %s", emptyAsNone(m_scriptResult.image_ref));
  ImGui::TextWrapped("overlay_ref: %s", emptyAsNone(m_scriptResult.overlay_ref));
  ImGui::TextWrapped("result_ref: %s", emptyAsNone(m_scriptResult.result_ref));
  ImGui::TextWrapped("evidence_ref: %s", emptyAsNone(m_scriptResult.evidence_ref));
  ImGui::TextWrapped("issue_entry_ref: %s", emptyAsNone(m_scriptResult.issue_entry_ref));
  ImGui::Separator();
  ImGui::Text("Log:");
  for (const std::string& line : m_scriptResult.log_lines) ImGui::BulletText("%s", line.c_str());
  ImGui::End();

  ImGui::SetNextWindowPos(ImVec2(rightX, margin + runHeight + margin + contentHeight * 0.25f + margin), layoutCondition);
  ImGui::SetNextWindowSize(ImVec2(rightWidth, contentHeight * 0.45f), layoutCondition);
  ImGui::Begin("Gauge Workbench", nullptr, panelFlags);

  ManualGaugeState& gauge = m_manualTest.current_gauge;

  ImGui::Text("=== Current Evidence ===");
  ImGui::Text("case_id: %s", emptyAsNone(gauge.case_id));
  ImGui::Text("image_id: %s", emptyAsNone(gauge.image_id));
  ImGui::Text("target_id: %s", emptyAsNone(gauge.target_id));
  ImGui::Text("tool: %s", emptyAsNone(gauge.tool));
  ImGui::Text("source: %s", emptyAsNone(gauge.source));
  ImGui::Text("review_status: %s", emptyAsNone(gauge.review_status));
  ImGui::Text("dirty: %s | accepted: %s", gauge.dirty ? "yes" : "no", gauge.accepted ? "yes" : "no");
  ImGui::Separator();

  ImGui::Text("=== Current Gauge ===");
  if (gauge.has_line_gauge)
  {
    if (ImGui::InputInt("line_x0", &gauge.line_x0)) { gauge.dirty = true; gauge.review_status = "editing"; }
    if (ImGui::InputInt("line_y0", &gauge.line_y0)) { gauge.dirty = true; gauge.review_status = "editing"; }
    if (ImGui::InputInt("line_x1", &gauge.line_x1)) { gauge.dirty = true; gauge.review_status = "editing"; }
    if (ImGui::InputInt("line_y1", &gauge.line_y1)) { gauge.dirty = true; gauge.review_status = "editing"; }
    if (ImGui::InputInt("tool_half_width", &gauge.tool_half_width)) { gauge.dirty = true; gauge.review_status = "editing"; }
    if (ImGui::InputInt("wgap", &gauge.wgap)) { gauge.dirty = true; gauge.review_status = "editing"; }
    if (ImGui::InputInt("hgap", &gauge.hgap)) { gauge.dirty = true; gauge.review_status = "editing"; }
    if (ImGui::InputInt("linegap", &gauge.linegap)) { gauge.dirty = true; gauge.review_status = "editing"; }
    if (ImGui::InputInt("threshold", &gauge.threshold)) { gauge.dirty = true; gauge.review_status = "editing"; }
    if (ImGui::InputInt("filterprofile", &gauge.filterprofile)) { gauge.dirty = true; gauge.review_status = "editing"; }
    if (ImGui::InputInt("method", &gauge.method)) { gauge.dirty = true; gauge.review_status = "editing"; }
  }
  else if (gauge.has_circle_gauge)
  {
    if (ImGui::InputInt("circle_cx", &gauge.circle_cx)) { gauge.dirty = true; gauge.review_status = "editing"; }
    if (ImGui::InputInt("circle_cy", &gauge.circle_cy)) { gauge.dirty = true; gauge.review_status = "editing"; }
    if (ImGui::InputInt("circle_px", &gauge.circle_px)) { gauge.dirty = true; gauge.review_status = "editing"; }
    if (ImGui::InputInt("circle_py", &gauge.circle_py)) { gauge.dirty = true; gauge.review_status = "editing"; }
    if (ImGui::InputInt("radius", &gauge.radius)) { gauge.dirty = true; gauge.review_status = "editing"; }
    if (ImGui::InputInt("inner_radius", &gauge.inner_radius)) { gauge.dirty = true; gauge.review_status = "editing"; }
    if (ImGui::InputInt("outer_radius", &gauge.outer_radius)) { gauge.dirty = true; gauge.review_status = "editing"; }
    if (ImGui::InputInt("gap", &gauge.gap)) { gauge.dirty = true; gauge.review_status = "editing"; }
    if (ImGui::InputInt("linegap", &gauge.linegap)) { gauge.dirty = true; gauge.review_status = "editing"; }
    if (ImGui::InputInt("threshold", &gauge.threshold)) { gauge.dirty = true; gauge.review_status = "editing"; }
    if (ImGui::InputInt("method", &gauge.method)) { gauge.dirty = true; gauge.review_status = "editing"; }
  }
  else
  {
    ImGui::TextDisabled("No gauge active. Run a script or select from catalog.");
  }
  ImGui::Separator();

  ImGui::Text("=== Actions ===");
  if (ImGui::Button("Apply Gauge To Globals"))
  {
    ApplyManualGaugeToGlobals(m_manualTest);
  }
  ImGui::SameLine();
  if (ImGui::Button("Accept Manual Gauge"))
  {
    std::string reason;
    if (ValidateManualGaugeGeometryForEditing(gauge, reason))
    {
      NormalizeManualGaugeGeometry(gauge);
      gauge.accepted = true;
      gauge.dirty = false;
      gauge.review_status = "manual_accepted";
      m_manualTest.debug_status = "gauge_accepted";
      m_manualTest.debug_reason.clear();
    }
    else
    {
      gauge.accepted = false;
      m_manualTest.debug_status = "gauge_accept_failed";
      m_manualTest.debug_reason = reason;
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Save Gauge Annotation"))
  {
    std::string path;
    std::string reason;
    if (SaveManualGaugeAnnotation(m_manualTest, "", "", path, reason))
    {
      m_manualTest.debug_status = "gauge_annotation_saved";
      m_manualTest.debug_reason = path;
    }
    else
    {
      m_manualTest.debug_status = "gauge_annotation_save_failed";
      m_manualTest.debug_reason = reason;
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Reset From Runtime"))
  {
    gauge.has_line_gauge = false;
    gauge.has_circle_gauge = false;
    gauge.dirty = false;
    gauge.accepted = false;
  }
  ImGui::Separator();

  ImGui::Text("=== Param Regression ===");
  if (!gauge.accepted)
  {
    ImGui::TextDisabled("Parameter regression disabled: manual gauge not accepted.");
  }
  else
  {
    ImGui::Text("ready for candidate generation");
  }
  ImGui::Button("Generate Candidates (pending_binding)");
  ImGui::SameLine();
  ImGui::Button("Run Selected Probe (pending_binding)");
  ImGui::SameLine();
  ImGui::Button("Export Candidate (pending_binding)");

  ImGui::End();
}
void ViewController::initWindow (int theWidth, int theHeight, const char* theTitle)
{
  glfwSetErrorCallback (ViewController::errorCallback);
  glfwInit();
  const bool toAskCoreProfile = true;
  if (toAskCoreProfile)
  {
    glfwWindowHint (GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint (GLFW_CONTEXT_VERSION_MINOR, 3);
#if defined (__APPLE__)
    glfwWindowHint (GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    glfwWindowHint (GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  }

  myOcctWindow = new Window (theWidth, theHeight, theTitle);
  glfwSetWindowUserPointer       (myOcctWindow->getGlfwWindow(), this);
  glfwSetWindowSizeCallback      (myOcctWindow->getGlfwWindow(), ViewController::onResizeCallback);

  glfwSetFramebufferSizeCallback (myOcctWindow->getGlfwWindow(), ViewController::onFBResizeCallback);

  glfwSetScrollCallback          (myOcctWindow->getGlfwWindow(), ViewController::onMouseScrollCallback);

  glfwSetMouseButtonCallback     (myOcctWindow->getGlfwWindow(), ViewController::onMouseButtonCallback);

  glfwSetCursorPosCallback       (myOcctWindow->getGlfwWindow(), ViewController::onMouseMoveCallback);

  glfwMakeContextCurrent(myOcctWindow->getGlfwWindow());
  glfwSwapInterval(1);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
      std::cerr << "Failed to initialize GLAD" << std::endl;
      return ;
  }

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO(); (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

  ImGui::StyleColorsDark();
  ImGui_ImplGlfw_InitForOpenGL(myOcctWindow->getGlfwWindow(), true);

  const char* glsl_version = "#version 450";
  ImGui_ImplOpenGL3_Init(glsl_version);

  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
  (void)clear_color;

}

double ViewController::GetScale()
{
    if (!m_myView.IsNull())
    {
        m_myView->FitAll(0.1, false);
        return m_myView->Scale();
    }
    else
    {
        return 1;
    }
}

void ViewController::FitAll()
{
    try
    {
        if (!m_myView.IsNull())
        {
            m_myView->FitAll(0.1);
            m_myView->ZFitAll();
            m_myView->Update();

        }
    }
    catch (const Standard_Failure&)
    {
        return;
    }
};

void ViewController::ViewerUpDate()
{
}

void ViewController::DisplayShape(const Handle(AIS_InteractiveObject)& AShape, Standard_Boolean isShow)
{
   (void)AShape;
   (void)isShow;
}

void ViewController::RemoveAllShapes(Standard_Boolean isUpDate)
{
    (void)isUpDate;
}

#include <Graphic3d_Texture2Dmanual.hxx>
#include <Image_PixMap.hxx>
unsigned int  ViewController::CreateTextureFromMat0(const cv::Mat& mat)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    glBindTexture(GL_TEXTURE_2D, textureID);

    //
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    cv::Mat img;
    if (mat.channels() == 3)
        cv::cvtColor(mat, img, cv::COLOR_BGR2RGB);
    else
        img = mat;

    GLenum format = (img.channels() == 1) ? GL_RED : (img.channels() == 3) ? GL_RGB : GL_RGBA;

    glTexImage2D(GL_TEXTURE_2D, 0, format, img.cols, img.rows, 0, format, GL_UNSIGNED_BYTE, img.data);
    glGenerateMipmap(GL_TEXTURE_2D);

    return textureID;
}

void ViewController::UpdateImageViewImage(const cv::Mat& image)
{
    if (image.empty()) return;
    if (!m_imageViewImage.empty() && m_imageViewImage.size() != image.size())
    {
        m_annotationLayer.Clear();
        m_annotationStatus = "image size changed; annotation refs cleared";
    }
    image.copyTo(m_imageViewImage);
    if (!m_parserDebugBridge.SetGlobalMatInput(m_imageViewImage))
    {
        m_manualTest.debug_status = "image_bind_failed";
        m_manualTest.debug_reason =
            "Image View loaded, but global_matInput binding failed";
    }
    if (m_imageViewTexture != 0)
    {
        glDeleteTextures(1, &m_imageViewTexture);
        m_imageViewTexture = 0;
    }
    m_imageViewTexture = CreateTextureFromMat0(m_imageViewImage);
}
#include <Prs3d_ShadingAspect.hxx>
#include <Prs3d_Drawer.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <Geom_Surface.hxx>
#include <BRep_Tool.hxx>
#include <gp_Pln.hxx>
#include <gp_Ax2.hxx>
#include <gp_Pnt.hxx>
#include <gp_Dir.hxx>
#include <Graphic3d_AspectFillArea3d.hxx>
void ViewController::SetTexturedtoBoxFace(const cv::Mat& image)
{
    Handle(Image_AlienPixMap) occtImage = new Image_AlienPixMap();

    if (!occtImage->InitZero(Image_Format_RGB, image.cols, image.rows))
    {
        std::cerr << "Failed to initialize Image_AlienPixMap." << std::endl;
        return;
    }
    for (int row = 0; row < image.rows; ++row)
    {
        const uchar* sourceRow = image.ptr<uchar>(row);
        memcpy(occtImage->ChangeRow(row), sourceRow, image.cols * image.channels() * sizeof(uchar));
    }
    Handle(Graphic3d_Texture2D) texture = new Graphic3d_Texture2D(
        "MemoryTexture"
    );
    texture->SetImage(occtImage);
    texture->EnableRepeat();
    texture->EnableSmooth();
    texture->EnableModulate();

    gp_Ax2 anAxis;
    anAxis.SetLocation(gp_Pnt(0.0, 0.0, 0.0));
    TopoDS_Shape boxShape = BRepPrimAPI_MakeBox(anAxis, 1280, 1024, 50).Shape();

    TopExp_Explorer explorer(boxShape, TopAbs_FACE);

    const  TopoDS_Face& frontFace = TopoDS::Face(explorer.Current());

    if (frontFace.IsNull())
    {
        std::cerr << "Failed to find the front face of the box." << std::endl;
        return;
    }

    Handle(AIS_Shape) aBoxAIS = new AIS_Shape(boxShape);

    Handle(Graphic3d_AspectFillArea3d) fillAspect = new Graphic3d_AspectFillArea3d();
    fillAspect->SetTextureMapOn(true);
    fillAspect->SetTextureMap(texture);

    Handle(Prs3d_Drawer) drawer = new Prs3d_Drawer();
    Handle(Prs3d_ShadingAspect) shadingAspect = new Prs3d_ShadingAspect();
    shadingAspect->SetAspect(fillAspect);
    drawer->SetShadingAspect(shadingAspect);

    Handle(AIS_Shape) frontFaceAIS = new AIS_Shape(frontFace);
    frontFaceAIS->SetAttributes(drawer);

    myContext->Display(aBoxAIS, Standard_True);
    myContext->Display(frontFaceAIS, Standard_True);
    myContext->UpdateCurrentViewer();

    myContext->SetDisplayMode(AIS_Shaded, true);
}

void ViewController::SetTexturedtoPlane(const cv::Mat& image)
 {
        Handle(Image_AlienPixMap) occtImage = new Image_AlienPixMap();

        if (!occtImage->InitZero(Image_Format_RGB, image.cols, image.rows))
        {
            std::cerr << "Failed to initialize Image_AlienPixMap." << std::endl;
            return;
        }

        for (int row = 0; row < image.rows; ++row)
        {
            const uchar* sourceRow = image.ptr<uchar>(row);
            memcpy(occtImage->ChangeRow(row), sourceRow, image.cols * image.channels() * sizeof(uchar));
        }

        Handle(Graphic3d_Texture2D) texture = CreateTextureFromImage(occtImage);

        gp_Pln plane(gp::XOY());

        BRepBuilderAPI_MakeEdge mkEdge1(gp_Pnt(0, 0, 0), gp_Pnt(1024, 0, 0));
        BRepBuilderAPI_MakeEdge mkEdge2(gp_Pnt(1024, 0, 0), gp_Pnt(1024, 768, 0));
        BRepBuilderAPI_MakeEdge mkEdge3(gp_Pnt(1024, 768, 0), gp_Pnt(0, 768, 0));
        BRepBuilderAPI_MakeEdge mkEdge4(gp_Pnt(0, 768, 0), gp_Pnt(0, 0, 0));

        BRepBuilderAPI_MakeWire mkWire;
        mkWire.Add(mkEdge1.Edge());
        mkWire.Add(mkEdge2.Edge());
        mkWire.Add(mkEdge3.Edge());
        mkWire.Add(mkEdge4.Edge());

        BRepBuilderAPI_MakeFace mkFace(plane, mkWire.Wire());
        TopoDS_Shape planeShape = mkFace.Face();

        Handle(AIS_Shape) planeAIS = new AIS_Shape(planeShape);

        Handle(Graphic3d_AspectFillArea3d) fillAspect = new Graphic3d_AspectFillArea3d();
        fillAspect->SetTextureMapOn();
        fillAspect->SetTextureMap(texture);

        Handle(Prs3d_Drawer) drawer = new Prs3d_Drawer();
        Handle(Prs3d_ShadingAspect) shadingAspect = new Prs3d_ShadingAspect();
        shadingAspect->SetAspect(fillAspect);
        drawer->SetShadingAspect(shadingAspect);

        planeAIS->SetAttributes(drawer);

        myContext->Display(planeAIS, Standard_True);
}


Handle(Graphic3d_Texture2D) ViewController::CreateTextureFromImage(const Handle(Image_PixMap)& image)
{
    if (image.IsNull())
    {
        std::cerr << "Invalid Image_PixMap provided." << std::endl;
        return nullptr;
    }

    Handle(Graphic3d_Texture2D) texture = new Graphic3d_Texture2D(
        "MemoryTexture"
    );
    texture->SetImage(image);

    return texture;
}

Handle(Image_PixMap)  ViewController::ConvertCvMatToOcctImage(const cv::Mat& mat)
{
    Handle(Image_AlienPixMap) pixMap = new Image_AlienPixMap();

    if (mat.type() == CV_8UC3)
    {
        if (!pixMap->InitZero(Image_Format_RGB, mat.cols, mat.rows))
            return nullptr;
        for (int row = 0; row < mat.rows; ++row)
        {
            const uchar* sourceRow = mat.ptr<uchar>(row);
            memcpy(pixMap->ChangeRow(row), sourceRow, mat.cols * mat.channels() * sizeof(uchar));
        }
    }
    else if (mat.type() == CV_8UC4)
    {
        if (!pixMap->InitZero(Image_Format_RGB32, mat.cols, mat.rows))
            return nullptr;
        for (int row = 0; row < mat.rows; ++row)
        {
             const uchar* sourceRow = mat.ptr<uchar>(row);
             uchar* destRow = pixMap->ChangeRow(row);
             for (int col = 0; col < mat.cols; ++col)
             {
                destRow[col * 4 + 0] = sourceRow[col * 3 + 2];
                destRow[col * 4 + 1] = sourceRow[col * 3 + 1];
                destRow[col * 4 + 2] = sourceRow[col * 3 + 0];
                destRow[col * 4 + 3] = 255;
             }
        }
    }


    return pixMap;
}

#include <V3d_View.hxx>
#include <Aspect_Window.hxx>
#include <Graphic3d_ArrayOfTriangles.hxx>
#include <Bnd_Box.hxx>

void ViewController::AdjustModelBoundingBoxToImageSize(const Handle(V3d_View)& myView, const Standard_CString imagePath)
{
    (void)imagePath;
    Standard_Integer viewWidth, viewHeight;
    myView->Window()->Size(viewWidth, viewHeight);

    int imageWidth = 2048;
    int imageHeight = 1536;

    Bnd_Box boundingBox;
    double modelWidth = imageWidth;
    double modelHeight = imageHeight;

    boundingBox.Update(0, 0, 0, modelWidth, modelHeight, 0);

    double centerX = modelWidth / 2.0;
    double centerY = modelHeight / 2.0;
    double centerZ = 0.0;
    (void)centerZ;

    myView->SetSize(std::max(modelWidth, modelHeight));
    myView->ZFitAll();
    myView->SetCenter(centerX, centerY);

    myView->Redraw();
}

void ViewController::SetBackgroundInView(Handle(V3d_View)& view, const cv::Mat& image)
{
    UpdateImageViewImage(image);
    if (!m_renderImageInOcctBackground) return;
    Handle(Image_PixMap) occtImage = ConvertCvMatToOcctImage(image);

    if (!occtImage.IsNull())
    {
        Handle(Graphic3d_Texture2Dmanual) texture = CreateTextureFromImage(occtImage);

        if (!texture.IsNull())
        {

            Standard_Real scale = 1;
            view->SetScale(scale);

            view->SetProj(V3d_XposYposZpos);

            view->SetEye(0, 0, -100);

            view->SetAt(0, 0, 0);

            view->SetUp(0, -1, 0);

            view->SetDepth(100);

            view->SetZoom(1);

            Standard_Integer viewWidth, viewHeight;
            view->Window()->Size(viewWidth, viewHeight);

            int imageWidth = image.cols;
            int imageHeight = image.rows;

            m_dscalex = (1.0*imageWidth)/(1.0* viewWidth);
            m_dscaley = (1.0* imageHeight )/(1.0 * viewHeight);

            view->SetBackgroundImage(texture, Aspect_FM_STRETCH, true);
            double modelWidth = imageWidth;
            double modelHeight = imageHeight;
            double centerX = modelWidth / 2.0;
            double centerY = modelHeight / 2.0;
            double centerZ = 0.0;

            view->SetSize(std::max(modelWidth, modelHeight));
            view->ZFitAll();
            view->SetCenter(centerX, centerY);

            view->Redraw();
            myContext->UpdateCurrentViewer();
            myContext->UpdateCurrent();
        }
        else
        {
            std::cerr << "Failed to create texture from image." << std::endl;
        }
    }
    else
    {
        std::cerr << "Failed to convert image." << std::endl;
    }
}

void ViewController::initViewer(int theWidth, int theHeight)
{
    if (myOcctWindow.IsNull()
        || myOcctWindow->getGlfwWindow() == nullptr)
    {
        return;
    }

    Handle(OpenGl_GraphicDriver) aGraphicDriver = new OpenGl_GraphicDriver(myOcctWindow->GetDisplay(), false);
    Handle(V3d_Viewer) aViewer = new V3d_Viewer(aGraphicDriver);
    aViewer->SetDefaultLights();
    aViewer->SetLightOn();
    aViewer->SetDefaultTypeOfView(V3d_ORTHOGRAPHIC);
    if(0)
    aViewer->ActivateGrid(Aspect_GT_Rectangular, Aspect_GDM_Lines);

    m_myView = new V3d_CustomView(aViewer);

    m_myView->SetImmediateUpdate(false);
    m_myView->SetWindow(myOcctWindow, myOcctWindow->NativeGlContext());
    m_myView->ChangeRenderingParams().ToShowStats = true;
    myContext = new AIS_InteractiveContext(aViewer);

    Handle(Prs3d_Drawer) aSelectionStyle = myContext->SelectionStyle();
    aSelectionStyle->SetColor(Quantity_NOC_WHITE);
    myContext->SetSelectionStyle(aSelectionStyle);

    m_myView->SetProj(V3d_TypeOfOrientation::V3d_XposYposZpos);

    Standard_Real eyeX = 0.0, eyeY = 0.0, eyeZ =  -500.0;
    m_myView->SetEye(eyeX, eyeY, eyeZ);

    Standard_Real atX = 0.0, atY = 0.0, atZ = 0.0;
    m_myView->SetAt(atX, atY, atZ);

    Standard_Real upX = 0.0, upY = -1.0, upZ = 0.0;
    m_myView->SetUp(upX, upY, upZ);

     m_myView->SetDepth(100);
     m_myView->SetCenter(theWidth, theHeight);

     Standard_Real scale = 1;
     m_myView->SetScale(scale);

     m_myView->SetProj(V3d_XposYposZpos);

     m_myView->SetEye(0, 0, -10000);

     m_myView->SetAt(0, 0, 0);

     m_myView->SetUp(0, -1, 0);

     m_myView->SetDepth(10000);

     m_myView->SetZoom(1);

     m_myView->SetCenter(theWidth , theHeight );

    m_myView->ZFitAll();
    m_myView->Redraw();


}
void ViewController::drawline()
{
    gp_Pnt pn_Start;
    pn_Start.SetX(10);
    pn_Start.SetY(20);
    pn_Start.SetZ(0);

    gp_Pnt pn_End;
    pn_End.SetX(50);
    pn_End.SetY(60);
    pn_End.SetZ(0);

    TopoDS_Vertex V1 = BRepBuilderAPI_MakeVertex(pn_Start);
    TopoDS_Vertex V2 = BRepBuilderAPI_MakeVertex(pn_End);

    TopoDS_Shape aShape = BRepBuilderAPI_MakeEdge(V1, V2);

    Handle(AIS_Shape) aisLine = new AIS_Shape(aShape);

    myContext->Display(aisLine, AIS_Shaded, 0, false);
}

void ViewController::initDemoScene()
{
  if (myContext.IsNull())
  {
    return;
  }
   SetAllowZooming(Standard_False);
   SetAllowRotation(Standard_False);

  if (1)
  {
      if(0)
      {
          gp_Trsf shapeTrsf = aBox->LocalTransformation();

          gp_Pnt position = gp_Pnt(shapeTrsf.TranslationPart());

          gp_Dir xAxis;
          gp_Dir yAxis;

          if (shapeTrsf.Form() == gp_Identity || shapeTrsf.Form() == gp_Translation) {
              xAxis = gp::DX();
              yAxis = gp::DY();
          }
          else {
              gp_Mat rotationMatrix = shapeTrsf.VectorialPart();

              xAxis = gp_Dir(rotationMatrix.Column(1));
              yAxis = gp_Dir(rotationMatrix.Column(2));
          }

          gp_Ax2 axis(position, yAxis, xAxis);

      }
  }

gp_Path m_gpath;
m_gpath.SetContext(myContext);
m_gpath.SetView(m_myView);

    std::string init_reason;
    if (!m_parserOwner.Initialize(init_reason))
    {
        m_os << "parser initialization failed: " << init_reason << std::endl;
    }
    else
    {
        m_parserOwner.ConfigureStreams(&m_os, &m_createcodeos);
        m_parserDebugBridge.Bind(&m_parserOwner);
    }

    initialparser();

    myContext->SetDisplayMode(AIS_Shaded, true);

  TCollection_AsciiString aGlInfo;
  {
    TColStd_IndexedDataMapOfStringString aRendInfo;
    m_myView->DiagnosticInformation (aRendInfo, Graphic3d_DiagnosticInfo_Basic);
    for (TColStd_IndexedDataMapOfStringString::Iterator aValueIter (aRendInfo); aValueIter.More(); aValueIter.Next())
    {
      if (!aGlInfo.IsEmpty()) { aGlInfo += "\n"; }
      aGlInfo += TCollection_AsciiString("  ") + aValueIter.Key() + ": " + aValueIter.Value();
    }
  }
  Message::DefaultMessenger()->Send (TCollection_AsciiString("OpenGL info:\n") + aGlInfo, Message_Info);

}

static ImVec2 last_window_pos = ImVec2(0, 0);
void ViewController::mainloop()
{
     int ifirstrun = 1;
     while (!glfwWindowShouldClose(myOcctWindow->getGlfwWindow()))
     {
         glfwWaitEvents();

         if (!m_myView.IsNull())
         {

             ImGui_ImplOpenGL3_NewFrame();
             ImGui_ImplGlfw_NewFrame();
             ImGui::NewFrame();

             ImGuiIO& io = ImGui::GetIO();
             const bool imguiCapturesMouse =
                 io.WantCaptureMouse ||
                 ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) ||
                 ImGui::IsAnyItemHovered() ||
                 ImGui::IsAnyItemActive();

             const bool canvasInteraction =
                 !imguiCapturesMouse &&
                 (ImGui::IsMouseDown(ImGuiMouseButton_Left) ||
                  ImGui::IsMouseDown(ImGuiMouseButton_Right) ||
                  ImGui::IsMouseDown(ImGuiMouseButton_Middle) ||
                  io.MouseWheel != 0.0f);

             if (canvasInteraction || ifirstrun == 1)
             {
                 ifirstrun = 0;
                 FlushViewEvents(myContext, m_myView, true);
                 m_myView->Redraw();
             }

             if (m_showLegacyGpuWork)
             {
               ImGui::Begin("GPU work");

             {
                 ImVec2 current_window_pos = ImGui::GetWindowPos();
                 m_current_window_posx = current_window_pos.x;
                 m_current_window_posy = current_window_pos.y;

                  m_imguiw = ImGui::GetWindowWidth();
                  m_imguih = ImGui::GetWindowHeight();
                 if (current_window_pos.x != last_window_pos.x || current_window_pos.y != last_window_pos.y)
                 {
                     ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Window Moved!");
                     ifirstrun = 0;
                     FlushViewEvents(myContext, m_myView, true);
                     m_myView->Redraw();
                 }
                 last_window_pos = current_window_pos;
             }

             ImGui::Text("Parser code input here (%s) (%d)", IMGUI_VERSION, IMGUI_VERSION_NUM);
             ImGui::Checkbox("Show Image", &m_imageshow);
             ImGui::Checkbox("Pick Points", &m_ipickpoints);
             ImGui::Checkbox("Line Scan", &m_ilinescan);
             ImGui::Checkbox("Attach Line", &m_iattachline);
             ImGui::Spacing();
             static char text[512 * 106] =
                 "if(0){aimage1.load(\"1.bmp\");}\n"
                 "aimage1.Show(1);\n"
                 "amatch0.getshape(ashape0);\n"
                 "amatch0.setobjfilter(1);\n"
                 "amatch0.setwhgap(5, 5); \n"
                 "amatch0.setthre(35);\n"
                 "amatch0.setlinegap(3);\n"
                 "amatch0.setcompgap(20);\n"
                 "amatch0.learn(aimage1);\n"
                 "amatch0.savemodel(\"D:\\test.pat\");\n"
                 "amatch0.pattern2org(); \n"
                 "if(0){amatch0.pattern2org();}\n"
                 "if(0){amatch0.reorgpattern();}\n"
                 "if(0){amatch0.patterngap(0.5);}\n"
                 "if(0){amatch0.patternsample(3);}\n"
                 "if(0){amatch0.modelzero();amatch0.modelrotate(15.0);}\n"
                 "amatch0.Show(8);\n"
                 ;

             static char showtext[512 * 106];
             static ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput;
             ImGui::InputTextMultiline("##source", text, IM_ARRAYSIZE(text), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 6), flags);

             static int clicked = 0;
             if (ImGui::Button("Parser Run"))
             {
                 clicked++;
                 clearos();
                 clearcreateos();

                 auto start = std::chrono::high_resolution_clock::now();

                 if(0)
                 m_parserOwner.Compile(text);

                 auto end = std::chrono::high_resolution_clock::now();
                 std::chrono::duration<int, std::milli> elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
                 m_iruntimes = elapsed_time.count();

                 ifirstrun = 0;
                 FlushViewEvents(myContext, m_myView, true);
                 m_myView->Redraw();
             }
             static char text2[512 * 106] =
                 "amatch0.loadmodel(\"D:\\test.pat\");\n"
                 "amatch0.loadrotatemodel(\"D:\\test.pat\");\n"
                 "amatch0.samplemodel(100);\n"
                 "amatch0.setmatchrect(50,50,2200,1900);\n"
                 "amatch0.matchstepgap(10, 10);\n"
                 "amatch0.setmatchthre(10);\n"
                 "amatch0.setminscore(0.65);\n"
                 "amatch0.setfindnum(1);\n"
                 "if(0){\n"
                 "amatch0.match(aimage1);\n"
                 "}\n"
                 "\n"
                 "if(0){\n"
                 "amatch0.setanglescale(-10,10);\n"
                 "amatch0.rotatematchAB(aimage1);\n"
                 "}\n"
                 "if(0){\n"
                 "amatch0.reorgpattern();\n"
                 "amatch0.patterngap(0.2); \n"
                 "amatch0.matchmore(aimage1);\n"
                 "}\n"
                 "amatch0.Show(8);\n"
                 "dvalue1 = amatch0.getmaxresult();\n"
                 "dvalue2 = amatch0.getresultcentx(-1);\n"
                 "dvalue3 = amatch0.getresultcenty(-1);\n"
                 ;

             static char showtext2[512 * 106];
             static ImGuiInputTextFlags flags2 = ImGuiInputTextFlags_AllowTabInput;
             ImGui::InputTextMultiline("##source2", text2, IM_ARRAYSIZE(text2), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 6), flags2);
             if (ImGui::Button("Parser Run2"))
             {
                 clicked++;
                 clearos();
                 clearcreateos();

                 auto start = std::chrono::high_resolution_clock::now();

                 m_parserOwner.Compile(text2);

                 auto end = std::chrono::high_resolution_clock::now();
                 std::chrono::duration<int, std::milli> elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
                 m_iruntimes = elapsed_time.count();

                 ifirstrun = 0;
                 FlushViewEvents(myContext, m_myView, true);
                 m_myView->Redraw();
                 myContext->UpdateCurrentViewer();
                 myContext->UpdateCurrent();

             }

             static char text3[512 * 106] =
                 "aimage1.loadfiles(\"D:\\TestImage\\135\");\n"
                 "aimage1.getshape(ashape0);\n"
                 "if(0){\n"
                 "aimage1.roipyrdown(5);\n"
                 "aimage1.roieasythre(255);\n"
                 "}\n"
                 "aimage1.Show(1); \n"
                 "if(0){aimage1.rotate(10);}\n"
                 "if(0){aimage1.roi_5bgmb(3,1,0,2);}\n"
                 "if(0){aimage1.roi_5bgmbh(3,1,0,2);}\n"
                 "if(0){aimage1.roi_7bgmb(3,1,0,2);}\n"
                 "if(0){aimage1.roi_7bgmbh(3,1,0,2);}\n"
                 "if(0){aimage1.roisobel(0,1,7);}\n"
                 "if(0){aimage1.roischarr(0,1);}\n"
                 ;

             static char showtext3[512 * 106];
             static ImGuiInputTextFlags flags3 = ImGuiInputTextFlags_AllowTabInput;
             ImGui::InputTextMultiline("##source3", text3, IM_ARRAYSIZE(text3), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 6), flags3);
             if (ImGui::Button("Parser Run3"))
             {
                 clicked++;
                 clearos();
                 clearcreateos();

                 auto start = std::chrono::high_resolution_clock::now();


                 m_parserOwner.Compile(text3);

                 auto end = std::chrono::high_resolution_clock::now();
                 std::chrono::duration<int, std::milli> elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
                 m_iruntimes = elapsed_time.count();

                 ifirstrun = 0;
                 FlushViewEvents(myContext, m_myView, true);
                 m_myView->Redraw();
                 myContext->UpdateCurrentViewer();
                 myContext->UpdateCurrent();

             }

             static char text4[512 * 106] =
                 "if(0){aimage1.reload();}\n"
                 "aimage2.CopyFrom(aimage1);\n"
                 "aimage1.getshape(ashape0);\n"
                 "aimage1.Show(1);\n"
                 "if(0){\n"
                 "asam.setp(apoints0);\n"
                 "asam.setn(apoints1);\n"
                 "asam.run(aimage1);\n"
                 "aimage1.Or(aimage2);\n"
                 "}\n"
                 "if(0){\n"
                 "afindline.getshape(ashape0);\n"
                 "afindline.setwhgap(5, 5);\n"
                 "afindline.setlinegap(3);\n"
                 "afindline.setthre(38);\n"
                 "afindline.setmethod(0);\n"
                 "afindline.setobjfilter(0);\n"
                 "afindline.measure(aimage1);\n"
                 "afindline.sfilter(-1, -1); \n"
                 "afindline.Show(1);\n"
                 "afindline.inflectionpoint(apoints1);\n"
                 "apoints1.setcolor(255,0,0);\n"
                 "apoints1.Show(1);\n"
                 "}\n"
                 "if(0){\n"
                 "afindcircle.setgap(5);\n"
                 "afindcircle.getshape(ashape0);\n"
                 "afindcircle.setmethod(0);\n"
                 "afindcircle.setthre(20);\n"
                 "afindcircle.setlinegap(3);\n"
                 "afindcircle.setcirclegap(380);\n"
                 "afindcircle.measure(aimage1);\n"
                 "afindcircle.fitcircle();\n"
                 "afindcircle.setfitmeasuregap(80);\n"
                 "afindcircle.fitmeasure(aimage1);\n"
                 "afindcircle.Show(1);\n"
                 "dvalue1=afindcircle.getavgdist();\n"
                 "}\n"
                 ;
             static char showtext4[512 * 106];
             static ImGuiInputTextFlags flags4 = ImGuiInputTextFlags_AllowTabInput;
             ImGui::InputTextMultiline("##source4", text4, IM_ARRAYSIZE(text4), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 5), flags4);
             if (ImGui::Button("Parser Run4"))
             {
                 clicked++;
                 clearos();
                 clearcreateos();
                 auto start = std::chrono::high_resolution_clock::now();
                 m_parserOwner.Compile(text4);
                 auto end = std::chrono::high_resolution_clock::now();
                 std::chrono::duration<int, std::milli> elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
                 m_iruntimes = elapsed_time.count();
                 ifirstrun = 0;
                 FlushViewEvents(myContext, m_myView, true);
                 m_myView->Redraw();
                 myContext->UpdateCurrentViewer();
                 myContext->UpdateCurrent();
             }
             static char text5[512 * 106] =
                 "if(1){apoints0.load(\"D:\\26.data\");apoints0.Show(1);}\n"
                 "if(0){apoints0.save(\"D:\\2.data\");apoints0.Show(1);}\n"
                 "if(0){apoints0.aptfilter(5);apoints0.Show(1);}\n"
                 "if(0){apoints0.cluster(10,8);apoints0.Show(16);}\n"
                 "if(0){apoints0.sortpoints(80,0,10,45);apoints0.Show(16);}\n"
                 "if(0){apoints0.clear();apoints0.Show(1);}\n"
                 "if(0){apoints0.obbanglecenter(apoints1);}\n"
                 "if(0){apoints0.filter(20, 1); apoints0.Show(1);}\n"
                 "if(0){apoints0.findcross(apoints1);apoints1.Show(1);}\n"
                 ;
             static char showtext5[512 * 106];
             static ImGuiInputTextFlags flags5 = ImGuiInputTextFlags_AllowTabInput;
             ImGui::InputTextMultiline("##source5", text5, IM_ARRAYSIZE(text5), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 5), flags5);
             if (ImGui::Button("Parser Run5"))
             {
                 clicked++;
                 clearos();
                 clearcreateos();

                 auto start = std::chrono::high_resolution_clock::now();

                 m_parserOwner.Compile(text5);

                 auto end = std::chrono::high_resolution_clock::now();
                 std::chrono::duration<int, std::milli> elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
                 m_iruntimes = elapsed_time.count();

                 ifirstrun = 0;
                 FlushViewEvents(myContext, m_myView, true);
                 m_myView->Redraw();
                 myContext->UpdateCurrentViewer();
                 myContext->UpdateCurrent();

             }

             static char text6[512 * 106] =
                 "TestRun arun;\n"
                 "arun.testrun();\n"
                 "ahttp.runserver(); \n"
                 "ahttp.runclient(); \n"
                 ;
             static char showtext6[512 * 106];
             static ImGuiInputTextFlags flags6 = ImGuiInputTextFlags_AllowTabInput;
             ImGui::InputTextMultiline("##source6", text6, IM_ARRAYSIZE(text6), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 2), flags6);
             if (ImGui::Button("Parser Run6"))
             {
                 clicked++;
                 clearos();
                 clearcreateos();

                 auto start = std::chrono::high_resolution_clock::now();

                 m_parserOwner.Compile(text6);

                 auto end = std::chrono::high_resolution_clock::now();
                 std::chrono::duration<int, std::milli> elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
                 m_iruntimes = elapsed_time.count();

                 ifirstrun = 0;
                 FlushViewEvents(myContext, m_myView, true);
                 m_myView->Redraw();
                 myContext->UpdateCurrentViewer();
                 myContext->UpdateCurrent();

             }

             if (clicked & 1)
             {
                 clicked = 0;
                  m_imageshow = 1;
                 strcpy(showtext, getoutputstring().c_str());
                 strcat(showtext, "\n");

             }
             ImGui::SameLine();
             ImGui::Text("Parser Run result output here!");
             ImGui::Text(showtext);
             std::string strtime = string("elapsed time:")+std::to_string(m_iruntimes)+ string(" ms");
             ImGui::Text(strtime.c_str());
             ImGuiIO& io = ImGui::GetIO();
             ImGui::TextWrapped(" ");
             if (ImGui::Button("Run"))
             {
                 std::cout << "run!" << std::endl;
             }
             ImGui::Text("run gpu and ai.");

             if (ImGui::CollapsingHeader("OpenCV Editor"))
             {
                 ImGui::Checkbox("Show OpenCV Editor", &opencvSW);
                 ImGui::Checkbox("Show OpenCV Blur", &opencvblur);
                 ImGui::SliderInt("slider thre", &ivalue1, 1, 255);
                 ImGui::SliderInt("slider gap", &ivalue2, 1, 255);
                 ImGui::Checkbox("Show GPU Blur and HSV augmentation", &gpublur);
                 ImGui::Checkbox("reset OpenCV ", &opencvreset);
                 ImGui::Checkbox("Edge Image", &irunedge);

                 ImGui::Checkbox("pyramid thre Image", &ipythre);
                 ImGui::Checkbox("iotsuThreshold thre Image", &iotsuThreshold);

                 ImGui::Checkbox("H run", &ihedge);
                 ImGui::Checkbox("V run", &iwedge);
                 ImGui::Checkbox("B2W", &ib2wedge);
                 ImGui::Checkbox("W2B", &iw2bedge);
                 ImGui::SliderInt("pyramidDynamicThreshold levels", &ivalue3, 1, 8);
                 ImGui::SliderInt("pyramidDynamicThreshold blockSize", &ivalue4, 1, 19);
                 ImGui::SliderInt("value 5", &ivalue5, 1, 255);
                 ImGui::SliderInt("value 6", &ivalue6, 1, 255);
                 ImGui::SliderInt("value 7", &ivalue7, 0, 255);
                 ImGui::SliderInt("value 8", &ivalue8, 0, 255);
                 ImGui::SliderInt("value 9", &ivalue9, 0, 255);
                 ImGui::SliderInt("value 10", &ivalue10, 0, 255);
             }

             ImGui::End();
             }

             drawScriptAcceptancePanels();
             const SemanticFlowAction flowAction = m_semanticFlowGraph.Draw();
             HandleSemanticFlowAction(flowAction);
             drawEvidenceAlbumWindow();
             drawAnnotationToolWindow();
             drawKeyParameterControlsWindow();
             drawParameterTuningAndConclusionWindow();
             drawManualStateTestConsole();

             ImGui::SetNextWindowPos(ImVec2(650, 20), ImGuiCond_FirstUseEver); // Normally user code doesn't need/want to call this because positions are saved in .ini file anyway. Here we just want to make the demo initial state a bit more friendly!

             if (1 == m_imageshow)
             {
                 m_imageshow = 0;
                 Image* pshowimage = nullptr;
                 for (int i = 0; i < m_parserOwner.ObjectCount("Image"); i++)
                 {
                     Image* pimage = (Image*)m_parserOwner.GetClassObj("Image", i);
                     if (pimage->getshow() == 1)
                         pshowimage = pimage;
                 }

                 ImageManager* pmodule = (ImageManager*)m_parserOwner.GetClassObj("Module", "amodule");
                 Image* pmoduleimage = nullptr;
                 int imoduleshow = 0;
                 if (nullptr != pmodule)
                 {
                     pmoduleimage = pmodule->GetBackImage();
                     imoduleshow = pmodule->getshow();
                 }
                 if (0 != pmodule&& imoduleshow > 0 )
                 {
                     if(1== imoduleshow)
                        SetBackgroundInView(m_myView, pmodule->GetBackImage()->getmat());
                     else if(2== imoduleshow)
                        SetBackgroundInView(m_myView, pmodule->GetMapImage()->getmat());
                 }
                 else if (nullptr != pshowimage)
                 {
                    SetBackgroundInView(m_myView, pshowimage->getmat());
                 }
             }
             m_shapex = (Shape*)m_parserOwner.GetClassObj("Shape", "ashape0");
             m_apoints = (PointsShape*)m_parserOwner.GetClassObj("PointsShape", "apoints0");
             m_bpoints = (PointsShape*)m_parserOwner.GetClassObj("PointsShape", "apoints1");
             m_afindline = (Findline*)m_parserOwner.GetClassObj("Findline", "afindline");
             if (opencvSW)
                 Imgui_OpenCV_Window0(&opencvSW);

             ImGui::Render();
             ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

             glfwSwapBuffers(myOcctWindow->getGlfwWindow());
         }
     }
}

void ViewController::cleanup()
{
  if (m_imageViewTexture != 0)
  {
    glDeleteTextures(1, &m_imageViewTexture);
    m_imageViewTexture = 0;
  }
  if (!m_myView.IsNull())
  {
      m_myView->Remove();
  }
  if (!myOcctWindow.IsNull())
  {
    myOcctWindow->Close();
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwTerminate();
}

void  ViewController::Imgui_OpenCV_Ini0()
{
    const std::string imagePath = "D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/01.jpg";
    s_img0 = cv::imread(imagePath);
    if (s_img0.empty())
    {
        std::cerr << "Failed to load initial Image View image: " << imagePath << std::endl;
        return;
    }

    UpdateImageViewImage(s_img0);
    m_scriptResult.image_ref = imagePath;
    m_scriptResult.log_lines.push_back("Initial Image View image loaded.");
}


vector<float> ViewController::createGaussianKernel(int kernelSize, double sigma) {
    int kernelRadius = kernelSize / 2;
    vector<float> kernel(kernelSize * kernelSize, 0.0f);
    float sum = 0.0f;

    for (int j = -kernelRadius; j <= kernelRadius; ++j) {
        for (int i = -kernelRadius; i <= kernelRadius; ++i) {
            float r = sqrt(static_cast<float>(i * i + j * j));
            float val = exp(-(r * r) / (2 * sigma * sigma));
            kernel[(j + kernelRadius) * kernelSize + (i + kernelRadius)] = val;
            sum += val;
        }
    }

    for (float& val : kernel) {
        val /= sum;
    }

    return kernel;
}


#if CXCORE_ENABLE_VIEWCONTROLLER_CUDA
GLuint CreateTextureCubeX(cv::Mat& src) {
    if (src.empty()) {
        std::cerr << "Input image is empty!" << std::endl;
        return 0;
    }

    cv::Mat src_rgba;
    if (src.channels() == 3) {
        cv::cvtColor(src, src_rgba, cv::COLOR_BGR2RGBA);
    }
    else if (src.channels() == 4) {
        src_rgba = src;
    }
    else {
        std::cerr << "Unsupported number of channels: " << src.channels() << std::endl;
        return 0;
    }

    GLuint gl_texture_id;
    glGenTextures(1, &gl_texture_id);
    glBindTexture(GL_TEXTURE_2D, gl_texture_id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, src.cols, src.rows, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, src_rgba.data);

    glBindTexture(GL_TEXTURE_2D, 0);

    cudaGraphicsResource* cuda_tex_resource;
    cudaCheckErrors(cudaGraphicsGLRegisterImage(&cuda_tex_resource, gl_texture_id,
        GL_TEXTURE_2D, cudaGraphicsMapFlagsWriteDiscard));

    void* dev_ptr;
    size_t num_bytes;
    cudaCheckErrors(cudaGraphicsMapResources(1, &cuda_tex_resource, 0));
    cudaCheckErrors(cudaGraphicsSubResourceGetMappedArray((cudaArray**)&dev_ptr, cuda_tex_resource, 0, 0));

    cudaChannelFormatDesc channel_desc = cudaCreateChannelDesc<uchar4>();
    cudaMemcpyToArray((cudaArray*)dev_ptr, 0, 0, src_rgba.data,
        src.cols * src.rows * sizeof(uchar4),
        cudaMemcpyHostToDevice);

    cudaCheckErrors(cudaGraphicsUnmapResources(1, &cuda_tex_resource, 0));

    return gl_texture_id;
}

GLuint CreateTextureCubeY(cv::Mat& src) {
    if (src.empty()) {
        std::cerr << "Input image is empty!" << std::endl;
        return 0;
    }

    cv::Mat src_rgba;
    if (src.channels() == 3) {
        cv::cvtColor(src, src_rgba, cv::COLOR_BGR2RGBA);
    }
    else if (src.channels() == 4) {
        src_rgba = src.clone();
    }
    else {
        std::cerr << "Unsupported number of channels: " << src.channels() << std::endl;
        return 0;
    }

    const int imageW = src.cols;
    const int imageH = src.rows;

    GLuint gl_texture_id;
    glGenTextures(1, &gl_texture_id);
    glBindTexture(GL_TEXTURE_2D, gl_texture_id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, imageW, imageH, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glBindTexture(GL_TEXTURE_2D, 0);

    cudaGraphicsResource* cuda_tex_resource;
    cudaCheckErrors(cudaGraphicsGLRegisterImage(&cuda_tex_resource, gl_texture_id,
        GL_TEXTURE_2D, cudaGraphicsMapFlagsWriteDiscard));

    cudaChannelFormatDesc channelDesc = cudaCreateChannelDesc<uchar4>();
    cudaArray* cu_array = nullptr;
    cudaCheckErrors(cudaMallocArray(&cu_array, &channelDesc, imageW, imageH));
    cudaCheckErrors(cudaMemcpyToArray(cu_array, 0, 0, src_rgba.data,
        imageW * imageH * sizeof(uchar4),
        cudaMemcpyHostToDevice));

    cudaTextureObject_t texImage0 = 0;
    {
        cudaResourceDesc resDesc = {};
        resDesc.resType = cudaResourceTypeArray;
        resDesc.res.array.array = cu_array;
        cudaTextureDesc texDesc = {};
        texDesc.addressMode[0] = cudaAddressModeClamp;
        texDesc.addressMode[1] = cudaAddressModeClamp;
        texDesc.filterMode = cudaFilterModePoint;
        texDesc.readMode = cudaReadModeElementType;
        texDesc.normalizedCoords = 0;
        cudaCheckErrors(cudaCreateTextureObject(&texImage0, &resDesc, &texDesc, nullptr));
    }

    uchar4* d_dst = nullptr;
    cudaCheckErrors(cudaGraphicsMapResources(1, &cuda_tex_resource, 0));
    cudaCheckErrors(cudaGraphicsSubResourceGetMappedArray((cudaArray**)&d_dst, cuda_tex_resource, 0, 0));

    runImageFiltersx((TColor*)d_dst, imageW, imageH, 0, texImage0);

    cudaCheckErrors(cudaGraphicsUnmapResources(1, &cuda_tex_resource, 0));

    cudaCheckErrors(cudaDestroyTextureObject(texImage0));
    cudaCheckErrors(cudaFreeArray(cu_array));
    cudaCheckErrors(cudaGraphicsUnregisterResource(cuda_tex_resource));

    return gl_texture_id;
}

GLuint CreateTextureCubeZ(cv::Mat& src) {
    if (src.empty()) {
        std::cerr << "Input image is empty!" << std::endl;
        return 0;
    }

    cv::Mat src_rgba;
    if (src.channels() == 3) {
        cv::cvtColor(src, src_rgba, cv::COLOR_BGR2RGBA);
    }
    else if (src.channels() == 4) {
        src_rgba = src.clone();
    }
    else {
        std::cerr << "Unsupported number of channels: " << src.channels() << std::endl;
        return 0;
    }

    const int imageW = src.cols;
    const int imageH = src.rows;

    GLuint gl_texture_id;
    glGenTextures(1, &gl_texture_id);
    glBindTexture(GL_TEXTURE_2D, gl_texture_id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, imageW, imageH, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glBindTexture(GL_TEXTURE_2D, 0);

    cudaGraphicsResource* cuda_tex_resource;
    checkCudaErrors(cudaGraphicsGLRegisterImage(&cuda_tex_resource, gl_texture_id,
        GL_TEXTURE_2D, cudaGraphicsMapFlagsWriteDiscard));

    cudaTextureObject_t texImage = 0;
    uchar4* h_Src = (uchar4*)src_rgba.data;
    checkCudaErrors(CUDA_MallocArray(&h_Src, imageW, imageH));

    uchar4* d_dst = nullptr;
    checkCudaErrors(cudaGraphicsMapResources(1, &cuda_tex_resource, 0));
    checkCudaErrors(cudaGraphicsSubResourceGetMappedArray((cudaArray**)&d_dst, cuda_tex_resource, 0, 0));

    runImageFiltersx((TColor*)d_dst, imageW, imageH,0, texImage);
    cudaDeviceSynchronize();

    checkCudaErrors(cudaGraphicsUnmapResources(1, &cuda_tex_resource, 0));

    checkCudaErrors(CUDA_FreeArray());
    checkCudaErrors(cudaGraphicsUnregisterResource(cuda_tex_resource));

    return gl_texture_id;
}

void ViewController::Imgui_GPU_NLM_main(cv::Mat& src_host, cv::Mat& dst_host) {
    if (src_host.empty() || src_host.channels() != 4) {

        if (src_host.empty() || src_host.channels() != 3) {
            throw std::invalid_argument("Source image must be 24-bit (3 channels).");
        }
        cv::Mat rgba;

        cv::cvtColor(src_host, rgba, cv::COLOR_BGR2BGRA);
    }

    int imageW = src_host.cols;
    int imageH = src_host.rows;
    size_t num_bytes = imageW * imageH * sizeof(uchar4);

    cudaChannelFormatDesc channelDesc = cudaCreateChannelDesc<uchar4>();
    cudaArray* cu_array = nullptr;
    checkCudaErrors(cudaMallocArray(&cu_array, &channelDesc, imageW, imageH));
    if (!src_host.isContinuous()) {
        cv::Mat tmp_src = src_host.clone();
        checkCudaErrors(cudaMemcpyToArray(cu_array, 0, 0, tmp_src.data, num_bytes, cudaMemcpyHostToDevice));
    }
    else {
        checkCudaErrors(cudaMemcpyToArray(cu_array, 0, 0, src_host.data, num_bytes, cudaMemcpyHostToDevice));
    }

    //
    struct cudaResourceDesc resDesc = {};
    memset(&resDesc, 0, sizeof(resDesc));
    resDesc.resType = cudaResourceTypeArray;
    resDesc.res.array.array = cu_array;

    struct cudaTextureDesc texDesc = {};
    memset(&texDesc, 0, sizeof(texDesc));
    texDesc.addressMode[0] = cudaAddressModeClamp;
    texDesc.addressMode[1] = cudaAddressModeClamp;
    texDesc.filterMode = cudaFilterModePoint;
    texDesc.readMode = cudaReadModeElementType;
    texDesc.normalizedCoords = 0;

    checkCudaErrors(cudaCreateTextureObject(&texImage, &resDesc, &texDesc, nullptr));

    TColor* d_dst = nullptr;
    checkCudaErrors(cudaMalloc(&d_dst, num_bytes));

    cuda_Copy(d_dst, imageW, imageH, texImage);

    size_t imageSize = src_host.step * src_host.rows;
    cv::Mat augmented_host(src_host.size(), src_host.type());
    checkCudaErrors(cudaMemcpy(augmented_host.data, d_dst, imageSize, cudaMemcpyDeviceToHost));

    checkCudaErrors(cudaDestroyTextureObject(texImage));
    checkCudaErrors(cudaFreeArray(cu_array));
    checkCudaErrors(cudaFree(d_dst));
    dst_host = augmented_host;
}

void ViewController::Imgui_GPU_NLM_main0(cv::Mat& src_host, cv::Mat& dst_host)
{
    TColor* d_dst = NULL;
    size_t num_bytes;
    uchar4* h_Src = matToUchar4(src_host);

    imageW = src_host.cols;
    imageH = src_host.rows;
    CUDA_MallocArray(&h_Src, imageW, imageH);

    unsigned char* h_dst = NULL;
    cudaMalloc((void**)&d_dst, imageW * imageH * sizeof(TColor));
    h_dst = (unsigned char*)malloc(imageH * imageW * 4);

    int kernel = 1 ;

    runImageFiltersx(d_dst,imageH,imageW,0, texImage);

    cudaDeviceSynchronize();

    cudaMemcpy(dst_host.data, d_dst, imageW * imageH * sizeof(TColor),cudaMemcpyDeviceToHost);

    CUDA_FreeArray();
    free(h_Src);
    cudaFree(d_dst);
    free(h_dst);

}
void ViewController::Imgui_GPU_Gauss_main(cv::Mat& src_host, cv::Mat& dst_host)
{

    int kernelSize = 11;
    int kernelRadius = kernelSize / 2;
    double sigma = 0;

    if (sigma == 0) {
        sigma = 0.3 * ((kernelRadius - 1) * 0.5 - 1) + 0.8;
    }

    vector<float> h_kernel = createGaussianKernel(kernelSize, sigma);
    float* kernelData = h_kernel.data();

    unsigned char* d_input = nullptr, * d_output = nullptr;
    float* d_kernel = nullptr;
    size_t imageSize = src_host.step * src_host.rows;
    cudaMalloc(&d_input, imageSize);
    cudaMalloc(&d_output, imageSize);
    cudaMalloc(&d_kernel, sizeof(float) * h_kernel.size());
    cudaMemcpy(d_input, src_host.data, imageSize, cudaMemcpyHostToDevice);
    cudaMemcpy(d_kernel, kernelData, sizeof(float) * h_kernel.size(), cudaMemcpyHostToDevice);

    applyGaussianBlurOnGPU(d_input, d_output, src_host.cols, src_host.rows, src_host.channels(), d_kernel, kernelRadius);

    cv::Mat blurred_host(src_host.size(), src_host.type());
    cudaMemcpy(blurred_host.data, d_output, imageSize, cudaMemcpyDeviceToHost);

    float hueDelta = 30.0f;
    float saturationScale = 1.2f;
    float valueScale = 1.1f;

    cudaMemcpy(d_input, blurred_host.data, imageSize, cudaMemcpyHostToDevice);

    applyHSVAugmentationOnGPU(d_input, src_host.cols, src_host.rows, hueDelta, saturationScale, valueScale);

    cv::Mat augmented_host(src_host.size(), src_host.type());
    cudaMemcpy(augmented_host.data, d_input, imageSize, cudaMemcpyDeviceToHost);

    cudaFree(d_input);
    cudaFree(d_output);
    cudaFree(d_kernel);
    dst_host = augmented_host;

}

#else
void ViewController::Imgui_GPU_NLM_main(cv::Mat& src_host, cv::Mat& dst_host)
{
    src_host.copyTo(dst_host);
}

void ViewController::Imgui_GPU_NLM_main0(cv::Mat& src_host, cv::Mat& dst_host)
{
    src_host.copyTo(dst_host);
}

void ViewController::Imgui_GPU_Gauss_main(cv::Mat& src_host, cv::Mat& dst_host)
{
    if (src_host.empty()) {
        dst_host.release();
        return;
    }

    cv::GaussianBlur(src_host, dst_host, cv::Size(11, 11), 0.0);
}
#endif

void  ViewController::Imgui_OpenCV_Window0(bool* p_open)
{
    (void)p_open;
    ImGui::Begin("Image Viewer");
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    ImVec2 win_size = ImGui::GetWindowSize();
    float aspect_ratio = static_cast<float>(s_img0.cols) / s_img0.rows;
    ImVec2 img_display_size = ImVec2(win_size.x * 0.9f, win_size.y * 0.9f);
    if (img_display_size.x / img_display_size.y > aspect_ratio)
        img_display_size.x = img_display_size.y * aspect_ratio;
    else
        img_display_size.y = img_display_size.x / aspect_ratio;

    static bool use_text_color_for_tint = false;
    (void)use_text_color_for_tint;
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 uv_min = ImVec2(0.0f, 0.0f);                 // Top-left
    ImVec2 uv_max = ImVec2(1.0f, 1.0f);                 // Lower-right
    ImVec4 tint_col = use_text_color_for_tint ? ImGui::GetStyleColorVec4(ImGuiCol_Text) : ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // No tint
    ImVec4 border_col = ImGui::GetStyleColorVec4(ImGuiCol_Border);

    if (irunedge)
    {
        m_occtimage.copyFromMat(s_img0);
        m_occtimage.colorizeROI(cv::Scalar(0, 0, 0));
        if (iwedge)
        {
            if (ib2wedge)
            {
                Image octimageX;
                octimageX.copyFromMat(s_img0);
                octimageX.roi_7blur_gap_mud_thre_bw(ivalue1, 0, ivalue2, 1);
                m_occtimage.bitwiseOr(octimageX);
            }
            if (iw2bedge)
            {
                Image octimage0;
                octimage0.copyFromMat(s_img0);
                octimage0.roi_7blur_gap_mud_thre_bw(ivalue1, 0, ivalue2, 0);
                m_occtimage.bitwiseOr(octimage0);
            }
        }
        if (ihedge)
        {

            if (ib2wedge)
            {
                Image octimage1;
                octimage1.copyFromMat(s_img0);
                octimage1.roi_7blur_gap_mud_thre_bw_h(ivalue1, 0, ivalue2, 1);
                m_occtimage.bitwiseOr(octimage1);
            }
            if (iw2bedge)
            {
                Image octimage2;
                octimage2.copyFromMat(s_img0);
                octimage2.roi_7blur_gap_mud_thre_bw_h(ivalue1, 0, ivalue2, 0);
                m_occtimage.bitwiseOr(octimage2);
            }
        }

        SetBackgroundInView(m_myView, m_occtimage.getmat());
    }
    if (ipythre)
    {
        m_occtimage.copyFromMat(s_img0);

        Image octimagez0 = m_occtimage.getROI();
        Image binaryImage = octimagez0.pyramidDynamicThresholding(ivalue3, ivalue4, 0);

        m_occtimage.colorizeROI(cv::Scalar(0, 0, 0));
        m_occtimage.copyResizedToROI(binaryImage.getmat());

        //
        texture_id0 = CreateTextureFromMat0(m_occtimage.getmat());
        SetBackgroundInView(m_myView, m_occtimage.getmat());
    }
    if (iotsuThreshold)
    {
        m_occtimage.copyFromMat(s_img0);

        Image octimagez0 = m_occtimage.getROI();

        Image otsuBinaryImage = octimagez0.otsuThresholding(255, cv::THRESH_BINARY | cv::THRESH_OTSU);

        m_occtimage.colorizeROI(cv::Scalar(0, 0, 0));
        m_occtimage.copyResizedToROI(otsuBinaryImage.getmat());
        texture_id0 = CreateTextureFromMat0(m_occtimage.getmat());
        SetBackgroundInView(m_myView, m_occtimage.getmat());
    }
    if (0)
    {
        m_occtimage.copyFromMat(s_img0);

        Image octimagez0 = m_occtimage.getROI();
        Image adaptiveBinaryImage = octimagez0.adaptiveThresholding(255, cv::ADAPTIVE_THRESH_GAUSSIAN_C, cv::THRESH_BINARY, ivalue5, ivalue6);

        m_occtimage.colorizeROI(cv::Scalar(0, 0, 0));
        m_occtimage.copyResizedToROI(adaptiveBinaryImage.getmat());
        texture_id0 = CreateTextureFromMat0(m_occtimage.getmat());
        SetBackgroundInView(m_myView, m_occtimage.getmat());
    }


    if (1 == opencvblur)
    {
        if (1)
        {
            s_img0 = cv::imread("0.jpg");
            SetTexturedtoBoxFace(s_img0);
        }
        if (0)
        {
            m_occtimage.copyFromMat(s_img0);
            Image adaptiveBinaryImage = m_occtimage.adaptiveThresholding(255, cv::ADAPTIVE_THRESH_GAUSSIAN_C, cv::THRESH_BINARY, 11, 2);

            texture_id0 = CreateTextureFromMat0(adaptiveBinaryImage.getmat());
            SetBackgroundInView(m_myView, adaptiveBinaryImage.getmat());

            if (0)
            {
                m_occtimage.copyFromMat(s_img0);
                texture_id0 = CreateTextureFromMat0(m_occtimage.getmat());
                SetBackgroundInView(m_myView, m_occtimage.getmat());
            }
        }
        if (0)
        {
            cv::Mat  blurred;
            GaussianBlur(s_img0, blurred, cv::Size(17, 17), 0);
            texture_id0 = CreateTextureFromMat0(blurred);
            SetBackgroundInView(m_myView, blurred);
        }
        else
        {
            if (0)
            {
                Image octimagez;
                octimagez.copyFromMat(s_img0);
                octimagez.analyzeConnectedComponentsColor(50.0, 100, 0.5, 2.0);
                octimagez.analyzeConnectedComponentsPyramid(100, 0.5, 2.0);
                texture_id0 = CreateTextureFromMat0(octimagez.getmat());

                SetBackgroundInView(m_myView, octimagez.getmat());

            }
            if (0)
            {
                Image octimagez;
                octimagez.copyFromMat(s_img0);
                Image binaryImage = octimagez.pyramidDynamicThresholding(4, 11, 0);
                texture_id0 = CreateTextureFromMat0(binaryImage.getmat());
                SetBackgroundInView(m_myView, binaryImage.getmat());
            }
            if (0)
            {
                Image octimagez;
                octimagez.copyFromMat(s_img0);
                Image octimagez0 = octimagez.getROI();
                octimagez0.colorFillConnectedComponents(100, 50, 50, cv::Scalar(0, 0, 255));
                //
                texture_id0 = CreateTextureFromMat0(octimagez0.getmat());
                SetBackgroundInView(m_myView, octimagez0.getmat());
            }
        }
        opencvblur = 0;
    }
    else if (1 == gpublur)
    {
        cv::Mat  blurred;

        texture_id0 = CreateTextureCube(s_img0);
#if CXCORE_ENABLE_VIEWCONTROLLER_CUDA
        UpdateTextureWithCuda(2048,1536);
#else
        SetBackgroundInView(m_myView, s_img0);
#endif
        gpublur = 0;
    }
    else if (0 == opencvblur && 0 == texture_id0)
    {
        //
        texture_id0 = CreateTextureFromMat0(s_img0);
        SetBackgroundInView(m_myView, s_img0);
    }
    else if (1 == opencvreset)
    {
        //
        texture_id0 = CreateTextureFromMat0(s_img0);
        SetBackgroundInView(m_myView, s_img0);
        opencvreset = 0;
    }

    ImGui::Image((ImTextureID)(uint64_t)texture_id0, img_display_size, ImVec2(0, 0), ImVec2(1, 1));

    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGuiIO& io0 = ImGui::GetIO();
        float my_tex_w = img_display_size.x;
        float my_tex_h = img_display_size.y;
        float region_sz = 32.0f;
        float region_x = io0.MousePos.x - pos.x - region_sz * 0.5f;
        float region_y = io0.MousePos.y - pos.y - region_sz * 0.5f;
        float zoom = 10.0f;

        if (region_x < 0.0f) { region_x = 0.0f; }
        else if (region_x > my_tex_w - region_sz) { region_x = my_tex_w - region_sz; }
        if (region_y < 0.0f) { region_y = 0.0f; }
        else if (region_y > my_tex_h - region_sz) { region_y = my_tex_h - region_sz; }
        ImGui::Text("Min: (%.2f, %.2f)", region_x, region_y);
        ImGui::Text("Max: (%.2f, %.2f)", region_x + region_sz, region_y + region_sz);
        ImVec2 uv0 = ImVec2((region_x) / my_tex_w, (region_y) / my_tex_h);
        ImVec2 uv1 = ImVec2((region_x + region_sz) / my_tex_w, (region_y + region_sz) / my_tex_h);

        ImGui::Image((ImTextureID)(uint64_t)texture_id0, ImVec2(region_sz * zoom, region_sz * zoom), uv0, uv1, tint_col, border_col);//
        ImGui::EndTooltip();
    }
    ImGui::End();

}
void ViewController::onResize(int theWidth, int theHeight)
{
    if (theWidth != 0
        && theHeight != 0
        && !m_myView.IsNull())
    {
        m_myView->Window()->DoResize();
        m_myView->MustBeResized();
        m_myView->Invalidate();
        m_myView->Redraw();
    }
}
void ViewController::onMouseScroll(double theOffsetX, double theOffsetY)
{
    if (ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse)
        return;
    (void)theOffsetX;
    if (!m_myView.IsNull())
    {
        UpdateZoom(Aspect_ScrollDelta(myOcctWindow->CursorPosition(), int(theOffsetY * 8.0)));
    }
}
void ViewController::SetParserValue(const string& codestr, double dvalue)
{
    if (!m_parserOwner.IsObjectVar(codestr.c_str()))
        return;
    string astr(codestr+"="+formatNumber(dvalue)+";");
    m_parserOwner.Compile(astr.c_str());
}
double ViewController::GetParserValue(const string& codestr)
{
    double* pdouble = NULL;
    if (!m_parserOwner.IsObjectVar((const char*)codestr.c_str()))
        return 0x00;
    pdouble = (double*)m_parserOwner.GetDoubleValue((const char*)codestr.c_str());
    if (NULL == pdouble)
        return 0x00;
    double dvalue = 0;
    dvalue = (*pdouble);
    if (dvalue < 0.0000001 && dvalue>0)
        return 0;
    return dvalue;
}
string ViewController::initialparser()
{
    string str;
    bool bresult = m_parserOwner.Compile("Module amodule;");
    if (!bresult)
    {
        str = str + "build Module fail!\r\n";
        str = str + getoutputstring();
    }
    else
    {
        str = str + "build Module ok\r\n";
    }
    clearos();
    string strfile = getlocationstring("./static.h");
    string strcode = loadfilestring(strfile);
    bresult = m_parserOwner.Compile(strcode.c_str());
    if (!bresult)
    {
        str = str + "build " + strfile + " fail!\r\n";
        str = str + getoutputstring();
    }
    else
    {
        str = str + "build " + strfile + " ok\r\n";
    }
    clearos();
    vector<string> files;
    files = DirFileFind(getlocationstring(string("./")), string("*.ums"));
    for (int i = 0; i < files.size(); ++i)
    {
        string filename = files[i];

        string getfilename = getFullFileName(filename);

        m_strcode = loadfilestring(filename);

        bresult = m_parserOwner.Compile(m_strcode.c_str());
        if (!bresult)
        {
            str = str + "build " + getfilename + " fail!\r\n";
            str = str + getoutputstring();
        }
        else
        {
            str = str + "build " + getfilename + " ok\r\n";
        }
        clearos();
    }
    for (int i = 0; i < m_parserOwner.ObjectCount("Image"); i++)
    {
        Image* pimage = (Image*)m_parserOwner.GetClassObj("Image", i);
        if (nullptr != pimage)
            *pimage = Image(2048,1536,CV_32FC3);
    }

    m_dzoomx = GetParserValue("m_dzoomx");
    m_dzoomy = GetParserValue("m_dzoomy");
    if (0.0 == m_dzoomx)
        m_dzoomx = 1;
    if (0.0 == m_dzoomy)
        m_dzoomy = 1;
    return str;
}
void ViewController::clearos()
{
    m_os.str("");
    m_os.clear();
}
void ViewController::clearcreateos()
{
    m_createcodeos.str("");
    m_createcodeos.clear();
}
void ViewController::clearparserobject()
{
    m_parserOwner.ClearAll();
}
void ViewController::resetparser()
{
    clearparserobject();
    initialparser();
}
Shape* ViewController::indexAt(const gp_Pnt& pos)
{
    int isize = m_parserOwner.ObjectCount("Shape");
    for (int i = 0; i < isize; i++)
    {
        Shape* pshape = (Shape*)m_parserOwner.GetClassObj("Shape", i);

        if (pshape->rect().contains(pos))
            if (pshape->show())
                return pshape;
    }
    isize = m_parserOwner.ObjectCount("Findline");
    for (int i = 0; i < isize; i++)
    {
        Shape* pshape = (Shape*)m_parserOwner.GetClassObj("Findline", i);

        if (pshape->rect().contains(pos))
            if (pshape->show())
                return pshape;
    }
    isize = m_parserOwner.ObjectCount("findcircle");
    for (int i = 0; i < isize; i++)
    {
        Shape* pshape = (Shape*)m_parserOwner.GetClassObj("findcircle", i);

        if (pshape->rect().contains(pos))
            if (pshape->show())
                return pshape;
    }
    isize = m_parserOwner.ObjectCount("ASR");
    for (int i = 0; i < isize; i++)
    {
        Shape* pshape = (Shape*)m_parserOwner.GetClassObj("ASR", i);

        if (pshape->rect().contains(pos))
            if (pshape->show())
                return pshape;
    }
    isize = m_parserOwner.ObjectCount("findobject");
    for (int i = 0; i < isize; i++)
    {
        Shape* pshape = (Shape*)m_parserOwner.GetClassObj("findobject", i);

        if (pshape->rect().contains(pos))
            if (pshape->show())
                return pshape;
    }
    isize = m_parserOwner.ObjectCount("Imageroi");
    for (int i = 0; i < isize; i++)
    {
        Shape* pshape = (Shape*)m_parserOwner.GetClassObj("Imageroi", i);

        if (pshape->rect().contains(pos))
            if (pshape->show())
                return pshape;
    }
    isize = m_parserOwner.ObjectCount("imagecodeparser");
    for (int i = 0; i < isize; i++)
    {
        Shape* pshape = (Shape*)m_parserOwner.GetClassObj("imagecodeparser", i);

        if (pshape->rect().contains(pos))
            if (pshape->show())
                return pshape;
    }
    isize = m_parserOwner.ObjectCount("gridobject");
    for (int i = 0; i < isize; i++)
    {
        Shape* pshape = (Shape*)m_parserOwner.GetClassObj("gridobject", i);

        if (pshape->rect().contains(pos))
            if (pshape->show())
                return pshape;
    }
    isize = m_parserOwner.ObjectCount("fastmatch");
    for (int i = 0; i < isize; i++)
    {
        Shape* pshape = (Shape*)m_parserOwner.GetClassObj("fastmatch", i);

        if (pshape->rect().contains(pos))
            if (pshape->show())
                return pshape;
    }
    isize = m_parserOwner.ObjectCount("easyorc");
    for (int i = 0; i < isize; i++)
    {
        Shape* pshape = (Shape*)m_parserOwner.GetClassObj("easyorc", i);

        if (pshape->rect().contains(pos))
            if (pshape->show())
                return pshape;
    }
    return 0;
}

void ViewController::onMouseButton(int theButton, int theAction, int theMods)
{
    if (ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse)
    {
        if (theAction == GLFW_RELEASE) isDragging = false;
        return;
    }
    if (!m_myView.IsNull())
    {
        const Graphic3d_Vec2i aPos = myOcctWindow->CursorPosition();
        if (theAction == GLFW_PRESS)
        {
            m_mousePressPos = gp_Pnt(aPos.x(),aPos.y() ,0);
            isDragging = true;
             myContext->Select(true);
             if (true == m_ilinescan
                 && 0 == theButton)
             {
                 if (m_mousePressPos.X() >= m_current_window_posx
                     && m_mousePressPos.Y() >= m_current_window_posy
                     && m_mousePressPos.X() < m_current_window_posx + m_imguiw
                     && m_mousePressPos.Y() < m_current_window_posy + m_imguih)
                 {
                 }
                 else
                 if (0 == m_ibtntimes)
                 {
                     m_point0 = gp_Pnt(aPos.x() , aPos.y() , 0);
                 }
             }
             else if (true == m_ipickpoints
                 && 0 == theButton)
             {
                 if (m_mousePressPos.X() >= m_current_window_posx
                     && m_mousePressPos.Y() >= m_current_window_posy
                     && m_mousePressPos.X() < m_current_window_posx + m_imguiw
                     && m_mousePressPos.Y() < m_current_window_posy + m_imguih)
                 {
                 }
                 else
                 {
                     m_apoints->addpoint((int)(m_mousePressPos.X() * m_dscalex), (int)(m_mousePressPos.Y() * m_dscaley));
                     m_apoints->setcolor(0, 0,250);
                     m_apoints->setshow(1);
                 }
             }
             else if (true == m_ipickpoints
                 && 1 == theButton)
             {
                 if (m_mousePressPos.X() >= m_current_window_posx
                     && m_mousePressPos.Y() >= m_current_window_posy
                     && m_mousePressPos.X() < m_current_window_posx + m_imguiw
                     && m_mousePressPos.Y() < m_current_window_posy + m_imguih)
                 {
                 }
                 else
                 {
                     m_bpoints->addpoint((int)(m_mousePressPos.X() * m_dscalex), (int)(m_mousePressPos.Y() * m_dscaley));
                     m_bpoints->setcolor(250, 0, 0);
                     m_bpoints->setshow(1);

                 }
             }
             else if (true == m_ipickpoints
                 && 2 == theButton)
             {
                 if (m_mousePressPos.X() >= m_current_window_posx
                     && m_mousePressPos.Y() >= m_current_window_posy
                     && m_mousePressPos.X() < m_current_window_posx + m_imguiw
                     && m_mousePressPos.Y() < m_current_window_posy + m_imguih)
                 {
                 }
                 else
                 {
                     m_bpoints->clear();
                     m_apoints->clear();


                 }
             }
             else if (true == m_iattachline
                 && 0 == theButton)
             {
                 if (m_mousePressPos.X() >= m_current_window_posx
                     && m_mousePressPos.Y() >= m_current_window_posy
                     && m_mousePressPos.X() < m_current_window_posx + m_imguiw
                     && m_mousePressPos.Y() < m_current_window_posy + m_imguih)
                 {
                 }
                 else
                 {
                     m_apoints->addpoint((int)(m_mousePressPos.X() * m_dscalex), (int)(m_mousePressPos.Y() * m_dscaley));
                     m_apoints->setcolor(0, 250, 0);
                     m_apoints->setshow(8);
                 }
             }

             if (myContext->HasDetected())
             {
                 Handle(AIS_InteractiveObject) detectedShape = myContext->DetectedInteractive();
                 selectedShape = Handle(AIS_Shape)::DownCast(detectedShape);
             }
             PressMouseButton(aPos, mouseButtonFromGlfw(theButton), keyFlagsFromGlfw(theMods), false);
        }
        else
        {

            ReleaseMouseButton(aPos, mouseButtonFromGlfw(theButton), keyFlagsFromGlfw(theMods), false);

            if (m_mousePressPos.X() >= m_current_window_posx
                && m_mousePressPos.Y() >= m_current_window_posy
                && m_mousePressPos.X() < m_current_window_posx + m_imguiw
                && m_mousePressPos.Y() < m_current_window_posy + m_imguih)
            {
            }
            else
            if (true == m_ilinescan && 1 == theButton)
            {
                if(0==m_ibtntimes)
                {
                    m_ibtntimes = 1;
                }
                else if (1 == m_ibtntimes)
                {
                    m_ibtntimes = 0;
                    m_point1 = gp_Pnt(aPos.x(), aPos.y(), 0);
                    m_afindline->setlinesegment(m_point0.X() * m_dscalex, m_point0.Y() * m_dscaley, m_point1.X() * m_dscalex, m_point1.Y() * m_dscaley, 80);
                    m_afindline->setshow(1);
                }
            }
            if (m_mousePressPos.X() >= m_current_window_posx
                && m_mousePressPos.Y() >= m_current_window_posy
                && m_mousePressPos.X() < m_current_window_posx + m_imguiw
                && m_mousePressPos.Y() < m_current_window_posy + m_imguih)
            {
            }
            else
            if (true != m_ipickpoints )
                if (aPos.x() - m_mousePressPos.X() > 100 && aPos.y() - m_mousePressPos.Y() > 100)
            {
                m_shapex->settype(Shape::Rectangle);
                m_shapex->setrect(m_mousePressPos.X() * m_dscalex,
                    m_mousePressPos.Y() * m_dscaley,
                    aPos.x() * m_dscalex - m_mousePressPos.X() * m_dscalex,
                    aPos.y() * m_dscaley - m_mousePressPos.Y() * m_dscaley);
                m_shapex->setshow(1);
            }
             isDragging = false;
        }


        if (0)
        {
            double dx = (aPos.x() / m_dzoomx) - m_dmovx;
            double dy = (aPos.y() / m_dzoomy) - m_dmovy;
            gp_Pnt curpos(dx, dy, 0);
            Shape* pshape = indexAt(curpos);
            if (pshape != 0)
            {
                m_resizeHandlePressed = pshape->resizeHandlez(m_dzoomx, m_dzoomy).contains(curpos);
                if (m_resizeHandlePressed)
                {
                    m_mousePressOffset.SetX(pshape->rect().BottomRight().X() - curpos.X());
                    m_mousePressOffset.SetY(pshape->rect().BottomRight().Y() - curpos.Y());
                }
                else
                {
                    m_mousePressOffset.SetX(curpos.X() - pshape->rect().TopLeft().X());
                    m_mousePressOffset.SetY(curpos.Y() - pshape->rect().TopLeft().Y());
                }
                m_pmousepressshape = pshape;
            }

        }
    }


}

void ViewController::onMouseMove(int thePosX, int thePosY)
{
     if (ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse)
         return;
     const Graphic3d_Vec2i aNewPos(thePosX, thePosY);
     if (true == isDragging)
     {
         if (m_mousePressPos.X() >= m_current_window_posx
             && m_mousePressPos.Y() >= m_current_window_posy
             && m_mousePressPos.X() < m_current_window_posx + m_imguiw
             && m_mousePressPos.Y() < m_current_window_posy + m_imguih)
         {
         }
         else
         if (false == m_ipickpoints)
         if (thePosX - m_mousePressPos.X() > 10 && thePosY - m_mousePressPos.Y() > 10)
         {
             m_shapex->settype(Shape::Rectangle);
             m_shapex->setrect(m_mousePressPos.X() * m_dscalex,
                 m_mousePressPos.Y() * m_dscaley,
                 thePosX * m_dscalex - m_mousePressPos.X() * m_dscalex,
                 thePosY * m_dscaley - m_mousePressPos.Y() * m_dscaley);
             m_shapex->setshow(1);
         }
         if(0)
         if (false == ismove)
         {
            ismove = true;
            m_shape0.setcontext(myContext);
            m_shape0.settype(Shape::Rectangle);
            m_shape0.setrect(m_mousePressPos.X(),
                m_mousePressPos.Y(),
                thePosX - m_mousePressPos.X(),
                thePosY - m_mousePressPos.Y());
            ismove = false;
         }

     }
     if (!m_myView.IsNull())
     {
         if (myContext->HasDetected())
         {
             UpdateMousePosition(aNewPos, PressedMouseButtons(), LastMouseFlags(), false);
         }
     }

}

#include "CxShapeInteractionRunner.h"
#include "CxRuntimeProjectionExecutor.h"
#include "CxUnifiedLog.h"
#include "Findline.h"
#include "Findcircle.h"
#include "Findellipse.h"
#include "FindRect.h"
#include "FindSegmentation.h"
#include "FastMatch.h"

#include <sstream>
#include <iomanip>
#include <chrono>

static std::string GenerateLocalShapeRunId()
{
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    std::stringstream ss;
    ss << "shape_local_" << std::setw(16) << std::setfill('0') << ms.count();
    return ss.str();
}

bool ViewController::RunShapeInteractionSmoke(
    const std::string& tool_manifest_path,
    const std::string& suite_path,
    const std::string& image_manifest_path,
    const std::string& out_dir,
    CxShapeInteractionBatchResult& result)
{
    std::string init_reason;
    if (!m_parserOwner.Initialize(init_reason))
    {
        result.pass = false;
        CXLOG_ERROR("ViewController", "shape_smoke_dispatch", "failed", "parser initialize failed: " + init_reason);
        return false;
    }

    CxShapeInteractionRunner runner;
    CxShapeInteractionOptions options;
    options.tool_manifest_path = tool_manifest_path;
    options.test_suite_path = suite_path;
    options.out_dir = out_dir;

    options.run_id =
        CxUnifiedLog::Instance().IsInitialized()
        ? CxUnifiedLog::Instance().RunId()
        : GenerateLocalShapeRunId();

    options.unified_log_enabled = CxUnifiedLog::Instance().IsInitialized();
    options.unified_log_path = CxUnifiedLog::Instance().IsInitialized()
        ? CxUnifiedLog::Instance().Path().string()
        : "";
    options.unified_log_status = options.unified_log_enabled ? "initialized" : "not_initialized";

    CXLOG_INFO(
        "ViewController",
        "shape_smoke_dispatch",
        "running",
        "run_id=" + options.run_id +
        ", suite=" + suite_path +
        ", manifest=" + tool_manifest_path +
        ", image_manifest=" + image_manifest_path +
        ", out=" + out_dir);

    CxAnnotationToolManifestSnapshot manifest;
    CxShapeTestSuiteSnapshot suite;
    CxScriptImageManifestRuntime image_manifest;
    std::string reason;

    result.manifest_path = tool_manifest_path;
    result.suite_path = suite_path;

    bool manifest_exists = std::filesystem::exists(tool_manifest_path);
    bool suite_exists = std::filesystem::exists(suite_path);

    if (!m_parserOwner.ParseAnnotationToolManifest(tool_manifest_path, manifest, reason))
    {
        result.pass = false;
        result.failure_stage = "manifest_parse";
        result.reason = reason;

        CXLOG_ERROR("ViewController", "shape_smoke_dispatch", "failed", "parse annotation manifest failed: " + reason);

        WriteShapeSuiteLoadReport(out_dir, tool_manifest_path, suite_path,
            manifest_exists, false, suite_exists, false, 0,
            result.failure_stage, result.reason);

        return false;
    }

    if (!m_parserOwner.ParseShapeInteractionSuite(suite_path, suite, reason))
    {
        result.pass = false;
        result.failure_stage = "suite_parse";
        result.reason = reason;

        CXLOG_ERROR("ViewController", "shape_smoke_dispatch", "failed", "parse shape suite failed: " + reason);

        WriteShapeSuiteLoadReport(out_dir, tool_manifest_path, suite_path,
            manifest_exists, true, suite_exists, false, 0,
            result.failure_stage, result.reason);

        return false;
    }

    bool image_manifest_loaded = false;
    if (!image_manifest_path.empty())
    {
        if (!LoadStage25ImageManifestJson(image_manifest_path, image_manifest, reason))
        {
            result.pass = false;
            result.failure_stage = "image_manifest_load";
            result.reason = reason;

            CXLOG_ERROR("ViewController", "shape_smoke_dispatch", "failed", "load image manifest failed: " + reason);

            return false;
        }

        auto validation = ValidateStage25ImageManifest(image_manifest);
        WriteManifestDryRunReport(image_manifest, out_dir);

        if (!validation.ok)
        {
            result.pass = false;
            result.failure_stage = "image_manifest_validation";
            result.reason = "image manifest validation failed";

            CXLOG_ERROR("ViewController", "shape_smoke_dispatch", "failed", "image manifest validation failed");

            return false;
        }

        image_manifest_loaded = true;
    }

    CxRuntimeProjectionExecutor executor;
    CxShapeInteractionBatchResultEx ex_result;
    const bool ok = runner.RunSuite(manifest, suite, image_manifest, executor, options, ex_result);

    result.pass = ex_result.pass;
    for (const auto& ex_case : ex_result.extended_cases)
    {
        CxShapeInteractionCaseResult cr;
        cr.case_id = ex_case.case_id;
        cr.tool_id = ex_case.tool_id;
        cr.shape_kind = ex_case.shape_kind;
        cr.pass = ex_case.pass;
        cr.conclusion = ex_case.status;
        cr.reason = ex_case.reason;
        result.cases.push_back(cr);
    }

    return ok;
}

void ViewController::SyncRuntimeObjectsToShapeElements()
{
    for (const RuntimeObjectView& object : m_manualTest.runtime_objects)
    {
        if (object.type == "Findline")
        {
            Findline* tool = static_cast<Findline*>(
                m_parserDebugBridge.QueryClassObject("Findline", object.name));
            if (tool != nullptr)
            {
                const uint64_t generation =
                    m_annotationLayer.BeginRuntimeOwnerPublish("Findline", object.name);
                tool->PublishDisplayShapes(m_annotationLayer, object.name);
                m_annotationLayer.EndRuntimeOwnerPublish("Findline", object.name, generation);
            }
        }
        else if (object.type == "Findcircle")
        {
            Findcircle* tool = static_cast<Findcircle*>(
                m_parserDebugBridge.QueryClassObject("Findcircle", object.name));
            if (tool != nullptr)
            {
                const uint64_t generation =
                    m_annotationLayer.BeginRuntimeOwnerPublish("Findcircle", object.name);
                tool->PublishDisplayShapes(m_annotationLayer, object.name);
                m_annotationLayer.EndRuntimeOwnerPublish("Findcircle", object.name, generation);
            }
        }
        else if (object.type == "Findellipse")
        {
            Findellipse* tool = static_cast<Findellipse*>(
                m_parserDebugBridge.QueryClassObject("Findellipse", object.name));
            if (tool != nullptr)
            {
                const uint64_t generation =
                    m_annotationLayer.BeginRuntimeOwnerPublish("Findellipse", object.name);
                tool->PublishDisplayShapes(m_annotationLayer, object.name);
                m_annotationLayer.EndRuntimeOwnerPublish("Findellipse", object.name, generation);
            }
        }
        else if (object.type == "FindRect")
        {
            FindRect* tool = static_cast<FindRect*>(
                m_parserDebugBridge.QueryClassObject("FindRect", object.name));
            if (tool != nullptr)
            {
                const uint64_t generation =
                    m_annotationLayer.BeginRuntimeOwnerPublish("FindRect", object.name);
                tool->PublishDisplayShapes(m_annotationLayer, object.name);
                m_annotationLayer.EndRuntimeOwnerPublish("FindRect", object.name, generation);
            }
        }
        else if (object.type == "fastmatch")
        {
            fastmatch* tool = static_cast<fastmatch*>(
                m_parserDebugBridge.QueryClassObject("fastmatch", object.name));
            if (tool != nullptr)
            {
                const uint64_t generation =
                    m_annotationLayer.BeginRuntimeOwnerPublish("fastmatch", object.name);
                tool->PublishDisplayShapes(m_annotationLayer, object.name);
                m_annotationLayer.EndRuntimeOwnerPublish("fastmatch", object.name, generation);
            }
        }
        else if (object.type == "FindSegmentation")
        {
            FindSegmentation* tool = static_cast<FindSegmentation*>(
                m_parserDebugBridge.QueryClassObject("FindSegmentation", object.name));
            if (tool != nullptr)
            {
                const uint64_t generation =
                    m_annotationLayer.BeginRuntimeOwnerPublish("FindSegmentation", object.name);
                tool->PublishDisplayShapes(m_annotationLayer, object.name);
                m_annotationLayer.EndRuntimeOwnerPublish("FindSegmentation", object.name, generation);
            }
        }
    }
}

static const char* ShapeHandleLabel(CxShapeHandleRole role, int vertexIndex)
{
    switch (role)
    {
    case CxShapeHandleRole::Center: return "C";
    case CxShapeHandleRole::Start: return "P0";
    case CxShapeHandleRole::End: return "P1";
    case CxShapeHandleRole::Corner0: return "K0";
    case CxShapeHandleRole::Corner1: return "K1";
    case CxShapeHandleRole::Corner2: return "K2";
    case CxShapeHandleRole::Corner3: return "K3";
    case CxShapeHandleRole::Radius: return "R";
    case CxShapeHandleRole::InnerRadius: return "Rin";
    case CxShapeHandleRole::WidthPositive: return "W+";
    case CxShapeHandleRole::WidthNegative: return "W-";
    case CxShapeHandleRole::RadiusX: return "Rx";
    case CxShapeHandleRole::RadiusY: return "Ry";
    case CxShapeHandleRole::Vertex:
    {
        static char buf[16];
        sprintf_s(buf, "V%d", vertexIndex);
        return buf;
    }
    case CxShapeHandleRole::Body: return "Body";
    default: return "?";
    }
}

static ImU32 ShapeHandleColor(CxShapeHandleRole role, const CxShapeElement& element)
{
    if (element.result_element)
        return IM_COL32(255, 180, 64, 255);
    if (element.editable && element.selected)
        return IM_COL32(80, 255, 170, 255);
    return IM_COL32(160, 160, 200, 255);
}

static ImU32 ShapeElementStrokeColor(const CxShapeElement& element)
{
    if (element.result_element)
        return IM_COL32(255, 180, 64, 255);
    if (element.editable)
        return IM_COL32(80, 255, 170, 200);
    return IM_COL32(160, 160, 200, 200);
}

void ViewController::DrawShapeElementOnImageView(const CxShapeElement& element, ImDrawList* drawList)
{
    if (!element.shape || !element.visible)
        return;

    const float sx = m_imageViewZoom;
    const float sy = m_imageViewZoom;

    auto ImageToScreen = [&](double x, double y) -> ImVec2 {
        return ImVec2(m_annotationImagePosX + (float)x * sx, m_annotationImagePosY + (float)y * sy);
    };

    const ImU32 color = ShapeElementStrokeColor(element);
    const ImU32 selectedColor = IM_COL32(255, 64, 64, 255);
    const float thickness = element.selected ? 3.0f : 2.0f;

    switch (element.shape->kind())
    {
    case CxShapeKind::Line:
    case CxShapeKind::LineGauge:
    {
        CxShapePoint p0, p1;
        if (element.shape->exportLine(p0, p1))
        {
            drawList->AddLine(ImageToScreen(p0.x, p0.y), ImageToScreen(p1.x, p1.y),
                              element.selected ? selectedColor : color, thickness);

            if (element.shape->kind() == CxShapeKind::LineGauge)
            {
                const auto* gauge =
                    dynamic_cast<const LineGaugeShape*>(element.shape.get());
                if (gauge != nullptr)
                {
                    const double dx = p1.x - p0.x;
                    const double dy = p1.y - p0.y;
                    const double length = std::hypot(dx, dy);
                    if (length > 1.0)
                    {
                        const double nx = -dy / length;
                        const double ny = dx / length;
                        const double halfWidth = gauge->halfWidth();
                        const CxShapePoint corners[4] = {
                            {p0.x + nx * halfWidth, p0.y + ny * halfWidth},
                            {p1.x + nx * halfWidth, p1.y + ny * halfWidth},
                            {p1.x - nx * halfWidth, p1.y - ny * halfWidth},
                            {p0.x - nx * halfWidth, p0.y - ny * halfWidth}
                        };
                        for (int i = 0; i < 4; ++i)
                        {
                            const int next = (i + 1) % 4;
                            drawList->AddLine(
                                ImageToScreen(corners[i].x, corners[i].y),
                                ImageToScreen(corners[next].x, corners[next].y),
                                element.selected ? selectedColor : color,
                                thickness);
                        }
                    }
                }
            }
        }
        break;
    }
    case CxShapeKind::Rect:
    {
        std::vector<CxShapePoint> points;
        bool closed;
        element.shape->exportPolyline(points, closed);
        if (points.size() >= 4)
        {
            for (size_t i = 0; i < 4; ++i)
            {
                const size_t j = (i + 1) % 4;
                drawList->AddLine(ImageToScreen(points[i].x, points[i].y), 
                                  ImageToScreen(points[j].x, points[j].y), 
                                  element.selected ? selectedColor : color, thickness);
            }
        }
        break;
    }
    case CxShapeKind::Circle:
    {
        CxShapePoint center;
        double radius, inner_radius;
        if (element.shape->exportCircle(center, radius, inner_radius))
        {
            const ImVec2 c = ImageToScreen(center.x, center.y);
            const float r = (float)radius * sx;
            
            drawList->AddCircle(c, r, element.selected ? selectedColor : color, 32, thickness);

            if (inner_radius > 0)
            {
                const float ir = (float)inner_radius * sx;
                drawList->AddCircle(c, ir, IM_COL32(255, 180, 64, 200), 32, thickness);
            }
        }
        break;
    }
    case CxShapeKind::Ellipse:
    {
        CxShapePoint center;
        double radius_x, radius_y, angle;
        if (element.shape->exportEllipse(center, radius_x, radius_y, angle))
        {
            const ImVec2 c = ImageToScreen(center.x, center.y);
            const float rx = (float)radius_x * sx;
            const float ry = (float)radius_y * sy;
            
            drawList->AddEllipse(c, ImVec2(rx, ry), element.selected ? selectedColor : color, 0.0f, 64, thickness);
        }
        break;
    }
    case CxShapeKind::Polyline:
    {
        std::vector<CxShapePoint> points;
        bool closed;
        element.shape->exportPolyline(points, closed);
        if (points.size() >= 2)
        {
            for (size_t i = 0; i < points.size() - 1; ++i)
            {
                drawList->AddLine(ImageToScreen(points[i].x, points[i].y), 
                                  ImageToScreen(points[i+1].x, points[i+1].y), 
                                  element.selected ? selectedColor : color, thickness);
            }
            if (closed && points.size() >= 3)
            {
                drawList->AddLine(ImageToScreen(points.back().x, points.back().y), 
                                  ImageToScreen(points[0].x, points[0].y), 
                                  element.selected ? selectedColor : color, thickness);
            }
        }
        break;
    }
    case CxShapeKind::Points:
    {
        std::vector<CxShapePoint> points;
        element.shape->exportPoints(points);
        for (const auto& pt : points)
        {
            const ImVec2 p = ImageToScreen(pt.x, pt.y);
            drawList->AddCircleFilled(p, 4.0f, color);
            drawList->AddCircle(p, 5.0f, IM_COL32(255, 255, 255, 255), 8, 1.5f);
        }
        break;
    }
    }

    if (element.editable)
    {
        std::vector<CxShapeHandle> handles;
        element.shape->enumerateHandles(handles);

        for (const CxShapeHandle& h : handles)
        {
            if (h.role == CxShapeHandleRole::Body)
                continue;

            ImVec2 p = ImageToScreen(h.p.x, h.p.y);
            const char* label = h.label.empty() ? ShapeHandleLabel(h.role, h.vertex_index) : h.label.c_str();
            ImU32 fill = ShapeHandleColor(h.role, element);

            drawList->AddCircleFilled(p, 8.0f, IM_COL32(0, 0, 0, 220));
            drawList->AddCircleFilled(p, 6.0f, fill);
            drawList->AddCircle(p, 8.0f, IM_COL32(255, 255, 255, 255), 20, 1.5f);
            drawList->AddText(ImVec2(p.x + 8.0f, p.y - 12.0f),
                              IM_COL32(255, 255, 255, 255),
                              label);
        }
    }
}
