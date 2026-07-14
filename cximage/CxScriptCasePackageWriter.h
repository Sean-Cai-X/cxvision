#ifndef CXIMAGE_CXSCRIPT_CASE_PACKAGE_WRITER_H
#define CXIMAGE_CXSCRIPT_CASE_PACKAGE_WRITER_H

#include <string>
#include "ViewController.h"

bool SaveCasePackage(
    ManualTestContext& context,
    const std::string& caseName,
    const std::string& outputDir,
    std::string& outPath,
    std::string& outReason);

#endif
