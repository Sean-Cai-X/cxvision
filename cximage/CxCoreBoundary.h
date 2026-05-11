#ifndef CXCORE_CORE_CXCOREBOUNDARY_H
#define CXCORE_CORE_CXCOREBOUNDARY_H

#include <string>
#include <vector>

class Image;
class PointsShape;
class Findline;
class Findcircle;
class Findellipse;
class FindObject;
class fastmatch;

namespace cxcore {

// Stable output contracts for the classical analysis layer.
// These structs intentionally stop at direct measurements so higher layers
// can own optimization, fusion, and model-based reasoning.
struct OutputPoint
{
    double x = 0.0;
    double y = 0.0;
};

struct OutputRect
{
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
};

struct PointSetOutput
{
    std::vector<OutputPoint> points;
    OutputRect bounds;
};

struct ImageComponentOutput
{
    OutputRect bounds;
    OutputPoint centroid;
    int area = 0;
};

struct ImageAnalysisOutput
{
    int width = 0;
    int height = 0;
    int image_type = 0;
    std::vector<ImageComponentOutput> connected_components;
};

struct LineMeasurementOutput
{
    PointSetOutput horizontal_samples;
    PointSetOutput vertical_samples;
    OutputRect measure_bounds;
};

struct CircleMeasurementOutput
{
    PointSetOutput sample_points;
    OutputRect measure_bounds;
    OutputPoint center;
    double radius = 0.0;
    double average_distance = 0.0;
    bool has_direct_fit = false;
};

struct EllipseMeasurementOutput
{
    PointSetOutput sample_points;
    OutputRect measure_bounds;
};

struct DetectionOutput
{
    std::vector<OutputRect> boxes;
};

struct MatchCandidateOutput
{
    OutputRect bounds;
    OutputPoint center;
    double score = 0.0;
};

struct MatchOutput
{
    std::vector<MatchCandidateOutput> candidates;
    double max_score = 0.0;
    double image_model_score = 0.0;
};

// Baseline validation layer for cxcore structured outputs.
// The schema is intentionally flat so downstream evaluators can emit
// deterministic CSV rows and compare classical baselines directly.
struct BaselineFeatureSampleV1
{
    std::string sample_id;
    std::string source_image_id;
    std::string roi_id;
    std::string label;
    std::string split;

    double roi_x = 0.0;
    double roi_y = 0.0;
    double roi_w = 0.0;
    double roi_h = 0.0;
    double roi_area = 0.0;
    double roi_aspect_ratio = 0.0;

    double image_width = 0.0;
    double image_height = 0.0;
    double image_type = 0.0;

    double gray_mean = 0.0;
    double gray_std = 0.0;
    double gray_min = 0.0;
    double gray_max = 0.0;
    double edge_pixel_ratio = 0.0;
    double binary_foreground_ratio = 0.0;

    double component_count = 0.0;
    double largest_component_area = 0.0;
    double largest_component_ratio = 0.0;
    double largest_bbox_x = 0.0;
    double largest_bbox_y = 0.0;
    double largest_bbox_w = 0.0;
    double largest_bbox_h = 0.0;
    double largest_bbox_aspect_ratio = 0.0;
    double largest_centroid_x = 0.0;
    double largest_centroid_y = 0.0;

    double line_w_points_count = 0.0;
    double line_h_points_count = 0.0;
    double line_measure_bbox_x = 0.0;
    double line_measure_bbox_y = 0.0;
    double line_measure_bbox_w = 0.0;
    double line_measure_bbox_h = 0.0;

    double circle_points_count = 0.0;
    double circle_center_x = 0.0;
    double circle_center_y = 0.0;
    double circle_radius = 0.0;
    double circle_avg_dist = 0.0;
    double circle_measure_bbox_x = 0.0;
    double circle_measure_bbox_y = 0.0;
    double circle_measure_bbox_w = 0.0;
    double circle_measure_bbox_h = 0.0;
    double circle_fit_valid = 0.0;

    double match_candidate_count = 0.0;
    double match_best_score = 0.0;
    double match_best_center_x = 0.0;
    double match_best_center_y = 0.0;
    double match_best_rect_x = 0.0;
    double match_best_rect_y = 0.0;
    double match_best_rect_w = 0.0;
    double match_best_rect_h = 0.0;
    double image_model_score = 0.0;
};

enum class BaselineFeatureSetV1
{
    CoreGeom,
    MatchOnly,
    AllV1
};

struct BaselineSummaryRecordV1
{
    std::string task;
    std::string feature_set;
    std::string model;
    double accuracy = 0.0;
    double macro_f1 = 0.0;
    double fit_time_ms = 0.0;
    double infer_time_ms = 0.0;
    double feature_dim = 0.0;
    std::string notes;
};

PointSetOutput ExportPointSet(PointsShape& points);
ImageAnalysisOutput ExportImageAnalysis(const Image& image, double min_area, int min_width = 0, int min_height = 0);
LineMeasurementOutput ExportLineMeasurement(Findline& line);
CircleMeasurementOutput ExportCircleMeasurement(Findcircle& circle);
EllipseMeasurementOutput ExportEllipseMeasurement(Findellipse& ellipse);
DetectionOutput ExportDetections(FindObject& object);
MatchOutput ExportMatchOutput(fastmatch& matcher, int max_candidates);
BaselineFeatureSampleV1 ExportBaselineFeatureSampleV1(
    const Image& image,
    const ImageAnalysisOutput& analysis,
    const LineMeasurementOutput& line,
    const CircleMeasurementOutput& circle,
    const MatchOutput& match);
std::vector<std::string> GetBaselineMetadataNamesV1();
std::vector<std::string> GetBaselineFeatureNamesV1();
std::vector<std::string> ExportBaselineMetadataValuesV1(const BaselineFeatureSampleV1& sample);
std::vector<double> ExportBaselineFeatureValuesV1(const BaselineFeatureSampleV1& sample);
std::string ExportBaselineCsvHeaderV1();
std::string ExportBaselineCsvRowV1(const BaselineFeatureSampleV1& sample);
const char* BaselineFeatureSetNameV1(BaselineFeatureSetV1 feature_set);
std::vector<std::string> GetBaselineFeatureNamesV1(BaselineFeatureSetV1 feature_set);
std::vector<double> ExportBaselineFeatureValuesV1(
    const BaselineFeatureSampleV1& sample,
    BaselineFeatureSetV1 feature_set);
std::string ExportBaselineCsvHeaderV1(BaselineFeatureSetV1 feature_set);
std::string ExportBaselineCsvRowV1(
    const BaselineFeatureSampleV1& sample,
    BaselineFeatureSetV1 feature_set);
std::vector<std::string> GetBaselineSummaryNamesV1();
std::vector<std::string> ExportBaselineSummaryValuesV1(const BaselineSummaryRecordV1& record);
std::string ExportBaselineSummaryCsvHeaderV1();
std::string ExportBaselineSummaryCsvRowV1(const BaselineSummaryRecordV1& record);

} // namespace cxcore

#endif
