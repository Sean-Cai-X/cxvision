#include "CxAutomaticDiagnosticClosure.h"

#include "CxImageReferenceCandidateGenerator.h"
#include "CxTorchExecutionAdapter.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>
#include <map>
#include <numeric>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <set>
#include <string>
#include <vector>

namespace
{
struct ClosureBinding
{
    bool bound = false;
    std::string status;
    std::string task_id;
    std::string model_id;
    std::filesystem::path manifest_path;
    std::filesystem::path dataset_root;
    std::string device = "cpu";
    std::string extra_json;
};

struct ClosureSample
{
    std::string case_id;
    std::filesystem::path image_path;
    std::filesystem::path label_mask_path;
    std::filesystem::path label_companion_path;
};

struct ClosureRow
{
    std::string row_id;
    std::string review_item;
    int implementation_phase = 0;
    std::string case_track;
    std::string annotation_contract_id;
    std::string model_track_id;
    std::string evaluator_id;
    bool taxonomy_valid = false;
    bool annotation_contract_valid = false;
    bool model_track_valid = false;
    bool evaluator_contract_valid = false;
    bool evaluator_runtime_supported = false;
    std::string evaluator_runtime_status;
    std::filesystem::path binding_package_path;
    bool binding_package_valid = false;
    std::string typed_label_kind;
    std::filesystem::path typed_label_candidate_ref;
    std::filesystem::path source_manifest;
    std::string source_set_id;
    ClosureBinding parent;
    ClosureBinding child;
    std::string label_binding_status;
    std::filesystem::path label_mask_path;
    CxImageReferenceCandidateRequest candidate;
    std::vector<ClosureSample> samples;
    bool explicit_frozen_validation = false;
};

struct ProcessStage
{
    int index = 0;
    std::string stage_id;
    std::string status_source;
    std::vector<std::string> requires;
};

struct ProcessDefinition
{
    std::string process_id;
    std::vector<ProcessStage> stages;
};

struct ContractRegistry
{
    std::set<std::string> case_tracks;
    std::map<std::string, std::string> case_assignments;
    std::map<std::string, std::string> annotation_tracks;
    std::map<std::string, std::set<std::string>> model_tracks;
    std::map<std::string, std::set<std::string>> evaluator_tracks;
    std::map<std::string, std::string> evaluator_status;
};

struct CompletedCase
{
    std::string row_id;
    std::string case_id;
    double parent_child_iou = 0.0;
    double parent_label_iou = 0.0;
    double child_label_iou = 0.0;
    double label_delta = 0.0;
};

struct GatePolicy
{
    int minimum_case_count = 3;
    int maximum_regression_count = 0;
    double minimum_child_label_iou = 0.85;
    double minimum_mean_label_iou_delta = 0.0;
    double maximum_child_label_iou_stddev = 0.05;
};

std::string JsonEscape(const std::string& value)
{
    std::string escaped;
    for (const char ch : value)
    {
        if (ch == '\\' || ch == '"')
            escaped += '\\';
        if (ch == '\n') { escaped += "\\n"; continue; }
        if (ch == '\r') { escaped += "\\r"; continue; }
        escaped += ch;
    }
    return escaped;
}

std::string NodeString(const cv::FileNode& node, const char* key)
{
    const cv::FileNode value = node[key];
    return value.empty() ? std::string() : static_cast<std::string>(value);
}

double NodeDouble(const cv::FileNode& node, const char* key, double fallback)
{
    const cv::FileNode value = node[key];
    return value.empty() ? fallback : static_cast<double>(value);
}

int NodeInt(const cv::FileNode& node, const char* key, int fallback)
{
    const cv::FileNode value = node[key];
    return value.empty() ? fallback : static_cast<int>(value);
}

std::set<std::string> NodeStringSet(const cv::FileNode& node)
{
    std::set<std::string> values;
    if (!node.isSeq())
        return values;
    for (const cv::FileNode& value : node)
    {
        if (value.isString())
            values.insert(static_cast<std::string>(value));
    }
    return values;
}

std::vector<std::string> NodeStringVector(const cv::FileNode& node)
{
    std::vector<std::string> values;
    if (!node.isSeq())
        return values;
    for (const cv::FileNode& value : node)
    {
        if (value.isString())
            values.push_back(static_cast<std::string>(value));
    }
    return values;
}

bool LoadProcessDefinition(
    const std::filesystem::path& path,
    ProcessDefinition& process,
    std::string& reason)
{
    cv::FileStorage storage(path.string(), cv::FileStorage::READ | cv::FileStorage::FORMAT_JSON);
    if (!storage.isOpened() || !storage["stages"].isSeq())
    {
        reason = "automatic diagnostic process asset cannot be opened or has no stages";
        return false;
    }
    if (NodeInt(storage.root(), "training_enabled", 1) != 0 ||
        NodeString(storage.root(), "execution") != "strict_serial")
    {
        reason = "automatic diagnostic process must disable training and require strict serial execution";
        return false;
    }
    process.process_id = NodeString(storage.root(), "process_id");
    const std::set<std::string> allowed_sources = {
        "contract_assets_valid", "source_images_bound", "typed_labels_bound",
        "runtime_models_bound", "evaluators_supported", "cases_executed",
        "frozen_validation_bound", "aggregate_emitted", "promotion_gate_emitted",
        "final_human_review"};
    std::set<std::string> prior_ids;
    int expected_index = 1;
    for (const cv::FileNode& node : storage["stages"])
    {
        ProcessStage stage;
        stage.index = NodeInt(node, "index", 0);
        stage.stage_id = NodeString(node, "stage_id");
        stage.status_source = NodeString(node, "status_source");
        stage.requires = NodeStringVector(node["requires"]);
        if (stage.index != expected_index || stage.stage_id.empty() ||
            allowed_sources.count(stage.status_source) == 0 ||
            prior_ids.count(stage.stage_id) != 0)
        {
            reason = "automatic diagnostic process stages are not uniquely and contiguously ordered";
            return false;
        }
        for (const std::string& requirement : stage.requires)
        {
            if (prior_ids.count(requirement) == 0)
            {
                reason = "automatic diagnostic process dependency does not reference an earlier stage";
                return false;
            }
        }
        prior_ids.insert(stage.stage_id);
        process.stages.push_back(stage);
        ++expected_index;
    }
    return !process.process_id.empty() && !process.stages.empty();
}

bool LoadContractRegistry(
    const std::filesystem::path& taxonomy_path,
    const std::filesystem::path& annotation_path,
    const std::filesystem::path& model_path,
    const std::filesystem::path& evaluator_path,
    ContractRegistry& registry,
    std::string& reason)
{
    cv::FileStorage taxonomy(taxonomy_path.string(), cv::FileStorage::READ | cv::FileStorage::FORMAT_JSON);
    cv::FileStorage annotations(annotation_path.string(), cv::FileStorage::READ | cv::FileStorage::FORMAT_JSON);
    cv::FileStorage models(model_path.string(), cv::FileStorage::READ | cv::FileStorage::FORMAT_JSON);
    cv::FileStorage evaluators(evaluator_path.string(), cv::FileStorage::READ | cv::FileStorage::FORMAT_JSON);
    if (!taxonomy.isOpened() || !annotations.isOpened() || !models.isOpened() || !evaluators.isOpened())
    {
        reason = "one or more automatic diagnostic contract assets cannot be opened";
        return false;
    }
    if (!taxonomy["tracks"].isSeq() || !taxonomy["case_assignments"].isSeq() ||
        !annotations["contracts"].isSeq() || !models["tracks"].isSeq() ||
        !evaluators["evaluators"].isSeq())
    {
        reason = "automatic diagnostic contract assets do not satisfy their collection schema";
        return false;
    }
    for (const cv::FileNode& node : taxonomy["tracks"])
        registry.case_tracks.insert(NodeString(node, "track_id"));
    for (const cv::FileNode& node : taxonomy["case_assignments"])
        registry.case_assignments[NodeString(node, "case_id")] = NodeString(node, "track_id");
    for (const cv::FileNode& node : annotations["contracts"])
        registry.annotation_tracks[NodeString(node, "contract_id")] = NodeString(node, "track_id");
    for (const cv::FileNode& node : models["tracks"])
        registry.model_tracks[NodeString(node, "model_track_id")] = NodeStringSet(node["case_tracks"]);
    for (const cv::FileNode& node : evaluators["evaluators"])
    {
        const std::string id = NodeString(node, "evaluator_id");
        registry.evaluator_tracks[id] = NodeStringSet(node["case_tracks"]);
        registry.evaluator_status[id] = NodeString(node, "runtime_status");
    }
    return true;
}

std::filesystem::path ResolveAssetPath(
    const std::filesystem::path&,
    const std::filesystem::path& value)
{
    if (value.empty() || value.is_absolute())
        return value.lexically_normal();
    return (std::filesystem::current_path() / value).lexically_normal();
}

bool IsReadableImage(const std::filesystem::path& path)
{
    if (!std::filesystem::is_regular_file(path))
        return false;
    const std::string extension = path.extension().string();
    return extension == ".png" || extension == ".jpg" || extension == ".jpeg" ||
           extension == ".bmp" || extension == ".tif" || extension == ".tiff";
}

std::filesystem::path FirstImageInPath(const std::filesystem::path& source)
{
    if (IsReadableImage(source))
        return source;
    if (!std::filesystem::is_directory(source))
        return {};

    std::vector<std::filesystem::path> images;
    std::error_code error;
    for (std::filesystem::recursive_directory_iterator iterator(
             source,
             std::filesystem::directory_options::skip_permission_denied,
             error), end;
         iterator != end;
         iterator.increment(error))
    {
        if (error)
        {
            error.clear();
            continue;
        }
        if (iterator->is_symlink(error))
        {
            if (iterator->is_directory(error))
                iterator.disable_recursion_pending();
            continue;
        }
        if (IsReadableImage(iterator->path()))
            images.push_back(iterator->path());
    }
    std::sort(images.begin(), images.end());
    return images.empty() ? std::filesystem::path() : images.front();
}

std::filesystem::path ResolveFirstManifestImage(
    const std::filesystem::path& manifest_path,
    const std::string& set_id)
{
    cv::FileStorage manifest(manifest_path.string(), cv::FileStorage::READ | cv::FileStorage::FORMAT_JSON);
    if (!manifest.isOpened())
        return {};
    const cv::FileNode sets = manifest["sets"];
    if (!sets.isSeq())
        return {};
    for (const cv::FileNode& set : sets)
    {
        if (NodeString(set, "case_id") != set_id)
            continue;
        const cv::FileNode sources = set["image_sources"];
        if (!sources.isSeq())
            return {};
        for (const cv::FileNode& source : sources)
        {
            const std::filesystem::path candidate = NodeString(source, "path");
            const std::filesystem::path image = FirstImageInPath(candidate);
            if (!image.empty())
                return image;
        }
    }
    return {};
}

ClosureBinding ParseBinding(const cv::FileNode& node)
{
    ClosureBinding binding;
    if (node.empty())
    {
        binding.status = "missing";
        return binding;
    }
    if (node.isString())
    {
        binding.status = static_cast<std::string>(node);
        return binding;
    }
    binding.status = NodeString(node, "status");
    binding.task_id = NodeString(node, "task_id");
    binding.model_id = NodeString(node, "model_id");
    binding.manifest_path = NodeString(node, "manifest_path");
    binding.dataset_root = NodeString(node, "dataset_root");
    const std::string device = NodeString(node, "requested_device");
    if (!device.empty())
        binding.device = device;
    binding.extra_json = NodeString(node, "extra_json");
    binding.bound = binding.status == "bound" &&
                    !binding.task_id.empty() &&
                    !binding.model_id.empty() &&
                    std::filesystem::is_regular_file(binding.manifest_path);
    return binding;
}

bool LoadBindingPackage(
    const std::filesystem::path& package_path,
    const std::string& expected_track,
    ClosureRow& row,
    std::string& reason)
{
    cv::FileStorage package(package_path.string(), cv::FileStorage::READ | cv::FileStorage::FORMAT_JSON);
    if (!package.isOpened() ||
        NodeString(package.root(), "schema") != "cxvision.single_image_runtime_binding.v1" ||
        NodeInt(package.root(), "training_enabled", 1) != 0)
    {
        reason = "single-image binding package is invalid or enables training";
        return false;
    }
    if (NodeString(package.root(), "case_track") != expected_track)
    {
        reason = "single-image binding package track does not match its matrix row";
        return false;
    }
    const cv::FileNode sample_node = package["sample"];
    const cv::FileNode label_node = package["typed_label"];
    ClosureSample sample;
    sample.case_id = NodeString(package.root(), "case_id");
    sample.image_path = ResolveAssetPath(package_path, NodeString(sample_node, "image_path"));
    sample.label_mask_path = ResolveAssetPath(package_path, NodeString(label_node, "path"));
    sample.label_companion_path = ResolveAssetPath(package_path, NodeString(label_node, "companion_path"));
    row.typed_label_kind = NodeString(label_node, "kind");
    row.label_binding_status = NodeString(label_node, "status");
    row.typed_label_candidate_ref = ResolveAssetPath(
        package_path, NodeString(label_node, "candidate_reference"));
    row.parent = ParseBinding(package["parent_binding"]);
    row.child = ParseBinding(package["child_binding"]);
    row.parent.manifest_path = ResolveAssetPath(package_path, row.parent.manifest_path);
    row.child.manifest_path = ResolveAssetPath(package_path, row.child.manifest_path);
    row.parent.bound = row.parent.status == "bound" && !row.parent.task_id.empty() &&
        !row.parent.model_id.empty() && std::filesystem::is_regular_file(row.parent.manifest_path);
    row.child.bound = row.child.status == "bound" && !row.child.task_id.empty() &&
        !row.child.model_id.empty() && std::filesystem::is_regular_file(row.child.manifest_path);

    const std::filesystem::path audit_path = ResolveAssetPath(
        package_path, NodeString(package.root(), "runtime_candidate_audit"));
    if (sample.case_id.empty() || row.typed_label_kind.empty() ||
        !std::filesystem::is_regular_file(sample.image_path) ||
        !std::filesystem::is_regular_file(row.typed_label_candidate_ref) ||
        !std::filesystem::is_regular_file(audit_path))
    {
        reason = "single-image binding package is missing its sample, typed-label candidate, or runtime audit";
        return false;
    }
    if (row.label_binding_status == "bound")
    {
        if (!std::filesystem::is_regular_file(sample.label_mask_path))
        {
            reason = "bound typed label does not reference a physical label asset";
            return false;
        }
        if (row.typed_label_kind == "instance_id_mask_with_class" &&
            !std::filesystem::is_regular_file(sample.label_companion_path))
        {
            reason = "bound instance label is missing its class companion asset";
            return false;
        }
    }
    row.samples.clear();
    row.samples.push_back(sample);
    return true;
}

CxTorchTaskKind TaskKindFromId(const std::string& task_id)
{
    if (task_id.find("segmentation") != std::string::npos ||
        task_id.find("deeplab") != std::string::npos)
        return CxTorchTaskKind::Segmentation;
    if (task_id.find("detection") != std::string::npos ||
        task_id.find("yolo") != std::string::npos)
        return CxTorchTaskKind::Detection;
    if (task_id.find("classification") != std::string::npos)
        return CxTorchTaskKind::Classification;
    return CxTorchTaskKind::Unknown;
}

CxTorchTaskSpec MakeTask(
    const ClosureBinding& binding,
    const ClosureSample& sample,
    const std::filesystem::path& output_dir)
{
    CxTorchTaskSpec task;
    task.kind = TaskKindFromId(binding.task_id);
    task.task_id = binding.task_id;
    task.case_id = sample.case_id;
    task.model_id = binding.model_id;
    task.manifest_path = binding.manifest_path;
    task.dataset_root = binding.dataset_root;
    task.input_image_path = sample.image_path;
    task.output_dir = output_dir;
    task.requested_device = binding.device;
    task.extra_json = binding.extra_json;
    return task;
}

double Mean(const std::vector<double>& values)
{
    if (values.empty())
        return 0.0;
    return std::accumulate(values.begin(), values.end(), 0.0) / values.size();
}

double StandardDeviation(const std::vector<double>& values, double mean)
{
    if (values.size() < 2)
        return 0.0;
    double sum = 0.0;
    for (const double value : values)
        sum += (value - mean) * (value - mean);
    return std::sqrt(sum / values.size());
}

bool WritePreflight(
    const std::filesystem::path& path,
    const std::vector<ClosureRow>& rows,
    const CxAutomaticDiagnosticClosureResult& result,
    std::string& reason)
{
    std::ofstream file(path, std::ios::trunc);
    if (!file)
    {
        reason = "cannot write binding preflight";
        return false;
    }
    file << "{\n  \"schema\": \"cxvision.automatic_diagnostic_binding_preflight.v2\",\n"
         << "  \"status\": \"" << JsonEscape(result.status) << "\",\n"
         << "  \"reason\": \"" << JsonEscape(result.reason) << "\",\n"
         << "  \"discovered_rows\": " << result.discovered_rows << ",\n"
         << "  \"bound_rows\": " << result.bound_rows << ",\n"
         << "  \"rows\": [\n";
    for (std::size_t index = 0; index < rows.size(); ++index)
    {
        const ClosureRow& row = rows[index];
        const bool image_bound = !row.samples.empty() &&
            std::filesystem::is_regular_file(row.samples.front().image_path);
        const bool label_bound = row.label_binding_status == "bound" &&
            !row.samples.empty() &&
            std::filesystem::is_regular_file(row.samples.front().label_mask_path);
        const bool contracts_valid = row.taxonomy_valid && row.annotation_contract_valid &&
            row.model_track_valid && row.evaluator_contract_valid;
        file << "    {\"matrix_row\": \"" << JsonEscape(row.row_id)
             << "\", \"review_item\": \"" << JsonEscape(row.review_item)
             << "\", \"implementation_phase\": " << row.implementation_phase
             << ", \"case_track\": \"" << JsonEscape(row.case_track)
             << "\", \"annotation_contract_id\": \"" << JsonEscape(row.annotation_contract_id)
             << "\", \"model_track_id\": \"" << JsonEscape(row.model_track_id)
             << "\", \"evaluator_id\": \"" << JsonEscape(row.evaluator_id)
             << "\", \"contracts_valid\": " << (contracts_valid ? "true" : "false")
             << ", \"taxonomy_valid\": " << (row.taxonomy_valid ? "true" : "false")
             << ", \"annotation_contract_valid\": " << (row.annotation_contract_valid ? "true" : "false")
             << ", \"model_track_valid\": " << (row.model_track_valid ? "true" : "false")
             << ", \"evaluator_contract_valid\": " << (row.evaluator_contract_valid ? "true" : "false")
             << ", \"evaluator_runtime_supported\": " << (row.evaluator_runtime_supported ? "true" : "false")
             << ", \"evaluator_runtime_status\": \"" << JsonEscape(row.evaluator_runtime_status)
             << "\", \"image_bound\": " << (image_bound ? "true" : "false")
             << ", \"parent_bound\": " << (row.parent.bound ? "true" : "false")
             << ", \"child_bound\": " << (row.child.bound ? "true" : "false")
             << ", \"dataset_label_bound\": " << (label_bound ? "true" : "false")
             << ", \"parent_status\": \"" << JsonEscape(row.parent.status)
             << "\", \"child_status\": \"" << JsonEscape(row.child.status)
             << "\", \"label_status\": \"" << JsonEscape(row.label_binding_status) << "\"}"
             << (index + 1 < rows.size() ? "," : "") << "\n";
    }
    file << "  ]\n}\n";
    return file.good();
}

bool WriteProcessStatus(
    const std::filesystem::path& path,
    const ProcessDefinition& process,
    const std::vector<ClosureRow>& rows,
    const CxAutomaticDiagnosticClosureResult& result,
    std::string& reason)
{
    bool contracts_valid = !rows.empty();
    bool images_bound = !rows.empty();
    bool labels_bound = !rows.empty();
    bool models_bound = !rows.empty();
    bool evaluators_supported = !rows.empty();
    bool frozen_validation_bound = !rows.empty();
    int total_samples = 0;
    for (const ClosureRow& row : rows)
    {
        contracts_valid = contracts_valid && row.taxonomy_valid && row.annotation_contract_valid &&
            row.model_track_valid && row.evaluator_contract_valid;
        images_bound = images_bound && !row.samples.empty();
        labels_bound = labels_bound && row.label_binding_status == "bound" && !row.samples.empty();
        models_bound = models_bound && row.parent.bound && row.child.bound;
        evaluators_supported = evaluators_supported && row.evaluator_runtime_supported;
        frozen_validation_bound = frozen_validation_bound && row.explicit_frozen_validation && !row.samples.empty();
        for (const ClosureSample& sample : row.samples)
        {
            ++total_samples;
            images_bound = images_bound && std::filesystem::is_regular_file(sample.image_path);
            labels_bound = labels_bound && std::filesystem::is_regular_file(sample.label_mask_path);
            frozen_validation_bound = frozen_validation_bound &&
                std::filesystem::is_regular_file(sample.image_path) &&
                std::filesystem::is_regular_file(sample.label_mask_path);
        }
    }
    const bool cases_executed = total_samples > 0 && result.completed_cases >= total_samples;
    const bool aggregate_emitted = std::filesystem::is_regular_file(result.aggregate_ref) &&
        std::filesystem::is_regular_file(result.stability_ref);
    const bool promotion_gate_emitted = std::filesystem::is_regular_file(result.promotion_gate_ref);

    std::map<std::string, bool> facts = {
        {"contract_assets_valid", contracts_valid},
        {"source_images_bound", images_bound},
        {"typed_labels_bound", labels_bound},
        {"runtime_models_bound", models_bound},
        {"evaluators_supported", evaluators_supported},
        {"cases_executed", cases_executed},
        {"frozen_validation_bound", frozen_validation_bound},
        {"aggregate_emitted", aggregate_emitted},
        {"promotion_gate_emitted", promotion_gate_emitted},
        {"final_human_review", false}};
    std::map<std::string, std::string> stage_status;
    std::string next_allowed_stage;
    for (const ProcessStage& stage : process.stages)
    {
        bool dependencies_complete = true;
        for (const std::string& requirement : stage.requires)
            dependencies_complete = dependencies_complete && stage_status[requirement] == "COMPLETE";
        if (!dependencies_complete)
            stage_status[stage.stage_id] = "BLOCKED";
        else if (facts[stage.status_source])
            stage_status[stage.stage_id] = "COMPLETE";
        else if (stage.status_source == "final_human_review")
            stage_status[stage.stage_id] = "PENDING_HUMAN_REVIEW";
        else
            stage_status[stage.stage_id] = "PENDING_BINDING";
        if (next_allowed_stage.empty() && stage_status[stage.stage_id] != "COMPLETE" && dependencies_complete)
            next_allowed_stage = stage.stage_id;
    }

    std::ofstream file(path, std::ios::trunc);
    if (!file)
    {
        reason = "cannot write automatic diagnostic process status";
        return false;
    }
    file << "{\n  \"schema\": \"cxvision.automatic_diagnostic_process_status.v1\",\n"
         << "  \"process_id\": \"" << JsonEscape(process.process_id) << "\",\n"
         << "  \"next_allowed_stage\": \"" << JsonEscape(next_allowed_stage) << "\",\n"
         << "  \"stages\": [\n";
    for (std::size_t index = 0; index < process.stages.size(); ++index)
    {
        const ProcessStage& stage = process.stages[index];
        file << "    {\"index\": " << stage.index
             << ", \"stage_id\": \"" << JsonEscape(stage.stage_id)
             << "\", \"status_source\": \"" << JsonEscape(stage.status_source)
             << "\", \"status\": \"" << stage_status[stage.stage_id] << "\"}"
             << (index + 1 < process.stages.size() ? "," : "") << "\n";
    }
    file << "  ]\n}\n";
    return file.good();
}

bool WriteAggregateReports(
    const std::filesystem::path& output_dir,
    const std::vector<CompletedCase>& cases,
    const GatePolicy& policy,
    CxAutomaticDiagnosticClosureResult& result,
    std::string& reason)
{
    std::vector<double> parent_child;
    std::vector<double> parent_label;
    std::vector<double> child_label;
    std::vector<double> deltas;
    int regression_count = 0;
    for (const CompletedCase& item : cases)
    {
        parent_child.push_back(item.parent_child_iou);
        parent_label.push_back(item.parent_label_iou);
        child_label.push_back(item.child_label_iou);
        deltas.push_back(item.label_delta);
        if (item.label_delta < 0.0)
            ++regression_count;
    }
    const double mean_parent_child = Mean(parent_child);
    const double mean_parent_label = Mean(parent_label);
    const double mean_child_label = Mean(child_label);
    const double mean_delta = Mean(deltas);
    const double child_stddev = StandardDeviation(child_label, mean_child_label);

    result.aggregate_ref = output_dir / "frozen_validation_aggregate.json";
    std::ofstream aggregate(result.aggregate_ref, std::ios::trunc);
    if (!aggregate) { reason = "cannot write frozen validation aggregate"; return false; }
    aggregate << "{\n  \"schema\": \"cxvision.frozen_validation_aggregate.v1\",\n"
              << "  \"case_count\": " << cases.size() << ",\n"
              << "  \"mean_parent_child_iou\": " << mean_parent_child << ",\n"
              << "  \"mean_parent_label_iou\": " << mean_parent_label << ",\n"
              << "  \"mean_child_label_iou\": " << mean_child_label << ",\n"
              << "  \"mean_label_iou_delta\": " << mean_delta << ",\n"
              << "  \"regression_count\": " << regression_count << "\n}\n";
    if (!aggregate.good()) { reason = "failed to write frozen validation aggregate"; return false; }

    result.stability_ref = output_dir / "stability_analysis.json";
    std::ofstream stability(result.stability_ref, std::ios::trunc);
    if (!stability) { reason = "cannot write stability analysis"; return false; }
    stability << "{\n  \"schema\": \"cxvision.frozen_validation_stability.v1\",\n"
              << "  \"sample_count\": " << child_label.size() << ",\n"
              << "  \"child_label_iou_mean\": " << mean_child_label << ",\n"
              << "  \"child_label_iou_stddev\": " << child_stddev << ",\n"
              << "  \"stable\": " << (child_stddev <= policy.maximum_child_label_iou_stddev ? "true" : "false") << "\n}\n";
    if (!stability.good()) { reason = "failed to write stability analysis"; return false; }

    const bool candidate =
        static_cast<int>(cases.size()) >= policy.minimum_case_count &&
        regression_count <= policy.maximum_regression_count &&
        mean_child_label >= policy.minimum_child_label_iou &&
        mean_delta >= policy.minimum_mean_label_iou_delta &&
        child_stddev <= policy.maximum_child_label_iou_stddev;
    result.promotion_gate_ref = output_dir / "promotion_gate.json";
    std::ofstream gate(result.promotion_gate_ref, std::ios::trunc);
    if (!gate) { reason = "cannot write promotion gate"; return false; }
    gate << "{\n  \"schema\": \"cxvision.incremental_promotion_gate.v1\",\n"
         << "  \"automatic_checks_complete\": true,\n"
         << "  \"candidate_satisfies_thresholds\": " << (candidate ? "true" : "false") << ",\n"
         << "  \"promotion_allowed\": false,\n"
         << "  \"status\": \"" << (candidate ? "PENDING_HUMAN_REVIEW" : "FAIL") << "\",\n"
         << "  \"human_review_required\": true,\n"
         << "  \"reason\": \"" << (candidate
                ? "automatic frozen-set checks passed; final promotion requires human confirmation"
                : "automatic frozen-set thresholds were not satisfied") << "\"\n}\n";
    if (!gate.good()) { reason = "failed to write promotion gate"; return false; }

    result.status = candidate ? "PENDING_HUMAN_REVIEW" : "FAIL";
    result.reason = candidate
        ? "frozen validation and stability checks complete; final human review required"
        : "frozen validation promotion thresholds failed";
    return true;
}
} // namespace

bool RunCxAutomaticDiagnosticClosure(
    const CxAutomaticDiagnosticClosureOptions& options,
    CxAutomaticDiagnosticClosureResult& result,
    std::string& reason)
{
    result = {};
    result.executed = true;
    try
    {
        if (!std::filesystem::is_regular_file(options.matrix_path))
        {
            reason = "automatic diagnostic matrix is missing";
            result.status = "ASSET_PREFLIGHT_FAIL";
            result.reason = reason;
            return false;
        }
        if (options.output_dir.empty())
        {
            reason = "automatic diagnostic output directory is empty";
            result.status = "ASSET_PREFLIGHT_FAIL";
            result.reason = reason;
            return false;
        }
        std::filesystem::create_directories(options.output_dir);

        cv::FileStorage matrix(options.matrix_path.string(), cv::FileStorage::READ | cv::FileStorage::FORMAT_JSON);
        if (!matrix.isOpened() || !matrix["rows"].isSeq())
        {
            reason = "automatic diagnostic matrix is not valid JSON or has no rows";
            result.status = "ASSET_PREFLIGHT_FAIL";
            result.reason = reason;
            return false;
        }

        const cv::FileNode contract_assets = matrix["asset_contracts"];
        const std::filesystem::path taxonomy_path = ResolveAssetPath(
            options.matrix_path, NodeString(contract_assets, "case_taxonomy"));
        const std::filesystem::path annotation_path = ResolveAssetPath(
            options.matrix_path, NodeString(contract_assets, "annotation_contracts"));
        const std::filesystem::path model_path = ResolveAssetPath(
            options.matrix_path, NodeString(contract_assets, "model_tracks"));
        const std::filesystem::path evaluator_path = ResolveAssetPath(
            options.matrix_path, NodeString(contract_assets, "evaluator_contracts"));
        const std::filesystem::path composite_path = ResolveAssetPath(
            options.matrix_path, NodeString(contract_assets, "composite_pipeline"));
        const std::filesystem::path process_path = ResolveAssetPath(
            options.matrix_path, NodeString(contract_assets, "closure_process"));
        const std::filesystem::path single_case_contract_path = ResolveAssetPath(
            options.matrix_path, NodeString(contract_assets, "single_image_case_contract"));
        const std::filesystem::path runtime_contract_path = ResolveAssetPath(
            options.matrix_path, NodeString(contract_assets, "runtime_binding_contract"));
        const std::filesystem::path frozen_contract_path = ResolveAssetPath(
            options.matrix_path, NodeString(contract_assets, "frozen_validation_contract"));
        ContractRegistry contracts;
        ProcessDefinition process;
        if (!std::filesystem::is_regular_file(composite_path) ||
            !std::filesystem::is_regular_file(single_case_contract_path) ||
            !std::filesystem::is_regular_file(runtime_contract_path) ||
            !std::filesystem::is_regular_file(frozen_contract_path) ||
            !LoadContractRegistry(taxonomy_path, annotation_path, model_path, evaluator_path, contracts, reason) ||
            !LoadProcessDefinition(process_path, process, reason))
        {
            if (reason.empty())
                reason = "automatic diagnostic composite pipeline contract is missing";
            result.status = "ASSET_PREFLIGHT_FAIL";
            result.reason = reason;
            return false;
        }

        GatePolicy policy;
        const cv::FileNode gate = matrix["promotion_gate"];
        if (!gate.empty())
        {
            policy.minimum_case_count = NodeInt(gate, "minimum_case_count", policy.minimum_case_count);
            policy.maximum_regression_count = NodeInt(gate, "maximum_regression_count", policy.maximum_regression_count);
            policy.minimum_child_label_iou = NodeDouble(gate, "minimum_child_label_iou", policy.minimum_child_label_iou);
            policy.minimum_mean_label_iou_delta = NodeDouble(gate, "minimum_mean_label_iou_delta", policy.minimum_mean_label_iou_delta);
            policy.maximum_child_label_iou_stddev = NodeDouble(gate, "maximum_child_label_iou_stddev", policy.maximum_child_label_iou_stddev);
        }

        std::vector<ClosureRow> rows;
        for (const cv::FileNode& node : matrix["rows"])
        {
            ClosureRow row;
            row.row_id = NodeString(node, "matrix_row");
            row.review_item = NodeString(node, "review_item");
            row.implementation_phase = NodeInt(node, "implementation_phase", 0);
            row.case_track = NodeString(node, "case_track");
            row.annotation_contract_id = NodeString(node, "annotation_contract_id");
            row.model_track_id = NodeString(node, "model_track_id");
            row.evaluator_id = NodeString(node, "evaluator_id");
            row.source_manifest = ResolveAssetPath(options.matrix_path, NodeString(node, "source_manifest"));
            row.source_set_id = NodeString(node, "source_set_id");
            const auto assignment = contracts.case_assignments.find(row.source_set_id);
            row.taxonomy_valid = contracts.case_tracks.count(row.case_track) != 0 &&
                assignment != contracts.case_assignments.end() && assignment->second == row.case_track;
            const auto annotation = contracts.annotation_tracks.find(row.annotation_contract_id);
            row.annotation_contract_valid = annotation != contracts.annotation_tracks.end() &&
                annotation->second == row.case_track;
            const auto model = contracts.model_tracks.find(row.model_track_id);
            row.model_track_valid = model != contracts.model_tracks.end() &&
                model->second.count(row.case_track) != 0;
            const auto evaluator = contracts.evaluator_tracks.find(row.evaluator_id);
            row.evaluator_contract_valid = evaluator != contracts.evaluator_tracks.end() &&
                evaluator->second.count(row.case_track) != 0;
            const auto evaluator_status = contracts.evaluator_status.find(row.evaluator_id);
            row.evaluator_runtime_status = evaluator_status == contracts.evaluator_status.end()
                ? "missing" : evaluator_status->second;
            row.evaluator_runtime_supported = row.evaluator_runtime_status == "implemented";
            row.parent = ParseBinding(node["parent_binding"]);
            row.child = ParseBinding(node["child_binding"]);
            row.parent.manifest_path = ResolveAssetPath(options.matrix_path, row.parent.manifest_path);
            row.child.manifest_path = ResolveAssetPath(options.matrix_path, row.child.manifest_path);
            row.parent.bound = row.parent.status == "bound" &&
                !row.parent.task_id.empty() && !row.parent.model_id.empty() &&
                std::filesystem::is_regular_file(row.parent.manifest_path);
            row.child.bound = row.child.status == "bound" &&
                !row.child.task_id.empty() && !row.child.model_id.empty() &&
                std::filesystem::is_regular_file(row.child.manifest_path);

            const cv::FileNode label = node["dataset_label_mask_binding"];
            if (label.isString())
                row.label_binding_status = static_cast<std::string>(label);
            else if (!label.empty())
            {
                row.label_binding_status = NodeString(label, "status");
                row.label_mask_path = ResolveAssetPath(options.matrix_path, NodeString(label, "mask_path"));
            }
            else
                row.label_binding_status = "missing";

            const cv::FileNode candidate = node["cximage_candidate"];
            row.candidate.algorithm_id = NodeString(candidate, "algorithm_id");
            row.candidate.threshold = NodeDouble(candidate, "threshold", 0.5);
            const cv::FileNode roi = candidate["roi_xyxy"];
            if (roi.isSeq() && roi.size() == 4)
            {
                row.candidate.has_roi = true;
                row.candidate.roi_x0 = static_cast<int>(roi[0]);
                row.candidate.roi_y0 = static_cast<int>(roi[1]);
                row.candidate.roi_x1 = static_cast<int>(roi[2]);
                row.candidate.roi_y1 = static_cast<int>(roi[3]);
            }

            const cv::FileNode samples = node["frozen_validation_set"];
            row.explicit_frozen_validation = samples.isSeq() && !samples.empty();
            if (samples.isSeq())
            {
                for (const cv::FileNode& sample_node : samples)
                {
                    ClosureSample sample;
                    sample.case_id = NodeString(sample_node, "case_id");
                    sample.image_path = ResolveAssetPath(options.matrix_path, NodeString(sample_node, "image_path"));
                    sample.label_mask_path = ResolveAssetPath(options.matrix_path, NodeString(sample_node, "label_mask_path"));
                    row.samples.push_back(sample);
                }
            }
            if (row.samples.empty())
            {
                ClosureSample sample;
                sample.case_id = row.row_id;
                sample.image_path = ResolveFirstManifestImage(row.source_manifest, row.source_set_id);
                sample.label_mask_path = row.label_mask_path;
                row.samples.push_back(sample);
            }
            rows.push_back(row);
        }

        result.discovered_rows = static_cast<int>(rows.size());
        int invalid_contract_rows = 0;
        for (const ClosureRow& row : rows)
        {
            const bool contracts_valid = row.taxonomy_valid && row.annotation_contract_valid &&
                row.model_track_valid && row.evaluator_contract_valid;
            if (!contracts_valid)
                ++invalid_contract_rows;
            bool all_samples_bound = !row.samples.empty();
            for (const ClosureSample& sample : row.samples)
            {
                all_samples_bound = all_samples_bound &&
                    std::filesystem::is_regular_file(sample.image_path) &&
                    std::filesystem::is_regular_file(sample.label_mask_path);
            }
            if (contracts_valid && row.evaluator_runtime_supported &&
                row.parent.bound && row.child.bound &&
                row.label_binding_status == "bound" && all_samples_bound)
                ++result.bound_rows;
        }

        result.preflight_ref = options.output_dir / "binding_preflight.json";
        result.process_status_ref = options.output_dir / "closure_process_status.json";
        if (invalid_contract_rows != 0)
        {
            result.status = "ASSET_PREFLIGHT_FAIL";
            result.reason = "one or more matrix rows violate taxonomy, annotation, model, or evaluator contracts";
            if (!WritePreflight(result.preflight_ref, rows, result, reason))
                return false;
            if (!WriteProcessStatus(result.process_status_ref, process, rows, result, reason))
                return false;
            reason = result.reason;
            return false;
        }
        if (result.bound_rows != result.discovered_rows || result.discovered_rows == 0)
        {
            result.status = "PENDING_BINDING";
            result.reason = "typed evaluator runtime, parent, child, dataset label, or frozen validation asset binding is incomplete";
            if (!WritePreflight(result.preflight_ref, rows, result, reason))
                return false;
            if (!WriteProcessStatus(result.process_status_ref, process, rows, result, reason))
                return false;
            reason.clear();
            return true;
        }

        result.status = "ASSET_PREFLIGHT_PASS";
        result.reason = "all automatic diagnostic bindings are complete";
        if (!WritePreflight(result.preflight_ref, rows, result, reason))
            return false;
        if (!WriteProcessStatus(result.process_status_ref, process, rows, result, reason))
            return false;

        std::vector<CompletedCase> completed;
        CxTorchExecutionAdapter adapter;
        for (const ClosureRow& row : rows)
        {
            for (const ClosureSample& sample : row.samples)
            {
                const std::filesystem::path case_dir = options.output_dir / "cases" / row.row_id / sample.case_id;
                CxPairedInferenceRequest request;
                request.parent_task = MakeTask(row.parent, sample, case_dir / "parent");
                request.child_task = MakeTask(row.child, sample, case_dir / "child");
                request.dataset_label_mask_path = sample.label_mask_path;
                request.report_path = case_dir / "paired_inference_diagnostic.json";
                request.cximage_candidate_request = row.candidate;
                request.cximage_candidate_request->input_image_path = sample.image_path;
                request.cximage_candidate_request->output_dir = case_dir / "cximage";

                CxPairedInferenceDiagnostic diagnostic;
                ++result.executed_cases;
                if (!adapter.ExecutePair(request, diagnostic, reason) || !diagnostic.complete ||
                    !diagnostic.parent_label.has_value() || !diagnostic.child_label.has_value() ||
                    !diagnostic.label_cximage.has_value())
                {
                    ++result.rejected_cases;
                    result.status = diagnostic.status.empty() ? "FAIL" : diagnostic.status;
                    result.reason = reason.empty() ? diagnostic.reason : reason;
                    return false;
                }
                CompletedCase item;
                item.row_id = row.row_id;
                item.case_id = sample.case_id;
                item.parent_child_iou = diagnostic.parent_child.iou;
                item.parent_label_iou = diagnostic.parent_label->iou;
                item.child_label_iou = diagnostic.child_label->iou;
                item.label_delta = item.child_label_iou - item.parent_label_iou;
                completed.push_back(item);
                ++result.completed_cases;
            }
        }

        if (!WriteAggregateReports(options.output_dir, completed, policy, result, reason))
            return false;
        if (!WriteProcessStatus(result.process_status_ref, process, rows, result, reason))
            return false;
        result.complete = true;
        reason.clear();
        return true;
    }
    catch (const cv::Exception& error)
    {
        reason = error.what();
    }
    catch (const std::exception& error)
    {
        reason = error.what();
    }
    result.status = "FAIL";
    result.reason = reason;
    return false;
}
