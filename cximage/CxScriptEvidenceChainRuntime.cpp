#include "CxScriptEvidenceChainRuntime.h"
#include "CxScriptEvidenceChainRegister.h"
#include "muParser.h"
#include <fstream>
#include <sstream>
#include <filesystem>

const CxScriptEvidenceCase* CxScriptEvidenceChainRuntime::FindCase(const std::string& evidence_id) const
{
    for (const auto& c : cases)
    {
        if (c.evidence_id == evidence_id)
            return &c;
    }
    return nullptr;
}

void CxScriptEvidenceChainRuntime::Clear()
{
    chain_id.clear();
    chain_name.clear();
    catalog_path.clear();
    image_manifest_path.clear();
    output_root.clear();
    cases.clear();
}

bool LoadCxScriptEvidenceChainFile(
    const std::string& script_path,
    CxScriptEvidenceChainRuntime& out_chain,
    std::string& out_reason)
{
    std::filesystem::path path(script_path);
    if (!std::filesystem::exists(path))
    {
        out_reason = "Evidence chain file not found: " + script_path;
        return false;
    }

    std::ifstream file(path);
    if (!file.is_open())
    {
        out_reason = "Cannot open evidence chain file: " + script_path;
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string script = buffer.str();

    g_cxscript_evidence_chain.Clear();
    g_current_evidence_case = nullptr;

    mu::Parser parser;
    parser.UsingClass(true);
    RegisterCxScriptEvidenceChainBindings(parser);

    try
    {
        parser.SetExpr(script);
        parser.Eval();
    }
    catch (const mu::Parser::exception_type& e)
    {
        out_reason = "Evidence chain parse error: " + std::string(e.GetMsg());
        return false;
    }

    out_chain = g_cxscript_evidence_chain;
    return true;
}
