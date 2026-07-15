$EXE = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\build01\Release\cxvision_imgui_acceptance.exe"
$REPO = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo"
$IMAGE = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\test_images\L1_high_contrast\line_high_contrast_001.jpg"
$SCRIPT = "$REPO\cxparser\cxscript\module\cximage\stage25\param_regression\param_self_tuning_findline_direct.cxsc"
$RUN_ROOT = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxscript_runs\param_self_tuning"
$RUN_ID = "run_$(Get-Date -Format 'yyyyMMdd_HHmmss')"

$BASE_X0 = 82
$BASE_Y0 = 183
$BASE_X1 = 1210
$BASE_Y1 = 183

$BASE_THRESHOLD = 20
$BASE_WGAP = 8
$BASE_HGAP = 32
$BASE_LINEGAP = 6
$BASE_METHOD = 2
$BASE_FILTERPROFILE = 1
$BASE_TOOL_HALF_WIDTH = 50

$PHASE_DIR = "$RUN_ROOT\phase1\$RUN_ID"
New-Item -ItemType Directory -Path $PHASE_DIR -Force | Out-Null

Write-Host "===== Phase 1: Gauge ROI Scan ====="
Write-Host "Run ID: $RUN_ID"
Write-Host "Base ROI: ($BASE_X0,$BASE_Y0)-($BASE_X1,$BASE_Y1)"
Write-Host "Base Parameters: threshold=$BASE_THRESHOLD, wgap=$BASE_WGAP, hgap=$BASE_HGAP, linegap=$BASE_LINEGAP, method=$BASE_METHOD, filterprofile=$BASE_FILTERPROFILE, tool_half_width=$BASE_TOOL_HALF_WIDTH"
Write-Host ""

$results = @()
$results += "offset_y,points,fit,avgdist,failure_stage"

for ($offset = -50; $offset -le 50; $offset += 5) {
    $ROI_Y0 = $BASE_Y0 + $offset
    $ROI_Y1 = $BASE_Y1 + $offset
    
    $CASE_ID = "offset_$offset"
    $OUT_DIR = "$PHASE_DIR\$CASE_ID"
    New-Item -ItemType Directory -Path $OUT_DIR -Force | Out-Null
    
    Write-Host "Running: offset_y=$offset, roi_y0=$ROI_Y0, roi_y1=$ROI_Y1"
    
    & $EXE `
        --cxscript-headless `
        --image $IMAGE `
        --script $SCRIPT `
        --out $OUT_DIR `
        --case-name $CASE_ID `
        --roi-x0 $BASE_X0 `
        --roi-y0 $ROI_Y0 `
        --roi-x1 $BASE_X1 `
        --roi-y1 $ROI_Y1 `
        --threshold $BASE_THRESHOLD `
        --wgap $BASE_WGAP `
        --hgap $BASE_HGAP `
        --linegap $BASE_LINEGAP `
        --method $BASE_METHOD `
        --filterprofile $BASE_FILTERPROFILE `
        --tool-half-width $BASE_TOOL_HALF_WIDTH
    
    $POINTS = 0
    $FIT = $false
    $AVGDIST = 0.0
    $FAILURE_STAGE = "none"
    
    $summaryPath = "$OUT_DIR\result_summary.json"
    if (Test-Path $summaryPath) {
        try {
            $summary = Get-Content $summaryPath -Raw | ConvertFrom-Json
            $POINTS = $summary.valid_points_count
            $FIT = $summary.has_fit_line
            $AVGDIST = $summary.avgdist
            $FAILURE_STAGE = if ($summary.failure_stage) { $summary.failure_stage } else { "none" }
        } catch {
            Write-Host "Error parsing $summaryPath"
        }
    }
    
    $results += "$offset,$POINTS,$FIT,$AVGDIST,$FAILURE_STAGE"
    Write-Host "  -> points=$POINTS, fit=$FIT, avgdist=$AVGDIST, failure_stage=$FAILURE_STAGE"
}

$results | Out-File "$PHASE_DIR\phase1_results.csv" -Encoding utf8

Write-Host ""
Write-Host "===== Phase 1 Complete ====="
Write-Host "Results written to: $PHASE_DIR\phase1_results.csv"