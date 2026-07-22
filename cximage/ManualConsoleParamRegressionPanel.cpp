#include "pch.h"
#include "ManualConsoleParamRegressionPanel.h"
#include "ManualConsoleUtils.h"
#include "ManualConsoleGauge.h"
#include "ManualConsoleScriptDebugPanel.h"
#include "CxParameterProfileRuntime.h"

#include <sstream>
#include <fstream>

CxParamRegressionTask BuildParamRegressionTaskFromManualGauge(
    const ManualTestContext& context)
{
    const ManualGaugeState& gauge = context.current_gauge;
    CxParamRegressionTask task;
    task.case_id = gauge.case_id.empty() ? "manual_case" : gauge.case_id;
    task.image_id = gauge.image_id;
    task.target_id = gauge.target_id;
    task.tool = gauge.tool.empty() ? "Findline" : gauge.tool;
    task.task_id = "param_regression_" + task.case_id;
    task.gauge_annotation_path =
        (ManualGaugeCaseDir(context) / "gauge_annotation.json").string();
    task.base_script_id = task.tool == "Findcircle"
        ? "findcircle_stage25_direct_ok"
        : "findline_stage25_filter20_ok";
    task.base_parameter_profile_id = "manual_gauge_seed";
    task.max_candidates = context.param_regression.max_candidates;
    task.max_case_seconds = context.param_regression.max_case_seconds;
    task.max_total_seconds = context.param_regression.max_total_seconds;
    task.require_manual_gauge = true;
    task.allow_mlpack_rank = true;
    task.allow_ensmallen_opt = true;
    task.allow_promote = false;
    return task;
}

CxParamEvalRecord BuildManualSeedEvalRecord(
    const ManualTestContext& context,
    const CxParamRegressionTask& task)
{
    const ResultRefView& result = context.current_result_ref;
    CxParamEvalRecord record;
    record.candidate_id = "manual_seed";
    record.case_id = task.case_id;
    record.tool = task.tool;
    record.executed = true;
    if (task.tool == "Findcircle")
    {
        record.points = result.valid_points_count > 0
            ? result.valid_points_count
            : result.points_count;
        record.fit_available = result.fit_radius > 0.0f ||
            result.status == "geometry_result_available";
        record.mean_distance = result.avgdist;
    }
    else
    {
        record.points = result.valid_line_points_count > 0
            ? result.valid_line_points_count
            : (result.line_points_count > 0 ? result.line_points_count : result.valid_points_count);
        record.fit_available = result.line_result_status == "geometry_result_available" ||
            result.status == "geometry_result_available" ||
            (result.line_x0 != result.line_x1 || result.line_y0 != result.line_y1);
        record.mean_distance = result.line_avgdist;
    }
    record.support_score = record.points > 0 ? 1.0 : 0.0;
    record.failure_stage = record.fit_available ? "" : "pending_probe_or_no_fit";
    record.classification = record.fit_available ? "manual_seed_geometry_available" : "manual_seed_needs_probe";
    const std::filesystem::path case_dir = ManualGaugeCaseDir(context);
    record.result_summary_path = (case_dir / "result_summary.json").string();
    record.tool_display_path = (case_dir / "tool_display.png").string();
    record.replay_package_path = (case_dir / "replay_package.json").string();
    return record;
}

CxParamAccuracyStats BuildManualSeedAccuracyStats(
    const CxParamRegressionTask& task,
    const CxParamEvalRecord& record,
    const ManualGaugeState& gauge)
{
    CxParamAccuracyStats stats;
    stats.candidate_id = "manual_seed";
    stats.tool = task.tool;
    stats.total_cases = 1;
    stats.executed_cases = record.executed ? 1 : 0;
    stats.timeout_cases = record.timeout ? 1 : 0;
    stats.geometry_pass = record.fit_available ? 1 : 0;
    stats.evidence_pass = record.points > 0 ? 1 : 0;
    stats.human_accept = ManualGaugeAcceptedForParamRegression(gauge) ? 1 : 0;
    stats.geometry_pass_rate = static_cast<double>(stats.geometry_pass);
    stats.evidence_pass_rate = static_cast<double>(stats.evidence_pass);
    stats.human_accept_rate = static_cast<double>(stats.human_accept);
    stats.avg_support_score = record.support_score;
    stats.avg_mean_distance = record.mean_distance;
    stats.avg_fit_offset = record.fit_offset;
    stats.stability_score = record.fit_available ? 0.5 : 0.0;
    stats.risk_score = record.fit_available ? 0.4 : 0.8;
    return stats;
}

bool InitializeParamRegressionFromGauge(
    ManualTestContext& context,
    std::string& reason)
{
    ManualParamRegressionState& state = context.param_regression;
    if (!ValidateParamRegressionPrerequisites(context, reason))
    {
        state.initialized = false;
        state.status = "blocked";
        state.reason = reason;
        return false;
    }

    state.task = BuildParamRegressionTaskFromManualGauge(context);
    state.range_set = MakeConservativeRangeSet(state.task.tool);
    state.range_set.max_candidates = state.max_candidates;
    state.range_set.max_case_seconds = state.max_case_seconds;
    state.range_set.max_total_seconds = state.max_total_seconds;
    state.candidates = GenerateBasicParamCandidates(state.range_set, state.max_candidates);
    state.records.clear();
    CxParamEvalRecord manual_seed = BuildManualSeedEvalRecord(context, state.task);
    state.records.push_back(manual_seed);
    state.accuracy_stats.clear();
    state.accuracy_stats.push_back(
        BuildManualSeedAccuracyStats(state.task, manual_seed, context.current_gauge));
    state.output_dir = (ManualGaugeCaseDir(context) / "param_regression").string();
    state.initialized = true;
    state.status = "ready";
    state.reason =
        "Manual gauge accepted. Phase 1 can export parameter range, candidates, and evidence reports.";
    reason.clear();
    return true;
}

CxParamCandidate CandidateFromManualGauge(
    const ManualGaugeState& gauge,
    const std::string& id,
    const std::string& source)
{
    CxParamCandidate c;
    c.candidate_id = id;
    c.source = source;
    c.method = gauge.method;
    c.threshold = gauge.threshold;
    c.gap = gauge.gap;
    c.linegap = gauge.linegap;
    c.wgap = gauge.wgap;
    c.hgap = gauge.hgap;
    c.filterprofile = gauge.filterprofile;
    c.samplerate = 1;
    c.predicted_quality = source == "manual_seed" ? 0.65 : 0.55;
    c.predicted_risk = source == "manual_seed" ? 0.30 : 0.45;
    c.predicted_failure_class = "pending_probe";
    c.selected_for_probe = true;
    return c;
}

void AddMlpackRankPlaceholderCandidates(ManualParamRegressionState& state)
{
    (void)state;
}

void AddEnsmallenOptPlaceholderCandidates(ManualParamRegressionState& state)
{
    (void)state;
}

void RefreshParamRegressionExportedFiles(ManualParamRegressionState& state)
{
    state.exported_files.clear();
    const std::filesystem::path root(state.output_dir);
    const char* names[] = {
        "param_regression_task.json",
        "param_range_report.json",
        "param_range_report.csv",
        "param_range_report.md",
        "param_candidates.json",
        "param_candidates.csv",
        "param_eval_records.jsonl",
        "hit_distribution.json",
        "hit_distribution.csv",
        "param_hit_distribution_report.md",
        "param_accuracy_matrix.json",
        "param_accuracy_matrix.csv",
        "param_accuracy_matrix.md",
        "param_candidate_distribution.md",
        "param_optimization_trace.json",
        "param_stability_report.md",
        "param_recommendation_report.md",
        "param_profile_promotion_gate.md",
        "param_profile_candidate.cxsc",
        "manual_acceptance_checklist.md"
    };
    for (const char* name : names)
    {
        const std::filesystem::path path = root / name;
        if (std::filesystem::exists(path))
            state.exported_files.push_back(path.string());
    }
}

bool ExportParamRegressionManualAcceptanceChecklist(
    const ManualTestContext& context,
    const std::filesystem::path& path,
    std::string& reason)
{
    const ManualParamRegressionState& reg = context.param_regression;
    const ManualGaugeState& gauge = context.current_gauge;
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path);
    if (!file.is_open())
    {
        reason = "failed to write manual acceptance checklist";
        return false;
    }

    file << "# Parameter Regression Manual Acceptance Checklist\n\n";
    file << "## Current Context\n\n";
    file << "- Case: `" << gauge.case_id << "`\n";
    file << "- Image: `" << gauge.image_id << "`\n";
    file << "- Target: `" << gauge.target_id << "`\n";
    file << "- Tool: `" << gauge.tool << "`\n";
    file << "- Gauge Review Status: `" << gauge.review_status << "`\n";
    file << "- Gauge Accepted: `" << (ManualGaugeAcceptedForParamRegression(gauge) ? "yes" : "no") << "`\n";
    file << "- Output Dir: `" << reg.output_dir << "`\n\n";

    file << "## Checklist\n\n";
    file << "- [ ] Manual gauge position/direction/width or circle ring is correct.\n";
    file << "- [ ] `Apply Gauge To Globals` has been used before probe/replay.\n";
    file << "- [ ] Candidate table contains manual seed and selected probe candidates.\n";
    file << "- [ ] Candidate parameter values are visible and editable in UI.\n";
    file << "- [ ] `param_candidates.json/csv` matches UI candidate table.\n";
    file << "- [ ] `param_eval_records.jsonl` contains manual seed evidence or selected probe result after probe.\n";
    file << "- [ ] `param_hit_distribution_report.md` is present for evidence review.\n";
    file << "- [ ] `param_accuracy_matrix.md/json/csv` is present for stability review.\n";
    file << "- [ ] `param_profile_promotion_gate.md` says promotion is disabled unless mini-regression passes.\n";
    file << "- [ ] `param_profile_candidate.cxsc` is diagnostic-only, not baseline.\n\n";

    file << "## Candidate Snapshot\n\n";
    file << "| Index | Candidate | Source | Selected | Method | Threshold | Gap | LineGap | WGap | HGap | Filter | Risk |\n";
    file << "|---:|---|---|---|---:|---:|---:|---:|---:|---:|---:|---:|\n";
    for (std::size_t i = 0; i < reg.candidates.size(); ++i)
    {
        const auto& c = reg.candidates[i];
        file << "| " << i << " | " << c.candidate_id << " | " << c.source << " | "
             << (c.selected_for_probe ? "yes" : "no") << " | " << c.method << " | "
             << c.threshold << " | " << c.gap << " | " << c.linegap << " | "
             << c.wgap << " | " << c.hgap << " | " << c.filterprofile << " | "
             << c.predicted_risk << " |\n";
    }
    reason.clear();
    return true;
}

bool IsFindlineFindcircleContext(ManualTestContext& context)
{
    const ManualGaugeState& g = context.current_gauge;
    return (g.tool == "Findline" || g.tool == "Findcircle") ||
           g.has_line_gauge || g.has_circle_gauge;
}

void DrawKeyParameterUnavailableNotice(const ManualTestContext& context)
{
  if (!ImGui::CollapsingHeader("关键参数 UI / 参数整定图", ImGuiTreeNodeFlags_DefaultOpen))
    return;
  ImGui::TextColored(ImVec4(1.0f, 0.68f, 0.25f, 1.0f),
                     "Current image/tool is not suitable for Findline/Findcircle key-parameter tuning.");
  ImGui::TextWrapped(
    "Select or create a Line/Circle annotation tool, or load a script containing Findline/Findcircle. "
    "After selecting a Line/Circle element, the center/boundary handles sync ManualGaugeState and the key-parameter UI will appear.");
  ImGui::Text("current gauge tool=%s line_gauge=%s circle_gauge=%s",
              context.current_gauge.tool.c_str(),
              context.current_gauge.has_line_gauge ? "yes" : "no",
              context.current_gauge.has_circle_gauge ? "yes" : "no");
}

void DrawCxScriptWorkbenchOverview(ManualTestContext& context)
{
  if (!ImGui::CollapsingHeader("CxScript Workbench / 人工验收总览", ImGuiTreeNodeFlags_DefaultOpen))
    return;

  const ManualGaugeState& gauge = context.current_gauge;
  const ManualParamRegressionState& reg = context.param_regression;
  const bool gaugeAccepted = ManualGaugeAcceptedForParamRegression(gauge);
  const bool keyParamSuitable = IsFindlineFindcircleContext(context);

  ImGui::TextWrapped(
    "This overview mirrors the design map: Evidence/Annotation -> cxparser script template -> Key Parameters -> Param Regression -> Conclusion/Evidence.");
  ImGui::Separator();

  if (ImGui::BeginTable("cxscript_workbench_map", 3,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
  {
    ImGui::TableSetupColumn("证据链 / 标注工具集");
    ImGui::TableSetupColumn("cxparser script 基础模板");
    ImGui::TableSetupColumn("关键参数 / 结论");
    ImGui::TableHeadersRow();

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text("Image");
    ImGui::BulletText("image_file: %s", UiTextOrDash(context.image_file_path));
    ImGui::BulletText("global_matInput: %s", context.global_variable_views.empty() ? "pending" : context.global_variable_views.front().status.c_str());
    ImGui::BulletText("annotation elements: %d", context.manual_elements_count);
    ImGui::BulletText("source preview: %s", context.source_preview_enabled ? "on" : "off");
    ImGui::BulletText("gauge annotation: %s",
                      (ManualGaugeCaseDir(context) / "gauge_annotation.json").string().c_str());

    ImGui::TableSetColumnIndex(1);
    ImGui::Text("Template");
    ImGui::BulletText("script: %s", InferCurrentTemplatePath(context).c_str());
    ImGui::BulletText("tool: %s", InferCurrentTemplateTool(context).c_str());
    ImGui::BulletText("editor source: %s", context.editor_source.c_str());
    ImGui::BulletText("editor dirty: %s", context.editor_dirty ? "yes" : "no");
    ImGui::BulletText("catalog loaded: %s (%d entries)",
                      context.catalog_loaded ? "yes" : "no",
                      static_cast<int>(context.catalog_entries.size()));
    ImGui::BulletText("semantic lines: %d", static_cast<int>(context.line_views.size()));

    ImGui::TableSetColumnIndex(2);
    ImGui::Text("Status");
    ImGui::BulletText("gauge: %s", gaugeAccepted ? "manual_accepted" : gauge.review_status.c_str());
    ImGui::BulletText("param regression: %s", reg.status.c_str());
    ImGui::BulletText("key parameter UI: %s", keyParamSuitable ? "visible" : "waiting for Findline/Findcircle");
    ImGui::BulletText("candidates: %d selected: %d",
                      static_cast<int>(reg.candidates.size()),
                      CountSelectedParamCandidates(context));
    ImGui::BulletText("result: %s", context.current_result_ref.status.c_str());
    ImGui::BulletText("debug: %s | %s",
                      context.debug_status.c_str(),
                      context.debug_reason.c_str());

    ImGui::EndTable();
  }

  ImGui::Separator();
  if (ImGui::BeginTable("cxscript_design_flow", 6,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
  {
    ImGui::TableSetupColumn("1 Evidence");
    ImGui::TableSetupColumn("2 Template");
    ImGui::TableSetupColumn("3 Gauge");
    ImGui::TableSetupColumn("4 Params");
    ImGui::TableSetupColumn("5 Conclusion");
    ImGui::TableSetupColumn("6 Export");
    ImGui::TableHeadersRow();
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextWrapped("%s", context.image_file_path.empty() ? "image pending" : "image selected");
    ImGui::TableSetColumnIndex(1);
    ImGui::TextWrapped("%s", context.editor_text.empty() ? "script pending" : "script loaded");
    ImGui::TableSetColumnIndex(2);
    ImGui::TextWrapped("%s", gaugeAccepted ? "accepted" : "needs review");
    ImGui::TableSetColumnIndex(3);
    ImGui::TextWrapped("%s", reg.initialized ? "candidate table ready" : "initialize after gauge");
    ImGui::TableSetColumnIndex(4);
    ImGui::TextWrapped("%s", context.current_result_ref.status.empty() ? "no result" : context.current_result_ref.status.c_str());
    ImGui::TableSetColumnIndex(5);
    ImGui::TextWrapped("%d files indexed", static_cast<int>(reg.exported_files.size()));
    ImGui::EndTable();
  }
}

void DrawEvidenceCaseListPanel(ManualTestContext& context)
{
  if (!ImGui::CollapsingHeader("Evidence Case List", ImGuiTreeNodeFlags_DefaultOpen))
    return;

  ImGui::TextWrapped("Evidence cases organized by case/image/target/tool/gauge_status/probe_status/contract_status/review_status");

  if (context.evidence_items.empty())
  {
    ImGui::TextDisabled("No evidence cases loaded.");
    ImGui::Text("Loading from catalog entries...");

    for (const auto& entry : context.catalog_entries)
    {
      bool isVisible = entry.manual_visible && entry.frozen &&
          (entry.expected_result == "ok" || entry.expected_result == "ng_expected");
      if (!isVisible) continue;

      ManualEvidenceItem item;
      item.case_id = entry.script_id;
      item.tool = entry.tool;
      item.script_id = entry.script_id;
      item.gauge_status = "unannotated";
      item.probe_status = "pending";
      item.contract_status = "pending";
      item.review_status = "unreviewed";
      context.evidence_items.push_back(item);
    }
    context.script_evidence_groups_dirty = true;
    context.workbench_assets_loaded = false;
  }

  if (!context.evidence_items.empty())
  {
    ImGui::BeginChild("evidence_case_list", ImVec2(-1, 200), true);

    if (ImGui::BeginTable("evidence_case_table", 10,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
    {
      ImGui::TableSetupColumn("Case", ImGuiTableColumnFlags_WidthFixed, 120.0f);
      ImGui::TableSetupColumn("Tool", ImGuiTableColumnFlags_WidthFixed, 80.0f);
      ImGui::TableSetupColumn("Image", ImGuiTableColumnFlags_WidthFixed, 100.0f);
      ImGui::TableSetupColumn("Target", ImGuiTableColumnFlags_WidthFixed, 100.0f);
      ImGui::TableSetupColumn("Gauge", ImGuiTableColumnFlags_WidthFixed, 90.0f);
      ImGui::TableSetupColumn("Probe", ImGuiTableColumnFlags_WidthFixed, 80.0f);
      ImGui::TableSetupColumn("Contract", ImGuiTableColumnFlags_WidthFixed, 80.0f);
      ImGui::TableSetupColumn("Review", ImGuiTableColumnFlags_WidthFixed, 90.0f);
      ImGui::TableSetupColumn("Script", ImGuiTableColumnFlags_WidthFixed, 100.0f);
      ImGui::TableSetupColumn("Param", ImGuiTableColumnFlags_WidthFixed, 100.0f);
      ImGui::TableHeadersRow();

      for (const auto& item : context.evidence_items)
      {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(item.case_id.c_str());
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(item.tool.c_str());
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted(item.image_id.empty() ? "-" : item.image_id.c_str());
        ImGui::TableSetColumnIndex(3);
        ImGui::TextUnformatted(item.target_id.empty() ? "-" : item.target_id.c_str());
        ImGui::TableSetColumnIndex(4);
        ImGui::TextUnformatted(item.gauge_status.c_str());
        ImGui::TableSetColumnIndex(5);
        ImGui::TextUnformatted(item.probe_status.c_str());
        ImGui::TableSetColumnIndex(6);
        ImGui::TextUnformatted(item.contract_status.c_str());
        ImGui::TableSetColumnIndex(7);
        ImGui::TextUnformatted(item.review_status.c_str());
        ImGui::TableSetColumnIndex(8);
        ImGui::TextUnformatted(item.script_id.empty() ? "-" : item.script_id.c_str());
        ImGui::TableSetColumnIndex(9);
        ImGui::TextUnformatted(item.parameter_profile_id.empty() ? "-" : item.parameter_profile_id.c_str());
      }

      ImGui::EndTable();
    }

    ImGui::EndChild();
  }
}

void DrawCxScriptTemplateSummaryPanel(const ManualTestContext& context)
{
  if (!ImGui::CollapsingHeader("cxparser script 基础模板 / 当前模板摘要", ImGuiTreeNodeFlags_DefaultOpen))
    return;

  ImGui::Text("Current script: %s", InferCurrentTemplatePath(context).c_str());
  ImGui::Text("Tool: %s | Source: %s | Dirty: %s",
              InferCurrentTemplateTool(context).c_str(),
              context.editor_source.c_str(),
              context.editor_dirty ? "yes" : "no");
  ImGui::Text("Active case: %s | purpose: %s",
              UiTextOrDash(context.active_script_case_name),
              UiTextOrDash(context.active_script_case_purpose));
  ImGui::Separator();

  if (ImGui::BeginTable("template_object_method_summary", 4,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable,
                        ImVec2(-1, 120)))
  {
    ImGui::TableSetupColumn("Object");
    ImGui::TableSetupColumn("Type");
    ImGui::TableSetupColumn("Declared Line");
    ImGui::TableSetupColumn("Runtime State");
    ImGui::TableHeadersRow();
    for (const auto& object : context.object_views)
    {
      std::string runtime_state = "not_created";
      for (const auto& runtime : context.runtime_objects)
      {
        if (runtime.name == object.name)
        {
          runtime_state = runtime.exists_in_parser ? runtime.runtime_state : "stale";
          break;
        }
      }
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(object.name.c_str());
      ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(object.type.c_str());
      ImGui::TableSetColumnIndex(2); ImGui::Text("%d", object.declared_line);
      ImGui::TableSetColumnIndex(3); ImGui::TextUnformatted(runtime_state.c_str());
    }
    ImGui::EndTable();
  }
}

void DrawKeyParameterSummaryPanel(const ManualTestContext& context)
{
  if (!ImGui::CollapsingHeader("关键参数表 / Key Parameter Summary", ImGuiTreeNodeFlags_DefaultOpen))
    return;

  const ManualGaugeState& g = context.current_gauge;
  if (ImGui::BeginTable("key_parameter_summary", 4,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
  {
    ImGui::TableSetupColumn("Group");
    ImGui::TableSetupColumn("Parameter");
    ImGui::TableSetupColumn("Value");
    ImGui::TableSetupColumn("Source");
    ImGui::TableHeadersRow();
    auto row = [&](const char* group, const char* name, int value, const char* source) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(group);
      ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(name);
      ImGui::TableSetColumnIndex(2); ImGui::Text("%d", value);
      ImGui::TableSetColumnIndex(3); ImGui::TextUnformatted(source);
    };

    row("line", "x0", g.line_x0, g.source.c_str());
    row("line", "y0", g.line_y0, g.source.c_str());
    row("line", "x1", g.line_x1, g.source.c_str());
    row("line", "y1", g.line_y1, g.source.c_str());
    row("line", "tool_half_width", g.tool_half_width, g.source.c_str());
    row("line", "wgap", g.wgap, g.source.c_str());
    row("line", "hgap", g.hgap, g.source.c_str());
    row("common", "linegap", g.linegap, g.source.c_str());
    row("common", "threshold", g.threshold, g.source.c_str());
    row("common", "method", g.method, g.source.c_str());
    row("common", "filterprofile", g.filterprofile, g.source.c_str());
    row("circle", "cx", g.circle_cx, g.source.c_str());
    row("circle", "cy", g.circle_cy, g.source.c_str());
    row("circle", "px", g.circle_px, g.source.c_str());
    row("circle", "py", g.circle_py, g.source.c_str());
    row("circle", "gap", g.gap, g.source.c_str());
    ImGui::EndTable();
  }
}

void DrawConclusionSummaryPanel(const ManualTestContext& context)
{
  if (!ImGui::CollapsingHeader("结论 UI / Result Conclusion", ImGuiTreeNodeFlags_DefaultOpen))
    return;

  const ResultRefView& r = context.current_result_ref;
  ImGui::Text("result_ref: %s = %s", UiTextOrDash(r.name), UiTextOrDash(r.value));
  ImGui::Text("status: %s | reason: %s", UiTextOrDash(r.status), UiTextOrDash(r.reason));

  if (ImGui::BeginTable("conclusion_summary_table", 4,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
  {
    ImGui::TableSetupColumn("Metric");
    ImGui::TableSetupColumn("Findline");
    ImGui::TableSetupColumn("Findcircle");
    ImGui::TableSetupColumn("Evidence");
    ImGui::TableHeadersRow();
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("points");
    ImGui::TableSetColumnIndex(1); ImGui::Text("%d", r.valid_line_points_count > 0 ? r.valid_line_points_count : r.line_points_count);
    ImGui::TableSetColumnIndex(2); ImGui::Text("%d", r.valid_points_count > 0 ? r.valid_points_count : r.points_count);
    ImGui::TableSetColumnIndex(3); ImGui::TextUnformatted(context.image_overlay_summary.empty() ? "pending" : context.image_overlay_summary.c_str());
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("fit");
    ImGui::TableSetColumnIndex(1); ImGui::Text("(%.1f,%.1f)-(%.1f,%.1f)", r.line_x0, r.line_y0, r.line_x1, r.line_y1);
    ImGui::TableSetColumnIndex(2); ImGui::Text("(%.1f,%.1f) r=%.1f", r.fit_cx, r.fit_cy, r.fit_radius);
    ImGui::TableSetColumnIndex(3); ImGui::Text("avgdist line=%.2f circle=%.2f", r.line_avgdist, r.avgdist);
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("failure hint");
    ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(r.line_measure_failure_hint.empty() ? r.line_result_reason.c_str() : r.line_measure_failure_hint.c_str());
    ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(r.reason.c_str());
    ImGui::TableSetColumnIndex(3); ImGui::TextUnformatted(context.debug_reason.c_str());
    ImGui::EndTable();
  }
}

void SyncKeyParameterUiToGauge(ManualTestContext& context)
{
  ManualGaugeState& g = context.current_gauge;
  ManualParamRegressionState& ui = context.param_regression;
  g.threshold = ui.contrast_percent;
  g.linegap = ui.measure_order;
  g.filterprofile = ui.enable_filter ? 1 : 0;
  if (g.has_line_gauge || g.tool != "Findcircle")
  {
    g.wgap = std::max(1, ui.sample_points);
    g.hgap = std::max(1, ui.valid_length_percent);
  }
  if (g.has_circle_gauge || g.tool == "Findcircle")
  {
    g.gap = std::max(1, ui.sample_points);
  }
  g.method = ui.edge_mode;
  g.dirty = true;
}

void ResetKeyParameterUiDefaults(ManualTestContext& context)
{
  ManualParamRegressionState& ui = context.param_regression;
  ui.edge_mode = 0;
  ui.contrast_percent = 20;
  ui.valid_length_percent = 50;
  ui.interference_length_percent = 20;
  ui.roughness = 8;
  ui.burr_filter_percent = 0;
  ui.measure_order = 3;
  ui.black_index = 50;
  ui.sample_points = 8;
  ui.catch_method = 0;
  ui.enable_fast_measure = true;
  ui.enable_filter = true;
  SyncKeyParameterUiToGauge(context);
}

void DrawKeyParameterControlPanel(ManualTestContext& context)
{
  ManualGaugeState& gauge = context.current_gauge;

  ImGui::TextUnformatted("Tool: ");
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "%s", gauge.tool.c_str());
  ImGui::Separator();

  ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
  if (ImGui::CollapsingHeader("Geometry"))
  {
      ImGui::PushID("geometry");
      if (gauge.tool == "Findcircle" || gauge.has_circle_gauge)
      {
          ImGui::SetNextItemWidth(120.0f); ImGui::InputInt("cx", &gauge.circle_cx);
          ImGui::SameLine(); ImGui::SetNextItemWidth(120.0f); ImGui::InputInt("cy", &gauge.circle_cy);
          ImGui::SameLine(); ImGui::SetNextItemWidth(120.0f); ImGui::InputInt("radius", &gauge.radius);

          ImGui::SetNextItemWidth(120.0f); ImGui::InputInt("inner_radius", &gauge.inner_radius);
          ImGui::SameLine(); ImGui::SetNextItemWidth(120.0f); ImGui::InputInt("outer_radius", &gauge.outer_radius);
          ImGui::SameLine(); ImGui::SetNextItemWidth(120.0f); ImGui::InputInt("gap", &gauge.gap);
      }
      else
      {
          ImGui::SetNextItemWidth(120.0f); ImGui::InputInt("x0", &gauge.line_x0);
          ImGui::SameLine(); ImGui::SetNextItemWidth(120.0f); ImGui::InputInt("y0", &gauge.line_y0);
          ImGui::SameLine(); ImGui::SetNextItemWidth(120.0f); ImGui::InputInt("half_width", &gauge.tool_half_width);

          ImGui::SetNextItemWidth(120.0f); ImGui::InputInt("x1", &gauge.line_x1);
          ImGui::SameLine(); ImGui::SetNextItemWidth(120.0f); ImGui::InputInt("y1", &gauge.line_y1);
      }
      ImGui::PopID();
  }

  ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
  if (ImGui::CollapsingHeader("Edge Params"))
  {
      ImGui::PushID("edge_params");

      ImGui::TextUnformatted("threshold");
      ImGui::SameLine(80.0f);
      ImGui::SetNextItemWidth(180.0f); ImGui::SliderInt("##threshold", &gauge.threshold, 0, 255);
      ImGui::SameLine(); ImGui::SetNextItemWidth(70.0f); ImGui::InputInt("##t_val", &gauge.threshold);
      gauge.threshold = std::max(0, std::min(255, gauge.threshold));

      ImGui::TextUnformatted("method");
      ImGui::SameLine(80.0f);
      const char* methods[] = {"0", "1", "2", "3"};
      ImGui::SetNextItemWidth(100.0f); ImGui::Combo("##method", &gauge.method, methods, IM_ARRAYSIZE(methods));

      ImGui::TextUnformatted("linegap");
      ImGui::SameLine(80.0f);
      ImGui::SetNextItemWidth(180.0f); ImGui::SliderInt("##linegap", &gauge.linegap, 0, 50);
      ImGui::SameLine(); ImGui::SetNextItemWidth(70.0f); ImGui::InputInt("##lg_val", &gauge.linegap);
      gauge.linegap = std::max(0, std::min(50, gauge.linegap));

      if (gauge.tool == "Findcircle" || gauge.has_circle_gauge)
      {
          ImGui::TextUnformatted("gap");
          ImGui::SameLine(80.0f);
          ImGui::SetNextItemWidth(180.0f); ImGui::SliderInt("##gap", &gauge.gap, 0, 200);
          ImGui::SameLine(); ImGui::SetNextItemWidth(70.0f); ImGui::InputInt("##gap_val", &gauge.gap);
          gauge.gap = std::max(0, std::min(200, gauge.gap));
      }
      else
      {
          ImGui::TextUnformatted("wgap");
          ImGui::SameLine(80.0f);
          ImGui::SetNextItemWidth(180.0f); ImGui::SliderInt("##wgap", &gauge.wgap, 0, 50);
          ImGui::SameLine(); ImGui::SetNextItemWidth(70.0f); ImGui::InputInt("##wg_val", &gauge.wgap);
          gauge.wgap = std::max(0, std::min(50, gauge.wgap));

          ImGui::TextUnformatted("hgap");
          ImGui::SameLine(80.0f);
          ImGui::SetNextItemWidth(180.0f); ImGui::SliderInt("##hgap", &gauge.hgap, 0, 50);
          ImGui::SameLine(); ImGui::SetNextItemWidth(70.0f); ImGui::InputInt("##hg_val", &gauge.hgap);
          gauge.hgap = std::max(0, std::min(50, gauge.hgap));

          ImGui::TextUnformatted("filterprofile");
          ImGui::SameLine(80.0f);
          ImGui::SetNextItemWidth(180.0f); ImGui::SliderInt("##fp", &gauge.filterprofile, 0, 10);
          ImGui::SameLine(); ImGui::SetNextItemWidth(70.0f); ImGui::InputInt("##fp_val", &gauge.filterprofile);
          gauge.filterprofile = std::max(0, std::min(10, gauge.filterprofile));
      }
      ImGui::PopID();
  }

  ImGui::Separator();
  ImGui::TextUnformatted("Actions");

  const float btnWidth = (ImGui::GetContentRegionAvail().x - 30.0f) / 3.0f;

  ImGui::PushID("actions");
  if (ImGui::Button("Apply To Gauge", ImVec2(btnWidth, 0)))
  {
    SyncKeyParameterUiToGauge(context);
    gauge.dirty = true;
    gauge.review_status = "editing";
  }
  ImGui::SameLine();
  if (ImGui::Button("Apply To Globals", ImVec2(btnWidth, 0)))
  {
    ApplyManualGaugeToGlobals(context);
  }
  ImGui::SameLine();
  if (ImGui::Button("Run Script", ImVec2(btnWidth, 0)))
  {
    context.run_state = "running";
  }

  if (ImGui::Button("Save Candidate", ImVec2(btnWidth, 0)))
  {
    SyncKeyParameterUiToGauge(context);
  }
  ImGui::SameLine();
  if (ImGui::Button("Reset", ImVec2(btnWidth, 0)))
  {
    ResetKeyParameterUiDefaults(context);
    SyncKeyParameterUiToGauge(context);
  }
  ImGui::PopID();
}

void DrawParamTuningScatterPanel(ManualTestContext& context)
{
  if (!ImGui::CollapsingHeader("参数整定图 / Parameter Tuning Map", ImGuiTreeNodeFlags_DefaultOpen))
    return;

  ManualParamRegressionState& reg = context.param_regression;
  const char* tabs[] = {"Tuning", "Reading", "Test", "LastTest", "Search"};
  for (int i = 0; i < IM_ARRAYSIZE(tabs); ++i)
  {
    if (i > 0) ImGui::SameLine();
    if (ImGui::Selectable(tabs[i], reg.tuning_tab == i, 0, ImVec2(74, 0)))
      reg.tuning_tab = i;
  }

  ImGui::TextWrapped(
    "Scatter view uses current parameter candidates. X=threshold, Y=predicted quality/risk score. Selected candidates are highlighted.");

  const ImVec2 plotSize(520.0f, 260.0f);
  ImVec2 p0 = ImGui::GetCursorScreenPos();
  ImVec2 p1(p0.x + plotSize.x, p0.y + plotSize.y);
  ImDrawList* draw = ImGui::GetWindowDrawList();
  draw->AddRectFilled(p0, p1, IM_COL32(18, 18, 18, 255));
  draw->AddRect(p0, p1, IM_COL32(230, 230, 230, 255));

  for (int gx = 0; gx <= 10; ++gx)
  {
    const float x = p0.x + plotSize.x * gx / 10.0f;
    draw->AddLine(ImVec2(x, p0.y), ImVec2(x, p1.y), IM_COL32(80, 80, 80, 180));
  }
  for (int gy = 0; gy <= 10; ++gy)
  {
    const float y = p0.y + plotSize.y * gy / 10.0f;
    draw->AddLine(ImVec2(p0.x, y), ImVec2(p1.x, y), IM_COL32(80, 80, 80, 180));
  }

  auto mapX = [&](int threshold) {
    const float t = static_cast<float>(std::max(0, std::min(100, threshold))) / 100.0f;
    return p0.x + 36.0f + t * (plotSize.x - 54.0f);
  };
  auto mapY = [&](double quality, double risk) {
    double score = quality > 0.0 ? quality : (1.0 - risk);
    score = std::max(0.0, std::min(1.0, score));
    return p1.y - 28.0f - static_cast<float>(score) * (plotSize.y - 52.0f);
  };

  const ImU32 colors[] = {
    IM_COL32(255, 90, 90, 255),
    IM_COL32(80, 210, 255, 255),
    IM_COL32(120, 255, 120, 255),
    IM_COL32(255, 230, 80, 255),
    IM_COL32(255, 150, 255, 255)
  };

  for (std::size_t i = 0; i < reg.candidates.size(); ++i)
  {
    const CxParamCandidate& c = reg.candidates[i];
    const float x = mapX(c.threshold);
    const float y = mapY(c.predicted_quality, c.predicted_risk);
    const float radius = c.selected_for_probe ? 5.5f : 3.5f;
    draw->AddCircleFilled(ImVec2(x, y), radius, colors[i % IM_ARRAYSIZE(colors)]);
    if (static_cast<int>(i) == reg.selected_candidate_index)
      draw->AddCircle(ImVec2(x, y), radius + 4.0f, IM_COL32(255, 255, 255, 255), 16, 2.0f);
  }

  draw->AddText(ImVec2(p0.x + 8.0f, p0.y + 6.0f), IM_COL32(240, 240, 240, 255), "Quality / Risk");
  draw->AddText(ImVec2(p1.x - 110.0f, p1.y - 22.0f), IM_COL32(240, 240, 240, 255), "Threshold");
  ImGui::Dummy(plotSize);

  ImGui::SameLine();
  ImGui::BeginGroup();
  ImGui::TextUnformatted("After Parameter");
  ImGui::SetNextItemWidth(90.0f);
  ImGui::SliderInt("Edge", &reg.edge_mode, 0, 2);
  ImGui::SetNextItemWidth(90.0f);
  ImGui::SliderInt("Scale", &reg.contrast_percent, 0, 100);
  ImGui::SetNextItemWidth(90.0f);
  ImGui::SliderInt("X", &reg.valid_length_percent, 0, 100);
  ImGui::SetNextItemWidth(90.0f);
  ImGui::SliderInt("Y", &reg.interference_length_percent, 0, 100);
  ImGui::RadioButton("Visual", reg.tuning_tab == 0);
  ImGui::RadioButton("Param", reg.tuning_tab == 1);
  ImGui::RadioButton("Grid", reg.tuning_tab == 2);
  if (ImGui::Button("Reset Transform"))
    ResetKeyParameterUiDefaults(context);
  if (ImGui::Button("Animate"))
  {
    context.debug_action = "Tuning Map Animate";
    context.debug_status = "PENDING";
    context.debug_reason = "visual placeholder; no runtime execution";
  }
  if (ImGui::Button("Show Source"))
    context.source_preview_enabled = true;
  if (ImGui::Button("What?"))
  {
    context.debug_action = "Tuning Map Help";
    context.debug_status = "PENDING";
    context.debug_reason = "Tuning map plots candidates. White ring means focused candidate.";
  }
  ImGui::EndGroup();

  ImGui::Text("Candidates=%d Selected=%d Focus=%d",
              static_cast<int>(reg.candidates.size()),
              CountSelectedParamCandidates(context),
              reg.selected_candidate_index);
}
