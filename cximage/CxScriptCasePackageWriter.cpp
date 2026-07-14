#include "pch.h"
#include "CxScriptCasePackageWriter.h"
#include "ManualConsoleUtils.h"

#include <sstream>
#include <fstream>

bool SaveCasePackage(ManualTestContext& context, const std::filesystem::path& root)
{
  std::filesystem::create_directories(root);

  const std::filesystem::path scriptPath = root / "script.cxsc";
  if (!WriteTextFile(scriptPath, context.editor_text))
  {
    context.debug_reason = "Failed to save script.cxsc";
    return false;
  }

  const std::filesystem::path snapshotPath = root / "snapshot.txt";
  std::string snapshot;
  for (const auto& line : context.line_views)
  {
    snapshot += std::to_string(line.line_no) + ": " + line.statement + "\n";
  }
  if (!WriteTextFile(snapshotPath, snapshot))
  {
    context.debug_reason = "Failed to save snapshot.txt";
    return false;
  }

  const std::filesystem::path resultPath = root / "result_summary.json";
  std::string resultJson = "{\n";
  resultJson += "  \"case_id\": \"" + JsonEscape(context.active_script_case_name) + "\",\n";
  resultJson += "  \"image\": \"" + JsonEscape(context.image_file_path) + "\",\n";
  resultJson += "  \"tool\": \"" + JsonEscape(context.current_gauge.tool) + "\",\n";
  resultJson += "  \"status\": \"" + JsonEscape(context.current_result_ref.status) + "\",\n";
  resultJson += "  \"debug_action\": \"" + JsonEscape(context.debug_action) + "\",\n";
  resultJson += "  \"debug_status\": \"" + JsonEscape(context.debug_status) + "\",\n";
  resultJson += "  \"debug_reason\": \"" + JsonEscape(context.debug_reason) + "\"\n";
  resultJson += "}\n";
  if (!WriteTextFile(resultPath, resultJson))
  {
    context.debug_reason = "Failed to save result_summary.json";
    return false;
  }

  context.debug_action = "Save Case Package";
  context.debug_status = "PENDING";
  context.debug_reason = "Case package saved to: " + root.string();
  return true;
}
