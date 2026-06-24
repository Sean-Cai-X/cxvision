# libtorch_module Pure-LibTorch Development Guide

This guide defines the current development scope for `libtorch_module` while `OpenCV` and `OCC` remain deferred to the next stage.

## Current stage

Current work is limited to pure `LibTorch` components and their integration contracts.

Included in this stage:

- `torch_feature_head.h`
- `torch_fusion_head.h`
- `torch_incremental_pipeline.h`
- `torch_prototype_index.h`
- contract-smoke and minimal pure-LibTorch validation targets

Deferred to the next stage:

- `OpenCV` dataset and image IO tests
- `torch_test_main.cpp` full validation path
- `occ_semantic_geometry.h` runtime integration tests
- any `gp_Path` / `OCC` extraction flow that requires external geometry toolchain validation

## Design boundaries

Keep the current boundaries explicit:

1. Structure extraction
   - Source: `cximage_v1 / gp_Path / OCC`
   - Output: fixed-width semantic geometry descriptor
   - File boundary: `occ_semantic_geometry.h`
   - Constraint: no `torch` dependency

2. Tensor bridge
   - Source: semantic geometry descriptor
   - Output: `[B, D]` `torch::Tensor`
   - File boundary: `torch_occ_bridge.h`
   - Constraint: no feature-head logic

3. Feature encoding
   - Source: feature map + optional external tensors
   - Output: `MultiBranchEmbedding`
   - File boundary: `torch_feature_head.h`
   - Constraint: no `gp_Path`, no extractor logic

4. Pipeline orchestration
   - Source: `RoiSample`
   - Output: `PipelinePrediction`, prototype updates
   - File boundary: `torch_incremental_pipeline.h`
   - Constraint: no handcrafted feature extraction logic

## Validation loop

### Stage 1: contract smoke

Use this as the default loop during current development.

```powershell
cd D:\Codex-WorkDir\Sean_WorkDir\libtorch_module
.\run_minimal_validation.ps1
```

What it verifies:

- feature-head base path
- feature-head with external descriptors
- incremental pipeline contract

Success criteria:

- target builds cleanly
- all contract tests pass on CPU

### Stage 2: minimal pure-LibTorch validation

Run this only after contract smoke is stable.

```powershell
cd D:\Codex-WorkDir\Sean_WorkDir\libtorch_module
.\run_minimal_validation.ps1 -Mode minimal
```

What it verifies:

- model config and utility helpers
- `torch_nnmodule`
- backbone + PAN path
- assigner + loss
- `ResNet18`
- `MobileViTv2`
- current feature-head / incremental-pipeline smoke additions

Success criteria:

- no regressions in existing pure-LibTorch model paths

## Recommended development order

1. Keep `contract smoke` green
2. Extend `FeatureHead` and `IncrementalPipeline` only through narrow tensor interfaces
3. Add unit-smoke tests before widening runtime scope
4. Re-run `minimal` after any change that touches shared tensor utilities or backbone modules
5. Only after that, reopen `OpenCV` and `OCC` validation

## Current known issues

- `torch_resnet18.h` still emits MSVC warning `C4819`
- the `Torch` CMake package still emits the `libnvrtc.so` shorthash warning on Windows
- full validation remains blocked until `OpenCV` is discoverable by CMake

## Immediate next-step candidates

- tighten `FeatureHeadConfig` validation and defaults
- add explicit bridge smoke for `torch_occ_bridge.h`
- isolate `minimal` failures unrelated to the current feature-head/pipeline work
