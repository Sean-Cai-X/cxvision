#ifndef _findline_Header
#define _findline_Header
#include "Shape.h"
#include "Image.h"
#include "shapebase.h"
#include "findobject.h"
#include "CxImageRuntimeOverlay.h"
#include <string>
#include <map>
#include <string>
#include <array>


class FindObject;

struct FindlineDisplaySnapshot
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
};

struct ScanLineEdgeBands
{
    int scan_index = -1;
    int scan_type = 0; // 0=w, 1=h
    std::vector<EdgeBandCandidate> bands;
};

struct FindlineMeasureProfileStats
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

struct FindlineMeasureInputDebug
{
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
};

class Findline :public Shape
{
public:
    enum class FitlineMode
    {
        Unspecified = 0, LeastSquares = 1, MinimumZone = 2, Ransac = 3,
        SingleEdge = 4, EdgePairCenter = 5,
        HorizontalVerticalPriority = 6, WeightedMeasurementPoints = 7
    };

    Findline();
    ~Findline();
    int wgap() { return m_iwgap; }
    int hgap() { return m_ihgap; }
    int thre();
    int linegap() { return m_iSelectPointGap; }

    void clear();
    void setshow(int ishow);
    void getshape(void* pshape);
    void setlinesegment(double ix0, double iy0, double ix1, double iy1, double iscale);
    void setline(int ix0, int iy0, int ix1, int iy1, int iscale)
    {
        setlinesegment(ix0, iy0, ix1, iy1, iscale);
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
    
    void measure(void* pimage);
    void pyrimage(void* pimage);

    
    void findpattern(void* pimage);

    void setlinesamplerate(double dsamplerate);
    void setlinegap(int igap);
    void setmethod(int imethod);
    void setthre(int ithre);
    void setgamarate(int igama);
    void setobjfilter(int ifindset);
    void setfilter(int ifilterborw, int ifiltermin, int ifiltermax);//21 w ,22 b
    void setselectedgenum(int iedgenum);

    void setshowlines(int ilines) { m_ishowlines = ilines; }
    PointsShape& getresultpointsw();
    PointsShape& getresultpointsh();
    void Measure(Image& image);
    void MeasureBalanced(Image& image);
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
    bool getdisplaysnapshot(FindlineDisplaySnapshot& out) const;
    void exportmeasuredebugpoints(std::vector<float>& outXY) const;
    const FindlineMeasureInputDebug& lastmeasureinputdebug() const
    {
        return m_lastMeasureInputDebug;
    }
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
    const FindlineMeasureProfileStats& lastmeasureprofilestats() const { return m_lastMeasureProfile; }
private:
    void ClearMeasureState();
    void BuildScanProfiles(Image& image, FindlineMeasureProfileStats& stats);
    void CollectAllEdgeBands(Image& image, FindlineMeasureProfileStats& stats);
    void BuildEdgeBandGraph(FindlineMeasureProfileStats& stats);
    void SolveBestEdgeChain(FindlineMeasureProfileStats& stats);
    void ConvertBestChainToMeasurePoints(FindlineMeasureProfileStats& stats);
    void RefineBestChainSubpixel(Image& image, FindlineMeasureProfileStats& stats);
    void FilterMeasurePoints(FindlineMeasureProfileStats& stats);
    void FitWeightedLeastSquares(FindlineMeasureProfileStats& stats);
    void RefineJointConsistency(FindlineMeasureProfileStats& stats);
    void ProbeDisplayRoiGrayStats(Image& image);
    bool MeasureSimpleRoiGradientPoints(Image& image,
                                        FindlineMeasureProfileStats& stats);
private:
    std::vector<ScanLineEdgeBands> m_scanEdgeBands;
    std::vector<EdgeBandCandidate> m_bestEdgeChain;
    FindlineMeasureProfileStats m_lastMeasureProfile;
    FindlineMeasureInputDebug m_lastMeasureInputDebug;
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
};

#endif //_findline_Header
