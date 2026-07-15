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

$PHASE_DIR = "$RUN_ROOT\phase2\$RUN_ID"
New-Item -ItemType Directory -Path $PHASE_DIR -Force | Out-Null

Write-Host "===== Phase 2: Parameter Grid Search ====="
Write-Host "Run ID: $RUN_ID"
Write-Host "ROI: ($BASE_X0,$BASE_Y0)-($BASE_X1,$BASE_Y1)"
Write-Host "Fixed Parameters: method=$BASE_METHOD, tool_half_width=$BASE_TOOL_HALF_WIDTH"
Write-Host ""

$results = @()
$results += "threshold,wgap,hgap,linegap,filterprofile,points,fit,avgdist,failure_stage"

$thresholds = @(10, 15, 20, 25)
$wgaps = @(5, 8, 11, 14)
$hgaps = @(20, 32, 40)
$linegaps = @(3, 6, 9)
$filterprofiles = @(0, 1, 2)

$total = $thresholds.Count * $wgaps.Count * $hgaps.Count * $linegaps.Count * $filterprofiles.Count
$count = 0

foreach ($threshold in $thresholds) {
    foreach ($wgap in $wgaps) {
        foreach ($hgap in $hgaps) {
            foreach ($linegap in $linegaps) {
                foreach ($filterprofile in $filterprofiles) {
                    $count++
                    $CASE_ID = "t${threshold}_w${wgap}_h${hgap}_l${linegap}_f${filterprofile}"
                    $OUT_DIR = "$PHASE_DIR\$CASE_ID"
                    New-Item -ItemType Directory -Path $OUT_DIR -Force | Out-Null
                    
                    Write-Host "Running [$count/$total]: threshold=$threshold, wgap=$wgap, hgap=$hgap, linegap=$linegap, filterprofile=$filterprofile"
                    
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
                        --method $BASE_METHOD `
                        --filterprofile $filterprofile `
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
                    
                    $results += "$threshold,$wgap,$hgap,$linegap,$filterprofile,$POINTS,$FIT,$AVGDIST,$FAILURE_STAGE"
                    Write-Host "  -> points=$POINTS, fit=$FIT, avgdist=$AVGDIST"
                }
            }
        }
    }
}

$results | Out-File "$PHASE_DIR\phase2_results.csv" -Encoding utf8

Write-Host ""
Write-Host "Top 10 candidates by points:"
$results | ConvertFrom-Csv | Where-Object { $_.fit -eq "True" } | Sort-Object -Property points -Descending | Select-Object -First 10

Write-Host ""
Write-Host "===== Phase 2 Complete ====="
Write-Host "Results written to: $PHASE_DIR\phase2_results.csv"