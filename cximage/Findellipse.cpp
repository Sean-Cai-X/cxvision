#include "pch.h"

#include "Findellipse.h"
#include "occtinclude.h"
#include "imagemanager.h"
#include "findobject.h"
#include "ImageAnnotationLayer.h"
#include "EllipseShape.h"

#include <opencv2/opencv.hpp>		
#include <opencv2/core/version.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/video/tracking.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>
 
namespace
{
int ClampSizeToInt(std::size_t value)
{
    const std::size_t max_value = static_cast<std::size_t>(std::numeric_limits<int>::max());
    return value > max_value ? std::numeric_limits<int>::max() : static_cast<int>(value);
}

int RoundToInt(double value)
{
    if (!std::isfinite(value))
        return 0;
    const double clamped = std::min(
        std::max(value, static_cast<double>(std::numeric_limits<int>::min())),
        static_cast<double>(std::numeric_limits<int>::max()));
    return static_cast<int>(std::lround(clamped));
}

int ClampLongLongToInt(long long value)
{
    if (value < static_cast<long long>(std::numeric_limits<int>::min()))
        return std::numeric_limits<int>::min();
    if (value > static_cast<long long>(std::numeric_limits<int>::max()))
        return std::numeric_limits<int>::max();
    return static_cast<int>(value);
}

int ComputeEllipseLineStep(int gap_degrees, int point_count)
{
    if (gap_degrees <= 0 || point_count <= 0)
        return 1;

    const double angle_rate = gap_degrees / 360.0;
    const int step = RoundToInt(angle_rate * static_cast<double>(point_count));
    return step > 0 ? step : 1;
}
}
 
 
int Findellipse::m_curfindlinenum = 0;
Findellipse::Findellipse() :Shape(),
m_igap(6),
m_iSelectPointGap(3),
m_iMethod(1),
m_iThreshold(8),
m_igamarate(0),
m_dsamplerate(0.004),
m_ifindset(1),
m_ifilterborw(21),
m_ifiltermin(50),
m_ifiltermax(100000),
m_iselectedgenum(0),//any
m_ineedfixs(2),
m_icomparegap(2),
m_ishowlines(1),
m_measurepointsboundingRect(gp_Pnt(0,0,0),0,0)
{
    string strname = string("fellipse%1");// .arg(m_curfindlinenum);
    setname(strname.c_str());
    m_curfindlinenum = m_curfindlinenum + 1;

    int icurmodule = ImageManager::GetCurMode();
    g_pbackimage = ImageManager::GetBackImage(icurmodule);
    g_pbackfindobject = ImageManager::Getbackfindobject(icurmodule);

}
Findellipse::~Findellipse()
{

}
void Findellipse::setcomparegap(int igap)
{
    m_icomparegap = igap;
}

void Findellipse::setshow(int ishow)
{
    if (ishow == 0)
    {
        for (std::size_t i = 0; i < m_lines.size(); ++i)
            m_lines[i].setshow(false);
        Shape::setshow(ishow);
        return;
    }
    if (ishow & 0x02)
    {
        m_measurepoints.setshow(2);
    }
    if (1 == ishow)
    {
        m_measurepoints.setshow(1); 
    }
    if (0x04 == ishow)
    {     
        for (std::size_t i = 0; i < m_lines.size(); ++i)
            m_lines[i].setshow(true);
    }
    else
    {
        for (std::size_t i = 0; i < m_lines.size(); ++i)
            m_lines[i].setshow(false);
    } 
    Shape::setshow(ishow);
}
void Findellipse::setselectedgenum(int iedgenum)
{
    m_iselectedgenum = iedgenum;
}
void Findellipse::clear()
{
    m_lines.clear();
    m_has_display_roi = false;
    m_roi_x0 = 0;
    m_roi_y0 = 0;
    m_roi_x1 = 0;
    m_roi_y1 = 0;
}
void Findellipse::Setgap(int gap)
{
    m_igap = gap;
    for (std::size_t i = 0; i < m_lines.size(); ++i)
    {
        m_lines[i].clear();
    }
    m_lines.clear();

    const int isize = ClampSizeToInt(getpath().ElementCount());
    if (m_igap > 0 && isize > 0)
    {
        LineShape aline1;
        const int igapadd = ComputeEllipseLineStep(m_igap, isize);
        for (int i = 0; i < isize; i += igapadd)
        {
            gp_Pnt apoint = getpath().ElementAt(i);
            m_lines.push_back(aline1);
            m_lines[m_lines.size() - 1].setline(RoundToInt(getcent().X()), RoundToInt(getcent().Y()), RoundToInt(apoint.X()), RoundToInt(apoint.Y()));
            m_lines[m_lines.size() - 1].setPercent(m_dsamplerate);
        }
    }
}
void Findellipse::setellipse(int icentx, int icenty, int ipax, int ipay)
{
    Shape::setellipse(icentx, icenty, ipax, ipay);

    m_roi_x0 = std::min(icentx, ipax);
    m_roi_y0 = std::min(icenty, ipay);
    m_roi_x1 = std::max(icentx, ipax);
    m_roi_y1 = std::max(icenty, ipay);
    m_has_display_roi = m_roi_x1 > m_roi_x0 && m_roi_y1 > m_roi_y0;

    for (std::size_t i = 0; i < m_lines.size(); ++i)
    {
        m_lines[i].clear();
    }
    m_lines.clear();
    const int isize = ClampSizeToInt(getpath().ElementCount());
    if (m_igap > 0 && isize > 0)
    {
        LineShape aline1;
        const int igapadd = ComputeEllipseLineStep(m_igap, isize);
        int icx0 = (icentx + ipax) / 2;
        int icy0 = (icenty + ipay) / 2;
        for (int i = 0; i < isize; i += igapadd)
        {
            gp_Pnt apoint = getpath().ElementAt(i);
            m_lines.push_back(aline1);
            m_lines[m_lines.size() - 1].setline(icx0, icy0, RoundToInt(apoint.X()), RoundToInt(apoint.Y()));
            m_lines[m_lines.size() - 1].setPercent(m_dsamplerate);
        }
    }
    /*
    m_Line.setline(icentx, icenty, ipax, ipay); 
    m_Line.setshow(false); 
    for (std::size_t i = 0; i < m_lines.size(); ++i)
    {
        m_lines[i].clear();
    }  
    m_lines.clear(); 
    int iplinesize = m_igap > 0 ? (360 / m_igap) : 0; 
    LineShape aline1 ;
    for (int i = 0; i < iplinesize; i++)
    { 
        m_lines.push_back(aline1);
        m_lines[i].copy(m_Line);
        m_lines_w[i].Move(m_iwgap * i, 0);
        m_lines_w[i].setPercent(m_dsamplerate);
        m_lines_w[i].setshow(false);
    }
    for (int i = 0; i < iplinehsize; i++)
    { 
        m_lines_h.push_back(aline2);
        m_lines_h[i].copy(m_LineA);
        m_lines_h[i].Move(0, m_ihgap * i);
        m_lines_h[i].setPercent(m_dsamplerate);
        m_lines_h[i].setshow(false);
    }
    */
}
void Findellipse::setellipse2(int icentx, int icenty, int ipax, int ipay,int idis)
{
    Shape::setellipse2(icentx, icenty, ipax, ipay, idis);

    m_roi_x0 = std::min(icentx, ipax);
    m_roi_y0 = std::min(icenty, ipay);
    m_roi_x1 = std::max(icentx, ipax);
    m_roi_y1 = std::max(icenty, ipay);
    m_has_display_roi = m_roi_x1 > m_roi_x0 && m_roi_y1 > m_roi_y0;
    for (std::size_t i = 0; i < m_lines.size(); ++i)
    {
        m_lines[i].clear();
    }
    m_lines.clear();
    const int isize = ClampSizeToInt(getpath().ElementCount());
    if (m_igap > 0 && isize > 0)
    {
        LineShape aline1;
        const int igapadd = ComputeEllipseLineStep(m_igap, isize);
        int icx0 = icentx;
        int icy0 = icenty;
        for (int i = 0; i < isize; i += igapadd)
        {
            gp_Pnt apoint = getpath().ElementAt(i);
            aline1.setline(icx0, icy0, RoundToInt(apoint.X()), RoundToInt(apoint.Y()));
            std::vector<gp_Pnt> acrosspoints = aline1.getpath().IntersectPaths(getpath2());
            if (!acrosspoints.empty())
            {
                LineShape scan_line;
                scan_line.setline(RoundToInt(acrosspoints[0].X()), RoundToInt(acrosspoints[0].Y()), RoundToInt(apoint.X()), RoundToInt(apoint.Y()));
                scan_line.setPercent(m_dsamplerate);
                m_lines.push_back(scan_line);
            }
            aline1.clear();
        }
    }
     
    /*
    m_Line.setline(icentx, icenty, ipax, ipay);
    m_Line.setshow(false);
    for (std::size_t i = 0; i < m_lines.size(); ++i)
    {
        m_lines[i].clear();
    }
    m_lines.clear();
    int iplinesize = m_igap > 0 ? (360 / m_igap) : 0;
    LineShape aline1 ;
    for (int i = 0; i < iplinesize; i++)
    {
        m_lines.push_back(aline1);
        m_lines[i].copy(m_Line);
        m_lines_w[i].Move(m_iwgap * i, 0);
        m_lines_w[i].setPercent(m_dsamplerate);
        m_lines_w[i].setshow(false);
    }
    for (int i = 0; i < iplinehsize; i++)
    {
        m_lines_h.push_back(aline2);
        m_lines_h[i].copy(m_LineA);
        m_lines_h[i].Move(0, m_ihgap * i);
        m_lines_h[i].setPercent(m_dsamplerate);
        m_lines_h[i].setshow(false);
    }
    */
}

void Findellipse::translate(int ix,int iy)
{
    Translate(gp_Vec(ix, iy, 0)); 
}
void Findellipse::Translate(const gp_Vec& translationVector)
{ 
   int ix0 = RoundToInt(translationVector.X());
   int iy0 = RoundToInt(translationVector.Y());
   getpath().Translate(translationVector);
    m_Line.Move(ix0, iy0);
    LineShape aline1, aline2;
    for (std::size_t i = 0; i < m_lines.size(); ++i)
    { 
        m_lines[i].Move(ix0, iy0);
    }

    m_roi_x0 += ix0;
    m_roi_y0 += iy0;
    m_roi_x1 += ix0;
    m_roi_y1 += iy0;
}
void Findellipse::drawpattern()
{ 
    m_modelpoints.setshow(8);
    m_modelpoints.drawshape(getpath());
    m_measurepoints.drawshape(getpath());
    m_measurepoints_.drawshape(getpath());

}
void Findellipse::drawpatternx(double dmovx, double dmovy,
    double dangle,
    double dzoomx, double dzoomy)
{
    m_modelpoints.setshow(8);
    m_modelpoints.drawshapex(getpath(), dmovx, dmovy,
        dangle, dzoomx, dzoomy);
    m_measurepoints.drawshapex(getpath(), dmovx, dmovy,
        dangle, dzoomx, dzoomy);
    m_measurepoints_.drawshapex(getpath(), dmovx, dmovy,
        dangle, dzoomx, dzoomy);
}
void Findellipse::edgepattern(Image& image)
{
    m_measurepoints.clear();
    m_measurepoints_.clear();
    m_modelpoints.clear();

    Setgap(gap());

    setselectedgenum(0);
    setmethod(0);
    Measure(image);
    m_measurepoints.addpoints(getresultpoints());

    m_measurepoints.doublepattern(m_icomparegap, 12, m_modelpoints);

    setselectedgenum(0);
    setmethod(1);
    Measure(image);
    m_measurepoints_.addpoints(getresultpoints());

    m_measurepoints_.doublepattern(m_icomparegap, 6, m_modelpoints);

}
void Findellipse::patternzeroposition()
{
    gp_Rectangle arect1 = m_modelpoints.boundingRect();
    m_modelpoints.Move(RoundToInt(-arect1.TopLeft().X()), RoundToInt(-arect1.TopLeft().Y()));
}
void Findellipse::savepatternfile(const char* pchar)
{
    m_modelpoints.save(pchar);
}
void Findellipse::loadpatternfile(const char* pchar)
{
    m_modelpoints.load(pchar);
}
gp_Rectangle Findellipse::patternboundingrect()
{
    return m_modelpoints.boundingRect();
}
void Findellipse::patterngap2gap(int inewgap)
{
    m_modelpoints.patterngap2gap(inewgap);
}
void Findellipse::patternrootgrid(double itype, double drate, double ilevel)
{
    m_modelpoints.keysrootgrid(RoundToInt(itype), drate, RoundToInt(ilevel));
}
void Findellipse::patterntranform(int igap, int itype, int isgap, int iline)
{
    m_modelpoints.patterntranform(igap, itype, isgap, iline);
}
void Findellipse::patternzoom(double dx, double dy, double igap, double itype)
{
    m_modelpoints.patternzoom(RoundToInt(dx), RoundToInt(dy), RoundToInt(igap), RoundToInt(itype));
}
void Findellipse::patternrotate(double dangle)
{
    m_modelpoints.Rotate(RoundToInt(dangle));
}
void Findellipse::modelzoom(double dx, double dy)
{
    m_modelpoints.Zoom(RoundToInt(dx), RoundToInt(dy));
}
gp_Path& Findellipse::getpatternpath()
{
    return m_modelpoints.getpath();
}
PointsShape& Findellipse::getpattern()
{
    return m_modelpoints;
}
void Findellipse::findpattern(void* pimage)
{
    Image* pgetimage = (Image*)pimage;
    if (pgetimage == nullptr)
        return;
    edgepattern(*pgetimage);
}
void Findellipse::drawshape()
{
    Shape::drawshape();
}
void Findellipse::drawshapex(
    double dmovx, double dmovy,
    double dangle,
    double dzoomx, double dzoomy)
{
    Shape::drawshapex(  dmovx, dmovy,
        dangle, dzoomx, dzoomy);
}

void Findellipse::setlinesamplerate(double dsamplerate)
{
    m_dsamplerate = dsamplerate;
}
void Findellipse::setlinegap(int igap)
{
    m_iSelectPointGap = igap;
}
void Findellipse::setmethod(int imethod)
{
    m_iMethod = imethod;
}
void Findellipse::setthre(int ithre)
{
    m_iThreshold = ithre;
}
int Findellipse::thre()
{
    return m_iThreshold;
}
void Findellipse::setgamarate(int igama)
{
    m_igamarate = igama;
}

void Findellipse::setfindsetting(int ifindset)
{
    m_ifindset = ifindset;
}
void Findellipse::setfilter(int ifilterborw, int ifiltermin, int ifiltermax)
{
    m_ifilterborw = ifilterborw;
    m_ifiltermin = ifiltermin;
    m_ifiltermax = ifiltermax;
}
void Findellipse::MeasureT(void *pimage)
{
    (void)pimage;
}
void Findellipse::Measure(Image& image)
{
    if (image.getWidth() < rect().TopLeft().X() + rect().Width()
        || image.getHeight() < rect().TopLeft().Y() + rect().Height())
        return;//error process
    if (rect().TopLeft().X() < 0 || rect().TopLeft().Y() < 0)
        return;//error process
    m_measurepoints.clear();
    int isize = ClampSizeToInt(m_lines.size());
    if (isize <= 0 || g_pbackimage == nullptr ||
        g_pbackimage == &image ||
        g_pbackimage->getmat().empty())
        return;

    int ilineslen1 = 0;

    if (isize > 0)
        ilineslen1 = m_lines[0].getlinesize();

    int iprocessw = ilineslen1;

    if (iprocessw <= 0 ||
        isize > g_pbackimage->getHeight() ||
        iprocessw > g_pbackimage->getWidth())
        return;

    for (int i = 0; i < isize; i++)
    {
        m_lines[i].linecopyex(image, *g_pbackimage, 0, i);
    }

    g_pbackimage->setroi(0, 0, iprocessw, isize);

    g_pbackimage->roi_7blur_gap_mud_thre_bw(m_iThreshold, m_igamarate, m_iSelectPointGap, m_iMethod);

    if ((m_ifindset & 0x01) && g_pbackfindobject != nullptr)
    {
        g_pbackfindobject->setrect(0, 0, iprocessw, isize);
        g_pbackfindobject->setbrow(m_ifilterborw);//21 22
        g_pbackfindobject->setminmaxarea(ClampLongLongToInt(static_cast<long long>(m_ifiltermin)), ClampLongLongToInt(static_cast<long long>(m_ifiltermax)));
        g_pbackfindobject->Measure(*g_pbackimage);
    }

    std::vector<int> irecordpoint;
    irecordpoint.reserve(128);
    bool bcollectBegin = false;

    int icurlinenum = 0;
    int icurlineposition = 0;

    int ifixvalue = 3;

    cv::Vec3b icolor = 0;
    for (int inumy = 0 + ifixvalue; inumy < isize - ifixvalue; inumy++)
    {
        irecordpoint.clear();
        icurlinenum = 0;
        bcollectBegin = false;
        for (int inumx = 0; inumx < ilineslen1; inumx++)
        {
            icolor = g_pbackimage->pixel(inumx, inumy);
            if ((icolor[0]) > 0)
            {
                irecordpoint.push_back(inumx);
                bcollectBegin = true;
            }
            else
            {
                if (true == bcollectBegin
                    && !irecordpoint.empty()
                    && irecordpoint.size() <= 70)
                {
                    icurlineposition = m_ineedfixs + irecordpoint[(irecordpoint.size() >> 1)];
                    //icurlineposition = icurlineposition>ilineslen1?ilineslen1-1:icurlineposition;

                    icurlinenum++;
                    if (icurlinenum == m_iselectedgenum
                        || m_iselectedgenum == 0)//0 any
                    {
                        if (icurlineposition<(ilineslen1 - m_iSelectPointGap - 3)
                            && icurlineposition>m_iSelectPointGap + 3)
                        {
                            gp_Pnt apoint = m_lines[inumy].getlinepoint(icurlineposition);
                            m_measurepoints.addpoint(apoint);
                            if (icurlinenum == m_iselectedgenum)
                                break;
                        }
                    }
                }
                irecordpoint.clear();
                bcollectBegin = false;
            }
        }
        if (true == bcollectBegin
            && !irecordpoint.empty())
        {
            icurlineposition = m_ineedfixs + irecordpoint[(irecordpoint.size() >> 1)];
            //icurlineposition = icurlineposition>ilineslen1?ilineslen1-1:icurlineposition;
            icurlinenum++;
            if (icurlinenum == m_iselectedgenum
                || m_iselectedgenum == 0)
            {
                if (icurlineposition<(ilineslen1 - m_iSelectPointGap - 3)
                    && icurlineposition>m_iSelectPointGap + 3)
                {
                    gp_Pnt apoint = m_lines[inumy].getlinepoint(icurlineposition);
                    m_measurepoints.addpoint(apoint);
                    if (icurlinenum == m_iselectedgenum)
                        break;
                }
            }
            irecordpoint.clear();
            bcollectBegin = false;
        }

    }

    
}

PointsShape& Findellipse::getresultpoints()
{
    return m_measurepoints;
}


void Findellipse::measure(void* pimage)
{
    Image* pgetimage = (Image*)pimage;
    if (pgetimage == nullptr)
        return;
    Measure(*pgetimage);
}
void Findellipse::shapesetroi(void* pshape)
{
    if (pshape == nullptr)
        return;
    Shape::shapesetroi(pshape);
}
void Findellipse::easycluster(int igapx, int igapy, int iclusternum)
{
    PointsShape resultpoints;
    resultpoints.addpoints(getresultpoints());
    vector<int> numlist;
    int isize = ClampSizeToInt(resultpoints.size());
    if (0 == isize)
        return;
    for (int i = 0; i < isize; i++)
    {
        numlist.push_back(1);
    }
    for (int i = 0; i < isize; i++)
    {
        int ix0 = RoundToInt(resultpoints.getx(i));
        int iy0 = RoundToInt(resultpoints.gety(i));
        if (i + 1 < isize)
            for (int j = i + 1; j < isize; j++)
            {
                int ix1 = RoundToInt(resultpoints.getx(j));
                int iy1 = RoundToInt(resultpoints.gety(j));
                if (abs(ix0 - ix1) < igapx
                    && abs(iy0 - iy1) < igapy)
                {
                    numlist[i]++;
                    numlist[j]++;
                }
            }
    }
    PointsShape amodelpointsw;
    PointsShape amodelpointsh;
    int isize1 = ClampSizeToInt(getresultpoints().size());
    getresultpoints().clear();
    int inum = 0;
    for (inum = 0; inum < isize1; inum++)
    {
        int ix0 = RoundToInt(resultpoints.getx(inum));
        int iy0 = RoundToInt(resultpoints.gety(inum));
        int inumsum = numlist[inum];
        if (inumsum > iclusternum)
        {
            getresultpoints().addpoint(ix0, iy0);
        }
    }

}

bool Findellipse::getdisplaysnapshot(FindellipseDisplaySnapshot& out) const
{
    out = {};

    out.has_roi = m_has_display_roi;

    if (m_has_display_roi)
    {
        out.center_x = static_cast<double>(m_roi_x0 + m_roi_x1) * 0.5;
        out.center_y = static_cast<double>(m_roi_y0 + m_roi_y1) * 0.5;
        out.radius_x = std::abs(static_cast<double>(m_roi_x1 - m_roi_x0)) * 0.5;
        out.radius_y = std::abs(static_cast<double>(m_roi_y1 - m_roi_y0)) * 0.5;
    }

    const PointsShape& points = const_cast<Findellipse*>(this)->getresultpoints();
    out.has_measure_points = points.size() > 0;
    out.measure_points_count = static_cast<int>(points.size());

    return out.has_roi || out.has_measure_points;
}

void Findellipse::PublishDisplayShapes(
    ICxShapeSink& sink,
    const std::string& owner_ref) const
{
    FindellipseDisplaySnapshot snapshot;
    if (!getdisplaysnapshot(snapshot))
        return;

    if (snapshot.has_roi)
    {
        auto roi = std::make_unique<EllipseShape>(
            snapshot.center_x,
            snapshot.center_y,
            snapshot.radius_x,
            snapshot.radius_y);

        sink.UpsertShape(
            owner_ref + ".roi_ellipse",
            "Findellipse",
            owner_ref,
            "setellipse",
            "roi",
            true,
            false,
            std::move(roi));
    }

    const PointsShape& points = const_cast<Findellipse*>(this)->getresultpoints();
    if (points.size() > 0)
    {
        auto resultPoints = std::make_unique<PointsShape>();
        for (int i = 0; i < static_cast<int>(points.size()); ++i)
        {
            resultPoints->addpoint(points.getx(i), points.gety(i));
        }

        sink.UpsertShape(
            owner_ref + ".measure_points",
            "Findellipse",
            owner_ref,
            "",
            "measure_points",
            false,
            true,
            std::move(resultPoints));
    }
}
