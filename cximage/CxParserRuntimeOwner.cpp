#include "pch.h"
#include "CxParserRuntimeOwner.h"
#include "CxAnnotationToolRegister.h"
#include "CxShapeTestRegister.h"
#include "ParserClass.h"
#include <fstream>

class CxParserExecutionGuard
{
public:
    explicit CxParserExecutionGuard(CxParserRuntimeOwner& owner)
        : m_owner(owner)
    {
    }

    ~CxParserExecutionGuard()
    {
        m_owner.EndExecution();
    }

private:
    CxParserRuntimeOwner& m_owner;
};

CxParserRuntimeOwner::CxParserRuntimeOwner()
{
}

CxParserRuntimeOwner::~CxParserRuntimeOwner()
{
}

bool CxParserRuntimeOwner::Initialize(std::string& reason)
{
    if (m_initialized)
        return true;

    try
    {
        m_runtime = std::make_unique<mu::CxParserRuntime>();
        m_runtime->ParserInitialClassFunction(0);

        RegisterCxAnnotationToolBindings(m_runtime->m_parser);
        RegisterCxShapeTestBindings(m_runtime->m_parser);

        m_initialized = true;
        return true;
    }
    catch (...)
    {
        m_runtime.reset();
        reason = "parser runtime initialization failed";
        return false;
    }
}

bool CxParserRuntimeOwner::ReadScript(
    const std::string& path,
    std::string& source,
    std::string& reason) const
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        reason = "cannot open script file: " + path;
        return false;
    }

    source = std::string(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>());

    if (source.empty())
    {
        reason = "script file is empty: " + path;
        return false;
    }

    return true;
}

bool CxParserRuntimeOwner::BeginExecution(
    CxParserDocumentKind kind,
    const std::string& source_path,
    std::string& reason)
{
    if (!m_initialized)
    {
        reason = "parser owner is not initialized";
        return false;
    }

    if (m_executing)
    {
        reason = "parser re-entry is forbidden in phase 1";
        return false;
    }

    m_executing = true;
    return true;
}

void CxParserRuntimeOwner::EndExecution()
{
    m_executing = false;
}

bool CxParserRuntimeOwner::IsExecuting() const
{
    return m_executing;
}

bool CxParserRuntimeOwner::Compile(const std::string& source, std::string& reason)
{
    if (!m_initialized)
    {
        reason = "parser owner is not initialized";
        return false;
    }

    try
    {
        return m_runtime->Compile(source.c_str());
    }
    catch (...)
    {
        reason = "compile failed";
        return false;
    }
}

bool CxParserRuntimeOwner::Compile(const char* source)
{
    if (!m_initialized || !m_runtime)
        return false;

    try
    {
        return m_runtime->Compile(source);
    }
    catch (...)
    {
        return false;
    }
}

void CxParserRuntimeOwner::ConfigureStreams(std::ostream* runtime_stream, std::ostream* code_stream)
{
    if (m_runtime)
    {
        m_runtime->SetStream(runtime_stream);
        m_runtime->SetCreateCodeStream(code_stream);
    }
}

int CxParserRuntimeOwner::ObjectCount(const std::string& type) const
{
    if (!m_runtime)
        return 0;
    return m_runtime->GetClassObjSum(type);
}

void* CxParserRuntimeOwner::GetClassObj(const std::string& strclass, const std::string& strobj)
{
    if (!m_runtime)
        return nullptr;
    return m_runtime->GetClassObj(strclass, strobj);
}

void* CxParserRuntimeOwner::GetClassObj(const std::string& strclass, const int& iobjnum)
{
    if (!m_runtime)
        return nullptr;
    return m_runtime->GetClassObj(strclass, iobjnum);
}

void* CxParserRuntimeOwner::GetDoubleValue(const std::string& strname)
{
    if (!m_runtime)
        return nullptr;
    return m_runtime->GetDoubleValue(strname);
}

void CxParserRuntimeOwner::ClearAll()
{
    if (m_runtime)
        m_runtime->ClearAll();
}

bool CxParserRuntimeOwner::IsObjectVar(const char* sz)
{
    if (!m_runtime)
        return false;
    return m_runtime->IsObjectVar(sz);
}

bool CxParserRuntimeOwner::ParseAnnotationToolManifest(
    const std::string& path,
    CxAnnotationToolManifestSnapshot& snapshot,
    std::string& reason)
{
    snapshot = {};

    std::string source;
    if (!ReadScript(path, source, reason))
        return false;

    if (!BeginExecution(
            CxParserDocumentKind::AnnotationToolManifest,
            path,
            reason))
        return false;

    CxParserExecutionGuard guard(*this);

    CxAnnotationToolRuntime::Reset();

    try
    {
        m_runtime->m_parser.SetExpr(source);
        m_runtime->m_parser.Eval();

        snapshot.source_path = path;
        snapshot.tools = CxAnnotationToolRuntime::Tools();
    }
    catch (const mu::Parser::exception_type& error)
    {
        reason = "annotation manifest parse error: " +
                 std::string(error.GetMsg());

        CxAnnotationToolRuntime::Reset();
        return false;
    }
    catch (const std::exception& e)
    {
        reason = "annotation manifest execution error: " +
                 std::string(e.what());

        CxAnnotationToolRuntime::Reset();
        return false;
    }
    catch (...)
    {
        reason = "unknown annotation manifest execution failure";

        CxAnnotationToolRuntime::Reset();
        return false;
    }

    CxAnnotationToolRuntime::Reset();

    if (snapshot.tools.empty())
    {
        reason = "annotation manifest produced no tools";
        return false;
    }

    return true;
}

bool CxParserRuntimeOwner::ParseShapeInteractionSuite(
    const std::string& path,
    CxShapeTestSuiteSnapshot& snapshot,
    std::string& reason)
{
    snapshot = {};

    std::string source;
    if (!ReadScript(path, source, reason))
        return false;

    if (!BeginExecution(
            CxParserDocumentKind::ShapeInteractionSuite,
            path,
            reason))
        return false;

    CxParserExecutionGuard guard(*this);

    CxShapeTestRuntime::Reset();

    try
    {
        m_runtime->m_parser.SetExpr(source);
        m_runtime->m_parser.Eval();

        snapshot.source_path = path;
        snapshot.cases = CxShapeTestRuntime::Cases();
    }
    catch (const mu::Parser::exception_type& error)
    {
        reason = "shape suite parse error: " +
                 std::string(error.GetMsg());

        CxShapeTestRuntime::Reset();
        return false;
    }
    catch (const std::exception& e)
    {
        reason = "shape suite execution error: " +
                 std::string(e.what());

        CxShapeTestRuntime::Reset();
        return false;
    }
    catch (...)
    {
        reason = "unknown shape suite execution failure";

        CxShapeTestRuntime::Reset();
        return false;
    }

    CxShapeTestRuntime::Reset();

    if (snapshot.cases.empty())
    {
        reason = "shape suite produced no cases";
        return false;
    }

    return true;
}