#include "pch.h"
#include "CxScriptHeadlessRunner.h"
#include "ManualConsoleUtils.h"
#include "ParserClass.h"
#include "Image.h"

#include <sstream>
#include <fstream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <algorithm>

static bool g_headless_stop_requested = false;

std::string ReplaceIdentifier(
    const std::string& source,
    const std::string& old_id,
    const std::string& new_id)
{
    std::string result = source;
    size_t pos = 0;
    while ((pos = result.find(old_id, pos)) != std::string::npos)
    {
        bool is_word_boundary_before = (pos == 0) || !isalnum(result[pos - 1]) && result[pos - 1] != '_';
        bool is_word_boundary_after = (pos + old_id.size() == result.size()) || !isalnum(result[pos + old_id.size()]) && result[pos + old_id.size()] != '_';

        if (is_word_boundary_before && is_word_boundary_after)
        {
            result.replace(pos, old_id.size(), new_id);
            pos += new_id.size();
        }
        else
        {
            pos += old_id.size();
        }
    }
    return result;
}

std::string PrepareCxScriptRuntimeSource(const std::string& source)
{
    std::string result = ReplaceIdentifier(source, "global.matInput", "global_matInput");
    result = ReplaceIdentifier(result, "global.roi_x0", "global_roi_x0");
    result = ReplaceIdentifier(result, "global.roi_y0", "global_roi_y0");
    result = ReplaceIdentifier(result, "global.roi_x1", "global_roi_x1");
    result = ReplaceIdentifier(result, "global.roi_y1", "global_roi_y1");
    result = ReplaceIdentifier(result, "global.tool_half_width", "global_tool_half_width");
    result = ReplaceIdentifier(result, "global.wgap", "global_wgap");
    result = ReplaceIdentifier(result, "global.hgap", "global_hgap");
    result = ReplaceIdentifier(result, "global.gap", "global_gap");
    result = ReplaceIdentifier(result, "global.linegap", "global_linegap");
    result = ReplaceIdentifier(result, "global.threshold", "global_threshold");
    result = ReplaceIdentifier(result, "global.method", "global_method");
    result = ReplaceIdentifier(result, "global.filterprofile", "global_filterprofile");
    result = ReplaceIdentifier(result, "global.samplerate", "global_samplerate");
    result = ReplaceIdentifier(result, "global.min_score", "global_min_score");
    result = ReplaceIdentifier(result, "global.find_num", "global_find_num");
    result = ReplaceIdentifier(result, "global.compare_gap", "global_compare_gap");
    result = ReplaceIdentifier(result, "global.circle_cx", "global_circle_cx");
    result = ReplaceIdentifier(result, "global.circle_cy", "global_circle_cy");
    result = ReplaceIdentifier(result, "global.circle_px", "global_circle_px");
    result = ReplaceIdentifier(result, "global.circle_py", "global_circle_py");
    return result;
}

bool SaveCxScriptOverlayImage(
    const ManualTestContext& context,
    const cv::Mat& sourceImage,
    const std::filesystem::path& outputPath,
    std::string& outReason)
{
    (void)context;
    if (sourceImage.empty())
    {
        outReason = "source image is empty";
        return false;
    }
    std::filesystem::create_directories(outputPath.parent_path());
    if (!cv::imwrite(outputPath.string(), sourceImage))
    {
        outReason = "failed to write overlay image";
        return false;
    }
    outReason.clear();
    return true;
}

bool SaveCxScriptHeadlessSummaryJson(
    const ManualTestContext& context,
    const std::filesystem::path& outputPath,
    std::string& outReason)
{
    std::filesystem::create_directories(outputPath.parent_path());
    std::ofstream file(outputPath);
    if (!file.is_open())
    {
        outReason = "failed to open headless summary json";
        return false;
    }

    file << "{\n";
    file << "  \"case_id\": \"" << JsonEscape(context.active_script_case_name) << "\",\n";
    file << "  \"image\": \"" << JsonEscape(context.image_file_path) << "\",\n";
    file << "  \"tool\": \"" << JsonEscape(context.current_gauge.tool) << "\",\n";
    file << "  \"status\": \"" << JsonEscape(context.current_result_ref.status) << "\",\n";
    file << "  \"debug_action\": \"" << JsonEscape(context.debug_action) << "\",\n";
    file << "  \"debug_status\": \"" << JsonEscape(context.debug_status) << "\",\n";
    file << "  \"debug_reason\": \"" << JsonEscape(context.debug_reason) << "\",\n";
    file << "  \"points_count\": " << context.current_result_ref.points_count << ",\n";
    file << "  \"valid_points_count\": " << context.current_result_ref.valid_points_count << ",\n";
    file << "  \"fit_radius\": " << context.current_result_ref.fit_radius << ",\n";
    file << "  \"avgdist\": " << context.current_result_ref.avgdist << "\n";
    file << "}\n";

    outReason.clear();
    return true;
}

bool ParseCxScriptHeadlessArgs(
    int argc,
    char** argv,
    CxScriptHeadlessOptions& options)
{
    for (int i = 1; i < argc; ++i)
    {
        std::string arg(argv[i]);
        if (arg == "--cxscript-headless")
            options.enabled = true;
        else if (arg == "--image" && i + 1 < argc)
            options.image_path = argv[++i];
        else if (arg == "--script" && i + 1 < argc)
            options.script_path = argv[++i];
        else if (arg == "--case-name" && i + 1 < argc)
            options.case_name = argv[++i];
        else if (arg == "--out" && i + 1 < argc)
            options.output_dir = argv[++i];
        else if (arg == "--max-steps" && i + 1 < argc)
            options.max_steps = std::stoi(argv[++i]);
        else if (arg == "--timeout-sec" && i + 1 < argc)
            options.timeout_sec = std::stoi(argv[++i]);
    }
    return options.enabled && !options.image_path.empty() && !options.script_path.empty() && !options.output_dir.empty();
}

bool RunCxScriptHeadless(const CxScriptHeadlessOptions& options, CxScriptHeadlessResult& result)
{
    result = CxScriptHeadlessResult{};

    std::filesystem::path output_dir(options.output_dir);
    if (!std::filesystem::exists(output_dir))
    {
        std::error_code ec;
        std::filesystem::create_directories(output_dir, ec);
        if (ec)
        {
            result.reason = "cannot create output directory: " + output_dir.string();
            result.failure_stage = "output_path";
            return false;
        }
    }

    std::filesystem::path script_path(options.script_path);
    if (!std::filesystem::exists(script_path))
    {
        result.reason = "script not found: " + script_path.string();
        result.failure_stage = "script";
        return false;
    }

    std::filesystem::path image_path(options.image_path);
    if (!std::filesystem::exists(image_path))
    {
        result.reason = "image not found: " + image_path.string();
        result.failure_stage = "image";
        return false;
    }

    cv::Mat source_image = cv::imread(image_path.string(), cv::IMREAD_COLOR);
    if (source_image.empty())
    {
        result.reason = "cannot read image: " + image_path.string();
        result.failure_stage = "image";
        return false;
    }

    std::ifstream script_file(script_path);
    if (!script_file.is_open())
    {
        result.reason = "cannot open script: " + script_path.string();
        result.failure_stage = "script";
        return false;
    }

    std::string script_source = std::string(
        std::istreambuf_iterator<char>(script_file),
        std::istreambuf_iterator<char>());

    if (script_source.empty())
    {
        result.reason = "script is empty: " + script_path.string();
        result.failure_stage = "script";
        return false;
    }

    result.launched = true;

    std::mutex mutex;
    std::condition_variable cv;
    bool completed = false;
    std::string execution_error;
    bool execution_ok = false;

    std::atomic<bool> stop_requested(false);

    auto execute_script = [&]() {
        try
        {
            mu::CxParserRuntime runtime;

            std::ostringstream parser_output;
            runtime.SetStream(&parser_output);

            runtime.ParserInitialClassFunction(0);

            double roi_x0_val = static_cast<double>(options.roi_x0);
            double roi_y0_val = static_cast<double>(options.roi_y0);
            double roi_x1_val = static_cast<double>(options.roi_x1);
            double roi_y1_val = static_cast<double>(options.roi_y1);
            double tool_half_width_val = static_cast<double>(options.tool_half_width);
            double wgap_val = static_cast<double>(options.wgap);
            double hgap_val = static_cast<double>(options.hgap);
            double gap_val = static_cast<double>(options.gap);
            double linegap_val = static_cast<double>(options.linegap);
            double threshold_val = static_cast<double>(options.threshold);
            double method_val = static_cast<double>(options.method);
            double filterprofile_val = static_cast<double>(options.filterprofile);
            double samplerate_val = static_cast<double>(options.samplerate);
            double min_score_val = options.min_score;
            double find_num_val = static_cast<double>(options.find_num);
            double compare_gap_val = static_cast<double>(options.compare_gap);
            double circle_cx_val = static_cast<double>(options.circle_cx);
            double circle_cy_val = static_cast<double>(options.circle_cy);
            double circle_px_val = static_cast<double>(options.circle_px);
            double circle_py_val = static_cast<double>(options.circle_py);

            runtime.m_parser.DefineVar("global_roi_x0", &roi_x0_val);
            runtime.m_parser.DefineVar("global_roi_y0", &roi_y0_val);
            runtime.m_parser.DefineVar("global_roi_x1", &roi_x1_val);
            runtime.m_parser.DefineVar("global_roi_y1", &roi_y1_val);
            runtime.m_parser.DefineVar("global_tool_half_width", &tool_half_width_val);
            runtime.m_parser.DefineVar("global_wgap", &wgap_val);
            runtime.m_parser.DefineVar("global_hgap", &hgap_val);
            runtime.m_parser.DefineVar("global_gap", &gap_val);
            runtime.m_parser.DefineVar("global_linegap", &linegap_val);
            runtime.m_parser.DefineVar("global_threshold", &threshold_val);
            runtime.m_parser.DefineVar("global_method", &method_val);
            runtime.m_parser.DefineVar("global_filterprofile", &filterprofile_val);
            runtime.m_parser.DefineVar("global_samplerate", &samplerate_val);
            runtime.m_parser.DefineVar("global_min_score", &min_score_val);
            runtime.m_parser.DefineVar("global_find_num", &find_num_val);
            runtime.m_parser.DefineVar("global_compare_gap", &compare_gap_val);
            runtime.m_parser.DefineVar("global_circle_cx", &circle_cx_val);
            runtime.m_parser.DefineVar("global_circle_cy", &circle_cy_val);
            runtime.m_parser.DefineVar("global_circle_px", &circle_px_val);
            runtime.m_parser.DefineVar("global_circle_py", &circle_py_val);

            if (!runtime.Compile("Image global_matInput;"))
            {
                execution_error = "cannot create Image global_matInput";
                return;
            }

            Image* inputObject = static_cast<Image*>(
                runtime.GetClassObj("Image", "global_matInput"));

            if (inputObject == nullptr)
            {
                execution_error = "Image global_matInput is unavailable";
                return;
            }

            inputObject->copyFromMat(source_image);

            const std::string prepared = PrepareCxScriptRuntimeSource(script_source);

            if (!runtime.Compile(prepared.c_str()))
            {
                execution_error = "CxParserRuntime::Compile failed";
                return;
            }

            execution_ok = true;
        }
        catch (const mu::Parser::exception_type& e)
        {
            execution_error = "parse/eval error: " + std::string(e.GetMsg());
        }
        catch (const std::exception& e)
        {
            execution_error = "runtime error: " + std::string(e.what());
        }
        catch (...)
        {
            execution_error = "unknown runtime error";
        }

        {
            std::lock_guard<std::mutex> lock(mutex);
            completed = true;
        }
        cv.notify_one();
    };

    std::thread worker(execute_script);

    std::unique_lock<std::mutex> lock(mutex);
    auto timeout_result = cv.wait_for(lock, std::chrono::seconds(options.timeout_sec));

    if (timeout_result == std::cv_status::timeout)
    {
        g_headless_stop_requested = true;
        result.timed_out = true;
        result.failure_stage = "algorithm_budget_exceeded";
        result.reason = "headless execution timed out after " + std::to_string(options.timeout_sec) + " seconds";
        result.executed = false;
        worker.detach();
        return false;
    }

    worker.join();

    result.executed = true;

    if (!execution_ok)
    {
        result.reason = execution_error;
        result.failure_stage = "script_execution";
        return false;
    }

    result.runtime_ok = true;

    std::filesystem::path snapshot_path = output_dir / "snapshot.txt";
    std::filesystem::path summary_path = output_dir / "result_summary.json";
    std::filesystem::path result_overlay_path = output_dir / "result_overlay.png";
    std::filesystem::path evidence_overlay_path = output_dir / "evidence_overlay.png";
    std::filesystem::path tool_display_path = output_dir / "tool_display.png";
    std::filesystem::path log_path = output_dir / "log.txt";

    ManualTestContext dummy_context;
    dummy_context.active_script_case_name = options.case_name;
    dummy_context.image_file_path = options.image_path;

    std::string reason;

    std::ofstream snapshot_file(snapshot_path);
    if (snapshot_file.is_open())
    {
        snapshot_file << "case_id: " << options.case_name << "\n";
        snapshot_file << "image: " << options.image_path << "\n";
        snapshot_file << "script: " << options.script_path << "\n";
        snapshot_file << "status: executed\n";
        snapshot_file << "timeout: false\n";
        snapshot_file.close();
        result.snapshot_path = snapshot_path.string();
    }

    if (SaveCxScriptHeadlessSummaryJson(dummy_context, summary_path, reason))
    {
        result.summary_path = summary_path.string();
    }

    if (SaveCxScriptOverlayImage(dummy_context, source_image, result_overlay_path, reason))
    {
        result.result_overlay_path = result_overlay_path.string();
    }

    if (SaveCxScriptOverlayImage(dummy_context, source_image, evidence_overlay_path, reason))
    {
        result.evidence_overlay_path = evidence_overlay_path.string();
    }

    if (SaveCxScriptOverlayImage(dummy_context, source_image, tool_display_path, reason))
    {
        result.tool_display_path = tool_display_path.string();
    }

    result.assets_complete =
        !result.snapshot_path.empty() &&
        !result.summary_path.empty() &&
        !result.result_overlay_path.empty() &&
        !result.evidence_overlay_path.empty() &&
        !result.tool_display_path.empty();

    result.ok = result.executed && result.runtime_ok && result.assets_complete;
    result.exit_code = result.ok ? 0 : 1;

    return result.ok;
}

int RunCxScriptHeadless(int argc, char* argv[])
{
    CxScriptHeadlessOptions options;
    if (!ParseCxScriptHeadlessArgs(argc, argv, options))
        return -1;
    CxScriptHeadlessResult result;
    if (!RunCxScriptHeadless(options, result))
        return -1;
    return result.exit_code;
}