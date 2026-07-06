#ifndef CX_SCRIPT_IMAGE_EVIDENCE_ANALYZER_H
#define CX_SCRIPT_IMAGE_EVIDENCE_ANALYZER_H

#include <vector>
#include <string>
#include <filesystem>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>

struct CxImageEvidenceOptions
{
    bool enabled = true;

    double nearest_point_support_px = 3.0;
    double line_distance_support_px = 3.0;

    double min_gradient = 6.0;
    double min_gradient_ratio = 0.35;

    int profile_half_width = 40;
    int gradient_sample_radius = 2;

    int max_profiles = 200;

    bool use_line_distance_for_findline_support = true;

    bool save_profile_debug = false;

    std::string profile_name = "normal";
};

struct CxPointEvidence
{
    double measured_x = 0.0;
    double measured_y = 0.0;

    double reference_x = 0.0;
    double reference_y = 0.0;

    double nearest_reference_distance_px = 0.0;
    double reference_line_distance_px = 0.0;

    double local_gradient = 0.0;
    double reference_gradient = 0.0;
    double gradient_ratio = 0.0;

    bool distance_supported = false;
    bool gradient_supported = false;
    bool combined_supported = false;

    std::string reference_polarity;
    std::string reason;
};

struct CxImageEvidenceSummary
{
    std::string tool;
    std::string object_name;

    bool reference_available = false;
    int reference_points_count = 0;

    int measured_points_count = 0;
    int supported_points_count = 0;
    int unsupported_points_count = 0;

    double mean_error_px = 0.0;
    double max_error_px = 0.0;
    double edge_support_score = 0.0;

    double distance_support_score = 0.0;
    double gradient_support_score = 0.0;
    double combined_edge_support_score = 0.0;

    double mean_reference_line_distance_px = 0.0;
    double max_reference_line_distance_px = 0.0;

    double mean_gradient_ratio = 0.0;

    int distance_supported_points = 0;
    int gradient_supported_points = 0;
    int combined_supported_points = 0;

    bool reference_fit_available = false;
    bool measured_fit_available = false;

    double fit_offset_error_px = 0.0;
    double fit_angle_error_deg = 0.0;

    double circle_center_error_px = 0.0;
    double circle_radius_error_px = 0.0;

    std::string best_reference_polarity;
    int positive_reference_points = 0;
    int negative_reference_points = 0;
    int abs_reference_points = 0;
    double mean_error_positive = 0.0;
    double mean_error_negative = 0.0;
    double mean_error_abs = 0.0;

    std::string metric_quality;
    std::string conclusion;
    std::string support_conclusion;

    std::vector<CxPointEvidence> point_evidences;
};

struct ManualTestContext;

bool AnalyzeCxScriptImageEvidence(
    const cv::Mat& image,
    const ManualTestContext& context,
    const CxImageEvidenceOptions& options,
    const std::filesystem::path& outputDir,
    std::string& outReason);

#endif