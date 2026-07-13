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

static double CxShapeTest_setinitialpoints(const char* points_str)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current && points_str)
    {
        current->has_initial_points = true;
        current->initial_points.clear();
        
        std::string str = points_str;
        size_t pos = 0;
        while (pos < str.size())
        {
            size_t end = str.find_first_of(", ", pos);
            if (end == std::string::npos)
                end = str.size();
            
            try
            {
                current->initial_points.push_back(std::stod(str.substr(pos, end - pos)));
            }
            catch (...)
            {
            }
            
            pos = end + 1;
            while (pos < str.size() && (str[pos] == ',' || str[pos] == ' '))
                pos++;
        }
    }
    return 0.0;
}

static double CxShapeTest_setinitialrect(double x0, double y0, double x1, double y1)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
    {
        current->has_initial_rect = true;
        current->initial_rx0 = x0;
        current->initial_ry0 = y0;
        current->initial_rx1 = x1;
        current->initial_ry1 = y1;
    }
    return 0.0;
}

static double CxShapeTest_setinitialcircle(double cx, double cy, double radius)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
    {
        current->has_initial_circle = true;
        current->initial_ccx = cx;
        current->initial_ccy = cy;
        current->initial_cradius = radius;
    }
    return 0.0;
}

static double CxShapeTest_setinitialellipse(double cx, double cy, double rx, double ry)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
    {
        current->has_initial_ellipse = true;
        current->initial_ex = cx;
        current->initial_ey = cy;
        current->initial_erx = rx;
        current->initial_ery = ry;
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

static double CxShapeTest_setpointersequence(const char* sequence)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
        current->pointer_sequence = sequence ? sequence : "";
    return 0.0;
}

static double CxShapeTest_setexpectedshapedelta(double delta)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
        current->expected_shape_count_delta = static_cast<int>(delta);
    return 0.0;
}

static double CxShapeTest_setexpectedcreatedkind(const char* kind)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
        current->expected_created_kind = kind ? kind : "";
    return 0.0;
}

static double CxShapeTest_setexpectedphase(const char* phase)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
        current->expected_phase = phase ? phase : "";
    return 0.0;
}

static double CxShapeTest_setexpectedstatus(const char* status)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
        current->expected_status = status ? status : "";
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
    parser.DefineFun("CxShapeTest_setinitialpoints", (mu::strfun_type1)&CxShapeTest_setinitialpoints);
    parser.DefineFun("CxShapeTest_setinitialrect", (mu::fun_type4)&CxShapeTest_setinitialrect);
    parser.DefineFun("CxShapeTest_setinitialcircle", (mu::fun_type3)&CxShapeTest_setinitialcircle);
    parser.DefineFun("CxShapeTest_setinitialellipse", (mu::fun_type4)&CxShapeTest_setinitialellipse);
    parser.DefineFun("CxShapeTest_seteditable", (mu::fun_type1)&CxShapeTest_seteditable);
    parser.DefineFun("CxShapeTest_setvisible", (mu::fun_type1)&CxShapeTest_setvisible);
    parser.DefineFun("CxShapeTest_setexpectedhit", (mu::fun_type1)&CxShapeTest_setexpectedhit);
    parser.DefineFun("CxShapeTest_setexpectedbegindrag", (mu::fun_type1)&CxShapeTest_setexpectedbegindrag);
    parser.DefineFun("CxShapeTest_setexpectedcommit", (mu::fun_type1)&CxShapeTest_setexpectedcommit);
    parser.DefineFun("CxShapeTest_setexpectedradiusx", (mu::fun_type1)&CxShapeTest_setexpectedradiusx);
    parser.DefineFun("CxShapeTest_setexpectedradiusy", (mu::fun_type1)&CxShapeTest_setexpectedradiusy);
    parser.DefineFun("CxShapeTest_setpointersequence", (mu::strfun_type1)&CxShapeTest_setpointersequence);
    parser.DefineFun("CxShapeTest_setexpectedshapedelta", (mu::fun_type1)&CxShapeTest_setexpectedshapedelta);
    parser.DefineFun("CxShapeTest_setexpectedcreatedkind", (mu::strfun_type1)&CxShapeTest_setexpectedcreatedkind);
    parser.DefineFun("CxShapeTest_setexpectedphase", (mu::strfun_type1)&CxShapeTest_setexpectedphase);
    parser.DefineFun("CxShapeTest_setexpectedstatus", (mu::strfun_type1)&CxShapeTest_setexpectedstatus);
}