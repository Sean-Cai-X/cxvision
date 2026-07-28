param(
  [Parameter(Mandatory = $true)]
  [string]$BuildDir,

  [string]$RepoRoot = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo",
  [string]$RunId = ("run_" + (Get-Date).ToString("yyyyMMdd_HHmmss") + "_findobject_three_stage"),
  [int]$Repeat = 3
)

$ErrorActionPreference = "Stop"

function Read-ElapsedMs {
  param([string]$SnapshotPath)
  if (-not (Test-Path -LiteralPath $SnapshotPath)) {
    return $null
  }
  foreach ($line in Get-Content -LiteralPath $SnapshotPath) {
    if ($line -match '^elapsed_ms:\s*(\d+)') {
      return [int]$Matches[1]
    }
  }
  return $null
}

function Add-Rect {
  param(
    [System.Drawing.Graphics]$Graphics,
    [System.Drawing.Pen]$Pen,
    [int]$X,
    [int]$Y,
    [int]$W,
    [int]$H
  )
  $Graphics.DrawRectangle($Pen, $X, $Y, $W, $H)
}

Set-Location -LiteralPath $RepoRoot

$binary = Join-Path $BuildDir "Release\cxvision_imgui_acceptance.exe"
if (-not (Test-Path -LiteralPath $binary)) {
  throw "Binary not found: $binary"
}

$runRoot = Join-Path $RepoRoot ("cxscript_runs\headless\" + $RunId)
$assetRoot = Join-Path $runRoot "assets"
New-Item -ItemType Directory -Force -Path $assetRoot | Out-Null

Add-Type -AssemblyName System.Drawing
$imagePath = Join-Path $assetRoot "findobject_binary_regions.png"
$bitmap = New-Object System.Drawing.Bitmap 220, 160
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$graphics.Clear([System.Drawing.Color]::Black)
$whiteBrush = [System.Drawing.Brushes]::White
$graphics.FillRectangle($whiteBrush, 30, 25, 42, 34)
$graphics.FillRectangle($whiteBrush, 115, 28, 55, 43)
$graphics.FillRectangle($whiteBrush, 50, 98, 94, 34)
$graphics.Dispose()
$bitmap.Save($imagePath, [System.Drawing.Imaging.ImageFormat]::Png)
$bitmap.Dispose()

$scriptRoot = Join-Path $RepoRoot "cxparser\cxscript\module\cximage\diagnostic\findobject"
$cases = @(
  @{ Case = "measure_baseline"; Script = "findobject_measure_baseline_compare.cxsc"; Group = "Measure"; Branch = "baseline_original_bfs" },
  @{ Case = "measure_fast"; Script = "findobject_measure_fast_compare.cxsc"; Group = "Measure"; Branch = "phase1_bfs_hotpath_optimized" },
  @{ Case = "measure_connected_components"; Script = "findobject_measure_connected_components_compare.cxsc"; Group = "Measure"; Branch = "phase3_opencv_connected_components" },
  @{ Case = "measure_peak_local_bfs"; Script = "findobject_measure_peak_local_bfs_compare.cxsc"; Group = "Measure"; Branch = "phase4_peak_local_bfs" },
  @{ Case = "measurex_baseline"; Script = "findobject_measurex_baseline_compare.cxsc"; Group = "MeasureX"; Branch = "baseline_original_gap_bfs" },
  @{ Case = "measurex_fast"; Script = "findobject_measurex_fast_compare.cxsc"; Group = "MeasureX"; Branch = "phase1_gap_bfs_hotpath_optimized" },
  @{ Case = "measurex_connected_components"; Script = "findobject_measurex_connected_components_compare.cxsc"; Group = "MeasureX"; Branch = "phase3_opencv_connected_components_reference" },
  @{ Case = "measurex_peak_local_bfs"; Script = "findobject_measurex_peak_local_bfs_compare.cxsc"; Group = "MeasureX"; Branch = "phase4_peak_local_bfs" }
)

$rows = New-Object System.Collections.Generic.List[object]

foreach ($case in $cases) {
  for ($i = 1; $i -le $Repeat; $i++) {
    $caseOut = Join-Path $runRoot ($case.Case + "\r" + $i)
    New-Item -ItemType Directory -Force -Path $caseOut | Out-Null
    $scriptPath = Join-Path $scriptRoot $case.Script
    $args = @(
      "--headless",
      "--cxscript-headless",
      "--image", $imagePath,
      "--script", $scriptPath,
      "--case-name", ($case.Case + "_r" + $i),
      "--out", $caseOut,
      "--max-steps", "10000",
      "--timeout-sec", "10",
      "--roi-x0", "0",
      "--roi-y0", "0",
      "--roi-x1", "220",
      "--roi-y1", "160",
      "--method", "1",
      "--gap", "8",
      "--filterprofile", "1",
      "--threshold", "5",
      "--wgap", "20",
      "--hgap", "20",
      "--max-elapsed-ms", "10000",
      "--max-scan-lines", "4096",
      "--max-samples", "200000"
    )

    $started = Get-Date
    & $binary @args | Tee-Object -FilePath (Join-Path $caseOut "stdout.txt")
    $exitCode = $LASTEXITCODE
    $ended = Get-Date

    $objectStatePath = Join-Path $caseOut "object_state.json"
    $snapshotPath = Join-Path $caseOut "snapshot.txt"
    $summaryPath = Join-Path $caseOut "result_summary.json"
    $objectState = $null
    if (Test-Path -LiteralPath $objectStatePath) {
      $objectState = Get-Content -LiteralPath $objectStatePath -Raw | ConvertFrom-Json
    }
    $summaryDoc = $null
    if (Test-Path -LiteralPath $summaryPath) {
      $summaryDoc = Get-Content -LiteralPath $summaryPath -Raw | ConvertFrom-Json
    }

    $rows.Add([pscustomobject]@{
      group = $case.Group
      branch = $case.Branch
      case_name = $case.Case
      repeat = $i
      exit_code = $exitCode
      wall_ms = [int](($ended - $started).TotalMilliseconds)
      elapsed_ms = Read-ElapsedMs $snapshotPath
      result_rect_count = if ($objectState) { $objectState.result_rect_count } else { $null }
      has_result_rect = if ($objectState) { $objectState.has_result_rect } else { $null }
      component_count = if ($summaryDoc) { $summaryDoc.runtime_globals.global_candidate_count } else { $null }
      accepted_count = if ($summaryDoc) { $summaryDoc.runtime_globals.global_rendered_result_count } else { $null }
      rejected_count = if ($summaryDoc) { $summaryDoc.runtime_globals.global_valid_points_count } else { $null }
      max_component_area = if ($summaryDoc) { $summaryDoc.runtime_globals.global_debug_max_component_area } else { $null }
      max_component_w = if ($summaryDoc) { $summaryDoc.runtime_globals.global_debug_max_component_w } else { $null }
      max_component_h = if ($summaryDoc) { $summaryDoc.runtime_globals.global_debug_max_component_h } else { $null }
      top1_rect = if ($objectState) {
        ("{0},{1},{2},{3}" -f $objectState.top1_rect_x, $objectState.top1_rect_y, $objectState.top1_rect_w, $objectState.top1_rect_h)
      } else {
        "not_exported"
      }
      result_summary_path = if (Test-Path -LiteralPath $summaryPath) { $summaryPath } else { "" }
      object_state_path = if (Test-Path -LiteralPath $objectStatePath) { $objectStatePath } else { "" }
      output_dir = $caseOut
    })
  }
}

$csvPath = Join-Path $runRoot "findobject_three_stage_efficiency_compare.csv"
$jsonPath = Join-Path $runRoot "findobject_three_stage_efficiency_compare.json"
$mdPath = Join-Path $runRoot "findobject_three_stage_efficiency_compare.md"

$rows | Export-Csv -LiteralPath $csvPath -NoTypeInformation -Encoding UTF8
$rows | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $jsonPath -Encoding UTF8

$summary = $rows |
  Group-Object group, branch |
  ForEach-Object {
    $items = $_.Group
    $first = $items[0]
    $elapsed = @($items | Where-Object { $null -ne $_.elapsed_ms } | ForEach-Object { [double]$_.elapsed_ms })
    [pscustomobject]@{
      group = $first.group
      branch = $first.branch
      runs = $items.Count
      exit_codes = (($items | Select-Object -ExpandProperty exit_code -Unique) -join ",")
      avg_elapsed_ms = if ($elapsed.Count -gt 0) { [math]::Round(($elapsed | Measure-Object -Average).Average, 3) } else { $null }
      min_elapsed_ms = if ($elapsed.Count -gt 0) { ($elapsed | Measure-Object -Minimum).Minimum } else { $null }
      max_elapsed_ms = if ($elapsed.Count -gt 0) { ($elapsed | Measure-Object -Maximum).Maximum } else { $null }
      result_rect_counts = (($items | Select-Object -ExpandProperty result_rect_count -Unique) -join ",")
      component_counts = (($items | Select-Object -ExpandProperty component_count -Unique) -join ",")
      accepted_counts = (($items | Select-Object -ExpandProperty accepted_count -Unique) -join ",")
      rejected_counts = (($items | Select-Object -ExpandProperty rejected_count -Unique) -join ",")
      max_component_area = (($items | Select-Object -ExpandProperty max_component_area -Unique) -join ",")
      max_component_wh = (($items | ForEach-Object { "{0}x{1}" -f $_.max_component_w, $_.max_component_h } | Select-Object -Unique) -join ",")
      top1_rect = (($items | Select-Object -ExpandProperty top1_rect -Unique) -join ";")
    }
  }

$baselineByGroup = @{}
foreach ($item in $summary) {
  if ($item.branch -like "baseline_*") {
    $baselineByGroup[$item.group] = $item.avg_elapsed_ms
  }
}

$md = New-Object System.Text.StringBuilder
[void]$md.AppendLine("# FindObject Three-Stage Headless Efficiency Compare")
[void]$md.AppendLine("")
[void]$md.AppendLine("- repo_root: $RepoRoot")
[void]$md.AppendLine("- build_dir: $BuildDir")
[void]$md.AppendLine("- binary: $binary")
[void]$md.AppendLine("- run_id: $RunId")
[void]$md.AppendLine("- image: $imagePath")
[void]$md.AppendLine("- repeat: $Repeat")
[void]$md.AppendLine("")
[void]$md.AppendLine("| Group | Branch | Runs | Exit Codes | Avg elapsed ms | Min | Max | Speed vs baseline | Components | Accepted | Rejected | Max component area | Max component WH | Result rect counts | Top1 rect |")
[void]$md.AppendLine("|---|---|---:|---|---:|---:|---:|---:|---|---|---|---|---|---|---|")
foreach ($item in $summary) {
  $base = $baselineByGroup[$item.group]
  $speed = ""
  if ($null -ne $base -and $null -ne $item.avg_elapsed_ms -and $item.avg_elapsed_ms -gt 0) {
    $speed = [math]::Round($base / $item.avg_elapsed_ms, 3)
  }
  [void]$md.AppendLine("| $($item.group) | $($item.branch) | $($item.runs) | $($item.exit_codes) | $($item.avg_elapsed_ms) | $($item.min_elapsed_ms) | $($item.max_elapsed_ms) | $speed | $($item.component_counts) | $($item.accepted_counts) | $($item.rejected_counts) | $($item.max_component_area) | $($item.max_component_wh) | $($item.result_rect_counts) | $($item.top1_rect) |")
}
[void]$md.AppendLine("")
[void]$md.AppendLine("## Policy")
[void]$md.AppendLine("")
[void]$md.AppendLine("- Speed is only meaningful when geometry fields match.")
[void]$md.AppendLine("- Components/Accepted/Rejected are script-level FindObject diagnostics for branch comparison.")
[void]$md.AppendLine("- `top1_rect=not_exported` means the current headless capture did not expose the field; inspect overlays/object state before claiming geometry equivalence.")
[void]$md.AppendLine("- `MeasureXConnectedComponents` is a reference branch only for binary region statistics and does not reproduce gap-growth semantics.")
[System.IO.File]::WriteAllText($mdPath, $md.ToString(), [System.Text.UTF8Encoding]::new($false))

Write-Output "[RUN_ROOT] $runRoot"
Write-Output "[CSV] $csvPath"
Write-Output "[JSON] $jsonPath"
Write-Output "[REPORT] $mdPath"
