#include "torch_runtime_artifact_writer.h"
#include <opencv2/imgcodecs.hpp>
#include <fstream>

bool WriteTorchTextArtifact(
    const std::filesystem::path& path,
    const std::string& content,
    std::string& reason)
{
    try
    {
        std::filesystem::create_directories(path.parent_path());

        std::ofstream file(path);
        if (!file.is_open())
        {
            reason = "failed to open file for writing: " + path.string();
            return false;
        }

        file << content;
        if (!file)
        {
            reason = "failed to write content to file: " + path.string();
            return false;
        }

        file.close();
        return true;
    }
    catch (const std::exception& e)
    {
        reason = "exception while writing artifact: " + std::string(e.what());
        return false;
    }
}

bool WriteTorchImageArtifact(
    const std::filesystem::path& path,
    const cv::Mat& image,
    std::string& reason)
{
    try
    {
        std::filesystem::create_directories(path.parent_path());

        std::string ext = path.extension().string();
        std::vector<int> params = {cv::IMWRITE_PNG_COMPRESSION, 3};

        bool success = cv::imwrite(path.string(), image, params);
        if (!success)
        {
            reason = "failed to write image to file: " + path.string();
            return false;
        }

        return true;
    }
    catch (const std::exception& e)
    {
        reason = "exception while writing image artifact: " + std::string(e.what());
        return false;
    }
}

std::filesystem::path BuildTorchCaseDirectory(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request,
    std::string& reason)
{
    if (config.output_root.empty())
    {
        reason = "output_root is empty";
        return {};
    }

    std::string case_name = request.case_name;
    if (case_name.empty())
    {
        case_name = "unnamed_case";
    }

    std::replace(case_name.begin(), case_name.end(), '/', '_');
    std::replace(case_name.begin(), case_name.end(), '\\', '_');
    std::replace(case_name.begin(), case_name.end(), ':', '_');
    std::replace(case_name.begin(), case_name.end(), '*', '_');
    std::replace(case_name.begin(), case_name.end(), '?', '_');
    std::replace(case_name.begin(), case_name.end(), '"', '_');
    std::replace(case_name.begin(), case_name.end(), '<', '_');
    std::replace(case_name.begin(), case_name.end(), '>', '_');
    std::replace(case_name.begin(), case_name.end(), '|', '_');

    std::filesystem::path case_dir = std::filesystem::path(config.output_root) / case_name;

    try
    {
        std::filesystem::create_directories(case_dir);
    }
    catch (const std::exception& e)
    {
        reason = "failed to create case directory: " + std::string(e.what());
        return {};
    }

    return case_dir;
}
