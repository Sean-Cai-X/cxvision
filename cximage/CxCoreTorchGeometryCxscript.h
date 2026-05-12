#ifndef CXCORE_CORE_CXCORETORCHGEOMETRYCXSCRIPT_H
#define CXCORE_CORE_CXCORETORCHGEOMETRYCXSCRIPT_H

#include <string>

namespace cxcore {

inline const char* TorchGeometryCxscriptFragmentName_InputPriorRoiLinePointSet()
{
    return "feature.integration.torch_geometry_input_prior_roi_line_pointset";
}

inline const char* TorchGeometryCxscriptFragmentName_LabelMaskBoundaryKeypoints()
{
    return "feature.integration.torch_geometry_label_mask_boundary_keypoints";
}

inline const char* TorchGeometryCxscriptFragmentName_AttachBackToGeometry()
{
    return "feature.integration.torch_geometry_attach_back";
}

inline std::string TorchGeometryCxscript_InputPriorRoiLinePointSet()
{
    return
        "Image source;\n"
        "Shape roi;\n"
        "Findline line;\n"
        "Match matcher;\n"
        "\n"
        "roi.setname(\"roi_input_prior\");\n"
        "source.getshape(roi);\n"
        "line.shapesetroi(roi);\n"
        "matcher.shapesetroi(roi);\n"
        "\n"
        "cxcore.export.roi_object(source, roi, \"roi_main\");\n"
        "cxcore.export.line_object(line, \"line_main\");\n"
        "cxcore.export.pointset_object(line, \"pointset_line_samples\");\n"
        "\n"
        "cxcore.align.input_prior(\"roi_main\", \"line_main\");\n"
        "cxcore.align.input_prior(\"roi_main\", \"pointset_line_samples\");\n"
        "cxcore.build.torch_request(\"roi_main\", \"input_prior_bundle\");\n"
        "cxcore.attach.descriptor(\"input_prior_bundle\", \"line_main\", \"external_geometry_descriptor\");\n"
        "cxcore.attach.descriptor(\"input_prior_bundle\", \"pointset_line_samples\", \"external_shape_descriptor\");\n"
        "cxcore.route.to_torch(\"input_prior_bundle\");\n";
}

inline std::string TorchGeometryCxscript_LabelMaskBoundaryKeypoints()
{
    return
        "Image source;\n"
        "Image labelMask;\n"
        "Shape roi;\n"
        "Findline line;\n"
        "\n"
        "source.getshape(roi);\n"
        "labelMask.getshape(roi);\n"
        "line.shapesetroi(roi);\n"
        "\n"
        "cxcore.export.roi_object(source, roi, \"roi_train\");\n"
        "cxcore.export.mask_object(labelMask, roi, \"mask_gt\");\n"
        "cxcore.export.boundary_object(labelMask, roi, \"boundary_gt\");\n"
        "cxcore.export.keypoints_object(line, roi, \"keypoints_gt\");\n"
        "\n"
        "cxcore.align.training_label(\"roi_train\", \"mask_gt\");\n"
        "cxcore.align.training_label(\"roi_train\", \"boundary_gt\");\n"
        "cxcore.align.training_label(\"roi_train\", \"keypoints_gt\");\n"
        "cxcore.build.torch_label_packet(\"roi_train\", \"torch_label_packet\");\n"
        "cxcore.attach.label(\"torch_label_packet\", \"mask_gt\");\n"
        "cxcore.attach.label(\"torch_label_packet\", \"boundary_gt\");\n"
        "cxcore.attach.label(\"torch_label_packet\", \"keypoints_gt\");\n";
}

inline std::string TorchGeometryCxscript_AttachBackToGeometry()
{
    return
        "cxcore.load.roi_object(\"roi_main\");\n"
        "cxcore.load.mask_object(\"mask_pred\");\n"
        "cxcore.load.boundary_object(\"boundary_pred\");\n"
        "cxcore.load.keypoints_object(\"keypoints_pred\");\n"
        "\n"
        "cxcore.attach.result_to_roi(\"roi_main\", \"attach_cls_001\");\n"
        "cxcore.attach.result_label(\"attach_cls_001\", \"predicted_class\");\n"
        "cxcore.attach.result_score(\"attach_cls_001\", 0.95);\n"
        "cxcore.attach.result_embedding(\"attach_cls_001\", \"embedding_main\");\n"
        "\n"
        "cxcore.attach.result_mask(\"roi_main\", \"mask_pred\", \"attach_seg_001\");\n"
        "cxcore.attach.result_boundary(\"roi_main\", \"boundary_pred\", \"attach_seg_001\");\n"
        "cxcore.attach.result_keypoints(\"roi_main\", \"keypoints_pred\", \"attach_kp_001\");\n"
        "\n"
        "cxcore.publish.attach_record(\"attach_cls_001\");\n"
        "cxcore.publish.attach_record(\"attach_seg_001\");\n"
        "cxcore.publish.attach_record(\"attach_kp_001\");\n";
}

inline std::string TorchGeometryCxscript_MinimalEndToEndRoiMaskBoundary()
{
    return
        TorchGeometryCxscript_InputPriorRoiLinePointSet() +
        std::string("\n") +
        TorchGeometryCxscript_LabelMaskBoundaryKeypoints() +
        std::string("\n") +
        TorchGeometryCxscript_AttachBackToGeometry();
}

} // namespace cxcore

#endif
