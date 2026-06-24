#include "pch.h"

#include "Findline.h"
#include "../../cxgeom/include/CxSetLineBuild.h"
#include "occtinclude.h"
#include "imagemanager.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>

#include <opencv2/opencv.hpp>		
#include <opencv2/core/version.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/video/tracking.hpp>
				

#if defined USE_AI
#include "mlpackrun.h"
#endif

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
    const long long min_value = static_cast<long long>(std::numeric_limits<int>::min());
    const long long max_value = static_cast<long long>(std::numeric_limits<int>::max());
    return static_cast<int>(std::min(std::max(value, min_value), max_value));
}

int PositiveGap(int gap)
{
    return std::max(1, gap);
}

int PositiveExtent(int extent)
{
    return std::max(1, extent);
}

int CeilPositiveToInt(double value)
{
    if (!std::isfinite(value) || value <= 0.0)
        return 1;
    const double max_value = static_cast<double>(std::numeric_limits<int>::max());
    return static_cast<int>(std::ceil(std::min(value, max_value)));
}
cxgeom::CxSetLineBuildMeta BuildLineScanMeta(double x0,
                                             double y0,
                                             double x1,
                                             double y1,
                                             int gap,
                                             int roi_width,
                                             int roi_height)
{
    cxgeom::CxSetLineBuildMeta meta;
    cxgeom::CxSetLineRequest request;
    request.entity_id = 0;
    request.line_name = "findline_scan";
    request.x0 = x0;
    request.y0 = y0;
    request.z0 = 0.0;
    request.x1 = x1;
    request.y1 = y1;
    request.z1 = 0.0;
    request.gap = PositiveGap(gap);
    request.roi_width = PositiveExtent(roi_width);
    request.roi_height = PositiveExtent(roi_height);

    const cxgeom::CxSetLineBuild builder;
    const cxgeom::CxSetLineBuildResult result = builder.Build(request);
    if (result.success)
        meta = result.meta;
    return meta;
}

int ComputeLineScanCount(double length_hint,
                         int gap,
                         const cxgeom::CxSetLineBuildMeta& meta)
{
    if (gap <= 0)
        return 0;

    int scan_count = ClampSizeToInt(meta.scan_count_hint);
    if (scan_count <= 0 && std::isfinite(length_hint) && length_hint > 0.0)
        scan_count = CeilPositiveToInt(length_hint / static_cast<double>(gap));
    if (scan_count <= 0)
        scan_count = 1;

    if (meta.compact_roi_hint)
        scan_count = std::min(scan_count, 16);

    return std::max(1, scan_count);
}

double ElapsedMilliseconds(const std::chrono::steady_clock::time_point &begin,
                           const std::chrono::steady_clock::time_point &end)
{
    return std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(end - begin).count();
}

struct EdgeChainState
{
    double cost = 0.0;
    int prev_index = -1;
    bool valid = false;
};

double ComputeEdgeTransitionCost(const EdgeBandCandidate& previous,
                                 const EdgeBandCandidate& current)
{
    const double position_delta = previous.scan_type == 0
        ? std::abs(previous.x - current.x)
        : std::abs(previous.y - current.y);
    const double width_delta = std::abs(previous.width - current.width);
    const double polarity_penalty = previous.polarity == current.polarity ? 0.0 : 2.0;
    const double rank_penalty = std::abs(previous.edge_rank - current.edge_rank) * 0.5;
    const double strength_penalty = 1.0 / std::max(0.25, current.response_strength);

    return position_delta + width_delta + polarity_penalty + rank_penalty + strength_penalty;
}
}

int Findline::m_curfindlinenum = 0;
Findline::Findline() :Shape(),
m_ihgap(6),
m_iwgap(6),
m_iSelectPointGap(3),
m_iMethod(1),
m_iThreshold(8),
m_igamarate(0),
m_dsamplerate(0.004),
m_iobjfilterset(1),
m_ifilterborw(21),
m_ifiltermin(50),
m_ifiltermax(100000),
m_iselectedgenum(0),//any
m_ineedfixs(2),
m_icomparegap(2),
m_ishowlines(1),
m_measurepointsboundingRect(gp_Pnt(0,0,0),0,0)
{ 
    string strname = string("fline%1");// .arg(m_curfindlinenum);
    setname(strname.c_str());
    m_curfindlinenum = m_curfindlinenum + 1;
    int icurmodule = ImageManager::GetCurMode();
    if (icurmodule == 0)
    {
        ImageManager::CurMode();
        icurmodule = ImageManager::GetCurMode();
    }
    g_pbackimage = ImageManager::GetBackImage(icurmodule);
    g_pbackfindobject = ImageManager::Getbackfindobject(icurmodule);
    g_pyrimage0 = ImageManager::GetPyrDownImage(icurmodule, 0);
    g_pyrimage1 = ImageManager::GetPyrDownImage(icurmodule, 1);
    g_pyrimage2 = ImageManager::GetPyrDownImage(icurmodule, 2);
}
Findline::~Findline()
{

}
void Findline::setcomparegap(int igap)
{
    m_icomparegap = igap;
}
void Findline::setshow(int ishow)
{
    if (ishow & 0x02)
    {
        m_measurepoints_w.setshow(2);
        m_measurepoints_h.setshow(2);
    }
    if (1 == ishow)
    {
        m_measurepoints_w.setshow(1); 
        m_measurepoints_h.setshow(1); 
    }
    if (0x04 == ishow)
    {    
        for (int i = 0; i < m_lines_w.size(); i++)
        {
            if (i == 0)
                m_lines_w[i].setcolor(255, 255, 0);
            if (i == m_lines_w.size() - 1)
                m_lines_w[i].setcolor(255, 0, 0);
            m_lines_w[i].setshow(true); 
        }
        for (int i = 0; i < m_lines_h.size(); i++)
        {
            if (i == 0)
                m_lines_h[i].setcolor(255, 255, 0);
            if (i == m_lines_h.size() - 1)
                m_lines_h[i].setcolor(255, 0, 0);
            m_lines_h[i].setshow(true);
        }
    }
    else
    {
        for (int i = 0; i < m_lines_w.size(); i++)
            m_lines_w[i].setshow(false);
        for (int i = 0; i < m_lines_h.size(); i++)
            m_lines_h[i].setshow(false); 
    }
    if (8 == ishow)
    {
        /*
        m_measurepointsA.setshow(1);
        m_measurepointsA.setcolor(180, 0, 0);
        m_measurepointsA.MakePointShape();
        m_measurepointsB.setshow(1);
        m_measurepointsB.setcolor(0, 180, 0);
        m_measurepointsB.MakePointShape();

        m_measurepointsA_.setshow(1);
        m_measurepointsA_.setcolor(180, 100, 0);
        m_measurepointsA_.MakePointShape();
        m_measurepointsB_.setshow(1);
        m_measurepointsB_.setcolor(100, 180, 0);
        m_measurepointsB_.MakePointShape();
        */
        m_modelpoints.setshow(2);
         
    }
    if (16 == ishow)
    {
        m_measurepointsA.setcolor(0, 0, 100);
        m_measurepointsA.setshow(1);
        m_measurepointsB.setcolor(0, 100, 150);
        m_measurepointsB.setshow(1);
        m_measurepointsA_.setcolor(100, 0, 100);
        m_measurepointsA_.setshow(1);
        m_measurepointsB_.setcolor(200, 0, 250);
        m_measurepointsB_.setshow(1);
    }
    if (32 == ishow)
    {
        int isize0 = ClampSizeToInt(m_l_measure_w_seek.size());
        int isize1 = ClampSizeToInt(m_l_measure_h_seek.size());
        for (int i = 0; i < isize0; i++)
        {
            if (m_ishowlines == i) 
                m_l_measure_w_seek[i].setshow(1);  
            else
                m_l_measure_w_seek[i].setshow(false);

        }
        for (int i = 0; i < isize1; i++)
        {
            if (m_ishowlines == -i)
                m_l_measure_h_seek[i].setshow(1);
            else
                m_l_measure_h_seek[i].setshow(false);
        }
    }
    //m_LineA.setshow(false);
    //m_LineB.setshow(false);
    //for (int i = 0; i < iplinewsize; i++) 
    //m_lines_w[i].setshow(false);
    //for (int i = 0; i < iplinehsize; i++)
    //m_lines_h[i].setshow(false);
    Shape::setshow(ishow);
}
void Findline::setselectedgenum(int iedgenum)
{
    m_iselectedgenum = iedgenum;
}
void Findline::clear()
{
    m_lines_w.clear();
    m_lines_h.clear();

}
void Findline::SetWHgap(int wgap, int hgap)
{
    m_iwgap = PositiveGap(wgap);
    m_ihgap = PositiveGap(hgap);

    gp_Path path = getpath();
    if (path.ElementCount() < 4)
        return;
    gp_Pnt p0 = path.ElementAt(0);
    gp_Pnt p1 = path.ElementAt(1);
    gp_Pnt p2 = path.ElementAt(2);
    gp_Pnt p3 = path.ElementAt(3);
    (void)p2;
    (void)p3;
  
    if (p0.X() == p1.X() || p0.Y() == p1.Y())
    {

        int ix = RoundToInt(rect().TopLeft().X());
        int iy = RoundToInt(rect().TopLeft().Y());

        int iw = RoundToInt(rect().Width());
        int ih = RoundToInt(rect().Height());
        setrect(ix, iy, iw, ih); 
        Shape::setrect(ix, iy, iw, ih);
    }
    else
    {
       // setlinesegment(double ix0, double iy0,
       //    double ix1, double iy1, double dscale);
    }


}
void Findline::setlinesegment(double ix0, double iy0,
    double ix1, double iy1, double dscale)
{
    if (!std::isfinite(ix0) || !std::isfinite(iy0) || !std::isfinite(ix1) || !std::isfinite(iy1) ||
        !std::isfinite(dscale) || dscale <= 0.0)
        return;

    const double dx = ix1 - ix0;
    const double dy = iy1 - iy0;
    const double dDist = std::sqrt(dx * dx + dy * dy);
    if (!std::isfinite(dDist) || dDist <= 1.0e-9)
        return;

    m_iwgap = PositiveGap(m_iwgap);
    m_ihgap = PositiveGap(m_ihgap);

    gp_Pnt ptRect[4];
    double kk2;

    if (iy0 == iy1)
    {
        ptRect[0].SetX(ix0);
        ptRect[0].SetY(iy0 - dscale);
        ptRect[1].SetX(ix0);
        ptRect[1].SetY(iy0 + dscale);
        ptRect[2].SetX(ix1);
        ptRect[2].SetY(iy1 + dscale);
        ptRect[3].SetX(ix1);
        ptRect[3].SetY(iy1 - dscale);

    }
    else
    {
        kk2 = (ix0 - ix1) / (iy0 - iy1);
        double theta = std::atan(kk2);
        if (theta < 0)
        {
            theta += CV_PI;
        }

        ptRect[0].SetX(ix0 + dscale * std::cos(theta));
        ptRect[0].SetY( iy0 - dscale * std::sin(theta));
        ptRect[1].SetX(ix0 - dscale * std::cos(theta));
        ptRect[1].SetY(iy0 + dscale * std::sin(theta));
        ptRect[2].SetX(ix1 - dscale * std::cos(theta));
        ptRect[2].SetY( iy1 + dscale * std::sin(theta));
        ptRect[3].SetX(ix1 + dscale * std::cos(theta));
        ptRect[3].SetY( iy1 - dscale * std::sin(theta));
    }
    m_LineA.setline(RoundToInt(ptRect[0].X()), RoundToInt(ptRect[0].Y()),
        RoundToInt(ptRect[1].X()), RoundToInt(ptRect[1].Y()));
    m_LineB.setline(RoundToInt(ptRect[1].X()), RoundToInt(ptRect[1].Y()),
        RoundToInt(ptRect[2].X()), RoundToInt(ptRect[2].Y()));
    m_LineA.setshow(0);
    m_LineB.setshow(0);
    for (int i = 0; i < m_lines_w.size(); i++)
    {
        m_lines_w[i].clear();
    }
    for (int i = 0; i < m_lines_h.size(); i++)
    {
        m_lines_h[i].clear();
    }
    m_lines_w.clear();
    m_lines_h.clear();

    for (int i = 0; i < m_l_measure_w_seek.size(); i++)
    {
        m_l_measure_w_seek[i].clear();
    }

    for (int i = 0; i < m_l_measure_h_seek.size(); i++)
    {
        m_l_measure_h_seek[i].clear();
    }
    m_l_measure_w_seek.clear();
    m_l_measure_h_seek.clear();

    double ilinesizeA = m_LineA.getlinedistance();
    double ilinesizeB = m_LineB.getlinedistance();
    const int roi_width_hint = CeilPositiveToInt(std::abs(ptRect[2].X() - ptRect[0].X()));
    const int roi_height_hint = CeilPositiveToInt(std::abs(ptRect[2].Y() - ptRect[0].Y()));
    const cxgeom::CxSetLineBuildMeta line_a_meta =
        BuildLineScanMeta(ptRect[0].X(), ptRect[0].Y(), ptRect[1].X(), ptRect[1].Y(), m_iwgap, roi_width_hint, roi_height_hint);
    const cxgeom::CxSetLineBuildMeta line_b_meta =
        BuildLineScanMeta(ptRect[1].X(), ptRect[1].Y(), ptRect[2].X(), ptRect[2].Y(), m_ihgap, roi_width_hint, roi_height_hint);
    const int dplinewsize = ComputeLineScanCount(ilinesizeA, m_iwgap, line_a_meta);
    const int dplinehsize = ComputeLineScanCount(ilinesizeB, m_ihgap, line_b_meta);

    double dlineax = (ptRect[1].X() - ptRect[0].X()) / dplinewsize;
    double dlineay = (ptRect[1].Y() - ptRect[0].Y()) / dplinewsize;

    double dlinebx = (ptRect[2].X() - ptRect[1].X()) / dplinehsize;
    double dlineby = (ptRect[2].Y() - ptRect[1].Y()) / dplinehsize;


    LineShape aline1, aline2;
  //  PointsShape alinepoints;
    for (int i = 0; i < dplinewsize; i++)
    {
        m_lines_w.push_back(aline1);
        m_lines_w[i].copy(m_LineB);
        m_lines_w[i].Move(RoundToInt(-dlineax * static_cast<double>(i)),
            RoundToInt(-dlineay * static_cast<double>(i)));
        m_lines_w[i].setPercent(m_dsamplerate);
        m_lines_w[i].setshow(0);
    //    m_l_measure_h.push_back(alinepoints);
    }
    for (int i = 0; i < dplinehsize; i++)
    {
        m_lines_h.push_back(aline2);
        m_lines_h[i].copy(m_LineA);
        m_lines_h[i].Move(RoundToInt(dlinebx * static_cast<double>(i)),
            RoundToInt(dlineby * static_cast<double>(i)));
        m_lines_h[i].setPercent(m_dsamplerate);
        m_lines_h[i].setshow(0);
    //    m_l_measure_w.push_back(alinepoints);
    }

    Shape::setrect2(ptRect[0].X(), ptRect[0].Y(), ptRect[1].X(), ptRect[1].Y(),
        ptRect[2].X(), ptRect[2].Y(), ptRect[3].X(), ptRect[3].Y()
    );

}
void Findline::setrect(int ix, int iy, int iw, int ih)
{ 
    m_iwgap = PositiveGap(m_iwgap);
    m_ihgap = PositiveGap(m_ihgap);
    if (iw < 0)
    {
        ix += iw;
        iw = -iw;
    }
    if (ih < 0)
    {
        iy += ih;
        ih = -ih;
    }

    m_LineA.setline(ix, iy, ix + iw, iy);
    m_LineB.setline(ix, iy, ix, iy + ih);
    m_LineA.setshow(false);
    m_LineB.setshow(false);
    for (int i = 0; i < m_lines_w.size(); i++)
    {
        m_lines_w[i].clear();
    }
    for (int i = 0; i < m_lines_h.size(); i++)
    {
        m_lines_h[i].clear();
    }
    m_lines_w.clear();
    m_lines_h.clear();

    for (int i = 0; i < m_l_measure_w_seek.size(); i++)
    {
        m_l_measure_w_seek[i].clear();
    }

    for (int i = 0; i < m_l_measure_h_seek.size(); i++)
    {
        m_l_measure_h_seek[i].clear();
    } 
    m_l_measure_w_seek.clear();
    m_l_measure_h_seek.clear();

    if (iw <= 0 || ih <= 0)
    {
        Shape::setrect(ix, iy, iw, ih);
        return;
    }

    const cxgeom::CxSetLineBuildMeta line_w_meta =
        BuildLineScanMeta(ix, iy, ix + iw, iy, m_iwgap, iw, ih);
    const cxgeom::CxSetLineBuildMeta line_h_meta =
        BuildLineScanMeta(ix, iy, ix, iy + ih, m_ihgap, iw, ih);
    const int iplinewsize = ComputeLineScanCount(static_cast<double>(iw), m_iwgap, line_w_meta);
    const int iplinehsize = ComputeLineScanCount(static_cast<double>(ih), m_ihgap, line_h_meta);
    LineShape aline1, aline2;
 //   PointsShape alinepoints;
    for (int i = 0; i < iplinewsize; i++)
    { 
        m_lines_w.push_back(aline1);
        m_lines_w[i].copy(m_LineB);
        m_lines_w[i].Move(RoundToInt(static_cast<double>(m_iwgap) * static_cast<double>(i)), 0);
        m_lines_w[i].setPercent(m_dsamplerate);
        m_lines_w[i].setshow(false);
 //       m_l_measure_w.push_back(alinepoints);
    }
    for (int i = 0; i < iplinehsize; i++)
    { 
        m_lines_h.push_back(aline2);
        m_lines_h[i].copy(m_LineA);
        m_lines_h[i].Move(0, RoundToInt(static_cast<double>(m_ihgap) * static_cast<double>(i)));
        m_lines_h[i].setPercent(m_dsamplerate);
        m_lines_h[i].setshow(false);
  //      m_l_measure_h.push_back(alinepoints);
    }

    Shape::setrect(ix, iy, iw, ih);
}
void Findline::getshape(void* pshape)
{
    Shape* pshape0 = (Shape*)pshape;
    if (pshape0 == nullptr)
        return;

    const gp_Rectangle arect = rect();
    pshape0->setrect(RoundToInt(arect.TopLeft().X()),
        RoundToInt(arect.TopLeft().Y()),
        RoundToInt(arect.Width()),
        RoundToInt(arect.Height()));
}
void Findline::translate(int ix,int iy)
{
    Translate(gp_Vec(ix, iy, 0)); 
}
void Findline::Translate(const gp_Vec& translationVector)
{ 
   int ix0 = RoundToInt(translationVector.X());
   int iy0 = RoundToInt(translationVector.Y());
   getpath().Translate(translationVector);
    m_LineA.Move(ix0, iy0);
    m_LineB.Move(ix0, iy0);
    LineShape aline1, aline2;
    for (int i = 0; i < m_lines_w.size(); i++)
    { 
        m_lines_w[i].Move(ix0, iy0);
    }
    for (int i = 0; i < m_lines_h.size(); i++)
    {
        m_lines_h[i].Move(ix0, iy0);
    } 
}
void Findline::drawpattern()
{ 
    m_modelpoints.setshow(8);
    m_modelpoints.drawshape(getpath());
    m_measurepointsA.drawshape(getpath());
    m_measurepointsB.drawshape(getpath());
    m_measurepointsA_.drawshape(getpath());
    m_measurepointsB_.drawshape(getpath());

}
void Findline::drawpatternx(double dmovx, double dmovy,
    double dangle,double dzoomx, double dzoomy)
{
    m_modelpoints.setshow(8);
    m_modelpoints.drawshapex(getpath(), dmovx, dmovy,
        dangle, dzoomx, dzoomy);
    m_measurepointsA.drawshapex(getpath(), dmovx, dmovy,
        dangle, dzoomx, dzoomy);
    m_measurepointsB.drawshapex(getpath(), dmovx, dmovy,
        dangle, dzoomx, dzoomy);
    m_measurepointsA_.drawshapex(getpath(), dmovx, dmovy,
        dangle, dzoomx, dzoomy);
    m_measurepointsB_.drawshapex(getpath(), dmovx, dmovy,
        dangle, dzoomx, dzoomy);
}
void Findline::edgepattern(Image& image)
{
    m_measurepointsA.clear();
    m_measurepointsB.clear();
    m_measurepointsA_.clear();
    m_measurepointsB_.clear();
    m_modelpoints.clear();

    SetWHgap(wgap(), hgap());
    setselectedgenum(0);
    setmethod(0);
    Measure(image);
    SmartFilter(-1, -1);
    m_measurepointsA.addpoints(getresultpointsw());
    m_measurepointsB.addpoints(getresultpointsh());

    m_measurepointsA.doublepattern(m_icomparegap, 12, m_modelpoints);
    m_measurepointsB.doublepattern(m_icomparegap, 9, m_modelpoints);
    
    setselectedgenum(0);
    setmethod(1);
    Measure(image);
    SmartFilter(-1, -1);
    m_measurepointsA_.addpoints(getresultpointsw());
    m_measurepointsB_.addpoints(getresultpointsh());

    //m_measurepointsA.doublepattern(m_icomparegap, 12, m_modelpoints);
    //m_measurepointsB.doublepattern(m_icomparegap, 9, m_modelpoints);
    m_measurepointsA_.doublepattern(m_icomparegap, 6, m_modelpoints);
    m_measurepointsB_.doublepattern(m_icomparegap, 3, m_modelpoints);

}
void Findline::patternzeroposition()
{
    gp_Rectangle arect1 = m_modelpoints.boundingRectAB();
    m_modelpoints.MoveAB(RoundToInt(-arect1.TopLeft().X()), RoundToInt(-arect1.TopLeft().Y()));

}
void Findline::savepatternfile(const char* pchar)
{
    //m_modelpoints.save(pchar);
    saveABpatternfile(pchar);
}
void Findline::loadpatternfile(const char* pchar)
{
    //m_modelpoints.load(pchar);
    loadABpatternfile(pchar);
}
void Findline::samplemodelAB(int inum)
{
    m_modelpoints.resampleAB(inum);
}
void Findline::ABtoShape(std::vector<cv::Point2f>& points)
{
    m_modelpoints.ABtoShape(points);
}
void Findline::saveABpatternfile(const char* pchar)
{
    m_modelpoints.saveAB(pchar);
}
void Findline::loadABpatternfile(const char* pchar)
{
    m_modelpoints.loadAB(pchar);
}
int Findline::ABpatternsize()
{
    return m_modelpoints.ABsize();
}
gp_Rectangle Findline::patternboundingrect()
{
    return m_modelpoints.boundingRect();
}
gp_Rectangle Findline::patternboundingrectAB()
{
    return m_modelpoints.boundingRectAB();
}
gp_Rectangle Findline::patternboundingrectA()
{
    return m_modelpoints.boundingRectA();
}
gp_Rectangle Findline::patternboundingrectB()
{
    return m_modelpoints.boundingRect();
}

void Findline::patterngap2gap(int inewgap)
{
    m_modelpoints.patterngap2gap(inewgap);
}
void Findline::patternABgap2gap(double dnewgaprate)
{
    m_modelpoints.patternABgap2gap(dnewgaprate);
}
void Findline::patternABsample(int irate)
{
    m_modelpoints.patternABsample(irate);
}
void Findline::pattern2org()
{
    m_modelpoints_org.copy(m_modelpoints);
}
void Findline::org2pattern()
{
    m_modelpoints.copy(m_modelpoints_org);
}
void Findline::patternrootgrid(double itype, double drate, double ilevel)
{
    m_modelpoints.keysrootgrid(RoundToInt(itype), drate, RoundToInt(ilevel));
}
void Findline::patterntranform(int igap, int itype, int isgap, int iline)
{
    m_modelpoints.patterntranform(igap, itype, isgap, iline);
}
void Findline::patternzoom(double dx, double dy, double igap, double itype)
{
    m_modelpoints.patternzoom(dx, dy, RoundToInt(igap), RoundToInt(itype));
}
void Findline::patternrotate(double dangle)
{
    m_modelpoints.RotateAB(dangle);
}
void Findline::modelzoom(double dx, double dy)
{
    m_modelpoints.Zoom(dx, dy);
}
gp_Path& Findline::getpatternpath()
{
    return m_modelpoints.getpath();
}
gp_Path& Findline::getpatternpathA()
{
    return m_modelpoints.getpathA();
}
gp_Path& Findline::getpatternpathB()
{
    return m_modelpoints.getpathB();
}
PointsShape& Findline::getpattern()
{
    return m_modelpoints;
}

void Findline::patternfilter(double distanceThreshold , double waveletThreshold)
{
    (void)distanceThreshold;
    (void)waveletThreshold;
#if defined USE_AI___
    //void AdaptiveDistfilter();
    //m_modelpoints.FftWaveletTransform( distanceThreshold ,  waveletThreshold );
    gp_Path& path = m_modelpoints.getpath();
    size_t numPoints = path.getpoints().size();
    arma::mat points(2, numPoints);
    for (size_t i = 0; i < numPoints; ++i)
    {
        points(0, i) = path.getpoints()[i].X(); //   一     x     
        points(1, i) = path.getpoints()[i].Y(); //  诙      y     
    }
    //path.Clear();
    //  缘  平  蟹   
    auto clusters = mlpackclass::ClusterPointCloud_(points, distanceThreshold);
    //vector<PointsShape>
    //    每            突   小        路   侄谓  
    std::cout << "        " << std::endl;
    //m_modelsegments
    for (int i = 0; i < m_modelsegments.size(); i++)
    {
        m_modelsegments[i].clear();
    }
    m_modelsegments.clear();
    for (const auto& [clusterId, clusterPoints] : clusters)
    {
        std::cout << "Cluster " << clusterId << ":" << std::endl;

        //   路      小       侄 
        std::vector<std::vector<size_t>> segments = mlpackclass::SegmentPathByWaveletAnalysis(points, clusterPoints, waveletThreshold);

        PointsShape segmentspoints;
        //     侄谓  
        for (size_t segIdx = 0; segIdx < segments.size(); ++segIdx)
        {
            std::cout << "   侄  " << segIdx + 1 << ": ";
            for (size_t pointIdx : segments[segIdx])
            {
                segmentspoints.addpoint(gp_Pnt(points(0, pointIdx), points(1, pointIdx), 0));
                std::cout << "(" << points(0, pointIdx) << ", " << points(1, pointIdx) << ") ";
            }
            std::cout << std::endl;
        }
        m_modelsegments.push_back(segmentspoints);
    }
#endif
}
void Findline::findpattern(void* pimage)
{
    Image* pgetimage = (Image*)pimage;
    if (pgetimage == nullptr)
        return;
    edgepattern(*pgetimage);
}
void Findline::drawshape()
{
    if (show() & 0x04)
    {
        m_LineA.setPen(255, 0, 0);
        m_LineB.setPen(255, 0, 0);
        m_LineA.drawshape(getpath());
        m_LineB.drawshape(getpath());
        //painter.setPen(QPen(QColor(0, 255, 0)));
        for (int i = 0; i < m_lines_w.size(); i++)
        {
            m_lines_w[i].drawshape(getpath());
        }
        for (int i = 0; i < m_lines_h.size(); i++)
        {
            m_lines_h[i].drawshape(getpath());
        }
    }
    if (show() & 0x02)
    {
        m_measurepoints_w.drawshape(getpath());
        m_measurepoints_h.drawshape(getpath());
    }
    if (show() & 0x08)
    {
        drawpattern();
    }
    Shape::drawshape();
}
void Findline::drawshapex(double dmovx,double dmovy,double dangle,double dzoomx, double dzoomy)
{
    if (show() & 0x04)
    {
        m_LineA.setPen(255, 0, 0);
        m_LineB.setPen(255, 0, 0);
        m_LineA.drawshape(getpath());
        m_LineB.drawshape(getpath());
        //painter.setPen((0, 255, 0));
        for (int i = 0; i < m_lines_w.size(); i++)
        {
            m_lines_w[i].drawshape(getpath());
        }
        for (int i = 0; i < m_lines_h.size(); i++)
        {
            m_lines_h[i].drawshape(getpath());
        }
    }
    if (show() & 0x02)
    {
        m_measurepoints_w.drawshapex(getpath(), dmovx, dmovy,
            dangle, dzoomx, dzoomy);
        m_measurepoints_h.drawshapex(getpath(), dmovx, dmovy,
            dangle, dzoomx, dzoomy);
    }
    if (show() & 0x08)
    {
        drawpatternx( dmovx, dmovy,
            dangle, dzoomx, dzoomy);
    }
    Shape::drawshapex( dmovx, dmovy, dangle, dzoomx, dzoomy);
}
void Findline::setlinesamplerate(double dsamplerate)
{
    m_dsamplerate = dsamplerate;
}
void Findline::setlinegap(int igap)
{
    m_iSelectPointGap = igap;
}
void Findline::setmethod(int imethod)
{
    m_iMethod = imethod;
}
void Findline::setthre(int ithre)
{
    m_iThreshold = ithre;
}
int Findline::thre()
{
    return m_iThreshold;
}
void Findline::setgamarate(int igama)
{
    m_igamarate = igama;
}
void Findline::setobjfilter(int ifindset)
{
    m_iobjfilterset = ifindset;
}
void Findline::setfilter(int ifilterborw, int ifiltermin, int ifiltermax)
{
    m_ifilterborw = ifilterborw;
    m_ifiltermin = ifiltermin;
    m_ifiltermax = ifiltermax;
}
void Findline::MeasureT(void *pimage)
{
    Image* image = (Image*)pimage;
    if (image == nullptr || g_pbackimage == nullptr)
        return;

    if (rect().TopLeft().X() < 0 || rect().TopLeft().Y() < 0)
        return;//error process
    m_measurepoints_w.clear();
    m_measurepoints_h.clear();
    int iwsize = ClampSizeToInt(m_lines_w.size());
    int ihsize = ClampSizeToInt(m_lines_h.size());
    for (int i = 0; i < iwsize; i++)
    {
        m_lines_w[i].linecopyex(*image, *g_pbackimage, 0, i);
    }
    for (int i = 0; i < ihsize; i++)
    {
        m_lines_h[i].linecopyex(*image, *g_pbackimage, 0, i + iwsize);
    }
    int ilineslen1 = 0;
    int ilineslen2 = 0;

    if (iwsize > 0)
        ilineslen1 = m_lines_w[0].getlinesize();
    if (ihsize > 0)
        ilineslen2 = m_lines_h[0].getlinesize();

    int iprocessw = ilineslen1 > ilineslen2 ? ilineslen1 : ilineslen2;

    g_pbackimage->setroi(0, 0, iprocessw, iwsize + ihsize);

    g_pbackimage->roi_7blur_gap_mud_thre_bw(m_iThreshold, m_igamarate, m_iSelectPointGap, m_iMethod);

}
void Findline::Measure(Image& image)
{
    if (g_pbackimage == nullptr)
        return;

    if (image.getWidth() < rect().TopLeft().X() + rect().Width()
        || image.getHeight() < rect().TopLeft().Y() + rect().Height())
        return;//error process
    if (rect().TopLeft().X() < 0 || rect().TopLeft().Y() < 0)
        return;//error process
    m_measurepoints_w.clear();
    m_measurepoints_h.clear();
    int iwsize = ClampSizeToInt(m_lines_w.size());
    int ihsize = ClampSizeToInt(m_lines_h.size());
    for (int i = 0; i < iwsize; i++)
    {
        m_lines_w[i].linecopyex(image, *g_pbackimage, 0, i);
    }
    for (int i = 0; i < ihsize; i++)
    {
        m_lines_h[i].linecopyex(image, *g_pbackimage, 0, i + iwsize);
    }
    int ilineslen1 = 0;
    int ilineslen2 = 0;

    if (iwsize > 0)
        ilineslen1 = m_lines_w[0].getlinesize();
    if (ihsize > 0)
        ilineslen2 = m_lines_h[0].getlinesize();

    int iprocessw = ilineslen1 > ilineslen2 ? ilineslen1 : ilineslen2;

    g_pbackimage->setroi(0, 0, iprocessw, iwsize + ihsize);

    g_pbackimage->roi_7blur_gap_mud_thre_bw(m_iThreshold, m_igamarate, m_iSelectPointGap, m_iMethod);

    if ((m_iobjfilterset & 0x01) && g_pbackfindobject != nullptr)
    {
         g_pbackfindobject->setrect(0, 0, iprocessw, iwsize + ihsize);
         g_pbackfindobject->setbrow(m_ifilterborw);//21 22
         g_pbackfindobject->setminmaxarea(ClampLongLongToInt(static_cast<long long>(m_ifiltermin)),
             ClampLongLongToInt(static_cast<long long>(m_ifiltermax)));
         g_pbackfindobject->Measure(*g_pbackimage);
         //mask 
    }

    int irecordpoint[100];
    int irecordnum = 0;
    bool bcollectBegin = false;

    int icurlinenum = 0;
    int icurlineposition = 0;

    int ifixvalue = 3;

    cv::Vec3b icolor = 0;
    for (int inumy = 0 + ifixvalue; inumy < iwsize - ifixvalue; inumy++)
    {
        irecordnum = 0;
        icurlinenum = 0;
        bcollectBegin = false;


        for (int inumx = 0; inumx < ilineslen1; inumx++)
        {
            icolor = g_pbackimage->pixel(inumx, inumy);
            if ((icolor[0]) > 0)
            {
                if (irecordnum < 100)
                {
                    irecordpoint[irecordnum] = inumx;
                    irecordnum++;
                }
                else
                {
                    irecordnum = 0;
                    break;
                }
                bcollectBegin = true;
            }
            else
            {
                if (true == bcollectBegin
                    && irecordnum > 0
                    && irecordnum <= 70)
                {
                    icurlineposition = m_ineedfixs + irecordpoint[(irecordnum >> 1)];
                    //icurlineposition = icurlineposition>ilineslen1?ilineslen1-1:icurlineposition;

                    icurlinenum++;
                    if (icurlinenum == m_iselectedgenum
                        || m_iselectedgenum == 0)//0 any
                    {
                        if (icurlineposition<(ilineslen1 - m_iSelectPointGap - 3)
                            && icurlineposition>m_iSelectPointGap + 3)
                        {
                            gp_Pnt apoint = m_lines_w[inumy].getlinepoint(icurlineposition);
                            m_measurepoints_w.addpoint(apoint);
                            //m_l_measure_w[icurlineposition].addpoint(apoint);
                            if (icurlinenum == m_iselectedgenum)
                                break;
                        }
                    }
                }
                irecordnum = 0;
                bcollectBegin = false;
            }
        }
        if (true == bcollectBegin
            && irecordnum > 0)
        {
            icurlineposition = m_ineedfixs + irecordpoint[(irecordnum >> 1)];
            //icurlineposition = icurlineposition>ilineslen1?ilineslen1-1:icurlineposition;
            icurlinenum++;
            if (icurlinenum == m_iselectedgenum
                || m_iselectedgenum == 0)
            {
                if (icurlineposition<(ilineslen1 - m_iSelectPointGap - 3)
                    && icurlineposition>m_iSelectPointGap + 3)
                {
                    gp_Pnt apoint = m_lines_w[inumy].getlinepoint(icurlineposition);
                    m_measurepoints_w.addpoint(apoint);
                    //m_l_measure_w[icurlineposition].addpoint(apoint);
                    if (icurlinenum == m_iselectedgenum)
                        break;
                }
            }
            irecordnum = 0;
            bcollectBegin = false;
        }

    }

    bcollectBegin = false;
    for (int inumy = iwsize + ifixvalue; inumy < iwsize + ihsize - ifixvalue; inumy++)
    {
        irecordnum = 0;
        icurlinenum = 0;
        bcollectBegin = false;



        for (int inumx = 0; inumx < ilineslen2; inumx++)
        {
            icolor = g_pbackimage->pixel(inumx, inumy);
            if ((icolor[0]) > 0)
            {
                if (irecordnum < 100)
                {
                    irecordpoint[irecordnum] = inumx;
                    irecordnum++;
                }
                else
                {
                    irecordnum = 0;
                    break;
                }
                bcollectBegin = true;
            }
            else
            {
                if (true == bcollectBegin
                    && irecordnum > 0
                    && irecordnum <= 70)
                {
                    icurlineposition = m_ineedfixs + irecordpoint[(irecordnum >> 1)];
                    //icurlineposition = icurlineposition>ilineslen2?ilineslen2-1:icurlineposition;
                    icurlinenum++;
                    if (icurlinenum == m_iselectedgenum
                        || m_iselectedgenum == 0)
                    {
                        if (icurlineposition<(ilineslen2 - m_iSelectPointGap - 3)
                            && icurlineposition>m_iSelectPointGap + 3)
                        {
                            gp_Pnt apoint = m_lines_h[inumy - iwsize].getlinepoint(icurlineposition);
                            m_measurepoints_h.addpoint(apoint);
                            //m_l_measure_h[icurlineposition].addpoint(apoint);

                            if (icurlinenum == m_iselectedgenum)
                                break;
                        }
                    }
                }
                irecordnum = 0;
                bcollectBegin = false;
            }
        }
        if (true == bcollectBegin
            && irecordnum > 0)
        {
            icurlineposition = m_ineedfixs + irecordpoint[(irecordnum >> 1)];
            // icurlineposition = icurlineposition>ilineslen2?ilineslen2-1:icurlineposition;

            icurlinenum++;
            if (icurlinenum == m_iselectedgenum
                || m_iselectedgenum == 0)
            {
                if (icurlineposition<(ilineslen2 - m_iSelectPointGap - 3)
                    && icurlineposition>m_iSelectPointGap + 3)
                {
                    gp_Pnt apoint = m_lines_h[inumy - iwsize].getlinepoint(icurlineposition);
                    m_measurepoints_h.addpoint(apoint);
                   //m_l_measure_h[icurlineposition].addpoint(apoint);

                    if (icurlinenum == m_iselectedgenum)
                        break;
                }
            }
            irecordnum = 0;
            bcollectBegin = false;
        }
    }
    /*
    int iwpointstotal = 0;
    int ihpointstotal = 0;
    int imaxpointsline = 0;
    int icurpoints = 0;
    int iprepointsnum = 0;
    int inextpointsnum = 0;
    for (int i = 0; i < m_l_measure_w.size(); i++)
    {
        icurpoints =  m_l_measure_w[i].size();
        iwpointstotal = iwpointstotal + icurpoints;
        if (i+1 < m_l_measure_w.size()-1)
        {
            inextpointsnum = m_l_measure_w[i+1].size();
        }
        if (imaxpointsline < icurpoints && inextpointsnum < icurpoints)
        {
            
            if(imaxpointsline < icurpoints)
                imaxpointsline = icurpoints;

        }
        
        iprepointsnum = icurpoints;
    }
    for (int i = 0; i < m_l_measure_h.size(); i++)
    {
        ihpointstotal = ihpointstotal + m_l_measure_w[i].size(); 
    }
    */

}
void Findline::MeasureBalanced(Image& image)
{
    FindlineMeasureProfileStats stats;
    const std::chrono::steady_clock::time_point total_begin = std::chrono::steady_clock::now();

    ClearMeasureState();
    BuildScanProfiles(image, stats);
    CollectAllEdgeBands(image, stats);
    BuildEdgeBandGraph(stats);
    SolveBestEdgeChain(stats);
    RefineBestChainSubpixel(image, stats);
    RefineJointConsistency(stats);
    ConvertBestChainToMeasurePoints(stats);
    FilterMeasurePoints(stats);
    FitWeightedLeastSquares(stats);

    stats.point_count = m_measurepoints_w.size() + m_measurepoints_h.size();
    stats.chain_length = static_cast<int>(m_bestEdgeChain.size());
    stats.total_ms = ElapsedMilliseconds(total_begin, std::chrono::steady_clock::now());
    m_lastMeasureProfile = stats;

}

void Findline::ClearMeasureState()
{
    m_scanEdgeBands.clear();
    m_bestEdgeChain.clear();
    m_lastMeasureProfile = FindlineMeasureProfileStats();
    m_measurepoints_w.clear();
    m_measurepoints_h.clear();
}

void Findline::BuildScanProfiles(Image& image, FindlineMeasureProfileStats& stats)
{
    const std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    if (g_pbackimage == nullptr)
    {
        stats.profile_ms = ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
        return;
    }

    if (image.getWidth() < rect().TopLeft().X() + rect().Width()
        || image.getHeight() < rect().TopLeft().Y() + rect().Height()
        || rect().TopLeft().X() < 0
        || rect().TopLeft().Y() < 0)
    {
        stats.profile_ms = ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
        return;
    }

    const int iwsize = ClampSizeToInt(m_lines_w.size());
    const int ihsize = ClampSizeToInt(m_lines_h.size());

    for (int i = 0; i < iwsize; ++i)
    {
        m_lines_w[i].linecopyex(image, *g_pbackimage, 0, i);
    }
    for (int i = 0; i < ihsize; ++i)
    {
        m_lines_h[i].linecopyex(image, *g_pbackimage, 0, i + iwsize);
    }

    int ilineslen1 = 0;
    int ilineslen2 = 0;
    if (iwsize > 0)
        ilineslen1 = m_lines_w[0].getlinesize();
    if (ihsize > 0)
        ilineslen2 = m_lines_h[0].getlinesize();

    const int iprocessw = ilineslen1 > ilineslen2 ? ilineslen1 : ilineslen2;
    g_pbackimage->setroi(0, 0, iprocessw, iwsize + ihsize);
    g_pbackimage->roi_7blur_gap_mud_thre_bw(m_iThreshold, m_igamarate, m_iSelectPointGap, m_iMethod);

    if ((m_iobjfilterset & 0x01) && g_pbackfindobject != nullptr)
    {
        g_pbackfindobject->setrect(0, 0, iprocessw, iwsize + ihsize);
        g_pbackfindobject->setbrow(m_ifilterborw);
        g_pbackfindobject->setminmaxarea(ClampLongLongToInt(static_cast<long long>(m_ifiltermin)),
            ClampLongLongToInt(static_cast<long long>(m_ifiltermax)));
        g_pbackfindobject->Measure(*g_pbackimage);
    }

    stats.profile_ms = ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
}

void Findline::CollectAllEdgeBands(Image& image, FindlineMeasureProfileStats& stats)
{
    (void)image;
    const std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    if (g_pbackimage == nullptr)
    {
        stats.edgeband_ms = ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
        return;
    }

    const int ifixvalue = 3;
    const int max_band_width = 70;
    const int iwsize = ClampSizeToInt(m_lines_w.size());
    const int ihsize = ClampSizeToInt(m_lines_h.size());
    const int ilineslen1 = iwsize > 0 ? m_lines_w[0].getlinesize() : 0;
    const int ilineslen2 = ihsize > 0 ? m_lines_h[0].getlinesize() : 0;

    auto append_scan_band = [&](int scan_type, int line_index, int row_index, int line_length)
    {
        ScanLineEdgeBands scan_bands;
        scan_bands.scan_index = static_cast<int>(m_scanEdgeBands.size());
        scan_bands.scan_type = scan_type;

        bool collecting = false;
        int band_start = 0;
        int band_rank = 0;

        auto finalize_band = [&](int band_end)
        {
            if (!collecting)
                return;

            const int band_width = band_end - band_start + 1;
            collecting = false;
            if (band_width <= 0 || band_width > max_band_width)
                return;

            const int center_index = m_ineedfixs + band_start + (band_width >> 1);
            if (center_index <= m_iSelectPointGap + 3 ||
                center_index >= line_length - m_iSelectPointGap - 3)
                return;

            EdgeBandCandidate candidate;
            candidate.scan_index = scan_bands.scan_index;
            candidate.scan_type = scan_type;
            candidate.line_index = line_index;
            candidate.candidate_index = static_cast<int>(scan_bands.bands.size());
            candidate.start_index = band_start;
            candidate.end_index = band_end;
            candidate.center_index = center_index;
            candidate.width = static_cast<double>(band_width);
            candidate.edge_rank = band_rank++;
            candidate.response_strength = static_cast<double>(band_width);
            candidate.polarity = m_iMethod == 0 ? 1.0 : -1.0;
            candidate.valid = (m_iselectedgenum == 0 || (candidate.edge_rank + 1) == m_iselectedgenum);

            const gp_Pnt point = scan_type == 0
                ? m_lines_w[line_index].getlinepoint(center_index)
                : m_lines_h[line_index].getlinepoint(center_index);
            candidate.x = point.X();
            candidate.y = point.Y();
            scan_bands.bands.push_back(candidate);
        };

        for (int col_index = 0; col_index < line_length; ++col_index)
        {
            const cv::Vec3b color = g_pbackimage->pixel(col_index, row_index);
            const bool active = color[0] > 0;
            if (active && !collecting)
            {
                collecting = true;
                band_start = col_index;
            }
            else if (!active && collecting)
            {
                finalize_band(col_index - 1);
            }
        }

        if (collecting)
            finalize_band(line_length - 1);

        if (!scan_bands.bands.empty())
            m_scanEdgeBands.push_back(scan_bands);
    };

    for (int line_index = ifixvalue; line_index < iwsize - ifixvalue; ++line_index)
        append_scan_band(0, line_index, line_index, ilineslen1);

    for (int line_index = ifixvalue; line_index < ihsize - ifixvalue; ++line_index)
        append_scan_band(1, line_index, iwsize + line_index, ilineslen2);

    stats.edgeband_count = 0;
    for (size_t i = 0; i < m_scanEdgeBands.size(); ++i)
        stats.edgeband_count += static_cast<int>(m_scanEdgeBands[i].bands.size());

    stats.edgeband_ms = ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
}

void Findline::BuildEdgeBandGraph(FindlineMeasureProfileStats& stats)
{
    const std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    stats.graph_ms = ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
}

void Findline::SolveBestEdgeChain(FindlineMeasureProfileStats& stats)
{
    const std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    m_bestEdgeChain.clear();
    if (m_scanEdgeBands.empty())
    {
        stats.path_ms = ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
        return;
    }

    std::vector<std::vector<EdgeChainState>> states(m_scanEdgeBands.size());
    for (size_t scan_idx = 0; scan_idx < m_scanEdgeBands.size(); ++scan_idx)
    {
        const ScanLineEdgeBands& scan_bands = m_scanEdgeBands[scan_idx];
        states[scan_idx].resize(scan_bands.bands.size());

        for (size_t band_idx = 0; band_idx < scan_bands.bands.size(); ++band_idx)
        {
            EdgeChainState& state = states[scan_idx][band_idx];
            const EdgeBandCandidate& current = scan_bands.bands[band_idx];
            if (!current.valid)
                continue;

            const double base_cost = 1.0 / std::max(0.25, current.response_strength);
            if (scan_idx == 0)
            {
                state.cost = base_cost;
                state.prev_index = -1;
                state.valid = true;
                continue;
            }

            const ScanLineEdgeBands& previous_scan = m_scanEdgeBands[scan_idx - 1];
            for (size_t prev_idx = 0; prev_idx < previous_scan.bands.size(); ++prev_idx)
            {
                const EdgeChainState& previous_state = states[scan_idx - 1][prev_idx];
                if (!previous_state.valid)
                    continue;

                const double transition_cost = ComputeEdgeTransitionCost(previous_scan.bands[prev_idx], current);
                const double total_cost = previous_state.cost + transition_cost + base_cost;
                if (!state.valid || total_cost < state.cost)
                {
                    state.cost = total_cost;
                    state.prev_index = static_cast<int>(prev_idx);
                    state.valid = true;
                }
            }
        }
    }

    int best_final_index = -1;
    double best_final_cost = 0.0;
    const size_t last_scan_idx = m_scanEdgeBands.size() - 1;
    for (size_t band_idx = 0; band_idx < states[last_scan_idx].size(); ++band_idx)
    {
        const EdgeChainState& state = states[last_scan_idx][band_idx];
        if (!state.valid)
            continue;
        if (best_final_index < 0 || state.cost < best_final_cost)
        {
            best_final_index = static_cast<int>(band_idx);
            best_final_cost = state.cost;
        }
    }

    if (best_final_index >= 0)
    {
        int current_index = best_final_index;
        for (int scan_idx = static_cast<int>(last_scan_idx); scan_idx >= 0 && current_index >= 0; --scan_idx)
        {
            m_bestEdgeChain.push_back(m_scanEdgeBands[scan_idx].bands[current_index]);
            current_index = states[scan_idx][current_index].prev_index;
        }
        std::reverse(m_bestEdgeChain.begin(), m_bestEdgeChain.end());
    }

    stats.path_ms = ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
}

void Findline::ConvertBestChainToMeasurePoints(FindlineMeasureProfileStats& stats)
{
    const std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    m_measurepoints_w.clear();
    m_measurepoints_h.clear();

    for (size_t i = 0; i < m_bestEdgeChain.size(); ++i)
    {
        const EdgeBandCandidate& candidate = m_bestEdgeChain[i];
        if (!candidate.valid)
            continue;

        if (candidate.scan_type == 0)
        {
            Standard_Real x = candidate.x;
            Standard_Real y = candidate.y;
            m_measurepoints_w.addpoint(x, y);
        }
        else
        {
            Standard_Real x = candidate.x;
            Standard_Real y = candidate.y;
            m_measurepoints_h.addpoint(x, y);
        }
    }

    stats.graph_ms += ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
}

void Findline::RefineBestChainSubpixel(Image& image, FindlineMeasureProfileStats& stats)
{
    const std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    double total_adjust = 0.0;
    int adjusted_count = 0;
    auto sample_intensity = [&](const EdgeBandCandidate& candidate, int sample_index) -> double
    {
        gp_Pnt point = candidate.scan_type == 0
            ? m_lines_w[candidate.line_index].getlinepoint(sample_index)
            : m_lines_h[candidate.line_index].getlinepoint(sample_index);

        const int px = static_cast<int>(std::round(point.X()));
        const int py = static_cast<int>(std::round(point.Y()));
        if (px < 0 || py < 0 || px >= image.getWidth() || py >= image.getHeight())
            return 0.0;
        const cv::Vec3b pixel = image.pixel(px, py);
        return static_cast<double>(pixel[0]) + static_cast<double>(pixel[1]) + static_cast<double>(pixel[2]);
    };

    for (size_t i = 0; i < m_bestEdgeChain.size(); ++i)
    {
        EdgeBandCandidate& candidate = m_bestEdgeChain[i];
        if (!candidate.valid)
            continue;

        if (candidate.scan_type == 0)
        {
            if (candidate.line_index < 0 || candidate.line_index >= static_cast<int>(m_lines_w.size()))
                continue;
        }
        else
        {
            if (candidate.line_index < 0 || candidate.line_index >= static_cast<int>(m_lines_h.size()))
                continue;
        }

        const int line_size = candidate.scan_type == 0
            ? m_lines_w[candidate.line_index].getlinesize()
            : m_lines_h[candidate.line_index].getlinesize();
        const int center = candidate.center_index;
        if (center - 2 < 0 || center + 2 >= line_size)
            continue;

        const double intensity_m2 = sample_intensity(candidate, center - 2);
        const double intensity_m1 = sample_intensity(candidate, center - 1);
        const double intensity_0 = sample_intensity(candidate, center);
        const double intensity_p1 = sample_intensity(candidate, center + 1);
        const double intensity_p2 = sample_intensity(candidate, center + 2);

        const double grad_left = std::abs(intensity_0 - intensity_m2);
        const double grad_center = std::abs(intensity_p1 - intensity_m1);
        const double grad_right = std::abs(intensity_p2 - intensity_0);
        const double denom = grad_left - 2.0 * grad_center + grad_right;
        double delta = 0.0;
        if (std::abs(denom) > 1e-6)
            delta = 0.5 * (grad_left - grad_right) / denom;
        delta = std::max(-0.5, std::min(0.5, delta));
        total_adjust += std::abs(delta);
        ++adjusted_count;

        gp_Pnt base_point = candidate.scan_type == 0
            ? m_lines_w[candidate.line_index].getlinepoint(center)
            : m_lines_h[candidate.line_index].getlinepoint(center);
        gp_Pnt neighbor_point = delta >= 0.0
            ? (candidate.scan_type == 0
                ? m_lines_w[candidate.line_index].getlinepoint(center + 1)
                : m_lines_h[candidate.line_index].getlinepoint(center + 1))
            : (candidate.scan_type == 0
                ? m_lines_w[candidate.line_index].getlinepoint(center - 1)
                : m_lines_h[candidate.line_index].getlinepoint(center - 1));

        const double blend = std::abs(delta);
        candidate.x = base_point.X() + (neighbor_point.X() - base_point.X()) * blend;
        candidate.y = base_point.Y() + (neighbor_point.Y() - base_point.Y()) * blend;
    }
    stats.subpixel_adjust_avg = adjusted_count > 0 ? total_adjust / adjusted_count : 0.0;
    stats.subpixel_ms = ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
}

void Findline::FilterMeasurePoints(FindlineMeasureProfileStats& stats)
{
    const std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    SmartFilter(-1, -1);
    stats.graph_ms += 0.0;
    ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
}

void Findline::FitWeightedLeastSquares(FindlineMeasureProfileStats& stats)
{
    const std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    if (m_bestEdgeChain.size() < 2)
    {
        stats.fit_ms = ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
        return;
    }

    double weight_sum = 0.0;
    double mean_x = 0.0;
    double mean_y = 0.0;
    for (size_t i = 0; i < m_bestEdgeChain.size(); ++i)
    {
        const EdgeBandCandidate& candidate = m_bestEdgeChain[i];
        if (!candidate.valid)
            continue;
        const double weight = std::max(0.25, candidate.response_strength);
        weight_sum += weight;
        mean_x += candidate.x * weight;
        mean_y += candidate.y * weight;
    }
    if (weight_sum <= 0.0)
    {
        stats.fit_ms = ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
        return;
    }

    mean_x /= weight_sum;
    mean_y /= weight_sum;

    double cov_xx = 0.0;
    double cov_xy = 0.0;
    double cov_yy = 0.0;
    for (size_t i = 0; i < m_bestEdgeChain.size(); ++i)
    {
        const EdgeBandCandidate& candidate = m_bestEdgeChain[i];
        if (!candidate.valid)
            continue;
        const double weight = std::max(0.25, candidate.response_strength);
        const double dx = candidate.x - mean_x;
        const double dy = candidate.y - mean_y;
        cov_xx += weight * dx * dx;
        cov_xy += weight * dx * dy;
        cov_yy += weight * dy * dy;
    }

    const double trace = cov_xx + cov_yy;
    const double det_term = std::sqrt(std::max(0.0, (cov_xx - cov_yy) * (cov_xx - cov_yy) + 4.0 * cov_xy * cov_xy));
    const double lambda = 0.5 * (trace + det_term);

    double dir_x = cov_xy;
    double dir_y = lambda - cov_xx;
    if (std::abs(dir_x) < 1e-9 && std::abs(dir_y) < 1e-9)
    {
        dir_x = 1.0;
        dir_y = 0.0;
    }
    const double norm = std::sqrt(dir_x * dir_x + dir_y * dir_y);
    dir_x /= norm;
    dir_y /= norm;

    const double normal_x = -dir_y;
    const double normal_y = dir_x;
    double weighted_error_sum = 0.0;
    double weighted_error_norm = 0.0;
    double max_error = 0.0;
    for (size_t i = 0; i < m_bestEdgeChain.size(); ++i)
    {
        const EdgeBandCandidate& candidate = m_bestEdgeChain[i];
        if (!candidate.valid)
            continue;
        const double weight = std::max(0.25, candidate.response_strength);
        const double residual = std::abs((candidate.x - mean_x) * normal_x + (candidate.y - mean_y) * normal_y);
        weighted_error_sum += residual * weight;
        weighted_error_norm += weight;
        max_error = std::max(max_error, residual);
    }

    stats.fit_error_avg = weighted_error_norm > 0.0 ? weighted_error_sum / weighted_error_norm : 0.0;
    stats.fit_error_max = max_error;
    stats.line_angle = std::atan2(dir_y, dir_x);
    stats.line_offset = mean_x * normal_x + mean_y * normal_y;

    const gp_Rectangle bbox = m_measurepoints_w.size() > 0 || m_measurepoints_h.size() > 0
        ? ([&]() {
            PointsShape points;
            points.addpoints(m_measurepoints_w);
            points.addpoints(m_measurepoints_h);
            return points.boundingRect();
        })()
        : gp_Rectangle(gp_Pnt(0, 0, 0), 0, 0);
    m_measurepointsboundingRect = bbox;

    stats.fit_ms = ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
}

void Findline::RefineJointConsistency(FindlineMeasureProfileStats& stats)
{
    const std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    if (m_bestEdgeChain.size() < 3)
    {
        stats.joint_refine_ms = ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
        return;
    }

    std::vector<EdgeBandCandidate> refined_chain;
    refined_chain.reserve(m_bestEdgeChain.size());
    refined_chain.push_back(m_bestEdgeChain.front());

    for (size_t i = 1; i + 1 < m_bestEdgeChain.size(); ++i)
    {
        EdgeBandCandidate current = m_bestEdgeChain[i];
        const EdgeBandCandidate& previous = m_bestEdgeChain[i - 1];
        const EdgeBandCandidate& next = m_bestEdgeChain[i + 1];

        if (!current.valid || !previous.valid || !next.valid)
            continue;

        if (previous.scan_type != current.scan_type || current.scan_type != next.scan_type)
        {
            refined_chain.push_back(current);
            continue;
        }

        const double expected_x = 0.5 * (previous.x + next.x);
        const double expected_y = 0.5 * (previous.y + next.y);
        const double dx = current.x - expected_x;
        const double dy = current.y - expected_y;
        const double deviation = std::sqrt(dx * dx + dy * dy);
        const double smooth_limit = std::max(2.5, 0.5 * (previous.width + next.width));
        const bool rank_switched = previous.edge_rank != current.edge_rank || current.edge_rank != next.edge_rank;
        if (rank_switched)
            ++stats.chain_switch_count;

        if (deviation > smooth_limit)
        {
            ++stats.neighbor_inconsistency_count;
            const double current_strength = std::max(0.25, current.response_strength);
            const double neighbor_strength = 0.5 * (std::max(0.25, previous.response_strength) + std::max(0.25, next.response_strength));

            if (current_strength <= neighbor_strength)
            {
                current.x = expected_x;
                current.y = expected_y;
                current.response_strength = std::max(current.response_strength, neighbor_strength);
            }
        }

        refined_chain.push_back(current);
    }

    refined_chain.push_back(m_bestEdgeChain.back());
    m_bestEdgeChain.swap(refined_chain);
    stats.joint_refine_ms = ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
}

void Findline::SmartFilter(double dist, double filtnum)
{
    int igapvalue_h = m_ihgap;
    int igapvalue_w = m_iwgap;
    if (-1 == dist && -1 == filtnum)
    {
        m_measurepoints_h.FilterPoints(igapvalue_h *2, 3);
        m_measurepoints_w.FilterPoints(igapvalue_w *2, 3);
    }
    else if(dist>0 && filtnum>0)
    {
        m_measurepoints_h.FilterPoints(dist, filtnum);
        m_measurepoints_w.FilterPoints(dist, filtnum); 
    }


   // m_measurepoints_h.SortPoints(0, igapvalue_h,10,45);
   // m_measurepoints_w.SortPoints(0, igapvalue_w,10,45);
    //m_measurepoints_h
}
void Findline::PyrImage(Image& image)
{
    (void)image;
}
PointsShape& Findline::getresultpointsw()
{
    return m_measurepoints_w;
}
PointsShape& Findline::getresultpointsh()
{
    return m_measurepoints_h;
}
void Findline::measure(void* pimage)
{
    Image* pgetimage = (Image*)pimage;
    if (pgetimage == nullptr)
        return;
    Measure(*pgetimage);
}
void Findline::pyrimage(void* pimage)
{
    Image* pgetimage = (Image*)pimage;
    if (pgetimage == nullptr)
        return;
    PyrImage(*pgetimage);
}
void Findline::shapesetroi(void* pshape)
{
    if (pshape == nullptr)
        return;
    Shape::shapesetroi(pshape);
}
void Findline::easycluster(int igapx, int igapy, int iclusternum)
{
    PointsShape resultpoints;
    resultpoints.addpoints(getresultpointsw());
    resultpoints.addpoints(getresultpointsh());
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
    int isize1 = ClampSizeToInt(getresultpointsw().size());
    int isize2 = isize;
    getresultpointsw().clear();
    getresultpointsh().clear();
    int inum = 0;
    for (inum = 0; inum < isize1; inum++)
    {
        int ix0 = RoundToInt(resultpoints.getx(inum));
        int iy0 = RoundToInt(resultpoints.gety(inum));
        int inumsum = numlist[inum];
        if (inumsum > iclusternum)
        {
            getresultpointsw().addpoint(ix0, iy0);
        }
    }
    for (; inum < isize2; inum++)
    {
        int ix0 = RoundToInt(resultpoints.getx(inum));
        int iy0 = RoundToInt(resultpoints.gety(inum));
        int inumsum = numlist[inum];
        if (inumsum > iclusternum)
        {
            getresultpointsh().addpoint(ix0, iy0);
        }
    }
}
void Findline::InflectionPoint(void* points)
{ 
    PointsShape* tpoints = (PointsShape*)points;
    if (tpoints == nullptr)
        return;
    if (m_measurepoints_h.size() > 0)
    {
        m_measurepoints_h.FindCrossPoints(points);
        return;
    }
    if (m_measurepoints_w.size() > 0)
    {
        m_measurepoints_w.FindCrossPoints(points);
        return;
    } 
}
/*void Findline::SeekPoints(PointsShape& seekpoints, gp_Pnt& point, int ivect)
{
    //LineMeasurePoints m_l_measure_h_seek;
    cv::Mat binaryImage;
    cv::threshold(image, binaryImage, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

    vector<vector<cv::Point>>contours;
    cv::findContours(binaryImage, contours, cv::RETR_LIST, cv::CHAIN_APPROX_NONE);

    int max_num = 0;
    std::vector<cv::Point> circle_pts;
    for (int i = 0; i < contours.size(); i++) {
        int pt_num = contours[i].size();
        if (pt_num > max_num) {
            circle_pts.clear();
            max_num = pt_num;
            circle_pts = contours[i];
        }
    }

    std::vector<cv::Point2f> edge_pts;
    for (const auto& pt : circle_pts) {
        edge_pts.push_back(cv::Point2f(float(pt.x), float(pt.y)));
    }

   
}*/
