$EXE = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\build01\Release\cxvision_imgui_acceptance.exe"
$REPO = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo"
$SCRIPT = "$REPO\cxparser\cxscript\module\cximage\stage25\param_regression\param_self_tuning_findline_direct.cxsc"
$RUN_ROOT = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\test_output"
$RUN_ID = "interference_scan_" + (Get-Date -Format 'yyyyMMdd_HHmmss')

$PHASE_DIR = $RUN_ROOT + "\" + $RUN_ID
New-Item -ItemType Directory -Path $PHASE_DIR -Force | Out-Null

$IMAGE = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\test_images\L3_complex_boundary\line_near_interference_001.jpg"

Write-Host "===== Detailed ROI Scan for L3_near_interference_001 ====="
Write-Host ("Run ID: " + $RUN_ID)
Write-Host "Image: Metal part with square hole pattern"
Write-Host "Scanning y=50 to y=450, step=25"
Write-Host ""

$results = @()
$results += "y,points,fit,avgdist"

for ($y = 50; $y -le 450; $y += 25) {
    $CASE_ID = "y" + $y
    $OUT_DIR = $PHASE_DIR + "\\" + $CASE_ID
    New-Item -ItemType Directory -Path $OUT_DIR -Force | Out-Null
    
    & $EXE `
        --cxscript-headless `
        --image $IMAGE `
        --script $SCRIPT `
        --out $OUT_DIR `
        --case-name $CASE_ID `
        --roi-x0 82 `
        --roi-y0 $y `
        --roi-x1 1210 `
        --roi-y1 $y `
        --threshold 20 `
        --wgap 8 `
        --hgap 32 `
        --linegap 6 `
        --method 1 `
        --filterprofile 0 `
        --tool-half-width 30
    
    $POINTS = 0
    $FIT = $false
    $AVGDIST = 0.0
    
    $summaryPath = $OUT_DIR + "\result_summary.json"
    if (Test-Path $summaryPath) {
        try {
            $summary = Get-Content $summaryPath -Raw | ConvertFrom-Json
            $POINTS = $summary.valid_points_count
            $FIT = $summary.has_fit_line
            $AVGDIST = $summary.avgdist
        } catch {}
    }
    
    $fit_str = "false"
    if ($FIT -eq $true) { $fit_str = "true" }
    $results += $y + "," + $POINTS + "," + $fit_str + "," + $AVGDIST
    
    if ($POINTS -gt 0) {
        Write-Host ("  y=" + $y + ": points=" + $POINTS + ", fit=" + $fit_str + ", avgdist=" + $AVGDIST)
    }
}

$results | Out-File ($PHASE_DIR + "\scan_results.csv") -Encoding utf8

Write-Host ""
Write-Host "===== Scan Results ====="
$results | ConvertFrom-Csv | Where-Object { [int]$_.points -gt 0 } | Format-Table y, points, avgdist

Write-Host ""
Write-Host ("Results written to: " + $PHASE_DIR + "\scan_results.csv")