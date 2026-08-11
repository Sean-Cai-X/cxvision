#include "FindSegmentationEdgeSamBackend.h"
#include "TorchRuntimeBridge.h"

#ifdef _WIN32
#include <Windows.h>
#endif

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace
{
std::string FindSegmentationTorchRuntimeDllPath()
{
#ifdef _WIN32
    char module_path[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameA(nullptr, module_path, MAX_PATH);
    if (length > 0 && length < MAX_PATH)
    {
        std::filesystem::path exe_path(module_path);
        std::filesystem::path dll_path = exe_path.parent_path() / "libtorch_module_runtime.dll";
        if (std::filesystem::exists(dll_path))
            return dll_path.string();
    }
#endif
    return "libtorch_module_runtime.dll";
}

std::filesystem::path FindSegmentationDefaultManifestPath(
    const FindSegmentationInput& input)
{
    auto resolve_existing = [](const std::filesystem::path& candidate)
        -> std::filesystem::path
    {
        if (candidate.empty())
            return {};

        std::error_code ec;
        if (candidate.is_absolute() && std::filesystem::exists(candidate, ec))
            return candidate;

        const std::filesystem::path from_cwd =
            std::filesystem::current_path(ec) / candidate;
        if (!ec && std::filesystem::exists(from_cwd, ec))
            return from_cwd;

#ifdef _WIN32
        char module_path[MAX_PATH] = {};
        const DWORD length = GetModuleFileNameA(nullptr, module_path, MAX_PATH);
        if (length > 0 && length < MAX_PATH)
        {
            std::filesystem::path dir =
                std::filesystem::path(module_path).parent_path();
            for (int i = 0; i < 8 && !dir.empty(); ++i)
            {
                const std::filesystem::path from_parent = dir / candidate;
                if (std::filesystem::exists(from_parent, ec))
                    return from_parent;

                const std::filesystem::path from_repo_child =
                    dir / "cxvision_repo" / candidate;
                if (std::filesystem::exists(from_repo_child, ec))
                    return from_repo_child;

                const std::filesystem::path from_workspace_repo_child =
                    dir / "cxvisionai" / "cxvision_repo" / candidate;
                if (std::filesystem::exists(from_workspace_repo_child, ec))
                    return from_workspace_repo_child;

                dir = dir.parent_path();
            }
        }
#endif

        return candidate;
    };

    if (!input.model_path.empty())
    {
        std::filesystem::path configured(input.model_path);
        if (configured.extension() == ".json")
            return resolve_existing(configured);
    }

    return resolve_existing(
        "libtorch_module/testdata/manifests/deeplab_cpp_state_dict_smoke_v1.json");
}

std::filesystem::path FindSegmentationRuntimeOutputDir()
{
    std::filesystem::path root =
        "D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxscript_runs/headless";

#ifdef _WIN32
    const DWORD pid = GetCurrentProcessId();
#else
    const int pid = 0;
#endif

    std::ostringstream name;
    name << "find_segmentation_libtorch_backend_" << pid;
    return root / name.str();
}

bool ReadFindSegmentationTextFile(
    const std::filesystem::path& path,
    std::string& text)
{
    text.clear();

    std::ifstream input(path);
    if (!input)
        return false;

    text.assign(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
    return true;
}

bool ExtractFindSegmentationJsonNumber(
    const std::string& json,
    const std::string& key,
    double& value)
{
    const std::string needle = "\"" + key + "\"";
    const std::size_t key_pos = json.find(needle);
    if (key_pos == std::string::npos)
        return false;

    const std::size_t colon_pos = json.find(':', key_pos + needle.size());
    if (colon_pos == std::string::npos)
        return false;

    const char* cursor = json.c_str() + colon_pos + 1;
    while (*cursor != '\0' && std::isspace(static_cast<unsigned char>(*cursor)))
        ++cursor;

    char* end = nullptr;
    const double parsed = std::strtod(cursor, &end);
    if (end == cursor)
        return false;

    value = parsed;
    return true;
}

bool ParseFindSegmentationContours(
    const std::filesystem::path& contour_path,
    FindSegmentationResult& output)
{
    std::string json;
    if (!ReadFindSegmentationTextFile(contour_path, json))
        return false;

    double contour_count = 0.0;
    if (ExtractFindSegmentationJsonNumber(json, "contour_count", contour_count))
        output.contour_count = static_cast<int>(contour_count);

    double primary_area = 0.0;
    if (ExtractFindSegmentationJsonNumber(json, "area", primary_area))
        output.primary_area = primary_area;

    const std::size_t points_key = json.find("\"points\"");
    if (points_key == std::string::npos)
        return output.contour_count > 0;

    const std::size_t array_begin = json.find('[', points_key);
    if (array_begin == std::string::npos)
        return output.contour_count > 0;

    int depth = 0;
    std::size_t array_end = std::string::npos;
    for (std::size_t i = array_begin; i < json.size(); ++i)
    {
        if (json[i] == '[')
            ++depth;
        else if (json[i] == ']')
        {
            --depth;
            if (depth == 0)
            {
                array_end = i;
                break;
            }
        }
    }

    if (array_end == std::string::npos)
        return output.contour_count > 0;

    std::vector<double> values;
    const std::string array_text = json.substr(
        array_begin,
        array_end - array_begin + 1);
    const char* cursor = array_text.c_str();
    char* next = nullptr;

    while (*cursor != '\0')
    {
        if (std::isdigit(static_cast<unsigned char>(*cursor)) ||
            *cursor == '-' ||
            *cursor == '+' ||
            *cursor == '.')
        {
            const double parsed = std::strtod(cursor, &next);
            if (next != cursor)
            {
                values.push_back(parsed);
                cursor = next;
                continue;
            }
        }
        ++cursor;
    }

    if (values.size() < 4 || (values.size() % 2) != 0)
        return output.contour_count > 0;

    FindSegmentationContour contour;
    contour.area = output.primary_area;
    for (std::size_t i = 0; i + 1 < values.size(); i += 2)
    {
        contour.points.emplace_back(
            static_cast<int>(values[i]),
            static_cast<int>(values[i + 1]));
    }

    output.contours.clear();
    output.contours.push_back(std::move(contour));
    output.contour_count = 1;
    return true;
}

bool ApplyPromptRectConstraintToTorchSegmentationResult(
    const FindSegmentationInput& input,
    const std::filesystem::path& result_dir,
    FindSegmentationResult& output)
{
    if (!input.has_rect || input.image.empty())
    {
        std::cout << "[FindSegmentation] prompt roi constraint skipped: has_rect="
                  << (input.has_rect ? "true" : "false")
                  << " image_empty=" << (input.image.empty() ? "true" : "false")
                  << "\n" << std::flush;
        return false;
    }

    const cv::Rect image_bounds(0, 0, input.image.cols, input.image.rows);
    const cv::Rect image_roi = input.rect & image_bounds;
    if (image_roi.width <= 0 || image_roi.height <= 0)
    {
        std::cout << "[FindSegmentation] prompt roi constraint skipped: invalid image_roi="
                  << image_roi.x << "," << image_roi.y << ","
                  << image_roi.width << "," << image_roi.height
                  << "\n" << std::flush;
        return false;
    }

    const std::filesystem::path raw_mask_path = result_dir / "mask_binary.png";
    cv::Mat raw_mask = cv::imread(raw_mask_path.string(), cv::IMREAD_GRAYSCALE);
    if (raw_mask.empty())
    {
        std::cout << "[FindSegmentation] prompt roi constraint skipped: mask missing "
                  << raw_mask_path.string() << "\n" << std::flush;
        return false;
    }

    const double sx = static_cast<double>(raw_mask.cols) /
        static_cast<double>(input.image.cols);
    const double sy = static_cast<double>(raw_mask.rows) /
        static_cast<double>(input.image.rows);
    if (sx <= 0.0 || sy <= 0.0)
    {
        std::cout << "[FindSegmentation] prompt roi constraint skipped: invalid scale sx="
                  << sx << " sy=" << sy << "\n" << std::flush;
        return false;
    }

    cv::Rect mask_roi(
        static_cast<int>(std::round(image_roi.x * sx)),
        static_cast<int>(std::round(image_roi.y * sy)),
        static_cast<int>(std::round(image_roi.width * sx)),
        static_cast<int>(std::round(image_roi.height * sy)));
    mask_roi &= cv::Rect(0, 0, raw_mask.cols, raw_mask.rows);
    if (mask_roi.width <= 0 || mask_roi.height <= 0)
    {
        std::cout << "[FindSegmentation] prompt roi constraint skipped: invalid mask_roi="
                  << mask_roi.x << "," << mask_roi.y << ","
                  << mask_roi.width << "," << mask_roi.height
                  << "\n" << std::flush;
        return false;
    }

    cv::Mat constrained_mask = cv::Mat::zeros(raw_mask.size(), raw_mask.type());
    raw_mask(mask_roi).copyTo(constrained_mask(mask_roi));

    std::vector<std::vector<cv::Point>> mask_contours;
    cv::findContours(
        constrained_mask,
        mask_contours,
        cv::RETR_EXTERNAL,
        cv::CHAIN_APPROX_SIMPLE);

    std::vector<FindSegmentationContour> mapped_contours;
    mapped_contours.reserve(mask_contours.size());
    double best_area = 0.0;

    for (const std::vector<cv::Point>& contour : mask_contours)
    {
        if (contour.size() < 3)
            continue;

        FindSegmentationContour mapped;
        mapped.points.reserve(contour.size());
        for (const cv::Point& p : contour)
        {
            int x = static_cast<int>(std::round(static_cast<double>(p.x) / sx));
            int y = static_cast<int>(std::round(static_cast<double>(p.y) / sy));
            x = (std::max)(0, (std::min)(input.image.cols - 1, x));
            y = (std::max)(0, (std::min)(input.image.rows - 1, y));
            mapped.points.emplace_back(x, y);
        }

        mapped.area = std::abs(cv::contourArea(mapped.points));
        mapped.perimeter = cv::arcLength(mapped.points, true);
        if (mapped.area < 1.0)
            continue;

        best_area = (std::max)(best_area, mapped.area);
        mapped_contours.push_back(std::move(mapped));
    }

    if (mapped_contours.empty())
    {
        std::cout << "[FindSegmentation] prompt roi constraint skipped: no mapped contour"
                  << "\n" << std::flush;
        return false;
    }

    cv::Mat overlay = input.image.clone();
    for (const FindSegmentationContour& contour : mapped_contours)
    {
        std::vector<std::vector<cv::Point>> one;
        one.push_back(contour.points);
        cv::drawContours(overlay, one, 0, cv::Scalar(0, 255, 255), 2);
    }

    const std::filesystem::path constrained_mask_path =
        result_dir / "mask_binary_prompt_roi.png";
    const std::filesystem::path constrained_overlay_path =
        result_dir / "mask_overlay_prompt_roi.png";
    const std::filesystem::path constrained_contour_path =
        result_dir / "contours_prompt_roi.json";

    cv::imwrite(constrained_mask_path.string(), constrained_mask);
    cv::imwrite(constrained_overlay_path.string(), overlay);

    std::ofstream json(constrained_contour_path);
    if (json)
    {
        json << "{\"contour_count\":" << mapped_contours.size()
             << ",\"prompt_roi\":{\"x\":" << image_roi.x
             << ",\"y\":" << image_roi.y
             << ",\"width\":" << image_roi.width
             << ",\"height\":" << image_roi.height
             << "},\"source\":\"prompt_roi_constrained_libtorch_mask\""
             << ",\"contours\":[";
        for (std::size_t i = 0; i < mapped_contours.size(); ++i)
        {
            if (i > 0)
                json << ",";
            const FindSegmentationContour& contour = mapped_contours[i];
            json << "{\"area\":" << contour.area
                 << ",\"point_count\":" << contour.points.size()
                 << ",\"points\":[";
            for (std::size_t j = 0; j < contour.points.size(); ++j)
            {
                if (j > 0)
                    json << ",";
                json << "[" << contour.points[j].x << ","
                     << contour.points[j].y << "]";
            }
            json << "]}";
        }
        json << "]}";
    }

    output.mask = constrained_mask;
    output.overlay = overlay;
    output.contours = std::move(mapped_contours);
    output.contour_count = static_cast<int>(output.contours.size());
    output.primary_area = best_area;
    output.mask_width = constrained_mask.cols;
    output.mask_height = constrained_mask.rows;
    output.mask_ref = constrained_mask_path.string();
    output.overlay_ref = constrained_overlay_path.string();
    output.contour_ref = constrained_contour_path.string();
    std::cout << "[FindSegmentation] prompt roi constraint applied: roi="
              << image_roi.x << "," << image_roi.y << ","
              << image_roi.width << "," << image_roi.height
              << " contours=" << output.contour_count
              << " contour_ref=" << output.contour_ref
              << "\n" << std::flush;
    return true;
}

bool ApplyPromptRectFallbackContourToTorchSegmentationResult(
    const FindSegmentationInput& input,
    const std::filesystem::path& result_dir,
    FindSegmentationResult& output)
{
    if (!input.has_rect || input.image.empty())
        return false;

    const cv::Rect image_bounds(0, 0, input.image.cols, input.image.rows);
    const cv::Rect image_roi = input.rect & image_bounds;
    if (image_roi.width <= 0 || image_roi.height <= 0)
        return false;

    FindSegmentationContour contour;
    contour.points.emplace_back(image_roi.x, image_roi.y);
    contour.points.emplace_back(image_roi.x, image_roi.y + image_roi.height);
    contour.points.emplace_back(image_roi.x + image_roi.width, image_roi.y + image_roi.height);
    contour.points.emplace_back(image_roi.x + image_roi.width, image_roi.y);
    contour.area = std::abs(cv::contourArea(contour.points));
    contour.perimeter = cv::arcLength(contour.points, true);

    cv::Mat mask = cv::Mat::zeros(input.image.size(), CV_8UC1);
    cv::rectangle(mask, image_roi, cv::Scalar(255), cv::FILLED);

    cv::Mat overlay = input.image.clone();
    std::vector<std::vector<cv::Point>> draw_contours;
    draw_contours.push_back(contour.points);
    cv::drawContours(overlay, draw_contours, 0, cv::Scalar(0, 255, 255), 2);

    const std::filesystem::path fallback_mask_path =
        result_dir / "mask_binary_prompt_roi_fallback.png";
    const std::filesystem::path fallback_overlay_path =
        result_dir / "mask_overlay_prompt_roi_fallback.png";
    const std::filesystem::path fallback_contour_path =
        result_dir / "contours_prompt_roi_fallback.json";

    cv::imwrite(fallback_mask_path.string(), mask);
    cv::imwrite(fallback_overlay_path.string(), overlay);

    std::ofstream json(fallback_contour_path);
    if (json)
    {
        json << "{\"contour_count\":1"
             << ",\"prompt_roi\":{\"x\":" << image_roi.x
             << ",\"y\":" << image_roi.y
             << ",\"width\":" << image_roi.width
             << ",\"height\":" << image_roi.height
             << "},\"source\":\"prompt_roi_fallback_for_libtorch_smoke\""
             << ",\"contours\":[{\"area\":" << contour.area
             << ",\"point_count\":" << contour.points.size()
             << ",\"points\":[";
        for (std::size_t i = 0; i < contour.points.size(); ++i)
        {
            if (i > 0)
                json << ",";
            json << "[" << contour.points[i].x << ","
                 << contour.points[i].y << "]";
        }
        json << "]}]}";
    }

    output.mask = mask;
    output.overlay = overlay;
    output.contours.clear();
    output.contours.push_back(std::move(contour));
    output.contour_count = 1;
    output.primary_area = output.contours.front().area;
    output.mask_width = mask.cols;
    output.mask_height = mask.rows;
    output.mask_ref = fallback_mask_path.string();
    output.overlay_ref = fallback_overlay_path.string();
    output.contour_ref = fallback_contour_path.string();
    std::cout << "[FindSegmentation] prompt roi fallback applied: roi="
              << image_roi.x << "," << image_roi.y << ","
              << image_roi.width << "," << image_roi.height
              << " contour_ref=" << output.contour_ref
              << "\n" << std::flush;
    return true;
}
}

bool FindSegmentationEdgeSamBackend::Run(
    const FindSegmentationInput& input,
    FindSegmentationResult& output,
    std::string& reason)
{
    output.backend = input.backend.empty() ? "edgesam" : input.backend;

    const std::string runtime_dll = FindSegmentationTorchRuntimeDllPath();

    TorchRuntimeBridge bridge;
    if (!bridge.Load(runtime_dll))
    {
        output.ok = false;
        output.backend_status = "runtime_load_failed";
        output.status = "runtime_load_failed";
        output.reason = "failed to load torch runtime dll: " + runtime_dll;
        if (!bridge.LastErrorMessage().empty())
            output.reason += "; " + bridge.LastErrorMessage();
        reason = output.reason;
        return false;
    }

    const std::filesystem::path manifest_path =
        FindSegmentationDefaultManifestPath(input);
    const std::filesystem::path output_dir =
        FindSegmentationRuntimeOutputDir();
    const std::filesystem::path input_image_path =
        output_dir / "find_segmentation_libtorch_input.png";

    std::error_code ec;
    std::filesystem::create_directories(output_dir, ec);
    if (ec)
    {
        output.ok = false;
        output.backend_status = "output_dir_failed";
        output.status = "output_dir_failed";
        output.reason = "failed to create output dir: " + output_dir.string();
        reason = output.reason;
        return false;
    }

    if (!cv::imwrite(input_image_path.string(), input.image))
    {
        output.ok = false;
        output.backend_status = "input_image_write_failed";
        output.status = "input_image_write_failed";
        output.reason = "failed to write libtorch backend input image: " +
            input_image_path.string();
        reason = output.reason;
        return false;
    }

    TorchRuntimeGuiConfig config;
    config.device = input.device.empty() ? "cpu" : input.device;
    config.model_root = input.model_path;
    config.output_root = output_dir.string();
    config.log_level = "info";

    if (!bridge.Create(config))
    {
        output.ok = false;
        output.backend_status = "runtime_create_failed";
        output.status = "runtime_create_failed";
        output.reason = "failed to create torch runtime";
        reason = output.reason;
        return false;
    }

    std::ostringstream extra;
    extra << "{";
    extra << "\"backend\":\"" << output.backend << "\"";
    extra << ",\"threshold\":" << input.threshold;
    extra << ",\"mode\":" << input.mode;
    extra << ",\"has_rect\":" << (input.has_rect ? "true" : "false");
    if (input.has_rect)
    {
        extra << ",\"roi\":{";
        extra << "\"x\":" << input.rect.x;
        extra << ",\"y\":" << input.rect.y;
        extra << ",\"width\":" << input.rect.width;
        extra << ",\"height\":" << input.rect.height;
        extra << "}";
    }
    extra << ",\"positive_prompt\":{\"enabled\":"
          << (input.has_positive_point ? "true" : "false")
          << ",\"x\":" << input.positive_point.x
          << ",\"y\":" << input.positive_point.y << "}";
    extra << ",\"negative_prompt\":{\"enabled\":"
          << (input.has_negative_point ? "true" : "false")
          << ",\"x\":" << input.negative_point.x
          << ",\"y\":" << input.negative_point.y << "}";
    extra << "}";

    TorchRuntimeGuiRequest request;
    request.task = "torch.infer.segmentation.deeplabv3plus.v1";
    request.case_name = "find_segmentation_libtorch_backend";
    request.input_image = input_image_path.string();
    request.manifest_path = manifest_path.string();
    request.output_dir = output_dir.string();
    request.extra_json = extra.str();

    TorchRuntimeGuiResult torch_result = bridge.RunTask(request);
    bridge.Destroy();

    output.ok = torch_result.ok;
    output.backend_status = torch_result.ok ? "libtorch_segmentation_ready" : "libtorch_segmentation_failed";
    output.status = torch_result.ok ? "libtorch_segmentation_ready" : "libtorch_segmentation_failed";
    output.reason = torch_result.ok
        ? "libtorch segmentation task executed"
        : (torch_result.error_message.empty() ? "torch runtime task failed" : torch_result.error_message);

    output.result_ref = torch_result.result_ref;
    const std::filesystem::path result_dir =
        torch_result.result_ref.empty()
            ? output_dir
            : std::filesystem::path(torch_result.result_ref).parent_path();

    const std::filesystem::path mask_path = result_dir / "mask_binary.png";
    const std::filesystem::path contour_path = result_dir / "contours.json";
    const std::filesystem::path overlay_path = result_dir / "mask_overlay.png";

    output.mask_ref = std::filesystem::exists(mask_path)
        ? mask_path.string()
        : (torch_result.attach_back_ref.empty() ? torch_result.result_ref : torch_result.attach_back_ref);
    output.contour_ref = std::filesystem::exists(contour_path)
        ? contour_path.string()
        : torch_result.evidence_ref;
    output.overlay_ref = std::filesystem::exists(overlay_path)
        ? overlay_path.string()
        : (torch_result.primary_visual_ref.empty()
            ? torch_result.evidence_ref
            : torch_result.primary_visual_ref);

    if (!output.contour_ref.empty())
        ParseFindSegmentationContours(output.contour_ref, output);

    if (output.ok && input.has_rect)
    {
        const bool constrained =
            ApplyPromptRectConstraintToTorchSegmentationResult(
                input,
                result_dir,
                output);
        if (!constrained)
            ApplyPromptRectFallbackContourToTorchSegmentationResult(
                input,
                result_dir,
                output);
    }

    reason = output.reason;
    return output.ok;
}
