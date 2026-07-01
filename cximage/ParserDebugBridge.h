#ifndef CXIMAGE_PARSER_DEBUG_BRIDGE_H
#define CXIMAGE_PARSER_DEBUG_BRIDGE_H

#include "ParserClass.h"

#include <opencv2/core/mat.hpp>

#include <string>
#include <vector>

class Image;

struct ParserDebugObjectSnapshot
{
  std::string name;
  std::string type;
  int declared_line = 0;
  bool exists_in_parser = false;
  std::string runtime_state = "PENDING";
  std::string last_method;
  int last_update_line = 0;
  std::string value_summary;
  bool visualizable = false;
  std::string visual_source = "stale_runtime";
  bool stale = true;
  bool has_circle = false;
  float circle_cx = 0.0f;
  float circle_cy = 0.0f;
  float circle_inner = 0.0f;
  float circle_radius = 0.0f;
};

struct ParserDebugVariableSnapshot
{
  std::string name;
  bool exists_in_parser = false;
  double value = 0.0;
};

class ParserDebugBridge
{
public:
  void Bind(mu::CxParserRuntime* runtime) { myRuntime = runtime; }
  bool CompileScript(const std::string& scriptText);
  bool RunScript(const std::string& scriptText);
  bool RunPrefixToLine(const std::string& scriptText, int lineNo);
  bool QueryObjectExists(const std::string& type, const std::string& name) const;
  void* QueryClassObject(const std::string& type, const std::string& name) const;
  Image* QueryImage(const std::string& name) const;
  bool QueryDouble(const std::string& name, double& value) const;
  bool SetDouble(const std::string& name, double value);
  bool ApplyStatement(const std::string& statement);
  bool SetGlobalMatInput(const cv::Mat& image);
  void ClearGlobalInputs() { myGlobalMatInput.release(); }
  bool HasGlobalMatInput() const { return !myGlobalMatInput.empty(); }
  int GlobalMatInputWidth() const { return myGlobalMatInput.cols; }
  int GlobalMatInputHeight() const { return myGlobalMatInput.rows; }
  std::vector<ParserDebugObjectSnapshot> SnapshotRuntimeObjects(
    const std::string& lastMethod, int lastUpdateLine,
    const std::string& runtimeStatus) const;
  std::vector<ParserDebugVariableSnapshot> SnapshotRuntimeVariables() const;
  void Stop();
  void ResetRuntime();

private:
  std::string PrepareScript(const std::string& scriptText) const;
  bool RebindGlobalInputs();

  mu::CxParserRuntime* myRuntime = nullptr;
  cv::Mat myGlobalMatInput;
};

#endif
