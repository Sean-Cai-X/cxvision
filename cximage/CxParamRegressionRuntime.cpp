#include "CxParamRegressionRuntime.h"
#include "CxParamRegressionRegister.h"
#include "muParser.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace
{
    std::string EscapeJson(const std::string& value)
    {
        std::string out;
        out.reserve(value.size() + 8);
        for (char ch : value)
        {
            switch (ch)
            {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(ch); break;
            }
        }
        return out;
    }

    CxParamRange RangeValues(
        const std::string& name,
        std::initializer_list<double> values,
        const std::string& role = "formal")
    {
        CxParamRange r;
        r.name = name;
        r.discrete_values.assign(values.begin(), values.end());
        if (!r.discrete_values.empty())
        {
            r.min_value = *std::min_element(r.discrete_values.begin(), r.discrete_values.end());
            r.max_value = *std::max_element(r.discrete_values.begin(), r.discrete_values.end());
        }
        r.step = 1.0;
        r.integer_only = true;
        r.enabled = true;
        r.role = role;
        return r;
    }

    int ValueAt(const CxParamRangeSet& ranges, const std::string& name, std::size_t index, int fallback)
    {
        for (const auto& r : ranges.ranges)
        {
            if (r.name == name && !r.discrete_values.empty())
            {
                return static_cast<int>(r.discrete_values[index % r.discrete_values.size()]);
            }
        }
        return fallback;
    }

    const CxParamCandidate* PickProfileCandidate(const std::vector<CxParamCandidate>& candidates)
    {
        if (candidates.empty())
            return nullptr;

        return &*std::min_element(
            candidates.begin(),
            candidates.end(),
            [](const CxParamCandidate& a, const CxParamCandidate& b)
            {
                if (a.selected_for_probe != b.selected_for_probe)
                    return a.selected_for_probe && !b.selected_for_probe;
                if (a.predicted_risk != b.predicted_risk)
                    return a.predicted_risk < b.predicted_risk;
                return a.candidate_id < b.candidate_id;
            });
    }
}

void CxParamRegressionRuntime::Clear()
{
    task = CxParamRegressionTask{};
    range_set = CxParamRangeSet{};
    candidates.clear();
    records.clear();
    optimization = CxParamOptimizationResult{};
}

CxParamRangeSet MakeConservativeRangeSet(const std::string& tool)
{
    CxParamRangeSet set;
    set.tool = tool;
    set.range_set_id = tool == "Findcircle"
        ? "findcircle_conservative_range"
        : "findline_conservative_range";
    set.max_candidates = 12;
    set.max_case_seconds = 10;
    set.max_total_seconds = 60;

    if (tool == "Findcircle")
    {
        set.ranges.push_back(RangeValues("threshold", {8, 12, 20, 30}));
        set.ranges.push_back(RangeValues("gap", {3, 5, 8, 12}));
        set.ranges.push_back(RangeValues("linegap", {4, 6, 10}));
        set.ranges.push_back(RangeValues("method", {0, 1}, "diagnostic"));
        set.ranges.push_back(RangeValues("samplerate", {1, 2, 4}, "diagnostic"));
    }
    else
    {
        set.ranges.push_back(RangeValues("threshold", {8, 12, 20, 30}));
        set.ranges.push_back(RangeValues("linegap", {4, 6, 10, 14}));
        set.ranges.push_back(RangeValues("wgap", {4, 8, 12}));
        set.ranges.push_back(RangeValues("hgap", {24, 32, 48}));
        set.ranges.push_back(RangeValues("filterprofile", {0, 1}));
        set.ranges.push_back(RangeValues("method", {0, 1}, "diagnostic"));
        set.ranges.push_back(RangeValues("samplerate", {1, 2}, "diagnostic"));
    }

    return set;
}

std::vector<CxParamCandidate> GenerateBasicParamCandidates(
    const CxParamRangeSet& ranges,
    int max_candidates)
{
    std::vector<CxParamCandidate> out;
    const int count = std::max(1, std::min(max_candidates, ranges.max_candidates));

    for (int i = 0; i < count; ++i)
    {
        CxParamCandidate c;
        c.candidate_id = i == 0 ? "manual_seed" : ("grid_" + std::to_string(i));
        c.source = i == 0 ? "manual_seed" : "grid";
        c.method = ValueAt(ranges, "method", i, 0);
        c.threshold = ValueAt(ranges, "threshold", i, 20);
        c.gap = ValueAt(ranges, "gap", i, 5);
        c.linegap = ValueAt(ranges, "linegap", i, 6);
        c.wgap = ValueAt(ranges, "wgap", i, 8);
        c.hgap = ValueAt(ranges, "hgap", i, 32);
        c.filterprofile = ValueAt(ranges, "filterprofile", i, 1);
        c.samplerate = ValueAt(ranges, "samplerate", i, 1);
        c.predicted_quality = 0.5;
        c.predicted_risk = i == 0 ? 0.25 : 0.5;
        c.predicted_failure_class = "pending_probe";
        c.selected_for_probe = i < 3;
        out.push_back(c);
    }

    return out;
}

bool ExportParamRegressionReports(
    const std::string& out_dir,
    const CxParamRegressionTask& task,
    const CxParamRangeSet& ranges,
    const std::vector<CxParamCandidate>& candidates,
    const std::vector<CxParamEvalRecord>& records,
    const std::vector<CxParamAccuracyStats>& stats,
    std::string& reason)
{
    namespace fs = std::filesystem;
    const fs::path root(out_dir);
    fs::create_directories(root);

    {
        std::ofstream file(root / "param_regression_task.json");
        if (!file.is_open()) { reason = "failed to write param_regression_task.json"; return false; }
        file << "{\n";
        file << "  \"task_id\": \"" << EscapeJson(task.task_id) << "\",\n";
        file << "  \"case_id\": \"" << EscapeJson(task.case_id) << "\",\n";
        file << "  \"image_id\": \"" << EscapeJson(task.image_id) << "\",\n";
        file << "  \"target_id\": \"" << EscapeJson(task.target_id) << "\",\n";
        file << "  \"tool\": \"" << EscapeJson(task.tool) << "\",\n";
        file << "  \"gauge_annotation_path\": \"" << EscapeJson(task.gauge_annotation_path) << "\",\n";
        file << "  \"require_manual_gauge\": " << (task.require_manual_gauge ? "true" : "false") << ",\n";
        file << "  \"allow_promote\": " << (task.allow_promote ? "true" : "false") << "\n";
        file << "}\n";
    }

    {
        std::ofstream json(root / "param_range_report.json");
        std::ofstream csv(root / "param_range_report.csv");
        std::ofstream md(root / "param_range_report.md");
        if (!json.is_open() || !csv.is_open() || !md.is_open())
        {
            reason = "failed to write parameter range reports";
            return false;
        }
        json << "{\n  \"range_set_id\": \"" << EscapeJson(ranges.range_set_id)
             << "\",\n  \"tool\": \"" << EscapeJson(ranges.tool) << "\",\n  \"ranges\": [\n";
        csv << "parameter,min,max,step,values,enabled,role\n";
        md << "# Parameter Range Report\n\n";
        md << "| Parameter | Min | Max | Step | Values | Enabled | Role |\n";
        md << "|---|---:|---:|---:|---|---|---|\n";
        for (std::size_t i = 0; i < ranges.ranges.size(); ++i)
        {
            const auto& r = ranges.ranges[i];
            std::ostringstream values;
            for (std::size_t j = 0; j < r.discrete_values.size(); ++j)
            {
                if (j) values << ",";
                values << r.discrete_values[j];
            }
            json << "    {\"name\":\"" << EscapeJson(r.name) << "\",\"min\":" << r.min_value
                 << ",\"max\":" << r.max_value << ",\"step\":" << r.step
                 << ",\"values\":\"" << EscapeJson(values.str()) << "\",\"enabled\":"
                 << (r.enabled ? "true" : "false") << ",\"role\":\"" << EscapeJson(r.role) << "\"}"
                 << (i + 1 < ranges.ranges.size() ? "," : "") << "\n";
            csv << r.name << "," << r.min_value << "," << r.max_value << "," << r.step
                << "," << values.str() << "," << (r.enabled ? "true" : "false")
                << "," << r.role << "\n";
            md << "| " << r.name << " | " << r.min_value << " | " << r.max_value
               << " | " << r.step << " | " << values.str() << " | "
               << (r.enabled ? "yes" : "no") << " | " << r.role << " |\n";
        }
        json << "  ]\n}\n";
    }

    {
        std::ofstream json(root / "param_candidates.json");
        std::ofstream csv(root / "param_candidates.csv");
        if (!json.is_open() || !csv.is_open())
        {
            reason = "failed to write candidate reports";
            return false;
        }
        json << "{\n  \"candidates\": [\n";
        csv << "candidate,source,method,threshold,gap,linegap,wgap,hgap,filterprofile,samplerate,predicted_quality,predicted_risk,selected\n";
        for (std::size_t i = 0; i < candidates.size(); ++i)
        {
            const auto& c = candidates[i];
            json << "    {\"candidate_id\":\"" << EscapeJson(c.candidate_id)
                 << "\",\"source\":\"" << EscapeJson(c.source)
                 << "\",\"method\":" << c.method
                 << ",\"threshold\":" << c.threshold
                 << ",\"gap\":" << c.gap
                 << ",\"linegap\":" << c.linegap
                 << ",\"wgap\":" << c.wgap
                 << ",\"hgap\":" << c.hgap
                 << ",\"filterprofile\":" << c.filterprofile
                 << ",\"samplerate\":" << c.samplerate
                 << ",\"predicted_quality\":" << c.predicted_quality
                 << ",\"predicted_risk\":" << c.predicted_risk
                 << ",\"selected_for_probe\":" << (c.selected_for_probe ? "true" : "false")
                 << "}" << (i + 1 < candidates.size() ? "," : "") << "\n";
            csv << c.candidate_id << "," << c.source << "," << c.method << ","
                << c.threshold << "," << c.gap << "," << c.linegap << ","
                << c.wgap << "," << c.hgap << "," << c.filterprofile << ","
                << c.samplerate << "," << c.predicted_quality << ","
                << c.predicted_risk << "," << (c.selected_for_probe ? "true" : "false") << "\n";
        }
        json << "  ]\n}\n";
    }

    {
        std::ofstream jsonl(root / "param_eval_records.jsonl");
        std::ofstream hitJson(root / "hit_distribution.json");
        std::ofstream hitCsv(root / "hit_distribution.csv");
        std::ofstream md(root / "param_hit_distribution_report.md");
        if (!jsonl.is_open() || !hitJson.is_open() || !hitCsv.is_open() || !md.is_open())
        {
            reason = "failed to write eval/hit distribution reports";
            return false;
        }
        hitJson << "{\n  \"schema\": \"param_hit_distribution_phase1\",\n  \"records\": [\n";
        hitCsv << "candidate,points,fit,mean_distance,support_score,failure_stage\n";
        md << "# Hit Distribution View\n\n";
        md << "Phase 1 placeholder: hit distribution bins will be populated from probe measure_points_xy.\n\n";
        md << "| Candidate | Points | Fit | MeanDist | FailureStage |\n";
        md << "|---|---:|---|---:|---|\n";
        for (std::size_t i = 0; i < records.size(); ++i)
        {
            const auto& r = records[i];
            jsonl << "{\"candidate_id\":\"" << EscapeJson(r.candidate_id)
                  << "\",\"case_id\":\"" << EscapeJson(r.case_id)
                  << "\",\"tool\":\"" << EscapeJson(r.tool)
                  << "\",\"executed\":" << (r.executed ? "true" : "false")
                  << ",\"timeout\":" << (r.timeout ? "true" : "false")
                  << ",\"points\":" << r.points
                  << ",\"fit_available\":" << (r.fit_available ? "true" : "false")
                  << ",\"support_score\":" << r.support_score
                  << ",\"mean_distance\":" << r.mean_distance
                  << ",\"fit_offset\":" << r.fit_offset
                  << ",\"failure_stage\":\"" << EscapeJson(r.failure_stage) << "\"}\n";
            hitJson << "    {\"candidate_id\":\"" << EscapeJson(r.candidate_id)
                    << "\",\"points\":" << r.points
                    << ",\"fit_available\":" << (r.fit_available ? "true" : "false")
                    << ",\"mean_distance\":" << r.mean_distance
                    << ",\"support_score\":" << r.support_score
                    << ",\"failure_stage\":\"" << EscapeJson(r.failure_stage) << "\"}"
                    << (i + 1 < records.size() ? "," : "") << "\n";
            hitCsv << r.candidate_id << "," << r.points << ","
                   << (r.fit_available ? "true" : "false") << ","
                   << r.mean_distance << "," << r.support_score << ","
                   << r.failure_stage << "\n";
            md << "| " << r.candidate_id << " | " << r.points << " | "
               << (r.fit_available ? "yes" : "no") << " | " << r.mean_distance
               << " | " << r.failure_stage << " |\n";
        }
        hitJson << "  ]\n}\n";
    }

    {
        std::ofstream json(root / "param_accuracy_matrix.json");
        std::ofstream csv(root / "param_accuracy_matrix.csv");
        std::ofstream md(root / "param_accuracy_matrix.md");
        if (!json.is_open() || !csv.is_open() || !md.is_open())
        {
            reason = "failed to write accuracy matrix reports";
            return false;
        }
        json << "{\n  \"schema\": \"param_accuracy_matrix_phase1\",\n  \"stats\": [\n";
        csv << "candidate,tool,total,executed,timeout,geometry_pass,evidence_pass,human_accept,geometry_rate,evidence_rate,human_rate,stability,risk\n";
        md << "# Accuracy / Stability Matrix\n\n";
        md << "| Candidate | Tool | Geometry | Evidence | Human | Stability | Risk |\n";
        md << "|---|---|---:|---:|---:|---:|---:|\n";
        for (std::size_t i = 0; i < stats.size(); ++i)
        {
            const auto& s = stats[i];
            json << "    {\"candidate_id\":\"" << EscapeJson(s.candidate_id)
                 << "\",\"tool\":\"" << EscapeJson(s.tool)
                 << "\",\"geometry_pass_rate\":" << s.geometry_pass_rate
                 << ",\"evidence_pass_rate\":" << s.evidence_pass_rate
                 << ",\"human_accept_rate\":" << s.human_accept_rate
                 << ",\"stability_score\":" << s.stability_score
                 << ",\"risk_score\":" << s.risk_score << "}"
                 << (i + 1 < stats.size() ? "," : "") << "\n";
            csv << s.candidate_id << "," << s.tool << "," << s.total_cases << ","
                << s.executed_cases << "," << s.timeout_cases << ","
                << s.geometry_pass << "," << s.evidence_pass << "," << s.human_accept << ","
                << s.geometry_pass_rate << "," << s.evidence_pass_rate << ","
                << s.human_accept_rate << "," << s.stability_score << "," << s.risk_score << "\n";
            md << "| " << s.candidate_id << " | " << s.tool << " | "
               << s.geometry_pass_rate << " | " << s.evidence_pass_rate
               << " | " << s.human_accept_rate << " | " << s.stability_score
               << " | " << s.risk_score << " |\n";
        }
        json << "  ]\n}\n";
    }

    {
        std::ofstream md(root / "param_candidate_distribution.md");
        if (!md.is_open())
        {
            reason = "failed to write candidate distribution report";
            return false;
        }
        md << "# Candidate Distribution\n\n";
        md << "| Candidate | Source | Threshold | Gap | LineGap | WGap | HGap | Filter | Method | Risk | Selected |\n";
        md << "|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---|\n";
        for (const auto& c : candidates)
        {
            md << "| " << c.candidate_id << " | " << c.source << " | "
               << c.threshold << " | " << c.gap << " | " << c.linegap << " | "
               << c.wgap << " | " << c.hgap << " | " << c.filterprofile
               << " | " << c.method << " | " << c.predicted_risk << " | "
               << (c.selected_for_probe ? "yes" : "no") << " |\n";
        }
    }

    {
        std::ofstream json(root / "param_optimization_trace.json");
        if (!json.is_open())
        {
            reason = "failed to write optimization trace";
            return false;
        }
        json << "{\n";
        json << "  \"schema\": \"param_optimization_trace_phase1\",\n";
        json << "  \"note\": \"mlpack/ensmallen are suggestion layers only; no automatic promotion.\",\n";
        json << "  \"steps\": [\n";
        json << "    {\"step\":\"manual_seed\",\"status\":\"available\"},\n";
        json << "    {\"step\":\"mlpack_rank\",\"status\":\"rule_based_placeholder\"},\n";
        json << "    {\"step\":\"ensmallen_opt\",\"status\":\"bounded_suggestion_placeholder\"}\n";
        json << "  ]\n";
        json << "}\n";
    }

    {
        std::ofstream md(root / "param_stability_report.md");
        if (!md.is_open())
        {
            reason = "failed to write stability report";
            return false;
        }
        md << "# Parameter Stability Report\n\n";
        md << "Phase 1 stability is single-anchor only. Mini-regression across L1/L2/L3 is required before promotion.\n\n";
        md << "| Candidate | Stability | Risk | Note |\n";
        md << "|---|---:|---:|---|\n";
        for (const auto& s : stats)
        {
            md << "| " << s.candidate_id << " | " << s.stability_score
               << " | " << s.risk_score << " | pending mini-regression |\n";
        }
    }

    {
        std::ofstream md(root / "param_recommendation_report.md");
        if (!md.is_open())
        {
            reason = "failed to write recommendation report";
            return false;
        }
        const CxParamCandidate* picked = PickProfileCandidate(candidates);
        md << "# Parameter Recommendation Report\n\n";
        md << "Recommendations are advisory only in phase 1.\n\n";
        md << "- Best Geometry: manual review required\n";
        md << "- Best Evidence: manual review required\n";
        md << "- Lowest Risk: " << (picked ? picked->candidate_id : "none") << "\n";
        md << "- Most Stable: requires mini-regression\n";
        md << "- Recommended Diagnostic: " << (picked ? picked->candidate_id : "none") << "\n";
    }

    {
        std::ofstream md(root / "param_profile_promotion_gate.md");
        if (!md.is_open())
        {
            reason = "failed to write promotion gate report";
            return false;
        }
        md << "# Profile Promotion Gate\n\n";
        md << "Can Promote: no\n\n";
        md << "Reason:\n\n";
        md << "- Phase 1 only exports range/candidate/evidence tables.\n";
        md << "- Mini-regression and human accept matrix are required before baseline promotion.\n";
        md << "- Candidates may be saved as diagnostic profile candidates only.\n";
    }

    {
        std::ofstream cxsc(root / "param_profile_candidate.cxsc");
        if (!cxsc.is_open())
        {
            reason = "failed to write param_profile_candidate.cxsc";
            return false;
        }
        const CxParamCandidate* picked = PickProfileCandidate(candidates);
        cxsc << "// Diagnostic profile candidate generated by parameter regression phase 1.\n";
        cxsc << "// Human review and mini-regression are required before promotion.\n\n";
        cxsc << "CxParameterProfile_reset(0);\n";
        cxsc << "CxParameterProfile_settool(\"" << EscapeJson(task.tool) << "\");\n";
        cxsc << "CxParameterProfile_add(\"" << EscapeJson(task.task_id) << "_diagnostic_candidate\");\n";
        cxsc << "CxParameterProfile_setrole(\"DIAGNOSTIC_CANDIDATE\");\n";
        if (picked)
        {
            cxsc << "CxParameterProfile_setmethod(" << picked->method << ");\n";
            cxsc << "CxParameterProfile_setthreshold(" << picked->threshold << ");\n";
            cxsc << "CxParameterProfile_setgap(" << picked->gap << ");\n";
            cxsc << "CxParameterProfile_setlinegap(" << picked->linegap << ");\n";
            cxsc << "CxParameterProfile_setwgap(" << picked->wgap << ");\n";
            cxsc << "CxParameterProfile_sethgap(" << picked->hgap << ");\n";
            cxsc << "CxParameterProfile_setfilterprofile(" << picked->filterprofile << ");\n";
        }
        cxsc << "CxParameterProfile_setdescription(\"phase1 diagnostic only; promotion disabled\");\n";
    }

    reason.clear();
    return true;
}

bool LoadCxParamRegressionFile(
    const std::string& script_path,
    CxParamRegressionRuntime& out_runtime,
    std::string& out_reason)
{
    namespace fs = std::filesystem;
    fs::path path(script_path);
    if (!fs::exists(path))
    {
        out_reason = "Param regression file not found: " + script_path;
        return false;
    }

    std::ifstream file(path);
    if (!file.is_open())
    {
        out_reason = "Cannot open param regression file: " + script_path;
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    mu::Parser parser;
    parser.UsingClass(true);
    RegisterCxParamRegressionBindings(parser);

    try
    {
        parser.SetExpr(buffer.str());
        parser.Eval();
    }
    catch (const mu::Parser::exception_type& e)
    {
        out_reason = "Param regression parse error: " + std::string(e.GetMsg());
        return false;
    }

    out_runtime = g_cxscript_param_regression;
    out_reason.clear();
    return true;
}
