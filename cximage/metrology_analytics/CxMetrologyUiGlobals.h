#ifndef CXIMAGE_METROLOGY_ANALYTICS_CXMETROLOGYUIGLOBALS_H
#define CXIMAGE_METROLOGY_ANALYTICS_CXMETROLOGYUIGLOBALS_H

#include <filesystem>
#include <string>
#include <vector>

namespace cxvision::metrology_analytics
{

struct CxMetrologyUiGlobalPair
{
    std::string name;
    int value = 0;
};

struct CxMetrologyUiGlobalFields
{
    bool enabled = false;
    int active_tab = 0;

    bool show_scan_profile = false;
    int scan_profile_source = 0;
    int scan_profile_max_lines = 256;
    int scan_profile_sample_stride = 1;
    int scan_profile_edge_band_index = 0;
    int scan_profile_smoothing_radius = 1;

    bool show_edge_band_candidates = false;
    int candidate_rank = 0;
    int candidate_min_gradient = 8;
    int candidate_max_width = 80;
    int feature_map_mode = 0;
    int feature_map_normalize = 1;

    int surface_source = 0;
    int surface_width = 256;
    int surface_height = 256;
    int surface_stride = 1;
    int surface_z_channel = 0;
    int surface_area_method = 1;
    int histogram_bins = 256;
    int histogram_mode = 0;
    bool histogram_log_scale = false;

    bool enable_plane_correction = false;
    int plane_method = 1;
    int plane_reference_mode = 0;
    int plane_huber_delta_permille = 100;

    int x_unit = 2;
    int y_unit = 2;
    int z_unit = 2;
    int x_scale_permille = 1000;
    int y_scale_permille = 1000;
    int z_scale_permille = 1000;
    bool enable_gaussian_z = false;
    int gaussian_z_sigma_permille = 0;
    int gaussian_seed = 42;

    bool enable_iso_roughness_1d = false;
    int roughness_profile_axis = 0;
    int roughness_profile_index = 0;
    int roughness_cutoff_px = 0;
    int roughness_bins = 1024;
};

CxMetrologyUiGlobalFields DefaultMetrologyUiGlobalFields();

std::vector<CxMetrologyUiGlobalPair> BuildMetrologyUiGlobalSnapshot(
    const CxMetrologyUiGlobalFields& fields);

std::string BuildMetrologyUiGlobalSummary(
    const CxMetrologyUiGlobalFields& fields);

bool WriteMetrologyUiGlobalSnapshotJson(
    const std::filesystem::path& path,
    const CxMetrologyUiGlobalFields& fields,
    std::string& reason);

} // namespace cxvision::metrology_analytics

#endif
