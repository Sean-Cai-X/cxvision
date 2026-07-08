\# Codex Project Instructions



\## Codebase Memory Usage



Before analyzing, modifying, or explaining this repository, prefer using the `codebase-memory` MCP server when the task involves code structure, call chains, symbol relationships, module boundaries, impact analysis, architecture overview, or locating implementation files.



Use codebase-memory-mcp before broad grep/read-file exploration for:

\- finding functions/classes/modules

\- tracing caller/callee relationships

\- understanding CMake/module dependencies

\- checking impact before editing

\- locating stale/deprecated files

\- comparing cxparser / cxscript / cximage / libtorch\_module responsibilities

\- identifying where a feature belongs architecturally



Do not rely only on text search when the task is architectural or cross-file. First query the code graph, then read the specific source files.



\## Architecture Constraints



This repository must preserve module boundaries.



Do not move algorithm/business logic into UI/debugger files for quick integration.

If a missing capability belongs to cxparser, cxscript, cxcore, cximage, or libtorch\_module, implement it in the corresponding module and expose only the minimal interface upward.



For measurement/gauge debugging:

\- preserve original tool semantics

\- verify ROI geometry, size, direction, rotation, and coordinate interpretation

\- prefer explicit visual/debug output over hidden assumptions



\## Current Project Focus



The current work focuses on:

\- cxscript semantic transformation

\- cxparser integration

\- cximage IMGUI debug/test interface

\- Gauge / Findline / Circle ROI visualization and reliability testing

\- Stage 2.5 image manifest and ROI verification

\- cleaning deprecated C++/header files from CMake only after dependency confirmation



Before modifying CMakeLists, first use codebase-memory to identify whether the file is referenced, included, registered, or reachable from current targets.

