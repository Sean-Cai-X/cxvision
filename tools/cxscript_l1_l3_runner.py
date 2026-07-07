import argparse
import json
import re
import subprocess
import cv2
import numpy as np
from pathlib import Path
from collections import defaultdict

ROOT = Path(r"D:\Codex-WorkDir\Sean_WorkDir\cxvisionai")
REPO = ROOT / "cxvision_repo"
DEFAULT_OUT = ROOT / "cxscript_runs" / "stage_2_5_l1_l3"
MANIFEST_PATH = REPO / "tools" / "cxscript_stage25_l1_l3_manifest.json"
IMAGE_MANIFEST_PATH = ROOT / "test_images" / "stage25_image_manifest.json"

SCRIPT_DIR = REPO / "cxparser" / "cxscript" / "module" / "cximage"
TEMPLATE_DIR = SCRIPT_DIR / "templates"


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
    if value is None:
        return None
    value = str(value).strip()
    if value.lower() in ("true", "false"):
        return value.lower() == "true"
    try:
        return float(value) if any(c in value for c in ".eE") else int(value)
    except ValueError:
        return value


def snapshot_value(text, label):
    if not text:
        return None
    matches = re.findall(rf"^\s*{re.escape(label)}:\s*(.*?)\s*$", text, re.MULTILINE)
    return parse_scalar(matches[-1]) if matches else None


def status_value(text, key):
    if not text:
        return None
    matches = re.findall(rf"(?:^|[,|])\s*{re.escape(key)}=([^,|\r\n]+)", text)
    return parse_scalar(matches[-1]) if matches else None


def render_template(template_path, context):
    template = template_path.read_text(encoding="utf-8-sig")
    for key, value in context.items():
        placeholder = "{{" + key + "}}"
        template = template.replace(placeholder, str(value))
    return template


def run_image_preflight(image_info, tool_target):
    path = Path(image_info["path"])
    if not path.exists():
        return {
            "image_id": image_info["image_id"],
            "target_id": tool_target.get("roi_name", ""),
            "tool": tool_target["tool"],
            "roi_valid": False,
            "preflight_class": "IMAGE_NOT_FOUND",
            "error": f"Image not found: {path}"
        }

    img = cv2.imread(str(path))
    if img is None:
        return {
            "image_id": image_info["image_id"],
            "target_id": tool_target.get("roi_name", ""),
            "tool": tool_target["tool"],
            "roi_valid": False,
            "preflight_class": "IMAGE_LOAD_FAILED",
            "error": f"Failed to load image: {path}"
        }

    height, width = img.shape[:2]
    gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)

    tool = tool_target["tool"]
    warnings = []

    if tool == "Findline":
        x0, y0 = tool_target["x0"], tool_target["y0"]
        x1, y1 = tool_target["x1"], tool_target["y1"]
        wgap = tool_target.get("wgap", 8)
        hgap = tool_target.get("hgap", 32)

        min_x = min(x0, x1) - wgap
        max_x = max(x0, x1) + wgap
        min_y = min(y0, y1) - hgap
        max_y = max(y0, y1) + hgap

        roi_inside = (min_x >= 0 and max_x <= width and min_y >= 0 and max_y <= height)
        if not roi_inside:
            warnings.append("ROI_OUT_OF_IMAGE")

        roi_width = max_x - min_x
        roi_height = max_y - min_y

        roi_gray = gray[max(0, min_y):min(height, max_y), max(0, min_x):min(width, max_x)]

    elif tool == "Findcircle":
        cx, cy = tool_target["cx"], tool_target["cy"]
        px, py = tool_target["px"], tool_target["py"]
        gap = tool_target.get("gap", 5)
        linegap = tool_target.get("linegap", 3)

        radius = int(np.sqrt((px - cx) ** 2 + (py - cy) ** 2))
        min_x = cx - radius - gap - linegap
        max_x = cx + radius + gap + linegap
        min_y = cy - radius - gap - linegap
        max_y = cy + radius + gap + linegap

        roi_inside = (min_x >= 0 and max_x <= width and min_y >= 0 and max_y <= height)
        if not roi_inside:
            warnings.append("ROI_OUT_OF_IMAGE")

        roi_width = max_x - min_x
        roi_height = max_y - min_y

        roi_gray = gray[max(0, min_y):min(height, max_y), max(0, min_x):min(width, max_x)]

    else:
        return {
            "image_id": image_info["image_id"],
            "target_id": tool_target.get("roi_name", ""),
            "tool": tool_target["tool"],
            "roi_valid": False,
            "preflight_class": "UNKNOWN_TOOL"
        }

    gray_mean = float(np.mean(roi_gray))
    gray_std = float(np.std(roi_gray))
    gray_min = float(np.min(roi_gray))
    gray_max = float(np.max(roi_gray))

    sobel_x = cv2.Sobel(roi_gray, cv2.CV_64F, 1, 0, ksize=3)
    sobel_y = cv2.Sobel(roi_gray, cv2.CV_64F, 0, 1, ksize=3)
    gradient_mag = np.sqrt(sobel_x ** 2 + sobel_y ** 2)
    gradient_mean = float(np.mean(gradient_mag))
    gradient_p90 = float(np.percentile(gradient_mag, 90))
    gradient_max = float(np.max(gradient_mag))

    saturation_ratio = float(np.sum(roi_gray <= 5) + np.sum(roi_gray >= 250)) / max(1, roi_gray.size)
    if saturation_ratio > 0.2:
        warnings.append("SATURATION_WARNING")

    blur_score = float(np.mean(gradient_mag))
    if blur_score < 5.0:
        warnings.append("BLUR_WARNING")

    contrast = gray_max - gray_min
    if contrast < 30:
        warnings.append("LOW_CONTRAST_WARNING")

    if gradient_mean < 3.0:
        warnings.append("LOW_GRADIENT_WARNING")

    if "ROI_OUT_OF_IMAGE" in warnings:
        preflight_class = "ROI_OUT_OF_IMAGE"
        roi_valid = False
    elif warnings:
        preflight_class = "WARNING_" + "_".join(sorted(warnings))
        roi_valid = True
    else:
        preflight_class = "OK"
        roi_valid = True

    return {
        "image_id": image_info["image_id"],
        "target_id": tool_target.get("roi_name", ""),
        "tool": tool_target["tool"],
        "level": image_info["level"],
        "image_width": width,
        "image_height": height,
        "roi_valid": roi_valid,
        "roi_inside_image": roi_inside,
        "roi_width": roi_width,
        "roi_height": roi_height,
        "gray_mean": round(gray_mean, 2),
        "gray_std": round(gray_std, 2),
        "gray_min": gray_min,
        "gray_max": gray_max,
        "gradient_mean": round(gradient_mean, 2),
        "gradient_p90": round(gradient_p90, 2),
        "gradient_max": round(gradient_max, 2),
        "saturation_ratio": round(saturation_ratio, 4),
        "blur_score": round(blur_score, 2),
        "contrast": contrast,
        "preflight_class": preflight_class,
        "warnings": warnings
    }


def run_case(exe, out_root, image_info, tool_target, profile, evidence_profile):
    image_path = Path(image_info["path"])
    level = image_info["level"]
    image_id = image_info["image_id"]
    target_id = tool_target.get("roi_name", "")
    profile_id = profile["profile_id"]
    tool = tool_target["tool"]

    template_path = REPO / profile["template"]
    if not template_path.exists():
        return {"error": f"Template not found: {template_path}"}

    context = {}
    if tool == "Findline":
        context.update({
            "x0": tool_target["x0"], "y0": tool_target["y0"],
            "x1": tool_target["x1"], "y1": tool_target["y1"],
            "wgap": tool_target.get("wgap", 8),
            "hgap": tool_target.get("hgap", 32),
            "method": profile.get("method", 0),
            "threshold": profile.get("threshold", 20),
            "linegap": profile.get("linegap", 6),
            "fitmode": profile.get("fitmode", 1),
            "script_scale": profile.get("script_scale", 1),
            "filter_profile": profile.get("filter_profile", 0),
        })
    elif tool == "Findcircle":
        context.update({
            "cx": tool_target["cx"], "cy": tool_target["cy"],
            "px": tool_target["px"], "py": tool_target["py"],
            "gap": tool_target.get("gap", 5),
            "linegap": tool_target.get("linegap", 3),
            "method": profile.get("method", 0),
            "threshold": profile.get("threshold", 20),
        })

    script_text = render_template(template_path, context)

    generated_dir = out_root / "generated_scripts" / image_id / target_id
    generated_dir.mkdir(parents=True, exist_ok=True)
    script_path = generated_dir / f"{profile_id}.cxsc"
    script_path.write_text(script_text, encoding="utf-8-sig")

    case_dir = out_root / level / image_id / target_id / profile_id / evidence_profile
    case_dir.mkdir(parents=True, exist_ok=True)

    evidence_arg = ["--evidence-profile", evidence_profile] if evidence_profile else []

    cmd = [str(exe), "--cxscript-headless", "--image", str(image_path),
           "--script", str(script_path), "--out", str(case_dir),
           "--case-name", f"{image_id}_{target_id}_{profile_id}"] + evidence_arg
    proc = subprocess.run(cmd, cwd=str(REPO), capture_output=True, text=True, timeout=120)

    summary_path = case_dir / "result_summary.json"
    snapshot_path = case_dir / "snapshot.txt"
    overlay_path = case_dir / "result_overlay.png"
    evidence_summary_path = case_dir / "evidence_summary.json"

    summary = {}
    if summary_path.exists():
        try:
            summary = json.loads(summary_path.read_text(encoding="utf-8-sig"))
        except:
            summary = {}

    evidence_summary = {}
    if evidence_summary_path.exists():
        try:
            evidence_summary = json.loads(evidence_summary_path.read_text(encoding="utf-8-sig"))
        except:
            evidence_summary = {}

    snapshot = snapshot_path.read_text(encoding="utf-8-sig", errors="replace") if snapshot_path.exists() else ""

    objects = summary.get("runtime_objects") or []
    obj = next((x for x in objects if x.get("type") == tool), {})

    evidence_summaries = evidence_summary.get("evidence_summaries") or []
    ev_summary = next((x for x in evidence_summaries if x.get("tool") == tool), {})

    points = obj.get("valid_line_points_count") if tool == "Findline" else obj.get("valid_points_count")

    record = {
        "image_id": image_id,
        "level": level,
        "target_id": target_id,
        "case_name": f"{image_id}_{target_id}_{profile_id}",
        "tool": tool,
        "profile_id": profile_id,
        "evidence_profile": evidence_profile,
        "exit_code": proc.returncode,
        "headless_ok": summary_path.exists() and snapshot_path.exists() and overlay_path.exists(),
        "run_state": summary.get("run_state"),
        "valid_points_count": points,
        "has_fit_line": obj.get("has_fit_line"),
        "has_fit_circle": obj.get("has_fit_result"),
        "method": profile.get("method"),
        "threshold": profile.get("threshold"),
        "linegap": profile.get("linegap"),
        "gap": profile.get("gap"),
        "filter_profile": profile.get("filter_profile"),
        "policy_classification": profile.get("policy_classification", "DEFAULT_POLICY"),
        "expected_edge": tool_target.get("expected_edge"),
        "edge_polarity_hint": tool_target.get("edge_polarity_hint"),
        "fit_offset_error_px": ev_summary.get("fit_offset_error_px"),
        "circle_center_error_px": ev_summary.get("circle_center_error_px"),
        "mean_error_px": ev_summary.get("mean_error_px"),
        "max_error_px": ev_summary.get("max_error_px"),
        "primary_error_metric": ev_summary.get("primary_error_metric"),
        "metric_valid": ev_summary.get("metric_valid"),
        "combined_edge_support_score": ev_summary.get("combined_edge_support_score"),
        "measured_local_support_score": ev_summary.get("measured_local_support_score"),
        "measured_local_mean_distance_px": ev_summary.get("measured_local_mean_distance_px"),
        "global_reference_mean_distance_px": ev_summary.get("global_reference_mean_distance_px"),
        "circle_local_support_score": ev_summary.get("circle_local_support_score"),
        "circle_local_mean_radial_distance_px": ev_summary.get("circle_local_mean_radial_distance_px"),
        "circle_global_reference_mean_distance_px": ev_summary.get("circle_global_reference_mean_distance_px"),
        "effective_filter_min": snapshot_value(snapshot, "line_measure_effective_filter_min"),
        "cc_selected_total": snapshot_value(snapshot, "line_findobject_component_total"),
        "cc_selected_accepted": snapshot_value(snapshot, "line_findobject_component_accepted"),
        "cc_selected_area_min": snapshot_value(snapshot, "line_findobject_area_min_observed"),
        "cc_selected_area_max": snapshot_value(snapshot, "line_findobject_area_max_observed"),
        "cc_selected_area_mean": snapshot_value(snapshot, "line_findobject_area_mean_observed"),
        "cc_selected_area_median": snapshot_value(snapshot, "line_findobject_area_median"),
        "cc_selected_area_p90": snapshot_value(snapshot, "line_findobject_area_p90"),
        "binary_foreground_pixels": snapshot_value(snapshot, "line_measure_binary_foreground_pixels"),
        "stdout": proc.stdout.strip(),
        "stderr": proc.stderr.strip(),
    }

    fallback_used = status_value(snapshot, "fallback_used")
    record["fallback_used"] = bool(fallback_used) if fallback_used is not None else False

    record = classify_case(record)

    return record


def classify_case(record):
    tool = record["tool"]
    fallback_used = record["fallback_used"]
    valid_points = as_int(record.get("valid_points_count"))
    has_fit = record["has_fit_line"] if tool == "Findline" else record["has_fit_circle"]
    mean_err = as_float(record.get("mean_error_px"))
    local_support = as_float(record.get("measured_local_support_score")) if tool == "Findline" else as_float(record.get("circle_local_support_score"))
    combined_support = as_float(record.get("combined_edge_support_score"))

    if not record["headless_ok"]:
        record["quality_classification"] = "HEADLESS_FAILED"
        record["t0_pass"] = False
        record["t1_pass"] = False
        record["t2_pass"] = False
        return record

    record["t0_pass"] = True

    if fallback_used:
        record["quality_classification"] = "FALLBACK_DIAGNOSTIC"
        record["t1_pass"] = False
        record["t2_pass"] = False
        return record

    if tool == "Findline":
        t1_pass = valid_points >= 2 and has_fit is True
    else:
        t1_pass = valid_points >= 3 and has_fit is True

    record["t1_pass"] = t1_pass

    if not t1_pass:
        record["quality_classification"] = "ALGORITHM_NO_RESULT"
        record["t2_pass"] = False
        return record

    if local_support >= 0.60:
        record["quality_classification"] = "ORIGINAL_LOCAL_EDGE_CONFIRMED"
        record["t2_pass"] = True
    elif combined_support >= 0.60:
        record["quality_classification"] = "ORIGINAL_EDGE_CONFIRMED"
        record["t2_pass"] = True
    elif mean_err <= 5.0:
        record["quality_classification"] = "GEOMETRY_MARGINAL_BUT_SAMPLED"
        record["t2_pass"] = False
    else:
        record["quality_classification"] = "SUSPICIOUS_CANDIDATE"
        record["t2_pass"] = False

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


def write_image_preflight_report(out_root, preflight_results):
    lines = ["# Stage 2.5 L1~L3 Image Preflight Report", "", "## Summary", ""]

    total = len(preflight_results)
    ok = sum(1 for r in preflight_results if r["preflight_class"] == "OK")
    warning = sum(1 for r in preflight_results if "WARNING" in r["preflight_class"])
    invalid = sum(1 for r in preflight_results if not r["roi_valid"])

    lines.append(f"- Total targets: {total}")
    lines.append(f"- OK: {ok}")
    lines.append(f"- With warnings: {warning}")
    lines.append(f"- Invalid (not run): {invalid}")

    lines += ["", "## Preflight Details", "",
              "| Level | Image | Target | Tool | ROIValid | PreflightClass | Contrast | GradientMean | BlurScore |"]
    lines += ["|---|---|---|---|---|---|---:|---:|---:|"]

    for r in sorted(preflight_results, key=lambda x: (x["level"], x["image_id"], x["target_id"])):
        lines.append(f"| {r['level']} | {r['image_id']} | {r['target_id']} | {r['tool']} | {r['roi_valid']} | {r['preflight_class']} | {r.get('contrast', '')} | {r.get('gradient_mean', '')} | {r.get('blur_score', '')} |")

    (out_root / "image_preflight_report.md").write_text("\n".join(lines), encoding="utf-8")
    (out_root / "image_preflight_report.json").write_text(json.dumps(preflight_results, indent=2, ensure_ascii=False), encoding="utf-8")


def write_image_coverage_report(out_root, image_manifest):
    lines = ["# Stage 2.5 L1~L3 Image Coverage Report", "", "## Summary", ""]

    level_stats = defaultdict(lambda: {"images": 0, "findline_targets": 0, "findcircle_targets": 0})

    for img in image_manifest["images"]:
        level = img["level"]
        level_stats[level]["images"] += 1
        for target in img["tool_targets"]:
            if target["tool"] == "Findline":
                level_stats[level]["findline_targets"] += 1
            elif target["tool"] == "Findcircle":
                level_stats[level]["findcircle_targets"] += 1

    total_images = sum(s["images"] for s in level_stats.values())
    total_findline = sum(s["findline_targets"] for s in level_stats.values())
    total_findcircle = sum(s["findcircle_targets"] for s in level_stats.values())

    lines.append(f"- Total images: {total_images}")
    lines.append(f"- Total Findline targets: {total_findline}")
    lines.append(f"- Total Findcircle targets: {total_findcircle}")

    l1_ok = level_stats.get("L1_high_contrast", {}).get("findline_targets", 0) >= 2 and \
            level_stats.get("L1_high_contrast", {}).get("findcircle_targets", 0) >= 2
    l2_ok = level_stats.get("L2_low_contrast_illumination", {}).get("findline_targets", 0) >= 2 and \
            level_stats.get("L2_low_contrast_illumination", {}).get("findcircle_targets", 0) >= 2
    l3_ok = level_stats.get("L3_complex_boundary", {}).get("findline_targets", 0) >= 2 and \
            level_stats.get("L3_complex_boundary", {}).get("findcircle_targets", 0) >= 2

    coverage_status = "OK" if (l1_ok and l2_ok and l3_ok) else "INSUFFICIENT_COVERAGE"
    lines.append(f"- Coverage status: {coverage_status}")

    lines += ["", "## Coverage Details", "",
              "| Level | ImageCount | FindlineTargets | FindcircleTargets | Status |"]
    lines += ["|---|---:|---:|---:|---|"]

    for level in sorted(level_stats.keys()):
        s = level_stats[level]
        status = "OK"
        if level == "L1_high_contrast":
            status = "OK" if l1_ok else "INSUFFICIENT"
        elif level == "L2_low_contrast_illumination":
            status = "OK" if l2_ok else "INSUFFICIENT"
        elif level == "L3_complex_boundary":
            status = "OK" if l3_ok else "INSUFFICIENT"
        lines.append(f"| {level} | {s['images']} | {s['findline_targets']} | {s['findcircle_targets']} | {status} |")

    (out_root / "image_coverage_report.md").write_text("\n".join(lines), encoding="utf-8")


def write_batch_report(out_root, records, preflight_results):
    lines = ["# Stage 2.5 L1~L3 Parameter Consistency Report", "", "## Summary", ""]

    total = len(records)
    t0_pass = sum(1 for r in records if r.get("t0_pass"))
    t1_pass = sum(1 for r in records if r.get("t1_pass"))
    t2_pass = sum(1 for r in records if r.get("t2_pass"))

    confirmed = sum(1 for r in records if r.get("quality_classification") in {"ORIGINAL_LOCAL_EDGE_CONFIRMED", "ORIGINAL_EDGE_CONFIRMED"})

    lines.append(f"- Total cases: {total}")
    lines.append(f"- T0 (execution) pass: {t0_pass}")
    lines.append(f"- T1 (algorithm) pass: {t1_pass}")
    lines.append(f"- T2 (evidence support) pass: {t2_pass}")
    lines.append(f"- Edge confirmed: {confirmed}")

    lines += ["", "## Findline Cases", "",
              "| Level | Image | Target | Profile | Points | Fit | LocalSupport | LocalMeanDist | GlobalLineDist | Quality | Policy |"]
    lines += ["|---|---|---|---|---:|---|---:|---:|---:|---|---|"]

    for r in sorted(records, key=lambda x: (x["level"], x["image_id"], x["target_id"], x["profile_id"])):
        if r["tool"] != "Findline":
            continue
        lines.append(f"| {r['level']} | {r['image_id']} | {r['target_id']} | {r['profile_id']} | {as_int(r['valid_points_count'])} | {r['has_fit_line']} | {as_float(r['measured_local_support_score']):.3f} | {as_float(r['measured_local_mean_distance_px']):.3f} | {as_float(r['global_reference_mean_distance_px']):.3f} | {r['quality_classification']} | {r['policy_classification']} |")

    lines += ["", "## Findcircle Cases", "",
              "| Level | Image | Target | Profile | Points | FitCircle | LocalSupport | LocalMeanRadialDist | GlobalRefMeanDist | CenterErr | Quality | Policy |"]
    lines += ["|---|---|---|---|---:|---|---:|---:|---:|---:|---|---|"]

    for r in sorted(records, key=lambda x: (x["level"], x["image_id"], x["target_id"], x["profile_id"])):
        if r["tool"] != "Findcircle":
            continue
        lines.append(f"| {r['level']} | {r['image_id']} | {r['target_id']} | {r['profile_id']} | {as_int(r['valid_points_count'])} | {r['has_fit_circle']} | {as_float(r['circle_local_support_score']):.3f} | {as_float(r['circle_local_mean_radial_distance_px']):.3f} | {as_float(r['circle_global_reference_mean_distance_px']):.3f} | {as_float(r['circle_center_error_px']):.3f} | {r['quality_classification']} | {r['policy_classification']} |")

    (out_root / "batch_report.md").write_text("\n".join(lines), encoding="utf-8")
    (out_root / "batch_report.json").write_text(json.dumps(records, indent=2, ensure_ascii=False), encoding="utf-8")


def write_parameter_stability_report(out_root, records):
    lines = ["# Stage 2.5 L1~L3 Parameter Stability Report", "", "## Summary", ""]

    for tool in ["Findline", "Findcircle"]:
        tool_records = [r for r in records if r["tool"] == tool]
        profiles = sorted(set(r["profile_id"] for r in tool_records))

        lines += [f"", f"## {tool} Stability By Level", "",
                  f"| Profile | Level | Targets | OriginalSuccess | LocalConfirmed | MeanLocalSupport | MeanLocalDist |"]
        lines += ["|---|---|---:|---:|---:|---:|---:|"]

        for profile in profiles:
            profile_records = [r for r in tool_records if r["profile_id"] == profile]
            levels = sorted(set(r["level"] for r in profile_records))

            for level in levels:
                level_records = [r for r in profile_records if r["level"] == level]
                total = len(level_records)
                success = sum(1 for r in level_records if r.get("t1_pass"))
                confirmed = sum(1 for r in level_records if r.get("quality_classification") in {"ORIGINAL_LOCAL_EDGE_CONFIRMED", "ORIGINAL_EDGE_CONFIRMED"})
                mean_support = sum(as_float(r.get("measured_local_support_score")) if tool == "Findline" else as_float(r.get("circle_local_support_score")) for r in level_records) / max(1, total)
                mean_dist = sum(as_float(r.get("measured_local_mean_distance_px")) if tool == "Findline" else as_float(r.get("circle_local_mean_radial_distance_px")) for r in level_records) / max(1, total)

                lines.append(f"| {profile} | {level} | {total} | {success}/{total} | {confirmed}/{total} | {mean_support:.3f} | {mean_dist:.3f} |")

        lines += [f"", f"## {tool} Overall Stability", "",
                  f"| Profile | Levels | Targets | OriginalSuccessRate | LocalConfirmedRate | MeanLocalSupport | Score | Recommendation |"]
        lines += ["|---|---|---:|---:|---:|---:|---:|---|"]

        for profile in profiles:
            profile_records = [r for r in tool_records if r["profile_id"] == profile]
            total = len(profile_records)
            levels = len(set(r["level"] for r in profile_records))

            if total == 0:
                continue

            success_rate = sum(1 for r in profile_records if r.get("t1_pass")) / total
            confirmed_rate = sum(1 for r in profile_records if r.get("quality_classification") in {"ORIGINAL_LOCAL_EDGE_CONFIRMED", "ORIGINAL_EDGE_CONFIRMED"}) / total
            mean_support = sum(as_float(r.get("measured_local_support_score")) if tool == "Findline" else as_float(r.get("circle_local_support_score")) for r in profile_records) / total
            mean_fit_offset = sum(as_float(r.get("fit_offset_error_px")) for r in profile_records) / total
            fit_offset_score = max(0, 1 - mean_fit_offset / 10.0)

            component_warnings = sum(1 for r in profile_records if as_float(r.get("cc_selected_area_max")) > 100000) / max(1, total)

            score = success_rate * 30 + confirmed_rate * 30 + mean_support * 20 + max(0, 1 - component_warnings) * 10 + fit_offset_score * 10

            if score >= 80 and levels >= 2 and total >= 6:
                recommendation = "PROFILE_RECOMMENDED"
            elif score >= 65 and total >= 4:
                recommendation = "PROFILE_CONDITIONALLY_RECOMMENDED"
            elif profile in ("gamma", "filter_relax_min1", "filter_relax"):
                recommendation = "PROFILE_DEBUG_ONLY"
            elif success_rate < 0.5:
                recommendation = "PROFILE_UNSTABLE"
            else:
                recommendation = "PROFILE_INCONCLUSIVE"

            lines.append(f"| {profile} | {levels} | {total} | {success_rate:.2f} | {confirmed_rate:.2f} | {mean_support:.3f} | {score:.1f} | {recommendation} |")

    (out_root / "parameter_stability_report.md").write_text("\n".join(lines), encoding="utf-8")


def write_parameter_policy_report(out_root, records):
    lines = ["# Stage 2.5 L1~L3 Parameter Policy Decision", "", "## Current Policy", "",
             "| Parameter/Profile | Current Status | Decision | Reason |",
             "|---|---|---|---|",
             "| Findline legacy filter_min=50 | preserved | keep as legacy | needed for original compare |",
             "| Findline stage25_filter20 | candidate | keep as test profile | produces points and matches component stats |",
             "| Findline threshold8 | candidate | keep as weak edge candidate | may perform better on L2 low contrast |",
             "| Findline linegap10 | candidate | keep for comparison | fewer points but may reduce interference |",
             "| Findline filter_relax_min1 | debug | do not promote | too permissive |",
             "| Findline gamma | risky | do not promote | binary collapses to large component |",
             "| Findcircle direct | candidate | keep as primary | currently most stable |",
             "| Findcircle threshold8 | candidate | keep as weak edge candidate |",
             "| Findcircle method1 | risky | do not promote | polarity risk observed |",
             "| Findcircle filter_relax | debug | do not promote | may introduce false positives |",
             "| FastMatch | not evaluated | deferred | not in current test scope |"]

    (out_root / "parameter_policy_report.md").write_text("\n".join(lines), encoding="utf-8")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", help="Path to cxvision_imgui_acceptance.exe")
    parser.add_argument("--out", help="Output directory", default=str(DEFAULT_OUT))
    parser.add_argument("--manifest", help="L1~L3 profile manifest", default=str(MANIFEST_PATH))
    parser.add_argument("--image-manifest", help="Image manifest", default=str(IMAGE_MANIFEST_PATH))
    args = parser.parse_args()

    exe = find_exe(args.exe)
    out_root = Path(args.out)
    out_root.mkdir(parents=True, exist_ok=True)

    with open(args.manifest, "r", encoding="utf-8") as f:
        l1_l3_manifest = json.load(f)

    with open(args.image_manifest, "r", encoding="utf-8") as f:
        image_manifest = json.load(f)

    findline_profiles = l1_l3_manifest["findline_profiles"]
    findcircle_profiles = l1_l3_manifest["findcircle_profiles"]
    evidence_profiles = l1_l3_manifest["evidence_profiles"]

    write_image_coverage_report(out_root, image_manifest)

    preflight_results = []
    for img in image_manifest["images"]:
        for target in img["tool_targets"]:
            preflight = run_image_preflight(img, target)
            preflight_results.append(preflight)

    write_image_preflight_report(out_root, preflight_results)

    valid_preflight = {f"{r['image_id']}_{r['target_id']}": r for r in preflight_results if r["roi_valid"]}

    records = []

    for evidence_profile in evidence_profiles:
        ep_name = evidence_profile["name"]

        for img in image_manifest["images"]:
            for target in img["tool_targets"]:
                key = f"{img['image_id']}_{target.get('roi_name', '')}"
                if key not in valid_preflight:
                    continue

                if target["tool"] == "Findline":
                    profiles = findline_profiles
                elif target["tool"] == "Findcircle":
                    profiles = findcircle_profiles
                else:
                    continue

                for profile in profiles:
                    print(f"Running: {img['level']}/{img['image_id']}/{target.get('roi_name', '')}/{profile['profile_id']}/{ep_name}")
                    record = run_case(exe, out_root, img, target, profile, ep_name)
                    if "error" not in record:
                        records.append(record)

    write_batch_report(out_root, records, preflight_results)
    write_parameter_stability_report(out_root, records)
    write_parameter_policy_report(out_root)

    print(f"Reports written to: {out_root}")


if __name__ == "__main__":
    main()