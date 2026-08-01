#ifndef _findline_Header
#define _findline_Header
#include "Shape.h"
#include "Image.h"
#include "shapebase.h"
#include "FindObject.h"
#include "CxImageRuntimeOverlay.h"
#include "CxAlgorithmBudget.h"
#include <string>
#include <map>
#include <array>
#include <cstdint>
#include <chrono>
#include <vector>


class FindObject;

struct FindLineDisplaySnapshot
{
    bool has_line_roi = false;

    float x0 = 0.0f;
    float y0 = 0.0f;
    float x1 = 0.0f;
    float y1 = 0.0f;
    float scale = 1.0f;

    int wgap = 0;
    int hgap = 0;
    int linegap = 0;

    float scan_half_width = 0.0f;

    bool has_scan_box = false;

    std::array<float, 8> scan_box_xy = {
        0.0f, 0.0f,
        0.0f, 0.0f,
        0.0f, 0.0f,
        0.0f, 0.0f
    };

    std::string source;
};

struct FindLineMeasureGeometryRequest
{
    bool valid = false;

    double x0 = 0.0;
    double y0 = 0.0;
    double x1 = 0.0;
    double y1 = 0.0;

    double script_scale = 1.0;

    double measure_half_width = 1.0;

    int wgap = 0;
    int hgap = 0;
    int linegap = 0;

    std::uint64_t version = 0;
};

struct EdgeBandCandidate
{
    int scan_index = -1;
    int scan_type = 0; // 0=w, 1=h
    int line_index = -1;
    int candidate_index = -1;
    int start_index = -1;
    int end_index = -1;
    int center_index = -1;
    double x = 0.0;
    double y = 0.0;
    double response_strength = 0.0;
    double polarity = 0.0;
    double width = 0.0;
    int edge_rank = -1;
    bool valid = false;
    std::vector<double> profile;
};

struct FitCandidateSequence
{
    std::vector<cv::Point2d> points;
    double score = 0.0;
    int node_count = 0;
    double avg_ncc = 0.0;
    double total_response = 0.0;
    bool valid = false;
};

struct FeatureGraphNode
{
    int id = -1;
    EdgeBandCandidate candidate;
    std::vector<int> neighbors;
    bool visited = false;
    int component_id = -1;
};

struct FeatureGraphEdge
{
    int node_a = -1;
    int node_b = -1;
    double ncc_score = 0.0;
    double spatial_distance = 0.0;
    bool valid = false;
};

struct FeatureGraph
{
    std::vector<FeatureGraphNode> nodes;
    std::vector<FeatureGraphEdge> edges;
    int next_component_id = 0;
};

struct ScanLineEdgeBands
{
    int scan_index = -1;
    int scan_type = 0; // 0=w, 1=h
    std::vector<EdgeBandCandidate> bands;
};

struct FindLineMeasureProfileStats
{
    double total_ms = 0.0;
    double profile_ms = 0.0;
    double edgeband_ms = 0.0;
    double graph_ms = 0.0;
    double path_ms = 0.0;
    double subpixel_ms = 0.0;
    double fit_ms = 0.0;
    double joint_refine_ms = 0.0;
    double fit_error_avg = 0.0;
    double fit_error_max = 0.0;
    double line_angle = 0.0;
    double line_offset = 0.0;
    double subpixel_adjust_avg = 0.0;
    int point_count = 0;
    int chain_length = 0;
    int edgeband_count = 0;
    int chain_switch_count = 0;
    int neighbor_inconsistency_count = 0;
};

struct FindLineMeasureInputDebug
{
    struct EdgeEvaluation
    {
        int edge_index = 0;
        int candidate_scan_rows = 0;
        int accepted_points = 0;
        int rejected_by_selection = 0;
        int rejected_near_endpoint = 0;
        int over_length_runs = 0;
        double coverage = 0.0;
        double score = 0.0;
        bool selected = false;
        bool fit_possible = false;
    };

    struct ScanDiagnostic
    {
        int scan_index = -1;
        int scan_type = 0; // 0=w, 1=h
        int candidate_count = 0;
        bool accepted = false;
        double accepted_x = 0.0;
        double accepted_y = 0.0;
        std::string reject_reason;
    };

    bool image_ptr_valid = false;
    bool image_mat_ready = false;

    int image_width = 0;
    int image_height = 0;
    int image_channels = 0;
    int image_type = 0;

    bool has_line_roi = false;
    double roi_x0 = 0.0;
    double roi_y0 = 0.0;
    double roi_x1 = 0.0;
    double roi_y1 = 0.0;
    double roi_scan_half_width = 0.0;

    std::string line_orientation;
    double line_dx = 0.0;
    double line_dy = 0.0;
    double line_length = 0.0;
    double requested_tool_half_width = 0.0;
    double effective_tool_half_width = 0.0;

    bool roi_intersects_image = false;
    bool roi_fully_inside_image = false;

    int method = 0;
    int threshold = 0;
    int linegap = 0;
    int wgap = 0;
    int hgap = 0;

    int profile_count = 0;
    int sampled_pixel_count = 0;

    double gray_min = 0.0;
    double gray_max = 0.0;
    double gray_mean = 0.0;
    double max_gradient = 0.0;

    std::string image_source;
    std::string failure_stage;
    std::string detail;

    bool fallback_allowed = false;
    bool fallback_used = false;

    std::string measure_source;
    std::string original_failure_stage;
    std::string original_detail;

    int original_point_count = 0;
    int original_edgeband_count = 0;
    int original_chain_length = 0;

    bool measure_geometry_request_valid = false;
    bool measure_geometry_dirty = false;
    bool measure_geometry_ready = false;

    std::uint64_t measure_geometry_version = 0;
    std::uint64_t measure_geometry_built_version = 0;

    double measure_geometry_half_width = 0.0;

    int original_scan_w_count = 0;
    int original_scan_h_count = 0;
    int original_scan_w_length = 0;
    int original_scan_h_length = 0;
    int original_process_width = 0;

    // Original Measure() scan-run extraction evidence.  These counters
    // deliberately describe the legacy scan loop as executed; they do not
    // change its threshold, polarity, selection or fitting behaviour.
    int scan_rows_examined = 0;
    int scan_rows_with_foreground = 0;
    int scan_runs_total = 0;
    int scan_runs_within_length_limit = 0;
    int scan_runs_over_length_limit = 0;
    int scan_runs_rejected_by_selection = 0;
    int scan_runs_rejected_near_endpoint = 0;
    int scan_points_emitted = 0;
    int selected_edge_index = 0;
    int evaluated_edge_count = 0;
    int best_edge_index = 0;
    double best_edge_score = 0.0;
    std::vector<EdgeEvaluation> edge_evaluations;
    std::vector<ScanDiagnostic> scan_diagnostics;

    bool backimage_ready = false;
    bool findobject_ready = false;

    int objfilterset = 0;
    int filter_borw = 0;
    int filter_min = 0;
    int filter_max = 0;

    int filter_profile = 0;
    bool filter_explicit = false;

    int effective_filter_borw = 0;
    int effective_filter_min = 0;
    int effective_filter_max = 0;

    bool findobject_measure_called = false;
    bool findobject_measure_skipped = false;
    int findobject_strategy_id = 0;
    std::string findobject_algorithm_branch;

    int binary_foreground_pixels = 0;
    int binary_roi_width = 0;
    int binary_roi_height = 0;
    int findobject_foreground_before = 0;
    int findobject_foreground_after = 0;

    std::string result_empty_reason;

    int findobject_component_total = 0;
    int findobject_component_accepted = 0;
    int findobject_component_rejected_by_min = 0;
    int findobject_component_rejected_by_max = 0;
    int findobject_component_rejected_by_borw = 0;

    int findobject_area_min_observed = 0;
    int findobject_area_max_observed = 0;
    double findobject_area_mean_observed = 0.0;
    double findobject_area_median_observed = 0.0;
    double findobject_area_p90_observed = 0.0;

    std::vector<int> findobject_top_component_areas;
    std::vector<double> findobject_top_component_x;
    std::vector<double> findobject_top_component_y;

    struct ComponentStats
    {
        int component_total = 0;
        int accepted_by_area = 0;
        int rejected_by_min = 0;
        int rejected_by_max = 0;

        int area_min = 0;
        int area_max = 0;
        double area_mean = 0.0;
        double area_median = 0.0;
        double area_p90 = 0.0;

        std::vector<int> top_areas;
    };

    ComponentStats cc_white;
    ComponentStats cc_black;
    ComponentStats cc_selected;

    std::string cc_selected_foreground;
};

class ICxShapeSink;

class FindLine:public Shape
{
public:
    enum class FitlineMode
    {
        Unspecified = 0, LeastSquares = 1, MinimumZone = 2, Ransac = 3,
        SingleEdge = 4, EdgePairCenter = 5,
        HorizontalVerticalPriority = 6, WeightedMeasurementPoints = 7
    };

    FindLine();
    ~FindLine();

    void PublishDisplayShapes(ICxShapeSink& sink, const std::string& owner_ref) const;
    int wgap() { return m_iwgap; }
    int hgap() { return m_ihgap; }
    int thre();
    int linegap() { return m_iSelectPointGap; }
    int objfilter() const { return m_iobjfilterset; }

    void clear();
    void setshow(int ishow);
    void getshape(void* pshape);
    void setlinesegment(double ix0, double iy0, double ix1, double iy1, double iscale);
    void setline(int ix0, int iy0, int ix1, int iy1, int iscale)
    {
        setlinesegment(ix0, iy0, ix1, iy1, iscale);
    }
    void setline_script(int iscale, int iy1, int ix1, int iy0, int ix0)
    {
        setline(ix0, iy0, ix1, iy1, iscale);
    }
    virtual void setrect(int ix, int iy, int iw, int ih);
    virtual void drawshape();
    void drawshapex( double dmovx, double dmovy,
        double dangle, double dzoomx, double dzoomy);

    void drawpattern();
    void drawpatternx(double dmovx, double dmovy,
        double dangle,
        double dzoomx, double dzoomy);
    void edgepattern(Image& image);
    void setcomparegap(int igap);
    void patternzeroposition();
    void savepatternfile(const char* pchar);
    void loadpatternfile(const char* pchar);


    void ABtoShape(std::vector<cv::Point2f>& points);

    void saveABpatternfile(const char* pchar);
    void loadABpatternfile(const char* pchar);
    int ABpatternsize();
    int getlearnacount();
    int getlearnbcount();
    int getlearna2count();
    int getlearnb2count();
    void samplemodelAB(int inum);

    gp_Rectangle patternboundingrect();
    gp_Rectangle patternboundingrectAB();
    gp_Rectangle patternboundingrectA();
    gp_Rectangle patternboundingrectB();
    void patternrootgrid(double itype, double drate, double ilevel);
    void patternzoom(double dx, double dy, double igap, double itype);
    void patterntranform(int igap, int itype, int isgap, int iline);
    void patternrotate(double dangle);
    void modelzoom(double dx, double dy);
    void patterngap2gap(int inewgap);
    void patternABgap2gap(double dnewgaprate);
    void patternABsample(int irate);
    void pattern2org();
    void org2pattern();

    gp_Path& getpatternpath();
    gp_Path& getpatternpathA();
    gp_Path& getpatternpathB();

    void patternfilter(double distanceThreshold = 1.0, double waveletThreshold = 0.5);

    PointsShape& getpattern();
    void setpattern(PointsShape& apattern) { m_modelpoints = apattern; }

    void SetWHgap(int wgap = 2, int hgap = 2);
    // CxScript class callbacks receive numeric arguments in parser-stack
    // order.  Keep the script spelling SetWHgap(wgap, hgap) stable while
    // adapting that order at the binding boundary; native callers continue
    // to use SetWHgap(wgap, hgap) directly.
    void setwhgap_script(int hgap, int wgap)
    {
        SetWHgap(wgap, hgap);
    }

    void measure(void* pimage);
    void measureRobust(void* pimage);
    void pyrimage(void* pimage);


    void findpattern(void* pimage);

    void setlinesamplerate(double dsamplerate);
    void setlinegap(int igap);
    void setmethod(int imethod);
    void setthre(int ithre);
    void setgamarate(int igama);
    void setobjfilter(int ifindset);
    void setfilter(int ifilterborw, int ifiltermin, int ifiltermax);//21 w ,22 b
    void setfilterprofile(int profile);
    void setobjectfilterstrategy(int strategy);
    int objectfilterstrategy() const { return m_findobject_strategy_id; }
    int effectivefiltermin() const;
    int effectivefiltermax() const;
    int effectivefilterborw() const;
    void setselectedgenum(int iedgenum);

    void setshowlines(int ilines) { m_ishowlines = ilines; }
    PointsShape& getresultpointsw();
    PointsShape& getresultpointsh();
    const PointsShape& getresultpointsw() const;
    const PointsShape& getresultpointsh() const;
    void Measure(Image& image);
    void MeasureBalanced(Image& image);
    void MeasureRobust(Image& image);
    void PyrImage(Image& image);

    void SmartFilter(double dist,double filtnum);

    void shapesetroi(void* pshape);
    void MeasureT(void* pimage);

    int getconparegap() { return m_icomparegap; }

    void translate(int ix, int iy);
    void Translate(const gp_Vec& translationVector);

    void fitline();
    void fitline(FitlineMode mode);
    void FitLine() { fitline(); }
    void setfitmode(int mode);
    int getfitmodevalue() const { return static_cast<int>(m_fitline_mode); }
    const std::string& getfitstatus() const { return m_fitline_status; }
    void clearfitresult();
    void setfitpointweight(int index, double weight);
    void clearfitpointweights() { m_fit_point_weights.clear(); }
    double getresultx0() const { return m_result_x0; }
    double getresulty0() const { return m_result_y0; }
    double getresultx1() const { return m_result_x1; }
    double getresulty1() const { return m_result_y1; }
    double getavgdist() const { return m_result_avgdist; }
    int getvalidpointcount() const { return m_result_valid_points; }
    bool hasfitresult() const { return m_has_fit_result; }
    bool getdisplaysnapshot(FindLineDisplaySnapshot& out) const;
    void exportmeasuredebugpoints(std::vector<float>& outXY) const;
    int getscandiagnosticcount() const;
    bool getscandiagnostic(
        int index,
        FindLineMeasureInputDebug::ScanDiagnostic& out) const;
    bool getscandiagnosticline(
        int scan_type,
        int scan_index,
        CxShapePoint& p0,
        CxShapePoint& p1) const;
    int getscanlinecount(int scan_type) const;
    bool getscanline(
        int scan_type,
        int scan_index,
        CxShapePoint& p0,
        CxShapePoint& p1) const;
    const FindLineMeasureInputDebug& lastmeasureinputdebug() const
    {
        return m_lastMeasureInputDebug;
    }
    void setmeasurefallback(int mode);
    int getmeasurefallback() const
    {
        return m_measure_fallback_mode;
    }

    void setmaxelapsedms(int value);
    void setmaxscanlines(int value);
    void setmaxsamples(int value);

    bool budgetexceeded() const;
    int getelapsedms() const;
    int getscanlinecount() const;
    int getsamplecount() const;
    const std::string& getfailurestage() const;

    double get_result() { return m_has_fit_result ? 1.0 : 0.0; }
    double get_result_script() { return get_result(); }
    double getvalidpointcount_script() { return static_cast<double>(getvalidpointcount()); }
    double hasfitresult_script() { return hasfitresult() ? 1.0 : 0.0; }
    double getavgdist_script() { return getavgdist(); }
private:
    int m_icomparegap;
    PointsShape m_modelpoints;    //red(white 1) gap blue(black 0) model
    PointsShape m_modelpoints_org;

    bool m_has_display_line_roi = false;
    double m_display_line_x0 = 0.0;
    double m_display_line_y0 = 0.0;
    double m_display_line_x1 = 0.0;
    double m_display_line_y1 = 0.0;
    double m_display_line_scale = 1.0;

    vector<PointsShape>  m_modelsegments;

    PointsShape m_modelpoints_level0;    //5pyrDown   thre >50
    PointsShape m_modelpoints_level1;    //2pyrDown   thre >30
    PointsShape m_modelpoints_level2;    //1pyrDown   thre >10

    PointsShape m_measurepointsA;
    PointsShape m_measurepointsB;

    PointsShape m_measurepointsA_;
    PointsShape m_measurepointsB_;

    Image* g_pbackimage;
    FindObject* g_pbackfindobject;

    Image* g_pyrimage0;
    Image* g_pyrimage1;
    Image* g_pyrimage2;

    PointsShape m_measurepoints_w;
    LineMeasurePoints m_l_measure_w_seek;

    PointsShape m_measurepoints_h;
    LineMeasurePoints m_l_measure_h_seek;



    LineShape m_LineA;
    LineShape m_LineB;

    LineVector m_lines_w;
    LineVector m_lines_h;

    int m_ihgap;
    int m_iwgap;

    gp_Pnt* m_listscanorA;
    gp_Pnt* m_listcollectorA;

    int m_iSelectPointGap;
    int m_iMethod;
    int m_iThreshold;
    int m_igamarate;
    double m_dsamplerate;

    int m_iobjfilterset;
    int m_ifilterborw;//21 w ,22 b ,23
    int64 m_ifiltermax;
    int64 m_ifiltermin;

    int m_filter_profile = 0;
    bool m_filter_explicit = false;
    int m_findobject_strategy_id = 0;
    int m_effective_filter_borw = 0;
    int m_effective_filter_min = 0;
    int m_effective_filter_max = 0;

    int m_iselectedgenum;
    int m_ineedfixs;

    int m_ncurscan;
    int m_nscansize;

    int resultsize();

    static int m_curfindlinenum;

    int m_ishowlines;
    gp_Rectangle m_measurepointsboundingRect;
public:
    void easycluster(int igapx = 10, int igapy = 10, int iclusternum = 5);
    gp_Rectangle measurepointsboundingrect() { return m_measurepointsboundingRect; }
    void SeekPoints(PointsShape& seekpoints);
    void InflectionPoint(void* points);
    const FindLineMeasureProfileStats& lastmeasureprofilestats() const { return m_lastMeasureProfile; }
private:
    void ClearMeasureState();
    void BuildScanProfiles(Image& image, FindLineMeasureProfileStats& stats, const std::chrono::steady_clock::time_point& total_begin);
    void CollectAllEdgeBands(Image& image, FindLineMeasureProfileStats& stats);
    void BuildEdgeBandGraph(FindLineMeasureProfileStats& stats);
    void SolveBestEdgeChain(FindLineMeasureProfileStats& stats);
    void ConvertBestChainToMeasurePoints(FindLineMeasureProfileStats& stats);
    void RefineBestChainSubpixel(Image& image, FindLineMeasureProfileStats& stats);
    void FilterMeasurePoints(FindLineMeasureProfileStats& stats);
    void FitWeightedLeastSquares(FindLineMeasureProfileStats& stats);
    void RefineJointConsistency(FindLineMeasureProfileStats& stats);
    void ProbeDisplayRoiGrayStats(Image& image);
    bool MeasureSimpleRoiGradientPoints(Image& image,
                                        FindLineMeasureProfileStats& stats);
    void RunFindObjectPrefilter(Image& process_image);

    void BuildScanProfilesRobust(Image& image, FindLineMeasureProfileStats& stats);
    void CollectEdgeBandsRobust(Image& image, FindLineMeasureProfileStats& stats);
    void BuildFeatureGraph(FindLineMeasureProfileStats& stats);
    void FindComponentsInGraph(FindLineMeasureProfileStats& stats);
    void SelectBestSequence(FindLineMeasureProfileStats& stats);
    void ConvertSequenceToMeasurePoints(FitCandidateSequence& seq);

    void MarkMeasureGeometryDirty();

    double ComputeMeasureHalfWidthForLine(double x0,
                                          double y0,
                                          double x1,
                                          double y1) const;

    void UpdateMeasureGeometryRequest(double x0,
                                      double y0,
                                      double x1,
                                      double y1,
                                      double scriptScale);

    bool EnsureOriginalMeasureGeometryReady();

    bool BuildOriginalMeasureGeometryFromRequest(
        const FindLineMeasureGeometryRequest& request);

    void BuildOriginalMeasureGeometryCore(double ix0,
                                          double iy0,
                                          double ix1,
                                          double iy1,
                                          double measureHalfWidth);

    bool HasOriginalMeasureScanGeometry() const;

    void SyncMeasureGeometryCacheAfterNativeBuild(double nativeHalfWidth);

    void InvalidateMeasureAndFitAfterParamChange(
        const char* reason);

private:
    std::vector<ScanLineEdgeBands> m_scanEdgeBands;
    std::vector<EdgeBandCandidate> m_bestEdgeChain;
    FindLineMeasureProfileStats m_lastMeasureProfile;
    FindLineMeasureInputDebug m_lastMeasureInputDebug;
    int m_measure_fallback_mode = 0;

    std::vector<FitCandidateSequence> m_fit_candidate_sequences;
    int m_best_sequence_index = -1;

    FindLineMeasureGeometryRequest m_measure_geometry_request;

    bool m_measure_geometry_dirty = true;
    bool m_measure_geometry_ready = false;

    std::uint64_t m_measure_geometry_version = 0;
    std::uint64_t m_measure_geometry_built_version = 0;

    double m_result_x0 = 0.0;
    double m_result_y0 = 0.0;
    double m_result_x1 = 0.0;
    double m_result_y1 = 0.0;
    double m_result_avgdist = 0.0;
    int m_result_valid_points = 0;
    bool m_has_fit_result = false;
    FitlineMode m_fitline_mode = FitlineMode::LeastSquares;
    std::string m_fitline_status = "not_executed";
    std::vector<double> m_fit_point_weights;

    CxAlgorithmBudget m_budget;
    CxAlgorithmBudgetState m_budget_state;
};

#endif //_findline_Header
