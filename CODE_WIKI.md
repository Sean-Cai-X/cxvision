# CxVision Code Wiki v2.3

> **文档版本**：v2.3
> **对应分支**：`codex/cxcore-integration`
> **核验日期**：2026-07-26
> **文档标题**：Core Boundary、Torch Geometry Handoff 与统一执行链更新基线
> **替代版本**：v2.2，核验日期 2026-07-16
> **核验方式**：当前分支静态代码、CMake 构建清单、提交记录与已有模块测试结论综合判断

---

## 0. 文档状态规则

### 0.1 状态标签

| 标签                         | 定义                               |
| -------------------------- | -------------------------------- |
| **[Verified]**             | 已进入正式构建，并通过固定输入、固定输出和实际运行回归      |
| **[Module Verified]**      | 已在独立模块测试中完成，但尚未验证完整 cxvision 接入链 |
| **[Implemented]**          | 代码已实现且进入构建，尚未覆盖所有正式入口            |
| **[Partial]**              | 主链已建立，但存在明确数据断点或结果缺口             |
| **[Contract]**             | 数据结构、接口或协议已定义，尚未证明真实运行           |
| **[Scaffold]**             | 框架或示例存在，核心执行尚未接通                 |
| **[Placeholder]**          | 当前结果来自规则、静态引用或模拟数据               |
| **[Build Pending]**        | 文件存在，但未确认进入正式 CMake 目标           |
| **[Verification Pending]** | 功能实现存在，但缺固定 Case 或跨入口一致性验证       |
| **[Disabled]**             | 实现存在，但默认关闭                       |
| **[Legacy]**               | 兼容旧流程，禁止作为新功能主要入口                |
| **[Planned]**              | 尚未进入代码                           |

### 0.2 判定原则

本项目今后不得再将"文件存在"直接写成"功能完成"。状态必须依次区分：

```text
Source Exists
→ Build Registered
→ Runtime Reachable
→ Real Data Executed
→ Result Projected
→ Evidence Generated
→ Fixed Regression Verified
```

只有完成最后一步，才允许标记为 `[Verified]`。

---

# 1. 项目定位与当前阶段

## 1.1 项目定位

CxVision 是一个以 C++ 为核心的精密图像分析、几何测量、脚本执行和人工复核平台。系统通过 CxScript 驱动图像工具、几何对象和模型模块，并将执行结果统一投影到交互界面、证据链和回归体系中。

当前架构的核心不再是简单地"调用算法"，而是建立以下完整闭环：

```text
图像与几何输入
→ 脚本和人工工具定义
→ 统一执行入口
→ 算法或模型运行
→ 统一结果
→ 几何对象
→ Overlay
→ Evidence
→ Review
→ Regression
```

旧版 Wiki 已经定义了脚本运行、人工 Gauge、结果捕获和证据链三条标准执行链，但当前代码增加了 `CxCoreBoundary`、AI 路由、几何 Attach 和 Torch Handoff，因此需要把"模型结果如何回到几何层"正式纳入架构。

## 1.2 当前阶段判断

当前项目已经从"搭建调试框架"进入：

> **统一执行链固化、CxCore 边界定义、Torch 结果接入几何层、跨入口结果一致性验证阶段。**

当前不是重新开发 `libtorch_module` 的训练和推理能力。按模块测试结论，模型定义、训练、评估和推理已在模块内部完成；当前主要任务是：

```text
已有 libtorch_module 能力
→ TorchTask 脚本入口
→ Runtime DLL
→ CxInferenceResult
→ Geometry Attach / Shape
→ Overlay / Evidence
```

当前分支最近的主要公开提交包括 Torch Runtime Service 和 Torch Execution Adapter，说明外部调用骨架已经补齐；根 CMake 也已把 `TorchTask.cpp`、`CxTorchExecutionAdapter.cpp`、`CxTorchRuntimeService.cpp`、`TorchRuntimeResultAdapter.cpp` 和 `CxTorchResultProjector.cpp` 纳入主程序目标。

---

# 2. v2.2 到 v2.3 的主要架构变化

## 2.1 新增 CxCore 稳定边界

当前 `cximage` 中新增：

```text
CxCoreBoundary.h/.cpp
CxCoreAiBoundary.h
CxCoreGeometryAttach.h
CxCoreTorchHandoffBridge.h
CxCoreTorchGeometryCxscript.h
```

这些文件不是新的算法实现层，而是用于明确四类跨模块合同：

1. 传统图像算法和几何算法输出合同；
2. CxCore、mlpack、Torch 之间的 AI 路由合同；
3. 模型结果与几何对象之间的 Attach 合同；
4. Torch 任务输入、结果引用和证据引用的 Handoff 合同。

## 2.2 Torch 外层链已进入正式构建

v2.2 只描述了 `TorchRuntimeBridge` 和 `TorchRuntimeResultAdapter`。当前主程序构建清单已经纳入：

```text
TorchRuntimeBridge.cpp
CxTorchRuntimeService.cpp
TorchRuntimeResultAdapter.cpp
CxExecutionTypes.cpp
CxTorchExecutionAdapter.cpp
CxTorchResultProjector.cpp
TorchTask.cpp
```

因此 Torch 外层调用已经不能继续只描述为"桥接原型"，而应明确为：

```text
控制链：[Implemented]
真实模型主线接入：[Partial]
结构化结果解析：[Partial]
几何结果投影：[Partial]
证据链闭合：[Verification Pending]
```

## 2.3 Wiki 原目录描述需要纠正

v2.2 的目录示例把：

```text
TorchRuntimeBridge.cpp
TorchRuntimeResultAdapter.cpp
```

列在 `libtorch_module/` 下，但实际文件位于 `cximage/`。

正确职责是：

```text
cximage/
  TorchTask
  CxTorchExecutionAdapter
  CxTorchRuntimeService
  TorchRuntimeBridge
  TorchRuntimeResultAdapter
  CxTorchResultProjector

libtorch_module/
  模型结构
  训练和推理主线
  模块测试
  Runtime C API
  Runtime Core
```

---

# 3. 更新后的完整系统架构

以下是逻辑职责视图，不表示必须继续增加十二套类或十二个独立 DLL。

```text
┌───────────────────────────────────────────────────────────────┐
│ 1. Application / UI                                           │
│ ManualStateTestConsole / Image View / Script Catalog          │
│ Semantic Flow Graph / Parameter UI / Evidence UI              │
└──────────────────────────────┬────────────────────────────────┘
                               ▼
┌───────────────────────────────────────────────────────────────┐
│ 2. Workbench State & Interaction                              │
│ ManualTestContext / ManualGaugeState                          │
│ ImageAnnotationLayer / AnnotationToolRuntime                  │
│ Shape / HitTest / Drag / CommitEdit / Writeback               │
└──────────────────────────────┬────────────────────────────────┘
                               ▼
┌───────────────────────────────────────────────────────────────┐
│ 3. Manual Controllers                                         │
│ ManualConsoleGauge / RuntimeView / ScriptDebug                │
│ FindLineDebug / FindCircleDebug / ParamRegressionPanel        │
└──────────────────────────────┬────────────────────────────────┘
                               ▼
┌───────────────────────────────────────────────────────────────┐
│ 4. Unified Execution / Orchestration                          │
│ ParserDebugBridge / HeadlessRunner / SuiteRunner              │
│ ParamProbeRunner / CxTorchExecutionAdapter                    │
│ CxParserRuntimeOwner / Manifest Resolver                      │
└──────────────────────────────┬────────────────────────────────┘
                               ▼
┌───────────────────────────────────────────────────────────────┐
│ 5. CxScript Runtime & Asset System                            │
│ cxparser / cxparser_ext                                       │
│ Catalog / Frozen / Diagnostic / Manifest / Suite              │
│ Parameter Profile / Contract / Review / Trace                 │
└──────────────────────────────┬────────────────────────────────┘
                               ▼
┌───────────────────────────────────────────────────────────────┐
│ 6. CxCore Stable Boundary                                     │
│ CxCoreBoundary                                                │
│ CxCoreAiBoundary                                              │
│ BaselineFeatureSample / Route Decision                        │
└───────────────┬───────────────────────────────┬───────────────┘
                ▼                               ▼
┌──────────────────────────────┐ ┌──────────────────────────────┐
│ 7A. Vision Tool Runtime      │ │ 7B. Model Runtime            │
│ FindLine / FindCircle        │ │ libtorch_module              │
│ FindEllipse / FindRect       │ │ Segmentation / YOLO          │
│ FindObject / FastMatch       │ │ MobileViT / Train / Infer    │
│ FindSegmentation / Gauges    │ │ mlpack / ensmallen semantics │
└───────────────┬──────────────┘ └───────────────┬──────────────┘
                └────────────────────┬───────────┘
                                     ▼
┌───────────────────────────────────────────────────────────────┐
│ 8. Canonical Execution Result                                  │
│ CxExecutionResult / CxInferenceResult                         │
│ Detection / Mask / Metrics / Artifact Refs                    │
└──────────────────────────────┬────────────────────────────────┘
                               ▼
┌───────────────────────────────────────────────────────────────┐
│ 9. Geometry Object & Attach                                   │
│ CxCoreGeometryAttach                                          │
│ StableGeometryRef / Mask / Boundary / Keypoints               │
│ GeometryAttachRecord / TorchHandoff                           │
└──────────────────────────────┬────────────────────────────────┘
                               ▼
┌───────────────────────────────────────────────────────────────┐
│ 10. Projection / Display                                      │
│ CxRuntimeProjectionExecutor / CxTorchResultProjector          │
│ CxShapeElementSnapshot / Shape Overlay / ToolDisplay          │
└──────────────────────────────┬────────────────────────────────┘
                               ▼
┌───────────────────────────────────────────────────────────────┐
│ 11. Evidence / Review / Regression                            │
│ Snapshot / Summary / Trace / Overlay / Replay                 │
│ Contract / Review Gate / Parameter Regression                 │
└──────────────────────────────┬────────────────────────────────┘
                               ▼
┌───────────────────────────────────────────────────────────────┐
│ 12. Foundation / Build / Observability                        │
│ OpenCV / OCCT / cxgeom / LibTorch / CMake                     │
│ UnifiedLog / CrashLog / Runtime DLL                           │
└───────────────────────────────────────────────────────────────┘
```

该架构保留了 v2.2 的 UI、脚本、工具、结果、证据和参数回归主干，只在算法与模型之间增加一个稳定的 CxCore 边界，并把模型结果回填几何层的职责正式拆清。

---

# 4. 更新后的标准执行链

## 4.1 传统图像工具链

```text
Catalog / Frozen / Flow Node
→ CxScript
→ cxparser
→ ParserDebugBridge / HeadlessRunner / SuiteRunner
→ FindLine / FindCircle / FindEllipse / FindRect / FastMatch
→ Runtime Result Capture
→ Shape Snapshot
→ Overlay / Evidence
```

Manual、Headless 和 Suite 必须调用同一工具对象和同一结果捕获函数。旧版 Wiki 已明确禁止不同入口维护不同结果结构。

## 4.2 人工 Gauge 链

```text
ImageAnnotationLayer
→ Gauge Shape
→ ManualGaugeState
→ Apply To Globals
→ CxScript
→ 同一个 Find* 执行对象
→ Runtime Capture
→ 结果 Shape
→ Review
```

人工界面只生成几何输入和参数，不得在 UI 中重新实现算法。

## 4.3 CxCore AI 路由链

```text
CxCore Algorithm Output
→ BaselineFeatureSample / Geometry Descriptor
→ AiTaskEnvelope
→ Route Decision
   ├─ StayInCxcore
   ├─ RouteToMlpack
   ├─ UpgradeToTorchModule
   └─ ManualReview
```

`CxCoreAiBoundary` 已定义直接测量、数值分类、几何匹配、区域检测优化、密集分割和视觉嵌入等任务类型，并根据图像 Tensor、结构化描述符和端到端学习需求进行路由。它的作用是明确职责边界，而不是在路由层执行模型。

## 4.4 Torch 几何接入链

```text
ROI / Line / PointSet / Prior Geometry
→ TorchTask
→ CxTorchExecutionAdapter
→ CxTorchRuntimeService
→ Runtime DLL
→ libtorch_module 已有模型主线
→ Result JSON
→ CxInferenceResult
→ Geometry Attach
→ Rect / Mask / Boundary / Keypoints
→ Overlay / Evidence
```

`CxCoreTorchGeometryCxscript.h` 已经定义输入先验、训练标签、结果 Attach、结构证据以及结果回读等脚本片段；`CxCoreTorchHandoffBridge.h` 定义了 source hash、model version、result/evidence refs、bbox/mask/ROI/contour 和下一步动作等 Handoff 元数据。当前应把这些合同接到真实运行结果，而不是再建立第二套模型执行框架。

## 4.5 证据链

```text
Image + Geometry + Parameter + Script + Model
→ Snapshot
→ Result Summary
→ Result Overlay
→ Evidence Overlay
→ ToolDisplay
→ Trace
→ Replay
→ Contract
→ Human Review
```

每个入口必须生成可追溯的输入、执行结果和几何结果，不得仅依据 `status=success` 判定完成。旧版证据链已经具备 Snapshot、Summary、Trace、Overlay、Replay、Contract 和 Review Gate 基础结构。

---

# 5. CxCore 边界说明

## 5.1 `CxCoreBoundary`

### 定位

`CxCoreBoundary` 是传统图像和几何算法向上层提供稳定结果的边界。

主要结果包括：

```text
PointSetOutput
ImageAnalysisOutput
LineOutput
CircleOutput
EllipseOutput
DetectionOutput
MatchOutput
```

并扩展了：

```text
FractalPartition
DistanceField
Skeleton
Centerline
TopologyRepair
BaselineFeatureSampleV1
```

这些结构用于把图像工具结果转换为稳定、可记录、可供模型或统计模块使用的描述，而不是替代 `FindLine`、`FindCircle` 等算法本身。

### 当前状态

* 接口与主要实现：**[Implemented]**
* 文件已存在：**[Implemented]**
* `CxCoreBoundary.cpp` 已纳入 `CXIMAGE_CORE_SOURCES`：**[Implemented]**
* 与固定 Case 的输出一致性：**[Verification Pending]**

因此 CxCore 稳定输出合同已形成且已进入正式构建，但端到端使用位置仍需固化。

## 5.2 `CxCoreAiBoundary`

### 定位

定义从传统算法、结构化特征到 mlpack、Torch 或人工复核的路由规则。

### 约束

```text
Direct Measurement
→ 不应无理由升级到 Torch

Geometry / Descriptor Task
→ 优先 CxCore 或 mlpack

Dense Image / End-to-End Learning
→ Torch

信息不足或信号混合
→ Manual Review
```

### 当前状态

* 数据合同和路由规则：**[Contract/Implemented]**
* 是否已被正式执行入口调用：**[Verification Pending]**

## 5.3 `CxCoreGeometryAttach`

### 定位

管理模型结果与稳定几何对象之间的关系。

主要对象：

```text
RoiObject
LineObject
PointSetObject
MaskObject
BoundaryObject
KeypointsObject
FractalPartitionObject
DistanceFieldObject
SkeletonObject
```

主要关系：

```text
Model Result
→ GeometryAttachRecord
→ StableGeometryRef
→ Display Hint
→ Evidence Ref
```

它解决的是"模型输出属于哪个 ROI、对应哪个几何对象、如何显示和追溯"，而不是模型推理本身。

### 当前状态

* 对象合同和校验：**[Contract/Implemented]**
* 与 `CxInferenceResult` 的正式转换：**[Partial]**
* 与 `ImageAnnotationLayer` 的固定回归：**[Verification Pending]**

## 5.4 `CxCoreTorchHandoffBridge`

### 定位

描述 Torch 与 CxCore/几何层之间的引用和证据合同。

必须记录：

```text
source_hash
model_version
result_ref
evidence_ref
log_path
bbox_ref
mask_ref
roi_ref
contour_ref
geometry_ref
measurement_ref
next_action
```

### 当前状态

* Handoff 数据结构：**[Contract/Implemented]**
* 任务快照和发布字段：**[Implemented]**
* 真实 Runtime 结果填充：**[Partial]**
* CxScript 回读验证：**[Verification Pending]**

---

# 6. Vision Tool Runtime

## 6.1 当前工具范围

```text
FindLine
FindCircle
FindEllipse
FindRect
FindObject
FastMatch
FindSegmentation
CircleRingGauge
FormfitGauge
```

旧版 Wiki 已将这些基础算法标记为 Implemented，其中 FindSegmentation 仍需要完整效果验证；近期提交还包括 FastMatch 修复、FindEllipse 修复、Evidence Chain 修复以及形状投影测试。

## 6.2 运行结果捕获

`CxScriptRuntimeResultCapture` 当前支持捕获：

```text
FindLine
FindCircle
FindEllipse
FindRect
FindSegmentation
FastMatch
TorchTask
```

对于 TorchTask，它会读取 `CxInferenceResult`，调用 `CxTorchResultProjector::Project()`，并将模型结果加入统一 Shape Snapshot。这说明 Torch 已经进入统一结果捕获入口，而不是独立维护另一套 UI 输出。

## 6.3 当前主要传统工具缺口

| 项目                                          | 状态                     |
| ------------------------------------------- | ---------------------- |
| FindLine 基础运行和结果捕获                          | [Implemented]          |
| FindCircle 基础运行和结果捕获                        | [Implemented]          |
| FindEllipse / FindRect / FastMatch Shape 投影 | [Implemented]          |
| 固定 Gauge 与真实边缘一致性                           | [Verification Pending] |
| FindLine / FindCircle IMG-L1～L3 固定回归        | [Partial]              |
| FindSegmentation 多后端效果                      | [Verification Pending] |
| 多入口完全一致结果                                   | [Partial]              |
| 真实 Accuracy/Stability 跨 Case 汇总             | [Partial]              |

---

# 7. `libtorch_module` 模型能力

## 7.1 模块定位

`libtorch_module` 是模型定义、训练、推理、评估和模块测试层。

当前目录已经包含：

```text
DeepLabV3 / DeepLabV3Plus
MobileNetV3
MobileViTv2
YOLOv8
Segmentation Mainline Bridge
MobileViT Mainline Bridge
YOLO Mainline Bridge
Train / Eval / Infer
Task Routing
Test Host
Runtime C API
```

模块 CMake 还定义了 Minimal、Full、Contract、MobileViT、Segmentation 和 Runtime DLL 等目标，并为 MobileViT 与 Segmentation 分别建立 train/infer 测试入口。

## 7.2 模块内状态

按当前模块测试结论：

| 能力                        | 状态                |
| ------------------------- | ----------------- |
| 模型结构                      | [Module Verified] |
| Smoke Train               | [Module Verified] |
| Eval / Infer              | [Module Verified] |
| Device Policy             | [Implemented]     |
| Segmentation Mainline     | [Module Verified] |
| YOLO / Detection Mainline | [Module Verified] |
| MobileViT Mainline        | [Module Verified] |
| 任务边界和基础路由                 | [Implemented]     |

例如，Segmentation Mainline 已经包含模型构建、device 选择、forward、cross entropy、梯度检查、IoU、平均置信度、训练生命周期和统一 mainline summary。

## 7.3 Runtime DLL 当前实际状态

当前 Runtime DLL 构建目标主要包含：

```text
torch_runtime_c_api.cpp
torch_runtime_core.cpp
torch_test_host.h
torch_alltest.h
torch_alltest_2.h
```

`torch_runtime_core.cpp` 当前仍通过：

```cpp
TorchTestHost host;
host.run_task_report(request.task, requested_device);
```

执行任务，并生成状态、运行时间和静态 Handoff 引用。它尚未根据 `TorchTask` 请求直接调用已经存在的 Segmentation、YOLO 或 MobileViT 主线。

因此必须区分：

```text
libtorch_module 内部模型能力       [Module Verified]
Runtime DLL 生命周期和诊断         [Implemented]
Runtime DLL 调用真实模型主线       [Partial]
模型结果进入几何层                 [Partial]
```

这也是当前 Torch 链的核心断点。

---

# 8. Torch 外层接入链

## 8.1 当前调用骨架

```text
TorchTask
→ CxTorchExecutionAdapter
→ CxTorchRuntimeService
→ TorchRuntimeBridge
→ libtorch_module_runtime.dll
→ TorchRuntimeResultAdapter
→ CxInferenceResult
→ CxTorchResultProjector
→ CxScriptRuntimeResultCapture
```

上述 `.cpp` 已经进入根 CMake 的 `CXIMAGE_CORE_SOURCES`，因此控制骨架属于正式构建内容。

## 8.2 `TorchTask`

`TorchTask` 已具有：

```text
settask
setcase
setmodel
setmanifest
setdevice
setinput
settemplate
settimeout
run
getstatus
getresultcount
getmaskavailable
```

`CxTorchTaskSpec` 也已保存：

```text
task_id
case_id
model_id
model_path
manifest_path
input_image_path
template_image_path
requested_device
timeout_ms
extra_json
```

而 `CxInferenceResult` 已定义 detections、mask、metrics、artifact refs 和 evidence refs。

## 8.3 当前请求断点

虽然 `CxTorchTaskSpec` 已保存 `model_path`，但 `CxTorchExecutionAdapter::BuildRuntimeRequest()` 当前只传递：

```text
task
input_image
manifest_path
case_name
extra_json
```

`CxTorchRuntimeService` 和 C API 请求结构也没有 `model_path`、`template_image` 和 `timeout_ms`。

因此当前真实情况是：

```text
脚本能够设置模型
但 Runtime DLL 接收不到脚本指定的模型路径
```

这不是重新建设复杂 ABI 的理由，只需要在现有请求合同中补齐真正需要的少量字段。

## 8.4 Device 语义问题

外层 `CxTorchTaskSpec` 默认使用：

```text
cpu / cuda / auto
```

而当前 Runtime Core 判断的是：

```text
cpu / gpu
```

因此 `cuda` 与 `gpu` 语义需要统一。当前环境变量控制逻辑在收到 `gpu` 时才启用 CUDA。

建议全链统一：

```text
cpu
cuda
auto
```

---

# 9. Canonical Result 与数据边界

当前代码中存在四类结果结构，必须明确各自职责，避免继续重复扩展。

## 9.1 `CxCoreBoundary` 输出

用途：

```text
算法模块稳定输出
基础特征提取
AI 路由输入
跨模块低耦合合同
```

不得承载 UI 状态、Review 状态或完整执行生命周期。

## 9.2 `CxExecutionResult`

用途：

```text
一次完整运行的统一执行结果
输入 Gauge 和参数快照
测量点和拟合结果
运行指标
Trace
可选 inference_result
```

这是 Manual、Headless、Suite 和模型任务的统一执行结果。

## 9.3 `CxInferenceResult`

用途：

```text
模型任务的结构化结果
detections
mask
metrics
model/device/status
artifact refs
evidence refs
```

这是模型结果进入几何层的唯一正式入口。

## 9.4 `CxCoreGeometryAttach`

用途：

```text
把 CxInferenceResult 中的模型语义
关联到稳定 ROI、Mask、Boundary、Keypoints 和其他几何对象
```

它描述的是对象关系，不是另一个执行结果。

## 9.5 `CxShapeElementSnapshot`

用途：

```text
将算法或模型结果转换成显示和 Evidence 使用的几何快照
```

它属于投影层，不应反向成为模型或算法的核心结果。

## 9.6 统一转换方向

```text
Find* / Torch Module
→ CxCore Output 或 Module Result
→ CxExecutionResult / CxInferenceResult
→ GeometryAttachRecord
→ CxShapeElementSnapshot
→ Overlay / Evidence
```

禁止形成反向依赖：

```text
算法模块
→ 直接读取 UI Shape
```

也禁止形成并行结果：

```text
Torch GUI Result
Torch Evidence Result
Torch Shape Result
Torch Script Result
```

所有模型结果必须先进入同一个 `CxInferenceResult`。

---

# 10. Torch Result Adapter 当前状态

`TorchRuntimeResultAdapter` 当前已经写入：

```text
executed
ok
error_code
task_id
case_id
model_id
requested_device
actual_device
status
runtime
raw_result_json
result_ref
evidence_ref
primary_visual_ref
```

但没有解析 `result_json` 中的：

```text
schema
schema_version
model_hash
detections
mask
metrics
artifact_refs
```

因此 `CxInferenceResult.detections` 和 `CxInferenceResult.mask` 在当前外层链中仍可能为空。

### 更新状态

| 子项               | 状态            |
| ---------------- | ------------- |
| 状态和运行时间适配        | [Implemented] |
| Handoff 引用适配     | [Implemented] |
| Detection 结构化解析  | [Partial]     |
| Mask 结构化解析       | [Partial]     |
| Metrics 解析       | [Partial]     |
| Artifact refs 解析 | [Partial]     |
| Schema 校验        | [Planned]     |

---

# 11. Geometry Projection 当前状态

## 11.1 Detection

`CxTorchResultProjector` 已能够把每个 `CxTorchDetection` 转为矩形快照：

```text
x/y/width/height
→ center
→ 四个角点
→ closed rect
```

第一项标记为 `model_best_result`，其他项标记为 `model_candidate`。如果 `detections` 被正确填充，这条几何投影已具备基本能力。

状态：

```text
Detection → Rect Snapshot  [Implemented]
真实 Detection 数据进入   [Partial]
原图坐标一致性             [Verification Pending]
```

## 11.2 Mask

当前 Mask 投影只创建：

```text
shape_kind = mask
semantic_role = model_segmentation_mask
```

如果存在 `contour_ref`，再创建一个没有实际点集的 Polyline Snapshot。

它尚未把：

```text
mask.width
mask.height
mask_ref
overlay_ref
contour points
```

转成可渲染的真实几何内容。

状态：

```text
Mask 语义对象              [Implemented]
Mask 实际图像或像素投影    [Partial]
Contour 实际点集投影       [Partial]
Mask 与原图坐标对齐        [Verification Pending]
```

## 11.3 正确推进原则

不应在 `CxTorchResultProjector` 中重新运行分割或检测。

正确职责是：

```text
libtorch_module 产生模型结果
→ ResultAdapter 解析模型结果
→ GeometryAttach 建立对象关系
→ ResultProjector 转成 Shape 或 Overlay
```

---

# 12. CxScript Asset System

## 12.1 资产层级

```text
catalog/
frozen/
diagnostic/
manifest/
suite/
parameter profile/
contract/
review/
trace/
```

Catalog、Frozen 和 Flow Node 负责脚本选择和语义绑定；它们不得复制算法实现。

## 12.2 Torch 脚本资产

Torch 脚本应分为：

```text
runtime diagnostic
device diagnostic
module task invocation
segmentation inference
detection inference
training lifecycle
geometry handoff
result readback
```

`CxCoreTorchGeometryCxscript.h` 中已经给出输入先验、标签、结果 Attach 和证据回读协议。当前需要确认这些片段是否已注册成实际 CxScript 类型或函数；仅在头文件中存在示例文本不能直接标记为运行时 Verified。

## 12.3 CxScript 约束

1. `.cxsc` 只描述流程和参数；
2. 模型和算法实现留在所属模块；
3. Flow Node 只绑定脚本，不承载模型参数结构；
4. Manual、Headless 和 Suite 共享同一脚本对象；
5. 结果统一由 Runtime Capture 获取；
6. 禁止在脚本测试中伪造 PASS；
7. 引用不存在的 model、image 或 evidence 必须明确失败。

---

# 13. Evidence / Review / Artifact

## 13.1 每个固定 Case 的最低资产

```text
snapshot.txt
result_summary.json
result_overlay.png
evidence_overlay.png
tool_display.png
runtime_trace.json
contract_result.json
```

Torch Case 还应记录：

```text
task_id
model_id
model_path 或模型版本
model_hash
requested_device
actual_device
input_image_ref
result_ref
evidence_ref
mask_ref / bbox refs
```

旧版 Wiki 规定每个 Case 至少输出 snapshot、summary、result overlay、evidence overlay 和 tool display；新版需要把 Trace 和模型引用也提升为 Torch 接入的必需资产。

## 13.2 PASS 定义

### 传统工具 PASS

```text
算法实际执行
必要测量点满足最小要求
拟合结果存在
Shape 可投影
Contract 通过
Evidence 可回放
```

### Torch PASS

```text
模块真实执行
不是 TestHost 静态任务报告
结构化 Detection 或 Mask 存在
Geometry Attach 成功
Shape 或 Overlay 可见
Result 与 Evidence 引用有效
跨入口结果一致
```

仅有：

```text
status = success
result_ref 非空
```

不能判定完整 Torch Gate 通过。

---

# 14. Build / Runtime

## 14.1 根构建目标

主目标：

```text
cxvision_imgui_acceptance
```

当前根 CMake 已纳入：

* Manual Console；
* ImageAnnotationLayer；
* Find* 与 FastMatch；
* ParserDebugBridge；
* Headless、Suite、Evidence、Review、Trace；
* Torch Bridge、Service、Adapter、Projector 和 TorchTask；
* `CxCoreBoundary`（稳定边界输出合同）；
* cxgeom；
* cxparser；
* ImGui；
* `libtorch_module` 子目录。

## 14.2 Runtime DLL

当：

```text
CXVISION_ENABLE_TORCH_RUNTIME=ON
```

并成功建立 `libtorch_module_runtime` 目标时，主程序会依赖该目标，并将 DLL 复制到主程序输出目录。

## 14.3 当前构建问题

### CxCore 实现注册

`CxCoreBoundary.cpp` 已于本次（P0）修改中加入 `CXIMAGE_CORE_SOURCES`。后续应通过一次完整构建验证：主程序能够链接 CxCore 稳定边界，且下游 `FindRect` / `Formfit` / `cxparser_ext` 等对 `CxCoreBoundary` 的引用不再产生孤立目标。

### 环境路径

当前 CMake 仍包含多个 Windows 默认绝对路径：

```text
GLFW_ROOT
GLAD_ROOT
OCCT_ROOT
OpenCV_DIR
LIBTORCH_ROOT
NvToolsExt shim
```

这些路径可以作为本机默认值，但 Wiki 必须标记为可配置 Cache 变量，禁止将其描述成通用环境。

### Runtime DLL 复制

`CXVISION_COPY_RUNTIME_DLLS` 默认关闭，但 Torch Runtime DLL 在目标存在时会单独复制；OpenCV、OCCT 和完整 LibTorch DLL 的复制由可选开关控制。部署测试需要明确区分"编译成功"和"运行依赖完整"。

---

# 15. 更新后的 Current Status Matrix

| 子系统                           | 当前状态                               | 说明                               |
| ----------------------------- | ---------------------------------- | -------------------------------- |
| ManualStateTestConsole 模块拆分   | [Implemented]                      | 主工作台和控制器已拆分                      |
| ImageAnnotationLayer          | [Implemented]                      | Shape、HitTest、Drag、Commit        |
| Annotation Tool Runtime       | [Implemented/Partial]              | 基础创建完成，复杂交互仍需回归                  |
| ManualGaugeState              | [Implemented]                      | Line/Circle/Ring Gauge 状态        |
| Gauge → Globals               | [Implemented]                      | 可进入脚本执行参数                        |
| Runtime Projection            | [Implemented]                      | 传统工具基础投影                         |
| Runtime Writeback             | [Partial]                          | 需继续验证重新执行一致性                     |
| ParserDebugBridge             | [Implemented]                      | Manual 调试入口                      |
| SuiteRunner                   | [Implemented]                      | Suite 运行和报告基础存在                  |
| HeadlessRunner                | [Implemented/Partial]              | 通用入口存在，跨入口一致性未全部验证               |
| Runtime Result Capture        | [Implemented]                      | 已支持传统工具和 TorchTask               |
| Evidence Chain                | [Implemented/Partial]              | 基础资产存在，模型真实结果链未闭合                |
| Parameter Range / Candidate   | [Implemented Phase 1]              | 参数范围和候选结构存在                      |
| Candidate → Probe 循环          | [Partial]                          | UI 批量循环未完全闭合                     |
| Hit Distribution              | [Placeholder]                      | 仍需真实空间分布                         |
| Accuracy / Stability          | [Partial]                          | 缺跨 Case 固定汇总                     |
| mlpack 脚本资产                   | [Implemented Assets]               | 原生 Runtime 接入待验证                 |
| mlpack Rank                   | [Placeholder]                      | 尚未使用真实 EvalRecord                |
| ensmallen 脚本资产                | [Implemented Assets]               | 语义资产存在                           |
| ensmallen Optimize            | [Placeholder]                      | 尚未接真实 Objective                  |
| CxCoreBoundary 合同             | [Implemented]                      | 输出和特征合同已形成                       |
| CxCoreBoundary 主构建            | [Implemented]                      | `.cpp` 已纳入 `CXIMAGE_CORE_SOURCES` |
| CxCoreAiBoundary              | [Contract/Implemented]             | 路由规则已定义                          |
| CxCoreGeometryAttach          | [Contract/Implemented]             | 几何对象和 Attach 已定义                 |
| Torch 模块训练/推理                 | [Module Verified]                  | 模块内测试完成                          |
| Torch Runtime DLL 生命周期        | [Implemented]                      | create/run/free/version          |
| Torch 外层控制链                   | [Implemented]                      | Task/Adapter/Service/Bridge 已进构建 |
| Torch 真实模型主线接入                | [Partial]                          | Runtime Core 仍使用 TestHost        |
| Torch 请求模型参数                  | [Partial]                          | model_path 未传入 DLL               |
| Torch Result Adapter          | [Partial]                          | 状态已适配，mask/detections 未解析        |
| Detection Geometry Projection | [Implemented/Verification Pending] | Rect 生成逻辑存在                      |
| Mask Geometry Projection      | [Partial]                          | 目前主要为语义占位                        |
| Torch Evidence 闭环             | [Partial]                          | Handoff refs 存在，真实结果未完全填充        |
| Mini Regression               | [Planned]                          | 固定小型回归尚未闭合                       |
| Promotion Gate                | [Disabled]                         | 保持关闭                             |
| Legacy Stage25                | [Legacy/Disabled]                  | 默认不构建                            |

---

# 16. 当前关键断点

## P0：文档和构建一致性

1. ~~更新 `CODE_WIKI.md` 版本和核验日期~~（本次已完成）；
2. ~~修正 Torch 文件所属目录~~（本次已完成，见 §2.3）；
3. ~~将 `CxCoreBoundary.cpp` 纳入正式构建或明确独立目标~~（本次已完成，见 `CMakeLists.txt`）；
4. ~~为新增 CxCore 文件建立状态说明~~（本次已完成，见 §5）；
5. ~~修正旧版章节编号错位~~（本次已完成，章节编号已按 v2.3 重排）。

## P1：Torch 主线链接

当前 Runtime Core：

```text
TorchTask
→ TorchTestHost
```

目标：

```text
Diagnostic Task
→ TorchTestHost

Segmentation Task
→ 已有 Segmentation Mainline

Detection Task
→ 已有 YOLO Mainline

Classification / Embedding
→ 已有 MobileViT Mainline
```

禁止重新实现第二套 DeepLab 或 YOLO Executor。

## P2：请求合同补齐

在现有请求中补充必要字段：

```text
model_path
template_image
timeout_ms
task_kind 或明确 task id
```

不需要建设复杂版本化平台，但必须保证 `TorchTask.setmodel()` 的值能到达模块主线。

## P3：结构化结果接入

模块真实结果必须填入：

```text
CxInferenceResult.detections
CxInferenceResult.mask
CxInferenceResult.metrics
CxInferenceResult.artifact_refs
```

禁止只返回 summary 文本和静态引用。

## P4：几何 Attach 与投影

```text
Detection
→ Rect Shape

Mask
→ Mask Overlay

Boundary
→ Polyline

Keypoints
→ Point Set
```

必须保持原图坐标，并生成稳定 `owner_ref` 和 `stable_ref`。

## P5：固定回归

建立：

```text
T0 Asset Preflight
T1 Parser Binding
T2 Runtime Lifecycle
T3 Module/Mainline Parity
T4 Structured Result
T5 Geometry Projection
T6 Evidence Output
T7 Manual/Headless/Suite Parity
```

---

# 17. 更新后的推进顺序

| 优先级 | 工作                   | 完成定义                              |
| --- | -------------------- | --------------------------------- |
| P0  | Wiki 与 CMake 同步      | 文件、层级、状态和构建清单一致（本次完成）             |
| P1  | CxCoreBoundary 正式构建  | 主程序可调用稳定边界输出（已纳入 `CXIMAGE_CORE_SOURCES`） |
| P2  | Torch Runtime 调用已有主线 | 不再以 TestHost 代替真实推理               |
| P3  | 补齐 model/input 请求    | 脚本设置值可到达模块                        |
| P4  | 解析 Detection/Mask    | `CxInferenceResult` 有真实结构化数据      |
| P5  | Geometry Attach      | 模型结果关联稳定几何对象                      |
| P6  | Shape / Overlay      | Rect、Mask、Boundary、Keypoints 可见   |
| P7  | Evidence 固化          | Snapshot、Summary、Overlay、Trace 完整 |
| P8  | 跨入口一致性               | Manual、Headless、Suite 结果一致        |
| P9  | Mini Regression      | 固定 Case 自动回归                      |
| P10 | 参数 Rank/Optimize     | mlpack/ensmallen 接真实 EvalRecord   |

---

# 18. Architecture Rules

## 18.1 单一算法和模型主线

1. Manual、Headless、Suite 不得各自实现算法；
2. `libtorch_module` 模块测试和 cxvision Runtime 必须调用同一模型主线；
3. TestHost 只用于诊断和测试编排；
4. UI 不执行模型前处理或后处理；
5. Result Projector 不重新运行算法。

## 18.2 单一结果入口

1. 传统工具进入 `CxExecutionResult`；
2. 模型结果进入 `CxInferenceResult`；
3. `CxInferenceResult` 作为 `CxExecutionResult.inference_result` 的模型子结果；
4. Geometry Attach 只描述对象关系；
5. Shape Snapshot 只用于显示和证据；
6. 禁止新增平行的 Torch GUI Result、Torch Evidence Result 或 Torch Shape Result 作为正式数据源。

## 18.3 模块归属

```text
模型训练和推理
→ libtorch_module

传统图像算法
→ cximage

几何构建
→ cxgeom / OCCT

脚本解析
→ cxparser / cxparser_ext

执行编排和结果适配
→ cximage integration layer

显示和人工交互
→ Manual Console / ImageAnnotationLayer

证据和回归
→ CxScript Evidence / Suite / Regression
```

## 18.4 CxCore 边界规则

1. CxCoreBoundary 不依赖 UI；
2. CxCoreAiBoundary 不直接运行模型；
3. GeometryAttach 不保存模型对象；
4. Handoff Bridge 不替代真实结果；
5. 所有引用必须可追踪到 source hash、model version、result 和 evidence；
6. 合同文件必须通过固定 Case 验证后才能标记 Verified。

---

# 19. Wiki 更新清单

本次 v2.3 更新已覆盖以下内容：

1. 标题从 `v2.2` 更新为 `v2.3`；
2. 核验日期更新为 `2026-07-26`；
3. 项目阶段改为"CxCore 边界固化与 Torch 几何接入"；
4. 完整架构增加 CxCore Boundary 和 Geometry Attach；
5. 目录树修正 Torch Bridge 和 Adapter 的真实位置；
6. Torch 章节增加 Task、Adapter、Service、Projector；
7. 区分模块测试完成与 Runtime 接入完成；
8. 明确 Runtime Core 当前仍调用 TestHost；
9. 增加请求缺少 `model_path` 的断点；
10. 增加 `CxInferenceResult` 的正式字段说明；
11. 增加 Detection 与 Mask 投影的不同完成状态；
12. 增加 CxCoreBoundary 的 Build Pending/Implemented 状态；
13. 更新 Current Status Matrix；
14. 更新 Roadmap，将 Torch 主线和几何接入提升到 P0/P1；
15. 增加"Source/Build/Runtime/Regression"四级核验规则。

---

# 20. 当前管理结论

当前框架已经完成了主要骨架建设：

```text
GUI
CxScript
Parser
Headless/Suite
传统工具
Torch 外层控制链
Canonical Result
Projection
Evidence
```

新加入的 CxCore 边界又补充了：

```text
算法稳定输出
AI 路由
几何对象 Attach
Torch Handoff
```

当前最大问题已经不是"缺少框架"，而是多个已经存在的结构尚未由真实数据贯通：

```text
libtorch_module 已有模型能力
但 Runtime Core 仍走 TestHost；

TorchTask 已能设置 model_path
但 DLL 请求未携带 model_path；

CxInferenceResult 已有 detections 和 mask
但 ResultAdapter 尚未填充；

Detection 投影已有矩形逻辑
但真实 detection 尚未进入；

Mask 已有语义 Shape
但实际 mask、轮廓和坐标尚未进入；

CxCoreBoundary 已有实现
且已纳入 `CXIMAGE_CORE_SOURCES`，仍需通过构建和固定回归验证。
```
