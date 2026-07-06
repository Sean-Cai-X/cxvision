#include "CxScriptImageEvidenceAnalyzer.h"
#include "ManualStateTestConsole.h"
#include <opencv2/opencv.hpp>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

static std::string CxDebugJsonEscape(const std::string& text)
{
    std::ostringstream out;
    for (char ch : text)
    {
        switch (ch)
        {
        case '\\': out << "\\\\"; break;
        case '"': out << "\\\""; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default: out << ch; break;
        }
    }
    return out.str();
}

static double ComputeGradientAt(const cv::Mat& gray, int y, int x)
{
    if (y < 1 || y >= gray.rows - 1 || x < 1 || x >= gray.cols - 1)
        return 0.0;

    int dx = gray.at<uchar>(y, x + 1) - gray.at<uchar>(y, x - 1);
    int dy = gray.at<uchar>(y + 1, x) - gray.at<uchar>(y - 1, x);

    return std::sqrt(dx * dx + dy * dy);
}

static bool FindSubpixelEdgePeak(
    const std::vector<double>& profile,
    const std::vector<double>& gradient,
    int minGradient,
    double& subpixelPosition,
    double& maxGradient,
    bool findPositive)
{
    if (profile.size() < 3)
        return false;

    int peakIdx = -1;
    maxGradient = 0.0;

    for (int i = 1; i < (int)gradient.size() - 1; ++i)
    {
        double g = gradient[i];

        if (g < minGradient)
            continue;

        if (findPositive)
        {
            if (g > maxGradient && gradient[i] > gradient[i - 1] && gradient[i] > gradient[i + 1])
            {
                maxGradient = g;
                peakIdx = i;
            }
        }
        else
        {
            double absG = std::abs(g);
            if (absG > maxGradient && std::abs(gradient[i]) > std::abs(gradient[i - 1]) &&
                std::abs(gradient[i]) > std::abs(gradient[i + 1]))
            {
                maxGradient = absG;
                peakIdx = i;
            }
        }
    }

    if (peakIdx < 0 || maxGradient < minGradient)
        return false;

    if (peakIdx > 0 && peakIdx < (int)gradient.size() - 1)
    {
        double g0 = gradient[peakIdx - 1];
        double g1 = gradient[peakIdx];
        double g2 = gradient[peakIdx + 1];

        double delta = (g2 - g0) / (2.0 * (2.0 * g1 - g0 - g2));
        subpixelPosition = peakIdx + delta;
    }
    else
    {
        subpixelPosition = peakIdx;
    }

    return true;
}

static void ExtractProfileAlongDirection(
    const cv::Mat& gray,
    double cx, double cy,
    double dx, double dy,
    int halfWidth,
    std::vector<double>& profile)
{
    profile.clear();

    for (int i = -halfWidth; i <= halfWidth; ++i)
    {
        double px = cx + dx * i;
        double py = cy + dy * i;

        if (px >= 0 && px < gray.cols && py >= 0 && py < gray.rows)
        {
            profile.push_back(gray.at<uchar>((int)py, (int)px));
        }
        else
        {
            profile.push_back(0);
        }
    }
}

static void ComputeGradientProfile(
    const std::vector<double>& profile,
    std::vector<double>& gradient)
{
    gradient.resize(profile.size(), 0.0);

    for (int i = 1; i < (int)profile.size() - 1; ++i)
    {
        gradient[i] = profile[i + 1] - profile[i - 1];
    }
}

static bool FitLineFromPoints(
    const std::vector<cv::Point2d>& points,
    cv::Vec4d& line)
{
    if (points.size() < 2)
        return false;

    cv::Mat ptsMat(points.size(), 2, CV_64F);
    for (size_t i = 0; i < points.size(); ++i)
    {
        ptsMat.at<double>(i, 0) = points[i].x;
        ptsMat.at<double>(i, 1) = points[i].y;
    }

    cv::fitLine(ptsMat, line, cv::DIST_L2, 0, 0.01, 0.01);
    return true;
}

static bool FitCircleFromPoints(
    const std::vector<cv::Point2d>& points,
    cv::Vec3d& circle)
{
    if (points.size() < 3)
        return false;

    std::vector<cv::Point2f> pts;
    pts.reserve(points.size());
    for (size_t i = 0; i < points.size(); ++i)
    {
        pts.emplace_back(static_cast<float>(points[i].x), static_cast<float>(points[i].y));
    }

    cv::Point2f center;
    float radius;
    cv::minEnclosingCircle(pts, center, radius);
    circle[0] = center.x;
    circle[1] = center.y;
    circle[2] = radius;
    return true;
}

struct CxLineNormalForm
{
    double nx = 0.0;
    double ny = 0.0;
    double c = 0.0;
    bool valid = false;
};

static CxLineNormalForm BuildLineNormalForm(
    double x0,
    double y0,
    double x1,
    double y1)
{
    CxLineNormalForm out;

    const double dx = x1 - x0;
    const double dy = y1 - y0;
    const double len = std::sqrt(dx * dx + dy * dy);

    if (!std::isfinite(len) || len <= 1.0e-9)
        return out;

    out.nx = -dy / len;
    out.ny = dx / len;
    out.c = -(out.nx * x0 + out.ny * y0);
    out.valid = true;

    return out;
}

static void AlignLineNormalDirection(
    CxLineNormalForm& ref,
    const CxLineNormalForm& measured)
{
    const double dot = ref.nx * measured.nx + ref.ny * measured.ny;

    if (dot < 0.0)
    {
        ref.nx = -ref.nx;
        ref.ny = -ref.ny;
        ref.c = -ref.c;
    }
}

static double PointToLineDistance(
    const CxLineNormalForm& line,
    double x,
    double y)
{
    if (!line.valid)
        return std::numeric_limits<double>::quiet_NaN();

    return std::abs(line.nx * x + line.ny * y + line.c);
}

static void AnalyzeFindlineEvidence(
    const cv::Mat& image,
    const RuntimeObjectView& object,
    const CxImageEvidenceOptions& options,
    CxImageEvidenceSummary& summary,
    const fs::path& outputDir)
{
    summary.tool = "Findline";
    summary.object_name = object.name;
    summary.reference_available = false;

    if (!object.has_line_roi)
    {
        summary.conclusion = "Findline ROI not available";
        return;
    }

    cv::Mat gray;
    if (image.channels() == 1)
    {
        gray = image.clone();
    }
    else
    {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    }

    double dx = object.line_x1 - object.line_x0;
    double dy = object.line_y1 - object.line_y0;
    double lineLength = std::sqrt(dx * dx + dy * dy);

    if (lineLength < 1.0)
    {
        summary.conclusion = "Findline ROI too short";
        return;
    }

    double dirX = dx / lineLength;
    double dirY = dy / lineLength;

    double normalX = -dirY;
    double normalY = dirX;

    int halfWidth = options.profile_half_width;
    if (object.line_scan_half_width > 0)
    {
        halfWidth = (int)object.line_scan_half_width;
    }

    double step = object.linegap > 0 ? object.linegap : 4.0;
    int numSamples = (int)(lineLength / step);
    if (numSamples > options.max_profiles)
        numSamples = options.max_profiles;
    if (numSamples < 5)
        numSamples = 5;

    std::vector<cv::Point2d> referencePositivePoints;
    std::vector<cv::Point2d> referenceNegativePoints;
    std::vector<cv::Point2d> referenceAbsPoints;

    for (int i = 0; i < numSamples; ++i)
    {
        double t = (double)i / (double)(numSamples - 1);
        double cx = object.line_x0 + dx * t;
        double cy = object.line_y0 + dy * t;

        std::vector<double> profile;
        ExtractProfileAlongDirection(gray, cx, cy, normalX, normalY, halfWidth, profile);

        std::vector<double> gradient;
        ComputeGradientProfile(profile, gradient);

        double posSubpixel = 0.0, posGradient = 0.0;
        double negSubpixel = 0.0, negGradient = 0.0;

        bool foundPos = FindSubpixelEdgePeak(profile, gradient, options.min_gradient, posSubpixel, posGradient, true);
        bool foundNeg = FindSubpixelEdgePeak(profile, gradient, options.min_gradient, negSubpixel, negGradient, false);

        if (foundPos)
        {
            double relPos = (posSubpixel - (double)(profile.size() - 1) / 2.0) / (double)(profile.size() - 1) * 2.0;
            referencePositivePoints.emplace_back(
                cx + normalX * relPos * halfWidth,
                cy + normalY * relPos * halfWidth);
        }

        if (foundNeg)
        {
            double relPos = (negSubpixel - (double)(profile.size() - 1) / 2.0) / (double)(profile.size() - 1) * 2.0;
            referenceNegativePoints.emplace_back(
                cx + normalX * relPos * halfWidth,
                cy + normalY * relPos * halfWidth);
        }

        double absSubpixel = foundPos && posGradient > negGradient ? posSubpixel : negSubpixel;
        double absGradient = std::max(posGradient, negGradient);

        if (absGradient >= options.min_gradient)
        {
            double relPos = (absSubpixel - (double)(profile.size() - 1) / 2.0) / (double)(profile.size() - 1) * 2.0;
            referenceAbsPoints.emplace_back(
                cx + normalX * relPos * halfWidth,
                cy + normalY * relPos * halfWidth);
        }
    }

    summary.positive_reference_points = (int)referencePositivePoints.size();
    summary.negative_reference_points = (int)referenceNegativePoints.size();
    summary.abs_reference_points = (int)referenceAbsPoints.size();

    cv::Vec4d referenceLinePositive, referenceLineNegative, referenceLineAbs;
    bool refFitPos = FitLineFromPoints(referencePositivePoints, referenceLinePositive);
    bool refFitNeg = FitLineFromPoints(referenceNegativePoints, referenceLineNegative);
    bool refFitAbs = FitLineFromPoints(referenceAbsPoints, referenceLineAbs);

    auto ComputeMeanErrorForReference = [&](const std::vector<cv::Point2d>& refPts) -> double {
        if (refPts.empty() || object.line_measure_points_xy.size() < 2)
            return std::numeric_limits<double>::max();

        double totalDist = 0.0;
        int count = 0;

        for (size_t i = 0; i + 1 < object.line_measure_points_xy.size(); i += 2)
        {
            double mx = object.line_measure_points_xy[i];
            double my = object.line_measure_points_xy[i + 1];

            double minDist = options.nearest_point_support_px + 1.0;

            for (const auto& refPt : refPts)
            {
                double dx = mx - refPt.x;
                double dy = my - refPt.y;
                double dist = std::sqrt(dx * dx + dy * dy);
                if (dist < minDist)
                    minDist = dist;
            }

            if (minDist <= options.nearest_point_support_px)
            {
                totalDist += minDist;
                count++;
            }
        }

        return count > 0 ? totalDist / count : std::numeric_limits<double>::max();
    };

    summary.mean_error_positive = ComputeMeanErrorForReference(referencePositivePoints);
    summary.mean_error_negative = ComputeMeanErrorForReference(referenceNegativePoints);
    summary.mean_error_abs = ComputeMeanErrorForReference(referenceAbsPoints);

    std::vector<cv::Point2d> bestReferencePoints;
    cv::Vec4d bestReferenceLine;
    bool bestRefFit = false;

    if (summary.mean_error_positive <= summary.mean_error_negative &&
        summary.mean_error_positive <= summary.mean_error_abs &&
        !referencePositivePoints.empty())
    {
        summary.best_reference_polarity = "positive";
        bestReferencePoints = referencePositivePoints;
        bestReferenceLine = referenceLinePositive;
        bestRefFit = refFitPos;
    }
    else if (summary.mean_error_negative <= summary.mean_error_positive &&
             summary.mean_error_negative <= summary.mean_error_abs &&
             !referenceNegativePoints.empty())
    {
        summary.best_reference_polarity = "negative";
        bestReferencePoints = referenceNegativePoints;
        bestReferenceLine = referenceLineNegative;
        bestRefFit = refFitNeg;
    }
    else
    {
        summary.best_reference_polarity = "abs";
        bestReferencePoints = referenceAbsPoints;
        bestReferenceLine = referenceLineAbs;
        bestRefFit = refFitAbs;
    }

    summary.reference_points_count = (int)bestReferencePoints.size();

    if (bestRefFit)
    {
        summary.reference_fit_available = true;
        summary.reference_available = true;
    }

    summary.measured_points_count = (int)(object.line_measure_points_xy.size() / 2);
    summary.supported_points_count = 0;
    summary.unsupported_points_count = 0;
    summary.distance_supported_points = 0;
    summary.gradient_supported_points = 0;
    summary.combined_supported_points = 0;

    double totalError = 0.0;
    double maxError = 0.0;
    double totalLineDistance = 0.0;
    double maxLineDistance = 0.0;
    double totalGradientRatio = 0.0;
    int gradRatioCount = 0;

    CxLineNormalForm bestReferenceLineNorm;
    if (bestRefFit)
    {
        bestReferenceLineNorm = BuildLineNormalForm(
            bestReferenceLine[2], bestReferenceLine[3],
            bestReferenceLine[2] + bestReferenceLine[0],
            bestReferenceLine[3] + bestReferenceLine[1]);
    }

    for (size_t i = 0; i + 1 < object.line_measure_points_xy.size(); i += 2)
    {
        double mx = object.line_measure_points_xy[i];
        double my = object.line_measure_points_xy[i + 1];

        double minDist = options.nearest_point_support_px + 1.0;
        double refX = 0.0, refY = 0.0;
        double refGradient = 0.0;

        for (const auto& refPt : bestReferencePoints)
        {
            double dx = mx - refPt.x;
            double dy = my - refPt.y;
            double dist = std::sqrt(dx * dx + dy * dy);
            if (dist < minDist)
            {
                minDist = dist;
                refX = refPt.x;
                refY = refPt.y;
                refGradient = ComputeGradientAt(gray, (int)refPt.y, (int)refPt.x);
            }
        }

        double lineDistance = std::numeric_limits<double>::quiet_NaN();
        if (options.use_line_distance_for_findline_support && bestReferenceLineNorm.valid)
        {
            lineDistance = PointToLineDistance(bestReferenceLineNorm, mx, my);
        }

        double localGradient = ComputeGradientAt(gray, (int)my, (int)mx);
        double gradientRatio = refGradient > 1.0e-6
            ? std::abs(localGradient) / refGradient
            : 0.0;

        bool distanceSupported = false;
        if (options.use_line_distance_for_findline_support && bestReferenceLineNorm.valid)
        {
            distanceSupported = lineDistance <= options.line_distance_support_px;
        }
        else
        {
            distanceSupported = minDist <= options.nearest_point_support_px;
        }

        bool gradientSupported =
            std::abs(localGradient) >= options.min_gradient ||
            gradientRatio >= options.min_gradient_ratio;

        bool combinedSupported = distanceSupported && gradientSupported;

        CxPointEvidence pe;
        pe.measured_x = mx;
        pe.measured_y = my;
        pe.reference_x = refX;
        pe.reference_y = refY;
        pe.nearest_reference_distance_px = minDist;
        pe.reference_line_distance_px = lineDistance;
        pe.local_gradient = localGradient;
        pe.reference_gradient = refGradient;
        pe.gradient_ratio = gradientRatio;
        pe.distance_supported = distanceSupported;
        pe.gradient_supported = gradientSupported;
        pe.combined_supported = combinedSupported;
        pe.reference_polarity = summary.best_reference_polarity;

        std::vector<std::string> reasons;
        if (!distanceSupported)
        {
            if (options.use_line_distance_for_findline_support && bestReferenceLineNorm.valid)
                reasons.push_back("line_distance_exceeds");
            else
                reasons.push_back("too_far_from_reference");
        }
        if (!gradientSupported)
        {
            if (std::abs(localGradient) < options.min_gradient)
                reasons.push_back("low_local_gradient");
            if (gradientRatio < options.min_gradient_ratio)
                reasons.push_back("low_gradient_ratio");
        }
        pe.reason = combinedSupported ? "combined_supported" : (reasons.empty() ? "unknown" : reasons[0]);

        summary.point_evidences.push_back(pe);

        if (combinedSupported)
        {
            summary.supported_points_count++;
            summary.combined_supported_points++;
        }
        else
        {
            summary.unsupported_points_count++;
        }

        if (distanceSupported)
            summary.distance_supported_points++;
        if (gradientSupported)
            summary.gradient_supported_points++;

        if (minDist <= options.nearest_point_support_px)
        {
            totalError += minDist;
            if (minDist > maxError)
                maxError = minDist;
        }

        if (std::isfinite(lineDistance))
        {
            totalLineDistance += lineDistance;
            if (lineDistance > maxLineDistance)
                maxLineDistance = lineDistance;
        }

        if (gradientRatio > 0.0)
        {
            totalGradientRatio += gradientRatio;
            gradRatioCount++;
        }
    }

    if (summary.supported_points_count > 0)
    {
        summary.mean_error_px = totalError / summary.supported_points_count;
        summary.max_error_px = maxError;
    }

    if (summary.measured_points_count > 0)
    {
        summary.distance_support_score = (double)summary.distance_supported_points / summary.measured_points_count;
        summary.gradient_support_score = (double)summary.gradient_supported_points / summary.measured_points_count;
        summary.combined_edge_support_score = (double)summary.combined_supported_points / summary.measured_points_count;
        summary.edge_support_score = summary.combined_edge_support_score;
    }

    if (summary.measured_points_count > 0 && std::isfinite(totalLineDistance))
    {
        summary.mean_reference_line_distance_px = totalLineDistance / summary.measured_points_count;
        summary.max_reference_line_distance_px = maxLineDistance;
    }

    if (gradRatioCount > 0)
    {
        summary.mean_gradient_ratio = totalGradientRatio / gradRatioCount;
    }

    if (object.has_fit_line && summary.reference_fit_available)
    {
        summary.measured_fit_available = true;

        CxLineNormalForm measuredLine = BuildLineNormalForm(
            object.fit_line_x0,
            object.fit_line_y0,
            object.fit_line_x1,
            object.fit_line_y1);

        CxLineNormalForm referenceLine = BuildLineNormalForm(
            object.line_x0,
            object.line_y0,
            object.line_x1,
            object.line_y1);

        if (measuredLine.valid && referenceLine.valid)
        {
            AlignLineNormalDirection(referenceLine, measuredLine);

            const double mx = 0.5 * (object.fit_line_x0 + object.fit_line_x1);
            const double my = 0.5 * (object.fit_line_y0 + object.fit_line_y1);

            summary.fit_offset_error_px = PointToLineDistance(referenceLine, mx, my);

            const double dirDot =
                std::abs(measuredLine.nx * referenceLine.nx +
                         measuredLine.ny * referenceLine.ny);

            const double clampedDot =
                std::max(-1.0, std::min(1.0, dirDot));

            summary.fit_angle_error_deg =
                std::acos(clampedDot) * 180.0 / CV_PI;
        }
        else
        {
            summary.fit_offset_error_px = -1.0;
            summary.fit_angle_error_deg = -1.0;
        }
    }

    if (!summary.reference_available)
    {
        summary.conclusion = "No image evidence reference edge found";
    }
    else if (summary.measured_points_count == 0)
    {
        summary.conclusion = "Original Measure produced no points, but image has detectable edges";
    }
    else if (summary.mean_error_px <= 2.0 &&
             summary.fit_offset_error_px > 50.0 &&
             summary.fit_offset_error_px > 0.0)
    {
        summary.metric_quality = "inconsistent_line_fit_metric";
        summary.conclusion =
            "Point evidence is close, but fit offset is abnormally large. Check reference line selection or fit error calculation.";
    }
    else if (summary.edge_support_score >= 0.7 && summary.mean_error_px <= 2.0)
    {
        summary.conclusion = "Original Measure points well supported by image evidence";
    }
    else if (summary.edge_support_score >= 0.5)
    {
        summary.conclusion = "Original Measure partially supported by image evidence";
    }
    else
    {
        summary.conclusion = "Original Measure points poorly supported by image evidence";
    }
}

static void AnalyzeFindcircleEvidence(
    const cv::Mat& image,
    const RuntimeObjectView& object,
    const CxImageEvidenceOptions& options,
    CxImageEvidenceSummary& summary,
    const fs::path& outputDir)
{
    summary.tool = "Findcircle";
    summary.object_name = object.name;
    summary.reference_available = false;

    if (!object.has_circle)
    {
        summary.conclusion = "Findcircle ROI not available";
        return;
    }

    cv::Mat gray;
    if (image.channels() == 1)
    {
        gray = image.clone();
    }
    else
    {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    }

    double cx = object.circle_cx;
    double cy = object.circle_cy;
    double radius = object.circle_radius;

    if (object.has_fit_result)
    {
        cx = object.fit_cx;
        cy = object.fit_cy;
        radius = object.fit_radius;
    }

    if (radius < 1.0)
    {
        summary.conclusion = "Findcircle radius too small";
        return;
    }

    int numAngles = options.max_profiles;
    double angleStep = 2.0 * CV_PI / numAngles;

    int halfWidth = options.profile_half_width;
    int effectiveHalfWidth = halfWidth;

    std::vector<cv::Point2d> referencePositivePoints;
    std::vector<cv::Point2d> referenceNegativePoints;
    std::vector<cv::Point2d> referenceAbsPoints;

    for (int i = 0; i < numAngles; ++i)
    {
        double angle = i * angleStep;
        double dirX = std::cos(angle);
        double dirY = std::sin(angle);

        std::vector<double> profile;

        for (int j = -effectiveHalfWidth; j <= effectiveHalfWidth; ++j)
        {
            double px = cx + (radius + j) * dirX;
            double py = cy + (radius + j) * dirY;

            if (px >= 0 && px < gray.cols && py >= 0 && py < gray.rows)
            {
                profile.push_back(gray.at<uchar>((int)py, (int)px));
            }
            else
            {
                profile.push_back(0);
            }
        }

        std::vector<double> gradient;
        ComputeGradientProfile(profile, gradient);

        double posSubpixel = 0.0, posGradient = 0.0;
        double negSubpixel = 0.0, negGradient = 0.0;

        bool foundPos = FindSubpixelEdgePeak(profile, gradient, options.min_gradient, posSubpixel, posGradient, true);
        bool foundNeg = FindSubpixelEdgePeak(profile, gradient, options.min_gradient, negSubpixel, negGradient, false);

        if (foundPos)
        {
            double relPos = (posSubpixel - (double)(profile.size() - 1) / 2.0);
            referencePositivePoints.emplace_back(
                cx + (radius + relPos) * dirX,
                cy + (radius + relPos) * dirY);
        }

        if (foundNeg)
        {
            double relPos = (negSubpixel - (double)(profile.size() - 1) / 2.0);
            referenceNegativePoints.emplace_back(
                cx + (radius + relPos) * dirX,
                cy + (radius + relPos) * dirY);
        }

        double absGradient = std::max(posGradient, negGradient);

        if (absGradient >= options.min_gradient)
        {
            double bestSubpixel = foundPos && posGradient > negGradient ? posSubpixel : negSubpixel;
            double relPos = (bestSubpixel - (double)(profile.size() - 1) / 2.0);
            referenceAbsPoints.emplace_back(
                cx + (radius + relPos) * dirX,
                cy + (radius + relPos) * dirY);
        }
    }

    summary.positive_reference_points = (int)referencePositivePoints.size();
    summary.negative_reference_points = (int)referenceNegativePoints.size();
    summary.abs_reference_points = (int)referenceAbsPoints.size();

    cv::Vec3d referenceCirclePositive, referenceCircleNegative, referenceCircleAbs;
    bool refFitPos = FitCircleFromPoints(referencePositivePoints, referenceCirclePositive);
    bool refFitNeg = FitCircleFromPoints(referenceNegativePoints, referenceCircleNegative);
    bool refFitAbs = FitCircleFromPoints(referenceAbsPoints, referenceCircleAbs);

    auto ComputeMeanRadialErrorForReference = [&](const cv::Vec3d& refCircle, const std::vector<cv::Point2d>& refPts) -> double {
        if (refPts.empty() || object.measure_points_xy.size() < 2)
            return std::numeric_limits<double>::max();

        double totalDist = 0.0;
        int count = 0;

        for (size_t i = 0; i + 1 < object.measure_points_xy.size(); i += 2)
        {
            double mx = object.measure_points_xy[i];
            double my = object.measure_points_xy[i + 1];

            double minDist = options.nearest_point_support_px + 1.0;

            for (const auto& refPt : refPts)
            {
                double dx = mx - refPt.x;
                double dy = my - refPt.y;
                double dist = std::sqrt(dx * dx + dy * dy);
                if (dist < minDist)
                    minDist = dist;
            }

            if (minDist <= options.nearest_point_support_px)
            {
                totalDist += minDist;
                count++;
            }
        }

        return count > 0 ? totalDist / count : std::numeric_limits<double>::max();
    };

    summary.mean_error_positive = ComputeMeanRadialErrorForReference(referenceCirclePositive, referencePositivePoints);
    summary.mean_error_negative = ComputeMeanRadialErrorForReference(referenceCircleNegative, referenceNegativePoints);
    summary.mean_error_abs = ComputeMeanRadialErrorForReference(referenceCircleAbs, referenceAbsPoints);

    cv::Vec3d bestReferenceCircle;
    std::vector<cv::Point2d> bestReferencePoints;
    bool bestRefFit = false;

    if (summary.mean_error_positive <= summary.mean_error_negative &&
        summary.mean_error_positive <= summary.mean_error_abs &&
        !referencePositivePoints.empty())
    {
        summary.best_reference_polarity = "positive";
        bestReferencePoints = referencePositivePoints;
        bestReferenceCircle = referenceCirclePositive;
        bestRefFit = refFitPos;
    }
    else if (summary.mean_error_negative <= summary.mean_error_positive &&
             summary.mean_error_negative <= summary.mean_error_abs &&
             !referenceNegativePoints.empty())
    {
        summary.best_reference_polarity = "negative";
        bestReferencePoints = referenceNegativePoints;
        bestReferenceCircle = referenceCircleNegative;
        bestRefFit = refFitNeg;
    }
    else
    {
        summary.best_reference_polarity = "abs";
        bestReferencePoints = referenceAbsPoints;
        bestReferenceCircle = referenceCircleAbs;
        bestRefFit = refFitAbs;
    }

    summary.reference_points_count = (int)bestReferencePoints.size();

    if (bestRefFit)
    {
        summary.reference_fit_available = true;
        summary.reference_available = true;
    }

    summary.measured_points_count = object.measure_points_count;
    summary.supported_points_count = 0;
    summary.unsupported_points_count = 0;
    summary.distance_supported_points = 0;
    summary.gradient_supported_points = 0;
    summary.combined_supported_points = 0;

    double totalError = 0.0;
    double maxError = 0.0;
    double totalGradientRatio = 0.0;
    int gradRatioCount = 0;

    for (size_t i = 0; i + 1 < object.measure_points_xy.size(); i += 2)
    {
        double mx = object.measure_points_xy[i];
        double my = object.measure_points_xy[i + 1];

        double minDist = options.nearest_point_support_px + 1.0;
        double refX = 0.0, refY = 0.0;
        double refGradient = 0.0;

        for (const auto& refPt : bestReferencePoints)
        {
            double dx = mx - refPt.x;
            double dy = my - refPt.y;
            double dist = std::sqrt(dx * dx + dy * dy);
            if (dist < minDist)
            {
                minDist = dist;
                refX = refPt.x;
                refY = refPt.y;
                refGradient = ComputeGradientAt(gray, (int)refPt.y, (int)refPt.x);
            }
        }

        double localGradient = ComputeGradientAt(gray, (int)my, (int)mx);
        double gradientRatio = refGradient > 1.0e-6
            ? std::abs(localGradient) / refGradient
            : 0.0;

        bool distanceSupported = minDist <= options.nearest_point_support_px;
        bool gradientSupported =
            std::abs(localGradient) >= options.min_gradient ||
            gradientRatio >= options.min_gradient_ratio;
        bool combinedSupported = distanceSupported && gradientSupported;

        CxPointEvidence pe;
        pe.measured_x = mx;
        pe.measured_y = my;
        pe.reference_x = refX;
        pe.reference_y = refY;
        pe.nearest_reference_distance_px = minDist;
        pe.local_gradient = localGradient;
        pe.reference_gradient = refGradient;
        pe.gradient_ratio = gradientRatio;
        pe.distance_supported = distanceSupported;
        pe.gradient_supported = gradientSupported;
        pe.combined_supported = combinedSupported;
        pe.reference_polarity = summary.best_reference_polarity;

        std::vector<std::string> reasons;
        if (!distanceSupported)
            reasons.push_back("too_far_from_reference");
        if (!gradientSupported)
        {
            if (std::abs(localGradient) < options.min_gradient)
                reasons.push_back("low_local_gradient");
            if (gradientRatio < options.min_gradient_ratio)
                reasons.push_back("low_gradient_ratio");
        }
        pe.reason = combinedSupported ? "combined_supported" : (reasons.empty() ? "unknown" : reasons[0]);

        summary.point_evidences.push_back(pe);

        if (combinedSupported)
        {
            summary.supported_points_count++;
            summary.combined_supported_points++;
        }
        else
        {
            summary.unsupported_points_count++;
        }

        if (distanceSupported)
            summary.distance_supported_points++;
        if (gradientSupported)
            summary.gradient_supported_points++;

        if (minDist <= options.nearest_point_support_px)
        {
            totalError += minDist;
            if (minDist > maxError)
                maxError = minDist;
        }

        if (gradientRatio > 0.0)
        {
            totalGradientRatio += gradientRatio;
            gradRatioCount++;
        }
    }

    if (summary.supported_points_count > 0)
    {
        summary.mean_error_px = totalError / summary.supported_points_count;
        summary.max_error_px = maxError;
    }

    if (summary.measured_points_count > 0)
    {
        summary.distance_support_score = (double)summary.distance_supported_points / summary.measured_points_count;
        summary.gradient_support_score = (double)summary.gradient_supported_points / summary.measured_points_count;
        summary.combined_edge_support_score = (double)summary.combined_supported_points / summary.measured_points_count;
        summary.edge_support_score = summary.combined_edge_support_score;
    }

    if (gradRatioCount > 0)
    {
        summary.mean_gradient_ratio = totalGradientRatio / gradRatioCount;
    }

    if (object.has_fit_result && summary.reference_fit_available)
    {
        summary.measured_fit_available = true;

        double dcx = object.fit_cx - bestReferenceCircle[0];
        double dcy = object.fit_cy - bestReferenceCircle[1];
        summary.circle_center_error_px = std::sqrt(dcx * dcx + dcy * dcy);
        summary.circle_radius_error_px = std::abs(object.fit_radius - bestReferenceCircle[2]);
    }

    if (!summary.reference_available)
    {
        summary.conclusion = "No image evidence reference circle edge found";
    }
    else if (summary.measured_points_count == 0)
    {
        summary.conclusion = "Original Measure produced no points, but image has detectable circle edges";
    }
    else if (summary.edge_support_score >= 0.7 && summary.circle_center_error_px <= 2.0)
    {
        summary.conclusion = "Original Measure circle well supported by image evidence";
    }
    else if (summary.edge_support_score >= 0.5)
    {
        summary.conclusion = "Original Measure circle partially supported by image evidence";
    }
    else
    {
        summary.conclusion = "Original Measure circle poorly supported by image evidence";
    }
}

static bool SaveEvidenceOverlay(
    const cv::Mat& image,
    const ManualTestContext& context,
    const std::vector<CxImageEvidenceSummary>& summaries,
    const fs::path& outputPath)
{
    try
    {
        cv::Mat canvas;
        image.copyTo(canvas);

        if (canvas.channels() == 1)
        {
            cv::cvtColor(canvas, canvas, cv::COLOR_GRAY2BGR);
        }

        for (const RuntimeObjectView& object : context.runtime_objects)
        {
            if (object.type == "Findline")
            {
                if (object.has_line_roi)
                {
                    cv::line(canvas,
                        cv::Point((int)object.line_x0, (int)object.line_y0),
                        cv::Point((int)object.line_x1, (int)object.line_y1),
                        cv::Scalar(0, 255, 0), 2);
                }

                if (object.has_line_scan_box)
                {
                    std::vector<cv::Point> box;
                    for (size_t i = 0; i + 1 < object.line_scan_box_xy.size(); i += 2)
                    {
                        box.emplace_back((int)object.line_scan_box_xy[i], (int)object.line_scan_box_xy[i + 1]);
                    }
                    if (box.size() >= 4)
                        cv::polylines(canvas, box, true, cv::Scalar(0, 180, 0), 1);
                }

                for (size_t i = 0; i + 1 < object.line_measure_points_xy.size(); i += 2)
                {
                    cv::circle(canvas,
                        cv::Point((int)object.line_measure_points_xy[i], (int)object.line_measure_points_xy[i + 1]),
                        3, cv::Scalar(0, 0, 255), -1);
                }

                if (object.has_fit_line)
                {
                    cv::line(canvas,
                        cv::Point((int)object.fit_line_x0, (int)object.fit_line_y0),
                        cv::Point((int)object.fit_line_x1, (int)object.fit_line_y1),
                        cv::Scalar(0, 255, 255), 2);
                }
            }
            else if (object.type == "Findcircle")
            {
                if (object.has_circle)
                {
                    cv::circle(canvas,
                        cv::Point((int)object.circle_cx, (int)object.circle_cy),
                        (int)object.circle_radius,
                        cv::Scalar(0, 255, 0), 2);
                }

                if (object.has_measure_points)
                {
                    for (size_t i = 0; i + 1 < object.measure_points_xy.size(); i += 2)
                    {
                        cv::circle(canvas,
                            cv::Point((int)object.measure_points_xy[i], (int)object.measure_points_xy[i + 1]),
                            3, cv::Scalar(0, 0, 255), -1);
                    }
                }

                if (object.has_fit_result)
                {
                    cv::circle(canvas,
                        cv::Point((int)object.fit_cx, (int)object.fit_cy),
                        (int)object.fit_radius,
                        cv::Scalar(0, 255, 255), 2);
                }
            }
        }

        for (const CxImageEvidenceSummary& summary : summaries)
        {
            for (const CxPointEvidence& pe : summary.point_evidences)
            {
                cv::circle(canvas,
                    cv::Point((int)pe.reference_x, (int)pe.reference_y),
                    3, cv::Scalar(255, 255, 0), -1);

                if (pe.combined_supported)
                {
                    cv::line(canvas,
                        cv::Point((int)pe.measured_x, (int)pe.measured_y),
                        cv::Point((int)pe.reference_x, (int)pe.reference_y),
                        cv::Scalar(128, 0, 128), 1);
                }
            }
        }

        fs::create_directories(outputPath.parent_path());
        return cv::imwrite(outputPath.string(), canvas);
    }
    catch (...)
    {
        return false;
    }
}

static bool SaveEvidenceSummaryJson(
    const std::vector<CxImageEvidenceSummary>& summaries,
    const fs::path& outputPath)
{
    try
    {
        fs::create_directories(outputPath.parent_path());

        std::ofstream file(outputPath.string());
        if (!file.is_open())
            return false;

        file << "{\n";
        file << "  \"evidence_summaries\": [\n";

        for (size_t i = 0; i < summaries.size(); ++i)
        {
            const CxImageEvidenceSummary& s = summaries[i];

            file << "    {\n";
            file << "      \"tool\": \"" << CxDebugJsonEscape(s.tool) << "\",\n";
            file << "      \"object_name\": \"" << CxDebugJsonEscape(s.object_name) << "\",\n";
            file << "      \"reference_available\": " << (s.reference_available ? "true" : "false") << ",\n";
            file << "      \"reference_points_count\": " << s.reference_points_count << ",\n";
            file << "      \"measured_points_count\": " << s.measured_points_count << ",\n";
            file << "      \"supported_points_count\": " << s.supported_points_count << ",\n";
            file << "      \"unsupported_points_count\": " << s.unsupported_points_count << ",\n";
            file << "      \"mean_error_px\": " << s.mean_error_px << ",\n";
            file << "      \"max_error_px\": " << s.max_error_px << ",\n";
            file << "      \"edge_support_score\": " << s.edge_support_score << ",\n";
            file << "      \"distance_support_score\": " << s.distance_support_score << ",\n";
            file << "      \"gradient_support_score\": " << s.gradient_support_score << ",\n";
            file << "      \"combined_edge_support_score\": " << s.combined_edge_support_score << ",\n";
            file << "      \"mean_reference_line_distance_px\": " << s.mean_reference_line_distance_px << ",\n";
            file << "      \"max_reference_line_distance_px\": " << s.max_reference_line_distance_px << ",\n";
            file << "      \"mean_gradient_ratio\": " << s.mean_gradient_ratio << ",\n";
            file << "      \"distance_supported_points\": " << s.distance_supported_points << ",\n";
            file << "      \"gradient_supported_points\": " << s.gradient_supported_points << ",\n";
            file << "      \"combined_supported_points\": " << s.combined_supported_points << ",\n";
            file << "      \"reference_fit_available\": " << (s.reference_fit_available ? "true" : "false") << ",\n";
            file << "      \"measured_fit_available\": " << (s.measured_fit_available ? "true" : "false") << ",\n";
            file << "      \"fit_offset_error_px\": " << s.fit_offset_error_px << ",\n";
            file << "      \"fit_angle_error_deg\": " << s.fit_angle_error_deg << ",\n";
            file << "      \"circle_center_error_px\": " << s.circle_center_error_px << ",\n";
            file << "      \"circle_radius_error_px\": " << s.circle_radius_error_px << ",\n";
            file << "      \"best_reference_polarity\": \"" << CxDebugJsonEscape(s.best_reference_polarity) << "\",\n";
            file << "      \"positive_reference_points\": " << s.positive_reference_points << ",\n";
            file << "      \"negative_reference_points\": " << s.negative_reference_points << ",\n";
            file << "      \"abs_reference_points\": " << s.abs_reference_points << ",\n";
            file << "      \"mean_error_positive\": " << s.mean_error_positive << ",\n";
            file << "      \"mean_error_negative\": " << s.mean_error_negative << ",\n";
            file << "      \"mean_error_abs\": " << s.mean_error_abs << ",\n";
            file << "      \"metric_quality\": \"" << CxDebugJsonEscape(s.metric_quality) << "\",\n";
            file << "      \"conclusion\": \"" << CxDebugJsonEscape(s.conclusion) << "\"\n";
            file << "    }";

            if (i < summaries.size() - 1)
                file << ",";
            file << "\n";
        }

        file << "  ]\n";
        file << "}\n";

        return true;
    }
    catch (...)
    {
        return false;
    }
}

static bool SavePointEvidenceCsv(
    const std::vector<CxImageEvidenceSummary>& summaries,
    const fs::path& outputPath)
{
    try
    {
        fs::create_directories(outputPath.parent_path());

        std::ofstream file(outputPath.string());
        if (!file.is_open())
            return false;

        file << "tool,object_name,point_index,measured_x,measured_y,reference_x,reference_y,nearest_reference_distance_px,reference_line_distance_px,local_gradient,reference_gradient,gradient_ratio,distance_supported,gradient_supported,combined_supported,reference_polarity,reason\n";

        for (const CxImageEvidenceSummary& s : summaries)
        {
            for (size_t i = 0; i < s.point_evidences.size(); ++i)
            {
                const CxPointEvidence& pe = s.point_evidences[i];
                file << s.tool << ","
                     << s.object_name << ","
                     << i << ","
                     << pe.measured_x << ","
                     << pe.measured_y << ","
                     << pe.reference_x << ","
                     << pe.reference_y << ","
                     << pe.nearest_reference_distance_px << ","
                     << pe.reference_line_distance_px << ","
                     << pe.local_gradient << ","
                     << pe.reference_gradient << ","
                     << pe.gradient_ratio << ","
                     << (pe.distance_supported ? "true" : "false") << ","
                     << (pe.gradient_supported ? "true" : "false") << ","
                     << (pe.combined_supported ? "true" : "false") << ","
                     << pe.reference_polarity << ","
                     << pe.reason << "\n";
            }
        }

        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool AnalyzeCxScriptImageEvidence(
    const cv::Mat& image,
    const ManualTestContext& context,
    const CxImageEvidenceOptions& options,
    const fs::path& outputDir,
    std::string& outReason)
{
    try
    {
        std::vector<CxImageEvidenceSummary> summaries;

        for (const RuntimeObjectView& object : context.runtime_objects)
        {
            CxImageEvidenceSummary summary;

            if (object.type == "Findline")
            {
                AnalyzeFindlineEvidence(image, object, options, summary, outputDir);
                summaries.push_back(summary);
            }
            else if (object.type == "Findcircle")
            {
                AnalyzeFindcircleEvidence(image, object, options, summary, outputDir);
                summaries.push_back(summary);
            }
        }

        fs::path overlayPath = outputDir / "evidence_overlay.png";
        if (!SaveEvidenceOverlay(image, context, summaries, overlayPath))
        {
            outReason = "failed to save evidence overlay";
            return false;
        }

        fs::path summaryPath = outputDir / "evidence_summary.json";
        if (!SaveEvidenceSummaryJson(summaries, summaryPath))
        {
            outReason = "failed to save evidence summary json";
            return false;
        }

        fs::path csvPath = outputDir / "point_evidence.csv";
        if (!SavePointEvidenceCsv(summaries, csvPath))
        {
            outReason = "failed to save point evidence csv";
            return false;
        }

        return true;
    }
    catch (const std::exception& e)
    {
        outReason = "AnalyzeCxScriptImageEvidence exception: " + std::string(e.what());
        return false;
    }
}