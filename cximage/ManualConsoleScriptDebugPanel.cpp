#include "pch.h"
#include "ManualConsoleScriptDebugPanel.h"
#include "ManualConsoleCxScriptDebug.h"
#include "ManualConsoleGauge.h"
#include "ManualConsoleUtils.h"
#include "ManualStateTestConsole.h"
#include "CxCrashLogHandler.h"
#include "CxScriptCasePackageWriter.h"
#include "CxUnifiedLog.h"

#include <cctype>
#include <filesystem>
#include <cstring>
#include <unordered_set>

namespace
{
// A catalog/flow script is a file-backed asset.  When the editor has not been
// changed by the operator, execute the current file content rather than a
// stale copy retained from an earlier catalog load.  This is especially
// important after a CxScript calling-convention migration: otherwise Run can
// recreate geometry using the old method argument order and then appear to
// "undo" the Key Parameter Controls values.
bool RefreshCleanEditorFromSource(ManualTestContext& context,
                                  std::string& reason)
{
  reason.clear();
  if (context.editor_dirty || context.loaded_script_path.empty())
    return true;

  const std::filesystem::path sourcePath =
      ResolveWorkspaceFile(context.loaded_script_path);
  std::string diskText;
  if (!ReadTextFile(sourcePath.string(), diskText))
  {
    reason = "cannot refresh clean Script Editor from " +
             sourcePath.string();
    return false;
  }

  if (diskText != context.editor_text)
  {
    context.editor_text = diskText;
    context.analyzed_text.clear();
    reason = "clean Script Editor refreshed from current file before Run";
  }
  return true;
}

// These snapshots were produced before the public CxScript FindCircle
// convention was normalized to (cx, cy, innerRadius, outerRadius) and
// (cx, cy, px, py), and before scan-sector globals were part of the direct
// script contract.  Preserve the historical file as evidence, but do not run
// it with a different parser convention.  Migration is deliberately narrow:
// rewrite the two known generated statements and, when absent, add the one
// public setscanarc() call immediately before measure().
bool MigrateLegacyFindCircleCallsForRun(ManualTestContext& context,
                                        std::string& reason)
{
  reason.clear();
  if (context.editor_text.find("FindCircle") == std::string::npos)
    return false;

  struct Replacement
  {
    const char* legacy;
    const char* canonical;
  };
  static const Replacement replacements[] = {
      {"m_circle.setannulus(outer_radius, inner_radius, cy, cx);",
       "m_circle.setannulus(cx, cy, inner_radius, outer_radius);"},
      {"m_circle.setcircle(py, px, cy, cx);",
       "m_circle.setcircle(cx, cy, px, py);"},
  };

  bool changed = false;
  bool callOrderChanged = false;
  bool scanArcAdded = false;
  for (const Replacement& replacement : replacements)
  {
    std::size_t pos = 0;
    while ((pos = context.editor_text.find(replacement.legacy, pos)) !=
           std::string::npos)
    {
      context.editor_text.replace(
          pos,
          std::strlen(replacement.legacy),
          replacement.canonical);
      pos += std::strlen(replacement.canonical);
      changed = true;
      callOrderChanged = true;
    }
  }

  // Historical Evidence snapshots do not know about the scan-sector input.
  // Running such a snapshot creates a fresh full-circle runtime object, which
  // then replaces the correctly edited sector in Image View.  Resolve the
  // declared FindCircle object name and make the missing public method call
  // explicit in the in-memory working revision.  Do not modify the immutable
  // evidence snapshot on disk.
  if (context.editor_text.find(".setscanarc(") == std::string::npos)
  {
    const std::size_t typePos = context.editor_text.find("FindCircle");
    std::size_t nameBegin = typePos + std::strlen("FindCircle");
    while (nameBegin < context.editor_text.size() &&
           std::isspace(static_cast<unsigned char>(context.editor_text[nameBegin])))
      ++nameBegin;

    std::size_t nameEnd = nameBegin;
    while (nameEnd < context.editor_text.size())
    {
      const unsigned char ch =
          static_cast<unsigned char>(context.editor_text[nameEnd]);
      if (!std::isalnum(ch) && ch != '_')
        break;
      ++nameEnd;
    }

    if (nameEnd > nameBegin)
    {
      const std::string objectName =
          context.editor_text.substr(nameBegin, nameEnd - nameBegin);
      const std::string measureCall = objectName + ".measure(";
      const std::size_t measurePos = context.editor_text.find(measureCall);
      if (measurePos != std::string::npos)
      {
        const std::string scanArcCall =
            objectName +
            ".setscanarc(global_findcircle_arc_start_deg, "
            "global_findcircle_arc_end_deg, "
            "global_findcircle_arc_enabled);\n\n";
        context.editor_text.insert(measurePos, scanArcCall);
        changed = true;
        scanArcAdded = true;
      }
    }
  }

  if (!changed)
    return false;

  // This text now differs from the immutable evidence snapshot.  It must not
  // be silently treated as a saved/verified script.  Save As Candidate creates
  // a new evidence package if the operator wants to retain it.
  context.editor_dirty = true;
  context.analyzed_text.clear();
  reason = "legacy FindCircle working revision migrated in memory (call_order=" +
           std::string(callOrderChanged ? "yes" : "no") +
           ", scan_arc_call=" + std::string(scanArcAdded ? "added" : "present") +
           "); historical script_snapshot.cxsc was not modified";
  return true;
}
} // namespace

void ViewController::DrawScriptEditorBlock(ManualTestContext& context)
{
  SetCxCrashBreadcrumb("drawManualStateTestConsole:ScriptEditor:begin");
  if (!ImGui::CollapsingHeader("Script Editor", ImGuiTreeNodeFlags_DefaultOpen))
    return;

  ImGui::PushID("script_editor");

  const float toolbarHeight = 30.0f;
  ImGui::BeginChild("script_editor_toolbar", ImVec2(-1, toolbarHeight), false);

  ImGui::PushItemWidth(150.0f);
  int source_index = 0;
  if (context.editor_source == "catalog") source_index = 1;
  else if (context.editor_source == "direct_file") source_index = 2;
  else if (context.editor_source == "semantic_flow") source_index = 3;
  if (ImGui::Combo("##source", &source_index,
                   "manual\0catalog\0direct_file\0semantic_flow\0"))
  {
    if (source_index == 0) context.editor_source = "manual";
    else if (source_index == 1) context.editor_source = "catalog";
    else if (source_index == 2) context.editor_source = "direct_file";
    else if (source_index == 3) context.editor_source = "semantic_flow";
  }
  ImGui::PopItemWidth();

  ImGui::SameLine();
  if (ImGui::Button("Open"))
  {
    context.debug_action = "Open Script";
    context.debug_status = "PENDING";
    context.debug_reason = "Open file dialog placeholder";
  }

  ImGui::SameLine();
  if (ImGui::Button("Save"))
  {
    // There is no direct file write here.  Do not mark an unsaved in-memory
    // edit as clean, otherwise a later Run would legitimately reload the
    // catalog source and silently discard that edit.
    context.debug_action = "Save Script";
    context.debug_status = "EDITOR_SAVE_NOT_PERSISTED";
    context.debug_reason =
        "Script Editor has no direct source-file save; use Save As Candidate "
        "to preserve this edited text.";
  }

  ImGui::SameLine();
  if (ImGui::Button("Save As Candidate"))
  {
    CxEvidenceCandidateSaveOptions options;
    options.mode = "script_editor_draft";
    options.request_run = false;
    CxEvidenceCandidateSaveResult result;
    if (!SaveEvidenceCandidatePackage(context, options, result))
    {
      context.debug_action = "Save Script Candidate";
      context.debug_status = "EVIDENCE_CANDIDATE_SAVE_FAILED";
      context.debug_reason = result.reason;
    }
  }

  ImGui::SameLine();
  if (context.editor_dirty)
    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "*");

  if (!context.loaded_script_path.empty())
    ImGui::TextWrapped("Loaded: %s", context.loaded_script_path.c_str());

  ImGui::EndChild();

  // Keep the script editor as a bounded edit area.  Using (-1, -1) here makes
  // the multiline editor consume all remaining parent space, so long scripts
  // visually stretch the Manual State Test Console and push the compiler/result
  // blocks away.  Long code should scroll inside this child instead.
  const float editorHeight = ImGui::GetTextLineHeightWithSpacing() * 14.0f;
  ImGui::BeginChild("script_editor_text",
                    ImVec2(-1, editorHeight),
                    true,
                    ImGuiWindowFlags_HorizontalScrollbar);
  if (InputTextMultilineString(
        "##script_text", context.editor_text, ImVec2(-1, -1)))
  {
    context.editor_dirty = true;
    context.analyzed_text.clear();
  }
  ImGui::EndChild();

  ImGui::PopID();
  SetCxCrashBreadcrumb("drawManualStateTestConsole:ScriptEditor:end");
}

void ViewController::DrawScriptDebugCompilerBlock(ManualTestContext& context)
{
  SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:begin");
  if (!ImGui::CollapsingHeader("Debug Compiler", ImGuiTreeNodeFlags_DefaultOpen))
    return;

  ImGui::PushID("debug_compiler");
  SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:push_id");

  const float btnWidth = 90.0f;

  SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:header_text");
  ImGui::TextDisabled(
    "Compile: source preflight/line analysis; Run: execute exact editor text via CxParserRuntime::Compile");

  SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:compile_button");
  if (ImGui::Button("Compile", ImVec2(btnWidth, 0)))
  {
    context.debug_action = "Compile";
    if (context.editor_text.empty())
    {
      context.debug_status = "compile_failed";
      context.debug_reason = "Script Editor is empty";
    }
    else
    {
      AnalyzeScript(context);
      context.debug_status = "source_analyzed";
      context.debug_reason =
        "CxScript source analyzed; Run executes the exact editor text";
    }
  }

  ImGui::SameLine();
  SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:run_button");
  const bool runRequestedByCandidateSave =
      !context.pending_execution_candidate_id.empty() &&
      context.pending_execution_candidate_id ==
          context.last_evidence_candidate_id;
  const bool runRequestedByKeyParameterControls =
      context.debug_action == "Key Parameter Controls Run Script" &&
      context.debug_status == "MANUAL_RUN_REQUESTED";
  const bool usePendingExecutionSnapshot =
      (runRequestedByCandidateSave || runRequestedByKeyParameterControls) &&
      context.has_pending_execution_snapshot;
  if (ImGui::Button("Run", ImVec2(btnWidth, 0)) ||
      runRequestedByCandidateSave ||
      runRequestedByKeyParameterControls)
  {
    SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:Run:begin");
    context.debug_action = "Run";
    if (context.editor_text.empty())
    {
      context.run_state = "failed";
      context.debug_status = "run_failed";
      context.debug_reason = "Script Editor is empty";
    }
    else
    {
      std::string sourceRefreshReason;
      if (!RefreshCleanEditorFromSource(context, sourceRefreshReason))
      {
        context.run_state = "failed";
        context.debug_status = "run_failed";
        context.debug_reason = sourceRefreshReason;
        ImGui::PopID();
        SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:Run:source_refresh_failed");
        return;
      }

      std::string legacyMigrationReason;
      const bool legacyFindCircleMigrated =
          MigrateLegacyFindCircleCallsForRun(context, legacyMigrationReason);
      if (legacyFindCircleMigrated)
      {
        CXLOG_WARN(
            "ManualConsole",
            "legacy_findcircle_script_migrated",
            "MIGRATED_IN_MEMORY",
            std::string("script=") + context.loaded_script_path +
                ", reason=" + legacyMigrationReason);
      }

      // A candidate run must use the values captured by the Save/Run action.
      // Do not let an Evidence refresh or runtime-object refresh replace them
      // between the button click and this deferred compiler pass.
      const ManualGaugeState frozenGauge = context.pending_execution_gauge;
      const std::unordered_map<std::string, int> frozenGlobals =
          context.pending_execution_globals;

      SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:Run:analyze");
      AnalyzeScript(context);
      if (usePendingExecutionSnapshot)
      {
        context.current_gauge = frozenGauge;
        context.runtime_int_vars = frozenGlobals;
        context.debug_reason =
            "candidate input snapshot restored before execution: candidate_id=" +
            context.pending_execution_candidate_id;
      }
      else if (context.current_gauge.has_line_gauge ||
               context.current_gauge.has_circle_gauge ||
               context.current_gauge.has_ellipse_gauge)
      {
        SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:Run:apply_gauge");
        ApplyManualGaugeToGlobals(context);
      }
      SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:Run:set_globals");
      for (const auto& input : context.runtime_int_vars)
      {
        if (input.first.rfind("global_", 0) == 0)
          m_parserDebugBridge.SetGlobalInt(input.first, input.second);
      }
      // CxScript result statements (for example
      // global_valid_points_count = m_line.getvalidpointcount();) are external
      // numeric destinations too.  They are discovered by AnalyzeScript but
      // were never registered with the parser, so a successful tool call could
      // still fail at its first result write.  Bind only missing global_* names;
      // global_matInput is an Image object and must never enter this numeric
      // external-variable path.
      for (const ScriptVariableView& observed : context.global_variable_views)
      {
        if (observed.name == "global_matInput" ||
            observed.name.rfind("global_", 0) != 0)
          continue;
        if (context.runtime_int_vars.find(observed.name) ==
            context.runtime_int_vars.end())
        {
          context.runtime_int_vars.emplace(observed.name, 0);
          m_parserDebugBridge.SetGlobalInt(observed.name, 0);
        }
      }

      std::stringstream ss;
      ss << "\nManual effective globals:";
      ss << "\nimage=" << (m_imageViewImage.empty() ? "s_img0" : "m_imageViewImage");
      if (!m_imageViewImage.empty())
        ss << " size=" << m_imageViewImage.cols << "x" << m_imageViewImage.rows;
      else if (!s_img0.empty())
        ss << " size=" << s_img0.cols << "x" << s_img0.rows;
      ss << "\nscript=" << context.loaded_script_path;
      ss << "\neditor_source=" << context.editor_source;
      ss << "\nellipse_roi=(" << context.runtime_int_vars["global_ellipse_x0"] << ","
         << context.runtime_int_vars["global_ellipse_y0"] << ","
         << context.runtime_int_vars["global_ellipse_x1"] << ","
         << context.runtime_int_vars["global_ellipse_y1"] << ")";
      ss << "\nroi=(" << context.runtime_int_vars["global_roi_x0"] << ","
         << context.runtime_int_vars["global_roi_y0"] << ","
         << context.runtime_int_vars["global_roi_x1"] << ","
         << context.runtime_int_vars["global_roi_y1"] << ")";
      ss << "\ncircle=(" << context.runtime_int_vars["global_circle_cx"] << ","
         << context.runtime_int_vars["global_circle_cy"] << ")";
      ss << "\ncircle_point=(" << context.runtime_int_vars["global_circle_px"] << ","
         << context.runtime_int_vars["global_circle_py"] << ")";
      ss << "\ncircle_inner_radius="
         << context.runtime_int_vars["global_circle_inner_radius"];
      ss << "\ncircle_outer_radius="
         << context.runtime_int_vars["global_circle_outer_radius"];
      ss << "\ncircle_ring_width="
         << context.runtime_int_vars["global_circle_ring_width"];
      ss << "\nfindcircle_arc=(enabled="
         << context.runtime_int_vars["global_findcircle_arc_enabled"]
         << ", start=" << context.runtime_int_vars["global_findcircle_arc_start_deg"]
         << ", end=" << context.runtime_int_vars["global_findcircle_arc_end_deg"]
         << ")";
      ss << "\ngap=" << context.runtime_int_vars["global_gap"];
      ss << "\nlinegap=" << context.runtime_int_vars["global_linegap"];
      ss << "\nthreshold=" << context.runtime_int_vars["global_threshold"];
      ss << "\nmethod=" << context.runtime_int_vars["global_method"];
      const std::string effectiveGlobals = ss.str();

      CXLOG_INFO(
          "ManualConsole",
          "manual_run_input_snapshot",
          "READY",
          std::string("source=") +
              (sourceRefreshReason.empty() ? "editor" : "disk_refresh") +
              effectiveGlobals);

      if (legacyFindCircleMigrated)
        context.debug_reason = legacyMigrationReason;

      SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:Run:bind_image");
      bool imageBound = true;
      if (!m_imageViewImage.empty())
        imageBound = m_parserDebugBridge.StageGlobalMatInput(m_imageViewImage);
      else if (!s_img0.empty())
        imageBound = m_parserDebugBridge.StageGlobalMatInput(s_img0);

      SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:Run:clear_old_runtime_shapes");
      m_annotationLayer.RemoveRuntimeOwnersNotIn(std::unordered_set<std::string>{});

      SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:Run:run_script");
      context.run_state = "running";
      const bool ran = imageBound && m_parserDebugBridge.RunScript(context.editor_text);
      context.run_state = ran ? "runtime_finished" : "failed";
      context.debug_status = ran ? "runtime_executed" : "run_failed";
      context.debug_reason = ran
        ? "exact Script Editor text executed through ParserDebugBridge"
        : (imageBound
          ? ("ParserDebugBridge rejected the Script Editor text: " +
             m_parserDebugBridge.LastError())
          : "ParserDebugBridge rejected Run: no Image View/default image available for global_matInput");
      context.debug_reason += effectiveGlobals;

      m_scriptResult.source = "manual_console_editor";
      m_scriptResult.script_path = context.loaded_script_path;
      m_scriptResult.status = ran ? "PENDING" : "BLOCKED";
      m_scriptResult.reason = context.debug_reason;
      m_scriptResult.runtime_fillback_status = ran
        ? "runtime_objects_queried"
        : "not_started";

      SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:Run:refresh_runtime_objects");
      RefreshRuntimeObjectTable(
        "Manual Console Run", ran ? "runtime_executed" : "BLOCKED");

      // Runtime projection is allowed to update result objects, but it must
      // not visually roll the candidate editor back to values from the
      // selected Evidence row.  Preserve the input snapshot until the run has
      // been fully packaged.
      if (usePendingExecutionSnapshot)
      {
        context.current_gauge = frozenGauge;
        context.runtime_int_vars = frozenGlobals;
      }

      SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:Run:save_snapshot");
      std::string snapshotPath;
      std::string snapshotReason;
      if (!SaveCxDebugSnapshotText(context, snapshotPath, snapshotReason))
      {
        context.debug_reason += " | debug snapshot save failed: " + snapshotReason;
      }
      if (runRequestedByCandidateSave &&
          !context.last_evidence_candidate_id.empty() &&
          !context.last_evidence_candidate_dir.empty())
      {
        std::filesystem::path candidateDir(context.last_evidence_candidate_dir);
        CxEvidenceCandidateSaveOptions options;
        if (candidateDir.has_parent_path() &&
            candidateDir.parent_path().has_parent_path())
        {
          options.root_dir = candidateDir.parent_path().parent_path().string();
        }
        options.candidate_id = context.last_evidence_candidate_id;
        options.case_id_override =
            candidateDir.has_parent_path()
                ? candidateDir.parent_path().filename().string()
                : std::string();
        options.mode = ran ? "runtime_finished" : "runtime_failed";
        options.request_run = false;
        options.add_to_evidence_chain = false;
        options.preserve_input_snapshots = true;
        CxEvidenceCandidateSaveResult candidateResult;
        if (!SaveEvidenceCandidatePackage(context, options, candidateResult))
        {
          context.debug_reason +=
              " | evidence candidate post-run update failed: " +
              candidateResult.reason;
        }
        else
        {
          context.last_evidence_candidate_dir = candidateResult.candidate_dir;
        }
      }
      SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:Run:end");
      if (usePendingExecutionSnapshot)
      {
        context.has_pending_execution_snapshot = false;
        context.pending_execution_globals.clear();
        context.pending_execution_candidate_id.clear();
        context.last_evidence_candidate_id.clear();
        context.last_evidence_candidate_dir.clear();
        context.last_evidence_candidate_reason.clear();
      }
    }
  }

  ImGui::SameLine();
  SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:step_button");
  if (ImGui::Button("Step", ImVec2(btnWidth, 0)))
  {
    SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:Step:begin");
    DebugStepOnceWithSnapshot(context);
    RequestRuntimeShapeSync("DebugCompiler:Step");
    std::string snapshotPath;
    std::string snapshotReason;
    if (!SaveCxDebugSnapshotText(context, snapshotPath, snapshotReason))
    {
      context.debug_reason += " | debug snapshot save failed: " + snapshotReason;
    }
    SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:Step:end");
  }

  ImGui::SameLine();
  SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:continue_button");
  if (ImGui::Button("Continue", ImVec2(btnWidth, 0)))
  {
    SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:Continue:begin");
    context.run_state = "running";
    AnalyzeScript(context);
    const int maxContinueSteps = 512;
    int executedSteps = 0;
    while (context.run_state == "running" ||
           context.run_state == "runtime_step")
    {
      if (context.current_line >= static_cast<int>(context.line_views.size()))
      {
        context.run_state = "runtime_finished";
        context.debug_status = "PENDING";
        context.debug_reason =
          "script finished; global_current_status remains PENDING; judge/rule not executed";
        break;
      }

      const int beforeLine = context.current_line;
      DebugStepOnceWithSnapshot(context);
      ++executedSteps;

      if (context.run_state == "blocked" ||
          context.run_state == "failed" ||
          context.run_state == "runtime_finished")
      {
        break;
      }

      if (executedSteps >= maxContinueSteps)
      {
        context.run_state = "blocked";
        context.debug_status = "BLOCKED";
        context.debug_reason =
          "Continue stopped by step budget; possible debug cursor stall";
        break;
      }

      if (context.current_line == beforeLine)
      {
        context.run_state = "blocked";
        context.debug_status = "BLOCKED";
        context.debug_reason =
          "Continue stopped because debug cursor did not advance";
        break;
      }
    }

    RequestRuntimeShapeSync("DebugCompiler:Continue");
    std::string snapshotPath;
    std::string snapshotReason;
    if (!SaveCxDebugSnapshotText(context, snapshotPath, snapshotReason))
    {
      context.debug_reason += " | debug snapshot save failed: " + snapshotReason;
    }
    SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:Continue:end");
  }

  ImGui::SameLine();
  SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:reset_button");
  if (ImGui::Button("Reset", ImVec2(btnWidth, 0)))
  {
    ResetDebugRuntimeForReplay(context);
    context.debug_action = "Reset";
    context.debug_status = "PENDING";
    context.debug_reason = "Debug runtime reset";
  }

  ImGui::Separator();

  SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:status_text");
  ImGui::Text("Line: %d/%d",
              context.current_line + 1,
              static_cast<int>(context.line_views.size()));
  ImGui::Text("State: %s", context.run_state.c_str());
  ImGui::Text("Status: %s", context.debug_status.c_str());

  if (!context.debug_reason.empty())
  {
    std::string displayReason = context.debug_reason;
    constexpr std::size_t kMaxReasonDisplay = 4096;
    if (displayReason.size() > kMaxReasonDisplay)
    {
      displayReason.resize(kMaxReasonDisplay);
      displayReason += "\n... [reason truncated in UI; full text is saved in debug snapshot/log]";
    }
    ImGui::TextWrapped("Reason: %s", displayReason.c_str());
  }

  ImGui::PopID();
  SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:end");
}

void ViewController::DrawCxParserExtLineViewsPanel(ManualTestContext& context)
{
  if (!ImGui::CollapsingHeader("Line Views", ImGuiTreeNodeFlags_DefaultOpen))
    return;

  if (context.line_views.empty())
  {
    ImGui::TextDisabled("No line views available. Compile a script first.");
    return;
  }

  ImGui::BeginChild("line_views", ImVec2(-1, 150), true);

  if (ImGui::BeginTable("line_views_table", 4,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
  {
    ImGui::TableSetupColumn("Line");
    ImGui::TableSetupColumn("Type");
    ImGui::TableSetupColumn("Content");
    ImGui::TableSetupColumn("Status");
    ImGui::TableHeadersRow();

    for (std::size_t i = 0; i < context.line_views.size(); ++i)
    {
      const ScriptLineView& lv = context.line_views[i];
      bool isCurrent = static_cast<int>(i) == context.current_line;

      ImGui::TableNextRow();
      if (isCurrent)
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(40, 80, 140, 255));

      ImGui::TableSetColumnIndex(0); ImGui::Text("%d", lv.line_no);
      ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(lv.statement.c_str());
      ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(lv.statement.c_str());
      ImGui::TableSetColumnIndex(3); ImGui::TextUnformatted(lv.status.c_str());
    }

    ImGui::EndTable();
  }

  ImGui::EndChild();
}

void ViewController::DrawCxParserExtStatementViewsPanel(ManualTestContext& context)
{
  if (!ImGui::CollapsingHeader("Statement Views", ImGuiTreeNodeFlags_DefaultOpen))
    return;

  if (context.cxparser_ext_statement_views.empty())
  {
    ImGui::TextDisabled("No statement views available.");
    return;
  }

  ImGui::BeginChild("statement_views", ImVec2(-1, 150), true);

  if (ImGui::BeginTable("statement_views_table", 3,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
  {
    ImGui::TableSetupColumn("Line");
    ImGui::TableSetupColumn("Type");
    ImGui::TableSetupColumn("Details");
    ImGui::TableHeadersRow();

    for (const auto& sv : context.cxparser_ext_statement_views)
    {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0); ImGui::Text("%d", sv.line_no);
      ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(sv.statement_type.c_str());
      ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(sv.reason.c_str());
    }

    ImGui::EndTable();
  }

  ImGui::EndChild();
}

void ViewController::DrawCxParserExtObjectAssignmentsPanel(ManualTestContext& context)
{
  if (!ImGui::CollapsingHeader("Object Assignments", ImGuiTreeNodeFlags_DefaultOpen))
    return;

  if (context.cxparser_ext_object_assignments.empty())
  {
    ImGui::TextDisabled("No object assignments available.");
    return;
  }

  ImGui::BeginChild("object_assignments", ImVec2(-1, 150), true);

  if (ImGui::BeginTable("object_assignments_table", 4,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
  {
    ImGui::TableSetupColumn("Object");
    ImGui::TableSetupColumn("Property");
    ImGui::TableSetupColumn("Value");
    ImGui::TableSetupColumn("Line");
    ImGui::TableHeadersRow();

    for (const auto& oa : context.cxparser_ext_object_assignments)
    {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(oa.lhs_variable.c_str());
      ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(oa.lhs_type.c_str());
      ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(oa.returned_object_ref.c_str());
      ImGui::TableSetColumnIndex(3); ImGui::Text("%d", oa.line_no);
    }

    ImGui::EndTable();
  }

  ImGui::EndChild();
}

const char* UiTextOrDash(const std::string& text)
{
  return text.empty() ? "-" : text.c_str();
}

std::string InferCurrentTemplateTool(const ManualTestContext& context)
{
  for (const auto& entry : context.catalog_entries)
  {
    if (entry.script_id == context.active_script_case_name)
      return entry.tool;
  }
  return "unknown";
}

std::string InferCurrentTemplatePath(const ManualTestContext& context)
{
  for (const auto& entry : context.catalog_entries)
  {
    if (entry.script_id == context.active_script_case_name)
      return entry.path;
  }
  return "";
}

int CountSelectedParamCandidates(const ManualTestContext& context)
{
  int count = 0;
  for (const auto& candidate : context.param_regression.candidates)
  {
    if (candidate.selected_for_probe)
      ++count;
  }
  return count;
}
