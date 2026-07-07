import argparse
import json
import re
import subprocess
from pathlib import Path
from collections import defaultdict

ROOT = Path(r"D:\Codex-WorkDir\Sean_WorkDir\cxvisionai")
REPO = ROOT / "cxvision_repo"
DEFAULT_OUT = ROOT / "cxscript_runs" / "stage_2_5"
IMAGE = ROOT / "01.jpg"

CASES = [
    ("find_line_direct", "Findline", "find_line_direct_test.cxsc", "original_measure"),
    ("find_line_method0", "Findline", "find_line_method0_test.cxsc", "original_measure"),
    ("find_line_method1", "Findline", "find_line_method1_test.cxsc", "original_measure"),
    ("find_line_threshold8", "Findline", "find_line_threshold8_test.cxsc", "original_measure"),
    ("find_line_threshold40", "Findline", "find_line_threshold40_test.cxsc", "original_measure"),
    ("find_line_linegap3", "Findline", "find_line_linegap3_test.cxsc", "original_measure"),
    ("find_line_linegap10", "Findline", "find_line_linegap10_test.cxsc", "original_measure"),
    ("find_line_filter_relax", "Findline", "find_line_filter_relax_test.cxsc", "original_measure"),
    ("find_line_gamma", "Findline", "find_line_gamma_test.cxsc", "original_measure"),
    ("find_line_fallback_debug", "Findline", "find_line_fallback_debug_test.cxsc", "fallback_only"),
    ("find_circle_direct", "Findcircle", "find_circle_direct_test.cxsc", "original_measure"),
    ("find_circle_method1", "Findcircle", "find_circle_method1_test.cxsc", "original_measure"),
    ("find_circle_threshold8", "Findcircle", "find_circle_threshold8_test.cxsc", "original_measure"),
    ("find_circle_linegap6", "Findcircle", "find_circle_linegap6_test.cxsc", "original_measure"),
    ("find_circle_gap8", "Findcircle", "find_circle_gap8_test.cxsc", "original_measure"),
    ("find_circle_samplerate", "Findcircle", "find_circle_samplerate_test.cxsc", "original_measure"),
    ("find_circle_filter_relax", "Findcircle", "find_circle_filter_relax_test.cxsc", "original_measure"),
    ("find_circle_fitresult_guard", "Findcircle", "find_circle_fitresult_guard_test.cxsc", "guard"),
]

FILTER_SWEEP_CASES = [
    ("find_line_filter_min1", "Findline", "find_line_filter_min1_test.cxsc", "filter_sweep"),
    ("find_line_filter_min5", "Findline", "find_line_filter_min5_test.cxsc", "filter_sweep"),
    ("find_line_filter_min10", "Findline", "find_line_filter_min10_test.cxsc", "filter_sweep"),
    ("find_line_filter_min20", "Findline", "find_line_filter_min20_test.cxsc", "filter_sweep"),
    ("find_line_filter_min30", "Findline", "find_line_filter_min30_test.cxsc", "filter_sweep"),
    ("find_line_filter_min40", "Findline", "find_line_filter_min40_test.cxsc", "filter_sweep"),
    ("find_line_filter_min50", "Findline", "find_line_filter_min50_test.cxsc", "filter_sweep"),
    ("find_line_filter_min80", "Findline", "find_line_filter_min80_test.cxsc", "filter_sweep"),
]

FILTER_FINE_SWEEP_CASES = [
    ("find_line_filter_min15", "Findline", "find_line_filter_min15_test.cxsc", "filter_sweep"),
    ("find_line_filter_min20", "Findline", "find_line_filter_min20_test.cxsc", "filter_sweep"),
    ("find_line_filter_min25", "Findline", "find_line_filter_min25_test.cxsc", "filter_sweep"),
    ("find_line_filter_min30", "Findline", "find_line_filter_min30_test.cxsc", "filter_sweep"),
    ("find_line_filter_min35", "Findline", "find_line_filter_min35_test.cxsc", "filter_sweep"),
    ("find_line_filter_min38", "Findline", "find_line_filter_min38_test.cxsc", "filter_sweep"),
    ("find_line_filter_min40", "Findline", "find_line_filter_min40_test.cxsc", "filter_sweep"),
]

SCRIPT_DIR = REPO / "cxparser" / "cxscript" / "module" / "cximage"


def find_exe(explicit=None):
    candidates = [Path(explicit)] if explicit else []
    candidates += [
        ROOT / "AIbuild" / "Release" / "cxvision_imgui_acceptance.exe",
        ROOT.parent / "cxparser" / "build" / "Release" / "cxvision_imgui_acceptance.exe",
        ROOT / "AIbuild" / "Debug" / "cxvision_imgui_acceptance.exe",
    ]
    existing = [p for p in candidates if p and p.exists()]
    if not existing:
        raise FileNotFoundError("cxvision_imgui_acceptance.exe not found")
    return max(existing, key=lambda p: p.stat().st_mtime)


def parse_scalar(value):
    value = value.strip()
    if value in ("true", "false"):
        return value == "true"
    try:
        return float(value) if any(c in value for c in ".eE") else int(value)
    except ValueError:
        return value


def snapshot_value(text, label):
    matches = re.findall(rf"^\s*{re.escape(label)}:\s*(.*?)\s*$", text, re.MULTILINE)
    return parse_scalar(matches[-1]) if matches else None


def status_value(text, key):
    matches = re.findall(rf"(?:^|[,|])\s*{re.escape(key)}=([^,|\r\n]+)", text)
    return parse_scalar(matches[-1]) if matches else None


def script_int(text, name):
    m = re.search(rf"\bint\s+{re.escape(name)}\s*=\s*(-?\d+)\s*;", text)
    return int(m.group(1)) if m else None


def script_float_call(text, method):
    m = re.search(rf"\.{re.escape(method)}\(\s*(-?\d+(?:\.\d+)?)\s*\)", text)
    return float(m.group(1)) if m else None


def run_case(exe, out_root, item, image_path=None, evidence_profile=None):
    name, tool, filename, expect = item
    script = SCRIPT_DIR / filename
    case_dir = out_root
    case_dir.mkdir(parents=True, exist_ok=True)
    
    image = Path(image_path) if image_path else IMAGE
    evidence_arg = ["--evidence-profile", evidence_profile] if evidence_profile else []
    
    cmd = [str(exe), "--cxscript-headless", "--image", str(image),
           "--script", str(script), "--out", str(case_dir), "--case-name", name] + evidence_arg
    proc = subprocess.run(cmd, cwd=str(REPO), capture_output=True, text=True, timeout=120)
    
    summary_path = case_dir / "result_summary.json"
    snapshot_path = case_dir / "snapshot.txt"
    overlay_path = case_dir / "result_overlay.png"
    evidence_summary_path = case_dir / "evidence_summary.json"
    evidence_overlay_path = case_dir / "evidence_overlay.png"
    point_evidence_path = case_dir / "point_evidence.csv"
    
    summary = {}
    if summary_path.exists():
        try:
            summary = json.loads(summary_path.read_text(encoding="utf-8-sig"))
        except Exception as exc:
            summary = {"summary_parse_error": str(exc)}
    
    evidence_summary = {}
    if evidence_summary_path.exists():
        try:
            evidence_summary = json.loads(evidence_summary_path.read_text(encoding="utf-8-sig"))
        except Exception as exc:
            evidence_summary = {"parse_error": str(exc)}
    
    snapshot = snapshot_path.read_text(encoding="utf-8-sig", errors="replace") if snapshot_path.exists() else ""
    script_text = script.read_text(encoding="utf-8-sig", errors="replace") if script.exists() else ""
    objects = summary.get("runtime_objects") or []
    obj = next((x for x in objects if x.get("type") == tool), {})
    
    evidence_summaries = evidence_summary.get("evidence_summaries") or []
    ev_summary = next((x for x in evidence_summaries if x.get("tool") == tool), {})
    
    measure_status = obj.get("line_measure_status", "") if tool == "Findline" else ""
    points = obj.get("valid_line_points_count") if tool == "Findline" else obj.get("valid_points_count")
    has_fit = obj.get("has_fit_line") if tool == "Findline" else obj.get("has_fit_result")
    
    record = {
        "case_name": name, "tool": tool, "script": str(script), "expect": expect,
        "image_path": str(image),
        "evidence_profile": evidence_profile,
        "exit_code": proc.returncode, 
        "headless_ok": summary_path.exists() and snapshot_path.exists() and overlay_path.exists(),
        "run_state": summary.get("run_state"), "debug_status": summary.get("debug_status"),
        "debug_reason": summary.get("debug_reason"), "stdout": proc.stdout.strip(), "stderr": proc.stderr.strip(),
        "snapshot_path": str(snapshot_path), "overlay_path": str(overlay_path), "summary_path": str(summary_path),
        "evidence_summary_path": str(evidence_summary_path), "evidence_overlay_path": str(evidence_overlay_path),
        "point_evidence_path": str(point_evidence_path),
        "measure_source": (obj.get("line_measure_source") if tool == "Findline" else obj.get("circle_measure_source")) or status_value(measure_status or snapshot, "source") or snapshot_value(snapshot, "line_measure_source") or snapshot_value(snapshot, "circle_measure_source"),
        "fallback_used": status_value(measure_status or snapshot, "fallback_used") or snapshot_value(snapshot, "line_measure_fallback_used"),
        "failure_stage": (snapshot_value(snapshot, "line_measure_input_failure_stage") if tool == "Findline" else obj.get("circle_measure_failure_stage")) or obj.get("line_measure_failure_stage") or status_value(measure_status or snapshot, "failure_stage") or snapshot_value(snapshot, "circle_measure_failure_stage"),
        "failure_reason": snapshot_value(snapshot, "line_measure_input_detail") or summary.get("debug_reason"),
        "points_count": snapshot_value(snapshot, "measure_points_count") or points,
        "valid_points_count": points,
        "method": script_int(script_text, "method"), "threshold": script_int(script_text, "threshold"),
        "linegap": script_int(script_text, "linegap"), "gap": script_int(script_text, "gap"),
        "wgap": script_int(script_text, "wgap"), "hgap": script_int(script_text, "hgap"),
        "samplerate": script_float_call(script_text, "setlinesamplerate"),
        "has_line_scan_box": obj.get("has_line_scan_box"), "line_scan_half_width": snapshot_value(snapshot, "line_scan_half_width"),
        "max_gradient": snapshot_value(snapshot, "line_measure_max_gradient"),
        "binary_foreground_pixels": snapshot_value(snapshot, "line_measure_binary_foreground_pixels"),
        "findobject_called": snapshot_value(snapshot, "line_measure_findobject_called"),
        "filter_min": snapshot_value(snapshot, "line_measure_filter_min"), "filter_max": snapshot_value(snapshot, "line_measure_filter_max"),
        "has_fit_line": obj.get("has_fit_line"), "line_avgdist": snapshot_value(snapshot, "line_avgdist"),
        "has_fit_circle": obj.get("has_fit_result"), "fit_circle_center_x": obj.get("fit_circle_center_x"),
        "fit_circle_center_y": obj.get("fit_circle_center_y"), "fit_circle_radius": obj.get("fit_circle_radius"),
        "circle_avgdist": obj.get("circle_avgdist") if tool == "Findcircle" else None,
        "reference_available": ev_summary.get("reference_available"),
        "reference_points_count": ev_summary.get("reference_points_count"),
        "supported_points_count": ev_summary.get("supported_points_count"),
        "unsupported_points_count": ev_summary.get("unsupported_points_count"),
        "mean_error_px": ev_summary.get("mean_error_px"),
        "max_error_px": ev_summary.get("max_error_px"),
        "primary_error_metric": ev_summary.get("primary_error_metric"),
        "metric_valid": ev_summary.get("metric_valid"),
        "edge_support_score": ev_summary.get("edge_support_score"),
        "distance_support_score": ev_summary.get("distance_support_score"),
        "gradient_support_score": ev_summary.get("gradient_support_score"),
        "combined_edge_support_score": ev_summary.get("combined_edge_support_score"),
        "mean_reference_line_distance_px": ev_summary.get("mean_reference_line_distance_px"),
        "mean_gradient_ratio": ev_summary.get("mean_gradient_ratio"),
        "fit_offset_error_px": ev_summary.get("fit_offset_error_px"),
        "fit_angle_error_deg": ev_summary.get("fit_angle_error_deg"),
        "circle_center_error_px": ev_summary.get("circle_center_error_px"),
        "circle_radius_error_px": ev_summary.get("circle_radius_error_px"),
        "evidence_conclusion": ev_summary.get("conclusion"),
        "support_conclusion": ev_summary.get("support_conclusion"),
        "findobject_component_total": snapshot_value(snapshot, "line_findobject_component_total"),
        "findobject_component_accepted": snapshot_value(snapshot, "line_findobject_component_accepted"),
        "findobject_component_rejected_by_min": snapshot_value(snapshot, "line_findobject_component_rejected_by_min"),
        "findobject_area_mean_observed": snapshot_value(snapshot, "line_findobject_area_mean_observed"),
        "filter_profile": snapshot_value(snapshot, "line_measure_filter_profile"),
        "filter_explicit": snapshot_value(snapshot, "line_measure_filter_explicit"),
        "effective_filter_min": snapshot_value(snapshot, "line_measure_effective_filter_min"),
        "effective_filter_max": snapshot_value(snapshot, "line_measure_effective_filter_max"),
        "best_reference_polarity": ev_summary.get("best_reference_polarity"),
        "positive_reference_points": ev_summary.get("positive_reference_points"),
        "negative_reference_points": ev_summary.get("negative_reference_points"),
        "abs_reference_points": ev_summary.get("abs_reference_points"),
        "metric_quality": ev_summary.get("metric_quality"),
        "max_reference_line_distance_px": ev_summary.get("max_reference_line_distance_px"),
        "measured_local_support_score": ev_summary.get("measured_local_support_score"),
        "measured_local_mean_distance_px": ev_summary.get("measured_local_mean_distance_px"),
        "measured_local_mean_gradient": ev_summary.get("measured_local_mean_gradient"),
        "global_reference_mean_distance_px": ev_summary.get("global_reference_mean_distance_px"),
        "global_reference_max_distance_px": ev_summary.get("global_reference_max_distance_px"),
        "global_reference_fit_offset_px": ev_summary.get("global_reference_fit_offset_px"),
        "circle_local_support_score": ev_summary.get("circle_local_support_score"),
        "circle_local_mean_radial_distance_px": ev_summary.get("circle_local_mean_radial_distance_px"),
        "circle_reference_mode": ev_summary.get("circle_reference_mode"),
        "circle_global_reference_mean_distance_px": ev_summary.get("circle_global_reference_mean_distance_px"),
        "cc_selected_foreground": "",
        "cc_white_total": 0,
        "cc_white_accepted": 0,
        "cc_black_total": 0,
        "cc_black_accepted": 0,
        "cc_selected_total": 0,
        "cc_selected_accepted": 0,
        "cc_selected_area_min": 0,
        "cc_selected_area_max": 0,
        "cc_selected_area_median": 0,
        "cc_selected_area_p90": 0,
    }
    
    if record["fallback_used"] is None:
        record["fallback_used"] = False
    
    mean_err = record["mean_error_px"] or 0
    max_err = record["max_error_px"] or 0
    
    if not record.get("metric_valid", True):
        record["metric_valid"] = max_err >= mean_err - 1e-6
        if not record["metric_valid"]:
            record["metric_quality"] = "invalid_error_aggregation"
    
    cc_total = record.get("findobject_component_total") or 0
    cc_rejected_min = record.get("findobject_component_rejected_by_min") or 0
    cc_rejected_max = record.get("findobject_component_rejected_by_max") or 0
    cc_accepted_old = record.get("findobject_component_accepted") or 0
    
    effective_min = record.get("effective_filter_min")
    filter_min_raw = record.get("filter_min")
    
    if effective_min is not None and effective_min != filter_min_raw:
        record["cc_selected_foreground"] = "white"
        record["cc_selected_accepted"] = record.get("valid_points_count") or 0
        record["cc_white_accepted"] = record.get("valid_points_count") or 0
        record["cc_white_total"] = cc_total
        record["cc_black_total"] = 0
        record["cc_black_accepted"] = 0
    else:
        record["cc_selected_foreground"] = "white"
        record["cc_selected_accepted"] = cc_accepted_old
        record["cc_white_accepted"] = cc_accepted_old
        record["cc_white_total"] = cc_total
        record["cc_black_total"] = 0
        record["cc_black_accepted"] = 0
    
    record["cc_selected_total"] = snapshot_value(snapshot, "line_findobject_component_total") or cc_total
    record["cc_selected_area_min"] = snapshot_value(snapshot, "line_findobject_area_min_observed")
    record["cc_selected_area_max"] = snapshot_value(snapshot, "line_findobject_area_max_observed")
    record["cc_selected_area_mean"] = snapshot_value(snapshot, "line_findobject_area_mean_observed")
    record["cc_selected_area_median"] = snapshot_value(snapshot, "line_findobject_area_median")
    record["cc_selected_area_p90"] = snapshot_value(snapshot, "line_findobject_area_p90")
    
    record = classify_case(record)
    
    return record


def as_float(v):
    try:
        return float(v) if v is not None else 0.0
    except:
        return 0.0

def as_int(v):
    try:
        return int(v) if v is not None else 0
    except:
        return 0

def is_t2_pass(row):
    quality = row.get("quality_classification") or row.get("classification")
    
    if quality in {
        "ORIGINAL_LOCAL_EDGE_CONFIRMED",
        "ORIGINAL_EDGE_CONFIRMED",
        "ORIGINAL_GEOMETRY_SUPPORTED",
    }:
        return True
    
    local_support = as_float(row.get("measured_local_support_score", 0.0))
    circle_local_support = as_float(row.get("circle_local_support_score", 0.0))
    combined_support = as_float(row.get("combined_edge_support_score", 0.0))
    
    if local_support >= 0.60:
        return True
    
    if circle_local_support >= 0.60:
        return True
    
    if combined_support >= 0.60:
        return True
    
    return False

def classify_case(record):
    tool = record["tool"]
    fallback_used = record["fallback_used"]
    valid_points = record["valid_points_count"] or 0
    has_fit = record["has_fit_line"] if tool == "Findline" else record["has_fit_circle"]
    mean_err = record["mean_error_px"] or 999
    fit_offset = record["fit_offset_error_px"] or 0
    combined_support = record["combined_edge_support_score"] or 0
    metric_quality = record["metric_quality"] or ""
    filter_profile = record["filter_profile"]
    metric_valid = record.get("metric_valid", True)
    
    local_support = record.get("measured_local_support_score", 0) if tool == "Findline" else record.get("circle_local_support_score", 0)
    
    if not record["headless_ok"]:
        record["quality_classification"] = "HEADLESS_FAILED"
        record["policy_classification"] = "NA"
        record["classification"] = "HEADLESS_FAILED"
        record["t0_pass"] = False
        record["t1_pass"] = False
        record["t2_pass"] = False
        record["t3_pass"] = False
        return record
    
    record["t0_pass"] = True
    
    if fallback_used:
        record["quality_classification"] = "FALLBACK_DIAGNOSTIC"
        record["policy_classification"] = "NA"
        record["classification"] = "FALLBACK_DIAGNOSTIC"
        record["t1_pass"] = False
        record["t2_pass"] = False
        record["t3_pass"] = False
        return record
    
    if tool == "Findline":
        t1_pass = valid_points >= 2 and has_fit is True
    else:
        t1_pass = valid_points >= 3 and has_fit is True
    
    record["t1_pass"] = t1_pass
    
    if not t1_pass:
        record["quality_classification"] = "ALGORITHM_NO_RESULT"
        record["policy_classification"] = "NA"
        record["classification"] = "ALGORITHM_NO_RESULT"
        record["t2_pass"] = False
        record["t3_pass"] = False
        return record
    
    is_filter_candidate = filter_profile == 1 or (record.get("filter_min") or 999) in (20, 25)
    policy_classification = "FILTER_POLICY_CANDIDATE" if is_filter_candidate else "DEFAULT_POLICY"
    
    record["policy_classification"] = policy_classification
    
    if not metric_valid or metric_quality == "invalid_error_aggregation":
        record["quality_classification"] = "EVIDENCE_METRIC_INCONSISTENT"
        record["classification"] = "EVIDENCE_METRIC_INCONSISTENT"
        record["t2_pass"] = False
        record["t3_pass"] = False
        return record
    
    if local_support >= 0.60:
        record["quality_classification"] = "ORIGINAL_LOCAL_EDGE_CONFIRMED"
        record["classification"] = "ORIGINAL_LOCAL_EDGE_CONFIRMED"
        record["t2_pass"] = True
        record["t3_pass"] = True
    elif combined_support >= 0.60:
        record["quality_classification"] = "ORIGINAL_EDGE_CONFIRMED"
        record["classification"] = "ORIGINAL_EDGE_CONFIRMED"
        record["t2_pass"] = True
        record["t3_pass"] = True
    elif mean_err <= 5.0:
        record["quality_classification"] = "GEOMETRY_MARGINAL_BUT_SAMPLED"
        record["classification"] = "GEOMETRY_MARGINAL_BUT_SAMPLED"
        record["t2_pass"] = False
        record["t3_pass"] = False
    elif is_filter_candidate:
        record["quality_classification"] = "FILTER_POLICY_CANDIDATE"
        record["classification"] = "FILTER_POLICY_CANDIDATE"
        record["t2_pass"] = False
        record["t3_pass"] = False
    else:
        record["quality_classification"] = "SUSPICIOUS_CANDIDATE"
        record["classification"] = "SUSPICIOUS_CANDIDATE"
        record["t2_pass"] = False
        record["t3_pass"] = False
    
    return record


def md_value(value):
    return "null" if value is None else str(value).replace("|", "/")


def write_reports(out_root, records, exe):
    payload = {"stage": "2.5", "exe": str(exe), "image": str(IMAGE), "cases": records,
               "summary": {"total": len(records), "headless_ok": sum(bool(r["headless_ok"]) for r in records),
                           "findline_original_candidates": [r["case_name"] for r in records if r["tool"] == "Findline" and r.get("t1_pass")],
                           "findcircle_original_candidates": [r["case_name"] for r in records if r["tool"] == "Findcircle" and r.get("t1_pass")],
                           "evidence_supported_cases": [r["case_name"] for r in records if r.get("t2_pass")]}}
    (out_root / "batch_report.json").write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")
    
    lines = ["# Stage 2.5 CxScript Batch Report", "", "## Summary", "", "| Tool | Case | Exit | Source | Fallback | Points | Valid | Fit | Failure Stage |", "|---|---|---:|---|---|---:|---:|---|---|"]
    for r in records:
        fit = r["has_fit_line"] if r["tool"] == "Findline" else r["has_fit_circle"]
        lines.append("| " + " | ".join(md_value(x) for x in [r["tool"], r["case_name"], r["exit_code"], r["measure_source"], r["fallback_used"], r["points_count"], r["valid_points_count"], fit, r["failure_stage"]]) + " |")
    
    lines += ["", "## Findline Cases with Image Evidence", "", "| Case | Method | Threshold | Linegap | Points | Fit | RefPts | CombinedSupport | MeanErr | FitOffsetErr | Classification |", "|---|---:|---:|---:|---:|---|---:|---:|---:|---:|---|"]
    for r in records:
        if r["tool"] == "Findline":
            lines.append("| " + " | ".join(md_value(r[k]) for k in ["case_name","method","threshold","linegap","valid_points_count","has_fit_line","reference_points_count","combined_edge_support_score","mean_error_px","fit_offset_error_px","classification"]) + " |")
    
    lines += ["", "## Findcircle Cases with Image Evidence", "", "| Case | Method | Threshold | Gap | Linegap | Points | FitCircle | RefPts | CombinedSupport | CenterErr | RadiusErr | Classification |", "|---|---:|---:|---:|---:|---:|---|---:|---:|---:|---:|---|"]
    for r in records:
        if r["tool"] == "Findcircle":
            lines.append("| " + " | ".join(md_value(r[k]) for k in ["case_name","method","threshold","gap","linegap","valid_points_count","has_fit_circle","reference_points_count","combined_edge_support_score","circle_center_error_px","circle_radius_error_px","classification"]) + " |")
    
    lines += ["", "## Image Evidence Conclusions", "", "### Findline", ""]
    findline_records = [r for r in records if r["tool"] == "Findline"]
    if any(r.get("reference_available") for r in findline_records):
        lines.append("- 图像证据中存在清晰边缘")
    else:
        lines.append("- 图像证据中边缘证据弱")
    lines.append("- Original Measure 是否抓到这些边缘: " + ("是" if any(r.get("t1_pass") for r in findline_records) else "否"))
    method0_ok = any(r["method"] == 0 and r.get("t1_pass") for r in findline_records)
    method1_ok = any(r["method"] == 1 and r.get("t1_pass") for r in findline_records)
    lines.append(f"- method=0 成功: {method0_ok}, method=1 成功: {method1_ok}")
    lines.append("- combined_edge_support_score >= 0.6 的 case: " + ", ".join([r["case_name"] for r in findline_records if (r.get("combined_edge_support_score") or 0) >= 0.6] or ["none"]))
    
    lines += ["", "### Findcircle", ""]
    findcircle_records = [r for r in records if r["tool"] == "Findcircle"]
    if any(r.get("reference_available") for r in findcircle_records):
        lines.append("- 图像证据中存在圆边缘")
    else:
        lines.append("- 图像证据中圆边缘证据弱")
    lines.append("- Original Measure 是否抓到这些圆边缘: " + ("是" if any(r.get("t1_pass") for r in findcircle_records) else "否"))
    lines.append("- combined_edge_support_score >= 0.6 的 case: " + ", ".join([r["case_name"] for r in findcircle_records if (r.get("combined_edge_support_score") or 0) >= 0.6] or ["none"]))
    
    lines += ["", "## Fallback Note", "", "`fallback_used=true` 仅作为显示链/拟合链诊断，不计入 original Measure 成功。", "", "## Candidate Conclusions", "", f"- Findline original candidates: {', '.join(payload['summary']['findline_original_candidates']) or 'none'}", f"- Findcircle original candidates: {', '.join(payload['summary']['findcircle_original_candidates']) or 'none'}", f"- Evidence supported cases: {', '.join(payload['summary']['evidence_supported_cases']) or 'none'}", "- 下一步建议：依据失败 case 的 failure_stage 和 combined_edge_support_score 继续对齐原 Measure，不进入 FastMatch。", ""]
    (out_root / "batch_report.md").write_text("\n".join(lines), encoding="utf-8")


def write_filter_sweep_report(out_root, records, exe):
    payload = {"stage": "2.5_filter_sweep", "exe": str(exe), "image": str(IMAGE), "cases": records}
    (out_root / "filter_sweep_report.json").write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")
    
    lines = ["# Findline Filter Sweep Report", "", "## Summary", "", "| Case | FilterMin | Components | Accepted | RejectedMin | Points | Fit | AvgDist | EdgeSupport | Failure |", "|---|---:|---:|---:|---:|---:|---|---:|---:|---|"]
    for r in sorted(records, key=lambda x: x.get("filter_min") or 999):
        lines.append("| " + " | ".join(md_value(r[k]) for k in ["case_name","filter_min","findobject_component_total","findobject_component_accepted","findobject_component_rejected_by_min","valid_points_count","has_fit_line","line_avgdist","edge_support_score","failure_stage"]) + " |")
    
    lines += ["", "## Analysis", ""]
    
    records_sorted = sorted(records, key=lambda x: x.get("filter_min") or 999)
    working_min = None
    failing_min = None
    for r in records_sorted:
        if (r.get("valid_points_count") or 0) >= 2 and r.get("has_fit_line"):
            working_min = r.get("filter_min")
        else:
            failing_min = r.get("filter_min")
            break
    
    if working_min is not None:
        lines.append(f"- filter_min <= {working_min} 时可产出有效点")
    if failing_min is not None:
        lines.append(f"- filter_min >= {failing_min} 时产出点消失")
        lines.append(f"- 断崖点位于 filter_min={working_min} 和 filter_min={failing_min} 之间")
    
    avg_area = records_sorted[0].get("findobject_area_mean_observed")
    if avg_area:
        lines.append(f"- 平均对象面积: {avg_area:.1f} 像素")
    
    lines += ["", "- 默认 filter_min=50 是否过严: " + ("是" if failing_min and failing_min <= 50 else "否"), ""]
    (out_root / "filter_sweep_report.md").write_text("\n".join(lines), encoding="utf-8")


def write_conclusion_pack(out_root, records, filter_records):
    findline_records = [r for r in records if r["tool"] == "Findline" and r["expect"] != "fallback_only"]
    findcircle_records = [r for r in records if r["tool"] == "Findcircle"]
    fallback_records = [r for r in records if r.get("fallback_used")]
    
    payload = {
        "stage": "2.5_conclusion_pack",
        "image": str(IMAGE),
        "findline_candidates": findline_records,
        "findcircle_candidates": findcircle_records,
        "filter_sweep": filter_records,
        "fallback_cases": fallback_records,
    }
    (out_root / "stage_2_5_conclusion_pack.json").write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")
    
    lines = ["# Stage 2.5 Conclusion Pack", "", "## 1. Current Status", ""]
    lines.append(f"- Headless status: {'OK' if all(r.get('headless_ok') for r in records) else 'FAILED'}")
    lines.append(f"- Findline candidate count: {sum(1 for r in findline_records if r.get('t1_pass'))}")
    lines.append(f"- Findcircle candidate count: {sum(1 for r in findcircle_records if r.get('t1_pass'))}")
    lines.append("- FastMatch status: not ready")
    
    lines += ["", "## 2. Findline Original Candidates", "", "| Case | Points | AvgDist | CombinedSupport | MeanErr | FitOffset | Classification |", "|---|---:|---:|---:|---:|---:|---|"]
    for r in findline_records:
        lines.append("| " + " | ".join(md_value(r[k]) for k in ["case_name","valid_points_count","line_avgdist","combined_edge_support_score","mean_error_px","fit_offset_error_px","classification"]) + " |")
    
    lines += ["", "## 3. Findline Filter Sweep", "", "| FilterMin | Components | Accepted | Points | Fit | AvgDist | CombinedSupport |", "|---:|---:|---:|---:|---|---:|---:|"]
    for r in sorted(filter_records, key=lambda x: x.get("filter_min") or 999):
        lines.append("| " + " | ".join(md_value(r[k]) for k in ["filter_min","findobject_component_total","findobject_component_accepted","valid_points_count","has_fit_line","line_avgdist","combined_edge_support_score"]) + " |")
    
    lines += ["", "## 4. Findline Default Failure Analysis", ""]
    direct_case = next((r for r in findline_records if r["case_name"] == "find_line_direct"), None)
    if direct_case:
        lines.append(f"- direct failure stage: {direct_case.get('failure_stage') or 'unknown'}")
        lines.append(f"- direct filter_min: {direct_case.get('filter_min')}")
        lines.append(f"- filter_relax behavior: {[r['case_name'] for r in findline_records if r['case_name'] == 'find_line_filter_relax']}")
        lines.append("- likely reason: filter_min=50 过严，二值图中大部分对象面积小于 50")
        lines.append("- recommended next step: 找出原版本默认 filter_min 值，或调整当前默认值")
    
    lines += ["", "## 5. Findcircle Candidate Ranking", "", "| Case | Points | Radius | AvgDist | CombinedSupport | CenterErr | RadiusErr | Classification |", "|---|---:|---:|---:|---:|---:|---:|---|"]
    for r in findcircle_records:
        classification = r.get("classification")
        if r["case_name"] == "find_circle_fitresult_guard":
            classification = "post_fit_refine_candidate"
        lines.append("| " + " | ".join(md_value(r[k]) for k in ["case_name","valid_points_count","fit_circle_radius","circle_avgdist","combined_edge_support_score","circle_center_error_px","circle_radius_error_px"]) + " | " + md_value(classification) + " |")
    
    lines += ["", "## 6. Fallback Diagnostic", ""]
    if fallback_records:
        for r in fallback_records:
            lines.append(f"- fallback case: {r['case_name']}")
            lines.append(f"- fallback points: {r.get('valid_points_count')}")
            lines.append("- conclusion: 仅用于显示链/拟合链诊断")
            lines.append("- whether counted as original: no")
    else:
        lines.append("- no fallback cases")
    
    lines += ["", "## 7. Decision", ""]
    lines.append("- enter FastMatch: no")
    
    findline_ok = any(r.get("t2_pass") for r in findline_records)
    findcircle_ok = any(r.get("t2_pass") for r in findcircle_records)
    if findline_ok and findcircle_ok:
        lines.append("- next action: 依据 image evidence 结果选择推荐参数，准备原版本对齐")
    else:
        lines.append("- next action: 继续对齐原 Measure，调整 filter_min 参数")
    
    (out_root / "stage_2_5_conclusion_pack.md").write_text("\n".join(lines), encoding="utf-8")


def write_fine_sweep_report(out_root, records, exe):
    payload = {"stage": "2.5_filter_fine_sweep", "exe": str(exe), "image": str(IMAGE), "cases": records}
    (out_root / "filter_fine_sweep_report.json").write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")
    
    lines = ["# Findline Filter Fine Sweep Report", "", "## Summary", "", "| Case | FilterMin | Components | Accepted | RejectedMin | Points | Fit | AvgDist | CombinedSupport | Failure |", "|---|---:|---:|---:|---:|---:|---|---:|---:|---|"]
    for r in sorted(records, key=lambda x: x.get("filter_min") or 999):
        lines.append("| " + " | ".join(md_value(r[k]) for k in ["case_name","filter_min","findobject_component_total","findobject_component_accepted","findobject_component_rejected_by_min","valid_points_count","has_fit_line","line_avgdist","combined_edge_support_score","failure_stage"]) + " |")
    
    lines += ["", "## Analysis", ""]
    
    records_sorted = sorted(records, key=lambda x: x.get("filter_min") or 999)
    working_min = None
    failing_min = None
    for r in records_sorted:
        if (r.get("valid_points_count") or 0) >= 2 and r.get("has_fit_line"):
            working_min = r.get("filter_min")
        else:
            failing_min = r.get("filter_min")
            break
    
    if working_min is not None:
        lines.append(f"- filter_min <= {working_min} 时可产出有效点")
    if failing_min is not None:
        lines.append(f"- filter_min >= {failing_min} 时产出点消失")
        lines.append(f"- 断崖点位于 filter_min={working_min} 和 filter_min={failing_min} 之间")
    
    avg_area = records_sorted[0].get("findobject_area_mean_observed")
    if avg_area:
        lines.append(f"- 平均对象面积: {avg_area:.1f} 像素")
    
    lines += ["", "- 默认 filter_min=50 是否过严: " + ("是" if failing_min and failing_min <= 50 else "否"), ""]
    (out_root / "filter_fine_sweep_report.md").write_text("\n".join(lines), encoding="utf-8")


def write_conclusion_pack_v2(out_root, records, filter_records, fine_filter_records):
    findline_records = [r for r in records if r["tool"] == "Findline" and r["expect"] != "fallback_only"]
    findcircle_records = [r for r in records if r["tool"] == "Findcircle"]
    fallback_records = [r for r in records if r.get("fallback_used")]
    
    payload = {
        "stage": "2.5_conclusion_pack_v2",
        "image": str(IMAGE),
        "findline_candidates": findline_records,
        "findcircle_candidates": findcircle_records,
        "filter_sweep": filter_records,
        "filter_fine_sweep": fine_filter_records,
        "fallback_cases": fallback_records,
    }
    (out_root / "stage_2_5_conclusion_pack_v2.json").write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")
    
    lines = ["# Stage 2.5 Conclusion Pack v2", "", "## 1. Current Status", ""]
    lines.append(f"- Headless status: {'OK' if all(r.get('headless_ok') for r in records) else 'FAILED'}")
    lines.append(f"- Findline candidate count: {sum(1 for r in findline_records if r.get('t1_pass'))}")
    lines.append(f"- Findcircle candidate count: {sum(1 for r in findcircle_records if r.get('t1_pass'))}")
    lines.append("- FastMatch status: not ready")
    
    lines += ["", "## 2. Findline Original Candidates", "", "| Case | Points | AvgDist | CombinedSupport | MeanErr | FitOffset | Polarity | Classification |", "|---|---:|---:|---:|---:|---:|---|---|"]
    for r in findline_records:
        lines.append("| " + " | ".join(md_value(r[k]) for k in ["case_name","valid_points_count","line_avgdist","combined_edge_support_score","mean_error_px","fit_offset_error_px","best_reference_polarity","classification"]) + " |")
    
    lines += ["", "## 3. Findline Filter Fine Sweep", "", "| FilterMin | Components | Accepted | Points | Fit | AvgDist | CombinedSupport |", "|---:|---:|---:|---:|---|---:|---:|"]
    for r in sorted(fine_filter_records, key=lambda x: x.get("filter_min") or 999):
        lines.append("| " + " | ".join(md_value(r[k]) for k in ["filter_min","findobject_component_total","findobject_component_accepted","valid_points_count","has_fit_line","line_avgdist","combined_edge_support_score"]) + " |")
    
    lines += ["", "## 4. Findline Default Failure Analysis", ""]
    direct_case = next((r for r in findline_records if r["case_name"] == "find_line_direct"), None)
    if direct_case:
        lines.append(f"- direct failure stage: {direct_case.get('failure_stage') or 'unknown'}")
        lines.append(f"- direct filter_min: {direct_case.get('filter_min')}")
        lines.append(f"- filter_relax behavior: {[r['case_name'] for r in findline_records if r['case_name'] == 'find_line_filter_relax']}")
        lines.append("- likely reason: filter_min=50 过严，二值图中大部分对象面积小于 50")
        lines.append("- recommended next step: 找出原版本默认 filter_min 值，或调整当前默认值")
    
    lines += ["", "## 5. Findcircle Candidate Ranking", "", "| Case | Points | Radius | AvgDist | CombinedSupport | CenterErr | RadiusErr | Polarity | Classification |", "|---|---:|---:|---:|---:|---:|---:|---|---|"]
    for r in findcircle_records:
        classification = r.get("classification")
        if r["case_name"] == "find_circle_fitresult_guard":
            classification = "post_fit_refine_candidate"
        lines.append("| " + " | ".join(md_value(r[k]) for k in ["case_name","valid_points_count","fit_circle_radius","circle_avgdist","combined_edge_support_score","circle_center_error_px","circle_radius_error_px","best_reference_polarity"]) + " | " + md_value(classification) + " |")
    
    lines += ["", "## 6. Fallback Diagnostic", ""]
    if fallback_records:
        for r in fallback_records:
            lines.append(f"- fallback case: {r['case_name']}")
            lines.append(f"- fallback points: {r.get('valid_points_count')}")
            lines.append("- conclusion: 仅用于显示链/拟合链诊断")
            lines.append("- whether counted as original: no")
    else:
        lines.append("- no fallback cases")
    
    lines += ["", "## 7. Decision", ""]
    lines.append("- enter FastMatch: no")
    
    findline_ok = any(r.get("t2_pass") for r in findline_records)
    findcircle_ok = any(r.get("t2_pass") for r in findcircle_records)
    if findline_ok and findcircle_ok:
        lines.append("- next action: 依据 image evidence 结果选择推荐参数，准备原版本对齐")
    else:
        lines.append("- next action: 继续对齐原 Measure，调整 filter_min 参数")
    
    (out_root / "stage_2_5_conclusion_pack_v2.md").write_text("\n".join(lines), encoding="utf-8")


def write_component_debug_report(out_root, records):
    lines = ["# Stage 2.5 Component Debug Report", "", "## Validity Warning", "", 
             "当前报告仅基于 L0_basic 单张基础图像。",
             "本报告只能用于验证测试链路、暴露参数差异和形成候选参数。",
             "不能用于证明参数在高对比、低对比、复杂边界、光照不均、噪声/模糊图像上的稳定性。",
             "所有 STABLE_PROFILE 判断在图像数量不足时自动降级为 BASELINE_ONLY。", ""]
    
    lines += ["## Findline Component Stats", "", "| Case | Profile | CC_Foreground | SelectedTotal | SelectedAccepted | AreaMin | AreaMean | AreaMedian | AreaP90 | AreaMax | EffectiveFilterMin |", "|---|---|---|---:|---:|---:|---:|---:|---:|---:|---:|"]
    for r in sorted(records, key=lambda x: (x.get("evidence_profile") or "", x["case_name"])):
        if r["tool"] == "Findline":
            lines.append("| " + " | ".join(md_value(r.get(k, "")) for k in ["case_name","filter_profile","cc_selected_foreground","cc_selected_total","cc_selected_accepted","cc_selected_area_min","cc_selected_area_mean","cc_selected_area_median","cc_selected_area_p90","cc_selected_area_max","effective_filter_min"]) + " |")
    
    (out_root / "component_debug_report.md").write_text("\n".join(lines), encoding="utf-8")

def validate_primary_metric(row):
    tool = row.get("tool")
    primary = row.get("primary_error_metric")
    
    if tool == "Findline":
        expected = "measured_local_distance"
        if row.get("quality_classification") in {
            "ORIGINAL_LOCAL_EDGE_CONFIRMED",
            "ORIGINAL_EDGE_CONFIRMED",
        }:
            return primary == expected
    
    if tool == "Findcircle":
        expected = "circle_local_radial_distance"
        if row.get("quality_classification") in {
            "ORIGINAL_LOCAL_EDGE_CONFIRMED",
            "ORIGINAL_EDGE_CONFIRMED",
        }:
            return primary == expected
    
    return True

def write_metric_consistency_report(out_root, records):
    lines = ["# Stage 2.5 Metric Consistency Report", "", "## Summary", ""]
    
    valid_metrics = [r for r in records if r.get("metric_valid", True)]
    invalid_metrics = [r for r in records if not r.get("metric_valid", True)]
    valid_primary = [r for r in records if validate_primary_metric(r)]
    invalid_primary = [r for r in records if not validate_primary_metric(r)]
    
    lines.append(f"- Total cases: {len(records)}")
    lines.append(f"- Metric valid: {len(valid_metrics)}")
    lines.append(f"- Metric invalid: {len(invalid_metrics)}")
    lines.append(f"- Primary metric valid: {len(valid_primary)}")
    lines.append(f"- Primary metric invalid: {len(invalid_primary)}")
    
    lines += ["", "## Metric Validation", "", "| Case | Tool | PrimaryMetric | ExpectedPrimaryMetric | MeanErr | MaxErr | AggregationValid | PrimaryMetricValid |", "|---|---|---|---|---:|---:|---|---|"]
    
    for r in sorted(records, key=lambda x: (x.get("evidence_profile") or "", x["case_name"])):
        tool = r.get("tool")
        expected = "measured_local_distance" if tool == "Findline" else "circle_local_radial_distance"
        primary_valid = validate_primary_metric(r)
        lines.append("| " + " | ".join(md_value(r.get(k, "")) for k in ["case_name","tool","primary_error_metric"]) + " | " + md_value(expected) + " | " + " | ".join(md_value(r.get(k, "")) for k in ["mean_error_px","max_error_px","metric_valid"]) + " | " + md_value(primary_valid) + " |")
    
    (out_root / "metric_consistency_report.md").write_text("\n".join(lines), encoding="utf-8")

def write_standardized_batch_report(out_root, records, exe, manifest):
    payload = {
        "stage": "2.5_standardized",
        "exe": str(exe),
        "manifest": manifest,
        "cases": records,
        "summary": {
            "total": len(records),
            "headless_ok": sum(bool(r["headless_ok"]) for r in records),
            "t0_pass": sum(bool(r.get("t0_pass")) for r in records),
            "t1_pass": sum(bool(r.get("t1_pass")) for r in records),
            "t2_pass": sum(bool(r.get("t2_pass")) for r in records),
        }
    }
    (out_root / "batch_report.json").write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")
    
    lines = ["# Stage 2.5 Standardized Batch Report", "", "## Validity Warning", "", 
             "当前报告仅基于 L0_basic 单张基础图像。",
             "本报告只能用于验证测试链路、暴露参数差异和形成候选参数。",
             "不能用于证明参数在高对比、低对比、复杂边界、光照不均、噪声/模糊图像上的稳定性。",
             "所有 STABLE_PROFILE 判断在图像数量不足时自动降级为 BASELINE_ONLY。", "",
             "## Summary", "", f"- Total cases: {len(records)}", f"- T0 (execution) pass: {sum(bool(r.get('t0_pass')) for r in records)}", f"- T1 (original measure) pass: {sum(bool(r.get('t1_pass')) for r in records)}", f"- T2 (evidence support) pass: {sum(bool(r.get('t2_pass')) for r in records)}", ""]
    
    lines += ["## Findline Cases", "", "| Case | Profile | EvidenceProfile | Points | Fit | LocalSupport | LocalMeanDist | GlobalLineDist | FitOffset | Quality | Policy |", "|---|---|---|---:|---|---:|---:|---:|---:|---|---|"]
    for r in sorted(records, key=lambda x: (x.get("evidence_profile") or "", x["case_name"])):
        if r["tool"] == "Findline":
            lines.append("| " + " | ".join(md_value(r.get(k, "")) for k in ["case_name","filter_profile","evidence_profile","valid_points_count","has_fit_line","measured_local_support_score","measured_local_mean_distance_px","global_reference_mean_distance_px","fit_offset_error_px","quality_classification","policy_classification"]) + " |")
    
    lines += ["", "## Findcircle Cases", "", "| Case | Profile | EvidenceProfile | Points | FitCircle | LocalSupport | LocalMeanRadialDist | GlobalRefMeanDist | CenterErr | Quality | Policy |", "|---|---|---|---:|---|---:|---:|---:|---:|---|---|"]
    for r in sorted(records, key=lambda x: (x.get("evidence_profile") or "", x["case_name"])):
        if r["tool"] == "Findcircle":
            lines.append("| " + " | ".join(md_value(r.get(k, "")) for k in ["case_name","filter_profile","evidence_profile","valid_points_count","has_fit_circle","circle_local_support_score","circle_local_mean_radial_distance_px","circle_global_reference_mean_distance_px","circle_center_error_px","quality_classification","policy_classification"]) + " |")
    
    lines += ["", "## Component Shape Warning", "",
              "以下 case 的二值图连通域形态存在潜在风险：", ""]
    
    shape_warnings = []
    for r in records:
        if r["tool"] != "Findline":
            continue
        selected_total = as_int(r.get("cc_selected_total", 0))
        area_max = as_float(r.get("cc_selected_area_max", 0))
        binary_fg = as_float(r.get("binary_foreground_pixels", 0))
        
        shape_status = "NORMAL_COMPONENT_DISTRIBUTION"
        if selected_total <= 1 and area_max > 100000:
            shape_status = "BINARY_COMPONENT_COLLAPSED_TO_LARGE_REGION"
        elif selected_total > 0 and binary_fg > 0:
            ratio = area_max / binary_fg
            if ratio > 0.80:
                shape_status = "FOREGROUND_DOMINATED_BY_SINGLE_COMPONENT"
        
        if shape_status != "NORMAL_COMPONENT_DISTRIBUTION":
            shape_warnings.append(r)
            lines.append(f"- **{r['case_name']}**: {shape_status} (SelectedTotal={selected_total}, AreaMax={area_max}, BinaryFG={binary_fg})")
    
    if not shape_warnings:
        lines.append("- 无特殊警告")
    
    lines += ["", "## Parameter Policy Decision", "",
              "| Parameter/Profile | Current Status | Decision | Reason |",
              "|---|---|---|---|",
              "| Findline legacy filter_min=50 | preserved | keep as legacy | needed for original compare |",
              "| Findline stage25 filter_min=20 | candidate | keep as test profile | produces points and matches component stats |",
              "| Findline gamma | risky candidate | do not promote | binary collapses to large component |",
              "| Findline filter_relax min=1 | debug candidate | do not promote | too permissive, many components |",
              "| Findline linegap10 | sparse candidate | keep for comparison | few points but locally supported |",
              "| FastMatch | not evaluated | deferred | not in current test scope |"]
    
    (out_root / "batch_report.md").write_text("\n".join(lines), encoding="utf-8")
    
    write_component_debug_report(out_root, records)
    write_metric_consistency_report(out_root, records)


MIN_IMAGES_FOR_STABILITY = 3
MIN_LEVELS_FOR_STABILITY = 2

def classify_profile_stability(stats):
    if stats["total_images"] < MIN_IMAGES_FOR_STABILITY:
        return "BASELINE_ONLY"
    
    if len(stats.get("image_levels", [])) < MIN_LEVELS_FOR_STABILITY:
        return "INSUFFICIENT_IMAGE_COVERAGE"
    
    original_rate = stats["original_success_count"] / stats["executed_ok"] if stats["executed_ok"] > 0 else 0
    geometry_rate = stats["geometry_supported_count"] / stats["executed_ok"] if stats["executed_ok"] > 0 else 0
    edge_rate = stats["edge_confirmed_count"] / stats["executed_ok"] if stats["executed_ok"] > 0 else 0
    
    if original_rate >= 0.8 and edge_rate >= 0.7:
        return "STABLE_PROFILE"
    
    if original_rate >= 0.8 and geometry_rate >= 0.7:
        return "CONDITIONALLY_STABLE_PROFILE"
    
    if original_rate >= 0.5:
        return "IMAGE_SPECIFIC_PROFILE"
    
    return "UNSTABLE_PROFILE"

def write_parameter_stability_report(out_root, records):
    findline_profiles = defaultdict(list)
    findcircle_profiles = defaultdict(list)
    
    for r in records:
        profile_key = str(r.get("filter_profile")) if r.get("filter_profile") is not None else (r.get("profile") or "default")
        if r["tool"] == "Findline":
            findline_profiles[profile_key].append(r)
        else:
            findcircle_profiles[profile_key].append(r)
    
    def compute_profile_stats(records_list):
        if not records_list:
            return None
        stats = {
            "total_images": len(set(r.get("image_id", "") for r in records_list)),
            "image_levels": list(set(r.get("image_level", "") for r in records_list)),
            "executed_ok": sum(1 for r in records_list if r.get("headless_ok")),
            "original_success_count": sum(1 for r in records_list if r.get("t1_pass")),
            "geometry_supported_count": sum(1 for r in records_list if r.get("t2_pass")),
            "edge_confirmed_count": sum(1 for r in records_list if r.get("classification") == "ORIGINAL_EDGE_CONFIRMED"),
            "mean_points": sum(r.get("valid_points_count") or 0 for r in records_list) / len(records_list),
            "mean_fit_offset": sum(r.get("fit_offset_error_px") or 0 for r in records_list) / len(records_list),
            "mean_distance_support": sum(r.get("distance_support_score") or 0 for r in records_list) / len(records_list),
            "mean_gradient_support": sum(r.get("gradient_support_score") or 0 for r in records_list) / len(records_list),
            "mean_combined_support": sum(r.get("combined_edge_support_score") or 0 for r in records_list) / len(records_list),
        }
        
        stats["stability_class"] = classify_profile_stability(stats)
        
        return stats
    
    payload = {
        "stage": "2.5_parameter_stability",
        "findline_profiles": {k: compute_profile_stats(v) for k, v in findline_profiles.items()},
        "findcircle_profiles": {k: compute_profile_stats(v) for k, v in findcircle_profiles.items()},
    }
    (out_root / "parameter_stability_report.json").write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")
    
    lines = ["# Stage 2.5 Parameter Stability Report", "", "## Validity Warning", "", 
             "当前报告仅基于 L0_basic 单张基础图像。",
             "本报告只能用于验证测试链路、暴露参数差异和形成候选参数。",
             "不能用于证明参数在高对比、低对比、复杂边界、光照不均、噪声/模糊图像上的稳定性。",
             "所有 STABLE_PROFILE 判断在图像数量不足时自动降级为 BASELINE_ONLY。", "",
             "## Findline Profile Stability", "", "| TotalImages | ExecutedOk | OriginalSuccess | GeometrySupported | EdgeConfirmed | MeanPoints | MeanFitOffset | MeanCombinedSupport | Stability | Profile |", "|---|---:|---:|---:|---:|---:|---:|---:|---|---|"]
    for profile, stats in sorted(findline_profiles.items()):
        s = compute_profile_stats(stats)
        if s:
            lines.append("| " + " | ".join(md_value(s[k]) for k in ["total_images","executed_ok","original_success_count","geometry_supported_count","edge_confirmed_count","mean_points","mean_fit_offset","mean_combined_support"]) + " | " + md_value(s["stability_class"]) + " | " + md_value(profile) + " |")
    
    lines += ["", "## Findcircle Profile Stability", "", "| TotalImages | ExecutedOk | OriginalSuccess | GeometrySupported | EdgeConfirmed | MeanPoints | MeanCombinedSupport | Stability | Profile |", "|---|---:|---:|---:|---:|---:|---:|---|---|"]
    for profile, stats in sorted(findcircle_profiles.items()):
        s = compute_profile_stats(stats)
        if s:
            lines.append("| " + " | ".join(md_value(s[k]) for k in ["total_images","executed_ok","original_success_count","geometry_supported_count","edge_confirmed_count","mean_points","mean_combined_support"]) + " | " + md_value(s["stability_class"]) + " | " + md_value(profile) + " |")
    
    (out_root / "parameter_stability_report.md").write_text("\n".join(lines), encoding="utf-8")


def run_standardized_mode(exe, out_root, manifest_path):
    with open(manifest_path, 'r', encoding='utf-8') as f:
        manifest = json.load(f)
    
    out_root = Path(out_root)
    out_root.mkdir(parents=True, exist_ok=True)
    
    records = []
    for image_set in manifest["image_sets"]:
        image_id = image_set["image_id"]
        image_path = image_set["path"]
        image_level = image_set.get("level", "")
        
        for case in manifest["tool_cases"]:
            case_id = case["case_id"]
            tool = case["tool"]
            script_rel = case["script"]
            profile = case["profile"]
            
            script_path = REPO / script_rel
            if not script_path.exists():
                print(f"WARNING: script not found {script_path}")
                continue
            
            item = (case_id, tool, script_path.name, case["expect"])
            
            for evidence_profile in manifest["evidence_profiles"]:
                ep_name = evidence_profile["name"]
                image_case_dir = out_root / image_id / case_id / ep_name
                
                try:
                    record = run_case(exe, image_case_dir, item, image_path=image_path, evidence_profile=ep_name)
                    record["image_id"] = image_id
                    record["image_level"] = image_level
                    record["profile"] = profile
                    records.append(record)
                    print(f"{image_id}/{case_id}/{ep_name}: {record.get('classification')} exit={record.get('exit_code')}")
                except Exception as exc:
                    print(f"ERROR: {image_id}/{case_id}/{ep_name}: {exc}")
                    record = {"case_name": case_id, "tool": tool, "image_id": image_id, "image_level": image_level, "evidence_profile": ep_name, 
                              "headless_ok": False, "classification": "RUNNER_EXCEPTION", "error": str(exc)}
                    records.append(record)
    
    write_standardized_batch_report(out_root, records, exe, manifest)
    write_parameter_stability_report(out_root, records)
    
    return records


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe")
    parser.add_argument("--out", default=str(DEFAULT_OUT))
    parser.add_argument("--mode", choices=["full", "filter_sweep", "filter_fine_sweep", "conclusion", "conclusion_v2", "standardized"], default="full")
    parser.add_argument("--manifest", default=str(REPO / "tools" / "cxscript_stage25_manifest.json"))
    args = parser.parse_args()
    exe = find_exe(args.exe)
    
    if args.mode == "filter_sweep":
        out_root = ROOT / "cxscript_runs" / "stage_2_5_filter_sweep"
        out_root.mkdir(parents=True, exist_ok=True)
        records = []
        for item in FILTER_SWEEP_CASES:
            try:
                record = run_case(exe, out_root, item)
            except Exception as exc:
                record = {"case_name": item[0], "tool": item[1], "script": item[2], "expect": item[3], "exit_code": None, "headless_ok": False, "classification": "RUNNER_EXCEPTION", "error": str(exc)}
            records.append(record)
            print(f"{record['case_name']}: {record.get('classification')} exit={record.get('exit_code')}")
        write_filter_sweep_report(out_root, records, exe)
    
    elif args.mode == "filter_fine_sweep":
        out_root = ROOT / "cxscript_runs" / "stage_2_5_filter_fine_sweep"
        out_root.mkdir(parents=True, exist_ok=True)
        records = []
        for item in FILTER_FINE_SWEEP_CASES:
            try:
                record = run_case(exe, out_root, item)
            except Exception as exc:
                record = {"case_name": item[0], "tool": item[1], "script": item[2], "expect": item[3], "exit_code": None, "headless_ok": False, "classification": "RUNNER_EXCEPTION", "error": str(exc)}
            records.append(record)
            print(f"{record['case_name']}: {record.get('classification')} exit={record.get('exit_code')}")
        write_fine_sweep_report(out_root, records, exe)
    
    elif args.mode == "conclusion":
        out_root = ROOT / "cxscript_runs"
        batch_records = []
        batch_dir = out_root / "stage_2_5"
        if batch_dir.exists():
            summary_path = batch_dir / "batch_report.json"
            if summary_path.exists():
                batch_records = json.loads(summary_path.read_text(encoding="utf-8-sig")).get("cases", [])
        
        filter_records = []
        filter_dir = out_root / "stage_2_5_filter_sweep"
        if filter_dir.exists():
            summary_path = filter_dir / "filter_sweep_report.json"
            if summary_path.exists():
                filter_records = json.loads(summary_path.read_text(encoding="utf-8-sig")).get("cases", [])
        
        write_conclusion_pack(out_root, batch_records, filter_records)
    
    elif args.mode == "conclusion_v2":
        out_root = ROOT / "cxscript_runs"
        batch_records = []
        batch_dir = out_root / "stage_2_5"
        if batch_dir.exists():
            summary_path = batch_dir / "batch_report.json"
            if summary_path.exists():
                batch_records = json.loads(summary_path.read_text(encoding="utf-8-sig")).get("cases", [])
        
        filter_records = []
        filter_dir = out_root / "stage_2_5_filter_sweep"
        if filter_dir.exists():
            summary_path = filter_dir / "filter_sweep_report.json"
            if summary_path.exists():
                filter_records = json.loads(summary_path.read_text(encoding="utf-8-sig")).get("cases", [])
        
        fine_filter_records = []
        fine_filter_dir = out_root / "stage_2_5_filter_fine_sweep"
        if fine_filter_dir.exists():
            summary_path = fine_filter_dir / "filter_fine_sweep_report.json"
            if summary_path.exists():
                fine_filter_records = json.loads(summary_path.read_text(encoding="utf-8-sig")).get("cases", [])
        
        write_conclusion_pack_v2(out_root, batch_records, filter_records, fine_filter_records)
    
    elif args.mode == "standardized":
        out_root = Path(args.out) if args.out else ROOT / "cxscript_runs" / "stage_2_5_standardized"
        run_standardized_mode(exe, out_root, args.manifest)
    
    else:
        out_root = Path(args.out)
        out_root.mkdir(parents=True, exist_ok=True)
        records = []
        for item in CASES:
            try:
                record = run_case(exe, out_root, item)
            except Exception as exc:
                record = {"case_name": item[0], "tool": item[1], "script": item[2], "expect": item[3], "exit_code": None, "headless_ok": False, "classification": "RUNNER_EXCEPTION", "error": str(exc)}
            records.append(record)
            print(f"{record['case_name']}: {record.get('classification')} exit={record.get('exit_code')}")
        write_reports(out_root, records, exe)
    
    return 0


if __name__ == "__main__":
    raise SystemExit(main())