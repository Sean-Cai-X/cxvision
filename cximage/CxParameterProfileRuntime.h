#pragma once

#include <string>
#include <vector>

struct CxParameterProfile
{
    std::string profile_id;
    std::string tool;
    std::string role;
    std::string description;

    int method = 0;
    int threshold = 20;
    int gap = 5;
    int linegap = 6;
    int wgap = 32;
    int hgap = 8;
    int filterprofile = 0;
};

struct CxParameterProfileRuntime
{
    std::string current_tool;
    std::vector<CxParameterProfile> profiles;

    const CxParameterProfile* FindProfile(const std::string& profile_id) const;
    const CxParameterProfile* FindProfileByToolAndRole(const std::string& tool, const std::string& role) const;
    void Clear();
};

bool LoadCxParameterProfileFile(
    const std::string& script_path,
    CxParameterProfileRuntime& out_profiles,
    std::string& out_reason);
