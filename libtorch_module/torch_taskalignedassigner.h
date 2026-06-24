#ifndef TORCH_TASKALIGNEDASSIGNER_H
#define TORCH_TASKALIGNEDASSIGNER_H

// NOTE: formatting normalized during the libtorch_module cleanup pass.

#include <torch/torch.h>
#include <vector>
#include <cmath>
#include <stdexcept>
#include <tuple>

// Comment tags used in this file:
// KEY: important execution path.
// MODIFIED: behavior adjusted during the current cleanup pass.
// CHECK: confirm during integration/debug.
// RISK: known limitation or possible issue.
// EVOLVE: recommended future improvement.
// VERIFY: needs explicit runtime or numerical validation.

class TaskAlignedAssignerImpl : public torch::nn::Module {
public:
    TaskAlignedAssignerImpl(int64_t topk = 10, int64_t num_classes = 80, float alpha = 1.0f, float beta = 6.0f)
        : topk_(topk), num_classes_(num_classes), alpha_(alpha), beta_(beta) {

        TORCH_CHECK(topk > 0, "TaskAlignedAssigner topk must be positive");
        TORCH_CHECK(num_classes > 0, "TaskAlignedAssigner num_classes must be positive");

        // register_buffer("topk_val", torch::tensor(topk));
    }

    // KEY: maps predicted anchors to gt indices using task-aligned score * IoU metric.
    // gt_labels: [B, MaxGT, 1] or [B, MaxGT]
    // gt_bboxes: [B, MaxGT, 4]
    // mask_gt:   [B, MaxGT, 1] or [B, MaxGT]
    torch::Tensor forward(
        const torch::Tensor& pd_scores_in,
        const torch::Tensor& pd_bboxes_in,
        const torch::Tensor& gt_labels_in,
        const torch::Tensor& gt_bboxes_in,
        const torch::Tensor& mask_gt_in) {

        torch::Tensor pd_scores = pd_scores_in;
        if (pd_scores.dim() == 4 && pd_scores.size(1) == 1) {
            pd_scores = pd_scores.squeeze(1);
        }

        torch::Tensor pd_bboxes = pd_bboxes_in;
        if (pd_bboxes.dim() == 4 && pd_bboxes.size(1) == 1) {
            pd_bboxes = pd_bboxes.squeeze(1);
        }

        int64_t batch_size = pd_scores.size(0);
        int64_t num_anchors = pd_scores.size(1);
        int64_t num_classes = pd_scores.size(2);
        int64_t max_gt = gt_labels_in.size(1);

        torch::Device device = pd_scores.device();

        // KEY: normalize all targets onto the same device and expected batch-major layout.
        torch::Tensor gt_labels = gt_labels_in.to(device).reshape({ batch_size, max_gt });
        torch::Tensor gt_bboxes = gt_bboxes_in.to(device).reshape({ batch_size, max_gt, 4 });
        torch::Tensor mask_gt = mask_gt_in.to(device).reshape({ batch_size, max_gt });

        // KEY: output uses 0 for background and 1-based gt indices for positives.
        torch::Tensor assigned_gt_inds = torch::zeros({ batch_size, num_anchors },
            torch::dtype(torch::kLong).device(device));

        for (int64_t b = 0; b < batch_size; ++b) {
            torch::Tensor valid_mask = mask_gt[b].to(torch::kBool); // [MaxGT]

            if (!valid_mask.any().item<bool>()) continue;

            // gt_labels_b: [N_valid]
            torch::Tensor gt_labels_b = gt_labels[b].masked_select(valid_mask).to(torch::kLong);
            // gt_bboxes_b: [N_valid, 4]
            torch::Tensor valid_indices = torch::nonzero(valid_mask).squeeze(1); // [N_valid]
            torch::Tensor gt_bboxes_b = gt_bboxes[b].index_select(0, valid_indices);

            int64_t num_valid_gt = gt_labels_b.size(0);

            // KEY: task-aligned metric = class score^alpha * IoU^beta.

            // pd_bboxes[b]: [A, 4]
            // gt_bboxes_b:  [N_valid, 4]
            torch::Tensor iou = bbox_iou(pd_bboxes[b], gt_bboxes_b); // [A, N_valid]

            // pd_scores[b]: [A, C]

            torch::Tensor gt_labels_expand = gt_labels_b.unsqueeze(0).expand({ num_anchors, num_valid_gt });

            // pd_scores[b].gather(1, gt_labels_expand) -> [A, N_valid]
            torch::Tensor cls_scores = pd_scores[b].gather(1, gt_labels_expand);

            torch::Tensor metric = torch::pow(cls_scores, alpha_) * torch::pow(iou, beta_); // [A, N_valid]

            // metric: [A, N_valid] -> topk(dim=0)
            int64_t k = std::min(topk_, num_anchors);
            auto topk_res = torch::topk(metric, k, 0, true); // values, indices: [K, N_valid]
            torch::Tensor topk_metrics = std::get<0>(topk_res);
            torch::Tensor topk_idxs = std::get<1>(topk_res);

            // mask_topk: [A, N_valid]
            torch::Tensor mask_topk = torch::zeros_like(metric, torch::kBool);
            // scatter topk indices to mask
            // src: ones [K, N_valid]
            mask_topk.scatter_(0, topk_idxs, true);

            // EVOLVE: optionally enforce positive-IoU filtering before final matching.

            // mask_topk: [A, N_valid]
            torch::Tensor sum_mask = mask_topk.sum(1); // [A]
            if ((sum_mask > 1).any().item<bool>()) {
                // metric: [A, N_valid]
                auto max_res = metric.max(1, true); // [A, 1]
                torch::Tensor max_metric = std::get<0>(max_res);
                mask_topk &= (metric == max_metric);
            }

            // mask_topk: [A, N_valid] (bool)

            // nonzero: [N_match, 2] (anchor_idx, gt_idx_in_valid)
            auto match_indices = torch::nonzero(mask_topk);
            if (match_indices.size(0) > 0) {
                torch::Tensor anchor_idxs = match_indices.select(1, 0);
                torch::Tensor gt_idxs_in_valid = match_indices.select(1, 1);

                // real_gt_idxs = valid_indices[gt_idxs_in_valid] + 1
                torch::Tensor real_gt_idxs = valid_indices.index_select(0, gt_idxs_in_valid) + 1;

                // assigned_gt_inds[b][anchor_idxs] = real_gt_idxs
                assigned_gt_inds[b].index_put_({ anchor_idxs }, real_gt_idxs);
            }
        }

        return assigned_gt_inds;
    }

private:
    int64_t topk_;
    int64_t num_classes_;
    float alpha_;
    float beta_;

    // KEY: pairwise IoU between predicted boxes [A,4] and gt boxes [G,4].
    torch::Tensor bbox_iou(const torch::Tensor& box1, const torch::Tensor& box2) {
        // box1: [A, 4] (x1, y1, x2, y2)
        // box2: [G, 4]

        int64_t A = box1.size(0);
        int64_t G = box2.size(0);

        // b1: [A, 1, 4]
        // b2: [1, G, 4]
        auto b1 = box1.unsqueeze(1);
        auto b2 = box2.unsqueeze(0);

        // KEY: broadcasted intersection/union computation keeps the implementation vectorized.
        auto inter_x1 = torch::max(b1.slice(2, 0, 1), b2.slice(2, 0, 1));
        auto inter_y1 = torch::max(b1.slice(2, 1, 2), b2.slice(2, 1, 2));
        auto inter_x2 = torch::min(b1.slice(2, 2, 3), b2.slice(2, 2, 3));
        auto inter_y2 = torch::min(b1.slice(2, 3, 4), b2.slice(2, 3, 4));

        auto inter_w = (inter_x2 - inter_x1).clamp_min(0);
        auto inter_h = (inter_y2 - inter_y1).clamp_min(0);
        auto inter_area = inter_w * inter_h; // [A, G, 1]

        // Union
        auto w1 = b1.slice(2, 2, 3) - b1.slice(2, 0, 1);
        auto h1 = b1.slice(2, 3, 4) - b1.slice(2, 1, 2);
        auto area1 = w1 * h1;

        auto w2 = b2.slice(2, 2, 3) - b2.slice(2, 0, 1);
        auto h2 = b2.slice(2, 3, 4) - b2.slice(2, 1, 2);
        auto area2 = w2 * h2;

        auto union_area = area1 + area2 - inter_area + 1e-7;

        return (inter_area / union_area).squeeze(2); // [A, G]
    }
};

TORCH_MODULE(TaskAlignedAssigner);

#endif // TORCH_TASKALIGNEDASSIGNER_H
