# Torch Runtime UI Evidence Cases

This local Evidence Chain package gives the UI torch thread a stable image and case set for small CPU validation.

## Local Image

- image_id: `torch_l1_line_high_contrast_001`
- image_path: `D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/evidence_images/torch_l1_line_high_contrast_001.jpg`
- sha256: `0151D08512E9E715BCE37292903DE9736097BD5464E327EB7EF403DCF7681C21`
- source: copied from the local validation image `D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/test_images/L1_high_contrast/line_high_contrast_001.jpg`
- storage rule: images stay under `D:/Codex-WorkDir/Sean_WorkDir/cxvisionai` and must not be placed under `cxvision_repo` or committed to Git.

## Evidence Chain

- chain: `cxparser/cxscript/module/cximage/evidence/torch_runtime_ui_evidence_chain.cxsc`
- output_root: `cxscript_runs/evidence/torch_runtime_ui`
- owner: `testing UI torch thread`
- scope: small local UI Evidence Chain validation only

## Cases

| case_id | level | script | expected UI evidence |
|---|---|---|---|
| `torch_segmentation_cpp_state_dict_cpu_artifact_evidence` | T5 | `cxparser/cxscript/module/torch/torch_segmentation_cpp_state_dict_cpu_direct.cxsc` | `torch_result_ref`, `torch_evidence_ref`, `torch_mask_ref`, `torch_overlay_ref` |
| `torch_segmentation_contract_cpu_evidence` | T4 | `cxparser/cxscript/module/torch/torch_segmentation_contract_direct.cxsc` | segmentation contract route status |
| `torch_detection_yolov8_cpu_artifact_evidence` | T6 | `cxparser/cxscript/module/torch/torch_detection_cpu_direct.cxsc` | detection result refs when detector artifacts are produced |
| `torch_detection_contract_cpu_evidence` | T4 | `cxparser/cxscript/module/torch/torch_detection_contract_direct.cxsc` | detection contract route status |
| `torch_train_lifecycle_cpu_evidence` | T3 | `cxparser/cxscript/module/torch/torch_train_lifecycle_direct_test.cxsc` | `global_headless_ok` and `global_torch_ok` for tiny CPU train lifecycle route |

## Guardrails

- These cases are local Evidence Chain cases; do not write remote code or remote evidence for this package.
- The segmentation smoke case validates runtime/artifact plumbing, not semantic segmentation accuracy.
- The detection case remains a CPU smoke route until the model/thread resolves the known weights/class compatibility issue.
- The train lifecycle case is a tiny CPU route check only; it is not a full training quality acceptance case.
- The UI pass condition is evidence visibility and artifact binding, not final model quality acceptance.
