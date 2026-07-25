#include "CxTorchResultProjector.h"

bool CxTorchResultProjector::Project(
    const CxInferenceResult& inference_result,
    const std::string& owner_type,
    const std::string& owner_ref,
    std::vector<CxShapeElementSnapshot>& shapes)
{
    shapes.clear();

    if (!inference_result.executed)
        return false;

    if (!inference_result.ok)
        return false;

    ProjectDetections(inference_result.detections, owner_type, owner_ref, shapes);

    if (inference_result.mask.has_value())
        ProjectMask(inference_result.mask.value(), owner_type, owner_ref, shapes);

    return !shapes.empty();
}

void CxTorchResultProjector::ProjectDetections(
    const std::vector<CxTorchDetection>& detections,
    const std::string& owner_type,
    const std::string& owner_ref,
    std::vector<CxShapeElementSnapshot>& shapes)
{
    for (std::size_t index = 0; index < detections.size(); ++index)
    {
        const auto& detection = detections[index];

        CxShapeElementSnapshot box;

        box.shape_kind = "rect";
        box.semantic_role = (index == 0) ? "model_best_result" : "model_candidate";
        box.owner_type = owner_type;
        box.owner_ref = owner_ref;
        box.result_element = true;
        box.editable = false;

        box.center_x = detection.x + detection.width * 0.5;
        box.center_y = detection.y + detection.height * 0.5;

        box.points.push_back(detection.x);
        box.points.push_back(detection.y);
        box.points.push_back(detection.x + detection.width);
        box.points.push_back(detection.y);
        box.points.push_back(detection.x + detection.width);
        box.points.push_back(detection.y + detection.height);
        box.points.push_back(detection.x);
        box.points.push_back(detection.y + detection.height);
        box.closed = true;

        shapes.push_back(std::move(box));
    }
}

void CxTorchResultProjector::ProjectMask(
    const CxTorchMask& mask,
    const std::string& owner_type,
    const std::string& owner_ref,
    std::vector<CxShapeElementSnapshot>& shapes)
{
    if (!mask.available)
        return;

    CxShapeElementSnapshot mask_element;
    mask_element.shape_kind = "mask";
    mask_element.semantic_role = "model_segmentation_mask";
    mask_element.owner_type = owner_type;
    mask_element.owner_ref = owner_ref;
    mask_element.result_element = true;
    mask_element.editable = false;

    shapes.push_back(std::move(mask_element));

    if (!mask.contour_ref.empty())
    {
        CxShapeElementSnapshot contour_element;
        contour_element.shape_kind = "polyline";
        contour_element.semantic_role = "model_segmentation_contour";
        contour_element.owner_type = owner_type;
        contour_element.owner_ref = owner_ref;
        contour_element.result_element = true;
        contour_element.editable = false;

        shapes.push_back(std::move(contour_element));
    }
}
