import argparse
import json
import subprocess
import cv2
import numpy as np
import time
from pathlib import Path
from datetime import datetime
from collections import defaultdict

ROOT = Path(r"D:\Codex-WorkDir\Sean_WorkDir\cxvisionai")
REPO = ROOT / "cxvision_repo"
DEFAULT_OUT = ROOT / "cxscript_runs" / "param_regression"
MANIFEST_PATH = REPO / "cxparser" / "cxscript" / "module" / "cximage" / "stage25" / "manifests" / "stage25_l1_l3_manifest.json"

SCRIPT_PATH = REPO / "cxparser" / "cxscript" / "module" / "cximage" / "stage25" / "param_regression" / "param_self_tuning_findline_direct.cxsc"

MAX_CASE_SECONDS = 10
MAX_TOTAL_SECONDS = 150
MAX_ITERATIONS = 5

def find_exe(explicit=None):
    candidates = [Path(explicit)] if explicit else []
    candidates += [
        ROOT / "build01" / "Release" / "cxvision_imgui_acceptance.exe",
        ROOT / "build" / "Release" / "cxvision_imgui_acceptance.exe",
        ROOT / "build01" / "Debug" / "cxvision_imgui_acceptance.exe",
        ROOT / "build" / "Debug" / "cxvision_imgui_acceptance.exe",
    ]
    existing = [p for p in candidates if p and p.exists()]
    if not existing:
        raise FileNotFoundError("cxvision_imgui_acceptance.exe not found")
    return max(existing, key=lambda p: p.stat().st_mtime)

def parse_scalar(value):
    if value is None:
        return None
    value = str(value).strip()
    if value.lower() in ("true", "false"):
        return value.lower() == "true"
    try:
        return float(value) if any(c in value for c in ".eE") else int(value)
    except ValueError:
        return value

def run_headless(exe, image_path, out_dir, case_name, params):
    out_dir.mkdir(parents=True, exist_ok=True)
    
    cmd = [
        str(exe),
        "--headless",
        "--cxscript-headless",
        "--image", str(image_path),
        "--script", str(SCRIPT_PATH),
        "--case-name", case_name,
        "--out", str(out_dir),
        "--max-steps", "10000",
    ]
    
    for key, value in params.items():
        cmd.append(f"--{key}")
        cmd.append(str(value))
    
    proc = subprocess.run(cmd, cwd=str(REPO), capture_output=True, text=True, timeout=MAX_CASE_SECONDS + 5)
    
    summary_path = out_dir / "result_summary.json"
    snapshot_path = out_dir / "snapshot.txt"
    overlay_path = out_dir / "result_overlay.png"
    evidence_overlay_path = out_dir / "evidence_overlay.png"
    tool_display_path = out_dir / "tool_display.png"
    
    summary = {}
    if summary_path.exists():
        try:
            summary = json.loads(summary_path.read_text(encoding="utf-8-sig"))
        except:
            summary = {}
    
    snapshot = snapshot_path.read_text(encoding="utf-8-sig", errors="replace") if snapshot_path.exists() else ""
    
    assets_present = all([
        snapshot_path.exists(),
        summary_path.exists(),
        overlay_path.exists(),
        evidence_overlay_path.exists(),
        tool_display_path.exists()
    ])
    
    objects = summary.get("runtime_objects") or []
    obj = next((x for x in objects if x.get("type") == "Findline"), {})
    
    points = obj.get("valid_line_points_count")
    has_fit = obj.get("has_fit_line")
    support = obj.get("local_support")
    avgdist = obj.get("local_mean_distance_px")
    
    elapsed_ms = parse_scalar(summary.get("elapsed_ms"))
    
    return {
        "case_name": case_name,
        "exit_code": proc.returncode,
        "assets_present": assets_present,
        "valid_points_count": points,
        "has_fit_line": has_fit,
        "support": support,
        "avgdist": avgdist,
        "elapsed_ms": elapsed_ms,
        "timeout": proc.returncode == -1 or (elapsed_ms and elapsed_ms > MAX_CASE_SECONDS * 1000),
        "failure_stage": summary.get("failure_stage"),
        "snapshot": snapshot,
        "stdout": proc.stdout.strip(),
        "stderr": proc.stderr.strip(),
        "params": params,
    }

def run_phase1(exe, image_path, base_roi, baseline_params, run_dir):
    phase1_dir = run_dir / "phase1"
    phase1_dir.mkdir(parents=True, exist_ok=True)
    
    x0, y0, x1, y1 = base_roi
    results = []
    
    for offset_y in range(-50, 51, 5):
        case_id = f"roi_scan_offset_y_{offset_y:+04d}"
        out_dir = phase1_dir / case_id
        
        params = dict(baseline_params)
        params.update({
            "roi_x0": x0,
            "roi_y0": y0 + offset_y,
            "roi_x1": x1,
            "roi_y1": y1 + offset_y,
        })
        
        result = run_headless(exe, image_path, out_dir, case_id, params)
        result["offset_y"] = offset_y
        results.append(result)
        
        print(f"Phase1: offset_y={offset_y:+4d} -> points={result['valid_points_count']}, fit={result['has_fit_line']}")
    
    sorted_results = sorted(results, key=lambda r: (
        not r.get("has_fit_line"),
        not (5 <= (r.get("valid_points_count") or 0) <= 30),
        not ((r.get("support") or 0) >= 0.7),
        r.get("avgdist") or 9999,
        r.get("timeout") or False,
    ))
    
    best = sorted_results[0] if sorted_results else None
    
    phase1_output = {
        "results": results,
        "sorted_results": [r["case_name"] for r in sorted_results],
        "best_candidate": best,
    }
    
    with open(phase1_dir / "phase1_roi_scan_matrix.json", "w", encoding="utf-8") as f:
        json.dump(phase1_output, f, indent=2, ensure_ascii=False)
    
    with open(phase1_dir / "phase1_best_candidate.json", "w", encoding="utf-8") as f:
        json.dump(best or {}, f, indent=2, ensure_ascii=False)
    
    write_phase1_report(phase1_dir, results, best)
    
    if best and best.get("assets_present") and best.get("has_fit_line"):
        return "PHASE1_ROI_CANDIDATE_FOUND", best
    elif best and best.get("assets_present"):
        return "PHASE1_ROI_CANDIDATE_FOUND", best
    else:
        return "PHASE1_ROI_NO_VALID_CANDIDATE", best

def write_phase1_report(phase1_dir, results, best):
    lines = ["# Phase 1: Gauge ROI Scan Report", "", "## Summary", ""]
    
    total = len(results)
    valid = sum(1 for r in results if r.get("assets_present"))
    fit_success = sum(1 for r in results if r.get("has_fit_line"))
    
    lines.append(f"- Total candidates: {total}")
    lines.append(f"- Assets present: {valid}")
    lines.append(f"- Fit success: {fit_success}")
    
    if best:
        lines.append(f"- Best offset_y: {best['offset_y']}")
        lines.append(f"- Best points: {best.get('valid_points_count')}")
        lines.append(f"- Best fit: {best.get('has_fit_line')}")
        lines.append(f"- Best support: {best.get('support')}")
        lines.append(f"- Best avgdist: {best.get('avgdist')}")
    
    lines += ["", "## ROI Scan Results", "",
              "| OffsetY | Points | Fit | Support | AvgDist | Timeout | Assets |"]
    lines += ["|---|---:|---|---:|---:|---|---|"]
    
    for r in sorted(results, key=lambda x: x["offset_y"]):
        lines.append(f"| {r['offset_y']:+4d} | {r.get('valid_points_count') or ''} | {r.get('has_fit_line')} | {r.get('support') or ''} | {r.get('avgdist') or ''} | {r.get('timeout')} | {r.get('assets_present')} |")
    
    (phase1_dir / "phase1_roi_scan_report.md").write_text("\n".join(lines), encoding="utf-8")

def run_phase2(exe, image_path, best_roi, baseline_params, run_dir):
    phase2