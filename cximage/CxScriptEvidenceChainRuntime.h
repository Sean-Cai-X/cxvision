#pragma once

#include <string>
#include <vector>

struct CxScriptEvidenceAnnotation
{
    std::string image_id;
    std::string shape_kind = "RectShape";
    std::string semantic_role = "bbox";
    std::string owner_binding = "label_bbox";
    std::string label = "anomaly";
    int class_id = -1;
    double x0 = 0.0;
    double y0 = 0.0;
    double x1 = 0.0;
    double y1 = 0.0;
    bool normalized = false;
};

struct CxScriptEvidenceDatasetImage
{
    std::string image_id;
    std::string image_path;
    std::string split = "train";
    std::string label = "unlabeled";
    std::string source = "evidence_dataset";
};

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
    std::vector<CxScriptEvidenceDatasetImage> dataset_images;
    std::vector<CxScriptEvidenceAnnotation> annotations;
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
