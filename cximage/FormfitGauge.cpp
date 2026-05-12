#include "pch.h"

#include "FormfitGauge.h"

#include <cmath>

namespace cxcore
{
namespace formfit
{

namespace
{
double ClampPositive(double value, double fallback)
{
    return value > 0.0 ? value : fallback;
}

double EstimateLineAngleDegrees(const LineMeasurementOutput& line)
{
    if (line.measure_bounds.width >= line.measure_bounds.height)
    {
        return 0.0;
    }
    return 90.0;
}

double EstimateLineLength(const LineMeasurementOutput& line)
{
    return std::max(ClampPositive(line.measure_bounds.width, 0.0),
                    ClampPositive(line.measure_bounds.height, 0.0));
}

ResidualType RelationResidualType(GaugeRelationType relation_type)
{
    (void)relation_type;
    return ResidualType::Relation;
}

ResidualType ConstraintResidualType(GaugeConstraintType constraint_type)
{
    (void)constraint_type;
    return ResidualType::Constraint;
}

FitVariable MakeVariable(const char* name,
                         double value,
                         double lower_bound,
                         double upper_bound,
                         double weight = 1.0,
                         bool locked = false)
{
    FitVariable variable;
    variable.name = name ? name : "";
    variable.value = value;
    variable.lower_bound = lower_bound;
    variable.upper_bound = upper_bound;
    variable.weight = weight;
    variable.locked = locked;
    return variable;
}
}

FormfitGauge MakeGauge(const char* gauge_id, const char* name)
{
    FormfitGauge gauge;
    gauge.gauge_id = gauge_id ? gauge_id : "";
    gauge.name = name ? name : "";
    gauge.overall_tolerance = 1.0;
    gauge.overall_weight = 1.0;
    gauge.learn_score = 0.0;
    return gauge;
}

GaugeElement MakeRectGaugeElement(const OutputRect& rect,
                                  const char* element_id,
                                  const char* source_entity_id,
                                  double confidence)
{
    GaugeElement element;
    element.element_id = element_id ? element_id : "rect";
    element.element_type = GaugeElementType::Rect;
    element.source_entity_id = source_entity_id ? source_entity_id : "";
    element.confidence = confidence;
    element.bbox_x = rect.x;
    element.bbox_y = rect.y;
    element.bbox_width = rect.width;
    element.bbox_height = rect.height;
    element.variables.push_back(MakeVariable("rect.center_x", rect.x + rect.width * 0.5, rect.x - rect.width, rect.x + rect.width * 2.0));
    element.variables.push_back(MakeVariable("rect.center_y", rect.y + rect.height * 0.5, rect.y - rect.height, rect.y + rect.height * 2.0));
    element.variables.push_back(MakeVariable("rect.width", rect.width, 1.0, std::max(1.0, rect.width * 3.0)));
    element.variables.push_back(MakeVariable("rect.height", rect.height, 1.0, std::max(1.0, rect.height * 3.0)));
    element.variables.push_back(MakeVariable("rect.rotation", 0.0, -180.0, 180.0, 0.5));
    return element;
}

GaugeElement MakeLineGaugeElement(const LineMeasurementOutput& line,
                                  const char* element_id,
                                  const char* source_entity_id,
                                  double confidence)
{
    GaugeElement element;
    element.element_id = element_id ? element_id : "line";
    element.element_type = GaugeElementType::Line;
    element.source_entity_id = source_entity_id ? source_entity_id : "";
    element.confidence = confidence;
    element.bbox_x = line.measure_bounds.x;
    element.bbox_y = line.measure_bounds.y;
    element.bbox_width = line.measure_bounds.width;
    element.bbox_height = line.measure_bounds.height;

    const double center_x = line.measure_bounds.x + line.measure_bounds.width * 0.5;
    const double center_y = line.measure_bounds.y + line.measure_bounds.height * 0.5;
    const double length = ClampPositive(EstimateLineLength(line), 1.0);
    const double angle = EstimateLineAngleDegrees(line);

    element.variables.push_back(MakeVariable("line.center_x", center_x, center_x - length, center_x + length));
    element.variables.push_back(MakeVariable("line.center_y", center_y, center_y - length, center_y + length));
    element.variables.push_back(MakeVariable("line.length", length, 1.0, std::max(1.0, length * 3.0)));
    element.variables.push_back(MakeVariable("line.angle", angle, -180.0, 180.0, 0.75));
    return element;
}

GaugeElement MakeCircleGaugeElement(const CircleMeasurementOutput& circle,
                                    const char* element_id,
                                    const char* source_entity_id,
                                    double confidence)
{
    GaugeElement element;
    element.element_id = element_id ? element_id : "circle";
    element.element_type = GaugeElementType::Circle;
    element.source_entity_id = source_entity_id ? source_entity_id : "";
    element.confidence = confidence;
    element.bbox_x = circle.measure_bounds.x;
    element.bbox_y = circle.measure_bounds.y;
    element.bbox_width = circle.measure_bounds.width;
    element.bbox_height = circle.measure_bounds.height;
    element.variables.push_back(MakeVariable("circle.center_x", circle.center.x, circle.center.x - circle.radius * 2.0, circle.center.x + circle.radius * 2.0));
    element.variables.push_back(MakeVariable("circle.center_y", circle.center.y, circle.center.y - circle.radius * 2.0, circle.center.y + circle.radius * 2.0));
    element.variables.push_back(MakeVariable("circle.radius", circle.radius, 1.0, std::max(1.0, circle.radius * 3.0)));
    return element;
}

GaugeElement MakeMatchRectGaugeElement(const MatchCandidateOutput& match_candidate,
                                       const char* element_id,
                                       const char* source_entity_id,
                                       double confidence)
{
    OutputRect rect;
    rect.x = match_candidate.bounds.x;
    rect.y = match_candidate.bounds.y;
    rect.width = match_candidate.bounds.width;
    rect.height = match_candidate.bounds.height;
    GaugeElement element = MakeRectGaugeElement(rect, element_id ? element_id : "match_rect", source_entity_id, confidence);
    return element;
}

FormfitGauge MakeRectCircleGauge(const OutputRect& rect,
                                 const CircleMeasurementOutput& circle,
                                 const char* gauge_id,
                                 const char* name)
{
    FormfitGauge gauge = MakeGauge(gauge_id, name);
    gauge.elements.push_back(MakeRectGaugeElement(rect, "rect_0", "rect", 1.0));
    gauge.elements.push_back(MakeCircleGaugeElement(circle, "circle_0", "circle", circle.has_direct_fit ? 1.0 : 0.75));

    GaugeRelation concentric_like;
    concentric_like.relation_id = "rect_circle_center_distance";
    concentric_like.lhs_element_id = "rect_0";
    concentric_like.rhs_element_id = "circle_0";
    concentric_like.relation_type = GaugeRelationType::Distance;
    concentric_like.target_value = 0.0;
    concentric_like.tolerance = std::max(1.0, circle.radius * 0.5);
    concentric_like.weight = 1.5;
    gauge.relations.push_back(concentric_like);

    GaugeConstraint rect_aspect;
    rect_aspect.constraint_id = "rect_aspect";
    rect_aspect.target_element_id = "rect_0";
    rect_aspect.constraint_type = GaugeConstraintType::AspectRatio;
    rect_aspect.target_value = rect.height != 0.0 ? rect.width / rect.height : 0.0;
    rect_aspect.tolerance = 0.25;
    rect_aspect.weight = 1.0;
    gauge.constraints.push_back(rect_aspect);

    GaugeConstraint circle_radius;
    circle_radius.constraint_id = "circle_radius";
    circle_radius.target_element_id = "circle_0";
    circle_radius.constraint_type = GaugeConstraintType::Radius;
    circle_radius.target_value = circle.radius;
    circle_radius.tolerance = std::max(1.0, circle.radius * 0.15);
    circle_radius.weight = 1.2;
    gauge.constraints.push_back(circle_radius);

    gauge.learn_score = circle.has_direct_fit ? 1.0 : 0.8;
    return gauge;
}

FormfitGauge MakeRectCircleLineGauge(const OutputRect& rect,
                                     const CircleMeasurementOutput& circle,
                                     const LineMeasurementOutput& line,
                                     const char* gauge_id,
                                     const char* name)
{
    FormfitGauge gauge = MakeRectCircleGauge(rect, circle, gauge_id, name);
    gauge.elements.push_back(MakeLineGaugeElement(line, "line_0", "line", 1.0));

    const double rect_center_x = rect.x + rect.width * 0.5;
    const double rect_center_y = rect.y + rect.height * 0.5;
    const double line_center_x = line.measure_bounds.x + line.measure_bounds.width * 0.5;
    const double line_center_y = line.measure_bounds.y + line.measure_bounds.height * 0.5;
    const double line_center_distance = std::hypot(rect_center_x - line_center_x, rect_center_y - line_center_y);

    GaugeRelation rect_line_distance;
    rect_line_distance.relation_id = "rect_line_center_distance";
    rect_line_distance.lhs_element_id = "rect_0";
    rect_line_distance.rhs_element_id = "line_0";
    rect_line_distance.relation_type = GaugeRelationType::Distance;
    rect_line_distance.target_value = line_center_distance;
    rect_line_distance.tolerance = std::max(2.0, std::max(rect.width, rect.height) * 0.2);
    rect_line_distance.weight = 1.0;
    gauge.relations.push_back(rect_line_distance);

    GaugeConstraint line_length;
    line_length.constraint_id = "line_length";
    line_length.target_element_id = "line_0";
    line_length.constraint_type = GaugeConstraintType::Length;
    line_length.target_value = EstimateLineLength(line);
    line_length.tolerance = std::max(2.0, line_length.target_value * 0.2);
    line_length.weight = 1.0;
    gauge.constraints.push_back(line_length);

    gauge.learn_score = (gauge.learn_score + 1.0) * 0.5;
    return gauge;
}

FormfitGauge MakeRectCircleLineMatchGauge(const OutputRect& rect,
                                          const CircleMeasurementOutput& circle,
                                          const LineMeasurementOutput& line,
                                          const MatchOutput& match,
                                          const char* gauge_id,
                                          const char* name)
{
    FormfitGauge gauge = MakeRectCircleLineGauge(rect, circle, line, gauge_id, name);
    if (match.candidates.empty())
    {
        return gauge;
    }

    const MatchCandidateOutput& best = match.candidates.front();
    const double match_confidence = std::max(0.25, best.score);
    gauge.elements.push_back(MakeMatchRectGaugeElement(best, "match_rect_0", "fastmatch", match_confidence));

    const double rect_center_x = rect.x + rect.width * 0.5;
    const double rect_center_y = rect.y + rect.height * 0.5;
    const double match_center_distance = std::hypot(rect_center_x - best.center.x, rect_center_y - best.center.y);

    GaugeRelation rect_match_distance;
    rect_match_distance.relation_id = "rect_match_center_distance";
    rect_match_distance.lhs_element_id = "rect_0";
    rect_match_distance.rhs_element_id = "match_rect_0";
    rect_match_distance.relation_type = GaugeRelationType::Distance;
    rect_match_distance.target_value = match_center_distance;
    rect_match_distance.tolerance = std::max(2.0, std::max(best.bounds.width, best.bounds.height) * 0.2);
    rect_match_distance.weight = 1.25;
    gauge.relations.push_back(rect_match_distance);

    GaugeConstraint match_width;
    match_width.constraint_id = "match_rect_width";
    match_width.target_element_id = "match_rect_0";
    match_width.constraint_type = GaugeConstraintType::Length;
    match_width.target_value = best.bounds.width;
    match_width.tolerance = std::max(2.0, best.bounds.width * 0.15);
    match_width.weight = 0.8;
    gauge.constraints.push_back(match_width);

    GaugeConstraint match_height;
    match_height.constraint_id = "match_rect_height";
    match_height.target_element_id = "match_rect_0";
    match_height.constraint_type = GaugeConstraintType::Length;
    match_height.target_value = best.bounds.height;
    match_height.tolerance = std::max(2.0, best.bounds.height * 0.15);
    match_height.weight = 0.8;
    gauge.constraints.push_back(match_height);

    gauge.learn_score = (gauge.learn_score + match_confidence) * 0.5;
    return gauge;
}

FitTaskSpec MakeTaskSpecFromGauge(const FormfitGauge& gauge, const char* task_id, FitTaskType task_type)
{
    FitTaskSpec task = MakeFitTaskSpec(task_id ? task_id : gauge.gauge_id.c_str(), task_type);
    task.gauge_ids.push_back(gauge.gauge_id);

    for (const GaugeElement& element : gauge.elements)
    {
        task.input_element_ids.push_back(element.element_id);
        task.output_target_ids.push_back(element.source_entity_id);
    }

    for (const GaugeConstraint& constraint : gauge.constraints)
    {
        task.constraint_ids.push_back(constraint.constraint_id);

        FitResidual residual;
        residual.name = constraint.constraint_id;
        residual.residual_type = ConstraintResidualType(constraint.constraint_type);
        residual.value = constraint.target_value;
        residual.weight = constraint.weight;
        residual.tolerance = constraint.tolerance;
        task.reference_residuals.push_back(residual);
    }

    for (const GaugeRelation& relation : gauge.relations)
    {
        task.relation_ids.push_back(relation.relation_id);
    }

    task.preferred_stage_ids.push_back("coarse");
    task.preferred_stage_ids.push_back("refine");
    task.preferred_stage_ids.push_back("final");

    return task;
}

FitProblem MakeFitProblemFromGauge(const FormfitGauge& gauge, const char* problem_id)
{
    FitProblem problem;
    problem.problem_id = problem_id ? problem_id : gauge.gauge_id;
    problem.global_tolerance_scale = gauge.overall_tolerance > 0.0 ? gauge.overall_tolerance : 1.0;
    problem.global_weight_scale = gauge.overall_weight > 0.0 ? gauge.overall_weight : 1.0;

    for (const GaugeElement& element : gauge.elements)
    {
        problem.element_ids.push_back(element.element_id);
        for (const FitVariable& variable : element.variables)
        {
            problem.variables.push_back(variable);
        }
    }

    for (const GaugeRelation& relation : gauge.relations)
    {
        problem.relation_ids.push_back(relation.relation_id);
        FitResidual residual;
        residual.name = relation.relation_id;
        residual.residual_type = RelationResidualType(relation.relation_type);
        residual.value = relation.target_value;
        residual.weight = relation.weight;
        residual.tolerance = relation.tolerance;
        problem.residuals.push_back(residual);
    }

    for (const GaugeConstraint& constraint : gauge.constraints)
    {
        problem.constraint_ids.push_back(constraint.constraint_id);
        FitResidual residual;
        residual.name = constraint.constraint_id;
        residual.residual_type = ConstraintResidualType(constraint.constraint_type);
        residual.value = constraint.target_value;
        residual.weight = constraint.weight;
        residual.tolerance = constraint.tolerance;
        problem.residuals.push_back(residual);
    }

    FitStage coarse = MakeFitStage("coarse", "coarse", FitMethodType::LeastSquares, 16);
    FitStage refine = MakeFitStage("refine", "refine", FitMethodType::WeightedLeastSquares, 32);
    FitStage final_stage = MakeFitStage("final", "final", FitMethodType::Constrained, 24);
    coarse.order_index = 0;
    coarse.stage_weight = 0.75;
    refine.order_index = 1;
    refine.stage_weight = 1.0;
    final_stage.order_index = 2;
    final_stage.stage_weight = 1.25;
    final_stage.tolerance_scale = 0.85;

    for (const GaugeElement& element : gauge.elements)
    {
        coarse.active_element_ids.push_back(element.element_id);
        refine.active_element_ids.push_back(element.element_id);
        final_stage.active_element_ids.push_back(element.element_id);
    }
    for (const GaugeRelation& relation : gauge.relations)
    {
        refine.active_relation_ids.push_back(relation.relation_id);
        final_stage.active_relation_ids.push_back(relation.relation_id);
    }
    for (const GaugeConstraint& constraint : gauge.constraints)
    {
        final_stage.active_constraint_ids.push_back(constraint.constraint_id);
    }

    problem.stages.push_back(coarse);
    problem.stages.push_back(refine);
    problem.stages.push_back(final_stage);
    return problem;
}

const char* GaugeElementTypeName(GaugeElementType element_type)
{
    switch (element_type)
    {
    case GaugeElementType::Line:
        return "line";
    case GaugeElementType::Circle:
        return "circle";
    case GaugeElementType::Rect:
        return "rect";
    case GaugeElementType::Arc:
        return "arc";
    case GaugeElementType::PointSet:
        return "pointset";
    case GaugeElementType::Unknown:
    default:
        return "unknown";
    }
}

const char* GaugeRelationTypeName(GaugeRelationType relation_type)
{
    switch (relation_type)
    {
    case GaugeRelationType::Parallel:
        return "parallel";
    case GaugeRelationType::Orthogonal:
        return "orthogonal";
    case GaugeRelationType::Tangent:
        return "tangent";
    case GaugeRelationType::Concentric:
        return "concentric";
    case GaugeRelationType::Inside:
        return "inside";
    case GaugeRelationType::Adjacent:
        return "adjacent";
    case GaugeRelationType::Distance:
        return "distance";
    case GaugeRelationType::Unknown:
    default:
        return "unknown";
    }
}

const char* GaugeConstraintTypeName(GaugeConstraintType constraint_type)
{
    switch (constraint_type)
    {
    case GaugeConstraintType::Length:
        return "length";
    case GaugeConstraintType::Radius:
        return "radius";
    case GaugeConstraintType::Angle:
        return "angle";
    case GaugeConstraintType::AspectRatio:
        return "aspect_ratio";
    case GaugeConstraintType::Closure:
        return "closure";
    case GaugeConstraintType::Symmetry:
        return "symmetry";
    case GaugeConstraintType::Unknown:
    default:
        return "unknown";
    }
}

} // namespace formfit
} // namespace cxcore
