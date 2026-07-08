#pragma once

#include "CxScriptSuiteRunner.h"
#include "CxScriptImageManifestRuntime.h"
#include <filesystem>

class CxScriptSuiteReportWriter
{
public:
    static void WriteSuiteRunReport(
        const std::filesystem::path& outRoot,
        const std::vector<CxScriptSuiteCaseResult>& caseResults);

    static void WriteSuiteRunReportJson(
        const std::filesystem::path& outRoot,
        const std::vector<CxScriptSuiteCaseResult>& caseResults);

    static void WriteImageManifestContractReport(
        const std::filesystem::path& outRoot,
        const CxScriptImageManifestRuntime& manifest,
        const CxScriptImageManifestValidationResult& validation);

    static void WriteImageManifestContractReportJson(
        const std::filesystem::path& outRoot,
        const CxScriptImageManifestRuntime& manifest,
        const CxScriptImageManifestValidationResult& validation);

    static void WriteBestDetectionGallery(
        const std::filesystem::path& outRoot,
        const std::vector<CxScriptSuiteCaseResult>& caseResults);

    static void WriteToolDisplayIndex(
        const std::filesystem::path& outRoot,
        const std::vector<CxScriptSuiteCaseResult>& caseResults);

    static void WriteFindlineAlgorithmIterationReport(
        const std::filesystem::path& outRoot,
        const std::vector<CxScriptSuiteCaseResult>& caseResults);

    static void WriteFindcircleAlgorithmIterationReport(
        const std::filesystem::path& outRoot,
        const std::vector<CxScriptSuiteCaseResult>& caseResults);

    static void WriteFailureClassificationReport(
        const std::filesystem::path& outRoot,
        const std::vector<CxScriptSuiteCaseResult>& caseResults);
};