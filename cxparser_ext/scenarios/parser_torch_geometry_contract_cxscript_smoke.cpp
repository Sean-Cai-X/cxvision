#include <iostream>
#include <string>

#include "../runtime/cxscript_runtime.h"

namespace
{
struct ContractScriptCase
{
  const char *layer;
  const char *case_id;
  const char *must_contain_check;
  const char *must_contain_value;
};

bool ContainsText(const std::string &text, const char *pattern)
{
  return pattern != 0 && text.find(pattern) != std::string::npos;
}

bool MatchesRequestedCase(const std::string &requested_case, const ContractScriptCase &script_case)
{
  return requested_case.empty() || requested_case == script_case.case_id;
}

std::string BuildContractScriptPath(const ContractScriptCase &script_case)
{
  const std::string relative_path = std::string("cxscript/integration/torch_geometry/") +
                                    script_case.layer + "." + script_case.case_id + ".cxsc";
#ifdef CXPARSER_WORKSPACE_ROOT
  return std::string(CXPARSER_WORKSPACE_ROOT) + "/" + relative_path;
#else
  return relative_path;
#endif
}

bool RunCase(const ContractScriptCase &script_case)
{
  cxparser_ext::CxscriptExecutionArgs args;
  args.script_type = "integration";
  args.integration_name = "torch_geometry";
  args.layer = script_case.layer;
  args.case_id = script_case.case_id;
  args.script_path = BuildContractScriptPath(script_case);

  cxparser_ext::CxscriptIdentity identity;
  if (!cxparser_ext::BuildCxscriptIdentity(args, identity))
  {
    std::cerr << "[FAIL] identity build failed for torch_geometry."
              << script_case.layer << "." << script_case.case_id << "\n";
    return false;
  }

  std::string script_text;
  std::string script_origin;
  if (!cxparser_ext::LoadCxscriptText(identity, "", script_text, script_origin))
  {
    std::cerr << "[FAIL] script load failed: " << identity.file_path << "\n";
    return false;
  }

  if (script_origin != "file")
  {
    std::cerr << "[FAIL] contract script origin should be file: "
              << identity.file_path << "\n";
    return false;
  }

  cxparser_ext::CxscriptExecutionResult result;
  cxparser_ext::BuildCxscriptStructureSummary(script_text, result);

  if (!result.ir_valid)
  {
    std::cerr << "[FAIL] contract summary invalid: " << identity.file_path
              << " error=" << result.ir_error_message << "\n";
    return false;
  }

  if (result.flow_profile.script_style != "call_style" ||
      !result.layer_profile.has_compile_bridge ||
      !result.layer_profile.bridge_exec_safe ||
      result.layer_profile.bridge_exec_subset != "numeric_stmt" ||
      !result.basic_semantics.has_assignment ||
      !result.basic_semantics.has_call_stmt ||
      result.binding_semantics.binding_scope != "native_only")
  {
    std::cerr << "[FAIL] contract bridge classification mismatch: "
              << identity.file_path << "\n";
    return false;
  }

  if (!ContainsText(script_text, script_case.must_contain_check) ||
      !ContainsText(script_text, script_case.must_contain_value))
  {
    std::cerr << "[FAIL] contract check marker missing: "
              << identity.file_path << "\n";
    return false;
  }

  std::cout << "[PASS] file=" << identity.file_path
            << " style=" << result.flow_profile.script_style
            << " safe=" << result.layer_profile.bridge_exec_safe
            << " subset=" << result.layer_profile.bridge_exec_subset
            << " stmt=" << result.ir_stmt_count
            << " check=" << script_case.must_contain_check
            << "\n";
  return true;
}
}

int main(int argc, char **argv)
{
  const std::string requested_case = argc > 1 ? argv[1] : "";

  const ContractScriptCase cases[] = {
    {"feature", "input_prior_contract",
     "input_contract_ok=geom_check_input_ready(",
     "input_contract_ok;"},
    {"feature", "label_align_contract",
     "label_contract_ok=geom_check_label_ready(",
     "label_contract_ok;"},
    {"feature", "attach_back_contract",
     "attach_contract_ok=geom_check_attach_ready(",
     "attach_contract_ok;"},
    {"infer", "replay_contract",
     "replay_contract_ok=geom_check_replay_ready(",
     "replay_contract_ok;"}
  };

  bool matched_case = requested_case.empty();
  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
  {
    if (!MatchesRequestedCase(requested_case, cases[i]))
      continue;
    matched_case = true;
    if (!RunCase(cases[i]))
      return 1;
  }

  if (!matched_case)
  {
    std::cerr << "[FAIL] unknown torch_geometry contract case: " << requested_case << "\n";
    return 1;
  }

  std::cout << "[PASS] parser_torch_geometry_contract_cxscript_smoke\n";
  return 0;
}