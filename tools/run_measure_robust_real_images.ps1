param(
  [string]$BuildDir = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\build01",
  [string]$RepoRoot = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo",
  [string]$TestImageRoot = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\test_images",
  [string]$RunId = ("run_" + (Get-Date).ToString("yyyyMMdd_HHmmss") + "_measure_robust_real_images")
)

$ErrorActionPreference = "Stop"

Set-Location -LiteralPath $RepoRoot

$binary = Join-Path $BuildDir "Release\cxvision_imgui_acceptance.exe"
if (-not (Test-Path -LiteralPath $binary)) {
  throw "Binary not found: $binary"
}

$runRoot = Join-Path $RepoRoot ("cxscript_runs\headless\" + $RunId)
New-Item -ItemType Directory -Force -Path $runRoot | Out-Null

$testCases = @(
  @{
    Case = "line_hc_001_standard"
    Script = "findline_headless_standard.cxsc"
    ScriptDir = "cxparser\cxscript\module\cximage\frozen\findline"
    Image = "$TestImageRoot\L1_high_contrast\line_high_contrast_001.jpg"
    Tool = "FindLine"
    Params = @{
      roi_x0 = 82; roi_y0 = 183; roi_x1 = 1210; roi_y1 = 183
      wgap = 8; hgap = 32; linegap = 6
      method = 0; threshold = 20; filterprofile = 0
      tool_half_width = 20
      max_elapsed_ms = 15000; max_scan_lines = 4096; max_samples = 200000
    }
  },
  @{
    Case = "line_hc_001_robust"
    Script = "findline_headless_robust.cxsc"
    ScriptDir = "cxparser\cxscript\module\cximage\frozen\findline"
    Image = "$TestImageRoot\L1_high_contrast\line_high_contrast_001.jpg"
    Tool = "FindLine"
    Params = @{
      roi_x0 = 82; roi_y0 = 183; roi_x1 = 1210; roi_y1 = 183
      wgap = 8; hgap = 32; linegap = 6
      method = 0; threshold = 20; filterprofile = 0
      tool_half_width = 20
      max_elapsed_ms = 15000; max_scan_lines = 4096; max_samples = 200000
    }
  },
  @{
    Case = "line_hc_002_standard"
    Script = "findline_headless_standard.cxsc"
    ScriptDir = "cxparser\cxscript\module\cximage\frozen\findline"
    Image = "$TestImageRoot\L1_high_contrast\line_high_contrast_002.jpg"
    Tool = "FindLine"
    Params = @{
      roi_x0 = 687; roi_y0 = 760; roi_x1 = 799; roi_y1 = 680
      wgap = 8; hgap = 28; linegap = 6
      method = 2; threshold = 20; filterprofile = 0
      tool_half_width = 20
      max_elapsed_ms = 15000; max_scan_lines = 4096; max_samples = 200000
    }
  },
  @{
    Case = "line_hc_002_robust"
    Script = "findline_headless_robust.cxsc"
    ScriptDir = "cxparser\cxscript\module\cximage\frozen\findline"
    Image = "$TestImageRoot\L1_high_contrast\line_high_contrast_002.jpg"
    Tool = "FindLine"
    Params = @{
      roi_x0 = 687; roi_y0 = 760; roi_x1 = 799; roi_y1 = 680
      wgap = 8; hgap = 28; linegap = 6
      method = 2; threshold = 20; filterprofile = 0
      tool_half_width = 20
      max_elapsed_ms = 15000; max_scan_lines = 4096; max_samples = 200000
    }
  },
  @{
    Case = "line_lc_001_standard"
    Script = "findline_headless_standard.cxsc"
    ScriptDir = "cxparser\cxscript\module\cximage\frozen\findline"
    Image = "$TestImageRoot\L2_low_contrast_illumination\line_low_contrast_001.jpg"
    Tool = "FindLine"
    Params = @{
      roi_x0 = 181; roi_y0 = 20; roi_x1 = 244; roi_y1 = 407
      wgap = 8; hgap = 24; linegap = 6
      method = 1; threshold = 15; filterprofile = 0
      tool_half_width = 20
      max_elapsed_ms = 15000; max_scan_lines = 4096; max_samples = 200000
    }
  },
  @{
    Case = "line_lc_001_robust"
    Script = "findline_headless_robust.cxsc"
    ScriptDir = "cxparser\cxscript\module\cximage\frozen\findline"
    Image = "$TestImageRoot\L2_low_contrast_illumination\line_low_contrast_001.jpg"
    Tool = "FindLine"
    Params = @{
      roi_x0 = 181; roi_y0 = 20; roi_x1 = 244; roi_y1 = 407
      wgap = 8; hgap = 24; linegap = 6
      method = 1; threshold = 15; filterprofile = 0
      tool_half_width = 20
      max_elapsed_ms = 15000; max_scan_lines = 4096; max_samples = 200000
    }
  },
  @{
    Case = "line_near_intf_standard"
    Script = "findline_headless_standard.cxsc"
    ScriptDir = "cxparser\cxscript\module\cximage\frozen\findline"
    Image = "$TestImageRoot\L3_complex_boundary\line_near_interference_001.jpg"
    Tool = "FindLine"
    Params = @{
      roi_x0 = 330; roi_y0 = 121; roi_x1 = 460; roi_y1 = 121
      wgap = 10; hgap = 34; linegap = 9
      method = 1; threshold = 20; filterprofile = 0
      tool_half_width = 20
      max_elapsed_ms = 15000; max_scan_lines = 4096; max_samples = 200000
    }
  },
  @{
    Case = "line_near_intf_robust"
    Script = "findline_headless_robust.cxsc"
    ScriptDir = "cxparser\cxscript\module\cximage\frozen\findline"
    Image = "$TestImageRoot\L3_complex_boundary\line_near_interference_001.jpg"
    Tool = "FindLine"
    Params = @{
      roi_x0 = 330; roi_y0 = 121; roi_x1 = 460; roi_y1 = 121
      wgap = 10; hgap = 34; linegap = 9
      method = 1; threshold = 20; filterprofile = 0
      tool_half_width = 20
      max_elapsed_ms = 15000; max_scan_lines = 4096; max_samples = 200000
    }
  },
  @{
    Case = "circle_hc_001_robust"
    Script = "findcircle_headless_robust.cxsc"
    ScriptDir = "cxparser\cxscript\module\cximage\frozen\findcircle"
    Image = "$TestImageRoot\L1_high_contrast\circle_high_contrast_001.jpg"
    Tool = "FindCircle"
    Params = @{
      circle_cx = 498; circle_cy = 704; circle_px = 563; circle_py = 704
      gap = 5; linegap = 3
      method = 0; threshold = 20
      max_elapsed_ms = 15000; max_scan_lines = 4096; max_samples = 200000
    }
  },
  @{
    Case = "circle_hc_002_robust"
    Script = "findcircle_headless_robust.cxsc"
    ScriptDir = "cxparser\cxscript\module\cximage\frozen\findcircle"
    Image = "$TestImageRoot\L1_high_contrast\circle_high_contrast_002.jpg"
    Tool = "FindCircle"
    Params = @{
      circle_cx = 288; circle_cy = 261; circle_px = 420; circle_py = 261
      gap = 5; linegap = 3
      method = 0; threshold = 20
      max_elapsed_ms = 15000; max_scan_lines = 4096; max_samples = 200000
    }
  },
  @{
    Case = "circle_lc_001_robust"
    Script = "findcircle_headless_robust.cxsc"
    ScriptDir = "cxparser\cxscript\module\cximage\frozen\findcircle"
    Image = "$TestImageRoot\L2_low_contrast_illumination\circle_low_contrast_001.jpg"
    Tool = "FindCircle"
    Params = @{
      circle_cx = 635; circle_cy = 516; circle_px = 932; circle_py = 516
      gap = 6; linegap = 3
      method = 0; threshold = 20
      max_elapsed_ms = 15000; max_scan_lines = 4096; max_samples = 200000
    }
  }
)

$rows = New-Object System.Collections.Generic.List[object]

foreach ($case in $testCases) {
  $imgPath = $case.Image
  if (-not (Test-Path -LiteralPath $imgPath)) {
    Write-Output "[SKIP] Image not found: $imgPath"
    continue
  }

  $caseOut = Join-Path $runRoot $case.Case
  New-Item -ItemType Directory -Force -Path $caseOut | Out-Null
  $scriptPath = Join-Path $RepoRoot (Join-Path $case.ScriptDir $case.Script)

  if (-not (Test-Path -LiteralPath $scriptPath)) {
    Write-Output "[SKIP] Script not found: $scriptPath"
    continue
  }

  $p = $case.Params

  $args = @(
    "--headless",
    "--cxscript-headless",
    "--image", $imgPath,
    "--script", $scriptPath,
    "--case-name", $case.Case,
    "--out", $caseOut,
    "--max-steps", "10000",
    "--timeout-sec", "15"
  )

  switch ($case.Tool) {
    "FindLine" {
      $args += @(
        "--roi-x0", "$($p.roi_x0)",
        "--roi-y0", "$($p.roi_y0)",
        "--roi-x1", "$($p.roi_x1)",
        "--roi-y1", "$($p.roi_y1)",
        "--method", "$($p.method)",
        "--threshold", "$($p.threshold)",
        "--filterprofile", "$($p.filterprofile)",
        "--wgap", "$($p.wgap)",
        "--hgap", "$($p.hgap)",
        "--linegap", "$($p.linegap)",
        "--tool-half-width", "$($p.tool_half_width)",
        "--max-elapsed-ms", "$($p.max_elapsed_ms)",
        "--max-scan-lines", "$($p.max_scan_lines)",
        "--max-samples", "$($p.max_samples)"
      )
    }
    "FindCircle" {
      $args += @(
        "--circle-cx", "$($p.circle_cx)",
        "--circle-cy", "$($p.circle_cy)",
        "--circle-px", "$($p.circle_px)",
        "--circle-py", "$($p.circle_py)",
        "--method", "$($p.method)",
        "--gap", "$($p.gap)",
        "--threshold", "$($p.threshold)",
        "--linegap", "$($p.linegap)",
        "--max-elapsed-ms", "$($p.max_elapsed_ms)",
        "--max-scan-lines", "$($p.max_scan_lines)",
        "--max-samples", "$($p.max_samples)"
      )
    }
    "FindEllipse" {
      $args += @(
        "--roi-x0", "$($p.roi_x0)",
        "--roi-y0", "$($p.roi_y0)",
        "--roi-x1", "$($p.roi_x1)",
        "--roi-y1", "$($p.roi_y1)",
        "--method", "$($p.method)",
        "--gap", "$($p.gap)",
        "--threshold", "$($p.threshold)",
        "--linegap", "$($p.linegap)",
        "--max-elapsed-ms", "$($p.max_elapsed_ms)",
        "--max-scan-lines", "$($p.max_scan_lines)",
        "--max-samples", "$($p.max_samples)"
      )
    }
  }

  $started = Get-Date
  Write-Output "[RUN] $($case.Case) ..."
  & $binary @args 2>&1 | Tee-Object -FilePath (Join-Path $caseOut "stdout.txt")
  $exitCode = $LASTEXITCODE
  $ended = Get-Date

  $summaryPath = Join-Path $caseOut "result_summary.json"
  $snapshotPath = Join-Path $caseOut "snapshot.txt"

  $summaryDoc = $null
  if (Test-Path -LiteralPath $summaryPath) {
    $summaryDoc = Get-Content -LiteralPath $summaryPath -Raw | ConvertFrom-Json
  }

  $elapsedMs = $null
  $failureStage = $null
  $validPointsCount = 0
  $hasFitLine = $false
  $hasFitCircle = $false
  $fitLineErr = 0
  $fitCircleErr = 0
  
  if (Test-Path -LiteralPath $snapshotPath) {
    $content = Get-Content -LiteralPath $snapshotPath -Raw
    if ($content -match 'elapsed_ms:\s*(\d+)') {
      $elapsedMs = [int]$Matches[1]
    }
    if ($content -match 'failure_stage:\s*(\S+)') {
      $failureStage = $Matches[1]
    }
  }

  if ($summaryDoc -ne $null) {
    $validPointsCount = $summaryDoc.metrics.valid_points_count
    $hasFitLine = ($summaryDoc.facts.has_fit_line -eq "true")
    $hasFitCircle = ($summaryDoc.facts.has_fit_circle -eq "true")
    if ($hasFitLine) {
      $fitLineErr = $summaryDoc.metrics.avgdist
    }
    if ($hasFitCircle) {
      $fitCircleErr = $summaryDoc.metrics.circle_radius
    }
  }

  $rows.Add([pscustomobject]@{
    tool = $case.Tool
    case_name = $case.Case
    image = [System.IO.Path]::GetFileName($imgPath)
    exit_code = $exitCode
    wall_ms = [int](($ended - $started).TotalMilliseconds)
    elapsed_ms = $elapsedMs
    failure_stage = $failureStage
    valid_points = $validPointsCount
    has_fit_line = $hasFitLine
    has_fit_circle = $hasFitCircle
    fit_line_err = $fitLineErr
    fit_circle_radius = $fitCircleErr
    output_dir = $caseOut
  })

  Write-Output "[DONE] $($case.Case) exit=$exitCode elapsed=${elapsedMs}ms points=$validPointsCount fit_line=$hasFitLine fit_circle=$hasFitCircle failure=$failureStage"
}

$csvPath = Join-Path $runRoot "real_image_compare.csv"
$jsonPath = Join-Path $runRoot "real_image_compare.json"
$mdPath = Join-Path $runRoot "real_image_compare.md"

$rows | Export-Csv -LiteralPath $csvPath -NoTypeInformation -Encoding UTF8
$rows | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $jsonPath -Encoding UTF8

$md = New-Object System.Text.StringBuilder
[void]$md.AppendLine("# Real Image Measure vs MeasureRobust Comparison")
[void]$md.AppendLine("")
[void]$md.AppendLine("- repo_root: $RepoRoot")
[void]$md.AppendLine("- build_dir: $BuildDir")
[void]$md.AppendLine("- binary: $binary")
[void]$md.AppendLine("- run_id: $RunId")
[void]$md.AppendLine("- test_images: $TestImageRoot")
[void]$md.AppendLine("")
[void]$md.AppendLine("| Tool | Case | Image | Exit | Wall ms | Elapsed ms | Points | Fit Line | Fit Circle | Error/Dist | Failure Stage |")
[void]$md.AppendLine("|---|---|---|---:|---:|---:|---:|---|---|---:|---|")
foreach ($row in $rows) {
  $errInfo = if ($row.has_fit_line) { "$($row.fit_line_err)" } elseif ($row.has_fit_circle) { "$($row.fit_circle_radius)" } else { "-" }
  [void]$md.AppendLine("| $($row.tool) | $($row.case_name) | $($row.image) | $($row.exit_code) | $($row.wall_ms) | $($row.elapsed_ms) | $($row.valid_points) | $($row.has_fit_line) | $($row.has_fit_circle) | $errInfo | $($row.failure_stage) |")
}
[System.IO.File]::WriteAllText($mdPath, $md.ToString(), [System.Text.UTF8Encoding]::new($false))

Write-Output ""
Write-Output "[RUN_ROOT] $runRoot"
Write-Output "[CSV] $csvPath"
Write-Output "[JSON] $jsonPath"
Write-Output "[REPORT] $mdPath"
