$baseDir = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai"
$repoDir = "$baseDir\cxvision_repo"
$buildDir = "$baseDir\build01"
$testImagesDir = "$baseDir\test_images"
$outputDir = "$baseDir\test_output\self_tuning_optimized_$(Get-Date -Format 'yyyyMMdd_HHmmss')"

New-Item -ItemType Directory -Path $outputDir -Force | Out-Null

$exePath = "$buildDir\bin\cxvision_imgui_acceptance.exe"
$scriptPath = "$repoDir\cxparser\cxscript\module\cximage\stage25\param_regression\param_self_tuning_findline_direct_v2.cxsc"

$testCases = @(
    @{ Name = "L1_high_contrast_001"; ImagePath = "$testImagesDir\L1_high_contrast\line_high_contrast_001.jpg"; X0 = 82; X1 = 1210; YCenter = 183; YRange = @(150, 165, 180, 195, 210, 225) },
    @{ Name = "L1_high_contrast_002"; ImagePath = "$testImagesDir\L1_high_contrast\line_high_contrast_002.jpg"; X0 = 82; X1 = 1210; YCenter = 180; YRange = @(50, 100, 150, 200, 250, 300, 350, 400) },
    @{ Name = "L2_low_contrast_001"; ImagePath = "$testImagesDir\L2_low_contrast_illumination\line_low_contrast_001.jpg"; X0 = 181; X1 = 244; YCenter = 214; YRange = @(50, 100, 150, 200, 250, 300, 350) },
    @{ Name = "L2_uneven_light_001"; ImagePath = "$testImagesDir\L2_low_contrast_illumination\line_uneven_light_001.jpg"; X0 = 181; X1 = 244; YCenter = 214; YRange = @(50, 100, 150, 200, 250, 300, 350) },
    @{ Name = "L3_complex_boundary_001"; ImagePath = "$testImagesDir\L3_complex_boundary\line_complex_boundary_001.jpg"; X0 = 1440; X1 = 1625; YCenter = 928; YRange = @(500, 600, 700, 800, 900, 1000, 1100, 1200) },
    @{ Name = "L3_near_interference_001"; ImagePath = "$testImagesDir\L3_complex_boundary\line_near_interference_001.jpg"; X0 = 82; X1 = 1210; YCenter = 180; YRange = @(50, 100, 150, 200, 250, 300, 350, 400, 450) }
)

$paramSets = @(
    @{ Name = "base_fmin1"; FilterMin = 1; Threshold = 20; Linegap = 6; Method = 1; FilterProfile = 0 },
    @{ Name = "base_fmin10"; FilterMin = 10; Threshold = 20; Linegap = 6; Method = 1; FilterProfile = 0 },
    @{ Name = "base_fmin20"; FilterMin = 20; Threshold = 20; Linegap = 6; Method = 1; FilterProfile = 1 },
    @{ Name = "low_contrast"; FilterMin = 1; Threshold = 8; Linegap = 6; Method = 1; FilterProfile = 0 },
    @{ Name = "low_contrast_fmin5"; FilterMin = 5; Threshold = 8; Linegap = 6; Method = 1; FilterProfile = 0 },
    @{ Name = "method0"; FilterMin = 1; Threshold = 20; Linegap = 6; Method = 0; FilterProfile = 0 },
    @{ Name = "method2"; FilterMin = 1; Threshold = 20; Linegap = 6; Method = 2; FilterProfile = 0 },
    @{ Name = "wide_linegap"; FilterMin = 1; Threshold = 20; Linegap = 10; Method = 1; FilterProfile = 0 },
    @{ Name = "tight_threshold"; FilterMin = 1; Threshold = 30; Linegap = 6; Method = 1; FilterProfile = 0 },
    @{ Name = "filter_profile1"; FilterMin = 20; Threshold = 20; Linegap = 6; Method = 1; FilterProfile = 1 },
    @{ Name = "filter_profile2"; FilterMin = 1; Threshold = 20; Linegap = 6; Method = 1; FilterProfile = 2 },
    @{ Name = "high_wgap"; FilterMin = 1; Threshold = 20; Linegap = 6; Method = 1; FilterProfile = 0 }
)

$results = @()

foreach ($testCase in $testCases) {
    $caseName = $testCase.Name
    $imagePath = $testCase.ImagePath
    $x0 = $testCase.X0
    $x1 = $testCase.X1
    $yRange = $testCase.YRange
    
    Write-Host "`n=== Processing $caseName ==="
    Write-Host "Image: $imagePath"
    
    foreach ($y in $yRange) {
        foreach ($paramSet in $paramSets) {
            $caseDir = "$outputDir\$caseName\y$y\$($paramSet.Name)"
            New-Item -ItemType Directory -Path $caseDir -Force | Out-Null
            
            $args = @(
                "--headless",
                "--cxscript-headless",
                "--image", "`"$imagePath`"",
                "--script", "`"$scriptPath`"",
                "--case-name", "`"$caseName`"",
                "--out", "`"$caseDir`"",
                "--max-steps", "10000",
                "--global-roi-x0", $x0,
                "--global-roi-y0", $y,
                "--global-roi-x1", $x1,
                "--global-roi-y1", $y,
                "--global-tool-half-width", "30",
                "--global-wgap", "10",
                "--global-hgap", "30",
                "--global-linegap", $paramSet.Linegap,
                "--global-threshold", $paramSet.Threshold,
                "--global-method", $paramSet.Method,
                "--global-filterprofile", $paramSet.FilterProfile,
                "--global-objfilter", "1",
                "--global-filter-min", $paramSet.FilterMin,
                "--global-filter-max", "100000",
                "--global-max-elapsed-ms", "5000",
                "--global-max-scan-lines", "4096",
                "--global-max-samples", "200000"
            )
            
            try {
                $process = Start-Process -FilePath $exePath -ArgumentList $args -Wait -NoNewWindow -PassThru -ErrorAction Stop
                
                $summaryPath = "$caseDir\result_summary.json"
                if (Test-Path $summaryPath) {
                    $summary = Get-Content $summaryPath | ConvertFrom-Json
                    $points = $summary.valid_points_count
                    $fit = $summary.has_fit_line
                    $avgdist = $summary.avgdist
                    $failureStage = $summary.failure_stage
                    
                    $result = [PSCustomObject]@{
                        Name = $caseName
                        Y = $y
                        ParamSet = $paramSet.Name
                        FilterMin = $paramSet.FilterMin
                        Threshold = $paramSet.Threshold
                        Linegap = $paramSet.Linegap
                        Method = $paramSet.Method
                        FilterProfile = $paramSet.FilterProfile
                        Points = $points
                        Fit = $fit
                        AvgDist = $avgdist
                        FailureStage = $failureStage
                    }
                    $results += $result
                    
                    if ($points -gt 0 -and $fit -eq $true) {
                        Write-Host "  SUCCESS: y=$y, params=$($paramSet.Name), points=$points, avgdist=$avgdist"
                    } elseif ($points -gt 0) {
                        Write-Host "  PARTIAL: y=$y, params=$($paramSet.Name), points=$points, fit=$fit"
                    }
                }
            } catch {
                Write-Host "  ERROR: y=$y, params=$($paramSet.Name) - $_"
            }
        }
    }
}

$results | Export-Csv -Path "$outputDir\all_results.csv" -NoTypeInformation

Write-Host "`n=== Summary ==="
Write-Host "Total runs: $($results.Count)"
Write-Host "Runs with points: $($results | Where-Object { $_.Points -gt 0 } | Measure-Object).Count"
Write-Host "Runs with fit: $($results | Where-Object { $_.Fit -eq $true } | Measure-Object).Count"

foreach ($case in $testCases) {
    $caseName = $case.Name
    $caseResults = $results | Where-Object { $_.Name -eq $caseName -and $_.Points -gt 0 -and $_.Fit -eq $true }
    
    if ($caseResults.Count -gt 0) {
        $best = $caseResults | Sort-Object AvgDist | Select-Object -First 1
        Write-Host "`n$caseName Best Result:"
        Write-Host "  Y Position: $($best.Y)"
        Write-Host "  Param Set: $($best.ParamSet)"
        Write-Host "  FilterMin: $($best.FilterMin)"
        Write-Host "  Threshold: $($best.Threshold)"
        Write-Host "  Linegap: $($best.Linegap)"
        Write-Host "  Method: $($best.Method)"
        Write-Host "  FilterProfile: $($best.FilterProfile)"
        Write-Host "  Points: $($best.Points)"
        Write-Host "  AvgDist: $($best.AvgDist)"
        Write-Host "  Output Dir: $outputDir\$caseName\y$($best.Y)\$($best.ParamSet)"
    } else {
        $caseResultsNoFit = $results | Where-Object { $_.Name -eq $caseName -and $_.Points -gt 0 }
        if ($caseResultsNoFit.Count -gt 0) {
            $bestNoFit = $caseResultsNoFit | Sort-Object Points -Descending | Select-Object -First 1
            Write-Host "`n$caseName Partial Result (no fit):"
            Write-Host "  Y Position: $($bestNoFit.Y)"
            Write-Host "  Points: $($bestNoFit.Points)"
        } else {
            Write-Host "`n${caseName}: No successful results"
        }
    }
}