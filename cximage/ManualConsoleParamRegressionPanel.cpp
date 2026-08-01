#include "pch.h"
#include "ManualConsoleParamRegressionPanel.h"
#include "ManualConsoleUtils.h"
#include "ManualConsoleGauge.h"
#include "ManualConsoleScriptDebugPanel.h"
#include "CxParameterProfileRuntime.h"
#include "CxScriptCasePackageWriter.h"
#include "CxUnifiedLog.h"

#include <algorithm>
#include <cctype>
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
    task.tool = gauge.tool.empty() ? "FindLine" : gauge.tool;
    task.task_id = "param_regression_" + task.case_id;
    task.gauge_annotation_path =
        (ManualGaugeCaseDir(context) / "gauge_annotation.json").string();
    task.base_script_id = task.tool == "FindCircle"
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
    if (task.tool == "FindCircle")
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

bool IsFindLineFindCircleContext(ManualTestContext& context)
{
    const ManualGaugeState& g = context.current_gauge;
    return (g.tool == "FindLine" || g.tool == "FindCircle") ||
           g.has_line_gauge || g.has_circle_gauge;
}

static std::string ToLowerAsciiLocal(std::string text)
{
  std::transform(text.begin(), text.end(), text.begin(),
                 [](unsigned char ch) {
                   return static_cast<char>(std::tolower(ch));
                 });
  return text;
}

static bool ContainsNoCaseLocal(const std::string& text, const std::string& needle)
{
  return ToLowerAsciiLocal(text).find(ToLowerAsciiLocal(needle)) != std::string::npos;
}

static const RuntimeObjectView* FindPrimaryTorchRuntimeObjectLocal(
    const ManualTestContext& context)
{
  const std::string& primary = context.current_gauge.primary_object_name;
  if (!primary.empty())
  {
    for (const RuntimeObjectView& object : context.runtime_objects)
    {
      if ((object.type == "TorchTask" || object.type == "FindSegmentation") &&
          object.name == primary)
      {
        return &object;
      }
    }
  }

  for (const RuntimeObjectView& object : context.runtime_objects)
  {
    if (object.type == "TorchTask")
      return &object;
  }
  for (const RuntimeObjectView& object : context.runtime_objects)
  {
    if (object.type == "FindSegmentation")
      return &object;
  }
  return nullptr;
}

bool IsTorchContext(const ManualTestContext& context)
{
  const std::string tool = ToLowerAsciiLocal(context.current_gauge.tool);
  if (tool == "findsegmentation" || tool == "torchtask" ||
      tool == "torch" || tool == "segmentation")
    return true;

  for (const RuntimeObjectView& object : context.runtime_objects)
  {
    if (object.type == "TorchTask" || object.type == "FindSegmentation")
      return true;
  }

  return ContainsNoCaseLocal(context.loaded_script_path, "torch") ||
         ContainsNoCaseLocal(context.loaded_script_path, "find_segmentation") ||
         ContainsNoCaseLocal(context.editor_text, "TorchTask") ||
         ContainsNoCaseLocal(context.editor_text, "FindSegmentation") ||
         ContainsNoCaseLocal(context.editor_text, "torch.");
}

static void DrawReadonlyFieldLocal(const char* label, const std::string& value)
{
  ImGui::Text("%s: %s", label, value.empty() ? "-" : value.c_str());
}

static void DrawReadonlyFieldLocal(const char* label, int value)
{
  ImGui::Text("%s: %d", label, value);
}

static void DrawReadonlyFieldLocal(const char* label, double value)
{
  ImGui::Text("%s: %.3f", label, value);
}

static void DrawPendingBindingLineLocal(const char* label, const char* reason)
{
  ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.25f, 1.0f),
                     "%s: pending_binding", label);
  if (reason != nullptr && reason[0] != '\0')
    ImGui::SameLine(), ImGui::TextDisabled("(%s)", reason);
}

void DrawTorchKeyStatusPanel(const ManualTestContext& context)
{
  const RuntimeObjectView* object = FindPrimaryTorchRuntimeObjectLocal(context);

  if (!ImGui::CollapsingHeader("Torch Inference Status", ImGuiTreeNodeFlags_DefaultOpen))
  {
    // Keep the following Torch sections independent.  Collapsing inference
    // must not hide training/prompt status from the operator.
  }
  else
  {
    ImGui::TextWrapped(
        "Read-only bridge for TorchTask / FindSegmentation runtime fields. "
        "This panel displays existing headless/runtime refs; it does not claim detection acceptance.");

    if (object == nullptr)
    {
      ImGui::TextColored(ImVec4(1.0f, 0.68f, 0.25f, 1.0f),
                         "No TorchTask or FindSegmentation runtime object is available.");
      ImGui::Text("script: %s", UiTextOrDash(context.loaded_script_path));
    }
    else
    {
      ImGui::Separator();
      ImGui::Text("Primary Object: %s %s", object->type.c_str(), object->name.c_str());

      if (object->type == "TorchTask")
      {
        DrawReadonlyFieldLocal("status", object->torch_status);
        DrawReadonlyFieldLocal("reason", object->torch_reason);
        DrawReadonlyFieldLocal("failure_stage", object->torch_failure_stage);
        DrawReadonlyFieldLocal("ok", object->torch_ok);
        DrawReadonlyFieldLocal("error_code", object->torch_error_code);
        DrawReadonlyFieldLocal("actual_device", object->torch_actual_device);
        DrawReadonlyFieldLocal("runtime infer_ms", object->torch_infer_ms);
        DrawReadonlyFieldLocal("runtime total_ms", object->torch_total_ms);
        DrawReadonlyFieldLocal("result_count", object->torch_result_count);
        DrawReadonlyFieldLocal("mask_available", object->torch_mask_available);
        DrawReadonlyFieldLocal("result_ref", object->torch_result_ref);
        DrawReadonlyFieldLocal("evidence_ref", object->torch_evidence_ref);
        DrawReadonlyFieldLocal("primary_visual_ref", object->torch_primary_visual_ref);
        DrawReadonlyFieldLocal("mask_ref", object->torch_mask_ref);
        DrawReadonlyFieldLocal("overlay_ref", object->torch_overlay_ref);
        if (object->torch_result_count <= 0)
        {
          ImGui::TextColored(ImVec4(1.0f, 0.68f, 0.25f, 1.0f),
                             "detection rectangle projection: PENDING_DATA");
        }
        ImGui::TextColored(ImVec4(0.60f, 0.82f, 1.0f, 1.0f),
                           "semantic_quality: NOT_CLAIMED (runtime smoke / model unverified)");
      }
      else if (object->type == "FindSegmentation")
      {
        DrawReadonlyFieldLocal("backend", object->segmentation_backend);
        DrawReadonlyFieldLocal("backend_status", object->segmentation_backend_status);
        DrawReadonlyFieldLocal("device", object->segmentation_device);
        DrawReadonlyFieldLocal("model_path", object->segmentation_model_path);
        DrawReadonlyFieldLocal("status_code", object->segmentation_status_code);
        DrawReadonlyFieldLocal("reason", object->segmentation_reason);
        DrawReadonlyFieldLocal("result_ref", object->segmentation_result_ref);
        DrawReadonlyFieldLocal("mask_ref", object->segmentation_mask_ref);
        DrawReadonlyFieldLocal("overlay_ref", object->segmentation_overlay_ref);
        DrawReadonlyFieldLocal("contour_ref", object->segmentation_contour_ref);
        DrawReadonlyFieldLocal("contour_count", object->segmentation_contour_count);
        DrawReadonlyFieldLocal("primary_area", object->segmentation_primary_area);
        ImGui::TextColored(ImVec4(0.60f, 0.82f, 1.0f, 1.0f),
                           "semantic_quality: NOT_CLAIMED (prompt/backend smoke)");
      }
    }
  }

  if (ImGui::CollapsingHeader("Prompt / ROI", ImGuiTreeNodeFlags_DefaultOpen))
  {
    if (object == nullptr)
    {
      DrawPendingBindingLineLocal("prompt_rect", "run or select a TorchTask/FindSegmentation script first");
      DrawPendingBindingLineLocal("positive_points", "pending runtime object");
      DrawPendingBindingLineLocal("negative_points", "pending runtime object");
    }
    else if (object->type == "FindSegmentation")
    {
      ImGui::Text("prompt_rect: %s",
                  object->segmentation_has_prompt_rect ? "available" : "pending_binding");
      ImGui::Text("backend: %s", object->segmentation_backend.empty() ? "-" : object->segmentation_backend.c_str());
      DrawPendingBindingLineLocal("positive_points", "UI edit/save binding not enabled yet");
      DrawPendingBindingLineLocal("negative_points", "UI edit/save binding not enabled yet");
    }
    else
    {
      DrawPendingBindingLineLocal("prompt_rect", "TorchTask does not own manual prompt geometry");
      DrawPendingBindingLineLocal("positive_points", "use FindSegmentation prompt tool");
      DrawPendingBindingLineLocal("negative_points", "use FindSegmentation prompt tool");
    }
  }

  if (ImGui::CollapsingHeader("Torch Training Status", ImGuiTreeNodeFlags_DefaultOpen))
  {
    if (object == nullptr)
    {
      DrawPendingBindingLineLocal("training lifecycle", "run torch_train_lifecycle_direct_test.cxsc");
    }
    else if (object->type == "TorchTask")
    {
      DrawReadonlyFieldLocal("epoch", std::string("tiny smoke exposes lifecycle summary, not full epoch table"));
      DrawReadonlyFieldLocal("trainer_summary", object->torch_trainer_lifecycle_summary);
      DrawReadonlyFieldLocal("mainline_summary", object->torch_unified_mainline_summary);
      DrawReadonlyFieldLocal("train_ms", object->torch_train_ms);
      DrawReadonlyFieldLocal("total_ms", object->torch_total_ms);
      if (object->torch_trainer_lifecycle_summary.empty())
        DrawPendingBindingLineLocal("epoch-loss detail", "not produced by this runtime object");
    }
    else
    {
      DrawPendingBindingLineLocal("training lifecycle", "FindSegmentation is inference/prompt object");
    }
  }
}

void DrawTorchEvidenceAndReviewPanel(const ManualTestContext& context)
{
  const RuntimeObjectView* object = FindPrimaryTorchRuntimeObjectLocal(context);

  if (ImGui::CollapsingHeader("Torch Artifact Evidence", ImGuiTreeNodeFlags_DefaultOpen))
  {
    if (object == nullptr)
    {
      ImGui::TextDisabled("No torch runtime object selected.");
    }
    else if (object->type == "TorchTask")
    {
      DrawReadonlyFieldLocal("mask_binary", object->torch_mask_ref);
      DrawReadonlyFieldLocal("mask_overlay", object->torch_overlay_ref);
      DrawReadonlyFieldLocal("contours", object->torch_primary_visual_ref);
      DrawReadonlyFieldLocal("result_json", object->torch_result_ref);
      DrawReadonlyFieldLocal("evidence_json", object->torch_evidence_ref);
    }
    else
    {
      DrawReadonlyFieldLocal("mask_binary", object->segmentation_mask_ref);
      DrawReadonlyFieldLocal("mask_overlay", object->segmentation_overlay_ref);
      DrawReadonlyFieldLocal("contours", object->segmentation_contour_ref);
      DrawReadonlyFieldLocal("result_json", object->segmentation_result_ref);
    }
  }

  if (ImGui::CollapsingHeader("Shape Attach", ImGuiTreeNodeFlags_DefaultOpen))
  {
    if (object == nullptr)
    {
      ImGui::TextDisabled("No shape attach state.");
    }
    else if (object->type == "FindSegmentation")
    {
      ImGui::Text("boundary_polyline: %s",
                  object->segmentation_has_boundary ? "available" : "pending");
      ImGui::Text("boundary_bbox: %s",
                  object->segmentation_has_boundary ? "available_from_contour" : "pending");
      ImGui::Text("mask shape state: %s",
                  object->segmentation_real_mask_attach_ready ? "attach_ready" : "pending_binding");
      DrawReadonlyFieldLocal("contour_ref", object->segmentation_contour_ref);
    }
    else
    {
      ImGui::Text("boundary_polyline: %s",
                  object->torch_primary_visual_ref.empty() ? "pending" : object->torch_primary_visual_ref.c_str());
      ImGui::Text("boundary_bbox: %s",
                  object->torch_result_count > 0 ? "available_if_projected" : "PENDING_DATA");
      ImGui::Text("mask shape state: %s",
                  object->torch_mask_available != 0 ? "mask_available" : "pending");
    }
  }

  if (ImGui::CollapsingHeader("Contract / Review", ImGuiTreeNodeFlags_DefaultOpen))
  {
    DrawReadonlyFieldLocal("contract_status", context.current_evidence_selection.status);
    DrawReadonlyFieldLocal("contract_reason", context.current_evidence_selection.reason);
    DrawReadonlyFieldLocal("human_review_status", context.current_gauge.review_status);
    const bool promotionAllowed =
        context.current_gauge.review_status == "accepted" &&
        object != nullptr &&
        ((object->type == "TorchTask" && object->torch_ok != 0) ||
         (object->type == "FindSegmentation" && object->segmentation_real_mask_attach_ready));
    ImGui::Text("promotion_allowed: %s", promotionAllowed ? "candidate_ready_for_gate" : "false");
  }

  if (ImGui::CollapsingHeader("Training Curve / Param Map", ImGuiTreeNodeFlags_DefaultOpen))
  {
    const ImVec2 plotSize(520.0f, 180.0f);
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 p1(p0.x + plotSize.x, p0.y + plotSize.y);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(p0, p1, IM_COL32(18, 18, 18, 255));
    draw->AddRect(p0, p1, IM_COL32(230, 230, 230, 255));
    for (int gx = 0; gx <= 8; ++gx)
    {
      const float x = p0.x + plotSize.x * gx / 8.0f;
      draw->AddLine(ImVec2(x, p0.y), ImVec2(x, p1.y), IM_COL32(75, 75, 75, 180));
    }
    for (int gy = 0; gy <= 4; ++gy)
    {
      const float y = p0.y + plotSize.y * gy / 4.0f;
      draw->AddLine(ImVec2(p0.x, y), ImVec2(p1.x, y), IM_COL32(75, 75, 75, 180));
    }
    draw->AddText(ImVec2(p0.x + 8.0f, p0.y + 6.0f),
                  IM_COL32(240, 240, 240, 255),
                  "epoch-loss / IoU curve");
    draw->AddText(ImVec2(p0.x + 8.0f, p1.y - 22.0f),
                  IM_COL32(255, 210, 80, 255),
                  "pending real curve samples");
    if (object != nullptr && object->type == "TorchTask" &&
        !object->torch_trainer_lifecycle_summary.empty())
    {
      draw->AddCircleFilled(ImVec2(p0.x + plotSize.x * 0.85f, p0.y + plotSize.y * 0.35f),
                            5.0f,
                            IM_COL32(120, 255, 160, 255));
      draw->AddText(ImVec2(p0.x + plotSize.x * 0.85f + 8.0f, p0.y + plotSize.y * 0.35f - 8.0f),
                    IM_COL32(220, 255, 220, 255),
                    "best checkpoint marker");
    }
    ImGui::Dummy(plotSize);
    if (object != nullptr && object->type == "TorchTask")
    {
      DrawReadonlyFieldLocal("trainer_summary", object->torch_trainer_lifecycle_summary);
      DrawReadonlyFieldLocal("mainline_summary", object->torch_unified_mainline_summary);
    }
  }
}

void DrawKeyParameterUnavailableNotice(const ManualTestContext& context)
{
  if (!ImGui::CollapsingHeader("关键参数 UI / 参数整定图", ImGuiTreeNodeFlags_DefaultOpen))
    return;
  ImGui::TextColored(ImVec4(1.0f, 0.68f, 0.25f, 1.0f),
                     "Current image/tool is not suitable for Findline/FindCircle key-parameter tuning.");
  ImGui::TextWrapped(
    "Select or create a Line/Circle annotation tool, or load a script containing Findline/FindCircle. "
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
  const bool keyParamSuitable = IsFindLineFindCircleContext(context);

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
    ImGui::BulletText("key parameter UI: %s", keyParamSuitable ? "visible" : "waiting for Findline/FindCircle");
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
    ImGui::TableSetupColumn("FindLine");
    ImGui::TableSetupColumn("FindCircle");
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
  if (g.has_line_gauge || g.tool != "FindCircle")
  {
    g.wgap = std::max(1, ui.sample_points);
    g.hgap = std::max(1, ui.valid_length_percent);
  }
  if (g.has_circle_gauge || g.tool == "FindCircle")
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

static std::string NormalizeKeyParamToolTypeLocal(const std::string& type)
{
  if (type == "Findline" || type == "FindLine") return "FindLine";
  if (type == "Findcircle" || type == "FindCircle") return "FindCircle";
  if (type == "Findellipse" || type == "FindEllipse") return "FindEllipse";
  if (type == "Findrect" || type == "FindRect") return "FindRect";
  if (type == "fastmatch" || type == "FastMatch" || type == "CFastMatch") return "FastMatch";
  return type;
}

static void ApplyPrimaryObjectToCurrentGauge(
    ManualTestContext& context,
    const CxEvidenceEditableObjectRef& ref,
    const char* status)
{
  ManualGaugeState& gauge = context.current_gauge;
  const std::string tool = NormalizeKeyParamToolTypeLocal(ref.type);

  gauge.primary_object_type = tool;
  gauge.primary_object_name = ref.name;
  gauge.primary_object_status = status == nullptr ? "manual_object_selected" : status;
  gauge.tool = tool;
  gauge.has_line_gauge = tool == "FindLine";
  gauge.has_circle_gauge = tool == "FindCircle";
  gauge.has_ellipse_gauge = tool == "FindEllipse";
  gauge.dirty = true;
  gauge.review_status = "editing";

  context.current_evidence_selection.primary_object_type = tool;
  context.current_evidence_selection.primary_object_name = ref.name;
  context.current_evidence_selection.primary_object_status =
      gauge.primary_object_status;
  context.debug_status = "PRIMARY_OBJECT_SELECTED";
  context.debug_reason =
      "Key Parameter Controls selected primary object: " +
      tool + " " + ref.name;
}

static void DrawPrimaryObjectSelector(ManualTestContext& context)
{
  ManualGaugeState& gauge = context.current_gauge;
  const CxEvidenceSelectionSnapshot& snapshot =
      context.current_evidence_selection;

  ImGui::Text("Primary Object: %s %s | %s",
              gauge.primary_object_type.empty() ? "-" : gauge.primary_object_type.c_str(),
              gauge.primary_object_name.empty() ? "-" : gauge.primary_object_name.c_str(),
              gauge.primary_object_status.empty() ? "-" : gauge.primary_object_status.c_str());

  if (!snapshot.valid || snapshot.editable_objects.empty())
  {
    ImGui::TextDisabled("No editable object candidates from selected Evidence row.");
    return;
  }

  int currentIndex = -1;
  for (int i = 0; i < static_cast<int>(snapshot.editable_objects.size()); ++i)
  {
    const auto& ref = snapshot.editable_objects[static_cast<std::size_t>(i)];
    if (ref.name == gauge.primary_object_name &&
        NormalizeKeyParamToolTypeLocal(ref.type) ==
            NormalizeKeyParamToolTypeLocal(gauge.primary_object_type))
    {
      currentIndex = i;
      break;
    }
  }

  std::string preview = currentIndex >= 0
      ? NormalizeKeyParamToolTypeLocal(
            snapshot.editable_objects[static_cast<std::size_t>(currentIndex)].type) +
            " " + snapshot.editable_objects[static_cast<std::size_t>(currentIndex)].name
      : "Select primary editable object";

  ImGui::SetNextItemWidth(320.0f);
  if (ImGui::BeginCombo("Primary Object##evidence_primary_object", preview.c_str()))
  {
    for (int i = 0; i < static_cast<int>(snapshot.editable_objects.size()); ++i)
    {
      const auto& ref = snapshot.editable_objects[static_cast<std::size_t>(i)];
      const std::string label =
          NormalizeKeyParamToolTypeLocal(ref.type) + " " + ref.name +
          "  line " + std::to_string(ref.declared_line);
      const bool selected = i == currentIndex;
      if (ImGui::Selectable(label.c_str(), selected))
      {
        ApplyPrimaryObjectToCurrentGauge(
            context,
            ref,
            "manual_object_selected");
      }
      if (selected)
        ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }

  if (gauge.primary_object_status == "needs_object_selection" ||
      gauge.primary_object_status == "needs_object_selection_no_tool_match")
  {
    ImGui::TextColored(ImVec4(1.0f, 0.68f, 0.25f, 1.0f),
                       "Multiple editable objects are present. Select the object before tuning parameters.");
  }
}

static const RuntimeObjectView* FindCurrentFindLineObject(
    const ManualTestContext& context)
{
  const std::string& primary = context.current_gauge.primary_object_name;
  if (!primary.empty())
  {
    for (const RuntimeObjectView& object : context.runtime_objects)
    {
      if (object.type == "FindLine" && object.name == primary)
        return &object;
    }
  }

  for (const RuntimeObjectView& object : context.runtime_objects)
  {
    if (object.type == "FindLine")
      return &object;
  }
  return nullptr;
}

static ManualFindLineEdgeParamState MakeFindLineEdgeParamsFromGauge(
    const ManualGaugeState& gauge)
{
  ManualFindLineEdgeParamState params;
  params.initialized = true;
  params.threshold = gauge.threshold;
  params.method = gauge.method;
  params.linegap = gauge.linegap;
  params.wgap = gauge.wgap;
  params.hgap = gauge.hgap;
  params.filterprofile = gauge.filterprofile;
  return params;
}

static void ApplyFindLineEdgeParamsToGauge(
    const ManualFindLineEdgeParamState& params,
    ManualGaugeState& gauge)
{
  gauge.threshold = params.threshold;
  gauge.method = params.method;
  gauge.linegap = params.linegap;
  gauge.wgap = params.wgap;
  gauge.hgap = params.hgap;
  gauge.filterprofile = params.filterprofile;
}

static void EnsureFindLineEdgeParamStorage(ManualTestContext& context)
{
  context.findline_scan_edge_count =
      std::max(1, std::min(16, context.findline_scan_edge_count));
  context.findline_selected_scan_edge =
      std::max(0, std::min(context.findline_selected_scan_edge,
                           context.findline_scan_edge_count));

  const std::size_t required =
      static_cast<std::size_t>(context.findline_scan_edge_count + 1);
  if (context.findline_edge_params.size() < required)
    context.findline_edge_params.resize(required);

  for (int i = 1; i <= context.findline_scan_edge_count; ++i)
  {
    ManualFindLineEdgeParamState& params =
        context.findline_edge_params[static_cast<std::size_t>(i)];
    if (!params.initialized)
      params = MakeFindLineEdgeParamsFromGauge(context.current_gauge);
  }
}

static bool DrawFindLineEdgeSelectorPanel(ManualTestContext& context)
{
  if (!(context.current_gauge.tool == "FindLine" ||
        context.current_gauge.has_line_gauge))
  {
    return false;
  }

  bool edited = false;
  EnsureFindLineEdgeParamStorage(context);

  ImGui::Separator();
  ImGui::TextUnformatted("Detection Edge / Point Column");
  ImGui::TextDisabled(
      "Select which gauge side/point column is being tuned.  Edge-specific values are saved to globals for evidence replay.");

  ImGui::SetNextItemWidth(100.0f);
  int edgeCount = context.findline_scan_edge_count;
  if (ImGui::InputInt("edge count", &edgeCount))
  {
    context.findline_scan_edge_count = std::max(1, std::min(16, edgeCount));
    if (context.findline_selected_scan_edge >
        context.findline_scan_edge_count)
    {
      context.findline_selected_scan_edge =
          context.findline_scan_edge_count;
    }
    EnsureFindLineEdgeParamStorage(context);
    edited = true;
  }

  ImGui::SameLine();
  ImGui::SetNextItemWidth(190.0f);
  std::string currentLabel = context.findline_selected_scan_edge == 0
      ? "All edges"
      : ("Edge " + std::to_string(context.findline_selected_scan_edge));
  if (ImGui::BeginCombo("selected edge", currentLabel.c_str()))
  {
    const bool selectedAll = context.findline_selected_scan_edge == 0;
    if (ImGui::Selectable("All edges", selectedAll))
    {
      context.findline_selected_scan_edge = 0;
      edited = true;
    }
    for (int i = 1; i <= context.findline_scan_edge_count; ++i)
    {
      const std::string label = "Edge " + std::to_string(i);
      const bool selected = context.findline_selected_scan_edge == i;
      if (ImGui::Selectable(label.c_str(), selected))
      {
        context.findline_selected_scan_edge = i;
        EnsureFindLineEdgeParamStorage(context);
        ApplyFindLineEdgeParamsToGauge(
            context.findline_edge_params[static_cast<std::size_t>(i)],
            context.current_gauge);
        edited = true;
      }
    }
    ImGui::EndCombo();
  }

  if (context.findline_selected_scan_edge > 0)
  {
    ManualFindLineEdgeParamState& params =
        context.findline_edge_params[
            static_cast<std::size_t>(context.findline_selected_scan_edge)];
    params.initialized = true;

    ImGui::Text("Current: Edge %d / %d",
                context.findline_selected_scan_edge,
                context.findline_scan_edge_count);

    ImGui::SetNextItemWidth(100.0f);
    edited |= ImGui::InputInt("edge threshold", &params.threshold);
    params.threshold = std::max(0, std::min(255, params.threshold));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.0f);
    edited |= ImGui::InputInt("edge method", &params.method);
    params.method = std::max(0, std::min(3, params.method));

    ImGui::SetNextItemWidth(100.0f);
    edited |= ImGui::InputInt("edge linegap", &params.linegap);
    params.linegap = std::max(0, std::min(50, params.linegap));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.0f);
    edited |= ImGui::InputInt("edge wgap", &params.wgap);
    params.wgap = std::max(0, std::min(50, params.wgap));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.0f);
    edited |= ImGui::InputInt("edge hgap", &params.hgap);
    params.hgap = std::max(0, std::min(50, params.hgap));

    ImGui::SetNextItemWidth(100.0f);
    edited |= ImGui::InputInt("edge filterprofile", &params.filterprofile);
    params.filterprofile = std::max(0, std::min(10, params.filterprofile));

    if (edited)
      ApplyFindLineEdgeParamsToGauge(params, context.current_gauge);

    if (ImGui::Button("Copy Current Edge Params To All"))
    {
      for (int i = 1; i <= context.findline_scan_edge_count; ++i)
      {
        context.findline_edge_params[static_cast<std::size_t>(i)] = params;
        context.findline_edge_params[static_cast<std::size_t>(i)].initialized = true;
      }
      edited = true;
    }
  }
  else
  {
    ImGui::TextDisabled(
        "All edges selected: Geometry/Edge Params below edit the shared baseline.");
    if (ImGui::Button("Copy Shared Params To All Edges"))
    {
      const ManualFindLineEdgeParamState params =
          MakeFindLineEdgeParamsFromGauge(context.current_gauge);
      for (int i = 1; i <= context.findline_scan_edge_count; ++i)
        context.findline_edge_params[static_cast<std::size_t>(i)] = params;
      edited = true;
    }
  }

  return edited;
}

static void DrawFindLineEdgeEvaluationPanel(ManualTestContext& context)
{
  if (!(context.current_gauge.tool == "FindLine" ||
        context.current_gauge.has_line_gauge))
  {
    return;
  }

  const RuntimeObjectView* object = FindCurrentFindLineObject(context);

  ImGui::Separator();
  ImGui::TextUnformatted("Edge Result Evaluation");
  ImGui::TextDisabled(
      "Runtime evidence for Edge 1 / Edge 2 / ... .  Best edge is a basic score from accepted points and coverage; it does not replace human review.");

  if (object == nullptr)
  {
    ImGui::TextDisabled("Run script to collect per-edge result evidence.");
    return;
  }

  ImGui::Text("selected_edge=%d evaluated_edges=%d best_edge=%d best_score=%.2f",
              object->line_selected_edge_index,
              object->line_evaluated_edge_count,
              object->line_best_edge_index,
              object->line_best_edge_score);

  if (object->line_edge_evaluations.empty())
  {
    ImGui::TextDisabled(
        "No edge evaluation captured. Re-run a FindLine script that calls measure().");
    return;
  }

  const ImGuiTableFlags flags =
      ImGuiTableFlags_Borders |
      ImGuiTableFlags_RowBg |
      ImGuiTableFlags_Resizable |
      ImGuiTableFlags_SizingStretchProp;

  if (ImGui::BeginTable("findline_edge_evaluation_table", 9, flags))
  {
    ImGui::TableSetupColumn("Edge");
    ImGui::TableSetupColumn("Sel");
    ImGui::TableSetupColumn("Best");
    ImGui::TableSetupColumn("Rows");
    ImGui::TableSetupColumn("Accepted");
    ImGui::TableSetupColumn("Coverage");
    ImGui::TableSetupColumn("Score");
    ImGui::TableSetupColumn("Reject");
    ImGui::TableSetupColumn("Fit?");
    ImGui::TableHeadersRow();

    for (const CxFindLineEdgeEvaluationSnapshot& edge :
         object->line_edge_evaluations)
    {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::Text("Edge %d", edge.edge_index);
      ImGui::TableSetColumnIndex(1);
      ImGui::TextUnformatted(edge.selected ? "*" : "");
      ImGui::TableSetColumnIndex(2);
      ImGui::TextUnformatted(
          edge.edge_index == object->line_best_edge_index ? "best" : "");
      ImGui::TableSetColumnIndex(3);
      ImGui::Text("%d", edge.candidate_scan_rows);
      ImGui::TableSetColumnIndex(4);
      ImGui::Text("%d", edge.accepted_points);
      ImGui::TableSetColumnIndex(5);
      ImGui::Text("%.3f", edge.coverage);
      ImGui::TableSetColumnIndex(6);
      ImGui::Text("%.1f", edge.score);
      ImGui::TableSetColumnIndex(7);
      ImGui::Text("sel=%d end=%d len=%d",
                  edge.rejected_by_selection,
                  edge.rejected_near_endpoint,
                  edge.over_length_runs);
      ImGui::TableSetColumnIndex(8);
      ImGui::TextUnformatted(edge.fit_possible ? "yes" : "no");
    }

    ImGui::EndTable();
  }
}

static void DrawFindLineScanSemanticsPanel(ManualTestContext& context)
{
  if (!(context.current_gauge.tool == "FindLine" ||
        context.current_gauge.has_line_gauge))
  {
    return;
  }

  const RuntimeObjectView* object = FindCurrentFindLineObject(context);
  const int linegap = std::max(1, context.current_gauge.linegap);
  const double lineLength = object != nullptr && object->line_length > 0.0
      ? object->line_length
      : std::hypot(
            static_cast<double>(context.current_gauge.line_x1 -
                                context.current_gauge.line_x0),
            static_cast<double>(context.current_gauge.line_y1 -
                                context.current_gauge.line_y0));
  const int previewTicks = lineLength > 1.0
      ? std::max(1, static_cast<int>(std::floor(lineLength / linegap)) + 1)
      : 0;

  ImGui::Separator();
  ImGui::TextUnformatted("Gauge Scan Semantics");
  ImGui::TextColored(ImVec4(1.0f, 0.92f, 0.45f, 1.0f),
                     "scan tick = sampling opportunity");
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.18f, 1.0f),
                     "| accepted point = algorithm result");

  if (object == nullptr)
  {
    ImGui::TextDisabled("Run script to show accepted/rejected counters.");
    ImGui::Text("preview_ticks=%d linegap=%d method=%d",
                previewTicks,
                linegap,
                context.current_gauge.method);
    return;
  }

  ImGui::Text("preview_ticks=%d scan_rows_examined=%d foreground_rows=%d",
              previewTicks,
              object->line_scan_rows_examined,
              object->line_scan_rows_with_foreground);
  ImGui::Text("scan_runs=%d emitted=%d accepted_points=%d valid_points=%d",
              object->line_scan_runs_total,
              object->line_scan_points_emitted,
              object->line_measure_points_count,
              object->valid_line_points_count);
  ImGui::Text("rejected_near_endpoint=%d rejected_by_selection=%d over_length=%d",
              object->line_scan_runs_rejected_near_endpoint,
              object->line_scan_runs_rejected_by_selection,
              object->line_scan_runs_over_length_limit);
  ImGui::Text("fit_line=%s linegap=%d method=%d threshold=%d",
              object->has_fit_line ? "true" : "false",
              object->line_measure_linegap,
              object->line_measure_method,
              object->line_measure_threshold);
}

void DrawKeyParameterControlPanel(ManualTestContext& context)
{
  ManualGaugeState& gauge = context.current_gauge;
  bool gaugeEdited = false;

  ImGui::TextUnformatted("Tool: ");
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "%s", gauge.tool.c_str());
  DrawPrimaryObjectSelector(context);
  if (gauge.tool == "FindLine" || gauge.has_line_gauge)
  {
      ImGui::Checkbox("Show single-line gauge scan ticks",
                      &context.show_line_gauge_scan_lines);
      gaugeEdited |= DrawFindLineEdgeSelectorPanel(context);
      DrawFindLineEdgeEvaluationPanel(context);
      DrawFindLineScanSemanticsPanel(context);
  }
  ImGui::Separator();

  ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
  if (ImGui::CollapsingHeader("Geometry"))
  {
      ImGui::PushID("geometry");
      if (gauge.tool == "FindCircle" || gauge.has_circle_gauge)
      {
          ImGui::SetNextItemWidth(120.0f); gaugeEdited |= ImGui::InputInt("cx", &gauge.circle_cx);
          ImGui::SameLine(); ImGui::SetNextItemWidth(120.0f); gaugeEdited |= ImGui::InputInt("cy", &gauge.circle_cy);
          ImGui::SameLine(); ImGui::SetNextItemWidth(120.0f); gaugeEdited |= ImGui::InputInt("radius", &gauge.radius);

          ImGui::SetNextItemWidth(120.0f); gaugeEdited |= ImGui::InputInt("inner_radius", &gauge.inner_radius);
          ImGui::SameLine(); ImGui::SetNextItemWidth(120.0f); gaugeEdited |= ImGui::InputInt("outer_radius", &gauge.outer_radius);
          ImGui::SameLine(); ImGui::SetNextItemWidth(120.0f); gaugeEdited |= ImGui::InputInt("gap", &gauge.gap);
      }
      else
      {
          ImGui::SetNextItemWidth(120.0f); gaugeEdited |= ImGui::InputInt("x0", &gauge.line_x0);
          ImGui::SameLine(); ImGui::SetNextItemWidth(120.0f); gaugeEdited |= ImGui::InputInt("y0", &gauge.line_y0);
          ImGui::SameLine(); ImGui::SetNextItemWidth(120.0f); gaugeEdited |= ImGui::InputInt("half_width", &gauge.tool_half_width);

          ImGui::SetNextItemWidth(120.0f); gaugeEdited |= ImGui::InputInt("x1", &gauge.line_x1);
          ImGui::SameLine(); ImGui::SetNextItemWidth(120.0f); gaugeEdited |= ImGui::InputInt("y1", &gauge.line_y1);
      }
      ImGui::PopID();
  }

  ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
  if (ImGui::CollapsingHeader("Edge Params"))
  {
      ImGui::PushID("edge_params");

      ImGui::TextUnformatted("threshold");
      ImGui::SameLine(80.0f);
      ImGui::SetNextItemWidth(180.0f); gaugeEdited |= ImGui::SliderInt("##threshold", &gauge.threshold, 0, 255);
      ImGui::SameLine(); ImGui::SetNextItemWidth(70.0f); gaugeEdited |= ImGui::InputInt("##t_val", &gauge.threshold);
      gauge.threshold = std::max(0, std::min(255, gauge.threshold));

      ImGui::TextUnformatted("method");
      ImGui::SameLine(80.0f);
      const char* methods[] = {"0", "1", "2", "3"};
      ImGui::SetNextItemWidth(100.0f); gaugeEdited |= ImGui::Combo("##method", &gauge.method, methods, IM_ARRAYSIZE(methods));

      ImGui::TextUnformatted("linegap");
      ImGui::SameLine(80.0f);
      ImGui::SetNextItemWidth(180.0f); gaugeEdited |= ImGui::SliderInt("##linegap", &gauge.linegap, 0, 50);
      ImGui::SameLine(); ImGui::SetNextItemWidth(70.0f); gaugeEdited |= ImGui::InputInt("##lg_val", &gauge.linegap);
      gauge.linegap = std::max(0, std::min(50, gauge.linegap));

      if (gauge.tool == "FindCircle" || gauge.has_circle_gauge)
      {
          ImGui::TextUnformatted("gap");
          ImGui::SameLine(80.0f);
          ImGui::SetNextItemWidth(180.0f); gaugeEdited |= ImGui::SliderInt("##gap", &gauge.gap, 0, 200);
          ImGui::SameLine(); ImGui::SetNextItemWidth(70.0f); gaugeEdited |= ImGui::InputInt("##gap_val", &gauge.gap);
          gauge.gap = std::max(0, std::min(200, gauge.gap));
      }
      else
      {
          ImGui::TextUnformatted("wgap");
          ImGui::SameLine(80.0f);
          ImGui::SetNextItemWidth(180.0f); gaugeEdited |= ImGui::SliderInt("##wgap", &gauge.wgap, 0, 50);
          ImGui::SameLine(); ImGui::SetNextItemWidth(70.0f); gaugeEdited |= ImGui::InputInt("##wg_val", &gauge.wgap);
          gauge.wgap = std::max(0, std::min(50, gauge.wgap));

          ImGui::TextUnformatted("hgap");
          ImGui::SameLine(80.0f);
          ImGui::SetNextItemWidth(180.0f); gaugeEdited |= ImGui::SliderInt("##hgap", &gauge.hgap, 0, 50);
          ImGui::SameLine(); ImGui::SetNextItemWidth(70.0f); gaugeEdited |= ImGui::InputInt("##hg_val", &gauge.hgap);
          gauge.hgap = std::max(0, std::min(50, gauge.hgap));

          ImGui::TextUnformatted("filterprofile");
          ImGui::SameLine(80.0f);
          ImGui::SetNextItemWidth(180.0f); gaugeEdited |= ImGui::SliderInt("##fp", &gauge.filterprofile, 0, 10);
          ImGui::SameLine(); ImGui::SetNextItemWidth(70.0f); gaugeEdited |= ImGui::InputInt("##fp_val", &gauge.filterprofile);
          gauge.filterprofile = std::max(0, std::min(10, gauge.filterprofile));
      }
      ImGui::PopID();
  }

  if (gaugeEdited)
  {
    gauge.dirty = true;
    gauge.review_status = "editing";
    ++context.key_parameter_edit_revision;
    context.last_key_parameter_edit_summary =
        "threshold=" + std::to_string(gauge.threshold) +
        " method=" + std::to_string(gauge.method) +
        " linegap=" + std::to_string(gauge.linegap) +
        " wgap=" + std::to_string(gauge.wgap) +
        " hgap=" + std::to_string(gauge.hgap) +
        " filterprofile=" + std::to_string(gauge.filterprofile) +
        " selected_edge=" +
        std::to_string(context.findline_selected_scan_edge) +
        "/" + std::to_string(context.findline_scan_edge_count) +
        " roi=(" + std::to_string(gauge.line_x0) + "," +
        std::to_string(gauge.line_y0) + "," +
        std::to_string(gauge.line_x1) + "," +
        std::to_string(gauge.line_y1) + ")";
    CXLOG_INFO(
        "KeyParameterControls",
        "key_parameter_ui_edit",
        "edited",
        "revision=" + std::to_string(context.key_parameter_edit_revision) +
        " " + context.last_key_parameter_edit_summary);
  }

  ImGui::Separator();
  ImGui::TextUnformatted("Actions");

  const float btnWidth = (ImGui::GetContentRegionAvail().x - 30.0f) / 3.0f;

  ImGui::PushID("actions");
  if (ImGui::Button("Apply To Gauge", ImVec2(btnWidth, 0)))
  {
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
    gauge.dirty = true;
    gauge.review_status = "editing";
    context.debug_action = "Key Parameter Controls Run Script";
    if (ApplyManualGaugeToGlobals(context))
    {
      context.pending_execution_gauge = context.current_gauge;
      context.pending_execution_globals = context.runtime_int_vars;
      context.has_pending_execution_snapshot = true;
      context.debug_status = "MANUAL_RUN_REQUESTED";
      context.debug_reason =
          "Run requested from Key Parameter Controls; Debug Compiler will execute exact Script Editor text";
      context.run_state = "running";
    }
  }

  if (ImGui::Button("Save Draft Candidate", ImVec2(btnWidth, 0)))
  {
    gauge.dirty = true;
    gauge.review_status = "editing";
    context.debug_action = "Save Evidence Candidate";
    if (ApplyManualGaugeToGlobals(context))
    {
      CxEvidenceCandidateSaveOptions options;
      options.mode = "draft";
      options.request_run = false;
      CxEvidenceCandidateSaveResult result;
      if (!SaveEvidenceCandidatePackage(context, options, result))
      {
        context.debug_status = "EVIDENCE_CANDIDATE_SAVE_FAILED";
        context.debug_reason = result.reason;
      }
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Save And Run Candidate", ImVec2(btnWidth, 0)))
  {
    gauge.dirty = true;
    gauge.review_status = "editing";
    context.debug_action = "Save And Run Evidence Candidate";
    if (ApplyManualGaugeToGlobals(context))
    {
      // Freeze exactly what was saved.  The Debug Compiler executes on a
      // later UI pass, after Evidence rows and runtime results may refresh.
      context.pending_execution_gauge = context.current_gauge;
      context.pending_execution_globals = context.runtime_int_vars;
      context.has_pending_execution_snapshot = true;
      CxEvidenceCandidateSaveOptions options;
      options.mode = "run_requested";
      options.request_run = true;
      CxEvidenceCandidateSaveResult result;
      if (!SaveEvidenceCandidatePackage(context, options, result))
      {
        context.debug_status = "EVIDENCE_CANDIDATE_SAVE_FAILED";
        context.debug_reason = result.reason;
      }
      else
      {
        // This identifier is the durable run request.  Do not use the mutable
        // Debug UI status as the only trigger for a deferred candidate run.
        context.pending_execution_candidate_id = result.candidate_id;
      }
    }
  }

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
