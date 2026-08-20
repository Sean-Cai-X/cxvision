#include "torch_runtime_yolov8_seg_executor.h"

#include "torch_runtime_manifest.h"
#include "torch_segmentation_evidence.h"
#include "torch_yolov8_seg.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <sstream>
#include <torch/cuda.h>

namespace
{
struct SegLetterbox
{
    double scale = 1.0;
    int pad_x = 0;
    int pad_y = 0;
    int resized_width = 0;
    int resized_height = 0;
};

struct SegCandidate
{
    int class_id = -1;
    float score = 0.0f;
    float x0 = 0.0f;
    float y0 = 0.0f;
    float x1 = 0.0f;
    float y1 = 0.0f;
    torch::Tensor coefficients;
};

std::string QuoteSegJson(const std::string& value)
{
    std::ostringstream output;
    output << '"';
    for (const char ch : value)
    {
        switch (ch)
        {
        case '\\': output << "\\\\"; break;
        case '"': output << "\\\""; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default: output << ch; break;
        }
    }
    output << '"';
    return output.str();
}

std::string Fnv1a64File(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    std::uint64_t hash = 1469598103934665603ull;
    char buffer[4096];
    while (input.good())
    {
        input.read(buffer, sizeof(buffer));
        for (std::streamsize index = 0;
             index < input.gcount();
             ++index)
        {
            hash ^= static_cast<unsigned char>(buffer[index]);
            hash *= 1099511628211ull;
        }
    }
    std::ostringstream output;
    output << "fnv1a64:" << std::hex << std::setw(16)
           << std::setfill('0') << hash;
    return output.str();
}

TorchTaskResultCpp SegFailure(
    const std::string& stage,
    const std::string& reason)
{
    TorchTaskResultCpp result;
    result.ok = false;
    result.status = "failed";
    result.error_code = -1;
    result.error_message = reason;
    result.result_json =
        "{\"schema\":\"cxvision.segmentation_evidence.v2\","
        "\"status\":\"failed\",\"failure_stage\":" +
        QuoteSegJson(stage) + ",\"reason\":" +
        QuoteSegJson(reason) + "}";
    return result;
}

torch::Tensor MakeSegInput(
    const cv::Mat& bgr,
    const TorchModelManifest& manifest,
    SegLetterbox& letterbox)
{
    letterbox.scale = std::min(
        manifest.input_width / static_cast<double>(bgr.cols),
        manifest.input_height / static_cast<double>(bgr.rows));
    letterbox.resized_width = std::max(
        1,
        static_cast<int>(std::round(bgr.cols * letterbox.scale)));
    letterbox.resized_height = std::max(
        1,
        static_cast<int>(std::round(bgr.rows * letterbox.scale)));
    letterbox.pad_x =
        (manifest.input_width - letterbox.resized_width) / 2;
    letterbox.pad_y =
        (manifest.input_height - letterbox.resized_height) / 2;

    cv::Mat resized;
    cv::resize(
        bgr,
        resized,
        cv::Size(
            letterbox.resized_width,
            letterbox.resized_height),
        0.0,
        0.0,
        cv::INTER_LINEAR);
    cv::Mat canvas(
        manifest.input_height,
        manifest.input_width,
        CV_8UC3,
        cv::Scalar(114, 114, 114));
    resized.copyTo(canvas(cv::Rect(
        letterbox.pad_x,
        letterbox.pad_y,
        letterbox.resized_width,
        letterbox.resized_height)));
    cv::Mat rgb;
    cv::cvtColor(canvas, rgb, cv::COLOR_BGR2RGB);
    torch::Tensor tensor = torch::from_blob(
        rgb.data,
        {1, rgb.rows, rgb.cols, 3},
        torch::kUInt8).clone();
    return tensor.permute({0, 3, 1, 2})
        .to(torch::kFloat32)
        .div_(255.0);
}

float BoxIou(const SegCandidate& lhs, const SegCandidate& rhs)
{
    const float x0 = std::max(lhs.x0, rhs.x0);
    const float y0 = std::max(lhs.y0, rhs.y0);
    const float x1 = std::min(lhs.x1, rhs.x1);
    const float y1 = std::min(lhs.y1, rhs.y1);
    const float intersection =
        std::max(0.0f, x1 - x0) *
        std::max(0.0f, y1 - y0);
    const float lhs_area =
        std::max(0.0f, lhs.x1 - lhs.x0) *
        std::max(0.0f, lhs.y1 - lhs.y0);
    const float rhs_area =
        std::max(0.0f, rhs.x1 - rhs.x0) *
        std::max(0.0f, rhs.y1 - rhs.y0);
    return intersection /
        std::max(1.0e-6f, lhs_area + rhs_area - intersection);
}

std::vector<SegCandidate> DecodeCandidates(
    const YoloV8SegRawOutput& raw,
    const YoloV8SegmentHead& head,
    const TorchModelManifest& manifest)
{
    const std::vector<float> strides{8.0f, 16.0f, 32.0f};
    std::vector<SegCandidate> candidates;
    for (std::size_t level = 0; level < 3; ++level)
    {
        const int64_t height = raw.box_logits[level].size(2);
        const int64_t width = raw.box_logits[level].size(3);
        const int64_t anchors = height * width;
        const torch::Tensor boxes = raw.box_logits[level]
            .view({1, 64, anchors});
        const torch::Tensor distances =
            head->dfl_module()->expectation(boxes)
                .squeeze(0).to(torch::kCPU);
        const torch::Tensor classes = raw.class_logits[level]
            .view({1, manifest.num_classes, anchors})
            .sigmoid()
            .squeeze(0).to(torch::kCPU);
        const auto class_max = classes.max(0);
        const torch::Tensor scores =
            std::get<0>(class_max).contiguous();
        const torch::Tensor class_ids =
            std::get<1>(class_max).contiguous();
        const torch::Tensor coefficients =
            raw.mask_coefficients[level]
                .view({1, manifest.mask_channels, anchors})
                .squeeze(0)
                .transpose(0, 1)
                .to(torch::kCPU)
                .contiguous();
        for (int64_t anchor = 0; anchor < anchors; ++anchor)
        {
            const float score = scores[anchor].item<float>();
            if (score < manifest.confidence_threshold)
                continue;
            const int64_t row = anchor / width;
            const int64_t column = anchor % width;
            const float center_x =
                (static_cast<float>(column) + 0.5f) * strides[level];
            const float center_y =
                (static_cast<float>(row) + 0.5f) * strides[level];
            SegCandidate candidate;
            candidate.class_id =
                class_ids[anchor].item<int64_t>();
            candidate.score = score;
            candidate.x0 = center_x -
                distances[0][anchor].item<float>() * strides[level];
            candidate.y0 = center_y -
                distances[1][anchor].item<float>() * strides[level];
            candidate.x1 = center_x +
                distances[2][anchor].item<float>() * strides[level];
            candidate.y1 = center_y +
                distances[3][anchor].item<float>() * strides[level];
            candidate.coefficients =
                coefficients[anchor].clone();
            candidates.push_back(std::move(candidate));
        }
    }

    std::sort(
        candidates.begin(),
        candidates.end(),
        [](const SegCandidate& lhs, const SegCandidate& rhs)
        {
            return lhs.score > rhs.score;
        });
    std::vector<SegCandidate> selected;
    for (const auto& candidate : candidates)
    {
        bool suppressed = false;
        for (const auto& kept : selected)
        {
            if (candidate.class_id == kept.class_id &&
                BoxIou(candidate, kept) > manifest.iou_threshold)
            {
                suppressed = true;
                break;
            }
        }
        if (!suppressed)
        {
            selected.push_back(candidate);
            if (selected.size() >=
                static_cast<std::size_t>(manifest.max_detections))
            {
                break;
            }
        }
    }
    return selected;
}

cv::Mat DecodeMask(
    const SegCandidate& candidate,
    const torch::Tensor& prototypes,
    const TorchModelManifest& manifest,
    const SegLetterbox& letterbox,
    const cv::Size& original_size,
    double& quality,
    double& stability)
{
    const int64_t proto_height = prototypes.size(2);
    const int64_t proto_width = prototypes.size(3);
    torch::Tensor probability =
        torch::matmul(
            candidate.coefficients,
            prototypes.squeeze(0)
                .view({manifest.mask_channels, -1})
                .to(torch::kCPU))
            .sigmoid()
            .view({proto_height, proto_width});

    const double proto_scale_x =
        proto_width / static_cast<double>(manifest.input_width);
    const double proto_scale_y =
        proto_height / static_cast<double>(manifest.input_height);
    const int crop_x0 = std::clamp(
        static_cast<int>(std::floor(candidate.x0 * proto_scale_x)),
        0,
        static_cast<int>(proto_width));
    const int crop_y0 = std::clamp(
        static_cast<int>(std::floor(candidate.y0 * proto_scale_y)),
        0,
        static_cast<int>(proto_height));
    const int crop_x1 = std::clamp(
        static_cast<int>(std::ceil(candidate.x1 * proto_scale_x)),
        0,
        static_cast<int>(proto_width));
    const int crop_y1 = std::clamp(
        static_cast<int>(std::ceil(candidate.y1 * proto_scale_y)),
        0,
        static_cast<int>(proto_height));
    torch::Tensor crop_gate = torch::zeros_like(probability);
    if (crop_x1 > crop_x0 && crop_y1 > crop_y0)
    {
        crop_gate.index_put_(
            {torch::indexing::Slice(crop_y0, crop_y1),
             torch::indexing::Slice(crop_x0, crop_x1)},
            1.0);
    }
    probability = probability * crop_gate;

    const int image_x0 = std::clamp(
        static_cast<int>(std::floor(
            letterbox.pad_x * proto_scale_x)),
        0,
        static_cast<int>(proto_width - 1));
    const int image_y0 = std::clamp(
        static_cast<int>(std::floor(
            letterbox.pad_y * proto_scale_y)),
        0,
        static_cast<int>(proto_height - 1));
    const int image_x1 = std::clamp(
        static_cast<int>(std::ceil(
            (letterbox.pad_x + letterbox.resized_width) *
            proto_scale_x)),
        image_x0 + 1,
        static_cast<int>(proto_width));
    const int image_y1 = std::clamp(
        static_cast<int>(std::ceil(
            (letterbox.pad_y + letterbox.resized_height) *
            proto_scale_y)),
        image_y0 + 1,
        static_cast<int>(proto_height));
    probability = probability.index({
        torch::indexing::Slice(image_y0, image_y1),
        torch::indexing::Slice(image_x0, image_x1)});
    probability = torch::nn::functional::interpolate(
        probability.unsqueeze(0).unsqueeze(0),
        torch::nn::functional::InterpolateFuncOptions()
            .size(std::vector<int64_t>{
                original_size.height,
                original_size.width})
            .mode(torch::kBilinear)
            .align_corners(false))
        .squeeze()
        .contiguous();

    const torch::Tensor mask =
        probability.ge(manifest.mask_threshold);
    const double foreground =
        std::max(1.0, mask.sum().item<double>());
    quality =
        (probability * mask.to(torch::kFloat32))
            .sum().item<double>() / foreground;
    const torch::Tensor stable_mask =
        probability.ge(std::min(1.0f, manifest.mask_threshold + 0.05f));
    const double intersection =
        (mask.logical_and(stable_mask)).sum().item<double>();
    const double union_area =
        std::max(
            1.0,
            (mask.logical_or(stable_mask)).sum().item<double>());
    stability = intersection / union_area;
    const torch::Tensor bytes =
        mask.to(torch::kUInt8).mul(255).to(torch::kCPU).contiguous();
    cv::Mat output(
        original_size.height,
        original_size.width,
        CV_8UC1,
        bytes.data_ptr<unsigned char>());
    return output.clone();
}

std::vector<cv::Point> LargestContour(const cv::Mat& mask)
{
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(
        mask,
        contours,
        cv::RETR_EXTERNAL,
        cv::CHAIN_APPROX_NONE);
    if (contours.empty())
        return {};
    return *std::max_element(
        contours.begin(),
        contours.end(),
        [](const auto& lhs, const auto& rhs)
        {
            return cv::contourArea(lhs) < cv::contourArea(rhs);
        });
}

void WritePointArray(
    std::ostream& output,
    const std::vector<cv::Point>& points)
{
    output << "[";
    for (std::size_t index = 0; index < points.size(); ++index)
    {
        if (index > 0)
            output << ",";
        output << "{\"x\":" << points[index].x
               << ",\"y\":" << points[index].y << "}";
    }
    output << "]";
}
} // namespace

TorchTaskResultCpp ExecuteTorchYoloV8SegTask(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request)
{
    try
    {
        TorchModelManifest manifest;
        std::string reason;
        if (!LoadTorchModelManifest(
                request.manifest_path,
                config.model_root,
                manifest,
                reason) ||
            !ValidateInstanceSegmentationManifest(manifest, reason))
        {
            return SegFailure("manifest", reason);
        }
        cv::Mat image =
            cv::imread(request.input_image, cv::IMREAD_COLOR);
        if (image.empty())
            return SegFailure("input", "input image is unreadable");

        const std::string device_name =
            (request.device == "cuda" || config.device == "cuda") &&
                    torch::cuda::is_available()
                ? "cuda"
                : "cpu";
        const torch::Device device(device_name);
        SegLetterbox letterbox;
        torch::Tensor input =
            MakeSegInput(image, manifest, letterbox).to(device);

        YoloV8Segment model;
        const YoloV8SegWeightMappingReport mapping =
            model->load_state_dict_strict(
                manifest.weights_path.string());
        model->to(device);
        model->eval();

        const auto started = std::chrono::steady_clock::now();
        torch::NoGradGuard no_grad;
        YoloV8SegRawOutput raw = model->forward(input);
        for (auto& tensor : raw.box_logits) tensor = tensor.to(torch::kCPU);
        for (auto& tensor : raw.class_logits) tensor = tensor.to(torch::kCPU);
        for (auto& tensor : raw.mask_coefficients) tensor = tensor.to(torch::kCPU);
        raw.prototypes = raw.prototypes.to(torch::kCPU);
        const std::vector<SegCandidate> candidates =
            DecodeCandidates(raw, model->head(), manifest);
        const auto finished = std::chrono::steady_clock::now();

        const std::filesystem::path output_dir(request.output_dir);
        std::filesystem::create_directories(output_dir);
        const auto masks_dir = output_dir / "instance_masks";
        std::filesystem::create_directories(masks_dir);
        const auto instances_ref = output_dir / "instances.json";
        const auto labels_ref = output_dir / "mask_labels.png";
        const auto overlay_ref = output_dir / "mask_overlay.png";
        const auto contours_ref = output_dir / "contours.json";
        const auto metrics_ref =
            output_dir / "segmentation_metrics.json";
        const auto evidence_ref =
            output_dir / "torch_runtime_evidence.json";
        const auto trace_ref =
            output_dir / "tensor_shape_trace.json";
        const auto mapping_ref =
            output_dir / "weight_mapping_report.json";
        const auto refined_ref =
            output_dir / "refined_edge_points.json";
        const auto rejected_ref =
            output_dir / "rejected_edge_points.json";
        const auto measurement_ref =
            output_dir / "measurement_evidence.json";
        const auto measurement_overlay_ref =
            output_dir / "measurement_overlay.png";

        cv::Mat labels(
            image.rows, image.cols, CV_16UC1, cv::Scalar(0));
        cv::Mat overlay = image.clone();
        cv::Mat measurement_overlay = image.clone();
        cv::Mat gray;
        cv::Mat source_edges;
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
        cv::Canny(gray, source_edges, 50.0, 150.0);

        std::ofstream instances(instances_ref);
        std::ofstream contours(contours_ref);
        std::ofstream refined(refined_ref);
        std::ofstream rejected(rejected_ref);
        std::ofstream measurements(measurement_ref);
        instances << "{\"schema\":\"cxvision.segmentation_evidence.v2\","
                  << "\"provider\":\"yolov8_seg\","
                  << "\"model_id\":" << QuoteSegJson(manifest.model_id) << ","
                  << "\"weights_hash\":" << QuoteSegJson(manifest.weights_hash) << ","
                  << "\"input_image_ref\":" << QuoteSegJson(request.input_image) << ","
                  << "\"input_image_hash\":" << QuoteSegJson(Fnv1a64File(request.input_image)) << ","
                  << "\"transform\":{\"original_width\":" << image.cols
                  << ",\"original_height\":" << image.rows
                  << ",\"roi_x\":0,\"roi_y\":0,\"roi_width\":" << image.cols
                  << ",\"roi_height\":" << image.rows
                  << ",\"letterbox_scale\":" << letterbox.scale
                  << ",\"pad_x\":" << letterbox.pad_x
                  << ",\"pad_y\":" << letterbox.pad_y
                  << ",\"network_width\":" << manifest.input_width
                  << ",\"network_height\":" << manifest.input_height
                  << ",\"prototype_width\":" << raw.prototypes.size(3)
                  << ",\"prototype_height\":" << raw.prototypes.size(2)
                  << "},\"instances\":[";
        contours << "{\"instances\":[";
        refined << "{\"instances\":[";
        rejected << "{\"instances\":[";
        measurements << "{\"schema\":\"cxvision.measurement_evidence.v1\","
                     << "\"provider\":\"original_image_edge_projector\","
                     << "\"instances\":[";

        int accepted_instances = 0;
        for (std::size_t index = 0; index < candidates.size(); ++index)
        {
            double quality = 0.0;
            double stability = 0.0;
            cv::Mat mask = DecodeMask(
                candidates[index],
                raw.prototypes,
                manifest,
                letterbox,
                image.size(),
                quality,
                stability);
            std::vector<cv::Point> contour = LargestContour(mask);
            if (contour.size() < 3)
                continue;

            const std::string stable_id =
                "yolov8_seg_instance_" +
                std::to_string(accepted_instances);
            const auto mask_ref =
                masks_dir / (stable_id + ".png");
            cv::imwrite(mask_ref.string(), mask);
            labels.setTo(
                cv::Scalar(accepted_instances + 1),
                mask);

            const cv::Rect bbox = cv::boundingRect(contour);
            const cv::Moments moments = cv::moments(contour);
            const double centroid_x =
                moments.m00 != 0.0 ? moments.m10 / moments.m00 : 0.0;
            const double centroid_y =
                moments.m00 != 0.0 ? moments.m01 / moments.m00 : 0.0;
            const cv::RotatedRect oriented =
                cv::minAreaRect(contour);

            cv::Mat contour_image =
                cv::Mat::zeros(mask.size(), CV_8UC1);
            std::vector<std::vector<cv::Point>> contour_list{contour};
            cv::drawContours(
                contour_image,
                contour_list,
                0,
                cv::Scalar(255),
                1);
            cv::Mat band;
            cv::dilate(
                contour_image,
                band,
                cv::getStructuringElement(
                    cv::MORPH_ELLIPSE, cv::Size(5, 5)));
            cv::Mat refined_image;
            cv::bitwise_and(source_edges, band, refined_image);
            std::vector<cv::Point> refined_points;
            cv::findNonZero(refined_image, refined_points);
            std::vector<cv::Point> rejected_points;
            for (const auto& point : contour)
            {
                if (source_edges.at<unsigned char>(point) == 0)
                    rejected_points.push_back(point);
            }

            const cv::Scalar color(
                40 + (accepted_instances * 71) % 180,
                220 - (accepted_instances * 43) % 160,
                80 + (accepted_instances * 97) % 160);
            cv::Mat color_layer = overlay.clone();
            color_layer.setTo(color, mask);
            cv::addWeighted(
                color_layer, 0.35, overlay, 0.65, 0.0, overlay);
            cv::rectangle(overlay, bbox, color, 2);
            cv::drawContours(
                measurement_overlay,
                contour_list,
                0,
                cv::Scalar(0, 165, 255),
                1);
            for (const auto& point : refined_points)
                measurement_overlay.at<cv::Vec3b>(point) =
                    cv::Vec3b(0, 255, 0);
            cv::Point2f vertices[4];
            oriented.points(vertices);
            for (int vertex = 0; vertex < 4; ++vertex)
            {
                cv::line(
                    measurement_overlay,
                    vertices[vertex],
                    vertices[(vertex + 1) % 4],
                    cv::Scalar(255, 0, 255),
                    2);
            }

            if (accepted_instances > 0)
            {
                instances << ",";
                contours << ",";
                refined << ",";
                rejected << ",";
                measurements << ",";
            }
            const std::string class_name =
                candidates[index].class_id >= 0 &&
                candidates[index].class_id <
                    static_cast<int>(manifest.class_names.size())
                    ? manifest.class_names[candidates[index].class_id]
                    : "unknown";
            instances
                << "{\"stable_id\":" << QuoteSegJson(stable_id)
                << ",\"class_id\":" << candidates[index].class_id
                << ",\"class_name\":" << QuoteSegJson(class_name)
                << ",\"class_confidence\":" << candidates[index].score
                << ",\"mask_quality\":" << quality
                << ",\"stability_score\":" << stability
                << ",\"bbox\":{\"x0\":" << bbox.x
                << ",\"y0\":" << bbox.y
                << ",\"x1\":" << bbox.x + bbox.width
                << ",\"y1\":" << bbox.y + bbox.height
                << "},\"binary_mask_ref\":" << QuoteSegJson(mask_ref.string())
                << ",\"contour_ref\":" << QuoteSegJson(contours_ref.string())
                << ",\"pixel_area\":" << cv::contourArea(contour)
                << ",\"centroid\":{\"x\":" << centroid_x
                << ",\"y\":" << centroid_y << "}}";
            contours << "{\"stable_id\":" << QuoteSegJson(stable_id)
                     << ",\"outer_contours\":[";
            WritePointArray(contours, contour);
            contours << "],\"holes\":[]}";
            refined << "{\"stable_id\":" << QuoteSegJson(stable_id)
                    << ",\"points\":";
            WritePointArray(refined, refined_points);
            refined << "}";
            rejected << "{\"stable_id\":" << QuoteSegJson(stable_id)
                     << ",\"points\":";
            WritePointArray(rejected, rejected_points);
            rejected << "}";
            measurements
                << "{\"instance_id\":" << QuoteSegJson(stable_id)
                << ",\"raw_mask_contour_ref\":" << QuoteSegJson(contours_ref.string())
                << ",\"refined_edge_points_ref\":" << QuoteSegJson(refined_ref.string())
                << ",\"rejected_edge_points_ref\":" << QuoteSegJson(rejected_ref.string())
                << ",\"fitted_primitive\":\"oriented_rectangle\""
                << ",\"major_axis_pixels\":"
                << std::max(oriented.size.width, oriented.size.height)
                << ",\"minor_axis_pixels\":"
                << std::min(oriented.size.width, oriented.size.height)
                << ",\"pixel_area\":" << cv::contourArea(contour)
                << ",\"calibration\":1.0"
                << ",\"physical_unit\":\"pixel\""
                << ",\"uncertainty\":"
                << (refined_points.empty()
                    ? 1.0
                    : rejected_points.size() /
                      static_cast<double>(
                          refined_points.size() +
                          rejected_points.size()))
                << "}";
            ++accepted_instances;
        }
        instances << "],\"overlay_ref\":" << QuoteSegJson(overlay_ref.string())
                  << ",\"metrics_ref\":" << QuoteSegJson(metrics_ref.string())
                  << "}\n";
        contours << "]}\n";
        refined << "]}\n";
        rejected << "]}\n";
        measurements << "],\"overlay_ref\":"
                     << QuoteSegJson(measurement_overlay_ref.string())
                     << "}\n";

        cv::imwrite(labels_ref.string(), labels);
        cv::imwrite(overlay_ref.string(), overlay);
        cv::imwrite(
            measurement_overlay_ref.string(),
            measurement_overlay);

        std::ofstream(mapping_ref)
            << "{\"schema\":\"cxvision.torch.weight_mapping.v1\","
            << "\"source_count\":" << mapping.source_count
            << ",\"target_count\":" << mapping.target_count
            << ",\"loaded_count\":" << mapping.loaded_count
            << ",\"missing_keys\":[],\"unknown_keys\":[],"
            << "\"shape_mismatches\":[],\"complete\":true}\n";
        std::ofstream(trace_ref)
            << "{\"schema\":\"cxvision.torch.tensor_shape_trace.v1\","
            << "\"input\":[1,3," << manifest.input_height << ","
            << manifest.input_width << "],"
            << "\"box_scales\":[[1,64,"
            << raw.box_logits[0].size(2) << "," << raw.box_logits[0].size(3)
            << "],[1,64," << raw.box_logits[1].size(2) << ","
            << raw.box_logits[1].size(3) << "],[1,64,"
            << raw.box_logits[2].size(2) << ","
            << raw.box_logits[2].size(3) << "]],"
            << "\"class_channels\":" << manifest.num_classes << ","
            << "\"mask_coefficient_channels\":" << manifest.mask_channels << ","
            << "\"prototypes\":[1," << raw.prototypes.size(1) << ","
            << raw.prototypes.size(2) << "," << raw.prototypes.size(3)
            << "]}\n";
        const double elapsed_ms =
            std::chrono::duration<double, std::milli>(
                finished - started).count();
        std::ofstream(metrics_ref)
            << "{\"schema\":\"cxvision.segmentation_metrics.v1\","
            << "\"candidate_count\":" << candidates.size()
            << ",\"instance_count\":" << accepted_instances
            << ",\"elapsed_ms\":" << elapsed_ms
            << ",\"semantic_quality\":\"pending_human_review\"}\n";
        std::ofstream(evidence_ref)
            << "{\"schema\":\"cxvision.torch.runtime_evidence.v1\","
            << "\"provider\":\"yolov8_seg\","
            << "\"segmentation_evidence_ref\":" << QuoteSegJson(instances_ref.string())
            << ",\"measurement_evidence_ref\":" << QuoteSegJson(measurement_ref.string())
            << ",\"tensor_shape_trace_ref\":" << QuoteSegJson(trace_ref.string())
            << ",\"weight_mapping_report_ref\":" << QuoteSegJson(mapping_ref.string())
            << ",\"overlay_ref\":" << QuoteSegJson(overlay_ref.string())
            << ",\"human_review_required\":true}\n";

        TorchTaskResultCpp result;
        result.ok = true;
        result.status = "success";
        result.requested_device = request.device;
        result.actual_device = device_name;
        result.infer_runtime_ms = elapsed_ms;
        result.algorithm_runtime_ms = elapsed_ms;
        result.result_ref = instances_ref.string();
        result.evidence_ref = evidence_ref.string();
        result.input_image_ref = request.input_image;
        result.primary_visual_ref = overlay_ref.string();
        result.visualization_refs =
            labels_ref.string() + ";" + overlay_ref.string() + ";" +
            measurement_overlay_ref.string();
        result.result_json =
            "{\"schema\":\"cxvision.segmentation_evidence.v2\","
            "\"status\":\"success\",\"provider\":\"yolov8_seg\","
            "\"instance_count\":" + std::to_string(accepted_instances) +
            ",\"result_ref\":" + QuoteSegJson(instances_ref.string()) +
            ",\"evidence_ref\":" + QuoteSegJson(evidence_ref.string()) +
            ",\"overlay_ref\":" + QuoteSegJson(overlay_ref.string()) +
            ",\"measurement_evidence_ref\":" +
            QuoteSegJson(measurement_ref.string()) +
            ",\"semantic_quality\":\"pending_human_review\"}";
        return result;
    }
    catch (const std::exception& error)
    {
        return SegFailure("exception", error.what());
    }
}
