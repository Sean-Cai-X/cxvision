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

    bool has_initial_ellipse = false;
    double initial_cx = 0.0;
    double initial_cy = 0.0;
    double initial_rx = 0.0;
    double initial_ry = 0.0;

    bool editable = true;
    bool visible = true;

    int expected_hit = 1;
    int expected_begin_drag = 1;
    int expected_commit = 1;

    bool has_expected_radius_x = false;
    bool has_expected_radius_y = false;
    double expected_radius_x = 0.0;
    double expected_radius_y = 0.0;
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