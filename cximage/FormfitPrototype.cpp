#include "pch.h"

#include "FormfitPrototype.h"

#include <memory>

namespace cxcore
{
namespace formfit
{

FitOperationConfig MakeRectCircleCompositeOperationConfig()
{
    FitOperationConfig config;
    config.config_id = "rect_circle_composite_config";
    config.operation_kind = FitOperationKind::SetStage;
    config.has_global_tolerance_scale = true;
    config.global_tolerance_scale = 1.0;
    config.has_global_weight_scale = true;
    config.global_weight_scale = 1.0;

    FitStageSetting coarse;
    coarse.stage_id = "coarse";
    coarse.has_method = true;
    coarse.method_type = FitMethodType::LeastSquares;
    coarse.has_order_index = true;
    coarse.order_index = 0;
    coarse.has_stage_weight = true;
    coarse.stage_weight = 0.75;
    coarse.has_tolerance_scale = true;
    coarse.tolerance_scale = 1.20;
    coarse.has_max_iterations = true;
    coarse.max_iterations = 16;
    config.stage_settings.push_back(coarse);

    FitStageSetting refine;
    refine.stage_id = "refine";
    refine.has_method = true;
    refine.method_type = FitMethodType::WeightedLeastSquares;
    refine.has_order_index = true;
    refine.order_index = 1;
    refine.has_stage_weight = true;
    refine.stage_weight = 1.00;
    refine.has_tolerance_scale = true;
    refine.tolerance_scale = 1.00;
    refine.has_max_iterations = true;
    refine.max_iterations = 32;
    config.stage_settings.push_back(refine);

    FitStageSetting final_stage;
    final_stage.stage_id = "final";
    final_stage.has_method = true;
    final_stage.method_type = FitMethodType::Constrained;
    final_stage.has_order_index = true;
    final_stage.order_index = 2;
    final_stage.has_stage_weight = true;
    final_stage.stage_weight = 1.25;
    final_stage.has_tolerance_scale = true;
    final_stage.tolerance_scale = 0.85;
    final_stage.has_max_iterations = true;
    final_stage.max_iterations = 24;
    config.stage_settings.push_back(final_stage);

    return config;
}

FitOperationConfig MakeRectCircleLineCompositeOperationConfig()
{
    FitOperationConfig config = MakeRectCircleCompositeOperationConfig();
    config.config_id = "rect_circle_line_composite_config";
    config.has_global_weight_scale = true;
    config.global_weight_scale = 1.1;
    return config;
}

FitOperationConfig MakeRectCircleLineMatchCompositeOperationConfig()
{
    FitOperationConfig config = MakeRectCircleLineCompositeOperationConfig();
    config.config_id = "rect_circle_line_match_composite_config";
    config.has_global_tolerance_scale = true;
    config.global_tolerance_scale = 0.9;
    config.has_global_weight_scale = true;
    config.global_weight_scale = 1.2;

    FitStageSetting refine_stage;
    refine_stage.stage_id = "refine";
    refine_stage.has_method = true;
    refine_stage.method_type = FitMethodType::Robust;
    refine_stage.has_tolerance_scale = true;
    refine_stage.tolerance_scale = 0.85;
    refine_stage.has_stage_weight = true;
    refine_stage.stage_weight = 1.15;
    config.stage_settings.push_back(refine_stage);

    FitStageSetting final_stage;
    final_stage.stage_id = "final";
    final_stage.has_method = true;
    final_stage.method_type = FitMethodType::Constrained;
    final_stage.has_max_iterations = true;
    final_stage.max_iterations = 48;
    final_stage.has_stage_weight = true;
    final_stage.stage_weight = 1.35;
    config.stage_settings.push_back(final_stage);

    FitResidualSetting match_center_distance;
    match_center_distance.residual_name = "rect_match_center_distance";
    match_center_distance.has_weight = true;
    match_center_distance.weight = 1.5;
    match_center_distance.has_tolerance = true;
    match_center_distance.tolerance = 3.5;
    config.residual_settings.push_back(match_center_distance);

    FitResidualSetting match_width;
    match_width.residual_name = "match_rect_width";
    match_width.has_weight = true;
    match_width.weight = 1.1;
    match_width.has_tolerance = true;
    match_width.tolerance = 3.0;
    config.residual_settings.push_back(match_width);

    FitResidualSetting match_height;
    match_height.residual_name = "match_rect_height";
    match_height.has_weight = true;
    match_height.weight = 1.1;
    match_height.has_tolerance = true;
    match_height.tolerance = 3.0;
    config.residual_settings.push_back(match_height);

    FitVariableSetting rect_rotation;
    rect_rotation.variable_name = "rect.rotation";
    rect_rotation.has_locked = true;
    rect_rotation.locked = true;
    rect_rotation.has_weight = true;
    rect_rotation.weight = 0.25;
    config.variable_settings.push_back(rect_rotation);

    FitVariableSetting circle_radius;
    circle_radius.variable_name = "circle.radius";
    circle_radius.has_weight = true;
    circle_radius.weight = 1.4;
    config.variable_settings.push_back(circle_radius);

    return config;
}

PrototypeRunResult RunRectCirclePrototype(const OutputRect& rect,
                                          const CircleMeasurementOutput& circle,
                                          const FitOperationConfig& operation_config)
{
    PrototypeRunResult result;
    result.gauge = MakeRectCircleGauge(rect, circle, "rect_circle_gauge", "rect_circle_gauge");
    result.task = MakeTaskSpecFromGauge(result.gauge, "rect_circle_task", FitTaskType::GaugeFit);
    result.problem = MakeFitProblemFromGauge(result.gauge, "rect_circle_problem");
    result.configured_problem = result.problem;
    result.method = MakeFitMethodDescriptor(
        "rect_circle_composite",
        FitMethodCategory::CompositeFamily,
        FitMethodType::Constrained);
    result.operation_config = operation_config;
    FitOperationController::Apply(result.operation_config, result.configured_problem);

    std::shared_ptr<IOptimizerBackend> backend = std::make_shared<PassthroughOptimizerBackend>();
    std::shared_ptr<ICompositeFitter> fitter = std::make_shared<CompositeFitter>(backend);
    DefaultMethodExecutor executor(fitter);
    result.method_result = executor.Execute(result.task, result.method, result.operation_config, result.problem);
    result.evaluation_score = EvaluateFitTask(result.task, result.method_result);
    return result;
}

PrototypeRunResult RunRectCircleLinePrototype(const OutputRect& rect,
                                              const CircleMeasurementOutput& circle,
                                              const LineMeasurementOutput& line,
                                              const FitOperationConfig& operation_config)
{
    PrototypeRunResult result;
    result.gauge = MakeRectCircleLineGauge(rect, circle, line, "rect_circle_line_gauge", "rect_circle_line_gauge");
    result.task = MakeTaskSpecFromGauge(result.gauge, "rect_circle_line_task", FitTaskType::StructureFit);
    result.problem = MakeFitProblemFromGauge(result.gauge, "rect_circle_line_problem");
    result.configured_problem = result.problem;
    result.method = MakeFitMethodDescriptor(
        "rect_circle_line_composite",
        FitMethodCategory::CompositeFamily,
        FitMethodType::Constrained);
    result.operation_config = operation_config;
    FitOperationController::Apply(result.operation_config, result.configured_problem);

    std::shared_ptr<IOptimizerBackend> backend = std::make_shared<PassthroughOptimizerBackend>();
    std::shared_ptr<ICompositeFitter> fitter = std::make_shared<CompositeFitter>(backend);
    DefaultMethodExecutor executor(fitter);
    result.method_result = executor.Execute(result.task, result.method, result.operation_config, result.problem);
    result.evaluation_score = EvaluateFitTask(result.task, result.method_result);
    return result;
}

PrototypeRunResult RunRectCircleLineMatchPrototype(const OutputRect& rect,
                                                   const CircleMeasurementOutput& circle,
                                                   const LineMeasurementOutput& line,
                                                   const MatchOutput& match,
                                                   const FitOperationConfig& operation_config)
{
    auto run_single = [&](const MatchOutput& single_match, int candidate_index) -> PrototypeRunResult
    {
        PrototypeRunResult result;
        result.gauge = MakeRectCircleLineMatchGauge(rect, circle, line, single_match, "rect_circle_line_match_gauge", "rect_circle_line_match_gauge");
        result.task = MakeTaskSpecFromGauge(result.gauge, "rect_circle_line_match_task", FitTaskType::ComparativeFit);
        result.problem = MakeFitProblemFromGauge(result.gauge, "rect_circle_line_match_problem");
        result.configured_problem = result.problem;
        result.method = MakeFitMethodDescriptor(
            "rect_circle_line_match_composite",
            FitMethodCategory::CompositeFamily,
            FitMethodType::Constrained);
        result.operation_config = operation_config;
        FitOperationController::Apply(result.operation_config, result.configured_problem);

        std::shared_ptr<IOptimizerBackend> backend = std::make_shared<PassthroughOptimizerBackend>();
        std::shared_ptr<ICompositeFitter> fitter = std::make_shared<CompositeFitter>(backend);
        DefaultMethodExecutor executor(fitter);
        result.method_result = executor.Execute(result.task, result.method, result.operation_config, result.problem);
        result.evaluation_score = EvaluateFitTask(result.task, result.method_result);
        result.selected_match_candidate_index = candidate_index;
        result.evaluated_match_candidate_count = 1;
        return result;
    };

    if (match.candidates.empty())
    {
        return run_single(match, -1);
    }

    PrototypeRunResult best_result;
    bool has_best = false;
    int evaluated_count = 0;
    std::vector<PrototypeCandidateScore> candidate_scores;
    for (size_t i = 0; i < match.candidates.size(); ++i)
    {
        MatchOutput single_match = match;
        single_match.candidates = { match.candidates[i] };
        PrototypeRunResult current = run_single(single_match, static_cast<int>(i));
        ++evaluated_count;
        current.evaluated_match_candidate_count = evaluated_count;
        PrototypeCandidateScore score_entry;
        score_entry.candidate_index = static_cast<int>(i);
        score_entry.evaluation_score = current.evaluation_score;
        score_entry.valid = current.method_result.valid;
        candidate_scores.push_back(score_entry);
        if (!has_best || current.evaluation_score < best_result.evaluation_score)
        {
            best_result = current;
            has_best = true;
        }
    }
    if (has_best)
    {
        best_result.evaluated_match_candidate_count = evaluated_count;
        best_result.candidate_scores = candidate_scores;
        return best_result;
    }
    return run_single(match, -1);
}

} // namespace formfit
} // namespace cxcore
