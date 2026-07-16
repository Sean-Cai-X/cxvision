$EXE = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\build01\Release\cxvision_imgui_acceptance.exe"
$REPO = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo"
$SCRIPT = "$REPO\cxparser\cxscript\module\cximage\stage25\param_regression\param_self_tuning_findline_direct.cxsc"
$RUN_ROOT = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxscript_runs\findline_extension"
$RUN_ID = "run_" + (Get-Date -Format 'yyyyMMdd')

$PHASE_DIR = $RUN_ROOT + "\" + $RUN_ID
New-Item -ItemType Directory -Path $PHASE_DIR -Force | Out-Null

Write-Host "===== Findline Extension Test ====="
Write-Host ("Run ID: " + $RUN_ID)
Write-Host ""

$tests = @(
    @{ Name = "L1_high_contrast_002"; Image = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\test_images\L1_high_contrast\line_high_contrast_002.jpg"; Y=180; Threshold=30; Wgap=11; Hgap=20; Linegap=3; Method=1; Filterprofile=1 },
    @{ Name = "L2_low_contrast_001"; Image = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\test_images\L2_low_contrast_illumination\line_low_contrast_001.jpg"; Y=180; Threshold=15; Wgap=8; Hgap=32; Linegap=6; Method=1; Filterprofile=0 },
    @{ Name = "L2_uneven_light_001"; Image = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\test_images\L2_low_contrast_illumination\line_uneven_light_001.jpg"; Y=180; Threshold=15; Wgap=8; Hgap=32; Linegap=6; Method=0; Filterprofile=0 },
    @{ Name = "L3_complex_boundary_001"; Image = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\test_images\L3_complex_boundary\line_complex_boundary_001.jpg"; Y=180; Threshold=20; Wgap=8; Hgap=32; Linegap=6; Method=1; Filterprofile=0 },
    @{ Name = "L3_near_interference_001"; Image = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\test_images\L3_complex_boundary\line_near_interference_001.jpg"; Y=180; Threshold=25; Wgap=11; Hgap=20; Linegap=3; Method=1; Filterprofile=1 }
)

$results = @()
$results += "name,y,threshold,wgap,hgap,linegap,method,filterprofile,points,fit,avgdist"

foreach ($test in $tests) {
    $CASE_ID = $test.Name
    $OUT_DIR = $PHASE_DIR + "\\" + $CASE_ID
    New-Item -ItemType Directory -Path $OUT_DIR -Force | Out-Null
    
    Write-Host ("Running: " + $CASE_ID)
    Write-Host ("  y=" + $test.Y + ", t=" + $test.Threshold + ", w=" + $test.Wgap + ", h=" + $test.Hgap)
    
    & $EXE `
        --cxscript-headless `
        --image $test.Image `
        --script $SCRIPT `
        --out $OUT_DIR `
        --case-name $CASE_ID `
        --roi-x0 82 `
        --roi-y0 $test.Y `
        --roi-x1 1210 `
        --roi-y1 $test.Y `
        --threshold $test.Threshold `
        --wgap $test.Wgap `
        --hgap $test.Hgap `
        --linegap $test.Linegap `
        --method $test.Method `
        --filterprofile $test.Filterprofile `
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
    
    $fit_str = "false"
    if ($FIT -eq $true) { $fit_str = "true" }
    $results += $CASE_ID + "," + $test.Y + "," + $test.Threshold + "," + $test.Wgap + "," + $test.Hgap + "," + $test.Linegap + "," + $test.Method + "," + $test.Filterprofile + "," + $POINTS + "," + $fit_str + "," + $AVGDIST
    
    Write-Host ("  -> points=" + $POINTS + ", fit=" + $fit_str + ", avgdist=" + $AVGDIST)
    Write-Host ""
}

$results | Out-File ($PHASE_DIR + "\results.csv") -Encoding utf8

Write-Host "===== Results ====="
$results | ConvertFrom-Csv | Format-Table name, points, fit, avgdist

Write-Host ""
Write-Host ("Results written to: " + $PHASE_DIR + "\results.csv")