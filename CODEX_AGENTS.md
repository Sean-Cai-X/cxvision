# CxParser 开发边界

- CxParser/CxScript 由单一 `CxParserRuntimeOwner` 持有，并在 Owner 线程内严格串行执行；一个 case 完整执行、结果回收、资产导出和清理后，才能开始下一个 case。
- 禁止在 Parser 执行链中使用 `std::thread`、`std::async`、线程池、并行 case、后台 worker 或 `detach()`；不得为了 UI 响应或超时把 Parser 放入后台线程。
- 禁止复制、共享或对外返回 Parser、工具对象、Image、Shape 的裸指针/引用。工具对象只能在当前 Parser 会话存活期间访问，并在 Parser 销毁前转换为数值、字符串和 Shape snapshot 等值语义结果。
- Parser 注册只负责类型、方法、函数和对象状态读取能力，不得启动线程、绑定流程、执行脚本或判断业务 PASS/FAIL。
- 超时优先由 Findline/Findcircle 内部的 elapsed/scan/sample 预算协作停止；无法协作停止时，只允许串行启动一个隔离的 Headless 子进程，禁止用 detached worker 模拟超时。
- 当前阶段禁止开展 Parser 多实例并行、资源池化或并发性能优化；如需改变以上边界，必须先单独评审并更新本文件。

　或并发性能优化；如需改变以上边界，必须先单独评审并更新本文件。

### CxScript Parser 执行入口约束
#### 1. CxScript 必须使用 `CxParserRuntime::Compile()`

> 1. CxScript 统一使用 `CxParserRuntime::Compile()`，禁止用 `SetExpr/Eval` 执行脚本。
> 2. 外部变量统一使用 `global_` 前缀，禁止 `global.xxx` 伪对象语法。
> 3. `global_matInput` 必须是当前 Parser Runtime 中的真实 `Image` 对象。
 
凡是包含以下任意内容的 cxscript：

- 对象声明
- 多条语句
- 对象方法调用
- 赋值语句
- `if` 语句
- `return;`

必须使用：

```cpp
mu::CxParserRuntime runtime;
runtime.ParserInitialClassFunction(0);
runtime.Compile(script.c_str());
```

禁止使用：

```cpp
runtime.SetExpr(script);
runtime.Eval();
```

`SetExpr()` / `Eval()` 仅允许用于单个纯数值或布尔表达式，例如：

```cpp
a + b
threshold > 10
sin(angle)
```

禁止使用 `SetExpr()` / `Eval()` 执行：

```cpp
Image image;
Findline line;
line.measure(image);
```

原因：表达式求值入口不支持 CxScript 的对象声明和多语句语义，会产生：
Invalid function-, variable- or constant name

不得通过开启 VarFactory、修改类注册或重复注册 Binding 来绕过执行入口错误。

### Parser 初始化与注册约束

每个 Parser Runtime 实例只能执行一次核心初始化：
runtime.ParserInitialClassFunction(0);

禁止对同一个 Parser Runtime 重复调用核心初始化。
扩展 Binding 必须按脚本用途注册：
普通 cximage 算法脚本：
  只注册核心类型和方法。
Annotation Tool manifest：
  注册 Annotation Tool bindings。
Shape interaction suite：
  注册 Shape Test bindings。


禁止普通算法 Headless Runner 无条件调用包含所有扩展注册的聚合函数。

禁止对同一个 Parser 重复注册同名的类、函数、方法、常量或变量。出现 `Name conflict` 时，必须先检查初始化和注册调用次数，不得通过捕获并忽略异常处理。

凡是调用 `CxParserRuntime::Compile()` 的 Runtime Owner，必须先绑定生命周期覆盖 Runtime 的 stream；stream 未绑定属于初始化错误，不允许通过修改 muParser 或绕开 Compile 修复。

### 外部变量命名约束

禁止使用点号伪装对象成员或命名空间：
global.roi_x0
global.threshold
global.matInput


禁止：
parser.DefineVar("global.roi_x0", &roi_x0);


外部注入的数值统一使用 `global_` 前缀：
global_roi_x0
global_roi_y0
global_roi_x1
global_roi_y1
global_threshold
global_method


C++ 与 cxscript 必须使用完全相同的名称：
parser.DefineVar("global_roi_x0", &roi_x0);
parser.DefineVar("global_threshold", &threshold);

int x0 = global_roi_x0;
int threshold = global_threshold;


禁止创建名为 `global` 的类或对象。
禁止同时兼容 `global.xxx` 和 `global_xxx` 两种运行语义。

### Headless 图像输入约束

`global_matInput` 必须是解析器中的真实 `Image` 对象，不是：
- `double` 变量
- 图片路径字符串
- `cv::Mat*` 数值地址
- 伪造的 `global` 对象属性
 
cxscript 使用：
Image m_image;
m_image.copyFromMat(global_matInput);
禁止通过 `DefineVar()` 注册 `global_matInput`。
禁止把对象声明和用户脚本拼接后交给 `SetExpr()` / `Eval()`。
禁止只声明空的 `global_matInput` 而不注入真实图片。

### Parser Runtime 对象生命周期约束
通过以下接口注册的地址：
parser.DefineVar(name, &value);

其指向的变量必须至少存活到该脚本执行完全结束。

禁止：
{
    double value = 10;
    parser.DefineVar("global_threshold", &value);
}
// value 已失效后再执行脚本
runtime.Compile(script);


Parser 中创建的 `Image`、`Findline`、`Findcircle` 等对象只能通过当前 Runtime 查询和使用。

禁止把 Parser 对象指针保存到 Runtime 生命周期之外。

禁止跨 Parser Runtime 传递类对象指针。

禁止复制或共享 `CxParserRuntime`、`mu::Parser` 及其内部对象指针。

### Headless 标准执行顺序

普通算法 Headless 执行器必须遵循以下顺序：

1. 读取并验证 cxscript。
2. 读取 image manifest、target、参数和 contract。
3. 完成 dry-run；证据链不完整立即停止。
4. 创建一个 `CxParserRuntime`。
5. 调用一次 `ParserInitialClassFunction(0)`。
6. 只注册当前脚本用途所需的扩展 Binding。
7. 通过 `DefineVar()` 注入 `global_*` 数值。
8. 加载图片并验证尺寸。
9. 通过 `Compile("Image global_matInput;")` 创建输入对象。
10. 查询 `global_matInput` 并注入真实 `cv::Mat`。
11. 使用 `CxParserRuntime::Compile()` 执行 cxscript。
12. 查询脚本产生的工具对象和事实指标。
13. 输出 snapshot、summary 和 overlay 资产。
14. 最后执行 cxscript contract。

禁止跳过 dry-run。

禁止在图片为空、对象查询失败或脚本执行失败后继续算法。

禁止伪造 PASS。


### 错误定位顺序

遇到：
Invalid function-, variable- or constant name
必须按以下顺序检查：
1. 是否错误地使用了 `SetExpr()` / `Eval()` 执行 CxScript。
2. 是否使用了未迁移的 `global.xxx` 名称。
3. 变量是否已通过 `DefineVar()` 注入。
4. 类是否通过 `ParserInitialClassFunction(0)` 注册。
5. 对象是否在使用前通过 CxScript 声明。
6. 方法名是否已注册且大小写一致。
7. 对象参数是否来自同一个 Parser Runtime。

遇到：
Name conflict

必须按以下顺序检查：

1. `ParserInitialClassFunction(0)` 是否重复调用。
2. 扩展 Binding 是否重复注册。
3. 普通算法脚本是否错误加载了 Suite/Annotation Binding。
4. 同一个 Parser 是否被不同 Owner 重复初始化。
5. 同名变量是否重复 `DefineVar()`。

禁止优先使用以下方式“修复”上述错误：
- 启用 `SetVarFactory()`
- 忽略异常
- 重命名随机函数
- 重复初始化 Parser
- 新建第二套 Parser 执行链
- 在脚本前后拼接不可追踪的隐藏代码

### 禁止重复定义

- 每个 Parser Runtime 只能调用一次 `ParserInitialClassFunction(0)`。
- 每个对象只能声明一次；已存在的 `global_matInput` 必须查询并复用。
- 注入新图片只调用 `copyFromMat()`，不得重新声明 `Image global_matInput;`。
- 扩展 Binding 只能注册一次。
- 执行用户脚本前，应检查脚本是否重复声明外部输入对象。
- Parser 重置或新建 Runtime 后，才允许重新注册类型和声明输入对象。


### global 输入变量命名约束

禁止在 cxscript 和 C++ Binding 中使用带点号的伪对象变量名：

- 禁止 `global.roi_x0`
- 禁止 `global.threshold`
- 禁止 `global.matInput`
- 禁止 `DefineVar("global.xxx", ...)`

原因：当前 `global.xxx` 不是对象成员访问，而是解析器中的扁平变量名。该写法容易与 `object.method()` 和对象成员语义混淆，并迫使 `global.matInput` 使用源码字符串替换等特殊适配。

所有外部注入变量统一使用 `global_` 前缀：

```cpp
global_roi_x0
global_roi_y0
global_roi_x1
global_roi_y1
global_threshold
global_method
global_matInput
C++ 注入必须使用相同名称：
parser.DefineVar("global_roi_x0", &roi_x0);
parser.DefineVar("global_threshold", &threshold);
cxscript 必须直接读取扁平变量：
int x0 = global_roi_x0;
int threshold = global_threshold;

Image m_image;
m_image.copyFromMat(global_matInput);
其中：
global_* 仅表示由 GUI、manifest、suite 或 Headless 环境注入的外部变量。
global_matInput 必须是解析器中的 Image 对象，不得通过 DefineVar 注册。
禁止将图片路径字符串伪装成 global_matInput。
禁止把 cv::Mat 指针注册为普通数值变量。
禁止创建名为 global 的类或对象。
禁止把 global.xxx 自动替换成 global_xxx；脚本源文件必须直接使用正确名称。
禁止同时兼容点号命名与下划线命名，避免形成双重运行语义。
所有 catalog、suite、contract、frozen、diagnostic 和 template 脚本必须采用相同命名规则。

> cxscript 不允许使用点号伪装命名空间或对象成员。点号只用于已注册对象的方法调用；外部输入统一使用 `global_` 前缀的扁平名称

---

# CxScript 对象参数与 DefineClassFun 注册约束

## 一、CxScript 对象参数的唯一书写语义

CxScript 中，对象作为已注册方法的参数时，必须直接传递对象变量名。

正确：

```cpp
Image m_image;
Match m_match;

m_image.copyFromMat(global_matInput);
m_match.learn(m_image);
---

# 编译、测试与验收硬约束

## 一、环境变量与路径约定

所有线程必须使用占位符，不得擅自固定或新建 build 目录。

```text
<REPO_ROOT>   = cxvision_repo 根目录
<BUILD_DIR>   = 用户明确指定或已经生成的 CMake build 目录
<BINARY>      = <BUILD_DIR>/Release/cxvision_imgui_acceptance.exe
<IMAGE_ROOT>  = 测试图片目录
<MANIFEST>    = stage25/stage26 image manifest
<RUN_ROOT>    = cxvisionai/cxscript_runs
<RUN_ID>      = run_YYYYMMDD_HHMMSS_<topic>
<SHARED_LOG>  = <RUN_ROOT>/_shared/cxvision_imgui_acceptance.jsonl
```

硬约束：

- 不得假设 `<BUILD_DIR>` 是 `build`、`build01` 或 `AIbuild`。
- 不得删除、清空、重新初始化用户提供的 build 目录。
- 不得执行 `cmake --fresh`、递归删除 build、`git reset --hard`。
- 未经用户授权，不得重新运行 CMake Configure/Generate。
- 用户只要求代码修改时，不得擅自编译或启动 GUI。
- 用户授权编译时，只编译明确目标。
- 测试输出必须写入新的 `<RUN_ID>` 目录，不得覆盖旧报告。
- 统一日志必须增量追加，不得截断。
- 不得修改 `<BUILD_DIR>/Release` 中的脚本、测试资产作为源码。
- 依赖缺失时必须报告，不得静默从其他 build 目录复制；用户明确允许后才可复制，并记录来源和目标。

## 二、工作目录规则

编译：

```powershell
cmake --build <BUILD_DIR> --config Release --target cxvision_imgui_acceptance
```

运行测试前，工作目录必须满足 runtime 能解析：

```text
cxparser/cxscript/...
```

建议工作目录为：

```text
<REPO_ROOT>
```

或者程序已明确支持的 runtime root。

每次报告必须记录：

```text
repo_root
build_dir
binary_path
binary_last_write_time
working_directory
source_revision 或 git diff 状态
suite_path
catalog_path
manifest_path
output_dir
unified_log_path
完整命令行
```

## 三、什么时候需要编译

### 必须编译

修改以下任一内容后必须编译：

```text
*.h
*.cpp
CMakeLists.txt
parser 注册函数
Shape/HitTest/Drag
ViewController
ManualStateTestConsole
Runtime Summary
Suite/Headless Runner
FastMatch/Findline/Findcircle 算法或接口
统一日志
```

### 通常不需要编译

只修改以下 cxscript 资产时，可直接使用现有二进制测试：

```text
catalog/　*.cxsc
suite/ *.cxsc
contract/ *.cxsc
profile/ *.cxsc
frozen/ *.cxsc
diagnostic/ *.cxsc
manifest/ *.cxsc
```

但满足以下情况仍必须编译：

```text
cxscript 使用了新的注册类型
cxscript 调用了新的注册方法
cxscript 依赖新的 global 字段
cxscript 依赖新的 summary 字段
parser 语法能力发生变化
```

## 四、编译验收规则

编译命令：

```powershell
cmake --build <BUILD_DIR> `
  --config Release `
  --target cxvision_imgui_acceptance
```

编译 PASS 必须同时满足：

```text
命令退出码为 0
cxvision_imgui_acceptance.exe 存在
二进制修改时间晚于本轮相关源文件
没有 LNK1169/LNK2005 等重复符号
没有 unresolved external symbol
没有使用旧目标或错误 build 目录
```

只看到：

```text
Build succeeded
```

不能单独作为结论，必须同时报告二进制路径和时间。

编译结论只能使用：

```text
COMPILE_PASS
COMPILE_FAIL
COMPILE_NOT_RUN
COMPILE_BLOCKED_ENV
```

禁止把 `COMPILE_PASS` 写成工具功能 `PASS`。

## 五、测试执行顺序

所有工具测试必须按以下层级逐级执行，前一级失败则停止后续测试。

```text
T0：脚本/资产预检
T1：Shape/Runtime 投影
T2：GUI Pointer/HitTest/Drag
T3：单图 Headless 工具运行
T4：Suite dry-run
T5：L1 单图回归
T6：L2 多图 mini-regression
T7：L3 稳定性回归
T8：人工 GUI 复核
T9：Contract/Promotion gate
```

不得因为 T1 Shape 测试通过，就宣称真实算法通过。

---

# 六、T0 脚本与资产预检

必须检查：

```text
script 存在
image 存在
target_id 存在
ROI 存在
parameter profile 存在
contract 存在
catalog 能解析 script_id
suite 能解析 image_id + target_id
输出目录可创建
```

cxscript 必须符合允许语法：

```text
对象声明
int/double
简单赋值
已注册方法调用
global.xxx
简单 if
contract API
return;
```

发现以下内容立即失败：

```text
JSON/YAML 风格 cxsc
step 表格
method_input/method_output
for/while
auto
std::vector/std::map
new/delete
复杂 &&/||
对象返回赋值
脚本内文件 IO
脚本内 OpenCV
script: xxx
method_status: xxx
```

结论：

```text
ASSET_PREFLIGHT_PASS
ASSET_PREFLIGHT_FAIL
```

---

# 七、T1 Shape/Runtime 投影测试

命令模板：

```powershell
<BINARY> `
  --shape-interaction-smoke `
  --annotation-tool-manifest cxparser/cxscript/module/cximage/tool_annotation_basic.cxsc `
  --shape-interaction-suite <SHAPE_SUITE> `
  --out <RUN_ROOT>/shape_interaction/<RUN_ID> `
  --unified-log <SHARED_LOG>
```

FastMatch：

```text
<SHAPE_SUITE> =
cxparser/cxscript/module/cximage/tests/shape_fastmatch_projection_l1.cxsc
```

必须验证：

```text
learn_roi 存在且 editable=true
search_roi 存在且 editable=true
result 元素 editable=false
stable_ref 无重复
owner_type=fastmatch
owner_ref 正确
owner_binding 正确
重新发布不产生重复元素
ROI 编辑后结果 stale=true
```

真实 learn/match 未执行时，只能得出：

```text
FASTMATCH_ROI_PROJECTION_PASS
```

不能得出：

```text
FASTMATCH_RESULT_PASS
```

必须生成：

```text
shape_interaction_report.json
shape_interaction_report.md
shape_interaction_failures.md
shape_elements_before.json
shape_elements_after.json
projection_summary.json
interaction_trace.json
```

## Shape 测试禁止假阳性

必须检查：

```text
expected_handle == actual_handle
expected_vertex == actual_vertex
expected shape delta 为 0 时也执行比较
actual_shape_kind 从真实 created_ref 查询
commit_result.committed == true
active drag 在 release 后为 false
runtime before/after 确实发生预期变化
```

禁止：

```cpp
actual_shape_kind = expected_shape_kind;
pass = true;
```

---

# 八、T2 GUI Pointer/Drag 测试

命令模板：

```powershell
<BINARY> `
  --shape-interaction-smoke `
  --annotation-tool-manifest cxparser/cxscript/module/cximage/tool_annotation_basic.cxsc `
  --shape-interaction-suite <GUI_SUITE> `
  --out <RUN_ROOT>/gui_pointer/<RUN_ID> `
  --unified-log <SHARED_LOG>
```

FastMatch：

```text
<GUI_SUITE> =
cxparser/cxscript/module/cximage/tests/shape_fastmatch_gui_pointer_l2.cxsc
```

必须经过真实统一入口：

```cpp
ViewController::ProcessImageAnnotationPointerFrame()
```

不得直接调用：

```cpp
shape->dragHandle()
```

代替 GUI 测试。

必须覆盖：

```text
选择 learn ROI
拖动 learn ROI Center
拖动 learn ROI Corner
选择 search ROI
拖动 search ROI Center
拖动 search ROI Corner
拖出图像后 release
ESC cancel
结果框不可编辑
缩放 78%/100%/150%
窗口平移后 HitTest
```

每个 case 保存：

```text
screen coordinates
image coordinates
zoom
image origin
canvas bounds
pointer events
hit result
selected_ref
created_ref
commit result
geometry before/after
runtime before/after
```

结论：

```text
GUI_POINTER_PASS
GUI_POINTER_FAIL
GUI_POINTER_PENDING_MANUAL
```

自动 GUI Pointer 测试通过后，仍必须进行人工 GUI 复核。

---

# 九、T3 单图 Headless 测试

命令模板：

```powershell
<BINARY> `
  --headless `
  --cxscript-headless `
  --image <IMAGE_PATH> `
  --script <DIRECT_SCRIPT> `
  --case-name <CASE_ID> `
  --out <RUN_ROOT>/headless/<RUN_ID>/<CASE_ID> `
  --max-steps 10000 `
  --unified-log <SHARED_LOG>
```

说明：

- `--cxscript-headless` 是实际执行入口。
- `--headless` 用于统一日志模式识别；如代码已修正模式识别，可省略。
- 每个 case 必须设置外部 wall-clock timeout。
- FastMatch/Findcircle 等可能长时间运行的工具不得仅依赖 `--max-steps`。

每个 case 必须生成：

```text
snapshot.txt
result_summary.json
result_overlay.png
evidence_overlay.png
tool_display.png
line_trace.json
variable_snapshot.json
object_state.json
log.txt
```

若当前入口还不能生成全部资产，缺失项必须标记：

```text
ASSET_MISSING
```

不能 PASS。

## FastMatch 单图参数

必须显式记录：

```text
learn_roi_x
learn_roi_y
learn_roi_w
learn_roi_h

search_roi_x
search_roi_y
search_roi_w
search_roi_h

threshold
linegap
min_score
find_num
method/profile，如适用
```

结果必须记录：

```text
model_available
model_point_count
candidate_count
best_index
best_score
best_x
best_y
has_result_box
failure_stage
elapsed_ms
timeout
```

无真实 model/candidate/result package 时：

```text
FASTMATCH_PENDING_RESULT
```

禁止 PASS。

---

# 十、T4 Suite dry-run

命令模板：

```powershell
<BINARY> `
  --suite `
  --cxscript-suite <SUITE_PATH> `
  --image-manifest <MANIFEST> `
  --catalog <CATALOG_PATH> `
  --out <RUN_ROOT>/suite/<RUN_ID> `
  --suite-dry-run `
  --unified-log <SHARED_LOG>
```

dry-run 必须验证完整证据链：

```text
script
image
target
ROI
parameter profile
contract
output path
tool type
```

缺少任意一项立即停止。

结论：

```text
SUITE_DRY_RUN_PASS
SUITE_DRY_RUN_FAIL
```

dry-run 通过不代表算法通过。

---

# 十一、人工参数回归 L1/L2/L3

## L1：单图、单 target、单候选

输入：

```text
accepted manual gauge
一个 candidate
一张图
一个 target
一个 direct script
一个 contract
max_case_seconds
```

必须验证：

```text
Gauge 已人工接受
candidate 参数完整
只运行一次
未超时
五项强制证据存在
result_summary 可读
人工 review 可保存
promotion_allowed=false
```

输出目录：

```text
<RUN_ROOT>/param_regression/<RUN_ID>/L1/<candidate_id>/<case_id>/
```

结论：

```text
L1_EXECUTION_PASS
L1_EXECUTION_FAIL
L1_HUMAN_ACCEPTED
L1_HUMAN_REJECTED
L1_PENDING_HUMAN_REVIEW
```

注意：`L1_EXECUTION_PASS` 不等于 `L1_HUMAN_ACCEPTED`。

## L2：3～5 张图 mini-regression

每个候选必须运行相同工具的少量代表图：

```text
高对比
低对比
正常样本
轻微位移/旋转
弱边界或困难样本
```

输出：

```text
candidate_case_matrix.json
candidate_case_matrix.md
failure_classification.md
human_review_matrix.json
best_detection_gallery.md
```

结论：

```text
L2_EXECUTION_COMPLETE
L2_CONTRACT_PASS
L2_CONTRACT_FAIL
L2_PENDING_HUMAN_REVIEW
L2_HUMAN_ACCEPTED
L2_HUMAN_REJECTED
```

## L3：稳定性复测

测试：

```text
重复运行
ROI 小偏移
threshold 相邻值
gap/linegap 相邻值
FastMatch min_score 相邻值
FastMatch search ROI 小偏移
```

输出：

```text
stability_matrix.json
stability_report.md
timeout_report.md
result_variation.json
human_review.json
```

结论：

```text
L3_STABILITY_PASS
L3_STABILITY_FAIL
L3_PENDING_HUMAN_REVIEW
L3_HUMAN_ACCEPTED
L3_HUMAN_REJECTED
```

---

# 十二、参数回归预算保护

每个参数任务必须定义：

```text
max_candidates
max_case_seconds
max_total_seconds
```

推荐第一阶段：

```text
L1:
  max_candidates = 1
  max_case_seconds = 10
  max_total_seconds = 15

L2:
  max_candidates = 3
  max_case_seconds = 10
  max_total_seconds = 150

L3:
  max_candidates = 3
  max_case_seconds = 10
  max_total_seconds = 180
```

FastMatch 初始 smoke 可按实际算法耗时调整，但必须显式记录。

超过预算：

```text
timeout=true
failure_stage=algorithm_budget_exceeded
```

不得自动重试无限次。

## Findcircle 特殊保护

必须限制：

```text
最大扫描线数
最大采样点数
最大 measure 时间
最大 fit 时间
```

## FastMatch 特殊保护

必须限制：

```text
learn 最大时间
match 最大时间
最大 probe 数
最大 candidate 数
最大 result 数
搜索 ROI 像素面积
```

---

# 十三、AI 辅导轮测试规则

AI 接入前：

```text
AI Rank = pending_binding
AI Optimize = pending_binding
```

禁止固定参数冒充 AI 输出。

AI 只允许输出建议：

```text
candidate parameters
evidence references
expected effect
risk
failure class
source/provenance
```

AI 不得：

```text
自动写回 Gauge
自动运行长链
自动判 PASS
自动修改 contract
自动 promote profile
```

AI suggestion 必须由人工点击：

```text
Add To Human Candidates
```

然后重新走：

```text
L1 → L2 → L3 → human review
```

AI 接口测试结论：

```text
AI_ADVISOR_INTERFACE_PASS
AI_ADVISOR_PENDING_BINDING
AI_SUGGESTION_GENERATED
AI_SUGGESTION_HUMAN_SELECTED
AI_SUGGESTION_REJECTED
```

不得使用：

```text
AI_OPTIMIZATION_PASS
```

除非存在真实模型/优化器输出和完整证据。

---

# 十四、人工 GUI 验收

自动测试完成后，线程必须给出人工步骤，不得只说“请人工测试”。

FastMatch 人工验收步骤：

1. 启动 `<BINARY>`。
2. 加载测试图片。
3. 选择 FastMatch frozen direct script。
4. 确认 Image View 显示 learn ROI。
5. 拖动 learn ROI 中心，确认整体平移。
6. 拖动 learn ROI 角点，确认仅对应长宽变化。
7. 确认 search ROI 可独立拖动。
8. 执行 learn，确认 model points 可见。
9. 执行 match，确认 candidate centers、result boxes、best result 可见。
10. 修改 learn ROI，确认旧 model/result 标记 stale。
11. 修改 search ROI，确认旧 match result 标记 stale。
12. 重新运行，确认 stale 清除。
13. 检查关键参数 UI 与当前对象同步。
14. 检查 result summary 与画面一致。
15. 保存人工 review。

人工结论：

```text
MANUAL_GUI_PASS
MANUAL_GUI_FAIL
MANUAL_GUI_PARTIAL
MANUAL_GUI_NOT_RUN
```

`MANUAL_GUI_PASS` 必须由人工明确反馈，自动线程不得自行填写。

---

# 十五、统一日志规则

所有自动测试使用：

```powershell
--unified-log <SHARED_LOG>
```

日志必须增量追加。

每次运行至少包含：

```text
run_start
suite_begin
case_begin
pointer/algorithm events
case_end
suite_end
run_end
exit_code
elapsed_ms
conclusion
```

上下文：

```text
run_id
suite_id
case_id
image_id
target_id
script_id
candidate_id
tool
object_ref
shape_ref
```

禁止：

```text
每帧无事件时记录 pointer_begin
记录敏感 token/password
截断共享日志
多个线程无锁写入
```

日志写失败：

```text
LOG_WRITE_FAIL
```

不得忽略后继续宣称完整验收通过。

---

# 十六、测试报告结论格式

每个线程最终必须按以下格式报告：

```markdown
## Environment

- Repo:
- Build Dir:
- Binary:
- Binary Timestamp:
- Working Directory:
- Run ID:
- Unified Log:

## Compile

- Command:
- Exit Code:
- Conclusion: COMPILE_PASS / COMPILE_FAIL / NOT_RUN

## Tests

| Level | Suite | Cases | Executed | Pass | Fail | Pending | Conclusion |
|---|---|---:|---:|---:|---:|---:|---|

## Required Assets

| Asset | Exists | Path |
|---|---|---|

## Runtime Results

- Tool:
- Script:
- Image:
- Target:
- Parameters:
- Result metrics:
- Timeout:
- Failure stage:

## Human Review

- Required:
- Performed:
- Decision:
- Reason:

## Final Conclusion

- Code:
- Reason:
- Remaining blocker:
- Next allowed step:
```

---

# 十七、允许使用的最终结论

只允许使用精确分层结论：

```text
COMPILE_PASS
ASSET_PREFLIGHT_PASS
SHAPE_PROJECTION_PASS
GUI_POINTER_PASS
HEADLESS_EXECUTION_PASS
SUITE_DRY_RUN_PASS
CONTRACT_PASS
L1_EXECUTION_PASS
L2_CONTRACT_PASS
L3_STABILITY_PASS
PENDING_HUMAN_REVIEW
MANUAL_GUI_PASS
PENDING_BINDING
BLOCKED_ENV
TIMEOUT
FAIL
```

## 禁止结论

禁止笼统写：

```text
全部完成
功能正常
算法通过
验收通过
PASS
```

除非明确说明是哪一层。

例如正确结论：

```text
FastMatch Shape projection: PASS
FastMatch GUI pointer automation: PASS
FastMatch real learn/match: PENDING
FastMatch contract: NOT RUN
Manual GUI review: NOT RUN
Overall: NOT ACCEPTED
```

---

# 十八、PASS 的最高规则

只有同时满足以下条件，工具才能给出阶段性 `ACCEPTED`：

```text
编译成功
最新二进制运行
dry-run 通过
真实 headless 执行完成
五项强制资产存在
result summary 有真实 runtime 结果
contract cxscript 通过
没有伪造 PASS
没有 timeout
人工要求的 review 已完成
统一日志完整
报告来自本轮 RUN_ID
```

任何一项缺失，只能给出：

```text
PARTIAL
PENDING
BLOCKED
FAIL
```

不得给出最终 PASS。


基本规则：
- 根据提供方法修改代码，生成，调试
- 不扩展搜索不直接相关文件
- 不上网找关联资料
- 不网络传数据和文件
- 下载必须经过允许和确认
- 安装新软件必须经过允许  　
- 修改任何muParser*.h,muParser*.cpp有关的代码必须给出原因，且请求和提示
- 错误删除代码，或丢失文件，提示出来，不要弥补修复，让人工处理
- cxscript 只用于测试组织、工具调用、Catalog/Suite/Contract 描述和结果判断
- 不要把 cxscript 当成完整 C++ 使用。优先保证脚本稳定、可读、可调试、可复现
C++ 只负责通用能力：
- 加载 catalog / suite / image manifest
- 执行 cxscript
- 注入 global 输入
- 读取 result_summary / snapshot / overlay
- 导出 report / tool_display / best_examples
- 提供最小 case / contract / file 查询接口
业务判断、OK/NG 判断、Contract 判断、Suite 组织、Catalog 显示规则，优先放在 cxscript 中。
　
当前只允许：
* 对象声明
* `int` / `double`
* 简单赋值
* 已注册对象方法调用
* `global.xxx` 输入输出
* 简单 `if (condition) { ... }`
* `contract.reset / fail / pass / failed / setstatus / setconclusion`
\* `return;`：立即结束当前 cxscript 执行
禁止使用：
auto
std::vector
std::map
new/delete
lambda
template
namespace
class / struct 定义
for / while
return 1;
return x;
return object;
switch
else if
复杂 && / ||
对象返回赋值
数组字面量
文件 IO
OpenCV 代码

复杂判断必须拆成多个简单 `if`。
return; 结束整份当前脚本，不只是退出 if 块。不表示 C++ 函数返回。不产生返回对象。不支持对象返回赋值。



当前项目已经完成大量框架扩展，但开发过程中出现过以下问题：

- 已验证算法在新框架接入后发生回归；
- Manual、Suite、Headless、参数回归形成不同执行路径；
- 为解决单个问题不断增加兼容、Fallback、UI 和状态；
- 框架持续扩张，但基础算法链路反复回跳；
- 固定的 L1/L2/L3 测试区域被重新解释或绕开；
- 参数调整掩盖了真实的集成差异；
- 新工具接入时重复建设执行链和显示链。

后续开发必须先固化标准链路，再在标准链路上扩展工具。

---

# 2. 项目当前主目标

当前主目标不是继续增加新的独立功能，而是建立并固化一套可复用的标准开发链。

必须固化以下三条链路：

1. **CxScript 脚本载入到算法运行链路**
2. **ManualStateTestConsole 界面仿真、参数编辑、自调参到算法运行链路**
3. **算法运行结果到 Image View、ToolDisplay、Evidence 和结论 UI 的显示链路**

三条链路必须共用同一个算法执行核心，不允许各自形成独立实现。

标准结构：

CxScript / Manual UI / Suite / Param Regression
                    │
                    ▼
          Unified Execution Request
                    │
                    ▼
            Unified Execution Core
                    │
                    ▼
           Tool Runtime Adapter
                    │
                    ▼
 Findline / Findcircle / FastMatch / torch / mlpack
                    │
                    ▼
           Unified Execution Result
                    │
        ┌───────────┴───────────┐
        ▼                       ▼
 Result Projector         Evidence Package
        ▼                       ▼
 Image View/UI       Trace/Replay/Contract/Review


---

# 3. 最高优先级原则

## 3.1 一个执行核心

以下入口允许存在：

* ManualStateTestConsole
* CxScriptSuiteRunner
* CxScriptHeadlessRunner
* CxParamProbeRunner
* Semantic Flow Graph
* CLI

但所有入口最终必须进入同一个执行核心。

推荐统一入口：

CxExecutionResult ExecuteCxScriptTask(
    const CxExecutionRequest& request);


或：

CxExecutionEngine::Execute(
    const CxExecutionRequest& request,
    CxExecutionResult& result);


禁止在不同入口中分别实现：

* 图像输入；
* global 注入；
* Tool 对象创建；
* `measure()`；
* `fitline()` / `fitcircle()`；
* Runtime Result Capture；
* Overlay 生成；
* 成功失败判断。

---

## 3.2 已验证算法优先于新框架解释

Findline、Findcircle 等算法在早期版本中已经通过基础验证。

出现以下情况时：

相同图片
相同 ROI
相同参数
相同调用顺序
当前版本结果与基线不同


必须将问题视为 **集成回归**，而不是重新调参或重新优化算法。

处理顺序必须是：

输入差分
→ 参数差分
→ 调用顺序差分
→ 对象状态差分
→ 算法分支差分
→ 找到第一个差异点


不得先通过改变阈值、Gap、FilterProfile 或增加 Fallback 让 case 临时通过。

---

## 3.3 固定 L1/L2/L3 区域不得漂移

L1/L2/L3 是预先确定的验证区域，不是临时探索区域。

固定资产至少包括：

* 图片路径与图片 Hash；
* `case_id`；
* `image_id`；
* `target_id`；
* Gauge/ROI；
* 参数 Profile；
* CxScript Snapshot；
* 预期算法分支；
* 预期测量点摘要；
* 预期拟合结果；
* Result Overlay；
* ToolDisplay；
* Contract。

不允许因为当前实现失败而：

* 换区域；
* 缩小到更容易成功的目标；
* 修改预期结论；
* 将原有失败解释为测试数据问题；
* 用新候选替代原基线后宣布通过。

---

# 4. 标准数据契约

所有入口和工具必须使用以下公共契约。

## 4.1 CxExecutionRequest

struct CxExecutionRequest
{
    std::string run_id;
    std::string case_id;
    std::string image_id;
    std::string target_id;

    std::string tool;
    std::string script_path;

    CxImageInput image;
    CxGaugeSnapshot gauge;
    CxParameterSnapshot parameters;

    CxExecutionBudget budget;
    CxContractSpec contract;
};


Request 必须完整记录：

* 本次使用的图片；
* 本次使用的 Gauge；
* 本次使用的参数；
* 本次运行的脚本；
* 本次预算和超时；
* 本次 Contract。

不得从多个隐式状态源拼装本次执行。

---

## 4.2 CxExecutionResult
struct CxExecutionResult
{
    std::string run_id;
    std::string case_id;
    std::string tool;
    std::string object_name;

    bool executed = false;
    bool geometry_available = false;
    bool fit_available = false;

    std::string status;
    std::string failure_stage;
    std::string reason;

    CxGaugeSnapshot input_gauge;
    CxParameterSnapshot input_parameters;

    std::vector<CxPoint2D> measure_points;
    std::vector<CxPoint2D> valid_points;
    std::vector<CxPoint2D> rejected_points;

    std::optional<CxLineResult> line_result;
    std::optional<CxCircleResult> circle_result;
    std::optional<CxMatchResult> match_result;
    std::optional<CxInferenceResult> inference_result;

    CxMetricSummary metrics;
    CxExecutionTraceSummary trace;
};


所有显示、报告、参数回归和 Contract 必须读取同一个 `CxExecutionResult`。

禁止：

Manual Console 从 RuntimeObjectView 取一套结果
SuiteRunner 从 result_summary 取一套结果
ParamProbeRunner 从 options 取一套结果
ToolDisplay 直接从算法对象取一套结果


---

## 4.3 CxEvidencePackage

struct CxEvidencePackage
{
    std::string snapshot_path;
    std::string summary_path;
    std::string result_overlay_path;
    std::string evidence_overlay_path;
    std::string tool_display_path;
    std::string trace_path;
    std::string replay_package_path;
    std::string contract_result_path;
};


所有执行模式必须遵守同一套证据产物契约。

---

## 4.4 CxReviewDecision

struct CxReviewDecision
{
    std::string run_id;
    std::string decision;
    std::string reason;
    std::string reviewer;
};


允许的人工决策：

accept
reject_gauge
reject_parameter
reject_algorithm
reject_overlay
derive_profile
stop


机器结果与人工结果必须分开记录。

---

# 5. 三条标准链路

## 5.1 链路一：CxScript 到算法运行

标准路径：
Catalog / Case
→ CxScript
→ cxparser / cxparser_ext
→ registered tool object
→ image / gauge / parameter injection
→ algorithm execution
→ CxExecutionResult


CxScript 负责：

* 对象创建；
* 参数设置；
* 调用顺序；
* 结果绑定；
* Contract 调用；
* 业务状态。

C++ Tool 模块负责：

* 边缘搜索；
* 测量点生成；
* 连通域处理；
* 拟合；
* 模型推理；
* 优化算法内部实现。

UI、SuiteRunner 和 HeadlessRunner 不得重新实现算法。

---

## 5.2 链路二：界面仿真和参数自调参

ManualStateTestConsole 标准路径：

Evidence Case
→ Load Image / Script / Profile / Gauge
→ Human Edit Gauge
→ Human Edit Parameters
→ Build CxExecutionRequest
→ Unified Execution Core
→ CxExecutionResult
→ UI Review


参数回归标准路径：

Manual Accepted Gauge
→ Parameter Range
→ Candidate Generator
→ Build CxExecutionRequest
→ Unified Execution Core
→ CxExecutionResult
→ EvalRecord
→ Candidate Comparison

人工运行和自动调参的区别只能是：

* 谁生成参数；
* 谁选择下一组参数；
* 谁决定停止。

算法执行过程必须完全一致。

---

## 5.3 链路三：算法结果到界面显示

标准路径：

CxExecutionResult
→ CxResultProjector
→ OverlayElement
→ Image View
→ ToolDisplay
→ Evidence UI
→ Conclusion UI


UI 不得直接读取 Findline、Findcircle、FastMatch 内部成员并自行判断含义。

统一 Overlay Role：

enum class CxOverlayRole
{
    InputGauge,
    MeasurePoint,
    RejectedPoint,
    FitResult,
    ReferenceGeometry,
    MatchRegion,
    ModelOutput,
    Anomaly,
    HumanAnnotation
};


Overlay 至少携带：

run_id
case_id
tool
role
source
gauge_revision
geometry
status

---

# 6. Findline 标准参考实现

Findline 是第一套标准工具接入模板。

在扩展其它工具前，Findline 必须完整闭合以下链路。

## 6.1 脚本链

Script
→ globals
→ Findline.setline
→ parameter methods
→ measure
→ fitline
→ get_result
→ CxExecutionResult


## 6.2 界面链

拖动 Line Gauge
→ ManualGaugeState
→ CxExecutionRequest
→ Unified Execution Core
→ CxExecutionResult


## 6.3 显示链

CxExecutionResult
→ Input Gauge
→ Measure Points
→ Rejected Points
→ Fit Line
→ Metrics
→ Image View / ToolDisplay


## 6.4 证据链

必须生成：

snapshot
result_summary
result_overlay
evidence_overlay
tool_display
run_trace
replay_package
contract_result
human_review
　

## 6.5 固定回归链

Findline 必须对预定 L1/L2/L3 固定区域进行回归。

不得只通过 L0 或单张容易图像后宣布标准链完成。

---

# 7. 新工具接入规则

新增工具必须按 Findline 标准模板接入。

允许新增：

* Tool Runtime Adapter；
* CxScript；
* Parameter Spec；
* Result 扩展；
* Result Projector；
* Contract；
* 固定测试 Case；
* 参数搜索策略适配。

不允许新增：

* 第二套执行器；
* 第二套 Request；
* 第二套 Result；
* 第二套 Evidence 格式；
* 第二套参数状态；
* 第二套 Image View；
* 第二套成功失败语义。

---

## 7.1 Findcircle / Findellipse / FindRect

新增：

Gauge 类型
Tool Adapter
CxScript
Result 类型
Result Projector
Contract
固定回归 Case
　

执行核心、Evidence 和 UI 框架不变。

---

## 7.2 FastMatch

标准路径：

CxExecutionRequest
→ FastMatchRuntimeAdapter
→ FastMatch
→ CxExecutionResult.match_result
→ MatchResultProjector
　

Match Result 可包含：

template_id
search_roi
score
angle
scale
matched_region
candidate_matches
　

不得在 ManualStateTestConsole 中直接实现 FastMatch 调用逻辑。

---

## 7.3 torch

标准路径：

CxScript
→ TorchRuntimeAdapter
→ Model / Tensor / Infer
→ CxExecutionResult.inference_result
→ Result Projector
　

UI 只显示标准结果：
　
class_id
confidence
mask
bounding_box
feature_summary
　

torch 内部推理不得直接侵入 ManualStateTestConsole。

---

## 7.4 mlpack

mlpack 可用于：

* 参数候选质量预测；
* 失败类型分类；
* 候选排序；
* 基础分类和回归。

mlpack 不允许：

* 直接修改 Product Default；
* 绕过 Unified Execution Core 运行 Findline；
* 自行生成另一套结果结构；
* 将预测结果冒充真实 Probe 结果。

---

## 7.5 ensmallen

ensmallen 是搜索策略，不是算法执行路径。

标准循环：
　
Current EvalRecords
→ Objective Function
→ ensmallen proposes parameters
→ CxExecutionRequest
→ Unified Execution Core
→ CxExecutionResult
→ EvalRecord
　

ensmallen 只控制：

* 下一组参数；
* 搜索方向；
* 是否停止；
* 候选排序。

ensmallen 不控制：

* 图像如何输入；
* Tool 如何执行；
* 结果如何显示；
* Contract 如何判断；
* Profile 是否提升。

---

# 8. 参数搜索策略规则

允许逐步实现：
Manual Seed
Grid Search
Coarse-to-Fine
Historical Best
Random/LHS Candidate
mlpack Rank
ensmallen Optimize
　

每个 Candidate 都必须走完整链：
Candidate
→ CxExecutionRequest
→ Execute
→ CxExecutionResult
→ Evidence
→ EvalRecord
　

禁止为了搜索速度直接调用简化版算法函数。

禁止只根据以下单个指标选最佳参数：

* 点数最多；
* Fit 成功；
* Contract Pass；
* 单图准确率；
* 单次最低误差。

参数评估至少需要：

　
Geometry Pass
Evidence Pass
Human Accept
Hit Distribution
Stability
Timeout
Failure Stage
False Fit Risk
　

# 9. Gauge 状态规则

Gauge 唯一状态主链：

　
Manifest / Replay / Annotation
→ ManualGaugeState
→ Human Edit
→ CxGaugeSnapshot
→ CxExecutionRequest
→ CxExecutionResult.input_gauge
→ ToolDisplay / Result Summary
　

不得同时使用多个 Gauge 来源运行同一任务。

建议每次 Gauge 更新生成：

gauge_revision
gauge_source
gauge_hash
　

以下位置必须记录相同 revision：

* ManualStateTestConsole；
* CxExecutionRequest；
* CxExecutionResult；
* Snapshot；
* Result Summary；
* ToolDisplay；
* Replay Package。
　

# 10. 基线和回归规则

## 10.1 基线不随开发漂移

每个固定 case 必须保存：

image_hash
gauge_snapshot
parameter_snapshot
script_snapshot
execution_branch
point_summary
fit_summary
artifact_paths
　
## 10.2 发现差异时先做差分

必须依次比较：

1. Mat rows/cols/type/channels/hash
2. Gauge 最终坐标
3. 参数最终值
4. 参数方法调用顺序
5. measure 前对象状态
6. 算法实际分支
7. 中间点和组件统计
8. 第一个产生差异的函数
　

## 10.3 禁止掩盖回归

未经明确批准，不得：

* 放宽 threshold；
* 修改 filter profile；
* 增加 fallback；
* 用临时点替代算法点；
* 生成假拟合；
* 把 Evidence 点当算法结果；
* 修改测试期望；
* 更换固定 ROI；
* 将失败 case 从回归集中删除。
　

# 11. Manual、Suite、Headless 的职责边界

## ManualStateTestConsole

负责：

* Case 选择；
* Image/Gauge/Parameter 编辑；
* 单步、断点、前缀运行；
* Runtime 观察；
* 人工审核；
* Replay。

不负责独立算法执行语义。

## CxScriptSuiteRunner

负责：

* Manifest；
* Suite；
* Contract；
* 批量固定回归；
* 报告；
* Review Gate。

不负责重新定义 Tool 执行逻辑。

## CxScriptHeadlessRunner

负责：

* 单脚本；
* 单 Candidate；
* 无界面执行；
* 标准 Artifact 输出。

不负责另一套 Result Capture。

## CxParamProbeRunner

负责：

* 将 Candidate 转换成 Request；
* 调用统一执行核心；
* 将 Result 转换成 EvalRecord。

不允许从输入 Options 读取“运行结果”。
　

# 12. 代码修改流程

任何代理开始修改前，必须执行以下步骤。

## 12.1 修改前

1. 明确当前任务属于哪条标准链。
2. 列出当前输入和预期输出。
3. 确认是否已经存在对应公共接口。
4. 确认修改不会建立第二套执行路径。
5. 确认固定回归 case。
6. 记录当前基线结果。

## 12.2 修改中

1. 优先修复已有链路，不新增平行实现。
2. 缺口属于哪个模块，就在对应模块实现。
3. UI 文件不得承载算法实现。
4. Runtime Adapter 不得承载业务判断。
5. Result Projector 不得修改算法结果。
6. Param Strategy 不得直接修改 Product Default。
7. 每完成一个节点立即编译和运行固定 case。

## 12.3 修改后

必须报告：

修改文件
修改原因
影响链路
是否改变公共契约
是否改变算法
是否改变参数默认值
是否增加 fallback
固定 case 结果
与基线的差异
剩余问题
　

---

# 13. 验收标准

不能以“编译通过”作为完成标准。

一个工具链完成必须满足：

1. CxScript 可以运行。
2. Manual UI 使用同一执行核心。
3. Headless 使用同一执行核心。
4. Suite 使用同一执行核心。
5. 参数 Candidate 使用同一执行核心。
6. Result 使用统一结构。
7. Overlay 使用统一 Projector。
8. Evidence 产物完整。
9. Replay 可以复现。
10. 固定 L1/L2/L3 回归通过或明确记录真实差异。
　

任何一个入口产生与其它入口不同的结果，任务不能标记完成。

　
# 14. 禁止事项

严禁以下行为：
为快速打通而在 UI 中直接实现算法
为单 case 通过而修改算法默认参数
在多个 Runner 中复制执行逻辑
用 Evidence/Fallback 结果伪装原算法成功
自动删除失败 case
自动修改 Product Default
自动提升参数 Profile
无人工审核直接进入 Promotion
把 Placeholder 结果写成真实模型输出
把已编译写成已完成
把数据结构存在写成功能可用
　

# 15. 当前开发优先级

当前推荐顺序：
　
1. Findline 三条链路固化
2. 固定 L1/L2/L3 基线回归
3. Findcircle 按标准模板接入
4. FastMatch 按标准模板接入
5. 真实 Param Probe
6. Hit Distribution
7. Accuracy / Stability
8. torch Tool Adapter
9. mlpack Rank
10. ensmallen Objective
11. Mini Regression
12. Profile Promotion
　

禁止在 Findline 标准链未固化前，再建立新的平行框架。

　

# 16. 任务状态用语

所有报告使用以下状态：
　
[Verified]
已通过实际运行和固定回归验证。

[Implemented]
核心代码已实现，但尚未完成全部入口验收。

[Partial]
部分路径可运行，仍有明确缺口。

[Scaffold]
接口和文件存在，但核心逻辑为空或固定返回。

[Placeholder]
结果是规则值、模拟值或报告占位。

[Disabled]
代码存在但默认关闭。

[Legacy]
旧实现，仅保留兼容。

[Planned]
尚未进入代码。
　

禁止使用模糊表述：
　
基本完成
大体可用
应该正常
框架已经打通
后续再验证
　

必须指出具体通过了哪个入口、哪个 case、哪个产物。

---

# 17. 代理响应格式

每次任务结束，代理必须按以下格式汇报：

```markdown
## 本次目标

## 修改文件

## 固化或修复的链路

## 未修改内容

## 编译结果

## 固定 Case 测试结果

## 与基线的差异

## 证据产物

## 当前状态
[Verified / Implemented / Partial / Scaffold / Placeholder]

## 下一步唯一任务
```

“下一步唯一任务”最多一个，不得同时扩展多个方向。

---

# 18. 最终原则

本项目后续开发遵循：

　
先固化链路
再扩展工具

先恢复基线
再优化参数

先统一执行
再增加入口

先产生真实结果
再训练模型

先完成人工审核
再允许 Promotion
　

任何修改都不得以牺牲原有已验证算法行为为代价。

任何新工具都必须按 Findline 标准参考实现接入。

任何参数搜索、mlpack 或 ensmallen 结果都必须通过同一个真实算法执行链验证。




＃＃当前 cxvision_imgui_acceptance 的统一运行/调试日志主文件是：

D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxscript_runs\_shared\cxvision_imgui_acceptance.jsonl

后续所有 Headless、Suite、Shape、GUI Pointer、参数回归等自动测试，都应统一追加写入这个 JSONL 文件，不允许截断或覆盖。

分析运行问题时，优先查看 shared jsonl 的最后 50～200 行，重点看：
run_start
suite_begin
case_begin
script_id
image_id
target_id
tool
shape / pointer / algorithm event
case_end
suite_end
run_end
exit_code
conclusion
elapsed_ms
reason

如果 shared jsonl 中有 run_start 但没有 run_end，说明进程可能中途异常退出、崩溃、超时或被杀掉。
如果有 case_begin 但没有 case_end，优先定位最后一个 case_id / script_id / tool / image_id。
如果有 pointer 或 drag 事件但没有 commit/release，优先排查 GUI 交互链。
如果有 algorithm begin 但没有 algorithm end，优先排查算法超时、死循环、预算保护或工具内部异常。
如果有 LOG_WRITE_FAIL，说明日志本身不完整，不能只凭 shared jsonl 下结论。

崩溃分析不能只依赖 shared jsonl。必须同时收集：
1. shared jsonl 最后 50～200 行
2. 当前 case 输出目录
3. case 下的 log.txt
4. result_summary.json
5. snapshot.txt
6. variable_snapshot.json
7. object_state.json
8. overlay_validation.json，如存在
9. 启动命令
10. 崩溃时间点
11. 是否生成 case_end / run_end
12. Visual Studio 输出窗口或调用堆栈，如有

结论规则：
- shared jsonl 有完整 run_end 且 exit_code=0，只能说明该运行链路正常结束。
- shared jsonl 没有 run_end，不能说测试通过。
- 自动日志通过，不等于人工 GUI 验收通过。
- Shape/Pointer 测试通过，不等于真实算法通过。
- Headless 执行通过，不等于 Contract 通过。
- Contract 通过，不等于人工接受。
- 没有人工确认时，结论最多是 PENDING_HUMAN_REVIEW，不能写 ACCEPTED。

推荐排查顺序：
1. 先看 shared jsonl 是否有 run_start / run_end。
2. 再看最后一个 case_begin 对应的 case_id。
3. 查该 case 输出目录是否完整。
4. 查 result_summary.json 的 executed、failure_stage、reason、elapsed_ms、timeout、valid_points_count、has_fit_line / has_fit_circle。
5. 查 overlay_validation.json，确认 saved image 是否真的画出了 ROI、Gauge、测量点、拟合结果。
6. 如果日志在算法中断，优先检查预算参数：max_elapsed_ms、max_scan_lines、max_samples、timeout-sec。
7. 如果日志在 GUI/drag 中断，优先检查 pointer frame、hit_test、selected_ref、commit_result。
8. 如果 shared jsonl 没任何有效记录，说明崩溃可能发生在日志初始化前，需要看 VS/Windows 崩溃信息。

新线程处理问题时，必须在报告中写清楚：
- 使用的二进制路径
- 二进制时间戳
- 工作目录
- 完整命令行
- run_id
- shared log 路径
- case 输出目录
- 最后一个日志事件
- 是否有 run_end
- 是否有 case_end
- 最终分层结论

允许的结论示例：
COMPILE_PASS
HEADLESS_EXECUTION_PASS
GUI_POINTER_PASS
CONTRACT_PASS
TIMEOUT
FAIL
PENDING_HUMAN_REVIEW
BLOCKED_ENV

禁止直接写：
PASS
全部完成
功能正常
验收通过
算法通过


＃＃脚本加入 CxScript Templates 列表的方式：

1. 不允许为了新增脚本显示而修改 ViewController / ManualStateTestConsole / Catalog UI C++ 代码。
2. 只允许通过新增或拷贝 .cxsc / .cxs 文件到以下可扫描目录：
   - cxparser/cxscript/module/...
   - cxparser/cxscript/integration/...

3. 如果希望默认不勾选 “Show all catalog scripts” 时也能显示，脚本文件名必须包含以下任一标识：
   - direct_test
   - _direct
   - _smoke
   或放在：
   - cxparser/cxscript/module/ **/headless/

4. 推荐人工测试脚本放置路径：
   - cxparser/cxscript/module/cximage/manual/
   - cxparser/cxscript/module/cximage/headless/
   - cxparser/cxscript/module/cximage/diagnostic/<tool>/

5. 脚本命名建议：
   - fastmatch_l1_direct.cxsc
   - fastmatch_t3_smoke.cxsc
   - findrect_l1_direct.cxsc
   - findellipse_l1_direct.cxsc
   - findline_l1_direct.cxsc
   - findcircle_l1_direct.cxsc

6. 禁止通过修改 C++ 默认选中项、硬编码脚本路径、增加特殊列表项来暴露脚本。
7. 禁止把 build/Release 目录里的脚本当源码修改。
8. 如果脚本需要进入 suite/catalog/contract 体系，再单独修改 cximage_catalog.cxsc 或 suite cxsc；但单纯人工手测只需要拷贝脚本文件。
9. 新脚本必须使用 CxScript 语句级风格，不允许 JSON/YAML/step 表格/method_input/method_output。
10. 新脚本外部输入统一使用 global_ 前缀，不允许 global.xxx。


＃＃g_pbackimage 设计是 ImageManager 初始化出来的算法工作底图
1. 输入图只读。
2. BackImage 只从 ImageManager 获取。
3. measure() 内禁止 g_pbackimage = 输入图。
4. BackImage 尺寸不够时，由 ImageManager::EnsureAlgorithmRuntimeResources(width, height) 扩容。
5. 如果 BackImage 不存在、为空、或等于输入图，直接失败返回，不继续算法。


### Headless Options / Result 结构约束

`CxScriptHeadlessOptions` 只允许保存 Headless 执行入口元信息，不允许继续扩展为工具参数大表。

允许字段：

- case_id
- script_path
- image_path
- template_image_path
- output_dir
- globals_path
- manifest_path
- image_id
- target_id
- timeout_sec
- max_steps
- contract_context_enabled
- runtime_capture_smoke
- cli_global_overrides

禁止新增以下类型字段：

- global_roi_x0 / roi_x0
- threshold / method / gap / linegap
- circle_cx / circle_px
- learn_roi_x / search_roi_x
- torch input size / mean / std
- mlpack feature size / threshold
- ensmallen optimizer 参数
- 任意具体工具参数

工具参数必须通过以下方式进入 cxscript：

```text
headless_globals.cxsc 声明变量
globals value 文件 / manifest / suite / CLI override 提供值
C++ 循环绑定变量
cxscript 直接读取 global_xxx
