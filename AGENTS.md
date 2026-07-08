\# Codex Project Instructions



\## Codebase Memory Usage



Before analyzing, modifying, or explaining this repository, prefer using the `codebase-memory` MCP server when the task involves code structure, call chains, symbol relationships, module boundaries, impact analysis, architecture overview, or locating implementation files.



Use codebase-memory-mcp before broad grep/read-file exploration for:

\- finding functions/classes/modules

\- tracing caller/callee relationships

\- understanding CMake/module dependencies

\- checking impact before editing

\- locating stale/deprecated files

\- comparing cxparser / cxscript / cximage / libtorch\_module responsibilities

\- identifying where a feature belongs architecturally



Do not rely only on text search when the task is architectural or cross-file. First query the code graph, then read the specific source files.



\## Architecture Constraints



This repository must preserve module boundaries.



Do not move algorithm/business logic into UI/debugger files for quick integration.

If a missing capability belongs to cxparser, cxscript, cxcore, cximage, or libtorch\_module, implement it in the corresponding module and expose only the minimal interface upward.



For measurement/gauge debugging:

\- preserve original tool semantics

\- verify ROI geometry, size, direction, rotation, and coordinate interpretation

\- prefer explicit visual/debug output over hidden assumptions



\## Current Project Focus



The current work focuses on:

\- cxscript semantic transformation

\- cxparser integration

\- cximage IMGUI debug/test interface

\- Gauge / Findline / Circle ROI visualization and reliability testing

\- Stage 2.5 image manifest and ROI verification

\- cleaning deprecated C++/header files from CMake only after dependency confirmation



Before modifying CMakeLists, first use codebase-memory to identify whether the file is referenced, included, registered, or reachable from current targets.




　

````md
# cxparser / cxscript Development Rules

## 1. 目标

当前 cxscript 只用于测试组织、工具调用、Catalog/Suite/Contract 描述和结果判断。  
不要把 cxscript 当成完整 C++ 使用。优先保证脚本稳定、可读、可调试、可复现。

C++ 只负责通用能力：

- 加载 catalog / suite / image manifest
- 执行 cxscript
- 注入 global 输入
- 读取 result_summary / snapshot / overlay
- 导出 report / tool_display / best_examples
- 提供最小 case / contract / file 查询接口

业务判断、OK/NG 判断、Contract 判断、Suite 组织、Catalog 显示规则，优先放在 cxscript 中。

---

## 2. cxscript 最小语法集

允许使用：

```cpp
Image m_image;
Findline m_line;
Findcircle m_circle;

int x0 = global.roi_x0;
double score = case.get_double("local_support");

m_line.setline(x0, y0, x1, y1, tool_half_width);
m_line.measure(&m_image);
m_line.fitline();

if (case.get_int("valid_points_count") < 2) {
    contract.fail("not enough points");
}
````

当前只允许：

* 对象声明
* `int` / `double`
* 简单赋值
* 已注册对象方法调用
* `global.xxx` 输入输出
* 简单 `if (condition) { ... }`
* `contract.reset / fail / pass / failed / setstatus / setconclusion`

禁止使用：

```text
auto
std::vector
std::map
new/delete
lambda
template
namespace
class / struct 定义
return
for / while
switch
else if
复杂 && / ||
对象返回赋值
数组字面量
文件 IO
OpenCV 代码
```

复杂判断必须拆成多个简单 `if`。

---

## 3. 目录职责

统一使用以下结构：

```text
cxparser/cxscript/module/cximage/

  catalog/
    cximage_catalog.cxsc

  frozen/
    findline/
    findcircle/

  diagnostic/
    fastmatch/

  stage25/
    suites/
    contracts/
    selectors/
```

职责划分：

```text
catalog/
  只登记可执行工具脚本，控制 GUI 可见性。

frozen/
  固化脚本，文件名必须带 ok 或 ng_expected。

diagnostic/
  诊断脚本，默认不进入普通 GUI 列表。

stage25/suites/
  只组织测试 case，不直接调用算法。

stage25/contracts/
  只判断 pass/fail。

stage25/selectors/
  后续用于 best case / gallery 选择。
```

---

## 4. GUI Script Catalog 规则

GUI 普通 Script Catalog 列表只能来自：

```text
cxparser/cxscript/module/cximage/catalog/cximage_catalog.cxsc
```

普通 GUI 列表只显示：

```text
frozen = 1
manual_visible = 1
expected_result in { ok, ng_expected }
```

禁止普通列表显示：

```text
*_test.cxsc
deprecated/*
draft/*
diagnostic/*
stage25/suites/*
stage25/contracts/*
stage25/selectors/*
torch/*
mlpack/*
ensmallen/*
```

诊断脚本只能：

```text
manual_visible = 0
advanced_visible = 1
```

GUI 需要提供 `Catalog Review` 面板，显示所有 catalog entry、hidden reason、contract_path 缺失警告，方便人工审核。

---

## 5. Catalog 规则

Catalog 只登记“可执行工具脚本”，不登记 suite / contract / selector。

每个 regression-visible 脚本必须设置 contract：

```cpp
CxScriptCatalog_addscript("findline_vertical_stage25_filter20_ok");
CxScriptCatalog_script_setlabel("[OK] Findline Vertical - Stage25 Filter20");
CxScriptCatalog_script_setpath("cxparser/cxscript/module/cximage/frozen/findline/findline_vertical_stage25_filter20_ok.cxsc");
CxScriptCatalog_script_settool("Findline");
CxScriptCatalog_script_setexpected("ok");
CxScriptCatalog_script_setexpectedpolicyguard("MEASURE_AND_FIT_AVAILABLE");
CxScriptCatalog_script_setcontract("cxparser/cxscript/module/cximage/stage25/contracts/findline_ok_contract.cxsc");
CxScriptCatalog_script_setfrozen(1);
CxScriptCatalog_script_setmanualvisible(1);
CxScriptCatalog_script_setregressionvisible(1);
```

固化脚本命名必须表达结论：

```text
*_ok.cxsc
*_ng_expected.cxsc
```

---

## 6. Suite 规则

Suite 只组织 case，不做算法，不做 contract 判断。

Suite case 必须使用：

```cpp
CxScriptSuite_addcase("L1_line_001_findline_ok");
CxScriptSuite_case_setscriptid("findline_vertical_stage25_filter20_ok");
CxScriptSuite_case_setimage("line_high_contrast_001");
CxScriptSuite_case_settarget("plate_top_edge");
CxScriptSuite_case_setlevel("L1_high_contrast");
CxScriptSuite_case_setexpected("ok");
CxScriptSuite_case_setexpectedpolicyguard("MEASURE_AND_FIT_AVAILABLE");
```

禁止在 suite 中写：

```text
Findline / Findcircle 对象声明
measure / fit 调用
contract.fail
contract.pass
```

---

## 7. 工具脚本规则

工具脚本只做：

```text
1. 从 global 读取图像和 ROI
2. 调用工具
3. measure / fit
4. 写 global.xxx_ref
5. 写 global.current_status
```

Findline 脚本必须 manifest-driven，不允许硬编码 ROI：

```cpp
Image m_image;
Findline m_line;

m_image.copyFromMat(global.matInput);

int x0 = global.roi_x0;
int y0 = global.roi_y0;
int x1 = global.roi_x1;
int y1 = global.roi_y1;

int tool_half_width = global.tool_half_width;
int wgap = global.wgap;
int hgap = global.hgap;
int linegap = global.linegap;
int threshold = global.threshold;
int method = global.method;

m_line.SetWHgap(wgap, hgap);
m_line.setlinegap(linegap);
m_line.setline(x0, y0, x1, y1, tool_half_width);
m_line.setmethod(method);
m_line.setthre(threshold);
m_line.setfilterprofile(1);

m_line.measure(&m_image);
m_line.fitline();

global.line_ref = m_line.get_result();
global.current_status = "geometry_result_available";
```

Findcircle 同理，从 `global.circle_cx/cy/px/py` 读取 ROI，不允许硬编码坐标。

---

## 8. Contract 规则

C++ 不允许实现 `EvaluateSuiteCaseContract`。
所有 OK/NG 判断必须由 contract cxscript 完成。

Contract 脚本必须以：

```cpp
contract.reset();
```

开始。

Findline OK 最小判断：

```cpp
contract.reset();

if (case.get_bool("headless_ok") == 0) {
    contract.fail("headless execution failed");
}

if (case.get_int("valid_points_count") < 2) {
    contract.fail("Findline OK requires valid_points_count >= 2");
}

if (case.get_bool("has_fit_line") == 0) {
    contract.fail("Findline OK requires has_fit_line == true");
}

if (case.string_equals("actual_policy_guard", "MEASURE_AND_FIT_AVAILABLE") == 0) {
    contract.fail("Findline OK requires MEASURE_AND_FIT_AVAILABLE");
}

if (case.file_exists("result_overlay_path") == 0) {
    contract.fail("missing result_overlay.png");
}

if (case.file_exists("tool_display_path") == 0) {
    contract.fail("missing tool_display.png");
}

if (contract.failed() == 1) {
    contract.setstatus("findline_ok_failed");
    contract.setconclusion("Findline OK failed.");
}

if (contract.failed() == 0) {
    contract.pass("Findline OK passed.");
    contract.setstatus("findline_ok_passed");
    contract.setconclusion("Findline OK: points, fitted line, overlay and tool display are available.");
}
```

原则：

```text
contract.fail(...) 一旦发生，ContractPass 必须为 no。
不允许 Points=0 / Fit=no 但 ContractPass=yes。
```

---

## 9. ROI 与测试图规则

13 张图统一以：

```text
D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/test_images/stage25_image_manifest.json
```

作为路径、尺寸、ROI、来源契约。

Suite case 必须通过：

```text
image_id + target_id
```

找到 ROI，然后由 C++ 注入：

```text
global.roi_x0 / roi_y0 / roi_x1 / roi_y1
global.circle_cx / circle_cy / circle_px / circle_py
global.tool_half_width
global.wgap / hgap / gap / linegap
global.threshold / method
```

工具脚本只能读取这些 global 字段。

---

## 10. 输出产物规则

每个 case 必须输出：

```text
snapshot.txt
result_summary.json
result_overlay.png
tool_display.png
```

应尽量输出：

```text
evidence_overlay.png
```

失败 case 也必须输出最小 `tool_display.png`，至少显示原图、ROI 和失败状态。

报告必须包含：

```text
suite_run_report.md
tool_display_index.md
findline_algorithm_iteration_report.md
findcircle_algorithm_iteration_report.md
failure_classification_report.md
best_detection_gallery.md
```

`tool_display.png` 必须二次绘制：

```text
绿色 ROI
红色测量点
黄色拟合线 / 拟合圆
PASS / FAIL 状态
```

不能只依赖旧 overlay。

---

## 11. C++ 允许新增的最小接口

如果 cxscript 缺能力，优先新增最小 binding：

```cpp
case.get_int("key")
case.get_bool("key")
case.get_double("key")
case.string_equals("key", "value")
case.file_exists("path_key")

contract.reset()
contract.fail("reason")
contract.pass("reason")
contract.failed()
contract.passed()
contract.setstatus("status")
contract.setconclusion("text")
```

不要为了测试方便在 C++ 中新增业务模型、工具判断器、参数策略器、CaseMatrix 或 BestCaseSelector。

---

## 12. 开发禁令

禁止：

```text
C++ hardcode 脚本列表
C++ hardcode Findline/Findcircle OK/NG 判断
C++ hardcode FastMatch formal gate
C++ hardcode Stage25 case matrix
GUI 递归扫描脚本作为普通列表
deprecated / diagnostic / draft 出现在普通列表
工具脚本硬编码 ROI
ContractPass 与几何结果矛盾
```
　