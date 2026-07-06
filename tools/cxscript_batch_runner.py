import argparse
import json
import re
import subprocess
from pathlib import Path

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


def run_case(exe, out_root, item):
    name, tool, filename, expect = item
    script = SCRIPT_DIR / filename
    case_dir = out_root / name
    case_dir.mkdir(parents=True, exist_ok=True)
    cmd = [str(exe), "--cxscript-headless", "--image", str(IMAGE),
           "--script", str(script), "--out", str(case_dir), "--case-name", name]
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
        "exit_code": proc.returncode, "headless_ok": summary_path.exists() and snapshot_path.exists() and overlay_path.exists(),
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
        "edge_support_score": ev_summary.get("edge_support_score"),
        "fit_offset_error_px": ev_summary.get("fit_offset_error_px"),
        "fit_angle_error_deg": ev_summary.get("fit_angle_error_deg"),
        "circle_center_error_px": ev_summary.get("circle_center_error_px"),
        "circle_radius_error_px": ev_summary.get("circle_radius_error_px"),
        "evidence_conclusion": ev_summary.get("conclusion"),
        "findobject_component_total": snapshot_value(snapshot, "line_findobject_component_total"),
        "findobject_component_accepted": snapshot_value(snapshot, "line_findobject_component_accepted"),
        "findobject_component_rejected_by_min": snapshot_value(snapshot, "line_findobject_component_rejected_by_min"),
        "findobject_area_mean_observed": snapshot_value(snapshot, "line_findobject_area_mean_observed"),
    }
    if record["fallback_used"] is None:
        record["fallback_used"] = False
    
    edge_support_ok = (record["edge_support_score"] or 0) >= 0.7
    error_ok = False
    if tool == "Findline":
        error_ok = (record["mean_error_px"] or 999) <= 2.0 and (record["fit_offset_error_px"] or 999) <= 2.0
        candidate = record["fallback_used"] is False and (record["valid_points_count"] or 0) >= 2 and record["has_fit_line"] is True
    else:
        error_ok = (record["circle_center_error_px"] or 999) <= 2.0 and (record["circle_radius_error_px"] or 999) <= 2.0
        candidate = record["fallback_used"] is False and (record["valid_points_count"] or 0) >= 3 and record["has_fit_circle"] is True
    
    record["evidence_supported"] = edge_support_ok and error_ok
    record["original_candidate"] = candidate
    
    if not record["headless_ok"]:
        record["classification"] = "HEADLESS_FAILED"
    elif record["fallback_used"]:
        record["classification"] = "FALLBACK_DIAGNOSTIC"
    elif candidate:
        valid = record["valid_points_count"] or 0
        support = record["edge_support_score"] or 0
        mean_err = record["mean_error_px"] or 999
        if valid >= 20 and support >= 0.75 and mean_err <= 2.0:
            record["classification"] = "STRONG_ORIGINAL_CANDIDATE"
        elif 2 <= valid < 20 and support >= 0.75 and mean_err <= 2.0:
            record["classification"] = "SPARSE_BUT_GOOD_CANDIDATE"
        elif support < 0.6:
            record["classification"] = "SUSPICIOUS_CANDIDATE"
        else:
            record["classification"] = "ORIGINAL_CANDIDATE"
    else:
        record["classification"] = "ALGORITHM_NO_RESULT"
    
    return record


def md_value(value):
    return "null" if value is None else str(value).replace("|", "/")


def write_reports(out_root, records, exe):
    payload = {"stage": "2.5", "exe": str(exe), "image": str(IMAGE), "cases": records,
               "summary": {"total": len(records), "headless_ok": sum(bool(r["headless_ok"]) for r in records),
                           "findline_original_candidates": [r["case_name"] for r in records if r["tool"] == "Findline" and r["original_candidate"]],
                           "findcircle_original_candidates": [r["case_name"] for r in records if r["tool"] == "Findcircle" and r["original_candidate"]],
                           "evidence_supported_cases": [r["case_name"] for r in records if r.get("evidence_supported")]}}
    (out_root / "batch_report.json").write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")
    
    lines = ["# Stage 2.5 CxScript Batch Report", "", "## Summary", "", "| Tool | Case | Exit | Source | Fallback | Points | Valid | Fit | Failure Stage |", "|---|---|---:|---|---|---:|---:|---|---|"]
    for r in records:
        fit = r["has_fit_line"] if r["tool"] == "Findline" else r["has_fit_circle"]
        lines.append("| " + " | ".join(md_value(x) for x in [r["tool"], r["case_name"], r["exit_code"], r["measure_source"], r["fallback_used"], r["points_count"], r["valid_points_count"], fit, r["failure_stage"]]) + " |")
    
    lines += ["", "## Findline Cases with Image Evidence", "", "| Case | Method | Threshold | Linegap | Points | Fit | RefPts | Support | MeanErr | FitOffsetErr | Classification |", "|---|---:|---:|---:|---:|---|---:|---:|---:|---:|---|"]
    for r in records:
        if r["tool"] == "Findline":
            lines.append("| " + " | ".join(md_value(r[k]) for k in ["case_name","method","threshold","linegap","valid_points_count","has_fit_line","reference_points_count","edge_support_score","mean_error_px","fit_offset_error_px","classification"]) + " |")
    
    lines += ["", "## Findcircle Cases with Image Evidence", "", "| Case | Method | Threshold | Gap | Linegap | Points | FitCircle | RefPts | Support | CenterErr | RadiusErr | Classification |", "|---|---:|---:|---:|---:|---:|---|---:|---:|---:|---:|---|"]
    for r in records:
        if r["tool"] == "Findcircle":
            lines.append("| " + " | ".join(md_value(r[k]) for k in ["case_name","method","threshold","gap","linegap","valid_points_count","has_fit_circle","reference_points_count","edge_support_score","circle_center_error_px","circle_radius_error_px","classification"]) + " |")
    
    lines += ["", "## Image Evidence Conclusions", "", "### Findline", ""]
    findline_records = [r for r in records if r["tool"] == "Findline"]
    if any(r.get("reference_available") for r in findline_records):
        lines.append("- 图像证据中存在清晰边缘")
    else:
        lines.append("- 图像证据中边缘证据弱")
    lines.append("- Original Measure 是否抓到这些边缘: " + ("是" if any(r["original_candidate"] for r in findline_records) else "否"))
    method0_ok = any(r["method"] == 0 and r["original_candidate"] for r in findline_records)
    method1_ok = any(r["method"] == 1 and r["original_candidate"] for r in findline_records)
    lines.append(f"- method=0 成功: {method0_ok}, method=1 成功: {method1_ok}")
    lines.append("- edge_support_score >= 0.7 的 case: " + ", ".join([r["case_name"] for r in findline_records if (r.get("edge_support_score") or 0) >= 0.7] or ["none"]))
    
    lines += ["", "### Findcircle", ""]
    findcircle_records = [r for r in records if r["tool"] == "Findcircle"]
    if any(r.get("reference_available") for r in findcircle_records):
        lines.append("- 图像证据中存在圆边缘")
    else:
        lines.append("- 图像证据中圆边缘证据弱")
    lines.append("- Original Measure 是否抓到这些圆边缘: " + ("是" if any(r["original_candidate"] for r in findcircle_records) else "否"))
    lines.append("- edge_support_score >= 0.7 的 case: " + ", ".join([r["case_name"] for r in findcircle_records if (r.get("edge_support_score") or 0) >= 0.7] or ["none"]))
    
    lines += ["", "## Fallback Note", "", "`fallback_used=true` 仅作为显示链/拟合链诊断，不计入 original Measure 成功。", "", "## Candidate Conclusions", "", f"- Findline original candidates: {', '.join(payload['summary']['findline_original_candidates']) or 'none'}", f"- Findcircle original candidates: {', '.join(payload['summary']['findcircle_original_candidates']) or 'none'}", f"- Evidence supported cases: {', '.join(payload['summary']['evidence_supported_cases']) or 'none'}", "- 下一步建议：依据失败 case 的 failure_stage 和 edge_support_score 继续对齐原 Measure，不进入 FastMatch。", ""]
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
    lines.append(f"- Findline candidate count: {sum(1 for r in findline_records if r.get('original_candidate'))}")
    lines.append(f"- Findcircle candidate count: {sum(1 for r in findcircle_records if r.get('original_candidate'))}")
    lines.append("- FastMatch status: not ready")
    
    lines += ["", "## 2. Findline Original Candidates", "", "| Case | Points | AvgDist | EdgeSupport | MeanErr | FitOffset | Classification |", "|---|---:|---:|---:|---:|---:|---|"]
    for r in findline_records:
        lines.append("| " + " | ".join(md_value(r[k]) for k in ["case_name","valid_points_count","line_avgdist","edge_support_score","mean_error_px","fit_offset_error_px","classification"]) + " |")
    
    lines += ["", "## 3. Findline Filter Sweep", "", "| FilterMin | Components | Accepted | Points | Fit | AvgDist | EdgeSupport |", "|---:|---:|---:|---:|---|---:|---:|"]
    for r in sorted(filter_records, key=lambda x: x.get("filter_min") or 999):
        lines.append("| " + " | ".join(md_value(r[k]) for k in ["filter_min","findobject_component_total","findobject_component_accepted","valid_points_count","has_fit_line","line_avgdist","edge_support_score"]) + " |")
    
    lines += ["", "## 4. Findline Default Failure Analysis", ""]
    direct_case = next((r for r in findline_records if r["case_name"] == "find_line_direct"), None)
    if direct_case:
        lines.append(f"- direct failure stage: {direct_case.get('failure_stage') or 'unknown'}")
        lines.append(f"- direct filter_min: {direct_case.get('filter_min')}")
        lines.append(f"- filter_relax behavior: {[r['case_name'] for r in findline_records if r['case_name'] == 'find_line_filter_relax']}")
        lines.append("- likely reason: filter_min=50 过严，二值图中大部分对象面积小于 50")
        lines.append("- recommended next step: 找出原版本默认 filter_min 值，或调整当前默认值")
    
    lines += ["", "## 5. Findcircle Candidate Ranking", "", "| Case | Points | Radius | AvgDist | EdgeSupport | CenterErr | RadiusErr | Classification |", "|---|---:|---:|---:|---:|---:|---:|---|"]
    for r in findcircle_records:
        classification = r.get("classification")
        if r["case_name"] == "find_circle_fitresult_guard":
            classification = "post_fit_refine_candidate"
        lines.append("| " + " | ".join(md_value(r[k]) for k in ["case_name","valid_points_count","fit_circle_radius","circle_avgdist","edge_support_score","circle_center_error_px","circle_radius_error_px"]) + " | " + md_value(classification) + " |")
    
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
    
    findline_ok = any(r.get("evidence_supported") for r in findline_records)
    findcircle_ok = any(r.get("evidence_supported") for r in findcircle_records)
    if findline_ok and findcircle_ok:
        lines.append("- next action: 依据 image evidence 结果选择推荐参数，准备原版本对齐")
    else:
        lines.append("- next action: 继续对齐原 Measure，调整 filter_min 参数")
    
    (out_root / "stage_2_5_conclusion_pack.md").write_text("\n".join(lines), encoding="utf-8")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe")
    parser.add_argument("--out", default=str(DEFAULT_OUT))
    parser.add_argument("--mode", choices=["full", "filter_sweep", "conclusion"], default="full")
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
                record = {"case_name": item[0], "tool": item[1], "script": item[2], "expect": item[3], "exit_code": None, "headless_ok": False, "classification": "RUNNER_EXCEPTION", "error": str(exc), "original_candidate": False}
            records.append(record)
            print(f"{record['case_name']}: {record.get('classification')} exit={record.get('exit_code')}")
        write_filter_sweep_report(out_root, records, exe)
    
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
    
    else:
        out_root = Path(args.out)
        out_root.mkdir(parents=True, exist_ok=True)
        records = []
        for item in CASES:
            try:
                record = run_case(exe, out_root, item)
            except Exception as exc:
                record = {"case_name": item[0], "tool": item[1], "script": item[2], "expect": item[3], "exit_code": None, "headless_ok": False, "classification": "RUNNER_EXCEPTION", "error": str(exc), "original_candidate": False}
            records.append(record)
            print(f"{record['case_name']}: {record.get('classification')} exit={record.get('exit_code')}")
        write_reports(out_root, records, exe)
    
    return 0


if __name__ == "__main__":
    raise SystemExit(main())