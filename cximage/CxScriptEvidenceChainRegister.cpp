#include "muParser.h"
#include "CxScriptEvidenceChainRuntime.h"
#include "CxScriptEvidenceChainRegister.h"

#include <cstdlib>
#include <sstream>
#include <string>
#include <unordered_map>

CxScriptEvidenceChainRuntime g_cxscript_evidence_chain;
CxScriptEvidenceCase* g_current_evidence_case = nullptr;

static std::unordered_map<std::string, std::string> ParseEvidenceKeyValueListLocal(
    const char* value)
{
    std::unordered_map<std::string, std::string> result;
    if (!value)
        return result;

    std::istringstream stream(value);
    std::string token;
    while (stream >> token)
    {
        const std::size_t eq = token.find('=');
        if (eq == std::string::npos)
            continue;
        const std::string key = token.substr(0, eq);
        const std::string val = token.substr(eq + 1);
        if (!key.empty())
            result[key] = val;
    }
    return result;
}

static std::string GetEvidenceKvLocal(
    const std::unordered_map<std::string, std::string>& values,
    const char* key,
    const std::string& fallback = std::string())
{
    const auto it = values.find(key);
    return it == values.end() ? fallback : it->second;
}

static double GetEvidenceKvDoubleLocal(
    const std::unordered_map<std::string, std::string>& values,
    const char* key,
    double fallback = 0.0)
{
    const auto it = values.find(key);
    if (it == values.end())
        return fallback;
    return std::strtod(it->second.c_str(), nullptr);
}

static int GetEvidenceKvIntLocal(
    const std::unordered_map<std::string, std::string>& values,
    const char* key,
    int fallback = -1)
{
    const auto it = values.find(key);
    if (it == values.end())
        return fallback;
    return static_cast<int>(std::strtol(it->second.c_str(), nullptr, 10));
}

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

double CxEvidenceChain_case_adddatasetimage(const char* value)
{
    if (!g_current_evidence_case)
        return 0.0;

    const auto values = ParseEvidenceKeyValueListLocal(value);
    CxScriptEvidenceDatasetImage image;
    image.image_id = GetEvidenceKvLocal(values, "image_id");
    image.image_path = GetEvidenceKvLocal(values, "path");
    image.split = GetEvidenceKvLocal(values, "split", "train");
    image.label = GetEvidenceKvLocal(values, "label", "unlabeled");
    image.source = GetEvidenceKvLocal(values, "source", "evidence_dataset");
    if (!image.image_id.empty() || !image.image_path.empty())
        g_current_evidence_case->dataset_images.push_back(image);
    return 0.0;
}

double CxEvidenceChain_case_addbbox_xywh_norm(const char* value)
{
    if (!g_current_evidence_case)
        return 0.0;

    const auto values = ParseEvidenceKeyValueListLocal(value);
    const double cx = GetEvidenceKvDoubleLocal(values, "cx", 0.0);
    const double cy = GetEvidenceKvDoubleLocal(values, "cy", 0.0);
    const double w = GetEvidenceKvDoubleLocal(values, "w", 0.0);
    const double h = GetEvidenceKvDoubleLocal(values, "h", 0.0);

    CxScriptEvidenceAnnotation annotation;
    annotation.image_id = GetEvidenceKvLocal(values, "image_id");
    annotation.shape_kind = "RectShape";
    annotation.semantic_role = GetEvidenceKvLocal(values, "role", "bbox");
    annotation.owner_binding = GetEvidenceKvLocal(values, "binding", "label_bbox");
    annotation.label = GetEvidenceKvLocal(values, "label", "anomaly");
    annotation.class_id = GetEvidenceKvIntLocal(values, "class_id", -1);
    annotation.x0 = cx - (w * 0.5);
    annotation.y0 = cy - (h * 0.5);
    annotation.x1 = cx + (w * 0.5);
    annotation.y1 = cy + (h * 0.5);
    annotation.normalized = true;
    if (!annotation.image_id.empty() && w > 0.0 && h > 0.0)
        g_current_evidence_case->annotations.push_back(annotation);
    return 0.0;
}

double CxEvidenceChain_case_clone_dataset_from(const char* value)
{
    if (!g_current_evidence_case || !value || value[0] == '\0')
        return 0.0;

    const std::string source_id = value;
    for (const CxScriptEvidenceCase& source : g_cxscript_evidence_chain.cases)
    {
        if (source.evidence_id != source_id)
            continue;
        g_current_evidence_case->dataset_images = source.dataset_images;
        g_current_evidence_case->annotations = source.annotations;
        return 0.0;
    }
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
    parser.DefineFun("CxEvidenceChain_case_adddatasetimage", (mu::strfun_type1)&CxEvidenceChain_case_adddatasetimage);
    parser.DefineFun("CxEvidenceChain_case_addbbox_xywh_norm", (mu::strfun_type1)&CxEvidenceChain_case_addbbox_xywh_norm);
    parser.DefineFun("CxEvidenceChain_case_clone_dataset_from", (mu::strfun_type1)&CxEvidenceChain_case_clone_dataset_from);
}
