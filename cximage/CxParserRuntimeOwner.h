#ifndef CXIMAGE_CXPARSER_RUNTIME_OWNER_H
#define CXIMAGE_CXPARSER_RUNTIME_OWNER_H

#include <string>
#include <vector>
#include <memory>

#include "CxParserSnapshotTypes.h"

namespace mu
{
    class CxParserRuntime;
}

enum class CxParserDocumentKind
{
    AnnotationToolManifest,
    ShapeInteractionSuite
};

class CxParserExecutionGuard;

class CxParserRuntimeOwner
{
    friend class CxParserExecutionGuard;

public:
    CxParserRuntimeOwner();
    ~CxParserRuntimeOwner();

    CxParserRuntimeOwner(const CxParserRuntimeOwner&) = delete;
    CxParserRuntimeOwner& operator=(const CxParserRuntimeOwner&) = delete;
    CxParserRuntimeOwner(CxParserRuntimeOwner&&) = delete;
    CxParserRuntimeOwner& operator=(CxParserRuntimeOwner&&) = delete;

    bool Initialize(std::string& reason);

    bool ParseAnnotationToolManifest(
        const std::string& path,
        CxAnnotationToolManifestSnapshot& snapshot,
        std::string& reason);

    bool ParseShapeInteractionSuite(
        const std::string& path,
        CxShapeTestSuiteSnapshot& snapshot,
        std::string& reason);

    bool IsExecuting() const;

    bool Compile(const std::string& source, std::string& reason);
    bool Compile(const char* source);
    void ConfigureStreams(std::ostream* runtime_stream, std::ostream* code_stream);
    int ObjectCount(const std::string& type) const;
    void* GetClassObj(const std::string& strclass, const std::string& strobj);
    void* GetClassObj(const std::string& strclass, const int& iobjnum);
    void* GetDoubleValue(const std::string& strname);
    void ClearAll();
    bool IsObjectVar(const char* sz);

private:
    bool ReadScript(
        const std::string& path,
        std::string& source,
        std::string& reason) const;

    bool BeginExecution(
        CxParserDocumentKind kind,
        const std::string& source_path,
        std::string& reason);

    void EndExecution();

private:
    std::unique_ptr<mu::CxParserRuntime> m_runtime;
    bool m_initialized = false;
    bool m_executing = false;
};

#endif