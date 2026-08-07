# mlpack Evidence Chain local cases

## Scope

This note records the local Evidence Chain landing for the current
`mlpack基础模型` thread.

Current project root:

```text
D:\Codex-WorkDir\Sean_WorkDir\cxvisionai
```

Remote writeback is not used in this step. Images, case metadata, and Evidence
Chain rows are landed locally only.

## Image Storage Rule

Image files must stay outside the git repository.

Allowed image root:

```text
D:\Codex-WorkDir\Sean_WorkDir\cxvisionai
```

Current mlpack image root:

```text
D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\test_images\mlpack
```

Forbidden image root:

```text
D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo
```

Evidence Chain files inside `cxvision_repo` may only store path references,
case metadata, script snapshots, and TSV/JSON records. They must not store or
copy image bodies. Future mlpack, torch, cximage, and ensmallen image evidence
must follow the same rule.

## Landed Image Package

Source package:

```text
D:\Codex-WorkDir\Sean_WorkDir\local_test\mlpack_baseline_thread\ELPV-Classification-Handoff
```

Local project image target:

```text
D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\test_images\mlpack\ELPV-Classification-Handoff
```

Landed count:

```text
24 ELPV images
```

Dataset split:

```text
G0.baseline_manual:
  manual label spot-check images

G1.semantic_handoff:
  semantic handoff images for baseline_class_ref, cluster_ref, distance_ref,
  and anomaly_ref review
```

## Landed Evidence Chain Cases

Case manifest:

```text
D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cxscript_runs\evidence_chain\mlpack_elpv_evidence_cases_20260807.tsv
```

Evidence candidate root:

```text
D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cxscript_runs\evidence_candidates
```

Each sample is materialized as:

```text
<case_id>\candidate_20260807_local_evidence_chain\
  script_snapshot.cxsc
  mlpack_case_metadata.json
```

Current case naming:

```text
MLPACK_ELPV_<bucket>_<sample_id>_<class_label>_semantic_refs
```

Example:

```text
MLPACK_ELPV_G1.semantic_handoff_cell2137_defect_semantic_refs
```

## UI Evidence Chain Registration

The following local Evidence Chain files were extended with mlpack rows:

```text
D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cxscript_runs\evidence_chain\evidence_chain_loaded_elements_debug.tsv
D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cxscript_runs\evidence_chain\evidence_chain_ui_classification_debug.tsv
```

Added group:

```text
Mlpack / ELPV Semantic Refs
```

Added tool label:

```text
MlpackBaseline
```

Added status:

```text
pending_headless_module_test
```

Added display major:

```text
Process Validation
```

## Script Snapshot

The local case snapshots currently use:

```text
D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cxparser\cxscript\module\mlpack\baseline_logreg_official_min_infer.cxsc
```

This is intentional for Evidence Chain entry. The landed case is not claiming
runtime acceptance. It is binding image evidence and mlpack semantic review
fields to a known mlpack cxscript case snapshot.

## Expected Flow

The metadata records this intended flow:

```text
baseline_feature_direct_test
baseline_logreg_official_min_infer
baseline_distance_official_min_score
baseline_cluster_ref_test_or_binding
baseline_anomaly_official_min_probe
```

Review refs:

```text
baseline_class_ref
cluster_ref
distance_ref
anomaly_ref
```

## Current Conclusions

```text
IMAGE_LOCAL_COPY: complete for ELPV, 24/24
EVIDENCE_CASE_LANDING: complete for ELPV, 24/24
UI_CHAIN_ROWS: appended, 24 loaded-elements rows and 24 classification rows
HEADLESS_EXECUTION: NOT_RUN
COMPILE: NOT_RUN
MANUAL_GUI_REVIEW: NOT_RUN
FINAL_ACCEPTANCE: NOT_ACCEPTED
```

## Known Gap

The HALCON mlpack manual handoff package currently has manifests locally, but
the checked local image entity count is zero for regular image extensions. Do
not create HALCON Evidence Chain image cases until image files are locally
available.

Gap label:

```text
HALCON_IMAGE_ENTITY_MISSING_LOCAL
```

## Next Allowed Step

Run local UI or Headless validation only after the owner authorizes testing.

Suggested T0/T3 sequence:

```text
T0:
  Check script exists, image exists, case metadata exists, and output directory
  can be created.

T3:
  <BINARY> --headless --cxscript-headless --image <image_path>
  --script cxparser/cxscript/module/mlpack/baseline_logreg_official_min_infer.cxsc
  --case-name <case_id>
  --out <RUN_ROOT>/headless/<RUN_ID>/<case_id>
  --max-steps 10000
  --unified-log <RUN_ROOT>/_shared/cxvision_imgui_acceptance.jsonl
```

Allowed result after this landing step:

```text
PENDING_HEADLESS_MODULE_TEST
PENDING_HUMAN_REVIEW
```
