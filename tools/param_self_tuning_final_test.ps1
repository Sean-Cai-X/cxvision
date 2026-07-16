$EXE = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\build01\Release\cxvision_imgui_acceptance.exe"
$REPO = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo"
$IMAGE = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\test_images\L1_high_contrast\line_high_contrast_001.jpg"
$SCRIPT = "$REPO\cxparser\cxscript\module\cximage\stage25\param_regression\param_self_tuning_findline_direct.cxsc"
$RUN_ROOT = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxscript_runs\param_self_tuning"
$RUN_ID = "run_$(Get-Date -Format 'yyyyMMdd_HHmmss')"

$BASE_X0 = 82
$BASE_Y0 = 180
$BASE_X1 = 1210
$BASE_Y1 = 180

$BASE_TOOL_HALF_WIDTH = 20

$PHASE_DIR = "$RUN_ROOT\final_test\$RUN_ID"
New-Item -ItemType Directory -Path $PHASE_DIR -Force | Out-Null

Write-Host "===== Final Parameter Test (with setobjfilter=1) ====="
Write-Host "Run ID: $RUN_ID"
Write-Host "ROI: ($BASE_X0,$BASE_Y0)-($BASE_X1,$BASE_Y1)"
Write-Host "Image Feature: High contrast horizontal edge (black->white at top)"
Write-Host "objfilter=1 (object filter enabled)"
Write-Host ""

$results = @()
$results += "threshold,wgap,hgap,linegap,filterprofile,method,points,fit,avgdist"

$thresholds = @(20, 30, 40, 50, 60)
$wgaps = @(5, 8, 11)
$hgaps = @(20, 32)
$linegaps = @(3, 6)
$filterprofiles = @(0, 1, 2)
$methods = @(0, 1, 2)

$total = $thresholds.Count * $wgaps.Count * $hgaps.Count * $linegaps.Count * $filterprofiles.Count * $methods.Count
$count = 0

foreach ($threshold in $thresholds) {
    foreach ($wgap in $wgaps) {
        foreach ($hgap in $hgaps) {
            foreach ($linegap in $linegaps) {
                foreach ($filterprofile in $filterprofiles) {
                    foreach ($method in $methods) {
                        $count++
                        $CASE_ID = "t${threshold}_w${wgap}_h${hgap}_l${linegap}_f${filterprofile}_m${method}"
                        $OUT_DIR = "$PHASE_DIR\$CASE_ID"
                        New-Item -ItemType Directory -Path $OUT_DIR -Force | Out-Null
                        
                        Write-Host "Running [$count/$total]: t=$threshold, w=$wgap, h=$hgap, l=$linegap, f=$filterprofile, m=$method"
                        
                        & $EXE `
                            --cxscript-headless `
                            --image $IMAGE `
                            --script $SCRIPT `
                            --out $OUT_DIR `
                            --case-name $CASE_ID `
                            --roi-x0 $BASE_X0 `
                            --roi-y0 $BASE_Y0 `
                            --roi-x1 $BASE_X1 `
                            --roi-y1 $BASE_Y1 `
                            --threshold $threshold `
                            --wgap $wgap `
                            --hgap $hgap `
                            --linegap $linegap `
                            --method $method `
                            --filterprofile $filterprofile `
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
                        
                        $results += "$threshold,$wgap,$hgap,$linegap,$filterprofile,$method,$POINTS,$FIT,$AVGDIST"
                        Write-Host "  -> points=$POINTS, fit=$FIT, avgdist=$AVGDIST"
                    }
                }
            }
        }
    }
}

$results | Out-File "$PHASE_DIR\final_results.csv" -Encoding utf8

Write-Host ""
Write-Host "=== Top Candidates (fit=True, points 50-500, avgdist < 25) ==="
$results | ConvertFrom-Csv | Where-Object { 
    $_.fit -eq "True" -and 
    [int]$_.points -ge 50 -and 
    [int]$_.points -le 500 -and 
    [double]$_.avgdist -lt 25 
} | Sort-Object -Property avgdist, points | Select-Object -First 10

Write-Host ""
Write-Host "===== Final Test Complete ====="
Write-Host "Results written to: $PHASE_DIR\final_results.csv"