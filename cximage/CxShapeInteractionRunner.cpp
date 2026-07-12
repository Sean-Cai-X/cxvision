#include "CxShapeInteractionRunner.h"
#include "CxAnnotationToolRuntime.h"
#include "CxAnnotationToolRegister.h"
#include "CxShapeTestRegister.h"
#include "muParser.h"
#include "CxUnifiedLog.h"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <unordered_set>

namespace fs = std::filesystem;

bool CxShapeInteractionRunner::RunSuite(const CxShapeInteractionOptions& options, CxShapeInteractionBatchResultEx& result)
{
    std::string reason;

    if (options.run_id.empty())
    {
        result.pass = false;
        CXLOG_ERROR(
            "CxShapeInteractionRunner",
            "shape_suite_validate",
            "failed",
            "run_id is empty");

        if (!options.out_dir.empty())
        {
            fs::create_directories(options.out_dir);
            std::ofstream fail_file(fs::path(options.out_dir) / "shape_interaction_report.json");
            if (fail_file)
            {
                fail_file << "{\n";
                fail_file << "  \"pass\": false,\n";
                fail_file << "  \"total_cases\": 0,\n";
                fail_file << "  \"executed_cases\": 0,\n";
                fail_file << "  \"pass_count\": 0,\n";
                fail_file << "  \"fail_count\": 0,\n";
                fail_file << "  \"error\": \"run_id_empty\"\n";
                fail_file << "}\n";
            }
        }

        return false;
    }

    if (!LoadToolManifest(options.tool_manifest_path, reason))
    {
        result.pass = false;
        return false;
    }

    if (!LoadTestSuite(options.test_suite_path, reason))
    {
        result.pass = false;
        return false;
    }

    CXLOG_INFO("CxShapeInteractionRunner", "shape_suite_begin", "running", 
               "suite_path=" + options.test_suite_path + ", run_id=" + options.run_id);

    const auto& cases = CxShapeTestRuntime::Cases();

    if (cases.empty())
    {
        result.pass = false;
        CXLOG_ERROR("CxShapeInteractionRunner", "shape_suite_validate", "failed", "test suite contains no cases");

        if (!options.out_dir.empty())
        {
            fs::create_directories(options.out_dir);
            std::ofstream fail_file(fs::path(options.out_dir) / "shape_interaction_report.json");
            if (fail_file)
            {
                fail_file << "{\n";
                fail_file << "  \"pass\": false,\n";
                fail_file << "  \"total_cases\": 0,\n";
                fail_file << "  \"executed_cases\": 0,\n";
                fail_file << "  \"pass_count\": 0,\n";
                fail_file << "  \"fail_count\": 0,\n";
                fail_file << "  \"error\": \"empty_test_suite\"\n";
                fail_file << "}\n";
            }
        }

        return false;
    }

    std::unordered_set<std::string> case_ids;
    for (const auto& tc : cases)
    {
        if (tc.case_id.empty())
        {
            result.pass = false;
            CXLOG_ERROR("CxShapeInteractionRunner", "shape_suite_validate", "failed", "test suite contains case with empty case_id");
            return false;
        }

        if (!case_ids.insert(tc.case_id).second)
        {
            result.pass = false;
            CXLOG_ERROR("CxShapeInteractionRunner", "shape_suite_validate", "failed", "duplicate case_id: " + tc.case_id);
            return false;
        }
    }

    result.extended_cases.resize(cases.size());

    int pass_count = 0;
    for (size_t i = 0; i < cases.size(); ++i)
    {
        CxShapeInteractionTrace trace;

        CxUnifiedLogContext caseContext;
        caseContext.case_id = cases[i].case_id;
        caseContext.tool = cases[i].tool_id;
        caseContext.run_id = options.run_id;

        CxScopedLogContext caseScope(caseContext);

        CXLOG_INFO("CxShapeInteractionRunner", "shape_case_begin", "running",
            "operation=" + cases[i].operation + ", handle=" + cases[i].handle);

        RunTestCase(cases[i], options, result.extended_cases[i], trace);

        const auto& caseResult = result.extended_cases[i];
        if (caseResult.pass)
            pass_count++;

        std::string caseDir;
        if (!options.out_dir.empty())
        {
            caseDir = (fs::path(options.out_dir) / "cases" / caseResult.case_id).string();
            GenerateCaseOutput(caseResult, trace, options, options.out_dir);
        }

        std::string status = caseResult.pass ? "passed" : "failed";
        std::string detail =
            "expected_handle=" + caseResult.expected_handle +
            ", actual_handle=" + caseResult.actual_handle +
            ", acceptance_scope=" + caseResult.acceptance_scope +
            ", runtime_writeback=" + std::string(caseResult.runtime_writeback ? "true" : "false");

        if (!caseDir.empty())
            detail += ", case_dir=" + caseDir;

        if (caseResult.pass)
        {
            CXLOG_INFO("CxShapeInteractionRunner", "shape_case_end", status, detail);
        }
        else
        {
            CXLOG_ERROR("CxShapeInteractionRunner", "shape_case_end", status,
                detail + ", reason=" + caseResult.reason);
        }
    }

    result.pass = (pass_count == static_cast<int>(cases.size()));
    result.cases.reserve(result.extended_cases.size());
    for (const auto& ec : result.extended_cases)
        result.cases.push_back(ec);

    if (!options.out_dir.empty())
        GenerateBatchOutput(result, options, options.out_dir);

    CXLOG_INFO("CxShapeInteractionRunner", "shape_suite_end", 
               result.pass ? "passed" : "failed", 
               "total=" + std::to_string(cases.size()) + ", pass=" + std::to_string(pass_count));

    return result.pass;
}

bool CxShapeInteractionRunner::LoadToolManifest(const std::string& path, std::string& reason)
{
    std::ifstream stream{fs::path(path)};
    if (!stream)
    {
        reason = "tool manifest not found: " + path;
        return false;
    }

    std::stringstream buffer;
    buffer << stream.rdbuf();
    std::string script = buffer.str();

    CxAnnotationToolRuntime::Reset();

    mu::Parser parser;
    parser.UsingClass(true);
    RegisterCxAnnotationToolBindings(parser);

    try
    {
        parser.SetExpr(script);
        parser.Eval();
    }
    catch (const mu::Parser::exception_type& e)
    {
        reason = "parse error: " + std::string(e.GetMsg());
        return false;
    }

    return true;
}

bool CxShapeInteractionRunner::LoadTestSuite(const std::string& path, std::string& reason)
{
    std::ifstream stream{fs::path(path)};
    if (!stream)
    {
        reason = "test suite not found: " + path;
        return false;
    }

    std::stringstream buffer;
    buffer << stream.rdbuf();
    std::string script = buffer.str();

    CxShapeTestRuntime::Reset();

    mu::Parser parser;
    parser.UsingClass(true);
    RegisterCxShapeTestBindings(parser);

    try
    {
        parser.SetExpr(script);
        parser.Eval();
    }
    catch (const mu::Parser::exception_type& e)
    {
        reason = "parse error: " + std::string(e.GetMsg());
        return false;
    }

    return true;
}

bool CxShapeInteractionRunner::VerifyHitExpectation(
    const CxShapeTestCase& tc,
    const CxShapeHitResult& hit,
    std::string& reason)
{
    const std::string actual_handle = HandleName(hit.shape_hit.role);
    const int actual_vertex = hit.shape_hit.vertex_index;

    if (actual_handle != tc.handle)
    {
        reason = "expected handle=" + tc.handle + ", actual handle=" + actual_handle;
        return false;
    }

    if (tc.vertex_index != -1 && actual_vertex != tc.vertex_index)
    {
        reason = "expected vertex=" + std::to_string(tc.vertex_index) +
                 ", actual vertex=" + std::to_string(actual_vertex);
        return false;
    }

    reason = "hit expectation verified";
    return true;
}

bool CxShapeInteractionRunner::RunTestCase(const CxShapeTestCase& tc, const CxShapeInteractionOptions& options, CxShapeInteractionCaseResultEx& case_result, CxShapeInteractionTrace& trace)
{
    case_result.case_id = tc.case_id;
    case_result.tool_id = tc.tool_id;
    case_result.operation = tc.operation;
    case_result.expected_handle = tc.handle;
    case_result.geometry_assertion = tc.expected;
    case_result.expected_vertex = tc.vertex_index;

    ImageAnnotationLayer layer;
    layer.ClearShapeElements();

    auto tool = CxAnnotationToolRuntime::FindById(tc.tool_id);
    if (!tool)
    {
        case_result.pass = false;
        case_result.conclusion = "tool_not_registered";
        case_result.status = "FAIL";
        case_result.reason = "tool not registered: " + tc.tool_id;
        return false;
    }

    case_result.shape_kind = tool->shape_type;
    case_result.owner_type = tool->owner_tool;
    case_result.owner_binding = tool->owner_binding;

    std::vector<CxShapePoint> initial_points;
    if (tc.operation == "drag_handle")
    {
        if (tool->shape_type == "LineShape" || tool->shape_type == "LineGaugeShape")
        {
            if (tc.handle == "End")
            {
                initial_points.push_back({tc.from_x - 50.0, tc.from_y - 30.0});
                initial_points.push_back({tc.from_x, tc.from_y});
            }
            else if (tc.handle == "Center")
            {
                initial_points.push_back({tc.from_x - 25.0, tc.from_y - 15.0});
                initial_points.push_back({tc.from_x + 25.0, tc.from_y + 15.0});
            }
            else if (tc.handle == "WidthPositive")
            {
                const double cx = tc.from_x;
                const double cy = tc.from_y - 20.0;
                initial_points.push_back({cx - 30.0, cy});
                initial_points.push_back({cx + 30.0, cy});
            }
            else if (tc.handle == "WidthNegative")
            {
                const double cx = tc.from_x;
                const double cy = tc.from_y + 20.0;
                initial_points.push_back({cx - 30.0, cy});
                initial_points.push_back({cx + 30.0, cy});
            }
            else
            {
                initial_points.push_back({tc.from_x, tc.from_y});
                initial_points.push_back({tc.from_x + 50.0, tc.from_y + 30.0});
            }
        }
        else if (tool->shape_type == "RectShape")
        {
            if (tc.handle == "Center")
            {
                initial_points.push_back({tc.from_x - 30.0, tc.from_y - 20.0});
                initial_points.push_back({tc.from_x + 30.0, tc.from_y + 20.0});
            }
            else
            {
                initial_points.push_back({tc.from_x, tc.from_y});
                initial_points.push_back({tc.from_x + 60.0, tc.from_y + 40.0});
            }
        }
        else if (tool->shape_type == "CircleShape")
        {
            if (tc.handle == "Radius")
            {
                const double initial_radius = 40.0;
                initial_points.push_back({tc.from_x - initial_radius, tc.from_y});
                initial_points.push_back({tc.from_x, tc.from_y});
            }
            else
            {
                initial_points.push_back({tc.from_x, tc.from_y});
                initial_points.push_back({tc.from_x + 40.0, tc.from_y});
            }
        }
        else if (tool->shape_type == "EllipseShape")
        {
            if (tc.has_initial_ellipse)
            {
                initial_points.push_back({tc.initial_cx - tc.initial_rx, tc.initial_cy - tc.initial_ry});
                initial_points.push_back({tc.initial_cx + tc.initial_rx, tc.initial_cy + tc.initial_ry});
            }
            else if (tc.handle == "RadiusX")
            {
                initial_points.push_back({tc.from_x - 40.0, tc.from_y - 25.0});
                initial_points.push_back({tc.from_x + 40.0, tc.from_y + 25.0});
            }
            else if (tc.handle == "RadiusY")
            {
                initial_points.push_back({tc.from_x - 40.0, tc.from_y - 25.0});
                initial_points.push_back({tc.from_x + 40.0, tc.from_y + 25.0});
            }
            else
            {
                initial_points.push_back({tc.from_x - 40.0, tc.from_y - 25.0});
                initial_points.push_back({tc.from_x + 40.0, tc.from_y + 25.0});
            }
        }
        else if (tool->shape_type == "PolylineShape")
        {
            if (tc.handle == "Center")
            {
                initial_points.push_back({tc.from_x - 30.0, tc.from_y - 15.0});
                initial_points.push_back({tc.from_x, tc.from_y + 10.0});
                initial_points.push_back({tc.from_x + 30.0, tc.from_y - 15.0});
            }
            else
            {
                initial_points.push_back({tc.from_x, tc.from_y});
                initial_points.push_back({tc.from_x + 30.0, tc.from_y + 20.0});
                initial_points.push_back({tc.from_x + 60.0, tc.from_y});
            }
        }
        else if (tool->shape_type == "PointsShape")
        {
            initial_points.push_back({tc.from_x, tc.from_y});
        }
    }

    std::unique_ptr<ShapeBase> shape = CreateInitialShapeForTool(*tool, initial_points);
    if (!shape)
    {
        case_result.pass = false;
        case_result.conclusion = "shape_creation_failed";
        case_result.status = "FAIL";
        case_result.reason = "failed to create shape for tool: " + tc.tool_id;
        return false;
    }

    CxShapeGeometrySnapshot before_snap;
    shape->snapshot(before_snap);

    layer.CreateFromTool(*tool, std::move(shape));

    if (tc.operation == "drag_handle")
    {
        trace = {};
        const bool success = layer.SimulatePointerDrag(
            tc.from_x, tc.from_y,
            tc.to_x, tc.to_y,
            options.drag_steps, options.tolerance, trace);

        case_result.hit_test_pass = trace.hit;
        case_result.drag_pass = trace.begin_drag_ok && trace.update_drag_ok;
        case_result.commit_pass = trace.commit_ok;

        if (!trace.hits.empty())
        {
            case_result.actual_handle = HandleName(trace.hits[0].shape_hit.role);
            case_result.actual_vertex = trace.hits[0].shape_hit.vertex_index;
        }

        bool outcomePass = trace.hit == (tc.expected_hit != 0);

        if (tc.expected_begin_drag >= 0)
        {
            outcomePass = outcomePass &&
                trace.begin_drag_ok == (tc.expected_begin_drag != 0);
        }

        if (tc.expected_commit >= 0)
        {
            outcomePass = outcomePass &&
                trace.commit_ok == (tc.expected_commit != 0);
        }

        if (!trace.hits.empty() && trace.hit && tc.expected_hit != 0)
        {
            std::string hit_reason;
            if (!VerifyHitExpectation(tc, trace.hits[0], hit_reason))
            {
                case_result.pass = false;
                case_result.hit_test_pass = false;
                case_result.status = "FAIL";
                case_result.conclusion = "unexpected_handle";
                case_result.reason = hit_reason;
                return false;
            }
        }

        if (outcomePass)
        {
            case_result.pass = true;

            if (tc.expected_commit != 0 && !trace.snapshots.empty())
            {
                const auto& after_snap = trace.snapshots.back();
                std::string verify_reason;
                bool assertion_pass = VerifyGeometryAssertion(tc.expected, before_snap, after_snap, verify_reason);

                if (assertion_pass)
                {
                    case_result.conclusion = "interaction_passed";
                    case_result.reason = "hit=" + case_result.actual_handle + ", geometry verified";

                    const bool runtimeBoundTool = !tool->owner_tool.empty() && !tool->owner_binding.empty();
                    if (runtimeBoundTool)
                    {
                        case_result.status = "GEOMETRY_PASS_PENDING_WRITEBACK";
                        case_result.acceptance_scope = "L0_L1_GEOMETRY";
                        case_result.runtime_writeback = false;
                    }
                    else
                    {
                        case_result.status = "PASS";
                        case_result.acceptance_scope = "L0_L1_MANUAL_SHAPE";
                        case_result.runtime_writeback = false;
                    }
                }
                else
                {
                    case_result.pass = false;
                    case_result.conclusion = "geometry_mismatch";
                    case_result.status = "FAIL";
                    case_result.reason = verify_reason;
                }
            }
            else
            {
                case_result.conclusion = "interaction_passed";
                case_result.reason = "hit=" + std::to_string(trace.hit) +
                    ", begin_drag=" + std::to_string(trace.begin_drag_ok) +
                    ", commit=" + std::to_string(trace.commit_ok);

                case_result.status = "PASS";
                case_result.acceptance_scope = "L0_L1_MANUAL_SHAPE";
                case_result.runtime_writeback = false;
            }
        }
        else
        {
            case_result.pass = false;
            case_result.conclusion = "interaction_failed";
            case_result.status = "FAIL";
            if (!trace.hit && tc.expected_hit != 0)
                case_result.reason = "expected hit but hit test failed";
            else if (trace.hit && tc.expected_hit == 0)
                case_result.reason = "expected no hit but hit test succeeded";
            else if (!trace.begin_drag_ok && tc.expected_begin_drag != 0)
                case_result.reason = "expected begin_drag but it failed";
            else if (trace.begin_drag_ok && tc.expected_begin_drag == 0)
                case_result.reason = "expected no begin_drag but it succeeded";
            else if (!trace.commit_ok && tc.expected_commit != 0)
                case_result.reason = "expected commit but it failed";
            else if (trace.commit_ok && tc.expected_commit == 0)
                case_result.reason = "expected no commit but it succeeded";
            else
                case_result.reason = "interaction failed";
        }
    }
    else if (tc.operation == "create")
    {
        case_result.hit_test_pass = false;
        case_result.drag_pass = false;
        case_result.commit_pass = false;

        case_result.pass = true;
        case_result.conclusion = "shape_created";
        case_result.status = "PASS";
        case_result.reason = "shape created successfully";
        case_result.runtime_writeback = false;
        case_result.acceptance_scope = "FULL_INTERACTION";
    }
    else
    {
        case_result.pass = false;
        case_result.conclusion = "unsupported_operation";
        case_result.status = "FAIL";
        case_result.reason = "unsupported operation: " + tc.operation;
    }

    return case_result.pass;
}

bool CxShapeInteractionRunner::VerifyGeometryAssertion(
    const std::string& assertion,
    const CxShapeGeometrySnapshot& before,
    const CxShapeGeometrySnapshot& after,
    std::string& reason)
{
    const double epsilon = 1.0;

    if (assertion == "point_visible")
    {
        if (after.points.size() >= 1)
        {
            reason = "point exists";
            return true;
        }
        reason = "no points in shape";
        return false;
    }

    if (assertion == "target_moved_only")
    {
        if (before.points.size() != after.points.size())
        {
            reason = "point count changed";
            return false;
        }
        bool moved = false;
        for (size_t i = 0; i < before.points.size(); ++i)
        {
            const double dx = after.points[i].x - before.points[i].x;
            const double dy = after.points[i].y - before.points[i].y;
            if (std::abs(dx) > epsilon || std::abs(dy) > epsilon)
            {
                if (moved)
                {
                    reason = "multiple points moved";
                    return false;
                }
                moved = true;
            }
        }
        if (moved)
        {
            reason = "single point moved";
            return true;
        }
        reason = "no points moved";
        return false;
    }

    if (assertion == "start_moved_end_unchanged")
    {
        if (before.points.size() >= 2 && after.points.size() >= 2)
        {
            const bool start_moved = std::abs(after.points[0].x - before.points[0].x) > epsilon ||
                                     std::abs(after.points[0].y - before.points[0].y) > epsilon;
            const bool end_unchanged = std::abs(after.points[1].x - before.points[1].x) <= epsilon &&
                                       std::abs(after.points[1].y - before.points[1].y) <= epsilon;
            if (start_moved && end_unchanged)
            {
                reason = "start moved, end unchanged";
                return true;
            }
            reason = "start not moved or end changed";
            return false;
        }
        reason = "not enough points";
        return false;
    }

    if (assertion == "translate_only")
    {
        if (before.points.size() != after.points.size())
        {
            reason = "point count changed";
            return false;
        }
        double dx = 0, dy = 0;
        bool first = true;
        for (size_t i = 0; i < before.points.size(); ++i)
        {
            const double cdx = after.points[i].x - before.points[i].x;
            const double cdy = after.points[i].y - before.points[i].y;
            if (first)
            {
                dx = cdx;
                dy = cdy;
                first = false;
            }
            else
            {
                if (std::abs(cdx - dx) > epsilon || std::abs(cdy - dy) > epsilon)
                {
                    reason = "translation not uniform";
                    return false;
                }
            }
        }
        if (std::abs(dx) > epsilon || std::abs(dy) > epsilon)
        {
            reason = "uniform translation";
            return true;
        }
        reason = "no translation";
        return false;
    }

    if (assertion == "outer_radius_only")
    {
        if (std::abs(after.center.x - before.center.x) <= epsilon &&
            std::abs(after.center.y - before.center.y) <= epsilon &&
            std::abs(after.inner_radius - before.inner_radius) <= epsilon &&
            std::abs(after.radius - before.radius) > epsilon)
        {
            reason = "outer radius changed only";
            return true;
        }
        reason = "center, inner radius changed, or outer radius unchanged";
        return false;
    }

    if (assertion == "end_moved_start_unchanged")
    {
        if (before.points.size() >= 2 && after.points.size() >= 2)
        {
            const bool end_moved = std::abs(after.points[1].x - before.points[1].x) > epsilon ||
                                   std::abs(after.points[1].y - before.points[1].y) > epsilon;
            const bool start_unchanged = std::abs(after.points[0].x - before.points[0].x) <= epsilon &&
                                         std::abs(after.points[0].y - before.points[0].y) <= epsilon;
            if (end_moved && start_unchanged)
            {
                reason = "end moved, start unchanged";
                return true;
            }
            reason = "end not moved or start changed";
            return false;
        }
        reason = "not enough points";
        return false;
    }

    if (assertion == "corner0_moved_others_unchanged")
    {
        if (before.points.size() >= 4 && after.points.size() >= 4)
        {
            const bool corner0_moved = std::abs(after.points[0].x - before.points[0].x) > epsilon ||
                                       std::abs(after.points[0].y - before.points[0].y) > epsilon;
            bool others_unchanged = true;
            for (size_t i = 1; i < 4 && i < before.points.size(); ++i)
            {
                if (std::abs(after.points[i].x - before.points[i].x) > epsilon ||
                    std::abs(after.points[i].y - before.points[i].y) > epsilon)
                {
                    others_unchanged = false;
                    break;
                }
            }
            if (corner0_moved && others_unchanged)
            {
                reason = "corner0 moved, others unchanged";
                return true;
            }
            reason = "corner0 not moved or other corners changed";
            return false;
        }
        reason = "not enough points";
        return false;
    }

    if (assertion == "vertex0_moved_others_unchanged")
    {
        if (before.points.size() >= 1 && after.points.size() >= 1)
        {
            const bool vertex0_moved = std::abs(after.points[0].x - before.points[0].x) > epsilon ||
                                       std::abs(after.points[0].y - before.points[0].y) > epsilon;
            bool others_unchanged = true;
            for (size_t i = 1; i < before.points.size(); ++i)
            {
                if (std::abs(after.points[i].x - before.points[i].x) > epsilon ||
                    std::abs(after.points[i].y - before.points[i].y) > epsilon)
                {
                    others_unchanged = false;
                    break;
                }
            }
            if (vertex0_moved && others_unchanged)
            {
                reason = "vertex0 moved, others unchanged";
                return true;
            }
            reason = "vertex0 not moved or other vertices changed";
            return false;
        }
        reason = "not enough points";
        return false;
    }

    if (assertion == "half_width_increased")
    {
        if (after.half_width > before.half_width + epsilon)
        {
            reason = "half_width increased";
            return true;
        }
        reason = "half_width not increased";
        return false;
    }

    if (assertion == "half_width_decreased")
    {
        if (after.half_width < before.half_width - epsilon)
        {
            reason = "half_width decreased";
            return true;
        }
        reason = "half_width not decreased";
        return false;
    }

    if (assertion == "radius_x_only")
    {
        if (std::abs(after.center.x - before.center.x) <= epsilon &&
            std::abs(after.center.y - before.center.y) <= epsilon &&
            std::abs(after.radius_y - before.radius_y) <= epsilon &&
            std::abs(after.radius_x - before.radius_x) > epsilon)
        {
            reason = "radius_x changed only";
            return true;
        }
        reason = "center, radius_y changed, or radius_x unchanged";
        return false;
    }

    if (assertion == "radius_y_only")
    {
        if (std::abs(after.center.x - before.center.x) <= epsilon &&
            std::abs(after.center.y - before.center.y) <= epsilon &&
            std::abs(after.radius_x - before.radius_x) <= epsilon &&
            std::abs(after.radius_y - before.radius_y) > epsilon)
        {
            reason = "radius_y changed only";
            return true;
        }
        reason = "center, radius_x changed, or radius_y unchanged";
        return false;
    }

    if (assertion == "ellipse_translate_only")
    {
        if (std::abs(after.radius_x - before.radius_x) <= epsilon &&
            std::abs(after.radius_y - before.radius_y) <= epsilon &&
            (std::abs(after.center.x - before.center.x) > epsilon ||
             std::abs(after.center.y - before.center.y) > epsilon))
        {
            reason = "ellipse translated only";
            return true;
        }
        reason = "radii changed or no translation";
        return false;
    }

    reason = "unknown assertion: " + assertion;
    return false;
}

static std::string EscapeJson(const std::string& s)
{
    std::ostringstream oss;
    for (char c : s)
    {
        switch (c)
        {
        case '\\': oss << "\\\\"; break;
        case '"': oss << "\\\""; break;
        case '\n': oss << "\\n"; break;
        case '\r': oss << "\\r"; break;
        case '\t': oss << "\\t"; break;
        default:
            if (c >= 0 && c < 0x20)
                oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)c;
            else
                oss << c;
            break;
        }
    }
    return oss.str();
}

void CxShapeInteractionRunner::GenerateCaseOutput(
    const CxShapeInteractionCaseResultEx& case_result,
    const CxShapeInteractionTrace& trace,
    const CxShapeInteractionOptions& options,
    const std::string& out_dir)
{
    const fs::path case_dir = fs::path(out_dir) / "cases" / case_result.case_id;
    fs::create_directories(case_dir);

    std::ofstream conclusion_file(case_dir / "conclusion.json");
    if (conclusion_file)
    {
        conclusion_file << "{\n";
        conclusion_file << "  \"run_id\": \"" << EscapeJson(options.run_id) << "\",\n";
        conclusion_file << "  \"case_id\": \"" << EscapeJson(case_result.case_id) << "\",\n";
        conclusion_file << "  \"tool_id\": \"" << EscapeJson(case_result.tool_id) << "\",\n";
        conclusion_file << "  \"operation\": \"" << EscapeJson(case_result.operation) << "\",\n";
        conclusion_file << "  \"shape_kind\": \"" << EscapeJson(case_result.shape_kind) << "\",\n";
        conclusion_file << "  \"expected_handle\": \"" << EscapeJson(case_result.expected_handle) << "\",\n";
        conclusion_file << "  \"actual_handle\": \"" << EscapeJson(case_result.actual_handle) << "\",\n";
        conclusion_file << "  \"expected_vertex\": " << case_result.expected_vertex << ",\n";
        conclusion_file << "  \"actual_vertex\": " << case_result.actual_vertex << ",\n";
        conclusion_file << "  \"geometry_assertion\": \"" << EscapeJson(case_result.geometry_assertion) << "\",\n";
        conclusion_file << "  \"hit_test_pass\": " << (case_result.hit_test_pass ? "true" : "false") << ",\n";
        conclusion_file << "  \"drag_pass\": " << (case_result.drag_pass ? "true" : "false") << ",\n";
        conclusion_file << "  \"commit_pass\": " << (case_result.commit_pass ? "true" : "false") << ",\n";
        conclusion_file << "  \"render_pass\": " << (case_result.render_pass ? "true" : "false") << ",\n";
        conclusion_file << "  \"runtime_writeback\": " << (case_result.runtime_writeback ? "true" : "false") << ",\n";
        conclusion_file << "  \"acceptance_scope\": \"" << EscapeJson(case_result.acceptance_scope) << "\",\n";
        conclusion_file << "  \"status\": \"" << EscapeJson(case_result.status) << "\",\n";
        conclusion_file << "  \"reason\": \"" << EscapeJson(case_result.reason) << "\",\n";
        conclusion_file << "  \"pass\": " << (case_result.pass ? "true" : "false") << ",\n";
        conclusion_file << "  \"conclusion\": \"" << EscapeJson(case_result.conclusion) << "\"\n";
        conclusion_file << "}\n";
    }

    std::ofstream trace_file(case_dir / "interaction_trace.json");
    if (trace_file)
    {
        trace_file << "{\n";
        trace_file << "  \"case_id\": \"" << EscapeJson(case_result.case_id) << "\",\n";
        trace_file << "  \"events\": [\n";
        for (size_t i = 0; i < trace.pointer_events.size(); ++i)
        {
            const auto& evt = trace.pointer_events[i];
            std::string type_str;
            switch (evt.type)
            {
            case CxPointerEvent::Type::Move: type_str = "Move"; break;
            case CxPointerEvent::Type::LeftDown: type_str = "LeftDown"; break;
            case CxPointerEvent::Type::LeftDrag: type_str = "LeftDrag"; break;
            case CxPointerEvent::Type::LeftUp: type_str = "LeftUp"; break;
            }
            trace_file << "    {\n";
            trace_file << "      \"step\": " << evt.frame_no << ",\n";
            trace_file << "      \"type\": \"" << type_str << "\",\n";
            trace_file << "      \"image_x\": " << evt.image_x << ",\n";
            trace_file << "      \"image_y\": " << evt.image_y << "\n";
            trace_file << "    }";
            if (i < trace.pointer_events.size() - 1)
                trace_file << ",";
            trace_file << "\n";
        }
        trace_file << "  ],\n";
        trace_file << "  \"hits\": [\n";
        for (size_t i = 0; i < trace.hits.size(); ++i)
        {
            const auto& hit = trace.hits[i];
            trace_file << "    {\n";
            trace_file << "      \"hit\": " << (hit.hit ? "true" : "false") << ",\n";
            trace_file << "      \"element_index\": " << hit.element_index << ",\n";
            trace_file << "      \"role\": \"" << HandleName(hit.shape_hit.role) << "\",\n";
            trace_file << "      \"vertex_index\": " << hit.shape_hit.vertex_index << ",\n";
            trace_file << "      \"distance\": " << hit.shape_hit.distance << "\n";
            trace_file << "    }";
            if (i < trace.hits.size() - 1)
                trace_file << ",";
            trace_file << "\n";
        }
        trace_file << "  ],\n";
        trace_file << "  \"commit_reason\": \"" << EscapeJson(trace.commit_reason) << "\",\n";
        trace_file << "  \"commit_result\": {\n";
        trace_file << "    \"committed\": " << (trace.commit_result.committed ? "true" : "false") << ",\n";
        trace_file << "    \"owner_type\": \"" << EscapeJson(trace.commit_result.owner_type) << "\",\n";
        trace_file << "    \"owner_ref\": \"" << EscapeJson(trace.commit_result.owner_ref) << "\",\n";
        trace_file << "    \"owner_binding\": \"" << EscapeJson(trace.commit_result.owner_binding) << "\",\n";
        trace_file << "    \"result_marked_stale\": " << (trace.commit_result.result_marked_stale ? "true" : "false") << ",\n";
        trace_file << "    \"stale_result_count\": " << trace.commit_result.stale_result_count << "\n";
        trace_file << "  }\n";
        trace_file << "}\n";
    }

    if (!trace.snapshots.empty())
    {
        std::ofstream before_file(case_dir / "geometry_before.json");
        if (before_file)
        {
            const auto& snap = trace.snapshots.front();
            before_file << "{\n";
            before_file << "  \"points\": [\n";
            for (size_t i = 0; i < snap.points.size(); ++i)
            {
                before_file << "    {\"x\": " << snap.points[i].x << ", \"y\": " << snap.points[i].y << "}";
                if (i < snap.points.size() - 1)
                    before_file << ",";
                before_file << "\n";
            }
            before_file << "  ],\n";
            before_file << "  \"center\": {\"x\": " << snap.center.x << ", \"y\": " << snap.center.y << "},\n";
            before_file << "  \"radius\": " << snap.radius << ",\n";
            before_file << "  \"inner_radius\": " << snap.inner_radius << ",\n";
            before_file << "  \"half_width\": " << snap.half_width << ",\n";
            before_file << "  \"closed\": " << (snap.closed ? "true" : "false") << "\n";
            before_file << "}\n";
        }

        std::ofstream after_file(case_dir / "geometry_after.json");
        if (after_file)
        {
            const auto& snap = trace.snapshots.back();
            after_file << "{\n";
            after_file << "  \"points\": [\n";
            for (size_t i = 0; i < snap.points.size(); ++i)
            {
                after_file << "    {\"x\": " << snap.points[i].x << ", \"y\": " << snap.points[i].y << "}";
                if (i < snap.points.size() - 1)
                    after_file << ",";
                after_file << "\n";
            }
            after_file << "  ],\n";
            after_file << "  \"center\": {\"x\": " << snap.center.x << ", \"y\": " << snap.center.y << "},\n";
            after_file << "  \"radius\": " << snap.radius << ",\n";
            after_file << "  \"inner_radius\": " << snap.inner_radius << ",\n";
            after_file << "  \"half_width\": " << snap.half_width << ",\n";
            after_file << "  \"closed\": " << (snap.closed ? "true" : "false") << "\n";
            after_file << "}\n";
        }
    }
}

void CxShapeInteractionRunner::GenerateBatchOutput(
    const CxShapeInteractionBatchResultEx& result,
    const CxShapeInteractionOptions& options,
    const std::string& out_dir)
{
    fs::create_directories(out_dir);

    int pass_count = 0;
    int fail_count = 0;
    int pending_writeback_count = 0;
    int missing_tool_count = 0;

    for (const auto& c : result.extended_cases)
    {
        if (c.pass)
            pass_count++;
        else
            fail_count++;
        if (c.status == "GEOMETRY_PASS_PENDING_WRITEBACK")
            pending_writeback_count++;
        if (c.conclusion == "tool_not_registered")
            missing_tool_count++;
    }

    std::ofstream json_file(fs::path(out_dir) / "shape_interaction_report.json");
    if (json_file)
    {
        json_file << "{\n";
        json_file << "  \"run_id\": \"" << EscapeJson(options.run_id) << "\",\n";
        json_file << "  \"tool_manifest_path\": \"" << EscapeJson(options.tool_manifest_path) << "\",\n";
        json_file << "  \"test_suite_path\": \"" << EscapeJson(options.test_suite_path) << "\",\n";
        json_file << "  \"output_dir\": \"" << EscapeJson(options.out_dir) << "\",\n";
        json_file << "  \"drag_steps\": " << options.drag_steps << ",\n";
        json_file << "  \"hit_tolerance\": " << options.tolerance << ",\n";
        json_file << "  \"pass\": " << (result.pass ? "true" : "false") << ",\n";
        json_file << "  \"total_cases\": " << result.extended_cases.size() << ",\n";
        json_file << "  \"executed_cases\": " << result.extended_cases.size() << ",\n";
        json_file << "  \"pass_count\": " << pass_count << ",\n";
        json_file << "  \"fail_count\": " << fail_count << ",\n";
        json_file << "  \"pending_writeback_count\": " << pending_writeback_count << ",\n";
        json_file << "  \"missing_tool_count\": " << missing_tool_count << ",\n";
        json_file << "  \"cases\": [\n";
        for (size_t i = 0; i < result.extended_cases.size(); ++i)
        {
            const auto& c = result.extended_cases[i];
            json_file << "    {\n";
            json_file << "      \"run_id\": \"" << EscapeJson(options.run_id) << "\",\n";
            json_file << "      \"case_id\": \"" << EscapeJson(c.case_id) << "\",\n";
            json_file << "      \"tool_id\": \"" << EscapeJson(c.tool_id) << "\",\n";
            json_file << "      \"operation\": \"" << EscapeJson(c.operation) << "\",\n";
            json_file << "      \"shape_kind\": \"" << EscapeJson(c.shape_kind) << "\",\n";
            json_file << "      \"expected_handle\": \"" << EscapeJson(c.expected_handle) << "\",\n";
            json_file << "      \"actual_handle\": \"" << EscapeJson(c.actual_handle) << "\",\n";
            json_file << "      \"expected_vertex\": " << c.expected_vertex << ",\n";
            json_file << "      \"actual_vertex\": " << c.actual_vertex << ",\n";
            json_file << "      \"geometry_assertion\": \"" << EscapeJson(c.geometry_assertion) << "\",\n";
            json_file << "      \"hit_test_pass\": " << (c.hit_test_pass ? "true" : "false") << ",\n";
            json_file << "      \"drag_pass\": " << (c.drag_pass ? "true" : "false") << ",\n";
            json_file << "      \"commit_pass\": " << (c.commit_pass ? "true" : "false") << ",\n";
            json_file << "      \"runtime_writeback\": " << (c.runtime_writeback ? "true" : "false") << ",\n";
            json_file << "      \"acceptance_scope\": \"" << EscapeJson(c.acceptance_scope) << "\",\n";
            json_file << "      \"status\": \"" << EscapeJson(c.status) << "\",\n";
            json_file << "      \"reason\": \"" << EscapeJson(c.reason) << "\",\n";
            json_file << "      \"pass\": " << (c.pass ? "true" : "false") << ",\n";
            json_file << "      \"conclusion\": \"" << EscapeJson(c.conclusion) << "\"\n";
            json_file << "    }";
            if (i < result.extended_cases.size() - 1)
                json_file << ",";
            json_file << "\n";
        }
        json_file << "  ]\n";
        json_file << "}\n";
    }

    std::ofstream md_file(fs::path(out_dir) / "shape_interaction_report.md");
    if (md_file)
    {
        md_file << "# Shape Interaction Test Report\n\n";
        md_file << "## Summary\n\n";
        md_file << "- Total Cases: " << result.extended_cases.size() << "\n";
        md_file << "- Pass: " << pass_count << "\n";
        md_file << "- Fail: " << fail_count << "\n";
        md_file << "- Pending Writeback: " << pending_writeback_count << "\n";
        md_file << "- Missing Tools: " << missing_tool_count << "\n";
        md_file << "\n## Case Details\n\n";
        for (const auto& c : result.extended_cases)
        {
            md_file << "### " << c.case_id << "\n\n";
            md_file << "- Tool: " << c.tool_id << "\n";
            md_file << "- Status: " << c.status << "\n";
            md_file << "- Expected Handle: " << c.expected_handle << "\n";
            md_file << "- Actual Handle: " << c.actual_handle << "\n";
            md_file << "- Geometry Assertion: " << c.geometry_assertion << "\n";
            md_file << "- Reason: " << c.reason << "\n\n";
        }
    }

    std::ofstream failures_file(fs::path(out_dir) / "shape_interaction_failures.md");
    if (failures_file)
    {
        failures_file << "# Shape Interaction Failures\n\n";
        bool has_failures = false;
        for (const auto& c : result.extended_cases)
        {
            if (!c.pass)
            {
                has_failures = true;
                failures_file << "## " << c.case_id << "\n\n";
                failures_file << "- Tool: " << c.tool_id << "\n";
                failures_file << "- Expected Handle: " << c.expected_handle << "\n";
                failures_file << "- Actual Handle: " << c.actual_handle << "\n";
                failures_file << "- Geometry Assertion: " << c.geometry_assertion << "\n";
                failures_file << "- Status: " << c.status << "\n";
                failures_file << "- Reason: " << c.reason << "\n\n";
            }
        }
        if (!has_failures)
            failures_file << "No failures.\n";
    }

    std::ofstream snapshot_file(fs::path(out_dir) / "shape_interaction_snapshot.txt");
    if (snapshot_file)
    {
        snapshot_file << "Shape Interaction Test Snapshot\n";
        snapshot_file << "================================\n\n";
        snapshot_file << "Total: " << result.extended_cases.size() << "\n";
        snapshot_file << "Pass: " << pass_count << "\n";
        snapshot_file << "Fail: " << fail_count << "\n";
        snapshot_file << "\n";
        for (const auto& c : result.extended_cases)
        {
            snapshot_file << c.case_id << ": " << c.status << " (" << c.reason << ")\n";
        }
    }
}
