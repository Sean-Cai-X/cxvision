# CxVision Code Wiki v2.2

https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/diagram.png


> **文档版本**: v2.2  
> **对应分支**: codex/cxcore-integration  
> **核验日期**: 2026-07-16  
> **文档标题**: Standardized Execution Chain and Integration Baseline

## 目录

0. [文档元信息与状态标签](#0-文档元信息与状态标签)
1. [项目定位和当前阶段](#1-项目定位和当前阶段)
2. [完整系统架构](#2-完整系统架构)
3. [三条标准执行链](#3-三条标准执行链)
4. [Application / UI](#4-application--ui)
5. [Workbench State](#5-workbench-state)
6. [Interaction / Annotation](#6-interaction--annotation)
7. [Manual Console Controllers](#7-manual-console-controllers)
8. [Unified Execution / Orchestration](#8-unified-execution--orchestration)
9. [cxparser / cxparser_ext Runtime](#9-cxparser--cxparser_ext-runtime)
10. [CxScript Asset System](#10-cxscript-asset-system)
11. [Vision Tool Runtime](#11-vision-tool-runtime)
12. [Model & Optimization Capability](#12-model--optimization-capability)
    - 12.1 [torch 主模型](#121-torch-主模型)
    - 12.2 [mlpack 基础模型](#122-mlpack-基础模型)
    - 12.3 [ensmallen 优化层](#123-ensmallen-优化层)
    - 12.4 [参数搜索策略](#124-参数搜索策略)
13. [Canonical Result / Projection / Display](#13-canonical-result--projection--display)
14. [Evidence / Review / Artifact](#14-evidence--review--artifact)
15. [Parameter Regression](#15-parameter-regression)
16. [Fixed Baseline and Regression Policy](#16-fixed-baseline-and-regression-policy)
17. [Observability / Reliability](#17-observability--reliability)
18. [OpenCV / OCCT / cxgeom / cxcloud](#18-opencv--occt--cxgeom--cxcloud)
19. [Build / Runtime](#19-build--runtime)
20. [Test Taxonomy / Acceptance](#20-test-taxonomy--acceptance)
21. [Current Status Matrix](#21-current-status-matrix)
22. [Roadmap](#22-roadmap)
23. [Architecture Rules](#23-architecture-rules)

## Appendix A. Core File Index
## Appendix B. CxScript Asset Index
## Appendix C. Artifact Schemas
## Appendix D. CLI Options
## Appendix E. Placeholder Register
## Appendix F. Legacy Stage25 C++

---

## 0. 文档元信息与状态标签

### 0.1 状态标签定义

| 标签 | 含义 |
|------|------|
| **[Verified]** | 已通过实际操作和固定 Case 回归验证 |
| **[Implemented]** | 核心代码已经实现，但没有覆盖全部入口 |
| **[Partial]** | 部分路径可运行，仍存在明确断点 |
| **[Scaffold]** | 接口存在，核心实现尚未完成 |
| **[Placeholder]** | 使用模拟、规则或静态数据 |
| **[Disabled]** | 代码存在，但默认关闭 |
| **[Legacy]** | 旧路径，仅兼容保留 |
| **[Planned]** | 尚未进入代码 |

### 0.2 当前阶段定位

> **人工 Gauge 数据链和通用 Headless 执行链已经进入实现阶段；参数回归的数据、UI 和执行适配器已形成，但完整候选循环、真实命中分布、mlpack 排序和 ensmallen 优化仍未闭合。Torch 已经是实际运行能力层，不再只是规划项。**

---

## 1. 项目定位和当前阶段

### 1.1 项目简介

CxVision 是一个基于 C++ 的计算机视觉与几何分析平台，集成了图像处理、几何建模、脚本自动化以及交互式调试工作台。项目采用模块化设计，支持通过自定义脚本语言（CxScript）驱动视觉检测与几何测量工作流，并提供完整的证据链、审核门和参数回归体系。

### 1.2 核心能力

- **图像分析**：边缘检测、特征提取、模板匹配、形态学操作
- **几何建模**：基于 OpenCASCADE 的参数化几何构建与测量
- **脚本自动化**：基于 muParser 扩展的 CxScript 领域特定语言
- **GUI 交互**：基于 ImGui + GLFW + OpenCASCADE 的可视化界面
- **证据链管理**：Manifest/Catalog/Suite/Contract 完整证据体系
- **参数回归**：参数范围探索、候选生成、评估记录、准确性统计
- **机器学习集成**：Torch 主模型运行时、mlpack 基础模型、ensmallen 优化层
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

---

## 2. 完整系统架构

### 2.1 十一层架构

```
┌──────────────────────────────────────────────────────────────┐
│ 1. Application / UI                                          │
│ Script Catalog / Semantic Flow Graph / Image View            │
│ Manual State Test Console / Annotation / Evidence UI         │
│ Key Parameter UI / Tuning Map / Conclusion UI                │
└────────────────────────┬─────────────────────────────────────┘
                         │
┌────────────────────────▼─────────────────────────────────────┐
│ 2. Workbench State                                           │
│ ManualTestContext / ManualGaugeState                         │
│ ManualParamRegressionState / RuntimeObjectView               │
└────────────────────────┬─────────────────────────────────────┘
                         │
┌────────────────────────▼─────────────────────────────────────┐
│ 3. Interaction / Annotation                                  │
│ ImageAnnotationLayer / CxAnnotationToolRuntime               │
│ Shape Elements / HitTest / Drag / CommitEdit                 │
│ Runtime Projection / Runtime Writeback                       │
└────────────────────────┬─────────────────────────────────────┘
                         │
┌────────────────────────▼─────────────────────────────────────┐
│ 4. Manual Console Controllers                                │
│ ManualConsoleGauge / Evidence / Param / Debug                │
│ ManualConsoleFindlineDebug / FindcircleDebug / RuntimeView   │
└────────────────────────┬─────────────────────────────────────┘
                         │
┌────────────────────────▼─────────────────────────────────────┐
│ 5. Unified Execution & Orchestration                         │
│ ParserDebugBridge / CxParserRuntimeOwner                     │
│ CxScriptHeadlessRunner / CxScriptSuiteRunner                 │
│ CxParamProbeRunner / RuntimeResultCapture                    │
└────────────────────────┬─────────────────────────────────────┘
                         │
┌────────────────────────▼─────────────────────────────────────┐
│ 6. CxScript Runtime & Asset System                           │
│ cxparser / cxparser_ext                                      │
│ Catalog / Frozen / Diagnostic / Manifest / Suite             │
│ Contract / Parameter Profile / Param Regression              │
└────────────────────────┬─────────────────────────────────────┘
                         │
       ┌─────────────────┴─────────────────┐
       ▼                                   ▼
┌──────────────────────────────┐ ┌──────────────────────────────┐
│ 7. Vision Tool Runtime       │ │ 8. Model & Optimization      │
│ Findline / Findcircle        │ │ torch 主模型                 │
│ Findellipse / FindRect       │ │ mlpack 基础模型              │
│ FindObject / FastMatch       │ │ ensmallen 优化层             │
│ FindSegmentation             │ │ 参数搜索策略                 │
│ CircleRing / Formfit         │ │                              │
└──────────────┬───────────────┘ └──────────────┬───────────────┘
               └───────────────┬────────────────┘
                               ▼
┌──────────────────────────────────────────────────────┐
│ 9. Canonical Result & Projection                        │
│ Runtime Capture / Tool Result / Metrics                 │
│ Shape Overlay / Result Projector / ToolDisplay         │
└────────────────────────┬─────────────────────────────┘
                         │
        ┌────────────────┴────────────────┐
        ▼                                 ▼
┌──────────────────────────────┐ ┌──────────────────────────────┐
│ 10. Evidence / Review        │ │ 11. Parameter Regression     │
│ Snapshot / Summary / Trace   │ │ Range / Candidate / Probe    │
│ Overlay / Replay / Contract  │ │ HitDistribution / Accuracy   │
│ Human Review / Promotion     │ │ mlpack Rank / ensmallen Opt  │
└──────────────────────────────┘ └──────────────────────────────┘
                         │
┌────────────────────────▼─────────────────────────────────────┐
│ 12. Foundation & Observability                              │
│ OpenCV / OCCT / cxgeom / cxcloud                            │
│ UnifiedLog / CrashLog / Run Context                         │
└──────────────────────────────────────────────────────────────┘
```

### 2.2 目录结构

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
│   ├── ManualConsoleFindlineDebug.h/cpp         # Findline 调试
│   ├── ManualConsoleFindcircleDebug.h/cpp       # Findcircle 调试
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
│   ├── FindSegmentation.h/cpp                   # 分割检测算法
│   ├── FindSegmentationOpenCvSmokeBackend.h/cpp # OpenCV 分割后端
│   ├── FindSegmentationEdgeSamBackend.h/cpp     # EdgeSam 分割后端
│   ├── TorchRuntimeBridge.h/cpp                 # Torch 运行时桥接
│   ├── TorchRuntimeResultAdapter.h/cpp          # Torch 结果适配器
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
├── libtorch_module/            # PyTorch 模块
│   ├── TorchRuntimeBridge.cpp
│   ├── TorchRuntimeResultAdapter.cpp
│   └── ...
└── 3D/                         # 三维场景集成模块
    ├── src/
    └── tests/
```

---

## 3. 三条标准执行链

### 3.1 链路 1：脚本载入到算法/模型运行

```text
Catalog / Frozen / Flow Node
→ CxScript
→ cxparser
→ ParserDebugBridge / HeadlessRunner
→ Find* / FastMatch / torch / mlpack / ensmallen
→ Runtime Result
```

**目的**：确保脚本语义和实际算法调用的一致性。

### 3.2 链路 2：界面仿真与参数整定到运行

```text
Annotation / Gauge / Key Parameter UI
→ ManualGaugeState
→ Globals / Candidate
→ 同一个执行核心
→ Find* / Model
```

**约束**：人工调参、Grid Search、mlpack Rank、ensmallen Optimize 的差异只能是"谁生成下一组参数"，不能建立第二套算法执行路径。

### 3.3 链路 3：运行结果到界面和结论

```text
Runtime Result
→ Canonical Result Capture
→ Overlay / ToolDisplay
→ Image View / Conclusion UI
→ Evidence / Review
```

**约束**：Manual、Suite、Headless、Torch 必须共享统一的结果定义，不能各自维护不同结果结构。

### 3.4 证据链（扩展链）

```text
Image + Gauge + Parameter + Script
→ Snapshot
→ Result Summary
→ Result/Evidence Overlay
→ ToolDisplay
→ Trace
→ Replay
→ Contract
→ Human Review
```

### 3.5 参数回归链（扩展链）

```text
Accepted Gauge
→ Parameter Range
→ Candidate
→ Real Probe
→ EvalRecord
→ Hit Distribution
→ Accuracy/Stability
→ mlpack Rank
→ ensmallen Suggest
→ Human Review
→ Mini Regression
→ Promotion
```

---

## 4. Application / UI

### 4.1 ManualStateTestConsole

#### 定位
人工调试工作台主壳，提供证据链浏览、Gauge 编辑、参数调优、审核门和回放功能的统一入口。

#### 核心文件
- [ManualStateTestConsole.h](/cximage/ManualStateTestConsole.h)
- [ManualStateTestConsole.cpp](/cximage/ManualStateTestConsole.cpp)

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

### 4.2 ViewController

#### 定位
顶层场景控制器，继承自 OpenCASCADE 的 `AIS_ViewController`，负责 ImGui 界面与 OCCT 3D 视图的集成、事件路由和渲染编排。

#### 核心文件
- [ViewController.h](/cximage/ViewController.h)
- [ViewController.cpp](/cximage/ViewController.cpp)

#### 状态
- **[Implemented]**：基础事件路由、图像渲染、Annotation 集成

### 4.3 关键参数 UI

#### 定位
关键参数调优界面，提供参数范围、候选生成和整定散点图。

#### 状态
- **[Implemented/Partial]**：参数范围与候选模型已实现；关键参数 UI 部分实现；参数整定散点图为可视化占位

---

## 5. Workbench State

### 5.1 ImageAnnotationLayer

#### 定位
图像注释层，管理 ShapeElements 和 OverlayElements，提供统一的 HitTest、Drag、CommitEdit 接口。

#### 核心文件
- [ImageAnnotationLayer.h](/cximage/ImageAnnotationLayer.h)
- [ImageAnnotationLayer.cpp](/cximage/ImageAnnotationLayer.cpp)

#### 数据结构
- `CxShapeElement`：形状元素（含 stable_ref、owner、semantic_role）
- `OverlayElement`：覆盖层元素
- `CxShapeHitResult`：命中测试结果
- `CxShapeCommitResult`：提交结果

#### 执行流程
1. HitTest → BeginDrag → UpdateDrag → CommitEdit / CancelDrag
2. Runtime Object Publish → RefreshRuntimeObjectTable → SyncRuntimeObjectsToShapeElements
3. Annotation Tool Create → Draft → Commit → UpsertShape

#### 状态
- **[Implemented]**：ShapeElements 管理、HitTest、Drag、CommitEdit、Runtime 投影

### 5.2 CxAnnotationToolRuntime

#### 定位
注释工具运行时，管理 Point/Line/Rect/Circle/Ellipse/Polyline 工具的输入处理和状态。

#### 核心文件
- [CxAnnotationToolRuntime.h](/cximage/CxAnnotationToolRuntime.h)
- [CxAnnotationToolRuntime.cpp](/cximage/CxAnnotationToolRuntime.cpp)

#### 状态
- **[Implemented]**：Point/Line/Rect/Circle/Ellipse/Polyline 创建、最小尺寸保护、ESC 取消

### 5.3 ManualGaugeState

#### 定位
手动 Gauge 状态，存储 Line/Circle/Ring Gauge 的几何参数和交互状态。

#### 核心文件
定义于 [ManualStateTestConsole.h](/cximage/ManualStateTestConsole.h)

#### 数据结构
- `ManualGaugeState`：Gauge 状态（line_x0/y0/x1/y1, circle_cx/cy/radius 等）
- `GaugeHandleType`：Handle 类型（LineP0/LineP1/CircleCenter/CircleRadius 等）
- `LineGaugeGeometry`：直线 Gauge 几何
- `CircleGaugeGeometry`：圆形 Gauge 几何

#### 状态
- **[Implemented]**：Line/Circle/Ring Gauge 编辑、Handle 拖动、参数写回

---

## 6. Interaction / Annotation

### 6.1 ImageAnnotationLayer

#### 定位
图像注释层，管理 ShapeElements 和 OverlayElements，提供统一的 HitTest、Drag、CommitEdit 接口，以及 Runtime Projection 和 Runtime Writeback。

#### 核心文件
- [ImageAnnotationLayer.h](/cximage/ImageAnnotationLayer.h)
- [ImageAnnotationLayer.cpp](/cximage/ImageAnnotationLayer.cpp)

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

### 6.2 CxAnnotationToolRuntime

#### 定位
注释工具运行时，管理 Point/Line/Rect/Circle/Ellipse/Polyline 工具的输入处理和状态。

#### 核心文件
- [CxAnnotationToolRuntime.h](/cximage/CxAnnotationToolRuntime.h)
- [CxAnnotationToolRuntime.cpp](/cximage/CxAnnotationToolRuntime.cpp)

#### 状态
- **[Implemented]**：Point/Line/Rect/Circle/Ellipse/Polyline 创建、最小尺寸保护、ESC 取消

### 6.3 Shape Elements

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

### 6.4 Runtime Projection

#### 定位
将 Runtime 对象的几何信息投影到 ShapeElements，实现脚本执行结果的可视化。

#### 核心文件
- [CxRuntimeProjectionExecutor.h](/cximage/CxRuntimeProjectionExecutor.h)
- [CxRuntimeProjectionExecutor.cpp](/cximage/CxRuntimeProjectionExecutor.cpp)

#### 状态
- **[Implemented]**：基础 Runtime 对象投影

### 6.5 Runtime Writeback

#### 定位
将用户编辑的 Shape 几何写回 Runtime 对象和参数，实现交互闭环。

#### 状态
- **[Partial]**：Shape Commit → ManualGaugeState 同步
- **[Partial]**：ManualGaugeState → Globals 同步
- **[Partial]**：Globals → Runtime Tool Object 同步
- **[Verification Pending]**：Result Overlay 一致性

---

## 7. Manual Console Controllers

### 7.1 ManualConsoleGauge

#### 定位
Gauge 控制器，负责 Gauge 几何合法性验证、审核状态检查、参数注入和持久化。

#### 核心文件
- [ManualConsoleGauge.h](/cximage/ManualConsoleGauge.h)
- [ManualConsoleGauge.cpp](/cximage/ManualConsoleGauge.cpp)

#### 已实现能力
- Gauge 几何合法性验证
- `accepted / manual_accepted / dirty` 审核状态检查
- Findline 和 Findcircle 参数向脚本 globals 注入
- `gauge_annotation.json` 保存与加载
- `gauge_manifest_candidate.cxsc` 导出
- 参数回归前置条件检查
- `ManualGaugeAcceptedForParamRegression()` 不再固定返回 `false`

#### 状态
- **[Implemented]**：Gauge → Globals 注入、保存/加载、manifest candidate、accepted gate
- **[Implemented / Verification Pending]**：控制点实际显示、鼠标拖动、缩放坐标转换、运行后显示一致性

### 7.2 ManualConsoleParamRegressionPanel

#### 定位
参数回归面板控制器，提供参数范围、候选表、Probe Runner 和评估报告的 UI 集成。

#### 核心文件
- [ManualConsoleParamRegressionPanel.h](/cximage/ManualConsoleParamRegressionPanel.h)
- [ManualConsoleParamRegressionPanel.cpp](/cximage/ManualConsoleParamRegressionPanel.cpp)

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

### 7.3 ManualConsoleEvidenceChain

#### 定位
证据链控制器，管理证据链的加载、浏览和操作。

#### 核心文件
- [ManualConsoleEvidenceChain.h](/cximage/ManualConsoleEvidenceChain.h)
- [ManualConsoleEvidenceChain.cpp](/cximage/ManualConsoleEvidenceChain.cpp)

#### 状态
- **[Implemented]**：基础证据链加载和查询

### 7.4 ManualConsoleScriptDebugPanel

#### 定位
脚本调试面板，提供脚本编译、执行和调试功能。

#### 状态
- **[Implemented]**：基础脚本调试功能

### 7.5 ManualConsoleFindlineDebug

#### 定位
Findline 专用调试面板。

#### 状态
- **[Implemented]**：基础调试功能

### 7.6 ManualConsoleFindcircleDebug

#### 定位
Findcircle 专用调试面板。

#### 状态
- **[Implemented]**：基础调试功能

### 7.7 ManualConsoleRuntimeView

#### 定位
运行时视图，展示脚本执行后的对象和变量状态。

#### 核心文件
- [ManualConsoleRuntimeView.h](/cximage/ManualConsoleRuntimeView.h)

#### 状态
- **[Implemented]**：基础运行时对象展示

### 7.8 ManualConsoleCxScriptDebug

#### 定位
CxScript 专用调试面板。

#### 状态
- **[Implemented]**：基础调试功能

---

## 8. Unified Execution / Orchestration

### 8.1 ParserDebugBridge

#### 定位
脚本调试桥接器，负责 CxScript 的编译、执行、调试和全局输入注入。

#### 核心文件
- [ParserDebugBridge.h](/cximage/ParserDebugBridge.h)
- [ParserDebugBridge.cpp](/cximage/ParserDebugBridge.cpp)

#### 数据结构
- `ParserDebugObjectSnapshot`：对象快照
- `CxScriptLineView`：脚本行视图
- `CxScriptStatementView`：语句视图
- `CxScriptSemanticBridgeResult`：语义桥结果

#### 状态
- **[Implemented]**：脚本编译/执行、全局输入注入、分步执行、运行时快照

### 8.2 CxParserRuntimeOwner

#### 定位
解析器运行时所有权管理，确保运行时对象的生命周期正确管理。

#### 核心文件
- [CxParserRuntimeOwner.h](/cximage/CxParserRuntimeOwner.h)

#### 状态
- **[Implemented]**：运行时所有权管理

### 8.3 CxScriptHeadlessRunner

#### 定位
通用 Headless 运行器，不是返回空 `true` 的 Scaffold，执行结束后检查完整的 Artifact。

#### 核心文件
- [CxScriptHeadlessRunner.h](/cximage/CxScriptHeadlessRunner.h)
- [CxScriptHeadlessRunner.cpp](/cximage/CxScriptHeadlessRunner.cpp)

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

### 8.4 CxScriptHeadlessBindings

#### 定位
Headless 绑定注册，为 Headless Runner 提供统一的类型和方法绑定。

#### 核心文件
- [CxScriptHeadlessBindings.h](/cximage/CxScriptHeadlessBindings.h)
- [CxScriptHeadlessBindings.cpp](/cximage/CxScriptHeadlessBindings.cpp)

#### 状态
- **[Implemented]**：基础绑定注册

### 8.5 CxScriptRuntimeCaptureSmoke

#### 定位
Runtime Capture Smoke 测试，验证 Parser 执行后对象和几何 Shape 的捕获能力。

#### 核心文件
- [CxScriptRuntimeCaptureSmoke.h](/cximage/CxScriptRuntimeCaptureSmoke.h)
- [CxScriptRuntimeCaptureSmoke.cpp](/cximage/CxScriptRuntimeCaptureSmoke.cpp)

#### 状态
- **[Implemented]**：基础 Smoke 捕获验证

### 8.6 CxShapeInteractionRunner

#### 定位
Shape 交互测试运行器，执行 Shape 几何测试和交互测试套件。

#### 核心文件
- [CxShapeInteractionRunner.h](/cximage/CxShapeInteractionRunner.h)
- [CxShapeInteractionRunner.cpp](/cximage/CxShapeInteractionRunner.cpp)

#### 状态
- **[Implemented]**：基础交互测试执行

### 8.7 CxShapeInteractionTest

#### 定位
Shape 交互测试基类，提供统一的测试断言和验证框架。

#### 核心文件
- [CxShapeInteractionTest.h](/cximage/CxShapeInteractionTest.h)
- [CxShapeInteractionTest.cpp](/cximage/CxShapeInteractionTest.cpp)

#### 状态
- **[Implemented]**：基础测试框架

### 8.8 CxManifestProjectionRequestResolver

#### 定位
Manifest 投影请求解析器，将 manifest 中的目标和测试用例解析为投影请求。

#### 核心文件
- [CxManifestProjectionRequestResolver.h](/cximage/CxManifestProjectionRequestResolver.h)
- [CxManifestProjectionRequestResolver.cpp](/cximage/CxManifestProjectionRequestResolver.cpp)

#### 状态
- **[Implemented]**：基础投影请求解析

### 8.9 CxScriptSuiteRunner

#### 定位
Suite 运行器，支持完整的 5 步测试流程。

#### 核心文件
- [CxScriptSuiteRunner.h](/cximage/CxScriptSuiteRunner.h)
- [CxScriptSuiteRunner.cpp](/cximage/CxScriptSuiteRunner.cpp)

#### 执行流程
1. Dry-run（验证证据链）
2. ROI Preview（可选）
3. Headless Only（执行脚本）
4. Contract（契约判断）
5. Promotion（升级）

#### 状态
- **[Implemented]**：Dry-run、Headless、Contract、基础报告

### 8.10 CxParamProbeRunner

#### 定位
参数探测运行器，正确承接 Headless 结果并以 `probe_ok` 为准。

#### 核心文件
- [CxParamProbeRunner.h](/cximage/CxParamProbeRunner.h)
- [CxParamProbeRunner.cpp](/cximage/CxParamProbeRunner.cpp)

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

### 8.11 CxScriptCasePackageWriter

#### 定位
Case 包写入器，生成标准测试资产。

#### 核心文件
- [CxScriptCasePackageWriter.h](/cximage/CxScriptCasePackageWriter.h)

#### 状态
- **[Implemented]**：基础资产写入

---

## 9. cxparser / cxparser_ext Runtime

### 9.1 cxparser_ext

#### 定位
构建在 cxparser 核心之上的扩展层，提供完整的脚本执行流水线、类型绑定构建、流程路由、验证引擎、结果交付等企业级功能。

#### 核心文件
- [parser_pipeline.h](/cxparser_ext/pipeline/parser_pipeline.h)
- [parser_runtime_facade.h](/cxparser_ext/pipeline/parser_runtime_facade.h)
- [cxscript_runtime.h](/cxparser_ext/runtime/cxscript_runtime.h)

#### 执行流程
```
PrepareTask → MergeBindingSpec → MergeEvidence → Run → Validate → Deliver
```

#### 状态
- **[Implemented]**：基础流水线、绑定构建、验证引擎

### 9.2 cxparser

#### 定位
基于 muParser 扩展的脚本解析核心引擎，提供表达式求值、变量绑定、类方法调用、字节码执行等能力。

#### 核心文件
- [muParser.h](/cxparser/muParser.h)
- [muParserBase.h](/cxparser/muParserBase.h)
- [muParserBytecode.h](/cxparser/muParserBytecode.h)
- [muParserClass.h](/cxparser/muParserClass.h)

#### 状态
- **[Implemented]**：muParser 核心、类绑定、字节码执行

---

## 10. CxScript Asset System

### 10.1 Asset 目录结构

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

### 10.2 Asset 类型定义

| 类型 | 说明 | 状态 |
|------|------|------|
| Catalog | 脚本目录注册 | [Implemented] |
| Frozen | GUI 可见的冻结脚本 | [Implemented] |
| Diagnostic | 诊断脚本 | [Implemented] |
| Manifest | 图像清单 | [Implemented] |
| Suite | 测试套件 | [Implemented] |
| Contract | 契约定义 | [Implemented] |
| Parameter Profile | 参数配置 | [Implemented] |
| Param Regression | 参数回归脚本 | [Implemented Phase 1] |

---

## 11. Vision Tool Runtime

### 11.1 Findline

#### 定位
直线检测算法，支持亚像素级边缘细化和多种参数配置。

#### 核心文件
- [Findline.h](/cximage/Findline.h)
- [Findline.cpp](/cximage/Findline.cpp)

#### 输入
- ROI 区域（起点、终点、半宽）
- 参数（threshold, linegap, wgap, hgap, filterprofile, method）

#### 输出
- 测量点（w/h 方向）
- 拟合直线
- 支持度/距离统计

#### 状态
- **[Implemented]**：基础直线检测、拟合、参数配置

### 11.2 Findcircle

#### 定位
圆检测算法，支持环形区域扫描和拟合。

#### 核心文件
- [Findcircle.h](/cximage/Findcircle.h)
- [Findcircle.cpp](/cximage/Findcircle.cpp)

#### 状态
- **[Implemented]**：基础圆检测、拟合、超时保护

### 11.3 Findellipse

#### 定位
椭圆检测算法。

#### 状态
- **[Implemented]**：基础椭圆检测

### 11.4 FindRect

#### 定位
矩形检测算法。

#### 状态
- **[Implemented]**：基础矩形检测

### 11.5 FindObject

#### 定位
对象检测算法。

#### 状态
- **[Implemented]**：基础对象检测

### 11.6 FastMatch

#### 定位
快速模板匹配算法。

#### 核心文件
- [FastMatch.h](/cximage/FastMatch.h)

#### 状态
- **[Implemented]**：基础模板匹配

### 11.7 CircleRingGauge

#### 定位
圆环规，验证同心圆、厚度和位置。

#### 状态
- **[Implemented]**：基础圆环测量和判断

### 11.8 FormfitGauge

#### 定位
形位公差规，支持多种拟合方法。

#### 状态
- **[Implemented]**：基础形位公差测量

### 11.9 FindSegmentation

#### 定位
分割检测算法，支持多种后端实现。

#### 核心文件
- [FindSegmentation.h](/cximage/FindSegmentation.h)
- [FindSegmentation.cpp](/cximage/FindSegmentation.cpp)
- [FindSegmentationOpenCvSmokeBackend.h](/cximage/FindSegmentationOpenCvSmokeBackend.h)
- [FindSegmentationOpenCvSmokeBackend.cpp](/cximage/FindSegmentationOpenCvSmokeBackend.cpp)
- [FindSegmentationEdgeSamBackend.h](/cximage/FindSegmentationEdgeSamBackend.h)
- [FindSegmentationEdgeSamBackend.cpp](/cximage/FindSegmentationEdgeSamBackend.cpp)

#### 后端实现
- **OpenCV Smoke Backend**：基于 OpenCV 的基础分割实现
- **EdgeSam Backend**：基于 EdgeSam 的边缘分割实现

#### 状态
- **[Implemented]**：基础分割框架和后端接口
- **[Verification Pending]**：完整分割效果验证

---

## 12. Model & Optimization Capability

### 12.1 torch 主模型

#### 定位
PyTorch 主模型运行时，通过动态 DLL 加载实现 C API 调用。

#### 核心文件
- [TorchRuntimeBridge.h](/cximage/TorchRuntimeBridge.h)
- [TorchRuntimeBridge.cpp](/cximage/TorchRuntimeBridge.cpp)
- [TorchRuntimeResultAdapter.h](/cximage/TorchRuntimeResultAdapter.h)
- [TorchRuntimeResultAdapter.cpp](/cximage/TorchRuntimeResultAdapter.cpp)

#### 已实现能力
- 配置 `LIBTORCH_ROOT`
- 独立 `libtorch_module` 子目录构建
- 动态加载 Runtime DLL
- 解析 C API：
  - `torch_runtime_create`
  - `torch_runtime_destroy`
  - `torch_runtime_run_task`
  - `torch_runtime_free_result`
  - `torch_runtime_version`
- GUI 请求转换为 C API 请求
- 回收训练、推理和结果信息

#### 状态
- **[Implemented/Partial]**：动态 Runtime Bridge、C API Task 调用、结果适配、独立 libtorch_module 构建
- **[Partial]**：统一进入公共 ExecutionResult、统一 EvidencePackage、CxScript → Torch → UI 的正式回归验证

### 12.2 mlpack 基础模型

#### 定位
基于 mlpack 的机器学习模型脚本语义层。

#### 脚本资产
- Logistic Regression 脚本
- Handoff 脚本
- 参数评估脚本

#### 状态
- **[Implemented Assets]**：脚本语义层已存在
- **[Runtime Verification Pending]**：根目录 CMake 中无原生构建项
- **[Placeholder]**：Parameter Regression 的 mlpack Rank

### 12.3 ensmallen 优化层

#### 定位
基于 ensmallen 的参数优化脚本语义层。

#### 脚本资产
- 参数评估脚本
- 参数优化脚本
- 回放脚本
- 稳定性脚本
- baseline/best 对比脚本

#### 状态
- **[Implemented Assets]**：脚本语义层已存在
- **[Runtime Verification Pending]**：根目录 CMake 中无原生构建项
- **[Placeholder]**：Parameter Regression 的 ensmallen Suggest

### 12.4 参数搜索策略

| 策略 | 状态 | 说明 |
|------|------|------|
| 手动种子 | [Implemented] | 人工指定候选参数 |
| Grid Search | [Implemented] | 网格搜索 |
| mlpack Rank | [Placeholder] | 基于历史记录排序 |
| ensmallen Optimize | [Placeholder] | 基于真实 Probe 优化 |

---

## 13. Canonical Result / Projection / Display

### 13.1 RuntimeResultCapture

#### 定位
运行时结果捕获，统一收集脚本执行后的所有结果数据。

#### 状态
- **[Implemented]**：基础结果捕获

### 13.2 ToolResult

#### 定位
工具执行结果，包含测量点、拟合结果和统计指标。

#### 状态
- **[Implemented]**：基础工具结果

### 13.3 ShapeOverlay

#### 定位
形状覆盖层，在图像上渲染检测到的几何形状。

#### 状态
- **[Implemented]**：基础形状渲染

### 13.4 ResultProjector

#### 定位
结果投影器，将工具结果投影到 UI 和证据链。

#### 状态
- **[Implemented]**：基础投影

### 13.5 ToolDisplay

#### 定位
工具显示导出，生成工具执行的可视化输出。

#### 核心文件
- [CxScriptToolDisplayExporter.h](/cximage/CxScriptToolDisplayExporter.h)

#### 状态
- **[Implemented]**：基础工具显示导出

---

## 14. Evidence / Review / Artifact

### 14.1 EvidenceChain

#### 定位
证据链管理，关联 Case、Image、Target、Script、Parameter Profile、Contract。

#### 核心文件
- [CxScriptEvidenceChainRuntime.h](/cximage/CxScriptEvidenceChainRuntime.h)

#### 状态
- **[Implemented]**：基础证据链加载和查询

### 14.2 Snapshot

#### 定位
执行快照，记录脚本执行时的完整状态。

#### 状态
- **[Implemented]**：基础快照生成

### 14.3 Summary

#### 定位
结果摘要，生成结构化的执行结果摘要。

#### 状态
- **[Implemented]**：基础摘要生成

### 14.4 Trace

#### 定位
运行轨迹，记录执行过程中的关键事件。

#### 核心文件
- [CxScriptRunTraceRuntime.h](/cximage/CxScriptRunTraceRuntime.h)

#### 状态
- **[Implemented]**：基础轨迹记录

### 14.5 Overlay

#### 定位
覆盖层图像，包含结果叠加和证据叠加。

#### 状态
- **[Implemented]**：基础覆盖层生成

### 14.6 Replay

#### 定位
回放包，支持完整的执行过程重现。

#### 状态
- **[Implemented]**：基础回放包生成

### 14.7 Contract

#### 定位
契约定义，包含预期结果、最小点数、失败阶段等判断规则。

#### 状态
- **[Implemented]**：基础契约判断

### 14.8 Review Gate

#### 定位
审核门，管理审核阶段和决策。

#### 核心文件
- [CxScriptReviewGateRuntime.h](/cximage/CxScriptReviewGateRuntime.h)

#### 数据结构
- `CxReviewStage`：审核阶段（EvidenceResolved/RoiPreview/HeadlessResult/ToolDisplay/ContractResult/Promotion）
- `CxReviewDecision`：审核决策（Accept/RejectRoi/RejectParameter/DeriveProfile/Stop）

#### 状态
- **[Implemented]**：基础审核状态定义和 JSON 序列化

### 14.9 Human Review

#### 定位
人工审核记录和决策。

#### 状态
- **[Implemented]**：基础人工审核记录

### 13.10 Promotion

#### 定位
参数升级门，决定是否将候选参数升级为正式配置。

#### 状态
- **[Disabled]**：当前 Can Promote=no

---

## 14. Parameter Regression

### 14.1 ParamRange

#### 定位
参数范围定义，支持连续范围和离散值。

#### 数据结构
- `CxParamRange`：参数范围（name, min_value, max_value, step, discrete_values, role）

#### 状态
- **[Implemented]**：基础范围定义

### 14.2 Candidate

#### 定位
候选参数生成，支持手动种子、网格搜索、mlpack 排序、ensmallen 优化。

#### 数据结构
- `CxParamCandidate`：候选参数（source, method, threshold, gap, linegap, wgap, hgap, filterprofile, predicted_quality）

#### 状态
- **[Implemented]**：手动种子、基础网格搜索
- **[Placeholder]**：mlpack_rank、ensmallen_opt

### 14.3 EvalRecord

#### 定位
评估记录，记录候选参数的执行结果。

#### 数据结构
- `CxParamEvalRecord`：评估记录（candidate_id, case_id, executed, timeout, points, support_score, mean_distance）

#### 状态
- **[Implemented]**：基础评估记录

### 14.4 HitDistribution

#### 定位
命中分布统计，分析测量点的分布特征。

#### 状态
- **[Placeholder]**：分布区间和汇总均为占位实现

### 14.5 Accuracy/Stability

#### 定位
准确性和稳定性统计。

#### 数据结构
- `CxParamAccuracyStats`：准确性统计（total_cases, executed_cases, geometry_pass, evidence_pass, human_accept, stability_score, risk_score）

#### 状态
- **[Partial]**：基础统计指标已实现，但跨 Case 汇总不完整

### 14.6 mlpack Rank

#### 定位
基于 mlpack 的参数排序。

#### 状态
- **[Placeholder]**：`AddMlpackRankPlaceholderCandidates()` 为空实现

### 14.7 ensmallen Optimize

#### 定位
基于 ensmallen 的参数优化建议。

#### 状态
- **[Placeholder]**：`AddEnsmallenOptPlaceholderCandidates()` 为空实现

### 14.8 Mini Regression

#### 定位
迷你回归测试，验证参数候选的泛化能力。

#### 状态
- **[Planned]**：尚未进入代码

### 14.9 Promotion

#### 定位
参数升级门，决定是否将候选参数升级为正式配置。

#### 状态
- **[Disabled]**：代码存在，但默认关闭

### 14.10 参数回归状态汇总

| 子系统 | 当前状态 |
|--------|----------|
| 参数范围与候选模型 | [Implemented] |
| 关键参数 UI | [Implemented/Partial] |
| 候选与报告导出 | [Implemented Phase 1] |
| ParamProbe Adapter | [Implemented] |
| UI 候选批量 Probe 循环 | [Partial] |
| 真实 Hit Distribution | [Placeholder] |
| 真实 mlpack Rank | [Placeholder] |
| 真实 ensmallen Optimize | [Placeholder] |
| Mini Regression | [Planned] |
| Promotion | [Disabled] |

---

## 15. Observability / Reliability

### 15.1 CxUnifiedLog

#### 定位
统一日志系统，进程级、线程安全、跨进程安全、只追加的 JSONL 文件。

#### 核心文件
- [CxUnifiedLog.h](/cximage/CxUnifiedLog.h)
- [CxUnifiedLog.cpp](/cximage/CxUnifiedLog.cpp)

#### 状态
- **[Implemented]**：基础日志记录、线程安全、JSONL 格式

### 15.2 CxCrashLog

#### 定位
Crash 日志，记录程序崩溃时的状态信息。

#### 核心文件
- [CxCrashLog.h](/cximage/CxCrashLog.h)

#### 状态
- **[Implemented]**：基础 Crash 日志记录

### 15.3 Run Context

#### 定位
运行上下文，管理每次脚本执行的环境信息。

#### 状态
- **[Implemented]**：基础运行上下文管理

---

## 16. OpenCV / OCCT / cxgeom / cxcloud

### 16.1 OpenCV

#### 定位
图像处理库，提供图像加载、处理、特征检测等能力。

#### 状态
- **[Implemented]**：集成到 cximage 模块

### 16.2 OpenCASCADE (OCCT)

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

### 16.3 cxgeom

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

### 16.4 cxcloud

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

## 17. Build / Runtime

### 17.1 前置要求

- **操作系统**：Windows
- **编译器**：MSVC (Visual Studio)
- **CMake**：3.21 或更高
- **C++ 标准**：C++17 (主程序) / C++14 (cxparser)
- **第三方库**：GLFW 3.3.10、OpenCASCADE 7.7.0、OpenCV、PyTorch (libtorch)

### 17.2 编译选项

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `CXVISION_ENABLE_CXPARSER_EXT_DEBUG_INPROC` | ON | 嵌入 cxparser_ext 调试层 |
| `CXVISION_ENABLE_LEGACY_STAGE25_CPP` | OFF | 构建已弃用的 C++ Stage25 实现 |
| `CXVISION_BUILD_CXPARSER_RETURN_TESTS` | ON | 构建 return 关键字回归测试 |
| `LIBTORCH_ROOT` | - | PyTorch 库路径 |

### 17.3 构建目标

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

### 17.4 运行方式

直接运行 `cxvision_imgui_acceptance.exe`，启动 ManualStateTestConsole 工作台。

---

## 18. Test Taxonomy / Acceptance

### 18.1 测试分级命名规范

为避免与 Stage25 的图像难度等级混淆，测试分级统一命名为：

| 类别 | 等级 | 说明 |
|------|------|------|
| IMG-L0 / IMG-L1 / IMG-L2 / IMG-L3 | 图像难度 | 图像和算法难度等级 |
| UI-L0 / UI-L1 / UI-L2 / UI-L3 | UI 交互 | Shape、Gauge 和 Pointer 交互等级 |
| REG-S0 / REG-S1 / REG-S2 / REG-S3 | 回归测试 | 单 case、三锚点、mini regression、正式 regression |

### 18.2 测试策略

项目采用三锚点人工 Gauge 闭环 + 短时单 case Probe + 异常进入 Replay/Review + mini-regression 的测试策略。

### 18.3 测试流程

```
Load Case → Edit Gauge → Apply To Globals → Run Probe
    │            │              │              │
    ▼            ▼              ▼              ▼
 验证几何    人工审核        参数注入        结果评估
    │                                        │
    ▼                                        ▼
成功 → Short Probe → Contract → Promotion
失败 → Replay/Review → 参数调优 → Mini Regression
```

### 18.4 测试类型

| 类型 | 说明 | 状态 |
|------|------|------|
| IMG-L0 | 烟雾测试 | [Implemented] |
| IMG-L1 | 简单几何测试 | [Implemented] |
| IMG-L2 | 中等难度几何测试 | [Implemented] |
| IMG-L3 | 复杂几何测试 | [Planned] |
| UI-L0 | Shape 几何测试 | [Implemented] |
| UI-L1 | HitTest / Drag 测试 | [Implemented] |
| UI-L2 | GUI Pointer 测试 | [Implemented] |
| UI-L3 | 完整交互测试 | [Planned] |
| REG-S0 | 单 case 回归 | [Implemented] |
| REG-S1 | 三锚点回归 | [Implemented] |
| REG-S2 | Mini Regression | [Planned] |
| REG-S3 | 正式 Regression | [Planned] |

### 18.5 测试资产输出

每个测试 Case 必须生成：
- `snapshot.txt`
- `result_summary.json`
- `result_overlay.png`
- `evidence_overlay.png`
- `tool_display.png`

### 18.6 Contract Pass 规则

- **Findline**：有效点数 < 2 时失败
- **Findcircle**：有效点数 < 3 时失败

---

## 19. Current Status Matrix

| 子系统 | 当前状态 |
|--------|----------|
| Manual Console 模块拆分 | [Implemented] |
| Annotation Tool Runtime | [Implemented/Partial] |
| ManualGaugeState | [Implemented] |
| Gauge → Globals | [Implemented] |
| Gauge Annotation Save/Load | [Implemented] |
| Manifest Candidate | [Implemented] |
| Gauge Accepted Gate | [Implemented] |
| Gauge 实际拖动和显示一致性 | [Implemented / Verification Pending] |
| ParserDebugBridge | [Implemented] |
| SuiteRunner | [Implemented] |
| 通用 Headless Runner | [Implemented/Partial] |
| ParamProbe Adapter | [Implemented] |
| Key Parameter UI | [Implemented/Partial] |
| 参数范围/候选/报告 | [Implemented Phase 1] |
| UI 候选批量 Probe 循环 | [Partial] |
| Hit Distribution | [Placeholder] |
| Accuracy/Stability | [Partial] |
| Torch Runtime Bridge | [Implemented/Partial] |
| Torch 统一结果与证据链 | [Partial] |
| mlpack 脚本资产 | [Implemented Assets] |
| mlpack Param Rank | [Placeholder] |
| ensmallen 脚本资产 | [Implemented Assets] |
| ensmallen Param Optimize | [Placeholder] |
| Mini Regression | [Planned] |
| Profile Promotion | [Disabled] |
| Legacy C++ Stage25 | [Legacy/Disabled] |

---

## 20. Roadmap

### 当前推进顺序

现在不再需要继续新增框架，应该按现有框架完成固化：

| 优先级 | 任务 | 说明 |
|--------|------|------|
| P0 | 三条标准链跨入口一致性 | Manual / Headless / Suite 使用相同输入和结果 |
| P1 | Findline、Findcircle 固定 IMG-L1/L2/L3 基线 | 建立固定难度等级的测试基线 |
| P2 | Parameter Regression Panel | Candidate → ParamProbe → EvalRecord 真正循环 |
| P3 | 真实 Hit Distribution | Findline 沿线分布、Findcircle 角度和半径分布 |
| P4 | Accuracy / Stability 跨 Case 汇总 | 完整的准确性和稳定性统计 |
| P5 | mlpack Rank 接入真实历史记录 | 将 mlpack 排序接入实际评估记录 |
| P6 | ensmallen Objective 接入真实 Probe | 将 ensmallen 优化接入实际探测结果 |
| P7 | Torch Result 接入统一结果和 Evidence | 统一 Torch 结果格式 |
| P8 | Mini Regression 与 Promotion Gate | 完整的参数升级流程 |

---

## 21. Architecture Rules

### 21.1 统一执行原则

1. **单一算法入口**：所有脚本路径（Manual、Headless、Suite、Torch）必须使用同一套算法执行入口
2. **统一结果格式**：所有执行路径必须生成统一的 `RuntimeResult` 格式
3. **全局注入一致**：`ParserDebugBridge`、`HeadlessRunner`、`SuiteRunner` 必须共享一致的 global 注入语义

### 21.2 参数回归原则

1. **候选来源可替换**：人工种子、Grid Search、mlpack Rank、ensmallen Optimize 可以替换，但不能改变算法执行路径
2. **评估记录统一**：所有候选参数的执行结果必须写入统一的 `EvalRecord`
3. **Promotion Gate 独立**：参数升级门必须独立于参数生成和评估逻辑

### 21.3 证据链原则

1. **完整可追溯**：每次执行必须生成完整的证据链（Image + Gauge + Parameter + Script + Result）
2. **可回放**：证据链必须支持完整的执行过程重现
3. **审核门强制**：关键决策点必须经过审核门

### 21.4 状态标签原则

1. **禁止过度标记**：不能把脚本文件存在写成原生模型已完成
2. **禁止虚假验证**：不能把 UI 图表存在写成参数回归已闭环
3. **禁止提前标记**：不能把已实现写成已经通过固定 Case 验证

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
| mlpack Rank | v2.2 | mlpack 原生构建集成 |
| ensmallen Optimize | v2.2 | ensmallen 原生构建集成 |
| Hit Distribution | v2.1.1 | Findline/Findcircle 稳定基线 |
| Tuning Map Animate | v2.1.1 | 参数回归闭环完成 |

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
https://github.com/Sean-Cai-X/cxvision/blob/codex/cxcore-integration/diagram2.png
*文档版本: v2.1 | 对应分支: codex/cxcore-integration | 核验日期: 2026-07-15 | 基于仓库: cxvision_repo*
