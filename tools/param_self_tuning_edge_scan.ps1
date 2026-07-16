$EXE = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\build01\Release\cxvision_imgui_acceptance.exe"
$REPO = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo"
$IMAGE = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\test_images\L1_high_contrast\line_high_contrast_001.jpg"
$SCRIPT = "$REPO\cxparser\cxscript\module\cximage\stage25\param_regression\param_self_tuning_findline_direct.cxsc"
$RUN_ROOT = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxscript_runs\param_self_tuning"
$RUN_ID = "run_$(Get-Date -Format 'yyyyMMdd_HHmmss')"

$BASE_X0 = 82
$BASE_X1 = 1210

$BASE_THRESHOLD = 20
$BASE_WGAP = 8
$BASE_HGAP = 32
$BASE_LINEGAP = 6
$BASE_METHOD = 2
$BASE_FILTERPROFILE = 0
$BASE_TOOL_HALF_WIDTH = 20

$PHASE_DIR = "$RUN_ROOT\edge_scan\$RUN_ID"
New-Item -ItemType Directory -Path $PHASE_DIR -Force | Out-Null

Write-Host "===== Edge Position Scan ====="
Write-Host "Run ID: $RUN_ID"
Write-Host "Image Feature: Top edge of black rectangle (high contrast horizontal edge)"
Write-Host "Scanning y from 140 to 180 with 2-pixel steps"
Write-Host ""

$results = @()
$results += "roi_y,points,fit,avgdist"

$best_y = 0
$best_points = 0
$best_avgdist = 999

for ($y = 140; $y -le 180; $y += 2) {
    $CASE_ID = "y_$y"
    $OUT_DIR = "$PHASE_DIR\$CASE_ID"
    New-Item -ItemType Directory -Path $OUT_DIR -Force | Out-Null
    
    Write-Host "Running: roi_y=$y"
    
    & $EXE `
        --cxscript-headless `
        --image $IMAGE `
        --script $SCRIPT `
        --out $OUT_DIR `
        --case-name $CASE_ID `
        --roi-x0 $BASE_X0 `
        --roi-y0 $y `
        --roi-x1 $BASE_X1 `
        --roi-y1 $y `
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
    
    $summaryPath = "$OUT_DIR\result_summary.json"
    if (Test-Path $summaryPath) {
        try {
            $summary = Get-Content $summaryPath -Raw | ConvertFrom-Json
            $POINTS = $summary.valid_points_count
            $FIT = $summary.has_fit_line
            $AVGDIST = $summary.avgdist
        } catch {
            Write-Host "Error parsing $summaryPath"
        }
    }
    
    $results += "$y,$POINTS,$FIT,$AVGDIST"
    Write-Host "  -> points=$POINTS, fit=$FIT, avgdist=$AVGDIST"
    
    if ($FIT -eq $true -and $POINTS -ge 50 -and $POINTS -le 500 -and $AVGDIST -lt $best_avgdist) {
        $best_y = $y
        $best_points = $POINTS
        $best_avgdist = $AVGDIST
    }
}

$results | Out-File "$PHASE_DIR\edge_scan_results.csv" -Encoding utf8

Write-Host ""
Write-Host "=== Best Edge Position ==="
Write-Host "roi_y=$best_y, points=$best_points, avgdist=$best_avgdist"

Write-Host ""
Write-Host "===== Edge Scan Complete ====="
Write-Host "Results written to: $PHASE_DIR\edge_scan_results.csv"