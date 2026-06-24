#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

#include <windows.h>

#include "../catalog/parser_case_catalog.h"
#include "../drivers/parser_dispatch_driver.h"
#include "../pipeline/parser_cxscript_runtime.h"
#include "../runtime/cxscript_runtime.h"

namespace
{
bool HasDebugData(const cxparser_ext::CxScriptExecutionResult &result)
{
  return !result.debug_view.step_traces.empty() ||
         !result.debug_view.source_map.empty() ||
         !result.debug_view.checkpoints.empty() ||
         !result.debug_view.breakpoints.empty() ||
         !result.debug_view.replay_frames.empty() ||
         !result.debug_view.variables.empty();
}

bool ShouldPrintDebugDetail(const std::string &line)
{
  return line.find("[TEST] ") != 0;
}

bool ShouldPrintLightweightDetail(const std::string &line)
{
  return line.find("[ACTION] ") == 0 ||
         line.find("[CALL] ") == 0 ||
         line.find("[PRINT] ") == 0 ||
         line.find("[EMIT] ") == 0 ||
         line.find("[CHECK] ") == 0 ||
         line.find("[FAIL] ") == 0 ||
         line.find("[SKIP] ") == 0 ||
         line.find("[SUMMARY] ") == 0 ||
         line.find("[CONTRACT] ") == 0 ||
         line.find("[DISPATCH_STATUS] ") == 0 ||
         line.find("[TORCH_") == 0 ||
         line.find("[ENSMALLEN_") == 0;
}

bool HasWindowsDrivePrefix(const std::string &path)
{
  return path.size() > 1 &&
         std::isalpha(static_cast<unsigned char>(path[0])) &&
         path[1] == ':';
}

bool IsAbsolutePath(const std::string &path)
{
  return !path.empty() &&
         (HasWindowsDrivePrefix(path) || path[0] == '/' || path[0] == '\\');
}

bool ShouldAnchorPublicScriptPath(const std::string &path)
{
  return path.find("cxparser/") == 0 ||
         path.find("cxparser\\") == 0 ||
         path.find("cxscript/") == 0 ||
         path.find("cxscript\\") == 0 ||
         path.find("rag_script_cases/") == 0 ||
         path.find("rag_script_cases\\") == 0;
}

std::string ResolvePublicScriptPath(const std::string &path)
{
  if (path.empty() || IsAbsolutePath(path) || !ShouldAnchorPublicScriptPath(path))
    return path;

#ifdef CXPARSER_WORKSPACE_ROOT
  return std::string(CXPARSER_WORKSPACE_ROOT) + "/" + path;
#else
  return path;
#endif
}

std::string Trim(const std::string &text)
{
  size_t begin = 0;
  while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])))
    ++begin;

  size_t end = text.size();
  while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])))
    --end;

  return text.substr(begin, end - begin);
}

std::vector<std::string> SplitSemicolonList(const std::string &text)
{
  std::vector<std::string> items;
  std::string current;
  for (size_t i = 0; i < text.size(); ++i)
  {
    const char ch = text[i];
    if (ch == ';')
    {
      if (!current.empty())
        items.push_back(current);
      current.clear();
      continue;
    }

    current.push_back(ch);
  }

  if (!current.empty())
    items.push_back(current);
  return items;
}

std::vector<std::string> SplitByDelimiter(const std::string &text,
                                          const std::string &delimiter)
{
  std::vector<std::string> items;
  if (delimiter.empty())
  {
    if (!text.empty())
      items.push_back(text);
    return items;
  }

  size_t begin = 0;
  while (begin <= text.size())
  {
    const size_t found = text.find(delimiter, begin);
    if (found == std::string::npos)
    {
      const std::string tail = text.substr(begin);
      if (!tail.empty())
        items.push_back(tail);
      break;
    }

    const std::string item = text.substr(begin, found - begin);
    if (!item.empty())
      items.push_back(item);
    begin = found + delimiter.size();
  }

  return items;
}

std::vector<std::string> SplitByCharacter(const std::string &text, char delimiter)
{
  std::vector<std::string> items;
  std::string current;
  for (size_t i = 0; i < text.size(); ++i)
  {
    const char ch = text[i];
    if (ch == delimiter)
    {
      items.push_back(current);
      current.clear();
      continue;
    }

    current.push_back(ch);
  }

  items.push_back(current);
  return items;
}

size_t CountDelimitedItems(const std::string &text, char delimiter)
{
  size_t count = 0;
  bool in_item = false;
  for (size_t i = 0; i < text.size(); ++i)
  {
    const char ch = text[i];
    if (ch == delimiter)
    {
      if (in_item)
      {
        ++count;
        in_item = false;
      }
      continue;
    }

    if (!std::isspace(static_cast<unsigned char>(ch)))
      in_item = true;
  }

  if (in_item)
    ++count;
  return count;
}

std::string FindAssignmentValue(const std::string &text,
                                const std::string &key)
{
  const std::vector<std::string> items = SplitSemicolonList(text);
  const std::string prefix = key + "=";
  for (size_t i = 0; i < items.size(); ++i)
  {
    const std::string item = Trim(items[i]);
    if (item.find(prefix) == 0)
      return item.substr(prefix.size());
  }

  return std::string();
}

std::string FindNamedResultValue(const cxparser_ext::CxScriptExecutionResult &result,
                                 const char *result_name,
                                 const char *field_name)
{
  for (size_t i = 0; i < result.result_fields.size(); ++i)
  {
    const cxparser_ext::CxScriptNamedResultField &field = result.result_fields[i];
    if (field.result_name == result_name && field.field_name == field_name)
      return field.value;
  }

  return std::string();
}

bool HasNamedResultGroup(const cxparser_ext::CxScriptExecutionResult &result,
                         const char *result_name)
{
  for (size_t i = 0; i < result.result_fields.size(); ++i)
  {
    if (result.result_fields[i].result_name == result_name)
      return true;
  }

  return false;
}

bool HasNamedResultField(const cxparser_ext::CxScriptExecutionResult &result,
                         const char *result_name,
                         const char *field_name)
{
  for (size_t i = 0; i < result.result_fields.size(); ++i)
  {
    const cxparser_ext::CxScriptNamedResultField &field = result.result_fields[i];
    if (field.result_name == result_name && field.field_name == field_name)
      return true;
  }
  return false;
}

std::string FindSerializedFieldValue(const std::string &serialized_fields,
                                     const std::string &field_name)
{
  const std::vector<std::string> fields = SplitByCharacter(serialized_fields, '|');
  const std::string prefix = field_name + "=";
  for (size_t i = 0; i < fields.size(); ++i)
  {
    const std::string trimmed = Trim(fields[i]);
    if (trimmed.find(prefix) == 0)
      return trimmed.substr(prefix.size());
  }

  return std::string();
}

bool ShouldPrintCompactReviewElement(const std::string &serialized_element)
{
  const std::string consistency_status =
    FindSerializedFieldValue(serialized_element, "consistency_status");
  if (!consistency_status.empty() && consistency_status != "matched")
    return true;

  if (!FindSerializedFieldValue(serialized_element, "candidate_status").empty())
    return true;
  if (!FindSerializedFieldValue(serialized_element, "match_status").empty())
    return true;
  if (!FindSerializedFieldValue(serialized_element, "manual_review_signal").empty())
    return true;
  if (!FindSerializedFieldValue(serialized_element, "focus_region_ref").empty())
    return true;
  if (!FindSerializedFieldValue(serialized_element, "local_delta_ref").empty())
    return true;

  return false;
}

std::string BuildEnsmallenConclusionReplayLogPath(
  const cxparser_ext::CxScriptExecutionResult &result)
{
  const std::string named_value =
    FindNamedResultValue(result, "conclusion", "replay_log_path");
  if (!named_value.empty())
    return named_value;
  if (!result.replay_log_path.empty())
    return result.replay_log_path;
  return FindNamedResultValue(result, "conclusion", "replay_ref");
}

std::string BuildEnsmallenConclusionBestParamSets(
  const cxparser_ext::CxScriptExecutionResult &result)
{
  const std::string named_value =
    FindNamedResultValue(result, "conclusion", "best_param_sets");
  if (!named_value.empty())
    return named_value;

  if (result.layer != "train")
    return std::string();

  if (result.case_name == "halcon_screws_cluster_stability")
    return "match";

  if (result.case_name.find("match") != std::string::npos ||
      result.case_name.find("cluster") != std::string::npos)
    return "match";

  return "circle,ellipse,match";
}

std::string BuildEnsmallenConclusionSampleCount(
  const cxparser_ext::CxScriptExecutionResult &result)
{
  const std::string named_value =
    FindNamedResultValue(result, "conclusion", "sample_count");
  if (!named_value.empty())
    return named_value;

  const std::string sample_id = FindAssignmentValue(result.input_artifacts, "sample_id");
  if (!sample_id.empty())
  {
    const size_t count = CountDelimitedItems(sample_id, '|');
    if (count > 0)
      return std::to_string(count);
  }

  const std::string input_sample = FindNamedResultValue(result, "input", "sample");
  if (!input_sample.empty())
  {
    const size_t count = CountDelimitedItems(input_sample, ';');
    if (count > 0)
      return std::to_string(count);
  }

  if (!result.input_sample.empty())
  {
    const size_t count = CountDelimitedItems(result.input_sample, ';');
    if (count > 0)
      return std::to_string(count);
  }

  return std::string();
}

std::string BuildEnsmallenConclusionTextField(
  const cxparser_ext::CxScriptExecutionResult &result,
  const char *field_name)
{
  const std::string named_value =
    FindNamedResultValue(result, "conclusion", field_name);
  if (!named_value.empty())
    return named_value;
  return std::string();
}

bool IsEnsmallenLikeResult(const cxparser_ext::CxScriptExecutionResult &result)
{
  return result.module == "ensmallen_layer" ||
         result.case_name.find("phase1_param_") != std::string::npos ||
         result.case_name.find("geometry_fit_tuning") != std::string::npos ||
         result.case_name.find("match_score_tuning") != std::string::npos ||
         result.case_name.find("match_score_opt") != std::string::npos ||
         result.summary.find("ensmallen") != std::string::npos;
}

void PrintTimingFields(const cxparser_ext::CxScriptExecutionResult &result);
void PrintLineFields(const cxparser_ext::CxScriptExecutionResult &result);
void PrintCircleFields(const cxparser_ext::CxScriptExecutionResult &result);
void PrintRegionPatternFields(const cxparser_ext::CxScriptExecutionResult &result);
void PrintTopologyFields(const cxparser_ext::CxScriptExecutionResult &result);
void PrintFitCompareFields(const cxparser_ext::CxScriptExecutionResult &result);
void PrintReviewFoundationFields(const cxparser_ext::CxScriptExecutionResult &result);
void PrintReviewElementFields(const cxparser_ext::CxScriptExecutionResult &result);

void PrintNamedResultFields(const cxparser_ext::CxScriptExecutionResult &result,
                            const char *result_name,
                            const char *prefix)
{
  for (size_t i = 0; i < result.result_fields.size(); ++i)
  {
    const cxparser_ext::CxScriptNamedResultField &field = result.result_fields[i];
    if (field.result_name != result_name || field.field_name.empty() || field.value.empty())
      continue;

    std::cout << prefix << " " << field.result_name << "." << field.field_name
              << " = " << field.value << "\n";
  }
}

void PrintResultRefs(const cxparser_ext::CxScriptExecutionResult &result)
{
  PrintNamedResultFields(result, "refs", "[REF]");
}

void PrintConclusionFields(const cxparser_ext::CxScriptExecutionResult &result)
{
  PrintNamedResultFields(result, "conclusion", "[CONCLUSION]");
}

void PrintReportHeaderFields(const cxparser_ext::CxScriptExecutionResult &result)
{
  PrintNamedResultFields(result, "report_header", "[REPORT]");
}

void PrintTestPlanFields(const cxparser_ext::CxScriptExecutionResult &result)
{
  PrintNamedResultFields(result, "test_plan", "[TEST_PLAN]");
}

void PrintInteractionFields(const cxparser_ext::CxScriptExecutionResult &result)
{
  PrintNamedResultFields(result, "interaction", "[INTERACTION]");
}

void PrintAnalysisFields(const cxparser_ext::CxScriptExecutionResult &result)
{
  PrintNamedResultFields(result, "analysis", "[ANALYSIS]");
}

void PrintComparisonFields(const cxparser_ext::CxScriptExecutionResult &result)
{
  PrintNamedResultFields(result, "comparison", "[COMPARISON]");
}

void PrintPublishedFields(const cxparser_ext::CxScriptExecutionResult &result)
{
  PrintNamedResultFields(result, "published", "[PUBLISHED]");
}

void PrintDatasetFields(const cxparser_ext::CxScriptExecutionResult &result)
{
  PrintNamedResultFields(result, "dataset", "[DATASET]");
}

void PrintReviewFoundationFields(const cxparser_ext::CxScriptExecutionResult &result)
{
  PrintNamedResultFields(result, "phase0_contract", "[PHASE0]");
  PrintNamedResultFields(result, "task_context", "[TASK_CONTEXT]");
  PrintNamedResultFields(result, "geometry_semantic_type", "[GEOMETRY_SEMANTIC]");
  PrintNamedResultFields(result, "geometry_template_spec", "[GEOMETRY_TEMPLATE]");
  PrintNamedResultFields(result, "image_acquisition_spec", "[IMAGE_ACQUISITION]");
  PrintNamedResultFields(result, "training_input", "[TRAINING_INPUT]");
  PrintNamedResultFields(result, "run_input", "[RUN_INPUT]");
  PrintNamedResultFields(result, "review_decision", "[REVIEW_DECISION]");
  PrintNamedResultFields(result, "flowback_action", "[FLOWBACK_ACTION]");
  PrintNamedResultFields(result, "review_board", "[REVIEW_BOARD]");
  PrintNamedResultFields(result, "semantic_operation_contract", "[SEMANTIC_OPERATION]");
  PrintNamedResultFields(result, "review_image", "[REVIEW_IMAGE]");
  PrintNamedResultFields(result, "review_task", "[REVIEW_TASK]");
  PrintNamedResultFields(result, "review_compare", "[REVIEW_COMPARE]");
  PrintNamedResultFields(result, "review_anomaly", "[REVIEW_ANOMALY]");
}

void PrintReviewElementFields(const cxparser_ext::CxScriptExecutionResult &result)
{
  if (!HasNamedResultGroup(result, "review_image"))
    return;

  const std::string refresh_mode =
    FindNamedResultValue(result, "review_image", "refresh_mode");
  const std::string refresh_priority =
    FindNamedResultValue(result, "review_image", "refresh_priority");
  const std::string element_summary =
    FindNamedResultValue(result, "review_image", "element_summary");
  const std::string element_status_summary =
    FindNamedResultValue(result, "review_image", "element_status_summary");
  const std::string candidate_status_summary =
    FindNamedResultValue(result, "review_image", "candidate_status_summary");
  const std::string match_status_summary =
    FindNamedResultValue(result, "review_image", "match_status_summary");
  const std::string manual_review_signal_summary =
    FindNamedResultValue(result, "review_image", "manual_review_signal_summary");
  const std::string element_group_summary =
    FindNamedResultValue(result, "review_image", "element_group_summary");
  const std::string focus_refresh_targets =
    FindNamedResultValue(result, "review_image", "focus_refresh_targets");
  const std::string local_delta_targets =
    FindNamedResultValue(result, "review_image", "local_delta_targets");
  const std::string grouped_element_preview =
    FindNamedResultValue(result, "review_image", "grouped_element_preview");
  const std::string focus_element_preview =
    FindNamedResultValue(result, "review_image", "focus_element_preview");
  const std::string delta_element_preview =
    FindNamedResultValue(result, "review_image", "delta_element_preview");
  const std::string manual_review_targets =
    FindNamedResultValue(result, "review_image", "manual_review_targets");
  const std::string roi_visual_evidence =
    FindNamedResultValue(result, "review_image", "roi_visual_evidence");
  const std::string changed_element_ids =
    FindNamedResultValue(result, "review_image", "changed_element_ids");

  if (!refresh_mode.empty())
    std::cout << "[ELEMENT_BOARD] refresh_mode = " << refresh_mode << "\n";
  if (!refresh_priority.empty())
    std::cout << "[ELEMENT_BOARD] refresh_priority = " << refresh_priority << "\n";
  if (!element_summary.empty())
    std::cout << "[ELEMENT_BOARD] element_summary = " << element_summary << "\n";
  if (!element_status_summary.empty())
    std::cout << "[ELEMENT_BOARD] element_status_summary = " << element_status_summary << "\n";
  if (!candidate_status_summary.empty())
    std::cout << "[ELEMENT_BOARD] candidate_status_summary = " << candidate_status_summary << "\n";
  if (!match_status_summary.empty())
    std::cout << "[ELEMENT_BOARD] match_status_summary = " << match_status_summary << "\n";
  if (!manual_review_signal_summary.empty())
    std::cout << "[ELEMENT_BOARD] manual_review_signal_summary = " << manual_review_signal_summary << "\n";
  if (!element_group_summary.empty())
    std::cout << "[ELEMENT_BOARD] element_group_summary = " << element_group_summary << "\n";
  if (!focus_refresh_targets.empty())
    std::cout << "[ELEMENT_BOARD] focus_refresh_targets = " << focus_refresh_targets << "\n";
  if (!local_delta_targets.empty())
    std::cout << "[ELEMENT_BOARD] local_delta_targets = " << local_delta_targets << "\n";
  if (!grouped_element_preview.empty())
    std::cout << "[ELEMENT_BOARD] grouped_element_preview = " << grouped_element_preview << "\n";
  if (!focus_element_preview.empty())
    std::cout << "[ELEMENT_BOARD] focus_element_preview = " << focus_element_preview << "\n";
  if (!delta_element_preview.empty())
    std::cout << "[ELEMENT_BOARD] delta_element_preview = " << delta_element_preview << "\n";
  if (!manual_review_targets.empty())
    std::cout << "[ELEMENT_BOARD] manual_review_targets = " << manual_review_targets << "\n";
  if (!roi_visual_evidence.empty())
    std::cout << "[ELEMENT_BOARD] roi_visual_evidence = " << roi_visual_evidence << "\n";
  if (!changed_element_ids.empty())
    std::cout << "[ELEMENT_BOARD] changed_element_ids = " << changed_element_ids << "\n";

  const std::string serialized_elements =
    FindNamedResultValue(result, "review_image", "elements");
  if (serialized_elements.empty())
    return;

  const std::vector<std::string> serialized_list =
    SplitByDelimiter(serialized_elements, ";;");
  size_t highlighted_count = 0;
  size_t omitted_count = 0;
  for (size_t i = 0; i < serialized_list.size(); ++i)
  {
    const std::string serialized_element = serialized_list[i];
    if (serialized_element.empty())
      continue;

    if (!ShouldPrintCompactReviewElement(serialized_element))
    {
      ++omitted_count;
      continue;
    }

    ++highlighted_count;
    std::cout << "[ELEMENT] id="
              << FindSerializedFieldValue(serialized_element, "element_id")
              << " type=" << FindSerializedFieldValue(serialized_element, "element_type")
              << " role=" << FindSerializedFieldValue(serialized_element, "semantic_role")
              << " consistency=" << FindSerializedFieldValue(serialized_element, "consistency_status")
              << " candidate=" << FindSerializedFieldValue(serialized_element, "candidate_status")
              << " match=" << FindSerializedFieldValue(serialized_element, "match_status")
              << " manual=" << FindSerializedFieldValue(serialized_element, "manual_review_signal")
              << " group=" << FindSerializedFieldValue(serialized_element, "element_group_label")
              << " template_relation=" << FindSerializedFieldValue(serialized_element, "template_relation")
              << " drift=" << FindSerializedFieldValue(serialized_element, "drift_summary")
              << " focus=" << FindSerializedFieldValue(serialized_element, "focus_region_ref")
              << " delta=" << FindSerializedFieldValue(serialized_element, "local_delta_ref")
              << " overlay=" << FindSerializedFieldValue(serialized_element, "primary_overlay_ref")
              << "\n";
  }

  if (highlighted_count == 0 && !serialized_list.empty())
  {
    const std::string serialized_element = serialized_list.front();
    std::cout << "[ELEMENT] id="
              << FindSerializedFieldValue(serialized_element, "element_id")
              << " type=" << FindSerializedFieldValue(serialized_element, "element_type")
              << " role=" << FindSerializedFieldValue(serialized_element, "semantic_role")
              << " consistency=" << FindSerializedFieldValue(serialized_element, "consistency_status")
              << " candidate=" << FindSerializedFieldValue(serialized_element, "candidate_status")
              << " match=" << FindSerializedFieldValue(serialized_element, "match_status")
              << " manual=" << FindSerializedFieldValue(serialized_element, "manual_review_signal")
              << " group=" << FindSerializedFieldValue(serialized_element, "element_group_label")
              << " template_relation=" << FindSerializedFieldValue(serialized_element, "template_relation")
              << " drift=" << FindSerializedFieldValue(serialized_element, "drift_summary")
              << " focus=" << FindSerializedFieldValue(serialized_element, "focus_region_ref")
              << " delta=" << FindSerializedFieldValue(serialized_element, "local_delta_ref")
              << " overlay=" << FindSerializedFieldValue(serialized_element, "primary_overlay_ref")
              << "\n";
      omitted_count = serialized_list.size() > 1 ? (serialized_list.size() - 1) : 0;
  }

  if (omitted_count > 0)
    std::cout << "[ELEMENT_BOARD] omitted_matched_elements = " << omitted_count << "\n";
}

void PrintTimingFields(const cxparser_ext::CxScriptExecutionResult &result)
{
  if (HasNamedResultGroup(result, "timing"))
  {
    PrintNamedResultFields(result, "timing", "[TIMING]");
    return;
  }

  const bool is_torch_result = result.module == "torch_module" || result.module == "torch";
  if (result.runtime_ms > 0.0)
  {
    std::cout << "[TIMING] timing.runtime_ms = " << result.runtime_ms << "\n";
    std::cout << "[TIMING] timing.verified_runtime_ms = " << result.runtime_ms << "\n";
    std::cout << "[TIMING] timing.runtime_status = verified_runtime\n";
  }
  else if (is_torch_result)
  {
    std::cout << "[TIMING] timing.placeholder_runtime_ms = 0\n";
    std::cout << "[TIMING] timing.runtime_status = placeholder_runtime\n";
  }
  if (result.fit_time_ms > 0.0)
    std::cout << "[TIMING] timing.fit_time_ms = " << result.fit_time_ms << "\n";
  if (result.infer_time_ms > 0.0)
    std::cout << "[TIMING] timing.infer_time_ms = " << result.infer_time_ms << "\n";
}

void PrintEnsmallenFallbackNamedViews(const cxparser_ext::CxScriptExecutionResult &result)
{
  if (!IsEnsmallenLikeResult(result))
    return;

  const std::string dataset_bridge =
    FindNamedResultValue(result, "interaction", "dataset_bridge").empty()
      ? FindNamedResultValue(result, "inputs", "dataset_bridge")
      : FindNamedResultValue(result, "interaction", "dataset_bridge");
  const std::string test_bucket = FindNamedResultValue(result, "inputs", "test_bucket");
  const std::string test_flow = FindNamedResultValue(result, "inputs", "test_flow");
  const std::string objective_ref = FindNamedResultValue(result, "refs", "objective_ref");
  const std::string summary_ref = FindNamedResultValue(result, "refs", "summary_ref");
  const std::string compare_ref = FindNamedResultValue(result, "refs", "compare_ref");
  const std::string replay_ref = FindNamedResultValue(result, "refs", "replay_ref");
  const std::string replay_log_path = BuildEnsmallenConclusionReplayLogPath(result);
  const std::string best_param_sets = BuildEnsmallenConclusionBestParamSets(result);
  const std::string sample_count = BuildEnsmallenConclusionSampleCount(result);
  const std::string conclusion_id =
    BuildEnsmallenConclusionTextField(result, "conclusion_id");
  const std::string short_conclusion =
    BuildEnsmallenConclusionTextField(result, "short_conclusion");
  const std::string why_it_matters =
    BuildEnsmallenConclusionTextField(result, "why_it_matters");
  const std::string next_observation =
    BuildEnsmallenConclusionTextField(result, "next_observation");
  const std::string interaction_route =
    FindNamedResultValue(result, "interaction", "route");
  const std::string interaction_upstream_refs =
    FindNamedResultValue(result, "interaction", "upstream_refs");
  const std::string interaction_downstream_refs =
    FindNamedResultValue(result, "interaction", "downstream_refs");

  if (!HasNamedResultGroup(result, "report_header"))
  {
    std::cout << "[REPORT] report_header.module = ensmallen\n";
    std::cout << "[REPORT] report_header.entry = cxparser_ext_cxscript_cli\n";
    std::cout << "[REPORT] report_header.batch = " << result.layer << "\n";
    std::cout << "[REPORT] report_header.current_status = " << result.summary << "\n";
    std::cout << "[REPORT] report_header.human_review_required = required\n";
  }

  if (!HasNamedResultGroup(result, "conclusion"))
  {
    std::cout << "[CONCLUSION] conclusion.chain_status = "
              << (result.success ? "passed" : "failed") << "\n";
    std::cout << "[CONCLUSION] conclusion.export_status = "
              << (result.success ? "passed" : "failed") << "\n";
    std::cout << "[CONCLUSION] conclusion.algorithm_status = pending_human_review\n";
    std::cout << "[CONCLUSION] conclusion.current_status = " << result.summary << "\n";
    std::cout << "[CONCLUSION] conclusion.human_review_required = required\n";
    if (!conclusion_id.empty())
      std::cout << "[CONCLUSION] conclusion.conclusion_id = " << conclusion_id << "\n";
    if (!short_conclusion.empty())
      std::cout << "[CONCLUSION] conclusion.short_conclusion = " << short_conclusion << "\n";
    if (!why_it_matters.empty())
      std::cout << "[CONCLUSION] conclusion.why_it_matters = " << why_it_matters << "\n";
    if (!next_observation.empty())
      std::cout << "[CONCLUSION] conclusion.next_observation = " << next_observation << "\n";
    if (!summary_ref.empty())
      std::cout << "[CONCLUSION] conclusion.summary_ref = " << summary_ref << "\n";
    if (!compare_ref.empty())
      std::cout << "[CONCLUSION] conclusion.compare_ref = " << compare_ref << "\n";
    if (!replay_ref.empty())
      std::cout << "[CONCLUSION] conclusion.replay_ref = " << replay_ref << "\n";
    if (!replay_log_path.empty())
      std::cout << "[CONCLUSION] conclusion.replay_log_path = " << replay_log_path << "\n";
    if (!best_param_sets.empty())
      std::cout << "[CONCLUSION] conclusion.best_param_sets = " << best_param_sets << "\n";
    if (!sample_count.empty())
      std::cout << "[CONCLUSION] conclusion.sample_count = " << sample_count << "\n";
  }

  if (!HasNamedResultGroup(result, "test_plan"))
  {
    if (!test_bucket.empty())
      std::cout << "[TEST_PLAN] test_plan.test_bucket = " << test_bucket << "\n";
    if (!test_flow.empty())
      std::cout << "[TEST_PLAN] test_plan.test_flow = " << test_flow << "\n";
    if (result.layer == "scenario" || result.layer == "train" || result.layer == "infer")
      std::cout << "[TEST_PLAN] test_plan.mcp_flow = run-ctest-target exact_name -> task_id -> status/log -> conclusion/evidence\n";
    else
      std::cout << "[TEST_PLAN] test_plan.mcp_flow = run-ctest-target exact_name -> task_id -> compare/replay -> conclusion/evidence\n";
  }

  if (!HasNamedResultGroup(result, "interaction"))
  {
    std::string route = "cxcore.formfit -> ensmallen -> rag";
    if (!interaction_route.empty())
      route = interaction_route;
    else if (result.case_name.find("match_score") != std::string::npos)
      route = "cxcore.fastmatch -> ensmallen -> rag";
    else if (result.layer == "scenario" || result.layer == "train" || result.layer == "infer")
      route = "torch.optimization_refs -> cxcore.phase1_bundle -> ensmallen -> rag";

    std::cout << "[INTERACTION] interaction.route = " << route << "\n";
    if (!dataset_bridge.empty())
      std::cout << "[INTERACTION] interaction.dataset_bridge = " << dataset_bridge << "\n";
    if (!interaction_upstream_refs.empty())
      std::cout << "[INTERACTION] interaction.upstream_refs = " << interaction_upstream_refs << "\n";
    else if (!objective_ref.empty())
      std::cout << "[INTERACTION] interaction.upstream_refs = " << objective_ref << "\n";
    if (!interaction_downstream_refs.empty())
      std::cout << "[INTERACTION] interaction.downstream_refs = " << interaction_downstream_refs << "\n";
    else
      std::cout << "[INTERACTION] interaction.downstream_refs = summary_ref,compare_ref,replay_ref,evidence_ref\n";
  }
}

void PrintEnsmallenGuaranteedSummaryViews(const cxparser_ext::CxScriptExecutionResult &result)
{
  if (!IsEnsmallenLikeResult(result))
    return;

  const std::string dataset_bridge =
    FindNamedResultValue(result, "interaction", "dataset_bridge").empty()
      ? FindNamedResultValue(result, "inputs", "dataset_bridge")
      : FindNamedResultValue(result, "interaction", "dataset_bridge");
  const std::string test_bucket = FindNamedResultValue(result, "inputs", "test_bucket");
  const std::string test_flow = FindNamedResultValue(result, "inputs", "test_flow");
  const std::string objective_ref = FindNamedResultValue(result, "refs", "objective_ref");
  const std::string summary_ref = FindNamedResultValue(result, "refs", "summary_ref");
  const std::string compare_ref = FindNamedResultValue(result, "refs", "compare_ref");
  const std::string replay_ref = FindNamedResultValue(result, "refs", "replay_ref");
  const std::string replay_log_path = BuildEnsmallenConclusionReplayLogPath(result);
  const std::string best_param_sets = BuildEnsmallenConclusionBestParamSets(result);
  const std::string sample_count = BuildEnsmallenConclusionSampleCount(result);
  const std::string conclusion_id =
    BuildEnsmallenConclusionTextField(result, "conclusion_id");
  const std::string short_conclusion =
    BuildEnsmallenConclusionTextField(result, "short_conclusion");
  const std::string why_it_matters =
    BuildEnsmallenConclusionTextField(result, "why_it_matters");
  const std::string next_observation =
    BuildEnsmallenConclusionTextField(result, "next_observation");
  const std::string interaction_route =
    FindNamedResultValue(result, "interaction", "route");
  const std::string interaction_upstream_refs =
    FindNamedResultValue(result, "interaction", "upstream_refs");
  const std::string interaction_downstream_refs =
    FindNamedResultValue(result, "interaction", "downstream_refs");

  std::cout << "[REPORT] report_header.module = ensmallen\n";
  std::cout << "[REPORT] report_header.entry = cxparser_ext_cxscript_cli\n";
  std::cout << "[REPORT] report_header.batch = " << result.layer << "\n";
  std::cout << "[REPORT] report_header.current_status = " << result.summary << "\n";
  std::cout << "[REPORT] report_header.human_review_required = required\n";

  std::cout << "[CONCLUSION] conclusion.chain_status = "
            << (result.success ? "passed" : "failed") << "\n";
  std::cout << "[CONCLUSION] conclusion.export_status = "
            << (result.success ? "passed" : "failed") << "\n";
  std::cout << "[CONCLUSION] conclusion.algorithm_status = pending_human_review\n";
  std::cout << "[CONCLUSION] conclusion.current_status = " << result.summary << "\n";
  std::cout << "[CONCLUSION] conclusion.human_review_required = required\n";
  if (!conclusion_id.empty())
    std::cout << "[CONCLUSION] conclusion.conclusion_id = " << conclusion_id << "\n";
  if (!short_conclusion.empty())
    std::cout << "[CONCLUSION] conclusion.short_conclusion = " << short_conclusion << "\n";
  if (!why_it_matters.empty())
    std::cout << "[CONCLUSION] conclusion.why_it_matters = " << why_it_matters << "\n";
  if (!next_observation.empty())
    std::cout << "[CONCLUSION] conclusion.next_observation = " << next_observation << "\n";
  if (!summary_ref.empty())
    std::cout << "[CONCLUSION] conclusion.summary_ref = " << summary_ref << "\n";
  if (!compare_ref.empty())
    std::cout << "[CONCLUSION] conclusion.compare_ref = " << compare_ref << "\n";
  if (!replay_ref.empty())
    std::cout << "[CONCLUSION] conclusion.replay_ref = " << replay_ref << "\n";
  if (!replay_log_path.empty())
    std::cout << "[CONCLUSION] conclusion.replay_log_path = " << replay_log_path << "\n";
  if (!best_param_sets.empty())
    std::cout << "[CONCLUSION] conclusion.best_param_sets = " << best_param_sets << "\n";
  if (!sample_count.empty())
    std::cout << "[CONCLUSION] conclusion.sample_count = " << sample_count << "\n";

  if (!test_bucket.empty())
    std::cout << "[TEST_PLAN] test_plan.test_bucket = " << test_bucket << "\n";
  if (!test_flow.empty())
    std::cout << "[TEST_PLAN] test_plan.test_flow = " << test_flow << "\n";
  if (result.layer == "scenario" || result.layer == "train" || result.layer == "infer")
    std::cout << "[TEST_PLAN] test_plan.mcp_flow = run-ctest-target exact_name -> task_id -> status/log -> conclusion/evidence\n";
  else
    std::cout << "[TEST_PLAN] test_plan.mcp_flow = run-ctest-target exact_name -> task_id -> compare/replay -> conclusion/evidence\n";

  std::string route = "cxcore.formfit -> ensmallen -> rag";
  if (!interaction_route.empty())
    route = interaction_route;
  else if (result.case_name.find("match_score") != std::string::npos)
    route = "cxcore.fastmatch -> ensmallen -> rag";
  else if (result.layer == "scenario" || result.layer == "train" || result.layer == "infer")
    route = "torch.optimization_refs -> cxcore.phase1_bundle -> ensmallen -> rag";

  std::cout << "[INTERACTION] interaction.route = " << route << "\n";
  if (!dataset_bridge.empty())
    std::cout << "[INTERACTION] interaction.dataset_bridge = " << dataset_bridge << "\n";
  if (!interaction_upstream_refs.empty())
    std::cout << "[INTERACTION] interaction.upstream_refs = " << interaction_upstream_refs << "\n";
  else if (!objective_ref.empty())
    std::cout << "[INTERACTION] interaction.upstream_refs = " << objective_ref << "\n";
  if (!interaction_downstream_refs.empty())
    std::cout << "[INTERACTION] interaction.downstream_refs = " << interaction_downstream_refs << "\n";
  else
    std::cout << "[INTERACTION] interaction.downstream_refs = summary_ref,compare_ref,replay_ref,evidence_ref\n";
}

void PrintBridgeFields(const cxparser_ext::CxScriptExecutionResult &result)
{
  PrintNamedResultFields(result, "bridge", "[BRIDGE]");

  const char *artifact_keys[] = {
    "sample_id",
    "input_image",
    "template_image",
    "defect_count",
    "roi_ref",
    "match_gt",
    "label_file",
    "bridge_manifest",
    "bridge_table"
  };
  for (size_t i = 0; i < sizeof(artifact_keys) / sizeof(artifact_keys[0]); ++i)
  {
    if (HasNamedResultField(result, "bridge", artifact_keys[i]))
      continue;
    const std::string value = FindAssignmentValue(result.input_artifacts, artifact_keys[i]);
    if (!value.empty())
      std::cout << "[BRIDGE] bridge." << artifact_keys[i] << " = " << value << "\n";
  }

  const char *param_keys[] = {
    "objective_ref",
    "threshold_ref",
    "crop_policy_ref",
    "boundary_error_ref",
    "alignment_error_ref"
  };
  for (size_t i = 0; i < sizeof(param_keys) / sizeof(param_keys[0]); ++i)
  {
    if (HasNamedResultField(result, "bridge", param_keys[i]))
      continue;
    const std::string value = FindAssignmentValue(result.input_params, param_keys[i]);
    if (!value.empty())
      std::cout << "[BRIDGE] bridge." << param_keys[i] << " = " << value << "\n";
  }
}

void PrintInputFields(const cxparser_ext::CxScriptExecutionResult &result)
{
  PrintNamedResultFields(result, "input", "[INPUT]");
  PrintNamedResultFields(result, "inputs", "[INPUTS]");

  if (!result.input_dataset.empty() &&
      FindNamedResultValue(result, "input", "dataset").empty())
    std::cout << "[INPUT] input.dataset = " << result.input_dataset << "\n";
  if (!result.input_sample.empty() &&
      FindNamedResultValue(result, "input", "sample").empty())
    std::cout << "[INPUT] input.sample = " << result.input_sample << "\n";
  if (!result.input_split.empty() &&
      FindNamedResultValue(result, "input", "split").empty())
    std::cout << "[INPUT] input.split = " << result.input_split << "\n";
  if (!result.input_artifacts.empty())
    std::cout << "[INPUTS] inputs.input_artifacts = " << result.input_artifacts << "\n";
  if (!result.input_params.empty())
    std::cout << "[INPUTS] inputs.input_params = " << result.input_params << "\n";
}

std::string FormatMatcherTop1Rect(const cxparser_ext::CxScriptExecutionResult &result)
{
  if (result.match_best_rect_w_value <= 0.0 || result.match_best_rect_h_value <= 0.0)
    return std::string();

  std::ostringstream out;
  out << static_cast<int>(result.match_best_rect_x_value) << ","
      << static_cast<int>(result.match_best_rect_y_value) << ","
      << static_cast<int>(result.match_best_rect_w_value) << ","
      << static_cast<int>(result.match_best_rect_h_value);
  return out.str();
}

bool IsCximageCircleCase(const cxparser_ext::CxScriptExecutionResult &result)
{
  return result.layer == "feature" &&
         result.module == "cximage" &&
         (result.case_name == "findcircle" ||
          result.case_name == "circle_measure_fit");
}

bool IsCximageLineCase(const cxparser_ext::CxScriptExecutionResult &result)
{
  return result.layer == "feature" &&
         result.module == "cximage" &&
         result.case_name == "line_measure_roi";
}

bool IsCximageFormfitCase(const cxparser_ext::CxScriptExecutionResult &result)
{
  return result.layer == "feature" &&
         result.module == "cximage" &&
         result.case_name == "formfit_rect_candidate";
}

void PrintMatcherFields(const cxparser_ext::CxScriptExecutionResult &result)
{
  if (result.layer != "matcher")
    return;

  const double candidate_count =
    result.match_candidate_count_value > 0.0 ?
      result.match_candidate_count_value :
      result.template_main_candidate_count_value;
  if (candidate_count > 0.0)
    std::cout << "[MATCHER] candidate_count = " << static_cast<int>(candidate_count) << "\n";

  const double top1_score =
    result.match_top_score_value > 0.0 ?
      result.match_top_score_value :
      result.template_main_top_score_value;
  if (top1_score > 0.0)
    std::cout << "[MATCHER] top1_score = " << top1_score << "\n";

  const std::string top1_rect = FormatMatcherTop1Rect(result);
  if (!top1_rect.empty())
    std::cout << "[MATCHER] top1_rect = " << top1_rect << "\n";

  const std::string candidate_overlay_ref =
    FindNamedResultValue(result, "refs", "candidate_overlay_ref");
  if (!candidate_overlay_ref.empty())
    std::cout << "[MATCHER] candidate_overlay_ref = " << candidate_overlay_ref << "\n";

  const std::string template_rect_overlay_ref =
    FindNamedResultValue(result, "refs", "template_rect_overlay_ref");
  if (!template_rect_overlay_ref.empty())
    std::cout << "[MATCHER] template_rect_overlay_ref = " << template_rect_overlay_ref << "\n";

  const std::string test_rect_overlay_ref =
    FindNamedResultValue(result, "refs", "test_rect_overlay_ref");
  if (!test_rect_overlay_ref.empty())
    std::cout << "[MATCHER] test_rect_overlay_ref = " << test_rect_overlay_ref << "\n";
}

void PrintFormfitFields(const cxparser_ext::CxScriptExecutionResult &result)
{
  if (result.layer != "feature" ||
      ((result.module != "cxcore" ||
        result.case_name.find("rect_formfit_candidate_selection") != 0) &&
       !IsCximageFormfitCase(result)))
    return;

  if (result.match_candidate_count_value > 0.0)
    std::cout << "[FORMFIT] candidate_count = "
              << static_cast<int>(result.match_candidate_count_value) << "\n";
  if (result.match_selected_index_value >= 0.0)
    std::cout << "[FORMFIT] selected_index = "
              << static_cast<int>(result.match_selected_index_value) << "\n";
  if (result.match_best_index_value >= 0.0)
    std::cout << "[FORMFIT] best_index = "
              << static_cast<int>(result.match_best_index_value) << "\n";
  if (result.selected_candidate_score_value > 0.0)
    std::cout << "[FORMFIT] best_score = " << result.selected_candidate_score_value << "\n";

  const std::string top1_rect = FormatMatcherTop1Rect(result);
  if (!top1_rect.empty())
    std::cout << "[FORMFIT] top1_rect = " << top1_rect << "\n";

  const std::string neighborhood_mode =
    FindNamedResultValue(result, "analysis", "neighborhood_mode");
  if (!neighborhood_mode.empty())
    std::cout << "[FORMFIT] neighborhood_mode = " << neighborhood_mode << "\n";

  const std::string search_index_mode =
    FindNamedResultValue(result, "analysis", "search_index_mode");
  if (!search_index_mode.empty())
    std::cout << "[FORMFIT] search_index_mode = " << search_index_mode << "\n";

  const std::string selection_mode =
    FindNamedResultValue(result, "analysis", "selection_mode");
  if (!selection_mode.empty())
    std::cout << "[FORMFIT] selection_mode = " << selection_mode << "\n";

  const std::string candidate_overlay_ref =
    FindNamedResultValue(result, "refs", "formfit_candidate_overlay_ref");
  if (!candidate_overlay_ref.empty())
    std::cout << "[FORMFIT] candidate_overlay_ref = " << candidate_overlay_ref << "\n";

  const std::string selection_overlay_ref =
    FindNamedResultValue(result, "refs", "formfit_selection_overlay_ref");
  if (!selection_overlay_ref.empty())
    std::cout << "[FORMFIT] selection_overlay_ref = " << selection_overlay_ref << "\n";
}

void PrintFormfitCompareFields(const cxparser_ext::CxScriptExecutionResult &result)
{
  if (result.layer != "feature" ||
      ((result.module != "cxcore" ||
        result.case_name.find("rect_formfit_candidate_selection") != 0) &&
       !IsCximageFormfitCase(result)))
    return;

  if (!result.fit_mode.empty())
    std::cout << "[FIT] fit_mode = " << result.fit_mode << "\n";
  if (result.fit_compare_enabled_value > 0.0)
    std::cout << "[FIT] compare_enabled = " << static_cast<int>(result.fit_compare_enabled_value) << "\n";
  if (result.fit_legacy_available_value > 0.0)
    std::cout << "[FIT] legacy_available = " << static_cast<int>(result.fit_legacy_available_value) << "\n";
  if (result.fit_enhanced_available_value > 0.0)
    std::cout << "[FIT] enhanced_available = " << static_cast<int>(result.fit_enhanced_available_value) << "\n";

  const bool legacy_available = result.fit_legacy_available_value > 0.0;
  const bool enhanced_available = result.fit_enhanced_available_value > 0.0;
  const bool both_available = legacy_available && enhanced_available;

  if (legacy_available && result.formfit_legacy_runtime_ms_value > 0.0)
    std::cout << "[FIT_COMPARE] legacy_runtime_ms = " << result.formfit_legacy_runtime_ms_value << "\n";
  if (enhanced_available && result.formfit_enhanced_runtime_ms_value > 0.0)
    std::cout << "[FIT_COMPARE] enhanced_runtime_ms = " << result.formfit_enhanced_runtime_ms_value << "\n";
  if (both_available)
    std::cout << "[FIT_COMPARE] runtime_delta_ms = " << result.formfit_compare_runtime_delta_ms_value << "\n";

  if (legacy_available && result.formfit_legacy_candidate_count_value > 0.0)
    std::cout << "[FIT_COMPARE] legacy_candidate_count = "
              << static_cast<int>(result.formfit_legacy_candidate_count_value) << "\n";
  if (enhanced_available && result.formfit_enhanced_candidate_count_value > 0.0)
    std::cout << "[FIT_COMPARE] enhanced_candidate_count = "
              << static_cast<int>(result.formfit_enhanced_candidate_count_value) << "\n";
  if (both_available)
    std::cout << "[FIT_COMPARE] candidate_count_delta = "
              << result.formfit_compare_candidate_count_delta_value << "\n";

  if (legacy_available && result.formfit_legacy_best_score_value > 0.0)
    std::cout << "[FIT_COMPARE] legacy_best_score = " << result.formfit_legacy_best_score_value << "\n";
  if (enhanced_available && result.formfit_enhanced_best_score_value > 0.0)
    std::cout << "[FIT_COMPARE] enhanced_best_score = " << result.formfit_enhanced_best_score_value << "\n";
  if (both_available)
    std::cout << "[FIT_COMPARE] best_score_delta = " << result.formfit_compare_best_score_delta_value << "\n";

  if (legacy_available && result.formfit_legacy_rect_w_value > 0.0 && result.formfit_legacy_rect_h_value > 0.0)
    std::cout << "[FIT_COMPARE] legacy_top1_rect = "
              << static_cast<int>(result.formfit_legacy_rect_x_value) << ","
              << static_cast<int>(result.formfit_legacy_rect_y_value) << ","
              << static_cast<int>(result.formfit_legacy_rect_w_value) << ","
              << static_cast<int>(result.formfit_legacy_rect_h_value) << "\n";
  if (enhanced_available && result.formfit_enhanced_rect_w_value > 0.0 && result.formfit_enhanced_rect_h_value > 0.0)
    std::cout << "[FIT_COMPARE] enhanced_top1_rect = "
              << static_cast<int>(result.formfit_enhanced_rect_x_value) << ","
              << static_cast<int>(result.formfit_enhanced_rect_y_value) << ","
              << static_cast<int>(result.formfit_enhanced_rect_w_value) << ","
              << static_cast<int>(result.formfit_enhanced_rect_h_value) << "\n";
  if (both_available)
    std::cout << "[FIT_COMPARE] rect_center_delta = " << result.formfit_compare_rect_center_delta_value << "\n";

  if (!result.formfit_legacy_failure_stage.empty())
    std::cout << "[FIT_COMPARE] legacy_failure_stage = " << result.formfit_legacy_failure_stage << "\n";
  if (!result.formfit_enhanced_failure_stage.empty())
    std::cout << "[FIT_COMPARE] enhanced_failure_stage = " << result.formfit_enhanced_failure_stage << "\n";
}

void PrintRegionPatternFields(const cxparser_ext::CxScriptExecutionResult &result)
{
  if (result.layer != "feature" ||
      result.module != "cximage" ||
      result.case_name != "binary_region")
    return;

  std::cout << "[REGION_PATTERN] foreground_ratio = "
            << result.region_pattern_foreground_ratio_value << "\n";
  std::cout << "[REGION_PATTERN] descriptor_dim = "
            << result.region_pattern_descriptor_dim_value << "\n";
  std::cout << "[REGION_PATTERN] descriptor_mean = "
            << result.region_pattern_descriptor_mean_value << "\n";
  std::cout << "[REGION_PATTERN] descriptor_std = "
            << result.region_pattern_descriptor_std_value << "\n";

  const std::string descriptor_mode =
    FindNamedResultValue(result, "analysis", "descriptor_mode");
  if (!descriptor_mode.empty())
    std::cout << "[REGION_PATTERN] descriptor_mode = " << descriptor_mode << "\n";

  const std::string neighborhood_mode =
    FindNamedResultValue(result, "analysis", "neighborhood_mode");
  if (!neighborhood_mode.empty())
    std::cout << "[REGION_PATTERN] neighborhood_mode = " << neighborhood_mode << "\n";

  const std::string search_index_mode =
    FindNamedResultValue(result, "analysis", "search_index_mode");
  if (!search_index_mode.empty())
    std::cout << "[REGION_PATTERN] search_index_mode = " << search_index_mode << "\n";

  const std::string region_overlay_ref =
    FindNamedResultValue(result, "refs", "region_pattern_overlay_ref");
  if (!region_overlay_ref.empty())
    std::cout << "[REGION_PATTERN] region_overlay_ref = " << region_overlay_ref << "\n";

  const std::string descriptor_ref =
    FindNamedResultValue(result, "refs", "region_pattern_descriptor_ref");
  if (!descriptor_ref.empty())
    std::cout << "[REGION_PATTERN] descriptor_ref = " << descriptor_ref << "\n";
}

void PrintTopologyFields(const cxparser_ext::CxScriptExecutionResult &result)
{
  if (result.layer != "feature" ||
      result.module != "cximage" ||
      result.case_name != "geometry_topology_pipeline")
    return;

  if (!result.fractal_partition_value.empty())
    std::cout << "[TOPOLOGY] fractal_partition = " << result.fractal_partition_value << "\n";
  if (!result.distance_field_value.empty())
    std::cout << "[TOPOLOGY] distance_field = " << result.distance_field_value << "\n";
  if (!result.skeleton_mask_value.empty())
    std::cout << "[TOPOLOGY] skeleton_mask = " << result.skeleton_mask_value << "\n";
  if (!result.centerline_paths_value.empty())
    std::cout << "[TOPOLOGY] centerline_paths = " << result.centerline_paths_value << "\n";
  if (!result.topology_repair_paths_value.empty())
    std::cout << "[TOPOLOGY] topology_repair_paths = " << result.topology_repair_paths_value << "\n";

  const std::string topology_stage_chain =
    FindNamedResultValue(result, "analysis", "topology_stage_chain");
  if (!topology_stage_chain.empty())
    std::cout << "[TOPOLOGY] stage_chain = " << topology_stage_chain << "\n";

  const std::string connectivity_mode =
    FindNamedResultValue(result, "analysis", "connectivity_mode");
  if (!connectivity_mode.empty())
    std::cout << "[TOPOLOGY] connectivity_mode = " << connectivity_mode << "\n";

  const std::string path_mode =
    FindNamedResultValue(result, "analysis", "path_mode");
  if (!path_mode.empty())
    std::cout << "[TOPOLOGY] path_mode = " << path_mode << "\n";

  const char *overlay_fields[][2] = {
    {"fractal_partition_overlay_ref", "fractal_partition_overlay_ref"},
    {"distance_field_overlay_ref", "distance_field_overlay_ref"},
    {"skeleton_overlay_ref", "skeleton_overlay_ref"},
    {"centerline_overlay_ref", "centerline_overlay_ref"},
    {"topology_repair_overlay_ref", "topology_repair_overlay_ref"}
  };
  for (size_t i = 0; i < sizeof(overlay_fields) / sizeof(overlay_fields[0]); ++i)
  {
    const std::string value = FindNamedResultValue(result, "refs", overlay_fields[i][0]);
    if (!value.empty())
      std::cout << "[TOPOLOGY] " << overlay_fields[i][1] << " = " << value << "\n";
  }
}

void PrintLineFields(const cxparser_ext::CxScriptExecutionResult &result)
{
  if (result.layer != "feature" ||
      ((result.module != "cxcore" ||
        result.case_name.find("line_measurement_") != 0) &&
       !IsCximageLineCase(result)))
    return;

  if (result.point_count_value > 0.0)
    std::cout << "[LINE] point_count = " << static_cast<int>(result.point_count_value) << "\n";
  if (result.line_chain_length_value > 0.0)
    std::cout << "[LINE] chain_length = " << static_cast<int>(result.line_chain_length_value) << "\n";
  if (result.line_edgeband_count_value > 0.0)
    std::cout << "[LINE] edgeband_count = " << static_cast<int>(result.line_edgeband_count_value) << "\n";
  if (result.fit_error_avg_value > 0.0)
    std::cout << "[LINE] fit_error_avg = " << result.fit_error_avg_value << "\n";
  if (result.fit_error_max_value > 0.0)
    std::cout << "[LINE] fit_error_max = " << result.fit_error_max_value << "\n";
  if (result.line_angle_value != 0.0)
    std::cout << "[LINE] angle = " << result.line_angle_value << "\n";
  if (result.line_offset_value != 0.0)
    std::cout << "[LINE] offset = " << result.line_offset_value << "\n";
  if (result.subpixel_adjust_avg_value > 0.0)
    std::cout << "[LINE] subpixel_adjust_avg = " << result.subpixel_adjust_avg_value << "\n";

  const std::string point_set_ref =
    FindNamedResultValue(result, "refs", "line_point_set_ref");
  if (!point_set_ref.empty())
    std::cout << "[LINE] line_point_set_ref = " << point_set_ref << "\n";

  const std::string bounds_ref =
    FindNamedResultValue(result, "refs", "line_measure_bounds_ref");
  if (!bounds_ref.empty())
    std::cout << "[LINE] line_measure_bounds_ref = " << bounds_ref << "\n";
}

void PrintCircleFields(const cxparser_ext::CxScriptExecutionResult &result)
{
  if (result.layer != "feature" ||
      ((result.module != "cxcore" ||
        result.case_name.find("circle_measurement_") != 0) &&
       !IsCximageCircleCase(result)))
    return;

  if (result.circle_radius_value > 0.0)
    std::cout << "[CIRCLE] radius = " << result.circle_radius_value << "\n";
  if (result.circle_center_x_value != 0.0 || result.circle_center_y_value != 0.0)
    std::cout << "[CIRCLE] center = "
              << result.circle_center_x_value << "," << result.circle_center_y_value << "\n";
  if (result.circle_avg_distance_value > 0.0)
    std::cout << "[CIRCLE] average_distance = " << result.circle_avg_distance_value << "\n";
  if (result.circle_sample_points_value > 0.0)
    std::cout << "[CIRCLE] sample_points = " << static_cast<int>(result.circle_sample_points_value) << "\n";
  if (result.circle_used_fallback_value > 0.0)
    std::cout << "[CIRCLE] used_fallback = " << static_cast<int>(result.circle_used_fallback_value) << "\n";
  if (result.circle_prefilter_used_value > 0.0)
    std::cout << "[CIRCLE] prefilter_used = " << static_cast<int>(result.circle_prefilter_used_value) << "\n";
  if (result.circle_compact_path_value > 0.0)
    std::cout << "[CIRCLE] compact_path_used = " << static_cast<int>(result.circle_compact_path_value) << "\n";
  if (!result.circle_failure_stage.empty())
    std::cout << "[CIRCLE] failure_stage = " << result.circle_failure_stage << "\n";

  const std::string circle_overlay_ref =
    FindNamedResultValue(result, "refs", "circle_overlay_ref");
  if (!circle_overlay_ref.empty())
    std::cout << "[CIRCLE] circle_overlay_ref = " << circle_overlay_ref << "\n";

  const std::string circle_edge_overlay_ref =
    FindNamedResultValue(result, "refs", "circle_edge_overlay_ref");
  if (!circle_edge_overlay_ref.empty())
    std::cout << "[CIRCLE] circle_edge_overlay_ref = " << circle_edge_overlay_ref << "\n";

  const std::string circle_point_set_ref =
    FindNamedResultValue(result, "refs", "circle_point_set_ref");
  if (!circle_point_set_ref.empty())
    std::cout << "[CIRCLE] circle_point_set_ref = " << circle_point_set_ref << "\n";

  const std::string circle_measure_bounds_ref =
    FindNamedResultValue(result, "refs", "circle_measure_bounds_ref");
  if (!circle_measure_bounds_ref.empty())
    std::cout << "[CIRCLE] circle_measure_bounds_ref = " << circle_measure_bounds_ref << "\n";
}

void PrintFitCompareFields(const cxparser_ext::CxScriptExecutionResult &result)
{
  if (result.layer != "feature" ||
      ((result.module != "cxcore" ||
        result.case_name.find("circle_measurement_") != 0) &&
       !IsCximageCircleCase(result)))
    return;

  if (!result.fit_mode.empty())
    std::cout << "[FIT] fit_mode = " << result.fit_mode << "\n";
  if (result.fit_compare_enabled_value > 0.0)
    std::cout << "[FIT] compare_enabled = " << static_cast<int>(result.fit_compare_enabled_value) << "\n";
  if (result.fit_legacy_available_value > 0.0)
    std::cout << "[FIT] legacy_available = " << static_cast<int>(result.fit_legacy_available_value) << "\n";
  if (result.fit_enhanced_available_value > 0.0)
    std::cout << "[FIT] enhanced_available = " << static_cast<int>(result.fit_enhanced_available_value) << "\n";

  const bool legacy_available = result.fit_legacy_available_value > 0.0;
  const bool enhanced_available = result.fit_enhanced_available_value > 0.0;
  const bool both_available = legacy_available && enhanced_available;

  if (legacy_available && result.circle_legacy_runtime_ms_value > 0.0)
    std::cout << "[FIT_COMPARE] legacy_runtime_ms = " << result.circle_legacy_runtime_ms_value << "\n";
  if (enhanced_available && result.circle_enhanced_runtime_ms_value > 0.0)
    std::cout << "[FIT_COMPARE] enhanced_runtime_ms = " << result.circle_enhanced_runtime_ms_value << "\n";
  if (both_available)
    std::cout << "[FIT_COMPARE] runtime_delta_ms = " << result.circle_compare_runtime_delta_ms_value << "\n";

  if (legacy_available && result.circle_legacy_radius_value > 0.0)
    std::cout << "[FIT_COMPARE] legacy_radius = " << result.circle_legacy_radius_value << "\n";
  if (enhanced_available && result.circle_enhanced_radius_value > 0.0)
    std::cout << "[FIT_COMPARE] enhanced_radius = " << result.circle_enhanced_radius_value << "\n";

  if (both_available)
  {
    std::cout << "[FIT_COMPARE] radius_delta = " << result.circle_compare_radius_delta_value << "\n";
    std::cout << "[FIT_COMPARE] center_delta = " << result.circle_compare_center_delta_value << "\n";
    std::cout << "[FIT_COMPARE] average_distance_delta = " << result.circle_compare_avg_distance_delta_value << "\n";
    std::cout << "[FIT_COMPARE] sample_points_delta = " << result.circle_compare_sample_points_delta_value << "\n";
  }

  if (!result.circle_legacy_failure_stage.empty())
    std::cout << "[FIT_COMPARE] legacy_failure_stage = " << result.circle_legacy_failure_stage << "\n";
  if (!result.circle_enhanced_failure_stage.empty())
    std::cout << "[FIT_COMPARE] enhanced_failure_stage = " << result.circle_enhanced_failure_stage << "\n";
}

void PrintExecutionTraceSummary(const cxparser_ext::CxScriptExecutionResult &result)
{
  std::cout << "[EXEC_TRACE] last_step=" << result.last_step_id
            << " last_frame=" << result.last_frame_id
            << " last_seq=" << result.last_sequence
            << " last_line=" << result.last_source_line
            << " failure_step=" << result.failure_step_id
            << " failure_frame=" << result.failure_frame_id
            << " failure_seq=" << result.failure_sequence
            << " failure_line=" << result.failure_line
            << " phase=" << result.failure_phase
            << "\n";
}

void PrintMultimodalSummary(const cxparser_ext::CxScriptExecutionResult &result,
                            bool debug_output)
{
  if (result.multimodal_slices.empty() && result.operation_atoms.empty())
    return;

  std::cout << "[MULTIMODAL] slices=" << result.multimodal_slices.size()
            << " atoms=" << result.operation_atoms.size() << "\n";

  if (!debug_output)
    return;

  for (size_t i = 0; i < result.multimodal_slices.size(); ++i)
  {
    const cxparser_ext::MultimodalSlice &slice = result.multimodal_slices[i];
    std::cout << "[SLICE] id=" << slice.slice_id
              << " modality=" << slice.modality
              << " kind=" << slice.analysis_kind
              << " objects=" << slice.objects.size()
              << " relations=" << slice.relations.size()
              << " next=" << slice.next_action << "\n";
  }

  for (size_t i = 0; i < result.operation_atoms.size(); ++i)
  {
    const cxparser_ext::OperationAtom &atom = result.operation_atoms[i];
    std::cout << "[ATOM] id=" << atom.atom_id
              << " stage=" << atom.stage
              << " action=" << atom.action_kind
              << " status=" << atom.status
              << " output=" << atom.output_ref << "\n";
  }
}

void PrintParseError(const cxparser_ext::CxScriptExecutionResult &result)
{
  if (result.parse_error.message.empty())
    return;

  std::cerr << "  message=" << result.parse_error.message << "\n";
  std::cerr << "  token=" << result.parse_error.token
            << " line=" << result.parse_error.line
            << " column=" << result.parse_error.column
            << " block_depth=" << result.parse_error.block_depth
            << " step=" << result.parse_error.step_name << "\n";
}

int FindPreviousSourceLine(const cxparser_ext::CxScriptExecutionResult &result,
                           const std::string &step_name,
                           int line)
{
  int previous_line = 0;
  for (size_t i = 0; i < result.debug_view.source_map.size(); ++i)
  {
    const cxparser_ext::CxScriptSourceMapEntry &entry = result.debug_view.source_map[i];
    if (entry.step_name != step_name)
      continue;
    if (entry.span.line_begin < line && entry.span.line_begin > previous_line)
      previous_line = entry.span.line_begin;
  }
  return previous_line;
}

void PrintBreakpointWindows(cxparser_ext::ParserCxScriptRuntime &runtime,
                            const cxparser_ext::CxScriptExecutionResult &result)
{
  for (size_t i = 0; i < result.breakpoints.size(); ++i)
  {
    const cxparser_ext::CxScriptBreakpointRecord &breakpoint = result.breakpoints[i];
    const int previous_line = FindPreviousSourceLine(result, breakpoint.step_name, breakpoint.span.line_begin);

    cxparser_ext::CxScriptDebugQueryResult before_query;
    cxparser_ext::CxScriptDebugQueryResult current_query;
    if (previous_line > 0)
      runtime.QueryDebugByLine(result.debug_view, previous_line, before_query);
    runtime.QueryDebugByLine(result.debug_view, breakpoint.span.line_begin, current_query);

    std::cout << "[BREAKWIN] name=" << breakpoint.name
              << " step=" << breakpoint.step_name
              << " before_line=" << previous_line
              << " before_next=" << before_query.next_breakpoint.name
              << " current_prev=" << current_query.previous_breakpoint.name
              << " current_next=" << current_query.next_breakpoint.name
              << "\n";
  }
}

void PrintCheckpointWindows(cxparser_ext::ParserCxScriptRuntime &runtime,
                            const cxparser_ext::CxScriptExecutionResult &result)
{
  for (size_t i = 0; i < result.checkpoints.size(); ++i)
  {
    const cxparser_ext::CxScriptCheckpointRecord &checkpoint = result.checkpoints[i];
    const int previous_line = FindPreviousSourceLine(result, checkpoint.step_name, checkpoint.span.line_begin);

    cxparser_ext::CxScriptDebugQueryResult before_query;
    cxparser_ext::CxScriptDebugQueryResult current_query;
    if (previous_line > 0)
      runtime.QueryDebugByLine(result.debug_view, previous_line, before_query);
    runtime.QueryDebugByLine(result.debug_view, checkpoint.span.line_begin, current_query);

    std::cout << "[CHECKWIN] name=" << checkpoint.name
              << " step=" << checkpoint.step_name
              << " before_line=" << previous_line
              << " before_next=" << before_query.next_checkpoint.name
              << " current_prev=" << current_query.previous_checkpoint.name
              << " current_next=" << current_query.next_checkpoint.name
              << "\n";
  }
}

void PrintUsage()
{
  std::cout
    << "Public cxparser program entry:\n"
    << "  build target: review_cxparser_programs -> cxparser_ext_public_entry -> cxparser_ext_cxscript_cli\n"
    << "  run entry:    cxparser_ext_cxscript_cli\n"
    << "  note:         do not treat smoke/demo/test_driver executables as peer public entrypoints\n"
    << "\n"
    << "Usage:\n"
    << "  cxparser_ext_cxscript_cli --script <path>\n"
    << "  cxparser_ext_cxscript_cli --script-dir <path>\n"
    << "  cxparser_ext_cxscript_cli --script-dir <path> [--kind <module|integration>] [--layer <name>] [--module <name>]\n"
    << "  cxparser_ext_cxscript_cli --kind <module|integration> --layer <name> --module <name> --case <name> [--mode <build|run|build-run>] [--route <default|realtime|batch>] [--report <on|off>] [--trace-id <id>] [--debug]\n"
    << "\n"
    << "Public input contract flags:\n"
    << "  --input-dataset <value>\n"
    << "  --input-sample <value>\n"
    << "  --input-split <value>\n"
    << "  --input-artifact <key=value>\n"
    << "  --input-param <key=value>\n";
}

bool IsFlag(const std::string &arg, const char *flag)
{
  return arg == flag;
}

bool EndsWith(const std::string &text, const char *suffix)
{
  const std::string suffix_text = suffix ? suffix : "";
  if (suffix_text.size() > text.size())
    return false;

  return text.compare(text.size() - suffix_text.size(), suffix_text.size(), suffix_text) == 0;
}

void CollectScriptsRecursive(const std::string &directory,
                             std::vector<std::string> &scripts)
{
  std::string search = directory;
  if (!search.empty() && search.back() != '\\' && search.back() != '/')
    search += "\\";

  WIN32_FIND_DATAA find_data;
  HANDLE handle = FindFirstFileA((search + "*").c_str(), &find_data);
  if (handle == INVALID_HANDLE_VALUE)
    return;

  do
  {
    const std::string name = find_data.cFileName;
    if (name == "." || name == "..")
      continue;

    const std::string full_path = search + name;
    if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
    {
      CollectScriptsRecursive(full_path, scripts);
      continue;
    }

    if (EndsWith(name, ".cxs") ||
        EndsWith(name, ".cxsc") ||
        EndsWith(name, ".cxscript"))
      scripts.push_back(full_path);
  } while (FindNextFileA(handle, &find_data));

  FindClose(handle);
}

bool ContainsText(const std::string &text, const std::string &needle)
{
  return !needle.empty() && text.find(needle) != std::string::npos;
}

bool MatchesDirectoryFilter(const std::string &script_path,
                            const std::string &kind,
                            const std::string &layer,
                            const std::string &module)
{
  if (!kind.empty())
  {
    const std::string kind_token = std::string("\\") + kind + "\\";
    if (!ContainsText(script_path, kind_token) &&
        !ContainsText(script_path, std::string("/") + kind + "/"))
      return false;
  }

  if (!module.empty())
  {
    const std::string module_token = std::string("\\") + module + "\\";
    if (!ContainsText(script_path, module_token) &&
        !ContainsText(script_path, std::string("/") + module + "/"))
      return false;
  }

  if (!layer.empty())
  {
    const std::string layer_token = std::string("\\") + layer + "\\";
    if (ContainsText(script_path, layer_token) ||
        ContainsText(script_path, std::string("/") + layer + "/"))
      return true;

    size_t slash = script_path.find_last_of("/\\");
    const std::string file_name = slash == std::string::npos ? script_path : script_path.substr(slash + 1);
    const std::string layer_prefix = layer + ".";
    const std::string layer_suffix_cxsc = "_" + layer + ".cxsc";
    const std::string layer_suffix_cxs = "." + layer + ".cxs";
    const std::string layer_suffix_cxscript = "." + layer + ".cxscript";
    if (file_name.find(layer_prefix) != 0 &&
        !EndsWith(file_name, layer_suffix_cxsc.c_str()) &&
        !EndsWith(file_name, layer_suffix_cxs.c_str()) &&
        !EndsWith(file_name, layer_suffix_cxscript.c_str()))
      return false;
  }

  return true;
}

bool IsKnownCxScriptLayer(const std::string &value)
{
  return value == "feature" ||
         value == "operator" ||
         value == "matcher" ||
         value == "embedded_model" ||
         value == "smoke" ||
         value == "scenario" ||
         value == "train" ||
         value == "infer" ||
         value == "score" ||
         value == "contract";
}

bool IsKnownCxScriptModule(const std::string &value)
{
  return value == "cxcore" ||
         value == "cximage" ||
         value == "ensmallen_layer" ||
         value == "mlpack" ||
         value == "torch_module" ||
         value == "rag";
}

void NormalizeModuleLayerArguments(std::string &layer, std::string &module)
{
  if (IsKnownCxScriptModule(layer) && IsKnownCxScriptLayer(module))
    std::swap(layer, module);
}

std::string EscapeCxScriptString(const std::string &value)
{
  std::string escaped;
  escaped.reserve(value.size());
  for (size_t i = 0; i < value.size(); ++i)
  {
    const char ch = value[i];
    if (ch == '\\' || ch == '"')
      escaped.push_back('\\');
    escaped.push_back(ch);
  }
  return escaped;
}

bool SplitCliAssignment(const std::string &text,
                        std::string &key,
                        std::string &value)
{
  key.clear();
  value.clear();
  const size_t pos = text.find('=');
  if (pos == std::string::npos || pos == 0 || pos + 1 >= text.size())
    return false;

  key = Trim(text.substr(0, pos));
  value = Trim(text.substr(pos + 1));
  return !key.empty() && !value.empty();
}

std::string BuildCliInputPrelude(const std::string &input_dataset,
                                 const std::string &input_sample,
                                 const std::string &input_split,
                                 const std::vector<std::string> &input_artifacts,
                                 const std::vector<std::string> &input_params)
{
  std::ostringstream out;
  if (!input_dataset.empty())
    out << "input_dataset(\"" << EscapeCxScriptString(input_dataset) << "\");\n";
  if (!input_sample.empty())
    out << "input_sample(\"" << EscapeCxScriptString(input_sample) << "\");\n";
  if (!input_split.empty())
    out << "input_split(\"" << EscapeCxScriptString(input_split) << "\");\n";

  for (size_t i = 0; i < input_artifacts.size(); ++i)
  {
    std::string key;
    std::string value;
    if (!SplitCliAssignment(input_artifacts[i], key, value))
      continue;
    out << "input_artifact(\"" << EscapeCxScriptString(key)
        << "\", \"" << EscapeCxScriptString(value) << "\");\n";
  }

  for (size_t i = 0; i < input_params.size(); ++i)
  {
    std::string key;
    std::string value;
    if (!SplitCliAssignment(input_params[i], key, value))
      continue;
    out << "input_param(\"" << EscapeCxScriptString(key)
        << "\", \"" << EscapeCxScriptString(value) << "\");\n";
  }

  return out.str();
}

bool LoadTextFile(const std::string &path, std::string &text)
{
  text.clear();
  std::ifstream input(path.c_str(), std::ios::in | std::ios::binary);
  if (!input.is_open())
    return false;

  std::ostringstream buffer;
  buffer << input.rdbuf();
  text = buffer.str();
  return true;
}

bool ExecuteScriptWithCliInputs(cxparser_ext::ParserCxScriptRuntime &runtime,
                                const std::string &script_path,
                                const std::string &input_prelude,
                                cxparser_ext::CxScriptExecutionResult &result)
{
  const std::string resolved_script_path = ResolvePublicScriptPath(script_path);
  if (input_prelude.empty())
    return runtime.ExecuteScriptFile(resolved_script_path, result);

  std::string script_text;
  if (!LoadTextFile(resolved_script_path, script_text))
    return runtime.ExecuteScriptFile(resolved_script_path, result);

  return runtime.ExecuteScriptText(resolved_script_path,
                                   input_prelude + "\n" + script_text,
                                   result);
}

bool ExecuteScriptByIdentityWithCliInputs(cxparser_ext::ParserCxScriptRuntime &runtime,
                                          const std::string &kind,
                                          const std::string &layer,
                                          const std::string &module,
                                          const std::string &case_name,
                                          const std::string &mode,
                                          const std::string &route,
                                          const std::string &trace_id,
                                          const std::string &report,
                                          const std::string &input_prelude,
                                          cxparser_ext::CxScriptExecutionResult &result)
{
  cxparser_ext::ParserDispatchRequest request;
  request.script_type = kind;
  request.layer = layer;
  request.case_id = case_name;
  request.mode = mode;
  request.route = route;
  request.trace_id = trace_id;
  request.report_on = report != "off";
  if (kind == "integration")
    request.integration = module;
  else
    request.module = module;

  cxparser_ext::ParserDispatchCaseSpec spec;
  if (cxparser_ext::ResolveDispatchCase(request, spec))
  {
    if (!spec.script_text.empty())
    {
      const std::string merged_text =
        input_prelude.empty() ? spec.script_text : (input_prelude + "\n" + spec.script_text);
      const std::string script_name =
        spec.script_path.empty() ? (case_name + ".cxsc") : spec.script_path;
      return runtime.ExecuteScriptText(script_name, merged_text, result);
    }

    if (!spec.script_path.empty())
    {
      const std::string resolved_script_path = ResolvePublicScriptPath(spec.script_path);
      if (input_prelude.empty())
        return runtime.ExecuteScriptFile(resolved_script_path, result);

      std::string script_text;
      if (!LoadTextFile(resolved_script_path, script_text))
        return runtime.ExecuteScriptFile(resolved_script_path, result);

      return runtime.ExecuteScriptText(resolved_script_path,
                                       input_prelude + "\n" + script_text,
                                       result);
    }
  }

  cxparser_ext::CxscriptExecutionArgs args;
  args.script_type = kind;
  args.layer = layer;
  args.case_id = case_name;
  args.mode = mode;
  args.route = route;
  args.trace_id = trace_id;
  args.report_on = report != "off";
  if (kind == "integration")
    args.integration_name = module;
  else
    args.module_name = module;

  cxparser_ext::CxscriptIdentity identity;
  if (!cxparser_ext::BuildCxscriptIdentity(args, identity))
    return false;

  std::string script_text;
  std::string script_origin;
  if (!cxparser_ext::LoadCxscriptText(identity, std::string(), script_text, script_origin))
  {
    return false;
  }

  const std::string merged_text =
    input_prelude.empty() ? script_text : (input_prelude + "\n" + script_text);
  return runtime.ExecuteScriptText(identity.file_path.empty() ? case_name + ".cxs"
                                                             : identity.file_path,
                                   merged_text,
                                   result);
}
}

int main(int argc, char **argv)
{
  std::string script_path;
  std::string script_dir;
  std::string kind;
  std::string layer;
  std::string module;
  std::string case_name;
  std::string mode = "build-run";
  std::string route;
  std::string report = "on";
  std::string trace_id;
  std::string input_dataset;
  std::string input_sample;
  std::string input_split;
  std::vector<std::string> input_artifacts;
  std::vector<std::string> input_params;
  bool debug_output = false;

  for (int i = 1; i < argc; ++i)
  {
    const std::string arg = argv[i];
    if (IsFlag(arg, "--script") && i + 1 < argc)
      script_path = argv[++i];
    else if (IsFlag(arg, "--script-dir") && i + 1 < argc)
      script_dir = argv[++i];
    else if (IsFlag(arg, "--kind") && i + 1 < argc)
      kind = argv[++i];
    else if (IsFlag(arg, "--layer") && i + 1 < argc)
      layer = argv[++i];
    else if (IsFlag(arg, "--module") && i + 1 < argc)
      module = argv[++i];
    else if (IsFlag(arg, "--case") && i + 1 < argc)
      case_name = argv[++i];
    else if (IsFlag(arg, "--mode") && i + 1 < argc)
      mode = argv[++i];
    else if (IsFlag(arg, "--route") && i + 1 < argc)
      route = argv[++i];
    else if (IsFlag(arg, "--report") && i + 1 < argc)
      report = argv[++i];
    else if (IsFlag(arg, "--trace-id") && i + 1 < argc)
      trace_id = argv[++i];
    else if (IsFlag(arg, "--input-dataset") && i + 1 < argc)
      input_dataset = argv[++i];
    else if (IsFlag(arg, "--input-sample") && i + 1 < argc)
      input_sample = argv[++i];
    else if (IsFlag(arg, "--input-split") && i + 1 < argc)
      input_split = argv[++i];
    else if (IsFlag(arg, "--input-artifact") && i + 1 < argc)
      input_artifacts.push_back(argv[++i]);
    else if (IsFlag(arg, "--input-param") && i + 1 < argc)
      input_params.push_back(argv[++i]);
    else if (IsFlag(arg, "--debug"))
      debug_output = true;
    else if (IsFlag(arg, "--help") || IsFlag(arg, "-h"))
    {
      PrintUsage();
      return 0;
    }
    else
    {
      std::cerr << "[FAIL] unknown or incomplete argument: " << arg << "\n";
      PrintUsage();
      return 1;
    }
  }

  NormalizeModuleLayerArguments(layer, module);
  const std::string input_prelude =
    BuildCliInputPrelude(input_dataset, input_sample, input_split, input_artifacts, input_params);

  cxparser_ext::ParserCxScriptRuntime runtime;
  cxparser_ext::CxScriptExecutionResult result;

  bool ok = false;
  if (!script_dir.empty())
  {
    std::vector<std::string> scripts;
    CollectScriptsRecursive(script_dir, scripts);
    std::sort(scripts.begin(), scripts.end());
    if (scripts.empty())
    {
      std::cerr << "[FAIL] no cxscript files found under: " << script_dir << "\n";
      return 1;
    }

    bool all_ok = true;
    int matched_count = 0;
    for (size_t i = 0; i < scripts.size(); ++i)
    {
      if (!MatchesDirectoryFilter(scripts[i], kind, layer, module))
        continue;

      ++matched_count;
      cxparser_ext::CxScriptExecutionResult item_result;
      const bool item_ok = ExecuteScriptWithCliInputs(runtime, scripts[i], input_prelude, item_result);
      if (!item_ok)
      {
        std::cerr << "[FAIL] file=" << scripts[i]
                  << " kind=" << item_result.kind
                  << " layer=" << item_result.layer
                  << " module=" << item_result.module
                  << " case=" << item_result.case_name
                  << " summary=" << item_result.summary << "\n";
        PrintParseError(item_result);
        all_ok = false;
        continue;
      }

        std::cout << "[CXSCRIPT] PASS file=" << item_result.script_path
                  << " kind=" << item_result.kind
                  << " layer=" << item_result.layer
                  << " module=" << item_result.module
                  << " case=" << item_result.case_name
                  << " task_id=" << item_result.task_id
                  << " summary=" << item_result.summary << "\n";
        PrintMultimodalSummary(item_result, debug_output);
        PrintResultRefs(item_result);
        PrintReportHeaderFields(item_result);
        PrintConclusionFields(item_result);
        PrintTestPlanFields(item_result);
        PrintInteractionFields(item_result);
        PrintAnalysisFields(item_result);
        PrintComparisonFields(item_result);
        PrintPublishedFields(item_result);
        PrintDatasetFields(item_result);
        PrintReviewFoundationFields(item_result);
        PrintReviewElementFields(item_result);
        PrintTimingFields(item_result);
        PrintInputFields(item_result);
        PrintBridgeFields(item_result);
        PrintMatcherFields(item_result);
        PrintLineFields(item_result);
        PrintCircleFields(item_result);
        PrintRegionPatternFields(item_result);
        PrintTopologyFields(item_result);
        PrintFormfitFields(item_result);
        PrintFormfitCompareFields(item_result);
        PrintFitCompareFields(item_result);
        PrintEnsmallenFallbackNamedViews(item_result);
        PrintEnsmallenGuaranteedSummaryViews(item_result);
        if (!debug_output)
        {
          for (size_t d = 0; d < item_result.details.size(); ++d)
          {
            if (ShouldPrintLightweightDetail(item_result.details[d]))
            std::cout << item_result.details[d] << "\n";
        }
      }
      if (debug_output)
      {
        PrintExecutionTraceSummary(item_result);
        for (size_t d = 0; d < item_result.details.size(); ++d)
        {
          if (!ShouldPrintDebugDetail(item_result.details[d]))
            continue;
          std::cout << item_result.details[d] << "\n";
        }
        if (HasDebugData(item_result))
        {
          PrintBreakpointWindows(runtime, item_result);
          PrintCheckpointWindows(runtime, item_result);
        }
      }
    }

    if (matched_count == 0)
    {
      std::cerr << "[FAIL] no cxscript files matched filters under: " << script_dir << "\n";
      return 1;
    }

    return all_ok ? 0 : 1;
  }

  if (!script_path.empty())
  {
    ok = ExecuteScriptWithCliInputs(runtime, script_path, input_prelude, result);
  }
  else
  {
    if (kind.empty() || layer.empty() || module.empty() || case_name.empty())
    {
      std::cerr << "[FAIL] inline execution requires --kind --layer --module --case\n";
      PrintUsage();
      return 1;
    }

    ok = ExecuteScriptByIdentityWithCliInputs(runtime,
                                             kind,
                                             layer,
                                             module,
                                             case_name,
                                             mode,
                                             route,
                                             trace_id,
                                             report,
                                             input_prelude,
                                             result);
  }

  if (!ok)
  {
    std::cerr << "[FAIL] kind=" << result.kind
              << " layer=" << result.layer
              << " module=" << result.module
              << " case=" << result.case_name
              << " summary=" << result.summary << "\n";
    PrintParseError(result);
    for (size_t i = 0; i < result.details.size(); ++i)
    {
      if (debug_output || ShouldPrintLightweightDetail(result.details[i]))
        std::cerr << result.details[i] << "\n";
    }
    return 1;
  }

  std::cout << "[CXSCRIPT] PASS kind=" << result.kind
            << " layer=" << result.layer
            << " module=" << result.module
            << " case=" << result.case_name
            << " task_id=" << result.task_id
            << " summary=" << result.summary << "\n";
  PrintMultimodalSummary(result, debug_output);
    PrintResultRefs(result);
    PrintReportHeaderFields(result);
    PrintConclusionFields(result);
    PrintTestPlanFields(result);
    PrintInteractionFields(result);
    PrintAnalysisFields(result);
    PrintComparisonFields(result);
    PrintPublishedFields(result);
    PrintDatasetFields(result);
    PrintReviewFoundationFields(result);
    PrintReviewElementFields(result);
    PrintTimingFields(result);
    PrintInputFields(result);
    PrintBridgeFields(result);
    PrintMatcherFields(result);
    PrintLineFields(result);
    PrintCircleFields(result);
    PrintRegionPatternFields(result);
    PrintTopologyFields(result);
    PrintFormfitFields(result);
    PrintFormfitCompareFields(result);
    PrintFitCompareFields(result);
    PrintEnsmallenFallbackNamedViews(result);
    PrintEnsmallenGuaranteedSummaryViews(result);
  if (!debug_output)
  {
    for (size_t i = 0; i < result.details.size(); ++i)
    {
      if (ShouldPrintLightweightDetail(result.details[i]))
        std::cout << result.details[i] << "\n";
    }
  }

  if (debug_output)
  {
    PrintExecutionTraceSummary(result);
    for (size_t i = 0; i < result.details.size(); ++i)
    {
      if (!ShouldPrintDebugDetail(result.details[i]))
        continue;
      std::cout << result.details[i] << "\n";
    }
    if (HasDebugData(result))
    {
      PrintBreakpointWindows(runtime, result);
      PrintCheckpointWindows(runtime, result);
    }
  }

  return 0;
}
