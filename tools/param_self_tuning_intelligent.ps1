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

$BASE_METHOD = 2
$BASE_TOOL_HALF_WIDTH = 50

$PHASE_DIR = "$RUN_ROOT\intelligent\$RUN_ID"
New-Item -ItemType Directory -Path $PHASE_DIR -Force | Out-Null

Write-Host "===== Intelligent Self-Tuning ====="
Write-Host "Run ID: $RUN_ID"
Write-Host "Image: $IMAGE"
Write-Host ""

function RunFindline($x0, $y0, $x1, $y1, $threshold, $wgap, $hgap, $linegap, $filterprofile, $caseId) {
    $OUT_DIR = "$PHASE_DIR\$caseId"
    New-Item -ItemType Directory -Path $OUT_DIR -Force | Out-Null
    
    & $EXE `
        --cxscript-headless `
        --image $IMAGE `
        --script $SCRIPT `
        --out $OUT_DIR `
        --case-name $caseId `
        --roi-x0 $x0 `
        --roi-y0 $y0 `
        --roi-x1 $x1 `
        --roi-y1 $y1 `
        --threshold $threshold `
        --wgap $wgap `
        --hgap $hgap `
        --linegap $linegap `
        --method $BASE_METHOD `
        --filterprofile $filterprofile `
        --tool-half-width $BASE_TOOL_HALF_WIDTH
    
    $summaryPath = "$OUT_DIR\result_summary.json"
    if (Test-Path $summaryPath) {
        try {
            $summary = Get-Content $summaryPath -Raw | ConvertFrom-Json
            return [PSCustomObject]@{
                points = $summary.valid_points_count
                fit = $summary.has_fit_line
                avgdist = $summary.avgdist
                failure_stage = if ($summary.failure_stage) { $summary.failure_stage } else { "none" }
            }
        } catch {
            Write-Host "Error parsing $summaryPath"
        }
    }
    return [PSCustomObject]@{ points = 0; fit = $false; avgdist = 0; failure_stage = "none" }
}

Write-Host "=== Phase 1: Gauge Region Scan ==="
$regionResults = @()
for ($offset = -50; $offset -le 50; $offset += 5) {
    $roi_y = $BASE_Y0 + $offset
    $result = RunFindline $BASE_X0 $roi_y $BASE_X1 $roi_y 20 8 32 6 1 "scan_y${offset}"
    $regionResults += [PSCustomObject]@{
        offset_y = $offset
        roi_y = $roi_y
        points = $result.points
        fit = $result.fit
        avgdist = $result.avgdist
    }
    Write-Host "  offset_y=$offset (roi_y=$roi_y): points=$($result.points), fit=$($result.fit), avgdist=$($result.avgdist)"
}

Write-Host ""
Write-Host "=== Region Analysis ==="
$maxPointsRegion = $regionResults | Sort-Object points -Descending | Select-Object -First 1
$minDistRegion = $regionResults | Where-Object { $_.points -gt 0 } | Sort-Object avgdist | Select-Object -First 1
$balancedRegion = $regionResults | Where-Object { $_.points -gt 500 -and $_.points -lt 2000 } | Sort-Object avgdist | Select-Object -First 1

Write-Host "Max points region: offset_y=$($maxPointsRegion.offset_y) (roi_y=$($maxPointsRegion.roi_y)), points=$($maxPointsRegion.points), avgdist=$($maxPointsRegion.avgdist)"
Write-Host "Min avgdist region: offset_y=$($minDistRegion.offset_y) (roi_y=$($minDistRegion.roi_y)), points=$($minDistRegion.points), avgdist=$($minDistRegion.avgdist)"
Write-Host "Balanced region: offset_y=$($balancedRegion.offset_y) (roi_y=$($balancedRegion.roi_y)), points=$($balancedRegion.points), avgdist=$($balancedRegion.avgdist)"

$selectedRoiY = $balancedRegion.roi_y
Write-Host ""
Write-Host "Selected ROI y=$selectedRoiY for parameter search (balanced points/dist)"
Write-Host ""

Write-Host "=== Phase 2: Parameter Search at ROI y=$selectedRoiY ==="
$paramResults = @()

$thresholds = @(10, 15, 20, 25)
$wgaps = @(5, 8, 11)
$hgaps = @(20, 32, 40)
$linegaps = @(3, 6, 9)
$filterprofiles = @(0, 1)

foreach ($threshold in $