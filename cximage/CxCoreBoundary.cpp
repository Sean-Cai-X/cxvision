#include "CxCoreBoundary.h"

#include "FastMatch.h"
#include "Findcircle.h"
#include "Findellipse.h"
#include "Findline.h"
#include "Image.h"
#include "findobject.h"
#include "shapebase.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <sstream>

namespace cxcore {

namespace {

double QuietNaN()
{
    return std::numeric_limits<double>::quiet_NaN();
}

double SafeAspectRatio(double width, double height)
{
    if (height <= 0.0)
    {
        return QuietNaN();
    }
    return width / height;
}

double SafeRatio(double numerator, double denominator)
{
    if (denominator <= 0.0)
    {
        return QuietNaN();
    }
    return numerator / denominator;
}

std::string EscapeCsv(const std::string& value)
{
    const bool needs_quotes = value.find_first_of(",\"\n\r") != std::string::npos;
    if (!needs_quotes)
    {
        return value;
    }

    std::string escaped = "\"";
    for (const char ch : value)
    {
        if (ch == '"')
        {
            escaped += "\"\"";
        }
        else
        {
            escaped += ch;
        }
    }
    escaped += "\"";
    return escaped;
}

template<typename ValueType>
void AppendCsvValue(std::ostringstream& stream, const ValueType& value, bool& first)
{
    if (!first)
    {
        stream << ',';
    }
    stream << value;
    first = false;
}

void AppendCsvValue(std::ostringstream& stream, const std::string& value, bool& first)
{
    if (!first)
    {
        stream << ',';
    }
    stream << EscapeCsv(value);
    first = false;
}

std::vector<size_t> BaselineFeatureIndicesV1(BaselineFeatureSetV1 feature_set)
{
    switch (feature_set)
    {
    case BaselineFeatureSetV1::CoreGeom:
        return {
            0, 1, 2, 3, 4, 5,
            15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
            25, 26, 27, 28, 29, 30,
            31, 32, 33, 34, 35, 36, 37, 38, 39, 40
        };
    case BaselineFeatureSetV1::MatchOnly:
        return {
            41, 42, 43, 44, 45, 46, 47, 48, 49
        };
    case BaselineFeatureSetV1::AllV1:
        return {};
    }

    throw std::invalid_argument("Unsupported baseline feature set.");
}

template<typename ValueType>
std::vector<ValueType> SelectByIndices(
    const std::vector<ValueType>& values,
    const std::vector<size_t>& indices)
{
    if (indices.empty())
    {
        return values;
    }

    std::vector<ValueType> selected;
    selected.reserve(indices.size());
    for (const size_t index : indices)
    {
        if (index < values.size())
        {
            selected.push_back(values[index]);
        }
    }
    return selected;
}

cv::Rect MakeClampedRect(const Image& image)
{
    const int x = std::max(0, image.m_ix0);
    const int y = std::max(0, image.m_iy0);
    const int max_width = std::max(0, image.getWidth() - x);
    const int max_height = std::max(0, image.getHeight() - y);
    const int width = std::min(std::max(0, image.m_iw), max_width);
    const int height = std::min(std::max(0, image.m_ih), max_height);
    return cv::Rect(x, y, width, height);
}

cv::Mat MakeGrayRoi(const Image& image, const cv::Rect& roi)
{
    if (roi.width <= 0 || roi.height <= 0)
    {
        return cv::Mat();
    }

    const cv::Mat roi_image = const_cast<Image&>(image).getmat()(roi);
    cv::Mat gray;
    if (roi_image.channels() == 1)
    {
        gray = roi_image.clone();
    }
    else
    {
        cv::cvtColor(roi_image, gray, cv::COLOR_BGR2GRAY);
    }
    return gray;
}

void FillGrayStatistics(const cv::Mat& gray, BaselineFeatureSampleV1& sample)
{
    if (gray.empty())
    {
        sample.gray_mean = QuietNaN();
        sample.gray_std = QuietNaN();
        sample.gray_min = QuietNaN();
        sample.gray_max = QuietNaN();
        sample.edge_pixel_ratio = QuietNaN();
        sample.binary_foreground_ratio = QuietNaN();
        return;
    }

    cv::Scalar mean;
    cv::Scalar stddev;
    cv::meanStdDev(gray, mean, stddev);
    sample.gray_mean = mean[0];
    sample.gray_std = stddev[0];

    double min_value = 0.0;
    double max_value = 0.0;
    cv::minMaxLoc(gray, &min_value, &max_value);
    sample.gray_min = min_value;
    sample.gray_max = max_value;

    cv::Mat edges;
    cv::Canny(gray, edges, 50.0, 150.0);
    const double pixel_count = static_cast<double>(gray.rows * gray.cols);
    sample.edge_pixel_ratio = SafeRatio(static_cast<double>(cv::countNonZero(edges)), pixel_count);

    cv::Mat binary;
    cv::threshold(gray, binary, 0.0, 255.0, cv::THRESH_BINARY | cv::THRESH_OTSU);
    sample.binary_foreground_ratio = SafeRatio(static_cast<double>(cv::countNonZero(binary)), pixel_count);
}

void FillLargestComponent(const ImageAnalysisOutput& analysis, BaselineFeatureSampleV1& sample)
{
    sample.component_count = static_cast<double>(analysis.connected_components.size());
    if (analysis.connected_components.empty())
    {
        sample.largest_component_area = 0.0;
        sample.largest_component_ratio = 0.0;
        sample.largest_bbox_x = QuietNaN();
        sample.largest_bbox_y = QuietNaN();
        sample.largest_bbox_w = QuietNaN();
        sample.largest_bbox_h = QuietNaN();
        sample.largest_bbox_aspect_ratio = QuietNaN();
        sample.largest_centroid_x = QuietNaN();
        sample.largest_centroid_y = QuietNaN();
        return;
    }

    const auto largest = std::max_element(
        analysis.connected_components.begin(),
        analysis.connected_components.end(),
        [](const ImageComponentOutput& lhs, const ImageComponentOutput& rhs)
        {
            return lhs.area < rhs.area;
        });

    sample.largest_component_area = static_cast<double>(largest->area);
    sample.largest_component_ratio = SafeRatio(sample.largest_component_area, sample.roi_area);
    sample.largest_bbox_x = largest->bounds.x;
    sample.largest_bbox_y = largest->bounds.y;
    sample.largest_bbox_w = largest->bounds.width;
    sample.largest_bbox_h = largest->bounds.height;
    sample.largest_bbox_aspect_ratio = SafeAspectRatio(largest->bounds.width, largest->bounds.height);
    sample.largest_centroid_x = largest->centroid.x;
    sample.largest_centroid_y = largest->centroid.y;
}

void FillLineFields(const LineMeasurementOutput& line, BaselineFeatureSampleV1& sample)
{
    sample.line_w_points_count = static_cast<double>(line.horizontal_samples.points.size());
    sample.line_h_points_count = static_cast<double>(line.vertical_samples.points.size());
    sample.line_measure_bbox_x = line.measure_bounds.x;
    sample.line_measure_bbox_y = line.measure_bounds.y;
    sample.line_measure_bbox_w = line.measure_bounds.width;
    sample.line_measure_bbox_h = line.measure_bounds.height;
}

void FillCircleFields(const CircleMeasurementOutput& circle, BaselineFeatureSampleV1& sample)
{
    sample.circle_points_count = static_cast<double>(circle.sample_points.points.size());
    sample.circle_center_x = circle.center.x;
    sample.circle_center_y = circle.center.y;
    sample.circle_radius = circle.radius;
    sample.circle_avg_dist = circle.average_distance;
    sample.circle_measure_bbox_x = circle.measure_bounds.x;
    sample.circle_measure_bbox_y = circle.measure_bounds.y;
    sample.circle_measure_bbox_w = circle.measure_bounds.width;
    sample.circle_measure_bbox_h = circle.measure_bounds.height;
    sample.circle_fit_valid = circle.has_direct_fit ? 1.0 : 0.0;
}

void FillMatchFields(const MatchOutput& match, BaselineFeatureSampleV1& sample)
{
    sample.match_candidate_count = static_cast<double>(match.candidates.size());
    sample.image_model_score = match.image_model_score;

    if (match.candidates.empty())
    {
        sample.match_best_score = 0.0;
        sample.match_best_center_x = QuietNaN();
        sample.match_best_center_y = QuietNaN();
        sample.match_best_rect_x = QuietNaN();
        sample.match_best_rect_y = QuietNaN();
        sample.match_best_rect_w = QuietNaN();
        sample.match_best_rect_h = QuietNaN();
        return;
    }

    const auto best = std::max_element(
        match.candidates.begin(),
        match.candidates.end(),
        [](const MatchCandidateOutput& lhs, const MatchCandidateOutput& rhs)
        {
            return lhs.score < rhs.score;
        });

    sample.match_best_score = best->score;
    sample.match_best_center_x = best->center.x;
    sample.match_best_center_y = best->center.y;
    sample.match_best_rect_x = best->bounds.x;
    sample.match_best_rect_y = best->bounds.y;
    sample.match_best_rect_w = best->bounds.width;
    sample.match_best_rect_h = best->bounds.height;
}

OutputRect ToOutputRect(const gp_Rectangle& rect)
{
    const gp_Pnt top_left = rect.TopLeft();
    const gp_Pnt bottom_right = rect.BottomRight();
    const double min_x = std::min(top_left.X(), bottom_right.X());
    const double min_y = std::min(top_left.Y(), bottom_right.Y());
    const double width = std::fabs(bottom_right.X() - top_left.X());
    const double height = std::fabs(bottom_right.Y() - top_left.Y());

    return OutputRect{
        min_x,
        min_y,
        width,
        height
    };
}

PointSetOutput ExportPointSetInternal(PointsShape& points)
{
    PointSetOutput output;
    const int count = points.size();
    output.points.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i)
    {
        output.points.push_back(OutputPoint{ points.getx(i), points.gety(i) });
    }

    output.bounds = ToOutputRect(points.boundingRect());
    return output;
}

} // namespace

PointSetOutput ExportPointSet(PointsShape& points)
{
    return ExportPointSetInternal(points);
}

ImageAnalysisOutput ExportImageAnalysis(const Image& image, double min_area, int min_width, int min_height)
{
    ImageAnalysisOutput output;
    output.width = image.getWidth();
    output.height = image.getHeight();
    output.image_type = image.getType();

    const std::vector<std::vector<cv::Point>> components = image.findConnectedComponents(min_area, min_width, min_height);
    const std::vector<cv::Rect> boxes = image.getBoundingBoxes(components);
    const std::vector<int> areas = image.calculateAreas(components);
    const std::vector<cv::Point> centroids = image.getCentroids(min_area, min_width, min_height);

    const size_t item_count = std::min(boxes.size(), std::min(areas.size(), centroids.size()));
    output.connected_components.reserve(item_count);
    for (size_t i = 0; i < item_count; ++i)
    {
        const cv::Rect& box = boxes[i];
        const cv::Point& centroid = centroids[i];
        output.connected_components.push_back(ImageComponentOutput{
            OutputRect{
                static_cast<double>(box.x),
                static_cast<double>(box.y),
                static_cast<double>(box.width),
                static_cast<double>(box.height)
            },
            OutputPoint{
                static_cast<double>(centroid.x),
                static_cast<double>(centroid.y)
            },
            areas[i]
        });
    }

    return output;
}

LineMeasurementOutput ExportLineMeasurement(Findline& line)
{
    LineMeasurementOutput output;
    output.horizontal_samples = ExportPointSetInternal(line.getresultpointsw());
    output.vertical_samples = ExportPointSetInternal(line.getresultpointsh());
    output.measure_bounds = ToOutputRect(line.measurepointsboundingrect());
    return output;
}

CircleMeasurementOutput ExportCircleMeasurement(Findcircle& circle)
{
    CircleMeasurementOutput output;
    output.sample_points = ExportPointSetInternal(circle.getresultpoints());
    output.measure_bounds = ToOutputRect(circle.measurepointsboundingrect());
    output.center = OutputPoint{ circle.getresultcentx(), circle.getresultcenty() };
    output.radius = circle.getradius();
    output.average_distance = circle.getavgdist();
    output.has_direct_fit = output.radius > 0.0;
    return output;
}

EllipseMeasurementOutput ExportEllipseMeasurement(Findellipse& ellipse)
{
    EllipseMeasurementOutput output;
    output.sample_points = ExportPointSetInternal(ellipse.getresultpoints());
    output.measure_bounds = ToOutputRect(ellipse.measurepointsboundingrect());
    return output;
}

DetectionOutput ExportDetections(FindObject& object)
{
    DetectionOutput output;
    RectsShape& rects = object.getresultrects();
    const int count = rects.size();
    output.boxes.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i)
    {
        output.boxes.push_back(ToOutputRect(rects.getrect(i)));
    }
    return output;
}

MatchOutput ExportMatchOutput(fastmatch& matcher, int max_candidates)
{
    MatchOutput output;
    output.max_score = matcher.getmaxresult();
    output.image_model_score = matcher.getimagemodelreslut();

    const int actual_count = matcher.getresultrects() ? matcher.getresultrects()->size() : 0;
    const int candidate_count = std::max(0, std::min(max_candidates, actual_count));
    output.candidates.reserve(static_cast<size_t>(candidate_count));
    for (int i = 0; i < candidate_count; ++i)
    {
        output.candidates.push_back(MatchCandidateOutput{
            ToOutputRect(matcher.getresultrect(i)),
            OutputPoint{ matcher.getresultcentx(i), matcher.getresultcenty(i) },
            matcher.getresultnum(i)
        });
    }

    return output;
}

BaselineFeatureSampleV1 ExportBaselineFeatureSampleV1(
    const Image& image,
    const ImageAnalysisOutput& analysis,
    const LineMeasurementOutput& line,
    const CircleMeasurementOutput& circle,
    const MatchOutput& match)
{
    BaselineFeatureSampleV1 sample;

    sample.roi_x = static_cast<double>(image.m_ix0);
    sample.roi_y = static_cast<double>(image.m_iy0);
    sample.roi_w = static_cast<double>(image.m_iw);
    sample.roi_h = static_cast<double>(image.m_ih);
    sample.roi_area = sample.roi_w * sample.roi_h;
    sample.roi_aspect_ratio = SafeAspectRatio(sample.roi_w, sample.roi_h);

    sample.image_width = static_cast<double>(analysis.width);
    sample.image_height = static_cast<double>(analysis.height);
    sample.image_type = static_cast<double>(analysis.image_type);

    FillGrayStatistics(MakeGrayRoi(image, MakeClampedRect(image)), sample);
    FillLargestComponent(analysis, sample);
    FillLineFields(line, sample);
    FillCircleFields(circle, sample);
    FillMatchFields(match, sample);

    return sample;
}

std::vector<std::string> GetBaselineMetadataNamesV1()
{
    return {
        "sample_id",
        "source_image_id",
        "roi_id",
        "label",
        "split"
    };
}

std::vector<std::string> GetBaselineFeatureNamesV1()
{
    return {
        "roi_x",
        "roi_y",
        "roi_w",
        "roi_h",
        "roi_area",
        "roi_aspect_ratio",
        "image_width",
        "image_height",
        "image_type",
        "gray_mean",
        "gray_std",
        "gray_min",
        "gray_max",
        "edge_pixel_ratio",
        "binary_foreground_ratio",
        "component_count",
        "largest_component_area",
        "largest_component_ratio",
        "largest_bbox_x",
        "largest_bbox_y",
        "largest_bbox_w",
        "largest_bbox_h",
        "largest_bbox_aspect_ratio",
        "largest_centroid_x",
        "largest_centroid_y",
        "line_w_points_count",
        "line_h_points_count",
        "line_measure_bbox_x",
        "line_measure_bbox_y",
        "line_measure_bbox_w",
        "line_measure_bbox_h",
        "circle_points_count",
        "circle_center_x",
        "circle_center_y",
        "circle_radius",
        "circle_avg_dist",
        "circle_measure_bbox_x",
        "circle_measure_bbox_y",
        "circle_measure_bbox_w",
        "circle_measure_bbox_h",
        "circle_fit_valid",
        "match_candidate_count",
        "match_best_score",
        "match_best_center_x",
        "match_best_center_y",
        "match_best_rect_x",
        "match_best_rect_y",
        "match_best_rect_w",
        "match_best_rect_h",
        "image_model_score"
    };
}

std::vector<std::string> ExportBaselineMetadataValuesV1(const BaselineFeatureSampleV1& sample)
{
    return {
        sample.sample_id,
        sample.source_image_id,
        sample.roi_id,
        sample.label,
        sample.split
    };
}

std::vector<double> ExportBaselineFeatureValuesV1(const BaselineFeatureSampleV1& sample)
{
    return {
        sample.roi_x,
        sample.roi_y,
        sample.roi_w,
        sample.roi_h,
        sample.roi_area,
        sample.roi_aspect_ratio,
        sample.image_width,
        sample.image_height,
        sample.image_type,
        sample.gray_mean,
        sample.gray_std,
        sample.gray_min,
        sample.gray_max,
        sample.edge_pixel_ratio,
        sample.binary_foreground_ratio,
        sample.component_count,
        sample.largest_component_area,
        sample.largest_component_ratio,
        sample.largest_bbox_x,
        sample.largest_bbox_y,
        sample.largest_bbox_w,
        sample.largest_bbox_h,
        sample.largest_bbox_aspect_ratio,
        sample.largest_centroid_x,
        sample.largest_centroid_y,
        sample.line_w_points_count,
        sample.line_h_points_count,
        sample.line_measure_bbox_x,
        sample.line_measure_bbox_y,
        sample.line_measure_bbox_w,
        sample.line_measure_bbox_h,
        sample.circle_points_count,
        sample.circle_center_x,
        sample.circle_center_y,
        sample.circle_radius,
        sample.circle_avg_dist,
        sample.circle_measure_bbox_x,
        sample.circle_measure_bbox_y,
        sample.circle_measure_bbox_w,
        sample.circle_measure_bbox_h,
        sample.circle_fit_valid,
        sample.match_candidate_count,
        sample.match_best_score,
        sample.match_best_center_x,
        sample.match_best_center_y,
        sample.match_best_rect_x,
        sample.match_best_rect_y,
        sample.match_best_rect_w,
        sample.match_best_rect_h,
        sample.image_model_score
    };
}

const char* BaselineFeatureSetNameV1(BaselineFeatureSetV1 feature_set)
{
    switch (feature_set)
    {
    case BaselineFeatureSetV1::CoreGeom:
        return "core_geom";
    case BaselineFeatureSetV1::MatchOnly:
        return "match_only";
    case BaselineFeatureSetV1::AllV1:
        return "all_v1";
    }

    return "unknown";
}

std::vector<std::string> GetBaselineFeatureNamesV1(BaselineFeatureSetV1 feature_set)
{
    return SelectByIndices(GetBaselineFeatureNamesV1(), BaselineFeatureIndicesV1(feature_set));
}

std::vector<double> ExportBaselineFeatureValuesV1(
    const BaselineFeatureSampleV1& sample,
    BaselineFeatureSetV1 feature_set)
{
    return SelectByIndices(ExportBaselineFeatureValuesV1(sample), BaselineFeatureIndicesV1(feature_set));
}

std::string ExportBaselineCsvHeaderV1()
{
    std::ostringstream header;
    bool first = true;
    for (const std::string& name : GetBaselineMetadataNamesV1())
    {
        AppendCsvValue(header, name, first);
    }
    for (const std::string& name : GetBaselineFeatureNamesV1())
    {
        AppendCsvValue(header, name, first);
    }
    return header.str();
}

std::string ExportBaselineCsvHeaderV1(BaselineFeatureSetV1 feature_set)
{
    std::ostringstream header;
    bool first = true;
    for (const std::string& name : GetBaselineMetadataNamesV1())
    {
        AppendCsvValue(header, name, first);
    }
    for (const std::string& name : GetBaselineFeatureNamesV1(feature_set))
    {
        AppendCsvValue(header, name, first);
    }
    return header.str();
}

std::string ExportBaselineCsvRowV1(const BaselineFeatureSampleV1& sample)
{
    std::ostringstream row;
    bool first = true;
    for (const std::string& value : ExportBaselineMetadataValuesV1(sample))
    {
        AppendCsvValue(row, value, first);
    }
    for (const double value : ExportBaselineFeatureValuesV1(sample))
    {
        AppendCsvValue(row, value, first);
    }
    return row.str();
}

std::string ExportBaselineCsvRowV1(
    const BaselineFeatureSampleV1& sample,
    BaselineFeatureSetV1 feature_set)
{
    std::ostringstream row;
    bool first = true;
    for (const std::string& value : ExportBaselineMetadataValuesV1(sample))
    {
        AppendCsvValue(row, value, first);
    }
    for (const double value : ExportBaselineFeatureValuesV1(sample, feature_set))
    {
        AppendCsvValue(row, value, first);
    }
    return row.str();
}

std::vector<std::string> GetBaselineSummaryNamesV1()
{
    return {
        "task",
        "feature_set",
        "model",
        "accuracy",
        "macro_f1",
        "fit_time_ms",
        "infer_time_ms",
        "feature_dim",
        "notes"
    };
}

std::vector<std::string> ExportBaselineSummaryValuesV1(const BaselineSummaryRecordV1& record)
{
    std::vector<std::string> values;
    values.reserve(9);
    values.push_back(record.task);
    values.push_back(record.feature_set);
    values.push_back(record.model);
    values.push_back(std::to_string(record.accuracy));
    values.push_back(std::to_string(record.macro_f1));
    values.push_back(std::to_string(record.fit_time_ms));
    values.push_back(std::to_string(record.infer_time_ms));
    values.push_back(std::to_string(record.feature_dim));
    values.push_back(record.notes);
    return values;
}

std::string ExportBaselineSummaryCsvHeaderV1()
{
    std::ostringstream header;
    bool first = true;
    for (const std::string& name : GetBaselineSummaryNamesV1())
    {
        AppendCsvValue(header, name, first);
    }
    return header.str();
}

std::string ExportBaselineSummaryCsvRowV1(const BaselineSummaryRecordV1& record)
{
    std::ostringstream row;
    bool first = true;
    for (const std::string& value : ExportBaselineSummaryValuesV1(record))
    {
        AppendCsvValue(row, value, first);
    }
    return row.str();
}

} // namespace cxcore
