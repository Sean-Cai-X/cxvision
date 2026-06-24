#ifndef TORCH_DETECT_H
#define TORCH_DETECT_H

// NOTE: formatting normalized during the libtorch_module cleanup pass.

#include <torch/torch.h>
#include <vector>
#include <map>
#include <cmath>
#include <stdexcept>
#include <algorithm>

// Comment tags used in this file:
// KEY: important execution path.
// MODIFIED: behavior adjusted during the current cleanup pass.
// CHECK: confirm during integration/debug.
// RISK: known limitation or possible issue.
// EVOLVE: recommended future improvement.
// VERIFY: needs explicit runtime or numerical validation.

#define likely(x) (x)
#define unlikely(x) (x)

struct BBox {
    float x1, y1, x2, y2;
    float score;
    int64_t cls;

    inline torch::Tensor tensor() const {
        return torch::tensor({ x1, y1, x2, y2, score, static_cast<float>(cls) }, torch::kFloat);
    }
};

struct YoloPostProcessConfig {
    float conf_threshold = 0.25f;
    float iou_threshold = 0.45f;
    int64_t num_classes = 80;
    bool use_dfl = false;
    int64_t reg_max = 16;

    void validate() const {
        TORCH_CHECK(conf_threshold >= 0.0f && conf_threshold <= 1.0f, "conf_threshold must be in [0,1]");
        TORCH_CHECK(iou_threshold >= 0.0f && iou_threshold <= 1.0f, "iou_threshold must be in [0,1]");
        TORCH_CHECK(num_classes > 0, "num_classes must be positive");
        TORCH_CHECK(reg_max > 0, "reg_max must be positive");
    }

    int64_t box_channels() const {
        return use_dfl ? reg_max * 4 : 4;
    }
};

struct YoloEvalSummary {
    struct ClassStats {
        int64_t predicted_boxes = 0;
        int64_t target_boxes = 0;
        int64_t true_positives = 0;
        int64_t false_positives = 0;
        int64_t false_negatives = 0;
        float precision = 0.0f;
        float recall = 0.0f;
        float f1 = 0.0f;

        void validate() const {
            TORCH_CHECK(predicted_boxes >= 0, "class predicted_boxes must be non-negative");
            TORCH_CHECK(target_boxes >= 0, "class target_boxes must be non-negative");
            TORCH_CHECK(true_positives >= 0, "class true_positives must be non-negative");
            TORCH_CHECK(false_positives >= 0, "class false_positives must be non-negative");
            TORCH_CHECK(false_negatives >= 0, "class false_negatives must be non-negative");
            TORCH_CHECK(precision >= 0.0f && precision <= 1.0f, "class precision must be in [0,1]");
            TORCH_CHECK(recall >= 0.0f && recall <= 1.0f, "class recall must be in [0,1]");
            TORCH_CHECK(f1 >= 0.0f && f1 <= 1.0f, "class f1 must be in [0,1]");
        }
    };

    int64_t images = 0;
    int64_t predicted_boxes = 0;
    int64_t target_boxes = 0;
    int64_t true_positives = 0;
    int64_t false_positives = 0;
    int64_t false_negatives = 0;
    float avg_confidence = 0.0f;
    float score_proxy = 0.0f;
    float precision = 0.0f;
    float recall = 0.0f;
    float f1 = 0.0f;
    float matched_iou = 0.0f;
    std::map<int64_t, ClassStats> per_class;

    void validate() const {
        TORCH_CHECK(images >= 0, "images must be non-negative");
        TORCH_CHECK(predicted_boxes >= 0, "predicted_boxes must be non-negative");
        TORCH_CHECK(target_boxes >= 0, "target_boxes must be non-negative");
        TORCH_CHECK(true_positives >= 0, "true_positives must be non-negative");
        TORCH_CHECK(false_positives >= 0, "false_positives must be non-negative");
        TORCH_CHECK(false_negatives >= 0, "false_negatives must be non-negative");
        TORCH_CHECK(avg_confidence >= 0.0f, "avg_confidence must be non-negative");
        TORCH_CHECK(score_proxy >= 0.0f, "score_proxy must be non-negative");
        TORCH_CHECK(precision >= 0.0f && precision <= 1.0f, "precision must be in [0,1]");
        TORCH_CHECK(recall >= 0.0f && recall <= 1.0f, "recall must be in [0,1]");
        TORCH_CHECK(f1 >= 0.0f && f1 <= 1.0f, "f1 must be in [0,1]");
        TORCH_CHECK(matched_iou >= 0.0f, "matched_iou must be non-negative");
        for (const auto& [cls, stats] : per_class) {
            TORCH_CHECK(cls >= 0, "per-class key must be non-negative");
            stats.validate();
        }
    }
};

inline float bbox_iou_xyxy(const BBox& a, const BBox& b) {
    const float inter_x1 = std::max(a.x1, b.x1);
    const float inter_y1 = std::max(a.y1, b.y1);
    const float inter_x2 = std::min(a.x2, b.x2);
    const float inter_y2 = std::min(a.y2, b.y2);

    const float inter_w = std::max(0.0f, inter_x2 - inter_x1);
    const float inter_h = std::max(0.0f, inter_y2 - inter_y1);
    const float inter_area = inter_w * inter_h;

    const float area_a = std::max(0.0f, a.x2 - a.x1) * std::max(0.0f, a.y2 - a.y1);
    const float area_b = std::max(0.0f, b.x2 - b.x1) * std::max(0.0f, b.y2 - b.y1);
    const float denom = area_a + area_b - inter_area + 1e-9f;
    return inter_area / denom;
}

inline torch::Tensor collapse_yolo_box_predictions(
    const torch::Tensor& pred_boxes,
    const YoloPostProcessConfig& config) {
    config.validate();
    TORCH_CHECK(pred_boxes.dim() == 3, "pred_boxes must be [B, anchors, box_channels], got ", pred_boxes.sizes());
    TORCH_CHECK(pred_boxes.size(2) == config.box_channels(),
        "Unexpected box channel count, expected ", config.box_channels(), " got ", pred_boxes.size(2));

    if (!config.use_dfl) {
        return pred_boxes;
    }

    auto logits = pred_boxes.view({pred_boxes.size(0), pred_boxes.size(1), 4, config.reg_max});
    auto probs = torch::softmax(logits, -1);
    auto bins = torch::arange(config.reg_max, probs.options());
    return (probs * bins).sum(-1);
}

inline torch::Tensor nms(const torch::Tensor& bboxes, const torch::Tensor& scores, float iou_threshold, int64_t topk = -1) {
    TORCH_CHECK(bboxes.dim() == 2 && bboxes.size(1) == 4,
        "BBoxes must be 2D (N,4), got dimensions: ", bboxes.sizes());
    TORCH_CHECK(scores.dim() == 1 && scores.size(0) == bboxes.size(0),
        "Scores must match bboxes count! Scores size: ", scores.size(0), ", BBoxes size: ", bboxes.size(0));
    TORCH_CHECK(iou_threshold >= 0 && iou_threshold <= 1,
        "IoU threshold must be in [0,1], got: ", iou_threshold);

    // KEY: custom NMS keeps the implementation local and device-agnostic.
    torch::Tensor bboxes_float = bboxes.to(torch::kFloat);
    torch::Tensor scores_float = scores.to(torch::kFloat);
    torch::Device device = bboxes_float.device();

    auto [sorted_scores, sorted_idxs] = torch::sort(scores_float, 0, true);
    bboxes_float = bboxes_float.index_select(0, sorted_idxs);

    if (topk > 0 && topk < static_cast<int64_t>(sorted_idxs.size(0))) {
        sorted_idxs = sorted_idxs.narrow(0, 0, topk);
        bboxes_float = bboxes_float.narrow(0, 0, topk);
    }

    torch::Tensor keep = torch::zeros({ sorted_idxs.size(0) }, torch::kBool).to(device);
    torch::Tensor areas = (bboxes_float.select(1, 2) - bboxes_float.select(1, 0)) *
        (bboxes_float.select(1, 3) - bboxes_float.select(1, 1));

    for (int64_t i = 0; i < static_cast<int64_t>(sorted_idxs.size(0)); i++) {
        if (likely(!keep[i].item<bool>())) {
            keep[i] = true;
            torch::Tensor current_bbox = bboxes_float[i].unsqueeze(0);
            torch::Tensor current_area = areas[i].unsqueeze(0);

            int64_t rest_size = static_cast<int64_t>(sorted_idxs.size(0)) - i - 1;
            if (rest_size <= 0) break;

            torch::Tensor rest_bboxes = bboxes_float.narrow(0, i + 1, rest_size);
            torch::Tensor rest_areas = areas.narrow(0, i + 1, rest_size);

            auto inter_x1 = torch::max(current_bbox.select(1, 0), rest_bboxes.select(1, 0));
            auto inter_y1 = torch::max(current_bbox.select(1, 1), rest_bboxes.select(1, 1));
            auto inter_x2 = torch::min(current_bbox.select(1, 2), rest_bboxes.select(1, 2));
            auto inter_y2 = torch::min(current_bbox.select(1, 3), rest_bboxes.select(1, 3));

            auto inter_w = torch::clamp(inter_x2 - inter_x1, 0);
            auto inter_h = torch::clamp(inter_y2 - inter_y1, 0);
            auto inter_area = inter_w * inter_h;

            auto iou = inter_area / (current_area + rest_areas - inter_area + 1e-9f);
            auto mask = iou <= iou_threshold;

            auto rest_keep = keep.narrow(0, i + 1, rest_size);
            rest_keep = rest_keep & mask.to(torch::kBool);
            keep.narrow(0, i + 1, rest_size).copy_(rest_keep);
        }
    }

    return sorted_idxs.masked_select(keep);
}

inline std::vector<BBox> post_process(
    const torch::Tensor& pred,
    const YoloPostProcessConfig& config) {
    config.validate();

    // MODIFIED: prediction protocol is [B, anchors, box_channels + classes] with no extra objectness term.
    TORCH_CHECK(pred.dim() == 3 && pred.size(2) == config.box_channels() + config.num_classes,
        "Prediction must be 3D (batch, anchors, box_channels+classes), got: ", pred.sizes());

    torch::Tensor pred_float = pred.to(torch::kFloat);
    torch::Device device = pred_float.device();

    std::vector<BBox> final_bboxes;

    // KEY: current implementation decodes and returns detections for batch item 0 only.
    auto pred_batch = pred_float[0];
    auto raw_boxes = pred_batch.narrow(1, 0, config.box_channels()).unsqueeze(0);
    auto boxes = collapse_yolo_box_predictions(raw_boxes, config).squeeze(0);
    auto cls_scores = pred_batch.narrow(1, config.box_channels(), config.num_classes);
    auto max_result = torch::max(cls_scores, 1);
    auto max_cls_scores = std::get<0>(max_result);
    auto max_cls_idxs = std::get<1>(max_result);

    auto conf_mask = max_cls_scores > config.conf_threshold;
    boxes = boxes.masked_select(conf_mask.unsqueeze(1)).reshape({ -1, 4 });
    cls_scores = cls_scores.masked_select(conf_mask.unsqueeze(1)).reshape({ -1, config.num_classes });
    max_cls_scores = max_cls_scores.masked_select(conf_mask);
    max_cls_idxs = max_cls_idxs.masked_select(conf_mask);

    if (boxes.numel() == 0) {
        return final_bboxes;
    }

    for (int64_t cls = 0; cls < config.num_classes; cls++) {
        auto cls_mask = max_cls_idxs == cls;
        auto cls_boxes = boxes.masked_select(cls_mask.unsqueeze(1)).reshape({ -1, 4 });
        auto cls_scores = max_cls_scores.masked_select(cls_mask);

        if (cls_boxes.numel() == 0) {
            continue;
        }

        auto keep_idxs = nms(cls_boxes, cls_scores, config.iou_threshold);
        cls_boxes = cls_boxes.index_select(0, keep_idxs);
        cls_scores = cls_scores.index_select(0, keep_idxs);

        auto boxes_cpu = cls_boxes.to(torch::kCPU);
        auto scores_cpu = cls_scores.to(torch::kCPU);

        for (int64_t i = 0; i < static_cast<int64_t>(keep_idxs.size(0)); i++) {
            BBox bbox;
            bbox.x1 = boxes_cpu[i][0].item<float>();
            bbox.y1 = boxes_cpu[i][1].item<float>();
            bbox.x2 = boxes_cpu[i][2].item<float>();
            bbox.y2 = boxes_cpu[i][3].item<float>();
            bbox.score = scores_cpu[i].item<float>();
            bbox.cls = cls;
            final_bboxes.push_back(bbox);
        }
    }

    return final_bboxes;
}

inline std::vector<std::vector<BBox>> post_process_batch(
    const torch::Tensor& pred,
    const YoloPostProcessConfig& config) {
    config.validate();
    TORCH_CHECK(pred.dim() == 3 && pred.size(2) == config.box_channels() + config.num_classes,
        "Prediction must be 3D (batch, anchors, box_channels+classes), got: ", pred.sizes());

    std::vector<std::vector<BBox>> batch_boxes;
    batch_boxes.reserve(static_cast<size_t>(pred.size(0)));
    for (int64_t b = 0; b < pred.size(0); ++b) {
        batch_boxes.push_back(post_process(pred[b].unsqueeze(0), config));
    }
    return batch_boxes;
}

inline std::vector<BBox> post_process(
    const torch::Tensor& pred,
    float conf_threshold = 0.25f,
    float iou_threshold = 0.45f,
    int64_t num_classes = 80) {
    YoloPostProcessConfig config;
    config.conf_threshold = conf_threshold;
    config.iou_threshold = iou_threshold;
    config.num_classes = num_classes;
    return post_process(pred, config);
}

inline YoloEvalSummary summarize_yolo_detections(
    const std::vector<std::vector<BBox>>& detections,
    int64_t target_boxes) {
    YoloEvalSummary summary;
    summary.images = static_cast<int64_t>(detections.size());
    summary.target_boxes = target_boxes;

    float score_sum = 0.0f;
    for (const auto& per_image : detections) {
        summary.predicted_boxes += static_cast<int64_t>(per_image.size());
        for (const auto& box : per_image) {
            score_sum += box.score;
            summary.per_class[box.cls].predicted_boxes += 1;
        }
    }

    if (summary.predicted_boxes > 0) {
        summary.avg_confidence = score_sum / static_cast<float>(summary.predicted_boxes);
    }

    const float denom = static_cast<float>(std::max<int64_t>(summary.target_boxes, 1));
    summary.score_proxy = static_cast<float>(summary.predicted_boxes) / denom;
    summary.validate();
    return summary;
}

inline YoloEvalSummary summarize_yolo_matches(
    const std::vector<std::vector<BBox>>& detections,
    const std::vector<std::vector<BBox>>& targets,
    float iou_threshold = 0.5f) {
    TORCH_CHECK(iou_threshold >= 0.0f && iou_threshold <= 1.0f, "iou_threshold must be in [0,1]");
    TORCH_CHECK(detections.size() == targets.size(), "detections/targets image count mismatch");

    YoloEvalSummary summary;
    summary.images = static_cast<int64_t>(detections.size());

    int64_t true_positive = 0;
    float matched_iou_sum = 0.0f;
    float score_sum = 0.0f;

    for (size_t i = 0; i < detections.size(); ++i) {
        const auto& dets = detections[i];
        const auto& gts = targets[i];
        summary.predicted_boxes += static_cast<int64_t>(dets.size());
        summary.target_boxes += static_cast<int64_t>(gts.size());

        for (const auto& gt : gts) {
            summary.per_class[gt.cls].target_boxes += 1;
        }
        for (const auto& det : dets) {
            summary.per_class[det.cls].predicted_boxes += 1;
        }

        std::vector<bool> gt_used(gts.size(), false);
        for (const auto& det : dets) {
            score_sum += det.score;
            float best_iou = 0.0f;
            int64_t best_gt = -1;
            for (size_t g = 0; g < gts.size(); ++g) {
                if (gt_used[g] || det.cls != gts[g].cls) {
                    continue;
                }
                const float iou = bbox_iou_xyxy(det, gts[g]);
                if (iou > best_iou) {
                    best_iou = iou;
                    best_gt = static_cast<int64_t>(g);
                }
            }

            if (best_gt >= 0 && best_iou >= iou_threshold) {
                gt_used[best_gt] = true;
                true_positive += 1;
                summary.per_class[det.cls].true_positives += 1;
                matched_iou_sum += best_iou;
            }
        }
    }

    if (summary.predicted_boxes > 0) {
        summary.avg_confidence = score_sum / static_cast<float>(summary.predicted_boxes);
        summary.precision = static_cast<float>(true_positive) / static_cast<float>(summary.predicted_boxes);
    }
    if (summary.target_boxes > 0) {
        summary.recall = static_cast<float>(true_positive) / static_cast<float>(summary.target_boxes);
        summary.score_proxy = summary.recall;
    }
    if (summary.precision + summary.recall > 0.0f) {
        summary.f1 = 2.0f * summary.precision * summary.recall / (summary.precision + summary.recall);
    }
    if (true_positive > 0) {
        summary.matched_iou = matched_iou_sum / static_cast<float>(true_positive);
    }

    summary.true_positives = true_positive;
    summary.false_positives = summary.predicted_boxes - true_positive;
    summary.false_negatives = summary.target_boxes - true_positive;
    for (auto& [cls, stats] : summary.per_class) {
        stats.false_positives = stats.predicted_boxes - stats.true_positives;
        stats.false_negatives = stats.target_boxes - stats.true_positives;
        if (stats.predicted_boxes > 0) {
            stats.precision = static_cast<float>(stats.true_positives) / static_cast<float>(stats.predicted_boxes);
        }
        if (stats.target_boxes > 0) {
            stats.recall = static_cast<float>(stats.true_positives) / static_cast<float>(stats.target_boxes);
        }
        if (stats.precision + stats.recall > 0.0f) {
            stats.f1 = 2.0f * stats.precision * stats.recall / (stats.precision + stats.recall);
        }
    }

    summary.validate();
    return summary;
}

inline torch::Tensor scale_bboxes(const torch::Tensor& bboxes, const torch::Tensor& img_shape, const torch::Tensor& pad_shape) {
    TORCH_CHECK(bboxes.dim() == 2 && bboxes.size(1) == 4, "BBoxes must be 2D (N,4)");
    TORCH_CHECK(img_shape.dim() == 1 && img_shape.size(0) == 2, "Img shape must be (H,W)");
    TORCH_CHECK(pad_shape.dim() == 1 && pad_shape.size(0) == 2, "Pad shape must be (H,W)");

    torch::Tensor bboxes_float = bboxes.to(torch::kFloat);
    float img_h = img_shape[0].item<float>();
    float img_w = img_shape[1].item<float>();
    float pad_h = pad_shape[0].item<float>();
    float pad_w = pad_shape[1].item<float>();

    // KEY: inverse letterbox-style rescaling back to the source image coordinates.
    float scale = std::min(pad_h / img_h, pad_w / img_w);
    float pad_x = (pad_w - img_w * scale) / 2.0f;
    float pad_y = (pad_h - img_h * scale) / 2.0f;

    bboxes_float.select(1, 0) = (bboxes_float.select(1, 0) * pad_w - pad_x) / scale;
    bboxes_float.select(1, 1) = (bboxes_float.select(1, 1) * pad_h - pad_y) / scale;
    bboxes_float.select(1, 2) = (bboxes_float.select(1, 2) * pad_w - pad_x) / scale;
    bboxes_float.select(1, 3) = (bboxes_float.select(1, 3) * pad_h - pad_y) / scale;

    bboxes_float = torch::clamp(bboxes_float, 0.0f, static_cast<float>(std::max(img_w, img_h)));
    return bboxes_float;
}

#endif // TORCH_DETECT_H
