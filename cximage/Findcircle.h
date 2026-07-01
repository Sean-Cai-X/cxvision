#ifndef _findcircle_Header
#define _findcircle_Header
#include "Shape.h"
#include "Image.h"
#include "shapebase.h"
class FindObject;
class Findcircle :public Shape
{
public:
    Findcircle();
    ~Findcircle();
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
    void automeasure(void* pimage);

    void findpattern(void* pimage);

    void setlinesamplerate(double dsamplerate);
    void setlinegap(int igap);
    void setmethod(int imethod);
    void setthre(int ithre);
    void setgamarate(int igama);
    void setfindsetting(int ifindset);
    void setfilter(int ifilterborw, int ifiltermin, int ifiltermax);//21 w ,22 b
    void setselectedgenum(int iedgenum);
    void getshape(void* pshape);
    void setcirclegap(int ivalue);

    void setshowlines(int ilines) { m_ishowlines = ilines; }
    PointsShape& getresultpoints();

    void fitcircle();

    void Measure(Image& image);
    void MeasureBalanced(Image& image);

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
    int getcirclecentx() const { return m_icentx; }
    int getcirclecenty() const { return m_icenty; }
    int getcirclepax() const { return m_ipax; }
    int getcirclepay() const { return m_ipay; }
    int getdebugprefilterused() const { return m_last_prefilter_used; }
    int getdebugcompactpathused() const { return m_last_compact_path_used; }
    GeomAdaptor_Curve GetCurve(gp_Pnt center_p, Standard_Real radius);

    gp_Pnt FindClosestPointOnCurve(GeomAdaptor_Curve myCurve,gp_Pnt externalPoint);
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
public:
    void easycluster(int igapx = 10, int igapy = 10, int iclusternum = 5);
    gp_Rectangle measurepointsboundingrect() { return m_measurepointsboundingRect; }

};


#endif //_findline_Header
