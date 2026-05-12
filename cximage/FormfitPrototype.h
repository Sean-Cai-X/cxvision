#ifndef CXCORE_CORE_FORMFITPROTOTYPE_H
#define CXCORE_CORE_FORMFITPROTOTYPE_H

#include "CxCoreBoundary.h"
#include "FormfitGauge.h"

namespace cxcore
{
namespace formfit
{

struct PrototypeCandidateScore
{
    int candidate_index = -1;
    double evaluation_score = 0.0;
    bool valid = false;
};

struct PrototypeRunResult
{
    FormfitGauge gauge;
    FitTaskSpec task;
    FitProblem problem;
    FitProblem configured_problem;
    FitMethodDescriptor method;
    FitOperationConfig operation_config;
    FitMethodResult method_result;
    double evaluation_score = 0.0;
    int selected_match_candidate_index = -1;
    int evaluated_match_candidate_count = 0;
    std::vector<PrototypeCandidateScore> candidate_scores;
};

FitOperationConfig MakeRectCircleCompositeOperationConfig();
FitOperationConfig MakeRectCircleLineCompositeOperationConfig();
FitOperationConfig MakeRectCircleLineMatchCompositeOperationConfig();

PrototypeRunResult RunRectCirclePrototype(const OutputRect& rect,
                                          const CircleMeasurementOutput& circle,
                                          const FitOperationConfig& operation_config);
PrototypeRunResult RunRectCircleLinePrototype(const OutputRect& rect,
                                              const CircleMeasurementOutput& circle,
                                              const LineMeasurementOutput& line,
                                              const FitOperationConfig& operation_config);
PrototypeRunResult RunRectCircleLineMatchPrototype(const OutputRect& rect,
                                                   const CircleMeasurementOutput& circle,
                                                   const LineMeasurementOutput& line,
                                                   const MatchOutput& match,
                                                   const FitOperationConfig& operation_config);

} // namespace formfit
} // namespace cxcore

#endif
