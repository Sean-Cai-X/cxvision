# CxVision Code  v2.5

<p align="center">
<img src="https://raw.githubusercontent.com/Sean-Cai-X/cxvision/codex/cxcore-integration/diagram.png" width="100%">
</p>

> **文档版本**: v2.5
> **对应分支**: `codex/cxcore-integration`
> **核验日期**: 2026-08-18
> **代码基线**: 最新公开提交 `3b260e2` — `add sam module test`
> **文档标题**: CxScript / Evidence、Multi-Model Torch、Incremental Learning、Measurement Semantics 与 Metrology Integration Baseline
> **替代版本**: v2.4 / 2026-08-07 / `8c77433`

## 目录

0. [文档状态规则](#0-文档状态规则)
1. [当前项目阶段](#1-当前项目阶段)
2. [v2.4 → v2.5 的关键变化](#2-v24--v25-的关键变化)
3. [完整系统架构（8 个逻辑域）](#3-完整系统架构8-个逻辑域)
4. [六条标准执行链](#4-六条标准执行链)
5. [Application / Workbench](#5-application--workbench)
6. [CxScript Runtime & Asset System](#6-cxscript-runtime--asset-system)
7. [Vision / CxCore Runtime](#7-vision--cxcore-runtime)
8. [Model & Optimization Runtime](#8-model--optimization-runtime)
9. [Measurement / Metrology](#9-measurement--metrology)
10. [mlpack / ensmallen](#10-mlpack--ensmallen)
11. [Canonical Result](#11-canonical-result)
12. [Result Adapter 当前剩余缺口](#12-result-adapter-当前剩余缺口)
13. [Geometry Projection](#13-geometry-projection)
14. [Runtime Result Capture](#14-runtime-result-capture)
15. [Evidence / Review](#15-evidence--review)
16. [Build / Runtime](#16-build--runtime)
17. [Test Taxonomy / Acceptance](#17-test-taxonomy--acceptance)
18. [Current Status Matrix — v2.5](#18-current-status-matrix--v25)
19. [当前最重要的六个断点](#19-当前最重要的六个断点)
20. [Architecture Rules v2.5](#20-architecture-rules-v25)
21. [Interaction / Annotation](#21-interaction--annotation)
22. [Manual Console Controllers](#22-manual-console-controllers)
23. [Unified Execution / Orchestration](#23-unified-execution--orchestration)
24. [OpenCV / OCCT / cxgeom / cxcloud](#24-opencv--occt--cxgeom--cxcloud)
25. [Observability / Reliability](#25-observability--reliability)

A. [Core File Index](#appendix-a-core-file-index)
B. [CxScript Asset Index](#appendix-b-cxscript-asset-index)
C. [Artifact Schemas](#appendix-c-artifact-schemas)
D. [CLI Options](#appendix-d-cli-options)
E. [Placeholder Register](#appendix-e-placeholder-register)
F. [Legacy Stage25 C++](#appendix-f-legacy-stage25-c)

---

## 0. 文档状态规则

v2.5 状态定义：

| 状态 | 定义 |
|------|------|
| **[Verified]** | 正式构建、真实数据、固定 Case、Evidence 和结果验收全部通过 |
| **[Module Verified]** | 模块内部测试通过，但完整工具链尚未验收 |
| **[Implemented]** | 代码已实现并进入正式构建 |
| **[Partial]** | 主链已经存在，但还有明确的数据、资产或验证缺口 |
| **[Contract]** | 接口、Schema 或数据结构已经建立 |
| **[Draft]** | 有实际实现，但代码自身明确仍属于草案 |
| **[Verification Pending]** | 实现完成，尚缺固定 Case 或人工/自动验收 |
| **[Pending Binding]** | 接口已完成，但真实模型、权重、数据集或外部资产尚未绑定 |
| **[Placeholder]** | 当前仍为模拟、静态或规则结果 |
| **[Legacy]** | 兼容旧入口，不允许作为新功能主线 |
| **[Disabled]** | 实现存在但默认关闭 |

状态必须遵守：

```text
Source Exists
→ Build Registered
→ Runtime Reachable
→ Real Data Executed
→ Canonical Result
→ Geometry Projected
→ Evidence Generated
→ Fixed Regression Verified
```

只有最后一步才能标记 `[Verified]`。

---

## 1. 当前项目阶段

### 1.1 项目简介

CxVision 是一个基于 C++ 的计算机视觉与几何分析平台，集成了图像处理、几何建模、脚本自动化以及交互式调试工作台。项目采用模块化设计，支持通过自定义脚本语言（CxScript）驱动视觉检测与几何测量工作流，并提供完整的证据链、审核门和参数回归体系。

### 1.2 核心能力

- **图像分析**：边缘检测、特征提取、模板匹配、形态学操作
- **几何建模**：基于 OpenCASCADE 的参数化几何构建与测量
- **脚本自动化**：基于 muParser 扩展的 CxScript 领域特定语言
- **GUI 交互**：基于 ImGui + GLFW + OpenCASCADE 的可视化界面
- **证据链管理**：Manifest/Catalog/Suite/Contract 完整证据体系
- **参数回归**：参数范围探索、候选生成、评估记录、准确性统计
- **机器学习集成**：Torch Production Runtime、mlpack 基础模型、ensmallen 优化层
- **人工审核**：Review Gate、Replay Package、Human Review

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
| 机器学习 | PyTorch (libtorch) |
| 参数优化 | mlpack / ensmallen (脚本语义层) |

### 1.4 当前定位

截至 2026-08-18，最近主要提交已经从早期的 Torch Runtime / CxScript / Evidence，推进到：

```text
08-09  Metrology Analytics + Measurement Semantics
08-11  Torch / Test 修正
08-12  CxScript Case 修正
08-14  FastMatch / FindEllipse / FindSegmentation 修正
08-14  ResNet18 / ResNet50 Cases
08-16  ResNet18 / ResNet50 修正
08-17  Torch 修正
08-18  SAM Module Test
```

最新公开提交为 `3b260e2`。

当前更准确的定位是：

> **核心视觉、CxScript、Torch Runtime、Evidence、Pattern、Measurement 与 Metrology 框架已经基本形成；当前进入模型扩展之后的合同收敛、脚本资产闭合、统一结果升级、真实数据验收和跨入口一致性阶段。**

当前最大的风险已经由"有没有功能"转变为"功能越来越多，现有统一合同能否继续承载"。

---

## 2. v2.4 → v2.5 的关键变化

### 2.1 Torch 从两类模型扩展为多任务 Runtime

v2.4 时正式 Production Runtime 主要包括：

```text
DeepLabV3+ Segmentation
YOLOv8 Detection
Segmentation Training Lifecycle
```

当前 `TorchRuntimeTaskIds` 已增加：

```text
DeepLabV3Plus Segmentation
EdgeSAM Prompt Segmentation
YOLOv8 Detection
YOLOv8 Instance Segmentation
EdgeSAM Incremental Package
YOLOv8 Incremental Package
Prototype Incremental Lifecycle
Segmentation Training Lifecycle
```

Dispatcher 当前已经真实路由：

```text
DeepLab          → ExecuteTorchSegmentationTask
EdgeSAM          → ExecuteTorchEdgeSamTask
YOLO Detection   → ExecuteTorchDetectionTask
YOLO Instance Seg → ExecuteTorchYoloV8SegTask
Prototype Incr.  → ExecuteTorchPrototypeLifecycleTask
EdgeSAM Incr.    → ValidateTorchIncrementalPackageTask
YOLO Incr.       → ValidateTorchIncrementalPackageTask
Legacy           → TorchTestHost
```

因此：

```text
Torch Production Routing            [Implemented]
DeepLab Segmentation                 [Implemented]
YOLO Detection                       [Implemented/Verification Pending]
EdgeSAM Prompt Segmentation          [Implemented/Pending Binding]
YOLOv8 Instance Segmentation         [Implemented/Verification Pending]
Prototype Incremental Lifecycle      [Implemented/Experimental]
Incremental Model Package Gate       [Implemented/Pending Binding]
Legacy TestHost                      [Legacy]
```

### 2.2 Runtime DLL 已正式包含 EdgeSAM 与 YOLO-Seg

当前 `libtorch_module_runtime` 源集已经加入：

```text
torch_runtime_core.cpp
torch_runtime_task_dispatcher.cpp
torch_runtime_manifest.cpp
torch_runtime_artifact_writer.cpp
torch_runtime_contract.cpp

torch_runtime_segmentation_executor.cpp
torch_runtime_edgesam_executor.cpp
torch_runtime_yolov8_seg_executor.cpp
torch_runtime_detection_executor.cpp

torch_runtime_c_api.cpp
```

Prototype lifecycle 当前直接编译在 Dispatcher 中。

> 禁止再新增 EdgeSAM Runtime V2、YOLOSeg Runtime V2、Incremental Executor Framework、Model Runtime Manager V2。现有 Runtime 已经足够。新增模型应继续挂在 `DispatchTorchRuntimeTask()` 下面。

### 2.3 Torch Task Taxonomy 已开始落后

内部 `TorchRuntimeTaskIds` 已经拥有 Prompt Segmentation、Instance Segmentation、Prototype Incremental、Incremental Package。

但 `TorchProductionTaskKind` 仍只有：

```text
SegmentationInference
DetectionInference
Contract
Capabilities
Legacy
```

外层 `CxTorchTaskKind` 也缺少 InstanceSegmentation、PromptSegmentation、IncrementalUpdate。

> **task_id 已经成为真实模型语义，而 Kind 只剩粗粒度分类。**

v2.5 正式确定：`task_id = 精确执行合同`，`kind = 高层类别`。不再继续使用字符串 contains 来承担长期模型注册职责。

### 2.4 Canonical Result 成为新的核心瓶颈

现有 `CxInferenceResult` 对于普通检测和语义分割足够。但当前 Runtime 已开始输出 Classification、Feature Extraction、Prompt Segmentation、Instance Segmentation、Incremental Lifecycle。

YOLOv8-Seg Evidence 要求一个实例包含 stable ID、bbox、class/score、mask、contour、centroid、pixel area、oriented rectangle axes、rejected points、uncertainty。但 `TorchRuntimeResultAdapter` 实际只把 bbox/confidence/class_id 压成普通 `CxTorchDetection`，实例语义被压扁。

> **模型能力已经开始超过 Result Contract。**

### 2.5 Measurement Semantics 与 Metrology Analytics 已进入正式构建

当前根 CMake 已把 Vision、FindSegmentation EdgeSAM backend、CxCore、Calibration、Measurement Semantics、Metrology Analytics 和 Torch Integration 一并编入主目标。

Measurement Semantic Evidence 已能生成 13 个 sidecar JSON 文件，但 Calibration、Pattern model、Accuracy、Uncertainty 仍为占位状态。

---

## 3. 完整系统架构（8 个逻辑域）

v2.5 继续保持 **8 个逻辑域**。这些是职责域，不代表增加八层中间对象。

```text
┌─────────────────────────────────────────────────────────┐
│ 1. Application / Workbench                              │
│ Manual Console / Image View / Review / Evidence UI      │
└─────────────────────────┬───────────────────────────────┘
                          ▼
┌─────────────────────────────────────────────────────────┐
│ 2. CxScript Runtime & Assets                            │
│ Parser / Catalog / Case / Global / Manifest / Evidence  │
└─────────────────────────┬───────────────────────────────┘
                          ▼
┌─────────────────────────────────────────────────────────┐
│ 3. Unified Execution                                    │
│ Manual / Headless / Suite / Evidence / RuntimeCapture   │
└──────────────┬─────────────────────────┬────────────────┘
               ▼                         ▼
┌────────────────────────┐   ┌────────────────────────────┐
│ 4. Vision / CxCore     │   │ 5. Model / Learning       │
│ Find* / FastMatch      │   │ Torch / Pattern           │
│ Gauge / Geometry       │   │ mlpack / ensmallen        │
│ CxCore Boundary        │   │ Incremental Learning      │
└──────────────┬─────────┘   └─────────────┬──────────────┘
               └──────────────┬────────────┘
                              ▼
┌─────────────────────────────────────────────────────────┐
│ 6. Measurement / Metrology                              │
│ Calibration / Measurement Semantics / Surface Analytics │
└─────────────────────────┬───────────────────────────────┘
                          ▼
┌─────────────────────────────────────────────────────────┐
│ 7. Result / Geometry / Evidence                         │
│ CxExecutionResult / CxInferenceResult / Shape / Replay  │
└─────────────────────────┬───────────────────────────────┘
                          ▼
┌─────────────────────────────────────────────────────────┐
│ 8. Foundation / Build / Observability                   │
│ OpenCV / OCCT / LibTorch / cxgeom / CMake / Log         │
└─────────────────────────────────────────────────────────┘
```

当前根 CMake 已把 Vision、FindSegmentation EdgeSAM backend、CxCore、Calibration、Measurement Semantics、Metrology Analytics 和 Torch Integration 一并编入主目标。

### 3.1 目录结构

```
cxvision_repo/
├── CMakeLists.txt              # 根构建脚本
├── cximage/                    # 图像处理与 GUI 模块
│   ├── GuiMain.cpp             # GUI 入口
│   ├── ViewController.h/cpp    # 视图控制器 (顶层场景)
│   ├── ManualStateTestConsole.h/cpp  # 人工调试工作台主壳
│   ├── ManualConsoleGauge.h/cpp      # Gauge 控制器
│   ├── ManualConsoleEvidenceChain.h/cpp    # 证据链控制器
│   ├── ManualConsoleParamRegressionPanel.h/cpp  # 参数回归面板
│   ├── ManualConsoleScriptDebugPanel.h/cpp      # 脚本调试面板
│   ├── ManualConsoleFindLineDebug.h/cpp         # FindLine 调试
│   ├── ManualConsoleFindCircleDebug.h/cpp       # FindCircle 调试
│   ├── ManualConsoleRuntimeView.h/cpp           # 运行时视图
│   ├── ManualConsoleCxScriptDebug.h/cpp         # CxScript 调试
│   ├── ParserDebugBridge.h/cpp                  # 脚本调试桥接
│   ├── CxParserRuntimeOwner.h/cpp               # 解析器运行时所有权
│   ├── CxScriptHeadlessRunner.h/cpp             # 通用 Headless 运行器
│   ├── CxScriptHeadlessBindings.h/cpp           # Headless 绑定注册
│   ├── CxScriptSuiteRunner.h/cpp                # Suite 运行器
│   ├── CxParamProbeRunner.h/cpp                 # 参数探测运行器
│   ├── CxScriptCasePackageWriter.h/cpp          # Case 包写入器
│   ├── CxScriptRuntimeCaptureSmoke.h/cpp        # Runtime Capture Smoke
│   ├── CxRuntimeProjectionExecutor.h/cpp        # Runtime 投影执行器
│   ├── CxShapeInteractionRunner.h/cpp           # Shape 交互测试运行器
│   ├── CxShapeInteractionTest.h/cpp             # Shape 交互测试基类
│   ├── CxManifestProjectionRequestResolver.h/cpp  # Manifest 投影解析器
│   ├── ImageAnnotationLayer.h/cpp               # 图像注释层
│   ├── CxAnnotationToolRuntime.h/cpp            # 注释工具运行时
│   ├── CxCoreBoundary.h/cpp                     # CxCore 边界
│   ├── GridPatternClassNet.h/cpp                # Grid Pattern Net
│   ├── GridPatternClassTool.h/cpp               # Grid Pattern Tool
│   ├── RegionPatternNet.h/cpp                   # Region Pattern Net
│   ├── RegionPatternTool.h/cpp                  # Region Pattern Tool
│   ├── FindSegmentation.h/cpp                   # 分割检测算法
│   ├── FindSegmentationOpenCvSmokeBackend.h/cpp # OpenCV 分割后端
│   ├── FindSegmentationEdgeSamBackend.h/cpp     # EdgeSam 分割后端
│   ├── TorchRuntimeBridge.h/cpp                 # Torch 运行时桥接
│   ├── TorchRuntimeResultAdapter.h/cpp          # Torch 结果适配器
│   ├── CxTorchRuntimeService.h/cpp              # Torch Runtime Service
│   ├── CxTorchExecutionAdapter.h/cpp            # Torch 执行适配器
│   ├── CxTorchResultProjector.h/cpp             # Torch 结果投影器
│   ├── CxScriptEvidenceChainRuntime.h/cpp       # CxScript Evidence Chain Runtime
│   ├── CxScriptRuntimeResultCapture.h/cpp       # Runtime Result Capture
│   ├── CxUnifiedLog.h/cpp                       # 统一日志
│   ├── CxCrashLog.h/cpp                         # Crash 日志
│   └── ...
├── cxgeom/                     # 几何建模模块
│   ├── include/
│   └── src/
├── cxcloud/                    # 点云处理模块
│   ├── include/
│   └── src/
├── cxparser/                   # 脚本解析核心
│   ├── muParser*.h/cpp         # muParser 核心文件
│   ├── cxscript/               # CxScript 脚本案例
│   │   ├── module/
│   │   │   ├── cximage/        # cximage 脚本
│   │   │   ├── torch/          # torch 脚本
│   │   │   ├── mlpack/         # mlpack 脚本
│   │   │   └── ensmallen/      # ensmallen 脚本
│   │   └── integration/        # 集成测试
│   └── CMakeLists.txt
├── cxparser_ext/               # 脚本扩展层
│   ├── pipeline/               # 流水线组件
│   ├── runtime/                # CxScript 运行时
│   ├── catalog/                # 脚本目录
│   ├── validation/             # 验证引擎
│   ├── meta/                   # 元数据类型
│   ├── debug/                  # 调试嵌入层
│   ├── drivers/                # 驱动层
│   └── scenarios/              # 场景封装
├── libtorch_module/            # PyTorch 模块（模型内部能力）
│   ├── torch_runtime_core.cpp
│   ├── torch_runtime_task_dispatcher.cpp
│   ├── torch_runtime_manifest.cpp
│   ├── torch_runtime_artifact_writer.cpp
│   ├── torch_runtime_contract.cpp
│   ├── torch_runtime_segmentation_executor.cpp
│   ├── torch_runtime_detection_executor.cpp
│   └── torch_runtime_c_api.cpp
└── 3D/                         # 三维场景集成模块
    ├── src/
    └── tests/
```

---

## 4. 六条标准执行链

### 4.1 Traditional Vision Chain

```text
Image
→ ROI / Gauge
→ CxScript
→ FindLine / Circle / Ellipse / Rect / FastMatch
→ RuntimeResultCapture
→ CxExecutionResult
→ Geometry
→ Measurement Observation
→ Evidence
```

重点已经不应继续增加工具框架，而是固定图片、Gauge、参数和 Ground Truth。

### 4.2 Torch Inference Chain

```text
CxScript
→ TorchTask
→ CxTorchExecutionAdapter
→ CxTorchRuntimeService
→ Runtime DLL
→ DispatchTorchRuntimeTask
→ Model Executor
→ Runtime Artifact
→ TorchRuntimeResultAdapter
→ CxInferenceResult
→ CxTorchResultProjector
→ RuntimeCapture
→ Evidence
```

这条链已经成为正式模型运行主线。

### 4.3 Prompt / Instance Segmentation Chain

```text
Image
→ Prompt / Automatic Segmentation
→ EdgeSAM / YOLOv8-Seg
→ Mask / Instance / Boundary
→ Geometry
→ Boundary Refinement
→ Measurement Evidence
```

EdgeSAM 已进入 Dispatcher；YOLOv8n-Seg Case 则要求 per-instance mask、bbox、Segmentation Evidence、Measurement Evidence、tensor trace 和 weight mapping report。

### 4.4 Incremental Learning Chain

```text
Evidence Dataset
→ Incremental Update
→ Persisted Candidate Package
→ Paired Inference
→ Result
→ Evidence
→ Human Review
```

必须区分三种成熟度：

- **DeepLab Lifecycle**：persistent optimizer step + checkpoint/manifest export + paired mask inference + overlay。属于真实训练生命周期验证，但仍不能代表模型质量。
- **Prototype Lifecycle**：handcrafted semantic/geometry/texture/shape vector + PrototypeIndex.add_or_update + top1 query + persisted tensor + overlay。不是神经网络权重训练。状态必须写成 `network_weights_updated=false`。
- **EdgeSAM / YOLO Incremental**：当前属于 Package Gate，要求真实 TorchScript 导出文件存在且能够加载。没有真实权重时必须保持 `PENDING_BINDING`。

### 4.5 Measurement / Metrology Chain

```text
Vision / Model Geometry
→ CxCalibration
→ Physical Coordinates
→ Measurement Observation
→ Surface / Roughness / Area / Statistics
→ Reference Replay
→ Evidence
```

`CxCalibration` 当前被明确设计为轻量 value-semantic boundary，不拥有 Parser、Image、Shape、Find* 或 UI state。

### 4.6 Evidence / Acceptance Chain

```text
Image + Script + Globals + Model + Calibration + Expected Result
        ↓
Execution
        ↓
Canonical Result
        ↓
Geometry
        ↓
Measurement Semantic
        ↓
Overlay / Artifact / Trace
        ↓
Contract
        ↓
Human / Automated Review
```

核心原则：

```text
Runtime PASS
≠ Artifact PASS
≠ Geometry PASS
≠ Semantic PASS
≠ Accuracy PASS
```

---

## 5. Application / Workbench

当前 Workbench 仍围绕：

```text
ManualStateTestConsole
ManualConsoleGauge
ManualConsoleEvidenceChain
ManualConsoleParamRegressionPanel
ManualConsoleScriptDebugPanel
ManualConsoleRuntimeView
ImageAnnotationLayer
```

展开。

最近 Evidence Chain、Torch UI、HD Reference 和 Key Parameter Controls 持续修改，说明 UI 的工作重点已经从"有一个调试窗口"转变成：

> **给算法/脚本/模型提供统一的人工观察和证据审核表面。**

UI 的职责继续限定为：

```text
编辑
选择
运行
显示
审核
```

而不是：

```text
实现算法
实现模型
实现后处理
重新计算结果
```

### 5.1 ManualStateTestConsole

#### 定位
人工调试工作台主壳，提供证据链浏览、Gauge 编辑、参数调优、审核门和回放功能的统一入口。

#### 核心文件
- [ManualStateTestConsole.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/ManualStateTestConsole.h)
- [ManualStateTestConsole.cpp](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/ManualStateTestConsole.cpp)

#### 输入
- Catalog 脚本列表
- Image Manifest
- Evidence Chain
- 人工 Gauge 状态

#### 输出
- Script Evidence Thumbs
- ManualGaugeState 更新
- Param Regression 记录
- Replay Package 路径

#### 数据结构
- `ManualTestContext`：工作台主上下文
- `ManualGaugeState`：手动 Gauge 状态（line/circle 参数）
- `ManualParamRegressionState`：参数回归状态
- `EvidenceChainThumb`：证据链缩略图
- `ScriptEvidenceGroup`：脚本证据分组

#### 执行流程
1. Load Catalog → Load Manifest → Load Evidence Chain
2. Select Evidence Case → Load Gauge → Edit Gauge
3. Apply to Globals → Run Probe → View Result Overlay
4. Human Review → Save Annotation → Generate Manifest Candidate
5. (Optional) Param Regression → Mini Regression → Promotion

#### 状态
- **[Implemented]**：基础 UI 框架、证据链浏览、Gauge 编辑、参数调优面板
- **[Planned]**：完整 Promotion 流程、批量审核

### 5.2 ViewController

#### 定位
顶层场景控制器，继承自 OpenCASCADE 的 `AIS_ViewController`，负责 ImGui 界面与 OCCT 3D 视图的集成、事件路由和渲染编排。

#### 核心文件
- [ViewController.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/ViewController.h)
- [ViewController.cpp](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/ViewController.cpp)

#### 状态
- **[Implemented]**：基础事件路由、图像渲染、Annotation 集成

### 5.3 关键参数 UI

#### 定位
关键参数调优界面，提供参数范围、候选生成和整定散点图。

#### 状态
- **[Implemented/Partial]**：参数范围与候选模型已实现；关键参数 UI 部分实现；参数整定散点图为可视化占位

### 5.4 ManualGaugeState

#### 定位
手动 Gauge 状态，存储 Line/Circle/Ring Gauge 的几何参数和交互状态。

#### 核心文件
定义于 [ManualStateTestConsole.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/ManualStateTestConsole.h)

#### 数据结构
- `ManualGaugeState`：Gauge 状态（line_x0/y0/x1/y1, circle_cx/cy/radius 等）
- `GaugeHandleType`：Handle 类型（LineP0/LineP1/CircleCenter/CircleRadius 等）
- `LineGaugeGeometry`：直线 Gauge 几何
- `CircleGaugeGeometry`：圆形 Gauge 几何

#### 状态
- **[Implemented]**：Line/Circle/Ring Gauge 编辑、Handle 拖动、参数写回

---

## 6. CxScript Runtime & Asset System

### 6.1 当前主要资产类型

v2.4 统一整理为：

```text
catalog/
frozen/
diagnostic/
headless/
evidence/
manifest/
suite/
parameter/
contract/
review/
```

而不是继续按照历史开发阶段增加新的平行资产体系。

### 6.2 Global Value System

最新 Headless 工作加入了更系统的 `CxScriptGlobalValueSet`，支持：

```text
读取 global 声明
加载值文件
CLI / Case Override
Parser DefineVar
```

并严格限制 `global_xxx` 形式，禁止 `global.xxx` 点号写法。

这对于当前架构非常重要，因为：

```text
Manual 参数
Headless 参数
Evidence Locked 参数
```

开始具备共享同一套 CxScript Global 语义的条件。

### 6.3 类型注册

CxScript 当前除传统：

```text
Image
FindLine
FindCircle
FindEllipse
FindRect
FindObject
FindSegmentation
FastMatch
Gauge
```

之外，又开始纳入：

```text
GridPatternClassTool
RegionPatternTool
TorchTask
```

因此 CxScript 正从"传统图像工具脚本"升级为：

> **整个 CxVision 算法与模型工具链的统一逻辑层。**

### 6.4 CxScript Evidence Chain Runtime

`CxScriptEvidenceChainRuntime` 使用 muParser 直接执行 `.cxsc` Evidence Chain 定义，已成为真正运行时，而不再只是 UI 辅助配置。

#### 核心文件
- [CxScriptEvidenceChainRuntime.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxScriptEvidenceChainRuntime.h)

#### 状态
- **[Implemented]**：`.cxsc` Evidence Chain 可执行

### 6.5 cxparser / cxparser_ext Runtime

#### cxparser_ext
构建在 cxparser 核心之上的扩展层，提供完整的脚本执行流水线、类型绑定构建、流程路由、验证引擎、结果交付等企业级功能。

- 核心文件：[parser_pipeline.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cxparser_ext/pipeline/parser_pipeline.h)、[parser_runtime_facade.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cxparser_ext/pipeline/parser_runtime_facade.h)、[cxscript_runtime.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cxparser_ext/runtime/cxscript_runtime.h)
- 执行流程：`PrepareTask → MergeBindingSpec → MergeEvidence → Run → Validate → Deliver`
- 状态：**[Implemented]**

#### cxparser
基于 muParser 扩展的脚本解析核心引擎。

- 核心文件：[muParser.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cxparser/muParser.h)、[muParserBase.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cxparser/muParserBase.h)、[muParserBytecode.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cxparser/muParserBytecode.h)、[muParserClass.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cxparser/muParserClass.h)
- 状态：**[Implemented]**

### 6.6 Asset 目录结构

```
cxparser/cxscript/module/
├── cximage/                    # cximage 脚本资产
│   ├── catalog/                # 目录注册
│   ├── stage25/                # Stage2.5 资产
│   ├── frozen/                 # 冻结脚本
│   ├── tests/                  # 测试脚本
│   ├── frame_probe/            # 帧探测
│   └── diagnostic/             # 诊断脚本
├── torch/                      # torch 脚本资产
│   ├── detect_direct_test.cxsc
│   ├── segmentation_direct_test.cxsc
│   └── ...
├── mlpack/                     # mlpack 脚本资产
│   ├── logreg_predict_direct_test.cxsc
│   ├── mlpack_logreg_predict_direct_test.cxsc
│   └── ...
└── ensmallen/                  # ensmallen 脚本资产
    ├── ensmallen_geometry_tuning_direct_test.cxsc
    ├── geometry_tuning_direct_test.cxsc
    └── ...
```

---

## 7. Vision / CxCore Runtime

### 7.1 传统工具

当前主线仍包括：

```text
FindLine
FindCircle
FindEllipse
FindRect
FindObject
FindSegmentation
FastMatch
CircleRingGauge
FormfitGauge
```

`CxScriptRuntimeResultCapture` 当前已经直接获取这些对象的内部运行结果，并通过 `PublishDisplayShapes()` 转换成统一 Snapshot；FindCircle 还增加了 fit filter、candidate、boundary 等更细的调试信息。

传统工具下一阶段不应再重点增加框架，而应推进：

```text
固定图片
固定 Gauge
固定参数
真实结果
多入口一致性
```

#### 核心文件
- [FindLine.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/FindLine.h) / [FindLine.cpp](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/FindLine.cpp) — **[Implemented]**
- [FindCircle.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/FindCircle.h) / [FindCircle.cpp](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/FindCircle.cpp) — **[Implemented]**
- [FindEllipse.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/FindEllipse.h) / [FindEllipse.cpp](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/FindEllipse.cpp) — **[Implemented]**
- [FindRect](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/FindRect.h) — **[Implemented]**
- [FindObject.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/FindObject.h) / [FindObject.cpp](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/FindObject.cpp) — **[Implemented]**
- [FastMatch.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/FastMatch.h) / [FastMatch.cpp](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/FastMatch.cpp) — **[Implemented/Partial]**（GridPattern 接入进行中）
- CircleRingGauge / FormfitGauge — **[Implemented]**
- [FindSegmentation.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/FindSegmentation.h) / [FindSegmentation.cpp](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/FindSegmentation.cpp) — **[Implemented/Verification Pending]**（多后端质量待验证）

### 7.2 CxCore Boundary

当前 `CxCoreBoundary.cpp` 已经进入正式 `CXIMAGE_CORE_SOURCES`。

状态更新为：

```text
CxCoreBoundary contract        [Implemented]
CxCoreBoundary build           [Implemented]
CxCore Feature output          [Implemented]
AI semantic routing            [Implemented/Partial]
Fixed regression               [Verification Pending]
```

删除 v2.3 中 `[Build Pending]` 描述。

### 7.3 Pattern Tool

#### GridPatternClassTool / GridPatternClassNet
- Feature/Descriptor: **[Implemented]**
- Geometry Projection: **[Implemented]**
- Classifier Binding: **[Partial]**（`classifier=model_not_bound`）

可生成：Grid Feature Map、Hierarchy Descriptor、Active Cells、Orientation Geometry

#### RegionPatternTool / RegionPatternNet
- Feature/Descriptor: **[Implemented]**
- Geometry Projection: **[Implemented]**
- Classifier Binding: **[Partial]**（`classifier=model_not_bound`）

可生成：Region Descriptor、Foreground Ratio、Pooling Blocks、Mean / Std

两者都能够发布 ROI、Cell/Block 等真实 Shape。

---

## 8. Model & Optimization Runtime

### 8.1 libtorch_module

现有模块继续负责：

```text
DeepLabV3 / DeepLabV3Plus
YOLOv8
MobileViT
训练
评估
推理
模型结构
模型内部前后处理
```

Runtime DLL 不再只是 TestHost，而已经有 Production Dispatcher 和真实 Executor。

#### 模块内部生产源集

```text
torch_runtime_core.cpp
torch_runtime_task_dispatcher.cpp
torch_runtime_manifest.cpp
torch_runtime_artifact_writer.cpp
torch_runtime_contract.cpp
torch_runtime_segmentation_executor.cpp
torch_runtime_detection_executor.cpp
torch_runtime_c_api.cpp
```

#### 状态
- libtorch 模型内部: **[Module Verified]**（Train/Infer 基础完成）
- Torch Runtime DLL: **[Implemented]**（生产源集完整）
- Torch Production Dispatcher: **[Implemented]**（已成为 RunTorchTask 主路由）
- Torch Contract: **[Implemented]**（Seg/Detection）
- Legacy TestHost: **[Legacy / Diagnostic]**

### 8.2 Segmentation

当前 Segmentation Executor 已经完成：

```text
Manifest
→ Image Load
→ Resize / RGB / Normalize
→ Model Load
→ Forward
→ Argmax
→ Binary Mask
→ Contour
→ Metrics
→ Overlay
→ Evidence
```

实际会生成：

```text
mask_labels.png
mask_binary.png
mask_overlay.png
contours.json
segmentation_metrics.json
torch_segmentation_task_result.json
torch_runtime_evidence.json
```

并把轮廓面积、foreground ratio、输入/输出尺寸等写入结果资产。

#### 权重格式

当前支持：`cpp_archive / torchscript / jit_archive / cpp_state_dict`

对于 `python_state_dict`，DeepLab Executor 会明确返回 `python_state_dict_requires_cpp_archive_conversion`，而不再把它错误地直接当 TorchScript 执行。

现有 T5 smoke manifest 已经使用：
```text
weights_format = cpp_state_dict
architecture = deeplabv3plus
backbone = mobilenet_v3_large
input = 512 × 512
```

#### 状态
- Segmentation Executor: **[Implemented]**

### 8.3 Detection

当前 Detection Executor 已完成：

```text
Image
→ Letterbox
→ Tensor
→ YOLO Forward
→ Prediction Layout Normalize
→ Post Process / NMS
→ Original Image Coordinate Restore
→ detections.json
→ bbox_candidate_list.json
→ detection_overlay.png
→ Evidence
```

它对 `python_state_dict` 的处理与 DeepLab 不同：TorchScript-like archive 走 `jit::load`，否则走 YOLOv8 C++ model `load_checkpoint / load_weights / forward`。

当前 Torch Evidence 文档仍明确：detection CPU smoke 仍存在已知的 weights/class compatibility 问题。

#### 状态
- Production Executor: **[Implemented]**
- Artifact Generation: **[Implemented]**
- Model Compatibility: **[Partial]**
- Final Accuracy: **[Verification Pending]**

### 8.4 EdgeSAM Prompt Segmentation

EdgeSAM Executor 加载 encoder/decoder TorchScript 模块，要求 manifest 中 `"architecture": "edge_sam"`。

预处理：长边缩放到 1024，零填充到 1024×1024，RGB，按 mean `[123.675, 116.28, 103.53]` / std `[58.395, 57.12, 57.375]` 归一化。

推理：encoder 产出 embeddings，decoder 接受 `(embeddings, coords, labels)` 返回 `(scores, masks)`，选 best mask，插值回原图尺寸。

支持 Python/C++ 一致性校验（可选加载 embedding/point_coords/expected_scores/expected_masks 张量，对比 decoder 输出 max abs，容忍度默认 0.0001）。

产出 schema：`cxvision.torch.edgesam.result.v1` 和 `cxvision.torch.edgesam.evidence.v1`。

#### EdgeSAM 双入口

当前同时存在：
- `FindSegmentationEdgeSamBackend.cpp` — 统一 Vision Tool 抽象，可选择 EdgeSAM backend
- Torch EdgeSAM Prompt Executor — 直接模型验证 / Evidence / Model Lifecycle

正确原则：`FindSegmentationEdgeSamBackend` 应调用或复用同一个正式 EdgeSAM 模型执行核心，而不是独立复刻模型逻辑。

#### 状态
- EdgeSAM Prompt Runtime: **[Implemented/Pending Binding]**
- EdgeSAM Backend Integration: **[Implemented/Integration Pending]**

### 8.5 YOLOv8 Instance Segmentation

YoloV8-Seg Executor 完成多尺度 strides 8/16/32 候选解码、DFL expectation、NMS、mask 合成（prototypes 矩阵乘系数 + crop gate + 双线性插值）。

Evidence 要求 per-instance：stable_id、bbox、class/score、mask、contour、centroid、pixel_area、oriented rectangle axes、rejected points、uncertainty。

产出 schema：`cxvision.segmentation_evidence.v2`，写入 `instances.json`/`mask_labels.png`/`contours.json`/`tensor_shape_trace.json`/`weight_mapping_report.json`/`refined_edge_points.json`/`measurement_evidence.json`。

#### 状态
- YOLOv8 Instance Segmentation Runtime: **[Implemented/Verification Pending]**
- Instance Segmentation Canonical Result: **[Partial]**（实例语义被压扁为 CxTorchDetection）

### 8.6 ResNet18 / ResNet50

ResNet18 含 BasicBlock（2 个 ConvModule + downsample + ReLU），stem（3→64, 7×7 stride2）+ MaxPool + layer1-4（channels 64/128/256/512），feature 输出 {128,256,512}。

ResNet50 含 Bottleneck（3 个 ConvModule 1×1/3×3/1×1 + downsample + ReLU，expansion=4），fc 维度 2048→num_classes，feature 输出 {512,1024,2048}。

两者均含：torchvision key 重映射加载、AMP 训练（ManualGradScaler）、冻结骨干、特征金字塔输出（p3/p4/p5）、mermaid 结构图导出。

Evidence Case 验证：classifier_output_shape、p3/p4/p5 feature shapes、baseline_feature_ref、baseline_class_ref。

#### 状态
- ResNet18/50 Runtime: **[Implemented]**
- ResNet18/50 Evidence Case: **[Implemented]**
- Typed Classification Result: **[Partial]**
- Typed Feature Result: **[Partial]**
- Semantic Quality: **[Verification Pending]**

### 8.7 Prototype Incremental Lifecycle

使用 `PrototypeIndex` 完成 handcrafted 向量（semantic/geometry/texture/shape）的增量合并和 top-k 查询。

流程：Image → 灰度+Canny → 4 路向量计算 → `add_or_update` ×2 → `search_topk(query, 1)` → 写出 `prototype_vectors.pt`/`prototype_overlay.png`/`prototype_result.json`/`prototype_evidence.json`。

明确标记：`incremental_update_executed=true`、`network_weights_updated=false`、`semantic_quality="pending_human_review"`。

`IncrementalFeaturePipeline` 进一步集成 `MultiBranchFeatureHead` + `MultiFeatureFusionHead` + `LlamaBridge`（LLM 重排在 top1-top2 < 0.15 时触发）。

#### 状态
- Prototype descriptor update: **[Implemented]**
- Prototype persisted tensor: **[Implemented]**
- Prototype paired top1 query: **[Implemented]**
- Neural network update: **[Not Applicable]**
- Semantic quality: **[Verification Pending]**

### 8.8 Incremental Package Gate

EdgeSAM Incremental Package (`torch.train.segmentation.edgesam.decoder.v1`)：验证 encoder/decoder TorchScript 文件可加载。

YOLO Incremental Package (`torch.train.detection.yolov8.package.v1`)：验证 weights TorchScript 文件可加载。

没有真实权重时必须保持 `PENDING_BINDING`，不得产生模拟 PASS。

#### 状态
- EdgeSAM incremental package validation: **[Implemented/Pending Binding]**
- YOLO incremental package validation: **[Pending Binding]**
- YOLOv8-Seg incremental head/proto plan: **[Pending Binding]**
- Real EdgeSAM incremental network training: **[Pending Binding]**
- Real YOLO incremental model acceptance: **[Pending Binding]**

### 8.9 TorchTask 类型映射断点

`TorchRuntimeTaskIds` 已有 `torch.incremental.prototype.lifecycle.v1`，但 `TorchTask::settask()` 的类型判断主要依赖字符串关键字（train/segmentation/detection/classification/feature/template/smoke/infer），没有独立的 `incremental`/`prototype` 映射。

一个只包含 `incremental.prototype.lifecycle` 的正式 task id 会落到 `Unknown`，而 `ValidateCxTorchTaskSpec()` 会直接拒绝 `Unknown`。

#### 状态
- Dispatcher supports Prototype Lifecycle: **[Implemented]**
- TorchTask type mapping: **[Partial]**
- CxScript → Prototype Lifecycle: **[Blocked / Needs Fix]**

---

## 9. Measurement / Metrology

### 9.1 Measurement Semantics

当前 Measurement Semantic Evidence 已经能生成 13 个 sidecar JSON 文件：

```text
measurement_semantic_input.json
calibration_snapshot.json
coordinate_transform_trace.json
measurement_behavior_trace.json
measurement_observations.json
boundary_analysis.json
measurement_relations.json
measurement_feature_vector.json
semantic_pattern_result.json
accuracy_evaluation.json
uncertainty_budget.json
algorithm_provenance.json
measurement_semantic_contract_result.json
```

但三个关键状态仍然明确没有闭合：

```text
Calibration             CALIBRATION_NOT_BOUND
Pattern model           PENDING_MODEL_BINDING
Accuracy                PENDING_GROUND_TRUTH
Uncertainty             UNCERTAINTY_INCOMPLETE
```

#### 状态
- Semantic Sidecar Framework: **[Implemented]**
- Runtime Observation Export: **[Implemented]**
- Calibration Binding: **[Partial]**
- Pattern Model Binding: **[Pending Binding]**
- Ground Truth Accuracy: **[Pending Binding]**
- Uncertainty Budget: **[Partial]**

### 9.2 Calibration

`CxCalibration` 当前已经是一个较干净的 value-semantic 边界，具备：

```text
XY transform (scale_x/y, offset_x/y, rotation_deg, shear_x/y)
Z transform (z_scale, z_offset)
coordinate frame
physical units
uncertainty
snapshot + snapshot_hash
pixel ↔ physical
transform trace
```

但 Measurement Semantic Writer 当前仍固定输出：

```text
CALIBRATION_NOT_BOUND
image_pixel
px
IDENTITY_PIXEL_TRANSFORM
```

最重要的 Calibration 工作不是继续扩展类，而是：

```text
CxCalibrationSnapshot
→ CxScript Case / Headless Context
→ Measurement Semantic Writer
→ Physical Observation
→ Evidence
```

#### 状态
- Calibration Boundary: **[Implemented]**
- Calibration → Evidence Binding: **[Partial]**
- Physical Unit Measurement: **[Partial]**

### 9.3 Metrology Analytics

当前正式构建已经包含：

```text
CxPhysUnit
CxSurfaceField
CxSyntheticSurfaceFactory
CxSurfaceAreas
CxSurfaceBasicStats
CxSurfaceLevelPlane
CxSurfaceUnitConversion
CxRoughness1D
CxMetrologyUiGlobals
CxMetrologyReferenceReplay
CxMetrologyAnalyticsSmoke
```

其中 Observation Bridge 代码自己仍明确返回：`S3_S4_BRIDGE_DRAFT_ONLY_PENDING_S4_REVIEW`。

#### 2026-08-27 Key Parameter Controls 精细分析复核

`Key Parameter Controls -> Metrology Extension / Surface Analytics` 采用“可靠默认值先行、低频参数按需展开”的交互约束。常规使用不要求先调参数；默认参数应能直接形成可比较结果，`Fine Tuning` 默认折叠，仅用于边界样本或人工复核时的小范围修正。

| 模块 | 可靠默认值 | 当前运行边界 |
|------|------------|--------------|
| Find Peaks | 最大 12 个峰；按位置排序；双侧局部极小值基线；prominence 20/1000；最小间距 4 bins | 已绑定高度分布峰值分析，界面提供分布图、自动峰位和明细表 |
| Curve Fitting | ADF；Gaussian；自动初值；自动绘图；全范围 | 参数与界面已建立，真实拟合运行保持 `PENDING_BINDING` |
| Critical Dimension | Scan profile；Edge height (right)；自动拟合；全范围 | 六类函数选择与参数界面已建立，真实 CD 运行保持 `PENDING_BINDING` |

Find Peaks 用于把精度变化转化为可复核的屏幕证据。复核时至少比较峰位漂移、prominence、峰间距、峰数量、分布曲线与明细表的一致性，以及 FindCircle 精度调整前后结论点高度分布的集中程度和异常尾部。

参数区提供 `Restore Defaults`；恢复后必须回到上表基线。Fine Tuning 不应成为每次运行的必需步骤。

本轮实现和代码复核覆盖：

- Find Peaks 的真实高度分布峰值计算、排序、数量限制、prominence 和最小 bin 间距；
- Find Peaks 的分布图、峰标记和结果明细表；
- Curve Fitting 与 Critical Dimension 的可靠默认参数摘要、恢复默认值和折叠式 Fine Tuning；
- 62 个 `global_metrology_*` 参数值进入统一界面状态；
- smoke 增加逐 case 进度输出与 `metrology_analytics_smoke_progress.log` 入口标记。

自动 GUI 复核使用 `codex_lan_agent_18080` 的截图、鼠标、键盘、快捷键和窗口激活动作。已观察到 `F7` 可定位 Key Parameter Controls，`F12` 可定位 Analytics Smoke；Find Peaks 显示可靠默认值摘要，Fine Tuning 默认折叠，展开后可见峰数 12、位置排序、prominence 20/1000 和最小间距 4 bins。

截图证据：`verification_find_peaks_defaults.png`、`verification_find_peaks_fine_tuning.png`、`verification_metrology_extension_open.png`、`verification_shared_gui_after_processing.png`。

本轮编译命令：

```powershell
cmake --build D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\build01 --config Release --target cxvision_imgui_acceptance
```

编译退出码为 0，目标文件为 `D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\build01\Release\cxvision_imgui_acceptance.exe`，结论为 `COMPILE_PASS`。构建日志：`D:\Codex-WorkDir\Sean_WorkDir\codex-lan-agent\logs\build_target_cmake_20260827_121609.log`。

```text
Metrology UI/source implementation  IMPLEMENTED
Release target compile              COMPILE_PASS
Find Peaks updated runtime smoke    PENDING_HUMAN_REVIEW
Curve Fitting runtime               PENDING_BINDING
Critical Dimension runtime          PENDING_BINDING
Manual GUI review                   NOT_RUN
Final acceptance                    NOT_ACCEPTED
```

GUI 更新期间由多个线程共享同一窗口；更新完成后本线程只执行最终截图，没有继续改变共享界面状态。当前没有独占运行完整 smoke、真实 SurfaceField 分析或人工判定，因此不得提升为最终算法验收通过。下一步应在独占 GUI 会话中固定 SurfaceField 输入，复核 Restore Defaults、Fine Tuning、Find Peaks 分布图/峰表和重复性，再进入人工 Review。


##### 2026-08-27 Gauge Line NUM 选择补充

精细分析区域新增共享的 `Gauge Line NUM` 选择器，编号从 1 开始。FindCircle 已有运行时对象时，上限取真实 `circle_scan_lines_processed`；FindLine 已有运行时对象时，上限取真实 `line_scan_rows_examined`；尚未形成运行时扫描事实时，上限退回当前 `max scan lines` 配置。界面同时显示 `Selected Gauge Line: NUM n / total`，Find Peaks、Curve Fitting、Critical Dimension 曲线区域均显示当前 NUM。

选择值投影为 `global_metrology_gauge_line_num`。NUM 改变后会清除旧 Find Peaks 分析就绪状态和峰表，要求针对新选择重新分析，防止把上一条 Gauge Line 的结果误认为当前线结果。Curve Fitting 与 Critical Dimension 的单线运行数据仍为 `PENDING_BINDING`，参数预览不冒充真实拟合或 CD 结果。

本轮编译命令仍为：

```powershell
cmake --build D:\\Codex-WorkDir\\Sean_WorkDir\\cxvisionai\\build01 --config Release --target cxvision_imgui_acceptance
```

退出码为 0，结论为 `COMPILE_PASS`。构建日志：`D:\\Codex-WorkDir\\Sean_WorkDir\\codex-lan-agent\\logs\\build_target_cmake_20260827_151445.log` `verification_gauge_line_num_postbuild_20260827_1519.png` GUI `ui_analyze`  GUI  `PENDING_HUMAN_REVIEW`


##### 2026-08-27 

人工截图 `D:\\Screenshot 2026-08-27 154337.png` 暴露 Find Peaks、Curve Fitting、Critical Dimension 区域仍显示公式生成的 preview 曲线，容易被误认为真实 Gauge Line 结果。本轮已将 `ManualConsoleParamRegressionPanel.cpp` 中的 metrology preview 数据源改为只接受明确标记为 selected Gauge Line runtime profile 的来源，例如 `runtime:gauge_line:` 或 `gauge_line:` 前缀；整图灰度 SurfaceField 和 synthetic SurfaceField 都不能再驱动这三块 preview 曲线。

没有选中 Gauge Line 的真实 profile / ADF / BCDF 时，图表显示 `NO_RUNTIME_PROFILE` 空态，不再生成 synthetic ADF、fit 或 CD model 曲线。Find Peaks 只在有 selected Gauge Line runtime profile 时绘制 measured ADF 和检测到的 peak marker。Curve Fitting 与 Critical Dimension 在真实单线拟合/测量绑定完成前只允许显示 measured source profile；fit/model 曲线保持空，并标注 `PENDING_BINDING`，不得冒充 runtime fit 或 CD result。



```powershell
cmake --build D:\\Codex-WorkDir\\Sean_WorkDir\\cxvisionai\\build01 --config Release --target cxvision_imgui_acceptance
```

当前构建停在链接阶段，`LINK : fatal error LNK1104` 无法打开 `D:\\Codex-WorkDir\\Sean_WorkDir\\cxvisionai\\build01\\Release\\cxvision_imgui_acceptance.exe`，原因是 GUI 仍在运行并占用二进制。结论为 `COMPILE_BLOCKED_ENV`，构建日志：`D:\\Codex-WorkDir\\Sean_WorkDir\\codex-lan-agent\\logs\\build_target_cmake_20260827_160353.log`。下一步需要关闭或释放当前 GUI 后重跑同一目标编译，再做截图复核。




#### 
- Surface Data Model: **[Implemented]**
- Physical Unit: **[Implemented]**
- Surface Statistics: **[Implemented]**
- Surface Area: **[Implemented]**
- Plane Level: **[Implemented]**
- Roughness 1D: **[Implemented]**
- Reference Replay: **[Implemented/Verification Pending]**
- Analytics Observation Bridge: **[Draft]**
- Real Instrument Validation: **[Verification Pending]**

### 9.4 Pattern / mlpack

GridPattern  Feature MapHierarchyDescriptorActive CellGeometry Overlay summary  `classification=model_not_bound`

#### 
- Feature / Descriptor: **[Implemented]**
- Geometry: **[Implemented]**
- Classifier Binding: **[Partial]**
- End-to-End Classification: **[Verification Pending]**

---

## 10. mlpack / ensmallen

### 10.1 mlpack

8  7  ELPV  Evidence Chain

```text
24 / 24 images copied
24 / 24 evidence cases landed
24 UI loaded-elements rows
24 classification rows
```



```text
HEADLESS_EXECUTION: NOT_RUN
COMPILE: NOT_RUN
MANUAL_GUI_REVIEW: NOT_RUN
FINAL_ACCEPTANCE: NOT_ACCEPTED
```

#### 
- mlpack CxScript Assets: **[Implemented]**
- Local Evidence Landing: **[Implemented]**
- Semantic Refs: **[Implemented/Contract]**
- Headless Runtime Acceptance: **[Verification Pending]**
- Manual Review: **[Verification Pending]**
- Final Acceptance: **[Not Accepted]**

 Evidence Case  UI  mlpack  Runtime Verified

### 10.2 ensmallen

`Candidate / Objective    Best Parameter`

 ensmallen  Objective 

#### 
- CxScript Semantic Assets: **[Implemented]**
- Real Optimization Loop: **[Partial / Placeholder]**

 Verified

---

## 11. Canonical Result



```text
CxExecutionResult
  optional<CxInferenceResult>
```

`CxInferenceResult` 

```text
schema
schema_version
task_id
case_id
model_id
model_hash
requested_device
actual_device
status
runtime
detections
mask
metrics
artifact_refs
result_ref
evidence_ref
primary_visual_ref
```

 `TorchGuiResultV2``TorchGeometryResult``TorchEvidenceResult``ModelResultV3` 

 `CxInferenceResult`

---

## 12. Result Adapter 

 Adapter 

```text
Status
Device
Runtime
Result Ref
Evidence Ref
Detection
Mask Ref
Contour Ref
Overlay Ref
```

 `CxInferenceResult` 

```text
schema
schema_version
model_hash
metrics
artifact_refs
mask.width
mask.height
mask.foreground_ratio
detection.class_name
```

""

```text
Runtime Artifact
 ResultAdapter
  CxInferenceResult
```

---

## 13. Geometry Projection

### 13.1 Detection



```text
CxTorchDetection
 Rect Shape
 stable_ref
 owner_ref
 model_best_result / model_candidate
```

#### 
- Detection Shape Code: **[Implemented]**
- : **[Implemented]**
- : **[Verification Pending]**

### 13.2 Segmentation



```text
Mask
 Mask Shape

contours.json
 Parse First Contour
 Polyline Shape
```

 v2.3  Polyline

#### 
- Mask Semantic Shape: **[Implemented]**
- Contour Geometry: **[Implemented]**
- Mask Image Overlay: **[Implemented]**
- / Mask Geometry: **[Partial]**
- Fixed Coordinate Parity: **[Verification Pending]**

 contourholecomponent idmask dimensions

---

## 14. Runtime Result Capture

`CxScriptRuntimeResultCapture`  TorchTask  Capture 



```text
torch_ok
error_code
train/infer time
result_count
result_ref
evidence_ref
mask_ref
overlay_ref
contour_ref
```

 `CxTorchResultProjector::Project()`  Shape Snapshot



> **Torch  UI  Runtime Capture**

 v2.4 

#### 
- Runtime Result Capture: **[Implemented]** + Torch

---

## 15. Evidence / Review

### 15.1 Torch Evidence Chain

 Torch UI Evidence Chain

| Case | Level |  |
|------|-------|------|
| Segmentation cpp_state_dict | T5 | mask/result/evidence/overlay |
| Segmentation Contract | T4 | manifest + contract |
| YOLOv8 Detection | T6 | detection + artifacts |
| Detection Contract | T4 | detection contract |
| Training Lifecycle | T3 | tiny CPU lifecycle |

#### 
- Torch Evidence Chain: **[Implemented/Verification Pending]** Cases 
- Torch Semantic Accuracy: **[Verification Pending]** Runtime Smoke

### 15.2  Evidence Guardrail



```text
T5 smoke
 semantic segmentation accuracy

T6 detection smoke
 final detector quality

tiny training lifecycle
 model training quality acceptance

UI evidence visible
 final model acceptance
```

 Architecture Rules" = "

### 15.3 Evidence Chain Runtime

`CxScriptEvidenceChainRuntime`  `.cxsc` Evidence Chain 

#### 
- [CxScriptEvidenceChainRuntime.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxScriptEvidenceChainRuntime.h)

#### 
- **[Implemented]**`.cxsc` Evidence Chain 

### 15.4 

|  |  |
|------|------|
| Snapshot | [Implemented] |
| Summary | [Implemented] |
| Trace | [Implemented] |
| Overlay | [Implemented] |
| Replay | [Implemented] |
| Contract | [Implemented] |
| Review Gate | [Implemented] |
| Human Review | [Implemented] |
| Promotion | [Disabled] |

---

## 16. Build / Runtime

### 16.1 

 CMake 

```text
CxCoreBoundary
GridPatternClassTool
RegionPatternTool
TorchRuntimeBridge
CxTorchRuntimeService
TorchRuntimeResultAdapter
CxTorchExecutionAdapter
CxTorchResultProjector
TorchTask
CxScriptEvidenceChainRuntime
CxScriptRuntimeResultCapture
```

`libtorch_module_runtime` 

```text
Core
Dispatcher
Manifest
Contract
Artifact
Segmentation
Detection
C API
```



```text
cxvision_imgui_acceptance
        
        
cximage Integration
        
        
libtorch_module_runtime.dll
        
        
libtorch_module Model Runtime
```

 DLL

### 16.2 

- ****Windows
- ****MSVC (Visual Studio)
- **CMake**3.21 
- **C++ **C++17 () / C++14 (cxparser)
- ****GLFW 3.3.10OpenCASCADE 7.7.0OpenCVPyTorch (libtorch)

### 16.3 

|  |  |  |
|------|--------|------|
| `CXVISION_ENABLE_CXPARSER_EXT_DEBUG_INPROC` | ON |  cxparser_ext  |
| `CXVISION_ENABLE_LEGACY_STAGE25_CPP` | OFF |  C++ Stage25  |
| `CXVISION_BUILD_CXPARSER_RETURN_TESTS` | ON |  return  |
| `LIBTORCH_ROOT` | - | PyTorch  |

### 16.4 

#### cxvision_imgui_acceptance

```bash
mkdir build && cd build
cmake .. -DGLFW_ROOT="D:/glfw-3.3.10" ^
         -DGLAD_ROOT="path/to/glad" ^
         -DOCCT_ROOT="D:/OpenCASCADE-7.7.0-vc14-64/opencascade-7.7.0" ^
         -DOpenCV_DIR="D:/opencv/build" ^
         -DLIBTORCH_ROOT="path/to/libtorch"
cmake --build . --config Release
```

### 16.5 

 `cxvision_imgui_acceptance.exe` ManualStateTestConsole 

---

## 17. Test Taxonomy / Acceptance

### 17.1 

 Stage25 

|  |  |  |
|------|------|------|
| IMG-L0 / IMG-L1 / IMG-L2 / IMG-L3 |  |  |
| UI-L0 / UI-L1 / UI-L2 / UI-L3 | UI  | ShapeGauge  Pointer  |
| REG-S0 / REG-S1 / REG-S2 / REG-S3 |  |  casemini regression regression |
| T0 - T7 | Torch  | Torch Runtime  |

### 17.2 Torch T0-T7

v2.4  Torch 

```text
T0  Asset / Image / Manifest Preflight

T1  CxScript Type Binding
    TorchTask registered and callable

T2  Runtime Lifecycle
    DLL load/create/version/run/destroy

T3  Runtime Diagnostic / Tiny Train Lifecycle

T4  Contract
    Segmentation Contract
    Detection Contract

T5  Segmentation Runtime
    Forward
    Mask
    Contour
    Overlay
    Evidence
    Geometry Projection

T6  Detection Runtime
    Forward
    Detection
    BBox
    Overlay
    Evidence
    Geometry Projection

T7  Entry Parity
    Manual
    Headless
    Evidence Chain
    Suite
```

T5/T6 

```text
Runtime PASS
Model Quality PASS
```



### 17.5 

 Gauge  +  case Probe +  Replay/Review + mini-regression 

### 17.6 

 Case 
- `snapshot.txt`
- `result_summary.json`
- `result_overlay.png`
- `evidence_overlay.png`
- `tool_display.png`

### 17.7 Contract Pass 

- **FindLine** < 2 
- **FindCircle** < 3 

---

## 17. Current Status Matrix  v2.4

|  |  | v2.4  |
| ------ | -------- | --------- |
| Manual Console | `[Implemented]` |  |
| ImageAnnotationLayer | `[Implemented]` | Shape/Interaction  |
| Gauge  Globals | `[Implemented]` |  |
| Headless Global Value Set | `[Implemented]` |  |
| CxScript Parser | `[Implemented]` |  |
| CxScript Evidence Runtime | `[Implemented]` | `.cxsc` Evidence  |
| Runtime Result Capture | `[Implemented]` |  + Torch |
| Evidence UI | `[Implemented/Partial]` |  |
| HD Reference | `[Implemented/Partial]` |  Evidence Chain |
| FindLine | `[Implemented]` |  |
| FindCircle | `[Implemented]` | / |
| FindEllipse | `[Implemented]` |  |
| FastMatch | `[Implemented/Partial]` | GridPattern  |
| FindSegmentation | `[Implemented/Verification Pending]` |  |
| CxCoreBoundary | `[Implemented]` |  |
| GridPatternClassNet | `[Implemented]` | Feature/Hierarchy |
| GridPatternClassTool | `[Implemented/Partial]` | classifier  |
| RegionPatternNet | `[Implemented]` | Descriptor |
| RegionPatternTool | `[Implemented/Partial]` | classifier  |
| libtorch  | `[Module Verified]` | Train/Infer  |
| Torch Runtime DLL | `[Implemented]` |  |
| Torch Production Dispatcher | `[Implemented]` |  RunTorchTask  |
| Torch Contract | `[Implemented]` | Seg/Detection |
| Segmentation Executor | `[Implemented]` |  Forward + Artifact |
| Detection Executor | `[Implemented/Partial]` |  |
| Torch Result Adapter | `[Implemented/Partial]` | Mask/Detection  |
| Detection Geometry | `[Implemented]` | Rect Shape |
| Segmentation Contour Geometry | `[Implemented/Partial]` |  |
| Torch Evidence Chain | `[Implemented/Verification Pending]` |  Cases  |
| Torch Semantic Accuracy | `[Verification Pending]` |  Runtime Smoke |
| CUDA Device Contract | `[Partial]` | `cuda/gpu`  |
| mlpack Evidence Landing | `[Implemented]` | 24 ELPV Cases  |
| mlpack Headless Acceptance | `[Verification Pending]` |  |
| ensmallen Optimize | `[Partial/Placeholder]` |  |
| Parameter Regression | `[Partial]` |  |
| Promotion Gate | `[Disabled]` |  |

---

## 18. 

""

### P0  

```text
 cpu / cuda / auto
 gpu 

 CxInferenceResult
schema
model_hash
metrics
artifact_refs
mask dimensions
foreground_ratio
class_name
```



### P1  T5 / T6 

#### T5

 image manifest cpp_state_dict

Script  Runtime  Forward  Mask  Contour  Shape  Evidence

 Semantic Quality Case

#### T6

weights / class compatibility

Forward  Detection  Rect  Overlay  Evidence

### P2  Evidence 

 Case Manual / Headless / Evidence Chain / Suite

ScriptGlobal ParamsResult CountMain GeometryResult RefEvidence Ref 

### P3  Pattern / mlpack 

 Pattern descriptor 

Grid / Region Descriptor  mlpack classification  semantic result  Evidence

 Pattern Runtime

mlpack ELPV 24 Cases  Evidence Landing HeadlessCompile  Manual Review 

---

## 19. Architecture Rules v2.4

### 19.1 

```text
�个算法
一个模型
一个正式执行核心
```

Manual、Headless、Suite、Evidence 只是不同入口。

### 19.2 CxScript 是逻辑层

CxScript 负责：对象、参数、调用顺序、条件、结果引用

不负责复制：FindCircle 实现、YOLO 后处理、DeepLab 推理、Geometry Fit

### 19.3 Result 单一来源

```text
传统算法
→ CxExecutionResult

模型
→ CxInferenceResult
→ CxExecutionResult.inference_result
```

Shape 和 Evidence 都只能从正式 Result 投影。

### 19.4 Geometry 是可观察结果

```text
Line
Circle
Ellipse
Rect
Mask
Contour
Grid Cell
Region Block
```

应该成为：可显示、可保存、可比较、可回放的稳定对象。

### 19.5 Evidence 不等于 Accuracy

严格区分：

```text
Runtime Success
Artifact Success
Geometry Success
Evidence Success
Model Quality Success
```

五者不能互相替代。

### 19.6 TestHost 回归到测试职责

当前 Dispatcher 已经做到：

```text
Production Task
→ Production Executor

Legacy Task
→ TorchTestHost
```

应继续保持，不允许正式 T5/T6 再回到 TestHost。

### 19.7 状态标签原则

1. **禁止过度标记**：不能把脚本文件存在写成原生模型已完成
2. **禁止虚假验证**：不能把 UI 图表存在写成参数回归已闭环
3. **禁止提前标记**：不能把已实现写成已经通过固定 Case 验证

### 19.8 统一执行原则

1. **单一算法入口**：所有脚本路径（Manual、Headless、Suite、Torch）必须使用同一套算法执行入口
2. **统一结果格式**：所有执行路径必须生成统一的 `RuntimeResult` 格式
3. **全局注入一致**：`ParserDebugBridge`、`HeadlessRunner`、`SuiteRunner` 必须共享一致的 global 注入语义

### 19.9 当前总体结论

v2.3 时项目的核心判断还是：框架已经形成、真实数据仍需贯通。

到了当前 v2.4，状态已经进一步变化为：

```text
CxScript Runtime          已形成
Headless Runtime          已形成
Evidence Runtime          已形成
CxCore Boundary           已进入构建
Pattern Descriptor        已进入工具链
Torch Production Routing  已接通
Segmentation Executor     已接通
Detection Executor        已接通
Result Adapter            已接入真实 Mask/Detection
Geometry Projection       已接入真实 Rect/Contour
```

当前主要问题已经不再是"有没有框架"，而是：

```text
接口和结果字段的一致性
固定 Case 验证
模型兼容
CUDA 设备合同
跨入口结果一致性
模型质量与 Runtime 验收分离
Pattern/mlpack 的最后语义绑定
```

因此开发策略应从"继续搭框架"正式切换到"冻结框架、减少新增层、用真实 Case 疏通和验证现有链条"。

---

## 21. Interaction / Annotation

### 21.1 ImageAnnotationLayer

#### 定位
图像注释层，管理 ShapeElements 和 OverlayElements，提供统一的 HitTest、Drag、CommitEdit 接口，以及 Runtime Projection 和 Runtime Writeback。

#### 核心文件
- [ImageAnnotationLayer.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/ImageAnnotationLayer.h)
- [ImageAnnotationLayer.cpp](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/ImageAnnotationLayer.cpp)

#### 数据结构
- `CxShapeElement`：形状元素（含 stable_ref、owner、semantic_role）
- `OverlayElement`：覆盖层元素
- `CxShapeHitResult`：命中测试结果
- `CxShapeCommitResult`：提交结果

#### 交互流程
1. HitTest → BeginDrag → UpdateDrag → CommitEdit / CancelDrag
2. Runtime Object Publish → RefreshRuntimeObjectTable → SyncRuntimeObjectsToShapeElements
3. Annotation Tool Create → Draft → Commit → UpsertShape

#### 状态
- **[Implemented]**：ShapeElements 管理、HitTest、Drag、CommitEdit、Runtime 投影
- **[Partial]**：Commit → Runtime Object 即时写回

### 21.2 CxAnnotationToolRuntime

#### 定位
注释工具运行时，管理 Point/Line/Rect/Circle/Ellipse/Polyline 工具的输入处理和状态。

#### 核心文件
- [CxAnnotationToolRuntime.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxAnnotationToolRuntime.h)
- [CxAnnotationToolRuntime.cpp](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxAnnotationToolRuntime.cpp)

#### 状态
- **[Implemented]**：Point/Line/Rect/Circle/Ellipse/Polyline 创建、最小尺寸保护、ESC 取消

### 21.3 Shape Elements

#### LineGaugeShape
直线 Gauge 形状，支持 P0/P1/Center/Width 四个拖动控制点。

#### CircleGaugeShape
圆形 Gauge 形状，支持 Center 和 Radius 控制点。

#### RectShape
矩形形状，支持 Corner 和 Center 控制点。

#### EllipseShape
椭圆形状，支持 Center、Rx、Ry 和 Angle 控制点。

#### PolylineShape
折线形状，支持顶点拖动和新增。

#### 状态
- **[Implemented]**：Shape 拖动、stale 标记
- **[Verification Pending]**：重新运行后 Overlay 一致性

### 21.4 Runtime Projection

#### 定位
将 Runtime 对象的几何信息投影到 ShapeElements，实现脚本执行结果的可视化。

#### 核心文件
- [CxRuntimeProjectionExecutor.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxRuntimeProjectionExecutor.h)
- [CxRuntimeProjectionExecutor.cpp](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxRuntimeProjectionExecutor.cpp)

#### 状态
- **[Implemented]**：基础 Runtime 对象投影

### 21.5 Runtime Writeback

#### 定位
将用户编辑的 Shape 几何写回 Runtime 对象和参数，实现交互闭环。

#### 状态
- **[Partial]**：Shape Commit → ManualGaugeState 同步
- **[Partial]**：ManualGaugeState → Globals 同步
- **[Partial]**：Globals → Runtime Tool Object 同步
- **[Verification Pending]**：Result Overlay 一致性

---

## 22. Manual Console Controllers

### 22.1 ManualConsoleGauge

#### 定位
Gauge 控制器，负责 Gauge 几何合法性验证、审核状态检查、参数注入和持久化。

#### 核心文件
- [ManualConsoleGauge.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/ManualConsoleGauge.h)
- [ManualConsoleGauge.cpp](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/ManualConsoleGauge.cpp)

#### 已实现能力
- Gauge 几何合法性验证
- `accepted / manual_accepted / dirty` 审核状态检查
- FindLine 和 FindCircle 参数向脚本 globals 注入
- `gauge_annotation.json` 保存与加载
- `gauge_manifest_candidate.cxsc` 导出
- 参数回归前置条件检查
- `ManualGaugeAcceptedForParamRegression()`  `false`

#### 
- **[Implemented]**Gauge  Globals /manifest candidateaccepted gate
- **[Implemented / Verification Pending]**

### 22.2 ManualConsoleParamRegressionPanel

#### 
Probe Runner  UI 

#### 
- [ManualConsoleParamRegressionPanel.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/ManualConsoleParamRegressionPanel.h)
- [ManualConsoleParamRegressionPanel.cpp](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/ManualConsoleParamRegressionPanel.cpp)

#### 
-  Gauge 
-  UI
- 
- manual seed
- 
- 
- 
-  Checklist
- Workbench 
-  UI 

#### 
- **[Implemented Phase 1]**
- **[Partial]**UI  Probe 
- **[Placeholder]** Hit Distributionmlpack Rankensmallen Optimize

### 22.3 ManualConsoleEvidenceChain

#### 


#### 
- [ManualConsoleEvidenceChain.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/ManualConsoleEvidenceChain.h)
- [ManualConsoleEvidenceChain.cpp](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/ManualConsoleEvidenceChain.cpp)

#### 
- **[Implemented]**

### 22.4 ManualConsoleScriptDebugPanel

#### 


#### 
- **[Implemented]**

### 22.5 ManualConsoleFindLineDebug

#### 
FindLine 

#### 
- **[Implemented]**

### 22.6 ManualConsoleFindCircleDebug

#### 
FindCircle 

#### 
- **[Implemented]**

### 22.7 ManualConsoleRuntimeView

#### 


#### 
- [ManualConsoleRuntimeView.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/ManualConsoleRuntimeView.h)

#### 
- **[Implemented]**

### 22.8 ManualConsoleCxScriptDebug

#### 
CxScript 

#### 
- **[Implemented]**

---

## 23. Unified Execution / Orchestration

### 23.1 ParserDebugBridge

#### 
 CxScript 

#### 
- [ParserDebugBridge.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/ParserDebugBridge.h)
- [ParserDebugBridge.cpp](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/ParserDebugBridge.cpp)

#### 
- `ParserDebugObjectSnapshot`
- `CxScriptLineView`
- `CxScriptStatementView`
- `CxScriptSemanticBridgeResult`

#### 
- **[Implemented]**/

### 23.2 CxParserRuntimeOwner

#### 


#### 
- [CxParserRuntimeOwner.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxParserRuntimeOwner.h)

#### 
- **[Implemented]**

### 23.3 CxScriptHeadlessRunner

#### 
 Headless  `true`  Scaffold Artifact

#### 
- [CxScriptHeadlessRunner.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxScriptHeadlessRunner.h)
- [CxScriptHeadlessRunner.cpp](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxScriptHeadlessRunner.cpp)

#### 

- executed / runtime_ok
- snapshot / summary
- evidence overlay / result overlay
- tool display
- assets_complete  result.ok
- /

#### 
- **[Implemented/Partial]**Basic Sequential Headless Execution

#### 
 `ParserDebugBridge``CxScriptSuiteRunner`  global 

### 23.4 CxScriptHeadlessBindings

#### 
Headless  Headless Runner 

#### 
- [CxScriptHeadlessBindings.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxScriptHeadlessBindings.h)
- [CxScriptHeadlessBindings.cpp](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxScriptHeadlessBindings.cpp)

#### 
- **[Implemented]**

### 23.5 CxScriptRuntimeCaptureSmoke

#### 
Runtime Capture Smoke  Parser  Shape 

#### 
- [CxScriptRuntimeCaptureSmoke.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxScriptRuntimeCaptureSmoke.h)
- [CxScriptRuntimeCaptureSmoke.cpp](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxScriptRuntimeCaptureSmoke.cpp)

#### 
- **[Implemented]** Smoke 

### 23.6 CxShapeInteractionRunner

#### 
Shape  Shape 

#### 
- [CxShapeInteractionRunner.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxShapeInteractionRunner.h)
- [CxShapeInteractionRunner.cpp](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxShapeInteractionRunner.cpp)

#### 
- **[Implemented]**

### 23.7 CxShapeInteractionTest

#### 
Shape 

#### 
- [CxShapeInteractionTest.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxShapeInteractionTest.h)
- [CxShapeInteractionTest.cpp](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxShapeInteractionTest.cpp)

#### 
- **[Implemented]**

### 23.8 CxManifestProjectionRequestResolver

#### 
Manifest  manifest 

#### 
- [CxManifestProjectionRequestResolver.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxManifestProjectionRequestResolver.h)
- [CxManifestProjectionRequestResolver.cpp](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxManifestProjectionRequestResolver.cpp)

#### 
- **[Implemented]**

### 23.9 CxScriptSuiteRunner

#### 
Suite  5 

#### 
- [CxScriptSuiteRunner.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxScriptSuiteRunner.h)
- [CxScriptSuiteRunner.cpp](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxScriptSuiteRunner.cpp)

#### 
1. Dry-run
2. ROI Preview
3. Headless Only
4. Contract
5. Promotion

#### 
- **[Implemented]**Dry-runHeadlessContract

### 23.10 CxParamProbeRunner

#### 
 Headless  `probe_ok` 

#### 
- [CxParamProbeRunner.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxParamProbeRunner.h)
- [CxParamProbeRunner.cpp](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxParamProbeRunner.cpp)

#### 
 `headless_result` 
- launched / executed / runtime_ok
- assets_complete / timeout / exit_code
- snapshot / summary / overlay / tool display
- valid points
- fit line / fit circle
- average distance
- support score
- failure stage

 `probe_ok` ""

#### 
- **[Implemented]**Adapter 
- **[Integration Pending]**Parameter Regression Panel 

### 23.11 CxScriptCasePackageWriter

#### 
Case 

#### 
- [CxScriptCasePackageWriter.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxScriptCasePackageWriter.h)

#### 
- **[Implemented]**

---

## 24. OpenCV / OCCT / cxgeom / cxcloud

### 24.1 OpenCV

#### 


#### 
- **[Implemented]** cximage 

### 24.2 OpenCASCADE (OCCT)

#### 


#### 
- TKernel / TKMath / TKG2d / TKG3d
- TKService / TKV3d / TKOpenGl
- TKGeomBase / TKBRep / TKGeomAlgo
- TKTopAlgo / TKPrim / TKBO / TKOffset
- TKXSBase / TKSTEPBase / TKIGES / TKLCAF

#### 
- **[Implemented]** cxgeom  ViewController

### 24.3 cxgeom

#### 
 OpenCASCADE 

#### 
- `CxGeometryItem`
- `CxShapeHandle`OCCT 
- `CxCurveBuilder`
- `CxFaceBuilder`
- `CxGeometryOperations`

#### 
- **[Implemented]**

### 24.4 cxcloud

#### 


#### 
- `CxCloudItem`
- `CxOctreeAdapter`
- `CxNormalEstimator`
- `CxDistanceAnalyzer`

#### 
- **[Implemented]**

---

## 25. Observability / Reliability

### 25.1 CxUnifiedLog

#### 
 JSONL 

#### 
- [CxUnifiedLog.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxUnifiedLog.h)
- [CxUnifiedLog.cpp](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxUnifiedLog.cpp)

#### 
- **[Implemented]**JSONL 

### 25.2 CxCrashLog

#### 
Crash 

#### 
- [CxCrashLog.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxCrashLog.h)

#### 
- **[Implemented]** Crash 

### 25.3 Run Context

#### 


#### 
- **[Implemented]**

---

## Appendix A. Core File Index

### A.1 cximage 

|  |  |
|------|------|
| `GuiMain.cpp` | GUI  |
| `ViewController.h/cpp` |  |
| `ManualStateTestConsole.h/cpp` |  |
| `ManualConsoleGauge.h/cpp` | Gauge  |
| `ManualConsoleParamRegressionPanel.h/cpp` |  |
| `ManualConsoleEvidenceChain.h/cpp` |  |
| `ParserDebugBridge.h/cpp` |  |
| `CxParserRuntimeOwner.h/cpp` |  |
| `CxScriptHeadlessRunner.h/cpp` |  Headless  |
| `CxScriptSuiteRunner.h/cpp` | Suite  |
| `CxParamProbeRunner.h/cpp` |  |
| `TorchRuntimeBridge.h/cpp` | Torch  |
| `TorchRuntimeResultAdapter.h/cpp` | Torch  |
| `CxUnifiedLog.h/cpp` |  |
| `CxCrashLog.h/cpp` | Crash  |

### A.2 cxparser_ext 

|  |  |
|------|------|
| `parser_pipeline.h` |  |
| `parser_runtime_facade.h` |  |
| `cxscript_runtime.h` | CxScript  |
| `parser_binding_builder.h` |  |
| `parser_flow_router.h` |  |
| `parser_validation_engine.h` |  |

---

## Appendix B. CxScript Asset Index

### B.1 cximage 

|  |  |
|------|------|
| Catalog | `cxparser/cxscript/module/cximage/catalog/` |
| Stage25 | `cxparser/cxscript/module/cximage/stage25/` |
| Frozen | `cxparser/cxscript/module/cximage/frozen/` |
| Tests | `cxparser/cxscript/module/cximage/tests/` |
| Frame Probe | `cxparser/cxscript/module/cximage/frame_probe/` |
| Diagnostic | `cxparser/cxscript/module/cximage/diagnostic/` |

### B.2 torch 

|  |  |
|------|------|
| Detection | `cxparser/cxscript/module/torch/detect_direct_test.cxsc` |
| Segmentation | `cxparser/cxscript/module/torch/segmentation_direct_test.cxsc` |

### B.3 mlpack 

|  |  |
|------|------|
| Logistic Regression | `cxparser/cxscript/module/mlpack/logreg_predict_direct_test.cxsc` |
| Handoff | `cxparser/cxscript/module/mlpack/mlpack_logreg_predict_direct_test.cxsc` |

### B.4 ensmallen 

|  |  |
|------|------|
| Geometry Tuning | `cxparser/cxscript/module/ensmallen/ensmallen_geometry_tuning_direct_test.cxsc` |
| Parameter Optimization | `cxparser/cxscript/module/ensmallen/geometry_tuning_direct_test.cxsc` |

---

## Appendix C. Artifact Schemas

### C.1 result_summary.json

```json
{
  "case_id": "...",
  "script_path": "...",
  "executed": true,
  "runtime_ok": true,
  "assets_complete": true,
  "exit_code": 0,
  "timeout": false,
  "valid_points": 10,
  "fit_line": { "slope": 0.5, "intercept": 1.0 },
  "fit_circle": { "cx": 100, "cy": 200, "radius": 50, "mean_distance": 2.3 },
  "support_score": 0.95,
  "failure_stage": ""
}
```

### C.2 gauge_annotation.json

```json
{
  "gauge_type": "line",
  "geometry": { "x0": 100, "y0": 50, "x1": 200, "y1": 150 },
  "parameters": { "threshold": 50, "linegap": 10 },
  "accepted": true,
  "manual_accepted": true,
  "dirty": false
}
```

---

## Appendix D. CLI Options

### D.1 

```
cxvision_imgui_acceptance [options]

Options:
  --script <path>    Load and execute specified CxScript
  --manifest <path>  Load specified image manifest
  --catalog <path>   Load specified script catalog
  --debug            Enable debug mode
  --log <path>       Set unified log output path
```

---

## Appendix E. Placeholder Register

### E.1 

|  |  |  |
|--------|----------|------|
| `AddMlpackRankPlaceholderCandidates()` | `ManualConsoleParamRegressionPanel.cpp` | [Placeholder] |
| `AddEnsmallenOptPlaceholderCandidates()` | `ManualConsoleParamRegressionPanel.cpp` | [Placeholder] |
| Tuning Map Animate | `ManualConsoleParamRegressionPanel.cpp` | [Visual Placeholder] |
| Hit Distribution bins | `CxParamRegressionRuntime.cpp` | [Placeholder] |

### E.2 Placeholder 

|  |  |  |
|--------|-------------|----------|
| mlpack Rank | v2.5 | mlpack  + ELPV Headless  |
| ensmallen Optimize | v2.5 | ensmallen  +  Probe Objective  |
| Hit Distribution | v2.4.1 | FindLine/FindCircle  Case  |
| Tuning Map Animate | v2.4.1 |  Panel Probe  |

---

## Appendix F. Legacy Stage25 C++

 Legacy `CXVISION_ENABLE_LEGACY_STAGE25_CPP=ON` �：

| 文件 | 说明 |
|------|------|
| `CxScriptStage25Manifest.cpp` | Stage25 清单实现 |
| `CxScriptStage25Template.cpp` | Stage25 模板实现 |
| `CxScriptStage25Runner.cpp` | Stage25 运行器 |
| `CxScriptStage25ReportWriter.cpp` | Stage25 报告输出 |
| `CxScriptStage25Register.cpp` | Stage25 注册 |
| `CxScriptStage25JsonLite.cpp` | Stage25 JSON 轻量解析 |
| `CxScriptStage25PolicyValidator.cpp` | Stage25 策略验证 |
| `CxScriptStage25CaseMatrix.cpp` | Stage25 Case 矩阵 |

#### 状态
- **[Legacy/Disabled]**：默认不编译，仅兼容保留

---
<p align="center">
<img src="https://raw.githubusercontent.com/Sean-Cai-X/cxvision/codex/cxcore-integration/diagram2.png" width="100%">
</p>


 FindSegmentation->torch /  FindLine->FindGauge 
 两种工具在深度学习和传统分析上的交汇,
 FindLine和其工具参数链路代表的分析测量的原子语义链路的可解释性,
 而FindSegmentation开始的模型和增量训练带来的边界深度学习构建化,
 这里恰恰是Ensmallen 和Mlpack关键的两个原子路径的开端,
 所以当前的推进进入一个融合语义和特征和参数链路交错的节点,用理解上说,
 深度学习的神经网络的不可理解在分析网络的可理解进行了稀疏化处理,这是系统智能化关键路径 

这里的关键并不是简单地把“传统视觉 + 深度学习”放到同一个框架里，
而是要建立一条能够在两者之间转换、约束和验证的中间语义层。

  可以把当前节点理解为：

  像素 / 图像
     │
     ├─ FindSegmentation → Torch → 特征与边界概率
     │                         │
     │                         ▼
     │                 可学习但弱可解释的表征
     │
     └─ FindLine → FindGauge → 点、边、距离、方向、容差
                               │
                               ▼
                       可解释的测量原子语义

  真正需要融合的是下面这一层：

  模型特征
     ↓ 投影、筛选、约束
  测量原子
     ↓ 组合
  工程语义、Contract、PASS/FAIL

  ### 1. FindLine → FindGauge 是“可解释原子链”

  FindLine 不应只被理解为一个检测算法，它实际上定义了一组稳定、可追踪的分析原子：

  - 输入区域是什么；
  - 扫描方向是什么；
  - 极性、阈值、Gap、采样密度是什么；
  - 得到了哪些测量点；
  - 哪些点被过滤；
  - 使用什么拟合方法；
  - 输出直线的角度、位置、残差和置信度是什么；
  - 结果如何参与距离、角度、平行度等 Gauge 结论。

  因此 FindGauge 不是另一个孤立工具，而应是 FindLine、FindCircle、Shape、几何关系等原子的组合层：

  ROI
  → Scan
  → Edge Samples
  → Filtered Samples
  → Geometric Primitive
  → Gauge Relation
  → Tolerance Decision

  这条链的价值在于每个中间状态都能快照、比较、回放和解释。参数也不是零散 UI 数字，而是这个语义链上各阶段的控制变量。

  ### 2. FindSegmentation → Torch 是“可学习边界构造链”

  FindSegmentation 的意义也不应只停留在输出 mask。它提供的是传统固定算子难以稳定构造的边界先验：

  图像
  → 神经网络特征
  → 类别概率 / Mask
  → 边界概率
  → 连通区域或候选轮廓
  → 工程测量候选

  模型可以解决纹理复杂、对比度不稳定、边缘局部缺失、背景干扰等问题。但模型输出本身不等于最终测量事实。

  例如模型输出一块区域，不代表已经得到了可验收的宽度、圆心、直线或间距。它仍需投影到传统分析原子：

  Segmentation Mask
  → Boundary Candidate
  → FindLine/FindCircle Sampling
  → Robust Fit
  → Gauge Measurement
  → Contract

  这样 Torch 负责“在哪里找”和“哪些像素更可能属于目标”，FindLine/FindGauge 负责“最终测量了什么、为何得到这个数值”。

  ### 3. “不可理解被可理解网络稀疏化”需要进一步精确定义

   这里的“稀疏化”最好不要只理解成数学意义上的稀疏参数或稀疏权重。它更接近三种稀疏化：

  1. 空间稀疏化

     神经网络把整幅图压缩成有限的候选区域、边界或关键点。

  2. 语义稀疏化

     高维特征最终被投影成少量工程原子：线、圆、区域、中心、方向、距离。

  3. 决策稀疏化

     最终 Contract 不直接解释数百万个网络参数，而解释少量可验证事实：

     左边界 = ...
     右边界 = ...
     拟合残差 = ...
     宽度 = ...
     容差范围 = ...

  所以更准确的描述是：

  > 深度学习并未被完整解释，而是通过可解释测量原子形成了一个受约束的语义瓶颈。工程结论只允许从这个瓶颈之后产生。

  这很重要。不能声称 FindLine 解释了神经网络内部为什么作出某个像素分类；它解释的是模型输出如何被转换成最终测量事实。

  ### 4. Ensmallen 与 mlpack 的位置

  这两个路径不宜被放成 Torch 的替代品，而应成为连接模型特征和分析参数的两类基础能力。

  Ensmallen 更适合承担“参数优化原子”：

  固定输入与固定 ROI
  → 参数向量
  → 执行统一测量链
  → 计算可解释目标函数
  → 优化器提出下一组参数

  目标函数不能只写成“是否通过”，而应分解为：

  边界响应强度
  + 有效点覆盖率
  - 拟合残差
  - 参数敏感度
  - ROI 扰动敏感度
  - 超时和失败惩罚

  这样优化结果仍能映射回 threshold、gap、filter、sample interval 等明确参数。

  mlpack 更适合承担“轻量特征学习原子”：

  传统特征 / Torch embedding / 测量摘要
  → 分类、聚类、异常检测或回归
  → 候选排序、失败分类、参数建议

  它可以处在两个位置：

  - Torch 之前或旁边：用低成本模型判断是否需要运行重模型；
  - Torch 与 Gauge 之间：基于模型特征和传统测量特征选择算法分支、参数 Profile 或候选排序。

  因此三者可形成分层关系：

  Torch
  高维视觉表征与复杂分割
          │
          ▼
  mlpack
  特征选择、分类、回归、异常检测
          │
          ▼
  Ensmallen
  可解释参数链上的约束优化
          │
          ▼
  FindLine / FindCircle / FindGauge
  确定性测量与工程事实

  但它们都不能各自建设独立执行链，必须共享统一请求、执行核心、结果和证据结构。

  ### 5. 后续架构重点不是增加工具，而是定义公共中间表示

  [下一步]最有价值的不是马上实现更多模型或优化器，而是先固定一套跨工具的中间结果。

  建议至少包括四层：

  Layer 1：Perception Evidence
  概率图、Mask、Embedding、响应图、候选区域

  Layer 2：Measurement Evidence
  扫描线、采样点、边缘点、过滤点、拟合输入

  Layer 3：Geometric Facts
  Line、Circle、Contour、Center、Angle、Distance、Residual

  Layer 4：Business Facts
  尺寸、容差、缺陷类别、Contract 结论

  每层都要携带：

  - 来源工具和对象引用；
  - 输入图片及 ROI 标识；
  - 参数快照；
  - 坐标系；
  - 置信度或质量指标；
  - 上游证据引用；
  - stale/version 状态；
  - elapsed、timeout、failure stage；
  - 可回放所需信息。

  由此形成真正的证据图：

  模型版本
    → segmentation candidate
    → boundary candidate
    → measurement samples
    → fitted line
    → gauge value
    → contract conclusion

  UI、Headless、Suite 和参数回归只是这张证据图的不同消费者，不再自行解释结果。

  ### 6. 增量训练必须与在线测量链隔离

  FindSegmentation 引入增量训练后，最危险的问题不是训练失败，而是运行语义漂移：

  - 同一个 case 因模型更新得到不同 mask；
  - ROI 和参数未变，但候选边界改变；
  - 人工修正数据被立即用于模型更新；
  - 新模型覆盖旧模型后无法复现历史结果；
  - Contract 的变化被模型版本变化掩盖。

  因此增量训练应当是明确的离线闭环：

  运行证据
  → 人工审核
  → 标注修正
  → 生成候选训练集版本
  → 训练候选模型
  → 固定 L1/L2/L3 回归
  → 人工批准
  → 模型 Promotion

  在线执行只能消费已经批准、不可变的模型版本。一次运行必须冻结：

  - 模型 ID 与 Hash；
  - 数据集版本；
  - 预处理 Profile；
  - 后处理参数；
  - 测量参数；
  - 脚本快照；
  - Contract 版本。

  模型建议和参数优化都不能直接改变当前运行对象，也不能绕过人工选择与 Promotion gate。

  ### 7.  推进顺序

  第一阶段先固定 FindLine → FindGauge 的原子语义链，明确每个阶段的输入、输出、参数和证据。这是整个系统的解释基准。

  第二阶段让 FindSegmentation 只提供候选区域、概率图和边界先验，不直接输出最终业务 PASS/FAIL。

  第三阶段建立从 segmentation evidence 到 measurement evidence 的标准投影，使模型结果能够进入相同的 FindLine/FindCircle/
  Gauge 链。

  第四阶段引入 mlpack，先做失败分类、候选排序和参数建议，不直接改变 Gauge。

  第五阶段引入 Ensmallen，在固定图片、固定 ROI、固定 Contract 和严格预算内优化可解释参数；候选结果仍走 L1 → L2 → L3 → 人
  工审核。

  第六阶段才开展增量训练，并把数据集、模型、参数 Profile、Contract 的版本治理纳入统一证据链。

  最终目标可以概括为：

  > 深度学习负责扩大可感知边界，传统分析负责收敛为可解释测量；mlpack负责学习两者之间的映射，Ensmallen负责在约束内优化显
  > 式参数。所有结果最终必须落到统一的几何事实、证据引用和 Contract 上。

  这也意味着下一阶段最关键的设计对象不是某个新算法类，而是“模型证据如何可靠地成为测量证据”的统一契约。只要这个契约稳定，
  FindSegmentation、Torch、mlpack、Ensmallen 和 FindGauge 才会成为同一条开发链，而不是四套继续扩张的框架。

  
*文档版本: v2.4 | 对应分支: codex/cxcore-integration | 核验日期: 2026-08-16 | 代码基线: 8c77433 | 基于仓库: cxvision_repo*


---

## GUI 快捷键控制与窗口链路验收（2026-08-27）

### Environment

- Repo：`D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo`
- Build Dir：`D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\build01`
- Binary：`D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\build01\Release\cxvision_imgui_acceptance.exe`
- Binary 状态：存在；网关本轮未返回可用修改时间
- Working Directory：Repo 根目录
- GUI Window：`glfw occt image ai`
- PID：`18040`
- HWND：`6882852`
- Window Class：`GLFW30`
- Window Rect：`(-9,-9)–(1929,1029)`
- Worktree：dirty，保留现有用户修改
- 截图证据：`D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxscript_runs\gui_shortcut_validation\run_20260827_135800`

### Compile

- 本轮 GUI 控制分析未重新编译。
- 前序同一目标编译退出码：`0`
- 前序编译结论：`COMPILE_PASS`
- Build Log：`D:\Codex-WorkDir\Sean_WorkDir\codex-lan-agent\logs\build_target_cmake_20260827_134757.log`

### GUI 控制方式

系统界面工具已闭环验证以下能力：

- 按精确标题激活窗口；
- 查询前台窗口、HWND、PID、窗口类和窗口矩形；
- 控制鼠标移动与单击；
- 注入 F1–F12、Tab、Esc 等键盘输入；
- 每次输入后截图；
- 读取右上角 `AI GUI NAVIGATION / Location` 回执；
- 通过画面焦点框判断 ImGui 内部落点。

控制时必须使用精确标题：

```text
glfw occt image ai
```

不得使用 `cxcore` 子串定位。资源管理器窗口标题可能包含该分支名，会造成误激活。

推荐的单次快捷键控制顺序：

```text
1. 精确激活 "glfw occt image ai"
2. 注入一个功能键
3. 等待至少 1 秒，让 ImGui 完成一帧更新
4. 截图
5. 核对 Location、窗口/页签和可视焦点框
```

不能只根据键盘注入命令退出码为 0 判断成功；必须核对 GUI 回执。读取截图或切换到其他工具后，目标 GUI 可能失去前台焦点，因此每个快捷键前都应重新激活目标窗口。

### Shortcut Results

| 快捷键 | 实际落点与 Location 回执 | 窗口/页签 | 焦点 | 结果 |
|---|---|---|---|---|
| F1 | 打开 `AI GUI Shortcut Help` | 帮助窗口正确前置 | 帮助窗口可操作 | Location 仍显示未使用快捷键，缺少 F1 回执 |
| F2 | `Evidence Chain UI -> Image Set tab/list` | Image Set 选中 | 可见焦点框 | 符合 |
| F4 | `Manual Review / Evidence -> Evidence tab/items` | Evidence 选中并显示证据列表 | 页签/列表定位正确 | 符合 |
| F5 | `Manual Review / Evidence -> Review decision controls` | Review 选中，审核按钮可见 | 控件区域可操作 | 符合 |
| F6 | `Image Evidence / Annotation Tools (focus pending)` | Annotation Tools 区域可见 | 未落到工具；按 Tab 跳到左侧 Case 页签 | 不符合焦点要求 |
| F7 | `Key Parameter Controls -> active tool parameter controls` | 参数窗口前置 | 首个布尔控件有焦点框 | 符合 |
| F8 | `Torch Runtime / Evidence -> runtime status and review controls` | Evidence 窗口前置 | 首控件区域可见 | 符合 |
| F9 | `Torch Training Image Set -> dataset actions and image rails` | Training Image Set 前置 | 首按钮有焦点框 | 符合 |
| F10 | `Parameter Tuning Map / Result Conclusion -> first available control` | 参数调优窗口前置 | 首控件有焦点框 | 符合 |
| F11 | `Manual State Test Console -> script editor and debug compiler` | Console 前置 | 编辑框有焦点框 | 符合 |
| F12 | `Analytics Smoke / Metrology Bridge -> analytics controls` | Analytics 窗口前置 | Analytics 折叠栏有焦点框 | 符合 |

F3 不在本轮用户指定复核范围内。

### Mouse Control

鼠标链路已验证：移动到 F1 帮助窗口右上角关闭按钮并单击后，帮助窗口正常关闭。控制过程中没有触发运行、保存、接受、拒绝或参数写回。

### Focus Semantics

Windows 系统焦点查询确认前台窗口与 focused control 均为：

```text
title = glfw occt image ai
class = GLFW30
pid   = 18040
hwnd  = 6882852
```

ImGui 内部控件不是原生 Windows 子控件，因此系统工具只能识别顶层 GLFW 窗口。内部焦点必须通过以下两项共同判断：

1. 右上角 `Location` 语义回执；
2. 截图中的 ImGui 可视焦点框。

### Confirmed Gaps

1. F1 能打开帮助窗口，但没有把 Location 更新为 F1 帮助语义。
2. F6 回执明确为 `focus pending`。
3. F6 后按 Tab，焦点跳到 Evidence Chain 的 Case 页签，没有进入 Annotation Tools 的第一个可操作控件。
4. F6 需要补齐明确的窗口前置、滚动位置和首个可操作控件焦点。

### Human Review

- Required：是
- Performed：仅由人工启动程序；其余步骤由系统 GUI 控制工具执行
- Decision：`MANUAL_GUI_PARTIAL`
- 未执行任何人工审核决定或算法运行

### Final Conclusion

- Code：`PARTIAL`
- 已闭环：窗口激活、鼠标移动/单击、键盘注入、截图、图像判读和顶层焦点查询。
- Remaining blockers：F1 Location 回执缺失；F6 控件焦点链缺失。
- Manual GUI acceptance：`PENDING_HUMAN_REVIEW`
- Overall：尚未达到最终验收条件。

##### 2026-08-27 16:41 Gauge Line 真实曲线补充修正

针对人工截图 `D:\Screenshot 2026-08-27 161957.png` 中 Find Peaks、Curve Fitting、Critical Dimension 区域没有曲线的问题，定位到预览曲线仍依赖已运行的 `height_peak_analysis_ready` 分布数据；当用户只打开参数页、尚未点击 `Analyze Current Surface` 时，界面没有可绘制 source。此前为避免假曲线已经移除公式 fallback，因此暴露为“无曲线”。

本轮修正为：三个预览图优先从当前图像和当前 Gauge 几何按 `Gauge Line NUM` 实时采样真实 profile。FindCircle 按圆心到轮廓方向选择对应扫描线并沿半径方向采样；FindLine 按线方向和法向偏移选择对应扫描线并沿线采样。source ref 使用 `runtime:gauge_line:FindCircle:num=n/total` 或 `runtime:gauge_line:FindLine:num=n/total`，并附带当前 image path。只有在实时 profile 不可用且已有真实高度分布分析结果时，才回退绘制分析后的 ADF/BCDF；不再绘制公式生成曲线。

当前保持的边界：Curve Fitting 的拟合曲线、Critical Dimension 的模型/结果曲线仍标记 `PENDING_BINDING`；Find Peaks 的峰标记只来自真实 `Analyze Current Surface` 后的峰值结果。界面出现 profile 曲线不等于算法精度验收通过。

本轮构建：`cmake --build D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\build01 --config Release --target cxvision_imgui_acceptance`，退出码 0，构建日志 `D:\Codex-WorkDir\Sean_WorkDir\codex-lan-agent\logs\build_target_cmake_20260827_164054.log`，结论 `COMPILE_PASS`。

网关截图记录：`D:\Codex-WorkDir\Sean_WorkDir\codex-lan-agent\logs\cxvision_ui_bridge_20260827_164147.log` 生成截图 `D:/Codex-WorkDir/Sean_WorkDir/codex-lan-agent/cxvision_ui_runs/20260827_084147_812.png`。`ui_screenshot_analyze` 当前存在工具参数不一致问题：无参数调用报 `cxvision exe path is required`，传入 `--exe` 又报 `Unknown option 'exe'`；该工具问题已记录，不作为算法或 UI 结果 PASS 依据。

```text
Metrology live Gauge Line profile preview  IMPLEMENTED
Release target compile                     COMPILE_PASS
GUI screenshot capture                     CAPTURED
GUI screenshot semantic analysis           TOOL_BLOCKED
Find Peaks runtime peak markers            PENDING_HUMAN_REVIEW
Curve Fitting runtime fit curve            PENDING_BINDING
Critical Dimension runtime result curve    PENDING_BINDING
Final acceptance                           NOT_ACCEPTED
```
