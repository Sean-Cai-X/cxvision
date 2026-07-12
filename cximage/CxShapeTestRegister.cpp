#include "pch.h"
#include "CxShapeTestRuntime.h"
#include "muParser.h"

static double CxShapeTest_reset(double)
{
    CxShapeTestRuntime::Reset();
    return 0.0;
}

static double CxShapeTest_addcase(const char* case_id)
{
    CxShapeTestRuntime::AddCase(case_id ? case_id : "");
    return 0.0;
}

static double CxShapeTest_settool(const char* tool_id)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
        current->tool_id = tool_id ? tool_id : "";
    return 0.0;
}

static double CxShapeTest_setoperation(const char* operation)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
        current->operation = operation ? operation : "";
    return 0.0;
}

static double CxShapeTest_sethandle(const char* handle)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
        current->handle = handle ? handle : "";
    return 0.0;
}

static double CxShapeTest_setfrom(double x, double y)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
    {
        current->from_x = x;
        current->from_y = y;
    }
    return 0.0;
}

static double CxShapeTest_setto(double x, double y)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
    {
        current->to_x = x;
        current->to_y = y;
    }
    return 0.0;
}

static double CxShapeTest_setzoom(double zoom)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
        current->zoom = zoom;
    return 0.0;
}

static double CxShapeTest_setpan(double x, double y)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
    {
        current->pan_x = x;
        current->pan_y = y;
    }
    return 0.0;
}

static double CxShapeTest_setvertex(double index)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
        current->vertex_index = static_cast<int>(index);
    return 0.0;
}

static double CxShapeTest_setexpected(const char* expected)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
        current->expected = expected ? expected : "";
    return 0.0;
}

static double CxShapeTest_setinitialellipse(double cx, double cy, double rx, double ry)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
    {
        current->has_initial_ellipse = true;
        current->initial_cx = cx;
        current->initial_cy = cy;
        current->initial_rx = rx;
        current->initial_ry = ry;
    }
    return 0.0;
}

static double CxShapeTest_seteditable(double editable)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
        current->editable = (editable != 0.0);
    return 0.0;
}

static double CxShapeTest_setvisible(double visible)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
        current->visible = (visible != 0.0);
    return 0.0;
}

static double CxShapeTest_setexpectedhit(double expected)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
        current->expected_hit = static_cast<int>(expected);
    return 0.0;
}

static double CxShapeTest_setexpectedbegindrag(double expected)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
        current->expected_begin_drag = static_cast<int>(expected);
    return 0.0;
}

static double CxShapeTest_setexpectedcommit(double expected)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
        current->expected_commit = static_cast<int>(expected);
    return 0.0;
}

static double CxShapeTest_setexpectedradiusx(double expected)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
    {
        current->has_expected_radius_x = true;
        current->expected_radius_x = expected;
    }
    return 0.0;
}

static double CxShapeTest_setexpectedradiusy(double expected)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
    {
        current->has_expected_radius_y = true;
        current->expected_radius_y = expected;
    }
    return 0.0;
}

void RegisterCxShapeTestBindings(mu::Parser& parser)
{
    parser.DefineFun("CxShapeTest_reset", (mu::fun_type1)&CxShapeTest_reset);
    parser.DefineFun("CxShapeTest_addcase", (mu::strfun_type1)&CxShapeTest_addcase);
    parser.DefineFun("CxShapeTest_settool", (mu::strfun_type1)&CxShapeTest_settool);
    parser.DefineFun("CxShapeTest_setoperation", (mu::strfun_type1)&CxShapeTest_setoperation);
    parser.DefineFun("CxShapeTest_sethandle", (mu::strfun_type1)&CxShapeTest_sethandle);
    parser.DefineFun("CxShapeTest_setfrom", (mu::fun_type2)&CxShapeTest_setfrom);
    parser.DefineFun("CxShapeTest_setto", (mu::fun_type2)&CxShapeTest_setto);
    parser.DefineFun("CxShapeTest_setzoom", (mu::fun_type1)&CxShapeTest_setzoom);
    parser.DefineFun("CxShapeTest_setpan", (mu::fun_type2)&CxShapeTest_setpan);
    parser.DefineFun("CxShapeTest_setvertex", (mu::fun_type1)&CxShapeTest_setvertex);
    parser.DefineFun("CxShapeTest_setexpected", (mu::strfun_type1)&CxShapeTest_setexpected);
    parser.DefineFun("CxShapeTest_setinitialellipse", (mu::fun_type4)&CxShapeTest_setinitialellipse);
    parser.DefineFun("CxShapeTest_seteditable", (mu::fun_type1)&CxShapeTest_seteditable);
    parser.DefineFun("CxShapeTest_setvisible", (mu::fun_type1)&CxShapeTest_setvisible);
    parser.DefineFun("CxShapeTest_setexpectedhit", (mu::fun_type1)&CxShapeTest_setexpectedhit);
    parser.DefineFun("CxShapeTest_setexpectedbegindrag", (mu::fun_type1)&CxShapeTest_setexpectedbegindrag);
    parser.DefineFun("CxShapeTest_setexpectedcommit", (mu::fun_type1)&CxShapeTest_setexpectedcommit);
    parser.DefineFun("CxShapeTest_setexpectedradiusx", (mu::fun_type1)&CxShapeTest_setexpectedradiusx);
    parser.DefineFun("CxShapeTest_setexpectedradiusy", (mu::fun_type1)&CxShapeTest_setexpectedradiusy);
}