$EXE = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\build01\Release\cxvision_imgui_acceptance.exe"
$REPO = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo"
$SCRIPT = "$REPO\cxparser\cxscript\module\cximage\stage25\param_regression\param_self_tuning_findline_direct.cxsc"
$RUN_ROOT = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\test_output"
$RUN_ID = "roi_scan_" + (Get-Date -Format 'yyyyMMdd_HHmmss')

$PHASE_DIR = $RUN_ROOT + "\" + $RUN_ID
New-Item -ItemType Directory -Path $PHASE_DIR -Force | Out-Null

Write-Host "===== Findline ROI Position Scan ====="
Write-Host ("Run ID: " + $RUN_ID)
Write-Host "Scanning y=50 to y=450, step=5"
Write-Host ""

$images = @(
    @{ Name = "L1_high_contrast_002"; Image = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\test_images\L1_high_contrast\line_high_contrast_002.jpg" },
    @{ Name = "L2_low_contrast_001"; Image = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\test_images\L2_low_contrast_illumination\line_low_contrast_001.jpg" },
    @{ Name = "L2_uneven_light_001"; Image = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\test_images\L2_low_contrast_illumination\line_uneven_light_001.jpg" },
    @{ Name = "L3_complex_boundary_001"; Image = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\test_images\L3_complex_boundary\line_complex_boundary_001.jpg" },
    @{ Name = "L3_near_interference_001"; Image = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\test_images\L3_complex_boundary\line_near_interference_001.jpg" }
)

$scan_results = @()
$scan_results += "image_name,y,points,fit,avgdist"

foreach ($img in $images) {
    Write-Host ("=== " + $img.Name + " ===")
    $best_y = 0
    $best_points = 0
    $best_avgdist = 999
    
    for ($y = 50; $y -le 450; $y += 5) {
        $CASE_ID = $img.Name + "_y" + $y
        $OUT_DIR = $PHASE_DIR + "\\" + $CASE_ID
        New-Item -ItemType Directory -Path $OUT_DIR -Force | Out-Null
        
        & $EXE `
            --cxscript-headless `
            --image $img.Image `
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
        
        if ($FIT -eq $true -and $POINTS -ge 10) {
            $best_y = $y
            $best_points = $POINTS
            $best_avgdist = $AVGDIST
            Write-Host ("    y=" + $y + ": points=" + $POINTS + ", avgdist=" + $AVGDIST)
        }
        
        $fit_str = "false"
        if ($FIT -eq $true) { $fit_str = "true" }
        $scan_results += $img.Name + "," + $y + "," + $POINTS + "," + $fit_str + "," + $AVGDIST
    }
    
    Write-Host ("  Best Y: " + $best_y + " (" + $best_points + " points)")
    Write-Host ""
}

$scan_results | Out-File ($PHASE_DIR + "\scan_results.csv") -Encoding utf8

Write-Host "===== ROI Scan Complete ====="
Write-Host ("Results written to: " + $PHASE_DIR + "\scan_results.csv")