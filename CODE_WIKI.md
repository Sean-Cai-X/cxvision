# CxVision Code Wiki v2.0

> **文档版本**: v2.0  
> **对应分支**: main  
> **核验日期**: 2026-07-13  
> **功能状态**: Active Development

## 目录

1. [项目概述](#1-项目概述)
2. [整体架构](#2-整体架构)
3. [Application/UI 层](#3-applicationui-层)
4. [Workbench State 层](#4-workbench-state-层)
5. [Orchestration 层](#5-orchestration-层)
6. [cxparser_ext/cxparser Runtime 层](#6-cxparser_extcxparser-runtime-层)
7. [Vision Algorithms 层](#7-vision-algorithms-层)
8. [Evidence/Review 体系](#8-evidence-review-体系)
9. [Parameter Regression/Optimization](#9-parameter-regressionoptimization)
10. [构建与运行](#10-构建与运行)
11. [脚本系统 (CxScript)](#11-脚本系统-cxscript)
12. [测试体系](#12-测试体系)
13. [Legacy/Appendix](#13-legacyappendix)

---

## 1. 项目概述

### 1.1 项目简介

CxVision 是一个基于 C++ 的计算机视觉与几何分析平台，集成了图像处理、几何建模、脚本自动化以及交互式调试工作台。项目采用模块化设计，支持通过自定义脚本语言（CxScript）驱动视觉检测与几何测量工作流，并提供完整的证据链、审核门和参数回归体系。

### 1.2 核心能力

- **图像分析**：边缘检测、特征提取、模板匹配、形态学操作
- **几何建模**：基于 OpenCASCADE 的参数化几何构建与测量
- **脚本自动化**：基于 muParser 扩展的 CxScript 领域特定语言
- **GUI 交互**：基于 ImGui + GLFW + OpenCASCADE 的可视化界面
- **证据链管理**：Manifest/Catalog/Suite/Contract 完整证据体系
- **参数回归**：参数范围探索、候选生成、评估记录、准确性统计
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
| 参数优化 | mlpack / ensmallen (Placeholder) |

---

## 2. 整体架构

### 2.1 架构分层

```
┌─────────────────────────────────────────────────────────────────┐
│                    Application/UI 层                           │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │         ManualStateTestConsole (人工调试工作台)          │   │
│  │  证据链面板 / Gauge 编辑 / 参数调优 / 审核门 / 回放      │   │
│  └─────────────────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │              ViewController (顶层场景控制)               │   │
│  │  ImGui 集成 / OCCT 视图 / 事件路由 / 渲染编排           │   │
│  └─────────────────────────────────────────────────────────┘   │
└────────────────────────────────────┬────────────────────────────┘
                                     │
┌────────────────────────────────────▼────────────────────────────┐
│                   Workbench State 层                            │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │            ImageAnnotationLayer (注释层)                  │   │
│  │  ShapeElements / OverlayElements / HitTest / Drag       │   │
│  └──────────────────────────────────────────────────────────┘   │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │              CxAnnotationToolRuntime                     │   │
│  │  Point/Line/Rect/Circle/Ellipse/Polyline 工具运行时      │   │
│  └──────────────────────────────────────────────────────────┘   │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │              ManualGaugeState (手动 Gauge 状态)          │   │
│  │  Line/Circle/Ring Gauge 参数 / Handle 交互              │   │
│  └──────────────────────────────────────────────────────────┘   │
└────────────────────────────────────┬────────────────────────────┘
                                     │
┌────────────────────────────────────▼────────────────────────────┐
│                   Orchestration 层                             │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │              ParserDebugBridge (脚本桥接)                 │   │
│  │  CxScript 编译/执行/调试 / 全局输入注入                   │   │
│  └──────────────────────────────────────────────────────────┘   │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │              SuiteRunner (Headless Case 运行)            │   │
│  │  Dry-run / ROI Preview / Headless / Contract / Promotion │   │
│  └──────────────────────────────────────────────────────────┘   │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │              CxUnifiedLog (统一日志)                     │   │
│  │  进程级 JSONL / 线程安全 / 跨进程安全 / 只追加           │   │
│  └──────────────────────────────────────────────────────────┘   │
└────────────────────────────────────┬────────────────────────────┘
                                     │
┌────────────────────────────────────▼────────────────────────────┐
│            cxparser_ext/cxparser Runtime 层                    │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │              cxparser_ext (扩展层)                       │   │
│  │  Pipeline / 绑定构建 / 流程路由 / 验证引擎 / 交付API     │   │
│  └──────────────────────────────────────────────────────────┘   │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │              cxparser (核心引擎)                          │   │
│  │  muParser / 字节码 / 类绑定 / 表达式求值                 │   │
│  └──────────────────────────────────────────────────────────┘   │
└────────────────────────────────────┬────────────────────────────┘
                                     │
┌────────────────────────────────────▼────────────────────────────┐
│                   Vision Algorithms 层                        │
│  ┌───────────┐  ┌───────────┐  ┌───────────┐  ┌───────────┐   │
│  │ Findline  │  │Findcircle │  │Findellipse│  │ FindRect  │   │
│  │ 直线检测  │  │ 圆检测    │  │ 椭圆检测  │  │ 矩形检测  │   │
│  └───────────┘  └───────────┘  └───────────┘  └───────────┘   │
│  ┌───────────┐  ┌───────────┐  ┌───────────┐                 │
│  │ FastMatch │  │CircleRing │  │ Formfit   │                 │
│  │ 模板匹配  │  │ 圆环规    │  │ 形位公差  │                 │
│  └───────────┘  └───────────┘  └───────────┘                 │
└────────────────────────────────────┬────────────────────────────┘
                                     │
┌────────────────────────────────────▼────────────────────────────┐
│                   Evidence/Review 体系                         │
│  ┌───────────┐  ┌───────────┐  ┌───────────┐  ┌───────────┐   │
│  │ Manifest  │  │ Catalog   │  │  Suite    │  │ Contract  │   │
│  │ 图像清单  │  │ 脚本目录  │  │ 测试套件  │  │ 契约定义  │   │
│  └───────────┘  └───────────┘  └───────────┘  └───────────┘   │
│  ┌───────────┐  ┌───────────┐  ┌───────────┐  ┌───────────┐   │
│  │ Review    │  │   Trace   │  │  Replay   │  │ Artifacts │   │
│  │   Gate    │  │ 运行轨迹  │  │ 回放包    │  │ 证据资产  │   │
│  └───────────┘  └───────────┘  └───────────┘  └───────────┘   │
└────────────────────────────────────┬────────────────────────────┘
                                     │
┌────────────────────────────────────▼────────────────────────────┐
│             Parameter Regression/Optimization                  │
│  ┌───────────┐  ┌───────────┐  ┌───────────┐  ┌───────────┐   │
│  │ParamRange │  │ Candidate │  │EvalRecord │  │HitDistrib │   │
│  │ 参数范围  │  │ 候选参数  │  │ 评估记录  │  │ 命中分布  │   │
│  └───────────┘  └───────────┘  └───────────┘  └───────────┘   │
│  ┌───────────┐  ┌───────────┐  ┌───────────┐  ┌───────────┐   │
│  │ Accuracy  │  │mlpackRank │  │ensmallen  │  │ Promotion │   │
│  │ 准确性统计│  │(Placeholder)│ │(Placeholder)││ 升级门    │   │
│  └───────────┘  └───────────┘  └───────────┘  └───────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 目录结构

```
cxvision_repo/
├── CMakeLists.txt              # 根构建脚本 (cxvision_imgui_acceptance)
├── cximage/                    # 图像处理与 GUI 模块
│   ├── GuiMain.cpp             # GUI 入口
│   ├── ViewController.h/cpp    # 视图控制器 (顶层场景)
│   ├── ManualStateTestConsole.h/cpp  # 人工调试工作台
│   ├── ParserDebugBridge.h/cpp # 脚本调试桥接
│   ├── ImageAnnotationLayer.h/cpp    # 图像注释层
│   ├── ImageAnnotationUI.h/cpp # 注释 UI
│   ├── CxAnnotationToolRuntime.h/cpp # 注释工具运行时
│   ├── CxAnnotationToolRegister.h/cpp # 工具注册
│   ├── LineGaugeShape.h/cpp    # 直线 Gauge 形状
│   ├── CircleShape.h/cpp       # 圆形形状
│   ├── EllipseShape.h/cpp      # 椭圆形状
│   ├── RectShape.h/cpp         # 矩形形状
│   ├── PolylineShape.h/cpp     # 多段线形状
│   ├── CircleRingGauge.h/cpp   # 圆环规
│   ├── FormfitGauge.h/cpp      # 形位公差规
│   ├── Findline/Findcircle/... # 特征检测算法
│   ├── CxScriptCatalogRuntime.h/cpp # Catalog 运行时
│   ├── CxScriptSuiteRuntime.h/cpp   # Suite 运行时
│   ├── CxScriptSuiteRunner.h/cpp    # Suite 运行器
│   ├── CxScriptImageManifestRuntime.h/cpp # 图像清单
│   ├── CxScriptEvidenceChainRuntime.h/cpp # 证据链
│   ├── CxScriptReviewGateRuntime.h/cpp    # 审核门
│   ├── CxScriptRunTraceRuntime.h/cpp     # 运行轨迹
│   ├── CxScriptToolDisplayExporter.h/cpp # 工具显示导出
│   ├── CxParameterProfileRuntime.h/cpp   # 参数配置
│   ├── CxParamRegressionRuntime.h/cpp    # 参数回归
│   ├── CxAlgorithmTraceSink.h/cpp        # 算法轨迹
│   ├── CxUnifiedLog.h/cpp        # 统一日志
│   └── ...
├── cxgeom/                     # 几何建模模块
│   ├── include/                # 公共头文件
│   └── src/                    # 实现文件
├── cxcloud/                    # 点云处理模块
│   ├── include/
│   └── src/
├── cxparser/                   # 脚本解析核心
│   ├── muParser*.h/cpp         # muParser 核心文件
│   ├── cxscript/               # CxScript 脚本案例
│   │   ├── module/cximage/     # cximage 脚本
│   │   │   ├── catalog/        # 目录注册
│   │   │   ├── stage25/        # Stage2.5 资产
│   │   │   │   ├── contracts/  # 契约定义
│   │   │   │   ├── manifests/  # 图像清单
│   │   │   │   ├── parameters/ # 参数配置
│   │   │   │   ├── suites/     # 测试套件
│   │   │   │   ├── param_regression/ # 参数回归
│   │   │   │   └── templates/  # 模板
│   │   │   ├── frozen/         # 冻结脚本
│   │   │   ├── tests/          # 测试脚本
│   │   │   └── frame_probe/    # 帧探测
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
└── 3D/                         # 三维场景集成模块
    ├── src/
    └── tests/
```

---

## 3. Application/UI 层

### 3.1 ManualStateTestConsole

#### 定位
人工调试工作台，不是算法实现层。提供证据链浏览、Gauge 编辑、参数调优、审核门和回放功能的统一入口。

#### 核心文件
- [ManualStateTestConsole.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/ManualStateTestConsole.h)
- [ManualStateTestConsole.cpp](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/ManualStateTestConsole.cpp)

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
- **Implemented**：基础 UI 框架、证据链浏览、Gauge 编辑、参数调优面板
- **Planned**：完整 Promotion 流程、批量审核

#### 缺口
- Catalog 搜索/过滤功能
- 批量证据导入
- 审核历史追踪

### 3.2 ViewController

#### 定位
顶层场景控制器，继承自 OpenCASCADE 的 `AIS_ViewController`，负责 ImGui 界面与 OCCT 3D 视图的集成、事件路由和渲染编排。

#### 核心文件
- [ViewController.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/ViewController.h)
- [ViewController.cpp](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/ViewController.cpp)

#### 输入
- 用户鼠标/键盘事件
- ScriptResult 回调
- ImageAnnotationLayer 状态

#### 输出
- GUI 渲染命令
- OCCT 视图更新
- Annotation 事件转发

#### 数据结构
- `ProcessImageAnnotationPointerFrame`：统一指针帧处理入口
- `CxImagePointerFrame`：指针输入帧
- `CxImagePointerResult`：指针处理结果

#### 执行流程
1. ImGui 帧开始 → 采集输入状态
2. 调用 ProcessImageAnnotationPointerFrame
3. 更新 ImageAnnotationLayer
4. 渲染图像 + Overlay + ShapeElements
5. ImGui 帧结束

#### 状态
- **Implemented**：基础事件路由、图像渲染、Annotation 集成
- **Planned**：完整 3D/2D 切换、多视图支持

---

## 4. Workbench State 层

### 4.1 ImageAnnotationLayer

#### 定位
图像注释层，管理 ShapeElements 和 OverlayElements，提供统一的 HitTest、Drag、CommitEdit 接口。

#### 核心文件
- [ImageAnnotationLayer.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/ImageAnnotationLayer.h)
- [ImageAnnotationLayer.cpp](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/ImageAnnotationLayer.cpp)

#### 输入
- `CxImagePointerFrame`：指针输入
- Runtime Object 发布的 Shape
- Annotation Tool 创建请求

#### 输出
- `CxImagePointerResult`：处理结果
- `CxShapeCommitResult`：提交结果
- ShapeElements 列表

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
- **Implemented**：ShapeElements 管理、HitTest、Drag、CommitEdit、Runtime 投影
- **Planned**：完整的 Polyline 编辑、AutoBoundary

#### 缺口
- Polyline 顶点编辑
- 元素组合/分组

### 4.2 CxAnnotationToolRuntime

#### 定位
注释工具运行时，管理 Point/Line/Rect/Circle/Ellipse/Polyline 工具的输入处理和状态。

#### 核心文件
- [CxAnnotationToolRuntime.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/CxAnnotationToolRuntime.h)
- [CxAnnotationToolRuntime.cpp](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/CxAnnotationToolRuntime.cpp)

#### 输入
- 鼠标事件（点击、拖动、释放）
- 工具选择/启用状态

#### 输出
- Draft Shape（预览）
- Committed Shape（提交）

#### 数据结构
- `CxAnnotationToolSpec`：工具规格
- `CxShapePoint`：形状点
- `ImageToolMode`：工具模式

#### 执行流程
1. Select Tool → Enable Tool → Click to Start
2. Drag to Preview → Release to Commit
3. ESC to Cancel → Switch Tool to Cancel

#### 状态
- **Implemented**：Point/Line/Rect/Circle/Ellipse/Polyline 创建
- **Implemented**：最小尺寸保护、ESC 取消

### 4.3 ManualGaugeState

#### 定位
手动 Gauge 状态，存储 Line/Circle/Ring Gauge 的几何参数和交互状态。

#### 核心文件
定义于 [ManualStateTestConsole.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/ManualStateTestConsole.h#L433-L469)

#### 输入
- 用户拖动 Handle
- 参数面板修改

#### 输出
- 更新后的 Gauge 参数
- 脚本语句写回

#### 数据结构
- `ManualGaugeState`：Gauge 状态（line_x0/y0/x1/y1, circle_cx/cy/radius 等）
- `GaugeHandleType`：Handle 类型（LineP0/LineP1/CircleCenter/CircleRadius 等）
- `LineGaugeGeometry`：直线 Gauge 几何
- `CircleGaugeGeometry`：圆形 Gauge 几何

#### 执行流程
1. HitTestGaugeHandle → DragGaugeHandle → Update ManualGaugeState
2. Apply to ParserDebugBridge → Run Script → View Result

#### 状态
- **Implemented**：Line/Circle/Ring Gauge 编辑、Handle 拖动、参数写回

---

## 5. Orchestration 层

### 5.1 ParserDebugBridge

#### 定位
脚本调试桥接器，负责 CxScript 的编译、执行、调试和全局输入注入。

#### 核心文件
- [ParserDebugBridge.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/ParserDebugBridge.h)
- [ParserDebugBridge.cpp](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/ParserDebugBridge.cpp)

#### 输入
- Script Text / Script Path
- Global Inputs（MatInput, Int, Double, String）

#### 输出
- Runtime Objects（Findline/Findcircle 等）
- Runtime Variables
- Script Execution Result

#### 数据结构
- `ParserDebugObjectSnapshot`：对象快照
- `CxScriptLineView`：脚本行视图
- `CxScriptStatementView`：语句视图
- `CxScriptSemanticBridgeResult`：语义桥结果

#### 执行流程
1. CompileScript → Bind Global Inputs → RunScript
2. RunPrefixToLine（分步执行）
3. SnapshotRuntimeObjects / SnapshotRuntimeVariables

#### 状态
- **Implemented**：脚本编译/执行、全局输入注入、分步执行、运行时快照

### 5.2 SuiteRunner

#### 定位
Headless Case 运行器，支持完整的 5 步测试流程：Dry-run → ROI Preview → Headless → Contract → Promotion。

#### 核心文件
- [CxScriptSuiteRunner.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/CxScriptSuiteRunner.h)
- [CxScriptSuiteRunner.cpp](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/CxScriptSuiteRunner.cpp)

#### 输入
- `CxScriptSuiteRunOptions`：运行选项
- Suite Script / Image Manifest / Catalog

#### 输出
- `CxScriptSuiteRunResult`：运行结果
- Case-level 资产（snapshot.txt, result_summary.json, overlay images）
- Report（latest_run.json, report.json）

#### 数据结构
- `CxScriptSuiteRunOptions`：运行选项
- `CxScriptSuiteCaseResult`：单个 Case 结果
- `CxScriptSuiteRunResult`：Suite 运行结果

#### 执行流程
1. Dry-run（验证证据链）
2. ROI Preview（可选）
3. Headless Only（执行脚本）
4. Contract（契约判断）
5. Promotion（升级）

#### 状态
- **Implemented**：Dry-run、Headless、Contract、基础报告
- **Planned**：完整 Promotion 流程、并发执行

### 5.3 CxUnifiedLog

#### 定位
统一日志系统，进程级、线程安全、跨进程安全、只追加的 JSONL 文件。

#### 核心文件
- [CxUnifiedLog.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/CxUnifiedLog.h)
- [CxUnifiedLog.cpp](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/CxUnifiedLog.cpp)

#### 输入
- 日志事件（run_start/run_end/annotation_pointer_begin/shape_created 等）

#### 输出
- JSONL 日志文件

#### 数据结构
- `CxUnifiedLogOptions`：日志选项

#### 状态
- **Implemented**：基础日志记录、线程安全、JSONL 格式

---

## 6. cxparser_ext/cxparser Runtime 层

### 6.1 cxparser_ext

#### 定位
构建在 cxparser 核心之上的扩展层，提供完整的脚本执行流水线、类型绑定构建、流程路由、验证引擎、结果交付等企业级功能。

#### 核心文件
- [parser_pipeline.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxparser_ext/pipeline/parser_pipeline.h)
- [parser_runtime_facade.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxparser_ext/pipeline/parser_runtime_facade.h)
- [cxscript_runtime.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxparser_ext/runtime/cxscript_runtime.h)

#### 输入
- ExecutionTarget、ParserBindingSpec、ParserEvidenceBundle

#### 输出
- ExecutionResult、ParserValidationReport

#### 数据结构
- `ParserPipeline`：执行流水线
- `ParserRuntimeFacade`：运行时门面
- `ParserBindingBuilder`：绑定构建器
- `ParserFlowRouter`：流程路由器
- `ParserValidationEngine`：验证引擎

#### 执行流程
```
PrepareTask → MergeBindingSpec → MergeEvidence → Run → Validate → Deliver
```

#### 状态
- **Implemented**：基础流水线、绑定构建、验证引擎
- **Planned**：完整流程路由、复杂验证规则

### 6.2 cxparser

#### 定位
基于 muParser 扩展的脚本解析核心引擎，提供表达式求值、变量绑定、类方法调用、字节码执行等能力。

#### 核心文件
- [muParser.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxparser/muParser.h)
- [muParserBase.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxparser/muParserBase.h)
- [muParserBytecode.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxparser/muParserBytecode.h)
- [muParserClass.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cxparser/muParserClass.h)

#### 状态
- **Implemented**：muParser 核心、类绑定、字节码执行

---

## 7. Vision Algorithms 层

### 7.1 Findline

#### 定位
直线检测算法，支持亚像素级边缘细化和多种参数配置。

#### 核心文件
- [Findline.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/Findline.h)
- [Findline.cpp](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/Findline.cpp)

#### 输入
- ROI 区域（起点、终点、半宽）
- 参数（threshold, linegap, wgap, hgap, filterprofile, method）

#### 输出
- 测量点（w/h 方向）
- 拟合直线
- 支持度/距离统计

#### 状态
- **Implemented**：基础直线检测、拟合、参数配置

### 7.2 Findcircle

#### 定位
圆检测算法，支持环形区域扫描和拟合。

#### 核心文件
- [Findcircle.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/Findcircle.h)
- [Findcircle.cpp](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/Findcircle.cpp)

#### 输入
- 圆心、半径、间隙
- 参数（threshold, samplerate）

#### 输出
- 测量点
- 拟合圆（中心、半径、平均距离）

#### 状态
- **Implemented**：基础圆检测、拟合、超时保护

### 7.3 Findellipse

#### 定位
椭圆检测算法。

#### 核心文件
- [Findellipse.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/Findellipse.h)
- [Findellipse.cpp](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/Findellipse.cpp)

#### 状态
- **Implemented**：基础椭圆检测

### 7.4 FindRect

#### 定位
矩形检测算法。

#### 核心文件
- [FindRect.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/FindRect.h)
- [FindRect.cpp](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/FindRect.cpp)

#### 状态
- **Implemented**：基础矩形检测

### 7.5 CircleRingGauge

#### 定位
圆环规，验证同心圆、厚度和位置。

#### 核心文件
- [CircleRingGauge.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/CircleRingGauge.h)
- [CircleRingGauge.cpp](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/CircleRingGauge.cpp)

#### 输入
- 内圆/外圆测量结果

#### 输出
- 同心度、厚度、位置判断
- 评分和状态

#### 状态
- **Implemented**：基础圆环测量和判断

### 7.6 FormfitGauge

#### 定位
形位公差规，支持多种拟合方法。

#### 核心文件
- [FormfitGauge.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/FormfitGauge.h)
- [FormfitFitMethod.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/FormfitFitMethod.h)

#### 状态
- **Implemented**：基础形位公差测量

---

## 8. Evidence/Review 体系

### 8.1 Evidence Chain

#### 定位
证据链管理，关联 Case、Image、Target、Script、Parameter Profile、Contract。

#### 核心文件
- [CxScriptEvidenceChainRuntime.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/CxScriptEvidenceChainRuntime.h)
- [CxScriptEvidenceChainRuntime.cpp](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/CxScriptEvidenceChainRuntime.cpp)

#### 数据结构
- `CxScriptEvidenceCase`：证据 Case
- `CxScriptEvidenceChainRuntime`：证据链运行时

#### 状态
- **Implemented**：基础证据链加载和查询

### 8.2 Manifest

#### 定位
图像清单，定义测试图像集合及其属性。

#### 核心文件
- [CxScriptImageManifestRuntime.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/CxScriptImageManifestRuntime.h)

#### 状态
- **Implemented**：基础清单加载

### 8.3 Catalog

#### 定位
脚本目录，注册可执行的工具脚本。

#### 核心文件
- [CxScriptCatalogRuntime.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/CxScriptCatalogRuntime.h)

#### 过滤规则
- `frozen = 1`
- `manual_visible = 1`
- `expected_result` 为 `ok` 或 `ng_expected`

#### 状态
- **Implemented**：基础目录加载和过滤

### 8.4 Suite

#### 定位
测试套件，组织一组相关的 Case。

#### 核心文件
- [CxScriptSuiteRuntime.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/CxScriptSuiteRuntime.h)

#### 状态
- **Implemented**：基础 Suite 加载和执行

### 8.5 Contract

#### 定位
契约定义，包含预期结果、最小点数、失败阶段等判断规则。

#### 状态
- **Implemented**：基础契约判断（Findline < 2 points fail, Findcircle < 3 points fail）

### 8.6 Review Gate

#### 定位
审核门，管理审核阶段和决策。

#### 核心文件
- [CxScriptReviewGateRuntime.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/CxScriptReviewGateRuntime.h)

#### 数据结构
- `CxReviewStage`：审核阶段（EvidenceResolved/RoiPreview/HeadlessResult/ToolDisplay/ContractResult/Promotion）
- `CxReviewDecision`：审核决策（Accept/RejectRoi/RejectParameter/DeriveProfile/Stop）
- `CxScriptReviewRequest`：审核请求
- `CxScriptHumanReview`：人工审核记录

#### 状态
- **Implemented**：基础审核状态定义和 JSON 序列化

### 8.7 Trace

#### 定位
运行轨迹，记录执行过程中的关键事件。

#### 核心文件
- [CxScriptRunTraceRuntime.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/CxScriptRunTraceRuntime.h)

#### 状态
- **Implemented**：基础轨迹记录

### 8.8 Replay

#### 定位
回放包，支持完整的执行过程重现。

#### 状态
- **Implemented**：基础回放包生成

---

## 9. Parameter Regression/Optimization

### 9.1 ParamRange

#### 定位
参数范围定义，支持连续范围和离散值。

#### 核心文件
定义于 [CxParamRegressionRuntime.h](file:///d:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo/cximage/CxParamRegressionRuntime.h#L6-L16)

#### 数据结构
- `CxParamRange`：参数范围（name, min_value, max_value, step, discrete_values, role）

#### 状态
- **Implemented**：基础范围定义

### 9.2 Candidate

#### 定位
候选参数生成，支持手动种子、网格搜索、mlpack 排序、ensmallen 优化。

#### 数据结构
- `CxParamCandidate`：候选参数（source, method, threshold, gap, linegap, wgap, hgap, filterprofile, predicted_quality）

#### 状态
- **Implemented**：手动种子、基础网格搜索
- **mlpack_rank**：rule_based_placeholder
- **ensmallen_opt**：bounded_suggestion_placeholder

### 9.3 EvalRecord

#### 定位
评估记录，记录候选参数的执行结果。

#### 数据结构
- `CxParamEvalRecord`：评估记录（candidate_id, case_id, executed, timeout, points, support_score, mean_distance）

#### 状态
- **Implemented**：基础评估记录

### 9.4 HitDistribution

#### 定位
命中分布统计，分析测量点的分布特征。

#### 数据结构
- `CxHitDistributionBin`：分布区间
- `CxHitDistributionSummary`：分布汇总

#### 状态
- **Hit Distribution bins**：placeholder

### 9.5 Accuracy/Stability

#### 定位
准确性和稳定性统计。

#### 数据结构
- `CxParamAccuracyStats`：准确性统计（total_cases, executed_cases, geometry_pass, evidence_pass, human_accept, stability_score, risk_score）

#### 状态
- **Implemented**：基础统计指标

### 9.6 mlpack Rank

#### 定位
基于 mlpack 的参数排序。

#### 状态
- **rule_based_placeholder**：规则占位实现

### 9.7 ensmallen Suggest

#### 定位
基于 ensmallen 的参数优化建议。

#### 状态
- **bounded_suggestion_placeholder**：有界建议占位实现

### 9.8 Mini Regression

#### 定位
迷你回归测试，验证参数候选的泛化能力。

#### 状态
- **L1/L2/L3 mini-regression**：尚未完成

### 9.9 Promotion Gate

#### 定位
参数升级门，决定是否将候选参数升级为正式配置。

#### 状态
- **Profile Promotion**：当前 Can Promote=no

---

## 10. 构建与运行

### 10.1 前置要求

- **操作系统**：Windows
- **编译器**：MSVC (Visual Studio)
- **CMake**：3.21 或更高
- **C++ 标准**：C++17 (主程序) / C++14 (cxparser)
- **第三方库**：GLFW 3.3.10、OpenCASCADE 7.7.0、OpenCV

### 10.2 编译选项

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `CXVISION_ENABLE_CXPARSER_EXT_DEBUG_INPROC` | ON | 嵌入 cxparser_ext 调试层 |
| `CXVISION_ENABLE_LEGACY_STAGE25_CPP` | OFF | 构建已弃用的 C++ Stage25 实现 |
| `CXVISION_BUILD_CXPARSER_RETURN_TESTS` | ON | 构建 return 关键字回归测试 |

### 10.3 构建目标

#### 主目标：cxvision_imgui_acceptance

```bash
mkdir build && cd build
cmake .. -DGLFW_ROOT="D:/glfw-3.3.10" ^
         -DGLAD_ROOT="path/to/glad" ^
         -DOCCT_ROOT="D:/OpenCASCADE-7.7.0-vc14-64/opencascade-7.7.0" ^
         -DOpenCV_DIR="D:/opencv/build"
cmake --build . --config Release
```

### 10.4 运行方式

直接运行 `cxvision_imgui_acceptance.exe`，启动 ManualStateTestConsole 工作台。

---

## 11. 脚本系统 (CxScript)

### 11.1 脚本语言概述

CxScript 是基于 muParser 扩展的领域特定语言（DSL），用于描述视觉检测与几何测量工作流。

### 11.2 脚本文件类型

| 扩展名 | 说明 |
|--------|------|
| `.cxsc` | CxScript 脚本文件 |
| `.cxflow` | 状态机/流程定义文件 |
| `.cxs` | 脚本片段/参数文件 |

### 11.3 脚本目录结构

```
cxparser/cxscript/module/cximage/
├── catalog/                    # 目录注册脚本
│   └── cximage_catalog.cxsc
├── stage25/                    # Stage2.5 资产
│   ├── contracts/              # 契约定义
│   │   ├── findline_ok_contract.cxsc
│   │   ├── findcircle_ok_contract.cxsc
│   │   └── ...
│   ├── manifests/              # 图像清单
│   │   ├── stage25_smoke_manifest.cxsc
│   │   ├── stage25_l0_regression_manifest.cxsc
│   │   └── ...
│   ├── parameters/             # 参数配置
│   │   ├── findline_profiles.cxsc
│   │   └── findcircle_profiles.cxsc
│   ├── suites/                 # 测试套件
│   │   ├── stage25_findline_single_baseline_ok.cxsc
│   │   └── ...
│   ├── param_regression/       # 参数回归
│   │   ├── ranges/             # 参数范围定义
│   │   │   ├── findline_range_conservative.cxsc
│   │   │   └── findcircle_range_conservative.cxsc
│   │   ├── param_candidate_grid_basic.cxsc
│   │   ├── param_candidate_mlpack_rank.cxsc
│   │   ├── param_candidate_ensmallen_opt.cxsc
│   │   └── param_regression_contract.cxsc
│   └── templates/              # 模板
│       ├── find_line_stage25_template.cxsc
│       └── find_circle_stage25_template.cxsc
├── frozen/                     # 冻结脚本（GUI 可见）
│   ├── findline/
│   └── findcircle/
├── tests/                      # 测试脚本
│   ├── shape_tool_palette_l1.cxsc
│   ├── shape_gui_pointer_l2.cxsc
│   └── ...
├── frame_probe/                # 帧探测脚本
├── diagnostic/                 # 诊断脚本
└── deprecated/                 # 已弃用脚本
```

### 11.4 允许的 CxScript 构造

**允许：**
- 对象声明
- `int` / `double`
- 简单赋值
- 已注册对象方法调用
- `global.xxx` 输入输出
- 简单 `if (condition) { ... }`
- `contract.reset / fail / pass / failed / setstatus / setconclusion`
- `return;`（立即结束当前脚本执行）

**禁止：**
- `auto`
- `std::vector` / `std::map`
- `new` / `delete`
- lambda
- template
- namespace
- `class` / `struct` 定义
- `for` / `while`
- `return 1;` / `return x;` / `return object;`
- `switch`
- `else if`
- 复杂 `&&` / `||`
- 对象返回赋值
- 数组字面量
- 文件 IO
- OpenCV 代码

---

## 12. 测试体系

### 12.1 测试策略

项目采用三锚点人工 Gauge 闭环 + 短时单 case Probe + 异常进入 Replay/Review + mini-regression 的测试策略。

### 12.2 测试流程

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

### 12.3 测试类型

| 类型 | 说明 | 状态 |
|------|------|------|
| L0 烟雾测试 | 基础功能验证 | Implemented |
| L1 几何测试 | Shape 几何、HitTest、Drag | Implemented |
| L2 GUI Pointer 测试 | 真实鼠标交互测试 | Implemented |
| L3 Mini Regression | 参数回归测试 | 尚未完成 |

### 12.4 测试资产输出

每个测试 Case 必须生成：
- `snapshot.txt`
- `result_summary.json`
- `result_overlay.png`
- `evidence_overlay.png`
- `tool_display.png`

### 12.5 Contract Pass 规则

- **Findline**：有效点数 < 2 时失败
- **Findcircle**：有效点数 < 3 时失败

---

## 13. Legacy/Appendix

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
| **门面模式 (Facade)** | `ParserRuntimeFacade`, `ParserBindingBuilder` |
| **适配器模式 (Adapter)** | `BlenderSceneAdapter`, `CxGeomSceneAdapter` |
| **策略模式 (Strategy)** | `ISceneAdapter`, `IStructuredAssetGenerator` |
| **编排器模式 (Orchestrator)** | `ThreeDOrchestrator`, `ViewController` |
| **流水线模式 (Pipeline)** | `ParserPipeline` |
| **桥接模式 (Bridge)** | `ParserDebugBridge` |

### D. Legacy Stage25 C++（已弃用）

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

### E. 外部依赖

| 依赖库 | 版本 | 用途 | 配置路径 |
|--------|------|------|----------|
| GLFW | 3.3.10 | 窗口与输入管理 | `D:/glfw-3.3.10` |
| GLAD | - | OpenGL 函数加载 | `../../analysis_workspace/imGuIZMO.quat/libs/glad` |
| OpenCASCADE | 7.7.0 | 几何建模内核 | `D:/OpenCASCADE-7.7.0-vc14-64/opencascade-7.7.0` |
| OpenCV | - | 图像处理 | `D:/opencv/build` |
| ImGui | - | GUI 框架 | 项目内 `imgui/` 目录 |
| nanoflann | - | 最近邻搜索 | 项目内头文件 |

---

*文档版本: v2.0 | 最后更新: 2026-07-13 | 基于仓库: cxvision_repo*