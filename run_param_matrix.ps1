# 参数矩阵测试脚本 - 简化版
$exePath = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\AIbuild\Release\cxvision_imgui_acceptance.exe"
$imagePath = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\test_images\L1_high_contrast\line_high_contrast_001.jpg"
$scriptPath = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cxparser\cxscript\module\cximage\frozen\findline\findline_headless_baseline.cxsc"
$repoRoot = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo"

$thresholds = @(5, 10, 20, 40)
$methods = @(0, 1, 3)

$runId = "param_matrix2_$(Get-Date -Format 'yyyyMMdd_HHmmss')"
$baseOutDir = "$repoRoot\cxscript_runs\headless\$runId"

Set-Location $repoRoot

foreach ($method in $methods) {
    foreach ($threshold in $thresholds) {
        $caseName = "m${method}_t${threshold}"
        $outDir = "$baseOutDir\$caseName"
        New-Item -ItemType Directory -Force -Path $outDir | Out-Null
        
        Write-Host "Testing: method=$method threshold=$threshold"
        
        $output = & $exePath `
            --headless --cxscript-headless `
            --image $imagePath `
            --script $scriptPath `
            --case-name $caseName `
            --out $outDir `
            --max-steps 10000 `
            --timeout-sec 15 `
            --roi-x0 82 --roi-y0 183 --roi-x1 1210 --roi-y1 183 `
            --method $method `
            --threshold $threshold `
            --filterprofile 0 `
            --wgap 8 --hgap 32 --linegap 6 `
            --tool-half-width 20 `
            --max-elapsed-ms 15000 `
            --max-scan-lines 4096 `
            --max-samples 200000 2>&1
        
        $nonzero = 0
        $diffMax = 0
        foreach ($line in $output) {
            if ($line -match 'nonzero=(\d+)') { $nonzero = [int]$matches[1] }
            if ($line -match 'max_diff=(\d+)') { $diffMax = [int]$matches[1] }
        }
        
        $summaryPath = "$outDir\result_summary.json"
        $validPoints = 0
        $hasFit = "false"
        $stage = "unknown"
        if (Test-Path $summaryPath) {
            $s = Get-Content $summaryPath -Raw | ConvertFrom-Json
            $validPoints = $s.metrics.valid_points_count
            $hasFit = $s.facts.has_fit_line
            $stage = $s.facts.failure_stage
        }
        
        Write-Host "  nonzero=$nonzero diff_max=$diffMax valid_pts=$validPoints fit=$hasFit stage=$stage"
    }
}

Write-Host "`nDone."
