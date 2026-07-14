#include "pch.h"
#include "CxScriptHeadlessRunner.h"
#include "ManualConsoleUtils.h"

#include <sstream>
#include <fstream>

bool SaveCxScriptOverlayImage(
    const ManualTestContext& context,
    const cv::Mat& sourceImage,
    const std::filesystem::path& outputPath,
    std::string& outReason)
{
  (void)context;
  (void)sourceImage;
  (void)outputPath;
  (void)outReason;
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
    if (arg == "--image" && i + 1 < argc)
      options.image_path = argv[++i];
    else if (arg == "--script" && i + 1 < argc)
      options.script_path = argv[++i];
    else if (arg == "--case-name" && i + 1 < argc)
      options.case_name = argv[++i];
    else if (arg == "--out" && i + 1 < argc)
      options.output_dir = argv[++i];
    else if (arg == "--max-steps" && i + 1 < argc)
      options.max_steps = std::stoi(argv[++i]);
  }
  return !options.image_path.empty() && !options.script_path.empty();
}

bool RunCxScriptHeadless(const CxScriptHeadlessOptions& options, CxScriptHeadlessResult& result)
{
  (void)options;
  (void)result;
  return true;
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
