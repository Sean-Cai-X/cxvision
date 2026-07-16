$EXE = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\build01\Release\cxvision_imgui_acceptance.exe"
$REPO = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo"
$SCRIPT = "$REPO\cxparser\cxscript\module\cximage\stage25\param_regression\param_self_tuning_findline_direct.cxsc"
$RUN_ROOT = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxscript_runs\param_self_tuning"
$RUN_ID = "run_" + (Get-Date -Format 'yyyyMMdd_HHmmss')

$PHASE_DIR = $RUN_ROOT + "\simple_" + $RUN_ID
New-Item -ItemType Directory -Path $PHASE_DIR -Force | Out-Null

Write-Host "===== Simple Self-Tuning Extension ====="
Write-Host ("Run ID: " + $RUN_ID)
Write-Host ""

$images = @(
    @{ Name = "L1_line_high_contrast_002"; Path = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\test_images\L1_high_contrast\line_high_contrast_002.jpg"; Target="top edge" },
    @{ Name = "L2_line_low_contrast_001"; Path = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\test_images\L2_low_contrast_illumination\line_low_contrast_001.jpg"; Target="low contrast edge" },
    @{ Name = "L2_line_uneven_light_001"; Path = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\test_images\L2_low_contrast_illumination\line_uneven_light_001.jpg"; Target="uneven light edge" },
    @{ Name = "L3_line_complex_boundary_001"; Path = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\test_images\L3_complex_boundary\line_complex_boundary_001.jpg"; Target="complex boundary" },
    @{ Name = "L3_line_near_interference_001"; Path = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\test_images\L3_complex_boundary\line_near_interference_001.jpg"; Target="near interference" }
)

$final_results = @()
$final_results += "image_name,target,y,threshold,wgap,hgap,linegap,method,filterprofile,points,fit,avgdist"

foreach ($img in $images) {
    Write-Host ("=== " + $img.Name + " ===")
    
    $best_y = 180
    $best_points = 0
    
    Write-Host ("  Scanning ROI Y (50-400, step=10)...")
    for ($y = 50; $y -le 400; $y += 10) {
        $CASE_ID = $img.Name + "_y" + $y
        $OUT_DIR = $PHASE_DIR + "\\" + $CASE_ID
        New-Item -ItemType Directory -Path $OUT_DIR -Force | Out-Null
        
        & $EXE `
            --cxscript-headless `
            --image $img.Path `
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
        
        $summaryPath = $OUT_DIR + "\result_summary.json"
        if (Test-Path $summaryPath) {
            try {
                $summary = Get-Content $summaryPath -Raw | ConvertFrom-Json
                $POINTS = $summary.valid_points_count
                $FIT = $summary.has_fit_line
            } catch {}
        }
        
        if ($FIT -eq $true -and $POINTS -ge 10 -and $POINTS -le 500) {
            $best_y = $y
            $best_points = $POINTS
            Write-Host ("    y=" + $y + ": " + $POINTS + " points")
        }
    }
    
    Write-Host ("  Best Y: " + $best_y)
    
    Write-Host ("  Optimizing parameters...")
    $best_params = @{}
    $best_params.points = 0
    $best_params.avgdist = 999
    
    $thresholds = @(10, 15, 20, 25, 30)
    $wgaps = @(5, 8, 11)
    $hgaps = @(20, 32)
    $linegaps = @(3, 6)
    $filterprofiles = @(0, 1)
    $methods = @(0, 1, 2)
    
    foreach ($t in $thresholds) {
        foreach ($w in $wgaps) {
            foreach ($h in $hgaps) {
                foreach ($l in $linegaps) {
                    foreach ($f in $filterprofiles) {
                        foreach ($m in $methods) {
                            $CASE_ID = $img.Name + "_t" + $t + "_w" + $w + "_h" + $h + "_l" + $l + "_f" + $f + "_m" + $m
                            $OUT_DIR = $PHASE_DIR + "\\" + $CASE_ID
                            New-Item -ItemType Directory -Path $OUT_DIR -Force | Out-Null
                            
                            & $EXE `
                                --cxscript-headless `
                                --image $img.Path `
                                --script $SCRIPT `
                                --out $OUT_DIR `
                                --case-name $CASE_ID `
                                --roi-x0 82 `
                                --roi-y0 $best_y `
                                --roi-x1 1210 `
                                --roi-y1 $best_y `
                                --threshold $t `
                                --wgap $w `
                                --hgap $h `
                                --linegap $l `
                                --method $m `
                                --filterprofile $f `
                                --tool-half-width 20
                            
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
                            
                            if ($FIT -eq $true -and $POINTS -ge 30 -and $POINTS -le 500 -and $AVGDIST -lt $best_params.avgdist) {
                                $best_params.threshold = $t
                                $best_params.wgap = $w
                                $best_params.hgap = $h
                                $best_params.linegap = $l
                                $best_params.filterprofile = $f
                                $best_params.method = $m
                                $best_params.points = $POINTS
                                $best_params.avgdist = $AVGDIST
                            }
                        }
                    }
                }
            }
        }
    }
    
    $fit_str = "false"
    if ($best_params.points -gt 0) { $fit_str = "true" }
    $final_results += $img.Name + "," + $img.Target + "," + $best_y + "," + $best_params.threshold + "," + $best_params.wgap + "," + $best_params.hgap + "," + $best_params.linegap + "," + $best_params.method + "," + $best_params.filterprofile + "," + $best_params.points + "," + $fit_str + "," + $best_params.avgdist
    
    Write-Host ("  Result: y=" + $best_y + ", t=" + $best_params.threshold + ", w=" + $best_params.wgap + ", h=" + $best_params.hgap)
    Write-Host ("          points=" + $best_params.points + ", avgdist=" + $best_params.avgdist)
    Write-Host ""
}

$final_results | Out-File ($PHASE_DIR + "\results.csv") -Encoding utf8

Write-Host "===== Results ====="
$final_results | ConvertFrom-Csv | Format-Table image_name, target, y, points, avgdist

Write-Host ""
Write-Host ("Results: " + $PHASE_DIR + "\results.csv")