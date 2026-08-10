#include "torch_runtime_task_dispatcher.h"
#include "torch_runtime_task_types.h"
#include "torch_runtime_manifest.h"
#include "torch_runtime_segmentation_executor.h"
#include "torch_runtime_detection_executor.h"
#include "torch_segmentation_mainline_bridge.h"
#include "torch_test_host.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

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

TorchTaskResultCpp RunSegmentationTrainingLifecycleTask(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request)
{
    TorchTaskResultCpp result;
    result.requested_device = config.device.empty() ? "cpu" : config.device;
    constexpr uint64_t deterministic_seed = 1023;

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

        const auto train_images = MakeSegmentationLifecycleImages(runner_config);
        const auto train_masks = MakeSegmentationLifecycleMasks(runner_config);
        const auto eval_images = MakeSegmentationLifecycleImages(runner_config);
        const auto eval_masks = MakeSegmentationLifecycleMasks(runner_config);

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
