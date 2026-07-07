#ifndef CXCORE_CORE_FORMFITGAUGEBUILDER_H
#define CXCORE_CORE_FORMFITGAUGEBUILDER_H

#include "FastMatch.h"
#include "Findline.h"
#include "FindRect.h"
#include "Findcircle.h"
#include "FormfitPrototype.h"

namespace cxcore
{
namespace formfit
{

class GaugeBuilder
{
public:
    static OutputRect MakeRectOutput(const FindRect& rect, int index);
    static LineMeasurementOutput MakeLineOutput(Findline& line);
    static CircleMeasurementOutput MakeCircleOutput(Findcircle& circle);
    static MatchOutput MakeMatchOutput(fastmatch& matcher, int max_candidates = 1);
    static FitOperationConfig MakeRectCircleCompositeConfig();
    static FitOperationConfig MakeRectCircleLineCompositeConfig();
    static FitOperationConfig MakeRectCircleLineMatchCompositeConfig();
    static FitOperationConfig MakeCircleRingCompositeConfig();
    static PrototypeRunResult BuildAndRunRectCirclePrototype(const FindRect& rect,
                                                             Findcircle& circle,
                                                             const FitOperationConfig& operation_config,
                                                             int rect_index = 0);
    static PrototypeRunResult BuildAndRunRectCircleLinePrototype(const FindRect& rect,
                                                                 Findline& line,
                                                                 Findcircle& circle,
                                                                 const FitOperationConfig& operation_config,
                                                                 int rect_index = 0);
    static PrototypeRunResult BuildAndRunRectCircleLineMatchPrototype(const FindRect& rect,
                                                                      Findline& line,
                                                                      Findcircle& circle,
                                                                      fastmatch& matcher,
                                                                      const FitOperationConfig& operation_config,
                                                                      int rect_index = 0,
                                                                      int max_candidates = 1);
    static PrototypeRunResult BuildAndRunCircleRingPrototype(Findcircle& outer_circle,
                                                             Findcircle& inner_circle,
                                                             const FitOperationConfig& operation_config,
                                                             double center_tolerance = 3.0,
                                                             double thickness_tolerance = 5.0);
};

} // namespace formfit
} // namespace cxcore

#endif
