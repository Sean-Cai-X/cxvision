#pragma once

#include "CxExecutionTypes.h"
#include "CxTorchRuntimeService.h"

class CxTorchExecutionAdapter
{
public:
    bool Execute(const CxTorchTaskSpec& task, CxInferenceResult& result, std::string& reason);

private:
    bool EnsureRuntime(const CxTorchTaskSpec& task, std::string& reason);
    bool BuildRuntimeRequest(const CxTorchTaskSpec& task, CxTorchTaskRequest& request, std::string& reason) const;

    CxTorchRuntimeService service_;
};