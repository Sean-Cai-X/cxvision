#include "CircleRingGauge.h"
#include "CxCrashLogHandler.h"
#include "CxImageRuntimeOverlay.h"
#include "CxParamProbeRunner.h"
#include "CxScriptCasePackageWriter.h"
#include "CxScriptCatalogRuntime.h"
#include "CxScriptGeometryFrameProbe.h"
#include "CxScriptHeadlessRunner.h"
#include "CxScriptImageEvidenceAnalyzer.h"
#include "CxScriptRunTraceRuntime.h"
#include "CxUnifiedLog.h"
#include "FastMatch.h"
#include "FindCircle.h"
#include "FindEllipse.h"
#include "FindLine.h"
#include "FindLineParameterPolicy.h"
#include "FindObject.h"
#include "FindRect.h"
#include "FindSegmentation.h"
#include "GridPatternClassTool.h"
#include "Image.h"
#include "ManualConsoleCxScriptDebug.h"
#include "ManualConsoleEvidenceChain.h"
#include "ManualConsoleFindCircleDebug.h"
#include "ManualConsoleFindLineDebug.h"
#include "ManualConsoleGauge.h"
#include "ManualConsoleParamRegressionPanel.h"
#include "ManualConsoleRuntimeView.h"
#include "ManualConsoleScriptDebugPanel.h"
#include "ManualConsoleUtils.h"
#include "RegionPatternTool.h"
#include "TorchTask.h"
#include "ViewController.h"
#include "imagemanager.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <memory>
#include <opencv2/imgcodecs.hpp>
#include <sstream>
#include <unordered_map>

namespace {
namespace fs = std::filesystem;

static fs::path CxDebugLogDirectory() {
  return fs::path("docs") / "notes" / "cxscript_case";
}

static fs::path CxDebugRuntimeLogPath() {
  return CxDebugLogDirectory() / "cxscript_debug_runtime_latest.jsonl";
}

static fs::path CxDebugSnapshotPath() {
  return CxDebugLogDirectory() / "cxscript_debug_snapshot_latest.txt";
}

static fs::path ResolveCaseDirectory() {
  return fs::path("docs") / "notes" / "cxscript_case";
}

static std::string CurrentTimestamp() {
  const std::time_t now = std::time(nullptr);
  std::tm local_time = {};
#if defined(_WIN32)
  localtime_s(&local_time, &now);
#else
  localtime_r(&now, &local_time);
#endif
  char buffer[32] = {};
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &local_time);
  return buffer;
}

static std::string CxDebugJsonEscape(const std::string &text) {
  std::string out;
  out.reserve(text.size());
  for (char ch : text) {
    if (ch == '\\')
      out += "\\\\";
    else if (ch == '\"')
      out += "\\\"";
    else if (ch == '\n')
      out += "\\n";
    else if (ch == '\r')
      out += "\\r";
    else if (ch == '\t')
      out += "\\t";
    else
      out += ch;
  }
  return out;
}

static void CopyPointsToFloatXY(const PointsShape &points,
                                std::vector<float> &out) {
  out.clear();
  for (int i = 0; i < points.size(); ++i) {
    const double x = points.getx(i);
    const double y = points.gety(i);
    if (!std::isfinite(x) || !std::isfinite(y))
      continue;
    out.push_back(static_cast<float>(x));
    out.push_back(static_cast<float>(y));
  }
}

static void FillRuntimeObjectFromFindCircle(RuntimeObjectView &object,
                                            const std::string &name,
                                            FindCircle &circle) {
  object = RuntimeObjectView{};
  object.name = name;
  object.type = "FindCircle";
  object.exists_in_parser = true;
  object.last_runtime_status = "runtime_executed";
  object.runtime_state = "runtime_executed";
  object.visualizable = true;
  object.visual_source = "runtime_object";
  object.stale = false;

  object.has_circle = true;
  object.circle_cx = static_cast<float>(circle.getcirclecentx());
  object.circle_cy = static_cast<float>(circle.getcirclecenty());
  object.circle_px = static_cast<float>(circle.getcirclepax());
  object.circle_py = static_cast<float>(circle.getcirclepay());
  // Preserve the legacy snapshot fields until the diagnostic JSON schema is
  // migrated.  New GUI synchronization must use circle_px/circle_py.
  object.circle_inner = object.circle_px;
  object.circle_radius = object.circle_py;

  const PointsShape &points = circle.getresultpoints();
  CopyPointsToFloatXY(points, object.measure_points_xy);
  object.measure_points_count = points.size();
  object.valid_points_count =
      static_cast<int>(object.measure_points_xy.size() / 2);
  object.has_measure_points = !object.measure_points_xy.empty();

  object.has_fit_result = circle.hasfitresult();
  const FindCircleMeasureGeometryDebug &debug =
      circle.lastmeasuregeometrydebug();
  // Publish the runtime annulus as absolute radii.  circle_px/circle_py are
  // a boundary point, not an inner/outer pair; using them as legacy radius
  // fields was the source of a Key Parameter Controls mismatch after a
  // FindCircle script had run.
  const double runtime_outer_radius =
      std::hypot(static_cast<double>(object.circle_px - object.circle_cx),
                 static_cast<double>(object.circle_py - object.circle_cy));
  const double annulus_outer_radius =
      static_cast<double>(circle.getannulusouter());
  const double annulus_inner_radius =
      static_cast<double>(circle.getannulusinner());
  object.ring_outer_radius =
      annulus_outer_radius > 0.0 ? annulus_outer_radius : runtime_outer_radius;
  object.ring_inner_radius = (annulus_inner_radius > 0.0 &&
                              annulus_inner_radius < object.ring_outer_radius)
                                 ? annulus_inner_radius
                                 : 0.0;
  object.ring_thickness =
      std::max(0.0, object.ring_outer_radius - object.ring_inner_radius);
  object.circle_measure_image_ready = debug.image_ready;
  object.circle_measure_image_width = debug.image_width;
  object.circle_measure_image_height = debug.image_height;
  object.circle_measure_image_channels = debug.image_channels;
  object.circle_measure_backimage_ready = debug.backimage_ready;
  object.circle_measure_findobject_ready = debug.findobject_ready;
  object.circle_object_prefilter_requested = debug.object_prefilter_requested;
  object.circle_object_prefilter_applied = debug.object_prefilter_applied;
  object.circle_object_prefilter_restored = debug.object_prefilter_restored;
  object.circle_object_prefilter_runs_before =
      debug.object_prefilter_runs_before;
  object.circle_object_prefilter_runs_after = debug.object_prefilter_runs_after;
  object.circle_object_prefilter_effective_min =
      debug.object_prefilter_effective_min;
  object.circle_measure_source = debug.measure_source;
  object.circle_measure_failure_stage = debug.failure_stage;
  object.circle_measure_detail = debug.detail;
  object.circle_scan_line_count = debug.scan_line_count;
  object.circle_scan_line_length = debug.scan_line_length;
  object.circle_process_width = debug.process_width;
  object.circle_selected_edge_index = debug.selected_edge_index;
  object.circle_candidate_runs_total = debug.candidate_runs_total;
  object.circle_candidate_runs_max_per_line = debug.candidate_runs_max_per_line;
  object.circle_selected_edge_hits = debug.selected_edge_hits;
  object.circle_selected_edge_misses = debug.selected_edge_misses;
  object.circle_scan_boundary_clipped_lines = debug.scan_boundary_clipped_lines;
  object.circle_scan_boundary_extended_samples =
      debug.scan_boundary_extended_samples;
  object.circle_candidate_boundary_reject_count =
      debug.candidate_boundary_reject_count;
  object.circle_selected_edge_radius_avg = debug.selected_edge_radius_avg;
  object.circle_selected_edge_radius_min = debug.selected_edge_radius_min;
  object.circle_selected_edge_radius_max = debug.selected_edge_radius_max;
  object.circle_point_consistency_enabled = circle.getpointconsistencyenabled();
  object.circle_point_consistency_range = circle.getpointconsistencyrange();
  object.circle_point_consistency_input_points =
      circle.getpointconsistencyinputcount();
  object.circle_point_consistency_output_points =
      circle.getpointconsistencyoutputcount();
  object.circle_point_consistency_removed_points =
      circle.getpointconsistencyremovedcount();

  const FindCircleBoundaryAnalysisSnapshot precision =
      circle.boundaryanalysissnapshot();
  object.circle_boundary_status = precision.status;
  object.circle_boundary_reliability_level = precision.reliability_level;
  object.circle_boundary_expected_scan_count = precision.expected_scan_count;
  object.circle_boundary_accepted_point_count = precision.accepted_point_count;
  object.circle_boundary_refined_point_count =
      precision.interpolation_valid_count;
  object.circle_boundary_coverage_ratio = precision.coverage_ratio;
  object.circle_boundary_subpixel_offset_mean =
      precision.subpixel_offset_mean;
  object.circle_boundary_subpixel_offset_stddev =
      precision.subpixel_offset_stddev;
  object.circle_boundary_localization_sigma_mean_px =
      precision.localization_sigma_mean_px;
  object.circle_boundary_residual_rmse_px = precision.residual_rmse_px;
  object.circle_boundary_residual_p95_px = precision.residual_p95_px;
  object.circle_boundary_residual_max_px = precision.residual_max_px;
  object.circle_boundary_outlier_ratio = precision.outlier_ratio;
  object.circle_boundary_reliability_score = precision.reliability_score;
  object.circle_scan_lines_processed = debug.scan_lines_processed;
  object.circle_total_samples = debug.total_samples;
  object.circle_elapsed_ms = debug.elapsed_ms;
  if (object.has_fit_result) {
    object.fit_cx = static_cast<float>(circle.getresultcentx());
    object.fit_cy = static_cast<float>(circle.getresultcenty());
    object.fit_radius = static_cast<float>(circle.getradius());
    object.fit_avgdist = static_cast<float>(circle.getavgdist());
    object.runtime_state = "geometry_result_available";
    object.last_runtime_status = "runtime_executed";
  } else if (object.has_measure_points) {
    object.runtime_state = "fitcircle_degenerate_after_measure_points";
    object.last_runtime_status = "PENDING_BINDING";
  } else {
    object.runtime_state = "fitcircle_pending_binding";
    object.last_runtime_status = "PENDING_BINDING";
  }

  object.display_summary = BuildFindCircleGeometrySummary(object);
}

static void FillRuntimeObjectFromFindLine(RuntimeObjectView &object,
                                          const std::string &name,
                                          FindLine &line) {
  object = RuntimeObjectView{};
  object.name = name;
  object.type = "FindLine";
  object.exists_in_parser = true;
  object.last_runtime_status = "runtime_executed";
  object.runtime_state = "runtime_executed";
  object.visualizable = true;
  object.visual_source = "runtime_object";
  object.stale = false;

  FindLineDisplaySnapshot snapshot;
  if (line.getdisplaysnapshot(snapshot)) {
    object.has_line_roi = snapshot.has_line_roi;
    object.line_x0 = snapshot.x0;
    object.line_y0 = snapshot.y0;
    object.line_x1 = snapshot.x1;
    object.line_y1 = snapshot.y1;
    object.line_scale = snapshot.scale;
    object.line_tool_wgap = snapshot.wgap;
    object.line_tool_hgap = snapshot.hgap;
    object.linegap = snapshot.linegap;
    object.has_line_scan_box = snapshot.has_scan_box;
    object.line_scan_half_width = snapshot.scan_half_width;
    object.line_scan_box_xy = snapshot.scan_box_xy;
    object.line_display_source = snapshot.source;
    object.effective_tool_half_width = snapshot.scan_half_width;
    object.requested_tool_half_width = snapshot.scan_half_width;
  }

  const PointsShape &w_points = line.getresultpointsw();
  const PointsShape &h_points = line.getresultpointsh();
  object.line_pointsw_count = w_points.size();
  object.line_pointsh_count = h_points.size();
  CopyPointsToFloatXY(w_points, object.line_measure_points_xy);
  std::vector<float> h_xy;
  CopyPointsToFloatXY(h_points, h_xy);
  object.line_measure_points_xy.insert(object.line_measure_points_xy.end(),
                                       h_xy.begin(), h_xy.end());
  object.line_measure_points_count =
      static_cast<int>(object.line_measure_points_xy.size() / 2);
  object.valid_line_points_count = line.getvalidpointcount();
  object.valid_points_count = object.valid_line_points_count;
  object.has_line_measure_points = !object.line_measure_points_xy.empty();

  const FindLineMeasureInputDebug &inputDebug = line.lastmeasureinputdebug();
  const FindLineMeasureProfileStats &profileStats =
      line.lastmeasureprofilestats();
  object.line_measure_image_ready = inputDebug.image_mat_ready;
  object.line_measure_image_width = inputDebug.image_width;
  object.line_measure_image_height = inputDebug.image_height;
  object.line_measure_image_channels = inputDebug.image_channels;
  object.line_measure_image_type = inputDebug.image_type;
  object.line_measure_roi_intersects_image = inputDebug.roi_intersects_image;
  object.line_measure_roi_fully_inside_image =
      inputDebug.roi_fully_inside_image;
  object.line_measure_method = inputDebug.method;
  object.line_measure_threshold = inputDebug.threshold;
  object.line_measure_linegap = inputDebug.linegap;
  object.line_measure_wgap = inputDebug.wgap;
  object.line_measure_hgap = inputDebug.hgap;
  object.line_measure_profile_count = inputDebug.profile_count;
  object.line_measure_sampled_pixel_count = inputDebug.sampled_pixel_count;
  object.line_measure_gray_min = inputDebug.gray_min;
  object.line_measure_gray_max = inputDebug.gray_max;
  object.line_measure_gray_mean = inputDebug.gray_mean;
  object.line_measure_max_gradient = inputDebug.max_gradient;
  object.line_measure_image_source = inputDebug.image_source;
  object.line_measure_input_failure_stage = inputDebug.failure_stage;
  object.line_measure_input_detail = inputDebug.detail;
  object.line_measure_failure_stage = inputDebug.failure_stage;
  object.line_measure_failure_hint = inputDebug.detail;
  object.line_measure_source = inputDebug.measure_source;
  object.line_measure_fallback_allowed = inputDebug.fallback_allowed;
  object.line_measure_fallback_used = inputDebug.fallback_used;
  object.line_measure_original_failure_stage =
      inputDebug.original_failure_stage;
  object.line_measure_original_detail = inputDebug.original_detail;
  object.line_measure_original_point_count = inputDebug.original_point_count;
  object.line_measure_original_edgeband_count =
      inputDebug.original_edgeband_count;
  object.line_measure_original_chain_length = inputDebug.original_chain_length;
  object.line_measure_geometry_request_valid =
      inputDebug.measure_geometry_request_valid;
  object.line_measure_geometry_dirty = inputDebug.measure_geometry_dirty;
  object.line_measure_geometry_ready = inputDebug.measure_geometry_ready;
  object.line_measure_geometry_version = inputDebug.measure_geometry_version;
  object.line_measure_geometry_built_version =
      inputDebug.measure_geometry_built_version;
  object.line_measure_geometry_half_width =
      inputDebug.measure_geometry_half_width;
  object.line_original_scan_w_count = inputDebug.original_scan_w_count;
  object.line_original_scan_h_count = inputDebug.original_scan_h_count;
  object.line_original_scan_w_length = inputDebug.original_scan_w_length;
  object.line_original_scan_h_length = inputDebug.original_scan_h_length;
  object.line_original_process_width = inputDebug.original_process_width;
  object.line_scan_rows_examined = inputDebug.scan_rows_examined;
  object.line_scan_rows_with_foreground = inputDebug.scan_rows_with_foreground;
  object.line_scan_runs_total = inputDebug.scan_runs_total;
  object.line_scan_runs_within_length_limit =
      inputDebug.scan_runs_within_length_limit;
  object.line_scan_runs_over_length_limit =
      inputDebug.scan_runs_over_length_limit;
  object.line_scan_runs_rejected_by_selection =
      inputDebug.scan_runs_rejected_by_selection;
  object.line_scan_runs_rejected_near_endpoint =
      inputDebug.scan_runs_rejected_near_endpoint;
  object.line_scan_points_emitted = inputDebug.scan_points_emitted;
  object.line_point_consistency_enabled = inputDebug.point_consistency_enabled;
  object.line_point_consistency_range = inputDebug.point_consistency_range;
  object.line_point_consistency_input_points =
      inputDebug.point_consistency_input_points;
  object.line_point_consistency_output_points =
      inputDebug.point_consistency_output_points;
  object.line_point_consistency_removed_points =
      inputDebug.point_consistency_removed_points;
  object.line_measure_backimage_ready = inputDebug.backimage_ready;
  object.line_measure_findobject_ready = inputDebug.findobject_ready;
  object.line_measure_objfilterset = inputDebug.objfilterset;
  object.line_measure_filter_borw = inputDebug.filter_borw;
  object.line_measure_filter_min = inputDebug.filter_min;
  object.line_measure_filter_max = inputDebug.filter_max;
  object.line_measure_filter_profile = inputDebug.filter_profile;
  object.line_measure_filter_explicit = inputDebug.filter_explicit;
  object.line_measure_effective_filter_borw = inputDebug.effective_filter_borw;
  object.line_measure_effective_filter_min = inputDebug.effective_filter_min;
  object.line_measure_effective_filter_max = inputDebug.effective_filter_max;
  object.line_measure_findobject_called = inputDebug.findobject_measure_called;
  object.line_measure_findobject_skipped =
      inputDebug.findobject_measure_skipped;
  object.line_measure_binary_foreground_pixels =
      inputDebug.binary_foreground_pixels;
  object.line_measure_binary_roi_width = inputDebug.binary_roi_width;
  object.line_measure_binary_roi_height = inputDebug.binary_roi_height;
  object.line_measure_result_empty_reason = inputDebug.result_empty_reason;
  object.line_findobject_component_total =
      inputDebug.findobject_component_total;
  object.line_findobject_component_accepted =
      inputDebug.findobject_component_accepted;
  object.line_findobject_component_rejected_by_min =
      inputDebug.findobject_component_rejected_by_min;
  object.line_findobject_component_rejected_by_max =
      inputDebug.findobject_component_rejected_by_max;
  object.line_findobject_component_rejected_by_borw =
      inputDebug.findobject_component_rejected_by_borw;
  object.line_findobject_area_min_observed =
      inputDebug.findobject_area_min_observed;
  object.line_findobject_area_max_observed =
      inputDebug.findobject_area_max_observed;
  object.line_findobject_area_mean_observed =
      inputDebug.findobject_area_mean_observed;
  object.line_findobject_area_median =
      inputDebug.findobject_area_median_observed;
  object.line_findobject_area_p90 = inputDebug.findobject_area_p90_observed;
  object.line_profile_point_count = profileStats.point_count;
  object.line_edgeband_count = profileStats.edgeband_count;
  object.line_chain_length = profileStats.chain_length;
  object.line_fit_status = line.getfitstatus();
  object.line_fit_mode = std::to_string(line.getfitmodevalue());

  object.has_fit_line = line.hasfitresult();
  if (object.has_fit_line) {
    object.fit_line_x0 = static_cast<float>(line.getresultx0());
    object.fit_line_y0 = static_cast<float>(line.getresulty0());
    object.fit_line_x1 = static_cast<float>(line.getresultx1());
    object.fit_line_y1 = static_cast<float>(line.getresulty1());
    object.line_avgdist = static_cast<float>(line.getavgdist());
    object.runtime_state = "geometry_result_available";
  } else if (object.valid_line_points_count <= 0) {
    object.runtime_state = "runtime_result_unavailable";
    object.line_result_status = "no_measure_points";
    object.line_result_reason = object.line_measure_failure_stage.empty()
                                    ? "FindLine produced zero valid points."
                                    : object.line_measure_failure_stage + ": " +
                                          object.line_measure_failure_hint;
  } else {
    object.runtime_state = "fitline_unavailable";
    object.line_result_status = line.getfitstatus();
    object.line_result_reason =
        "FindLine produced measure points, but no fitted line is available.";
  }

  object.display_summary = BuildFindLineGeometrySummary(object);
}

static void FillRuntimeObjectFromFindEllipse(RuntimeObjectView &object,
                                             const std::string &name,
                                             FindEllipse &ellipse) {
  object = RuntimeObjectView{};
  object.name = name;
  object.type = "FindEllipse";
  object.exists_in_parser = true;
  object.last_runtime_status = "runtime_executed";
  object.runtime_state = "runtime_executed";
  object.visualizable = true;
  object.visual_source = "runtime_object";
  object.stale = false;

  FindEllipseDisplaySnapshot snapshot;
  if (ellipse.getdisplaysnapshot(snapshot)) {
    object.has_ellipse_roi = snapshot.has_roi;
    object.ellipse_cx = static_cast<float>(snapshot.center_x);
    object.ellipse_cy = static_cast<float>(snapshot.center_y);
    object.ellipse_rx = static_cast<float>(snapshot.radius_x);
    object.ellipse_ry = static_cast<float>(snapshot.radius_y);
    object.ellipse_inner_scale_percent = snapshot.inner_scale_percent;
    object.ellipse_inner_rx = static_cast<float>(snapshot.inner_radius_x);
    object.ellipse_inner_ry = static_cast<float>(snapshot.inner_radius_y);
    object.measure_points_count = snapshot.measure_points_count;
    object.valid_points_count = snapshot.measure_points_count;
    object.has_measure_points = snapshot.has_measure_points;

    object.ellipse_scan_line_count = snapshot.scan_line_count;
    object.ellipse_scan_line_length = snapshot.scan_line_length;
    object.ellipse_selected_edge_index = snapshot.selected_edge_index;
    object.ellipse_scan_lines_cross_outside_ellipse_count =
        snapshot.scan_lines_cross_outside_ellipse_count;
    object.ellipse_accepted_points_outside_ellipse_count =
        snapshot.accepted_points_outside_ellipse_count;
    object.ellipse_accepted_point_norm_min = snapshot.accepted_point_norm_min;
    object.ellipse_accepted_point_norm_avg = snapshot.accepted_point_norm_avg;
    object.ellipse_accepted_point_norm_max = snapshot.accepted_point_norm_max;
    object.ellipse_rejected_boundary_band_candidate_count =
        snapshot.rejected_boundary_band_candidate_count;
    object.ellipse_rejected_boundary_band_norm_min =
        snapshot.rejected_boundary_band_norm_min;
    object.ellipse_rejected_boundary_band_norm_avg =
        snapshot.rejected_boundary_band_norm_avg;
    object.ellipse_rejected_boundary_band_norm_max =
        snapshot.rejected_boundary_band_norm_max;
    object.ellipse_point_consistency_enabled =
        snapshot.point_consistency_enabled;
    object.ellipse_point_consistency_range = snapshot.point_consistency_range;
    object.ellipse_point_consistency_input_points =
        snapshot.point_consistency_input_points;
    object.ellipse_point_consistency_output_points =
        snapshot.point_consistency_output_points;
    object.ellipse_point_consistency_removed_points =
        snapshot.point_consistency_removed_points;
    object.ellipse_scan_geometry_policy = snapshot.scan_geometry_policy;
    object.ellipse_candidate_policy = snapshot.candidate_policy;

    if (!snapshot.measure_failure_stage.empty()) {
      object.ellipse_result_status = snapshot.measure_failure_stage;
      object.ellipse_result_reason =
          snapshot.measure_failure_reason +
          " params: gap=" + std::to_string(snapshot.gap) +
          ", linegap=" + std::to_string(snapshot.linegap) +
          ", threshold=" + std::to_string(snapshot.threshold) +
          ", method=" + std::to_string(snapshot.method) +
          ", selected_edge=" + std::to_string(snapshot.selected_edge_index) +
          ", scan_lines=" + std::to_string(snapshot.scan_line_count) +
          ", scan_len=" + std::to_string(snapshot.scan_line_length) +
          ", consistency=" +
          std::to_string(snapshot.point_consistency_enabled) + "/" +
          std::to_string(snapshot.point_consistency_range) +
          ", consistency_removed=" +
          std::to_string(snapshot.point_consistency_removed_points) +
          ", accepted_outside=" +
          std::to_string(snapshot.accepted_points_outside_ellipse_count) +
          ", accepted_norm=" +
          std::to_string(snapshot.accepted_point_norm_min) + "/" +
          std::to_string(snapshot.accepted_point_norm_avg) + "/" +
          std::to_string(snapshot.accepted_point_norm_max) +
          ", rejected_boundary_band=" +
          std::to_string(snapshot.rejected_boundary_band_candidate_count) +
          ", rejected_norm=" +
          std::to_string(snapshot.rejected_boundary_band_norm_min) + "/" +
          std::to_string(snapshot.rejected_boundary_band_norm_avg) + "/" +
          std::to_string(snapshot.rejected_boundary_band_norm_max);
    }
  }

  const PointsShape &points = ellipse.getresultpoints();
  CopyPointsToFloatXY(points, object.measure_points_xy);
  object.measure_points_count =
      static_cast<int>(object.measure_points_xy.size() / 2);
  object.valid_points_count = object.measure_points_count;
  object.has_measure_points = !object.measure_points_xy.empty();

  object.has_fit_ellipse = ellipse.hasfitresult() != 0.0;
  if (object.has_fit_ellipse) {
    object.fit_ellipse_cx = static_cast<float>(ellipse.getresultcentx());
    object.fit_ellipse_cy = static_cast<float>(ellipse.getresultcenty());
    object.fit_ellipse_rx = static_cast<float>(ellipse.getresultradiusx());
    object.fit_ellipse_ry = static_cast<float>(ellipse.getresultradiusy());
    object.fit_ellipse_angle_deg = static_cast<float>(ellipse.getresultangle());
    object.fit_ellipse_avgdist = static_cast<float>(ellipse.getavgdist());
    object.runtime_state = "geometry_result_available";
    object.ellipse_result_status = "fitellipse_available";
    object.ellipse_result_reason.clear();
  } else {
    object.runtime_state = object.has_measure_points
                               ? "fitellipse_unavailable"
                               : "runtime_result_unavailable";
    if (object.ellipse_result_status.empty()) {
      object.ellipse_result_status = object.has_measure_points
                                         ? "fitellipse_unavailable"
                                         : "no_measure_points";
    }
    if (object.ellipse_result_reason.empty()) {
      object.ellipse_result_reason =
          object.has_measure_points
              ? "FindEllipse produced measure points, but fitellipse result is "
                "unavailable."
              : "FindEllipse produced zero measure points.";
    }
  }

  object.display_summary = BuildGeometrySummary(object);
}

static std::string BuildRuntimeFeedbackReason(const RuntimeObjectView &object) {
  if (object.type == "FindLine") {
    if (object.has_fit_line) {
      return "FindLine " + object.name + " result available: valid_points=" +
             std::to_string(object.valid_line_points_count) +
             ", avgdist=" + std::to_string(object.line_avgdist);
    }

    std::string reason =
        "FindLine " + object.name + " has no fitted conclusion";
    reason +=
        ", valid_points=" + std::to_string(object.valid_line_points_count);
    if (!object.line_measure_failure_stage.empty())
      reason += ", failure_stage=" + object.line_measure_failure_stage;
    if (!object.line_measure_failure_hint.empty())
      reason += ", detail=" + object.line_measure_failure_hint;
    if (!object.line_result_reason.empty())
      reason += ", result_reason=" + object.line_result_reason;
    return reason;
  }

  if (object.type == "FindCircle") {
    if (object.has_fit_result)
      return "FindCircle " + object.name + " result available: valid_points=" +
             std::to_string(object.valid_points_count) +
             ", radius=" + std::to_string(object.fit_radius) +
             ", avgdist=" + std::to_string(object.fit_avgdist);

    return "FindCircle " + object.name +
           " has no fitted conclusion, valid_points=" +
           std::to_string(object.valid_points_count) + ", failure_stage=" +
           (object.circle_measure_failure_stage.empty()
                ? "(none)"
                : object.circle_measure_failure_stage) +
           ", detail=" +
           (object.circle_measure_detail.empty()
                ? "(none)"
                : object.circle_measure_detail);
  }

  if (object.type == "FindEllipse") {
    return "FindEllipse " + object.name +
           " roi=" + (object.has_ellipse_roi ? "available" : "missing") +
           ", measure_points=" + std::to_string(object.valid_points_count) +
           ", fit_status=" + object.ellipse_result_status +
           ", reason=" + object.ellipse_result_reason +
           ", scan_lines=" + std::to_string(object.ellipse_scan_line_count) +
           ", scan_len=" + std::to_string(object.ellipse_scan_line_length) +
           ", accepted_outside=" +
           std::to_string(
               object.ellipse_accepted_points_outside_ellipse_count) +
           ", accepted_norm=" +
           std::to_string(object.ellipse_accepted_point_norm_min) + "/" +
           std::to_string(object.ellipse_accepted_point_norm_avg) + "/" +
           std::to_string(object.ellipse_accepted_point_norm_max) +
           ", rejected_boundary_band=" +
           std::to_string(
               object.ellipse_rejected_boundary_band_candidate_count) +
           ", rejected_norm=" +
           std::to_string(object.ellipse_rejected_boundary_band_norm_min) +
           "/" +
           std::to_string(object.ellipse_rejected_boundary_band_norm_avg) +
           "/" +
           std::to_string(object.ellipse_rejected_boundary_band_norm_max) +
           ", scan_policy=" + object.ellipse_scan_geometry_policy +
           ", candidate_policy=" + object.ellipse_candidate_policy;
  }

  if (object.type == "FindSegmentation") {
    std::string reason =
        "FindSegmentation " + object.name + " status=" + object.runtime_state +
        ", backend=" + object.segmentation_backend +
        ", backend_status=" + object.segmentation_backend_status +
        ", contours=" + std::to_string(object.segmentation_contour_count) +
        ", area=" + std::to_string(object.segmentation_primary_area);
    if (object.segmentation_has_positive_point) {
      reason += ", positive=(" +
                std::to_string(object.segmentation_positive_x) + "," +
                std::to_string(object.segmentation_positive_y) + ")";
    }
    if (object.segmentation_has_negative_point) {
      reason += ", negative=(" +
                std::to_string(object.segmentation_negative_x) + "," +
                std::to_string(object.segmentation_negative_y) + ")";
    }
    if (!object.segmentation_reason.empty())
      reason += ", result_reason=" + object.segmentation_reason;
    if (object.segmentation_contour_count <= 0)
      reason += ", conclusion=boundary_unavailable";
    else
      reason += ", conclusion=boundary_available_pending_human_review";
    return reason;
  }

  return object.type + " " + object.name + " state=" + object.runtime_state;
}

} // namespace

void SeedDefaultManualGlobals(ManualTestContext &context,
                              const std::string &scriptPath) {
  auto set = [&](const char *name, int value) {
    context.runtime_int_vars[name] = value;
  };

  set("global_threshold", 20);
  set("global_method", 0);
  set("global_linegap", 6);
  set("global_wgap", 32);
  set("global_hgap", 8);
  set("global_tool_half_width", 32);
  // These budgets are read directly by the frozen FindCircle script.  They
  // are explicit Manual inputs, not an algorithm fallback: without the
  // matching external variables the parser stops before setcircle/measure.
  set("global_max_elapsed_ms", 2000);
  set("global_max_scan_lines", 256);
  set("global_max_samples", 4096);

  const bool isCircleScript =
      scriptPath.find("find_circle") != std::string::npos ||
      scriptPath.find("findcircle") != std::string::npos ||
      scriptPath.find("FindCircle") != std::string::npos;
  const bool isLineScript = scriptPath.find("find_line") != std::string::npos ||
                            scriptPath.find("findline") != std::string::npos ||
                            scriptPath.find("FindLine") != std::string::npos;
  const bool isEllipseScript =
      scriptPath.find("find_ellipse") != std::string::npos ||
      scriptPath.find("findellipse") != std::string::npos ||
      scriptPath.find("FindEllipse") != std::string::npos;
  const bool isRectScript = scriptPath.find("find_rect") != std::string::npos ||
                            scriptPath.find("findrect") != std::string::npos;
  const bool isFastMatchScript =
      scriptPath.find("fastmatch") != std::string::npos ||
      scriptPath.find("FastMatch") != std::string::npos;
  const bool isGridPatternScript =
      scriptPath.find("grid_pattern") != std::string::npos ||
      scriptPath.find("GridPatternClassTool") != std::string::npos;
  const bool isRegionPatternScript =
      scriptPath.find("region_pattern") != std::string::npos ||
      scriptPath.find("RegionPatternTool") != std::string::npos;
  const bool isSegmentationScript =
      scriptPath.find("find_segmentation") != std::string::npos ||
      scriptPath.find("findsegmentation") != std::string::npos ||
      scriptPath.find("FindSegmentation") != std::string::npos;

  const bool isVerticalLineScript =
      scriptPath.find("find_line_vertical") != std::string::npos ||
      scriptPath.find("findline_vertical") != std::string::npos;

  if (isVerticalLineScript) {
    set("global_roi_x0", 380);
    set("global_roi_y0", 120);
    set("global_roi_x1", 380);
    set("global_roi_y1", 820);
    set("global_wgap", 32);
    set("global_hgap", 8);
  } else {
    set("global_roi_x0", 120);
    set("global_roi_y0", 240);
    set("global_roi_x1", 980);
    set("global_roi_y1", 240);
  }

  if (isCircleScript) {
    // Match the original Run.cpp baseline geometry.  Manual/evidence runs
    // may override these globals from an accepted gauge or manifest target,
    // but the default must remain a known historical probe rather than a
    // new interpreted ROI.
    set("global_circle_cx", 850);
    set("global_circle_cy", 690);
    set("global_circle_px", 0);
    set("global_circle_py", 690);
    set("global_circle_inner_radius", 0);
    set("global_circle_outer_radius", 0);
    set("global_circle_ring_width", 0);
    set("global_findcircle_arc_enabled", 0);
    set("global_findcircle_arc_start_deg", 0);
    set("global_findcircle_arc_end_deg", 360);
    set("global_gap", 5);
    set("global_linegap", 3);
  } else if (isEllipseScript) {
    set("global_ellipse_x0", 600);
    set("global_ellipse_y0", 360);
    set("global_ellipse_x1", 930);
    set("global_ellipse_y1", 580);
    set("global_findellipse_inner_scale_percent", 0);
    set("global_gap", 5);
    set("global_linegap", 3);
    set("global_threshold", 8);
    set("global_method", 1);
  } else if (isRectScript) {
    set("global_roi_x", 120);
    set("global_roi_y", 120);
    set("global_roi_width", 640);
    set("global_roi_height", 480);
    set("global_gauge", 20);
    set("global_linegap", 3);
    set("global_threshold", 20);
    set("global_method", 0);
  } else if (isLineScript) {
    // Stage 2.5 baseline for the current dot-grid test image.  The GUI
    // default must match the verified Run/Headless call sequence; method=0
    // and SetWHgap(8,32) reproduce the "no output points" path.
    set("global_method", 2);
    set("global_wgap", 32);
    set("global_hgap", 8);
  }

  if (isFastMatchScript) {
    set("global_learn_roi_x", 120);
    set("global_learn_roi_y", 120);
    set("global_learn_roi_w", 120);
    set("global_learn_roi_h", 90);
    set("global_search_roi_x", 0);
    set("global_search_roi_y", 0);
    set("global_search_roi_w", 640);
    set("global_search_roi_h", 480);
    set("global_compare_gap", 20);
    set("global_objfilter", 1);
    set("global_fastmatch_learn_shared", 1);
    for (int dir = 0; dir < 4; ++dir) {
      const std::string suffix = "_" + std::to_string(dir);
      set(("global_fastmatch_learn_wgap" + suffix).c_str(), 32);
      set(("global_fastmatch_learn_hgap" + suffix).c_str(), 8);
      set(("global_fastmatch_learn_method" + suffix).c_str(), 0);
      set(("global_fastmatch_learn_threshold" + suffix).c_str(), 20);
      set(("global_fastmatch_learn_linegap" + suffix).c_str(), 6);
      set(("global_fastmatch_learn_objfilter" + suffix).c_str(), 1);
      set(("global_fastmatch_learn_compare_gap" + suffix).c_str(), 20);
    }
    set("global_find_num", 1);
    set("global_match_step_x", 10);
    set("global_match_step_y", 10);
    set("global_match_thre", 10);
    set("global_min_score_percent", 65);
    set("global_fastmatch_action", 3);

    // FastMatch direct scripts write these values back after learn/match.
    // CxScript assignments require their external destinations to be
    // registered before Compile(); Headless already does this, Manual did
    // not, which made the script fail after a successful learn call.
    set("global_learn_a_count", 0);
    set("global_learn_b_count", 0);
    set("global_learn_a2_count", 0);
    set("global_learn_b2_count", 0);
    set("global_learn_status_code", 0);
    set("global_match_count", 0);
    set("global_best_score", 0);
    set("global_model_point_count", 0);
  }

  if (isGridPatternScript) {
    set("global_learn_roi_x", 120);
    set("global_learn_roi_y", 120);
    set("global_learn_roi_w", 120);
    set("global_learn_roi_h", 90);
    set("global_search_roi_x", 0);
    set("global_search_roi_y", 0);
    set("global_search_roi_w", 640);
    set("global_search_roi_h", 480);
    set("global_grid_normalized_width", 48);
    set("global_grid_normalized_height", 48);
    set("global_grid_rows", 12);
    set("global_grid_cols", 12);
    set("global_grid_levels", 3);
    set("global_grid_orientation_bins", 8);
    set("global_grid_foreground_threshold", -1);
    set("global_grid_foreground_dark", 1);
    set("global_grid_equalize_contrast", 0);
    set("global_grid_active_foreground_percent", 5);
    set("global_grid_active_edge_percent", 3);
    set("global_grid_max_overlays", 96);
    set("global_grid_fusion_mode", 2);
    set("global_grid_status", 0);
    set("global_grid_active_cell_count", 0);
    set("global_grid_descriptor_dim", 0);
    set("global_grid_level_count", 0);
    set("global_grid_overlay_count", 0);
    set("global_grid_overlay_truncated", 0);
  }

  if (isSegmentationScript) {
    set("global_segmentation_mode", 2);
    set("global_segmentation_threshold_percent", 50);
    set("global_segmentation_positive_enabled", 1);
    set("global_segmentation_positive_x", 320);
    set("global_segmentation_positive_y", 260);
    set("global_segmentation_negative_enabled", 1);
    set("global_segmentation_negative_x", 80);
    set("global_segmentation_negative_y", 80);
    set("global_segmentation_prompt_x0", 120);
    set("global_segmentation_prompt_y0", 120);
    set("global_segmentation_prompt_x1", 980);
    set("global_segmentation_prompt_y1", 820);
  }

  if (isRegionPatternScript) {

    set("global_region_roi_x", 120);
    set("global_region_roi_y", 120);
    set("global_region_roi_w", 120);
    set("global_region_roi_h", 90);
    set("global_region_normalized_width", 32);
    set("global_region_normalized_height", 32);
    set("global_region_pooling_rows", 4);
    set("global_region_pooling_cols", 4);
    set("global_region_use_binary", 0);
    set("global_region_threshold", 128);
    set("global_region_foreground_dark", 1);
    set("global_region_max_overlays", 64);
    set("global_region_status", 0);
    set("global_region_descriptor_dim", 0);
    set("global_region_foreground_permille", 0);
    set("global_region_mean_permille", 0);
    set("global_region_std_permille", 0);
    set("global_region_pooling_rows_out", 0);
    set("global_region_pooling_cols_out", 0);
    set("global_region_overlay_count", 0);
    set("global_region_overlay_truncated", 0);
  }

  // The Script Editor and Gauge Workbench must start from the same value
  // snapshot.  Previously only runtime_int_vars were seeded, leaving the
  // visible/editable gauge at its zero-initialized geometry.
  ManualGaugeState gauge;
  gauge.source = "script_default";
  gauge.review_status = "editing";
  gauge.threshold = context.runtime_int_vars["global_threshold"];
  gauge.method = context.runtime_int_vars["global_method"];
  gauge.linegap = context.runtime_int_vars["global_linegap"];

  if (isCircleScript) {
    gauge.tool = "FindCircle";
    gauge.has_circle_gauge = true;
    gauge.circle_cx = context.runtime_int_vars["global_circle_cx"];
    gauge.circle_cy = context.runtime_int_vars["global_circle_cy"];
    gauge.circle_px = context.runtime_int_vars["global_circle_px"];
    gauge.circle_py = context.runtime_int_vars["global_circle_py"];
    gauge.gap = context.runtime_int_vars["global_gap"];
    gauge.radius = static_cast<int>(std::lround(
        std::hypot(static_cast<double>(gauge.circle_px - gauge.circle_cx),
                   static_cast<double>(gauge.circle_py - gauge.circle_cy))));
    // inner_radius / outer_radius are absolute radii.  Do not reseed them
    // as a cosmetic band: the same values are consumed by setcircle2().
    gauge.inner_radius =
        std::max(0, context.runtime_int_vars["global_circle_inner_radius"]);
    gauge.outer_radius = context.runtime_int_vars["global_circle_outer_radius"];
    if (gauge.outer_radius <= 0)
      gauge.outer_radius = gauge.radius;
    if (gauge.inner_radius >= gauge.outer_radius)
      gauge.inner_radius = std::max(0, gauge.outer_radius - 1);
    gauge.radius = gauge.outer_radius;
    gauge.circle_px = gauge.circle_cx + gauge.outer_radius;
    gauge.circle_py = gauge.circle_cy;
    gauge.circle_arc_enabled =
        context.runtime_int_vars["global_findcircle_arc_enabled"] != 0;
    gauge.circle_arc_start_deg =
        context.runtime_int_vars["global_findcircle_arc_start_deg"];
    gauge.circle_arc_end_deg =
        context.runtime_int_vars["global_findcircle_arc_end_deg"];
  } else if (isEllipseScript) {
    gauge.tool = "FindEllipse";
    gauge.has_ellipse_gauge = true;
    gauge.ellipse_x0 = context.runtime_int_vars["global_ellipse_x0"];
    gauge.ellipse_y0 = context.runtime_int_vars["global_ellipse_y0"];
    gauge.ellipse_x1 = context.runtime_int_vars["global_ellipse_x1"];
    gauge.ellipse_y1 = context.runtime_int_vars["global_ellipse_y1"];
    gauge.ellipse_inner_scale_percent =
        context.runtime_int_vars["global_findellipse_inner_scale_percent"];
    gauge.gap = context.runtime_int_vars["global_gap"];
  } else if (isLineScript) {
    gauge.tool = "FindLine";
    gauge.has_line_gauge = true;
    gauge.line_x0 = context.runtime_int_vars["global_roi_x0"];
    gauge.line_y0 = context.runtime_int_vars["global_roi_y0"];
    gauge.line_x1 = context.runtime_int_vars["global_roi_x1"];
    gauge.line_y1 = context.runtime_int_vars["global_roi_y1"];
    gauge.tool_half_width = context.runtime_int_vars["global_tool_half_width"];
    gauge.wgap = context.runtime_int_vars["global_wgap"];
    gauge.hgap = context.runtime_int_vars["global_hgap"];
  } else if (isGridPatternScript) {
    gauge.tool = "GridPatternClassTool";
    gauge.primary_object_type = "GridPatternClassTool";
    gauge.line_x0 = context.runtime_int_vars["global_learn_roi_x"];
    gauge.line_y0 = context.runtime_int_vars["global_learn_roi_y"];
    gauge.line_x1 =
        gauge.line_x0 + context.runtime_int_vars["global_learn_roi_w"];
    gauge.line_y1 =
        gauge.line_y0 + context.runtime_int_vars["global_learn_roi_h"];
  } else if (isRegionPatternScript) {
    gauge.tool = "RegionPatternTool";
    gauge.primary_object_type = "RegionPatternTool";
    gauge.line_x0 = context.runtime_int_vars["global_region_roi_x"];
    gauge.line_y0 = context.runtime_int_vars["global_region_roi_y"];
    gauge.line_x1 =
        gauge.line_x0 + context.runtime_int_vars["global_region_roi_w"];
    gauge.line_y1 =
        gauge.line_y0 + context.runtime_int_vars["global_region_roi_h"];
  } else if (isSegmentationScript) {
    gauge.tool = "FindSegmentation";
    gauge.primary_object_type = "FindSegmentation";
    gauge.primary_object_name = "m_seg";
    gauge.primary_object_status = "script_default";
    gauge.has_segmentation_prompt_rect = true;
    gauge.segmentation_prompt_x0 =
        context.runtime_int_vars["global_segmentation_prompt_x0"];
    gauge.segmentation_prompt_y0 =
        context.runtime_int_vars["global_segmentation_prompt_y0"];
    gauge.segmentation_prompt_x1 =
        context.runtime_int_vars["global_segmentation_prompt_x1"];
    gauge.segmentation_prompt_y1 =
        context.runtime_int_vars["global_segmentation_prompt_y1"];
    gauge.segmentation_mode =
        context.runtime_int_vars["global_segmentation_mode"];
    gauge.segmentation_threshold_percent =
        context.runtime_int_vars["global_segmentation_threshold_percent"];
    gauge.has_segmentation_positive_point =
        context.runtime_int_vars["global_segmentation_positive_enabled"] != 0;
    gauge.segmentation_positive_x =
        context.runtime_int_vars["global_segmentation_positive_x"];
    gauge.segmentation_positive_y =
        context.runtime_int_vars["global_segmentation_positive_y"];
    gauge.has_segmentation_negative_point =
        context.runtime_int_vars["global_segmentation_negative_enabled"] != 0;
    gauge.segmentation_negative_x =
        context.runtime_int_vars["global_segmentation_negative_x"];
    gauge.segmentation_negative_y =
        context.runtime_int_vars["global_segmentation_negative_y"];
  } else if (isFastMatchScript) {
    // FastMatch Key Parameter Controls read shared learn parameters from
    // ManualGaugeState and ROI/action parameters from runtime_int_vars.
    // Keep both stores initialized from the same script-default snapshot;
    // otherwise the first Learn click can re-inject stale/default gauge
    // values over edited FastMatch globals.
    gauge.tool = "FastMatch";
    gauge.primary_object_type = "FastMatch";
    gauge.primary_object_name = "m_match";
    gauge.primary_object_status = "script_default";
    gauge.line_x0 = context.runtime_int_vars["global_learn_roi_x"];
    gauge.line_y0 = context.runtime_int_vars["global_learn_roi_y"];
    gauge.line_x1 =
        gauge.line_x0 + context.runtime_int_vars["global_learn_roi_w"];
    gauge.line_y1 =
        gauge.line_y0 + context.runtime_int_vars["global_learn_roi_h"];
    gauge.wgap = context.runtime_int_vars["global_wgap"];
    gauge.hgap = context.runtime_int_vars["global_hgap"];
    gauge.filterprofile = context.runtime_int_vars["global_filterprofile"];
    gauge.findsetting = context.runtime_int_vars["global_findsetting"];
  }

  if (gauge.has_circle_gauge || gauge.has_line_gauge ||
      gauge.has_ellipse_gauge || isGridPatternScript || isRegionPatternScript ||
      isSegmentationScript || isFastMatchScript)
    context.current_gauge = gauge;
}

bool ViewController::QueryParserObjectExists(const std::string &type,
                                             const std::string &name) {
  return m_parserDebugBridge.QueryObjectExists(type, name);
}

bool ViewController::QueryParserDouble(const std::string &name, double &value) {
  return m_parserDebugBridge.QueryDouble(name, value);
}

void ViewController::initManualStateTestConsole() {
  m_manualTest.analyzed_text.clear();
  m_manualTest.editor_text.clear();
  m_manualTest.editor_dirty = false;
  m_manualTest.current_line = 0;
  m_manualTest.run_state = "ready";
  m_manualTest.debug_status = "PENDING";
  m_manualTest.debug_reason = "not executed";
  m_manualTest.runtime_current_status = "PENDING";
  m_manualTest.show_image = true;
  m_manualTest.case_directory = ResolveCaseDirectory().string();
  m_manualTest.catalog_path =
      "cxparser/cxscript/module/cximage/catalog/cximage_catalog.cxsc";
  m_manualTest.manifest_path =
      "cxparser/cxscript/module/cximage/image_manifest.cxsc";
}

bool ViewController::LoadBoundStateToManualConsole(
    const std::string &nodeId, const std::string &scriptPath,
    std::string &reason) {
  m_manualTest.active_script_case_name = nodeId;
  m_manualTest.active_script_case_path = scriptPath;
  if (scriptPath.empty()) {
    reason = "semantic node has no bound script path";
    m_manualTest.debug_status = "script_load_failed";
    m_manualTest.debug_reason = reason;
    return false;
  }

  const std::filesystem::path resolved = ResolveWorkspaceFile(scriptPath);
  std::string text;
  if (!ReadTextFile(resolved.string(), text)) {
    reason = "cannot read bound catalog script: " + resolved.string();
    m_manualTest.editor_text.clear();
    m_manualTest.loaded_script_path.clear();
    m_manualTest.script_file_path.clear();
    m_manualTest.editor_dirty = false;
    m_manualTest.analyzed_text.clear();
    m_manualTest.debug_status = "script_load_failed";
    m_manualTest.debug_reason = reason;
    return false;
  }

  m_manualTest.editor_text = text;
  m_manualTest.loaded_script_path = resolved.string();
  m_manualTest.script_file_path = resolved.string();
  m_manualTest.editor_source = "semantic_flow";
  m_manualTest.editor_dirty = false;
  m_manualTest.analyzed_text.clear();
  m_manualTest.current_line = 0;
  SeedDefaultManualGlobals(m_manualTest, scriptPath);
  m_manualTest.run_state = "ready";
  m_manualTest.debug_status = "script_loaded";
  m_manualTest.debug_reason = "loaded exact bound script: " + scriptPath;
  reason = m_manualTest.debug_reason;
  return true;
}

void ViewController::RefreshRuntimeObjectTable(
    const std::string &lastMethod, const std::string &runtimeStatus) {
  SetCxCrashBreadcrumb("RefreshRuntimeObjectTable:begin");
  m_manualTest.runtime_objects.clear();

  SetCxCrashBreadcrumb("RefreshRuntimeObjectTable:FindCircle:list");
  for (const std::string &name :
       m_parserDebugBridge.ListClassObjectNames("FindCircle")) {
    SetCxCrashBreadcrumb("RefreshRuntimeObjectTable:FindCircle:query:" + name);
    FindCircle *circle = static_cast<FindCircle *>(
        m_parserDebugBridge.QueryClassObject("FindCircle", name));
    if (circle == nullptr)
      continue;

    SetCxCrashBreadcrumb("RefreshRuntimeObjectTable:FindCircle:fill:" + name);
    RuntimeObjectView object;
    FillRuntimeObjectFromFindCircle(object, name, *circle);
    object.last_method = lastMethod;
    object.last_runtime_status = runtimeStatus;
    m_manualTest.runtime_objects.push_back(object);
  }

  SetCxCrashBreadcrumb("RefreshRuntimeObjectTable:FindLine:list");
  for (const std::string &name :
       m_parserDebugBridge.ListClassObjectNames("FindLine")) {
    SetCxCrashBreadcrumb("RefreshRuntimeObjectTable:FindLine:query:" + name);
    FindLine *line = static_cast<FindLine *>(
        m_parserDebugBridge.QueryClassObject("FindLine", name));
    if (line == nullptr)
      continue;

    SetCxCrashBreadcrumb("RefreshRuntimeObjectTable:FindLine:fill:" + name);
    RuntimeObjectView object;
    FillRuntimeObjectFromFindLine(object, name, *line);
    object.last_method = lastMethod;
    object.last_runtime_status = runtimeStatus;
    m_manualTest.runtime_objects.push_back(object);
  }

  SetCxCrashBreadcrumb("RefreshRuntimeObjectTable:FindEllipse:list");
  for (const std::string &name :
       m_parserDebugBridge.ListClassObjectNames("FindEllipse")) {
    SetCxCrashBreadcrumb("RefreshRuntimeObjectTable:FindEllipse:query:" + name);
    FindEllipse *ellipse = static_cast<FindEllipse *>(
        m_parserDebugBridge.QueryClassObject("FindEllipse", name));
    if (ellipse == nullptr)
      continue;

    SetCxCrashBreadcrumb("RefreshRuntimeObjectTable:FindEllipse:fill:" + name);
    RuntimeObjectView object;
    try {
      FillRuntimeObjectFromFindEllipse(object, name, *ellipse);
    } catch (const std::exception &ex) {
      object = RuntimeObjectView{};
      object.name = name;
      object.type = "FindEllipse";
      object.exists_in_parser = true;
      object.last_method = lastMethod;
      object.last_runtime_status = runtimeStatus;
      object.runtime_state = "runtime_object_refresh_failed";
      object.visualizable = false;
      object.stale = true;
      object.ellipse_result_status = "runtime_object_refresh_failed";
      object.ellipse_result_reason = ex.what();
      CXLOG_ERROR("ManualConsole", "findellipse_runtime_object_refresh",
                  "failed",
                  "object=" + name + " reason=" + std::string(ex.what()));
    } catch (...) {
      object = RuntimeObjectView{};
      object.name = name;
      object.type = "FindEllipse";
      object.exists_in_parser = true;
      object.last_method = lastMethod;
      object.last_runtime_status = runtimeStatus;
      object.runtime_state = "runtime_object_refresh_failed";
      object.visualizable = false;
      object.stale = true;
      object.ellipse_result_status = "runtime_object_refresh_failed";
      object.ellipse_result_reason = "unknown exception";
      CXLOG_ERROR("ManualConsole", "findellipse_runtime_object_refresh",
                  "failed", "object=" + name + " reason=unknown exception");
    }
    object.last_method = lastMethod;
    object.last_runtime_status = runtimeStatus;
    m_manualTest.runtime_objects.push_back(object);
  }

  SetCxCrashBreadcrumb("RefreshRuntimeObjectTable:FindRect:list");
  for (const std::string &name :
       m_parserDebugBridge.ListClassObjectNames("FindRect")) {
    SetCxCrashBreadcrumb("RefreshRuntimeObjectTable:FindRect:query:" + name);
    FindRect *rect = static_cast<FindRect *>(
        m_parserDebugBridge.QueryClassObject("FindRect", name));
    if (rect == nullptr)
      continue;

    RuntimeObjectView object;
    object.name = name;
    object.type = "FindRect";
    object.exists_in_parser = true;
    object.last_method = lastMethod;
    object.last_runtime_status = runtimeStatus;
    object.runtime_state = "runtime_object_available";
    object.stale = false;
    object.visualizable = true;
    object.visual_source = "runtime_object";
    object.measure_points_count = rect->getresultobjsnum();
    object.valid_points_count = rect->getresultobjsnum();
    object.display_summary = "FindRect " + name + " result_rects=" +
                             std::to_string(rect->getresultobjsnum());
    m_manualTest.runtime_objects.push_back(object);
  }

  SetCxCrashBreadcrumb("RefreshRuntimeObjectTable:FindObject:list");
  for (const std::string &name :
       m_parserDebugBridge.ListClassObjectNames("FindObject")) {
    FindObject *find_object = static_cast<FindObject *>(
        m_parserDebugBridge.QueryClassObject("FindObject", name));
    if (find_object == nullptr)
      continue;

    RuntimeObjectView object;
    object.name = name;
    object.type = "FindObject";
    object.exists_in_parser = true;
    object.last_method = lastMethod;
    object.last_runtime_status = runtimeStatus;
    object.runtime_state = "component_result_available";
    object.stale = false;
    object.visualizable = true;
    object.visual_source = "component_rectangles";
    object.measure_points_count = find_object->getresultobjsnum();
    object.valid_points_count = find_object->getresultobjsnum();
    object.display_summary =
        "FindObject " + name +
        " components=" + std::to_string(find_object->getdebugcomponentcount()) +
        " accepted=" + std::to_string(find_object->getdebugacceptedcount()) +
        " result_rects=" + std::to_string(find_object->getresultobjsnum());
    m_manualTest.runtime_objects.push_back(object);
  }

  SetCxCrashBreadcrumb("RefreshRuntimeObjectTable:FindSegmentation:list");
  for (const std::string &name :
       m_parserDebugBridge.ListClassObjectNames("FindSegmentation")) {
    SetCxCrashBreadcrumb("RefreshRuntimeObjectTable:FindSegmentation:query:" +
                         name);
    FindSegmentation *seg = static_cast<FindSegmentation *>(
        m_parserDebugBridge.QueryClassObject("FindSegmentation", name));
    if (seg == nullptr)
      continue;

    RuntimeObjectView object;
    object.name = name;
    object.type = "FindSegmentation";
    object.exists_in_parser = true;
    object.last_method = lastMethod;
    object.last_runtime_status = runtimeStatus;
    object.runtime_state = seg->m_status;
    object.segmentation_backend = seg->backend();
    object.segmentation_backend_status = seg->result().backend_status;
    object.segmentation_device = seg->device();
    object.segmentation_model_path = seg->model_path();
    object.segmentation_status_code = seg->status_code();
    object.segmentation_contour_count = seg->get_contour_count();
    object.segmentation_primary_area = seg->get_primary_area();
    object.segmentation_result_ref = seg->get_result();
    object.segmentation_mask_ref = seg->get_mask_ref();
    object.segmentation_contour_ref = seg->get_contour_ref();
    object.segmentation_overlay_ref = seg->get_overlay_ref();
    object.segmentation_reason = seg->m_reason;
    const FindSegmentationInputSnapshot &input = seg->lastinputrequest();
    object.segmentation_has_prompt_rect = input.has_rect;
    object.segmentation_has_positive_point = input.has_positive_point;
    object.segmentation_has_negative_point = input.has_negative_point;
    object.segmentation_positive_x = input.positive_point_x;
    object.segmentation_positive_y = input.positive_point_y;
    object.segmentation_negative_x = input.negative_point_x;
    object.segmentation_negative_y = input.negative_point_y;
    object.segmentation_has_boundary = object.segmentation_contour_count > 0;
    object.segmentation_has_libtorch_contract =
        object.segmentation_backend_status == "libtorch_contract_ready" ||
        object.segmentation_backend_status == "libtorch_segmentation_ready";
    object.segmentation_real_mask_attach_ready =
        object.segmentation_has_libtorch_contract &&
        object.segmentation_status_code != 0 &&
        object.segmentation_contour_count > 0 &&
        !object.segmentation_mask_ref.empty() &&
        !object.segmentation_overlay_ref.empty() &&
        !object.segmentation_contour_ref.empty();
    object.stale = false;
    object.visualizable = true;
    object.visual_source = object.segmentation_has_boundary
                               ? "segmentation_boundary"
                               : "segmentation_prompt";
    object.has_measure_points = seg->get_contour_count() > 0;
    object.measure_points_count = seg->get_contour_count();
    object.valid_points_count = seg->get_contour_count();
    object.display_summary =
        "FindSegmentation " + name + " status=" + seg->m_status +
        " contours=" + std::to_string(seg->get_contour_count()) +
        " area=" + std::to_string(seg->get_primary_area());
    m_manualTest.runtime_objects.push_back(object);
  }

  SetCxCrashBreadcrumb("RefreshRuntimeObjectTable:TorchTask:list");
  for (const std::string &name :
       m_parserDebugBridge.ListClassObjectNames("TorchTask")) {
    SetCxCrashBreadcrumb("RefreshRuntimeObjectTable:TorchTask:query:" + name);
    TorchTask *task = static_cast<TorchTask *>(
        m_parserDebugBridge.QueryClassObject("TorchTask", name));
    if (task == nullptr)
      continue;

    RuntimeObjectView object;
    object.name = name;
    object.type = "TorchTask";
    object.exists_in_parser = true;
    object.last_method = lastMethod;
    object.last_runtime_status = runtimeStatus;
    object.runtime_state = task->getstatus();
    object.is_torch_task = true;
    object.torch_ok = task->getok();
    object.torch_error_code = task->geterrorcode();
    object.torch_result_count = task->getresultcount();
    object.torch_mask_available = task->getmaskavailable();
    object.torch_infer_ms = task->getinferms();
    object.torch_train_ms = task->gettrainms();
    object.torch_total_ms = task->gettotalms();
    object.torch_status = task->getstatus();
    object.torch_reason = task->getreason();
    object.torch_failure_stage = task->getfailstage();
    object.torch_actual_device = task->getactualdevice();
    object.torch_result_ref = task->getresultref();
    object.torch_evidence_ref = task->getevidenceref();
    object.torch_primary_visual_ref = task->getprimaryvisualref();
    object.torch_mask_ref = task->getmaskref();
    object.torch_overlay_ref = task->getoverlayref();
    object.torch_trainer_lifecycle_summary = task->gettrainersummary();
    object.torch_unified_mainline_summary = task->getmainlinesummary();
    object.stale = false;
    object.visualizable = true;
    object.visual_source = object.torch_mask_available != 0
                               ? "torch_mask_overlay"
                               : "torch_runtime_evidence";
    object.measure_points_count = object.torch_result_count;
    object.valid_points_count = object.torch_result_count;
    object.display_summary =
        "TorchTask " + name + " status=" + object.torch_status +
        " device=" + object.torch_actual_device +
        " results=" + std::to_string(object.torch_result_count) +
        " mask=" + std::to_string(object.torch_mask_available);
    m_manualTest.runtime_objects.push_back(object);
  }

  SetCxCrashBreadcrumb("RefreshRuntimeObjectTable:FastMatch:list");
  for (const std::string &name :
       m_parserDebugBridge.ListClassObjectNames("FastMatch")) {
    SetCxCrashBreadcrumb(
        "RefreshRuntimeObjectTable:FastMatch:query:FastMatch:" + name);
    FastMatch *matcher = static_cast<FastMatch *>(
        m_parserDebugBridge.QueryClassObject("FastMatch", name));
    if (matcher == nullptr)
      continue;

    RuntimeObjectView object;
    object.name = name;
    object.type = "FastMatch";
    object.exists_in_parser = true;
    object.last_method = lastMethod;
    object.last_runtime_status = runtimeStatus;
    object.runtime_state = "runtime_object_available";
    object.stale = false;
    object.visualizable = true;
    object.visual_source = "runtime_object";
    object.has_fastmatch_diagnostic = true;
    object.fastmatch_status = "runtime_object_available";
    object.fastmatch_result_ref = "runtime_object:" + name;
    object.fastmatch_model_point_count = matcher->getmodelpointcount();
    object.fastmatch_learn_a_count = matcher->getlearnacount();
    object.fastmatch_learn_b_count = matcher->getlearnbcount();
    object.fastmatch_learn_a2_count = matcher->getlearna2count();
    object.fastmatch_learn_b2_count = matcher->getlearnb2count();
    object.fastmatch_learn_status_code = matcher->getlearnstatuscode();
    object.fastmatch_pattern_a_count = matcher->getpatternapointcount();
    object.fastmatch_pattern_b_count = matcher->getpatternbpointcount();
    object.fastmatch_candidate_count = matcher->getresultcandidatecount();
    object.fastmatch_best_score = matcher->getresultbestscore();
    object.fastmatch_learn_rect_x0 = matcher->getlearnrectx0();
    object.fastmatch_learn_rect_y0 = matcher->getlearnrecty0();
    object.fastmatch_learn_rect_x1 = matcher->getlearnrectx1();
    object.fastmatch_learn_rect_y1 = matcher->getlearnrecty1();
    object.fastmatch_match_rect_x0 = matcher->getmatchrectx0();
    object.fastmatch_match_rect_y0 = matcher->getmatchrecty0();
    object.fastmatch_match_rect_x1 = matcher->getmatchrectx1();
    object.fastmatch_match_rect_y1 = matcher->getmatchrecty1();
    object.measure_points_count = object.fastmatch_model_point_count;
    object.valid_points_count = object.fastmatch_candidate_count;
    object.display_summary =
        "FastMatch " + name + " learn_rect=(" +
        std::to_string(object.fastmatch_learn_rect_x0) + "," +
        std::to_string(object.fastmatch_learn_rect_y0) + "," +
        std::to_string(object.fastmatch_learn_rect_x1) + "," +
        std::to_string(object.fastmatch_learn_rect_y1) + ")" + " match_rect=(" +
        std::to_string(object.fastmatch_match_rect_x0) + "," +
        std::to_string(object.fastmatch_match_rect_y0) + "," +
        std::to_string(object.fastmatch_match_rect_x1) + "," +
        std::to_string(object.fastmatch_match_rect_y1) + ")" +
        " model_points=" + std::to_string(object.fastmatch_model_point_count) +
        " learnA=" + std::to_string(object.fastmatch_learn_a_count) +
        " learnB=" + std::to_string(object.fastmatch_learn_b_count) +
        " learnA2=" + std::to_string(object.fastmatch_learn_a2_count) +
        " learnB2=" + std::to_string(object.fastmatch_learn_b2_count) +
        " learn_status=" + std::to_string(object.fastmatch_learn_status_code) +
        " patternA=" + std::to_string(object.fastmatch_pattern_a_count) +
        " patternB=" + std::to_string(object.fastmatch_pattern_b_count) +
        " candidates=" + std::to_string(object.fastmatch_candidate_count) +
        " best_score=" + std::to_string(object.fastmatch_best_score);
    m_manualTest.runtime_objects.push_back(object);
  }

  SetCxCrashBreadcrumb("RefreshRuntimeObjectTable:GridPatternClassTool:list");
  for (const std::string &name :
       m_parserDebugBridge.ListClassObjectNames("GridPatternClassTool")) {
    GridPatternClassTool *grid_tool = static_cast<GridPatternClassTool *>(
        m_parserDebugBridge.QueryClassObject("GridPatternClassTool", name));
    if (grid_tool == nullptr)
      continue;

    RuntimeObjectView object;
    object.name = name;
    object.type = "GridPatternClassTool";
    object.exists_in_parser = true;
    object.last_method = lastMethod;
    object.last_runtime_status = runtimeStatus;
    object.runtime_state = grid_tool->getstatuscode() == 1
                               ? "grid_feature_available"
                               : "grid_feature_unavailable";
    object.stale = false;
    object.visualizable = true;
    object.visual_source = "runtime_object";
    object.has_grid_pattern = true;
    object.grid_pattern_status_code = grid_tool->getstatuscode();
    object.grid_pattern_active_cell_count = grid_tool->getactivecellcount();
    object.grid_pattern_descriptor_dim = grid_tool->getdescriptordim();
    object.grid_pattern_level_count = grid_tool->getlevelcount();
    object.grid_pattern_overlay_count = grid_tool->getoverlaycount();
    object.grid_pattern_overlay_truncated =
        grid_tool->getoverlaytruncated() != 0;
    object.grid_pattern_elapsed_ms = grid_tool->getelapsedms();
    object.grid_pattern_summary = grid_tool->getsummary();
    object.measure_points_count = object.grid_pattern_active_cell_count;
    object.valid_points_count = object.grid_pattern_active_cell_count;
    object.display_summary = object.grid_pattern_summary;
    m_manualTest.runtime_objects.push_back(object);
  }

  SetCxCrashBreadcrumb("RefreshRuntimeObjectTable:RegionPatternTool:list");
  for (const std::string &name :
       m_parserDebugBridge.ListClassObjectNames("RegionPatternTool")) {
    RegionPatternTool *region_tool = static_cast<RegionPatternTool *>(
        m_parserDebugBridge.QueryClassObject("RegionPatternTool", name));
    if (region_tool == nullptr)
      continue;

    RuntimeObjectView object;
    object.name = name;
    object.type = "RegionPatternTool";
    object.exists_in_parser = true;
    object.last_method = lastMethod;
    object.last_runtime_status = runtimeStatus;
    object.runtime_state = region_tool->getstatuscode() == 1
                               ? "region_descriptor_available"
                               : "region_descriptor_unavailable";
    object.stale = false;
    object.visualizable = true;
    object.visual_source = "runtime_object";
    object.has_region_pattern = true;
    object.region_pattern_status_code = region_tool->getstatuscode();
    object.region_pattern_descriptor_dim = region_tool->getdescriptordim();
    object.region_pattern_foreground_permille =
        region_tool->getforegroundpermille();
    object.region_pattern_mean_permille = region_tool->getmeanpermille();
    object.region_pattern_std_permille = region_tool->getstdpermille();
    object.region_pattern_pooling_rows = region_tool->getpoolingrows();
    object.region_pattern_pooling_cols = region_tool->getpoolingcols();
    object.region_pattern_overlay_count = region_tool->getoverlaycount();
    object.region_pattern_overlay_truncated =
        region_tool->getoverlaytruncated() != 0;
    object.region_pattern_elapsed_ms = region_tool->getelapsedms();
    object.region_pattern_summary = region_tool->getsummary();
    object.measure_points_count = object.region_pattern_overlay_count;
    object.valid_points_count = object.region_pattern_overlay_count;
    object.display_summary = object.region_pattern_summary;
    m_manualTest.runtime_objects.push_back(object);
  }

  SetCxCrashBreadcrumb("RefreshRuntimeObjectTable:summary");
  for (RuntimeObjectView &object : m_manualTest.runtime_objects) {
    if (object.stale)
      continue;

    object.display_summary = BuildGeometrySummary(object);
  }

  m_manualTest.geometry_summary = "";
  m_manualTest.image_overlay_summary = "";

  for (const RuntimeObjectView &object : m_manualTest.runtime_objects) {
    if (object.visualizable && !object.stale) {
      m_manualTest.geometry_summary += BuildGeometrySummary(object) + "\n";
      m_manualTest.image_overlay_summary +=
          BuildOverlaySummary(m_manualTest, object) + "\n";
    }
  }

  SetCxCrashBreadcrumb("RefreshRuntimeObjectTable:shape_sync_request");
  RequestRuntimeShapeSync("RefreshRuntimeObjectTable");

  SetCxCrashBreadcrumb("RefreshRuntimeObjectTable:result_ref");
  m_manualTest.current_result_ref = ResultRefView();
  m_scriptResult.result_ref.clear();
  m_scriptResult.overlay_ref.clear();
  for (const RuntimeObjectView &object : m_manualTest.runtime_objects) {
    if (!object.visualizable || object.stale)
      continue;

    m_manualTest.current_result_ref.source_object = object.name;
    m_manualTest.current_result_ref.value = "runtime_object:" + object.name;
    m_manualTest.current_result_ref.status = "runtime_object_available";

    if (object.type == "FindCircle") {
      m_manualTest.current_result_ref.name = "global_circle_ref";
      m_manualTest.current_result_ref.result_type = "FindCircleResult";
      m_manualTest.current_result_ref.status = object.has_fit_result
          ? "geometry_result_available"
          : object.runtime_state;
      m_manualTest.current_result_ref.reason = object.has_fit_result
          ? "bound to runtime object geometry result"
          : object.display_summary;
      m_manualTest.current_result_ref.fit_cx = object.fit_cx;
      m_manualTest.current_result_ref.fit_cy = object.fit_cy;
      m_manualTest.current_result_ref.fit_radius = object.fit_radius;
      m_manualTest.current_result_ref.avgdist = object.fit_avgdist;
      m_manualTest.current_result_ref.points_count =
          object.measure_points_count;
      m_manualTest.current_result_ref.valid_points_count =
          object.valid_points_count;
    } else if (object.type == "FindLine") {
      m_manualTest.current_result_ref.name = "global_line_ref";
      m_manualTest.current_result_ref.result_type = "FindLineResult";
      m_manualTest.current_result_ref.line_x0 = object.fit_line_x0;
      m_manualTest.current_result_ref.line_y0 = object.fit_line_y0;
      m_manualTest.current_result_ref.line_x1 = object.fit_line_x1;
      m_manualTest.current_result_ref.line_y1 = object.fit_line_y1;
      m_manualTest.current_result_ref.line_avgdist = object.line_avgdist;
      m_manualTest.current_result_ref.line_points_count =
          object.line_measure_points_count;
      m_manualTest.current_result_ref.valid_line_points_count =
          object.valid_points_count;
    } else if (object.type == "FindEllipse") {
      m_manualTest.current_result_ref.name = "global_ellipse_ref";
      m_manualTest.current_result_ref.result_type = "FindEllipseResult";
      m_manualTest.current_result_ref.status =
          object.ellipse_result_status.empty()
              ? (object.has_fit_ellipse ? "fitellipse_available"
                                        : "no_measure_points")
              : object.ellipse_result_status;
      m_manualTest.current_result_ref.fit_cx = object.fit_ellipse_cx;
      m_manualTest.current_result_ref.fit_cy = object.fit_ellipse_cy;
      m_manualTest.current_result_ref.fit_radius = object.fit_ellipse_rx;
      m_manualTest.current_result_ref.avgdist = object.fit_ellipse_avgdist;
      m_manualTest.current_result_ref.points_count =
          object.measure_points_count;
      m_manualTest.current_result_ref.valid_points_count =
          object.valid_points_count;
      m_manualTest.current_result_ref.reason = object.ellipse_result_reason;
    } else if (object.type == "FastMatch") {
      m_manualTest.current_result_ref.name = "global_match_ref";
      m_manualTest.current_result_ref.result_type = "FastMatchResult";
      m_manualTest.current_result_ref.value =
          object.fastmatch_result_ref.empty()
              ? ("runtime_object:" + object.name)
              : object.fastmatch_result_ref;
      m_manualTest.current_result_ref.status = object.fastmatch_status.empty()
                                                   ? "runtime_object_available"
                                                   : object.fastmatch_status;
      m_manualTest.current_result_ref.points_count =
          object.measure_points_count;
      m_manualTest.current_result_ref.valid_points_count =
          object.valid_points_count;
    } else if (object.type == "GridPatternClassTool") {
      m_manualTest.current_result_ref.name = "global_grid_ref";
      m_manualTest.current_result_ref.result_type = "GridPatternFeatureResult";
      m_manualTest.current_result_ref.status = object.runtime_state;
      m_manualTest.current_result_ref.reason = object.grid_pattern_summary;
      m_manualTest.current_result_ref.points_count =
          object.grid_pattern_active_cell_count;
      m_manualTest.current_result_ref.valid_points_count =
          object.grid_pattern_overlay_count;
    } else if (object.type == "RegionPatternTool") {
      m_manualTest.current_result_ref.name = "global_region_ref";
      m_manualTest.current_result_ref.result_type =
          "RegionPatternDescriptorResult";
      m_manualTest.current_result_ref.status = object.runtime_state;
      m_manualTest.current_result_ref.reason = object.region_pattern_summary;
      m_manualTest.current_result_ref.points_count =
          object.region_pattern_descriptor_dim;
      m_manualTest.current_result_ref.valid_points_count =
          object.region_pattern_overlay_count;
    } else if (object.type == "FindSegmentation") {
      m_manualTest.current_result_ref.name = "global_segmentation_ref";
      m_manualTest.current_result_ref.result_type = "FindSegmentationResult";
      m_manualTest.current_result_ref.value =
          object.segmentation_result_ref.empty()
              ? ("runtime_object:" + object.name)
              : object.segmentation_result_ref;
      m_manualTest.current_result_ref.status =
          object.segmentation_contour_count > 0
              ? "boundary_available_pending_human_review"
              : "boundary_unavailable";
      m_manualTest.current_result_ref.points_count =
          object.segmentation_contour_count;
      m_manualTest.current_result_ref.valid_points_count =
          object.segmentation_contour_count;
      m_manualTest.current_result_ref.reason =
          BuildRuntimeFeedbackReason(object);
    } else if (object.type == "TorchTask") {
      m_manualTest.current_result_ref.name = "global_torch_ref";
      m_manualTest.current_result_ref.result_type = "TorchTaskResult";
      m_manualTest.current_result_ref.value =
          object.torch_result_ref.empty() ? ("runtime_object:" + object.name)
                                          : object.torch_result_ref;
      m_manualTest.current_result_ref.status = object.torch_status.empty()
                                                   ? object.runtime_state
                                                   : object.torch_status;
      m_manualTest.current_result_ref.reason = object.torch_reason.empty()
                                                   ? object.torch_failure_stage
                                                   : object.torch_reason;
      m_manualTest.current_result_ref.points_count = object.torch_result_count;
      m_manualTest.current_result_ref.valid_points_count =
          object.torch_result_count;
    }

    m_scriptResult.result_ref = m_manualTest.current_result_ref.value;
    m_scriptResult.overlay_ref =
        "shape_owner:" + object.type + ":" + object.name;
    break;
  }

  if (runtimeStatus == "runtime_executed") {
    for (const RuntimeObjectView &object : m_manualTest.runtime_objects) {
      if (object.stale)
        continue;

      if (object.type == "FindLine" && !object.has_fit_line) {
        m_manualTest.debug_status = "runtime_executed_without_result";
        m_manualTest.debug_reason = BuildRuntimeFeedbackReason(object);
        m_scriptResult.status = "PENDING_REVIEW";
        m_scriptResult.reason = m_manualTest.debug_reason;
        if (m_manualTest.current_result_ref.source_object.empty()) {
          m_manualTest.current_result_ref.source_object = object.name;
          m_manualTest.current_result_ref.name = "global_line_ref";
          m_manualTest.current_result_ref.result_type = "FindLineResult";
          m_manualTest.current_result_ref.value =
              "runtime_object:" + object.name;
          m_manualTest.current_result_ref.status = "runtime_result_unavailable";
          m_manualTest.current_result_ref.line_points_count =
              object.line_measure_points_count;
          m_manualTest.current_result_ref.valid_line_points_count =
              object.valid_points_count;
        }
        break;
      }

      if (object.type == "FindCircle" && !object.has_fit_result) {
        m_manualTest.debug_status = "runtime_executed_without_result";
        m_manualTest.debug_reason = BuildRuntimeFeedbackReason(object);
        m_scriptResult.status = "PENDING_REVIEW";
        m_scriptResult.reason = m_manualTest.debug_reason;
        if (m_manualTest.current_result_ref.source_object.empty()) {
          m_manualTest.current_result_ref.source_object = object.name;
          m_manualTest.current_result_ref.name = "global_circle_ref";
          m_manualTest.current_result_ref.result_type = "FindCircleResult";
          m_manualTest.current_result_ref.value =
              "runtime_object:" + object.name;
          m_manualTest.current_result_ref.status = "runtime_result_unavailable";
          m_manualTest.current_result_ref.points_count =
              object.measure_points_count;
          m_manualTest.current_result_ref.valid_points_count =
              object.valid_points_count;
        }
        break;
      }

      if (object.type == "FindEllipse" && !object.has_fit_ellipse) {
        m_manualTest.debug_status = "runtime_executed_without_fitellipse";
        m_manualTest.debug_reason = BuildRuntimeFeedbackReason(object);
        m_scriptResult.status = "PENDING_REVIEW";
        m_scriptResult.reason = m_manualTest.debug_reason;
        if (m_manualTest.current_result_ref.source_object.empty()) {
          m_manualTest.current_result_ref.source_object = object.name;
          m_manualTest.current_result_ref.name = "global_ellipse_ref";
          m_manualTest.current_result_ref.result_type = "FindEllipseResult";
          m_manualTest.current_result_ref.value =
              "runtime_object:" + object.name;
          m_manualTest.current_result_ref.status = object.ellipse_result_status;
          m_manualTest.current_result_ref.points_count =
              object.measure_points_count;
          m_manualTest.current_result_ref.valid_points_count =
              object.valid_points_count;
          m_manualTest.current_result_ref.reason = object.ellipse_result_reason;
        }
        break;
      }

      if (object.type == "FindSegmentation") {
        const bool hasBoundary = object.segmentation_contour_count > 0;
        const bool promptQualityFail =
            object.segmentation_backend_status == "prompt_quality_fail" ||
            object.runtime_state == "prompt_quality_fail";
        const std::string segmentationResultStatus =
            hasBoundary ? "boundary_available_pending_human_review"
                        : (promptQualityFail ? "prompt_quality_fail"
                                             : "boundary_unavailable");

        m_manualTest.debug_status =
            hasBoundary ? "runtime_result_available" : segmentationResultStatus;
        m_manualTest.debug_reason = BuildRuntimeFeedbackReason(object);
        m_scriptResult.status = hasBoundary ? "PENDING_REVIEW" : "FAIL";
        m_scriptResult.reason = m_manualTest.debug_reason;
        if (m_manualTest.current_result_ref.source_object.empty()) {
          m_manualTest.current_result_ref.source_object = object.name;
          m_manualTest.current_result_ref.name = "global_segmentation_ref";
          m_manualTest.current_result_ref.result_type =
              "FindSegmentationResult";
          m_manualTest.current_result_ref.value =
              object.segmentation_result_ref.empty()
                  ? ("runtime_object:" + object.name)
                  : object.segmentation_result_ref;
          m_manualTest.current_result_ref.status = segmentationResultStatus;
          m_manualTest.current_result_ref.points_count =
              object.segmentation_contour_count;
          m_manualTest.current_result_ref.valid_points_count =
              object.segmentation_contour_count;
          m_manualTest.current_result_ref.reason =
              BuildRuntimeFeedbackReason(object);
        }
        break;
      }
    }
  }

  SetCxCrashBreadcrumb("RefreshRuntimeObjectTable:end");
}

void ViewController::drawManualStateTestConsole() {
  ImGui::SetNextWindowPos(ImVec2(8, 8), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(520, 380), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Manual State Test Console", nullptr,
                    ImGuiWindowFlags_NoCollapse)) {
    ImGui::End();
    return;
  }

  ImGui::Text("Script: %s", m_manualTest.loaded_script_path.c_str());
  ApplyAiGuiFocusHere(
      AiGuiDestination::ManualScriptConsole,
      "Manual State Test Console > script editor and debug compiler");

  ImGui::Text("Image: %s", m_manualTest.image_file_path.c_str());
  ImGui::Text("Run State: %s", m_manualTest.run_state.c_str());
  ImGui::Text("Debug Status: %s", m_manualTest.debug_status.c_str());
  ImGui::Text("Debug Reason: %s", m_manualTest.debug_reason.c_str());

  ImGui::Separator();

  DrawScriptEditorBlock(m_manualTest);

  ImGui::Separator();

  DrawScriptDebugCompilerBlock(m_manualTest);

  ImGui::Separator();

  DrawCxParserExtLineViewsPanel(m_manualTest);
  DrawCxParserExtStatementViewsPanel(m_manualTest);
  DrawCxParserExtObjectAssignmentsPanel(m_manualTest);

  ImGui::End();
}

void ViewController::drawKeyParameterControlsWindow() {
  ImGui::SetNextWindowPos(ImVec2(840, 8), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(520, 520), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Key Parameter Controls", nullptr,
                    ImGuiWindowFlags_NoCollapse)) {
    ImGui::End();
    return;
  }

  ApplyAiGuiFocusHere(
      AiGuiDestination::KeyParameters,
      "Key Parameter Controls > active tool parameter controls");
  if (IsTorchContext(m_manualTest) &&
      !IsFindLineFindCircleContext(m_manualTest)) {
    std::string promptSyncReason;
    SyncFindSegmentationPromptListsFromShapeElements(promptSyncReason);
    DrawTorchAnnotationKeyParameterPanel(m_manualTest);
  } else if (IsFindLineFindCircleContext(m_manualTest)) {
    DrawKeyParameterControlPanel(m_manualTest, &m_parserDebugBridge);
    if (m_manualTest.apply_gauge_to_shape_requested) {
      m_manualTest.apply_gauge_to_shape_requested = false;
      const bool preservePendingFastMatchRun =
          m_manualTest.debug_status == "FASTMATCH_RUN_REQUESTED" &&
          m_manualTest.has_pending_execution_snapshot;
      std::string reason;
      if (ApplyCurrentGaugeToEditableShape(reason)) {
        if (preservePendingFastMatchRun) {
          if (!m_manualTest.debug_reason.empty())
            m_manualTest.debug_reason += "; ";
          m_manualTest.debug_reason += reason;
        } else {
          m_manualTest.debug_status = "GAUGE_SHAPE_APPLIED";
          m_manualTest.debug_reason = reason;
        }
      } else {
        if (preservePendingFastMatchRun) {
          // FastMatch Learn/Match is a staged serial parser run.  The
          // ROI edit may fail to apply back to an editable Shape in
          // some transient UI states, but the already captured
          // pending_execution_snapshot is the authoritative input for
          // the next Run pass.  Do not overwrite
          // FASTMATCH_RUN_REQUESTED here; otherwise the Learn button
          // logs a request but the Debug Compiler never consumes it.
          if (!m_manualTest.debug_reason.empty())
            m_manualTest.debug_reason += "; ";
          m_manualTest.debug_reason +=
              "shape apply skipped before FastMatch run: " + reason;
          CXLOG_INFO("KeyParameterControls", "fastmatch_pending_run_preserved",
                     "PENDING_RUN", "reason=" + reason);
        } else {
          m_manualTest.debug_status = "GAUGE_SHAPE_APPLY_FAILED";
          m_manualTest.debug_reason = reason;
        }
      }
    }
  } else {
    DrawKeyParameterUnavailableNotice(m_manualTest);
  }

  if (!m_manualTest.pending_annotation_tool_id.empty()) {
    const std::string requestedTool = m_manualTest.pending_annotation_tool_id;
    const std::string requestedReason =
        m_manualTest.pending_annotation_tool_reason;
    m_manualTest.pending_annotation_tool_id.clear();
    m_manualTest.pending_annotation_tool_reason.clear();

    std::string reason;
    if (requestedTool == "__pointer_pan__") {
      TestSetToolModePointerPan();
      m_annotationStatus =
          requestedReason.empty()
              ? "annotation tool disabled from Key Parameter Controls"
              : requestedReason;
      m_manualTest.debug_status = "ANNOTATION_TOOL_POINTER_PAN";
      m_manualTest.debug_reason = m_annotationStatus;
      CXLOG_INFO("KeyParameterControls", "annotation_tool_request", "applied",
                 "tool=PointerPan reason=" + m_annotationStatus);
    } else if (TestSetActiveAnnotationTool(requestedTool, reason)) {
      m_annotationStatus = requestedReason.empty()
                               ? ("annotation tool selected: " + requestedTool)
                               : requestedReason;
      m_manualTest.debug_status = "ANNOTATION_TOOL_SELECTED";
      m_manualTest.debug_reason =
          "tool=" + requestedTool + "; " + m_annotationStatus;
      CXLOG_INFO("KeyParameterControls", "annotation_tool_request", "applied",
                 "tool=" + requestedTool + " reason=" + m_annotationStatus);
    } else {
      m_annotationStatus = reason;
      m_manualTest.debug_status = "ANNOTATION_TOOL_SELECT_FAILED";
      m_manualTest.debug_reason = "tool=" + requestedTool + "; " + reason;
      CXLOG_INFO("KeyParameterControls", "annotation_tool_request", "failed",
                 "tool=" + requestedTool + " reason=" + reason);
    }
  }

  ImGui::End();
}

void ViewController::drawMetrologyAnalyticsSmokeWindow() {
  ImGui::SetNextWindowPos(ImVec2(840, 1060), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(760, 520), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Analytics Smoke / Metrology Bridge", nullptr,
                    ImGuiWindowFlags_NoCollapse)) {
    ImGui::End();
    return;
  }

  ApplyAiGuiFocusHere(
      AiGuiDestination::AnalyticsSmoke,
      "Analytics Smoke / Metrology Bridge > analytics controls");
  cxvision::metrology_analytics::DrawManualConsoleAnalyticsSmokePanel(
      m_manualTest.analytics_smoke_ui);

  ImGui::End();
}

void ViewController::drawTorchRuntimeEvidenceWindow() {
  ImGui::SetNextWindowPos(ImVec2(1380, 8), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(620, 720), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Torch Runtime / Evidence", nullptr,
                    ImGuiWindowFlags_NoCollapse)) {
    ImGui::End();
    return;
  }

  ImGui::TextWrapped("Torch layer is separated from geometry key parameters. "
                     "Use this window for model/runtime/artifact/prompt/review "
                     "visibility.");
  ImGui::Separator();

  ApplyAiGuiFocusHere(
      AiGuiDestination::TorchRuntimeEvidence,
      "Torch Runtime / Evidence > runtime status and review controls");
  DrawTorchKeyStatusPanel(m_manualTest);
  ImGui::Separator();
  DrawTorchEvidenceAndReviewPanel(m_manualTest);

  ImGui::End();
}

void ViewController::drawParameterTuningAndConclusionWindow() {
  ImGui::SetNextWindowPos(ImVec2(840, 540), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(760, 430), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Parameter Tuning Map / Result Conclusion", nullptr,
                    ImGuiWindowFlags_NoCollapse)) {
    ImGui::End();
    return;
  }

  ApplyAiGuiFocusHere(
      AiGuiDestination::ParameterConclusion,
      "Parameter Tuning Map / Result Conclusion > first available control");
  if (IsFindLineFindCircleContext(m_manualTest)) {
    DrawParamTuningScatterPanel(m_manualTest);
  }
  DrawConclusionSummaryPanel(m_manualTest);

  ImGui::End();
}

void ViewController::drawEvidenceAlbumWindow() {
  ImGui::SetNextWindowPos(ImVec2(8, 1000), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(820, 420), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Evidence Album / Case Chain", nullptr,
                    ImGuiWindowFlags_NoCollapse)) {
    ImGui::End();
    return;
  }

  DrawEvidenceCaseListPanel(m_manualTest);

  ImGui::End();
}

void ViewController::drawAnnotationToolWindow() {
  ImGui::SetNextWindowPos(ImVec2(1610, 8), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(310, 960), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Annotation Tool Palette / Tool Inspector", nullptr,
                    ImGuiWindowFlags_NoCollapse)) {
    ImGui::End();
    return;
  }

  ImGui::Text("Manifest path");
  ImGui::SetNextItemWidth(280.0f);
  InputTextString("##manifest_path", m_annotationManifestPath);
  if (ImGui::Button("Load Tool Manifest")) {
    std::string reason;
    CxAnnotationToolManifestSnapshot snapshot;
    if (!m_parserOwner.ParseAnnotationToolManifest(m_annotationManifestPath,
                                                   snapshot, reason)) {
      m_annotationStatus = "parse failed: " + reason;
    } else if (!m_annotationLayer.ApplyToolManifestSnapshot(snapshot, reason)) {
      m_annotationStatus = "apply failed: " + reason;
    } else {
      m_annotationStatus = reason;
    }
  }

  ImGui::Separator();
  ImGui::Text("Tool enabled: %s | mode: %s", m_imageToolEnabled ? "YES" : "NO",
              ImageToolModeName(m_imageToolMode));
  const AnnotationToolDefinition *activeTool = m_annotationLayer.ActiveTool();
  if (activeTool != nullptr) {
    ImGui::TextWrapped("Active: %s | shape=%s | role=%s",
                       activeTool->label.empty() ? activeTool->name.c_str()
                                                 : activeTool->label.c_str(),
                       activeTool->shape_type.c_str(),
                       activeTool->role.c_str());
  } else {
    ImGui::TextDisabled("Active tool id: (none)");
  }
  ImGui::Text("ShapeElements: %d",
              static_cast<int>(m_annotationLayer.ShapeElements().size()));
  ImGui::TextWrapped(
      "Last pointer: %s | %s | %s", m_lastPointerResult.phase.c_str(),
      m_lastPointerResult.status.c_str(), m_lastPointerResult.reason.c_str());

  if (ImGui::CollapsingHeader("Tool Palette Buttons", 0)) {
    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f),
                       "Main palette is now docked above Image View.");
    DrawAnnotationToolButtonStrip(false);
  }

  if (ImGui::CollapsingHeader("Element List (ShapeElements)", 0)) {
    ImGui::BeginChild("annotation_elements", ImVec2(-1, 150), true);
    int elemIndex = 0;
    for (const auto &elem : m_annotationLayer.ShapeElements()) {
      ImGui::PushID(elemIndex++);
      ImGui::Text("%s | id=%d | tool=%s | role=%s", elem.stable_ref.c_str(),
                  elem.id, elem.tool_id.c_str(), elem.semantic_role.c_str());
      ImGui::PopID();
    }
    if (m_annotationLayer.ShapeElements().empty())
      ImGui::TextDisabled("No shape elements");
    ImGui::EndChild();
  }

  ImGui::Separator();
  ImGui::Text("Session path");
  ImGui::SetNextItemWidth(280.0f);
  InputTextString("##session_path", m_annotationSessionPath);

  if (ImGui::Button("Save Elements"))
    m_annotationStatus = "saving...";
  ImGui::SameLine();
  if (ImGui::Button("Load Elements"))
    m_annotationStatus = "loading...";
  ImGui::SameLine();
  if (ImGui::Button("Clear Elements")) {
    m_annotationLayer.Clear();
    m_annotationStatus = "cleared";
  }

  ImGui::Text("Status: %s", m_annotationStatus.c_str());
  ImGui::End();
}

void ViewController::DrawAnnotationToolButtonStrip(bool horizontal) {
  auto toolModeFromDefinition = [](const AnnotationToolDefinition &tool) {
    if (tool.kind == OverlayKind::Point)
      return ImageToolMode::PointCreate;
    if (tool.kind == OverlayKind::Line)
      return ImageToolMode::LineCreate;
    if (tool.kind == OverlayKind::Rect)
      return ImageToolMode::RectCreate;
    if (tool.kind == OverlayKind::Circle)
      return ImageToolMode::CircleCreate;
    if (tool.kind == OverlayKind::Ellipse)
      return ImageToolMode::EllipseCreate;
    if (tool.kind == OverlayKind::Polyline ||
        tool.kind == OverlayKind::BoundaryPolyline)
      return ImageToolMode::PolylineCreate;
    if (tool.kind == OverlayKind::AutoBoundaryRequest ||
        tool.action == "auto_segmentation")
      return ImageToolMode::AutoBoundary;
    return ImageToolMode::PointerPan;
  };

  auto drawButton = [&](const char *id, const std::string &label, bool active,
                        const ImVec2 &size,
                        const std::function<void()> &onClick) {
    ImGui::PushID(id);
    if (active) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.50f, 0.85f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                            ImVec4(0.25f, 0.60f, 0.95f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                            ImVec4(0.12f, 0.40f, 0.75f, 1.0f));
    }
    if (ImGui::Button(label.c_str(), size))
      onClick();
    if (active)
      ImGui::PopStyleColor(3);
    ImGui::PopID();
  };

  const float width = horizontal ? 112.0f : -1.0f;
  const ImVec2 buttonSize(width, 26.0f);
  const float availableWidth = ImGui::GetContentRegionAvail().x;
  float usedWidth = 0.0f;

  auto nextSameLine = [&](float itemWidth) {
    if (!horizontal)
      return;
    usedWidth += itemWidth + ImGui::GetStyle().ItemSpacing.x;
    if (usedWidth + itemWidth <= availableWidth)
      ImGui::SameLine();
    else
      usedWidth = 0.0f;
  };

  const bool hasSelectedTorchTrainingImage =
      m_manualTest.selected_torch_training_image >= 0 &&
      m_manualTest.selected_torch_training_image <
          static_cast<int>(m_manualTest.torch_training_images.size());

  drawButton("annotation_session_toggle",
             m_imageToolEnabled ? "Annot ON" : "Annot OFF", m_imageToolEnabled,
             buttonSize, [this]() {
               m_imageToolEnabled = !m_imageToolEnabled;
               if (!m_imageToolEnabled) {
                 m_imageToolMode = ImageToolMode::PointerPan;
                 CancelAnnotationCreate();
                 m_annotationLayer.SetActiveToolIndex(-1);
                 m_annotationStatus = "annotation session disabled";
               } else {
                 m_imageToolMode = ImageToolMode::PointerPan;
                 CancelAnnotationCreate();
                 m_annotationLayer.SetActiveToolIndex(-1);
                 m_annotationStatus = "annotation session enabled; choose a "
                                      "tool or edit existing shapes";
               }
             });
  nextSameLine(width);

  drawButton("annotation_edit_existing", "Edit Existing",
             m_imageToolEnabled &&
                 m_imageToolMode == ImageToolMode::PointerPan &&
                 m_annotationLayer.ActiveToolIndex() < 0,
             buttonSize, [this]() {
               m_imageToolEnabled = true;
               m_imageToolMode = ImageToolMode::PointerPan;
               CancelAnnotationCreate();
               m_annotationLayer.SetActiveToolIndex(-1);
               m_annotationStatus =
                   "annotation edit mode active; click a shape handle to drag";
             });
  nextSameLine(width);

  if (hasSelectedTorchTrainingImage) {
    TorchTrainingImageItem &selected =
        m_manualTest
            .torch_training_images[m_manualTest.selected_torch_training_image];

    auto drawLabelButton = [&](const char *id, const char *label) {
      const bool active = selected.label == label;
      drawButton(id, std::string("Label ") + label, active, buttonSize,
                 [this, &selected, label]() {
                   selected.label = label;
                   m_manualTest.torch_training_image_status =
                       "ANNOTATION_LABEL_UPDATED";
                   m_manualTest.torch_training_image_reason =
                       "selected image label=" + selected.label +
                       " path=" + selected.image_path;
                 });
      nextSameLine(width);
    };

    drawLabelButton("annotation_label_good", "good");
    drawLabelButton("annotation_label_anomaly", "anomaly");
    drawLabelButton("annotation_label_unlabeled", "unlabeled");
    drawLabelButton("annotation_label_pending", "pending");
  }

  ImGui::Text("Annotation: %s | mode: %s | tool: %s | status: %s",
              m_imageToolEnabled ? "enabled" : "disabled",
              ImageToolModeName(m_imageToolMode),
              m_annotationLayer.ActiveTool()
                  ? (m_annotationLayer.ActiveTool()->label.empty()
                         ? m_annotationLayer.ActiveTool()->name.c_str()
                         : m_annotationLayer.ActiveTool()->label.c_str())
                  : "Pointer / Pan",
              m_annotationStatus.c_str());

  const bool pointerActive = !m_imageToolEnabled ||
                             m_imageToolMode == ImageToolMode::PointerPan ||
                             m_annotationLayer.ActiveToolIndex() < 0;
  drawButton("Pointer / Pan", "Pointer / Pan", pointerActive, buttonSize,
             [this]() {
               m_imageToolEnabled = false;
               m_imageToolMode = ImageToolMode::PointerPan;
               CancelAnnotationCreate();
               m_annotationLayer.SetActiveToolIndex(-1);
               m_annotationStatus = "Pointer / Pan active";
             });
  nextSameLine(width);

  for (int i = 0; i < static_cast<int>(m_annotationLayer.Tools().size()); ++i) {
    const AnnotationToolDefinition &tool = m_annotationLayer.Tools()[i];
    if (!tool.manual_visible)
      continue;

    const bool active =
        m_imageToolEnabled && m_annotationLayer.ActiveToolIndex() == i;
    const std::string label = tool.label.empty() ? tool.name : tool.label;
    drawButton(tool.name.c_str(), label, active, buttonSize,
               [this, i, active, label, tool, &toolModeFromDefinition]() {
                 if (active) {
                   m_imageToolEnabled = false;
                   m_imageToolMode = ImageToolMode::PointerPan;
                   CancelAnnotationCreate();
                   m_annotationLayer.SetActiveToolIndex(-1);
                   m_annotationStatus = label + " disabled";
                 } else {
                   m_imageToolEnabled = true;
                   m_imageToolMode = toolModeFromDefinition(tool);
                   CancelAnnotationCreate();
                   m_annotationLayer.SetActiveToolIndex(i);
                   m_annotationStatus = "enabled tool_id=" + tool.name +
                                        " shape=" + tool.shape_type +
                                        " role=" + tool.role +
                                        " action=" + tool.action;
                 }
               });
    nextSameLine(width);
  }

  if (horizontal)
    ImGui::NewLine();
}