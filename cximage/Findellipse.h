#ifndef _findellipse_Header
#define _findellipse_Header
#include "Shape.h"
#include "Image.h"
#include "shapebase.h"
#include <string>
class FindObject;
class ICxShapeSink;

struct FindellipseDisplaySnapshot
{
    bool has_roi = false;

    double center_x = 0.0;
    double center_y = 0.0;
    double radius_x = 0.0;
    double radius_y = 0.0;

    bool has_measure_points = false;
    int measure_points_count = 0;

    bool has_fit_ellipse = false;
    double fit_center_x = 0.0;
    double fit_center_y = 0.0;
    double fit_radius_x = 0.0;
    double fit_radius_y = 0.0;
    double fit_angle_deg = 0.0;
    double fit_avgdist = 0.0;

    int gap = 0;
    int linegap = 0;
    int threshold = 0;
    int method = 0;
    int scan_line_count = 0;
    int scan_line_length = 0;
    std::string measure_failure_stage;
    std::string measure_failure_reason;
};

class Findellipse :public Shape
{
public:
    Findellipse();
    ~Findellipse();
    int gap() { return m_igap; }
    int thre();

    void clear();
    void setshow(int ishow);
    virtual void setrect(int ix, int iy, int iw, int ih) { (void)ix; (void)iy; (void)iw; (void)ih; }
    void setellipse(int icentx, int icenty, int ipax, int ipay);
    void setellipse2(int icentx, int icenty, int ipax, int ipay,int idis);
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
    void findpattern(void* pimage);

    void setlinesamplerate(double dsamplerate);
    void setlinegap(int igap);
    void setmethod(int imethod);
    void setthre(int ithre);
    void setgamarate(int igama);
    void setfindsetting(int ifindset);
    void setfilter(int ifilterborw, int ifiltermin, int ifiltermax);//21 w ,22 b
    void setselectedgenum(int iedgenum);

    void setshowlines(int ilines) { m_ishowlines = ilines; }
    PointsShape& getresultpoints();
    void Measure(Image& image);
    void fitellipse();
    double getresultcentx();
    double getresultcenty();
    double getresultradiusx();
    double getresultradiusy();
    double getresultangle();
    double getavgdist();
    double hasfitresult();
    double get_result() { return hasfitresult(); }

    void shapesetroi(void* pshape);
    void MeasureT(void* pimage);

    int getconparegap() { return m_icomparegap; }

    void translate(int ix, int iy);
    void Translate(const gp_Vec& translationVector);
private:
    int m_icomparegap;
    PointsShape m_modelpoints;    //red(white 1) gap blue(black 0) model

    PointsShape m_measurepoints;

    PointsShape m_measurepoints_;

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
public:
    void easycluster(int igapx = 10, int igapy = 10, int iclusternum = 5);
    gp_Rectangle measurepointsboundingrect() { return m_measurepointsboundingRect; }

    bool getdisplaysnapshot(FindellipseDisplaySnapshot& out) const;

    void PublishDisplayShapes(
        ICxShapeSink& sink,
        const std::string& owner_ref) const;

private:
    bool m_has_display_roi = false;
    int m_roi_x0 = 0;
    int m_roi_y0 = 0;
    int m_roi_x1 = 0;
    int m_roi_y1 = 0;

    bool m_has_fit_result = false;
    double m_fit_center_x = 0.0;
    double m_fit_center_y = 0.0;
    double m_fit_radius_x = 0.0;
    double m_fit_radius_y = 0.0;
    double m_fit_angle_deg = 0.0;
    double m_fit_avgdist = 0.0;
    std::string m_measure_failure_stage;
    std::string m_measure_failure_reason;

};


#endif //_findline_Header
