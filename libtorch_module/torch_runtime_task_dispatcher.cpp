#include "torch_runtime_task_dispatcher.h"
#include "torch_runtime_task_types.h"
#include "torch_runtime_manifest.h"
#include "torch_runtime_segmentation_executor.h"
#include "torch_runtime_detection_executor.h"
#include "torch_segmentation_mainline_bridge.h"
#include "torch_test_host.h"
#include <chrono>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <sstream>
#include <vector>

namespace
{

TorchTaskResultCpp MakeUnsupportedTask(
    const std::string& task)
{
    TorchTaskResultCpp result;

    result.ok = false;
    result.error_code =
        static_cast<int>(
            TorchRuntimeErrorCode::UnsupportedTask);

    result.status = "unsupported_task";
    result.error_message =
        "unsupported production torch task: " + task;

    result.result_json =
        "{"
        "\"schema\":\"cxvision.torch.error.v1\","
        "\"failure_stage\":\"runtime_task_unsupported\","
        "\"task\":\"" + task + "\""
        "}";

    return result;
}

bool IsLegacyTestHostTask(
    const std::string& task)
{
    return TorchTestHost::find_task_spec(task) != nullptr;
}

std::string QuoteRuntimeTaskJsonString(const std::string& value)
{
    std::ostringstream os;
    os << '"';
    for (const char ch : value)
    {
        switch (ch)
        {
        case '\\': os << "\\\\"; break;
        case '"': os << "\\\""; break;
        case '\n': os << "\\n"; break;
        case '\r': os << "\\r"; break;
        case '\t': os << "\\t"; break;
        default: os << ch; break;
        }
    }
    os << '"';
    return os.str();
}

torch::Tensor MakeSegmentationLifecycleImages(
    const SegmentationMainlineRunnerConfig& config)
{
    const auto device = resolve_segmentation_device(config.device_policy);
    return torch::randn(
        {config.batch_size, 3, config.input_size, config.input_size},
        torch::TensorOptions().dtype(torch::kFloat32).device(device));
}

torch::Tensor MakeSegmentationLifecycleMasks(
    const SegmentationMainlineRunnerConfig& config)
{
    const auto device = resolve_segmentation_device(config.device_policy);
    return torch::randint(
        0,
        config.num_classes,
        {config.batch_size, config.input_size, config.input_size},
        torch::TensorOptions().dtype(torch::kLong).device(device));
}

std::string HashFileFnv1a64(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    uint64_t hash = 1469598103934665603ull;
    char buffer[4096];
    while (input.good())
    {
        input.read(buffer, sizeof(buffer));
        const std::streamsize count = input.gcount();
        for (std::streamsize i = 0; i < count; ++i)
        {
            hash ^= static_cast<unsigned char>(buffer[i]);
            hash *= 1099511628211ull;
        }
    }

    std::ostringstream os;
    os << std::hex << std::setw(16) << std::setfill('0') << hash;
    return os.str();
}

struct EvidenceNormBbox
{
    int class_id = 0;
    double cx = 0.0;
    double cy = 0.0;
    double w = 0.0;
    double h = 0.0;
};

struct EvidenceDatasetImage
{
    std::string image_id;
    std::filesystem::path path;
    std::string split;
    std::string label;
    std::vector<EvidenceNormBbox> boxes;
};

struct EvidenceSplitSummary
{
    int train_images = 0;
    int val_images = 0;
    int test_images = 0;
    int train_annotations = 0;
    int val_annotations = 0;
    int test_annotations = 0;
};

struct LifecycleEvidenceArtifacts
{
    int total_annotations = 0;
    EvidenceSplitSummary split_summary;
    std::string split_summary_ref;
    std::string mask_preview_ref;
    std::string bbox_overlay_ref;
};

std::string ExtractEvidencePayload(const std::string& line)
{
    const std::size_t first = line.find('"');
    const std::size_t last = line.rfind('"');
    if (first == std::string::npos || last == std::string::npos || last <= first)
        return {};
    return line.substr(first + 1, last - first - 1);
}

std::map<std::string, std::string> ParseEvidenceKeyValuePayload(
    const std::string& payload)
{
    std::map<std::string, std::string> values;
    std::istringstream input(payload);
    std::string token;
    while (input >> token)
    {
        const std::size_t equals = token.find('=');
        if (equals == std::string::npos || equals == 0)
            continue;
        values[token.substr(0, equals)] = token.substr(equals + 1);
    }
    return values;
}

double ParseEvidenceDouble(
    const std::map<std::string, std::string>& values,
    const std::string& key)
{
    const auto it = values.find(key);
    if (it == values.end())
        return 0.0;
    try
    {
        return std::stod(it->second);
    }
    catch (...)
    {
        return 0.0;
    }
}

int ParseEvidenceInt(
    const std::map<std::string, std::string>& values,
    const std::string& key)
{
    const auto it = values.find(key);
    if (it == values.end())
        return 0;
    try
    {
        return std::stoi(it->second);
    }
    catch (...)
    {
        return 0;
    }
}

std::vector<EvidenceDatasetImage> CollectEvidenceManifestAnnotatedImages(
    const TorchTaskRequestCpp& request,
    const int max_images_per_split,
    int& annotation_count)
{
    annotation_count = 0;
    if (request.manifest_path.empty())
        return {};

    std::filesystem::path manifest_path(request.manifest_path);
    std::error_code ec;
    if (!std::filesystem::exists(manifest_path, ec))
        return {};

    std::ifstream manifest(manifest_path);
    if (!manifest)
        return {};

    std::vector<EvidenceDatasetImage> rows;
    std::map<std::string, std::vector<EvidenceNormBbox>> boxes_by_image_id;
    std::string line;
    while (std::getline(manifest, line))
    {
        if (line.find("CxEvidenceChain_case_adddatasetimage(") != std::string::npos)
        {
            const auto values =
                ParseEvidenceKeyValuePayload(ExtractEvidencePayload(line));
            const auto image_id = values.find("image_id");
            const auto path = values.find("path");
            if (image_id == values.end() || path == values.end())
                continue;

            EvidenceDatasetImage row;
            row.image_id = image_id->second;
            row.path = std::filesystem::path(path->second);
            const auto split = values.find("split");
            const auto label = values.find("label");
            row.split = split == values.end() ? "" : split->second;
            row.label = label == values.end() ? "" : label->second;
            rows.push_back(row);
            continue;
        }

        if (line.find("CxEvidenceChain_case_addbbox_xywh_norm(") != std::string::npos)
        {
            const auto values =
                ParseEvidenceKeyValuePayload(ExtractEvidencePayload(line));
            const auto image_id = values.find("image_id");
            if (image_id == values.end())
                continue;

            EvidenceNormBbox box;
            box.class_id = ParseEvidenceInt(values, "class_id");
            box.cx = ParseEvidenceDouble(values, "cx");
            box.cy = ParseEvidenceDouble(values, "cy");
            box.w = ParseEvidenceDouble(values, "w");
            box.h = ParseEvidenceDouble(values, "h");
            if (box.w <= 0.0 || box.h <= 0.0)
                continue;
            boxes_by_image_id[image_id->second].push_back(box);
        }
    }

    std::vector<EvidenceDatasetImage> annotated;
    for (auto& row : rows)
    {
        const auto boxes = boxes_by_image_id.find(row.image_id);
        if (boxes == boxes_by_image_id.end() || boxes->second.empty())
            continue;
        if (!std::filesystem::exists(row.path, ec))
            continue;
        row.boxes = boxes->second;
        annotated.push_back(row);
    }

    std::sort(
        annotated.begin(),
        annotated.end(),
        [](const EvidenceDatasetImage& lhs, const EvidenceDatasetImage& rhs)
        {
            if (lhs.split != rhs.split)
                return lhs.split < rhs.split;
            if (lhs.image_id != rhs.image_id)
                return lhs.image_id < rhs.image_id;
            return lhs.path.string() < rhs.path.string();
        });

    std::vector<EvidenceDatasetImage> selected;
    std::map<std::string, int> per_split_count;
    for (const auto& row : annotated)
    {
        int& split_count = per_split_count[row.split];
        if (max_images_per_split > 0 && split_count >= max_images_per_split)
            continue;
        selected.push_back(row);
        ++split_count;
    }

    for (const auto& row : selected)
        annotation_count += static_cast<int>(row.boxes.size());
    return selected;
}

std::vector<EvidenceDatasetImage> SelectEvidenceSplitImages(
    const std::vector<EvidenceDatasetImage>& images,
    const std::string& split)
{
    std::vector<EvidenceDatasetImage> selected;
    for (const auto& image : images)
    {
        if (image.split == split)
            selected.push_back(image);
    }
    return selected;
}

EvidenceSplitSummary MakeEvidenceSplitSummary(
    const std::vector<EvidenceDatasetImage>& images)
{
    EvidenceSplitSummary summary;
    for (const auto& image : images)
    {
        const int annotations = static_cast<int>(image.boxes.size());
        if (image.split == "train")
        {
            ++summary.train_images;
            summary.train_annotations += annotations;
        }
        else if (image.split == "val")
        {
            ++summary.val_images;
            summary.val_annotations += annotations;
        }
        else if (image.split == "test")
        {
            ++summary.test_images;
            summary.test_annotations += annotations;
        }
    }
    return summary;
}

cv::Mat MakeMaskFromEvidenceBboxes(
    const EvidenceDatasetImage& image,
    const int input_size)
{
    cv::Mat mask(input_size, input_size, CV_8U, cv::Scalar(0));
    for (const auto& box : image.boxes)
    {
        const int x0 = std::max(
            0,
            std::min(
                input_size - 1,
                static_cast<int>(std::round((box.cx - box.w * 0.5) * input_size))));
        const int y0 = std::max(
            0,
            std::min(
                input_size - 1,
                static_cast<int>(std::round((box.cy - box.h * 0.5) * input_size))));
        const int x1 = std::max(
            0,
            std::min(
                input_size - 1,
                static_cast<int>(std::round((box.cx + box.w * 0.5) * input_size))));
        const int y1 = std::max(
            0,
            std::min(
                input_size - 1,
                static_cast<int>(std::round((box.cy + box.h * 0.5) * input_size))));
        if (x1 < x0 || y1 < y0)
            continue;
        cv::rectangle(
            mask,
            cv::Rect(x0, y0, x1 - x0 + 1, y1 - y0 + 1),
            cv::Scalar(1),
            cv::FILLED);
    }
    return mask;
}

void DrawEvidenceBboxes(
    cv::Mat& image,
    const EvidenceDatasetImage& evidence)
{
    for (const auto& box : evidence.boxes)
    {
        const int width = image.cols;
        const int height = image.rows;
        const int x0 = std::max(
            0,
            std::min(
                width - 1,
                static_cast<int>(std::round((box.cx - box.w * 0.5) * width))));
        const int y0 = std::max(
            0,
            std::min(
                height - 1,
                static_cast<int>(std::round((box.cy - box.h * 0.5) * height))));
        const int x1 = std::max(
            0,
            std::min(
                width - 1,
                static_cast<int>(std::round((box.cx + box.w * 0.5) * width))));
        const int y1 = std::max(
            0,
            std::min(
                height - 1,
                static_cast<int>(std::round((box.cy + box.h * 0.5) * height))));
        if (x1 < x0 || y1 < y0)
            continue;
        cv::rectangle(
            image,
            cv::Rect(x0, y0, x1 - x0 + 1, y1 - y0 + 1),
            cv::Scalar(0, 0, 255),
            2);
    }
}

void WriteEvidenceSplitArtifacts(
    const std::filesystem::path& output_dir,
    const std::vector<EvidenceDatasetImage>& all_images,
    const EvidenceSplitSummary& summary,
    const int total_annotations,
    const int input_size,
    LifecycleEvidenceArtifacts& artifacts)
{
    if (output_dir.empty() || all_images.empty())
        return;

    std::filesystem::create_directories(output_dir);
    const std::filesystem::path summary_path =
        output_dir / "torch_training_dataset_split_summary.json";
    const std::filesystem::path mask_path =
        output_dir / "torch_training_mask_preview.png";
    const std::filesystem::path overlay_path =
        output_dir / "torch_training_bbox_overlay.png";

    {
        std::ofstream output(summary_path);
        output << "{";
        output << "\"schema\":\"cxvision.torch.training_dataset_split.v1\",";
        output << "\"source\":\"evidence_manifest\",";
        output << "\"total_annotations\":" << total_annotations << ",";
        output << "\"train_images\":" << summary.train_images << ",";
        output << "\"train_annotations\":" << summary.train_annotations << ",";
        output << "\"val_images\":" << summary.val_images << ",";
        output << "\"val_annotations\":" << summary.val_annotations << ",";
        output << "\"test_images\":" << summary.test_images << ",";
        output << "\"test_annotations\":" << summary.test_annotations;
        output << "}\n";
        artifacts.split_summary_ref = summary_path.string();
    }

    const EvidenceDatasetImage& preview = all_images.front();
    cv::Mat bgr = cv::imread(preview.path.string(), cv::IMREAD_COLOR);
    if (bgr.empty())
        return;

    cv::Mat resized;
    cv::resize(
        bgr,
        resized,
        cv::Size(input_size, input_size),
        0.0,
        0.0,
        cv::INTER_AREA);

    cv::Mat mask = MakeMaskFromEvidenceBboxes(preview, input_size);
    cv::Mat mask_visual;
    mask.convertTo(mask_visual, CV_8U, 255.0);
    cv::imwrite(mask_path.string(), mask_visual);
    artifacts.mask_preview_ref = mask_path.string();

    cv::Mat overlay = resized.clone();
    DrawEvidenceBboxes(overlay, preview);
    cv::imwrite(overlay_path.string(), overlay);
    artifacts.bbox_overlay_ref = overlay_path.string();
}

std::vector<std::filesystem::path> CollectSegmentationLifecycleImages(
    const TorchTaskRequestCpp& request,
    const int max_images)
{
    std::vector<std::filesystem::path> images;
    if (request.input_image.empty())
        return images;

    std::filesystem::path input_path(request.input_image);
    std::error_code ec;
    if (!std::filesystem::exists(input_path, ec))
        return images;

    images.push_back(input_path);
    const std::filesystem::path parent = input_path.parent_path();
    if (!parent.empty() && std::filesystem::is_directory(parent, ec))
    {
        for (const auto& entry : std::filesystem::directory_iterator(parent, ec))
        {
            if (ec || !entry.is_regular_file())
                continue;
            const std::filesystem::path path = entry.path();
            const std::string name = path.filename().string();
            if (name.find("torch_deeppcb_") == std::string::npos ||
                name.find("_input") == std::string::npos)
                continue;
            if (path == input_path)
                continue;
            images.push_back(path);
        }
    }

    std::sort(images.begin(), images.end());
    images.erase(std::unique(images.begin(), images.end()), images.end());
    if (static_cast<int>(images.size()) > max_images)
        images.resize(static_cast<std::size_t>(max_images));
    return images;
}

bool BuildEvidenceImageBatch(
    const std::vector<EvidenceDatasetImage>& evidence_images,
    SegmentationMainlineRunnerConfig& config,
    torch::Tensor& images,
    torch::Tensor& masks,
    double& mask_foreground_ratio,
    std::string& image_hash)
{
    if (evidence_images.empty())
        return false;

    std::vector<torch::Tensor> image_tensors;
    std::vector<torch::Tensor> mask_tensors;
    uint64_t combined_hash = 1469598103934665603ull;
    double foreground_pixels = 0.0;
    double total_pixels = 0.0;

    for (const auto& evidence : evidence_images)
    {
        cv::Mat bgr = cv::imread(evidence.path.string(), cv::IMREAD_COLOR);
        if (bgr.empty())
            continue;

        cv::Mat resized;
        cv::resize(
            bgr,
            resized,
            cv::Size(config.input_size, config.input_size),
            0.0,
            0.0,
            cv::INTER_AREA);

        cv::Mat rgb;
        cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);

        cv::Mat float_image;
        rgb.convertTo(float_image, CV_32F, 1.0 / 255.0);
        auto image_tensor = torch::from_blob(
            float_image.data,
            {config.input_size, config.input_size, 3},
            torch::kFloat32).clone().permute({2, 0, 1});
        image_tensors.push_back(image_tensor);

        cv::Mat binary = MakeMaskFromEvidenceBboxes(
            evidence,
            config.input_size);
        foreground_pixels += static_cast<double>(cv::countNonZero(binary));
        total_pixels += static_cast<double>(binary.rows * binary.cols);
        auto mask_tensor = torch::from_blob(
            binary.data,
            {config.input_size, config.input_size},
            torch::kUInt8).clone().to(torch::kLong);
        mask_tensors.push_back(mask_tensor);

        const std::string file_hash = HashFileFnv1a64(evidence.path);
        for (const char ch : file_hash)
        {
            combined_hash ^= static_cast<unsigned char>(ch);
            combined_hash *= 1099511628211ull;
        }
    }

    if (image_tensors.empty())
        return false;

    images = torch::stack(image_tensors, 0);
    masks = torch::stack(mask_tensors, 0);
    mask_foreground_ratio =
        total_pixels > 0.0 ? foreground_pixels / total_pixels : 0.0;
    std::ostringstream hash_os;
    hash_os << std::hex << std::setw(16) << std::setfill('0') << combined_hash;
    image_hash = hash_os.str();
    return true;
}

bool BuildSegmentationLifecycleRealImageBatch(
    const TorchTaskRequestCpp& request,
    SegmentationMainlineRunnerConfig& config,
    torch::Tensor& images,
    torch::Tensor& masks,
    torch::Tensor& eval_images,
    torch::Tensor& eval_masks,
    int& image_count,
    int& eval_image_count,
    double& mask_foreground_ratio,
    std::string& image_hash,
    std::string& dataset_source,
    std::string& mask_source,
    LifecycleEvidenceArtifacts& artifacts)
{
    int annotation_count = 0;
    std::vector<EvidenceDatasetImage> manifest_images =
        CollectEvidenceManifestAnnotatedImages(request, 4, annotation_count);
    artifacts.total_annotations = annotation_count;
    std::vector<std::filesystem::path> image_paths;
    if (!manifest_images.empty())
    {
        artifacts.split_summary = MakeEvidenceSplitSummary(manifest_images);
        const std::vector<EvidenceDatasetImage> train_images =
            SelectEvidenceSplitImages(manifest_images, "train");
        std::vector<EvidenceDatasetImage> validation_images =
            SelectEvidenceSplitImages(manifest_images, "val");
        if (validation_images.empty())
            validation_images = SelectEvidenceSplitImages(manifest_images, "test");
        if (train_images.empty())
            return false;

        if (!BuildEvidenceImageBatch(
                train_images,
                config,
                images,
                masks,
                mask_foreground_ratio,
                image_hash))
        {
            return false;
        }

        double eval_mask_foreground_ratio = 0.0;
        std::string eval_image_hash;
        if (!BuildEvidenceImageBatch(
                validation_images.empty() ? train_images : validation_images,
                config,
                eval_images,
                eval_masks,
                eval_mask_foreground_ratio,
                eval_image_hash))
        {
            eval_images = images.clone();
            eval_masks = masks.clone();
        }
        if (eval_images.defined() && eval_images.size(0) == 1)
        {
            eval_images = torch::cat({eval_images, eval_images.clone()}, 0);
            eval_masks = torch::cat({eval_masks, eval_masks.clone()}, 0);
        }

        image_count = static_cast<int>(images.size(0));
        eval_image_count = static_cast<int>(eval_images.size(0));
        config.batch_size = image_count;
        dataset_source = "evidence_manifest";
        mask_source = "bbox_xywh_norm";
        WriteEvidenceSplitArtifacts(
            std::filesystem::path(request.output_dir),
            manifest_images,
            artifacts.split_summary,
            annotation_count,
            config.input_size,
            artifacts);
        return true;
    }

    image_paths = CollectSegmentationLifecycleImages(request, 4);
    if (image_paths.empty())
    {
        return false;
    }

    std::vector<torch::Tensor> image_tensors;
    std::vector<torch::Tensor> mask_tensors;
    uint64_t combined_hash = 1469598103934665603ull;
    double foreground_pixels = 0.0;
    double total_pixels = 0.0;

    for (std::size_t i = 0; i < image_paths.size(); ++i)
    {
        const std::filesystem::path& path = image_paths[i];
        cv::Mat bgr = cv::imread(path.string(), cv::IMREAD_COLOR);
        if (bgr.empty())
            continue;

        cv::Mat resized;
        cv::resize(
            bgr,
            resized,
            cv::Size(config.input_size, config.input_size),
            0.0,
            0.0,
            cv::INTER_AREA);

        cv::Mat rgb;
        cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);

        cv::Mat float_image;
        rgb.convertTo(float_image, CV_32F, 1.0 / 255.0);
        auto image_tensor = torch::from_blob(
            float_image.data,
            {config.input_size, config.input_size, 3},
            torch::kFloat32).clone().permute({2, 0, 1});
        image_tensors.push_back(image_tensor);

        cv::Mat binary;
        cv::Mat gray;
        cv::cvtColor(resized, gray, cv::COLOR_BGR2GRAY);
        cv::threshold(gray, binary, 220, 1, cv::THRESH_BINARY_INV);
        foreground_pixels += static_cast<double>(cv::countNonZero(binary));
        total_pixels += static_cast<double>(binary.rows * binary.cols);
        auto mask_tensor = torch::from_blob(
            binary.data,
            {config.input_size, config.input_size},
            torch::kUInt8).clone().to(torch::kLong);
        mask_tensors.push_back(mask_tensor);

        const std::string file_hash = HashFileFnv1a64(path);
        for (const char ch : file_hash)
        {
            combined_hash ^= static_cast<unsigned char>(ch);
            combined_hash *= 1099511628211ull;
        }
    }

    if (image_tensors.empty())
        return false;

    images = torch::stack(image_tensors, 0);
    masks = torch::stack(mask_tensors, 0);
    eval_images = images.clone();
    eval_masks = masks.clone();
    image_count = static_cast<int>(image_tensors.size());
    eval_image_count = image_count;
    config.batch_size = image_count;
    mask_foreground_ratio =
        total_pixels > 0.0 ? foreground_pixels / total_pixels : 0.0;
    std::ostringstream hash_os;
    hash_os << std::hex << std::setw(16) << std::setfill('0') << combined_hash;
    image_hash = hash_os.str();
    dataset_source = "input_image_directory";
    mask_source = "threshold_binary_inv_220";
    return true;
}

TorchTaskResultCpp RunSegmentationTrainingLifecycleTask(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request)
{
    TorchTaskResultCpp result;
    result.requested_device = config.device.empty() ? "cpu" : config.device;
    constexpr uint64_t deterministic_seed = 1023;
    int lifecycle_image_count = 0;
    int lifecycle_eval_image_count = 0;
    double lifecycle_mask_foreground_ratio = 0.0;
    std::string lifecycle_image_hash;
    std::string lifecycle_dataset_source = "synthetic_random";
    std::string lifecycle_mask_source = "synthetic_random";
    LifecycleEvidenceArtifacts lifecycle_artifacts;

    const auto start = std::chrono::steady_clock::now();

    try
    {
        torch::manual_seed(deterministic_seed);

        auto runner_config =
            make_segmentation_mainline_runner_config(
                "deeplabv3plus",
                "mobilenet_v3_large",
                3,
                128,
                2);

        runner_config.enable_smoke_train = true;
        runner_config.enable_eval = true;
        runner_config.device_policy =
            (result.requested_device == "cuda" && torch::cuda::is_available())
                ? SegmentationDevicePolicy::ForceCUDA
                : SegmentationDevicePolicy::ForceCPU;

        torch::Tensor train_images;
        torch::Tensor train_masks;
        torch::Tensor eval_images;
        torch::Tensor eval_masks;
        if (!BuildSegmentationLifecycleRealImageBatch(
                request,
                runner_config,
                train_images,
                train_masks,
                eval_images,
                eval_masks,
                lifecycle_image_count,
                lifecycle_eval_image_count,
                lifecycle_mask_foreground_ratio,
                lifecycle_image_hash,
                lifecycle_dataset_source,
                lifecycle_mask_source,
                lifecycle_artifacts))
        {
            train_images = MakeSegmentationLifecycleImages(runner_config);
            train_masks = MakeSegmentationLifecycleMasks(runner_config);
            eval_images = train_images.clone();
            eval_masks = train_masks.clone();
            lifecycle_image_count = static_cast<int>(train_images.size(0));
            lifecycle_eval_image_count = static_cast<int>(eval_images.size(0));
            lifecycle_mask_foreground_ratio = train_masks.gt(0).to(torch::kFloat32).mean().item<double>();
            lifecycle_image_hash = "synthetic_seed_1023";
            lifecycle_mask_source = "synthetic_random";
        }

        const auto smoke =
            run_segmentation_smoke_train_step(
                train_images,
                train_masks,
                runner_config);
        smoke.validate();

        const auto session =
            run_segmentation_trainer_session(
                train_images,
                train_masks,
                eval_images,
                eval_masks,
                runner_config);
        session.validate();

        const auto analysis =
            build_segmentation_trainer_analysis(session);
        analysis.validate();

        const auto unified =
            build_segmentation_unified_mainline_bundle(
                session.session,
                analysis);
        unified.validate();

        const auto summary =
            build_segmentation_unified_mainline_summary(unified);
        summary.validate();

        const auto end = std::chrono::steady_clock::now();
        result.train_runtime_ms =
            static_cast<double>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    end - start).count());
        result.algorithm_runtime_ms = result.train_runtime_ms;
        result.actual_device =
            runner_config.device_policy == SegmentationDevicePolicy::ForceCUDA
                ? "cuda"
                : "cpu";
        result.ok = true;
        result.error_code = 0;
        result.status = "success";
        result.trainer_lifecycle_summary = analysis.lifecycle_summary.summary;
        result.unified_mainline_summary = summary.summary;

        std::filesystem::path output_dir(request.output_dir);
        if (!output_dir.empty())
            std::filesystem::create_directories(output_dir);

        const std::filesystem::path result_path =
            output_dir.empty()
                ? std::filesystem::path()
                : (output_dir / "torch_training_lifecycle_result.json");
        const std::filesystem::path evidence_path =
            output_dir.empty()
                ? std::filesystem::path()
                : (output_dir / "torch_training_lifecycle_evidence.json");

        std::ostringstream result_json;
        result_json << "{";
        result_json << "\"schema\":\"cxvision.torch.training_lifecycle.v1\",";
        result_json << "\"task\":" << QuoteRuntimeTaskJsonString(request.task) << ",";
        result_json << "\"status\":\"success\",";
        result_json << "\"requested_device\":" << QuoteRuntimeTaskJsonString(result.requested_device) << ",";
        result_json << "\"actual_device\":" << QuoteRuntimeTaskJsonString(result.actual_device) << ",";
        result_json << "\"deterministic_seed\":" << deterministic_seed << ",";
        result_json << "\"dataset_source\":" << QuoteRuntimeTaskJsonString(lifecycle_dataset_source) << ",";
        result_json << "\"mask_source\":" << QuoteRuntimeTaskJsonString(lifecycle_mask_source) << ",";
        result_json << "\"annotation_count\":" << lifecycle_artifacts.total_annotations << ",";
        result_json << "\"image_count\":" << lifecycle_image_count << ",";
        result_json << "\"eval_image_count\":" << lifecycle_eval_image_count << ",";
        result_json << "\"train_split_images\":" << lifecycle_artifacts.split_summary.train_images << ",";
        result_json << "\"val_split_images\":" << lifecycle_artifacts.split_summary.val_images << ",";
        result_json << "\"test_split_images\":" << lifecycle_artifacts.split_summary.test_images << ",";
        result_json << "\"train_split_annotations\":" << lifecycle_artifacts.split_summary.train_annotations << ",";
        result_json << "\"val_split_annotations\":" << lifecycle_artifacts.split_summary.val_annotations << ",";
        result_json << "\"test_split_annotations\":" << lifecycle_artifacts.split_summary.test_annotations << ",";
        result_json << "\"image_hash\":" << QuoteRuntimeTaskJsonString(lifecycle_image_hash) << ",";
        result_json << "\"mask_foreground_ratio\":" << lifecycle_mask_foreground_ratio << ",";
        result_json << "\"split_summary_ref\":" << QuoteRuntimeTaskJsonString(lifecycle_artifacts.split_summary_ref) << ",";
        result_json << "\"mask_preview_ref\":" << QuoteRuntimeTaskJsonString(lifecycle_artifacts.mask_preview_ref) << ",";
        result_json << "\"bbox_overlay_ref\":" << QuoteRuntimeTaskJsonString(lifecycle_artifacts.bbox_overlay_ref) << ",";
        result_json << "\"train_runtime_ms\":" << result.train_runtime_ms << ",";
        result_json << "\"effective_epochs\":1,";
        result_json << "\"effective_batch_size\":" << runner_config.batch_size << ",";
        result_json << "\"input_size\":" << runner_config.input_size << ",";
        result_json << "\"smoke_loss\":" << smoke.loss << ",";
        result_json << "\"grad_mean\":" << smoke.grad_mean << ",";
        result_json << "\"eval_loss\":" << session.session.eval.loss << ",";
        result_json << "\"foreground_iou\":" << session.session.eval.foreground_iou << ",";
        result_json << "\"avg_confidence\":" << session.session.eval.avg_confidence << ",";
        result_json << "\"trainer_lifecycle_summary\":"
                    << QuoteRuntimeTaskJsonString(result.trainer_lifecycle_summary) << ",";
        result_json << "\"unified_mainline_summary\":"
                    << QuoteRuntimeTaskJsonString(result.unified_mainline_summary);
        result_json << "}";
        result.result_json = result_json.str();

        if (!result_path.empty())
        {
            std::ofstream output(result_path);
            output << result.result_json << "\n";
            result.result_ref = result_path.string();
        }
        else
        {
            result.result_ref = "torch_train_segmentation_lifecycle_smoke.result";
        }

        if (!evidence_path.empty())
        {
            std::ofstream evidence(evidence_path);
            evidence << "{";
            evidence << "\"schema\":\"cxvision.torch.training_lifecycle_evidence.v1\",";
            evidence << "\"task\":" << QuoteRuntimeTaskJsonString(request.task) << ",";
            evidence << "\"training_stage\":\"tiny_smoke\",";
            evidence << "\"deterministic_seed\":" << deterministic_seed << ",";
            evidence << "\"dataset_source\":" << QuoteRuntimeTaskJsonString(lifecycle_dataset_source) << ",";
            evidence << "\"mask_source\":" << QuoteRuntimeTaskJsonString(lifecycle_mask_source) << ",";
            evidence << "\"annotation_count\":" << lifecycle_artifacts.total_annotations << ",";
            evidence << "\"image_count\":" << lifecycle_image_count << ",";
            evidence << "\"eval_image_count\":" << lifecycle_eval_image_count << ",";
            evidence << "\"train_split_images\":" << lifecycle_artifacts.split_summary.train_images << ",";
            evidence << "\"val_split_images\":" << lifecycle_artifacts.split_summary.val_images << ",";
            evidence << "\"test_split_images\":" << lifecycle_artifacts.split_summary.test_images << ",";
            evidence << "\"train_split_annotations\":" << lifecycle_artifacts.split_summary.train_annotations << ",";
            evidence << "\"val_split_annotations\":" << lifecycle_artifacts.split_summary.val_annotations << ",";
            evidence << "\"test_split_annotations\":" << lifecycle_artifacts.split_summary.test_annotations << ",";
            evidence << "\"image_hash\":" << QuoteRuntimeTaskJsonString(lifecycle_image_hash) << ",";
            evidence << "\"mask_foreground_ratio\":" << lifecycle_mask_foreground_ratio << ",";
            evidence << "\"split_summary_ref\":" << QuoteRuntimeTaskJsonString(lifecycle_artifacts.split_summary_ref) << ",";
            evidence << "\"mask_preview_ref\":" << QuoteRuntimeTaskJsonString(lifecycle_artifacts.mask_preview_ref) << ",";
            evidence << "\"bbox_overlay_ref\":" << QuoteRuntimeTaskJsonString(lifecycle_artifacts.bbox_overlay_ref) << ",";
            evidence << "\"epochs\":1,";
            evidence << "\"batch_size\":" << runner_config.batch_size << ",";
            evidence << "\"input_size\":" << runner_config.input_size << ",";
            evidence << "\"finite_loss\":true,";
            evidence << "\"grad_mean\":" << smoke.grad_mean << ",";
            evidence << "\"semantic_quality\":\"not_evaluated\"";
            evidence << "}\n";
            result.evidence_ref = evidence_path.string();
        }
        else
        {
            result.evidence_ref = "torch_train_segmentation_lifecycle_smoke.evidence";
        }

        if (!request.input_image.empty())
        {
            result.input_image_ref = request.input_image;
            result.primary_visual_ref = request.input_image;
        }
    }
    catch (const std::exception& e)
    {
        result.ok = false;
        result.error_code = -1;
        result.status = "exception";
        result.error_message = e.what();
        result.result_json =
            "{"
            "\"schema\":\"cxvision.torch.training_lifecycle.v1\","
            "\"status\":\"exception\","
            "\"reason\":" + QuoteRuntimeTaskJsonString(result.error_message) +
            "}";
    }
    catch (...)
    {
        result.ok = false;
        result.error_code = -2;
        result.status = "unknown_exception";
        result.error_message =
            "unknown exception in segmentation training lifecycle smoke";
    }

    return result;
}

} // namespace

TorchTaskResultCpp RunLegacyTorchTestHostTask(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request)
{
    TorchTaskResultCpp result;

    try {
        const std::string& requested_device = !config.device.empty() ? config.device : "auto";

        TorchTestHost host;
        const auto report = host.run_task_report(request.task, requested_device);

        result.ok = (report.failures == 0);
        result.error_code = report.failures;
        result.requested_device = report.requested_device;
        result.actual_device = report.actual_device;

        if (report.failures == 0) {
            result.status = "success";
        } else {
            result.status = "failed";
            result.error_message = report.summary;
        }

        if (request.task.find("train") != std::string::npos) {
            result.train_runtime_ms = static_cast<double>(report.runtime_ms);
        } else if (request.task.find("infer") != std::string::npos) {
            result.infer_runtime_ms = static_cast<double>(report.runtime_ms);
            result.algorithm_runtime_ms = static_cast<double>(report.runtime_ms);
        } else {
            result.algorithm_runtime_ms = static_cast<double>(report.runtime_ms);
        }

        std::ostringstream json_os;
        json_os << "{";
        json_os << "\"task\":\"" << request.task << "\",";
        json_os << "\"status\":\"" << result.status << "\",";
        json_os << "\"failures\":" << report.failures << ",";
        json_os << "\"summary\":\"" << report.summary << "\",";
        json_os << "\"requested_device\":\"" << report.requested_device << "\",";
        json_os << "\"actual_device\":\"" << report.actual_device << "\",";
        json_os << "\"runtime_ms\":" << report.runtime_ms;
        json_os << "}";
        result.result_json = json_os.str();

        const auto* spec = TorchTestHost::find_task_spec(request.task);
        if (spec != nullptr) {
            result.result_ref = torch_make_handoff_ref(spec->task_id, "result");
            result.evidence_ref = torch_make_handoff_ref(spec->task_id, "evidence");
            result.attach_back_ref = spec->attach_back_result;

            if (torch_task_id_contains(spec->task_id, "yolo")) {
                result.bbox_candidate_list_ref = torch_make_handoff_ref(spec->task_id, "bbox_candidates");
            }
            if (torch_task_id_contains(spec->task_id, "mobilevit")) {
                result.roi_crop_packet_ref = torch_make_handoff_ref(spec->task_id, "roi_crops");
                result.template_alignment_ref = torch_make_handoff_ref(spec->task_id, "template_alignment");
                result.roi_diff_candidate_ref = torch_make_handoff_ref(spec->task_id, "roi_diff");
            }
            if (torch_task_id_contains(spec->task_id, "train")) {
                result.trainer_lifecycle_summary = "trainer_lifecycle_completed";
                result.unified_mainline_summary = "unified_mainline_bundle_available";
            }
        }

        if (!request.input_image.empty()) {
            result.input_image_ref = request.input_image;
            result.primary_visual_ref = request.input_image;
        }

        result.visualization_refs = "";

    } catch (const std::exception& e) {
        result.ok = false;
        result.error_code = -1;
        result.status = "exception";
        result.error_message = e.what();
    } catch (...) {
        result.ok = false;
        result.error_code = -2;
        result.status = "unknown_exception";
        result.error_message = "Unknown exception during legacy torch task execution";
    }

    return result;
}

TorchTaskResultCpp DispatchTorchRuntimeTask(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request)
{
    if (request.task ==
        TorchRuntimeTaskIds::Capabilities)
    {
        return RunTorchCapabilitiesTask(
            config,
            request);
    }

    if (request.task ==
        TorchRuntimeTaskIds::SegmentationContract)
    {
        return ValidateTorchSegmentationContract(
            config,
            request);
    }

    if (request.task ==
        TorchRuntimeTaskIds::DetectionContract)
    {
        return ValidateTorchDetectionContract(
            config,
            request);
    }

    if (request.task ==
        TorchRuntimeTaskIds::
            DeepLabV3PlusSegmentation)
    {
        return ExecuteTorchSegmentationTask(
            config,
            request);
    }

    if (request.task ==
        TorchRuntimeTaskIds::YoloV8Detection)
    {
        return ExecuteTorchDetectionTask(
            config,
            request);
    }

    if (request.task ==
        TorchRuntimeTaskIds::SegmentationTrainingLifecycle)
    {
        return RunSegmentationTrainingLifecycleTask(
            config,
            request);
    }

    if (IsLegacyTestHostTask(request.task))
    {
        return RunLegacyTorchTestHostTask(
            config,
            request);
    }

    return MakeUnsupportedTask(request.task);
}
