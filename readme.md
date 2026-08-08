# CxVision Code Wiki v2.4

<p align="center">
<img src="https://raw.githubusercontent.com/Sean-Cai-X/cxvision/codex/cxcore-integration/diagram.png" width="100%">
</p>

> **文档版本**: v2.4
> **对应分支**: `codex/cxcore-integration`
> **核验日期**: 2026-08-07
> **代码基线**: 最新公开提交 `8c77433` — `add cxscript modules and evidence chain`
> **文档标题**: CxScript Module、Evidence Chain、CxCore Pattern 与 Torch Production Runtime Integration Baseline
> **替代版本**: v2.3 / 2026-07-26

## 目录

0. [文档状态规则](#0-文档状态规则)
1. [当前项目阶段](#1-当前项目阶段)
2. [v2.3 → v2.4 的关键变化](#2-v23--v24-的关键变化)
3. [完整系统架构（8 个逻辑域）](#3-完整系统架构8-个逻辑域)
4. [五条标准执行链](#4-五条标准执行链)
5. [Application / Workbench](#5-application--workbench)
6. [CxScript Runtime & Asset System](#6-cxscript-runtime--asset-system)
7. [Vision / CxCore Runtime](#7-vision--cxcore-runtime)
8. [Model & Optimization Runtime](#8-model--optimization-runtime)
9. [mlpack / ensmallen](#9-mlpack--ensmallen)
10. [Canonical Result](#10-canonical-result)
11. [Result Adapter 当前剩余缺口](#11-result-adapter-当前剩余缺口)
12. [Geometry Projection](#12-geometry-projection)
13. [Runtime Result Capture](#13-runtime-result-capture)
14. [Evidence / Review](#14-evidence--review)
15. [Build / Runtime](#15-build--runtime)
16. [Test Taxonomy / Acceptance](#16-test-taxonomy--acceptance)
17. [Current Status Matrix — v2.4](#17-current-status-matrix--v24)
18. [当前真正需要推进的事项](#18-当前真正需要推进的事项)
19. [Architecture Rules v2.4](#19-architecture-rules-v24)
20. [Interaction / Annotation](#20-interaction--annotation)
21. [Manual Console Controllers](#21-manual-console-controllers)
22. [Unified Execution / Orchestration](#22-unified-execution--orchestration)
23. [OpenCV / OCCT / cxgeom / cxcloud](#23-opencv--occt--cxgeom--cxcloud)
24. [Observability / Reliability](#24-observability--reliability)

A. [Core File Index](#appendix-a-core-file-index)
B. [CxScript Asset Index](#appendix-b-cxscript-asset-index)
C. [Artifact Schemas](#appendix-c-artifact-schemas)
D. [CLI Options](#appendix-d-cli-options)
E. [Placeholder Register](#appendix-e-placeholder-register)
F. [Legacy Stage25 C++](#appendix-f-legacy-stage25-c)

---

## 0. 文档状态规则

继续沿用 v2.3 的状态定义：

| 状态 | 定义 |
|------|------|
| **[Verified]** | 正式构建 + 真实数据 + 固定 Case 回归通过 |
| **[Module Verified]** | 独立模块测试完成，但完整系统链尚未验收 |
| **[Implemented]** | 已实现并进入正式构建 |
| **[Partial]** | 主链存在，但仍有明确结果或验证缺口 |
| **[Contract]** | 接口、协议或数据结构已经定义 |
| **[Verification Pending]** | 功能已存在，但固定回归或跨入口一致性尚未完成 |
| **[Placeholder]** | 当前仍是模拟、规则或静态引用 |
| **[Legacy]** | 兼容旧路径，不作为新功能主要入口 |
| **[Disabled]** | 代码存在，但默认关闭 |
| **[Planned]** | 尚未进入正式实现 |

完整状态晋级仍遵循：

```text
Source Exists
→ Build Registered
→ Runtime Reachable
→ Real Data Executed
→ Result Projected
→ Evidence Generated
→ Fixed Regression Verified
```

只有最后一步完成才标记 `[Verified]`。

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

CxVision 当前已经不再处于"搭建基本框架"阶段。

截至 2026-08-07，最近一系列提交主要集中在：

```text
Torch Runtime / Torch UI
CxScript Module
Evidence Chain
HD Reference
Manual Evidence Chain
FindLine / FindCircle
GridPattern / RegionPattern
Headless Case
Key Parameter Controls
```

7 月 25 日完成 Torch Runtime Service / Adapter，7 月 29～30 日继续修正 FindLine、Torch UI，8 月 1～7 日工作重心明显转向 Evidence Chain、Pattern Tool、HD Reference 和 CxScript 模块化。

因此当前阶段应重新定义为：

> **核心算法与模型能力基本形成，CxScript 正在成为统一操作入口，当前重点是将算法、Torch、Pattern、Evidence、Manual、Headless 和固定 Case 收敛到同一条可观察、可回放、可验证的运行链。**

---

## 2. v2.3 → v2.4 的关键变化

### 2.1 Torch 已从"外层骨架"进一步推进到生产 Runtime

v2.3 当时仍把 Torch 真实模型主线标记为 `Partial`，并认为 Runtime Core 主要依赖 `TorchTestHost`。当前代码已经变化：

```text
RunTorchTask()
→ DispatchTorchRuntimeTask()
```

Dispatcher 正式区分：

```text
Capabilities
Segmentation Contract
Detection Contract
DeepLabV3Plus Segmentation
YOLOv8 Detection
Segmentation Training Lifecycle
Legacy TestHost Task
```

真实分割进入 `ExecuteTorchSegmentationTask()`，真实检测进入 `ExecuteTorchDetectionTask()`，只有旧测试任务才回退到 `TorchTestHost`。

因此：

```text
Torch 控制链             [Implemented]
Torch Production Routing [Implemented]
Torch Contract            [Implemented]
Segmentation Executor     [Implemented]
Detection Executor        [Implemented]
Legacy TestHost           [Legacy / Diagnostic]
```

不再适合描述为"生产执行器尚未建立"。

### 2.2 `libtorch_module_runtime` 已形成完整生产源集

当前 DLL 构建已经包含：

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

TestHost 仍然存在，但只是生产 Dispatcher 的一个 Legacy 分支。

> 不再创建第二套 Segmentation / Detection Runtime。现有生产 Runtime 就是正式主线。

### 2.3 Torch Result 已真正进入结构化结果链

v2.3 中 `TorchRuntimeResultAdapter` 还主要处理状态和引用。当前代码已经会：

```text
detections.json
→ CxTorchDetection[]

mask_binary.png
contours.json
mask_overlay.png
→ CxTorchMask
```

并最终进入：

```text
CxInferenceResult
→ CxTorchResultProjector
→ Shape Snapshot
→ Runtime Capture
```

Detection 已生成实际 Rect，Segmentation 已能读取 `contours.json` 并把第一条轮廓转换为真实 Polyline 点集。

所以 Torch 几何接入已经从：

```text
Contract / Placeholder
```

推进到：

```text
真实 Result Adapter + 几何投影
```

### 2.4 CxCoreBoundary 已进入正式构建

v2.3 曾标记：

```text
CxCoreBoundary.cpp [Build Pending]
```

这一状态已经失效。当前根 CMake 已明确包含：

```text
CxCoreBoundary.cpp
RegionPatternNet.cpp
GridPatternClassNet.cpp
GridPatternClassTool.cpp
RegionPatternTool.cpp
```

以及 Torch Adapter、Result Projector 等相关源文件。

因此更新为：

```text
CxCoreBoundary          [Implemented]
GridPatternClassNet     [Implemented]
RegionPatternNet        [Implemented]
GridPatternClassTool    [Implemented/Partial]
RegionPatternTool       [Implemented/Partial]
```

### 2.5 CxScript Evidence Chain 已成为真正运行时

现在不是简单读取 TSV 或固定配置。`CxScriptEvidenceChainRuntime` 使用：

```cpp
mu::Parser parser;
parser.UsingClass(true);
RegisterCxScriptEvidenceChainBindings(parser);
parser.SetExpr(script);
parser.Eval();
```

直接执行 `.cxsc` Evidence Chain 定义。

这意味着 `Evidence Chain` 应该正式归入 `CxScript Runtime`，而不再只是 UI 辅助配置。

---

## 3. 完整系统架构（8 个逻辑域）

v2.4 不继续采用十二层甚至更多纵向架构，统一成 **8 个逻辑域**。这些是职责域，不代表八套 DLL，也不要求继续增加中间类。

```text
┌───────────────────────────────────────────────────────┐
│ 1. Application / Workbench                            │
│ Manual Console / Image View / Script / Evidence UI    │
└───────────────────────┬───────────────────────────────┘
                        ▼
┌───────────────────────────────────────────────────────┐
│ 2. CxScript Runtime & Asset System                    │
│ Parser / Catalog / Frozen / Diagnostic                │
│ Evidence Chain / Manifest / Suite / Globals           │
└───────────────────────┬───────────────────────────────┘
                        ▼
┌───────────────────────────────────────────────────────┐
│ 3. Unified Execution                                  │
│ Manual / Headless / Suite / ParserDebug               │
│ Runtime Result Capture                                │
└───────────────────────┬───────────────────────────────┘
                        ▼
           ┌────────────┴────────────┐
           ▼                         ▼
┌──────────────────────┐   ┌───────────────────────────┐
│ 4. Vision / CxCore   │   │ 5. Model / Optimization   │
│ Find* / FastMatch    │   │ libtorch_module           │
│ Grid / RegionPattern │   │ mlpack / ensmallen        │
│ Gauge / Geometry     │   │                           │
└──────────┬───────────┘   └─────────────┬─────────────┘
           └─────────────┬───────────────┘
                         ▼
┌───────────────────────────────────────────────────────┐
│ 6. Canonical Result & Geometry Projection             │
│ CxExecutionResult / CxInferenceResult                 │
│ Shape / Mask / Contour / Rect                         │
└───────────────────────┬───────────────────────────────┘
                        ▼
┌───────────────────────────────────────────────────────┐
│ 7. Evidence / Review / Regression                     │
│ Snapshot / Overlay / Trace / Contract / Replay        │
│ Manual Review / Fixed Case                            │
└───────────────────────┬───────────────────────────────┘
                        ▼
┌───────────────────────────────────────────────────────┐
│ 8. Foundation / Build / Observability                 │
│ OpenCV / OCCT / cxgeom / LibTorch / CMake / Log       │
└───────────────────────────────────────────────────────┘
```

这比 v2.3 的十二个逻辑层更适合当前代码实际形态。

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

## 4. 五条标准执行链

### 4.1 Manual Vision Chain

```text
ImageAnnotationLayer
→ Manual Gauge / Parameter
→ Global Variables
→ CxScript
→ FindLine / FindCircle / FindEllipse / FastMatch...
→ RuntimeResultCapture
→ Shape
→ Evidence
```

人工界面负责：

```text
定义 ROI
定义 Gauge
修改参数
观察结果
审核结果
```

不得复制算法。

### 4.2 Headless Chain

当前 Headless 已进一步增加统一 Global Value Set。`headless_globals.cxsc` 中声明的 `global_*` 参数可被读取、绑定到 Parser，再通过 CLI/Case Override 注入。当前覆盖范围已经包含 ROI、Circle、Ellipse、FindLine、FindCircle、FindRect、FastMatch、预算参数等大量变量。

标准链：

```text
Image
+ Script
+ global values
+ case
        ↓
CxScriptHeadlessRunner
        ↓
同一个 Parser / 同一个 Tool Object
        ↓
RuntimeResultCapture
        ↓
Snapshot / Overlay / Summary / Trace
```

Manual 和 Headless 不允许继续产生两套算法语义。

### 4.3 Evidence Chain

当前 Evidence Chain 正式链为：

```text
Evidence .cxsc
→ CxScriptEvidenceChainRuntime
→ Catalog
→ Image Manifest / HD Reference
→ Case
→ Script Snapshot
→ Locked Parameter
→ Manual / Headless Execution
→ Result Artifact
→ Human Review
```

`.cxsc` 已成为 Evidence Chain 的描述语言，而不是单纯 UI 配置文件。

### 4.4 Torch Chain

当前正式 Torch 链：

```text
CxScript
→ TorchTask
→ CxTorchExecutionAdapter
→ CxTorchRuntimeService
→ TorchRuntimeBridge
→ Runtime C API
→ RunTorchTask
→ DispatchTorchRuntimeTask
       ├─ Contract
       ├─ Segmentation
       ├─ Detection
       ├─ Training Lifecycle
       └─ Legacy Diagnostic
→ Runtime Artifact
→ TorchRuntimeResultAdapter
→ CxInferenceResult
→ CxTorchResultProjector
→ RuntimeResultCapture
→ Overlay / Evidence
```

这就是当前完整主线，不需要再增加额外的 Executor Registry、Model Manager、Projection V2 等中间架构。

### 4.5 CxCore Pattern Chain

新增 Pattern 路径：

```text
Image / ROI
→ GridPatternClassTool
   或 RegionPatternTool
→ CxCore Pattern Net
→ Feature / Descriptor
→ Geometry Overlay
→ 后续 mlpack / model semantic binding
```

GridPattern 当前可以生成：

```text
Grid Feature Map
Hierarchy Descriptor
Active Cells
Orientation Geometry
```

RegionPattern 可以生成：

```text
Region Descriptor
Foreground Ratio
Pooling Blocks
Mean / Std
```

并且都能够发布 ROI、Cell/Block 等真实 Shape。

但两者当前 summary 都明确：

```text
classifier=model_not_bound
```

因此准确状态是：

```text
Feature/Descriptor       [Implemented]
Geometry Projection      [Implemented]
Classifier Binding       [Partial]
End-to-End Classification [Verification Pending]
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

### 8.4 当前 Device 合同存在一个明确问题

外层 `CxTorchTaskSpec` 只接受：`cpu / cuda / auto`

但 Runtime Contract 和 CUDA 环境设置仍接受/判断：`gpu`

而真正 Segmentation / Detection Executor 判断的是：`device == "cuda"`

因此 CPU 链目前不受影响，但 GPU 路线存在合同不一致。

v2.4 明确加入 P0：统一全链 Device 为 `cpu / cuda / auto`，彻底删除 `gpu` 这个内部别名，或者只在一个兼容入口统一转换一次。

#### 状态
- CUDA Device Contract: **[Partial]**（`cuda/gpu` 语义不一致）

---

## 9. mlpack / ensmallen

### 9.1 mlpack

8 月 7 日已经落地 ELPV 本地 Evidence Chain：

```text
24 / 24 images copied
24 / 24 evidence cases landed
24 UI loaded-elements rows
24 classification rows
```

但文档明确记录：

```text
HEADLESS_EXECUTION: NOT_RUN
COMPILE: NOT_RUN
MANUAL_GUI_REVIEW: NOT_RUN
FINAL_ACCEPTANCE: NOT_ACCEPTED
```

#### 状态
- mlpack CxScript Assets: **[Implemented]**
- Local Evidence Landing: **[Implemented]**
- Semantic Refs: **[Implemented/Contract]**
- Headless Runtime Acceptance: **[Verification Pending]**
- Manual Review: **[Verification Pending]**
- Final Acceptance: **[Not Accepted]**

不能因为 Evidence Case 已进入 UI 就把 mlpack 写成 Runtime Verified。

### 9.2 ensmallen

当前仍保持原架构定位：`Candidate / Objective → 参数优化 → Best Parameter`

当前没有证据表明 ensmallen 已完成完整真实 Objective 优化闭环。

#### 状态
- CxScript Semantic Assets: **[Implemented]**
- Real Optimization Loop: **[Partial / Placeholder]**

不得提前提升为 Verified。

---

## 10. Canonical Result

当前统一结果结构已经比较清晰：

```text
CxExecutionResult
 └─ optional<CxInferenceResult>
```

`CxInferenceResult` 已定义：

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

因此后续不需要继续发明新的 `TorchGuiResultV2`、`TorchGeometryResult`、`TorchEvidenceResult`、`ModelResultV3` 作为系统正式结果。

所有模型数据最终应归一到 `CxInferenceResult`。

---

## 11. Result Adapter 当前剩余缺口

当前 Adapter 已经会填：

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

但是以下 `CxInferenceResult` 字段目前仍没有看到实际填充：

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

因此这里才是当前真正值得做的一次"小补齐"。不需要新层，只需要：

```text
Runtime Artifact
→ ResultAdapter
→ 把已有数据完整写入 CxInferenceResult
```

---

## 12. Geometry Projection

### 12.1 Detection

当前已经是真实几何：

```text
CxTorchDetection
→ Rect Shape
→ stable_ref
→ owner_ref
→ model_best_result / model_candidate
```

#### 状态
- Detection Shape Code: **[Implemented]**
- 真实数据接入: **[Implemented]**
- 固定图片坐标一致性: **[Verification Pending]**

### 12.2 Segmentation

当前：

```text
Mask
→ Mask Shape

contours.json
→ Parse First Contour
→ Polyline Shape
```

已经不再是 v2.3 所描述的空 Polyline。

#### 状态
- Mask Semantic Shape: **[Implemented]**
- Contour Geometry: **[Implemented]**
- Mask Image Overlay: **[Implemented]**
- 多轮廓/完整 Mask Geometry: **[Partial]**
- Fixed Coordinate Parity: **[Verification Pending]**

如果后续需要完善，优先考虑：多 contour、hole、component id、mask dimensions，而不是增加新的投影框架。

---

## 13. Runtime Result Capture

`CxScriptRuntimeResultCapture` 当前已经把 TorchTask 纳入与传统工具相同的 Capture 流程。

它会读取：

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

并继续调用 `CxTorchResultProjector::Project()` 生成 Shape Snapshot。

这意味着当前系统已经具备非常重要的一点：

> **Torch 不再是一个平行的特殊 UI 通道，而开始真正进入统一 Runtime Capture。**

这一点作为 v2.4 的核心架构结论。

#### 状态
- Runtime Result Capture: **[Implemented]**（传统工具 + Torch）

---

## 14. Evidence / Review

### 14.1 Torch Evidence Chain

当前已建立固定 Torch UI Evidence Chain，包含：

| Case | Level | 目标 |
|------|-------|------|
| Segmentation cpp_state_dict | T5 | mask/result/evidence/overlay |
| Segmentation Contract | T4 | manifest + contract |
| YOLOv8 Detection | T6 | detection + artifacts |
| Detection Contract | T4 | detection contract |
| Training Lifecycle | T3 | tiny CPU lifecycle |

#### 状态
- Torch Evidence Chain: **[Implemented/Verification Pending]**（固定 Cases 已形成）
- Torch Semantic Accuracy: **[Verification Pending]**（不等同 Runtime Smoke）

### 14.2 必须保留 Evidence Guardrail

当前文档已经特别强调：

```text
T5 smoke
≠ semantic segmentation accuracy

T6 detection smoke
≠ final detector quality

tiny training lifecycle
≠ model training quality acceptance

UI evidence visible
≠ final model acceptance
```

这应该正式写入 Architecture Rules。否则后续极易再次出现"链路跑通 = 算法质量通过"这种错误结论。

### 14.3 Evidence Chain Runtime

`CxScriptEvidenceChainRuntime` 已成为真正运行时，直接执行 `.cxsc` Evidence Chain 定义。

#### 核心文件
- [CxScriptEvidenceChainRuntime.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxScriptEvidenceChainRuntime.h)

#### 状态
- **[Implemented]**：`.cxsc` Evidence Chain 可执行

### 14.4 其他证据资产

| 资产 | 状态 |
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

## 15. Build / Runtime

### 15.1 当前构建关系

当前根 CMake 已纳入：

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

`libtorch_module_runtime` 又独立纳入：

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

因此当前构建关系已经比较合理：

```text
cxvision_imgui_acceptance
        │
        ▼
cximage Integration
        │
        ▼
libtorch_module_runtime.dll
        │
        ▼
libtorch_module Model Runtime
```

不建议继续增加独立 DLL。

### 15.2 前置要求

- **操作系统**：Windows
- **编译器**：MSVC (Visual Studio)
- **CMake**：3.21 或更高
- **C++ 标准**：C++17 (主程序) / C++14 (cxparser)
- **第三方库**：GLFW 3.3.10、OpenCASCADE 7.7.0、OpenCV、PyTorch (libtorch)

### 15.3 编译选项

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `CXVISION_ENABLE_CXPARSER_EXT_DEBUG_INPROC` | ON | 嵌入 cxparser_ext 调试层 |
| `CXVISION_ENABLE_LEGACY_STAGE25_CPP` | OFF | 构建已弃用的 C++ Stage25 实现 |
| `CXVISION_BUILD_CXPARSER_RETURN_TESTS` | ON | 构建 return 关键字回归测试 |
| `LIBTORCH_ROOT` | - | PyTorch 库路径 |

### 15.4 构建目标

#### 主目标：cxvision_imgui_acceptance

```bash
mkdir build && cd build
cmake .. -DGLFW_ROOT="D:/glfw-3.3.10" ^
         -DGLAD_ROOT="path/to/glad" ^
         -DOCCT_ROOT="D:/OpenCASCADE-7.7.0-vc14-64/opencascade-7.7.0" ^
         -DOpenCV_DIR="D:/opencv/build" ^
         -DLIBTORCH_ROOT="path/to/libtorch"
cmake --build . --config Release
```

### 15.5 运行方式

直接运行 `cxvision_imgui_acceptance.exe`，启动 ManualStateTestConsole 工作台。

---

## 16. Test Taxonomy / Acceptance

### 16.1 测试分级命名规范

为避免与 Stage25 的图像难度等级混淆，测试分级统一命名为：

| 类别 | 等级 | 说明 |
|------|------|------|
| IMG-L0 / IMG-L1 / IMG-L2 / IMG-L3 | 图像难度 | 图像和算法难度等级 |
| UI-L0 / UI-L1 / UI-L2 / UI-L3 | UI 交互 | Shape、Gauge 和 Pointer 交互等级 |
| REG-S0 / REG-S1 / REG-S2 / REG-S3 | 回归测试 | 单 case、三锚点、mini regression、正式 regression |
| T0 - T7 | Torch 测试 | Torch Runtime 专用分级 |

### 16.2 Torch 测试分级（T0-T7）

v2.4 将 Torch 固定为：

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

T5/T6 再分两种结论：

```text
Runtime PASS
Model Quality PASS
```

绝对不要合并。

### 16.3 测试策略

项目采用三锚点人工 Gauge 闭环 + 短时单 case Probe + 异常进入 Replay/Review + mini-regression 的测试策略。

### 16.4 测试资产输出

每个测试 Case 必须生成：
- `snapshot.txt`
- `result_summary.json`
- `result_overlay.png`
- `evidence_overlay.png`
- `tool_display.png`

### 16.5 Contract Pass 规则

- **FindLine**：有效点数 < 2 时失败
- **FindCircle**：有效点数 < 3 时失败

---

## 17. Current Status Matrix — v2.4

| 子系统 | 当前状态 | v2.4 判断 |
| ------ | -------- | --------- |
| Manual Console | `[Implemented]` | 已成为主要人工工作台 |
| ImageAnnotationLayer | `[Implemented]` | Shape/Interaction 主线 |
| Gauge → Globals | `[Implemented]` | 人工参数进入脚本 |
| Headless Global Value Set | `[Implemented]` | 参数覆盖明显增强 |
| CxScript Parser | `[Implemented]` | 统一脚本运行基础 |
| CxScript Evidence Runtime | `[Implemented]` | `.cxsc` Evidence 可执行 |
| Runtime Result Capture | `[Implemented]` | 传统工具 + Torch |
| Evidence UI | `[Implemented/Partial]` | 持续完善 |
| HD Reference | `[Implemented/Partial]` | 已用于 Evidence Chain |
| FindLine | `[Implemented]` | 重点进入固定回归 |
| FindCircle | `[Implemented]` | 调试/过滤信息增强 |
| FindEllipse | `[Implemented]` | 基础链完成 |
| FastMatch | `[Implemented/Partial]` | GridPattern 接入进行中 |
| FindSegmentation | `[Implemented/Verification Pending]` | 多后端质量待验证 |
| CxCoreBoundary | `[Implemented]` | 已进入主构建 |
| GridPatternClassNet | `[Implemented]` | Feature/Hierarchy |
| GridPatternClassTool | `[Implemented/Partial]` | classifier 未绑定 |
| RegionPatternNet | `[Implemented]` | Descriptor |
| RegionPatternTool | `[Implemented/Partial]` | classifier 未绑定 |
| libtorch 模型内部 | `[Module Verified]` | Train/Infer 基础完成 |
| Torch Runtime DLL | `[Implemented]` | 生产源集完整 |
| Torch Production Dispatcher | `[Implemented]` | 已成为 RunTorchTask 主路由 |
| Torch Contract | `[Implemented]` | Seg/Detection |
| Segmentation Executor | `[Implemented]` | 真实 Forward + Artifact |
| Detection Executor | `[Implemented/Partial]` | 执行器完成，模型兼容待闭合 |
| Torch Result Adapter | `[Implemented/Partial]` | Mask/Detection 已接，字段仍缺 |
| Detection Geometry | `[Implemented]` | Rect Shape |
| Segmentation Contour Geometry | `[Implemented/Partial]` | 首轮廓已接 |
| Torch Evidence Chain | `[Implemented/Verification Pending]` | 固定 Cases 已形成 |
| Torch Semantic Accuracy | `[Verification Pending]` | 不等同 Runtime Smoke |
| CUDA Device Contract | `[Partial]` | `cuda/gpu` 语义不一致 |
| mlpack Evidence Landing | `[Implemented]` | 24 ELPV Cases 已落地 |
| mlpack Headless Acceptance | `[Verification Pending]` | 尚未运行 |
| ensmallen Optimize | `[Partial/Placeholder]` | 真实优化闭环未完成 |
| Parameter Regression | `[Partial]` | 框架存在，真实批量闭环继续推进 |
| Promotion Gate | `[Disabled]` | 暂不开放 |

---

## 18. 当前真正需要推进的事项

当前已经没有必要再列十几个"大架构任务"。建议只保留四组。

### P0 — 一致性修复

```text
统一 cpu / cuda / auto
删除 gpu 语义分裂

补齐 CxInferenceResult：
schema
model_hash
metrics
artifact_refs
mask dimensions
foreground_ratio
class_name
```

这是代码一致性工作，不是框架建设。

### P1 — T5 / T6 固定闭环

#### T5

固定：同一个 image、同一个 manifest、同一个 cpp_state_dict

验证：Script → Runtime → Forward → Mask → Contour → Shape → Evidence

然后另设 Semantic Quality Case。

#### T6

先解决当前：weights / class compatibility

再固定：Forward → Detection → Rect → Overlay → Evidence

### P2 — Evidence 多入口一致性

同一 Case 对比：Manual / Headless / Evidence Chain / Suite

至少验证：Script、Global Params、Result Count、Main Geometry、Result Ref、Evidence Ref 一致。

### P3 — Pattern / mlpack 接入

当前 Pattern descriptor 已经存在。

后续顺序应是：Grid / Region Descriptor → mlpack classification → semantic result → Evidence

而不是继续设计第三套 Pattern Runtime。

mlpack ELPV 24 Cases 已有 Evidence Landing，但 Headless、Compile 和 Manual Review 尚未完成，因此从这里继续即可。

---

## 19. Architecture Rules v2.4

### 19.1 单一执行原则

```text
一个算法
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

## 20. Interaction / Annotation

### 20.1 ImageAnnotationLayer

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

### 20.2 CxAnnotationToolRuntime

#### 定位
注释工具运行时，管理 Point/Line/Rect/Circle/Ellipse/Polyline 工具的输入处理和状态。

#### 核心文件
- [CxAnnotationToolRuntime.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxAnnotationToolRuntime.h)
- [CxAnnotationToolRuntime.cpp](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxAnnotationToolRuntime.cpp)

#### 状态
- **[Implemented]**：Point/Line/Rect/Circle/Ellipse/Polyline 创建、最小尺寸保护、ESC 取消

### 20.3 Shape Elements

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

### 20.4 Runtime Projection

#### 定位
将 Runtime 对象的几何信息投影到 ShapeElements，实现脚本执行结果的可视化。

#### 核心文件
- [CxRuntimeProjectionExecutor.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxRuntimeProjectionExecutor.h)
- [CxRuntimeProjectionExecutor.cpp](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxRuntimeProjectionExecutor.cpp)

#### 状态
- **[Implemented]**：基础 Runtime 对象投影

### 20.5 Runtime Writeback

#### 定位
将用户编辑的 Shape 几何写回 Runtime 对象和参数，实现交互闭环。

#### 状态
- **[Partial]**：Shape Commit → ManualGaugeState 同步
- **[Partial]**：ManualGaugeState → Globals 同步
- **[Partial]**：Globals → Runtime Tool Object 同步
- **[Verification Pending]**：Result Overlay 一致性

---

## 21. Manual Console Controllers

### 21.1 ManualConsoleGauge

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
- `ManualGaugeAcceptedForParamRegression()` 不再固定返回 `false`

#### 状态
- **[Implemented]**：Gauge → Globals 注入、保存/加载、manifest candidate、accepted gate
- **[Implemented / Verification Pending]**：控制点实际显示、鼠标拖动、缩放坐标转换、运行后显示一致性

### 21.2 ManualConsoleParamRegressionPanel

#### 定位
参数回归面板控制器，提供参数范围、候选表、Probe Runner 和评估报告的 UI 集成。

#### 核心文件
- [ManualConsoleParamRegressionPanel.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/ManualConsoleParamRegressionPanel.h)
- [ManualConsoleParamRegressionPanel.cpp](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/ManualConsoleParamRegressionPanel.cpp)

#### 已实现能力
- 当前脚本、工具和 Gauge 上下文
- 关键参数 UI
- 参数范围与候选
- manual seed
- 候选表
- 参数整定散点图（可视化占位）
- 候选、评估、准确率和推荐报告的文件清单
- 人工验收 Checklist
- Workbench 总览
- 关键参数、参数回归和结论之间的 UI 映射

#### 状态
- **[Implemented Phase 1]**：参数范围、候选、报告导出
- **[Partial]**：UI 候选批量 Probe 循环
- **[Placeholder]**：真实 Hit Distribution、mlpack Rank、ensmallen Optimize

### 21.3 ManualConsoleEvidenceChain

#### 定位
证据链控制器，管理证据链的加载、浏览和操作。

#### 核心文件
- [ManualConsoleEvidenceChain.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/ManualConsoleEvidenceChain.h)
- [ManualConsoleEvidenceChain.cpp](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/ManualConsoleEvidenceChain.cpp)

#### 状态
- **[Implemented]**：基础证据链加载和查询

### 21.4 ManualConsoleScriptDebugPanel

#### 定位
脚本调试面板，提供脚本编译、执行和调试功能。

#### 状态
- **[Implemented]**：基础脚本调试功能

### 21.5 ManualConsoleFindLineDebug

#### 定位
FindLine 专用调试面板。

#### 状态
- **[Implemented]**：基础调试功能

### 21.6 ManualConsoleFindCircleDebug

#### 定位
FindCircle 专用调试面板。

#### 状态
- **[Implemented]**：基础调试功能

### 21.7 ManualConsoleRuntimeView

#### 定位
运行时视图，展示脚本执行后的对象和变量状态。

#### 核心文件
- [ManualConsoleRuntimeView.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/ManualConsoleRuntimeView.h)

#### 状态
- **[Implemented]**：基础运行时对象展示

### 21.8 ManualConsoleCxScriptDebug

#### 定位
CxScript 专用调试面板。

#### 状态
- **[Implemented]**：基础调试功能

---

## 22. Unified Execution / Orchestration

### 22.1 ParserDebugBridge

#### 定位
脚本调试桥接器，负责 CxScript 的编译、执行、调试和全局输入注入。

#### 核心文件
- [ParserDebugBridge.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/ParserDebugBridge.h)
- [ParserDebugBridge.cpp](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/ParserDebugBridge.cpp)

#### 数据结构
- `ParserDebugObjectSnapshot`：对象快照
- `CxScriptLineView`：脚本行视图
- `CxScriptStatementView`：语句视图
- `CxScriptSemanticBridgeResult`：语义桥结果

#### 状态
- **[Implemented]**：脚本编译/执行、全局输入注入、分步执行、运行时快照

### 22.2 CxParserRuntimeOwner

#### 定位
解析器运行时所有权管理，确保运行时对象的生命周期正确管理。

#### 核心文件
- [CxParserRuntimeOwner.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxParserRuntimeOwner.h)

#### 状态
- **[Implemented]**：运行时所有权管理

### 22.3 CxScriptHeadlessRunner

#### 定位
通用 Headless 运行器，不是返回空 `true` 的 Scaffold，执行结束后检查完整的 Artifact。

#### 核心文件
- [CxScriptHeadlessRunner.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxScriptHeadlessRunner.h)
- [CxScriptHeadlessRunner.cpp](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxScriptHeadlessRunner.cpp)

#### 已实现能力
执行结束后检查：
- executed / runtime_ok
- snapshot / summary
- evidence overlay / result overlay
- tool display
- assets_complete 和最终 result.ok
- 有效点数、直线/圆拟合状态、圆半径和平均距离回填

#### 状态
- **[Implemented/Partial]**：Basic Sequential Headless Execution

#### 待固化问题
与 `ParserDebugBridge`、`CxScriptSuiteRunner` 是否共享完全一致的 global 注入、对象生命周期、算法调用顺序和结果抓取语义。

### 22.4 CxScriptHeadlessBindings

#### 定位
Headless 绑定注册，为 Headless Runner 提供统一的类型和方法绑定。

#### 核心文件
- [CxScriptHeadlessBindings.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxScriptHeadlessBindings.h)
- [CxScriptHeadlessBindings.cpp](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxScriptHeadlessBindings.cpp)

#### 状态
- **[Implemented]**：基础绑定注册

### 22.5 CxScriptRuntimeCaptureSmoke

#### 定位
Runtime Capture Smoke 测试，验证 Parser 执行后对象和几何 Shape 的捕获能力。

#### 核心文件
- [CxScriptRuntimeCaptureSmoke.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxScriptRuntimeCaptureSmoke.h)
- [CxScriptRuntimeCaptureSmoke.cpp](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxScriptRuntimeCaptureSmoke.cpp)

#### 状态
- **[Implemented]**：基础 Smoke 捕获验证

### 22.6 CxShapeInteractionRunner

#### 定位
Shape 交互测试运行器，执行 Shape 几何测试和交互测试套件。

#### 核心文件
- [CxShapeInteractionRunner.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxShapeInteractionRunner.h)
- [CxShapeInteractionRunner.cpp](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxShapeInteractionRunner.cpp)

#### 状态
- **[Implemented]**：基础交互测试执行

### 22.7 CxShapeInteractionTest

#### 定位
Shape 交互测试基类，提供统一的测试断言和验证框架。

#### 核心文件
- [CxShapeInteractionTest.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxShapeInteractionTest.h)
- [CxShapeInteractionTest.cpp](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxShapeInteractionTest.cpp)

#### 状态
- **[Implemented]**：基础测试框架

### 22.8 CxManifestProjectionRequestResolver

#### 定位
Manifest 投影请求解析器，将 manifest 中的目标和测试用例解析为投影请求。

#### 核心文件
- [CxManifestProjectionRequestResolver.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxManifestProjectionRequestResolver.h)
- [CxManifestProjectionRequestResolver.cpp](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxManifestProjectionRequestResolver.cpp)

#### 状态
- **[Implemented]**：基础投影请求解析

### 22.9 CxScriptSuiteRunner

#### 定位
Suite 运行器，支持完整的 5 步测试流程。

#### 核心文件
- [CxScriptSuiteRunner.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxScriptSuiteRunner.h)
- [CxScriptSuiteRunner.cpp](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxScriptSuiteRunner.cpp)

#### 执行流程
1. Dry-run（验证证据链）
2. ROI Preview（可选）
3. Headless Only（执行脚本）
4. Contract（契约判断）
5. Promotion（升级）

#### 状态
- **[Implemented]**：Dry-run、Headless、Contract、基础报告

### 22.10 CxParamProbeRunner

#### 定位
参数探测运行器，正确承接 Headless 结果并以 `probe_ok` 为准。

#### 核心文件
- [CxParamProbeRunner.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxParamProbeRunner.h)
- [CxParamProbeRunner.cpp](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxParamProbeRunner.cpp)

#### 已实现能力
从 `headless_result` 读取：
- launched / executed / runtime_ok
- assets_complete / timeout / exit_code
- snapshot / summary / overlay / tool display
- valid points
- fit line / fit circle
- average distance
- support score
- failure stage

返回值以 `probe_ok` 为准，不再只是"进程启动成功"。

#### 状态
- **[Implemented]**：Adapter 已实现
- **[Integration Pending]**：Parameter Regression Panel 尚未完全连成自动循环

### 22.11 CxScriptCasePackageWriter

#### 定位
Case 包写入器，生成标准测试资产。

#### 核心文件
- [CxScriptCasePackageWriter.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxScriptCasePackageWriter.h)

#### 状态
- **[Implemented]**：基础资产写入

---

## 23. OpenCV / OCCT / cxgeom / cxcloud

### 23.1 OpenCV

#### 定位
图像处理库，提供图像加载、处理、特征检测等能力。

#### 状态
- **[Implemented]**：集成到 cximage 模块

### 23.2 OpenCASCADE (OCCT)

#### 定位
几何建模内核，提供参数化几何构建、布尔运算、渲染等能力。

#### 链接库
- TKernel / TKMath / TKG2d / TKG3d
- TKService / TKV3d / TKOpenGl
- TKGeomBase / TKBRep / TKGeomAlgo
- TKTopAlgo / TKPrim / TKBO / TKOffset
- TKXSBase / TKSTEPBase / TKIGES / TKLCAF

#### 状态
- **[Implemented]**：集成到 cxgeom 和 ViewController

### 23.3 cxgeom

#### 定位
几何建模模块，封装 OpenCASCADE 的几何对象创建、表示和操作。

#### 核心组件
- `CxGeometryItem`：几何项封装
- `CxShapeHandle`：OCCT 形状句柄
- `CxCurveBuilder`：曲线构建器
- `CxFaceBuilder`：曲面构建器
- `CxGeometryOperations`：几何操作门面

#### 状态
- **[Implemented]**：基础几何建模能力

### 23.4 cxcloud

#### 定位
点云处理模块，提供点云数据管理、八叉树索引、法向量估计等能力。

#### 核心组件
- `CxCloudItem`：点云项
- `CxOctreeAdapter`：八叉树索引
- `CxNormalEstimator`：法向量估计
- `CxDistanceAnalyzer`：距离分析

#### 状态
- **[Implemented]**：基础点云处理能力

---

## 24. Observability / Reliability

### 24.1 CxUnifiedLog

#### 定位
统一日志系统，进程级、线程安全、跨进程安全、只追加的 JSONL 文件。

#### 核心文件
- [CxUnifiedLog.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxUnifiedLog.h)
- [CxUnifiedLog.cpp](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxUnifiedLog.cpp)

#### 状态
- **[Implemented]**：基础日志记录、线程安全、JSONL 格式

### 24.2 CxCrashLog

#### 定位
Crash 日志，记录程序崩溃时的状态信息。

#### 核心文件
- [CxCrashLog.h](https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/cximage/CxCrashLog.h)

#### 状态
- **[Implemented]**：基础 Crash 日志记录

### 24.3 Run Context

#### 定位
运行上下文，管理每次脚本执行的环境信息。

#### 状态
- **[Implemented]**：基础运行上下文管理

---

## Appendix A. Core File Index

### A.1 cximage 核心文件

| 文件 | 定位 |
|------|------|
| `GuiMain.cpp` | GUI 入口 |
| `ViewController.h/cpp` | 视图控制器 |
| `ManualStateTestConsole.h/cpp` | 工作台主壳 |
| `ManualConsoleGauge.h/cpp` | Gauge 控制器 |
| `ManualConsoleParamRegressionPanel.h/cpp` | 参数回归面板 |
| `ManualConsoleEvidenceChain.h/cpp` | 证据链控制器 |
| `ParserDebugBridge.h/cpp` | 脚本调试桥接 |
| `CxParserRuntimeOwner.h/cpp` | 解析器运行时所有权 |
| `CxScriptHeadlessRunner.h/cpp` | 通用 Headless 运行器 |
| `CxScriptSuiteRunner.h/cpp` | Suite 运行器 |
| `CxParamProbeRunner.h/cpp` | 参数探测运行器 |
| `TorchRuntimeBridge.h/cpp` | Torch 运行时桥接 |
| `TorchRuntimeResultAdapter.h/cpp` | Torch 结果适配器 |
| `CxUnifiedLog.h/cpp` | 统一日志 |
| `CxCrashLog.h/cpp` | Crash 日志 |

### A.2 cxparser_ext 核心文件

| 文件 | 定位 |
|------|------|
| `parser_pipeline.h` | 执行流水线 |
| `parser_runtime_facade.h` | 运行时门面 |
| `cxscript_runtime.h` | CxScript 运行时 |
| `parser_binding_builder.h` | 绑定构建器 |
| `parser_flow_router.h` | 流程路由器 |
| `parser_validation_engine.h` | 验证引擎 |

---

## Appendix B. CxScript Asset Index

### B.1 cximage 脚本

| 类型 | 路径 |
|------|------|
| Catalog | `cxparser/cxscript/module/cximage/catalog/` |
| Stage25 | `cxparser/cxscript/module/cximage/stage25/` |
| Frozen | `cxparser/cxscript/module/cximage/frozen/` |
| Tests | `cxparser/cxscript/module/cximage/tests/` |
| Frame Probe | `cxparser/cxscript/module/cximage/frame_probe/` |
| Diagnostic | `cxparser/cxscript/module/cximage/diagnostic/` |

### B.2 torch 脚本

| 类型 | 路径 |
|------|------|
| Detection | `cxparser/cxscript/module/torch/detect_direct_test.cxsc` |
| Segmentation | `cxparser/cxscript/module/torch/segmentation_direct_test.cxsc` |

### B.3 mlpack 脚本

| 类型 | 路径 |
|------|------|
| Logistic Regression | `cxparser/cxscript/module/mlpack/logreg_predict_direct_test.cxsc` |
| Handoff | `cxparser/cxscript/module/mlpack/mlpack_logreg_predict_direct_test.cxsc` |

### B.4 ensmallen 脚本

| 类型 | 路径 |
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

### D.1 主程序命令行选项

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

### E.1 当前占位实现清单

| 占位项 | 文件位置 | 状态 |
|--------|----------|------|
| `AddMlpackRankPlaceholderCandidates()` | `ManualConsoleParamRegressionPanel.cpp` | [Placeholder] |
| `AddEnsmallenOptPlaceholderCandidates()` | `ManualConsoleParamRegressionPanel.cpp` | [Placeholder] |
| Tuning Map Animate | `ManualConsoleParamRegressionPanel.cpp` | [Visual Placeholder] |
| Hit Distribution bins | `CxParamRegressionRuntime.cpp` | [Placeholder] |

### E.2 Placeholder 清理计划

| 占位项 | 计划清理版本 | 依赖条件 |
|--------|-------------|----------|
| mlpack Rank | v2.5 | mlpack 原生构建集成 + ELPV Headless 通过 |
| ensmallen Optimize | v2.5 | ensmallen 原生构建集成 + 真实 Probe Objective 闭环 |
| Hit Distribution | v2.4.1 | FindLine/FindCircle 固定 Case 基线完成 |
| Tuning Map Animate | v2.4.1 | 参数回归 Panel Probe 循环贯通 |

---

## Appendix F. Legacy Stage25 C++

以下文件已标记为 Legacy，仅在 `CXVISION_ENABLE_LEGACY_STAGE25_CPP=ON` 时编译：

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
*文档版本: v2.4 | 对应分支: codex/cxcore-integration | 核验日期: 2026-08-07 | 代码基线: 8c77433 | 基于仓库: cxvision_repo*
