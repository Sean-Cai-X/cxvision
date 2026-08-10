[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$RepoRoot,

    [Parameter(Mandatory = $true)]
    [string]$BuildDir,

    [string]$RunRoot = "",
    [string]$SegmentationImage = "",
    [string]$DetectionImage = "",
    [int]$CaseTimeoutSeconds = 45
)

$ErrorActionPreference = "Stop"

$RepoRoot = (Resolve-Path -LiteralPath $RepoRoot).Path
$BuildDir = (Resolve-Path -LiteralPath $BuildDir).Path
$CxVisionRoot = Split-Path -Parent $RepoRoot

if ([string]::IsNullOrWhiteSpace($RunRoot)) {
    $RunRoot = Join-Path $CxVisionRoot "cxscript_runs"
}
if ([string]::IsNullOrWhiteSpace($SegmentationImage)) {
    $SegmentationImage = Join-Path $CxVisionRoot "test_images\L1_high_contrast\line_high_contrast_001.jpg"
}
if ([string]::IsNullOrWhiteSpace($DetectionImage)) {
    $DetectionImage = Join-Path $CxVisionRoot "test_images\L2_low_contrast_illumination\fastmatch_bottle_test_l2.jpg"
}

$Binary = Join-Path $BuildDir "Release\cxvision_imgui_acceptance.exe"
$RuntimeDll = Join-Path $BuildDir "Release\libtorch_module_runtime.dll"
$SharedLog = Join-Path $RunRoot "_shared\cxvision_imgui_acceptance.jsonl"
$RunId = "run_{0}_torch_mainline_headless" -f (Get-Date -Format "yyyyMMdd_HHmmss")
$OutputRoot = Join-Path $RunRoot "torch_mainline\$RunId"

$Scripts = [ordered]@{
    Capabilities = Join-Path $RepoRoot "cxparser\cxscript\module\torch\torch_capabilities_direct.cxsc"
    Train = Join-Path $RepoRoot "cxparser\cxscript\module\torch\torch_train_lifecycle_direct_test.cxsc"
    Segmentation = Join-Path $RepoRoot "cxparser\cxscript\module\torch\torch_segmentation_cpp_state_dict_cpu_direct.cxsc"
    DetectionContract = Join-Path $RepoRoot "cxparser\cxscript\module\torch\torch_detection_contract_direct.cxsc"
    Detection = Join-Path $RepoRoot "cxparser\cxscript\module\torch\torch_detection_yolov8_cpu_smoke_direct.cxsc"
    FindSegmentation = Join-Path $RepoRoot "cxparser\cxscript\module\cximage\headless\find_segmentation_libtorch_smoke_direct.cxsc"
}

$Assets = [ordered]@{
    Binary = $Binary
    RuntimeDll = $RuntimeDll
    SegmentationImage = $SegmentationImage
    DetectionImage = $DetectionImage
    SegmentationManifest = Join-Path $RepoRoot "libtorch_module\testdata\manifests\deeplab_cpp_state_dict_smoke_v1.json"
    SegmentationWeights = Join-Path $RepoRoot "libtorch_module\models\deeplab_cpp_state_dict_smoke_v1\weights\deeplab_cpp_state_dict_smoke.pt"
    DetectionManifest = Join-Path $RepoRoot "libtorch_module\testdata\manifests\yolov8_cpu_v1.json"
    DetectionWeights = Join-Path $RepoRoot "libtorch_module\testdata\manifests\y8_model.pt"
}
foreach ($entry in $Scripts.GetEnumerator()) {
    $Assets["Script_$($entry.Key)"] = $entry.Value
}

New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $SharedLog) | Out-Null

$PreflightItems = @()
foreach ($entry in $Assets.GetEnumerator()) {
    $exists = Test-Path -LiteralPath $entry.Value -PathType Leaf
    $length = 0
    if ($exists) {
        $length = (Get-Item -LiteralPath $entry.Value).Length
    }
    $PreflightItems += [pscustomobject]@{
        name = $entry.Key
        path = $entry.Value
        exists = $exists
        length = $length
    }
}
$PreflightPass = ($PreflightItems | Where-Object { -not $_.exists -or $_.length -le 0 }).Count -eq 0
$Preflight = [pscustomobject]@{
    schema = "cxvision.torch.mainline.preflight.v1"
    run_id = $RunId
    repo_root = $RepoRoot
    build_dir = $BuildDir
    binary = $Binary
    output_root = $OutputRoot
    unified_log = $SharedLog
    pass = $PreflightPass
    conclusion = $(if ($PreflightPass) { "ASSET_PREFLIGHT_PASS" } else { "ASSET_PREFLIGHT_FAIL" })
    items = $PreflightItems
}
$Preflight | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $OutputRoot "preflight.json") -Encoding UTF8

if (-not $PreflightPass) {
    Write-Output "ASSET_PREFLIGHT_FAIL"
    Write-Output "output_root=$OutputRoot"
    exit 2
}

$Results = [System.Collections.Generic.List[object]]::new()
$FatalReason = ""

function Invoke-CxProcess {
    param(
        [string]$CaseId,
        [string[]]$Arguments,
        [int]$TimeoutSeconds
    )

    $CaseDir = Join-Path $OutputRoot $CaseId
    New-Item -ItemType Directory -Force -Path $CaseDir | Out-Null
    $StdoutPath = Join-Path $CaseDir "process_stdout.txt"
    $StderrPath = Join-Path $CaseDir "process_stderr.txt"
    $StartedAt = Get-Date

    $Process = Start-Process `
        -FilePath $Binary `
        -ArgumentList $Arguments `
        -WorkingDirectory $RepoRoot `
        -RedirectStandardOutput $StdoutPath `
        -RedirectStandardError $StderrPath `
        -PassThru

    $Completed = $Process.WaitForExit($TimeoutSeconds * 1000)
    $TimedOut = -not $Completed
    if ($TimedOut) {
        try { $Process.Kill() } catch { }
        $Process.WaitForExit()
    }
    else {
        $Process.WaitForExit()
    }

    $Process.Refresh()
    $ExitCode = $(if ($TimedOut) { -1001 } else { [int]$Process.ExitCode })
    $ElapsedMs = [int]((Get-Date) - $StartedAt).TotalMilliseconds
    return [pscustomobject]@{
        case_id = $CaseId
        output_dir = $CaseDir
        exit_code = $ExitCode
        timeout = $TimedOut
        elapsed_ms = $ElapsedMs
        pass = (-not $TimedOut -and $ExitCode -eq 0)
        stdout = $StdoutPath
        stderr = $StderrPath
    }
}

function Invoke-HeadlessCase {
    param(
        [string]$CaseId,
        [string]$ImagePath,
        [string]$ScriptPath,
        [string[]]$ExtraArguments = @()
    )

    $CaseDir = Join-Path $OutputRoot $CaseId
    $Arguments = @(
        "--headless",
        "--cxscript-headless",
        "--image", $ImagePath,
        "--script", $ScriptPath,
        "--case-name", $CaseId,
        "--out", $CaseDir,
        "--max-steps", "10000",
        "--timeout-sec", "$CaseTimeoutSeconds",
        "--unified-log", $SharedLog
    ) + $ExtraArguments

    $Result = Invoke-CxProcess -CaseId $CaseId -Arguments $Arguments -TimeoutSeconds ($CaseTimeoutSeconds + 10)
    $SummaryPath = Join-Path $CaseDir "result_summary.json"
    $Result | Add-Member -NotePropertyName summary_path -NotePropertyValue $SummaryPath
    $Result | Add-Member -NotePropertyName summary_exists -NotePropertyValue (Test-Path -LiteralPath $SummaryPath)
    $Result.pass = $Result.pass -and $Result.summary_exists
    return $Result
}

function Add-ResultAndRequire {
    param([object]$Result, [string]$FailureMessage)
    $Results.Add($Result)
    if (-not $Result.pass) {
        throw "$FailureMessage case=$($Result.case_id) exit=$($Result.exit_code) timeout=$($Result.timeout)"
    }
}

try {
    $RuntimeDir = Join-Path $OutputRoot "C1_runtime_service"
    $RuntimeArgs = @(
        "--torch-runtime-smoke",
        "--torch-runtime-dll", $RuntimeDll,
        "--torch-device", "cpu",
        "--torch-model-root", (Join-Path $RepoRoot "libtorch_module\models"),
        "--out", $RuntimeDir,
        "--unified-log", $SharedLog
    )
    $RuntimeResult = Invoke-CxProcess -CaseId "C1_runtime_service" -Arguments $RuntimeArgs -TimeoutSeconds $CaseTimeoutSeconds
    $RuntimeSmokePath = Join-Path $RuntimeDir "torch_runtime_service_smoke.json"
    $RuntimeResult | Add-Member -NotePropertyName summary_path -NotePropertyValue $RuntimeSmokePath
    $RuntimeResult | Add-Member -NotePropertyName summary_exists -NotePropertyValue (Test-Path -LiteralPath $RuntimeSmokePath)
    $RuntimeResult.pass = $RuntimeResult.pass -and $RuntimeResult.summary_exists
    Add-ResultAndRequire $RuntimeResult "Torch runtime service gate failed"

    Add-ResultAndRequire `
        (Invoke-HeadlessCase "C1b_capabilities" $SegmentationImage $Scripts.Capabilities) `
        "Torch capabilities gate failed"

    Add-ResultAndRequire `
        (Invoke-HeadlessCase "C2_train_lifecycle" $SegmentationImage $Scripts.Train) `
        "Torch training lifecycle gate failed"

    Add-ResultAndRequire `
        (Invoke-HeadlessCase "C3_segmentation" $SegmentationImage $Scripts.Segmentation) `
        "Torch segmentation gate failed"

    foreach ($Repeat in 1..3) {
        Add-ResultAndRequire `
            (Invoke-HeadlessCase "C4_segmentation_repeat_$Repeat" $SegmentationImage $Scripts.Segmentation) `
            "Torch segmentation stability gate failed"
    }

    Add-ResultAndRequire `
        (Invoke-HeadlessCase "C5_detection_contract" $DetectionImage $Scripts.DetectionContract) `
        "Torch detection contract gate failed"

    Add-ResultAndRequire `
        (Invoke-HeadlessCase "C6_detection" $DetectionImage $Scripts.Detection) `
        "Torch detection execution gate failed"

    Add-ResultAndRequire `
        (Invoke-HeadlessCase `
            "C7_findsegmentation_libtorch" `
            $SegmentationImage `
            $Scripts.FindSegmentation `
            @("--roi-x0", "120", "--roi-y0", "120", "--roi-x1", "980", "--roi-y1", "820")) `
        "FindSegmentation libtorch gate failed"
}
catch {
    $FatalReason = $_.Exception.Message
}

$TrainResultPath = Join-Path $OutputRoot "C2_train_lifecycle\torch_training_lifecycle_result.json"
$TrainEvidencePath = Join-Path $OutputRoot "C2_train_lifecycle\torch_training_lifecycle_evidence.json"
$SegTaskResultPath = Join-Path $OutputRoot "C3_segmentation\torch_segmentation_task_result.json"
$SegMaskPath = Join-Path $OutputRoot "C3_segmentation\mask_binary.png"
$SegOverlayPath = Join-Path $OutputRoot "C3_segmentation\mask_overlay.png"
$SegContoursPath = Join-Path $OutputRoot "C3_segmentation\contours.json"
$DetectionPath = Join-Path $OutputRoot "C6_detection\detections.json"
$DetectionOverlayPath = Join-Path $OutputRoot "C6_detection\detection_overlay.png"

$TrainJson = $(if (Test-Path $TrainResultPath) { Get-Content $TrainResultPath -Raw | ConvertFrom-Json } else { $null })
$TrainEvidenceJson = $(if (Test-Path $TrainEvidencePath) { Get-Content $TrainEvidencePath -Raw | ConvertFrom-Json } else { $null })
$SegJson = $(if (Test-Path $SegTaskResultPath) { Get-Content $SegTaskResultPath -Raw | ConvertFrom-Json } else { $null })
$DetectionJson = $(if (Test-Path $DetectionPath) { Get-Content $DetectionPath -Raw | ConvertFrom-Json } else { $null })

$RepeatMetrics = @()
foreach ($Repeat in 1..3) {
    $Path = Join-Path $OutputRoot "C4_segmentation_repeat_$Repeat\torch_segmentation_task_result.json"
    if (Test-Path $Path) {
        $Json = Get-Content $Path -Raw | ConvertFrom-Json
        $RepeatMetrics += [pscustomobject]@{
            repeat = $Repeat
            foreground_pixels = $Json.foreground_pixels
            foreground_ratio = $Json.foreground_ratio
            contour_count = $Json.contour_count
            status = $Json.status
        }
    }
}
$StabilityPass = $RepeatMetrics.Count -eq 3
if ($StabilityPass) {
    $Baseline = $RepeatMetrics[0]
    foreach ($Metric in $RepeatMetrics) {
        if ($Metric.status -ne "success" -or
            $Metric.foreground_pixels -ne $Baseline.foreground_pixels -or
            $Metric.contour_count -ne $Baseline.contour_count) {
            $StabilityPass = $false
        }
    }
}

$RequiredArtifacts = @(
    $TrainResultPath,
    $TrainEvidencePath,
    $SegTaskResultPath,
    $SegMaskPath,
    $SegOverlayPath,
    $SegContoursPath,
    $DetectionPath,
    $DetectionOverlayPath
)
$ArtifactAudit = foreach ($Path in $RequiredArtifacts) {
    [pscustomobject]@{
        path = $Path
        exists = (Test-Path -LiteralPath $Path -PathType Leaf)
        length = $(if (Test-Path -LiteralPath $Path -PathType Leaf) { (Get-Item -LiteralPath $Path).Length } else { 0 })
    }
}
$ArtifactsPass = ($ArtifactAudit | Where-Object { -not $_.exists -or $_.length -le 0 }).Count -eq 0

$MechanicalPass = [string]::IsNullOrWhiteSpace($FatalReason) -and
    $StabilityPass -and
    $ArtifactsPass
$DetectionCount = $(if ($null -ne $DetectionJson) { [int]$DetectionJson.num_detections } else { 0 })
$ForegroundRatio = $(if ($null -ne $SegJson) { [double]$SegJson.foreground_ratio } else { 0.0 })

$ToVerify = @(
    [pscustomobject]@{
        item = "segmentation_semantic_quality"
        status = "PENDING_REAL_WEIGHT"
        reason = "cpp_state_dict smoke weight validates runtime mechanics only; foreground_ratio=$ForegroundRatio"
    },
    [pscustomobject]@{
        item = "detection_non_empty_result"
        status = $(if ($DetectionCount -gt 0) { "PENDING_HUMAN_REVIEW" } else { "PENDING_DATA" })
        reason = "num_detections=$DetectionCount"
    },
    [pscustomobject]@{
        item = "findsegmentation_positive_negative_prompt"
        status = "PENDING_BINDING"
        reason = "current prompt-point script does not expose positive/negative labels"
    },
    [pscustomobject]@{
        item = "manual_ui_artifact_and_shape_review"
        status = $(if ($MechanicalPass) { "PENDING_HUMAN_REVIEW" } else { "BLOCKED_BY_HEADLESS" })
        reason = "verify Headless artifacts, Image View projection, fields, and training curve in one UI review"
    }
)

$Report = [pscustomobject]@{
    schema = "cxvision.torch.mainline.headless_report.v1"
    run_id = $RunId
    generated_at = (Get-Date).ToString("o")
    repo_root = $RepoRoot
    build_dir = $BuildDir
    binary = $Binary
    working_directory = $RepoRoot
    output_root = $OutputRoot
    unified_log = $SharedLog
    preflight = $Preflight.conclusion
    cases = $Results
    stability = [pscustomobject]@{
        pass = $StabilityPass
        conclusion = $(if ($StabilityPass) { "L3_STABILITY_PASS" } else { "L3_STABILITY_FAIL" })
        repeats = $RepeatMetrics
    }
    training = [pscustomobject]@{
        status = $(if ($null -ne $TrainJson) { $TrainJson.status } else { "not_run" })
        smoke_loss = $(if ($null -ne $TrainJson) { $TrainJson.smoke_loss } else { $null })
        grad_mean = $(if ($null -ne $TrainJson) { $TrainJson.grad_mean } else { $null })
        epochs = $(if ($null -ne $TrainEvidenceJson) { $TrainEvidenceJson.epochs } else { 0 })
        semantic_quality = $(if ($null -ne $TrainEvidenceJson) { $TrainEvidenceJson.semantic_quality } else { "not_run" })
    }
    segmentation = [pscustomobject]@{
        status = $(if ($null -ne $SegJson) { $SegJson.status } else { "not_run" })
        foreground_pixels = $(if ($null -ne $SegJson) { $SegJson.foreground_pixels } else { 0 })
        foreground_ratio = $ForegroundRatio
        contour_count = $(if ($null -ne $SegJson) { $SegJson.contour_count } else { 0 })
        semantic_quality = "not_evaluated"
    }
    detection = [pscustomobject]@{
        num_detections = $DetectionCount
        conclusion = $(if ($DetectionCount -gt 0) { "PENDING_HUMAN_REVIEW" } else { "PENDING_DATA" })
    }
    artifacts = $ArtifactAudit
    artifacts_pass = $ArtifactsPass
    fatal_reason = $FatalReason
    ready_for_manual_ui = $MechanicalPass
    conclusion = $(if ($MechanicalPass) { "PENDING_HUMAN_REVIEW" } else { "FAIL" })
    to_verify = $ToVerify
}

$Report | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $OutputRoot "torch_mainline_headless_report.json") -Encoding UTF8
$ToVerify | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $OutputRoot "torch_to_verify.json") -Encoding UTF8

$Markdown = @(
    "# Torch Mainline Headless Report",
    "",
    "- Run ID: $RunId",
    "- Repo: $RepoRoot",
    "- Build Dir: $BuildDir",
    "- Binary: $Binary",
    "- Unified Log: $SharedLog",
    "- Preflight: $($Preflight.conclusion)",
    "- Ready for manual UI: $MechanicalPass",
    "- Conclusion: $($Report.conclusion)",
    "",
    "## Cases",
    "",
    "| Case | Exit | Timeout | Summary | Conclusion |",
    "|---|---:|---|---|---|"
)
foreach ($Case in $Results) {
    $CaseConclusion = $(if ($Case.pass) { "HEADLESS_EXECUTION_PASS" } else { "FAIL" })
    $Markdown += "| $($Case.case_id) | $($Case.exit_code) | $($Case.timeout) | $($Case.summary_exists) | $CaseConclusion |"
}
$Markdown += @(
    "",
    "## Runtime Facts",
    "",
    "- Training status: $($Report.training.status)",
    "- Training epochs: $($Report.training.epochs)",
    "- Training semantic quality: $($Report.training.semantic_quality)",
    "- Segmentation foreground ratio: $ForegroundRatio",
    "- Segmentation contour count: $($Report.segmentation.contour_count)",
    "- Detection count: $DetectionCount",
    "- Stability: $($Report.stability.conclusion)",
    "- Artifact audit: $ArtifactsPass",
    "",
    "## Final Conclusion",
    "",
    "- Code: $($Report.conclusion)",
    "- Remaining blocker: model semantics and manual GUI review",
    "- Next allowed step: run the consolidated manual UI checklist"
)
$Markdown | Set-Content -LiteralPath (Join-Path $OutputRoot "torch_mainline_headless_report.md") -Encoding UTF8

$Checklist = @(
    "# Torch Mainline Manual UI Checklist",
    "",
    "Use only after torch_mainline_headless_report.json has ready_for_manual_ui=true.",
    "",
    "1. Open cxvision_imgui_acceptance.exe from the recorded Build Dir.",
    "2. Select the Torch training Evidence case and one Training Image Set thumbnail; verify Key Parameter Controls shows the selected annotation geometry and Torch request parameters.",
    "3. In Torch Runtime / Evidence click Train Tiny Smoke; verify the request runs serially and status/loss/grad/runtime/summary are populated.",
    "4. Verify Training Curve / Param Map shows only real result/evidence values. For epochs=1 it must say tiny-smoke metric snapshot and must not fabricate a multi-epoch curve.",
    "5. Click Infer Segmentation; verify status/device/result_ref/mask_ref/overlay_ref/contour_ref.",
    "6. Verify mask_overlay.png and contour Shape use the same Headless artifact package.",
    "7. Click Infer Detection; verify detection status and zero/non-zero result is shown honestly.",
    "8. If detections exist, verify all boxes are non-editable and mapped to original-image coordinates.",
    "9. Select [SMOKE] FindSegmentation - LibTorch Contract; verify editable prompt ROI and non-editable boundary/bbox.",
    "10. Change prompt ROI, rerun, and verify previous result becomes stale then is replaced.",
    "11. Confirm the unified log contains torch_ui_run_requested and torch_annotation_request_staged for each button action.",
    "12. Confirm failures show failure_stage/reason rather than a silent or fabricated PASS.",
    "13. Record MANUAL_GUI_PASS, MANUAL_GUI_PARTIAL, or MANUAL_GUI_FAIL with reason."
)
$Checklist | Set-Content -LiteralPath (Join-Path $OutputRoot "manual_review_checklist.md") -Encoding UTF8

Write-Output "run_id=$RunId"
Write-Output "output_root=$OutputRoot"
Write-Output "ready_for_manual_ui=$($MechanicalPass.ToString().ToLowerInvariant())"
Write-Output "conclusion=$($Report.conclusion)"
if (-not [string]::IsNullOrWhiteSpace($FatalReason)) {
    Write-Output "reason=$FatalReason"
}

exit $(if ($MechanicalPass) { 0 } else { 1 })
