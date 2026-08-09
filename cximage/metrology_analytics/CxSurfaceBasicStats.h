#pragma once

#include "metrology_analytics/CxSurfaceField.h"

#include <vector>

namespace cxvision::metrology_analytics
{

struct CxHeightDistribution
{
    std::vector<double> bin_centers;
    std::vector<double> adf;
    std::vector<double> bcdf;
};

struct CxSurfaceBasicStats
{
    double min = 0.0;
    double max = 0.0;
    double mean = 0.0;
    double rms = 0.0;
    double ra = 0.0;
    double skewness = 0.0;
    double kurtosis_excess = 0.0;
    CxHeightDistribution height_distribution_primary;
    CxHeightDistribution height_distribution_1024;
};

CxSurfaceBasicStats computeSurfaceBasicStats(const CxSurfaceField& field, int bins_primary = 256);

} // namespace cxvision::metrology_analytics

