#ifndef CXIMAGE_SEMANTIC_FLOW_GRAPH_H
#define CXIMAGE_SEMANTIC_FLOW_GRAPH_H

#include "SemanticFlowTypes.h"

#include <string>

class SemanticFlowGraph
{
public:
  void Initialize(const std::string& repository_root);
  SemanticFlowAction Draw();
  void ApplyScriptResult(int node_index,
                         const std::string& status,
                         const std::string& result_ref,
                         const std::string& evidence_ref,
                         const std::string& issue_entry_ref,
                         const std::string& reason);

private:
  bool LoadFlowFile(const std::string& path);
  void LoadDemoFlow();
  void ClearFlow();
  void DrawGraphCanvas();
  void DrawNodeDetail(SemanticFlowAction& action);
  SemanticNode* SelectedNode();
  const SemanticNode* SelectedNode() const;

  std::string m_repositoryRoot;
  std::string m_demoRelativePath = "cxscript/state_machine/examples/mlpack_handoff_demo.cxflow";
  std::string m_currentWorkingDir;
  std::string m_resolvedDemoPath;
  std::string m_loadStatus = "PENDING";
  std::string m_loadReason = "not loaded";
  bool m_demoFileExists = false;
  std::string m_currentFlowPath;
  std::string m_lastLog;
  std::string m_sharedBoundNodeId;
  std::string m_sharedBoundScriptPath;
  SemanticFlow m_flow;
  bool m_open = true;
};

#endif