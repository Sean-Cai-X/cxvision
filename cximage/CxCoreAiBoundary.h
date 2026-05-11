#ifndef CXCORE_CORE_CXCOREAIBOUNDARY_H
#define CXCORE_CORE_CXCOREAIBOUNDARY_H

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "CxCoreBoundary.h"

namespace cxcore {

enum class AiTaskKind
{
    Unknown = 0,
    DirectMeasurement,
    NumericClassification,
    NumericClustering,
    GeometryMatching,
    GeometryClustering,
    RegionClassification,
    RegionDetectionRefinement,
    DenseSegmentation,
    VisualEmbedding
};

enum class AiRoute
{
    StayInCxcore = 0,
    RouteToMlpack,
    UpgradeToTorchModule,
    ManualReview
};

enum class AiExecutionMode
{
    Infer = 0,
    Train
};

struct TensorLayout
{
    int width = 0;
    int height = 0;
    int channels = 0;
    int batch = 0;
};

struct TensorBufferView
{
    TensorLayout layout;
    std::vector<float> values;
    bool normalized = false;
};

struct RoiInput
{
    OutputRect bounds;
    float confidence = 0.0f;
    int label_hint = -1;
};

struct FeatureVectorInput
{
    std::string name;
    std::vector<float> values;
};

struct GeometrySignalInput
{
    LineMeasurementOutput line;
    CircleMeasurementOutput circle;
    EllipseMeasurementOutput ellipse;
    MatchOutput match;
    bool has_line = false;
    bool has_circle = false;
    bool has_ellipse = false;
    bool has_match = false;
};

struct AiTaskEnvelope
{
    AiTaskKind task = AiTaskKind::Unknown;
    AiExecutionMode mode = AiExecutionMode::Infer;
    bool requires_end_to_end_learning = false;
    bool requires_dense_spatial_output = false;
    bool requires_classical_explainability = false;
    TensorBufferView image_tensor;
    std::vector<RoiInput> proposals;
    std::vector<FeatureVectorInput> descriptors;
    GeometrySignalInput geometry;
    int class_count = 0;
    int topk = 1;
};

struct AiRouteDecision
{
    AiRoute route = AiRoute::ManualReview;
    const char* reason = "task boundary is incomplete";
};

struct AiOutputRecord
{
    int label = -1;
    float score = 0.0f;
    OutputRect bounds;
    std::vector<float> embedding;
};

struct AiTaskResult
{
    AiRoute route = AiRoute::ManualReview;
    AiTaskKind task = AiTaskKind::Unknown;
    std::vector<AiOutputRecord> outputs;
    std::vector<float> losses;
    std::vector<std::string> diagnostics;
};

inline const char* AiTaskKindName(AiTaskKind task)
{
    switch (task)
    {
    case AiTaskKind::DirectMeasurement:
        return "direct_measurement";
    case AiTaskKind::NumericClassification:
        return "numeric_classification";
    case AiTaskKind::NumericClustering:
        return "numeric_clustering";
    case AiTaskKind::GeometryMatching:
        return "geometry_matching";
    case AiTaskKind::GeometryClustering:
        return "geometry_clustering";
    case AiTaskKind::RegionClassification:
        return "region_classification";
    case AiTaskKind::RegionDetectionRefinement:
        return "region_detection_refinement";
    case AiTaskKind::DenseSegmentation:
        return "dense_segmentation";
    case AiTaskKind::VisualEmbedding:
        return "visual_embedding";
    case AiTaskKind::Unknown:
    default:
        return "unknown";
    }
}

inline const char* AiRouteName(AiRoute route)
{
    switch (route)
    {
    case AiRoute::StayInCxcore:
        return "stay_in_cxcore";
    case AiRoute::RouteToMlpack:
        return "route_to_mlpack";
    case AiRoute::UpgradeToTorchModule:
        return "upgrade_to_torch_module";
    case AiRoute::ManualReview:
    default:
        return "manual_review";
    }
}

inline bool HasImageTensor(const AiTaskEnvelope& envelope)
{
    return envelope.image_tensor.layout.width > 0 &&
        envelope.image_tensor.layout.height > 0 &&
        envelope.image_tensor.layout.channels > 0 &&
        envelope.image_tensor.layout.batch > 0 &&
        !envelope.image_tensor.values.empty();
}

inline bool HasDescriptors(const AiTaskEnvelope& envelope)
{
    for (const auto& item : envelope.descriptors)
    {
        if (!item.values.empty())
        {
            return true;
        }
    }
    return false;
}

inline bool HasGeometrySignals(const AiTaskEnvelope& envelope)
{
    return envelope.geometry.has_line ||
        envelope.geometry.has_circle ||
        envelope.geometry.has_ellipse ||
        envelope.geometry.has_match;
}

inline bool ValidateAiTaskEnvelope(const AiTaskEnvelope& envelope, std::string* error = nullptr)
{
    const bool has_image = HasImageTensor(envelope);
    const bool has_descriptors = HasDescriptors(envelope);
    const bool has_geometry = HasGeometrySignals(envelope);
    const bool has_any_signal = has_image || has_descriptors || has_geometry || !envelope.proposals.empty();

    if (!has_any_signal)
    {
        if (error) *error = "AiTaskEnvelope requires image, roi, descriptor, or geometry input";
        return false;
    }

    if (has_image)
    {
        const auto& layout = envelope.image_tensor.layout;
        const int64_t expected = static_cast<int64_t>(layout.batch) * layout.channels * layout.height * layout.width;
        if (expected <= 0 || static_cast<int64_t>(envelope.image_tensor.values.size()) != expected)
        {
            if (error) *error = "AiTaskEnvelope image tensor size does not match BCHW layout";
            return false;
        }
    }

    if (envelope.mode == AiExecutionMode::Train && envelope.class_count <= 0)
    {
        if (error) *error = "training tasks require a positive class_count";
        return false;
    }

    if (envelope.topk <= 0)
    {
        if (error) *error = "topk must be positive";
        return false;
    }

    return true;
}

inline AiRouteDecision DecideAiRoute(const AiTaskEnvelope& envelope)
{
    const bool has_image = HasImageTensor(envelope);
    const bool has_descriptors = HasDescriptors(envelope);
    const bool has_geometry = HasGeometrySignals(envelope);

    switch (envelope.task)
    {
    case AiTaskKind::DirectMeasurement:
        return { AiRoute::StayInCxcore, "direct measurement stays in cxcore" };

    case AiTaskKind::NumericClassification:
    case AiTaskKind::NumericClustering:
    case AiTaskKind::GeometryMatching:
    case AiTaskKind::GeometryClustering:
        if (!has_image && (has_descriptors || has_geometry) && !envelope.requires_end_to_end_learning)
        {
            return { AiRoute::RouteToMlpack, "structured descriptor or geometry task routes to mlpack" };
        }
        if (!has_image && has_geometry && envelope.requires_classical_explainability)
        {
            return { AiRoute::StayInCxcore, "classical explainability requirement keeps task in cxcore" };
        }
        return { AiRoute::ManualReview, "structured task mixes incompatible upgrade signals" };

    case AiTaskKind::RegionClassification:
    case AiTaskKind::RegionDetectionRefinement:
    case AiTaskKind::DenseSegmentation:
    case AiTaskKind::VisualEmbedding:
        if (has_image)
        {
            return { AiRoute::UpgradeToTorchModule, "image-backed learning task upgrades to torch_module" };
        }
        return { AiRoute::ManualReview, "visual task requires image tensor input from cxcore" };

    case AiTaskKind::Unknown:
    default:
        break;
    }

    if (has_image && envelope.requires_end_to_end_learning)
    {
        return { AiRoute::UpgradeToTorchModule, "end-to-end image learning upgrades to torch_module" };
    }

    if (!has_image && (has_descriptors || has_geometry))
    {
        return { AiRoute::RouteToMlpack, "non-visual structured task routes to mlpack" };
    }

    return { AiRoute::ManualReview, "task boundary requires explicit route selection" };
}

inline AiTaskEnvelope MakeRegionClassificationEnvelope(
    const TensorBufferView& image_tensor,
    const std::vector<RoiInput>& proposals,
    const std::vector<FeatureVectorInput>& descriptors = {})
{
    AiTaskEnvelope envelope;
    envelope.task = AiTaskKind::RegionClassification;
    envelope.mode = AiExecutionMode::Infer;
    envelope.image_tensor = image_tensor;
    envelope.proposals = proposals;
    envelope.descriptors = descriptors;
    envelope.topk = 1;
    return envelope;
}

inline AiTaskEnvelope MakeStructuredGeometryEnvelope(
    AiTaskKind task,
    const GeometrySignalInput& geometry,
    const std::vector<FeatureVectorInput>& descriptors,
    bool requires_classical_explainability = false)
{
    AiTaskEnvelope envelope;
    envelope.task = task;
    envelope.mode = AiExecutionMode::Infer;
    envelope.geometry = geometry;
    envelope.descriptors = descriptors;
    envelope.requires_classical_explainability = requires_classical_explainability;
    envelope.topk = 1;
    return envelope;
}

inline AiTaskResult MakeEmptyAiTaskResult(const AiTaskEnvelope& envelope, AiRoute route)
{
    AiTaskResult result;
    result.route = route;
    result.task = envelope.task;
    return result;
}

} // namespace cxcore

#endif
