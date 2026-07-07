#include "CxScriptStage25ReportWriter.h"
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <algorithm>

void Stage25ReportWriter::WriteBatchReport(
    const std::filesystem::path& out_root,
    const std::vector<Stage25CaseResult>& results)
{
    std::ofstream file(out_root / "batch_report.md");

    file << "# Stage 2.5 L1~L3 Parameter Consistency Report\n\n";
    file << "## Summary\n\n";

    const int total = static_cast<int>(results.size());

    const int skipped =
        static_cast<int>(std::count_if(
            results.begin(),
            results.end(),
            [](const auto& r) { return r.skipped_by_preflight; }));

    const int executed = total - skipped;

    const int t0_pass =
        static_cast<int>(std::count_if(
            results.begin(),
            results.end(),
            [](const auto& r) {
                return !r.skipped_by_preflight && r.t0_pass;
            }));

    const int t1_pass =
        static_cast<int>(std::count_if(
            results.begin(),
            results.end(),
            [](const auto& r) {
                return !r.skipped_by_preflight && r.t1_pass;
            }));

    const int t2_pass =
        static_cast<int>(std::count_if(
            results.begin(),
            results.end(),
            [](const auto& r) {
                return !r.skipped_by_preflight && r.t2_pass;
            }));

    file << "- Total scheduled cases: " << total << "\n";
    file << "- Skipped by preflight: " << skipped << "\n";
    file << "- Executed cases: " << executed << "\n";
    file << "- T0 execution pass: " << t0_pass << "/" << executed << "\n";
    file << "- T1 algorithm pass: " << t1_pass << "/" << executed << "\n";
    file << "- T2 evidence support pass: " << t2_pass << "/" << executed << "\n\n";

    file << "## Findline Cases\n\n";
    file << "| Level | Image | Target | Profile | Points | Fit | T1 | T2 | LocalSupport | LocalMeanDist | Quality | Policy | Summary | Evidence |\n";
    file << "|---|---|---|---|---:|---|---|---|---:|---:|---|---|---|---|\n";

    for (const auto& r : results)
    {
        if (r.tool != "Findline") continue;
        file << "| " << r.level << " | " << r.image_id << " | " << r.target_id << " | "
             << r.profile_id << " | " << r.valid_points_count << " | " << (r.has_fit_line ? "true" : "false")
             << " | " << (r.t1_pass ? "true" : "false")
             << " | " << (r.t2_pass ? "true" : "false")
             << " | " << std::fixed << std::setprecision(3) << r.measured_local_support_score
             << " | " << r.measured_local_mean_distance_px
             << " | " << r.quality_classification << " | " << r.policy_classification << " | "
             << r.summary_path << " | " << r.evidence_summary_path << " |\n";
    }

    file << "\n## Findcircle Cases\n\n";
    file << "| Level | Image | Target | Profile | Points | FitCircle | T1 | T2 | LocalSupport | LocalMeanRadialDist | Quality | Policy | Summary | Evidence |\n";
    file << "|---|---|---|---|---:|---|---|---|---:|---:|---|---|---|---|\n";

    for (const auto& r : results)
    {
        if (r.tool != "Findcircle") continue;
        file << "| " << r.level << " | " << r.image_id << " | " << r.target_id << " | "
             << r.profile_id << " | " << r.valid_points_count << " | " << (r.has_fit_circle ? "true" : "false")
             << " | " << (r.t1_pass ? "true" : "false")
             << " | " << (r.t2_pass ? "true" : "false")
             << " | " << std::fixed << std::setprecision(3) << r.circle_local_support_score
             << " | " << r.circle_local_mean_radial_distance_px
             << " | " << r.quality_classification << " | " << r.policy_classification << " | "
             << r.summary_path << " | " << r.evidence_summary_path << " |\n";
    }

    file << "\n## Skipped By Preflight\n\n";
    file << "| Level | Image | Target | Tool | Reason |\n";
    file << "|---|---|---|---|---|\n";

    for (const auto& r : results)
    {
        if (!r.skipped_by_preflight) continue;
        file << "| " << r.level << " | " << r.image_id << " | " << r.target_id << " | "
             << r.tool << " | " << r.skip_reason << " |\n";
    }
}

void Stage25ReportWriter::WritePreflightReport(
    const std::filesystem::path& out_root,
    const std::vector<Stage25ImagePreflightResult>& results)
{
    std::ofstream file(out_root / "image_preflight_report.md");

    file << "# Stage 2.5 L1~L3 Image Preflight Report\n\n";
    file << "## Summary\n\n";

    int total = static_cast<int>(results.size());
    int ok = static_cast<int>(std::count_if(results.begin(), results.end(),
        [](const auto& r) { return r.preflight_class == "OK"; }));
    int warning = static_cast<int>(std::count_if(results.begin(), results.end(),
        [](const auto& r) { return r.preflight_class == "WARNING"; }));
    int invalid = static_cast<int>(std::count_if(results.begin(), results.end(),
        [](const auto& r) { return !r.roi_valid; }));

    file << "- Total targets: " << total << "\n";
    file << "- OK: " << ok << "\n";
    file << "- With warnings: " << warning << "\n";
    file << "- Invalid (not run): " << invalid << "\n\n";

    file << "## Preflight Details\n\n";
    file << "| Level | Image | Target | Tool | ROIValid | PreflightClass | Contrast | GradientMean | BlurScore |\n";
    file << "|---|---|---|---|---|---|---:|---:|---:|\n";

    for (const auto& r : results)
    {
        file << "| " << r.level << " | " << r.image_id << " | " << r.target_id << " | "
             << r.tool << " | " << (r.roi_valid ? "true" : "false") << " | "
             << r.preflight_class << " | " << std::fixed << std::setprecision(0)
             << (r.image_loaded ? (r.gray_std * 2.0) : 0) << " | "
             << std::setprecision(2) << r.gradient_mean << " | "
             << r.blur_score << " |\n";
    }
}

void Stage25ReportWriter::WriteCoverageReport(
    const std::filesystem::path& out_root,
    const Stage25Manifest& manifest)
{
    std::ofstream file(out_root / "image_coverage_report.md");

    file << "# Stage 2.5 L1~L3 Image Coverage Report\n\n";
    file << "## Summary\n\n";

    std::map<std::string, int> image_count_by_level;
    std::map<std::string, int> findline_target_count_by_level;
    std::map<std::string, int> findcircle_target_count_by_level;

    int total_images = 0;
    int total_findline = 0;
    int total_findcircle = 0;

    for (const auto& img : manifest.images)
    {
        image_count_by_level[img.level]++;
        total_images++;

        for (const auto& target : img.targets)
        {
            if (target.tool == "Findline")
            {
                findline_target_count_by_level[img.level]++;
                total_findline++;
            }
            else if (target.tool == "Findcircle")
            {
                findcircle_target_count_by_level[img.level]++;
                total_findcircle++;
            }
        }
    }

    file << "- Total images: " << total_images << "\n";
    file << "- Total Findline targets: " << total_findline << "\n";
    file << "- Total Findcircle targets: " << total_findcircle << "\n\n";

    bool l1_ok = findline_target_count_by_level["L1_high_contrast"] >= 2 &&
                 findcircle_target_count_by_level["L1_high_contrast"] >= 2;
    bool l2_ok = findline_target_count_by_level["L2_low_contrast_illumination"] >= 2 &&
                 findcircle_target_count_by_level["L2_low_contrast_illumination"] >= 2;
    bool l3_ok = findline_target_count_by_level["L3_complex_boundary"] >= 2 &&
                 findcircle_target_count_by_level["L3_complex_boundary"] >= 2;

    std::string coverage_status = (l1_ok && l2_ok && l3_ok) ? "OK" : "INSUFFICIENT_COVERAGE";
    file << "- Coverage status: " << coverage_status << "\n\n";

    file << "## Coverage Details\n\n";
    file << "| Level | ImageCount | FindlineTargets | FindcircleTargets | Coverage |\n";
    file << "|---|---:|---:|---:|---|\n";

    for (const auto& [level, count] : image_count_by_level)
    {
        std::string status = "OK";
        if (level == "L1_high_contrast") status = l1_ok ? "OK" : "INSUFFICIENT";
        else if (level == "L2_low_contrast_illumination") status = l2_ok ? "OK" : "INSUFFICIENT";
        else if (level == "L3_complex_boundary") status = l3_ok ? "OK" : "INSUFFICIENT";

        file << "| " << level << " | " << count << " | "
             << findline_target_count_by_level[level] << " | "
             << findcircle_target_count_by_level[level] << " | "
             << status << " |\n";
    }
}

void Stage25ReportWriter::WriteStabilityReport(
    const std::filesystem::path& out_root,
    const std::vector<Stage25CaseResult>& results,
    const Stage25Manifest& manifest)
{
    std::ofstream file(out_root / "parameter_stability_report.md");

    file << "# Stage 2.5 L1~L3 Parameter Stability Report\n\n";

    for (const std::string& tool : {"Findline", "Findcircle"})
    {
        std::vector<Stage25CaseResult> tool_results;
        std::copy_if(results.begin(), results.end(), std::back_inserter(tool_results),
            [&tool](const auto& r) { return r.tool == tool && !r.skipped_by_preflight; });

        std::map<std::string, std::vector<Stage25CaseResult>> profile_results;
        for (const auto& r : tool_results)
        {
            profile_results[r.profile_id].push_back(r);
        }

        file << "## " << tool << " Stability By Level\n\n";
        file << "| Profile | Level | Targets | OriginalSuccess | LocalConfirmed | MeanLocalSupport | MeanLocalDist |\n";
        file << "|---|---|---:|---:|---:|---:|---:|\n";

        for (const auto& [profile_id, prof_results] : profile_results)
        {
            std::map<std::string, std::vector<Stage25CaseResult>> level_results;
            for (const auto& r : prof_results)
            {
                level_results[r.level].push_back(r);
            }

            for (const auto& [level, lev_results] : level_results)
            {
                int total = static_cast<int>(lev_results.size());
                int success = static_cast<int>(std::count_if(lev_results.begin(), lev_results.end(),
                    [](const auto& r) { return r.t1_pass; }));
                int confirmed = static_cast<int>(std::count_if(lev_results.begin(), lev_results.end(),
                    [](const auto& r) { return r.quality_classification == "ORIGINAL_LOCAL_EDGE_CONFIRMED"; }));

                double mean_support = 0.0;
                double mean_dist = 0.0;

                for (const auto& r : lev_results)
                {
                    mean_support += (tool == "Findline") ? r.measured_local_support_score : r.circle_local_support_score;
                    mean_dist += (tool == "Findline") ? r.measured_local_mean_distance_px : r.circle_local_mean_radial_distance_px;
                }

                if (total > 0)
                {
                    mean_support /= total;
                    mean_dist /= total;
                }

                file << "| " << profile_id << " | " << level << " | " << total
                     << " | " << success << "/" << total << " | " << confirmed << "/" << total
                     << " | " << std::fixed << std::setprecision(3) << mean_support
                     << " | " << mean_dist << " |\n";
            }
        }

        file << "\n## " << tool << " Overall Stability\n\n";
        file << "| Profile | Levels | Targets | Score | Recommendation |\n";
        file << "|---|---|---:|---:|---|\n";

        for (const auto& [profile_id, prof_results] : profile_results)
        {
            int total = static_cast<int>(prof_results.size());
            std::set<std::string> level_set;
            for (const auto& r : prof_results)
                level_set.insert(r.level);
            int levels = static_cast<int>(level_set.size());

            int success = static_cast<int>(std::count_if(prof_results.begin(), prof_results.end(),
                [](const auto& r) { return r.t1_pass; }));
            int confirmed = static_cast<int>(std::count_if(prof_results.begin(), prof_results.end(),
                [](const auto& r) { return r.quality_classification == "ORIGINAL_LOCAL_EDGE_CONFIRMED"; }));

            double mean_support = 0.0;
            for (const auto& r : prof_results)
            {
                mean_support += (tool == "Findline") ? r.measured_local_support_score : r.circle_local_support_score;
            }
            if (total > 0) mean_support /= total;

            double success_rate = total > 0 ? static_cast<double>(success) / total : 0.0;
            double confirmed_rate = total > 0 ? static_cast<double>(confirmed) / total : 0.0;

            double score = success_rate * 30.0 + confirmed_rate * 30.0 + mean_support * 20.0;

            std::string recommendation = "BASELINE_ONLY";
            if (total >= 6 && levels >= 2)
            {
                if (score >= 80.0) recommendation = "PROFILE_RECOMMENDED";
                else if (score >= 65.0) recommendation = "PROFILE_CONDITIONALLY_RECOMMENDED";
                else if (success_rate < 0.5) recommendation = "PROFILE_UNSTABLE";
                else recommendation = "IMAGE_SPECIFIC_PROFILE";
            }

            file << "| " << profile_id << " | " << levels << " | " << total
                 << " | " << std::fixed << std::setprecision(1) << score
                 << " | " << recommendation << " |\n";
        }
    }
}

void Stage25ReportWriter::WritePolicyReport(
    const std::filesystem::path& out_root)
{
    std::ofstream file(out_root / "parameter_policy_report.md");

    file << "# Stage 2.5 L1~L3 Parameter Policy Decision\n\n";
    file << "## Current Policy\n\n";
    file << "| Parameter/Profile | Current Status | Decision | Reason |\n";
    file << "|---|---|---|---|\n";
    file << "| Findline legacy filter_min=50 | preserved | keep as legacy | needed for original compare |\n";
    file << "| Findline stage25_filter20 | candidate | keep as test profile | produces points and matches component stats |\n";
    file << "| Findline threshold8 | candidate | keep as weak edge candidate | may perform better on L2 low contrast |\n";
    file << "| Findline linegap10 | candidate | keep for comparison | fewer points but may reduce interference |\n";
    file << "| Findline filter_relax_min1 | debug | do not promote | too permissive |\n";
    file << "| Findline gamma | risky | do not promote | binary collapses to large component |\n";
    file << "| Findcircle direct | candidate | keep as primary | currently most stable |\n";
    file << "| Findcircle threshold8 | candidate | keep as weak edge candidate |\n";
    file << "| Findcircle method1 | risky | do not promote | polarity risk observed |\n";
    file << "| Findcircle filter_relax | debug | do not promote | may introduce false positives |\n";
    file << "| FastMatch | not evaluated | deferred | not in current test scope |\n";
}

void Stage25ReportWriter::WriteCaseFileIndex(
    const std::filesystem::path& out_root,
    const std::vector<Stage25CaseResult>& results)
{
    std::ofstream file(out_root / "case_file_index.md");

    file << "# Stage25 Case File Index\n\n";
    file << "| CaseId | Level | Image | Target | Tool | Profile | GeneratedScript | Snapshot | Summary | EvidenceSummary |\n";
    file << "|---|---|---|---|---|---|---|---|---|---|\n";

    for (const auto& r : results)
    {
        file << "| " << r.case_id << " | " << r.level << " | " << r.image_id << " | "
             << r.target_id << " | " << r.tool << " | " << r.profile_id << " | "
             << r.generated_script_path << " | " << r.snapshot_path << " | "
             << r.summary_path << " | " << r.evidence_summary_path << " |\n";
    }
}

static std::string DiagnoseStage25Case(const Stage25CaseResult& r)
{
    if (r.skipped_by_preflight)
        return "Skipped by preflight: " + r.skip_reason;

    if (!r.t0_pass)
        return "Headless execution failed";

    if (!r.summary_exists)
        return "Missing result_summary.json";

    if (!r.evidence_summary_exists)
        return "Missing evidence_summary.json";

    const double support =
        r.tool == "Findline"
            ? r.measured_local_support_score
            : r.circle_local_support_score;

    if (!r.t1_pass && support >= 0.60)
    {
        return "Image edge exists but original Measure produced no fitted result; check generated cxscript, ROI geometry, tool parameters, and SetLine/SetCircle mapping.";
    }

    if (!r.t1_pass)
    {
        return "Original Measure produced no result; check failure_stage/failure_reason and ROI.";
    }

    if (!r.t2_pass)
    {
        return "Original Measure produced result but local evidence support is weak.";
    }

    return "OK";
}

void Stage25ReportWriter::WriteDiagnosticReport(
    const std::filesystem::path& out_root,
    const std::vector<Stage25CaseResult>& results)
{
    std::ofstream file(out_root / "case_diagnostic_report.md");

    file << "# Stage25 Case Diagnostic Report\n\n";
    file << "## Summary\n\n";

    const int total = static_cast<int>(results.size());
    const int skipped = static_cast<int>(std::count_if(results.begin(), results.end(),
        [](const auto& r) { return r.skipped_by_preflight; }));
    const int executed = total - skipped;

    const int headless_failed = static_cast<int>(std::count_if(results.begin(), results.end(),
        [](const auto& r) { return !r.skipped_by_preflight && !r.t0_pass; }));

    const int no_result = static_cast<int>(std::count_if(results.begin(), results.end(),
        [](const auto& r) { return !r.skipped_by_preflight && r.t0_pass && !r.t1_pass; }));

    const int no_result_with_edge = static_cast<int>(std::count_if(results.begin(), results.end(),
        [](const auto& r) { return r.quality_classification == "ALGORITHM_NO_RESULT_WITH_IMAGE_EDGE"; }));

    const int ok = static_cast<int>(std::count_if(results.begin(), results.end(),
        [](const auto& r) { return r.t2_pass; }));

    file << "- Total scheduled cases: " << total << "\n";
    file << "- Skipped by preflight: " << skipped << "\n";
    file << "- Executed cases: " << executed << "\n";
    file << "- Headless failed: " << headless_failed << "\n";
    file << "- Algorithm no result: " << no_result << "\n";
    file << "- Algorithm no result but image edge detected: " << no_result_with_edge << "\n";
    file << "- Fully verified (T2 pass): " << ok << "\n\n";

    file << "## Case Diagnostics\n\n";
    file << "| CaseId | Tool | Profile | Points | Fit | FailureStage | FailureReason | LocalSupport | Diagnosis |\n";
    file << "|---|---|---|---:|---|---|---|---:|---|\n";

    for (const auto& r : results)
    {
        const double support =
            r.tool == "Findline"
                ? r.measured_local_support_score
                : r.circle_local_support_score;

        file << "| " << r.case_id << " | " << r.tool << " | " << r.profile_id << " | "
             << r.valid_points_count << " | "
             << (r.tool == "Findline" ? (r.has_fit_line ? "true" : "false") : (r.has_fit_circle ? "true" : "false"))
             << " | " << r.failure_stage << " | " << r.failure_reason << " | "
             << std::fixed << std::setprecision(3) << support << " | "
             << DiagnoseStage25Case(r) << " |\n";
    }
}