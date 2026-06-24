#ifndef _findline_Header
#define _findline_Header
#include "Shape.h"
#include "Image.h"
#include "shapebase.h"
#include "findobject.h"



class FindObject;

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

class Findline :public Shape
{
public:
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

    void FitLine()
    {
      /*  Vec4d lineParams; // 0:Vx,1:Vy,2:X1,3:Y1;
        fitLine(points, lineParams, DIST_L2, 0, 0.01, 0.01);
        vector<double> vec4d = vector<double>();
        vec4d.push_back(lineParams[0]);
        vec4d.push_back(lineParams[1]);
        vec4d.push_back(lineParams[2]);
        vec4d.push_back(lineParams[3]);*/
    }
private:
    int m_icomparegap;
    PointsShape m_modelpoints;    //red(white 1) gap blue(black 0) model
    PointsShape m_modelpoints_org;     


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
private:
    std::vector<ScanLineEdgeBands> m_scanEdgeBands;
    std::vector<EdgeBandCandidate> m_bestEdgeChain;
    FindlineMeasureProfileStats m_lastMeasureProfile;
};

#endif //_findline_Header
