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
#include <cfloat>
#include <cstdlib>
#include <cmath>
#include <sstream>
#include <fstream>
#include <vector>

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
    return (g.tool == "FindLine" || g.tool == "FindCircle" ||
            g.tool == "FindEllipse" || g.tool == "FindRect" ||
            g.tool == "FindSegmentation" ||
            g.tool == "FastMatch" || g.tool == "fastmatch" ||
            g.tool == "CFastMatch" || g.tool == "GridPatternClassTool" ||
            g.tool == "RegionPatternTool") ||
           g.has_line_gauge || g.has_circle_gauge || g.has_ellipse_gauge;
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

struct TorchCurveSampleLocal
{
  std::string label;
  double value = 0.0;
};

static bool ReadTorchTextFileLocal(const std::string& path, std::string& text)
{
  text.clear();
  if (path.empty())
    return false;
  std::ifstream input(path, std::ios::binary);
  if (!input.is_open())
    return false;
  std::ostringstream buffer;
  buffer << input.rdbuf();
  text = buffer.str();
  return true;
}

static bool ExtractTorchJsonNumberLocal(
    const std::string& json,
    const std::string& key,
    double& value)
{
  const std::string needle = "\"" + key + "\"";
  std::size_t pos = json.find(needle);
  if (pos == std::string::npos)
    return false;
  pos = json.find(':', pos + needle.size());
  if (pos == std::string::npos)
    return false;
  ++pos;
  while (pos < json.size() &&
         std::isspace(static_cast<unsigned char>(json[pos])))
  {
    ++pos;
  }
  const char* begin = json.c_str() + pos;
  char* end = nullptr;
  const double parsed = std::strtod(begin, &end);
  if (begin == end || !std::isfinite(parsed))
    return false;
  value = parsed;
  return true;
}

static bool ExtractTorchJsonBoolLocal(
    const std::string& json,
    const std::string& key,
    bool& value)
{
  const std::string needle = "\"" + key + "\"";
  std::size_t pos = json.find(needle);
  if (pos == std::string::npos)
    return false;
  pos = json.find(':', pos + needle.size());
  if (pos == std::string::npos)
    return false;
  ++pos;
  while (pos < json.size() &&
         std::isspace(static_cast<unsigned char>(json[pos])))
  {
    ++pos;
  }
  if (json.compare(pos, 4, "true") == 0)
  {
    value = true;
    return true;
  }
  if (json.compare(pos, 5, "false") == 0)
  {
    value = false;
    return true;
  }
  return false;
}

static std::vector<TorchCurveSampleLocal> BuildTorchCurveSamplesLocal(
    const RuntimeObjectView& object)
{
  std::vector<TorchCurveSampleLocal> samples;
  std::string json;
  double number = 0.0;
  bool flag = false;

  if (ReadTorchTextFileLocal(object.torch_result_ref, json))
  {
    if (ExtractTorchJsonNumberLocal(json, "smoke_loss", number))
      samples.push_back({"loss", number});
    if (ExtractTorchJsonNumberLocal(json, "grad_mean", number))
      samples.push_back({"grad", number});
    if (ExtractTorchJsonNumberLocal(json, "foreground_ratio", number))
      samples.push_back({"fg", number});
    if (ExtractTorchJsonNumberLocal(json, "infer_runtime_ms", number))
      samples.push_back({"infer_ms", number});
    if (ExtractTorchJsonNumberLocal(json, "train_runtime_ms", number))
      samples.push_back({"train_ms", number});
  }

  if (ReadTorchTextFileLocal(object.torch_evidence_ref, json))
  {
    if (ExtractTorchJsonNumberLocal(json, "epochs", number))
      samples.push_back({"epochs", number});
    if (ExtractTorchJsonBoolLocal(json, "finite_loss", flag))
      samples.push_back({"finite_loss", flag ? 1.0 : 0.0});
    if (ExtractTorchJsonNumberLocal(json, "grad_mean", number))
      samples.push_back({"grad_evidence", number});
  }

  return samples;
}

static std::vector<TorchCurveSampleLocal> BuildFindSegmentationCurveSamplesLocal(
    const RuntimeObjectView& object)
{
  std::vector<TorchCurveSampleLocal> samples;
  std::string json;
  double number = 0.0;

  if (ReadTorchTextFileLocal(object.segmentation_result_ref, json))
  {
    if (ExtractTorchJsonNumberLocal(json, "foreground_ratio", number))
      samples.push_back({"fg", number});
    if (ExtractTorchJsonNumberLocal(json, "contour_count", number))
      samples.push_back({"contours", number});
    if (ExtractTorchJsonNumberLocal(json, "changed_pixels", number))
      samples.push_back({"changed", number});
  }

  if (object.segmentation_contour_count > 0)
    samples.push_back({"contours_live", static_cast<double>(object.segmentation_contour_count)});
  if (object.segmentation_primary_area > 0.0)
    samples.push_back({"area", object.segmentation_primary_area});

  return samples;
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

  if (ImGui::CollapsingHeader("Training Image Set", ImGuiTreeNodeFlags_DefaultOpen))
  {
    int trainCount = 0;
    int valCount = 0;
    int testCount = 0;
    int goodCount = 0;
    int anomalyCount = 0;
    int unlabeledCount = 0;
    for (const TorchTrainingImageItem& item : context.torch_training_images)
    {
      if (item.split == "train")
        ++trainCount;
      else if (item.split == "val")
        ++valCount;
      else if (item.split == "test")
        ++testCount;

      if (item.label == "good")
        ++goodCount;
      else if (item.label == "anomaly")
        ++anomalyCount;
      else
        ++unlabeledCount;
    }

    DrawReadonlyFieldLocal("dataset_status", context.torch_training_image_status);
    DrawReadonlyFieldLocal("dataset_reason", context.torch_training_image_reason);
    ImGui::Text("split_count: train=%d val=%d test=%d",
                trainCount,
                valCount,
                testCount);
    ImGui::Text("label_count: good=%d anomaly=%d unlabeled_or_pending=%d",
                goodCount,
                anomalyCount,
                unlabeledCount);
    ImGui::TextColored(ImVec4(0.60f, 0.82f, 1.0f, 1.0f),
                       "Open the 'Torch Training Image Set' window for thumbnail rails and label editing.");
  }

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
    std::vector<TorchCurveSampleLocal> samples;
    if (object != nullptr && object->type == "TorchTask")
      samples = BuildTorchCurveSamplesLocal(*object);
    else if (object != nullptr && object->type == "FindSegmentation")
      samples = BuildFindSegmentationCurveSamplesLocal(*object);

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
                  "torch runtime metric curve");
    if (samples.empty())
    {
      draw->AddText(ImVec2(p0.x + 8.0f, p1.y - 22.0f),
                    IM_COL32(255, 210, 80, 255),
                    "pending real curve samples");
    }
    else
    {
      double maxAbs = 0.0;
      for (const TorchCurveSampleLocal& sample : samples)
        maxAbs = std::max(maxAbs, std::abs(sample.value));
      if (maxAbs <= 0.0)
        maxAbs = 1.0;

      ImVec2 prev;
      bool hasPrev = false;
      for (std::size_t i = 0; i < samples.size(); ++i)
      {
        const float t = samples.size() <= 1
            ? 0.5f
            : static_cast<float>(i) / static_cast<float>(samples.size() - 1);
        const double normalized = std::max(-1.0, std::min(1.0, samples[i].value / maxAbs));
        const float x = p0.x + 36.0f + (plotSize.x - 72.0f) * t;
        const float y = p1.y - 30.0f -
            static_cast<float>((normalized + 1.0) * 0.5) * (plotSize.y - 60.0f);
        const ImVec2 pt(x, y);
        if (hasPrev)
          draw->AddLine(prev, pt, IM_COL32(120, 255, 160, 255), 2.0f);
        draw->AddCircleFilled(pt, 4.5f, IM_COL32(120, 255, 160, 255));
        draw->AddText(ImVec2(pt.x + 5.0f, pt.y - 12.0f),
                      IM_COL32(220, 255, 220, 255),
                      samples[i].label.c_str());
        prev = pt;
        hasPrev = true;
      }
      draw->AddText(ImVec2(p0.x + 8.0f, p1.y - 22.0f),
                    IM_COL32(120, 255, 160, 255),
                    "real samples from torch result/evidence json");
    }
    ImGui::Dummy(plotSize);
    if (object != nullptr && object->type == "TorchTask")
    {
      DrawReadonlyFieldLocal("curve_samples_count", static_cast<int>(samples.size()));
      DrawReadonlyFieldLocal("trainer_summary", object->torch_trainer_lifecycle_summary);
      DrawReadonlyFieldLocal("mainline_summary", object->torch_unified_mainline_summary);
    }
    else if (object != nullptr && object->type == "FindSegmentation")
    {
      DrawReadonlyFieldLocal("curve_samples_count", static_cast<int>(samples.size()));
      DrawReadonlyFieldLocal("segmentation_result_ref", object->segmentation_result_ref);
      DrawReadonlyFieldLocal("segmentation_overlay_ref", object->segmentation_overlay_ref);
    }
  }
}

void DrawKeyParameterUnavailableNotice(const ManualTestContext& context)
{
  if (!ImGui::CollapsingHeader("关键参数 UI / 参数整定图", ImGuiTreeNodeFlags_DefaultOpen))
    return;
  ImGui::TextColored(ImVec4(1.0f, 0.68f, 0.25f, 1.0f),
                     "Current image/tool is not suitable for key-parameter tuning.");
  ImGui::TextWrapped(
    "Select or create a Line/Circle annotation tool, or load a script containing Findline/FindCircle/FastMatch. "
    "FastMatch uses Learn ROI + Match ROI plus staged Learn/Match parameters.");
  ImGui::Text("current gauge tool=%s line_gauge=%s circle_gauge=%s fastmatch=%s",
              context.current_gauge.tool.c_str(),
              context.current_gauge.has_line_gauge ? "yes" : "no",
              context.current_gauge.has_circle_gauge ? "yes" : "no",
              (context.current_gauge.tool == "FastMatch" ||
               context.current_gauge.tool == "fastmatch" ||
               context.current_gauge.tool == "CFastMatch") ? "yes" : "no");
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

  const std::size_t catalogSeedCount = context.catalog_entries.size();
  const bool shouldSeedEvidenceItems =
      context.evidence_items.empty() &&
      (!context.evidence_items_seed_attempted ||
       context.evidence_items_seed_catalog_count != catalogSeedCount);

  if (shouldSeedEvidenceItems)
  {
    ImGui::TextDisabled("No evidence cases loaded.");
    ImGui::Text("Loading from catalog entries...");

    const std::size_t beforeCount = context.evidence_items.size();
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
    context.evidence_items_seed_attempted = true;
    context.evidence_items_seed_catalog_count = catalogSeedCount;
    if (context.evidence_items.size() != beforeCount)
    {
      context.script_evidence_groups_dirty = true;
      context.workbench_assets_loaded = false;
    }
  }
  else if (context.evidence_items.empty())
  {
    ImGui::TextDisabled("No evidence cases loaded.");
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
  if (type == "FindSegmentation" || type == "findsegmentation" ||
      type == "Findsegmentation" || type == "segmentation")
    return "FindSegmentation";
  if (type == "fastmatch" || type == "FastMatch" || type == "CFastMatch") return "FastMatch";
  if (type == "gridpatternclasstool" || type == "GridPatternClassTool") return "GridPatternClassTool";
  if (type == "regionpatterntool" || type == "RegionPatternTool") return "RegionPatternTool";
  return type;
}

static int RuntimeIntOr(
    const ManualTestContext& context,
    const std::string& key,
    int fallback)
{
  const auto it = context.runtime_int_vars.find(key);
  if (it == context.runtime_int_vars.end())
    return fallback;
  return it->second;
}

static bool DrawRuntimeIntRow(
    ManualTestContext& context,
    const char* label,
    const char* key,
    int fallback,
    int minValue,
    int maxValue,
    float labelWidth = 110.0f)
{
  int value = RuntimeIntOr(context, key, fallback);
  value = std::max(minValue, std::min(maxValue, value));
  bool edited = false;
  ImGui::TextUnformatted(label);
  ImGui::SameLine(labelWidth);
  ImGui::SetNextItemWidth(180.0f);
  edited |= ImGui::SliderInt((std::string("##slider_") + key).c_str(),
                             &value,
                             minValue,
                             maxValue);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(80.0f);
  edited |= ImGui::InputInt((std::string("##input_") + key).c_str(), &value);
  value = std::max(minValue, std::min(maxValue, value));
  if (edited || context.runtime_int_vars.find(key) == context.runtime_int_vars.end())
    InjectManualGaugeInt(context, key, value);
  return edited;
}

static bool DrawFastMatchRoiControls(ManualTestContext& context)
{
  bool edited = false;
  ImGui::TextColored(
      ImVec4(0.4f, 0.8f, 1.0f, 1.0f),
      "FastMatch ROI ranges");
  ImGui::TextDisabled(
      "Learn ROI builds the template model; Match ROI limits the search range.");

  if (ImGui::BeginTable("fastmatch_roi_table", 5,
                        ImGuiTableFlags_Borders |
                            ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_SizingStretchProp))
  {
    ImGui::TableSetupColumn("Range");
    ImGui::TableSetupColumn("x");
    ImGui::TableSetupColumn("y");
    ImGui::TableSetupColumn("w");
    ImGui::TableSetupColumn("h");
    ImGui::TableHeadersRow();

    auto drawRoiRow =
        [&](const char* rowLabel,
            const char* keyX,
            const char* keyY,
            const char* keyW,
            const char* keyH,
            int fallbackX,
            int fallbackY,
            int fallbackW,
            int fallbackH)
    {
      int x = RuntimeIntOr(context, keyX, fallbackX);
      int y = RuntimeIntOr(context, keyY, fallbackY);
      int w = RuntimeIntOr(context, keyW, fallbackW);
      int h = RuntimeIntOr(context, keyH, fallbackH);
      x = std::max(0, std::min(10000, x));
      y = std::max(0, std::min(10000, y));
      w = std::max(1, std::min(10000, w));
      h = std::max(1, std::min(10000, h));

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(rowLabel);
      ImGui::TableSetColumnIndex(1);
      ImGui::SetNextItemWidth(-FLT_MIN);
      edited |= ImGui::InputInt((std::string("##") + keyX).c_str(), &x);
      ImGui::TableSetColumnIndex(2);
      ImGui::SetNextItemWidth(-FLT_MIN);
      edited |= ImGui::InputInt((std::string("##") + keyY).c_str(), &y);
      ImGui::TableSetColumnIndex(3);
      ImGui::SetNextItemWidth(-FLT_MIN);
      edited |= ImGui::InputInt((std::string("##") + keyW).c_str(), &w);
      ImGui::TableSetColumnIndex(4);
      ImGui::SetNextItemWidth(-FLT_MIN);
      edited |= ImGui::InputInt((std::string("##") + keyH).c_str(), &h);

      x = std::max(0, std::min(10000, x));
      y = std::max(0, std::min(10000, y));
      w = std::max(1, std::min(10000, w));
      h = std::max(1, std::min(10000, h));
      InjectManualGaugeInt(context, keyX, x);
      InjectManualGaugeInt(context, keyY, y);
      InjectManualGaugeInt(context, keyW, w);
      InjectManualGaugeInt(context, keyH, h);
    };

    drawRoiRow("Learn ROI", "global_learn_roi_x", "global_learn_roi_y",
               "global_learn_roi_w", "global_learn_roi_h",
               120, 120, 120, 90);
    drawRoiRow("Match ROI", "global_search_roi_x", "global_search_roi_y",
               "global_search_roi_w", "global_search_roi_h",
               0, 0, 640, 480);
    ImGui::EndTable();
  }
  return edited;
}

static bool DrawFindSegmentationPromptControls(ManualTestContext& context)
{
  ManualGaugeState& gauge = context.current_gauge;
  bool edited = false;

  if (!gauge.has_segmentation_prompt_rect)
  {
    gauge.segmentation_prompt_x0 =
        RuntimeIntOr(context, "global_roi_x0", gauge.segmentation_prompt_x0);
    gauge.segmentation_prompt_y0 =
        RuntimeIntOr(context, "global_roi_y0", gauge.segmentation_prompt_y0);
    gauge.segmentation_prompt_x1 =
        RuntimeIntOr(context, "global_roi_x1", gauge.segmentation_prompt_x1);
    gauge.segmentation_prompt_y1 =
        RuntimeIntOr(context, "global_roi_y1", gauge.segmentation_prompt_y1);
    gauge.segmentation_mode =
        RuntimeIntOr(context, "global_segmentation_mode", gauge.segmentation_mode);
    gauge.has_segmentation_prompt_rect = true;
  }

  ImGui::TextColored(
      ImVec4(0.35f, 0.88f, 0.62f, 1.0f),
      "FindSegmentation Prompt ROI");
  ImGui::TextDisabled(
      "Prompt rect is the editable region shown in Image View and exported to global_roi_*.");

  if (ImGui::BeginTable("findsegmentation_prompt_rect_table", 5,
                        ImGuiTableFlags_Borders |
                            ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_SizingStretchProp))
  {
    ImGui::TableSetupColumn("Prompt");
    ImGui::TableSetupColumn("x0");
    ImGui::TableSetupColumn("y0");
    ImGui::TableSetupColumn("x1");
    ImGui::TableSetupColumn("y1");
    ImGui::TableHeadersRow();
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted("Prompt ROI");
    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-FLT_MIN);
    edited |= ImGui::InputInt("##seg_prompt_x0", &gauge.segmentation_prompt_x0);
    ImGui::TableSetColumnIndex(2);
    ImGui::SetNextItemWidth(-FLT_MIN);
    edited |= ImGui::InputInt("##seg_prompt_y0", &gauge.segmentation_prompt_y0);
    ImGui::TableSetColumnIndex(3);
    ImGui::SetNextItemWidth(-FLT_MIN);
    edited |= ImGui::InputInt("##seg_prompt_x1", &gauge.segmentation_prompt_x1);
    ImGui::TableSetColumnIndex(4);
    ImGui::SetNextItemWidth(-FLT_MIN);
    edited |= ImGui::InputInt("##seg_prompt_y1", &gauge.segmentation_prompt_y1);
    ImGui::EndTable();
  }

  gauge.segmentation_prompt_x0 = std::max(0, gauge.segmentation_prompt_x0);
  gauge.segmentation_prompt_y0 = std::max(0, gauge.segmentation_prompt_y0);
  gauge.segmentation_prompt_x1 =
      std::max(gauge.segmentation_prompt_x0 + 1,
               gauge.segmentation_prompt_x1);
  gauge.segmentation_prompt_y1 =
      std::max(gauge.segmentation_prompt_y0 + 1,
               gauge.segmentation_prompt_y1);

  ImGui::TextUnformatted("mode");
  ImGui::SameLine(80.0f);
  ImGui::SetNextItemWidth(120.0f);
  edited |= ImGui::InputInt("##segmentation_mode", &gauge.segmentation_mode);
  gauge.segmentation_mode = std::max(0, std::min(16, gauge.segmentation_mode));

  InjectManualGaugeInt(context, "global_roi_x0", gauge.segmentation_prompt_x0);
  InjectManualGaugeInt(context, "global_roi_y0", gauge.segmentation_prompt_y0);
  InjectManualGaugeInt(context, "global_roi_x1", gauge.segmentation_prompt_x1);
  InjectManualGaugeInt(context, "global_roi_y1", gauge.segmentation_prompt_y1);
  InjectManualGaugeInt(context, "global_roi_x", gauge.segmentation_prompt_x0);
  InjectManualGaugeInt(context, "global_roi_y", gauge.segmentation_prompt_y0);
  InjectManualGaugeInt(
      context,
      "global_roi_width",
      gauge.segmentation_prompt_x1 - gauge.segmentation_prompt_x0);
  InjectManualGaugeInt(
      context,
      "global_roi_height",
      gauge.segmentation_prompt_y1 - gauge.segmentation_prompt_y0);
  InjectManualGaugeInt(
      context,
      "global_segmentation_mode",
      gauge.segmentation_mode);

  return edited;
}

static bool DrawFastMatchLearnParameterControls(ManualTestContext& context)
{
  ManualGaugeState& gauge = context.current_gauge;
  bool edited = false;
  ImGui::TextColored(
      ImVec4(0.4f, 0.8f, 1.0f, 1.0f),
      "Learn Setup");
  ImGui::TextDisabled(
      "Maps to: setrect + setobjfilter + SetWHgap + setthre + setlinegap + setcomparegap.");

  ImGui::TextUnformatted("learn_threshold");
  ImGui::SameLine(130.0f);
  ImGui::SetNextItemWidth(180.0f);
  edited |= ImGui::SliderInt("##fm_learn_threshold", &gauge.threshold, 0, 255);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(80.0f);
  edited |= ImGui::InputInt("##fm_learn_threshold_value", &gauge.threshold);
  gauge.threshold = std::max(0, std::min(255, gauge.threshold));

  ImGui::TextUnformatted("learn_method");
  ImGui::SameLine(130.0f);
  const char* methods[] = {"0", "1", "2", "3"};
  ImGui::SetNextItemWidth(100.0f);
  edited |= ImGui::Combo("##fm_learn_method", &gauge.method, methods, IM_ARRAYSIZE(methods));

  ImGui::TextUnformatted("learn_linegap");
  ImGui::SameLine(130.0f);
  ImGui::SetNextItemWidth(180.0f);
  edited |= ImGui::SliderInt("##fm_learn_linegap", &gauge.linegap, 0, 50);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(80.0f);
  edited |= ImGui::InputInt("##fm_learn_linegap_value", &gauge.linegap);
  gauge.linegap = std::max(0, std::min(50, gauge.linegap));

  ImGui::TextUnformatted("learn_wgap");
  ImGui::SameLine(130.0f);
  ImGui::SetNextItemWidth(180.0f);
  edited |= ImGui::SliderInt("##fm_learn_wgap", &gauge.wgap, 0, 100);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(80.0f);
  edited |= ImGui::InputInt("##fm_learn_wgap_value", &gauge.wgap);
  gauge.wgap = std::max(0, std::min(100, gauge.wgap));

  ImGui::TextUnformatted("learn_hgap");
  ImGui::SameLine(130.0f);
  ImGui::SetNextItemWidth(180.0f);
  edited |= ImGui::SliderInt("##fm_learn_hgap", &gauge.hgap, 0, 100);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(80.0f);
  edited |= ImGui::InputInt("##fm_learn_hgap_value", &gauge.hgap);
  gauge.hgap = std::max(0, std::min(100, gauge.hgap));

  edited |= DrawRuntimeIntRow(
      context, "objfilter", "global_objfilter", 1, 0, 10, 130.0f);
  edited |= DrawRuntimeIntRow(
      context, "compare_gap", "global_compare_gap", 20, 1, 200, 130.0f);

  ImGui::TextUnformatted("filterprofile");
  ImGui::SameLine(130.0f);
  ImGui::SetNextItemWidth(180.0f);
  edited |= ImGui::SliderInt("##fm_learn_filterprofile", &gauge.filterprofile, 0, 10);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(80.0f);
  edited |= ImGui::InputInt("##fm_learn_filterprofile_value", &gauge.filterprofile);
  gauge.filterprofile = std::max(0, std::min(10, gauge.filterprofile));

  InjectManualGaugeInt(context, "global_threshold", gauge.threshold);
  InjectManualGaugeInt(context, "global_method", gauge.method);
  InjectManualGaugeInt(context, "global_linegap", gauge.linegap);
  InjectManualGaugeInt(context, "global_wgap", gauge.wgap);
  InjectManualGaugeInt(context, "global_hgap", gauge.hgap);
  InjectManualGaugeInt(context, "global_filterprofile", gauge.filterprofile);
  return edited;
}

static bool DrawGridPatternRoiControls(ManualTestContext& context)
{
  bool edited = false;
  ImGui::TextColored(
      ImVec4(0.35f, 0.88f, 0.62f, 1.0f),
      "GridPattern Analysis ROI");
  ImGui::TextDisabled(
      "This ROI is the region-content input. FastMatch Search ROI is not consumed by this CASE.");
  edited |= DrawRuntimeIntRow(context, "analysis x", "global_learn_roi_x", 120, 0, 10000, 130.0f);
  edited |= DrawRuntimeIntRow(context, "analysis y", "global_learn_roi_y", 120, 0, 10000, 130.0f);
  edited |= DrawRuntimeIntRow(context, "analysis width", "global_learn_roi_w", 120, 1, 10000, 130.0f);
  edited |= DrawRuntimeIntRow(context, "analysis height", "global_learn_roi_h", 90, 1, 10000, 130.0f);
  return edited;
}

static bool DrawRegionPatternRoiControls(ManualTestContext& context)
{
  bool edited = false;
  ImGui::TextColored(
      ImVec4(0.9f, 0.72f, 0.32f, 1.0f),
      "RegionPattern Analysis ROI");
  ImGui::TextDisabled(
      "This ROI feeds the region-content descriptor. It is independent from FastMatch learn/search ROI.");
  edited |= DrawRuntimeIntRow(context, "region x", "global_region_roi_x", 120, 0, 10000, 130.0f);
  edited |= DrawRuntimeIntRow(context, "region y", "global_region_roi_y", 120, 0, 10000, 130.0f);
  edited |= DrawRuntimeIntRow(context, "region width", "global_region_roi_w", 120, 1, 10000, 130.0f);
  edited |= DrawRuntimeIntRow(context, "region height", "global_region_roi_h", 90, 1, 10000, 130.0f);
  return edited;
}

static bool DrawRegionPatternParameterControls(ManualTestContext& context)
{
  bool edited = false;
  ImGui::TextColored(
      ImVec4(0.9f, 0.72f, 0.32f, 1.0f),
      "Region Pattern Content Descriptor");
  ImGui::TextDisabled(
      "Region-content direction: normalized gray/binary pooling. Classifier binding and semantic accuracy are not claimed here.");

  edited |= DrawRuntimeIntRow(context, "normalized width", "global_region_normalized_width", 32, 8, 512, 160.0f);
  edited |= DrawRuntimeIntRow(context, "normalized height", "global_region_normalized_height", 32, 8, 512, 160.0f);
  edited |= DrawRuntimeIntRow(context, "pooling rows", "global_region_pooling_rows", 4, 1, 64, 160.0f);
  edited |= DrawRuntimeIntRow(context, "pooling cols", "global_region_pooling_cols", 4, 1, 64, 160.0f);
  edited |= DrawRuntimeIntRow(context, "use binary", "global_region_use_binary", 0, 0, 1, 160.0f);
  edited |= DrawRuntimeIntRow(context, "threshold", "global_region_threshold", 128, 0, 255, 160.0f);
  edited |= DrawRuntimeIntRow(context, "foreground dark", "global_region_foreground_dark", 1, 0, 1, 160.0f);
  edited |= DrawRuntimeIntRow(context, "max overlay blocks", "global_region_max_overlays", 64, 1, 512, 160.0f);

  ImGui::TextDisabled(
      "View chain: ROI -> pooled_region_block overlays -> descriptor metrics -> manual texture review.");
  return edited;
}

static bool DrawFastMatchMatchParameterControls(ManualTestContext& context)
{
  bool edited = false;
  ImGui::TextColored(
      ImVec4(0.4f, 0.8f, 1.0f, 1.0f),
      "Match Test Setup");
  ImGui::TextDisabled(
      "Maps to: setmatchrect + matchstepgap + setmatchthre + setminscore + setfindnum.");

  edited |= DrawRuntimeIntRow(
      context, "match_step_x", "global_match_step_x", 10, 1, 100, 130.0f);
  edited |= DrawRuntimeIntRow(
      context, "match_step_y", "global_match_step_y", 10, 1, 100, 130.0f);
  edited |= DrawRuntimeIntRow(
      context, "match_threshold", "global_match_thre", 10, 0, 255, 130.0f);
  edited |= DrawRuntimeIntRow(
      context, "min_score %", "global_min_score_percent", 65, 0, 100, 130.0f);
  edited |= DrawRuntimeIntRow(
      context, "find_num", "global_find_num", 1, 1, 20, 130.0f);
  return edited;
}

static bool DrawGridPatternParameterControls(ManualTestContext& context)
{
  bool edited = false;
  ImGui::TextColored(
      ImVec4(0.35f, 0.88f, 0.62f, 1.0f),
      "Grid Pattern Class Network (experimental)");
  ImGui::TextDisabled(
      "Region-content direction: normalized grid features and 3-5 pooled levels. FastMatch defaults are unchanged.");

  edited |= DrawRuntimeIntRow(context, "normalized width", "global_grid_normalized_width", 48, 16, 512, 160.0f);
  edited |= DrawRuntimeIntRow(context, "normalized height", "global_grid_normalized_height", 48, 16, 512, 160.0f);
  edited |= DrawRuntimeIntRow(context, "grid rows", "global_grid_rows", 12, 2, 64, 160.0f);
  edited |= DrawRuntimeIntRow(context, "grid cols", "global_grid_cols", 12, 2, 64, 160.0f);
  edited |= DrawRuntimeIntRow(context, "pooled levels", "global_grid_levels", 3, 3, 5, 160.0f);
  edited |= DrawRuntimeIntRow(context, "orientation bins", "global_grid_orientation_bins", 8, 2, 36, 160.0f);
  edited |= DrawRuntimeIntRow(context, "foreground threshold", "global_grid_foreground_threshold", -1, -1, 255, 160.0f);
  edited |= DrawRuntimeIntRow(context, "foreground dark", "global_grid_foreground_dark", 1, 0, 1, 160.0f);
  edited |= DrawRuntimeIntRow(context, "equalize contrast", "global_grid_equalize_contrast", 0, 0, 1, 160.0f);
  edited |= DrawRuntimeIntRow(context, "active foreground %", "global_grid_active_foreground_percent", 5, 0, 100, 160.0f);
  edited |= DrawRuntimeIntRow(context, "active edge %", "global_grid_active_edge_percent", 3, 0, 100, 160.0f);
  edited |= DrawRuntimeIntRow(context, "max overlay cells", "global_grid_max_overlays", 96, 1, 512, 160.0f);
  edited |= DrawRuntimeIntRow(context, "fusion mode", "global_grid_fusion_mode", 2, 0, 3, 160.0f);

  ImGui::TextDisabled(
      "fusion mode: 0 structural-only, 1 grid-only, 2 cascade, 3 score fusion. This CASE records the value but does not replace FastMatch matching.");
  return edited;
}

static void RequestFastMatchRunAction(
    ManualTestContext& context,
    int actionCode,
    const char* actionLabel)
{
  InjectManualGaugeInt(context, "global_fastmatch_action", actionCode);
  context.current_gauge.dirty = true;
  context.current_gauge.review_status = "editing";
  context.debug_action = actionLabel == nullptr
      ? "FastMatch Action"
      : actionLabel;
  if (ApplyManualGaugeToGlobals(context))
  {
    context.pending_execution_gauge = context.current_gauge;
    context.pending_execution_globals = context.runtime_int_vars;
    context.has_pending_execution_snapshot = true;
    context.debug_status = "FASTMATCH_RUN_REQUESTED";
    context.debug_reason =
        std::string(actionLabel == nullptr ? "FastMatch action" : actionLabel) +
        " requested; cxscript may use global_fastmatch_action to branch learn/match.";
    context.run_state = "running";
  }
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
  gauge.has_segmentation_prompt_rect = tool == "FindSegmentation";
  if (tool == "FindSegmentation")
  {
    gauge.segmentation_prompt_x0 =
        RuntimeIntOr(context, "global_roi_x0", gauge.segmentation_prompt_x0);
    gauge.segmentation_prompt_y0 =
        RuntimeIntOr(context, "global_roi_y0", gauge.segmentation_prompt_y0);
    gauge.segmentation_prompt_x1 =
        RuntimeIntOr(context, "global_roi_x1", gauge.segmentation_prompt_x1);
    gauge.segmentation_prompt_y1 =
        RuntimeIntOr(context, "global_roi_y1", gauge.segmentation_prompt_y1);
    gauge.segmentation_mode =
        RuntimeIntOr(context, "global_segmentation_mode", gauge.segmentation_mode);
  }
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

static const RuntimeObjectView* FindCurrentFindCircleObject(
    const ManualTestContext& context)
{
  const std::string& primary = context.current_gauge.primary_object_name;
  if (!primary.empty())
  {
    for (const RuntimeObjectView& object : context.runtime_objects)
    {
      if (object.type == "FindCircle" && object.name == primary)
        return &object;
    }
  }

  for (const RuntimeObjectView& object : context.runtime_objects)
  {
    if (object.type == "FindCircle")
      return &object;
  }
  return nullptr;
}

static const RuntimeObjectView* FindCurrentFindEllipseObject(
    const ManualTestContext& context)
{
  const std::string& primary = context.current_gauge.primary_object_name;
  if (!primary.empty())
  {
    for (const RuntimeObjectView& object : context.runtime_objects)
    {
      if (object.type == "FindEllipse" && object.name == primary)
        return &object;
    }
  }

  for (const RuntimeObjectView& object : context.runtime_objects)
  {
    if (object.type == "FindEllipse")
      return &object;
  }
  return nullptr;
}

static const RuntimeObjectView* FindCurrentFastMatchObject(
    const ManualTestContext& context)
{
  const std::string& primary = context.current_gauge.primary_object_name;
  if (!primary.empty())
  {
    for (const RuntimeObjectView& object : context.runtime_objects)
    {
      if (object.type == "FastMatch" && object.name == primary)
        return &object;
    }
  }

  for (const RuntimeObjectView& object : context.runtime_objects)
  {
    if (object.type == "FastMatch")
      return &object;
  }
  return nullptr;
}

static void DrawFastMatchTemplateStatusPanel(
    const ManualTestContext& context)
{
  const RuntimeObjectView* object = FindCurrentFastMatchObject(context);

  ImGui::Separator();
  ImGui::TextUnformatted("FastMatch Template / Match Evidence");
  ImGui::TextDisabled(
      "Learn must publish model points and template evidence; Match must publish candidate/result evidence.");

  if (object == nullptr)
  {
    ImGui::TextDisabled(
        "No FastMatch runtime object is available. Run a FastMatch learn/match script first.");
    return;
  }

  ImGui::Text("model_points=%d learn_status=%d",
              object->fastmatch_model_point_count,
              object->fastmatch_learn_status_code);
  ImGui::Text("learn sets: A=%d B=%d A2=%d B2=%d",
              object->fastmatch_learn_a_count,
              object->fastmatch_learn_b_count,
              object->fastmatch_learn_a2_count,
              object->fastmatch_learn_b2_count);
  ImGui::Text("template patterns: A=%d B=%d",
              object->fastmatch_pattern_a_count,
              object->fastmatch_pattern_b_count);
  ImGui::Text("match candidates=%d best_score=%.3f",
              object->fastmatch_candidate_count,
              object->fastmatch_best_score);
  ImGui::Text("learn ROI=(%d,%d)-(%d,%d)",
              object->fastmatch_learn_rect_x0,
              object->fastmatch_learn_rect_y0,
              object->fastmatch_learn_rect_x1,
              object->fastmatch_learn_rect_y1);
  ImGui::Text("match ROI=(%d,%d)-(%d,%d)",
              object->fastmatch_match_rect_x0,
              object->fastmatch_match_rect_y0,
              object->fastmatch_match_rect_x1,
              object->fastmatch_match_rect_y1);

  const ImGuiTableFlags flags =
      ImGuiTableFlags_Borders |
      ImGuiTableFlags_RowBg |
      ImGuiTableFlags_Resizable |
      ImGuiTableFlags_SizingStretchProp;
  if (ImGui::BeginTable("fastmatch_template_status_table", 4, flags))
  {
    ImGui::TableSetupColumn("Asset");
    ImGui::TableSetupColumn("Count");
    ImGui::TableSetupColumn("State");
    ImGui::TableSetupColumn("Meaning");
    ImGui::TableHeadersRow();

    auto row = [](const char* asset, int count, const char* state, const char* meaning)
    {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(asset);
      ImGui::TableSetColumnIndex(1); ImGui::Text("%d", count);
      ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(state);
      ImGui::TableSetColumnIndex(3); ImGui::TextUnformatted(meaning);
    };

    row("Learn points",
        object->fastmatch_model_point_count,
        object->fastmatch_model_point_count > 0 ? "available" : "pending",
        "Image View should show learn ROI and model point evidence.");
    row("Template A",
        object->fastmatch_pattern_a_count,
        object->fastmatch_pattern_a_count > 0 ? "available" : "pending",
        "Template list source A.");
    row("Template B",
        object->fastmatch_pattern_b_count,
        object->fastmatch_pattern_b_count > 0 ? "available" : "pending",
        "Template list source B.");
    row("Match candidates",
        object->fastmatch_candidate_count,
        object->fastmatch_candidate_count > 0 ? "available" : "pending",
        "Image View should show search ROI and match result boxes.");

    ImGui::EndTable();
  }
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
      std::max(-1, std::min(context.findline_selected_scan_edge,
                           context.findline_scan_edge_count));
  context.findline_best_fit_edge =
      std::max(0, std::min(context.findline_best_fit_edge,
                           context.findline_scan_edge_count));
  context.findline_recommended_fit_edge =
      std::max(0, std::min(context.findline_recommended_fit_edge,
                           context.findline_scan_edge_count));
  context.findline_relation_edge =
      std::max(0, std::min(context.findline_relation_edge,
                           context.findline_scan_edge_count));
  context.findline_attach_edge =
      std::max(0, std::min(context.findline_attach_edge,
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

static std::string FindLineEdgeLabel(int edge)
{
  return edge == 0 ? "All edges" : ("Edge " + std::to_string(edge));
}

static std::string FindLineSelectedEdgeLabel(int edge, int edgeCount)
{
  (void)edgeCount;
  if (edge == -1)
    return "Last edge";
  if (edge == 0)
    return "All edges";
  return FindLineEdgeLabel(edge);
}

static ManualFindCircleEdgeParamState MakeFindCircleEdgeParamsFromGauge(
    const ManualGaugeState& gauge)
{
  ManualFindCircleEdgeParamState params;
  params.initialized = true;
  params.threshold = gauge.threshold;
  params.method = gauge.method;
  params.linegap = gauge.linegap;
  params.gap = gauge.gap;
  return params;
}

static void EnsureFindCircleEdgeParamStorage(ManualTestContext& context)
{
  context.findcircle_scan_edge_count =
      std::max(1, std::min(32, context.findcircle_scan_edge_count));
  context.findcircle_selected_scan_edge =
      std::max(-1, std::min(context.findcircle_selected_scan_edge,
                           context.findcircle_scan_edge_count));
  context.findcircle_best_fit_edge =
      std::max(0, std::min(context.findcircle_best_fit_edge,
                           context.findcircle_scan_edge_count));
  context.findcircle_recommended_fit_edge =
      std::max(0, std::min(context.findcircle_recommended_fit_edge,
                           context.findcircle_scan_edge_count));
  context.findcircle_relation_edge =
      std::max(0, std::min(context.findcircle_relation_edge,
                           context.findcircle_scan_edge_count));
  context.findcircle_attach_edge =
      std::max(0, std::min(context.findcircle_attach_edge,
                           context.findcircle_scan_edge_count));

  const std::size_t required =
      static_cast<std::size_t>(context.findcircle_scan_edge_count + 1);
  if (context.findcircle_edge_params.size() < required)
    context.findcircle_edge_params.resize(required);

  for (int i = 1; i <= context.findcircle_scan_edge_count; ++i)
  {
    ManualFindCircleEdgeParamState& params =
        context.findcircle_edge_params[static_cast<std::size_t>(i)];
    // FindCircle has one algorithm parameter set. Edge 1..N only selects the
    // Nth eligible boundary crossing on each radial scan line; it must never
    // select another threshold/method/gap profile. Keep the legacy per-edge
    // storage as a compatibility mirror for saved assets and old globals.
    params = MakeFindCircleEdgeParamsFromGauge(context.current_gauge);
  }
}

static std::string FindCircleEdgeLabel(int edge)
{
  // This is the candidate-edge ordinal on every radial scan line.  It is not
  // an angular sector; the CircleShape A0/A1 controls own that geometry.
  return edge == 0 ? "All edges" : ("Edge " + std::to_string(edge));
}

static std::string FindCircleSelectedEdgeLabel(int edge, int edgeCount)
{
  (void)edgeCount;
  if (edge == -1)
    return "Last edge";
  if (edge == 0)
    return "All edges";
  return FindCircleEdgeLabel(edge);
}

static void SyncFindCircleEdgeSelectionToGlobals(
    ManualTestContext& context,
    const char* reason)
{
  EnsureFindCircleEdgeParamStorage(context);
  InjectManualGaugeInt(
      context,
      "global_findcircle_edge_count",
      context.findcircle_scan_edge_count);
  InjectManualGaugeInt(
      context,
      "global_findcircle_selected_edge",
      context.findcircle_selected_scan_edge);
  InjectManualGaugeInt(
      context,
      "global_findcircle_best_edge",
      context.findcircle_best_fit_edge);
  InjectManualGaugeInt(
      context,
      "global_findcircle_recommended_edge",
      context.findcircle_recommended_fit_edge);
  InjectManualGaugeInt(
      context,
      "global_findcircle_relation_edge",
      context.findcircle_relation_edge);
  InjectManualGaugeInt(
      context,
      "global_findcircle_attach_edge",
      context.findcircle_attach_edge);

  context.current_gauge.dirty = true;
  context.current_gauge.review_status = "editing";
  context.debug_status = "FINDCIRCLE_EDGE_SELECTION_CHANGED";
  context.debug_reason =
      std::string(reason == nullptr ? "FindCircle edge selection changed" : reason) +
      "; globals updated, run script to refresh result points";
}

static bool DrawFindCircleEdgeRoleCombo(
    const char* label,
    int& edge,
    int edgeCount)
{
  bool edited = false;
  edge = std::max(0, std::min(edge, edgeCount));
  const std::string currentLabel = FindCircleEdgeLabel(edge);
  ImGui::SetNextItemWidth(150.0f);
  if (ImGui::BeginCombo(label, currentLabel.c_str()))
  {
    if (ImGui::Selectable("All edges", edge == 0))
    {
      edge = 0;
      edited = true;
    }
    for (int i = 1; i <= edgeCount; ++i)
    {
      const std::string item = FindCircleEdgeLabel(i);
      if (ImGui::Selectable(item.c_str(), edge == i))
      {
        edge = i;
        edited = true;
      }
    }
    ImGui::EndCombo();
  }
  return edited;
}

static bool DrawFindLineEdgeRoleCombo(
    const char* label,
    int& edge,
    int edgeCount)
{
  bool edited = false;
  edge = std::max(0, std::min(edge, edgeCount));
  const std::string currentLabel = FindLineEdgeLabel(edge);
  ImGui::SetNextItemWidth(150.0f);
  if (ImGui::BeginCombo(label, currentLabel.c_str()))
  {
    if (ImGui::Selectable("All edges", edge == 0))
    {
      edge = 0;
      edited = true;
    }
    for (int i = 1; i <= edgeCount; ++i)
    {
      const std::string item = FindLineEdgeLabel(i);
      if (ImGui::Selectable(item.c_str(), edge == i))
      {
        edge = i;
        edited = true;
      }
    }
    ImGui::EndCombo();
  }
  return edited;
}

static bool DrawFindCircleEdgeSelectorPanel(ManualTestContext& context)
{
  if (!(context.current_gauge.tool == "FindCircle" ||
        context.current_gauge.has_circle_gauge))
  {
    return false;
  }

  bool edited = false;
  EnsureFindCircleEdgeParamStorage(context);

  ImGui::Separator();
  ImGui::Text("Algorithm scan sector: %s", context.current_gauge.circle_arc_enabled
      ? ("A0=" + std::to_string(context.current_gauge.circle_arc_start_deg) +
         " deg, A1=" + std::to_string(context.current_gauge.circle_arc_end_deg) + " deg").c_str()
      : "full 360 deg");
  ImGui::TextDisabled("A0/A1 controls the angular scan domain. Edit it in Geometry; Edge selection below does not change it.");
  ImGui::TextUnformatted("Detection Edge / Point Column");
  ImGui::TextDisabled(
      "Edge N is the Nth eligible boundary crossing on every radial Gauge line. It is not an angular sector.");
  ImGui::TextDisabled(
      "Threshold/method/linegap/gap are one shared FindCircle parameter set; changing Edge never changes them.");

  ImGui::SetNextItemWidth(100.0f);
  int edgeCount = context.findcircle_scan_edge_count;
  if (ImGui::InputInt("edge count", &edgeCount))
  {
    context.findcircle_scan_edge_count = std::max(1, std::min(32, edgeCount));
    if (context.findcircle_selected_scan_edge >
        context.findcircle_scan_edge_count)
    {
      context.findcircle_selected_scan_edge =
          context.findcircle_scan_edge_count;
    }
    EnsureFindCircleEdgeParamStorage(context);
    SyncFindCircleEdgeSelectionToGlobals(
        context,
        "FindCircle edge count changed");
    edited = true;
  }

  ImGui::SameLine();
  ImGui::SetNextItemWidth(190.0f);
  const std::string currentLabel =
      FindCircleSelectedEdgeLabel(
          context.findcircle_selected_scan_edge,
          context.findcircle_scan_edge_count);
  if (ImGui::BeginCombo("selected edge", currentLabel.c_str()))
  {
    if (ImGui::Selectable("All edges", context.findcircle_selected_scan_edge == 0))
    {
      context.findcircle_selected_scan_edge = 0;
      SyncFindCircleEdgeSelectionToGlobals(
          context,
          "FindCircle selected All edges");
      edited = true;
    }
    for (int i = 1; i <= context.findcircle_scan_edge_count; ++i)
    {
      const std::string label = "Edge " + std::to_string(i);
      if (ImGui::Selectable(label.c_str(),
                            context.findcircle_selected_scan_edge == i))
      {
        context.findcircle_selected_scan_edge = i;
        const std::string reason = "FindCircle selected " + label;
        SyncFindCircleEdgeSelectionToGlobals(
            context,
            reason.c_str());
        edited = true;
      }
    }
    ImGui::Separator();
    if (ImGui::Selectable(
            "Last edge",
            context.findcircle_selected_scan_edge == -1))
    {
      context.findcircle_selected_scan_edge = -1;
      SyncFindCircleEdgeSelectionToGlobals(
          context,
          "FindCircle selected Last edge");
      edited = true;
    }
    ImGui::EndCombo();
  }

  if (context.findcircle_selected_scan_edge == -1)
  {
    ImGui::Text("Current: Last edge on every radial Gauge line");
    ImGui::TextDisabled(
        "Last edge is resolved independently on each scan line after all eligible crossings are collected.");
  }
  else if (context.findcircle_selected_scan_edge > 0)
  {
    ImGui::Text("Current: Edge %d / %d",
                context.findcircle_selected_scan_edge,
                context.findcircle_scan_edge_count);
    ImGui::TextDisabled(
        "The shared Edge Params panel below remains unchanged; only the candidate ordinal changes.");
  }
  else
  {
    ImGui::TextDisabled(
        "All edges selected: Geometry/Edge Params below edit the shared baseline.");
  }

  return edited;
}

static bool DrawFindCircleEdgeRolePanel(ManualTestContext& context)
{
  if (!(context.current_gauge.tool == "FindCircle" ||
        context.current_gauge.has_circle_gauge))
  {
    return false;
  }

  bool edited = false;
  EnsureFindCircleEdgeParamStorage(context);

  ImGui::Separator();
  ImGui::TextUnformatted("Edge Role Binding / 接入点");
  ImGui::TextDisabled(
      "Selection is an edge ordinal. Best/recommended/relation/attach are evidence metadata; none changes the scan sector.");

  ImGui::PushID("findcircle_edge_roles");
  edited |= DrawFindCircleEdgeRoleCombo(
      "best edge", context.findcircle_best_fit_edge,
      context.findcircle_scan_edge_count);
  ImGui::SameLine();
  if (ImGui::Button("Use All Edges"))
  {
    context.findcircle_best_fit_edge = 0;
    edited = true;
  }

  edited |= DrawFindCircleEdgeRoleCombo(
      "recommended edge", context.findcircle_recommended_fit_edge,
      context.findcircle_scan_edge_count);
  ImGui::SameLine();
  if (ImGui::Button("Recommend = Best"))
  {
    context.findcircle_recommended_fit_edge = context.findcircle_best_fit_edge;
    edited = true;
  }

  edited |= DrawFindCircleEdgeRoleCombo(
      "relation edge", context.findcircle_relation_edge,
      context.findcircle_scan_edge_count);
  ImGui::SameLine();
  ImGui::TextDisabled("future: related/combined edge point-set");

  edited |= DrawFindCircleEdgeRoleCombo(
      "attach edge", context.findcircle_attach_edge,
      context.findcircle_scan_edge_count);
  ImGui::SameLine();
  ImGui::TextDisabled("future: shape attach/binding");
  ImGui::PopID();

  ImGui::Text("edges: selected=%d best=%d recommended=%d relation=%d attach=%d",
              context.findcircle_selected_scan_edge,
              context.findcircle_best_fit_edge,
              context.findcircle_recommended_fit_edge,
              context.findcircle_relation_edge,
              context.findcircle_attach_edge);

  if (edited)
  {
    SyncFindCircleEdgeSelectionToGlobals(
        context,
        "FindCircle edge role binding changed");
  }

  return edited;
}

static void DrawFindCircleEdgeEvaluationPanel(ManualTestContext& context)
{
  if (!(context.current_gauge.tool == "FindCircle" ||
        context.current_gauge.has_circle_gauge))
  {
    return;
  }

  const RuntimeObjectView* object = FindCurrentFindCircleObject(context);

  ImGui::Separator();
  ImGui::TextUnformatted("Edge Result Evaluation");
  ImGui::TextDisabled(
      "The current runtime reports one selected-edge run. Per-edge scores require explicit runtime capture and are not inferred here.");

  if (object == nullptr)
  {
    ImGui::TextDisabled("Run script to collect circle result evidence.");
    return;
  }

  const int accepted = object->valid_points_count > 0
      ? object->valid_points_count
      : object->measure_points_count;
  const double score = object->has_fit_result
      ? std::max(0.0, 100.0 - static_cast<double>(object->fit_avgdist) * 10.0)
      : 0.0;

  ImGui::Text("ui_selected_edge=%d runtime_selected_edge=%d edge_count=%d fit_circle=%s score=%.2f",
              context.findcircle_selected_scan_edge,
              object->circle_selected_edge_index,
              context.findcircle_scan_edge_count,
              object->has_fit_result ? "true" : "false",
              score);
  if (object->circle_selected_edge_index !=
      context.findcircle_selected_scan_edge)
  {
    ImGui::TextColored(
        ImVec4(1.0f, 0.75f, 0.20f, 1.0f),
        "Runtime selected edge mismatch. Click Apply To Globals / Run Script again; otherwise global injection is not synchronized.");
  }
  ImGui::Text(
      "candidate_runs_total=%d max_per_line=%d selected_hits=%d selected_misses=%d",
      object->circle_candidate_runs_total,
      object->circle_candidate_runs_max_per_line,
      object->circle_selected_edge_hits,
      object->circle_selected_edge_misses);
  ImGui::Text(
      "selected radius avg/min/max = %.3f / %.3f / %.3f",
      object->circle_selected_edge_radius_avg,
      object->circle_selected_edge_radius_min,
      object->circle_selected_edge_radius_max);
  ImGui::Text(
      "source boundary: clipped_lines=%d extended_samples=%d candidate_reject=%d",
      object->circle_scan_boundary_clipped_lines,
      object->circle_scan_boundary_extended_samples,
      object->circle_candidate_boundary_reject_count);

  const ImGuiTableFlags flags =
      ImGuiTableFlags_Borders |
      ImGuiTableFlags_RowBg |
      ImGuiTableFlags_Resizable |
      ImGuiTableFlags_SizingStretchProp;

  if (ImGui::BeginTable("findcircle_edge_evaluation_table", 12, flags))
  {
    ImGui::TableSetupColumn("Run");
    ImGui::TableSetupColumn("Sel");
    ImGui::TableSetupColumn("RtSel");
    ImGui::TableSetupColumn("Lines");
    ImGui::TableSetupColumn("Len");
    ImGui::TableSetupColumn("Points");
    ImGui::TableSetupColumn("MaxCand");
    ImGui::TableSetupColumn("Hits");
    ImGui::TableSetupColumn("Miss");
    ImGui::TableSetupColumn("Fit?");
    ImGui::TableSetupColumn("AvgDist");
    ImGui::TableSetupColumn("Stage");
    ImGui::TableHeadersRow();

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted(
        FindCircleEdgeLabel(context.findcircle_selected_scan_edge).c_str());
    ImGui::TableSetColumnIndex(1);
    ImGui::TextUnformatted("*");
    ImGui::TableSetColumnIndex(2);
    ImGui::Text("%d", object->circle_selected_edge_index);
    ImGui::TableSetColumnIndex(3);
    ImGui::Text("%d", object->circle_scan_line_count);
    ImGui::TableSetColumnIndex(4);
    ImGui::Text("%d", object->circle_scan_line_length);
    ImGui::TableSetColumnIndex(5);
    ImGui::Text("%d", accepted);
    ImGui::TableSetColumnIndex(6);
    ImGui::Text("%d", object->circle_candidate_runs_max_per_line);
    ImGui::TableSetColumnIndex(7);
    ImGui::Text("%d", object->circle_selected_edge_hits);
    ImGui::TableSetColumnIndex(8);
    ImGui::Text("%d", object->circle_selected_edge_misses);
    ImGui::TableSetColumnIndex(9);
    ImGui::TextUnformatted(object->has_fit_result ? "yes" : "no");
    ImGui::TableSetColumnIndex(10);
    ImGui::Text("%.3f", object->fit_avgdist);
    ImGui::TableSetColumnIndex(11);
    ImGui::TextUnformatted(object->circle_measure_failure_stage.empty()
        ? "-"
        : object->circle_measure_failure_stage.c_str());

    ImGui::EndTable();
  }
}

static void DrawFindCircleScanSemanticsPanel(ManualTestContext& context)
{
  if (!(context.current_gauge.tool == "FindCircle" ||
        context.current_gauge.has_circle_gauge))
  {
    return;
  }

  const RuntimeObjectView* object = FindCurrentFindCircleObject(context);
  const int linegap = std::max(1, context.current_gauge.linegap);
  const double circumference =
      std::max(0, context.current_gauge.radius) * 2.0 * 3.14159265358979323846;
  const int previewTicks = circumference > 1.0
      ? std::max(1, static_cast<int>(std::floor(circumference / linegap)))
      : 0;

  ImGui::Separator();
  ImGui::TextUnformatted("Gauge Scan Semantics");
  ImGui::TextColored(ImVec4(0.55f, 0.90f, 1.0f, 1.0f),
                     "cyan radial line = sampling opportunity");
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.18f, 1.0f),
                     "| yellow point = accepted algorithm result");
  ImGui::TextDisabled(
      "Selected Edge is a candidate rank on each radial line; A0/A1 is the independent angular scan sector.");

  if (object == nullptr)
  {
    ImGui::TextDisabled("Run script to show accepted/rejected counters.");
    ImGui::Text("preview_ticks=%d linegap=%d method=%d gap=%d",
                previewTicks,
                linegap,
                context.current_gauge.method,
                context.current_gauge.gap);
    return;
  }

  ImGui::Text("preview_ticks=%d scan_lines=%d scan_len=%d process_w=%d",
              previewTicks,
              object->circle_scan_line_count,
              object->circle_scan_line_length,
              object->circle_process_width);
  ImGui::Text("measure_points=%d valid_points=%d fit_circle=%s avgdist=%.3f",
              object->measure_points_count,
              object->valid_points_count,
              object->has_fit_result ? "true" : "false",
              object->fit_avgdist);
  ImGui::Text("image_ready=%s backimage_ready=%s findobject_ready=%s",
              object->circle_measure_image_ready ? "true" : "false",
              object->circle_measure_backimage_ready ? "true" : "false",
              object->circle_measure_findobject_ready ? "true" : "false");
  ImGui::Text("source=%s failure=%s",
              object->circle_measure_source.c_str(),
              object->circle_measure_failure_stage.empty()
                  ? "-"
                  : object->circle_measure_failure_stage.c_str());
  ImGui::Text("consistency=%s range=%.1f in/out/removed=%d/%d/%d",
              object->circle_point_consistency_enabled ? "on" : "off",
              object->circle_point_consistency_range,
              object->circle_point_consistency_input_points,
              object->circle_point_consistency_output_points,
              object->circle_point_consistency_removed_points);
  ImGui::Text("source boundary clipped=%d extended=%d candidate_reject=%d",
              object->circle_scan_boundary_clipped_lines,
              object->circle_scan_boundary_extended_samples,
              object->circle_candidate_boundary_reject_count);
}

static ManualFindCircleEdgeParamState MakeFindEllipseEdgeParamsFromGauge(
    const ManualGaugeState& gauge)
{
  ManualFindCircleEdgeParamState params;
  params.initialized = true;
  params.threshold = gauge.threshold;
  params.method = gauge.method;
  params.linegap = gauge.linegap;
  params.gap = gauge.gap;
  return params;
}

static void EnsureFindEllipseEdgeParamStorage(ManualTestContext& context)
{
  context.findellipse_scan_edge_count =
      std::max(1, std::min(32, context.findellipse_scan_edge_count));
  context.findellipse_selected_scan_edge =
      std::max(-1, std::min(context.findellipse_selected_scan_edge,
                           context.findellipse_scan_edge_count));
  context.findellipse_best_fit_edge =
      std::max(0, std::min(context.findellipse_best_fit_edge,
                           context.findellipse_scan_edge_count));
  context.findellipse_recommended_fit_edge =
      std::max(0, std::min(context.findellipse_recommended_fit_edge,
                           context.findellipse_scan_edge_count));
  context.findellipse_relation_edge =
      std::max(0, std::min(context.findellipse_relation_edge,
                           context.findellipse_scan_edge_count));
  context.findellipse_attach_edge =
      std::max(0, std::min(context.findellipse_attach_edge,
                           context.findellipse_scan_edge_count));

  const std::size_t required =
      static_cast<std::size_t>(context.findellipse_scan_edge_count + 1);
  if (context.findellipse_edge_params.size() < required)
    context.findellipse_edge_params.resize(required);

  for (int i = 1; i <= context.findellipse_scan_edge_count; ++i)
  {
    ManualFindCircleEdgeParamState& params =
        context.findellipse_edge_params[static_cast<std::size_t>(i)];
    params = MakeFindEllipseEdgeParamsFromGauge(context.current_gauge);
  }
}

static std::string FindEllipseEdgeLabel(int edge)
{
  if (edge == -1)
    return "Last edge";
  return edge == 0 ? "All edges" : ("Edge " + std::to_string(edge));
}

static void SyncFindEllipseEdgeSelectionToGlobals(
    ManualTestContext& context,
    const char* reason)
{
  EnsureFindEllipseEdgeParamStorage(context);
  InjectManualGaugeInt(
      context,
      "global_findellipse_edge_count",
      context.findellipse_scan_edge_count);
  InjectManualGaugeInt(
      context,
      "global_findellipse_selected_edge",
      context.findellipse_selected_scan_edge);
  InjectManualGaugeInt(
      context,
      "global_findellipse_best_edge",
      context.findellipse_best_fit_edge);
  InjectManualGaugeInt(
      context,
      "global_findellipse_recommended_edge",
      context.findellipse_recommended_fit_edge);
  InjectManualGaugeInt(
      context,
      "global_findellipse_relation_edge",
      context.findellipse_relation_edge);
  InjectManualGaugeInt(
      context,
      "global_findellipse_attach_edge",
      context.findellipse_attach_edge);

  context.current_gauge.dirty = true;
  context.current_gauge.review_status = "editing";
  context.debug_status = "FINDELLIPSE_EDGE_SELECTION_CHANGED";
  context.debug_reason =
      std::string(reason == nullptr ? "FindEllipse edge selection changed" : reason) +
      "; globals updated, run script to refresh result points";
}

static bool DrawFindEllipseEdgeRoleCombo(
    const char* label,
    int& edge,
    int edgeCount)
{
  bool edited = false;
  edge = std::max(0, std::min(edge, edgeCount));
  const std::string currentLabel = FindEllipseEdgeLabel(edge);
  ImGui::SetNextItemWidth(150.0f);
  if (ImGui::BeginCombo(label, currentLabel.c_str()))
  {
    if (ImGui::Selectable("All edges", edge == 0))
    {
      edge = 0;
      edited = true;
    }
    for (int i = 1; i <= edgeCount; ++i)
    {
      const std::string item = FindEllipseEdgeLabel(i);
      if (ImGui::Selectable(item.c_str(), edge == i))
      {
        edge = i;
        edited = true;
      }
    }
    ImGui::EndCombo();
  }
  return edited;
}

static bool DrawFindEllipseEdgeSelectorPanel(ManualTestContext& context)
{
  if (!(context.current_gauge.tool == "FindEllipse" ||
        context.current_gauge.has_ellipse_gauge))
  {
    return false;
  }

  bool edited = false;
  EnsureFindEllipseEdgeParamStorage(context);

  ImGui::Separator();
  ImGui::TextUnformatted("Algorithm scan sector: full ellipse gauge");
  ImGui::TextDisabled("FindEllipse currently scans the full ellipse. Edge selection below is candidate ordinal, not an angular sector.");
  ImGui::TextUnformatted("Detection Edge / Point Column");
  ImGui::TextDisabled(
      "Edge N is the Nth eligible boundary crossing on every ellipse Gauge line. It mirrors FindCircle selected-edge semantics.");

  ImGui::SetNextItemWidth(100.0f);
  int edgeCount = context.findellipse_scan_edge_count;
  if (ImGui::InputInt("edge count", &edgeCount))
  {
    context.findellipse_scan_edge_count = std::max(1, std::min(32, edgeCount));
    if (context.findellipse_selected_scan_edge >
        context.findellipse_scan_edge_count)
    {
      context.findellipse_selected_scan_edge =
          context.findellipse_scan_edge_count;
    }
    EnsureFindEllipseEdgeParamStorage(context);
    SyncFindEllipseEdgeSelectionToGlobals(
        context,
        "FindEllipse edge count changed");
    edited = true;
  }

  ImGui::SameLine();
  ImGui::SetNextItemWidth(190.0f);
  const std::string currentLabel =
      FindEllipseEdgeLabel(context.findellipse_selected_scan_edge);
  if (ImGui::BeginCombo("selected edge", currentLabel.c_str()))
  {
    if (ImGui::Selectable("All edges", context.findellipse_selected_scan_edge == 0))
    {
      context.findellipse_selected_scan_edge = 0;
      SyncFindEllipseEdgeSelectionToGlobals(
          context,
          "FindEllipse selected All edges");
      edited = true;
    }
    for (int i = 1; i <= context.findellipse_scan_edge_count; ++i)
    {
      const std::string label = "Edge " + std::to_string(i);
      if (ImGui::Selectable(label.c_str(),
                            context.findellipse_selected_scan_edge == i))
      {
        context.findellipse_selected_scan_edge = i;
        const std::string reason = "FindEllipse selected " + label;
        SyncFindEllipseEdgeSelectionToGlobals(context, reason.c_str());
        edited = true;
      }
    }
    ImGui::Separator();
    if (ImGui::Selectable("Last edge",
                          context.findellipse_selected_scan_edge == -1))
    {
      context.findellipse_selected_scan_edge = -1;
      SyncFindEllipseEdgeSelectionToGlobals(
          context,
          "FindEllipse selected Last edge");
      edited = true;
    }
    ImGui::EndCombo();
  }

  if (context.findellipse_selected_scan_edge == -1)
    ImGui::Text("Current: Last edge on every ellipse Gauge line");
  else if (context.findellipse_selected_scan_edge > 0)
    ImGui::Text("Current: Edge %d / %d",
                context.findellipse_selected_scan_edge,
                context.findellipse_scan_edge_count);
  else
    ImGui::TextDisabled("All edges selected: shared Geometry/Edge Params remain unchanged.");

  return edited;
}

static bool DrawFindEllipseEdgeRolePanel(ManualTestContext& context)
{
  if (!(context.current_gauge.tool == "FindEllipse" ||
        context.current_gauge.has_ellipse_gauge))
  {
    return false;
  }

  bool edited = false;
  EnsureFindEllipseEdgeParamStorage(context);

  ImGui::Separator();
  ImGui::TextUnformatted("Edge Role Binding / 接入点");
  ImGui::TextDisabled(
      "Selection is an edge ordinal. Best/recommended/relation/attach are evidence metadata and future attach points.");

  ImGui::PushID("findellipse_edge_roles");
  edited |= DrawFindEllipseEdgeRoleCombo(
      "best edge", context.findellipse_best_fit_edge,
      context.findellipse_scan_edge_count);
  ImGui::SameLine();
  if (ImGui::Button("Use All Edges"))
  {
    context.findellipse_best_fit_edge = 0;
    edited = true;
  }

  edited |= DrawFindEllipseEdgeRoleCombo(
      "recommended edge", context.findellipse_recommended_fit_edge,
      context.findellipse_scan_edge_count);
  ImGui::SameLine();
  if (ImGui::Button("Recommend = Best"))
  {
    context.findellipse_recommended_fit_edge = context.findellipse_best_fit_edge;
    edited = true;
  }

  edited |= DrawFindEllipseEdgeRoleCombo(
      "relation edge", context.findellipse_relation_edge,
      context.findellipse_scan_edge_count);
  ImGui::SameLine();
  ImGui::TextDisabled("future: related/combined edge point-set");

  edited |= DrawFindEllipseEdgeRoleCombo(
      "attach edge", context.findellipse_attach_edge,
      context.findellipse_scan_edge_count);
  ImGui::SameLine();
  ImGui::TextDisabled("future: shape attach/binding");
  ImGui::PopID();

  ImGui::Text("edges: selected=%d best=%d recommended=%d relation=%d attach=%d",
              context.findellipse_selected_scan_edge,
              context.findellipse_best_fit_edge,
              context.findellipse_recommended_fit_edge,
              context.findellipse_relation_edge,
              context.findellipse_attach_edge);

  if (edited)
  {
    SyncFindEllipseEdgeSelectionToGlobals(
        context,
        "FindEllipse edge role binding changed");
  }

  return edited;
}

static void DrawFindEllipseEdgeEvaluationPanel(ManualTestContext& context)
{
  if (!(context.current_gauge.tool == "FindEllipse" ||
        context.current_gauge.has_ellipse_gauge))
  {
    return;
  }

  const RuntimeObjectView* object = FindCurrentFindEllipseObject(context);

  ImGui::Separator();
  ImGui::TextUnformatted("Edge Result Evaluation");
  ImGui::TextDisabled(
      "Runtime reports the current selected-edge run. Per-edge ranking can be added later from captured evidence.");

  if (object == nullptr)
  {
    ImGui::TextDisabled("Run script to collect ellipse result evidence.");
    return;
  }

  const int accepted = object->valid_points_count > 0
      ? object->valid_points_count
      : object->measure_points_count;
  const double score = object->has_fit_ellipse
      ? std::max(0.0, 100.0 - static_cast<double>(object->fit_ellipse_avgdist) * 10.0)
      : 0.0;

  ImGui::Text("ui_selected_edge=%d runtime_selected_edge=%d edge_count=%d fit_ellipse=%s score=%.2f",
              context.findellipse_selected_scan_edge,
              object->ellipse_selected_edge_index,
              context.findellipse_scan_edge_count,
              object->has_fit_ellipse ? "true" : "false",
              score);
  if (object->ellipse_selected_edge_index !=
      context.findellipse_selected_scan_edge)
  {
    ImGui::TextColored(
        ImVec4(1.0f, 0.75f, 0.20f, 1.0f),
        "Runtime selected edge mismatch. Click Apply To Globals / Run Script again.");
  }

  const ImGuiTableFlags flags =
      ImGuiTableFlags_Borders |
      ImGuiTableFlags_RowBg |
      ImGuiTableFlags_Resizable |
      ImGuiTableFlags_SizingStretchProp;
  if (ImGui::BeginTable("findellipse_edge_evaluation_table", 8, flags))
  {
    ImGui::TableSetupColumn("Run");
    ImGui::TableSetupColumn("Sel");
    ImGui::TableSetupColumn("Lines");
    ImGui::TableSetupColumn("Len");
    ImGui::TableSetupColumn("Points");
    ImGui::TableSetupColumn("Fit?");
    ImGui::TableSetupColumn("AvgDist");
    ImGui::TableSetupColumn("Stage");
    ImGui::TableHeadersRow();

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted(
        FindEllipseEdgeLabel(context.findellipse_selected_scan_edge).c_str());
    ImGui::TableSetColumnIndex(1);
    ImGui::TextUnformatted("*");
    ImGui::TableSetColumnIndex(2);
    ImGui::Text("%d", object->ellipse_scan_line_count);
    ImGui::TableSetColumnIndex(3);
    ImGui::Text("%d", object->ellipse_scan_line_length);
    ImGui::TableSetColumnIndex(4);
    ImGui::Text("%d", accepted);
    ImGui::TableSetColumnIndex(5);
    ImGui::TextUnformatted(object->has_fit_ellipse ? "yes" : "no");
    ImGui::TableSetColumnIndex(6);
    ImGui::Text("%.3f", object->fit_ellipse_avgdist);
    ImGui::TableSetColumnIndex(7);
    ImGui::TextUnformatted(object->ellipse_result_status.empty()
        ? "-"
        : object->ellipse_result_status.c_str());
    ImGui::EndTable();
  }
}

static void DrawFindEllipseScanSemanticsPanel(ManualTestContext& context)
{
  if (!(context.current_gauge.tool == "FindEllipse" ||
        context.current_gauge.has_ellipse_gauge))
  {
    return;
  }

  const RuntimeObjectView* object = FindCurrentFindEllipseObject(context);
  const int linegap = std::max(1, context.current_gauge.linegap);
  const double rx = std::abs(static_cast<double>(
      context.current_gauge.ellipse_x1 - context.current_gauge.ellipse_x0)) * 0.5;
  const double ry = std::abs(static_cast<double>(
      context.current_gauge.ellipse_y1 - context.current_gauge.ellipse_y0)) * 0.5;
  double circumference = 0.0;
  if (rx > 1.0 && ry > 1.0)
  {
    const double h = ((rx - ry) * (rx - ry)) / ((rx + ry) * (rx + ry));
    circumference =
        3.14159265358979323846 * (rx + ry) *
        (1.0 + (3.0 * h) / (10.0 + std::sqrt(std::max(0.0, 4.0 - 3.0 * h))));
  }
  const int previewTicks = circumference > 1.0
      ? std::max(1, static_cast<int>(std::floor(circumference / linegap)))
      : 0;

  ImGui::Separator();
  ImGui::TextUnformatted("Gauge Scan Semantics");
  ImGui::TextColored(ImVec4(0.55f, 0.90f, 1.0f, 1.0f),
                     "cyan ellipse line = sampling opportunity");
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.18f, 1.0f),
                     "| yellow point = accepted algorithm result");

  if (object == nullptr)
  {
    ImGui::TextDisabled("Run script to show accepted/rejected counters.");
    ImGui::Text("preview_ticks=%d linegap=%d method=%d gap=%d",
                previewTicks,
                linegap,
                context.current_gauge.method,
                context.current_gauge.gap);
    return;
  }

  ImGui::Text("preview_ticks=%d scan_lines=%d scan_len=%d selected_edge=%d",
              previewTicks,
              object->ellipse_scan_line_count,
              object->ellipse_scan_line_length,
              object->ellipse_selected_edge_index);
  ImGui::Text("measure_points=%d valid_points=%d fit_ellipse=%s avgdist=%.3f",
              object->measure_points_count,
              object->valid_points_count,
              object->has_fit_ellipse ? "true" : "false",
              object->fit_ellipse_avgdist);
  ImGui::Text("accepted_outside=%d accepted_norm=%.3f/%.3f/%.3f",
              object->ellipse_accepted_points_outside_ellipse_count,
              object->ellipse_accepted_point_norm_min,
              object->ellipse_accepted_point_norm_avg,
              object->ellipse_accepted_point_norm_max);
  ImGui::Text("rejected_boundary_band=%d rejected_norm=%.3f/%.3f/%.3f",
              object->ellipse_rejected_boundary_band_candidate_count,
              object->ellipse_rejected_boundary_band_norm_min,
              object->ellipse_rejected_boundary_band_norm_avg,
              object->ellipse_rejected_boundary_band_norm_max);
  ImGui::Text("consistency=%s range=%.1f in/out/removed=%d/%d/%d",
              object->ellipse_point_consistency_enabled ? "on" : "off",
              object->ellipse_point_consistency_range,
              object->ellipse_point_consistency_input_points,
              object->ellipse_point_consistency_output_points,
              object->ellipse_point_consistency_removed_points);
  ImGui::Text("scan_policy=%s candidate_policy=%s",
              object->ellipse_scan_geometry_policy.empty()
                  ? "-"
                  : object->ellipse_scan_geometry_policy.c_str(),
              object->ellipse_candidate_policy.empty()
                  ? "-"
                  : object->ellipse_candidate_policy.c_str());
}

static bool DrawFindLineEdgeRolePanel(
    ManualTestContext& context,
    const RuntimeObjectView* object)
{
  bool edited = false;
  EnsureFindLineEdgeParamStorage(context);

  ImGui::Separator();
  ImGui::TextUnformatted("Edge Role Binding / 接入点");
  ImGui::TextDisabled(
      "Placeholders for annotation/evidence relation. selected=edge used now; best=runtime/manual best; recommended=future advisor; relation=combined point-set; attach=shape binding.");

  if (object != nullptr &&
      object->line_best_edge_index > 0 &&
      object->line_best_edge_index <= context.findline_scan_edge_count &&
      context.findline_best_fit_edge == 0)
  {
    context.findline_best_fit_edge = object->line_best_edge_index;
  }

  ImGui::PushID("findline_edge_roles");
  edited |= DrawFindLineEdgeRoleCombo(
      "best edge", context.findline_best_fit_edge,
      context.findline_scan_edge_count);
  ImGui::SameLine();
  if (ImGui::Button("Use Runtime Best"))
  {
    if (object != nullptr &&
        object->line_best_edge_index > 0 &&
        object->line_best_edge_index <= context.findline_scan_edge_count)
    {
      context.findline_best_fit_edge = object->line_best_edge_index;
      edited = true;
    }
  }

  edited |= DrawFindLineEdgeRoleCombo(
      "recommended edge", context.findline_recommended_fit_edge,
      context.findline_scan_edge_count);
  ImGui::SameLine();
  if (ImGui::Button("Recommend = Best"))
  {
    context.findline_recommended_fit_edge = context.findline_best_fit_edge;
    edited = true;
  }

  edited |= DrawFindLineEdgeRoleCombo(
      "relation edge", context.findline_relation_edge,
      context.findline_scan_edge_count);
  ImGui::SameLine();
  ImGui::TextDisabled("future: related/combined point-set");

  edited |= DrawFindLineEdgeRoleCombo(
      "attach edge", context.findline_attach_edge,
      context.findline_scan_edge_count);
  ImGui::SameLine();
  ImGui::TextDisabled("future: shape attach/binding");
  ImGui::PopID();

  ImGui::Text("globals: selected=%d best=%d recommended=%d relation=%d attach=%d",
              context.findline_selected_scan_edge,
              context.findline_best_fit_edge,
              context.findline_recommended_fit_edge,
              context.findline_relation_edge,
              context.findline_attach_edge);

  return edited;
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
  std::string currentLabel = FindLineSelectedEdgeLabel(
      context.findline_selected_scan_edge,
      context.findline_scan_edge_count);
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
    ImGui::Separator();
    if (ImGui::Selectable(
            "Last edge",
            context.findline_selected_scan_edge == -1))
    {
      context.findline_selected_scan_edge = -1;
      edited = true;
    }
    ImGui::EndCombo();
  }

  if (context.findline_selected_scan_edge == -1)
  {
    ImGui::Text("Current: Last edge on every Gauge search line");
    ImGui::TextDisabled(
        "Last edge is resolved after the complete line profile is scanned; shared Edge Params remain active.");
  }
  else if (context.findline_selected_scan_edge > 0)
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

    context.current_gauge.scan_direction =
        context.current_gauge.scan_direction == 1 ? 1 : 2;
    if (ImGui::RadioButton(
            "W only##edge_scan_w",
            context.current_gauge.scan_direction == 1))
    {
      context.current_gauge.scan_direction = 1;
      edited = true;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton(
            "H only##edge_scan_h",
            context.current_gauge.scan_direction == 2))
    {
      context.current_gauge.scan_direction = 2;
      edited = true;
    }

    ImGui::BeginDisabled(context.current_gauge.scan_direction != 1);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.0f);
    edited |= ImGui::InputInt("edge wgap", &params.wgap);
    params.wgap = std::max(0, std::min(50, params.wgap));
    ImGui::EndDisabled();

    ImGui::BeginDisabled(context.current_gauge.scan_direction != 2);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.0f);
    edited |= ImGui::InputInt("edge hgap", &params.hgap);
    params.hgap = std::max(0, std::min(50, params.hgap));
    ImGui::EndDisabled();

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
  ImGui::Text("consistency=%s range=%.1f in/out/removed=%d/%d/%d",
              object->line_point_consistency_enabled ? "on" : "off",
              object->line_point_consistency_range,
              object->line_point_consistency_input_points,
              object->line_point_consistency_output_points,
              object->line_point_consistency_removed_points);
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
  const bool isFastMatch =
      gauge.tool == "FastMatch" ||
      NormalizeKeyParamToolTypeLocal(gauge.primary_object_type) == "FastMatch";
  const bool isFindEllipse =
      gauge.tool == "FindEllipse" ||
      NormalizeKeyParamToolTypeLocal(gauge.primary_object_type) == "FindEllipse" ||
      gauge.has_ellipse_gauge;
  const bool isFindRect =
      gauge.tool == "FindRect" ||
      NormalizeKeyParamToolTypeLocal(gauge.primary_object_type) == "FindRect";
  const bool isFindCircle =
      gauge.tool == "FindCircle" || gauge.has_circle_gauge ||
      NormalizeKeyParamToolTypeLocal(gauge.primary_object_type) == "FindCircle";
  const bool isGridPattern =
      gauge.tool == "GridPatternClassTool" ||
      NormalizeKeyParamToolTypeLocal(gauge.primary_object_type) == "GridPatternClassTool";
  const bool isRegionPattern =
      gauge.tool == "RegionPatternTool" ||
      NormalizeKeyParamToolTypeLocal(gauge.primary_object_type) == "RegionPatternTool";
  const bool isFindSegmentation =
      gauge.tool == "FindSegmentation" ||
      NormalizeKeyParamToolTypeLocal(gauge.primary_object_type) == "FindSegmentation";
  if (gauge.tool == "FindLine" || gauge.has_line_gauge)
  {
      ImGui::Checkbox("Show single-line gauge scan ticks",
                      &context.show_line_gauge_scan_lines);
      gaugeEdited |= DrawFindLineEdgeSelectorPanel(context);
      gaugeEdited |= DrawFindLineEdgeRolePanel(
          context,
          FindCurrentFindLineObject(context));
      DrawFindLineEdgeEvaluationPanel(context);
      DrawFindLineScanSemanticsPanel(context);
  }
  if (gauge.tool == "FindCircle" || gauge.has_circle_gauge)
  {
      ImGui::Checkbox("Show circle gauge scan ticks",
                      &context.show_circle_gauge_scan_lines);
      gaugeEdited |= DrawFindCircleEdgeSelectorPanel(context);
      gaugeEdited |= DrawFindCircleEdgeRolePanel(context);
      DrawFindCircleEdgeEvaluationPanel(context);
      DrawFindCircleScanSemanticsPanel(context);
  }
  if (isFindEllipse)
  {
      ImGui::Checkbox("Show ellipse gauge scan ticks",
                      &context.show_ellipse_gauge_scan_lines);
      gaugeEdited |= DrawFindEllipseEdgeSelectorPanel(context);
      gaugeEdited |= DrawFindEllipseEdgeRolePanel(context);
      DrawFindEllipseEdgeEvaluationPanel(context);
      DrawFindEllipseScanSemanticsPanel(context);
  }
  if (isFastMatch)
  {
      ImGui::TextColored(
          ImVec4(1.0f, 0.86f, 0.35f, 1.0f),
          "FastMatch: learn ROI + search ROI + matching params");
      DrawFastMatchTemplateStatusPanel(context);
  }
  if (isFindSegmentation)
  {
      ImGui::TextColored(
          ImVec4(0.35f, 0.88f, 0.62f, 1.0f),
          "FindSegmentation: prompt ROI -> segment -> boundary/overlay");
  }
  if (isGridPattern)
  {
      ImGui::TextColored(
          ImVec4(0.35f, 0.88f, 0.62f, 1.0f),
          "GridPattern: ROI -> grid cells -> pooled hierarchy -> evidence overlay");
  }
  if (isRegionPattern)
  {
      ImGui::TextColored(
          ImVec4(0.9f, 0.72f, 0.32f, 1.0f),
          "RegionPattern: ROI -> gray/binary pooled descriptor -> texture review signal");
  }
  ImGui::Separator();

  ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
  if (ImGui::CollapsingHeader("Geometry"))
  {
      ImGui::PushID("geometry");
      if (gauge.tool == "FindCircle" || gauge.has_circle_gauge)
      {
          bool circleGeometryEdited = false;
          ImGui::SetNextItemWidth(120.0f); circleGeometryEdited |= ImGui::InputInt("cx", &gauge.circle_cx);
          ImGui::SameLine(); ImGui::SetNextItemWidth(120.0f); circleGeometryEdited |= ImGui::InputInt("cy", &gauge.circle_cy);
          ImGui::SameLine(); ImGui::TextDisabled("annulus gauge");

          ImGui::SetNextItemWidth(120.0f); circleGeometryEdited |= ImGui::InputInt("inner_radius (Rin)", &gauge.inner_radius);
          ImGui::SameLine(); ImGui::SetNextItemWidth(120.0f); circleGeometryEdited |= ImGui::InputInt("outer_radius (Rout)", &gauge.outer_radius);
          ImGui::SameLine(); ImGui::Text("band_width: %d", std::max(0, gauge.outer_radius - gauge.inner_radius));

          const bool scanSectorToggled =
              ImGui::Checkbox("use scan sector", &gauge.circle_arc_enabled);
          circleGeometryEdited |= scanSectorToggled;
          // A0=0/A1=360 is the canonical full circle.  When the user enables
          // a sector from that state, create a real editable default rather
          // than immediately feeding the full-turn values back through Apply
          // and making the checkbox appear to bounce off.
          if (scanSectorToggled && gauge.circle_arc_enabled &&
              std::abs(gauge.circle_arc_end_deg - gauge.circle_arc_start_deg) >= 360)
          {
              gauge.circle_arc_end_deg = gauge.circle_arc_start_deg + 45;
          }
          if (gauge.circle_arc_enabled)
          {
              ImGui::SetNextItemWidth(120.0f); circleGeometryEdited |= ImGui::InputInt("arc_start_deg (A0)", &gauge.circle_arc_start_deg);
              ImGui::SameLine(); ImGui::SetNextItemWidth(120.0f); circleGeometryEdited |= ImGui::InputInt("arc_end_deg (A1)", &gauge.circle_arc_end_deg);
              ImGui::SameLine(); ImGui::TextDisabled("signed degrees allowed");
          }

          // radius is a legacy alias for outer_radius.  Keep it synchronized
          // here so the UI never presents two independently editable radii.
          gauge.outer_radius = std::max(1, gauge.outer_radius);
          gauge.inner_radius = std::max(0, std::min(gauge.inner_radius, gauge.outer_radius - 1));
          gauge.circle_arc_start_deg = std::max(-359, std::min(360, gauge.circle_arc_start_deg));
          gauge.circle_arc_end_deg = std::max(-359, std::min(360, gauge.circle_arc_end_deg));
          gauge.radius = gauge.outer_radius;
          gauge.circle_px = gauge.circle_cx + gauge.outer_radius;
          gauge.circle_py = gauge.circle_cy;
          gaugeEdited |= circleGeometryEdited;
          if (circleGeometryEdited)
          {
              // Keep the editable repository CircleShape in lockstep with the
              // controls.  The explicit Apply To Gauge button remains a
              // visible confirmation, but changing Rin/Rout must not leave a
              // stale single-circle drawing on the canvas.
              context.apply_gauge_to_shape_requested = true;
          }
      }
      else if (isFindEllipse)
      {
          bool ellipseGeometryEdited = false;
          ImGui::TextDisabled(
              "FindEllipse uses the same direct gauge-editing model as FindCircle: geometry first, then edge params.");
          ImGui::SetNextItemWidth(120.0f); ellipseGeometryEdited |= ImGui::InputInt("x0", &gauge.ellipse_x0);
          ImGui::SameLine(); ImGui::SetNextItemWidth(120.0f); ellipseGeometryEdited |= ImGui::InputInt("y0", &gauge.ellipse_y0);
          ImGui::SameLine(); ImGui::TextDisabled("ellipse center / first radius point");

          ImGui::SetNextItemWidth(120.0f); ellipseGeometryEdited |= ImGui::InputInt("x1", &gauge.ellipse_x1);
          ImGui::SameLine(); ImGui::SetNextItemWidth(120.0f); ellipseGeometryEdited |= ImGui::InputInt("y1", &gauge.ellipse_y1);
          ImGui::SameLine(); ImGui::TextDisabled("ellipse second radius point");

          gaugeEdited |= ellipseGeometryEdited;
          if (ellipseGeometryEdited)
          {
              context.apply_gauge_to_shape_requested = true;
          }
      }
      else if (isFindRect)
      {
          bool rectGeometryEdited = false;
          ImGui::TextDisabled(
              "FindRect extends FindLine controls: editable seed line/box with line-like scan params.");
          ImGui::SetNextItemWidth(120.0f); rectGeometryEdited |= ImGui::InputInt("x0", &gauge.line_x0);
          ImGui::SameLine(); ImGui::SetNextItemWidth(120.0f); rectGeometryEdited |= ImGui::InputInt("y0", &gauge.line_y0);
          ImGui::SameLine(); ImGui::SetNextItemWidth(120.0f); rectGeometryEdited |= ImGui::InputInt("half_width", &gauge.tool_half_width);

          ImGui::SetNextItemWidth(120.0f); rectGeometryEdited |= ImGui::InputInt("x1", &gauge.line_x1);
          ImGui::SameLine(); ImGui::SetNextItemWidth(120.0f); rectGeometryEdited |= ImGui::InputInt("y1", &gauge.line_y1);
          gauge.tool_half_width = std::max(1, gauge.tool_half_width);

          gaugeEdited |= rectGeometryEdited;
          if (rectGeometryEdited)
          {
              context.apply_gauge_to_shape_requested = true;
          }
      }
      else if (isGridPattern)
      {
          gaugeEdited |= DrawGridPatternRoiControls(context);
      }
      else if (isRegionPattern)
      {
          gaugeEdited |= DrawRegionPatternRoiControls(context);
      }
      else if (isFastMatch)
      {
          const bool fastMatchRoiEdited = DrawFastMatchRoiControls(context);
          gaugeEdited |= fastMatchRoiEdited;
          if (fastMatchRoiEdited)
          {
              context.apply_gauge_to_shape_requested = true;
          }
      }
      else if (isFindSegmentation)
      {
          const bool segPromptEdited = DrawFindSegmentationPromptControls(context);
          gaugeEdited |= segPromptEdited;
          if (segPromptEdited)
          {
              context.apply_gauge_to_shape_requested = true;
          }
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

      if (isGridPattern)
      {
          gaugeEdited |= DrawGridPatternParameterControls(context);
      }
      else if (isRegionPattern)
      {
          gaugeEdited |= DrawRegionPatternParameterControls(context);
      }
      else if (isFastMatch)
      {
          gaugeEdited |= DrawFastMatchLearnParameterControls(context);
          ImGui::Separator();
          gaugeEdited |= DrawFastMatchMatchParameterControls(context);
      }
      else
      {
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

      const int findsettingDefault =
          (gauge.tool == "FindLine" || gauge.has_line_gauge || isFindEllipse)
              ? 1
              : 0;
      if (gauge.findsetting < 0)
      {
          gauge.findsetting = findsettingDefault;
      }
      gauge.findsetting = std::max(0, std::min(255, gauge.findsetting));
      bool objectPrefilter = (gauge.findsetting & 0x01) != 0;
      const char* findsettingMethodName =
          (gauge.tool == "FindLine" || gauge.has_line_gauge)
              ? "setobjfilter"
              : "setfindsetting";
      ImGui::TextUnformatted("findsetting");
      ImGui::SameLine(110.0f);
      if (ImGui::Checkbox("object prefilter##findsetting_prefilter",
                          &objectPrefilter))
      {
          if (objectPrefilter)
              gauge.findsetting |= 0x01;
          else
              gauge.findsetting &= ~0x01;
          gaugeEdited = true;
      }
      ImGui::SameLine();
      ImGui::SetNextItemWidth(70.0f);
      gaugeEdited |= ImGui::InputInt("##findsetting_value", &gauge.findsetting);
      gauge.findsetting = std::max(0, std::min(255, gauge.findsetting));
      ImGui::SameLine();
      ImGui::TextDisabled("%s bit0, default=%d",
                          findsettingMethodName,
                          findsettingDefault);

      if (isFindCircle || isFindEllipse)
      {
          ImGui::TextUnformatted("gap");
          ImGui::SameLine(80.0f);
          ImGui::SetNextItemWidth(180.0f); gaugeEdited |= ImGui::SliderInt("##gap", &gauge.gap, 0, 200);
          ImGui::SameLine(); ImGui::SetNextItemWidth(70.0f); gaugeEdited |= ImGui::InputInt("##gap_val", &gauge.gap);
          gauge.gap = std::max(0, std::min(200, gauge.gap));

          if (gauge.tool == "FindCircle" || gauge.has_circle_gauge)
          {
              const int annulusWidth =
                  std::max(1, gauge.outer_radius - gauge.inner_radius);
              const int circleConsistencyDefaultRange =
                  std::max(1, annulusWidth / 2);
              if (context.findcircle_point_consistency_range <= 0)
              {
                  context.findcircle_point_consistency_range =
                      circleConsistencyDefaultRange;
              }

              bool circleConsistencyEnabled =
                  context.findcircle_point_consistency_enabled;
              if (ImGui::Checkbox(
                      "enable point consistency##findcircle_consistency",
                      &circleConsistencyEnabled))
              {
                  context.findcircle_point_consistency_enabled =
                      circleConsistencyEnabled;
                  if (context.findcircle_point_consistency_range <= 0)
                  {
                      context.findcircle_point_consistency_range =
                          circleConsistencyDefaultRange;
                  }
                  gaugeEdited = true;
              }
              ImGui::TextUnformatted("consistency");
              ImGui::SameLine(110.0f);
              ImGui::SetNextItemWidth(180.0f);
              gaugeEdited |= ImGui::SliderInt(
                  "##findcircle_consistency_range",
                  &context.findcircle_point_consistency_range,
                  1,
                  std::max(1, annulusWidth));
              ImGui::SameLine();
              ImGui::SetNextItemWidth(70.0f);
              gaugeEdited |= ImGui::InputInt(
                  "##findcircle_consistency_range_val",
                  &context.findcircle_point_consistency_range);
              context.findcircle_point_consistency_range =
                  std::max(1, std::min(10000,
                                       context.findcircle_point_consistency_range));
              ImGui::SameLine();
              ImGui::TextDisabled("default=%d", circleConsistencyDefaultRange);
          }
          else if (isFindEllipse)
          {
              const int ellipseRx = std::abs(gauge.ellipse_x1 - gauge.ellipse_x0) / 2;
              const int ellipseRy = std::abs(gauge.ellipse_y1 - gauge.ellipse_y0) / 2;
              const int ellipseConsistencyMax =
                  std::max(1, std::max(ellipseRx, ellipseRy));
              const int ellipseConsistencyDefaultRange =
                  std::max(1, std::max(1, gauge.gap) / 2);
              if (context.findellipse_point_consistency_range <= 0)
              {
                  context.findellipse_point_consistency_range =
                      ellipseConsistencyDefaultRange;
              }

              bool ellipseConsistencyEnabled =
                  context.findellipse_point_consistency_enabled;
              if (ImGui::Checkbox(
                      "enable point consistency##findellipse_consistency",
                      &ellipseConsistencyEnabled))
              {
                  context.findellipse_point_consistency_enabled =
                      ellipseConsistencyEnabled;
                  if (context.findellipse_point_consistency_range <= 0)
                  {
                      context.findellipse_point_consistency_range =
                          ellipseConsistencyDefaultRange;
                  }
                  gaugeEdited = true;
              }
              ImGui::TextUnformatted("consistency");
              ImGui::SameLine(110.0f);
              ImGui::SetNextItemWidth(180.0f);
              gaugeEdited |= ImGui::SliderInt(
                  "##findellipse_consistency_range",
                  &context.findellipse_point_consistency_range,
                  1,
                  ellipseConsistencyMax);
              ImGui::SameLine();
              ImGui::SetNextItemWidth(70.0f);
              gaugeEdited |= ImGui::InputInt(
                  "##findellipse_consistency_range_val",
                  &context.findellipse_point_consistency_range);
              context.findellipse_point_consistency_range =
                  std::max(1, std::min(10000,
                                       context.findellipse_point_consistency_range));
              ImGui::SameLine();
              ImGui::TextDisabled("default=%d", ellipseConsistencyDefaultRange);
          }
      }
      else
      {
          gauge.scan_direction = gauge.scan_direction == 1 ? 1 : 2;
          ImGui::TextUnformatted("scan direction");
          ImGui::SameLine(110.0f);
          if (ImGui::RadioButton("W only##scan_w", gauge.scan_direction == 1))
          {
              gauge.scan_direction = 1;
              gaugeEdited = true;
          }
          ImGui::SameLine();
          if (ImGui::RadioButton("H only##scan_h", gauge.scan_direction == 2))
          {
              gauge.scan_direction = 2;
              gaugeEdited = true;
          }

          ImGui::BeginDisabled(gauge.scan_direction != 1);
          ImGui::TextUnformatted("wgap");
          ImGui::SameLine(80.0f);
          ImGui::SetNextItemWidth(180.0f); gaugeEdited |= ImGui::SliderInt("##wgap", &gauge.wgap, 0, 50);
          ImGui::SameLine(); ImGui::SetNextItemWidth(70.0f); gaugeEdited |= ImGui::InputInt("##wg_val", &gauge.wgap);
          gauge.wgap = std::max(0, std::min(50, gauge.wgap));
          ImGui::EndDisabled();

          ImGui::BeginDisabled(gauge.scan_direction != 2);
          ImGui::TextUnformatted("hgap");
          ImGui::SameLine(80.0f);
          ImGui::SetNextItemWidth(180.0f); gaugeEdited |= ImGui::SliderInt("##hgap", &gauge.hgap, 0, 50);
          ImGui::SameLine(); ImGui::SetNextItemWidth(70.0f); gaugeEdited |= ImGui::InputInt("##hg_val", &gauge.hgap);
          gauge.hgap = std::max(0, std::min(50, gauge.hgap));
          ImGui::EndDisabled();

          const int consistencyDefaultRange =
              std::max(1, gauge.tool_half_width / 2);
          if (context.findline_point_consistency_range <= 0)
          {
              context.findline_point_consistency_range =
                  consistencyDefaultRange;
          }

          bool consistencyEnabled =
              context.findline_point_consistency_enabled;
          if (ImGui::Checkbox("enable point consistency##findline_consistency",
                              &consistencyEnabled))
          {
              context.findline_point_consistency_enabled =
                  consistencyEnabled;
              if (context.findline_point_consistency_range <= 0)
              {
                  context.findline_point_consistency_range =
                      consistencyDefaultRange;
              }
              gaugeEdited = true;
          }
          ImGui::TextUnformatted("consistency");
          ImGui::SameLine(110.0f);
          ImGui::SetNextItemWidth(180.0f);
          gaugeEdited |= ImGui::SliderInt(
              "##findline_consistency_range",
              &context.findline_point_consistency_range,
              1,
              std::max(1, gauge.tool_half_width));
          ImGui::SameLine();
          ImGui::SetNextItemWidth(70.0f);
          gaugeEdited |= ImGui::InputInt(
              "##findline_consistency_range_val",
              &context.findline_point_consistency_range);
          context.findline_point_consistency_range =
              std::max(1, std::min(10000,
                                   context.findline_point_consistency_range));
          ImGui::SameLine();
          ImGui::TextDisabled("default=%d", consistencyDefaultRange);

          ImGui::TextUnformatted("filterprofile");
          ImGui::SameLine(80.0f);
          ImGui::SetNextItemWidth(180.0f); gaugeEdited |= ImGui::SliderInt("##fp", &gauge.filterprofile, 0, 10);
          ImGui::SameLine(); ImGui::SetNextItemWidth(70.0f); gaugeEdited |= ImGui::InputInt("##fp_val", &gauge.filterprofile);
          gauge.filterprofile = std::max(0, std::min(10, gauge.filterprofile));
      }
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
        " findsetting=" + std::to_string(gauge.findsetting);
    if (gauge.tool == "FindCircle" || gauge.has_circle_gauge)
    {
      context.last_key_parameter_edit_summary +=
          " gap=" + std::to_string(gauge.gap) +
          " consistency=" +
          std::to_string(context.findcircle_point_consistency_enabled ? 1 : 0) +
          "/" + std::to_string(context.findcircle_point_consistency_range) +
          " selected_edge=" +
          std::to_string(context.findcircle_selected_scan_edge) +
          "/" + std::to_string(context.findcircle_scan_edge_count) +
          " best_edge=" + std::to_string(context.findcircle_best_fit_edge) +
          " recommended_edge=" + std::to_string(context.findcircle_recommended_fit_edge) +
          " relation_edge=" + std::to_string(context.findcircle_relation_edge) +
          " attach_edge=" + std::to_string(context.findcircle_attach_edge) +
          " circle=(" + std::to_string(gauge.circle_cx) + "," +
          std::to_string(gauge.circle_cy) + "," +
          std::to_string(gauge.circle_px) + "," +
          std::to_string(gauge.circle_py) + ")";
    }
    else if (isFindEllipse)
    {
      context.last_key_parameter_edit_summary +=
          " gap=" + std::to_string(gauge.gap) +
          " consistency=" +
          std::to_string(context.findellipse_point_consistency_enabled ? 1 : 0) +
          "/" + std::to_string(context.findellipse_point_consistency_range) +
          " selected_edge=" +
          std::to_string(context.findellipse_selected_scan_edge) +
          "/" + std::to_string(context.findellipse_scan_edge_count) +
          " best_edge=" + std::to_string(context.findellipse_best_fit_edge) +
          " recommended_edge=" + std::to_string(context.findellipse_recommended_fit_edge) +
          " relation_edge=" + std::to_string(context.findellipse_relation_edge) +
          " attach_edge=" + std::to_string(context.findellipse_attach_edge) +
          " ellipse=(" + std::to_string(gauge.ellipse_x0) + "," +
          std::to_string(gauge.ellipse_y0) + "," +
          std::to_string(gauge.ellipse_x1) + "," +
          std::to_string(gauge.ellipse_y1) + ")";
    }
    else if (isFastMatch || isGridPattern || isRegionPattern)
    {
      if (isRegionPattern)
      {
        context.last_key_parameter_edit_summary +=
            " region_roi=(" +
            std::to_string(RuntimeIntOr(context, "global_region_roi_x", 120)) + "," +
            std::to_string(RuntimeIntOr(context, "global_region_roi_y", 120)) + "," +
            std::to_string(RuntimeIntOr(context, "global_region_roi_w", 120)) + "," +
            std::to_string(RuntimeIntOr(context, "global_region_roi_h", 90)) + ")" +
            " pooling=" + std::to_string(RuntimeIntOr(context, "global_region_pooling_rows", 4)) +
            "x" + std::to_string(RuntimeIntOr(context, "global_region_pooling_cols", 4)) +
            " binary=" + std::to_string(RuntimeIntOr(context, "global_region_use_binary", 0)) +
            " threshold=" + std::to_string(RuntimeIntOr(context, "global_region_threshold", 128));
      }
      else
      {
        context.last_key_parameter_edit_summary +=
            " learn_roi=(" +
            std::to_string(RuntimeIntOr(context, "global_learn_roi_x", 120)) + "," +
            std::to_string(RuntimeIntOr(context, "global_learn_roi_y", 120)) + "," +
            std::to_string(RuntimeIntOr(context, "global_learn_roi_w", 120)) + "," +
            std::to_string(RuntimeIntOr(context, "global_learn_roi_h", 90)) + ")" +
            " search_roi=(" +
            std::to_string(RuntimeIntOr(context, "global_search_roi_x", 0)) + "," +
            std::to_string(RuntimeIntOr(context, "global_search_roi_y", 0)) + "," +
            std::to_string(RuntimeIntOr(context, "global_search_roi_w", 640)) + "," +
            std::to_string(RuntimeIntOr(context, "global_search_roi_h", 480)) + ")" +
            " wgap=" + std::to_string(gauge.wgap) +
            " hgap=" + std::to_string(gauge.hgap) +
            " filterprofile=" + std::to_string(gauge.filterprofile) +
            " find_num=" +
            std::to_string(RuntimeIntOr(context, "global_find_num", 1));
      }
      if (isGridPattern)
      {
        context.last_key_parameter_edit_summary +=
            " grid=" + std::to_string(RuntimeIntOr(context, "global_grid_rows", 12)) +
            "x" + std::to_string(RuntimeIntOr(context, "global_grid_cols", 12)) +
            " levels=" + std::to_string(RuntimeIntOr(context, "global_grid_levels", 3)) +
            " orientation_bins=" +
            std::to_string(RuntimeIntOr(context, "global_grid_orientation_bins", 8)) +
            " fusion_mode=" +
            std::to_string(RuntimeIntOr(context, "global_grid_fusion_mode", 2));
      }
    }
    else
    {
      context.last_key_parameter_edit_summary +=
          " wgap=" + std::to_string(gauge.wgap) +
          " hgap=" + std::to_string(gauge.hgap) +
          " scan_direction=" + std::to_string(gauge.scan_direction) +
          " filterprofile=" + std::to_string(gauge.filterprofile) +
          " findsetting=" + std::to_string(gauge.findsetting) +
          " selected_edge=" +
          std::to_string(context.findline_selected_scan_edge) +
          "/" + std::to_string(context.findline_scan_edge_count) +
          " best_edge=" + std::to_string(context.findline_best_fit_edge) +
          " recommended_edge=" + std::to_string(context.findline_recommended_fit_edge) +
          " relation_edge=" + std::to_string(context.findline_relation_edge) +
          " attach_edge=" + std::to_string(context.findline_attach_edge) +
          " roi=(" + std::to_string(gauge.line_x0) + "," +
          std::to_string(gauge.line_y0) + "," +
          std::to_string(gauge.line_x1) + "," +
          std::to_string(gauge.line_y1) + ")";
    }
    CXLOG_INFO(
        "KeyParameterControls",
        "key_parameter_ui_edit",
        "edited",
        "revision=" + std::to_string(context.key_parameter_edit_revision) +
        " " + context.last_key_parameter_edit_summary);
  }

  ImGui::Separator();
  if (isFastMatch)
  {
    ImGui::TextUnformatted("FastMatch Actions");
    const float fmBtnWidth =
        (ImGui::GetContentRegionAvail().x - 20.0f) / 3.0f;
    ImGui::PushID("fastmatch_actions");
    if (ImGui::Button("Learn", ImVec2(fmBtnWidth, 0)))
      RequestFastMatchRunAction(context, 1, "FastMatch Learn");
    ImGui::SameLine();
    if (ImGui::Button("Match", ImVec2(fmBtnWidth, 0)))
      RequestFastMatchRunAction(context, 2, "FastMatch Match");
    ImGui::SameLine();
    if (ImGui::Button("Learn + Match", ImVec2(fmBtnWidth, 0)))
      RequestFastMatchRunAction(context, 3, "FastMatch Learn + Match");
    ImGui::PopID();
    ImGui::Separator();
  }
  ImGui::TextUnformatted("Actions");

  const float btnWidth = (ImGui::GetContentRegionAvail().x - 30.0f) / 3.0f;

  ImGui::PushID("actions");
  if (ImGui::Button("Apply To Gauge", ImVec2(btnWidth, 0)))
  {
    gauge.dirty = true;
    gauge.review_status = "editing";
    context.apply_gauge_to_shape_requested = true;
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
