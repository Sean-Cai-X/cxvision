# Torch Runtime UI Evidence Cases

This local Evidence Chain package gives the UI torch thread a stable board-level image and case set for small CPU validation.

## Local Images

- image_id: `torch_deeppcb_g1_00041000_input`
- image_path: `D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/evidence_images/torch_deeppcb_g1_00041000_input.jpg`
- sha256: `624B448F0737AAA8BDC53963A752800147BFFB2591365B9810B3F2A83B956C4A`
- source: copied from `D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/evidence_assets/ensmallen/ENS_L1_deeppcb_template_match_phase1_opt/candidate_20260807_ensmallen_phase1/deeppcb_g1_00041000_input.jpg`
- label_source: `D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/evidence_assets/ensmallen/ENS_L1_deeppcb_template_match_phase1_opt/candidate_20260807_ensmallen_phase1/deeppcb_g1_00041000_label.txt`

- image_id: `torch_deeppcb_g0_00041008_input`
- image_path: `D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/evidence_images/torch_deeppcb_g0_00041008_input.jpg`
- sha256: `A4C2801292898B9F5A85120D2441C4C33903F226BE1832862497656C3E0BC5DB`
- source: copied from `D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/evidence_assets/ensmallen/ENS_L1_deeppcb_template_match_phase1_opt/candidate_20260807_ensmallen_phase1/deeppcb_g0_00041008_input.jpg`
- label_source: `D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/evidence_assets/ensmallen/ENS_L1_deeppcb_template_match_phase1_opt/candidate_20260807_ensmallen_phase1/deeppcb_g0_00041008_label.txt`

- storage rule: images stay under `D:/Codex-WorkDir/Sean_WorkDir/cxvisionai` and must not be placed under `cxvision_repo` or committed to Git.
- migration note: the earlier `torch_l1_line_high_contrast_001` binding was only a temporary UI plumbing image; torch candidate cases now use DeepPCB board evidence images.

## Evidence Chain

- chain: `cxparser/cxscript/module/cximage/evidence/torch_runtime_ui_evidence_chain.cxsc`
- output_root: `cxscript_runs/evidence/torch_runtime_ui`
- owner: `testing UI torch thread`
- scope: small local UI Evidence Chain validation only
- UI category: declared by each case through `CxEvidenceChain_case_setcategory("Torch Evidence Candidates")`
- UI group: declared by each case through `CxEvidenceChain_case_setgroup(...)`

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
- New torch cases must be added in `torch_*.cxsc`; C++ must not hard-code concrete torch case names, model paths, objectives, or OK/NG rules.
- The UI category and group are file-driven by this evidence chain; `ManualConsoleEvidenceChain.cpp` must remain a generic scanner/parser/display layer.
- The segmentation smoke case validates runtime/artifact plumbing, not semantic segmentation accuracy.
- The detection case remains a CPU smoke route until the model/thread resolves the known weights/class compatibility issue.
- The train lifecycle case is a tiny CPU route check only; it is not a full training quality acceptance case.
- The UI pass condition is evidence visibility and artifact binding, not final model quality acceptance.
