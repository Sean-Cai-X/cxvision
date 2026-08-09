#include "pch.h"
#include "measurement_semantics/CxMeasurementSemanticEvidenceWriter.h"
#include "CxScriptHeadlessRuntime.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <vector>

namespace
{
std::string JsonEscapeLocal(const std::string& value)
{
    std::ostringstream out;
    for (char ch : value)
    {
        switch (ch)
        {
        case '\\': out << "\\\\"; break;
        case '"': out << "\\\""; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (static_cast<unsigned char>(ch) < 0x20)
                out << "\\u00" << std::hex << static_cast<int>(static_cast<unsigned char>(ch));
            else
                out << ch;
            break;
        }
    }
    return out.str();
}

bool WriteTextAtomic(
    const std::filesystem::path& path,
    const std::string& text,
    std::string& out_reason)
{
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec)
    {
        out_reason = "failed to create measurement semantic directory: " + ec.message();
        return false;
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open())
    {
        out_reason = "failed to open measurement semantic file: " + path.string();
        return false;
    }
    file << text;
    file.flush();
    if (!file.good())
    {
        out_reason = "failed to write measurement semantic file: " + path.string();
        return false;
    }
    out_reason.clear();
    return true;
}

std::string BoolText(bool value)
{
    return value ? "true" : "false";
}

std::string ResolveToolName(
    const CxScriptExecutionCapture& capture,
    const CxScriptHeadlessOptions& options)
{
    if (!options.stage25_tool.empty())
        return options.stage25_tool;

    for (const CxShapeElementSnapshot& shape : capture.shapes)
    {
        if (!shape.owner_type.empty())
            return shape.owner_type;
    }

    const std::string& script = options.script_path;
    const auto contains = [&script](const char* token)
    {
        return script.find(token) != std::string::npos;
    };
    if (contains("findcircle") || contains("FindCircle"))
        return "FindCircle";
    if (contains("findellipse") || contains("FindEllipse"))
        return "FindEllipse";
    if (contains("findrect") || contains("FindRect"))
        return "FindRect";
    if (contains("fastmatch") || contains("FastMatch"))
        return "FastMatch";
    if (contains("segmentation") || contains("FindSegmentation"))
        return "FindSegmentation";
    return "FindLine";
}

std::vector<std::string> CollectObjectRefs(const CxScriptExecutionCapture& capture)
{
    std::set<std::string> unique;
    for (const CxShapeElementSnapshot& shape : capture.shapes)
    {
        if (!shape.owner_ref.empty())
            unique.insert(shape.owner_type + ":" + shape.owner_ref);
    }
    return std::vector<std::string>(unique.begin(), unique.end());
}

std::map<std::string, int> CountShapeRoles(const CxScriptExecutionCapture& capture)
{
    std::map<std::string, int> counts;
    for (const CxShapeElementSnapshot& shape : capture.shapes)
    {
        const std::string role = shape.semantic_role.empty() ? "unknown" : shape.semantic_role;
        ++counts[role];
    }
    return counts;
}

double SafeRatio(double numerator, double denominator)
{
    return denominator == 0.0 ? 0.0 : numerator / denominator;
}

std::string EvidenceStatus(const CxScriptExecutionCapture& capture)
{
    if (!capture.runtime_completed)
        return "RUNTIME_NOT_COMPLETED";
    if (capture.budget_exceeded)
        return "BUDGET_EXCEEDED";
    if (!capture.failure_stage.empty())
        return "RUNTIME_WITH_FAILURE_STAGE";
    return "MEASUREMENT_SEMANTIC_CAPTURE_AVAILABLE";
}

void WriteStringArray(std::ostream& out, const std::vector<std::string>& values)
{
    out << "[";
    for (size_t i = 0; i < values.size(); ++i)
    {
        if (i > 0)
            out << ", ";
        out << "\"" << JsonEscapeLocal(values[i]) << "\"";
    }
    out << "]";
}

std::string BuildMeasurementSemanticInput(
    const CxScriptExecutionCapture& capture,
    const CxScriptHeadlessOptions& options,
    const std::string& tool,
    const std::vector<std::string>& object_refs)
{
    std::ostringstream out;
    out << "{\n";
    out << "  \"schema\": \"cxvision.measurement_semantic_input.v1\",\n";
    out << "  \"status\": \"" << EvidenceStatus(capture) << "\",\n";
    out << "  \"case_id\": \"" << JsonEscapeLocal(options.case_name.empty() ? options.case_id : options.case_name) << "\",\n";
    out << "  \"image_id\": \"" << JsonEscapeLocal(options.image_id.empty() ? options.stage25_image_id : options.image_id) << "\",\n";
    out << "  \"target_id\": \"" << JsonEscapeLocal(options.target_id.empty() ? options.stage25_target_id : options.target_id) << "\",\n";
    out << "  \"tool\": \"" << JsonEscapeLocal(tool) << "\",\n";
    out << "  \"script_path\": \"" << JsonEscapeLocal(options.script_path) << "\",\n";
    out << "  \"image_path\": \"" << JsonEscapeLocal(options.image_path) << "\",\n";
    out << "  \"result_summary_ref\": \"result_summary.json\",\n";
    out << "  \"object_refs\": ";
    WriteStringArray(out, object_refs);
    out << ",\n";
    out << "  \"facts_source_priority\": [\"input_image\", \"gauge_parameter_snapshot\", \"runtime_capture\", \"shape_snapshot\", \"sidecar_observation\"]\n";
    out << "}\n";
    return out.str();
}

std::string BuildCalibrationSnapshot(const CxScriptHeadlessOptions& options)
{
    std::ostringstream out;
    out << "{\n";
    out << "  \"schema\": \"cxvision.calibration_snapshot.v1\",\n";
    out << "  \"status\": \"CALIBRATION_NOT_BOUND\",\n";
    out << "  \"coordinate_frame\": \"image_pixel\",\n";
    out << "  \"unit\": \"px\",\n";
    out << "  \"image_id\": \"" << JsonEscapeLocal(options.image_id.empty() ? options.stage25_image_id : options.image_id) << "\",\n";
    out << "  \"policy\": \"Identity pixel transform is used until a reviewed CxCalibration snapshot is bound.\"\n";
    out << "}\n";
    return out.str();
}

std::string BuildCoordinateTransformTrace()
{
    return
        "{\n"
        "  \"schema\": \"cxvision.coordinate_transform_trace.v1\",\n"
        "  \"status\": \"IDENTITY_PIXEL_TRANSFORM\",\n"
        "  \"steps\": [\n"
        "    {\"step\":\"image_pixel_to_measurement_pixel\", \"scale_x\":1.0, \"scale_y\":1.0, \"offset_x\":0.0, \"offset_y\":0.0, \"rotation_deg\":0.0}\n"
        "  ]\n"
        "}\n";
}

std::string BuildBehaviorTrace(
    const CxScriptExecutionCapture& capture,
    const std::string& tool)
{
    std::ostringstream out;
    out << "{\n";
    out << "  \"schema\": \"cxvision.measurement_behavior_trace.v1\",\n";
    out << "  \"tool\": \"" << JsonEscapeLocal(tool) << "\",\n";
    out << "  \"status\": \"" << EvidenceStatus(capture) << "\",\n";
    out << "  \"steps\": [\n";
    out << "    {\"index\":0, \"name\":\"script_execute\", \"status\":\"" << (capture.runtime_completed ? "done" : "not_completed") << "\"},\n";
    out << "    {\"index\":1, \"name\":\"gauge_and_parameters\", \"status\":\"captured\", \"method\":" << capture.tool_method
        << ", \"threshold\":" << capture.tool_threshold
        << ", \"gap\":" << capture.tool_linegap << "},\n";
    out << "    {\"index\":2, \"name\":\"sampling\", \"status\":\"captured\", \"scan_line_count\":" << capture.scan_line_count
        << ", \"sample_count\":" << capture.sample_count
        << ", \"valid_points_count\":" << capture.valid_points_count << "},\n";
    out << "    {\"index\":3, \"name\":\"fit\", \"status\":\"captured\", \"has_fit_line\":" << BoolText(capture.has_fit_line)
        << ", \"has_fit_circle\":" << BoolText(capture.has_fit_circle)
        << ", \"has_fit_ellipse\":" << BoolText(capture.has_fit_ellipse)
        << ", \"has_result_rect\":" << BoolText(capture.has_result_rect) << "},\n";
    out << "    {\"index\":4, \"name\":\"shape_projection\", \"status\":\"captured\", \"shape_count\":" << capture.shapes.size() << "}\n";
    out << "  ]\n";
    out << "}\n";
    return out.str();
}

std::string BuildObservations(
    const CxScriptExecutionCapture& capture,
    const std::string& tool)
{
    std::ostringstream out;
    out << "{\n";
    out << "  \"schema\": \"cxvision.measurement_observations.v1\",\n";
    out << "  \"tool\": \"" << JsonEscapeLocal(tool) << "\",\n";
    out << "  \"observations\": [\n";
    out << "    {\"name\":\"valid_points_count\", \"value\":" << capture.valid_points_count << ", \"unit\":\"count\", \"source\":\"runtime_capture\"},\n";
    out << "    {\"name\":\"fit_available\", \"value\":" << ((capture.has_fit_line || capture.has_fit_circle || capture.has_fit_ellipse || capture.has_result_rect) ? 1 : 0) << ", \"unit\":\"bool\", \"source\":\"runtime_capture\"},\n";
    out << "    {\"name\":\"avgdist\", \"value\":" << capture.avgdist << ", \"unit\":\"px\", \"source\":\"runtime_capture\"},\n";
    out << "    {\"name\":\"circle_radius\", \"value\":" << capture.circle_radius << ", \"unit\":\"px\", \"source\":\"runtime_capture\"},\n";
    out << "    {\"name\":\"rendered_measure_points_count\", \"value\":" << capture.rendered_measure_points_count << ", \"unit\":\"count\", \"source\":\"overlay_renderer\"},\n";
    out << "    {\"name\":\"result_overlay_changed_pixels\", \"value\":" << capture.result_overlay_changed_pixels << ", \"unit\":\"px\", \"source\":\"overlay_renderer\"}\n";
    out << "  ]\n";
    out << "}\n";
    return out.str();
}

std::string BuildRelations(
    const CxScriptExecutionCapture& capture,
    const std::string& tool,
    const std::map<std::string, int>& role_counts)
{
    std::ostringstream out;
    out << "{\n";
    out << "  \"schema\": \"cxvision.measurement_relations.v1\",\n";
    out << "  \"tool\": \"" << JsonEscapeLocal(tool) << "\",\n";
    out << "  \"relations\": [\n";
    out << "    {\"predicate\":\"tool_has_measure_points\", \"subject\":\"runtime_tool\", \"object\":\"measure_points\", \"value\":"
        << BoolText(capture.valid_points_count > 0) << "},\n";
    out << "    {\"predicate\":\"tool_has_fit_result\", \"subject\":\"runtime_tool\", \"object\":\"fit_result\", \"value\":"
        << BoolText(capture.has_fit_line || capture.has_fit_circle || capture.has_fit_ellipse || capture.has_result_rect) << "},\n";
    out << "    {\"predicate\":\"shape_roles_available\", \"subject\":\"shape_snapshot\", \"object\":\"runtime_tool\", \"roi_count\":"
        << (role_counts.count("roi") ? role_counts.at("roi") : 0)
        << ", \"scan_count\":" << (role_counts.count("scan") ? role_counts.at("scan") : 0)
        << ", \"result_count\":" << (role_counts.count("result") ? role_counts.at("result") : 0) << "}\n";
    out << "  ]\n";
    out << "}\n";
    return out.str();
}

std::string BuildFeatureVector(
    const CxScriptExecutionCapture& capture,
    const std::string& tool)
{
    const double fit_available =
        (capture.has_fit_line || capture.has_fit_circle || capture.has_fit_ellipse || capture.has_result_rect) ? 1.0 : 0.0;
    std::ostringstream out;
    out << "{\n";
    out << "  \"schema\": \"cxvision.measurement_feature_vector.v1\",\n";
    out << "  \"tool\": \"" << JsonEscapeLocal(tool) << "\",\n";
    out << "  \"source\": \"measurement_observations.json\",\n";
    out << "  \"features\": {\n";
    out << "    \"valid_points_count\": " << capture.valid_points_count << ",\n";
    out << "    \"fit_available\": " << fit_available << ",\n";
    out << "    \"avgdist_px\": " << capture.avgdist << ",\n";
    out << "    \"circle_radius_px\": " << capture.circle_radius << ",\n";
    out << "    \"rendered_result_count\": " << capture.rendered_result_count << ",\n";
    out << "    \"rendered_measure_points_count\": " << capture.rendered_measure_points_count << ",\n";
    out << "    \"point_render_ratio\": " << SafeRatio(capture.rendered_measure_points_count, std::max(1, capture.valid_points_count)) << "\n";
    out << "  },\n";
    out << "  \"validity_mask\": {\n";
    out << "    \"runtime_completed\": " << BoolText(capture.runtime_completed) << ",\n";
    out << "    \"not_budget_exceeded\": " << BoolText(!capture.budget_exceeded) << ",\n";
    out << "    \"has_runtime_points\": " << BoolText(capture.valid_points_count > 0) << "\n";
    out << "  }\n";
    out << "}\n";
    return out.str();
}

std::string BuildSemanticPatternResult()
{
    return
        "{\n"
        "  \"schema\": \"cxvision.semantic_pattern_result.v1\",\n"
        "  \"status\": \"PENDING_MODEL_BINDING\",\n"
        "  \"reason\": \"Measurement observations are exported; pattern model classification is not bound in S0/S1.\"\n"
        "}\n";
}

std::string BuildAccuracyEvaluation(const CxScriptExecutionCapture& capture)
{
    std::ostringstream out;
    out << "{\n";
    out << "  \"schema\": \"cxvision.accuracy_evaluation.v1\",\n";
    out << "  \"status\": \"PENDING_GROUND_TRUTH\",\n";
    out << "  \"residual_px\": " << capture.avgdist << ",\n";
    out << "  \"repeatability_status\": \"NOT_EVALUATED\",\n";
    out << "  \"stability_status\": \"NOT_EVALUATED\",\n";
    out << "  \"reason\": \"Ground truth and perturbation matrix are not bound for this sidecar stage.\"\n";
    out << "}\n";
    return out.str();
}

std::string BuildUncertaintyBudget()
{
    return
        "{\n"
        "  \"schema\": \"cxvision.uncertainty_budget.v1\",\n"
        "  \"status\": \"UNCERTAINTY_INCOMPLETE\",\n"
        "  \"components\": [],\n"
        "  \"reason\": \"Calibration, repeatability and model uncertainty inputs are not yet reviewed.\"\n"
        "}\n";
}

std::string BuildAlgorithmProvenance(const std::string& tool)
{
    std::ostringstream out;
    out << "{\n";
    out << "  \"schema\": \"cxvision.algorithm_provenance.v1\",\n";
    out << "  \"tool\": \"" << JsonEscapeLocal(tool) << "\",\n";
    out << "  \"implementation_identity\": \"cxvision_independent_implementation\",\n";
    out << "  \"gpl_source_policy\": \"No GPL source copying, translation, linking or distribution is used by this sidecar.\",\n";
    out << "  \"source_basis\": [\"cxvision runtime capture\", \"cxvision shape snapshot\", \"independent measurement semantics design\"]\n";
    out << "}\n";
    return out.str();
}

std::string BuildContractResult(const CxScriptExecutionCapture& capture)
{
    std::ostringstream out;
    out << "{\n";
    out << "  \"schema\": \"cxvision.measurement_semantic_contract_result.v1\",\n";
    out << "  \"status\": \"" << EvidenceStatus(capture) << "\",\n";
    out << "  \"sidecar_available\": true,\n";
    out << "  \"runtime_completed\": " << BoolText(capture.runtime_completed) << ",\n";
    out << "  \"budget_exceeded\": " << BoolText(capture.budget_exceeded) << ",\n";
    out << "  \"human_review_required\": true,\n";
    out << "  \"reason\": \"S0/S1 sidecars are generated for review; they do not promote or replace algorithm contracts.\"\n";
    out << "}\n";
    return out.str();
}
}

bool WriteMeasurementSemanticSidecars(
    const CxScriptExecutionCapture& capture,
    const CxScriptHeadlessOptions& options,
    const std::filesystem::path& output_dir,
    std::string& out_reason)
{
    const std::string tool = ResolveToolName(capture, options);
    const std::vector<std::string> object_refs = CollectObjectRefs(capture);
    const std::map<std::string, int> role_counts = CountShapeRoles(capture);

    const std::vector<std::pair<std::string, std::string>> files = {
        { "measurement_semantic_input.json", BuildMeasurementSemanticInput(capture, options, tool, object_refs) },
        { "calibration_snapshot.json", BuildCalibrationSnapshot(options) },
        { "coordinate_transform_trace.json", BuildCoordinateTransformTrace() },
        { "measurement_behavior_trace.json", BuildBehaviorTrace(capture, tool) },
        { "measurement_observations.json", BuildObservations(capture, tool) },
        { "measurement_relations.json", BuildRelations(capture, tool, role_counts) },
        { "measurement_feature_vector.json", BuildFeatureVector(capture, tool) },
        { "semantic_pattern_result.json", BuildSemanticPatternResult() },
        { "accuracy_evaluation.json", BuildAccuracyEvaluation(capture) },
        { "uncertainty_budget.json", BuildUncertaintyBudget() },
        { "algorithm_provenance.json", BuildAlgorithmProvenance(tool) },
        { "measurement_semantic_contract_result.json", BuildContractResult(capture) }
    };

    for (const auto& item : files)
    {
        if (!WriteTextAtomic(output_dir / item.first, item.second, out_reason))
            return false;
    }

    out_reason.clear();
    return true;
}
