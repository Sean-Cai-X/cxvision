#include <iostream>
#include <string>

#include "../pipeline/parser_cxscript_runtime.h"

namespace
{
bool HasResultField(const cxparser_ext::CxScriptExecutionResult &result,
                    const std::string &result_name,
                    const std::string &field_name)
{
  for (size_t i = 0; i < result.result_fields.size(); ++i)
  {
    if (result.result_fields[i].result_name == result_name &&
        result.result_fields[i].field_name == field_name &&
        !result.result_fields[i].value.empty())
    {
      return true;
    }
  }
  return false;
}

bool HasFeatureObjectReadBridge(const cxparser_ext::CxScriptExecutionResult &result)
{
  return HasResultField(result, "candidates", "count") &&
         HasResultField(result, "method", "selected") &&
         HasResultField(result, "config", "name") &&
         HasResultField(result, "score", "total");
}

bool RunScriptFile(cxparser_ext::ParserCxScriptRuntime &runtime,
                   const std::string &script_path,
                   bool require_object_read_bridge = false)
{
  cxparser_ext::CxScriptExecutionResult result;
  if (!runtime.ExecuteScriptFile(script_path, result))
  {
    std::cerr << "[FAIL] " << script_path
              << " summary=" << result.summary << "\n";
    return false;
  }

  if (!result.success)
  {
    std::cerr << "[FAIL] script not marked success: " << script_path
              << " summary=" << result.summary << "\n";
    return false;
  }

  if (result.task_id.empty())
  {
    std::cerr << "[FAIL] script missing task id: " << script_path << "\n";
    return false;
  }

  if (require_object_read_bridge && !HasFeatureObjectReadBridge(result))
  {
    std::cerr << "[FAIL] script missing feature object read bridge fields: "
              << script_path << "\n";
    return false;
  }

  std::cout << "[PASS] file=" << result.script_path
            << " case=" << result.case_name
            << " task_id=" << result.task_id
            << " summary=" << result.summary
            << "\n";
  return true;
}
}

int main()
{
  cxparser_ext::ParserCxScriptRuntime runtime;
  const std::string workspace_root = CXPARSER_WORKSPACE_ROOT;
  const std::string feature_root =
    workspace_root + "\\cxparser\\rag_script_cases\\cxcore\\feature\\";

  // Keep this runtime smoke on execution-compatible cstyle scripts.
  // Legacy mixed-body combo/suite scripts still contain old Check.* / Flow.* forms
  // that are assessed separately and should not gate the current execution chain.
  if (!RunScriptFile(runtime, feature_root + "cxcore_line_measurement_feature.cxsc") ||
      !RunScriptFile(runtime, feature_root + "cxcore_template_feature_match_feature.cxsc", true) ||
      !RunScriptFile(runtime, feature_root + "cxcore_circle_measurement_cstyle_feature.cxsc") ||
      !RunScriptFile(runtime, feature_root + "cxcore_circle_measurement_boundary_real_numeric_cstyle_feature.cxsc") ||
      !RunScriptFile(runtime, feature_root + "cxcore_region_boundary_analysis_feature.cxsc") ||
      !RunScriptFile(runtime, feature_root + "cxcore_line_measurement_golden_cstyle_feature.cxsc") ||
      !RunScriptFile(runtime, feature_root + "cxcore_region_boundary_analysis_golden_cstyle_feature.cxsc") ||
      !RunScriptFile(runtime, feature_root + "cxcore_region_boundary_analysis_noise_cstyle_feature.cxsc"))
  {
    return 1;
  }

  std::cout << "[PASS] parser_cxcore_feature_cxscript_smoke\n";
  return 0;
}