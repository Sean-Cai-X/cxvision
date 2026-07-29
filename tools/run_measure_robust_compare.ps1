param(
  [string]$BuildDir = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\build01",
  [string]$RepoRoot = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo",
  [string]$RunId = ("run_" + (Get-Date).ToString("yyyyMMdd_HHmmss") + "_measure_robust_compare"),
  [int]$Repeat = 1
)

$ErrorActionPreference = "Stop"

Set-Location -LiteralPath $RepoRoot

$binary = Join-Path $BuildDir "Release\cxvision_imgui_acceptance.exe"
if (-not (Test-Path -LiteralPath $binary)) {
  throw "Binary not found: $binary"
}

$runRoot = Join-Path $RepoRoot ("cxscript_runs\headless\" + $RunId)
$assetRoot = Join-Path $runRoot "assets"
New-Item -ItemType Directory -Force -Path $assetRoot | Out-Null

Add-Type -AssemblyName System.Drawing

$imagePath = Join-Path $assetRoot "line_test_chamfered.png"
$bitmap = New-Object System.Drawing.Bitmap 400, 200
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$graphics.Clear([System.Drawing.Color]::Black)
$whiteBrush = [System.Drawing.Brushes]::White

$graphics.FillRectangle($whiteBrush, 50, 80, 300, 6)
$graphics.FillRectangle($whiteBrush, 100, 40, 6, 120)
$graphics.FillRectangle($whiteBrush, 250, 40, 6, 120)

$graphics.Dispose()
$bitmap.Save($imagePath, [System.Drawing.Imaging.ImageFormat]::Png)
$bitmap.Dispose()

$testCases = @(
  @{
    Case = "findline_standard"
    Script = "findline_headless_standard.cxsc"
    ScriptDir = "cxparser\cxscript\module\cximage\frozen\findline"
    Tool = "FindLine"
  },
  @{
    Case = "findline_robust"
    Script = "findline_headless_robust.cxsc"
    ScriptDir = "cxparser\cxscript\module\cximage\frozen\findline"
    Tool = "FindLine"
  },
  @{
    Case = "findcircle_robust"
    Script = "findcircle_headless_robust.cxsc"
    ScriptDir = "cxparser\cxscript\module\cximage\frozen\findcircle"
    Tool = "FindCircle"
  },
  @{
    Case = "findellipse_robust"
    Script = "findellipse_headless_robust.cxsc"
    ScriptDir = "cxparser\cxscript\module\cximage\headless"
    Tool = "FindEllipse"
  }
)

$rows = New-Object System.Collections.Generic.List[object]

foreach ($case in $testCases) {
  for ($i = 1; $i -le $Repeat; $i++) {
    $caseOut = Join-Path $runRoot ($case.Case)
    New-Item -ItemType Directory -Force -Path $caseOut | Out-Null
    $scriptPath = Join-Path $RepoRoot (Join-Path $case.ScriptDir $case.Script)

    if (-not (Test-Path -LiteralPath $scriptPath)) {
      Write-Output "[SKIP] Script not found: $scriptPath"
      continue
    }

    $args = @(
      "--headless",
      "--cxscript-headless",
      "--image", $imagePath,
      "--script", $scriptPath,
      "--case-name", ($case.Case + "_r" + $i),
      "--out", $caseOut,
      "--max-steps", "10000",
      "--timeout-sec", "15",
      "--roi-x0", "40",
      "--roi-y0", "30",
      "--roi-x1", "360",
      "--roi-y1", "170",
      "--method", "1",
      "--gap", "20",
      "--filterprofile", "1",
      "--threshold", "30",
      "--wgap", "3",
      "--hgap", "3",
      "--max-elapsed-ms", "15000",
      "--max-scan-lines", "4096",
      "--max-samples", "200000",
      "--circle-cx", "200",
      "--circle-cy", "100",
      "--circle-px", "150",
      "--circle-py", "100"
    )

    $started = Get-Date
    & $binary @args | Tee-Object -FilePath (Join-Path $caseOut "stdout.txt")
    $exitCode = $LASTEXITCODE
    $ended = Get-Date

    $summaryPath = Join-Path $caseOut "result_summary.json"
    $snapshotPath = Join-Path $caseOut "snapshot.txt"
    $objectStatePath = Join-Path $caseOut "object_state.json"

    $summaryDoc = $null
    if (Test-Path -LiteralPath $summaryPath) {
      $summaryDoc = Get-Content -LiteralPath $summaryPath -Raw | ConvertFrom-Json
    }

    $elapsedMs = $null
    if (Test-Path -LiteralPath $snapshotPath) {
      foreach ($line in Get-Content -LiteralPath $snapshotPath) {
        if ($line -match '^elapsed_ms:\s*(\d+)') {
          $elapsedMs = [int]$Matches[1]
          break
        }
      }
    }

    $validPoints = $null
    $hasFitLine = $null
    $hasFitCircle = $null
    $candidateCount = $null

    if ($summaryDoc -and $summaryDoc.runtime_globals) {
      $validPoints = $summaryDoc.runtime_globals.global_valid_points_count
      $hasFitLine = $summaryDoc.runtime_globals.global_has_fit_line
      $hasFitCircle = $summaryDoc.runtime_globals.global_has_fit_circle
      $candidateCount = $summaryDoc.runtime_globals.global_candidate_count
    }

    $rows.Add([pscustomobject]@{
      tool = $case.Tool
      case_name = $case.Case
      repeat = $i
      exit_code = $exitCode
      wall_ms = [int](($ended - $started).TotalMilliseconds)
      elapsed_ms = $elapsedMs
      valid_points = $validPoints
      has_fit_line = $hasFitLine
      has_fit_circle = $hasFitCircle
      candidate_count = $candidateCount
      output_dir = $caseOut
    })
  }
}

$csvPath = Join-Path $runRoot "measure_robust_compare.csv"
$jsonPath = Join-Path $runRoot "measure_robust_compare.json"
$mdPath = Join-Path $runRoot "measure_robust_compare.md"

$rows | Export-Csv -LiteralPath $csvPath -NoTypeInformation -Encoding UTF8
$rows | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $jsonPath -Encoding UTF8

$md = New-Object System.Text.StringBuilder
[void]$md.AppendLine("# Measure vs MeasureRobust Comparison")
[void]$md.AppendLine("")
[void]$md.AppendLine("- repo_root: $RepoRoot")
[void]$md.AppendLine("- build_dir: $BuildDir")
[void]$md.AppendLine("- binary: $binary")
[void]$md.AppendLine("- run_id: $RunId")
[void]$md.AppendLine("- image: $imagePath")
[void]$md.AppendLine("")
[void]$md.AppendLine("| Tool | Case | Exit | Wall ms | Elapsed ms | Valid Points | Has Fit Line | Has Fit Circle | Candidates |")
[void]$md.AppendLine("|---|---|---|---:|---:|---:|---|---|---:|")
foreach ($row in $rows) {
  [void]$md.AppendLine("| $($row.tool) | $($row.case_name) | $($row.exit_code) | $($row.wall_ms) | $($row.elapsed_ms) | $($row.valid_points) | $($row.has_fit_line) | $($row.has_fit_circle) | $($row.candidate_count) |")
}
[System.IO.File]::WriteAllText($mdPath, $md.ToString(), [System.Text.UTF8Encoding]::new($false))

Write-Output "[RUN_ROOT] $runRoot"
Write-Output "[CSV] $csvPath"
Write-Output "[JSON] $jsonPath"
Write-Output "[REPORT] $mdPath"
