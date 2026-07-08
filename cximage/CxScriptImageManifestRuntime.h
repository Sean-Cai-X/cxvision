#pragma once

#include <string>
#include <vector>

struct CxScriptImageTargetRoi
{
    std::string target_id;
    std::string tool;
    std::string orientation;
    std::string roi_type;

    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;

    int cx = 0;
    int cy = 0;
    int px = 0;
    int py = 0;

    int wgap = 32;
    int hgap = 8;
    int gap = 5;
    int linegap = 6;

    std::string expected_edge;
    std::string edge_polarity_hint;
    std::string comment;
};

struct CxScriptImageManifestEntry
{
    std::string image_id;
    std::string level;
    std::string path;
    std::string source_path;

    int width = 0;
    int height = 0;
    int channels = 0;

    bool raw_not_cropped = false;
    bool raw_not_enhanced = false;
    bool raw_not_rotated = false;

    std::string sha256;

    std::vector<CxScriptImageTargetRoi> targets;
};

struct CxScriptImageManifestRuntime
{
    std::string manifest_path;
    std::string image_root;
    std::string schema_version;
    std::string purpose;
    std::string selection_policy;

    int total_images = 0;
    int l0_count = 0;
    int l1_count = 0;
    int l2_count = 0;
    int l3_count = 0;

    std::vector<CxScriptImageManifestEntry> images;
};

struct CxScriptImageManifestValidationIssue
{
    std::string severity;
    std::string image_id;
    std::string target_id;
    std::string message;
};

struct CxScriptImageManifestValidationResult
{
    bool ok = true;
    std::vector<CxScriptImageManifestValidationIssue> issues;
};

bool LoadStage25ImageManifestJson(
    const std::string& manifest_path,
    CxScriptImageManifestRuntime& out_manifest,
    std::string& out_reason);

CxScriptImageManifestValidationResult ValidateStage25ImageManifest(
    const CxScriptImageManifestRuntime& manifest);

const CxScriptImageManifestEntry* FindImageById(
    const CxScriptImageManifestRuntime& manifest,
    const std::string& image_id);