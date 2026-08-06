# FastMatch Grid Pattern Evidence Chain

## Scope

This change adds one explicit experimental CASE. It does not replace or mutate
the frozen FastMatch structural/edge-network baseline.

Evidence chain:

```text
fastmatch_structural_baseline_reference
  -> fastmatch_grid_pattern_class_evidence
```

The new branch analyzes region content using normalized grid cells and a
3-to-5-level pooled hierarchy. A classifier model is not bound in this CASE,
so the only valid conclusion is `grid_feature_available` or a factual failure
stage. Classification accuracy remains `model_not_bound`.

## Assets

- Evidence chain: `cxparser/cxscript/module/cximage/evidence/fastmatch_grid_pattern_class_chain.cxsc`
- Direct CASE: `cxparser/cxscript/module/cximage/diagnostic/fastmatch/fastmatch_grid_pattern_class_evidence.cxsc`
- Headless values: `cxparser/cxscript/module/cximage/diagnostic/fastmatch/fastmatch_grid_pattern_class_globals.values`
- Catalog id: `fastmatch_grid_pattern_class_evidence`
- Runtime object: `GridPatternClassTool`

## Key Parameter Controls

| Control | Global | Meaning |
|---|---|---|
| Learn ROI | `global_learn_roi_x/y/w/h` | Region-content analysis ROI; shared only as an evidence coordinate reference |
| Normalized size | `global_grid_normalized_width/height` | Canonical feature-map image size |
| Grid shape | `global_grid_rows/cols` | First-level cell layout |
| Pooled levels | `global_grid_levels` | Hierarchy depth, constrained to 3-5 |
| Orientation bins | `global_grid_orientation_bins` | Local gradient direction histogram resolution |
| Foreground threshold | `global_grid_foreground_threshold` | `-1` selects Otsu; otherwise 0-255 |
| Foreground polarity | `global_grid_foreground_dark` | `1` means dark foreground |
| Contrast equalization | `global_grid_equalize_contrast` | Optional pre-feature histogram equalization |
| Active foreground | `global_grid_active_foreground_percent` | Minimum foreground occupancy for an active cell |
| Active edge | `global_grid_active_edge_percent` | Minimum edge density for an active cell |
| Overlay cap | `global_grid_max_overlays` | Maximum focus cells published to the common view, default 96 |
| Fusion mode | `global_grid_fusion_mode` | Recorded policy: 0 structural, 1 grid, 2 cascade, 3 score fusion |

Fusion mode is evidence metadata in this CASE. It does not execute candidate
fusion and must not be reported as FastMatch accuracy improvement.

## View Chain

```text
CxScript CASE
  -> GridPatternClassTool::analyze(Image)
  -> RuntimeObjectView(GridPatternClassTool)
  -> GridPatternClassTool::PublishDisplayShapes
  -> ImageAnnotationLayer
  -> Image View / Evidence summary
```

Published elements:

| Element | Semantic role | Editable | Meaning |
|---|---|---:|---|
| Analysis ROI | `analysis_roi` | yes | Region passed to the grid feature network |
| Active cell rectangle | `active_grid_cell` | no | High-response region-content cell |
| Cell direction line | `cell_orientation` | no | Dominant local gradient direction |

Dragging the analysis ROI only updates `global_learn_roi_*`. The Parser-owned
tool is not mutated during the GUI frame; the next Run applies the updated ROI
through the same CxScript execution core.

Runtime facts exposed to the view:

```text
status_code
active_cell_count
descriptor_dim
level_count
overlay_count
overlay_truncated
elapsed_ms
summary
```

## Verification Gate

Use the user-selected `<BUILD_DIR>` only. Do not configure or create a parallel
build directory.

1. `T0`: catalog, evidence chain, direct script, image and globals values exist.
2. Compile `cxvision_imgui_acceptance` after the C++ changes.
3. `T3`: run the direct CASE with `--globals` pointing to the values file.
4. Confirm `global_grid_status=1`, nonzero descriptor dimension and 3-5 levels.
5. Confirm Image View contains one analysis ROI plus active-cell rectangles and
   direction lines on the actual ROI.
6. Change ROI/grid/threshold controls, rerun, and verify the runtime facts and
   overlays change together.

Allowed conclusion after this gate:

```text
GRID_PATTERN_FEATURE_PROJECTION_PASS
PENDING_CLASS_MODEL_BINDING
PENDING_HUMAN_REVIEW
```

Not allowed from this CASE alone:

```text
FASTMATCH_RESULT_PASS
GRID_CLASSIFICATION_ACCURACY_PASS
FASTMATCH_FUSION_PASS
```
