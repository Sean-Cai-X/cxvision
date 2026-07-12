#ifndef CXIMAGE_CX_UNIFIED_LOG_OPTIONS_H
#define CXIMAGE_CX_UNIFIED_LOG_OPTIONS_H

#include <string>
#include <filesystem>
#include "CxUnifiedLog.h"

struct CxUnifiedLogOptions
{
    std::filesystem::path path;
    bool enabled = true;
    bool capture_stdio = true;
    CxLogLevel min_level = CxLogLevel::Info;
    std::string smoke_id;
    bool smoke_mode = false;
};

bool ParseUnifiedLogArgs(
    int argc,
    char** argv,
    CxUnifiedLogOptions& options,
    std::string& reason);

std::string DetectCxVisionRunMode(int argc, char** argv);

std::string RedactCommandLine(int argc, char** argv);

#endif