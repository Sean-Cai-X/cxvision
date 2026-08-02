param(
    [string]$RepoRoot = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo",
    [string]$Binary = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\build01\Release\cxvision_imgui_acceptance.exe",
    [string]$RunRoot = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxscript_runs",
    [string]$SourceCasesRoot = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxscript_runs\suite\run_20260730_findline_findcircle_algorithm_boundary_v13\cases",
    [string]$ManualSeedSummary = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxscript_runs\evidence_chain\manual_candidate_inference_20260802\manual_candidate_inference_summary.json",
    [string]$SharedLog = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxscript_runs\_shared\cxvision_imgui_acceptance.jsonl",
    [string]$OutDir = "D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxscript_runs\evidence_chain\experience_propagation_20260802",
    [int]$MaxCaseSeconds = 10,
    [int]$MaxCases = 0,
    [switch]$DryRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function New-Dir([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) {
        New-Item -ItemType Directory -Force -Path $Path | Out-Null
    }
}

function Read-JsonFile([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "missing json file: $Path"
    }
    return (Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json)
}

function Get-PropValue($Obj, [string]$Name, $DefaultValue) {
    if ($null -eq $Obj) {
        return $DefaultValue
    }
    $prop = $Obj.PSObject.Properties[$Name]
    if ($null -eq $prop) {
        return $DefaultValue
    }
    if ($null -eq $prop.Value) {
        return $DefaultValue
    }
    return $prop.Value
}

function As-Int($Value, [int]$DefaultValue = 0) {
    if ($null -eq $Value -or "$Value" -eq "") {
        return $DefaultValue
    }
    return [int][double]$Value
}

function As-Double($Value, [double]$DefaultValue = 0.0) {
    if ($null -eq $Value -or "$Value" -eq "") {
        return $DefaultValue
    }
    return [double]$Value
}

function As-Bool($Value) {
    if ($Value -is [bool]) {
        return [bool]$Value
    }
    $text = "$Value".Trim().ToLowerInvariant()
    return ($text -eq "true" -or $text -eq "1" -or $text -eq "yes")
}

function Get-SummaryValue($Summary, [string]$Name, $DefaultValue) {
    $direct = Get-PropValue $Summary $Name $null
    if ($null -ne $direct) {
        return $direct
    }
    $facts = Get-PropValue $Summary "facts" $null
    $fromFacts = Get-PropValue $facts $Name $null
    if ($null -ne $fromFacts) {
        return $fromFacts
    }
    $metrics = Get-PropValue $Summary "metrics" $null
    $fromMetrics = Get-PropValue $metrics $Name $null
    if ($null -ne $fromMetrics) {
        return $fromMetrics
    }
    return $DefaultValue
}

function Sanitize-Id([string]$Text) {
    $safe = $Text -replace '[^A-Za-z0-9_\-]+', '_'
    $safe = $safe.Trim('_')
    if ($safe.Length -gt 96) {
        $safe = $safe.Substring(0, 96)
    }
    return $safe
}

function Pick-LineScript($Replay, $Summary) {
    $x0 = As-Int (Get-PropValue $Summary "roi_x0" (Get-PropValue $Replay.target "roi_x0" 0))
    $y0 = As-Int (Get-PropValue $Summary "roi_y0" (Get-PropValue $Replay.target "roi_y0" 0))
    $x1 = As-Int (Get-PropValue $Summary "roi_x1" (Get-PropValue $Replay.target "roi_x1" 0))
    $y1 = As-Int (Get-PropValue $Summary "roi_y1" (Get-PropValue $Replay.target "roi_y1" 0))
    if ([Math]::Abs($x1 - $x0) -ge [Math]::Abs($y1 - $y0)) {
        return "cxparser/cxscript/module/cximage/frozen/findline/findline_horizontal_stage25_filter20_ok.cxsc"
    }
    return "cxparser/cxscript/module/cximage/frozen/findline/findline_vertical_stage25_filter20_ok.cxsc"
}

function New-GlobalMap($Replay, $Summary) {
    $g = [ordered]@{}
    $g["global_roi_x0"] = As-Int (Get-PropValue $Summary "roi_x0" (Get-PropValue $Replay.target "roi_x0" 0))
    $g["global_roi_y0"] = As-Int (Get-PropValue $Summary "roi_y0" (Get-PropValue $Replay.target "roi_y0" 0))
    $g["global_roi_x1"] = As-Int (Get-PropValue $Summary "roi_x1" (Get-PropValue $Replay.target "roi_x1" 0))
    $g["global_roi_y1"] = As-Int (Get-PropValue $Summary "roi_y1" (Get-PropValue $Replay.target "roi_y1" 0))
    $g["global_tool_half_width"] = As-Int (Get-PropValue $Summary "effective_tool_half_width" (Get-PropValue $Replay.target "tool_half_width" 20))
    $g["global_circle_cx"] = As-Int (Get-PropValue $Summary "circle_cx" (Get-PropValue $Replay.target "circle_cx" 0))
    $g["global_circle_cy"] = As-Int (Get-PropValue $Summary "circle_cy" (Get-PropValue $Replay.target "circle_cy" 0))
    $g["global_circle_px"] = As-Int (Get-PropValue $Summary "circle_px" (Get-PropValue $Replay.target "circle_px" 0))
    $g["global_circle_py"] = As-Int (Get-PropValue $Summary "circle_py" (Get-PropValue $Replay.target "circle_py" 0))
    $g["global_wgap"] = As-Int (Get-PropValue $Summary "effective_wgap" (Get-PropValue $Replay.effective_gauge "wgap" 32))
    $g["global_hgap"] = As-Int (Get-PropValue $Summary "effective_hgap" (Get-PropValue $Replay.effective_gauge "hgap" 8))
    $g["global_gap"] = As-Int (Get-PropValue $Summary "effective_gap" (Get-PropValue $Replay.effective_gauge "gap" 5))
    $g["global_linegap"] = As-Int (Get-PropValue $Summary "effective_linegap" (Get-PropValue $Replay.effective_gauge "linegap" 6))
    $g["global_threshold"] = As-Int (Get-PropValue $Summary "effective_threshold" (Get-PropValue $Replay.effective_gauge "threshold" 20))
    $g["global_method"] = As-Int (Get-PropValue $Summary "effective_method" (Get-PropValue $Replay.effective_gauge "method" 0))
    $g["global_filterprofile"] = As-Int (Get-PropValue $Summary "effective_filterprofile" (Get-PropValue $Replay.effective_gauge "filterprofile" 0))
    $g["global_findline_edge_count"] = 0
    $g["global_findline_selected_edge"] = 0
    $g["global_findline_best_edge"] = 0
    $g["global_findline_recommended_edge"] = 0
    $g["global_findline_relation_edge"] = 0
    $g["global_findline_attach_edge"] = 0
    $g["global_strategy_id"] = 0
    $g["global_max_elapsed_ms"] = 2000
    $g["global_max_scan_lines"] = 2000
    $g["global_max_samples"] = 200000
    return $g
}

function Write-GlobalsFile([string]$Path, $Globals) {
    $lines = New-Object System.Collections.Generic.List[string]
    foreach ($key in $Globals.Keys) {
        $lines.Add("$key=$($Globals[$key])")
    }
    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllLines($Path, [string[]]$lines, $utf8NoBom)
}

function Quote-ProcessArgument([string]$Text) {
    if ($null -eq $Text) {
        return '""'
    }
    return '"' + ($Text -replace '"', '\"') + '"'
}

function Invoke-HeadlessCase(
    [string]$CaseId,
    [string]$ImagePath,
    [string]$ScriptPath,
    [string]$GlobalsPath,
    [string]$CaseOut,
    [int]$TimeoutSeconds
) {
    New-Dir $CaseOut
    $stdout = Join-Path $CaseOut "stdout.txt"
    $stderr = Join-Path $CaseOut "stderr.txt"
    $args = @(
        "--cxscript-headless",
        "--image", $ImagePath,
        "--script", $ScriptPath,
        "--globals", $GlobalsPath,
        "--case-name", $CaseId,
        "--out", $CaseOut,
        "--max-steps", "10000",
        "--unified-log", $SharedLog
    )

    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $Binary
    $psi.WorkingDirectory = $RepoRoot
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.Arguments = (($args | ForEach-Object { Quote-ProcessArgument $_ }) -join " ")

    $p = New-Object System.Diagnostics.Process
    $p.StartInfo = $psi
    [void]$p.Start()
    $completed = $p.WaitForExit($TimeoutSeconds * 1000)
    if (-not $completed) {
        try { $p.Kill() } catch {}
        Set-Content -LiteralPath $stdout -Encoding UTF8 -Value ($p.StandardOutput.ReadToEnd())
        Set-Content -LiteralPath $stderr -Encoding UTF8 -Value ($p.StandardError.ReadToEnd())
        return [pscustomobject]@{ ExitCode = -999; Timeout = $true; Reason = "case timeout after $TimeoutSeconds seconds" }
    }

    Set-Content -LiteralPath $stdout -Encoding UTF8 -Value ($p.StandardOutput.ReadToEnd())
    Set-Content -LiteralPath $stderr -Encoding UTF8 -Value ($p.StandardError.ReadToEnd())
    return [pscustomobject]@{ ExitCode = $p.ExitCode; Timeout = $false; Reason = "" }
}

function Make-LineCandidates($Seeds) {
    $list = New-Object System.Collections.Generic.List[object]
    $seen = @{}
    foreach ($seed in $Seeds) {
        $key = "$(As-Int $seed.method)-$(As-Int $seed.threshold)-$(As-Int $seed.linegap)-$(As-Int $seed.wgap)-$(As-Int $seed.hgap)-$(As-Int $seed.filterprofile)-$(As-Int $seed.selected_edge)"
        if ($seen.ContainsKey($key)) {
            continue
        }
        $seen[$key] = $true
        $list.Add([pscustomobject]@{
            candidate_id = "seed_" + (Sanitize-Id $seed.case_id)
            source_case_id = $seed.case_id
            method = As-Int $seed.method
            threshold = As-Int $seed.threshold
            linegap = As-Int $seed.linegap
            wgap = As-Int $seed.wgap
            hgap = As-Int $seed.hgap
            filterprofile = As-Int $seed.filterprofile
            selected_edge = As-Int $seed.selected_edge
        })
    }
    return $list
}

function Make-CircleCandidates($Summary, $Replay) {
    $baseMethod = As-Int (Get-PropValue $Summary "effective_method" (Get-PropValue $Replay.effective_gauge "method" 0))
    $baseThreshold = As-Int (Get-PropValue $Summary "effective_threshold" (Get-PropValue $Replay.effective_gauge "threshold" 20))
    $baseGap = As-Int (Get-PropValue $Summary "effective_gap" (Get-PropValue $Replay.effective_gauge "gap" 5))
    $baseLinegap = As-Int (Get-PropValue $Summary "effective_linegap" (Get-PropValue $Replay.effective_gauge "linegap" 6))
    $rows = @(
        @{ id = "baseline"; method = $baseMethod; threshold = $baseThreshold; gap = $baseGap; linegap = $baseLinegap },
        @{ id = "flip_method"; method = ($(if ($baseMethod -eq 0) { 1 } else { 0 })); threshold = $baseThreshold; gap = $baseGap; linegap = $baseLinegap },
        @{ id = "threshold8"; method = $baseMethod; threshold = 8; gap = $baseGap; linegap = $baseLinegap },
        @{ id = "method1_threshold8"; method = 1; threshold = 8; gap = $baseGap; linegap = $baseLinegap },
        @{ id = "gap3"; method = $baseMethod; threshold = $baseThreshold; gap = 3; linegap = $baseLinegap },
        @{ id = "method1_gap3_threshold8"; method = 1; threshold = 8; gap = 3; linegap = $baseLinegap }
    )
    $list = New-Object System.Collections.Generic.List[object]
    $seen = @{}
    foreach ($r in $rows) {
        $key = "$($r.method)-$($r.threshold)-$($r.gap)-$($r.linegap)"
        if ($seen.ContainsKey($key)) {
            continue
        }
        $seen[$key] = $true
        $list.Add([pscustomobject]@{
            candidate_id = $r.id
            method = [int]$r.method
            threshold = [int]$r.threshold
            gap = [int]$r.gap
            linegap = [int]$r.linegap
        })
    }
    return $list
}

function Classify-Result($Tool, $Run, $Summary) {
    if ($Run.Timeout) {
        return [pscustomobject]@{ Category = "To Verify"; Conclusion = "TIMEOUT"; Reason = $Run.Reason }
    }
    if ($Run.ExitCode -ne 0 -or $null -eq $Summary) {
        return [pscustomobject]@{ Category = "To Verify"; Conclusion = "HEADLESS_EXECUTION_FAIL"; Reason = "exit_code=$($Run.ExitCode)" }
    }

    $points = As-Int (Get-SummaryValue $Summary "valid_points_count" 0)
    $avg = As-Double (Get-SummaryValue $Summary "avgdist" 0)
    $failure = [string](Get-SummaryValue $Summary "failure_stage" "")

    if ($Tool -eq "FindLine") {
        $hasFit = As-Bool (Get-SummaryValue $Summary "has_fit_line" $false)
        if ($points -ge 2 -and $hasFit) {
            return [pscustomobject]@{ Category = "Process Validation"; Conclusion = "AUTO_PROCESS_VALIDATION_PASS"; Reason = "FindLine fit line available" }
        }
        return [pscustomobject]@{ Category = "To Verify"; Conclusion = "PENDING_HUMAN_REVIEW"; Reason = "FindLine no stable fit: points=$points fit=$hasFit failure=$failure" }
    }

    if ($Tool -eq "FindCircle") {
        $hasFit = As-Bool (Get-SummaryValue $Summary "has_fit_circle" $false)
        $radius = As-Double (Get-SummaryValue $Summary "circle_radius" 0)
        if ($points -ge 3 -and $hasFit -and $radius -gt 0 -and $avg -le 8.0) {
            return [pscustomobject]@{ Category = "Process Validation"; Conclusion = "AUTO_PROCESS_VALIDATION_PASS"; Reason = "FindCircle fit circle available" }
        }
        if ($points -ge 3 -and $hasFit -and $radius -gt 0) {
            return [pscustomobject]@{ Category = "To Verify"; Conclusion = "PENDING_HUMAN_REVIEW_HIGH_RESIDUAL"; Reason = "FindCircle fit exists but avgdist=$avg > 8.0" }
        }
        return [pscustomobject]@{ Category = "To Verify"; Conclusion = "PENDING_HUMAN_REVIEW"; Reason = "FindCircle no stable fit: points=$points fit=$hasFit radius=$radius failure=$failure" }
    }

    return [pscustomobject]@{ Category = "To Verify"; Conclusion = "UNSUPPORTED_TOOL"; Reason = "tool=$Tool" }
}

New-Dir $OutDir
New-Dir (Join-Path $OutDir "cases")

$seedDoc = Read-JsonFile $ManualSeedSummary
$lineSeeds = @($seedDoc.cases | Where-Object { $_.tool -eq "FindLine" -and $_.conclusion -eq "AUTO_PROCESS_VALIDATION_PASS" })
$lineCandidates = @(Make-LineCandidates $lineSeeds)

$caseFiles = @(Get-ChildItem -LiteralPath $SourceCasesRoot -Recurse -Filter result_summary.json |
    Where-Object { $_.FullName -notmatch '\\contract\\' } |
    Sort-Object FullName)
if ($MaxCases -gt 0) {
    $caseFiles = @($caseFiles | Select-Object -First $MaxCases)
}

$results = @()
$casePlans = @()

foreach ($summaryFile in $caseFiles) {
    $caseDir = $summaryFile.Directory.FullName
    $replayPath = Join-Path $caseDir "replay_package.json"
    if (-not (Test-Path -LiteralPath $replayPath)) {
        continue
    }

    $summary = Read-JsonFile $summaryFile.FullName
    $replay = Read-JsonFile $replayPath
    $tool = [string](Get-PropValue $replay.target "tool" (Get-PropValue $summary "tool" ""))
    if ($tool -ne "FindLine" -and $tool -ne "FindCircle") {
        continue
    }

    $caseId = [string](Get-PropValue $summary "case_id" $summaryFile.Directory.Name)
    $imagePath = [string](Get-PropValue $replay.image "path" "")
    if (-not (Test-Path -LiteralPath $imagePath)) {
        $results += [pscustomobject]@{
            case_id = $caseId; tool = $tool; candidate_id = "asset_preflight"; category = "To Verify";
            conclusion = "ASSET_PREFLIGHT_FAIL"; reason = "image missing: $imagePath";
            result_summary = ""; valid_points = 0; avgdist = 0; has_fit_line = $false; has_fit_circle = $false
        }
        continue
    }

    $candidates = @()
    $script = ""
    if ($tool -eq "FindLine") {
        $candidates = $lineCandidates
        $script = Pick-LineScript $replay $summary
    } elseif ($tool -eq "FindCircle") {
        $candidates = @(Make-CircleCandidates $summary $replay)
        $script = "cxparser/cxscript/module/cximage/frozen/findcircle/findcircle_stage25_direct_ok.cxsc"
    }

    $casePlans += [pscustomobject]@{
        case_id = $caseId
        tool = $tool
        image_id = [string](Get-PropValue $replay.image "image_id" (Get-PropValue $summary "image_id" ""))
        target_id = [string](Get-PropValue $replay.target "target_id" (Get-PropValue $summary "target_id" ""))
        script = $script
        candidate_count = @($candidates).Count
    }

    foreach ($candidate in $candidates) {
        $candidateSafe = Sanitize-Id $candidate.candidate_id
        $caseOutName = Sanitize-Id "$caseId`__$candidateSafe"
        $caseOut = Join-Path (Join-Path $OutDir "cases") $caseOutName
        New-Dir $caseOut

        $globals = New-GlobalMap $replay $summary
        if ($tool -eq "FindLine") {
            $globals["global_method"] = As-Int $candidate.method
            $globals["global_threshold"] = As-Int $candidate.threshold
            $globals["global_linegap"] = As-Int $candidate.linegap
            $globals["global_wgap"] = As-Int $candidate.wgap
            $globals["global_hgap"] = As-Int $candidate.hgap
            $globals["global_filterprofile"] = As-Int $candidate.filterprofile
            $globals["global_findline_selected_edge"] = As-Int $candidate.selected_edge
            $globals["global_findline_recommended_edge"] = As-Int $candidate.selected_edge
        } elseif ($tool -eq "FindCircle") {
            $globals["global_method"] = As-Int $candidate.method
            $globals["global_threshold"] = As-Int $candidate.threshold
            $globals["global_gap"] = As-Int $candidate.gap
            $globals["global_linegap"] = As-Int $candidate.linegap
        }

        $globalsPath = Join-Path $caseOut "globals.txt"
        Write-GlobalsFile $globalsPath $globals

        $run = [pscustomobject]@{ ExitCode = 0; Timeout = $false; Reason = "dry-run only" }
        if (-not $DryRun) {
            $run = Invoke-HeadlessCase $caseOutName $imagePath $script $globalsPath $caseOut $MaxCaseSeconds
        }

        $outSummaryPath = Join-Path $caseOut "result_summary.json"
        $outSummary = $null
        if (Test-Path -LiteralPath $outSummaryPath) {
            try { $outSummary = Read-JsonFile $outSummaryPath } catch { $outSummary = $null }
        }
        $classification = Classify-Result $tool $run $outSummary

        $results += [pscustomobject]@{
            case_id = $caseId
            image_id = [string](Get-PropValue $replay.image "image_id" (Get-PropValue $summary "image_id" ""))
            target_id = [string](Get-PropValue $replay.target "target_id" (Get-PropValue $summary "target_id" ""))
            tool = $tool
            source_case_dir = $caseDir
            candidate_id = [string]$candidate.candidate_id
            source_seed_case_id = [string](Get-PropValue $candidate "source_case_id" "")
            script = $script
            image_path = $imagePath
            globals_path = $globalsPath
            out_dir = $caseOut
            exit_code = $run.ExitCode
            timeout = $run.Timeout
            category = $classification.Category
            conclusion = $classification.Conclusion
            reason = $classification.Reason
            valid_points = As-Int (Get-SummaryValue $outSummary "valid_points_count" 0)
            has_fit_line = As-Bool (Get-SummaryValue $outSummary "has_fit_line" $false)
            has_fit_circle = As-Bool (Get-SummaryValue $outSummary "has_fit_circle" $false)
            circle_radius = As-Double (Get-SummaryValue $outSummary "circle_radius" 0)
            avgdist = As-Double (Get-SummaryValue $outSummary "avgdist" 0)
            failure_stage = [string](Get-SummaryValue $outSummary "failure_stage" "")
            result_summary = $outSummaryPath
            result_overlay = (Join-Path $caseOut "result_overlay.png")
            tool_display = (Join-Path $caseOut "tool_display.png")
        }
    }
}

$ranked = $results | Sort-Object `
    @{ Expression = { $_.case_id }; Ascending = $true },
    @{ Expression = { if ($_.category -eq "Process Validation") { 0 } else { 1 } }; Ascending = $true },
    @{ Expression = { -1 * [int]$_.valid_points }; Ascending = $true },
    @{ Expression = { [double]$_.avgdist }; Ascending = $true }

$bestByCase = @()
foreach ($group in ($ranked | Group-Object case_id)) {
    $bestByCase += ($group.Group | Select-Object -First 1)
}

$binaryLastWriteTime = ""
if (Test-Path -LiteralPath $Binary) {
    $binaryLastWriteTime = (Get-Item -LiteralPath $Binary).LastWriteTime.ToString("s")
}
$runId = Split-Path -Leaf $OutDir
$generatedAt = (Get-Date).ToString("s")
$casePlanArray = @($casePlans)
$resultArray = @($results)
$bestByCaseArray = @($bestByCase)
$processCandidateCount = @($resultArray | Where-Object { $_.category -eq "Process Validation" }).Count
$toVerifyCandidateCount = @($resultArray | Where-Object { $_.category -eq "To Verify" }).Count
$bestProcessCount = @($bestByCaseArray | Where-Object { $_.category -eq "Process Validation" }).Count
$bestToVerifyCount = @($bestByCaseArray | Where-Object { $_.category -eq "To Verify" }).Count

$summaryOut = [pscustomobject]@{
    run_id = $runId
    generated_at = $generatedAt
    repo_root = $RepoRoot
    binary = $Binary
    binary_last_write_time = $binaryLastWriteTime
    source_cases_root = $SourceCasesRoot
    manual_seed_summary = $ManualSeedSummary
    unified_log = $SharedLog
    dry_run = [bool]$DryRun
    total_source_cases = $casePlanArray.Count
    total_candidates = $resultArray.Count
    process_validation_candidates = $processCandidateCount
    to_verify_candidates = $toVerifyCandidateCount
    best_process_validation_cases = $bestProcessCount
    best_to_verify_cases = $bestToVerifyCount
    case_plan = $casePlanArray
    best_by_case = $bestByCaseArray
    candidates = $resultArray
}

$summaryPath = Join-Path $OutDir "experience_propagation_summary.json"
$summaryOut | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $summaryPath -Encoding UTF8

$report = New-Object System.Collections.Generic.List[string]
$report.Add("# Evidence Experience Propagation Report")
$report.Add("")
$report.Add("- Run ID: $runId")
$report.Add("- Generated: $generatedAt")
$report.Add("- Binary: $Binary")
$report.Add("- Source cases: $SourceCasesRoot")
$report.Add("- Manual seed summary: $ManualSeedSummary")
$report.Add("- Unified log: $SharedLog")
$report.Add("- Dry run: $([bool]$DryRun)")
$report.Add("")
$report.Add("## Summary")
$report.Add("")
$report.Add("| Metric | Value |")
$report.Add("|---|---:|")
$report.Add("| Source cases | $($casePlanArray.Count) |")
$report.Add("| Candidate runs | $($resultArray.Count) |")
$report.Add("| Process Validation candidates | $processCandidateCount |")
$report.Add("| To Verify candidates | $toVerifyCandidateCount |")
$report.Add("| Best Process Validation cases | $bestProcessCount |")
$report.Add("| Best To Verify cases | $bestToVerifyCount |")
$report.Add("")
$report.Add("## Best Candidate By Case")
$report.Add("")
$report.Add("| Category | Tool | Case | Candidate | Points | FitLine | FitCircle | Radius | AvgDist | Conclusion | Reason |")
$report.Add("|---|---|---|---|---:|---|---|---:|---:|---|---|")
foreach ($r in $bestByCase) {
    $report.Add("| $($r.category) | $($r.tool) | $($r.case_id) | $($r.candidate_id) | $($r.valid_points) | $($r.has_fit_line) | $($r.has_fit_circle) | $([Math]::Round([double]$r.circle_radius, 4)) | $([Math]::Round([double]$r.avgdist, 4)) | $($r.conclusion) | $($r.reason) |")
}
$report.Add("")
$report.Add("## All Candidate Runs")
$report.Add("")
$report.Add("| Category | Tool | Case | Candidate | Seed | Points | AvgDist | FailureStage | OutDir |")
$report.Add("|---|---|---|---|---|---:|---:|---|---|")
foreach ($r in $results) {
    $report.Add("| $($r.category) | $($r.tool) | $($r.case_id) | $($r.candidate_id) | $($r.source_seed_case_id) | $($r.valid_points) | $([Math]::Round([double]$r.avgdist, 4)) | $($r.failure_stage) | $($r.out_dir) |")
}

$reportPath = Join-Path $OutDir "experience_propagation_report.md"
Set-Content -LiteralPath $reportPath -Encoding UTF8 -Value $report

$processPath = Join-Path $OutDir "process_validation_candidates.tsv"
$toVerifyPath = Join-Path $OutDir "to_verify_remaining.tsv"
$results | Where-Object { $_.category -eq "Process Validation" } |
    Select-Object case_id,tool,candidate_id,source_seed_case_id,valid_points,avgdist,result_summary,out_dir |
    ConvertTo-Csv -NoTypeInformation -Delimiter "`t" |
    Set-Content -LiteralPath $processPath -Encoding UTF8
$bestByCase | Where-Object { $_.category -eq "To Verify" } |
    Select-Object case_id,tool,candidate_id,conclusion,reason,valid_points,avgdist,failure_stage,result_summary,out_dir |
    ConvertTo-Csv -NoTypeInformation -Delimiter "`t" |
    Set-Content -LiteralPath $toVerifyPath -Encoding UTF8

Write-Host "EXPERIENCE_PROPAGATION_DONE"
Write-Host "summary=$summaryPath"
Write-Host "report=$reportPath"
Write-Host "process_candidates=$processPath"
Write-Host "to_verify=$toVerifyPath"
if ($bestToVerifyCount -gt 0) {
    exit 2
}
exit 0
