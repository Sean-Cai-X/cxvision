#include "CxEvidenceSelfTestRuntime.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace
{
std::string JsonEscape(const std::string& text)
{
    std::string out;
    for (char c : text)
    {
        switch (c)
        {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': break;
        case '\t': out += "\\t"; break;
        default: out += c; break;
        }
    }
    return out;
}
}

void AddEvidenceSelfTestStep(
    CxEvidenceSelfTestResult& result,
    const std::string& code,
    const std::string& status,
    const std::string& reason)
{
    CxEvidenceSelfTest