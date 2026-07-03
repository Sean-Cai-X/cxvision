# CxVision 项目 Code Wiki

## 目录

1. [项目概述](#1-项目概述)
2. [整体架构](#2-整体架构)
3. [核心模块详解](#3-核心模块详解)
   - 3.1 [cximage - 图像处理模块](#31-cximage---图像处理模块)
   - 3.2 [cxgeom - 几何建模模块](#32-cxgeom---几何建模模块)
   - 3.3 [cxcloud - 点云处理模块](#33-cxcloud---点云处理模块)
   - 3.4 [cxparser - 脚本解析引擎](#34-cxparser---脚本解析引擎)
   - 3.5 [cxparser_ext - 脚本扩展层](#35-cxparser_ext---脚本扩展层)
   - 3.6 [3D - 三维场景与工具集成模块](#36-3d---三维场景与工具集成模块)
4. [关键类与函数](#4-关键类与函数)
5. [依赖关系](#5-依赖关系)
6. [构建与运行](#6-构建与运行)
7. [脚本系统 (CxScript)](#7-脚本系统-cxscript)
8. [测试体系](#8-测试体系)

---

## 1. 项目概述

### 1.1 项目简介

CxVision 是一个基于 C++ 的计算机视觉与几何分析平台，集成了图像处理、几何建模、点云处理、脚本自动化以及三维场景工具链。项目采用模块化设计，支持通过自定义脚本语言（CxScript）驱动视觉检测与几何测量工作流。

### 1.2 核心能力

- **图像分析**：边缘检测、特征提取、模板匹配、形态学操作
- **几何建模**：基于 OpenCASCADE 的参数化几何构建与测量
- **点云处理**：点云数据管理、法向量估计、距离分析
- **脚本自动化**：基于 muParser 扩展的 CxScript 领域特定语言
- **3D 工具集成**：Blender、Nova3D 等外部 3D 工具的 MCP 协议桥接
- **GUI 交互**：基于 ImGui + GLFW + OpenCASCADE 的可视化界面

### 1.3 技术栈

| 类别 | 技术 |
|------|------|
| 编程语言 | C++17 / C++14 |
| 构建系统 | CMake 3.21+ |
| 图像库 | OpenCV |
| 几何内核 | OpenCASCADE 7.7.0 |
| GUI 框架 | ImGui + GLFW + OpenGL (GLAD) |
| 脚本引擎 | muParser (扩展定制版) |
| 点云索引 | nanoflann |
| 3D 工具 | Blender, Nova3D |
| 通信协议 | MCP (Model Context Protocol) |

---

## 2. 整体架构

### 2.1 架构分层

```
┌─────────────────────────────────────────────────────────┐
│                    应用层 (Application)                 │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │   GUI 界面   │  │  CLI 命令行  │  │  测试驱动    │  │
│  │  (ImGui)     │  │              │  │  (Smoke)     │  │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘  │
└─────────┼─────────────────┼──────────────────┼──────────┘
          │                 │                  │
┌─────────▼─────────────────▼──────────────────▼──────────┐
│                   编排层 (Orchestration)                 │
│  ┌──────────────────────────────────────────────────┐   │
│  │              ViewController (cximage)            │   │
│  │  视图控制 / 脚本执行 / 状态管理 / 交互绑定        │   │
│  └──────────────────────────────────────────────────┘   │
│  ┌──────────────────────────────────────────────────┐   │
│  │           ThreeDOrchestrator (3D)                │   │
│  │  3D 资产生成 / 场景管理 / 工具编排 / MCP 适配     │   │
│  └──────────────────────────────────────────────────┘   │
└────────────────────────────┬─────────────────────────────┘
                             │
┌────────────────────────────▼─────────────────────────────┐
│                   领域服务层 (Domain Services)            │
│  ┌───────────┐  ┌───────────┐  ┌───────────┐             │
│  │  cximage  │  │  cxgeom   │  │  cxcloud  │             │
│  │  图像服务 │  │  几何服务 │  │  点云服务 │             │
│  └─────┬─────┘  └─────┬─────┘  └─────┬─────┘             │
└────────┼────────────────┼────────────────┼────────────────┘
         │                │                │
┌────────▼────────────────▼────────────────▼────────────────┐
│                  脚本运行时层 (Script Runtime)             │
│  ┌─────────────────────────────────────────────────────┐  │
│  │              cxparser_ext (扩展层)                   │  │
│  │  Pipeline / 绑定构建 / 流程路由 / 验证引擎 / 交付API  │  │
│  └──────────────────────┬──────────────────────────────┘  │
│                         │                                 │
│  ┌──────────────────────▼──────────────────────────────┐  │
│  │              cxparser (核心引擎)                      │  │
│  │  muParser / 字节码 / 类绑定 / 表达式求值              │  │
│  └─────────────────────────────────────────────────────┘  │
└───────────────────────────────────────────────────────────┘
                             │
┌────────────────────────────▼─────────────────────────────┐
│                  基础设施层 (Infrastructure)              │
│  ┌───────────┐  ┌───────────┐  ┌───────────────────┐     │
│  │ OpenCV    │  │ OpenCASCADE│  │  GLFW/ImGui/GLAD  │     │
│  └───────────┘  └───────────┘  └───────────────────┘     │
└───────────────────────────────────────────────────────────┘
```

### 2.2 模块依赖关系图

```
cximage (GUI/Image)
    │
    ├──► cxgeom (几何操作)
    │
    ├──► cxparser (脚本解析)
    │
    └──► OpenCV / OpenCASCADE / ImGui

cxparser_ext (脚本扩展)
    │
    ├──► cxparser (核心解析)
    │
    ├──► cximage (可选 - 真实桥接)
    │
    └──► cxgeom (可选 - 真实桥接)

3D (三维集成)
    │
    ├──► cxgeom (场景适配器)
    │
    ├──► cxcloud (点云适配器)
    │
    └──► 外部工具 (Blender/Nova3D via MCP)
```

### 2.3 目录结构

```
cxvision_repo/
├── CMakeLists.txt              # 根构建脚本 (cxvision_imgui_acceptance)
├── cximage/                    # 图像处理与 GUI 模块
│   ├── Image.h/cpp             # 图像封装类
│   ├── ViewController.h/cpp    # 视图控制器 (核心编排)
│   ├── Shape.h/cpp             # 形状基类
│   ├── Findline/Findcircle/... # 特征检测算法
│   ├── FastMatch.h/cpp         # 快速模板匹配
│   ├── ParserClass.h/cpp       # 解析器桥接类
│   ├── SemanticFlowGraph.h/cpp # 语义流图
│   └── GuiMain.cpp             # GUI 入口
├── cxgeom/                     # 几何建模模块
│   ├── include/                # 公共头文件
│   │   ├── CxGeometryItem.h    # 几何项封装
│   │   ├── CxGeometryOperations.h  # 几何操作
│   │   ├── CxShapeHandle.h     # 形状句柄
│   │   └── ...
│   └── src/                    # 实现文件
├── cxcloud/                    # 点云处理模块
│   ├── include/
│   │   ├── CxCloudItem.h       # 点云项
│   │   ├── CxCloudOperations.h # 点云操作
│   │   ├── CxOctreeAdapter.h   # 八叉树适配器
│   │   └── ...
│   └── src/
├── cxparser/                   # 脚本解析核心
│   ├── muParser*.h/cpp         # muParser 核心文件
│   ├── cxscript/               # CxScript 脚本案例
│   │   ├── module/             # 模块测试脚本
│   │   ├── integration/        # 集成测试脚本
│   │   └── state_machine/      # 状态机示例
│   ├── rag_script_cases/       # RAG 脚本案例库
│   └── CMakeLists.txt
├── cxparser_ext/               # 脚本扩展层
│   ├── pipeline/               # 流水线组件
│   ├── runtime/                # 运行时支持
│   ├── catalog/                # 脚本目录
│   ├── validation/             # 验证引擎
│   ├── meta/                   # 元数据类型
│   ├── drivers/                # 驱动层
│   ├── scenarios/              # 场景封装
│   └── adapters/               # 外部适配器
└── 3D/                         # 三维场景集成模块
    ├── src/                    # 核心实现
    ├── tests/                  # 烟雾测试
    ├── tools/                  # Python 桥接工具
    ├── runtime/                # 运行时资源
    └── CMakeLists.txt
```

---

## 3. 核心模块详解

### 3.1 cximage - 图像处理模块

#### 3.1.1 模块定位

cximage 是项目的核心前端模块，提供图像加载/处理、特征检测、几何可视化以及 GUI 交互界面。它是连接用户操作与底层算法的主要入口。

#### 3.1.2 主要子模块

| 子模块 | 职责 | 核心文件 |
|--------|------|----------|
| 图像基础 | 图像封装、ROI 操作、像素访问 | [Image.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/Image.h) |
| 特征检测 | 直线/圆/椭圆/矩形检测 | [Findline.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/Findline.h), [Findcircle.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/Findcircle.h), [Findellipse.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/Findellipse.h), [FindRect.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/FindRect.h) |
| 模板匹配 | 快速模板匹配算法 | [FastMatch.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/FastMatch.h) |
| 形状系统 | 形状基类与几何形状 | [Shape.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/Shape.h), [shapebase.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/shapebase.h) |
| 视图控制 | 场景渲染、交互、脚本执行 | [ViewController.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/ViewController.h) |
| 表单拟合 | 形位公差/拟合方法 | [FormfitGauge.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/FormfitGauge.h), [FormfitFitMethod.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/FormfitFitMethod.h) |
| 语义流 | 语义流图与状态机 | [SemanticFlowGraph.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/SemanticFlowGraph.h) |
| 图像管理 | 多图像管理 | [imagemanager.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/imagemanager.h) |

#### 3.1.3 核心类：Image

[Image](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/Image.h#L47-L335) 类是对 `cv::Mat` 的封装，提供面向检测任务的高级接口。

**关键能力：**
- ROI（感兴趣区域）管理与操作
- 像素级访问与颜色通道操作
- 边缘检测（Sobel、Laplacian、Canny）
- 阈值处理（固定、自适应、Otsu、金字塔）
- 形态学操作（腐蚀、膨胀、开运算、闭运算）
- 连通组件分析与特征提取
- 亚像素级边缘细化
- 图像合成模式（20+ 混合模式）

**关键方法签名：**
```cpp
// ROI 操作
Image getROI(int startX, int startY, int width, int height) const;
void setROI(const Image& roi, int startX, int startY);

// 边缘检测
Image cannyEdgeDetection(double threshold1, double threshold2, int apertureSize = 3) const;

// 阈值处理
Image adaptiveThresholding(int maxValue, int adaptiveMethod = cv::ADAPTIVE_THRESH_GAUSSIAN_C,
    int thresholdType = cv::THRESH_BINARY, int blockSize = 11, double C = 2) const;

// 连通组件
std::vector<std::vector<cv::Point>> findConnectedComponents(double minArea, int minWidth = 0, int minHeight = 0) const;

// 几何拟合
static std::tuple<cv::Point2f, float> CircleFit_(std::vector<cv::Point2f>& vecPt);
static std::vector<cv::Point2f> SubpixelProcess(const cv::Mat& gray, 
    const std::vector<cv::Point>& pixelPoints, std::vector<int>& boundaryIndices,
    double dThreshold, int localRange = 3, int subPixelDensity = 5);
```

#### 3.1.4 核心类：ViewController

[ViewController](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/ViewController.h#L34-L80) 是 GUI 应用的核心控制器，继承自 OpenCASCADE 的 `AIS_ViewController`。

**职责：**
- 管理 ImGui 界面与 OCCT 3D 视图的集成
- 维护脚本目录与执行状态
- 协调图像、几何、解析器之间的数据流动
- 处理用户交互事件

**关键成员：**
```cpp
void run();                          // 启动事件循环
ScriptResult RunCxScript(const std::string& theScriptPath);  // 执行脚本
void HandleSemanticFlowAction(const SemanticFlowAction& action);  // 语义流处理
```

---

### 3.2 cxgeom - 几何建模模块

#### 3.2.1 模块定位

cxgeom 基于 OpenCASCADE 提供参数化几何建模能力，封装了几何对象的创建、表示、样式管理与场景映射。采用"数据-表示-操作"分离的设计模式。

#### 3.2.2 核心组件

| 组件类别 | 类名 | 职责 |
|----------|------|------|
| 几何项 | [CxGeometryItem](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxgeom/include/CxGeometryItem.h) | 几何对象统一封装（载荷+样式+表示+版本） |
| 形状句柄 | [CxShapeHandle](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxgeom/include/CxShapeHandle.h) | OpenCASCADE 形状的 RAII 封装 |
| 操作接口 | [CxGeometryOperations](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxgeom/include/CxGeometryOperations.h) | 几何增删改操作的静态门面 |
| 曲线构建 | [CxCurveBuilder](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxgeom/include/CxCurveBuilder.h) | 参数化曲线构建器 |
| 曲面构建 | [CxFaceBuilder](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxgeom/include/CxFaceBuilder.h) | 曲面构建器 |
| 线框构建 | [CxWireBuilder](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxgeom/include/CxWireBuilder.h) | 线框构建器 |
| 圆构建 | [CxSetCircleBuild](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxgeom/include/CxSetCircleBuild.h) | 圆构建与显示 |
| 直线构建 | [CxSetLineBuild](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxgeom/include/CxSetLineBuild.h) | 直线构建与显示 |
| 样式 | [CxGeomRenderStyle](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxgeom/include/CxGeomRenderStyle.h) | 几何渲染样式 |
| 表示 | [CxGeomPresentation](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxgeom/include/CxGeomPresentation.h) | 场景表示数据 |
| 测量 | [CxGeomMeasure](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxgeom/include/CxGeomMeasure.h) | 几何测量工具 |
| 场景映射 | [CxSceneMapping](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxgeom/include/CxSceneMapping.h) | 场景实体映射管理 |
| 刷新决策 | [CxRefreshTracker](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxgeom/include/CxRefreshTracker.h) | 增量刷新决策 |
| OCCT 转换 | [CxOcctConvert](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxgeom/include/CxOcctConvert.h) | OCCT 类型转换工具 |

#### 3.2.3 设计模式

**Item-Payload-Style-Presentation 四层模型：**
- **Payload (CxShapeHandle)**：底层几何数据（OCCT TopoDS_Shape）
- **Style (CxGeomRenderStyle)**：渲染样式（颜色、线宽、可见性）
- **Presentation (CxGeomPresentation)**：场景表示状态（AIS 对象引用等）
- **Item (CxGeometryItem)**：统一封装，持有以上三者及版本号

**操作模式：**
[CxGeometryOperations](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxgeom/include/CxGeometryOperations.h#L23-L32) 提供静态操作方法，返回包含脏标志和刷新决策的结果对象：

```cpp
enum class CxGeometryOperationKind { Add, UpdateStyle, ReplacePayload, Remove };

struct CxGeometryOperationResult {
  CxGeometryOperationKind kind;
  CxSceneRevision revisions;
  std::uint32_t dirty_flags;
  CxRefreshDecision decision;
};
```

---

### 3.3 cxcloud - 点云处理模块

#### 3.3.1 模块定位

cxcloud 提供点云数据的管理、操作与渲染能力。设计模式与 cxgeom 保持一致（Item-Payload-Style-RenderData），便于在场景中统一管理。

#### 3.3.2 核心组件

| 组件类别 | 类名 | 职责 |
|----------|------|------|
| 点云项 | [CxCloudItem](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxcloud/include/CxCloudItem.h) | 点云对象统一封装 |
| 点云句柄 | [CxCloudHandle](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxcloud/include/CxCloudHandle.h) | 点云数据 RAII 封装 |
| 点云数据 | [CxPointCloudData](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxcloud/include/CxPointCloudData.h) | 原始点云数据结构 |
| 操作接口 | [CxCloudOperations](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxcloud/include/CxCloudOperations.h) | 点云增删改操作门面 |
| 点云子集 | [CxCloudSubset](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxcloud/include/CxCloudSubset.h) | 点云子集选择 |
| 渲染样式 | [CxCloudRenderStyle](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxcloud/include/CxCloudRenderStyle.h) | 点云渲染样式 |
| 渲染数据 | [CxCloudRenderData](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxcloud/include/CxCloudRenderData.h) | 渲染缓冲数据 |
| 八叉树 | [CxOctreeAdapter](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxcloud/include/CxOctreeAdapter.h) | 八叉树空间索引 |
| 法向量估计 | [CxNormalEstimator](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxcloud/include/CxNormalEstimator.h) | 点云法向量估计 |
| 距离分析 | [CxDistanceAnalyzer](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxcloud/include/CxDistanceAnalyzer.h) | 距离场分析 |
| 场景映射 | [CxCloudSceneMapping](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxcloud/include/CxCloudSceneMapping.h) | 场景映射管理 |

#### 3.3.3 操作结果模型

```cpp
enum CxCloudDirtyFlags : std::uint32_t {
  CxCloudDirtyNone         = 0,
  CxCloudDirtyContent      = 1u << 0,  // 内容变更
  CxCloudDirtyPresentation = 1u << 1,  // 表示变更
  CxCloudDirtyVisibility   = 1u << 2   // 可见性变更
};

enum class CxCloudRefreshHint {
  None, UpdatePresentation, RebuildCloudPresentation, RebuildScene
};
```

---

### 3.4 cxparser - 脚本解析引擎

#### 3.4.1 模块定位

cxparser 是基于 muParser 扩展的脚本解析核心引擎，提供表达式求值、变量绑定、类方法调用、字节码执行等能力。它是 CxScript 语言的执行内核。

#### 3.4.2 muParser 核心架构

| 文件 | 职责 |
|------|------|
| [muParser.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxparser/muParser.h) | 浮点解析器门面，注册内置函数/常量/运算符 |
| [muParserBase.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxparser/muParserBase.h) | 解析器基类，核心解析逻辑 |
| [muParserBytecode.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxparser/muParserBytecode.h) | 字节码生成与管理 |
| [muParserClass.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxparser/muParserClass.h) | 类绑定支持 |
| [muParserClassFunctionReader.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxparser/muParserClassFunctionReader.h) | 类方法读取器 |
| [muParserCallback.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxparser/muParserCallback.h) | 回调函数封装 |
| [muParserError.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxparser/muParserError.h) | 错误处理 |
| [muParserInt.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxparser/muParserInt.h) | 整数解析器 |
| [muParserRun.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxparser/muParserRun.h) | 运行时执行 |
| [muParserStack.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxparser/muParserStack.h) | 求值栈 |
| [muParserStrClassMap.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxparser/muParserStrClassMap.h) | 字符串类映射 |
| [muParserToken.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxparser/muParserToken.h) | Token 定义 |
| [muParserTreeNode.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxparser/muParserTreeNode.h) | 语法树节点 |

#### 3.4.3 内置函数库

**三角函数：**
`Sin, Cos, Tan, ASin, ACos, ATan, Sinh, Cosh, Tanh, ASinh, ACosh, ATanh`

**数学函数：**
`Log2, Log10, Ln, Exp, Abs, Sqrt, Rint, Sign, Ite (if-then-else)`

**聚合函数：**
`Sum, Avg, Min, Max, AvgFilter`

**特殊函数：**
`UnaryMinus, StrToFloat, TestForMultFunc, TestForLPExchange`

#### 3.4.4 测试入口

cxparser 模块包含多个烟雾测试与回归测试入口：

| 测试入口文件 | 测试内容 |
|-------------|----------|
| `basic_regression_main.cpp` | 基础回归测试 |
| `class_binding_smoke_main.cpp` | 类绑定烟雾测试 |
| `control_flow_regression_main.cpp` | 控制流回归测试 |
| `custom_type_contract_smoke_main.cpp` | 自定义类型契约测试 |
| `cxcore_contract_script_smoke_main.cpp` | cxcore 契约脚本测试 |
| `foundation_flow_smoke_main.cpp` | 基础流烟雾测试 |
| `cxparser_rag_script_smoke.cpp` | RAG 脚本烟雾测试 |

---

### 3.5 cxparser_ext - 脚本扩展层

#### 3.5.1 模块定位

cxparser_ext 是构建在 cxparser 核心之上的扩展层，提供完整的脚本执行流水线、类型绑定构建、流程路由、验证引擎、结果交付等企业级功能。

#### 3.5.2 子模块划分

| 子目录 | 职责 |
|--------|------|
| `pipeline/` | 核心流水线组件 |
| `runtime/` | CxScript 运行时支持 |
| `catalog/` | 脚本目录与案例管理 |
| `validation/` | 验证引擎与反馈计划 |
| `meta/` | 元数据类型定义 |
| `drivers/` | 驱动层（调度驱动等） |
| `scenarios/` | 场景封装（图像探测等） |
| `adapters/` | 外部适配器（Clang、Radare2） |

#### 3.5.3 核心流水线类：ParserPipeline

[ParserPipeline](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxparser_ext/pipeline/parser_pipeline.h#L14-L50) 是脚本执行的主入口，封装了完整的执行生命周期。

**执行流程：**
```
PrepareTask → MergeBindingSpec → MergeEvidence → Run → Validate
```

**关键方法：**
```cpp
void Reset();                                           // 重置状态
void SetGuardProfile(ExecutionGuardProfile profile);    // 设置保护配置
bool PrepareTask(const ExecutionTarget& target);        // 准备执行目标
bool MergeBindingSpec(const ParserBindingSpec& spec);   // 合并绑定规范
bool MergeEvidence(const ParserEvidenceBundle& bundle); // 合并证据
bool Run(ExecutionResult& result);                      // 执行
bool Validate(ParserValidationReport& report);          // 验证结果
void* GetClassObject(const std::string& class_name, 
                     const std::string& object_name);   // 获取绑定对象
```

#### 3.5.4 Pipeline 内部组件

| 组件类 | 职责 |
|--------|------|
| `ParserRuntimeFacade` | 运行时门面，统一调度各执行单元 |
| `ParserBindingBuilder` | 动态构建 C++ 类与脚本的绑定 |
| `ParserFlowRouter` | 脚本流程路由器 |
| `ParserTaskCoordinator` | 任务协调器 |
| `ParserTestRouter` | 测试路由器 |
| `ParserTestReporter` | 测试报告生成 |
| `ParserValidationEngine` | 验证引擎 |
| `ParserDeliveryApi` | 结果交付 API |

#### 3.5.5 CxScript 运行时

[cxscript_runtime.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxparser_ext/runtime/cxscript_runtime.h) 定义了 CxScript 脚本的完整处理管线：

**处理阶段：**
1. **身份构建** - `BuildCxscriptIdentity`：构建脚本身份标识
2. **上下文构建** - `BuildCxscriptContext`：构建执行上下文
3. **脚本加载** - `LoadCxscriptText`：加载脚本源码
4. **文本规范化** - `NormalizeCxscriptText`：规范化脚本文本
5. **元数据提取** - `ExtractCxscriptHeaderMetadata`：提取头部元数据
6. **流程分析** - `AnalyzeCxscriptFlow`：分析脚本控制流
7. **语义分析** - `AnalyzeCxscriptSemantics`：分析语义与绑定
8. **层级剖面构建** - `BuildCxscriptLayerProfile`：构建层级剖面
9. **安全性评估** - `EvaluateCxscriptCompileBridgeSafety`：编译桥接安全性评估

---

### 3.6 3D - 三维场景与工具集成模块

#### 3.6.1 模块定位

3D 模块（命名空间 `codex_lan_agent_3d`）提供三维资产生成、场景管理、外部工具集成以及 MCP 协议适配能力。它通过适配器模式将 Blender、Nova3D 等外部 3D 工具统一纳入 CxVision 的生态。

#### 3.6.2 核心类结构

```
ThreeDOrchestrator (总编排器)
    │
    ├── IStructuredAssetGenerator (资产生成接口)
    │   └── Nova3DAssetAdapter (Nova3D 实现)
    │
    ├── ISceneAdapter (场景适配接口)
    │   ├── BlenderSceneAdapter (Blender 场景)
    │   ├── CxGeomSceneAdapter (cxgeom 场景)
    │   └── CxCloudSceneAdapter (cxcloud 点云场景)
    │
    ├── ThreeDSceneBridge (场景桥接器)
    │
    ├── ThreeDControlSurface (控制面)
    │
    ├── ThreeDMcpAdapter (MCP 协议适配)
    │   └── ThreeDMcpProtocolBridge (协议桥)
    │   └── ThreeDMcpResourceAdapter (资源适配)
    │
    ├── ThreeDParserDispatchBridge (解析器调度桥)
    │
    ├── ThreeDHostedWorkflowCoordinator (托管工作流协调)
    │
    ├── ThreeDSessionStateStore (会话状态存储)
    │
    └── ThreeDHostValidation (主机验证)

外部工具调用:
    ExternalToolCommandInvoker (命令调用基类)
        ├── BlenderExternalToolInvoker (Blender 调用)
        └── Nova3DExternalToolInvoker (Nova3D 调用)

MCP 后端:
    BlenderMcpSceneBackend (Blender MCP 后端)
    Nova3DMcpHostedBackend (Nova3D MCP 后端)
```

#### 3.6.3 核心编排器：ThreeDOrchestrator

[ThreeDOrchestrator](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/3D/src/ThreeDOrchestrator.h#L97-L156) 是 3D 模块的总入口，协调资产生成与场景操作。

**核心能力：**
```cpp
// 资产生成
CommandResult GenerateStructuredAsset(const std::string& prompt, const std::string& preferred_model);
CommandResult RegenerateAssetPart(const std::string& asset_id, const std::string& part_id, 
    const std::string& description, const std::string& preferred_model);
CommandResult AddAssetPart(const std::string& asset_id, const std::string& description, 
    const std::string& preferred_model);
CommandResult ArticulateAsset(const std::string& asset_id, const std::string& articulation_request,
    const std::string& preferred_model);

// 场景操作
CommandResult ImportAssetToScene(const std::string& asset_id);
CommandResult TransformSceneObject(const std::string& object_id, const Vec3& translation,
    const Vec3& rotation, const Vec3& scale);

// 查询
CommandResult GetAssetSummary(const std::string& asset_id) const;
CommandResult GetSceneSummary() const;
```

**数据模型：**
- `StructuredAssetRecord`：结构化资产记录（包含 GLB URL、预览图、部件列表、关节数等）
- `SceneObjectRecord`：场景对象记录（变换矩阵、资产引用）
- `SceneSnapshot`：场景快照
- `AssetPart`：资产部件（支持关节）
- `ToolSpec`：工具规格描述

#### 3.6.4 MCP 协议适配层

[ThreeDMcpAdapter](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/3D/src/ThreeDMcpAdapter.h#L23-L53) 将 3D 模块的能力包装为 MCP 工具协议。

**MCP 工具模型：**
```cpp
struct McpToolDescriptor {
    std::string name;
    std::string description;
    std::vector<McpToolInputProperty> properties;
    ToolRouteContract route_contract;
};
```

**MCP 相关组件：**
| 组件 | 职责 |
|------|------|
| `ThreeDMcpAdapter` | MCP 工具列表/描述/调用 |
| `ThreeDMcpProtocolBridge` | MCP 协议消息桥接 |
| `ThreeDMcpResourceAdapter` | MCP 资源适配 |
| `BlenderMcpSceneBackend` | Blender MCP 后端实现 |
| `Nova3DMcpHostedBackend` | Nova3D MCP 后端实现 |

#### 3.6.5 外部工具集成

**工具调用链：**
```
ThreeDOrchestrator
    → ISceneAdapter
        → BlenderSceneAdapter
            → BlenderExternalToolInvoker
                → ExternalToolCommandInvoker
                    → 执行 Blender Python 脚本 (BlenderLiveBridge.py)
```

**Python 桥接工具（`3D/tools/`）：**
- `BlenderLiveBridge.py`：Blender 实时桥接服务
- `BlenderSceneWorker.py`：Blender 场景工作线程
- `Nova3DLiveBridge.py`：Nova3D 实时桥接服务

---

## 4. 关键类与函数

### 4.1 cximage 模块

| 类/函数 | 位置 | 功能说明 |
|---------|------|----------|
| `Image` | [Image.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/Image.h#L47) | 图像封装与处理核心类 |
| `Image::findConnectedComponents` | [Image.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/Image.h#L241) | 连通组件分析 |
| `Image::SubpixelProcess` | [Image.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/Image.h#L314) | 亚像素边缘细化 |
| `Image::CircleFit_` | [Image.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/Image.h#L310) | 圆拟合 |
| `ViewController` | [ViewController.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/ViewController.h#L34) | GUI 控制器 |
| `ViewController::RunCxScript` | [ViewController.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/ViewController.h#L76) | 执行 CxScript 脚本 |
| `FastMatch` | [FastMatch.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/FastMatch.h) | 快速模板匹配 |
| `Shape` | [Shape.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/Shape.h) | 形状基类 |
| `Findline` | [Findline.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/Findline.h) | 直线检测 |
| `Findcircle` | [Findcircle.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/Findcircle.h) | 圆检测 |
| `FormfitGauge` | [FormfitGauge.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/FormfitGauge.h) | 形位公差规 |
| `SemanticFlowGraph` | [SemanticFlowGraph.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/SemanticFlowGraph.h) | 语义流图 |
| `ParserClass` | [ParserClass.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/ParserClass.h) | 解析器桥接类 |
| `ImageAnnotationLayer` | [ImageAnnotationLayer.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/ImageAnnotationLayer.h) | 图像注释层 |
| `Grid` | [Grid.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/Grid.h) | 网格数据结构 |
| `gp_path` | [gp_path.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/gp_path.h) | 几何路径 |

### 4.2 cxgeom 模块

| 类/函数 | 位置 | 功能说明 |
|---------|------|----------|
| `CxGeometryItem` | [CxGeometryItem.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxgeom/include/CxGeometryItem.h#L10) | 几何项封装 |
| `CxGeometryOperations` | [CxGeometryOperations.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxgeom/include/CxGeometryOperations.h#L23) | 几何操作门面 |
| `CxShapeHandle` | [CxShapeHandle.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxgeom/include/CxShapeHandle.h) | OCCT 形状句柄 |
| `CxCurveBuilder` | [CxCurveBuilder.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxgeom/include/CxCurveBuilder.h) | 曲线构建器 |
| `CxFaceBuilder` | [CxFaceBuilder.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxgeom/include/CxFaceBuilder.h) | 曲面构建器 |
| `CxWireBuilder` | [CxWireBuilder.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxgeom/include/CxWireBuilder.h) | 线框构建器 |
| `CxSetCircleBuild` | [CxSetCircleBuild.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxgeom/include/CxSetCircleBuild.h) | 圆构建器 |
| `CxSetLineBuild` | [CxSetLineBuild.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxgeom/include/CxSetLineBuild.h) | 直线构建器 |
| `CxGeomMeasure` | [CxGeomMeasure.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxgeom/include/CxGeomMeasure.h) | 几何测量 |
| `CxRefreshTracker` | [CxRefreshTracker.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxgeom/include/CxRefreshTracker.h) | 刷新跟踪器 |
| `CxSceneMapping` | [CxSceneMapping.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxgeom/include/CxSceneMapping.h) | 场景映射 |

### 4.3 cxcloud 模块

| 类/函数 | 位置 | 功能说明 |
|---------|------|----------|
| `CxCloudItem` | [CxCloudItem.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxcloud/include/CxCloudItem.h#L10) | 点云项 |
| `CxCloudOperations` | [CxCloudOperations.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxcloud/include/CxCloudOperations.h#L44) | 点云操作门面 |
| `CxPointCloudData` | [CxPointCloudData.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxcloud/include/CxPointCloudData.h) | 点云数据 |
| `CxOctreeAdapter` | [CxOctreeAdapter.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxcloud/include/CxOctreeAdapter.h) | 八叉树索引 |
| `CxNormalEstimator` | [CxNormalEstimator.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxcloud/include/CxNormalEstimator.h) | 法向量估计 |
| `CxDistanceAnalyzer` | [CxDistanceAnalyzer.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxcloud/include/CxDistanceAnalyzer.h) | 距离分析 |
| `CxCloudSubset` | [CxCloudSubset.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxcloud/include/CxCloudSubset.h) | 点子集 |

### 4.4 cxparser 模块

| 类/函数 | 位置 | 功能说明 |
|---------|------|----------|
| `mu::Parser` | [muParser.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxparser/muParser.h#L19) | 浮点解析器门面 |
| `mu::ParserBase` | [muParserBase.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxparser/muParserBase.h) | 解析器基类 |
| `mu::ParserByteCode` | [muParserBytecode.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxparser/muParserBytecode.h) | 字节码 |
| `mu::ParserClass` | [muParserClass.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxparser/muParserClass.h) | 类绑定解析器 |
| `mu::ParserTreeNode` | [muParserTreeNode.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxparser/muParserTreeNode.h) | 语法树节点 |

### 4.5 cxparser_ext 模块

| 类/函数 | 位置 | 功能说明 |
|---------|------|----------|
| `ParserPipeline` | [parser_pipeline.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxparser_ext/pipeline/parser_pipeline.h#L14) | 脚本执行流水线 |
| `ParserRuntimeFacade` | [parser_runtime_facade.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxparser_ext/pipeline/parser_runtime_facade.h) | 运行时门面 |
| `ParserBindingBuilder` | [parser_binding_builder.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxparser_ext/pipeline/parser_binding_builder.h) | 绑定构建器 |
| `ParserFlowRouter` | [parser_flow_router.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxparser_ext/pipeline/parser_flow_router.h) | 流程路由器 |
| `ParserValidationEngine` | [parser_validation_engine.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxparser_ext/validation/parser_validation_engine.h) | 验证引擎 |
| `ParserTaskCoordinator` | [parser_task_coordinator.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxparser_ext/pipeline/parser_task_coordinator.h) | 任务协调器 |
| `ParserDeliveryApi` | [parser_delivery_api.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxparser_ext/pipeline/parser_delivery_api.h) | 交付 API |
| `BuildCxscriptIdentity` | [cxscript_runtime.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxparser_ext/runtime/cxscript_runtime.h#L9) | 构建脚本身份 |
| `AnalyzeCxscriptFlow` | [cxscript_runtime.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxparser_ext/runtime/cxscript_runtime.h#L21) | 分析脚本流程 |

### 4.6 3D 模块

| 类/函数 | 位置 | 功能说明 |
|---------|------|----------|
| `ThreeDOrchestrator` | [ThreeDOrchestrator.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/3D/src/ThreeDOrchestrator.h#L97) | 3D 总编排器 |
| `ISceneAdapter` | [ThreeDOrchestrator.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/3D/src/ThreeDOrchestrator.h#L84) | 场景适配器接口 |
| `IStructuredAssetGenerator` | [ThreeDOrchestrator.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/3D/src/ThreeDOrchestrator.h#L59) | 资产生成器接口 |
| `ThreeDMcpAdapter` | [ThreeDMcpAdapter.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/3D/src/ThreeDMcpAdapter.h#L23) | MCP 适配器 |
| `ThreeDControlSurface` | [ThreeDControlSurface.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/3D/src/ThreeDControlSurface.h) | 控制面 |
| `ThreeDSceneBridge` | [ThreeDSceneBridge.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/3D/src/ThreeDSceneBridge.h) | 场景桥接器 |
| `ThreeDHostedWorkflowCoordinator` | [ThreeDHostedWorkflowCoordinator.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/3D/src/ThreeDHostedWorkflowCoordinator.h) | 托管工作流协调器 |
| `BlenderSceneAdapter` | [BlenderSceneAdapter.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/3D/src/BlenderSceneAdapter.h) | Blender 场景适配器 |
| `CxGeomSceneAdapter` | [CxGeomSceneAdapter.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/3D/src/CxGeomSceneAdapter.h) | cxgeom 场景适配器 |
| `CxCloudSceneAdapter` | [CxCloudSceneAdapter.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/3D/src/CxCloudSceneAdapter.h) | cxcloud 场景适配器 |
| `Nova3DAssetAdapter` | [Nova3DAssetAdapter.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/3D/src/Nova3DAssetAdapter.h) | Nova3D 资产适配器 |
| `ExternalToolCommandInvoker` | [ExternalToolCommandInvoker.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/3D/src/ExternalToolCommandInvoker.h) | 外部工具调用基类 |
| `ThreeDSessionStateStore` | [ThreeDSessionStateStore.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/3D/src/ThreeDSessionStateStore.h) | 会话状态存储 |
| `ThreeDHostValidation` | [ThreeDHostValidation.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/3D/src/ThreeDHostValidation.h) | 主机验证 |
| `CommandResult` | [ThreeDTypes.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/3D/src/ThreeDTypes.h#L14) | 命令执行结果 |
| `Vec3` | [ThreeDTypes.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/3D/src/ThreeDTypes.h#L20) | 三维向量 |

---

## 5. 依赖关系

### 5.1 外部依赖

| 依赖库 | 版本 | 用途 | 配置路径 |
|--------|------|------|----------|
| GLFW | 3.3.10 | 窗口与输入管理 | `D:/glfw-3.3.10` |
| GLAD | - | OpenGL 函数加载 | `../../analysis_workspace/imGuIZMO.quat/libs/glad` |
| OpenCASCADE | 7.7.0 | 几何建模内核 | `D:/OpenCASCADE-7.7.0-vc14-64/opencascade-7.7.0` |
| OpenCV | - | 图像处理 | `D:/opencv/build` |
| ImGui | - | GUI 框架 | 项目内 `imgui/` 目录 |
| nanoflann | - | 最近邻搜索 | 项目内头文件 |

### 5.2 OpenCASCADE 链接库

根 CMakeLists.txt 中链接的 OCCT 库：

| 库名 | 功能 |
|------|------|
| TKernel | 核心基础 |
| TKMath | 数学库 |
| TKG2d | 2D 几何 |
| TKG3d | 3D 几何 |
| TKService | 服务层 |
| TKV3d | 3D 视图 |
| TKOpenGl | OpenGL 渲染 |
| TKGeomBase | 几何基础 |
| TKBRep | B-Rep 表示 |
| TKGeomAlgo | 几何算法 |
| TKTopAlgo | 拓扑算法 |
| TKPrim | 基本体 |
| TKBO | 布尔运算 |
| TKOffset | 偏移/加厚 |
| TKXSBase | 数据交换基础 |
| TKSTEPBase | STEP 格式基础 |
| TKIGES | IGES 格式 |
| TKLCAF | 轻量 CAF 框架 |

### 5.3 模块间依赖

```
cxvision_imgui_acceptance (主程序)
    ├── cximage/
    │   ├── cxgeom/include (接口依赖)
    │   ├── cxparser/ (muParser 头文件)
    │   ├── OpenCV
    │   ├── OpenCASCADE
    │   └── ImGui/GLFW/GLAD
    │
cxparser_ext_cxscript_cli (CLI 程序)
    ├── cxparser_core (静态库)
    └── cxcore_full_core (可选，通过 CXPARSER_ENABLE_CXCORE_REAL_BRIDGE 开关)
        ├── cxgeom/
        └── cximage/ (可选)

codex_lan_agent_3d (3D 库)
    └── (独立模块，通过适配器与 cxgeom/cxcloud 集成)
```

### 5.4 编译选项

**全局定义：**
- `_CRT_SECURE_NO_WARNINGS`：禁用 MSVC 安全警告
- `CXCORE_ENABLE_VIEWCONTROLLER_CUDA=0`：禁用 CUDA 加速
- `ImTextureID=ImU64`：ImGui 纹理 ID 类型
- `CXPARSER_WORKSPACE_ROOT`：工作区根目录

**MSVC 选项：**
- `/EHsc`：C++ 异常处理
- `/W3`：警告级别 3
- `/utf-8`：UTF-8 源文件
- `/FS`：强制同步 PDB 写入
- `/Z7`：调试信息格式

---

## 6. 构建与运行

### 6.1 前置要求

- **操作系统**：Windows
- **编译器**：MSVC (Visual Studio)
- **CMake**：3.21 或更高
- **C++ 标准**：C++17 (主程序) / C++14 (cxparser)
- **第三方库**：GLFW 3.3.10、OpenCASCADE 7.7.0、OpenCV

### 6.2 构建目标

#### 6.2.1 主目标：cxvision_imgui_acceptance

基于根目录 [CMakeLists.txt](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/CMakeLists.txt) 的 GUI 应用程序。

**构建步骤：**
```bash
# 1. 创建构建目录
mkdir build && cd build

# 2. 配置 CMake (确保第三方库路径正确)
cmake .. -DGLFW_ROOT="D:/glfw-3.3.10" ^
         -DGLAD_ROOT="path/to/glad" ^
         -DOCCT_ROOT="D:/OpenCASCADE-7.7.0-vc14-64/opencascade-7.7.0" ^
         -DOpenCV_DIR="D:/opencv/build"

# 3. 构建
cmake --build . --config Release
```

**输出位置：**
- 可执行文件：`D:/Codex-WorkDir/Sean_WorkDir/cxparser/build/Release/cxvision_imgui_acceptance.exe`
- OCCT DLL 会通过 POST_BUILD 命令自动复制到输出目录

#### 6.2.2 cxparser CLI 目标

基于 [cxparser/CMakeLists.txt](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxparser/CMakeLists.txt) 的命令行工具。

**构建目标：**
- `cxparser_core`：静态库（muParser 核心）
- `cxparser_ext_cxscript_cli`：CLI 可执行文件

**可选功能：**
```cmake
# 启用 cxcore 真实桥接
cmake .. -DCXPARSER_ENABLE_CXCORE_REAL_BRIDGE=ON
```

#### 6.2.3 3D 模块目标

基于 [3D/CMakeLists.txt](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/3D/CMakeLists.txt) 的 3D 集成库。

**构建目标：**
- `codex_lan_agent_3d`：静态库
- 多个 smoke 测试可执行文件（20+ 个）

**主要测试目标：**
| 测试程序 | 测试内容 |
|----------|----------|
| `codex_lan_agent_3d_smoke` | 系统级烟雾测试 |
| `codex_lan_agent_3d_bridge_smoke` | 场景桥接测试 |
| `codex_lan_agent_3d_adapter_smoke` | 适配器测试 |
| `codex_lan_agent_3d_control_smoke` | 控制面测试 |
| `codex_lan_agent_3d_mcp_smoke` | MCP 适配测试 |
| `codex_lan_agent_3d_live_end_to_end_smoke` | 端到端实时测试 |
| `codex_lan_agent_3d_live_validation_runner` | 实时验证运行器 |

### 6.3 运行方式

#### 6.3.1 GUI 应用

直接运行 `cxvision_imgui_acceptance.exe`，将启动基于 ImGui 的交互界面，包含：
- 3D 视图（OpenCASCADE 渲染）
- 脚本目录面板
- 图像证据面板
- 手动状态测试控制台
- 脚本执行结果显示

#### 6.3.2 CLI 脚本执行

```bash
cxparser_ext_cxscript_cli [脚本路径] [选项]
```

#### 6.3.3 3D 模块测试

```bash
# 运行系统烟雾测试
codex_lan_agent_3d_smoke.exe

# 运行端到端实时测试
codex_lan_agent_3d_live_end_to_end_smoke.exe
```

---

## 7. 脚本系统 (CxScript)

### 7.1 脚本语言概述

CxScript 是基于 muParser 扩展的领域特定语言（DSL），用于描述视觉检测与几何测量工作流。支持表达式求值、变量绑定、类方法调用以及控制流语句。

### 7.2 脚本文件类型

| 扩展名 | 说明 |
|--------|------|
| `.cxsc` | CxScript 脚本文件 |
| `.cxflow` | 状态机/流程定义文件 |
| `.cxs` | 脚本片段/参数文件 |

### 7.3 脚本目录结构

```
cxparser/
├── cxscript/
│   ├── module/               # 模块级测试脚本
│   │   ├── cximage/          # 图像模块测试
│   │   ├── torch/            # PyTorch 集成
│   │   ├── mlpack/           # mlpack 集成
│   │   └── ensmallen/        # ensmallen 优化
│   ├── integration/          # 集成测试脚本
│   │   └── cxcore_to_mlpack/
│   └── state_machine/        # 状态机示例
│       └── examples/
└── rag_script_cases/         # RAG 脚本案例库
    ├── cxcore/
    │   ├── feature/          # 特征检测脚本
    │   ├── infer/            # 推理脚本
    │   ├── scenario/         # 场景脚本
    │   └── smoke/            # 烟雾测试
    └── custom_type_template/ # 自定义类型模板
```

### 7.4 主要脚本类别

#### 7.4.1 特征检测脚本 (`cxcore/feature/`)

| 脚本族 | 功能 | 数量 |
|--------|------|------|
| 直线测量 | 直线检测与测量 | 15+ |
| 圆测量 | 圆检测与半径测量 | 15+ |
| 椭圆测量 | 椭圆检测与测量 | 1+ |
| 区域边界分析 | 区域边界检测 | 8+ |
| 模板特征匹配 | 模板匹配与定位 | 12+ |
| 矩形形状拟合 | 矩形检测与候选选择 | 4+ |
| 组合特征 | 多特征组合测试 | 10+ |
| 参数优化 | ensmallen 几何参数优化 | 6+ |

**典型脚本命名模式：**
- `cxcore_line_measurement_*.cxsc`：直线测量
- `cxcore_circle_measurement_*.cxsc`：圆测量
- `cxcore_region_boundary_analysis_*.cxsc`：区域边界分析
- `cxcore_template_feature_match_*.cxsc`：模板特征匹配

**变体后缀：**
- `_golden`：黄金样本
- `_noise`：噪声样本
- `_boundary`：边界情况
- `_degenerate`：退化情况
- `_balanced`：平衡样本
- `_probe`：探测样本
- `_cstyle`：C 风格绑定

#### 7.4.2 推理脚本 (`cxcore/infer/`)

- 边界结果推理路由
- 区域检测推理
- Halcon 交互评估
- 参数评估推理

#### 7.4.3 场景脚本 (`cxcore/scenario/`)

- 经典分析场景
- 几何回放场景
- 参数回放场景

### 7.5 脚本执行流程

```
用户触发 (GUI/CLI)
    │
    ▼
ParserPipeline::PrepareTask
    │
    ├─ 构建脚本身份 (BuildCxscriptIdentity)
    ├─ 加载脚本文本 (LoadCxscriptText)
    ├─ 文本规范化 (NormalizeCxscriptText)
    └─ 提取头部元数据
    │
    ▼
ParserPipeline::MergeBindingSpec
    │
    └─ ParserBindingBuilder 构建类绑定
    │
    ▼
ParserPipeline::MergeEvidence
    │
    └─ 合并证据包 (ParserEvidenceBundle)
    │
    ▼
ParserPipeline::Run
    │
    ├─ ParserRuntimeFacade 调度
    ├─ 流程分析 (AnalyzeCxscriptFlow)
    ├─ 语义分析 (AnalyzeCxscriptSemantics)
    ├─ muParser 字节码执行
    └─ 结果收集
    │
    ▼
ParserPipeline::Validate
    │
    └─ ParserValidationEngine 验证
    │
    ▼
结果交付 (ParserDeliveryApi)
```

### 7.6 绑定系统

**CxScript 通过绑定系统与 C++ 对象交互：**

1. **类注册**：通过 `ParserBindingBuilder` 注册 C++ 类的元信息
2. **对象绑定**：将 C++ 对象指针与脚本变量名关联
3. **方法调用**：脚本通过 `.` 操作符调用绑定对象的方法
4. **属性访问**：支持属性的读写访问

**支持的绑定类型：**
- 内置数值类型
- 自定义值类型
- 自定义记录类型
- 自定义类（CreateClass 模式）
- cxcore/cximage 系统对象

---

## 8. 测试体系

### 8.1 测试策略

项目采用 **烟雾测试 (Smoke Test)** 为主的测试策略，每个模块都有对应的烟雾测试入口，用于快速验证核心功能的可用性。

### 8.2 cximage 测试

cximage 模块的测试主要集成在 GUI 应用中，通过以下方式：
- 手动状态测试控制台（[ManualStateTestConsole.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/ManualStateTestConsole.h)）
- 脚本驱动的功能验证
- ParserDebugBridge 调试桥接

### 8.3 cxgeom 测试

cxgeom 模块采用以 `*Smoke.cpp` 命名的烟雾测试文件：

| 测试文件 | 测试内容 |
|----------|----------|
| `CxGeometryItemSmoke.cpp` | 几何项基础功能 |
| `CxGeometryOperationsSmoke.cpp` | 几何操作 |
| `CxGeometryBulkCreateSmoke.cpp` | 批量创建 |
| `CxGeometryBulkPresentationSmoke.cpp` | 批量表示 |
| `CxGeometryBulkReleaseSmoke.cpp` | 批量释放 |
| `CxGeomPresentationSmoke.cpp` | 几何表示 |
| `CxSceneMappingSmoke.cpp` | 场景映射 |
| `CxRefreshSmoke.cpp` | 刷新跟踪 |

### 8.4 cxcloud 测试

| 测试文件 | 测试内容 |
|----------|----------|
| `CxCloudSmoke.cpp` | 点云基础 |
| `CxCloudItemSmoke.cpp` | 点云项 |
| `CxCloudOperationsSmoke.cpp` | 点云操作 |
| `CxCloudBulkCreateSmoke.cpp` | 批量创建 |
| `CxCloudBulkRenderSmoke.cpp` | 批量渲染 |
| `CxCloudBulkReleaseSmoke.cpp` | 批量释放 |
| `CxCloudSceneMappingSmoke.cpp` | 场景映射 |

### 8.5 cxparser 测试

cxparser 模块拥有完整的回归测试体系：

| 测试入口 | 测试维度 |
|----------|----------|
| `basic_regression_main.cpp` | 基础表达式回归 |
| `class_binding_smoke_main.cpp` | 类绑定烟雾 |
| `control_flow_regression_main.cpp` | 控制流回归 |
| `custom_type_contract_smoke_main.cpp` | 自定义类型契约 |
| `custom_type_createclass_contract_main.cpp` | CreateClass 契约 |
| `custom_type_minimal_binding_main.cpp` | 最小绑定 |
| `custom_type_mixed_args_smoke_main.cpp` | 混合参数 |
| `custom_type_multi_instance_smoke_main.cpp` | 多实例 |
| `cxcore_contract_script_smoke_main.cpp` | cxcore 契约脚本 |
| `cxcore_minimal_binding_main.cpp` | cxcore 最小绑定 |
| `cxcore_type_registration_smoke_main.cpp` | cxcore 类型注册 |
| `cxgeom_cxcloud_contract_smoke.cpp` | 几何/点云契约 |
| `cximage_v1_minimal_binding_main.cpp` | cximage 最小绑定 |
| `foundation_flow_smoke_main.cpp` | 基础流 |
| `cxparser_rag_script_smoke.cpp` | RAG 脚本 |

### 8.6 3D 模块测试

3D 模块包含 20+ 个烟雾测试程序，覆盖各个子系统：

| 测试组 | 测试程序 |
|--------|----------|
| **核心系统** | `ThreeDSystemSmoke` |
| **场景桥接** | `ThreeDSceneBridgeSmoke` |
| **适配器** | `ThreeDAdapterSmoke` |
| **控制面** | `ThreeDControlSurfaceSmoke` |
| **MCP 协议** | `ThreeDMcpAdapterSmoke`, `ThreeDMcpProtocolSmoke` |
| **主机会话** | `ThreeDHostSessionSmoke` |
| **托管桥接** | `ThreeDHostedBridgeSmoke` |
| **工作流协调** | `ThreeDHostedWorkflowCoordinatorSmoke` |
| **主机验证** | `ThreeDHostValidationSmoke`, `ThreeDHostValidationFailureSmoke` |
| **外部工具** | `ThreeDExternalHostSmoke`, `ThreeDExternalHostFailureSmoke` |
| **实时桥接** | `ThreeDLiveBridgeSmoke`, `ThreeDLiveBridgeFailureSmoke` |
| **Blender 集成** | `ThreeDBlenderLiveOpsSmoke`, `ThreeDBlenderRemoteGlbSmoke` |
| **端到端** | `ThreeDLiveEndToEndSmoke` |
| **验证运行器** | `ThreeDLiveValidationRunner` |
| **工具桩** | `ThreeDToolStub` |
| **Nova3D** | `ThreeDNova3DEnvFileSmoke` |

### 8.7 脚本测试

脚本测试通过 `.cxsc` 文件驱动，按层级组织：
- `smoke/`：烟雾级脚本测试
- `feature/`：特征级测试
- `infer/`：推理级测试
- `scenario/`：场景级测试

---

## 附录

### A. 命名空间规范

| 命名空间 | 模块 |
|----------|------|
| `cxgeom` | 几何建模模块 |
| `cxcloud` | 点云处理模块 |
| `mu` | muParser 解析器核心 |
| `cxparser_ext` | 脚本扩展层 |
| `codex_lan_agent_3d` | 3D 集成模块 |

### B. 文件命名约定

| 模式 | 说明 |
|------|------|
| `Cx*.h` / `Cx*.cpp` | cxgeom/cxcloud 模块公共接口 |
| `muParser*.h` / `muParser*.cpp` | muParser 核心文件 |
| `*Smoke.cpp` | 烟雾测试文件 |
| `*.cxsc` | CxScript 脚本 |
| `*.cxflow` | 流程定义 |
| `ThreeD*.h` / `ThreeD*.cpp` | 3D 模块核心类 |

### C. 关键设计模式

| 模式 | 应用位置 |
|------|----------|
| **门面模式 (Facade)** | `CxGeometryOperations`, `CxCloudOperations`, `ParserRuntimeFacade` |
| **适配器模式 (Adapter)** | `BlenderSceneAdapter`, `CxGeomSceneAdapter`, `ThreeDMcpAdapter` |
| **策略模式 (Strategy)** | `ISceneAdapter`, `IStructuredAssetGenerator` 接口族 |
| **编排器模式 (Orchestrator)** | `ThreeDOrchestrator`, `ViewController` |
| **流水线模式 (Pipeline)** | `ParserPipeline` |
| **桥接模式 (Bridge)** | `ThreeDSceneBridge`, `ThreeDParserDispatchBridge` |
| **句柄/体素模式 (Handle-Body)** | `CxShapeHandle`, `CxCloudHandle` |

---

*文档生成时间：2026-07-03*
*基于仓库：cxvision_repo*
