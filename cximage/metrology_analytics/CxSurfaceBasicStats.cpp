#include "pch.h"
#include "metrology_analytics/CxSurfaceBasicStats.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace cxvision::metrology_analytics
{
namespace
{
CxHeightDistribution BuildDistribution(const CxSurfaceField& field, double min_z, double max_z, int bins)
{
    if (bins <= 0)
        throw std::invalid_argument("histogram bins must be positive.");

    CxHeightDistribution h;
    h.bin_centers.assign(static_cast<std::size_t>(bins), 0.0);
    h.adf.assign(static_cast<std::size_t>(bins), 0.0);
    h.bcdf.assign(static_cast<std::size_t>(bins), 0.0);

    const int n = field.valueCount();
    if (n <= 0)
        return h;

    const double range = max_z - min_z;
    if (range <= 0.0)
    {
        h.bin_centers[0] = min_z;
        h.adf[0] = 1.0;
        h.bcdf[0] = 1.0;
        for (int i = 1; i < bins; ++i)
        {
            h.bin_centers[static_cast<std::size_t>(i)] = min_z;
            h.bcdf[static_cast<std::size_t>(i)] = 0.0;
        }
        return h;
    }

    for (int i = 0; i < bins; ++i)
        h.bin_centers[static_cast<std::size_t>(i)] = min_z + (static_cast<double>(i) + 0.5) * range / static_cast<double>(bins);

    for (int y = 0; y < field.yres(); ++y)
    {
        for (int x = 0; x < field.xres(); ++x)
        {
            int idx = static_cast<int>(std::floor((field.at(x, y) - min_z) * static_cast<double>(bins) / range));
            idx = std::max(0, std::min(bins - 1, idx));
            h.adf[static_cast<std::size_t>(idx)] += 1.0;
        }
    }

    for (double& v : h.adf)
        v /= static_cast<double>(n);

    double running = 0.0;
    for (int i = bins - 1; i >= 0; --i)
    {
        running += h.adf[static_cast<std::size_t>(i)];
        h.bcdf[static_cast<std::size_t>(i)] = running;
    }
    return h;
}
}

CxSurfaceBasicStats computeSurfaceBasicStats(const CxSurfaceField& field, int bins_primary)
{
    if (field.valueCount() <= 0)
        throw std::invalid_argument("computeSurfaceBasicStats requires a non-empty field.");

    CxSurfaceBasicStats s;
    s.min = std::numeric_limits<double>::infinity();
    s.max = -std::numeric_limits<double>::infinity();

    double sum = 0.0;
    for (int y = 0; y < field.yres(); ++y)
    {
        for (int x = 0; x < field.xres(); ++x)
        {
            const double z = field.at(x, y);
            s.min = std::min(s.min, z);
            s.max = std::max(s.max, z);
            sum += z;
        }
    }

    const double n = static_cast<double>(field.valueCount());
    s.mean = sum / n;

    double sum_abs = 0.0;
    double sum2 = 0.0;
    double sum3 = 0.0;
    double sum4 = 0.0;
    for (int y = 0; y < field.yres(); ++y)
    {
        for (int x = 0; x < field.xres(); ++x)
        {
            const double d = field.at(x, y) - s.mean;
            const double d2 = d * d;
            sum_abs += std::abs(d);
            sum2 += d2;
            sum3 += d2 * d;
            sum4 += d2 * d2;
        }
    }

    s.ra = sum_abs / n;
    s.rms = std::sqrt(sum2 / n);
    if (s.rms > 0.0)
    {
        const double sigma3 = s.rms * s.rms * s.rms;
        const double sigma4 = sigma3 * s.rms;
        s.skewness = (sum3 / n) / sigma3;
        s.kurtosis_excess = (sum4 / n) / sigma4 - 3.0;
    }

    s.height_distribution_primary = BuildDistribution(field, s.min, s.max, bins_primary);
    s.height_distribution_1024 = BuildDistribution(field, s.min, s.max, 1024);
    return s;
}

} // namespace cxvision::metrology_analytics

