#ifndef _findrect_Header
#define _findrect_Header

#include "Shape.h"
#include "Image.h"
#include "shapebase.h"
#include "Findline.h"
#include "../cxgeom/include/CxGeomElementBody.h"

class ICxShapeSink;

class FindRect : public Shape
{
public:
    FindRect();
    ~FindRect();

    void clear();
    void setshow(int ishow);
    virtual void setrect(int ix, int iy, int iw, int ih);
    virtual void drawshape();
    void shapesetroi(void* pshape);

    void setthre(int ithre);
    int thre() const;
    void setcomparegap(int igap);
    int getcomparegap() const;
    void setlinegap(int igap);
    int linegap() const;
    void setmethod(int imethod);
    int method() const;
    void setgauge(int igauge);
    int gauge() const;
    void setfindsetting(int ifindset);
    void setfilter(int ifilterborw, int ifiltermin, int ifiltermax);

    void setminmaxarea(int imin, int imax);
    void setminmaxwh(int iminw, int imaxw, int iminh, int imaxh);
    void setpolygonepsilon(double epsilon_ratio);
    void setfillratio(double min_fill_ratio);

    void measure(void* pimage);
    void Measure(Image& image);

    RectsShape& getresultrects();
    gp_Rectangle getresultrect(int inum) const;
    int getresultobjsnum() const;
    cxgeom::CxSurfaceElement makeresultelement(int inum, int entity_id) const;

    bool hasresult() const
    {
        return m_lastresult.valid && m_resultrects.size() > 0;
    }

    void PublishDisplayShapes(
        ICxShapeSink& sink,
        const std::string& owner_ref) const;

    struct EdgeLearnResult
    {
        bool valid = false;
        struct FittedLineData
        {
            bool valid = false;
            double nx = 0.0;
            double ny = 0.0;
            double c = 0.0;
            int point_count = 0;
            double fit_error = 0.0;
            double angle = 0.0;
        } line;
        FindlineMeasureProfileStats stats;
        gp_Rectangle roi = gp_Rectangle(gp_Pnt(0, 0, 0), gp_Pnt(0, 0, 0));
    };

    struct RectLearnResult
    {
        bool valid = false;
        gp_Rectangle rect = gp_Rectangle(gp_Pnt(0, 0, 0), gp_Pnt(0, 0, 0));
        gp_Rectangle seed_rect = gp_Rectangle(gp_Pnt(0, 0, 0), gp_Pnt(0, 0, 0));
        EdgeLearnResult top;
        EdgeLearnResult bottom;
        EdgeLearnResult left;
        EdgeLearnResult right;
        int gauge = 0;
        double score = 0.0;
    };

private:
    bool LearnRectOnce(Image& image, const gp_Rectangle& working_rect, RectLearnResult& result);
    bool RefineRectOnce(Image& image, const gp_Rectangle& coarse_rect, RectLearnResult& result);

    RectsShape m_resultrects;
    RectLearnResult m_lastresult;
    Image* g_pbackimage;
    FindObject* g_pbackfindobject;
    int m_ithreshold;
    int m_icomparegap;
    int m_ilinegap;
    int m_imethod;
    int m_igauge;
    int m_ifindset;
    int m_ifilterborw;
    int64 m_ifiltermin;
    int64 m_ifiltermax;
    int m_iminarea;
    int m_imaxarea;
    int m_iminobjw;
    int m_imaxobjw;
    int m_iminobjh;
    int m_imaxobjh;
    double m_depsilonratio;
    double m_dminfillratio;
    Findline m_topfinder;
    Findline m_bottomfinder;
    Findline m_leftfinder;
    Findline m_rightfinder;
    static int m_curfindrectnum;
};

#endif
