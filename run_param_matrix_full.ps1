# 参数矩阵全链路诊断脚本
# 目的：验证 threshold/method 参数注入链路，分析 roi_7blur_gap_mud_thre_bw 各阶段统计
# 矩阵：threshold=5/10/20/40 × method=0/1/3

param(
    [Parameter(Mandatory = $true)]
    [string]$ExePath,
    [string]$ImagePath = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\test_images\L1_high_contrast\line_high_contrast_001.jpg",
    [string]$ScriptPath = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cxparser\cxscript\module\cximage\frozen\findline\findline_headless_baseline.cxsc",
    [string]$RepoRoot = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo"
)

if (-not (Test-Path -LiteralPath $ExePath)) {
    throw "Executable does not exist: $ExePath"
}
if (-not (Test-Path -LiteralPath $ImagePath)) {
    throw "Image does not exist: $ImagePath"
}
if (-not (Test-Path -LiteralPath $ScriptPath)) {
    throw "CxScript does not exist: $ScriptPath"
}
if (-not (Test-Path -LiteralPath $RepoRoot)) {
    throw "Repository root does not exist: $RepoRoot"
}

$exeItem = Get-Item -LiteralPath $ExePath
Write-Host "Binary: $($exeItem.FullName)"
Write-Host "BinaryLastWriteTime: $($exeItem.LastWriteTime.ToString('o'))"

$thresholds = @(5, 10, 20, 40)
$methods = @(0, 1, 3)

$runId = "param_matrix_full_$(Get-Date -Format 'yyyyMMdd_HHmmss')"
$baseOutDir = "$repoRoot\cxscript_runs\headless\$runId"
$sharedLog = "$repoRoot\cxscript_runs\_shared\cxvision_imgui_acceptance.jsonl"

Set-Location $repoRoot

$results = @()

foreach ($method in $methods) {
    foreach ($threshold in $thresholds) {
        $caseName = "m${method}_t${threshold}"
        $outDir = "$baseOutDir\$caseName"
        New-Item -ItemType Directory -Force -Path $outDir | Out-Null
        
        Write-Host "`n=== Testing: method=$method threshold=$threshold ==="
        
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
            --max-samples 200000 `
            --unified-log $sharedLog 2>&1
        
        # Parse DIAG output
        $diagMeasureEntry = ""
        $diagInputROI = ""
        $diagBlur = ""
        $diagGapDiff = ""
        $diagThreshold = ""
        $exitCode = "unknown"
        
        foreach ($line in $output) {
            if ($line -match 'FindLine::Measure entry: threshold=(\d+)') {
                $diagMeasureEntry = $line
                Write-Host "  [MEAS_ENTRY] threshold matched in C++: $($matches[1])"
            }
            if ($line -match 'input ROI=\(') {
                $diagInputROI = $line
            }
            if ($line -match 'after blur:') {
                $diagBlur = $line
            }
            if ($line -match 'gap diff.*max_diff=(\d+)') {
                $diagGapDiff = $line
                Write-Host "  [GAP_DIFF] max_diff=$($matches[1]) threshold=$threshold"
            }
            if ($line -match 'after threshold:.*nonzero=(\d+)') {
                $diagThreshold = $line
                Write-Host "  [THRESH] nonzero=$($matches[1])"
            }
            if ($line -match 'exit code.*(\d+)' -or $line -match 'ExitCode.*(\d+)') {
                $exitCode = $matches[1]
            }
        }
        
        # Read result_summary.json
        $summaryPath = "$outDir\result_summary.json"
        $validPoints = 0
        $hasFit = "false"
        $stage = "unknown"
        $elapsedMs = 0
        $actualThreshold = 0
        $actualMethod = -1
        $scriptSelectedThreshold = $null
        
        if (Test-Path $summaryPath) {
            $s = Get-Content $summaryPath -Raw | ConvertFrom-Json
            $validPoints = $s.metrics.valid_points_count
            $hasFit = $s.facts.has_fit_line
            $stage = $s.facts.failure_stage
            $elapsedMs = $s.metrics.elapsed_ms
            
            # selected_* is a script echo, not the injection source of truth.
            # Compare CLI/manifest values to injected_*; report echo separately.
            if ($null -ne $s.metrics.injected_threshold) {
                $actualThreshold = [int]$s.metrics.injected_threshold
            }
            if ($null -ne $s.metrics.injected_method) {
                $actualMethod = [int]$s.metrics.injected_method
            }
            if ($null -ne $s.metrics.script_selected_threshold) {
                $scriptSelectedThreshold = [int]$s.metrics.script_selected_threshold
            }
        }
        
        # Check if threshold was properly injected
        $injectionOk = ($actualThreshold -eq $threshold)
        Write-Host "  Result: valid_pts=$validPoints fit=$hasFit stage=$stage elapsed=$elapsedMs"
        Write-Host "  Injection: injected_threshold=$actualThreshold expected=$threshold ok=$injectionOk script_selected_threshold=$scriptSelectedThreshold"
        
        $results += [PSCustomObject]@{
            Method=$method
            Threshold=$threshold
            ActualThreshold=$actualThreshold
            ScriptSelectedThreshold=$scriptSelectedThreshold
            ActualMethod=$actualMethod
            InjectionOK=$injectionOk
            ValidPoints=$validPoints
            HasFit=$hasFit
            Stage=$stage
            ElapsedMs=$elapsedMs
            GapDiffMax=$(if ($diagGapDiff -match 'max_diff=(\d+)') {$matches[1]} else {"N/A"})
            NonzeroAfterThresh=$(if ($diagThreshold -match 'nonzero=(\d+)') {$matches[1]} else {"N/A"})
            BlurStats=$(if ($diagBlur -match 'min=([\d.]+) max=([\d.]+)') {"$($matches[1])-$($matches[2])"} else {"N/A"})
            InputStats=$(if ($diagInputROI -match 'min=([\d.]+) max=([\d.]+) mean=([\d.]+)') {"min=$($matches[1]) max=$($matches[2]) mean=$($matches[3])"} else {"N/A"})
        }
    }
}

Write-Host "`n=== FULL MATRIX RESULTS ==="
Write-Host $results | Format-Table -AutoSize | Out-String

$resultsPath = "$baseOutDir\matrix_results.csv"
$results | Export-Csv -Path $resultsPath -NoTypeInformation -Encoding UTF8
$runMetadataPath = "$baseOutDir\run_metadata.json"
[ordered]@{
    binary_path = $exeItem.FullName
    binary_last_write_time = $exeItem.LastWriteTime.ToString('o')
    image_path = $ImagePath
    script_path = $ScriptPath
    repo_root = $RepoRoot
    working_directory = (Get-Location).Path
    shared_log = $sharedLog
} | ConvertTo-Json | Set-Content -LiteralPath $runMetadataPath -Encoding UTF8
Write-Host "`nResults saved to: $resultsPath"
Write-Host "Run metadata saved to: $runMetadataPath"
Write-Host "Done."
