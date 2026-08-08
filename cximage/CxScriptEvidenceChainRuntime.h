#pragma once

#include <string>
#include <vector>

struct CxScriptEvidenceCase
{
    std::string evidence_id;
    std::string image_id;
    std::string target_id;
    std::string script_id;
    std::string parameter_profile_id;
    std::string contract_id;
    std::string expected_result;
    std::string expected_policy_guard;
    std::string tool;
    std::string level;
    std::string case_role;
    std::string source_case_id;
    std::string display_category;
    std::string display_group;
    bool manual_review_required = true;
    bool promotion_candidate = false;
};

struct CxScriptEvidenceChainRuntime
{
    std::string chain_id;
    std::string chain_name;
    std::string catalog_path;
    std::string image_manifest_path;
    std::string output_root;
    std::vector<CxScriptEvidenceCase> cases;

    const CxScriptEvidenceCase* FindCase(const std::string& evidence_id) const;
    void Clear();
};

bool LoadCxScriptEvidenceChainFile(
    const std::string& script_path,
    CxScriptEvidenceChainRuntime& out_chain,
    std::string& out_reason);
