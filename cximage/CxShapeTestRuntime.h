#ifndef CXSHAPETESTRUNTIME_H
#define CXSHAPETESTRUNTIME_H

#include <string>
#include <vector>

struct CxShapeTestCase
{
    std::string case_id;
    std::string tool_id;
    std::string operation;
    std::string handle;
    std::string expected;

    double from_x = 0.0;
    double from_y = 0.0;
    double to_x = 0.0;
    double to_y = 0.0;

    double zoom = 1.0;
    double pan_x = 0.0;
    double pan_y = 0.0;
    int vertex_index = -1;

    bool has_initial_points = false;
    std::vector<double> initial_points;

    bool has_initial_rect = false;
    double initial_rx0 = 0.0;
    double initial_ry0 = 0.0;
    double initial_rx1 = 0.0;
    double initial_ry1 = 0.0;

    bool has_initial_circle = false;
    double initial_ccx = 0.0;
    double initial_ccy = 0.0;
    double initial_cradius = 0.0;

    bool has_initial_ellipse = false;
    double initial_ex = 0.0;
    double initial_ey = 0.0;
    double initial_erx = 0.0;
    double initial_ery = 0.0;

    bool editable = true;
    bool visible = true;

    int expected_hit = 1;
    int expected_begin_drag = 1;
    int expected_commit = 1;

    bool has_expected_radius_x = false;
    bool has_expected_radius_y = false;
    double expected_radius_x = 0.0;
    double expected_radius_y = 0.0;

    std::string pointer_sequence;
    int expected_shape_count_delta = 0;
    std::string expected_created_kind;
    std::string expected_phase;
    std::string expected_status;
};

class CxShapeTestRuntime
{
public:
    static void Reset();
    static void AddCase(const std::string& case_id);
    static CxShapeTestCase* Current();
    static const std::vector<CxShapeTestCase>& Cases();
    static const CxShapeTestCase* FindCase(const std::string& case_id);

private:
    static std::vector<CxShapeTestCase> s_cases;
    static CxShapeTestCase* s_current;
};

#endif