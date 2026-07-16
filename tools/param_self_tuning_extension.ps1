$EXE = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\build01\Release\cxvision_imgui_acceptance.exe"
$REPO = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo"
$SCRIPT = "$REPO\cxparser\cxscript\module\cximage\stage25\param_regression\param_self_tuning_findline_direct.cxsc"
$RUN_ROOT = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxscript_runs\param_self_tuning"
$RUN_ID = "run_$(Get-Date -Format 'yyyyMMdd_HHmmss')"

$PHASE_DIR = "$RUN_ROOT\extension\$RUN_ID"
New-Item -ItemType Directory -Path $PHASE_DIR -Force | Out-Null

Write-Host "===== Self-Tuning Extension: L1-L3 Line Images ====="
Write-Host "Run ID: $RUN_ID"
Write-Host "Base Parameters: threshold=30, wgap=11, hgap=20, linegap=3, method=1, filterprofile=1, objfilter=1"
Write-Host ""

$images = @(
    @{ Name = "L1_line_high_contrast_001"; Path = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\test_images\L1_high_contrast\line_high_contrast_001.jpg"; X0=82; Y0=180; X1=1210; Y1=180; Target="top edge" },
    @{ Name = "L1_line_high_contrast_002"; Path = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\test_images\L1_high_contrast\line_high_contrast_002.jpg"; X0=82; Y0=180; X1=1210; Y1=180; Target="top edge" },
    @{ Name = "L2_line_low_contrast_001"; Path = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\test_images\L2_low_contrast_illumination\line_low_contrast_001.jpg"; X0=82; Y0=180; X1=1210; Y1=180; Target="low contrast edge" },
    @{ Name = "L2_line_uneven_light_001"; Path = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\test_images\L2_low_contrast_illumination\line_uneven_light_001.jpg"; X0=82; Y0=180; X1=1210; Y1=180; Target="uneven light edge" },
    @{ Name = "L3_line_complex_boundary_001"; Path = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\test_images\L3_complex_boundary\line_complex_boundary_001.jpg"; X0=82; Y0=180; X1=1210; Y1=180; Target="complex boundary" },
    @{ Name = "L3_line_near_interference_001"; Path = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\test_images\L3_complex_boundary\line_near_interference_001.jpg"; X0=82; Y0=180; X1=1210; Y1=180; Target="near interference" }
)

$results = @()
$results += "image_name,roi_x0,roi_y0,roi_x1,roi_y1,target,threshold,wgap,hgap,linegap,method,filterprofile,points,fit,avgdist,failure_stage"

foreach ($img in $images) {
    $CASE_ID = $img.Name
    $OUT_DIR = "$PHASE_DIR\$CASE_ID"
    New-Item -ItemType Directory -Path $OUT_DIR -Force | Out-Null
    
    Write-Host "Processing: $($img.Name)"
    Write-Host "  Image: $($img.Path)"
    Write-Host "  ROI: ($($img.X0),$($img.Y0))-($($img.X1),$($img.Y1))"
    Write-Host "  Target: $($img.Target)"
    
    & $EXE `
        --cxscript-headless `
        --image $img.Path `
        --script $SCRIPT `
        --out $OUT_DIR `
        --case-name $CASE_ID `
        --roi-x0 $img.X0 `
        --roi-y0 $img.Y0 `
        --roi-x1 $img.X1 `
        --roi-y1 $img.Y1 `
        --threshold 30 `
        --wgap 11 `
        --hgap 20 `
        --linegap 3 `
        --method 1 `
        --filterprofile 1 `
        --tool-half-width 20
    
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
    
    $results += "$($img.Name),$($img.X0),$($img.Y0),$($img.X1),$($img.Y1),$($img.Target),30,11,20,3,1,1,$POINTS,$FIT,$AVGDIST,$FAILURE_STAGE"
    Write-Host "  -> points=$POINTS, fit=$FIT, avgdist=$AVGDIST, failure_stage=$FAILURE_STAGE"
    Write-Host ""
}

$results | Out-File "$PHASE_DIR\extension_results.csv" -Encoding utf8

Write-Host "===== Extension Results Summary ====="
$results | ConvertFrom-Csv | Format-Table image_name, target, points, fit, avgdist, failure_stage

Write-Host ""
Write-Host "===== Extension Test Complete ====="
Write-Host "Results written to: $PHASE_DIR\extension_results.csv"