#ifndef _findcircle_Header
#define _findcircle_Header
#include "Shape.h"
#include "Image.h"
#include "shapebase.h"
#include "CxAlgorithmBudget.h"
#include <cstdint>
#include <string>
class FindObject;

struct FindCircleMeasureGeometryRequest
{
    bool valid = false;

    int center_x = 0;
    int center_y = 0;

    int pass_x = 0;
    int pass_y = 0;

    bool has_inner_gap = false;
    int inner_gap = 0;

    int gap_degrees = 0;

    int linegap = 0;

    double sample_rate = 1.0;

    std::uint64_t version = 0;
};

struct FindCircleMeasureGeometryDebug
{
    bool request_valid = false;
    bool geometry_dirty = false;
    bool geometry_ready = false;

    std::uint64_t geometry_version = 0;
    std::uint64_t geometry_built_version = 0;

    int center_x = 0;
    int center_y = 0;
    int pass_x = 0;
    int pass_y = 0;
    bool has_inner_gap = false;
    int inner_gap = 0;

    int gap_degrees = 0;
    int linegap = 0;

    int scan_line_count = 0;
    int scan_line_length = 0;
    int process_width = 0;

    bool image_ready = false;
    int image_width = 0;
    int image_height = 0;
    int image_channels = 0;

    bool backimage_ready = false;
    bool findobject_ready = false;

    int measure_points_count = 0;
    int valid_points_count = 0;

    std::string measure_source;
    std::string failure_stage;
    std::string detail;

    int scan_lines_processed = 0;
    int total_samples = 0;
    int elapsed_ms = 0;

    int budget_max_scan_lines = 2048;
    int budget_max_samples = 2000000;
    int budget_max_elapsed_ms = 3000;
};

struct CircleEdgeBandCandidate
{
    int scan_index = -1;
    int candidate_index = -1;
    int start_angle = 0;
    int end_angle = 0;
    int center_angle = 0;
    double x = 0.0;
    double y = 0.0;
    double response_strength = 0.0;
    double polarity = 0.0;
    double arc_length = 0.0;
    int edge_rank = -1;
    bool valid = false;
    std::vector<double> profile;
};

struct CircleFitCandidateSequence
{
    std::vector<cv::Point2d> points;
    double score = 0.0;
    int node_count = 0;
    double avg_ncc = 0.0;
    double total_response = 0.0;
    bool valid = false;
};

struct CircleFeatureNode
{
    int id = -1;
    CircleEdgeBandCandidate candidate;
    std::vector<int> neighbors;
    bool visited = false;
    int component_id = -1;
};

struct CircleFeatureEdge
{
    int node_a = -1;
    int node_b = -1;
    double ncc_score = 0.0;
    double angular_distance = 0.0;
    bool valid = false;
};

struct CircleFeatureGraph
{
    std::vector<CircleFeatureNode> nodes;
    std::vector<CircleFeatureEdge> edges;
    int next_component_id = 0;
};

class ICxShapeSink;

class FindCircle:public Shape
{
public:
    FindCircle();
    ~FindCircle();

    void PublishDisplayShapes(ICxShapeSink& sink, const std::string& owner_ref) const;

    int gap() { return m_igap; }
    int thre();

    void clear();
    void setshow(int ishow);
    virtual void setrect(int ix, int iy, int iw, int ih) { (void)ix; (void)iy; (void)iw; (void)ih; }
    void setcircle(int icentx, int icenty, int ipax, int ipay);
    void setcircle2(int icentx, int icenty, int ipax, int ipay, int idis);
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
    gp_Rectangle patternboundingrect();
    void patternrootgrid(double itype, double drate, double ilevel);
    void patternzoom(double dx, double dy, double igap, double itype);
    void patterntranform(int igap, int itype, int isgap, int iline);
    void patternrotate(double dangle);
    void modelzoom(double dx, double dy);
    void patterngap2gap(int inewgap);
    gp_Path& getpatternpath();
    PointsShape& getpattern();
    void setpattern(PointsShape& apattern) { m_modelpoints = apattern; }

    void Setgap(int gap = 2);
    void measure(void* pimage);
    void measureRobust(void* pimage);
    void automeasure(void* pimage);

    void findpattern(void* pimage);

    void setlinesamplerate(double dsamplerate);
    void setlinegap(int igap);
    void setmethod(int imethod);
    void setthre(int ithre);
    void setgamarate(int igama);
    void setfindsetting(int ifindset);
    void setfilter(int ifilterborw, int ifiltermin, int ifiltermax);//21 w ,22 b
    void setfilter_script(int stack_filter_max, int stack_filter_min, int stack_filter_borw)
    {
        setfilter(stack_filter_borw, stack_filter_min, stack_filter_max);
    }
    void setselectedgenum(int iedgenum);
    void getshape(void* pshape);
    void setcirclegap(int ivalue);

    void setshowlines(int ilines) { m_ishowlines = ilines; }
    PointsShape& getresultpoints();
    const PointsShape& getresultpoints() const;

    void fitcircle();
    void fitcirclefiltered();

    int getfitfilterinputcount() { return m_fitfilter_input_count; }
    int getfitfilterkeptcount() { return m_fitfilter_kept_count; }
    int getfitfilterrejectedcount() { return m_fitfilter_rejected_count; }
    double getfitfiltersigma() { return m_fitfilter_sigma; }
    double getfitfilterthreshold() { return m_fitfilter_threshold; }

    void Measure(Image& image);
    void MeasureBalanced(Image& image);
    void MeasureRobust(Image& image);

    void FitResultMeasure(void* pimage);
    void setfitmeasuregap(int igap);
    int m_fitmeasuregap = 80;

    void shapesetroi(void* pshape);
    void MeasureT(void* pimage);

    int getconparegap() { return m_icomparegap; }

    void translate(int ix, int iy);
    void Translate(const gp_Vec& translationVector);
    double getresultcentx();
    double getresultcenty();
    double getradius();
    double getavgdist();

    int getvalidpointcount();
    bool hasfitresult();
    bool canfitresultmeasure();

    double getresultcentx() const;
    double getresultcenty() const;
    double getradius() const;
    bool hasfitresult() const;

    int getcirclecentx() const { return m_icentx; }
    int getcirclecenty() const { return m_icenty; }
    int getcirclepax() const { return m_ipax; }
    int getcirclepay() const { return m_ipay; }
    int getdebugprefilterused() const { return m_last_prefilter_used; }
    int getdebugcompactpathused() const { return m_last_compact_path_used; }
    int getfindsetting() const { return m_ifindset; }
    int getfilterborw() const { return m_ifilterborw; }
    int getfiltermin() const { return static_cast<int>(m_ifiltermin); }
    int getfiltermax() const { return static_cast<int>(m_ifiltermax); }
    const FindCircleMeasureGeometryDebug& lastmeasuregeometrydebug() const
    {
        return m_lastMeasureGeometryDebug;
    }
    GeomAdaptor_Curve GetCurve(gp_Pnt center_p, Standard_Real radius);

    gp_Pnt FindClosestPointOnCurve(GeomAdaptor_Curve myCurve,gp_Pnt externalPoint);

    void setmaxelapsedms(int value);
    void setmaxscanlines(int value);
    void setmaxsamples(int value);

    bool budgetexceeded() const;
    int getelapsedms() const;
    int getscanlinecount() const;
    int getsamplecount() const;
    const std::string& getfailurestage() const;

    double get_result() const { return hasfitresult() ? 1.0 : 0.0; }
    double get_result_script() { return get_result(); }
private:

    double m_dresultcentx;
    double m_dresultcenty;
    double m_dradius;

    double m_avgdist;

    int m_icentx;
    int m_icenty;
    int m_ipax;
    int m_ipay;
    int m_idisgap;

    int m_icomparegap;
    PointsShape m_modelpoints;    //red(white 1) gap blue(black 0) model

    PointsShape m_measurepoints;

    PointsShape m_measurepoints_;

    Shape m_resultcircle;

    Image* g_pbackimage;

    LineShape m_Line;
 
    FindObject* g_pbackfindobject;
 
    LineVector m_lines;

    int m_igap;

    gp_Pnt* m_listscanorA;
    gp_Pnt* m_listcollectorA;

    int m_iSelectPointGap;
    int m_iMethod;
    int m_iThreshold;
    int m_igamarate;
    double m_dsamplerate;

    int m_ifindset;
    int m_ifilterborw;//21 w ,22 b ,23
    int64 m_ifiltermax;
    int64 m_ifiltermin;

    int m_iselectedgenum;
    int m_ineedfixs;

    int m_ncurscan;
    int m_nscansize;

    int resultsize();
     
    static int m_curfindlinenum;

    int m_ishowlines;
    gp_Rectangle m_measurepointsboundingRect;
    int m_last_prefilter_used;
    int m_last_compact_path_used;
    int m_fitfilter_input_count = 0;
    int m_fitfilter_kept_count = 0;
    int m_fitfilter_rejected_count = 0;
    double m_fitfilter_sigma = 0.0;
    double m_fitfilter_threshold = 0.0;

    FindCircleMeasureGeometryRequest m_measure_geometry_request;
    FindCircleMeasureGeometryDebug m_lastMeasureGeometryDebug;

    bool m_measure_geometry_dirty = true;
    bool m_measure_geometry_ready = false;

    std::uint64_t m_measure_geometry_version = 0;
    std::uint64_t m_measure_geometry_built_version = 0;

    CxAlgorithmBudget m_budget;
    CxAlgorithmBudgetState m_budget_state;

    void MarkCircleMeasureGeometryDirty();

    void UpdateCircleMeasureGeometryRequest(bool hasInnerGap);

    bool EnsureCircleMeasureGeometryReady();

    bool BuildCircleMeasureGeometryFromRequest(
        const FindCircleMeasureGeometryRequest& request);

    void BuildCircleMeasureGeometryCore(
        const FindCircleMeasureGeometryRequest& request);

    void ClearMeasureState();

    void CollectCircleEdgeBandsRobust(Image& image);
    void BuildCircleFeatureGraph();
    void FindCircleComponentsInGraph();
    void SelectBestCircleSequence();
    void ConvertCircleSequenceToMeasurePoints(int sequence_index);

    std::vector<CircleFitCandidateSequence> m_circle_fit_candidate_sequences;
    int m_circle_best_sequence_index = -1;
    std::vector<CircleEdgeBandCandidate> m_circle_edge_band_candidates;
    CircleFeatureGraph m_circle_feature_graph;
public:
    void easycluster(int igapx = 10, int igapy = 10, int iclusternum = 5);
    gp_Rectangle measurepointsboundingrect() { return m_measurepointsboundingRect; }

};


#endif //_findline_Header
