#include "CxPrecisionEvaluation.h"
#include "CxScriptSuiteRunner.h"

namespace
{
constexpr double kFindCircleResidualLimitPx = 8.0;
constexpr double kFindLineResidualCandidateLimitPx = 6.0;
constexpr double kFindLineSupportCandidateLimit = 0.90;
}

CxPrecisionGateSnapshot EvaluateCxPrecisionGate(
    const CxScriptSuiteCaseResult& result)
{
    CxPrecisionGateSnapshot gate;

    if (result.tool == "FindCircle")
    {
        gate.metric_family = "residual_avgdist";
        gate.residual_px = result.avgdist;
        gate.residual_limit_px = kFindCircleResidualLimitPx;

        if (!result.headless_ok)
        {
            gate.status = "NOT_EVALUATED_HEADLESS_FAILED";
            gate.reason = "Headless execution did not complete.";
            return gate;
        }

        if (result.valid_points_count < 3)
        {
            gate.status = "NOT_EVALUATED_INSUFFICIENT_POINTS";
            gate.reason = "FindCircle residual gate requires at least 3 valid points.";
            return gate;
        }

        if (!result.has_fit_circle || result.circle_radius <= 0.0)
        {
            gate.status = "NOT_EVALUATED_NO_FIT_CIRCLE";
            gate.reason = "FindCircle residual gate requires a fitted circle.";
            return gate;
        }

        gate.evaluated = true;
        gate.residual_pass = result.avgdist <= kFindCircleResidualLimitPx;
        gate.status = gate.residual_pass
            ? "RESIDUAL_GATE_PASS"
            : "RESIDUAL_GATE_FAIL";
        gate.reason = gate.residual_pass
            ? "FindCircle avgdist is within the residual limit."
            : "FindCircle avgdist exceeds the residual limit.";
        return gate;
    }

    if (result.tool == "FindLine")
    {
        gate.metric_family = "line_residual_support";
        gate.residual_px = result.avgdist;
        gate.residual_limit_px = kFindLineResidualCandidateLimitPx;
        gate.support = result.local_support;
        gate.support_limit = kFindLineSupportCandidateLimit;

        if (!result.headless_ok)
        {
            gate.status = "NOT_EVALUATED_HEADLESS_FAILED";
            gate.reason = "Headless execution did not complete.";
            return gate;
        }

        if (result.valid_points_count < 2)
        {
            gate.status = "NOT_EVALUATED_INSUFFICIENT_POINTS";
            gate.reason = "FindLine precision gate requires at least 2 valid points.";
            return gate;
        }

        if (!result.has_fit_line)
        {
            gate.status = "NOT_EVALUATED_NO_FIT_LINE";
            gate.reason = "FindLine precision gate requires a fitted line.";
            return gate;
        }

        gate.evaluated = true;
        gate.status = "PENDING_PRECISION_LIMIT";
        gate.reason =
            "FindLine residual/support candidate limits are available, but not yet activated without human review.";
        return gate;
    }

    return gate;
}
