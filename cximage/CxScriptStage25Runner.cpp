#include "CxScriptStage25Runner.h"
#include "CxScriptStage25Template.h"
#include "CxScriptImagePreflight.h"
#include "CxScriptStage25ReportWriter.h"
#include "CxScriptStage25Register.h"
#include "ManualStateTestConsole.h"
#include "CxScriptImageEvidenceAnalyzer.h"
#include "muParser.h"
#include <fstream>
#include <sstream>

namespace
{
    std::string GenerateCaseId(
        const std::string& image_id,
        const std::string& target_id,
        const std::string& profile_id,
        const std::string& evidence_profile)
    {
        return image_id + "__" + target_id + "__" + profile_id + "__" + evidence_profile;
    }

    Stage25TemplateContext BuildFindlineContext(
        const Stage25ImageTarget& target,
        const Stage25FindlineProfile& profile)
    {
        Stage25TemplateContext ctx;
        ctx.values["x0"] = std::to_string(target.x0);
        ctx.values["y0"] = std::to_string(target.y0);
        ctx.values["x1"] = std::to_string(target.x1);
        ctx.values["y1"] = std::to_string(target.y1);
        ctx.values["wgap"] = std::to_string(target.wgap);
        ctx.values["hgap"] = std::to_string(target.hgap);
        ctx.values["method"] = std::to_string(profile.method);
        ctx.values["threshold"] = std::to_string(profile.threshold);
        ctx.values["linegap"] = std::to_string(profile.linegap);
        ctx.values["fitmode"] = std::to_string(profile.fitmode);
        ctx.values["script_scale"] = std::to_string(profile.script_scale);
        ctx.values["filter_profile"] = std::to_string(profile.filter_profile);

        if (profile.has_explicit_filter)
        {
            ctx.values["optional_findline_filter"] =
                "m_line.setobjfilter(" + std::to_string(profile.objfilter) + ");\n"
                "m_line.setfilter(" + std::to_string(profile.filter_borw) + ", " +
                std::to_string(profile.filter_min) + ", " +
                std::to_string(profile.filter_max) + ");";
        }
        else
        {
            ctx.values["optional_findline_filter"] = "";
        }

        if (profile.has_gamma)
        {
            ctx.values["optional_findline_gamma"] =
                "m_line.setgamarate(" + std::to_string(profile.gamma) + ");";
        }
        else
        {
            ctx.values["optional_findline_gamma"] = "";
        }

        return ctx;
    }

    Stage25TemplateContext BuildFindcircleContext(
        const Stage25ImageTarget& target,
        const Stage25FindcircleProfile& profile)
    {
        Stage25TemplateContext ctx;
        ctx.values["cx"] = std::to_string(target.cx);
        ctx.values["cy"] = std::to_string(target.cy);
        ctx.values["px"] = std::to_string(target.px);
        ctx.values["py"] = std::to_string(target.py);
        ctx.values["gap"] = std::to_string(target.gap);
        ctx.values["linegap"] = std::to_string(target.linegap);
        ctx.values["method"] = std::to_string(profile.method);
        ctx.values["threshold"] = std::to_string(profile.threshold);

        if (profile.has_filter)
        {
            ctx.values["optional_findcircle_filter"] =
                "m_circle.setfindsetting(" + std::to_string(profile.findsetting) + ");\n"
                "m_circle.setfilter(" + std::to_string(profile.filter_borw) + ", " +
                std::to_string(profile.filter_min) + ", " +
                std::to_string(profile.filter_max) + ");";
        }
        else
        {
            ctx.values["optional_findcircle_filter"] = "";
        }

        if (profile.has_samplerate)
        {
            ctx.values["optional_findcircle_samplerate"] =
                "m_circle.setlinesamplerate(" + std::to_string(profile.samplerate) + ");";
        }
        else
        {
            ctx.values["optional_findcircle_samplerate"] = "";
        }

        return ctx;
    }

    std::string ClassifyQuality(const Stage25CaseResult& r)
    {
        if (!r.t0_pass) return "HEADLESS_FAILED";
        if (!r.t1_pass) return "ALGORITHM_NO_RESULT";

        double local_support = r.tool == "Findline" ?
            r.measured_local_support_score : r.circle_local_support_score;

        if (local_support >= 0.60) return "ORIGINAL_LOCAL_EDGE_CONFIRMED";

        return "GEOMETRY_MARGINAL_BUT_SAMPLED";
    }
}

bool RunStage25ManifestFile(
    const Stage25RunOptions& options,
    Stage25RunResult& result)
{
    std::filesystem::create_directories(options.out_root);

    auto manifest_dir = options.manifest_path.parent_path();
    auto templates_dir = manifest_dir / "templates";

    std::ifstream manifestFile(options.manifest_path);
    if (!manifestFile.is_open())
    {
        result.reason = "Cannot open manifest: " + options.manifest_path.string();
        return false;
    }

    std::stringstream buffer;
    buffer << manifestFile.rdbuf();
    std::string manifest_script = buffer.str();

    mu::Parser parser;
    parser.UsingClass(true);

    RegisterStage25CxScriptBindings(parser);

    try
    {
        parser.SetExpr(manifest_script);
        parser.Eval();
    }
    catch (const mu::Parser::exception_type& e)
    {
        result.reason = "Manifest parse error: " + std::string(e.GetMsg());
        return false;
    }

    Stage25Manifest& manifest = g_stage25_manifest;
    if (manifest.outroot.empty())
    {
        manifest.outroot = options.out_root.string();
    }

    std::vector<Stage25ImagePreflightResult> preflight_results;

    for (const auto& img : manifest.images)
    {
        for (const auto& target : img.targets)
        {
            auto preflight = Stage25ImagePreflight::Run(
                img.image_id,
                target.target_id,
                target.tool,
                img.level,
                img.path,
                target.tool == "Findline" ? target.x0 : target.cx,
                target.tool == "Findline" ? target.y0 : target.cy,
                target.tool == "Findline" ? target.x1 : target.px,
                target.tool == "Findline" ? target.y1 : target.py,
                target.wgap, target.hgap,
                target.gap, target.linegap);

            preflight_results.push_back(preflight);

            if (!preflight.roi_valid)
                continue;

            for (const auto& ev_profile : manifest.evidence_profiles)
            {
                if (target.tool == "Findline")
                {
                    for (const auto& profile : manifest.findline_profiles)
                    {
                        std::string case_id = GenerateCaseId(
                            img.image_id, target.target_id, profile.profile_id, ev_profile.name);

                        std::filesystem::path generated_script = options.out_root /
                            "generated_scripts" / img.image_id / target.target_id /
                            (profile.profile_id + ".cxsc");

                        Stage25TemplateContext ctx = BuildFindlineContext(target, profile);
                        Stage25TemplateEngine engine;
                        std::string reason;
                        engine.RenderFile(
                            templates_dir / "find_line_stage25_template.cxsc",
                            ctx, generated_script, reason);

                        std::filesystem::path case_dir = options.out_root /
                            img.level / img.image_id / target.target_id /
                            profile.profile_id / ev_profile.name;

                        CxScriptHeadlessOptions headlessOptions;
                        headlessOptions.enabled = true;
                        headlessOptions.image_path = img.path;
                        headlessOptions.script_path = generated_script.string();
                        headlessOptions.output_dir = case_dir.string();
                        headlessOptions.case_name = case_id;

                        CxScriptHeadlessResult headlessResult;
                        RunCxScriptHeadless(headlessOptions, headlessResult);

                        Stage25CaseResult case_result;
                        case_result.case_id = case_id;
                        case_result.image_id = img.image_id;
                        case_result.level = img.level;
                        case_result.target_id = target.target_id;
                        case_result.tool = target.tool;
                        case_result.profile_id = profile.profile_id;
                        case_result.evidence_profile = ev_profile.name;
                        case_result.policy_classification = profile.policy;

                        case_result.t0_pass = headlessResult.ok;
                        case_result.t1_pass = false;

                        if (headlessResult.ok)
                        {
                            std::ifstream summaryFile(headlessResult.summary_path);
                            if (summaryFile.is_open())
                            {
                                std::stringstream buffer;
                                buffer << summaryFile.rdbuf();
                                std::string summaryStr = buffer.str();

                                size_t pos = summaryStr.find("valid_points_count");
                                if (pos != std::string::npos)
                                {
                                    size_t start = summaryStr.find(":", pos) + 1;
                                    size_t end = summaryStr.find(",", start);
                                    case_result.valid_points_count =
                                        std::stoi(summaryStr.substr(start, end - start));
                                }

                                case_result.t1_pass =
                                    case_result.valid_points_count >= 2;
                            }
                        }

                        case_result.t2_pass = case_result.t1_pass &&
                            (case_result.measured_local_support_score >= 0.60);
                        case_result.quality_classification = ClassifyQuality(case_result);

                        result.case_results.push_back(case_result);
                    }
                }
                else if (target.tool == "Findcircle")
                {
                    for (const auto& profile : manifest.findcircle_profiles)
                    {
                        std::string case_id = GenerateCaseId(
                            img.image_id, target.target_id, profile.profile_id, ev_profile.name);

                        std::filesystem::path generated_script = options.out_root /
                            "generated_scripts" / img.image_id / target.target_id /
                            (profile.profile_id + ".cxsc");

                        Stage25TemplateContext ctx = BuildFindcircleContext(target, profile);
                        Stage25TemplateEngine engine;
                        std::string reason;
                        engine.RenderFile(
                            templates_dir / "find_circle_stage25_template.cxsc",
                            ctx, generated_script, reason);

                        std::filesystem::path case_dir = options.out_root /
                            img.level / img.image_id / target.target_id /
                            profile.profile_id / ev_profile.name;

                        CxScriptHeadlessOptions headlessOptions;
                        headlessOptions.enabled = true;
                        headlessOptions.image_path = img.path;
                        headlessOptions.script_path = generated_script.string();
                        headlessOptions.output_dir = case_dir.string();
                        headlessOptions.case_name = case_id;

                        CxScriptHeadlessResult headlessResult;
                        RunCxScriptHeadless(headlessOptions, headlessResult);

                        Stage25CaseResult case_result;
                        case_result.case_id = case_id;
                        case_result.image_id = img.image_id;
                        case_result.level = img.level;
                        case_result.target_id = target.target_id;
                        case_result.tool = target.tool;
                        case_result.profile_id = profile.profile_id;
                        case_result.evidence_profile = ev_profile.name;
                        case_result.policy_classification = profile.policy;

                        case_result.t0_pass = headlessResult.ok;
                        case_result.t1_pass = false;

                        if (headlessResult.ok)
                        {
                            case_result.t1_pass = true;
                        }

                        case_result.t2_pass = case_result.t1_pass &&
                            (case_result.circle_local_support_score >= 0.60);
                        case_result.quality_classification = ClassifyQuality(case_result);

                        result.case_results.push_back(case_result);
                    }
                }
            }
        }
    }

    result.total_cases = static_cast<int>(result.case_results.size());
    result.t0_pass = static_cast<int>(std::count_if(result.case_results.begin(),
        result.case_results.end(), [](const auto& r) { return r.t0_pass; }));
    result.t1_pass = static_cast<int>(std::count_if(result.case_results.begin(),
        result.case_results.end(), [](const auto& r) { return r.t1_pass; }));
    result.t2_pass = static_cast<int>(std::count_if(result.case_results.begin(),
        result.case_results.end(), [](const auto& r) { return r.t2_pass; }));

    Stage25ReportWriter::WriteBatchReport(options.out_root, result.case_results);
    Stage25ReportWriter::WritePreflightReport(options.out_root, preflight_results);
    Stage25ReportWriter::WriteCoverageReport(options.out_root, manifest);
    Stage25ReportWriter::WriteStabilityReport(options.out_root, result.case_results, manifest);
    Stage25ReportWriter::WritePolicyReport(options.out_root);

    result.ok = true;
    return true;
}