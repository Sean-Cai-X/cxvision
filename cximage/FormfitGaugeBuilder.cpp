#include "pch.h"

#include "FormfitGaugeBuilder.h"

#include "CxCoreBoundary.h"

#include <cmath>

namespace cxcore
{
namespace formfit
{

OutputRect GaugeBuilder::MakeRectOutput(const FindRect& rect, int index)
{
    OutputRect output;
    const gp_Rectangle result = rect.getresultrect(index);
    output.x = result.TopLeft().X();
    output.y = result.TopLeft().Y();
    output.width = result.Width();
    output.height = result.Height();
    return output;
}

LineMeasurementOutput GaugeBuilder::MakeLineOutput(Findline& line)
{
    return ExportLineMeasurement(line);
}

CircleMeasurementOutput GaugeBuilder::MakeCircleOutput(Findcircle& circle)
{
    return ExportCircleMeasurement(circle);
}

MatchOutput GaugeBuilder::MakeMatchOutput(fastmatch& matcher, int max_candidates)
{
    MatchOutput output = ExportMatchOutput(matcher, max_candidates);
    std::vector<MatchCandidateOutput> filtered;
    filtered.reserve(output.candidates.size());
    for (const MatchCandidateOutput& candidate : output.candidates)
    {
        const bool valid_bounds = candidate.bounds.width > 0.0 && candidate.bounds.height > 0.0;
        const bool valid_center = std::isfinite(candidate.center.x) && std::isfinite(candidate.center.y);
        const bool valid_score = candidate.score > 0.0;
        if (valid_bounds && valid_center && valid_score)
        {
            filtered.push_back(candidate);
        }
    }
    output.candidates = std::move(filtered);
    return output;
}

FitOperationConfig GaugeBuilder::MakeRectCircleCompositeConfig()
{
    return MakeRectCircleCompositeOperationConfig();
}

FitOperationConfig GaugeBuilder::MakeRectCircleLineCompositeConfig()
{
    return MakeRectCircleLineCompositeOperationConfig();
}

FitOperationConfig GaugeBuilder::MakeRectCircleLineMatchCompositeConfig()
{
    return MakeRectCircleLineMatchCompositeOperationConfig();
}

PrototypeRunResult GaugeBuilder::BuildAndRunRectCirclePrototype(const FindRect& rect,
                                                                Findcircle& circle,
                                                                const FitOperationConfig& operation_config,
                                                                int rect_index)
{
    const OutputRect rect_output = MakeRectOutput(rect, rect_index);
    CircleMeasurementOutput circle_output = MakeCircleOutput(circle);
    return RunRectCirclePrototype(rect_output, circle_output, operation_config);
}

PrototypeRunResult GaugeBuilder::BuildAndRunRectCircleLinePrototype(const FindRect& rect,
                                                                    Findline& line,
                                                                    Findcircle& circle,
                                                                    const FitOperationConfig& operation_config,
                                                                    int rect_index)
{
    const OutputRect rect_output = MakeRectOutput(rect, rect_index);
    LineMeasurementOutput line_output = MakeLineOutput(line);
    CircleMeasurementOutput circle_output = MakeCircleOutput(circle);
    return RunRectCircleLinePrototype(rect_output, circle_output, line_output, operation_config);
}

PrototypeRunResult GaugeBuilder::BuildAndRunRectCircleLineMatchPrototype(const FindRect& rect,
                                                                         Findline& line,
                                                                         Findcircle& circle,
                                                                         fastmatch& matcher,
                                                                         const FitOperationConfig& operation_config,
                                                                         int rect_index,
                                                                         int max_candidates)
{
    const OutputRect rect_output = MakeRectOutput(rect, rect_index);
    LineMeasurementOutput line_output = MakeLineOutput(line);
    CircleMeasurementOutput circle_output = MakeCircleOutput(circle);
    MatchOutput match_output = MakeMatchOutput(matcher, max_candidates);
    return RunRectCircleLineMatchPrototype(rect_output, circle_output, line_output, match_output, operation_config);
}

} // namespace formfit
} // namespace cxcore
