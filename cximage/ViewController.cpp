#include "viewcontroller.h"
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

#ifndef CXCORE_ENABLE_VIEWCONTROLLER_CUDA
#define CXCORE_ENABLE_VIEWCONTROLLER_CUDA 0
#endif

 //gpu
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
  //! Convert GLFW mouse button into Aspect_VKeyMouse.
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

  //! Convert GLFW key modifiers into Aspect_VKeyFlags.
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
#define REFRESH_DELAY 10  // ms
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
    //          同     茫 确      CUDA           
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
    //          同     茫 确      CUDA           
    cudaDeviceSynchronize();
    getLastCudaError("Filtering kernel execution failed.\n");
}

void InitializeTextureAndPBO(int imageW, int imageH) {
    //      PBO
    glGenBuffers(1, &gl_PBO);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, gl_PBO);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, imageW * imageH * sizeof(uchar4), NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    // 注   PBO    CUDA
    cudaGraphicsResource* cuda_pbo_resource;
    checkCudaErrors(cudaGraphicsGLRegisterBuffer(&cuda_pbo_resource, gl_PBO,
        cudaGraphicsMapFlagsWriteDiscard));

    //      OpenGL     
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
    // Step 1: 映   PBO    CUDA
    TColor* d_dst = NULL;
    size_t num_bytes;

    checkCudaErrors(cudaGraphicsMapResources(1, &cuda_pbo_resource, 0));
    getLastCudaError("cudaGraphicsMapResources failed");

    checkCudaErrors(cudaGraphicsResourceGetMappedPointer(
        (void**)&d_dst, &num_bytes, cuda_pbo_resource));
    getLastCudaError("cudaGraphicsResourceGetMappedPointer failed");

    // Step 2: 使   CUDA     图  
    runImageFilters(d_dst);

    // 同  确     
    checkCudaErrors(cudaDeviceSynchronize());

    // Step 3:    映  
    checkCudaErrors(cudaGraphicsUnmapResources(1, &cuda_pbo_resource, 0));

    // Step 4:      OpenGL     
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

    // Step 1:      OpenGL     
    GLuint gl_texture_id;
    glGenTextures(1, &gl_texture_id);
    glBindTexture(GL_TEXTURE_2D, gl_texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, src.cols, src.rows, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, src_rgba.data);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Step 2: 注   OpenGL       CUDA
    cudaGraphicsResource* cuda_tex_resource;
    cudaCheckErrors(cudaGraphicsGLRegisterImage(&cuda_tex_resource, gl_texture_id,
        GL_TEXTURE_2D, cudaGraphicsMapFlagsWriteDiscard));

    // Step 3: 映    源   洗    荩      要  
    void* dev_ptr;
    size_t num_bytes;
    cudaCheckErrors(cudaGraphicsMapResources(1, &cuda_tex_resource, 0));
    cudaCheckErrors(cudaGraphicsSubResourceGetMappedArray((cudaArray**)&dev_ptr, cuda_tex_resource, 0, 0));

    //           cudaMemcpyToArray        荩 
    cudaChannelFormatDesc channel_desc = cudaCreateChannelDesc<uchar4>();
    cudaMemcpyToArray((cudaArray*)dev_ptr, 0, 0, src_rgba.data,
        src.cols * src.rows * sizeof(uchar4),
        cudaMemcpyHostToDevice);

    cudaCheckErrors(cudaGraphicsUnmapResources(1, &cuda_tex_resource, 0));

    //      OpenGL      ID    ImGui
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

// ================================================================
// Function : run
// Purpose  :
// ================================================================
void ViewController::run()
{ 
    int iw = 2048 / 2;// 1224;
    int ih = 1536 / 2;// 1024;
  initWindow (iw, ih, "glfw occt image ai");
  initViewer(iw,ih);
  initDemoScene();

  //   始   PBO    Texture
#if CXCORE_ENABLE_VIEWCONTROLLER_CUDA
  InitializeTextureAndPBO(2048, 1536);
#endif
  Imgui_OpenCV_Ini0();
  mainloop();
  cleanup();
}

void ViewController::initImGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //      ImGui   式
    ImGui::StyleColorsDark();

    //   始   ImGui 平台    染     
    ImGui_ImplGlfw_InitForOpenGL(myOcctWindow->getGlfwWindow(), true);
    ImGui_ImplOpenGL3_Init("#version 450"); // 确  使    确   GLSL  姹?
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
  //   示GLFW    希       拇      汀             为  装 未  冢 也    没 斜呖 
  //glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
  myOcctWindow = new Window (theWidth, theHeight, theTitle);
  glfwSetWindowUserPointer       (myOcctWindow->getGlfwWindow(), this);
  // window callback
  glfwSetWindowSizeCallback      (myOcctWindow->getGlfwWindow(), ViewController::onResizeCallback);

  glfwSetFramebufferSizeCallback (myOcctWindow->getGlfwWindow(), ViewController::onFBResizeCallback);
  // mouse callback
  glfwSetScrollCallback          (myOcctWindow->getGlfwWindow(), ViewController::onMouseScrollCallback);
  //  诔 始   锥       臧磁?氐  
  glfwSetMouseButtonCallback     (myOcctWindow->getGlfwWindow(), ViewController::onMouseButtonCallback);
  //         贫  氐  
  glfwSetCursorPosCallback       (myOcctWindow->getGlfwWindow(), ViewController::onMouseMoveCallback);


  //
  glfwMakeContextCurrent(myOcctWindow->getGlfwWindow());
  glfwSwapInterval(1); // Enable vsync 

  //   始   GLAD
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
      std::cerr << "Failed to initialize GLAD" << std::endl;
      return ;
  }
  //   示GLFW    希       拇      汀             为  装 未  冢 也    没 斜呖 
  //glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO(); (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();
  // Setup Platform/Renderer backends
  ImGui_ImplGlfw_InitForOpenGL(myOcctWindow->getGlfwWindow(), true);

  const char* glsl_version = "#version 450";
  ImGui_ImplOpenGL3_Init(glsl_version);

  // Our state
  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
  (void)clear_color;

}

double ViewController::GetScale()
{
    if (!m_myView.IsNull())
    {
        //  取  图     啪  螅ù   图 谢 取 浠?   
        //gp_Trsf viewTransformation;
        //myView->Transformation(viewTransformation);

        ////            樱     通          挪  只  
        //Standard_Real scaleFactor = viewTransformation.ScaleFactor();
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
            m_myView->FitAll(0.1);  // margin 0.1
            //double scale = myView->Scale();
            //myView->SetScale(scale * 0.8);
            // myView->SetScale(1);
            m_myView->ZFitAll();
            m_myView->Update();
            //ViewerUpDate();
            ////   取  图    
            //Standard_Real vx, vy, vz;
            //myView->Proj(vx, vy, vz);
            //Standard_Real zoomX = vx;
            //Standard_Real zoomY = vy;

            //          械         
            //zoomX = matrix(1, 1); // X          
            //zoomY = matrix(2, 2); // Y          
            //Standard_Real scale = 1;
            //myView->SetScale(scale);

            //   取  前选 械亩   
            //Handle(AIS_InteractiveObject) pickedObject = GetDocument()->myAISContext->Current();
            //if (!pickedObject.IsNull())
            //{
            //	//     一   碌  Prs3d_Drawer     
            //	Handle(Prs3d_Drawer) drawer = pickedObject->Attributes();
            //	//   取 Aspect_LineStyle          呖 为 洗 值  实 旨哟 效  
            //	Handle(Prs3d_LineAspect) lineStyle = drawer->LineAspect();
            //	lineStyle->SetWidth(3.0); //      呖 为3.0     愿     要    

            //	drawer->SetColor(Quantity_NOC_PURPLE);
            //	//    薷暮      应    选 械亩   
            //	GetDocument()->myAISContext->HighlightStyle(pickedObject, drawer);

            //	//       示选 械亩   
            //	GetDocument()->myAISContext->Hilight(pickedObject, true);

            //}
        }
    }
    catch (const Standard_Failure&)
    {
       // PLOGD << "fitAll Debug: " << e.GetStackString() << endl;
        return;
    }
};

void ViewController::ViewerUpDate()
{
   // pDoc->UpDateViewer(view_Name);
}

void ViewController::DisplayShape(const Handle(AIS_InteractiveObject)& AShape, Standard_Boolean isShow)
{
   (void)AShape;
   (void)isShow;
   // pDoc->ShowShape(AShape, view_Name, isShow);
    //FitAll();
    //GetDocument()->myAISContext->SetSelected(AShape,true);
    //GetDocument()->myAISContext->AddSelect(AShape);
}

void ViewController::RemoveAllShapes(Standard_Boolean isUpDate)
{
    (void)isUpDate;
    //pDoc->GetAISContext(view_Name)->EraseAll(isUpDate);
    //pDoc->GetAISContext(view_Name)->RemoveAll(isUpDate);
    // myView->Update();
}

#include <Graphic3d_Texture2Dmanual.hxx>
#include <Image_PixMap.hxx> 
//           蟛⒓   图     莸  GPU
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

    //      要    转    色  式  OpenCV 默     BGR  
    cv::Mat img;
    if (mat.channels() == 3)
        cv::cvtColor(mat, img, cv::COLOR_BGR2RGB);
    else
        img = mat;

    //      Mat     选    实    馗 式
    GLenum format = (img.channels() == 1) ? GL_RED : (img.channels() == 3) ? GL_RGB : GL_RGBA;

    //  洗 图     莸     
    glTexImage2D(GL_TEXTURE_2D, 0, format, img.cols, img.rows, 0, format, GL_UNSIGNED_BYTE, img.data);
    glGenerateMipmap(GL_TEXTURE_2D);

    return textureID;
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
//      myContext   一    效   Handle(AIS_InteractiveContext)     
void ViewController::SetTexturedtoBoxFace(const cv::Mat& image)
{
    //      Image_AlienPixMap     
    Handle(Image_AlienPixMap) occtImage = new Image_AlienPixMap();

    //   始   Image_AlienPixMap
    if (!occtImage->InitZero(Image_Format_RGB, image.cols, image.rows))
    {
        std::cerr << "Failed to initialize Image_AlienPixMap." << std::endl;
        return;
    }

    //      荽  cv::Mat        Image_AlienPixMap
    for (int row = 0; row < image.rows; ++row)
    {
        const uchar* sourceRow = image.ptr<uchar>(row);
        memcpy(occtImage->ChangeRow(row), sourceRow, image.cols * image.channels() * sizeof(uchar));
    }
     
    //      Graphic3d_Texture2Dmanual      
    Handle(Graphic3d_Texture2D) texture = new Graphic3d_Texture2D(
        "MemoryTexture" //         
    );
    texture->SetImage(occtImage);
    texture->EnableRepeat(); //         平  
    texture->EnableSmooth(); //         平  
    texture->EnableModulate(); //            

    //     一   Box
    gp_Ax2 anAxis;
    anAxis.SetLocation(gp_Pnt(0.0, 0.0, 0.0));
    TopoDS_Shape boxShape = BRepPrimAPI_MakeBox(anAxis, 1280, 1024, 50).Shape();
  
    //   取 Box        妫?      XOY 平    妫?
    TopExp_Explorer explorer(boxShape, TopAbs_FACE);

    const  TopoDS_Face& frontFace = TopoDS::Face(explorer.Current());
 
    if (frontFace.IsNull())
    {
        std::cerr << "Failed to find the front face of the box." << std::endl;
        return;
    }

    //      AIS_Shape                 
    Handle(AIS_Shape) aBoxAIS = new AIS_Shape(boxShape);

    //           远   
    Handle(Graphic3d_AspectFillArea3d) fillAspect = new Graphic3d_AspectFillArea3d();
    fillAspect->SetTextureMapOn(true);
    fillAspect->SetTextureMap(texture);

    //      Drawer           影    
    Handle(Prs3d_Drawer) drawer = new Prs3d_Drawer();
    Handle(Prs3d_ShadingAspect) shadingAspect = new Prs3d_ShadingAspect();
    shadingAspect->SetAspect(fillAspect);
    drawer->SetShadingAspect(shadingAspect);

    // 应   Drawer         AIS_Shape
    Handle(AIS_Shape) frontFaceAIS = new AIS_Shape(frontFace);
    frontFaceAIS->SetAttributes(drawer);

    //                拥               示
    myContext->Display(aBoxAIS, Standard_True);
    myContext->Display(frontFaceAIS, Standard_True);
   // myContext->Display(aBox, AIS_Shaded, 0, false);
    //       图  确        确  示
    myContext->UpdateCurrentViewer();

    //       图为  色模式
    myContext->SetDisplayMode(AIS_Shaded, true);
}

void ViewController::SetTexturedtoPlane(const cv::Mat& image)
 {
        //      Image_AlienPixMap     
        Handle(Image_AlienPixMap) occtImage = new Image_AlienPixMap();

        //   始   Image_AlienPixMap
        if (!occtImage->InitZero(Image_Format_RGB, image.cols, image.rows))
        {
            std::cerr << "Failed to initialize Image_AlienPixMap." << std::endl;
            return;
        }

        //      荽  cv::Mat        Image_AlienPixMap
        for (int row = 0; row < image.rows; ++row)
        {
            const uchar* sourceRow = image.ptr<uchar>(row);
            memcpy(occtImage->ChangeRow(row), sourceRow, image.cols * image.channels() * sizeof(uchar));
        }

        //      Texture     
        Handle(Graphic3d_Texture2D) texture = CreateTextureFromImage(occtImage);

        //     一  平  ,      XOY 平  
        gp_Pln plane(gp::XOY());

        //     平  谋呓 
        BRepBuilderAPI_MakeEdge mkEdge1(gp_Pnt(0, 0, 0), gp_Pnt(1024, 0, 0));
        BRepBuilderAPI_MakeEdge mkEdge2(gp_Pnt(1024, 0, 0), gp_Pnt(1024, 768, 0));
        BRepBuilderAPI_MakeEdge mkEdge3(gp_Pnt(1024, 768, 0), gp_Pnt(0, 768, 0));
        BRepBuilderAPI_MakeEdge mkEdge4(gp_Pnt(0, 768, 0), gp_Pnt(0, 0, 0));

        BRepBuilderAPI_MakeWire mkWire;
        mkWire.Add(mkEdge1.Edge());
        mkWire.Add(mkEdge2.Edge());
        mkWire.Add(mkEdge3.Edge());
        mkWire.Add(mkEdge4.Edge());

        // 使   BRepBuilderAPI_MakeFace      TopoDS_Shape 平  
        BRepBuilderAPI_MakeFace mkFace(plane, mkWire.Wire());
        TopoDS_Shape planeShape = mkFace.Face();

        //      AIS_Shape     
        Handle(AIS_Shape) planeAIS = new AIS_Shape(planeShape);

        //           远   
        Handle(Graphic3d_AspectFillArea3d) fillAspect = new Graphic3d_AspectFillArea3d();
        fillAspect->SetTextureMapOn();
        fillAspect->SetTextureMap(texture);

        //      Drawer           影    
        Handle(Prs3d_Drawer) drawer = new Prs3d_Drawer();
        Handle(Prs3d_ShadingAspect) shadingAspect = new Prs3d_ShadingAspect();
        shadingAspect->SetAspect(fillAspect);
        drawer->SetShadingAspect(shadingAspect);

        // 应   Drawer    AIS_Shape
        planeAIS->SetAttributes(drawer);

        //          平    拥               示
        myContext->Display(planeAIS, Standard_True);
}


Handle(Graphic3d_Texture2D) ViewController::CreateTextureFromImage(const Handle(Image_PixMap)& image)
{
    if (image.IsNull())
    {
        std::cerr << "Invalid Image_PixMap provided." << std::endl;
        return nullptr;
    }

    //      Graphic3d_Texture2Dmanual     
    Handle(Graphic3d_Texture2D) texture = new Graphic3d_Texture2D(
        "MemoryTexture" //         
    );
    texture->SetImage(image);

    return texture;
}

Handle(Image_PixMap)  ViewController::ConvertCvMatToOcctImage(const cv::Mat& mat)
{
    Handle(Image_AlienPixMap) pixMap = new Image_AlienPixMap();

    //      Image_PixMap        每 取  叨群 通    
    if (mat.type() == CV_8UC3)
    {
        if (!pixMap->InitZero(Image_Format_RGB, mat.cols, mat.rows))
            return nullptr;
        //      荽  cv::Mat        Image_PixMap
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
         //      荽  cv::Mat        Image_AlienPixMap        Alpha 通  
        for (int row = 0; row < mat.rows; ++row)
        {
             const uchar* sourceRow = mat.ptr<uchar>(row); //   取  前 械 指  
             uchar* destRow = pixMap->ChangeRow(row);     //   取目   械 指  
             for (int col = 0; col < mat.cols; ++col)
             {
                // RGB     
                destRow[col * 4 + 0] = sourceRow[col * 3 + 2]; // R (OpenCV    BGR   式)
                destRow[col * 4 + 1] = sourceRow[col * 3 + 1]; // G
                destRow[col * 4 + 2] = sourceRow[col * 3 + 0]; // B
                // Alpha 通      为 255    全  透    
                destRow[col * 4 + 3] = 255; // A
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
    //   取  前  图   诘某叽 
    Standard_Integer viewWidth, viewHeight;
    myView->Window()->Size(viewWidth, viewHeight);

    //       知      图   实 食叽 
    int imageWidth = 2048;// 2448; //     图    
    int imageHeight = 1536;// 2048; //     图    

    //    帽   图     示模式
    //myView->SetBackgroundImage(imagePath, Aspect_FM_STRETCH, true);

    // 使 帽   图  某叽   为模 偷谋呓  
    Bnd_Box boundingBox;
    double modelWidth = imageWidth;
    double modelHeight = imageHeight;

    //     模  位  XY平   希 Z     系暮  为0       实           
    boundingBox.Update(0, 0, 0, modelWidth, modelHeight, 0);

    //     模 捅呓       图    
    double centerX = modelWidth / 2.0;
    double centerY = modelHeight / 2.0;
    double centerZ = 0.0; // 模     牡 Z    
    (void)centerZ;

    //        位 没  咏 
    myView->SetSize(std::max(modelWidth, modelHeight));
    myView->ZFitAll();
    myView->SetCenter(centerX, centerY);

    //       图
    myView->Redraw();
}

void ViewController::SetBackgroundInView(Handle(V3d_View)& view, const cv::Mat& image)
{
    //    cv::Mat 转  为 Image_PixMap
    Handle(Image_PixMap) occtImage = ConvertCvMatToOcctImage(image);
   /* if (!occtImage.IsNull())
    {
        view->SetBackgroundImage(occtImage);
        view->Redraw();
    }
    else
    {
        std::cerr << "Failed to convert image." << std::endl;
    }
    */
    if (!occtImage.IsNull())
    {
        //      Texture 
        Handle(Graphic3d_Texture2Dmanual) texture = CreateTextureFromImage(occtImage);

        if (!texture.IsNull())
        {
 
            // Aspect_FM_CENTER: 图       示
            // Aspect_FM_TILE: 图  平    示
            // Aspect_FM_STRETCH: 图          应  图
            //      图  应 酶   
            //m_myView->ZFitAll();
            //m_myView->Redraw();
            //   帽   图  
            //view->SetBackgroundType(Aspect_BT_TEXTURE);
            //强   鼗   图 愿     示
            //view->Redraw();
            Standard_Real scale = 1;
            view->SetScale(scale);

            //       图  投影    为    投影
            view->SetProj(V3d_XposYposZpos); //    鸥  Z  岱?      投影

            //      拥 位 煤 投影平  
           // Eye位   ( 鄄  )
            view->SetEye(0, 0, -100);

            // At位   ( 鄄 目   )
            view->SetAt(0, 0, 0);

            // Up     (      图   戏   )
            view->SetUp(0, -1, 0);

            //       图    确 围
            view->SetDepth(100); //  拥愕酵队捌? 木   为 100   位

            //       图     疟   
            view->SetZoom(1); //        疟   为 1.0

            //   取  前  图   诘某叽 
            Standard_Integer viewWidth, viewHeight;
            view->Window()->Size(viewWidth, viewHeight);

            //       知      图   实 食叽 
            int imageWidth = image.cols;//   2448; //     图    
            int imageHeight = image.rows; //2048; //     图    


            m_dscalex = (1.0*imageWidth)/(1.0* viewWidth);
            m_dscaley = (1.0* imageHeight )/(1.0 * viewHeight);
            //    帽   图     示模式
            //view->SetBackgroundImage(imagePath, Aspect_FM_STRETCH, true);

            //       图 谋   图  
            view->SetBackgroundImage(texture, Aspect_FM_STRETCH, true);
            // 使 帽   图  某叽   为模 偷谋呓  
           // Bnd_Box boundingBox;
            double modelWidth = imageWidth;
            double modelHeight = imageHeight;

            //     模  位  XY平   希 Z     系暮  为0       实           
           // boundingBox.Update(0, 0, 0, modelWidth, modelHeight, 0);

            //     模 捅呓       图    
            double centerX = modelWidth / 2.0;
            double centerY = modelHeight / 2.0;
            double centerZ = 0.0; // 模     牡 Z    

            //        位 没  咏 
            view->SetSize(std::max(modelWidth, modelHeight));
            view->ZFitAll();
            view->SetCenter(centerX, centerY);
            
            //       图
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
 
// ================================================================
// Function : initViewer
// Purpose  :
// ================================================================
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
    //aViewer->SetDefaultTypeOfView(V3d_ORTHOGRAPHIC);
    if(0)
    aViewer->ActivateGrid(Aspect_GT_Rectangular, Aspect_GDM_Lines);
    //myView = aViewer->CreateView();

    m_myView = new V3d_CustomView(aViewer);
 
    m_myView->SetImmediateUpdate(false);
    //////////////////////////////////////////////
    m_myView->SetWindow(myOcctWindow, myOcctWindow->NativeGlContext());
    m_myView->ChangeRenderingParams().ToShowStats = true;
    myContext = new AIS_InteractiveContext(aViewer);
 
    //
    Handle(Prs3d_Drawer) aSelectionStyle = myContext->SelectionStyle();
    aSelectionStyle->SetColor(Quantity_NOC_WHITE);
    myContext->SetSelectionStyle(aSelectionStyle);

  //       图为    投影
    m_myView->SetProj(V3d_TypeOfOrientation::V3d_XposYposZpos);

    //       图 姆         使X      为      Y      为      Z  指    幕  
    // Eye位   ( 鄄  )
    Standard_Real eyeX = 0.0, eyeY = 0.0, eyeZ =  -500.0; //  鄄    Z 岣?     
    m_myView->SetEye(eyeX, eyeY, eyeZ);

    // At位   ( 鄄 目   )
    Standard_Real atX = 0.0, atY = 0.0, atZ = 0.0; //  鄄 目     原  
    m_myView->SetAt(atX, atY, atZ);

    // Up     (      图   戏   )
    Standard_Real upX = 0.0, upY = -1.0, upZ = 0.0; // Y      为      
    m_myView->SetUp(upX, upY, upZ);
   //       图   牡    辖 1024, 800
   // Standard_Integer screenWidth = 1024;
   // Standard_Integer screenHeight = 800;
   // Standard_Real centerX =  (screenWidth / 2);
   // Standard_Real centerY =  (screenHeight / 2);

    //       图    确 围
     m_myView->SetDepth(100); //  拥愕酵队捌? 木   为 100   位
     m_myView->SetCenter(theWidth, theHeight);
  
     Standard_Real scale = 1;
     m_myView->SetScale(scale);
   
     //       图  投影    为    投影
     m_myView->SetProj(V3d_XposYposZpos); //    鸥  Z  岱?      投影

     //      拥 位 煤 投影平  
    // Eye位   ( 鄄  )
     m_myView->SetEye(0, 0, -10000);

     // At位   ( 鄄 目   )
     m_myView->SetAt(0, 0, 0);

     // Up     (      图   戏   )
     m_myView->SetUp(0, -1, 0);

     //       图    确 围
     m_myView->SetDepth(10000); //  拥愕酵队捌? 木   为 100   位

     //       图     疟   
     m_myView->SetZoom(1); //        疟   为 1.0
     
    // m_myView->SetCenter(theWidth+15, theHeight+15);
     m_myView->SetCenter(theWidth , theHeight );
    //       图  应 酶   
    // m_myView->FitAll();
     
    //       图  应 酶   
    m_myView->ZFitAll();
    m_myView->Redraw();


}
void ViewController::drawline()
{
    //       始  徒     
    gp_Pnt pn_Start;
    pn_Start.SetX(10);
    pn_Start.SetY(20);
    pn_Start.SetZ(0); //     Z    为0

    gp_Pnt pn_End;
    pn_End.SetX(50);
    pn_End.SetY(60);
    pn_End.SetZ(0); //     Z    为0

    //        愦?     
    TopoDS_Vertex V1 = BRepBuilderAPI_MakeVertex(pn_Start);
    TopoDS_Vertex V2 = BRepBuilderAPI_MakeVertex(pn_End);

    //       
    TopoDS_Shape aShape = BRepBuilderAPI_MakeEdge(V1, V2);

    //     AIS_Shape    
    Handle(AIS_Shape) aisLine = new AIS_Shape(aShape);

    //   示  缘        
    myContext->Display(aisLine, AIS_Shaded, 0, false);
}

// ================================================================
// Function : initDemoScene
// Purpose  :
// ================================================================
void ViewController::initDemoScene()
{
  if (myContext.IsNull())
  {
    return;
  }
  //      图       转        
   SetAllowZooming(Standard_False);
   SetAllowRotation(Standard_False); 
  
  if (1)
  {  
      //      Manipulator          位  
     //cxtmp  manipulator = new AIS_Manipulator();
    
       //manipulator->sets
      if(0)
      {
         //      manipulator   一  指    牟          指       
         // aisShape       要同  位 玫  AIS_Shape     

         //   取 AIS_Shape  木植  浠?   
          gp_Trsf shapeTrsf = aBox->LocalTransformation();

          //  泳植  浠?       取位    息
          gp_Pnt position = gp_Pnt(shapeTrsf.TranslationPart());

          //   取X 岱?  Y 岱? 
          // 注 猓?   维 占  校 通      要Z 岱?     全确  一      系  
          //    牵      gp_Ax2 只  要X   Y 幔?  强  约   Z    X   Y  牟 私  
          gp_Dir xAxis;
          gp_Dir yAxis;

          if (shapeTrsf.Form() == gp_Identity || shapeTrsf.Form() == gp_Translation) {
              //    没    转    只  平 疲   使  默 系 X   Y 岱? 
              xAxis = gp::DX();
              yAxis = gp::DY();
          }
          else {
              // 使 帽浠?    械   转          X   Y  姆   
              //    然 取 浠?      转    
              gp_Mat rotationMatrix = shapeTrsf.VectorialPart();

              //     X   Y    路   
              xAxis = gp_Dir(rotationMatrix.Column(1)); //   一 写   X 岱? 
              yAxis = gp_Dir(rotationMatrix.Column(2)); //  诙  写   Y 岱? 
          }

          //      gp_Ax2     
          gp_Ax2 axis(position, yAxis, xAxis); // 注  顺  : (位  ,       (Y  ),  慰     (X  ))

          //    貌       位  
      //cxtmp    manipulator->SetPosition(axis);
      } 
 
     //    貌   模式  默      平 啤   转     牛 //AIS_MM_None
     // manipulator->EnableMode(AIS_MM_Translation);
     // manipulator->EnableMode(AIS_MM_Rotation);
     // manipulator->EnableMode(AIS_MM_Scaling);
      
     //     Z   平  
   //cxtmp  manipulator->SetPart(2, AIS_MM_Translation, Standard_False); // 2   示 Z   

     //     Z     转
     // manipulator->SetPart(2, AIS_MM_Rotation, Standard_False);

      //      要      Z       
    //cxtmp  manipulator->SetPart(2, AIS_MM_Scaling, Standard_False);

      //     Z     转
     //cxtmp  manipulator->SetPart(1, AIS_MM_Rotation, Standard_False);

      //      要      Z       
    //cxtmp   manipulator->SetPart(1, AIS_MM_Scaling, Standard_False);
 
      //     Z     转
    //cxtmp   manipulator->SetPart(0, AIS_MM_Rotation, Standard_False);

      //      要      Z       
    //cxtmp   manipulator->SetPart(0, AIS_MM_Scaling, Standard_False);

     //cxtmp  manipulator->SetZoomPersistence(Standard_False);
    //cxtmp   manipulator->SetPart(AIS_MM_Scaling, Standard_False);
     // manipulator->SetPart(AIS_MM_TranslationPlane, Standard_False);
 
     //    呖   使   SetPart     一   越       模式 碌 Z     
     // manipulator->SetPart(AIS_ManipulatorMode_Translation, Standard_False);
     // manipulator->SetPart(AIS_ManipulatorMode_Rotation, Standard_False);
     // manipulator->SetPart(AIS_ManipulatorMode_Scaling, Standard_False);

     //    Manipulator   拥   示      
   //cxtmp   myContext->Display(manipulator, Standard_True);
     //    Manipulator   目       拥   示      
     //    TransformChanged  藕    拥  远  搴? 
  }

  //anAxis.SetLocation (gp_Pnt (25.0, 125.0, 0.0));
  //Handle(AIS_Shape) aCone = new AIS_Shape (BRepPrimAPI_MakeCone (anAxis, 25, 0, 50).Shape());
  //myContext->Display (aCone, AIS_Shaded, 0, false);
  /*
  //  
  gp_Pnt mcs_Start;
  mcs_Start.SetX(10);
  mcs_Start.SetY(20);
  mcs_Start.SetZ(0); //     Z    为0  
  gp_Pnt mcs_End; 
  mcs_End.SetX(50);
  mcs_End.SetY(60);
  mcs_End.SetZ(0); //     Z    为0  
  gp_Pnt mcs_End2;
  mcs_End2.SetX(120);
  mcs_End2.SetY(50);
  mcs_End2.SetZ(0); //     Z    为0  
  m_eline.Init();
  m_eline.mcs_Start = mcs_Start;
  m_eline.mcs_End = mcs_End;
  myContext->Display(m_eline.Draw(), AIS_Shaded, 0, false);

  m_eline2.Init();
  m_eline2.mcs_Start = mcs_Start;
  m_eline2.mcs_End = mcs_End2;
  myContext->Display(m_eline2.Draw(), AIS_Shaded, 0, false);
 
    gp_Pnt mcs_e;
    mcs_e.SetX(320);
    mcs_e.SetY(250);
    mcs_e.SetZ(0); //     Z    为0  
    m_ellipse.Init();
    m_ellipse.maxjorAxis = 80;//    
    m_ellipse.minorAxis =20;//    
    m_ellipse.rotateAngle = 20;//  转 嵌 (    )
 
    m_ellipse.m_pMCS->McsPosition = mcs_e;
    myContext->Display(m_ellipse.Draw(), AIS_Shaded, 0, false);

    gp_Pnt mcs_c;
    mcs_c.SetY(150);
    mcs_c.SetZ(0); //     Z    为0  
    m_ecircle.Init();
    m_ecircle.radius = 280;//   
    m_ecircle.m_pMCS->McsPosition = mcs_c;
    myContext->Display(m_ecircle.Draw(), AIS_Shaded, 0, false);

    gp_Pnt mcs_a;
    mcs_a.SetX(-113);
    mcs_a.SetY(-115);
    mcs_a.SetZ(0); //     Z    为0   

    gp_Pnt mcs_b;
    mcs_b.SetX(23);
    mcs_b.SetY(25);
    mcs_b.SetZ(0); //     Z    为0   
     
    m_erectangle.Init();
    m_erectangle.width = 20;
    m_erectangle.height = 50;
    m_erectangle.rotateAngle = 20;
    m_erectangle.LeftUpPoint = mcs_a;    //        系 
    m_erectangle.RightDownPoint = mcs_b; //        碌  
    myContext->Display(m_erectangle.Draw(), AIS_Shaded, 0, false);

    Handle(AIS_TextLabel) textLabel = new AIS_TextLabel();
    textLabel->SetText("My Label"); //             
    textLabel->SetPosition(gp_Pnt(30, 40, 0)); //        值 位 茫     选         盏 之   某  位  
    textLabel->SetColor(Quantity_NOC_GREEN); //           色为  色
    textLabel->SetFont("Arial"); //         
    textLabel->SetHeight(19); //        指叨  
    
    myContext->Display(textLabel, true); //      直 签  拥   示        
    

    m_eopencloudline.Init();
    gp_Pnt mcs_1;
    mcs_1.SetX(16);
    mcs_1.SetY(29);
    mcs_1.SetZ(0); //     Z    为0   

    gp_Pnt mcs_2;
    mcs_2.SetX(170);
    mcs_2.SetY(28);
    mcs_2.SetZ(0); //     Z    为0   

    gp_Pnt mcs_3;
    mcs_3.SetX(108);
    mcs_3.SetY(27);
    mcs_3.SetZ(0); //     Z    为0   

    gp_Pnt mcs_4;
    mcs_4.SetX(19);
    mcs_4.SetY(206);
    mcs_4.SetZ(0); //     Z    为0   

    gp_Pnt mcs_5;
    mcs_5.SetX(20);
    mcs_5.SetY(205);
    mcs_5.SetZ(0); //     Z    为0   

    gp_Pnt mcs_6;
    mcs_6.SetX(21);
    mcs_6.SetY(24);
    mcs_6.SetZ(0); //     Z    为0   

    m_eopencloudline.CloudLinePoints.push_back(mcs_1);
    m_eopencloudline.CloudLinePoints.push_back(mcs_2);
    m_eopencloudline.CloudLinePoints.push_back(mcs_3);
    m_eopencloudline.CloudLinePoints.push_back(mcs_4);
    m_eopencloudline.CloudLinePoints.push_back(mcs_5);
    m_eopencloudline.CloudLinePoints.push_back(mcs_6);
    myContext->Display(m_eopencloudline.Draw(), AIS_Shaded, 0, false); 
    */
gp_Path m_gpath; 
m_gpath.SetContext(myContext);
m_gpath.SetView(m_myView);
/*
 //   m_shape0.setcontext(myContext);
    m_shape0.settype(Shape::Rectangle);
    m_shape0.setrect(20, 20, 120, 120);
//   m_shape1.setcontext(myContext);
   m_shape1.settype(Shape::Rectangle);
   m_shape1.setrect(200, 200, 120, 120);
//    m_shape2.setcontext(myContext);
    m_shape2.settype(Shape::Rectangle);
    m_shape2.setrect(380, 380, 120, 120);
//    m_shape3.setcontext(myContext);
    m_shape3.settype(Shape::Rectangle);
    m_shape3.setrect(520, 520, 120, 120);
 //   m_findline.setcontext(myContext);
    m_findline.setrect(300, 200, 300, 200);
    */
    m_imageparser.ParserInitialClassFunction(0);
    m_imageparser.SetStream(&m_os);
    m_imageparser.SetCreateCodeStream(&m_createcodeos);

    initialparser();
    //m_findline.SetWHgap(2, 2);
    //m_findline.setshow(4);
    //m_findline.drawshape();
   // Handle(AIS_Shape) ashape0 = m_findline.getshape();
   // manipulator->Attach(ashape0);
   // m_imageparser.RunOptString("ashape0.translate(100,100);");
    if (1)
    {
     //s_img0 = cv::imread("0.jpg");
     //SetTexturedtoBoxFace(s_img0); 
     //SetTexturedtoPlane(s_img0);
     //m_myView->loadimage();
    }
   // SetTexturedtoPlane(s_img0);  
    myContext->SetDisplayMode(AIS_Shaded, true);
 
  /*
  EleArc m_earc;
  EleCalculator m_ecalculator;
  EleCircle m_ecircle;
  EleCloseCloudLine m_eclosecloudline;
  EleOpenCloudLine m_eopencloudline;
  EleDistance m_edistance;
  EleEllipse m_ellipse;
  EleGap m_egap;
  EleGroove m_egroove;
  EleHint m_ehint;
  EleOring m_eoring;
  ElePlane m_eplane;
  ElePointCloud m_epointcloud;
  EleRectangle m_erectangle;
  EleTextDescription m_etextdescription;
  */

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
 
  //      OpenCV 图  
  //s_img0 = cv::imread("0.jpg");

}
//  娲? 一帧 拇   位  
static ImVec2 last_window_pos = ImVec2(0, 0);
void ViewController::mainloop()
{
     int ifirstrun = 1;
     while (!glfwWindowShouldClose(myOcctWindow->getGlfwWindow()))
     {
         glfwWaitEvents();

         if (!m_myView.IsNull())
         {

             if (ImGui::IsMouseDown(ImGuiMouseButton_Left)
                 || ImGui::IsMouseDown(ImGuiMouseButton_Right)
                 || ImGui::IsMouseDown(ImGuiMouseButton_Middle)
                 || ImGui::GetIO().MouseWheel != 0
                 || 1 == ifirstrun)
             {
                 ifirstrun = 0;
                 //      色    然     
                 //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                 //      OpenCASCADE   图 母  潞  鼗 
                 FlushViewEvents(myContext, m_myView, true);
                 m_myView->Redraw();
             }

             //   始 碌  ImGui    
             ImGui_ImplOpenGL3_NewFrame();
             ImGui_ImplGlfw_NewFrame();
             ImGui::NewFrame();
              
             //        ImGui  丶     
             ImGui::Begin("GPU work");

             { //   取  前   诘 位  
                 ImVec2 current_window_pos = ImGui::GetWindowPos(); 
                 m_current_window_posx = current_window_pos.x;
                 m_current_window_posy = current_window_pos.y;

                  m_imguiw = ImGui::GetWindowWidth();
                  m_imguih = ImGui::GetWindowHeight();
                 //  卸洗    欠  贫 
                 if (current_window_pos.x != last_window_pos.x || current_window_pos.y != last_window_pos.y)
                 {  //    诒  贫 
                     ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Window Moved!");
                     ifirstrun = 0;
                     FlushViewEvents(myContext, m_myView, true);
                     m_myView->Redraw();
                 }  //       一帧  位  
                 last_window_pos = current_window_pos;
             }

             ///////////////////////////////////////////////////////////// 
             ImGui::Text("Parser code input here (%s) (%d)", IMGUI_VERSION, IMGUI_VERSION_NUM); 
             ImGui::Checkbox("Show Image", &m_imageshow);
             ImGui::Checkbox("Pick Points", &m_ipickpoints); 
             ImGui::Checkbox("Line Scan", &m_ilinescan);
             ImGui::Checkbox("Attach Line", &m_iattachline);
             //ImGui::Checkbox("Plane Rotate", &m_planerotate);
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

                 // 使 酶呔   时  
                 auto start = std::chrono::high_resolution_clock::now();
                // main_test_B_9();
                 //main_ModelConfigTest();
                 //main_UtilsTest();
                 //main_NetworkShapeTest();
                 if(0)
                 m_imageparser.Compile(text);
                  
                 //    憔?   时  
                 auto end = std::chrono::high_resolution_clock::now();
                 std::chrono::duration<int, std::milli> elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
                 m_iruntimes = elapsed_time.count();

                // myContext->UpdateCurrentViewer(); // 刷    图 苑 映 浠?
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

                 // 使 酶呔   时  
                 auto start = std::chrono::high_resolution_clock::now();

                 m_imageparser.Compile(text2);

                 //    憔?   时  
                 auto end = std::chrono::high_resolution_clock::now();
                 std::chrono::duration<int, std::milli> elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
                 m_iruntimes = elapsed_time.count();

                 // myContext->UpdateCurrentViewer(); // 刷    图 苑 映 浠?
                 ifirstrun = 0;
                 FlushViewEvents(myContext, m_myView, true);
                 m_myView->Redraw();
                 myContext->UpdateCurrentViewer();
                 myContext->UpdateCurrent();

             }

             /*
aimage1.getshape(ashape0);
aimage1.roipyrdown(2);
aimage1.roieasythre(255);
aimage1.Show(1);
             */ 
             //"amatch0.savemodel(\"D:\\test.pat\");\n"
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

                 // 使 酶呔   时  
                 auto start = std::chrono::high_resolution_clock::now();
               

                 m_imageparser.Compile(text3);

                 //    憔?   时  
                 auto end = std::chrono::high_resolution_clock::now();
                 std::chrono::duration<int, std::milli> elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
                 m_iruntimes = elapsed_time.count();

                 // myContext->UpdateCurrentViewer(); // 刷    图 苑 映 浠?
                 ifirstrun = 0;
                 FlushViewEvents(myContext, m_myView, true);
                 m_myView->Redraw();
                 myContext->UpdateCurrentViewer();
                 myContext->UpdateCurrent();

             }
             /*afindline.getshape(ashape0);
             afindline.setwhgap(25, 25);
             afindline.setlinegap(3);
             afindline.setthre(28);
             afindline.setfindsetting(1);
             afindline.measure(aimage1);
             */
             static char text4[512 * 106] =  
                // "apoints0.aptfilter();\n"
                // "apoints0.cluster(1);\n" 
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
                 // 使 酶呔   时  
                 auto start = std::chrono::high_resolution_clock::now();
                 m_imageparser.Compile(text4);
                 //    憔?   时  
                 auto end = std::chrono::high_resolution_clock::now();
                 std::chrono::duration<int, std::milli> elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
                 m_iruntimes = elapsed_time.count();
                 // myContext->UpdateCurrentViewer(); // 刷    图 苑 映 浠?
                 ifirstrun = 0;
                 FlushViewEvents(myContext, m_myView, true);
                 m_myView->Redraw();
                 myContext->UpdateCurrentViewer();
                 myContext->UpdateCurrent();
             }
             static char text5[512 * 106] = 
                //"if(0){afindline.setlinesegment(400, 300, 500, 700, 100);afindline.Show(1);}\n"
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

                 // 使 酶呔   时  
                 auto start = std::chrono::high_resolution_clock::now();

                 m_imageparser.Compile(text5);

                 //    憔?   时  
                 auto end = std::chrono::high_resolution_clock::now();
                 std::chrono::duration<int, std::milli> elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
                 m_iruntimes = elapsed_time.count();

                 // myContext->UpdateCurrentViewer(); // 刷    图 苑 映 浠?
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

                 // 使 酶呔   时  
                 auto start = std::chrono::high_resolution_clock::now();

                 m_imageparser.Compile(text6);

                 //    憔?   时  
                 auto end = std::chrono::high_resolution_clock::now();
                 std::chrono::duration<int, std::milli> elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
                 m_iruntimes = elapsed_time.count();

                 // myContext->UpdateCurrentViewer(); // 刷    图 苑 映 浠?
                 ifirstrun = 0;
                 FlushViewEvents(myContext, m_myView, true);
                 m_myView->Redraw();
                 myContext->UpdateCurrentViewer();
                 myContext->UpdateCurrent();

             }

             if (clicked & 1)
             {
                 clicked = 0;
                 ///
                  // lua_runstring(text);
                 /// 
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
             ImGui::TextWrapped(" ");//m_iruntimes
             //////////////////////////////////////////////////////////////
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

             ImGui::SetNextWindowPos(ImVec2(650, 20), ImGuiCond_FirstUseEver); // Normally user code doesn't need/want to call this because positions are saved in .ini file anyway. Here we just want to make the demo initial state a bit more friendly!
             
             // bool metricW = true;
            // ImGui::ShowMetricsWindow(&metricW);
             //if (1 == m_planerotate)
             //{
             //    m_planerotate = 0;
             //    SetAllowZooming(Standard_True);
             //    SetAllowRotation(Standard_True); 
             //}
             if (1 == m_imageshow)
             {
                 m_imageshow = 0;
                 //image show
                 Image* pshowimage = nullptr;
                 for (int i = 0; i < m_imageparser.GetClassObjSum("Image"); i++)
                 {
                     Image* pimage = (Image*)m_imageparser.GetClassObj("Image", i);
                     if (pimage->getshow() == 1)
                         pshowimage = pimage;
                 }
                 
                 //Module accd1;
                 //GetBackImage
                 ImageManager* pmodule = (ImageManager*)m_imageparser.GetClassObj("Module", "amodule");
                 Image* pmoduleimage = nullptr;
                 int imoduleshow = 0;
                 if (nullptr != pmodule)
                 {
                     pmoduleimage = pmodule->GetBackImage();
                     imoduleshow = pmodule->getshow();
                 }
                 if (0 != pmodule&& imoduleshow > 0 )
                 {   
                     //texture_id0 = CreateTextureFromMat0(pmodule->GetBackImage()->getmat());
                     if(1== imoduleshow)
                        SetBackgroundInView(m_myView, pmodule->GetBackImage()->getmat()); 
                     else if(2== imoduleshow)
                        SetBackgroundInView(m_myView, pmodule->GetMapImage()->getmat());

                     //m_myView->SetCenter(pmodule->GetBackImage()->getWidth()/2 , pmodule->GetBackImage()->getHeight()/2);
                 }
                 else if (nullptr != pshowimage)
                 {
                    //texture_id0 = CreateTextureFromMat0(pshowimage->getmat());
                    SetBackgroundInView(m_myView, pshowimage->getmat());  
                    //m_myView->SetCenter(pshowimage->getWidth()/2, pshowimage->getHeight()/2);
                 }
             } 
             m_shapex = (Shape*)m_imageparser.GetClassObj("Shape", "ashape0");
             m_apoints = (PointsShape*)m_imageparser.GetClassObj("PointsShape", "apoints0");
             m_bpoints = (PointsShape*)m_imageparser.GetClassObj("PointsShape", "apoints1");
             m_afindline = (Findline*)m_imageparser.GetClassObj("Findline", "afindline");
             if (opencvSW)
                 Imgui_OpenCV_Window0(&opencvSW);

             //   染 ImGui
             ImGui::Render();
             ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

             // 刷 麓   
             glfwSwapBuffers(myOcctWindow->getGlfwWindow());
         }
     }
}
// ================================================================
// Function : cleanup
// Purpose  :
// ================================================================
void ViewController::cleanup()
{
  if (!m_myView.IsNull())
  {
      m_myView->Remove();
  }
  if (!myOcctWindow.IsNull())
  {
    myOcctWindow->Close();
  }
  //      ImGui
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwTerminate();
}

void  ViewController::Imgui_OpenCV_Ini0()
{
    //      OpenCV 图  
    s_img0 = cv::imread("1.jpg");
    if (s_img0.empty()) {
        std::cerr << "Failed to load image" << std::endl;
        return;
    }

    //m_myView->SetBackgroundImage(ConvertCvMatToOcctImage(s_img0));
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

    // Normalize the kernel
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

    // Step 1:      OpenGL     
    GLuint gl_texture_id;
    glGenTextures(1, &gl_texture_id);
    glBindTexture(GL_TEXTURE_2D, gl_texture_id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, src.cols, src.rows, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, src_rgba.data);

    glBindTexture(GL_TEXTURE_2D, 0);

    // Step 2: 注   OpenGL       CUDA
    cudaGraphicsResource* cuda_tex_resource;
    cudaCheckErrors(cudaGraphicsGLRegisterImage(&cuda_tex_resource, gl_texture_id,
        GL_TEXTURE_2D, cudaGraphicsMapFlagsWriteDiscard));

    // Step 3: 映    源   洗    荩      要  
    void* dev_ptr;
    size_t num_bytes;
    cudaCheckErrors(cudaGraphicsMapResources(1, &cuda_tex_resource, 0));
    cudaCheckErrors(cudaGraphicsSubResourceGetMappedArray((cudaArray**)&dev_ptr, cuda_tex_resource, 0, 0));

    //           cudaMemcpyToArray        荩 
    cudaChannelFormatDesc channel_desc = cudaCreateChannelDesc<uchar4>();
    cudaMemcpyToArray((cudaArray*)dev_ptr, 0, 0, src_rgba.data,
        src.cols * src.rows * sizeof(uchar4),
        cudaMemcpyHostToDevice);

    cudaCheckErrors(cudaGraphicsUnmapResources(1, &cuda_tex_resource, 0));

    //      OpenGL      ID    ImGui
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

    // Step 1:      OpenGL       洗   始   荩   选  
    GLuint gl_texture_id;
    glGenTextures(1, &gl_texture_id);
    glBindTexture(GL_TEXTURE_2D, gl_texture_id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, imageW, imageH, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, nullptr); //   始  为  

    glBindTexture(GL_TEXTURE_2D, 0);

    // Step 2: 注   OpenGL       CUDA
    cudaGraphicsResource* cuda_tex_resource;
    cudaCheckErrors(cudaGraphicsGLRegisterImage(&cuda_tex_resource, gl_texture_id,
        GL_TEXTURE_2D, cudaGraphicsMapFlagsWriteDiscard));

    // Step 3:      CUDA        洗 原始图      
    cudaChannelFormatDesc channelDesc = cudaCreateChannelDesc<uchar4>();
    cudaArray* cu_array = nullptr;
    cudaCheckErrors(cudaMallocArray(&cu_array, &channelDesc, imageW, imageH));
    cudaCheckErrors(cudaMemcpyToArray(cu_array, 0, 0, src_rgba.data,
        imageW * imageH * sizeof(uchar4),
        cudaMemcpyHostToDevice));

    // Step 4:      CUDA       螅ü  runImageFilters 使 茫 
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

    // Step 5: 映   OpenGL      曰 取 璞钢? 
    uchar4* d_dst = nullptr;
    cudaCheckErrors(cudaGraphicsMapResources(1, &cuda_tex_resource, 0));
    cudaCheckErrors(cudaGraphicsSubResourceGetMappedArray((cudaArray**)&d_dst, cuda_tex_resource, 0, 0));

    // Step 6:     图   瞬           执        CUDA     
    runImageFiltersx((TColor*)d_dst, imageW, imageH, 0, texImage0);

    // Step 7:    映  
    cudaCheckErrors(cudaGraphicsUnmapResources(1, &cuda_tex_resource, 0));

    // Step 8:       源
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

    // Step 1:      OpenGL       洗   始   荩   选  
    GLuint gl_texture_id;
    glGenTextures(1, &gl_texture_id);
    glBindTexture(GL_TEXTURE_2D, gl_texture_id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, imageW, imageH, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, nullptr); //   始  为  

    glBindTexture(GL_TEXTURE_2D, 0);

    // Step 2: 注   OpenGL       CUDA
    cudaGraphicsResource* cuda_tex_resource;
    checkCudaErrors(cudaGraphicsGLRegisterImage(&cuda_tex_resource, gl_texture_id,
        GL_TEXTURE_2D, cudaGraphicsMapFlagsWriteDiscard));

    // Step 3: 使   峁?暮    洗 源图     莸  CUDA    椋?      CUDA        
    cudaTextureObject_t texImage = 0;
    uchar4* h_Src = (uchar4*)src_rgba.data;
    checkCudaErrors(CUDA_MallocArray(&h_Src, imageW, imageH));

    // Step 4: 映   OpenGL      曰 取 璞钢? 
    uchar4* d_dst = nullptr;
    checkCudaErrors(cudaGraphicsMapResources(1, &cuda_tex_resource, 0));
    checkCudaErrors(cudaGraphicsSubResourceGetMappedArray((cudaArray**)&d_dst, cuda_tex_resource, 0, 0));

    // Step 5:     图   瞬           执        CUDA     
    runImageFiltersx((TColor*)d_dst, imageW, imageH,0, texImage);
    cudaDeviceSynchronize();
    // Step 6:    映  
    checkCudaErrors(cudaGraphicsUnmapResources(1, &cuda_tex_resource, 0));

    // Step 7:       源
    checkCudaErrors(CUDA_FreeArray());
    checkCudaErrors(cudaGraphicsUnregisterResource(cuda_tex_resource));

    return gl_texture_id;
}

// 全 直   示        要    实         
//bool g_Diag = false;
//float knnNoise = 1.0f, nlmNoise = 1.0f, lerpC = 0.5f;
//int g_Kernel = 2; //     为2  选  NLM 瞬   
void ViewController::Imgui_GPU_NLM_main(cv::Mat& src_host, cv::Mat& dst_host) {
    if (src_host.empty() || src_host.channels() != 4) { // 确    RGBA  式
        //throw std::invalid_argument("Source image must be RGBA format.");
        if (src_host.empty() || src_host.channels() != 3) {
            throw std::invalid_argument("Source image must be 24-bit (3 channels).");
        }
        cv::Mat rgba;
        // Convert from BGR to BGRA by adding an alpha channel with full opacity
        cv::cvtColor(src_host, rgba, cv::COLOR_BGR2BGRA);
    }

    int imageW = src_host.cols;
    int imageH = src_host.rows;
    size_t num_bytes = imageW * imageH * sizeof(uchar4);

    //     CUDA   椴? src_host       洗 
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

   // cudaTextureObject_t texImage = 0;
    checkCudaErrors(cudaCreateTextureObject(&texImage, &resDesc, &texDesc, nullptr));

    //     目  图    璞?诖 
    TColor* d_dst = nullptr;
    checkCudaErrors(cudaMalloc(&d_dst, num_bytes));

    //     cuda_Copy    图    
    cuda_Copy(d_dst, imageW, imageH, texImage);

    //         璞?  苹     
    size_t imageSize = src_host.step * src_host.rows;
    cv::Mat augmented_host(src_host.size(), src_host.type());
    checkCudaErrors(cudaMemcpy(augmented_host.data, d_dst, imageSize, cudaMemcpyDeviceToHost));

    //       源
    checkCudaErrors(cudaDestroyTextureObject(texImage));
    checkCudaErrors(cudaFreeArray(cu_array));
    checkCudaErrors(cudaFree(d_dst));
    dst_host = augmented_host;
}
/*
void ViewController::Imgui_GPU_NLM_main(cv::Mat& src_host, cv::Mat& dst_host) {
    if (src_host.empty() || src_host.channels() != 4) { // 确    RGBA  式
        throw std::invalid_argument("Source image must be RGBA format.");
    }
    int imageW = src_host.cols;
    int imageH = src_host.rows;
    size_t num_bytes = imageW * imageH * sizeof(uchar4);

    //     CUDA   椴? src_host       洗 
    cudaChannelFormatDesc channelDesc = cudaCreateChannelDesc<uchar4>();
    cudaArray* cu_array = nullptr;
    checkCudaErrors(cudaMallocArray(&cu_array, &channelDesc, imageW, imageH));
    checkCudaErrors(cudaMemcpyToArray(cu_array, 0, 0, src_host.data, num_bytes, cudaMemcpyHostToDevice));

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

    cudaTextureObject_t texImage = 0;
    checkCudaErrors(cudaCreateTextureObject(&texImage, &resDesc, &texDesc, nullptr));

    //     目  图    璞?诖 
    uchar4* d_dst = nullptr;
    checkCudaErrors(cudaMalloc(&d_dst, num_bytes));

    //     cuda_Copy    图    
    dim3 threads(BLOCKDIM_X, BLOCKDIM_Y); //   要    BLOCKDIM_X    BLOCKDIM_Y
    dim3 grid(iDivUp(imageW, BLOCKDIM_X), iDivUp(imageH, BLOCKDIM_Y));
    cuda_Copy(d_dst, imageW, imageH, texImage);
    cuda_Copy(d_dst, imageW, imageH, texImage);

    //     目  图    璞?诖 
    imageW = src_host.cols;
    imageH = src_host.rows;
    size_t num_bytes = src_host.cols * src_host.rows * sizeof(TColor);
    TColor* d_dst = NULL;
    checkCudaErrors(cudaMalloc((void**)&d_dst, num_bytes));



    //         璞?  苹     
    checkCudaErrors(cudaMemcpy(dst_host.data, d_dst, num_bytes, cudaMemcpyDeviceToHost));

    //       源
    checkCudaErrors(cudaDestroyTextureObject(texImage));
    checkCudaErrors(cudaFreeArray(cu_array));
    checkCudaErrors(cudaFree(d_dst));
}

//               (i + j - 1) / j   值
inline int iDivUp(int i, int j) {
    return (i + j - 1) / j;
}

int main_xxxx() {
    cv::Mat src_24bit = cv::imread("path_to_your_image.jpg");
    if (src_24bit.empty()) {
        std::cerr << "Error loading image" << std::endl;
        return -1;
    }

    cv::Mat src_32bit;
    cv::cvtColor(src_24bit, src_32bit, cv::COLOR_BGR2BGRA); // 转  RGBA

    cv::Mat dst_host(src_32bit.size(), src_32bit.type());
    GPU_NLM(src_32bit, dst_host);

    cv::imshow("Original", src_24bit);
    cv::imshow("Copied", dst_host);
    cv::waitKey();
    return 0;
}*/
/*void ViewController::Imgui_GPU_NLM_main(cv::Mat& src_host, cv::Mat& dst_host)
{
    if (src_host.empty() || src_host.channels() != 4) { // 确    RGBA  式
        //throw std::invalid_argument("Source image must be RGBA format.");
        if (src_host.empty() || src_host.channels() != 3) {
            throw std::invalid_argument("Source image must be 24-bit (3 channels).");
        }
        cv::Mat rgba;
        // Convert from BGR to BGRA by adding an alpha channel with full opacity
        cv::cvtColor(src_host, rgba, cv::COLOR_BGR2BGRA);
    }
    //     CUDA            
    cudaChannelFormatDesc channelDesc = cudaCreateChannelDesc<uchar4>();
    cudaArray* cu_array = nullptr;
    checkCudaErrors(cudaMallocArray(&cu_array, &channelDesc, src_host.cols, src_host.rows));
    checkCudaErrors(cudaMemcpyToArray(cu_array, 0, 0, src_host.data, src_host.step * src_host.rows, cudaMemcpyHostToDevice));

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

  //  cudaTextureObject_t texImage = 0;
    checkCudaErrors(cudaCreateTextureObject(&texImage, &resDesc, &texDesc, nullptr));
   
   
    //     目  图    璞?诖 
    imageW = src_host.cols;
    imageH = src_host.rows;
    size_t num_bytes = src_host.cols * src_host.rows * sizeof(TColor);
    TColor* d_dst = NULL; 
    checkCudaErrors(cudaMalloc((void**)&d_dst, num_bytes));

    //     全 直         runImageFilters
    g_Kernel = 0; // 使  NLM 瞬   
    runImageFilters(d_dst);

    //         苹     
    checkCudaErrors(cudaMemcpy(dst_host.data, d_dst, num_bytes, cudaMemcpyDeviceToHost));

    //       源
    checkCudaErrors(cudaDestroyTextureObject(texImage));
    checkCudaErrors(cudaFreeArray(cu_array));
    checkCudaErrors(cudaFree(d_dst));
}
*/
void ViewController::Imgui_GPU_NLM_main0(cv::Mat& src_host, cv::Mat& dst_host)
{
    TColor* d_dst = NULL;
    size_t num_bytes;
    //uchar4* h_Src = NULL;
    uchar4* h_Src = matToUchar4(src_host);
    //     目  图    璞?诖 
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
    // Select kernel size: 5, 7, or <12
    int kernelSize = 11; // Change this value to 5, 7, or <12
    int kernelRadius = kernelSize / 2;
    double sigma = 0; // If sigma is 0, it will be calculated based on kernel size

    if (sigma == 0) {
        sigma = 0.3 * ((kernelRadius - 1) * 0.5 - 1) + 0.8;
    }

    vector<float> h_kernel = createGaussianKernel(kernelSize, sigma);
    float* kernelData = h_kernel.data();

    // Allocate device memory
    unsigned char* d_input = nullptr, * d_output = nullptr;
    float* d_kernel = nullptr;
    size_t imageSize = src_host.step * src_host.rows;
    cudaMalloc(&d_input, imageSize);
    cudaMalloc(&d_output, imageSize);
    cudaMalloc(&d_kernel, sizeof(float) * h_kernel.size());
    cudaMemcpy(d_input, src_host.data, imageSize, cudaMemcpyHostToDevice);
    cudaMemcpy(d_kernel, kernelData, sizeof(float) * h_kernel.size(), cudaMemcpyHostToDevice);

    // Apply Gaussian blur on GPU
    applyGaussianBlurOnGPU(d_input, d_output, src_host.cols, src_host.rows, src_host.channels(), d_kernel, kernelRadius);

    // Copy result back to host
    cv::Mat blurred_host(src_host.size(), src_host.type());
    cudaMemcpy(blurred_host.data, d_output, imageSize, cudaMemcpyDeviceToHost);

    // Apply HSV augmentation
    float hueDelta = 30.0f; //     色  
    float saturationScale = 1.2f; //    颖  投 
    float valueScale = 1.1f; //         

    // Reuse d_input as our working buffer for HSV augmentation
    cudaMemcpy(d_input, blurred_host.data, imageSize, cudaMemcpyHostToDevice);

    applyHSVAugmentationOnGPU(d_input, src_host.cols, src_host.rows, hueDelta, saturationScale, valueScale);

    // Copy result back to host after HSV augmentation
    cv::Mat augmented_host(src_host.size(), src_host.type());
    cudaMemcpy(augmented_host.data, d_input, imageSize, cudaMemcpyDeviceToHost);

    // Free device memory
    cudaFree(d_input);
    cudaFree(d_output);
    cudaFree(d_kernel);
    dst_host = augmented_host;
    //imshow("Blurred Image", blurred_host);
    //waitKey(0);

}

//   循  
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
{//图    
    (void)p_open;
    //     一     诓   示图  
    ImGui::Begin("Image Viewer");
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    //        疟       应   诖 小
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
         /*            
            //     蚀roi
            void erodeVerticalROI(int erosionHeight)  
            //     蚀roi
            void erodeHorizontalROI(int erosionWidth) 
         */

        SetBackgroundInView(m_myView, m_occtimage.getmat());
       // SetBackgroundInView(m_myView, m_occtimage.getmat());
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
        // 应      应  值  值  
        Image octimagez0 = m_occtimage.getROI();
        // 应   Otsu   值  值  
        Image otsuBinaryImage = octimagez0.otsuThresholding(255, cv::THRESH_BINARY | cv::THRESH_OTSU);

        m_occtimage.colorizeROI(cv::Scalar(0, 0, 0));
        m_occtimage.copyResizedToROI(otsuBinaryImage.getmat());
        texture_id0 = CreateTextureFromMat0(m_occtimage.getmat());
        SetBackgroundInView(m_myView, m_occtimage.getmat());
    }
    if (0)
    {
        m_occtimage.copyFromMat(s_img0);
        // 应      应  值  值  

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
            // 应      应  值  值  
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
            // opencv gaussblur
            GaussianBlur(s_img0, blurred, cv::Size(17, 17), 0);
            //         
            texture_id0 = CreateTextureFromMat0(blurred);
            SetBackgroundInView(m_myView, blurred);
        }
        else
        { 
            if (0)
            {
                Image octimagez;
                octimagez.copyFromMat(s_img0);
                //   图   械   通    蟹   
                octimagez.analyzeConnectedComponentsColor(50.0, 100, 0.5, 2.0); //       色  值为50    小   为100      确 围为0.5  2.0
                //   图   械   通    蟹   
                octimagez.analyzeConnectedComponentsPyramid(100, 0.5, 2.0); //       小   为100      确 围为0.5  2.0
                //         
                texture_id0 = CreateTextureFromMat0(octimagez.getmat());

                SetBackgroundInView(m_myView, octimagez.getmat());

            }
            if (0)
            {
                Image octimagez;
                octimagez.copyFromMat(s_img0);
                // 应 媒       态  值  
                Image binaryImage = octimagez.pyramidDynamicThresholding(4, 11, 0); // 使  4            小为11  偏    为0
                //         
                texture_id0 = CreateTextureFromMat0(binaryImage.getmat());
                SetBackgroundInView(m_myView, binaryImage.getmat());
            }
            if (0)
            {
                Image octimagez;
                octimagez.copyFromMat(s_img0);
                //          100     铱 群透叨染     50   氐   通   煤 色   
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
        // cuda gpu gaussblur
        // s_img0 = cv::imread("portrait_noise.bmp");
        
        // Imgui_GPU_Gauss_main(s_img0, blurred);
        // Imgui_GPU_NLM_main(s_img0, blurred);
        //         
        // texture_id0 = CreateTextureFromMat0(blurred);

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
    //(ImTextureID)(intptr_t)
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
// ================================================================
// Function : onResize
// Purpose  :
// ================================================================
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
// ================================================================
// Function : onMouseScroll
// Purpose  :
// ================================================================
void ViewController::onMouseScroll(double theOffsetX, double theOffsetY)
{
    (void)theOffsetX;
    if (!m_myView.IsNull())
    {
        UpdateZoom(Aspect_ScrollDelta(myOcctWindow->CursorPosition(), int(theOffsetY * 8.0)));
    }
}
void ViewController::SetParserValue(const string& codestr, double dvalue)
{ 
    if (!m_imageparser.IsObjectVar(codestr.c_str()))
        return; 
    string astr(codestr+"="+formatNumber(dvalue)+";");
    m_imageparser.Compile(astr.c_str());
}
double ViewController::GetParserValue(const string& codestr)
{ 
    double* pdouble = NULL;
    if (!m_imageparser.IsObjectVar((const char*)codestr.c_str()))
        return 0x00;//0xFFFF; 
    pdouble = (double*)m_imageparser.GetDoubleValue((const char*)codestr.c_str());
    if (NULL == pdouble)
        return 0x00;//0xFFFF;
    double dvalue = 0;
    dvalue = (*pdouble);
    if (dvalue < 0.0000001 && dvalue>0)
        return 0;
    return dvalue;
}
string ViewController::initialparser()
{
    string str;
    bool bresult = m_imageparser.Compile("Module amodule;");
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
    //load module
    string strfile = getlocationstring("./static.h");
    string strcode = loadfilestring(strfile);
    bresult = m_imageparser.Compile(strcode.c_str());
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
    //load roi ini 
    vector<string> files;
    files = DirFileFind(getlocationstring(string("./")), string("*.ums"));
    for (int i = 0; i < files.size(); ++i)
    {
        string filename = files[i];
       
        string getfilename = getFullFileName(filename);
          
        m_strcode = loadfilestring(filename);

        bresult = m_imageparser.Compile(m_strcode.c_str());
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
    for (int i = 0; i < m_imageparser.GetClassObjSum("Image"); i++)
    {
        Image* pimage = (Image*)m_imageparser.GetClassObj("Image", i); 
        if (nullptr != pimage)
            *pimage = Image(2048,1536,CV_32FC3);//Format_ARGB32_Premultiplied
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
    m_imageparser.ClearAll();
}
void ViewController::resetparser()
{
    clearparserobject();
    initialparser(); 
}
Shape* ViewController::indexAt(const gp_Pnt& pos)
{
    int isize = m_imageparser.GetClassObjSum("Shape");
    for (int i = 0; i < isize; i++)
    {
        Shape* pshape = (Shape*)m_imageparser.GetClassObj("Shape", i);

        if (pshape->rect().contains(pos))
            if (pshape->show())
                return pshape;
    }
    isize = m_imageparser.GetClassObjSum("Findline");
    for (int i = 0; i < isize; i++)
    {
        Shape* pshape = (Shape*)m_imageparser.GetClassObj("Findline", i);

        if (pshape->rect().contains(pos))
            if (pshape->show())
                return pshape;
    }
    isize = m_imageparser.GetClassObjSum("findcircle");
    for (int i = 0; i < isize; i++)
    {
        Shape* pshape = (Shape*)m_imageparser.GetClassObj("findcircle", i);

        if (pshape->rect().contains(pos))
            if (pshape->show())
                return pshape;
    }
    isize = m_imageparser.GetClassObjSum("ASR");
    for (int i = 0; i < isize; i++)
    {
        Shape* pshape = (Shape*)m_imageparser.GetClassObj("ASR", i);

        if (pshape->rect().contains(pos))
            if (pshape->show())
                return pshape;
    }
    isize = m_imageparser.GetClassObjSum("findobject");
    for (int i = 0; i < isize; i++)
    {
        Shape* pshape = (Shape*)m_imageparser.GetClassObj("findobject", i);

        if (pshape->rect().contains(pos))
            if (pshape->show())
                return pshape;
    }
    isize = m_imageparser.GetClassObjSum("Imageroi");
    for (int i = 0; i < isize; i++)
    {
        Shape* pshape = (Shape*)m_imageparser.GetClassObj("Imageroi", i);

        if (pshape->rect().contains(pos))
            if (pshape->show())
                return pshape;
    }
    isize = m_imageparser.GetClassObjSum("imagecodeparser");
    for (int i = 0; i < isize; i++)
    {
        Shape* pshape = (Shape*)m_imageparser.GetClassObj("imagecodeparser", i);

        if (pshape->rect().contains(pos))
            if (pshape->show())
                return pshape;
    }
    isize = m_imageparser.GetClassObjSum("gridobject");
    for (int i = 0; i < isize; i++)
    {
        Shape* pshape = (Shape*)m_imageparser.GetClassObj("gridobject", i);
        
        if (pshape->rect().contains(pos))
            if (pshape->show())
                return pshape;
    }
    isize = m_imageparser.GetClassObjSum("fastmatch");
    for (int i = 0; i < isize; i++)
    {
        Shape* pshape = (Shape*)m_imageparser.GetClassObj("fastmatch", i);

        if (pshape->rect().contains(pos))
            if (pshape->show())
                return pshape;
    }
    isize = m_imageparser.GetClassObjSum("easyorc");
    for (int i = 0; i < isize; i++)
    {
        Shape* pshape = (Shape*)m_imageparser.GetClassObj("easyorc", i);

        if (pshape->rect().contains(pos))
            if (pshape->show())
                return pshape;
    }
    return 0;
}
// ================================================================
// Function : onMouseButton
// Purpose  :
// ================================================================
void ViewController::onMouseButton(int theButton, int theAction, int theMods)
{
    if (!m_myView.IsNull())
    {
        const Graphic3d_Vec2i aPos = myOcctWindow->CursorPosition();
        if (theAction == GLFW_PRESS)
        {
            m_mousePressPos = gp_Pnt(aPos.x(),aPos.y() ,0);
            isDragging = true; //    帽  为      拽
            // 执  选     
             myContext->Select(true); // true   示   之前 募         录   
             if (true == m_ilinescan 
                 && 0 == theButton)
             {
                 if (m_mousePressPos.X() >= m_current_window_posx
                     && m_mousePressPos.Y() >= m_current_window_posy
                     && m_mousePressPos.X() < m_current_window_posx + m_imguiw
                     && m_mousePressPos.Y() < m_current_window_posy + m_imguih)
                 {
                     //  卸洗     
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
                 //  卸洗     
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
                     //  卸洗     
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
                     //  卸洗     
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
                     //  卸洗      
                 }
                 else
                 {
                     m_apoints->addpoint((int)(m_mousePressPos.X() * m_dscalex), (int)(m_mousePressPos.Y() * m_dscaley));
                     m_apoints->setcolor(0, 250, 0);
                     m_apoints->setshow(8);
                 }
             }

            //     囟    AIS_Shape  欠 选  
            // if (myContext->IsSelected(selectedShape))
             if (myContext->HasDetected())
             {
                 Handle(AIS_InteractiveObject) detectedShape = myContext->DetectedInteractive();
                 selectedShape = Handle(AIS_Shape)::DownCast(detectedShape);
              //cxtmp  if (!selectedShape.IsNull())
              //cxtmp   manipulator->Attach(selectedShape);  //    拥 目     
              //cxtmp  else
              //cxtmp  {
              //cxtmp     selectedTextLabel = Handle(AIS_TextLabel)::DownCast(detectedShape);
              //cxtmp    if (!selectedTextLabel.IsNull())
              //cxtmp        manipulator->Attach(selectedTextLabel);  //    拥 目      
              //cxtmp  }
             }             
             PressMouseButton(aPos, mouseButtonFromGlfw(theButton), keyFlagsFromGlfw(theMods), false);
        }
        else
        {
            // m_Match.setcontext(myContext);
            // m_Match.SetView(m_myView);
            // m_Match.setrect(m_mousePressPos.X(), m_mousePressPos.Y(), aPos.x()- m_mousePressPos.X(), aPos.y()- m_mousePressPos.Y());
            // m_shape0.setcontext(myContext); 
          
            ReleaseMouseButton(aPos, mouseButtonFromGlfw(theButton), keyFlagsFromGlfw(theMods), false);
           
            if (m_mousePressPos.X() >= m_current_window_posx
                && m_mousePressPos.Y() >= m_current_window_posy
                && m_mousePressPos.X() < m_current_window_posx + m_imguiw
                && m_mousePressPos.Y() < m_current_window_posy + m_imguih)
            {
                //  卸洗     
            }
            else
            if (true == m_ilinescan && 1 == theButton)
            {
                if(0==m_ibtntimes)
                { 
                    m_ibtntimes = 1;
                }
                else if (1 == m_ibtntimes)
                {//
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
                //  卸洗     
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

   //add drag
   // if (false == m_beditmanagerview)
   //     return;
   // mutexupdate();
   // event->accept();//m_dzoomx, m_dzoomy
        if (0)
        {
            double dx = (aPos.x() / m_dzoomx) - m_dmovx;
            double dy = (aPos.y() / m_dzoomy) - m_dmovy;
            gp_Pnt curpos(dx, dy, 0);
            Shape* pshape = indexAt(curpos);//event->pos());
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
// ================================================================
// Function : onMouseMove
// Purpose  :
// ================================================================
void ViewController::onMouseMove(int thePosX, int thePosY)
{  
     const Graphic3d_Vec2i aNewPos(thePosX, thePosY);
     if (true == isDragging)
     {
         if (m_mousePressPos.X() >= m_current_window_posx
             && m_mousePressPos.Y() >= m_current_window_posy
             && m_mousePressPos.X() < m_current_window_posx + m_imguiw
             && m_mousePressPos.Y() < m_current_window_posy + m_imguih)
         {
             //  卸洗     
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
 

 /*
// ================================================================
// Function : onMouseButton
// Purpose  :
// ================================================================
void ViewController::onMouseButton(int theButton, int theAction, int theMods)
{
    if (m_myView.IsNull()) { return; }

    const Graphic3d_Vec2i aPos = myOcctWindow->CursorPosition();
    if (theAction == GLFW_PRESS)
    {
        PressMouseButton(aPos, mouseButtonFromGlfw(theButton), keyFlagsFromGlfw(theMods), false);
    }
    else
    {
        ReleaseMouseButton(aPos, mouseButtonFromGlfw(theButton), keyFlagsFromGlfw(theMods), false);
    }
}

// ================================================================
// Function : onMouseMove
// Purpose  :
// ================================================================
void ViewController::onMouseMove(int thePosX, int thePosY)
{
    const Graphic3d_Vec2i aNewPos(thePosX, thePosY);
    if (!m_myView.IsNull())
    {
        UpdateMousePosition(aNewPos, PressedMouseButtons(), LastMouseFlags(), false);
    }
}
*/

