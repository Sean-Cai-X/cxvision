#include "pch.h"
#include "CxShapeTestRuntime.h"

std::vector<CxShapeTestCase> CxShapeTestRuntime::s_cases;
CxShapeTestCase* CxShapeTestRuntime::s_current = nullptr;

void CxShapeTestRuntime::Reset()
{
    s_cases.clear();
    s_current = nullptr;
}

void CxShapeTestRuntime::AddCase(const std::string& case_id)
{
    s_cases.push_back({});
    s_cases.back().case_id = case_id;
    s_current = &s_cases.back();
}

CxShapeTestCase* CxShapeTestRuntime::Current()
{
    return s_current;
}

const std::vector<CxShapeTestCase>& CxShapeTestRuntime::Cases()
{
    return s_cases;
}

const CxShapeTestCase* CxShapeTestRuntime::FindCase(const std::string& case_id)
{
    for (const auto& c : s_cases)
    {
        if (c.case_id == case_id)
            return &c;
    }
    return nullptr;
}