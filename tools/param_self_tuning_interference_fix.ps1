$EXE = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\build01\Release\cxvision_imgui_acceptance.exe"
$REPO = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo"
$SCRIPT = "$REPO\cxparser\cxscript\module\cximage\stage25\param_regression\param_self_tuning_findline_direct.cxsc"
$RUN_ROOT = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\test_output"
$RUN_ID = "interference_fix_" + (Get-Date -Format 'yyyyMMdd_HHmmss')

$PHASE_DIR = $RUN_ROOT + "\" + $RUN_ID
New-Item -ItemType Directory -Path $PHASE_DIR -Force | Out-Null

Write-Host "===== Fixing Interference in L3_near_interference_001 ====="
Write-Host ("Run ID: " + $RUN_ID)
Write-Host "Image: Metal part with square hole pattern"
Write-Host "Problem: Yellow line is tilted, detecting all edges of square"
Write-Host "Strategy: Increase linegap to reduce vertical scan density"
Write-Host ""

$tests = @(
    @{ Case = "baseline"; Y=200; T=20; W=8; H=32; L=6; M=1; F=0 },
    @{ Case = "linegap10"; Y=200; T=20; W=8; H=32; L=10; M=1; F=0 },
    @{ Case = "linegap15"; Y=200; T=20; W=8; H=32; L=15; M=1; F=0 },
    @{ Case = "linegap20"; Y=200; T=20; W=8; H=32; L=20; M=1; F=0 },
    @{ Case = "linegap10_hgap20"; Y=200; T=20; W=8; H=20; L=10; M=1; F=0 },
    @{ Case = "linegap10_wgap15"; Y=200; T=20; W=15; H=32; L=10; M=1; F=0 },
    @{ Case = "linegap10_t15"; Y=200; T=15; W=8; H=32; L=10; M=1; F=0 },
    @{ Case = "linegap10_method0"; Y=200; T=20; W=8; H=32; L=10; M=0; F=0 }
)

$IMAGE = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\test_images\L3_complex_boundary\line_near_interference_001.jpg"

$results = @()
$results += "case,y,threshold,wgap,hgap,linegap,method,filterprofile,points,fit,avgdist"

foreach ($test in $tests) {
    $CASE_ID = $test.Case
    $OUT_DIR = $PHASE_DIR + "\\" + $CASE_ID
    New-Item -ItemType Directory -Path $OUT_DIR -Force | Out-Null
    
    Write-Host ("Running: " + $CASE_ID)
    Write-Host ("  L=" + $test.L + ", H=" + $test.H + ", W=" + $test.W + ", T=" + $test.T + ", M=" + $test.M)
    
    & $EXE `
        --cxscript-headless `
        --image $IMAGE `
        --script $SCRIPT `
        --out $OUT_DIR `
        --case-name $CASE_ID `
        --roi-x0 82 `
        --roi-y0 $test.Y `
        --roi-x1 1210 `
        --roi-y1 $test.Y `
        --threshold $test.T `
        --wgap $test.W `
        --hgap $test.H `
        --linegap $test.L `
        --method $test.M `
        --filterprofile $test.F `
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
    $results += $CASE_ID + "," + $test.Y + "," + $test.T + "," + $test.W + "," + $test.H + "," + $test.L + "," + $test.M + "," + $test.F + "," + $POINTS + "," + $fit_str + "," + $AVGDIST
    
    Write-Host ("  -> points=" + $POINTS + ", fit=" + $fit_str + ", avgdist=" + $AVGDIST)
    Write-Host ""
}

$results | Out-File ($PHASE_DIR + "\results.csv") -Encoding utf8

Write-Host "===== Results ====="
$results | ConvertFrom-Csv | Format-Table case, linegap, wgap, hgap, points, fit, avgdist

Write-Host ""
Write-Host ("Results written to: " + $PHASE_DIR + "\results.csv")