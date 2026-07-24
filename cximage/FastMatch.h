#ifndef FASTMATCH_H
#define FASTMATCH_H
#include <map>
#include <vector>
#include "Shape.h"
#include "Image.h"
#include "shapebase.h"
#include "FindLine.h"
#include "Grid.h"
#include <opencv2/core/mat.hpp>

class Grid;
class ICxShapeSink;
typedef vector<int> Cluster;
using namespace std;

// FastMatch Learn Probe - enable for debugging, disable for release
#define FASTMATCH_LEARN_PROBE

// fastmatch extends Findline with grid/model learning and match result helpers.
class FastMatch :public FindLine
{
public:
    FastMatch();
    ~FastMatch();

    void setshow(int ishow);

    virtual void setrect(int ix, int iy, int iw, int ih);
    virtual void drawshape();

    void drawshapex(double dmovx, double dmovy,
        double dangle, double dzoomx, double dzoomy);
    void getshape(void* pshape);

    void SetWHgap(int wgap = 2, int hgap = 2);
    void measure(void* pimage);
    void setlinesamplerate(double dsamplerate);
    void setlinegap(int igap);
    void setmethod(int imethod);
    void setthre(int ithre);
    void setgamarate(int igama);
    void setobjfilter(int ifindset);
    void setfilter(int ifilterborw, int ifiltermin, int ifiltermax);//21 w ,22 b
    void setselectedgenum(int iedgenum);

    void learn_level0(void* pimage);//5pyrDown   thre >50  linegap 3
    void learn_level0_1(void* pimage);//5pyrDown   thre >50  linegap 7
    void learn_level1(void* pimage);//5pyrDown   thre >30  linegap 7
    void learn_level2(void* pimage);//3pyrDown   thre >30
    void learn_level3(void* pimage);//1pyrDown   thre >10
    void learn_level4(void* pimage);//thre >7

    void learn(void* pimage);
    void setcomparegap(int igap);
    void savemodelfile(const char* pchar);
    void loadmodelfile(const char* pchar);

    void ABtoShape(std::vector<cv::Point2f>& points);

    std::vector<cv::Point2f> getmodel();
    int getmodelpointcount();
    int getlearnacount();
    int getlearnbcount();
    int getlearna2count();
    int getlearnb2count();
    std::uintptr_t debuglastlearnargument() const noexcept
    {
        return reinterpret_cast<std::uintptr_t>(m_debug_last_learn_argument);
    }
    int getmodelwidth() const { return m_imodelwith; }
    int getmodelheight() const { return m_imodelheigh; }
    int getpatternapointcount() const;
    int getpatternbpointcount() const;
    double getpatternax() const;
    double getpatternay() const;
    double getpatternawidth() const;
    double getpatternaheight() const;
    double getpatternbx() const;
    double getpatternby() const;
    double getpatternbwidth() const;
    double getpatternbheight() const;

    int ABpatternsize();
    void loadrotatemodelfile(const char* pchar);
    void loadrotate05modelfile(const char* pchar);
    void loadrotate025modelfile(const char* pchar);

    void loadcalibration(const char* pchar);
    void savecalibration(const char* pchar);

    void setrotateangle(double danglel1);
    void setrotateanglescale(double dangle1, double dangle2);

    void patternrootgrid(double itype, double drate, double ilevel);

    void patternzoom(double dx, double dy, double igap, double itype);

    void patterntranform(int igap, int itype, int isgap, int iline);

    void patterngap2gap(int inewgap);
    void patternABgap2gap(double dnewgaprate);
    void patternABsample(int irate);
    void pattern2org();
    void org2pattern();

    void modelrotate(double dangle);
    void modelzoom(double dx, double dy);
    void setmodelwh(int iw, int ih);
    void modelzeroposition();
    void rotatemodelzeroposition();
    void rotatemodelzeropositionAB();
    void rotatemodel05zeroposition();
    void rotatemodel025zeroposition();

    void Distfilter();

    void samplemodelAB(int inum); 

    void MatchAB(Image& image);
    void match(void* pimage);

    void MatchABMore(Image& image);
    void matchmore(void* pimage);
 

    void MultiMatch(Image& image);
    void multimatch(void* pimage);



    void rotatematch(void* pimage);
    void rotatematchAB(void* pimage);
    void rotatematchAB_upgrade(void* pimage);
    void rotatematchAB05_upgrade(void* pimage);
    void rotatematchAB025_upgrade(void* pimage);

    void setupgradenum(int iresultnum);

    void setclustergap(int ixclustergap, int iyclustergap, int iangleclustergap);

    void savematchroi(const char* pfilename);

    void imagelearn(int ithre1, int iandor);
    void imagelearnex(int ithre1, int iandor, int igrid);
    void imagelearnmass(int ithre1, int iandor, int igridwh);
    void imagelearncheck(int iimagetype, int iandor, int igridwh);

    void imagematch(int ithre1, int iandor, int igrid = 12, int ineedthre = 1);
    void imagematchex(int igrid);
    void savematchimagemodel(const char* pfilename);

    void loadfastimagemodel(const char* pfilename);
    void savefastimagemodel(const char* pfilename);
    void savefastimagepatmodel(const char* pfilename);

    void SaveMatchROI(Image& image, const char* pfilename);
    void MatchImageLearn(Image& aimage, int ithre1, int iandor);
    void MatchImageLearnEx(Image& aimage, int ithre1, int iandor, int igrid);
    void MatchImageLearnMass(Image& aimage, int ithre1, int iandor, int igrid);
    void MatchImageCheck(Image& aimage, int iimagetype, int iandor, int igrid);

    void imagematch_grid(int ithre1, int iandor, int igrid);

    void MatchImageMatch(Image& aimage, int ithre1, int iandor, int igrid = 12, int ineedthre = 1);
    void MatchImageExMatch(Image& aimage, int igrid = 12);
    void MatchGrid(Grid* pgrid);

    void setmatchrectnum(int inum);
    void setmatchrect(int ix, int iy, int iw, int ih);
    void setrectxywh(int ih, int iw, int iy, int ix);
    void setmatchrectxywh(int ih, int iw, int iy, int ix);
    void setexpectedrect(double x0, double y0, double x1, double y1);

    void setmultimatchrect(int inum, int ix, int iy, int iw, int ih);
    void setmatchthre(int ithre);
    void setfindnum(int ifindnum);
    void setmatchmask(const cv::Mat* pmask);
    void clearmatchmask();
    int getrawmatchprobecount() const;
    int getrawmatchthresholdhitcount() const;
    int getmatchcallcount() const;
    int getmatchabcallcount() const;
    int getmatchsampleabcallcount() const;
    int getmatchlaststage() const;
    int getmatchimagewidth() const;
    int getmatchimageheight() const;
    int getmatchrectx0() const;
    int getmatchrecty0() const;
    int getmatchrectx1() const;
    int getmatchrecty1() const;
    int getresulttolistcallcount() const;
    int getresultcandidateinsertcount() const;
    int getresultcandidatereplacecount() const;
    int getresultcandidaterejectcount() const;
    int getresultcandidatecount();
    int getresultbestindex();
    double getresultbestscore();
    int getrawthresholdhitrecordcount() const;
    gp_Pnt getrawthresholdhitpoint(int inum) const;
    int getrawthresholdhitscore(int inum) const;
    double getresultnum(int inum);
    double getresultcentx(int inum);
    double getresultcenty(int inum);
    double getresolvedresultcentx(int inum);
    double getresolvedresultcenty(int inum);
    int getrotateresultcentx(int inum);
    int getrotateresultcenty(int inum);


    void setminscore(double dscore);

    double getmaxresult();
    double getimagemodelreslut();
    double getimagemodelreslut_check_1();

    int getmodeleasyobjectw_l72(int inum);
    int getmodeleasyobjectb_l72(int inum);

    int getmodeleasyobjectw_l36(int inum);
    int getmodeleasyobjectb_l36(int inum);

    int getmodeleasyobjectw_l12(int inum);
    int getmodeleasyobjectb_l12(int inum);

    int getmodeleasyobjectw_l6(int inum);
    int getmodeleasyobjectb_l6(int inum);

    int getmodeleasyobjectw_l3(int inum);
    int getmodeleasyobjectb_l3(int inum);

    int geteasyobjectb();
    int geteasyobjectw();

    void imagemodelcompareshow(int itype);
    void imagemodelcomparegrid(int itype);

    double imagegridresult(int itype);

    void imagemodelshow();
    void imagematchshow();

    void clearmodels_l12();
    void addmodels_l12(const char* pchar);

    void clearmodels_l36();
    void addmodels_l36(const char* pchar);

    void clearmodels_l72();
    void addmodels_l72(const char* pchar);

    void modelstocurrent_l72(int i);
    void modelstocurrent_l36(int i);
    void modelstocurrent_l12(int i);
    void modelstocurrent_l3(int i);
    void modelstocurrent_l6(int i);

    void imagemodesclear_l12();
    void addimagemodels_l12(const char* pchar);

    void imagemodesclear_l36();
    void addimagemodels_l36(const char* pchar);

    void imagemodesclear_l72();
    void addimagemodels_l72(const char* pchar);

    void clearmodels_rotate();
    void addmodels_rotate(const char* pchar);

    int GetRectGridLevel(int irectw);
    vector<int>* getcurimagemodel();
    bool modelcompare(vector<int>& modela, vector<int>& modelb);

    void clearmodel();
    void list_duplicatesmodel_l12();
    void list_duplicatesmodel_l36();
    void list_duplicatesmodel_l72();

    void levelmodels_l72tol36();
    void levelmodels_l36tol12();
    void levelmodels_l12tol6();
    void levelmodels_l6tol3();

    int imagefastmodelsize(int ilevel);
    void imagemodelstocurrent_l72(int i);
    void imagemodelstocurrent_l36(int i);
    void imagemodelstocurrent_l12(int i);
    void imagemodelstocurrent_l3(int i);
    void imagemodelstocurrent_l6(int i);

    void objectmodelstocurrent(int i);

    void setcurmodels(int inum);
    void setcurimagemodels(int inum);

    void setspecshow(int ishow);
    void setb2w(int ib2w);
    void modelmethod(int itype);
    RectsShape& getmatchrects();
    gp_Rectangle& getmatchrect();
    gp_Rectangle getresultrect(int inum) const;
    gp_Rectangle getresolvedresultrect(int inum) const;
    vector<PointsShape>& getmodels_l12();
    Grid* getgrid();
    void setgrid(int iw, int igrid);
    map<int, int >& getlevel3_6map();
    map<int, int >& getlevel6_12map();
    map<int, int >& getlevel12_36map();
    map<int, int >& getlevel36_72map();

    vector<int>& getduplicateslist_l72();
    vector<int>& getduplicateslist_l36();
    vector<int>& getduplicateslist_l12();
    void savelevel0_l1();
private:
    int m_istyle;
    Image* g_pmodelimage;
    Image* m_matchimage;
    const cv::Mat* m_matchmask;


    vector<int> m_imagefastmodel;

    vector<PointsShape> m_models_l72;

    vector<PointsShape> m_models_l36;

    vector<PointsShape> m_models_l12;

    vector<PointsShape> m_models_l3;

    vector<PointsShape> m_models_l6;

    typedef vector<int> IMAGEFASTMODEL;
    vector<IMAGEFASTMODEL> m_imagefastmodels_l72;

    vector<IMAGEFASTMODEL> m_imagefastmodels_l36;

    vector<IMAGEFASTMODEL> m_imagefastmodels_l12;

    vector<IMAGEFASTMODEL> m_imagefastmodels_l6;

    vector<IMAGEFASTMODEL> m_imagefastmodels_l3;


    vector<int>m_duplicates_list_l72;
    vector<int>m_duplicates_list_l36;
    vector<int>m_duplicates_list_l12;

    // QVector<IMAGEFASTMODEL> m_imagefastmodels_l12_l2;

    map<int, int > m_mapl3_l6;
    map<int, int > m_mapl6_l12;
    map<int, int > m_mapl12_l36;
    map<int, int > m_mapl36_l72;

    vector<easyobj> m_easyobjectmodels_l72;

    vector<easyobj> m_easyobjectmodels_l36;

    vector<easyobj> m_easyobjectmodels_l12;

    vector<easyobj> m_easyobjectmodels_l6;

    vector<easyobj> m_easyobjectmodels_l3;

    vector<int> m_imagefastmatchlist;

    int m_imodelwith;
    int m_imodelheigh;

    easyobj m_cureasyobject;

    easyobj m_easyobject;

    int m_imagemodelresult_NG;
    int m_imagemodelresult_OK;

    int m_easyobjectw_ng;

    int m_easyobjectb_ng;

    PointsShape m_modelpoints_sample1;//  /2
    PointsShape m_modelpoints_sample2;//  /4
    PointsShape m_modelpoints_sample3;//  /8

    int m_imaxmatchnum;
    int m_imatchthre;
    int m_iB2W;
    int m_imatchoffset;

    RectsShape m_matchrects;
    gp_Rectangle m_matchrect;
    gp_Rectangle m_expected_rect;

    int m_fastmatch_learn_a_count = 0;
    int m_fastmatch_learn_b_count = 0;
    int m_fastmatch_learn_a2_count = 0;
    int m_fastmatch_learn_b2_count = 0;

    int m_imatchrectnum;
    vector<gp_Pnt> m_resultpoints;
    vector<int> m_resultnums;

    RectsShape m_resultrects;

    int m_stepgapx;
    int m_stepgapy;

    double m_danglegap;//5

    double m_dangle_add;//10 
    double m_dangle_mud;//-10 

    vector<PointsShape> m_models_rotate;//1 degree
    vector<PointsShape> m_models_rotaterects;//4 points

    vector<PointsShape> m_models05_rotate;//1 degree
    vector<PointsShape> m_models05_rotaterects;//4 points

    vector<PointsShape> m_models025_rotate;//1 degree
    vector<PointsShape> m_models025_rotaterects;//4 points

    vector<PointsShape> m_rotateshaperesults;//4 points 
    //RectsShape m_rotateresultrects;

    vector<gp_Pnt> m_rotatereslutpoints;
    vector<double> m_rotateresults;//
    vector<double> m_rotatereslutangles;//

    int m_iupgradenum = 0;

    vector<Cluster> m_clusters;

    PointsShape m_calibration;

    int m_ixclustergap;
    int m_iyclustergap;
    int m_iangleclustergap;

    int m_iminfindnum;
    gp_Pnt m_iminpointkey;
    int m_imaxfindnum;
    gp_Pnt m_imaxpointkey;
    int m_rawmatch_probe_count;
    int m_rawmatch_threshold_hit_count;
    int m_match_call_count = 0;
    int m_matchab_call_count = 0;
    int m_matchsampleab_call_count = 0;
    int m_match_last_stage = 0;
    int m_match_debug_image_width = 0;
    int m_match_debug_image_height = 0;
    int m_match_debug_rect_x0 = 0;
    int m_match_debug_rect_y0 = 0;
    int m_match_debug_rect_x1 = 0;
    int m_match_debug_rect_y1 = 0;
    int m_resulttolist_call_count;
    int m_resultcandidate_insert_count;
    int m_resultcandidate_replace_count;
    int m_resultcandidate_reject_count;
    vector<gp_Pnt> m_rawthresholdhitpoints;
    vector<int> m_rawthresholdhitscores;

    void* m_debug_last_learn_argument = nullptr;

    Grid* m_pgrid;//12X12

    QRootGrid* m_rootgridA;

    //  Grid *m_pgrid_l0;//3X3
    //  Grid *m_pgrid_l1;//6X6

    void Learn(Image& image);

    void Learn_level0(Image& image);//5pyrDown   thre >50 
    void Learn_level1(Image& image);//5pyrDown   thre >30
    void Learn_level2(Image& image);//2pyrDown   thre >30
    void Learn_level3(Image& image);//1pyrDown   thre >10
    void Learn_level4(Image& image);//thre >7
 
    void resulttolist(gp_Pnt& apoint, int inum);
    void resultclear();
    void resultsort();
    void clusterclear();
    void rotateresultsort();

    void resultcluster(int ixgap, int iygap, int ianglegap);

    void MatchSample(Image& image, gp_Path& path);
    void MatchSampleAB(Image& image, gp_Path& pathA, gp_Path& pathB);
    void MatchSampleABMore(Image& image, gp_Path& pathA, gp_Path& pathB);

    void MultiMatchSample(Image& image, gp_Path& path);
    void RotateMatch(Image& image);
    void RotateMatchAB(Image& image);

    int m_iupgradexscale;
    int m_iupgradeyscale;

    int m_iupgradeanglescale;

    void RotateMatchAB_upgrade(Image& image);
    void RotateMatchAB05_upgrade(Image& image);
    void RotateMatchAB025_upgrade(Image& image);

    void RotateMatchSample(Image& image, gp_Path& path, PointsShape& modelrect, double dangle);
    void RotateMatchSample_upgrade(Image& image, gp_Path& path, PointsShape& modelrect, double dangle, gp_Pnt& resultpoint);

    void RotateMatchSampleAB(Image& image, gp_Path& pathA,
        gp_Path& pathB, PointsShape& modelrect,
        double dangle);

    int m_ispecshow;
    double m_dminscore;

    static int m_curfastmatchnum;

    FastMatch* m_prelationmatch;

    int m_irelationresultnum;

    double m_drelationzoomx;

    double m_drelationzoomy;

    gp_Rectangle m_irelationrect;

public:
    RectsShape* getresultrects() { return &m_resultrects; }
    const RectsShape* getresultrects() const { return &m_resultrects; }

    void setrelationrectfromresultnum(int inum);
    void setrelationrectfrom_matchresult(void* pmatch);
    void setrelationxy(int iprex1, int iprey1, int iendx1, int iendy1);
    void setrelationzoom(double drelationzoomx, double drelationzoomy);
    void setrelationtorect();
    void setcolorstyle(int istyle);
    void Setupgradescale(int isx, int isy);
    void Setupgradeanglescale(int iangle);

    void MinModelLearn(Image& image);

    void shapesetroi(void* pshape);
    void matchstepgap(int ix, int iy);

    double getrotateresultx();
    double getrotateresulty();
    double getrotateresulta();
    double getrotateresultscore();
    double getrotateresultscoreA(int inum);

    void rotateresultsortfilter(int ifdx, int ifdy, int itype);

    void rotateresultsortfilterA(int ifdx, int ifdy, int itype);

    int rotateresultsize();
    void setshownum(int ishownum);
    int m_ishownum = 1;

    void getresultcentpoints(void* points);
    void getrotateresultrectpoints(std::vector<cv::Point2f>& points);
    void ZeroPOS();

    void PublishDisplayShapes(ICxShapeSink& sink, const std::string& owner_ref);
    bool ApplyDisplayShapeEdit(const std::string& owner_binding, const std::string& semantic_role,
                               double x0, double y0, double x1, double y1, std::string& reason);
};

#endif //FASTMATCH_H
