#ifndef CXIMAGE_CXTORCH_RESULT_PROJECTOR_H
#define CXIMAGE_CXTORCH_RESULT_PROJECTOR_H

#include "CxExecutionTypes.h"
#include "CxRuntimeProjectionTypes.h"

class CxTorchResultProjector
{
public:
    static bool Project(
        const CxInferenceResult& inference_result,
        const std::string& owner_type,
        const std::string& owner_ref,
        std::vector<CxShapeElementSnapshot>& shapes);

private:
    static void ProjectDetections(
        const std::vector<CxTorchDetection>& detections,
        const std::string& owner_type,
        const std::string& owner_ref,
        std::vector<CxShapeElementSnapshot>& shapes);

    static void ProjectMask(
        const CxTorchMask& mask,
        const std::string& owner_type,
        const std::string& owner_ref,
        std::vector<CxShapeElementSnapshot>& shapes);
};

#endif
