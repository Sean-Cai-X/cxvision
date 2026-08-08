#include "muParser.h"
#include "CxScriptEvidenceChainRuntime.h"
#include "CxScriptEvidenceChainRegister.h"

CxScriptEvidenceChainRuntime g_cxscript_evidence_chain;
CxScriptEvidenceCase* g_current_evidence_case = nullptr;

double CxEvidenceChain_reset(double)
{
    g_cxscript_evidence_chain = CxScriptEvidenceChainRuntime{};
    g_current_evidence_case = nullptr;
    return 0.0;
}

double CxEvidenceChain_setid(const char* value)
{
    g_cxscript_evidence_chain.chain_id = value ? value : "";
    return 0.0;
}

double CxEvidenceChain_setname(const char* value)
{
    g_cxscript_evidence_chain.chain_name = value ? value : "";
    return 0.0;
}

double CxEvidenceChain_setcatalog(const char* value)
{
    g_cxscript_evidence_chain.catalog_path = value ? value : "";
    return 0.0;
}

double CxEvidenceChain_setmanifest(const char* value)
{
    g_cxscript_evidence_chain.image_manifest_path = value ? value : "";
    return 0.0;
}

double CxEvidenceChain_setoutputroot(const char* value)
{
    g_cxscript_evidence_chain.output_root = value ? value : "";
    return 0.0;
}

double CxEvidenceChain_addcase(const char* evidence_id)
{
    if (!evidence_id || evidence_id[0] == '\0')
        return 0.0;

    CxScriptEvidenceCase case_entry;
    case_entry.evidence_id = evidence_id;
    g_cxscript_evidence_chain.cases.push_back(case_entry);
    g_current_evidence_case = &g_cxscript_evidence_chain.cases.back();
    return 0.0;
}

double CxEvidenceChain_case_setimage(const char* value)
{
    if (!g_current_evidence_case)
        return 0.0;

    g_current_evidence_case->image_id = value ? value : "";
    return 0.0;
}

double CxEvidenceChain_case_settarget(const char* value)
{
    if (!g_current_evidence_case)
        return 0.0;

    g_current_evidence_case->target_id = value ? value : "";
    return 0.0;
}

double CxEvidenceChain_case_setscript(const char* value)
{
    if (!g_current_evidence_case)
        return 0.0;

    g_current_evidence_case->script_id = value ? value : "";
    return 0.0;
}

double CxEvidenceChain_case_setparameter(const char* value)
{
    if (!g_current_evidence_case)
        return 0.0;

    g_current_evidence_case->parameter_profile_id = value ? value : "";
    return 0.0;
}

double CxEvidenceChain_case_setcontract(const char* value)
{
    if (!g_current_evidence_case)
        return 0.0;

    g_current_evidence_case->contract_id = value ? value : "";
    return 0.0;
}

double CxEvidenceChain_case_setexpected(const char* value)
{
    if (!g_current_evidence_case)
        return 0.0;

    g_current_evidence_case->expected_result = value ? value : "";
    return 0.0;
}

double CxEvidenceChain_case_setexpectedpolicyguard(const char* value)
{
    if (!g_current_evidence_case)
        return 0.0;

    g_current_evidence_case->expected_policy_guard = value ? value : "";
    return 0.0;
}

double CxEvidenceChain_case_setrole(const char* value)
{
    if (!g_current_evidence_case)
        return 0.0;

    g_current_evidence_case->case_role = value ? value : "";
    return 0.0;
}

double CxEvidenceChain_case_setsourcecase(const char* value)
{
    if (!g_current_evidence_case)
        return 0.0;

    g_current_evidence_case->source_case_id = value ? value : "";
    return 0.0;
}

double CxEvidenceChain_case_settool(const char* value)
{
    if (!g_current_evidence_case)
        return 0.0;

    g_current_evidence_case->tool = value ? value : "";
    return 0.0;
}

double CxEvidenceChain_case_setlevel(const char* value)
{
    if (!g_current_evidence_case)
        return 0.0;

    g_current_evidence_case->level = value ? value : "";
    return 0.0;
}

double CxEvidenceChain_case_setcategory(const char* value)
{
    if (!g_current_evidence_case)
        return 0.0;

    g_current_evidence_case->display_category = value ? value : "";
    return 0.0;
}

double CxEvidenceChain_case_setgroup(const char* value)
{
    if (!g_current_evidence_case)
        return 0.0;

    g_current_evidence_case->display_group = value ? value : "";
    return 0.0;
}

void RegisterCxScriptEvidenceChainBindings(mu::Parser& parser)
{
    parser.DefineFun("CxEvidenceChain_reset", (mu::fun_type1)&CxEvidenceChain_reset);
    parser.DefineFun("CxEvidenceChain_setid", (mu::strfun_type1)&CxEvidenceChain_setid);
    parser.DefineFun("CxEvidenceChain_setname", (mu::strfun_type1)&CxEvidenceChain_setname);
    parser.DefineFun("CxEvidenceChain_setcatalog", (mu::strfun_type1)&CxEvidenceChain_setcatalog);
    parser.DefineFun("CxEvidenceChain_setmanifest", (mu::strfun_type1)&CxEvidenceChain_setmanifest);
    parser.DefineFun("CxEvidenceChain_setoutputroot", (mu::strfun_type1)&CxEvidenceChain_setoutputroot);
    parser.DefineFun("CxEvidenceChain_addcase", (mu::strfun_type1)&CxEvidenceChain_addcase);
    parser.DefineFun("CxEvidenceChain_case_setimage", (mu::strfun_type1)&CxEvidenceChain_case_setimage);
    parser.DefineFun("CxEvidenceChain_case_settarget", (mu::strfun_type1)&CxEvidenceChain_case_settarget);
    parser.DefineFun("CxEvidenceChain_case_setscript", (mu::strfun_type1)&CxEvidenceChain_case_setscript);
    parser.DefineFun("CxEvidenceChain_case_setparameter", (mu::strfun_type1)&CxEvidenceChain_case_setparameter);
    parser.DefineFun("CxEvidenceChain_case_setcontract", (mu::strfun_type1)&CxEvidenceChain_case_setcontract);
    parser.DefineFun("CxEvidenceChain_case_setexpected", (mu::strfun_type1)&CxEvidenceChain_case_setexpected);
    parser.DefineFun("CxEvidenceChain_case_setexpectedpolicyguard", (mu::strfun_type1)&CxEvidenceChain_case_setexpectedpolicyguard);
    parser.DefineFun("CxEvidenceChain_case_setrole", (mu::strfun_type1)&CxEvidenceChain_case_setrole);
    parser.DefineFun("CxEvidenceChain_case_setsourcecase", (mu::strfun_type1)&CxEvidenceChain_case_setsourcecase);
    parser.DefineFun("CxEvidenceChain_case_settool", (mu::strfun_type1)&CxEvidenceChain_case_settool);
    parser.DefineFun("CxEvidenceChain_case_setlevel", (mu::strfun_type1)&CxEvidenceChain_case_setlevel);
    parser.DefineFun("CxEvidenceChain_case_setcategory", (mu::strfun_type1)&CxEvidenceChain_case_setcategory);
    parser.DefineFun("CxEvidenceChain_case_setgroup", (mu::strfun_type1)&CxEvidenceChain_case_setgroup);
}
