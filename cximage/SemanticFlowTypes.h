#ifndef CXIMAGE_SEMANTIC_FLOW_TYPES_H
#define CXIMAGE_SEMANTIC_FLOW_TYPES_H

#include <string>
#include <vector>

struct SemanticNode
{
  std::string id;
  std::string stage;
  std::string module;
  std::string title;
  std::string script_path;
  std::string status_from;
  std::string status;
  std::string reason;
  std::string result_ref;
  std::string evidence_ref;
  std::string issue_entry_ref;
};

struct SemanticEdge
{
  std::string from;
  std::string to;
  std::string condition;
  std::string type;
};

struct SemanticFlow
{
  std::string id;
  std::string description;
  std::vector<SemanticNode> nodes;
  std::vector<SemanticEdge> edges;
  int selected_node_index = -1;
};

enum class SemanticFlowActionType
{
  None,
  LoadBoundScript,
  RunBoundScript
};

struct SemanticFlowAction
{
  SemanticFlowActionType type = SemanticFlowActionType::None;
  int node_index = -1;
  std::string node_id;
  std::string script_path;
};

#endif