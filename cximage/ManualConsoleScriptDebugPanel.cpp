#include "pch.h"
#include "ManualConsoleScriptDebugPanel.h"
#include "ManualConsoleCxScriptDebug.h"
#include "ManualStateTestConsole.h"

void ViewController::DrawScriptEditorBlock(ManualTestContext& context)
{
  if (!ImGui::CollapsingHeader("Script Editor", ImGuiTreeNodeFlags_DefaultOpen))
    return;

  ImGui::PushID("script_editor");

  const float toolbarHeight = 30.0f;
  ImGui::BeginChild("script_editor_toolbar", ImVec2(-1, toolbarHeight), false);

  ImGui::PushItemWidth(150.0f);
  int source_index = 0;
  if (context.editor_source == "catalog") source_index = 1;
  else if (context.editor_source == "direct_file") source_index = 2;
  if (ImGui::Combo("##source", &source_index,
                   "internal\0catalog\0direct_file\0"))
  {
    if (source_index == 0) context.editor_source = "manual";
    else if (source_index == 1) context.editor_source = "catalog";
    else if (source_index == 2) context.editor_source = "direct_file";
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
    context.editor_dirty = false;
    context.debug_action = "Save Script";
    context.debug_status = "PENDING";
    context.debug_reason = "Script saved";
  }

  ImGui::SameLine();
  if (context.editor_dirty)
    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "*");

  ImGui::EndChild();

  ImGui::BeginChild("script_editor_text", ImVec2(-1, -1), true);
  ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput |
                              ImGuiInputTextFlags_EnterReturnsTrue |
                              ImGuiInputTextFlags_CtrlEnterForNewLine |
                              ImGuiInputTextFlags_AutoSelectAll;
  if (ImGui::InputTextMultiline("##script_text",
                                 context.editor_text.data(),
                                 context.editor_text.size() + 1,
                                 ImVec2(-1, -1),
                                 flags))
  {
    context.editor_dirty = true;
  }
  ImGui::EndChild();

  ImGui::PopID();
}

void ViewController::DrawScriptDebugCompilerBlock(ManualTestContext& context)
{
  if (!ImGui::CollapsingHeader("Debug Compiler", ImGuiTreeNodeFlags_DefaultOpen))
    return;

  ImGui::PushID("debug_compiler");

  const float btnWidth = 90.0f;

  if (ImGui::Button("Compile", ImVec2(btnWidth, 0)))
  {
    AnalyzeScript(context);
    context.debug_action = "Compile";
    context.debug_status = "PENDING";
    context.debug_reason = "Script analyzed";
  }

  ImGui::SameLine();
  if (ImGui::Button("Run", ImVec2(btnWidth, 0)))
  {
    context.run_state = "running";
  }

  ImGui::SameLine();
  if (ImGui::Button("Step", ImVec2(btnWidth, 0)))
  {
    DebugStepOnceWithSnapshot(context);
  }

  ImGui::SameLine();
  if (ImGui::Button("Continue", ImVec2(btnWidth, 0)))
  {
    context.run_state = "running";
  }

  ImGui::SameLine();
  if (ImGui::Button("Reset", ImVec2(btnWidth, 0)))
  {
    ResetDebugRuntimeForReplay(context);
    context.debug_action = "Reset";
    context.debug_status = "PENDING";
    context.debug_reason = "Debug runtime reset";
  }

  ImGui::Separator();

  ImGui::Text("Line: %d/%d",
              context.current_line + 1,
              static_cast<int>(context.line_views.size()));
  ImGui::Text("State: %s", context.run_state.c_str());
  ImGui::Text("Status: %s", context.debug_status.c_str());

  if (!context.debug_reason.empty())
    ImGui::TextWrapped("Reason: %s", context.debug_reason.c_str());

  ImGui::PopID();
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
