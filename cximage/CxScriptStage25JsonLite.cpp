#include "CxScriptStage25JsonLite.h"
#include <fstream>
#include <sstream>
#include <cctype>

bool Stage25JsonLite::LoadFile(const std::filesystem::path& path)
{
    std::ifstream file(path);
    if (!file.is_open())
        return false;

    std::stringstream buffer;
    buffer << file.rdbuf();
    text_ = buffer.str();
    return true;
}

size_t Stage25JsonLite::FindKey(const std::string& key) const
{
    const std::string pattern = "\"" + key + "\"";
    return text_.find(pattern);
}

bool Stage25JsonLite::Has(const std::string& key) const
{
    return FindKey(key) != std::string::npos;
}

std::string Stage25JsonLite::GetString(
    const std::string& key,
    const std::string& fallback) const
{
    const auto pos = FindKey(key);
    if (pos == std::string::npos)
        return fallback;

    const auto colon = text_.find(":", pos);
    if (colon == std::string::npos)
        return fallback;

    const auto quote = text_.find("\"", colon + 1);
    if (quote == std::string::npos)
        return fallback;

    const auto end = text_.find("\"", quote + 1);
    if (end == std::string::npos)
        return fallback;

    return text_.substr(quote + 1, end - quote - 1);
}

int Stage25JsonLite::GetInt(
    const std::string& key,
    int fallback) const
{
    const auto pos = FindKey(key);
    if (pos == std::string::npos)
        return fallback;

    const auto colon = text_.find(":", pos);
    if (colon == std::string::npos)
        return fallback;

    const auto start = text_.find_first_of("-0123456789", colon + 1);
    if (start == std::string::npos)
        return fallback;

    const auto end = text_.find_first_not_of("-0123456789", start);

    try
    {
        return std::stoi(text_.substr(start, end - start));
    }
    catch (...)
    {
        return fallback;
    }
}

double Stage25JsonLite::GetDouble(
    const std::string& key,
    double fallback) const
{
    const auto pos = FindKey(key);
    if (pos == std::string::npos)
        return fallback;

    const auto colon = text_.find(":", pos);
    if (colon == std::string::npos)
        return fallback;

    const auto start = text_.find_first_of("-0123456789.", colon + 1);
    if (start == std::string::npos)
        return fallback;

    const auto end = text_.find_first_not_of("-0123456789.eE+", start);

    try
    {
        return std::stod(text_.substr(start, end - start));
    }
    catch (...)
    {
        return fallback;
    }
}

bool Stage25JsonLite::GetBool(
    const std::string& key,
    bool fallback) const
{
    const std::string value = GetString(key);
    if (value == "true")
        return true;
    if (value == "false")
        return false;
    return fallback;
}
