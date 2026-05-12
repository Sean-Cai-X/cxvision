#ifndef CXCORE_CORE_CXCOREGEOMETRYATTACH_H
#define CXCORE_CORE_CXCOREGEOMETRYATTACH_H

#include <cstdint>
#include <string>
#include <vector>

#include "CxCoreBoundary.h"
#include "../../cxgeom/include/CxGeomAnnotationBody.h"
#include "../../cxgeom/include/CxGeomElementBody.h"

namespace cxcore {

enum class GeometryObjectKind
{
    Unknown = 0,
    Roi,
    Line,
    PointSet,
    Mask,
    Boundary,
    Keypoints
};

enum class GeometryRole
{
    Unknown = 0,
    InputPrior,
    TrainingLabel,
    OutputAttach
};

struct StableGeometryRef
{
    std::string object_id;
    std::string source_image_id;
    std::string parent_roi_id;
    GeometryObjectKind kind = GeometryObjectKind::Unknown;
};

struct RoiObject
{
    StableGeometryRef ref;
    OutputRect bounds;
    float score_hint = 0.0f;
    int label_hint = -1;
};

struct LineObject
{
    StableGeometryRef ref;
    PointSetOutput samples;
    OutputPoint start;
    OutputPoint end;
    float score_hint = 0.0f;
};

struct PointSetObject
{
    StableGeometryRef ref;
    PointSetOutput samples;
    std::string sampling_mode;
};

struct MaskObject
{
    StableGeometryRef ref;
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> values;
};

struct BoundaryObject
{
    StableGeometryRef ref;
    PointSetOutput contour;
    bool closed = false;
};

struct KeypointsObject
{
    StableGeometryRef ref;
    std::vector<OutputPoint> points;
    std::vector<float> visibility;
};

struct GeometryAttachRecord
{
    StableGeometryRef target;
    std::string attach_id;
    std::string label;
    float score = 0.0f;
    OutputRect refined_bounds;
    std::vector<float> embedding;
    std::string mask_object_id;
    std::string boundary_object_id;
    std::string keypoints_object_id;
};

inline const char* GeometryObjectKindName(GeometryObjectKind kind)
{
    switch (kind)
    {
    case GeometryObjectKind::Roi:
        return "roi";
    case GeometryObjectKind::Line:
        return "line";
    case GeometryObjectKind::PointSet:
        return "pointset";
    case GeometryObjectKind::Mask:
        return "mask";
    case GeometryObjectKind::Boundary:
        return "boundary";
    case GeometryObjectKind::Keypoints:
        return "keypoints";
    case GeometryObjectKind::Unknown:
    default:
        return "unknown";
    }
}

inline bool IsValidGeometryRef(const StableGeometryRef& ref)
{
    return !ref.object_id.empty() && ref.kind != GeometryObjectKind::Unknown;
}

inline bool IsSuitableAsTorchInputPrior(GeometryObjectKind kind)
{
    switch (kind)
    {
    case GeometryObjectKind::Roi:
    case GeometryObjectKind::Line:
    case GeometryObjectKind::PointSet:
    case GeometryObjectKind::Mask:
    case GeometryObjectKind::Boundary:
    case GeometryObjectKind::Keypoints:
        return true;
    case GeometryObjectKind::Unknown:
    default:
        return false;
    }
}

inline bool IsSuitableAsTrainingLabel(GeometryObjectKind kind)
{
    switch (kind)
    {
    case GeometryObjectKind::Roi:
    case GeometryObjectKind::Mask:
    case GeometryObjectKind::Boundary:
    case GeometryObjectKind::Keypoints:
        return true;
    case GeometryObjectKind::Line:
    case GeometryObjectKind::PointSet:
    case GeometryObjectKind::Unknown:
    default:
        return false;
    }
}

inline bool NeedsModelOutputAttach(GeometryObjectKind kind)
{
    switch (kind)
    {
    case GeometryObjectKind::Roi:
    case GeometryObjectKind::Line:
    case GeometryObjectKind::PointSet:
    case GeometryObjectKind::Mask:
    case GeometryObjectKind::Boundary:
    case GeometryObjectKind::Keypoints:
        return true;
    case GeometryObjectKind::Unknown:
    default:
        return false;
    }
}

inline bool ValidateMaskObject(const MaskObject& object)
{
    if (!IsValidGeometryRef(object.ref))
    {
        return false;
    }
    if (object.width <= 0 || object.height <= 0)
    {
        return false;
    }
    const auto expected = static_cast<std::size_t>(object.width) * static_cast<std::size_t>(object.height);
    return object.values.size() == expected;
}

inline bool ValidateBoundaryObject(const BoundaryObject& object)
{
    return IsValidGeometryRef(object.ref) && !object.contour.points.empty();
}

inline bool ValidateKeypointsObject(const KeypointsObject& object)
{
    return IsValidGeometryRef(object.ref) &&
        !object.points.empty() &&
        object.points.size() == object.visibility.size();
}

inline GeometryAttachRecord MakeRoiAttachRecord(
    const RoiObject& roi,
    const std::string& attach_id,
    const std::string& label,
    float score,
    const OutputRect& refined_bounds,
    const std::vector<float>& embedding = {})
{
    GeometryAttachRecord record;
    record.target = roi.ref;
    record.attach_id = attach_id;
    record.label = label;
    record.score = score;
    record.refined_bounds = refined_bounds;
    record.embedding = embedding;
    return record;
}

inline GeometryAttachRecord MakeMaskAttachRecord(
    const RoiObject& roi,
    const std::string& attach_id,
    const std::string& mask_object_id,
    const std::string& boundary_object_id,
    float score)
{
    GeometryAttachRecord record;
    record.target = roi.ref;
    record.attach_id = attach_id;
    record.score = score;
    record.refined_bounds = roi.bounds;
    record.mask_object_id = mask_object_id;
    record.boundary_object_id = boundary_object_id;
    return record;
}

inline GeometryAttachRecord MakeKeypointsAttachRecord(
    const RoiObject& roi,
    const std::string& attach_id,
    const std::string& keypoints_object_id,
    float score)
{
    GeometryAttachRecord record;
    record.target = roi.ref;
    record.attach_id = attach_id;
    record.score = score;
    record.refined_bounds = roi.bounds;
    record.keypoints_object_id = keypoints_object_id;
    return record;
}

inline cxgeom::CxGeomBounds MakeGeomBounds(const OutputRect& rect)
{
    cxgeom::CxGeomBounds bounds;
    bounds.min_x = rect.x;
    bounds.min_y = rect.y;
    bounds.min_z = 0.0;
    bounds.max_x = rect.x + rect.width;
    bounds.max_y = rect.y + rect.height;
    bounds.max_z = 0.0;
    bounds.valid = rect.width > 0.0 || rect.height > 0.0;
    return bounds;
}

inline cxgeom::CxShapeHandle MakeMetadataShape(int entity_id,
                                               const std::string& name,
                                               cxgeom::CxShapeKind kind)
{
    return cxgeom::CxShapeHandle(entity_id, name, kind);
}

inline cxgeom::CxGeomElement MakeGeomElementFromRoi(const RoiObject& roi, int entity_id)
{
    cxgeom::CxGeomElement element = cxgeom::CxGeomElementBody::MakeElement(
        MakeMetadataShape(entity_id, roi.ref.object_id, cxgeom::CxShapeKind::Face),
        true);
    element.entity_type = "roi";
    element.source_stage = "analysis";
    element.status = "ok";
    element.measure_shape = element.shape;
    element.bbox = MakeGeomBounds(roi.bounds);
    element.confidence = roi.score_hint;
    element.success = true;
    return element;
}

inline cxgeom::CxCurveElement MakeCurveElementFromLine(const LineObject& line, int entity_id)
{
    cxgeom::CxCurveElement element = cxgeom::CxGeomElementBody::MakeCurveElement(
        MakeMetadataShape(entity_id, line.ref.object_id, cxgeom::CxShapeKind::Curve),
        0.0,
        false,
        true);
    element.base.entity_type = "line";
    element.base.source_stage = "analysis";
    element.base.status = "ok";
    element.base.bbox = line.samples.bounds.width > 0.0 || line.samples.bounds.height > 0.0
        ? MakeGeomBounds(line.samples.bounds)
        : MakeGeomBounds(OutputRect{ line.start.x, line.start.y, line.end.x - line.start.x, line.end.y - line.start.y });
    element.base.confidence = line.score_hint;
    element.base.success = !line.samples.points.empty();
    element.curve_type = "line";
    element.start_point = gp_Pnt(line.start.x, line.start.y, 0.0);
    element.end_point = gp_Pnt(line.end.x, line.end.y, 0.0);
    element.center_point = gp_Pnt((line.start.x + line.end.x) * 0.5, (line.start.y + line.end.y) * 0.5, 0.0);
    element.length_hint = std::hypot(line.end.x - line.start.x, line.end.y - line.start.y);
    return element;
}

inline cxgeom::CxCurveElement MakeCurveElementFromCircle(const CircleMeasurementOutput& circle, int entity_id, const char* name)
{
    cxgeom::CxCurveElement element = cxgeom::CxGeomElementBody::MakeCurveElement(
        MakeMetadataShape(entity_id, name ? name : "circle", cxgeom::CxShapeKind::Curve),
        circle.radius > 0.0 ? (2.0 * 3.14159265358979323846 * circle.radius) : 0.0,
        true,
        true);
    element.base.entity_type = "circle";
    element.base.source_stage = "analysis";
    element.base.status = circle.has_direct_fit ? "ok" : "fallback";
    element.base.bbox = MakeGeomBounds(circle.measure_bounds);
    element.base.confidence = circle.has_direct_fit ? 1.0 : 0.5;
    element.base.success = !circle.sample_points.points.empty();
    element.curve_type = "circle";
    element.center_point = gp_Pnt(circle.center.x, circle.center.y, 0.0);
    element.length_hint = circle.radius > 0.0 ? (2.0 * 3.14159265358979323846 * circle.radius) : 0.0;
    element.closed = true;
    return element;
}

inline cxgeom::CxSurfaceElement MakeSurfaceElementFromRect(const OutputRect& rect,
                                                           int entity_id,
                                                           const char* name,
                                                           double confidence = 1.0)
{
    cxgeom::CxSurfaceElement element = cxgeom::CxGeomElementBody::MakeSurfaceElement(
        MakeMetadataShape(entity_id, name ? name : "rect", cxgeom::CxShapeKind::Face),
        rect.width * rect.height,
        true);
    element.base.entity_type = "rect";
    element.base.source_stage = "analysis";
    element.base.status = "ok";
    element.base.bbox = MakeGeomBounds(rect);
    element.base.confidence = confidence;
    element.base.success = rect.width > 0.0 && rect.height > 0.0;
    element.surface_type = "rect";
    element.outer_boundary = {
        gp_Pnt(rect.x, rect.y, 0.0),
        gp_Pnt(rect.x + rect.width, rect.y, 0.0),
        gp_Pnt(rect.x + rect.width, rect.y + rect.height, 0.0),
        gp_Pnt(rect.x, rect.y + rect.height, 0.0)
    };
    return element;
}

inline cxgeom::CxGeomBatchElement MakeBatchElementFromPoints(const PointSetOutput& pointset,
                                                             int batch_id,
                                                             const char* name,
                                                             const char* element_type)
{
    cxgeom::CxGeomBatchElement batch = cxgeom::CxGeomElementBody::MakeBatchElement(batch_id, name, {}, true);
    batch.element_type = element_type ? element_type : "pointset";
    batch.source_stage = "analysis";
    batch.status = "ok";
    batch.bbox = MakeGeomBounds(pointset.bounds);
    batch.confidence = pointset.points.empty() ? 0.0 : 1.0;
    batch.success = !pointset.points.empty();
    batch.source_count = pointset.points.size();
    return batch;
}

inline cxgeom::CxGeomAnnotation MakeAnnotationForElement(int annotation_id,
                                                         const cxgeom::CxGeomElement& target,
                                                         const char* text,
                                                         double anchor_x,
                                                         double anchor_y,
                                                         double anchor_z = 0.0)
{
    return cxgeom::CxGeomAnnotationBody::MakeAnnotation(
        annotation_id,
        target.entity_id,
        0,
        text,
        anchor_x,
        anchor_y,
        anchor_z,
        true);
}

inline cxgeom::CxGeomAnnotation MakeAnnotationForBatch(int annotation_id,
                                                       const cxgeom::CxGeomBatchElement& target,
                                                       const char* text,
                                                       double anchor_x,
                                                       double anchor_y,
                                                       double anchor_z = 0.0)
{
    return cxgeom::CxGeomAnnotationBody::MakeAnnotation(
        annotation_id,
        0,
        target.batch_id,
        text,
        anchor_x,
        anchor_y,
        anchor_z,
        true);
}

} // namespace cxcore

#endif
