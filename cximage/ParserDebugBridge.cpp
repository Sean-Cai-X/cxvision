#include "ParserDebugBridge.h"

#include "Findcircle.h"
#include "findline.h"
#include "Image.h"
#include "CircleRingGauge.h"
#include "FastMatchDiagnostic.h"
#include "FastMatch.h"

#if defined(CXVISION_ENABLE_CXPARSER_EXT_DEBUG_INPROC)
#include "cxscript_debug_embedded_runner.h"
#endif

#include <sstream>
#include <utility>

namespace
{
std::string CanonicalGlobalName(const std::string& name)
{
  if (name.rfind("global_", 0) == 0)
    return name;
  return "global_" + name;
}
}

bool ParserDebugBridge::CompileScript(const std::string& scriptText)
{
  myLastError.clear();
  if (myOwner == nullptr || scriptText.empty()) return false;
  ResetRuntime();
  if (!RebindGlobalInputs()) return false;
  const std::string prepared = PrepareScript(scriptText);
  return myOwner->ExecuteScript(prepared, myLastError);
}

bool ParserDebugBridge::RunScript(const std::string& scriptText)
{
  myLastError.clear();
  if (myOwner == nullptr || scriptText.empty()) return false;
  ResetRuntime();
  if (!RebindGlobalInputs()) return false;
  if (scriptText.find("global_matInput") != std::string::npos)
  {
    Image* inputImage = QueryImage("global_matInput");
    if (inputImage == nullptr)
    {
      myLastError = "global_matInput is required by script but is not bound";
      return false;
    }
    if (inputImage->getmat().empty())
    {
      myLastError = "global_matInput image is empty";
      return false;
    }
  }
  const std::string prepared = PrepareScript(scriptText);
  return myOwner->ExecuteScript(prepared, myLastError);
}

bool ParserDebugBridge::RunPrefixToLine(const std::string& scriptText, int lineNo)
{
  myLastError.clear();
  if (myOwner == nullptr || lineNo <= 0) return false;
  ResetRuntime();
  if (!RebindGlobalInputs()) return false;
  std::istringstream input(scriptText);
  std::ostringstream prefix;
  std::string line;
  int current = 0;
  while (current < lineNo && std::getline(input, line))
  {
    ++current;
    if (line.find("log_alg") != std::string::npos)
      continue;
    prefix << line << '\n';
  }
  std::string prefixText = prefix.str();
  int braceDepth = 0;
  for (char value : prefixText)
  {
    if (value == '{') ++braceDepth;
    else if (value == '}' && braceDepth > 0) --braceDepth;
  }
  while (braceDepth-- > 0) prefixText += "}\n";
  prefixText = PrepareScript(prefixText);
  return current > 0 && myOwner->ExecuteScript(prefixText, myLastError);
}

void* ParserDebugBridge::QueryClassObject(const std::string& type,
                                          const std::string& name) const
{
  if (myOwner == nullptr) return nullptr;
  void* object = myOwner->GetClassObj(type, name);
  if (object == nullptr && type == "Findcircle")
    object = myOwner->GetClassObj("findcircle", name);
  if (object == nullptr && type == "fastmatch")
    object = myOwner->GetClassObj("FastMatch", name);
  if (object == nullptr && type == "fastmatch")
    object = myOwner->GetClassObj("CFastMatch", name);
  return object;
}

bool ParserDebugBridge::QueryObjectExists(const std::string& type,
                                          const std::string& name) const
{
  return QueryClassObject(type, name) != nullptr;
}

std::vector<std::string> ParserDebugBridge::ListClassObjectNames(
  const std::string& type) const
{
  std::vector<std::string> names;
  if (myOwner == nullptr)
    return names;
  const int count = myOwner->ObjectCount(type);
  for (int i = 0; i < count; ++i)
  {
    std::string name = myOwner->ObjectName(type, i);
    if (!name.empty())
      names.push_back(name);
  }
  return names;
}

Image* ParserDebugBridge::QueryImage(const std::string& name) const
{
  return static_cast<Image*>(QueryClassObject("Image", name));
}

bool ParserDebugBridge::QueryDouble(const std::string& name, double& value) const
{
  if (myOwner == nullptr || !myOwner->IsObjectVar(name.c_str())) return false;
  double* runtimeValue = static_cast<double*>(myOwner->GetDoubleValue(name));
  if (runtimeValue == nullptr) return false;
  value = *runtimeValue;
  return true;
}

bool ParserDebugBridge::SetDouble(const std::string& name, double value)
{
  if (myOwner == nullptr || !myOwner->IsObjectVar(name.c_str())) return false;
  return ApplyStatement(name + "=" + std::to_string(value) + ";");
}

bool ParserDebugBridge::SetGlobalInt(const std::string& name, int value)
{
  myGlobalNumericInputs[CanonicalGlobalName(name)] =
    static_cast<double>(value);
  return true;
}

bool ParserDebugBridge::SetGlobalDouble(const std::string& name, double value)
{
  myGlobalNumericInputs[CanonicalGlobalName(name)] = value;
  return true;
}

bool ParserDebugBridge::SetGlobalString(
  const std::string& name,
  const std::string& value)
{
  auto escape = [](const std::string& text) {
    std::string out;
    for (char ch : text)
    {
      if (ch == '\\') out += "\\\\";
      else if (ch == '"') out += "\\\"";
      else out.push_back(ch);
    }
    return out;
  };
  const std::string fullName = CanonicalGlobalName(name);
  const std::string quoted = "\"" + escape(value) + "\"";
  return ApplyStatement(fullName + "=" + quoted + ";");
}

bool ParserDebugBridge::ApplyStatement(const std::string& statement)
{
  return myOwner != nullptr && !statement.empty() &&
         myOwner->Compile(statement.c_str());
}

std::string ParserDebugBridge::PrepareScript(const std::string& scriptText) const
{
  std::string prepared = scriptText;
  // CxScript keeps a C/C++-like surface syntax, so users naturally write
  // tool.measure(&m_image).  The current muParser class-object call path,
  // however, passes object arguments by object token (m_image), not by C/C++
  // address-of syntax.  Leaving the ampersand in place can route an invalid
  // value into void* class methods such as Findline::measure(void*), which is
  // exactly the kind of crash the Manual Console must prevent.
  //
  // Keep the editor text untouched, but normalize the runtime text submitted to
  // the parser.  This is intentionally local to the debug bridge; it does not
  // modify source .cxsc files or muParser itself.
  std::size_t addressOf = 0;
  while ((addressOf = prepared.find("(&", addressOf)) != std::string::npos)
  {
    prepared.erase(addressOf + 1, 1);
    ++addressOf;
  }

  const std::string source = "global_matInput";
  const std::string runtimeName = "global_matInput";
  std::size_t position = 0;
  while ((position = prepared.find(source, position)) != std::string::npos)
  {
    prepared.replace(position, source.size(), runtimeName);
    position += runtimeName.size();
  }
  return prepared;
}

bool ParserDebugBridge::SetGlobalMatInput(const cv::Mat& image)
{
  if (image.empty()) return false;
  myGlobalMatInput = image.clone();
  if (myOwner == nullptr) return false;
  if (QueryImage("global_matInput") == nullptr)
  {
    if (!myOwner->Compile("Image global_matInput;")) return false;
  }
  Image* runtimeImage = QueryImage("global_matInput");
  if (runtimeImage == nullptr) return false;
  runtimeImage->copyFromMat(myGlobalMatInput);
  return true;
}

bool ParserDebugBridge::RebindGlobalInputs()
{
  if (myOwner == nullptr)
  {
    myLastError = "parser owner is not bound";
    return false;
  }

  std::string reason;
  for (auto& input : myGlobalNumericInputs)
  {
    if (!myOwner->DefineExternalDouble(
          input.first, &input.second, reason))
    {
      myLastError = reason;
      return false;
    }
  }

  if (myGlobalMatInput.empty())
    return true;
  if (!SetGlobalMatInput(myGlobalMatInput))
  {
    myLastError = "failed to bind global_matInput image";
    return false;
  }
  return true;
}

void ParserDebugBridge::ClearGlobalInputs()
{
  myGlobalMatInput.release();
  myGlobalNumericInputs.clear();
}

std::vector<ParserDebugObjectSnapshot> ParserDebugBridge::SnapshotRuntimeObjects(
  const std::string& lastMethod, int lastUpdateLine,
  const std::string& runtimeStatus) const
{
  const std::pair<const char*, const char*> objects[] = {
    {"Image", "global_matInput"}, {"Image", "m_occtimage"},
    {"Findcircle", "afindcircle0"},
    {"Findcircle", "afindcircle1"},
    {"CircleRingGauge", "ring_gauge"},
    {"FastMatchDiagnostic", "fm"},
    {"fastmatch", "m_match"}
  };
  std::vector<ParserDebugObjectSnapshot> snapshots;
  for (const auto& item : objects)
  {
    ParserDebugObjectSnapshot snapshot;
    snapshot.type = item.first;
    snapshot.name = item.second;
    snapshot.exists_in_parser = QueryObjectExists(snapshot.type, snapshot.name);
    snapshot.runtime_state = snapshot.exists_in_parser ? "alive" : "PENDING";
    snapshot.last_method = lastMethod;
    snapshot.last_update_line = lastUpdateLine;
    snapshot.visualizable = true;
    snapshot.stale = !snapshot.exists_in_parser ||
      (runtimeStatus != "runtime_executed" && runtimeStatus != "runtime_queried");
    snapshot.visual_source = snapshot.stale ? "stale_runtime" : "runtime_object";
    snapshot.value_summary = snapshot.exists_in_parser ? "runtime object available" :
                                                          "not found in parser";
    if (snapshot.type == "Image" && snapshot.exists_in_parser)
    {
      Image* image = QueryImage(snapshot.name);
      if (image != nullptr)
        snapshot.value_summary = "runtime image " +
          std::to_string(image->getmat().cols) + "x" +
          std::to_string(image->getmat().rows);
    }
    else if (snapshot.type == "Findcircle" && snapshot.exists_in_parser)
    {
      Findcircle* circle = static_cast<Findcircle*>(
        QueryClassObject(snapshot.type, snapshot.name));
      if (circle != nullptr)
      {
        snapshot.has_circle = true;
        snapshot.circle_cx = static_cast<float>(circle->getcirclecentx());
        snapshot.circle_cy = static_cast<float>(circle->getcirclecenty());
        snapshot.circle_inner = static_cast<float>(circle->getcirclepax());
        snapshot.circle_radius = static_cast<float>(circle->getcirclepay());
        snapshot.value_summary = "circle=" +
          std::to_string(circle->getcirclecentx()) + "," +
          std::to_string(circle->getcirclecenty()) + "," +
          std::to_string(circle->getcirclepax()) + "," +
          std::to_string(circle->getcirclepay());
      }
    }
    else if (snapshot.type == "CircleRingGauge" && snapshot.exists_in_parser)
    {
      CircleRingGauge* ring_gauge = static_cast<CircleRingGauge*>(
        QueryClassObject(snapshot.type, snapshot.name));
      if (ring_gauge != nullptr)
      {
        snapshot.ring_outer_radius = ring_gauge->m_outer_radius;
        snapshot.ring_inner_radius = ring_gauge->m_inner_radius;
        snapshot.ring_thickness = ring_gauge->m_thickness;
        snapshot.ring_center_distance = ring_gauge->m_center_distance;
        snapshot.ring_concentric_ok = ring_gauge->m_concentric_ok;
        snapshot.ring_inside_ok = ring_gauge->m_inside_ok;
        snapshot.ring_thickness_ok = ring_gauge->m_thickness_ok;
        snapshot.ring_score = ring_gauge->m_score;
        snapshot.ring_status = ring_gauge->m_status;
        snapshot.ring_reason = ring_gauge->m_reason;
        snapshot.ring_result_ref = ring_gauge->m_result_ref;
        snapshot.value_summary = "CircleRingGauge: status=" + ring_gauge->m_status +
          ", score=" + std::to_string(ring_gauge->m_score);
      }
    }
    else if (snapshot.type == "FastMatchDiagnostic" && snapshot.exists_in_parser)
    {
      FastMatchDiagnostic* fm = static_cast<FastMatchDiagnostic*>(
        QueryClassObject(snapshot.type, snapshot.name));
      if (fm != nullptr)
      {
        snapshot.has_fastmatch_diagnostic = true;
        snapshot.fastmatch_allowed = fm->allowed() != 0;
        snapshot.fastmatch_status = std::to_string(fm->status_code());
        snapshot.fastmatch_reason = std::to_string(fm->reason_code());
        snapshot.fastmatch_result_ref = fm->result_ref();
        snapshot.value_summary = "FastMatchDiagnostic: allowed=" +
          std::string(fm->allowed() ? "true" : "false") +
          ", status_code=" + std::to_string(fm->status_code());
      }
    }
    else if (snapshot.type == "fastmatch" && snapshot.exists_in_parser)
    {
      fastmatch* fm = static_cast<fastmatch*>(
        QueryClassObject(snapshot.type, snapshot.name));
      if (fm != nullptr)
      {
        snapshot.has_fastmatch = true;
        const int model_point_count = fm->getmodelpointcount();
        snapshot.fastmatch_model_available = model_point_count > 0;
        snapshot.fastmatch_model_point_count = model_point_count;
        snapshot.fastmatch_candidate_count = fm->getresultcandidatecount();
        snapshot.fastmatch_best_index = fm->getresultbestindex();
        snapshot.fastmatch_best_score = fm->getresultbestscore();
        const int candidate_count = fm->getresultcandidatecount();
        const int best_idx = fm->getresultbestindex();
        if (best_idx >= 0 && best_idx < candidate_count)
        {
          snapshot.fastmatch_best_x = fm->getresultcentx(best_idx);
          snapshot.fastmatch_best_y = fm->getresultcenty(best_idx);
        }
        const RectsShape& result_rects = *fm->getresultrects();
        snapshot.fastmatch_has_result_box = result_rects.size() > 0;
        snapshot.fastmatch_result_status = candidate_count > 0 ? "geometry_result_available" : "PENDING";
        snapshot.value_summary = "fastmatch: candidates=" + std::to_string(candidate_count) +
          ", best_score=" + std::to_string(fm->getresultbestscore());
      }
    }
    snapshots.push_back(snapshot);
  }
  return snapshots;
}

std::vector<ParserDebugVariableSnapshot>
ParserDebugBridge::SnapshotRuntimeVariables() const
{
  const char* names[] = {
    "doutputvalue",
    "current_status",
    "global_roi_x0",
    "global_roi_y0",
    "global_roi_x1",
    "global_roi_y1",
    "global_tool_half_width",
    "global_wgap",
    "global_hgap",
    "global_linegap",
    "global_threshold",
    "global_filterprofile",
    "global_method",
    "global_circle_cx",
    "global_circle_cy",
    "global_circle_px",
    "global_circle_py",
    "global_gap"
  };
  std::vector<ParserDebugVariableSnapshot> snapshots;
  for (const char* name : names)
  {
    ParserDebugVariableSnapshot snapshot;
    snapshot.name = name;
    snapshot.exists_in_parser = QueryDouble(name, snapshot.value);
    snapshots.push_back(snapshot);
  }
  return snapshots;
}


bool ParserDebugBridge::RunCxParserExtDebugInProcess(
  const std::string& scriptPath,
  CxScriptSemanticBridgeResult& outResult)
{
  outResult = CxScriptSemanticBridgeResult{};

#if !defined(CXVISION_ENABLE_CXPARSER_EXT_DEBUG_INPROC)
  outResult.ok = false;
  outResult.status = "disabled";
  outResult.reason =
    "CXVISION_ENABLE_CXPARSER_EXT_DEBUG_INPROC is not enabled";
  return false;
#else
  cxparser_ext::debug::EmbeddedDebugRunRequest request;
  request.script_path = scriptPath;
  request.capture_structured_log = true;
  request.enable_line_view = true;
  request.enable_statement_view = true;
  request.enable_object_assignment = true;
  request.enable_method_trace = true;
  request.enable_return_object_trace = true;

  const cxparser_ext::debug::EmbeddedDebugRunResult result =
    cxparser_ext::debug::RunCxScriptDebugEmbedded(request);

  outResult.ok = result.ok;
  outResult.status = result.status;
  outResult.reason = result.reason;
  outResult.raw_log = result.raw_log;

  for (const cxparser_ext::debug::EmbeddedDebugLineView& item :
       result.line_views)
  {
    CxScriptLineView view;
    view.line_no = item.line_no;
    view.source_line = item.source_line;
    view.statement_type = item.statement_type;
    view.status = item.status;
    view.reason = item.reason;
    outResult.line_views.push_back(view);
  }

  for (const cxparser_ext::debug::EmbeddedDebugStatementView& item :
       result.statement_views)
  {
    CxScriptStatementView view;
    view.statement_id = item.statement_id;
    view.line_no = item.line_no;
    view.statement_type = item.statement_type;
    view.lhs_variable = item.lhs_variable;
    view.lhs_type = item.lhs_type;
    view.source_object = item.source_object;
    view.method_name = item.method_name;
    view.returned_object_ref = item.returned_object_ref;
    view.status = item.status;
    view.reason = item.reason;
    outResult.statement_views.push_back(view);
  }

  for (const cxparser_ext::debug::EmbeddedDebugObjectAssignment& item :
       result.object_assignments)
  {
    CxScriptObjectAssignmentView view;
    view.lhs_variable = item.lhs_variable;
    view.lhs_type = item.lhs_type;
    view.source_object = item.source_object;
    view.method_name = item.method_name;
    view.returned_object_ref = item.returned_object_ref;
    view.source_line = item.source_line;
    view.line_no = item.line_no;
    view.status = item.status;
    view.reason = item.reason;
    outResult.object_assignments.push_back(view);
  }

  return outResult.ok;
#endif
}
void ParserDebugBridge::Stop()
{
  if (myOwner != nullptr) myOwner->StopRun();
}

void ParserDebugBridge::ResetRuntime()
{
  if (myOwner != nullptr) myOwner->ClearAll();
}
