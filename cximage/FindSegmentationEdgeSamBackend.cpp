#include "FindSegmentationEdgeSamBackend.h"

bool FindSegmentationEdgeSamBackend::Run(
    const FindSegmentationInput& input,
    FindSegmentationResult& output,
    std::string& reason)
{
    output.ok = false;
    output.backend = "edgesam";
    output.backend_status = "pending_binding";
    output.status = "pending_binding";
    output.reason = "EdgeSam backend pending binding to LibtorchSegmentation torch mainline";

    reason = output.reason;
    return false;
}