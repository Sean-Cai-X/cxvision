#include "ParserDebugBridge.h"

#include "Findcircle.h"
#include "Image.h"

#include <sstream>
#include <utility>

bool ParserDebugBridge::CompileScript(const std::string& scriptText)
{
  if (myRuntime == nullptr || scriptText.empty()) return false;
  ResetRuntime();
  if (!RebindGlobalInputs()) return false;
  const std::string prepared = PrepareScript(scriptText);
  return myRuntime->Compile(prepared.c_str());
}

bool ParserDebugBridge::RunScript(const std::string& scriptText)
{
  if (myRuntime == nullptr || scriptText.empty()) return false;
  ResetRuntime();
  if (!RebindGlobalInputs()) return false;
  const std::string prepared = PrepareScript(scriptText);
  return myRuntime->Compile(prepared.c_str());
}

bool ParserDebugBridge::RunPrefixToLine(const std::string& scriptText, int lineNo)
{
  if (myRuntime == nullptr || lineNo <= 0) return false;
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
  return current > 0 && myRuntime->Compile(prefixText.c_str());
}

void* ParserDebugBridge::QueryClassObject(const std::string& type,
                                          const std::string& name) const
{
  if (myRuntime == nullptr) return nullptr;
  void* object = myRuntime->GetClassObj(type, name);
  if (object == nullptr && type == "Findcircle")
    object = myRuntime->GetClassObj("findcircle", name);
  return object;
}

bool ParserDebugBridge::QueryObjectExists(const std::string& type,
                                          const std::string& name) const
{
  return QueryClassObject(type, name) != nullptr;
}

Image* ParserDebugBridge::QueryImage(const std::string& name) const
{
  return static_cast<Image*>(QueryClassObject("Image", name));
}

bool ParserDebugBridge::QueryDouble(const std::string& name, double& value) const
{
  if (myRuntime == nullptr || !myRuntime->IsObjectVar(name.c_str())) return false;
  double* runtimeValue = static_cast<double*>(myRuntime->GetDoubleValue(name));
  if (runtimeValue == nullptr) return false;
  value = *runtimeValue;
  return true;
}

bool ParserDebugBridge::SetDouble(const std::string& name, double value)
{
  if (myRuntime == nullptr || !myRuntime->IsObjectVar(name.c_str())) return false;
  return ApplyStatement(name + "=" + std::to_string(value) + ";");
}

bool ParserDebugBridge::ApplyStatement(const std::string& statement)
{
  return myRuntime != nullptr && !statement.empty() &&
         myRuntime->Compile(statement.c_str());
}

std::string ParserDebugBridge::PrepareScript(const std::string& scriptText) const
{
  std::string prepared = scriptText;
  const std::string source = "global.matInput";
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
  if (myRuntime == nullptr) return false;
  if (QueryImage("global_matInput") == nullptr)
  {
    if (!myRuntime->Compile("Image global_matInput;")) return false;
  }
  Image* runtimeImage = QueryImage("global_matInput");
  if (runtimeImage == nullptr) return false;
  runtimeImage->copyFromMat(myGlobalMatInput);
  return true;
}

bool ParserDebugBridge::RebindGlobalInputs()
{
  if (myGlobalMatInput.empty()) return true;
  return SetGlobalMatInput(myGlobalMatInput);
}

std::vector<ParserDebugObjectSnapshot> ParserDebugBridge::SnapshotRuntimeObjects(
  const std::string& lastMethod, int lastUpdateLine,
  const std::string& runtimeStatus) const
{
  const std::pair<const char*, const char*> objects[] = {
    {"Image", "global_matInput"}, {"Image", "m_occtimage"},
    {"Findcircle", "afindcircle0"},
    {"Findcircle", "afindcircle1"}
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
    snapshots.push_back(snapshot);
  }
  return snapshots;
}

std::vector<ParserDebugVariableSnapshot>
ParserDebugBridge::SnapshotRuntimeVariables() const
{
  const char* names[] = {"doutputvalue", "current_status"};
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

void ParserDebugBridge::Stop()
{
  if (myRuntime != nullptr) myRuntime->StopRun();
}

void ParserDebugBridge::ResetRuntime()
{
  if (myRuntime != nullptr) myRuntime->ClearAll();
}
