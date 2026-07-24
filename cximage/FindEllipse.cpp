#include "pch.h"

#include "Findellipse.h"
#include "occtinclude.h"
#include "imagemanager.h"
#include "findobject.h"
#include "ImageAnnotationLayer.h"
#include "EllipseShape.h"
#include "CxCrashLogHandler.h"
#include "CxUnifiedLog.h"

#include <opencv2/opencv.hpp>		
#include <opencv2/core/version.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/video/tracking.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <sstream>
#include <vector>
 
namespace
{
void LogFindellipseMeasureProbe(
    const char* phase,
    const char* status,
    const std::string& message)
{
    CXLOG_INFO("FindEllipse", phase, status, message);
    CxUnifiedLog::Instance().Flush();
}

double EllipseNorm(
    double x,
    double y,
    double cx,
    double cy,
    double rx,
    double ry)
{
    if (rx <= 1.0 || ry <= 1.0)
        return 999.0;

    const double nx = (x - cx) / rx;
    const double ny = (y - cy) / ry;

    return std::sqrt(nx * nx + ny * ny);
}

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

bool HasSufficientEllipseAngularCoverage(
    const std::vector<cv::Point2f>& points,
    double center_x,
    double center_y,
    double radius_x,
    double radius_y)
{
    if (points.size() < 5 || radius_x <= 1.0 || radius_y <= 1.0)
        return false;

    constexpr int kBins = 16;
    std::array<bool, kBins> occupied{};
    int occupied_count = 0;

    for (const cv::Point2f& point : points)
    {
        const double nx = (static_cast<double>(point.x) - center_x) / radius_x;
        const double ny = (static_cast<double>(point.y) - center_y) / radius_y;
        double angle = std::atan2(ny, nx);
        if (angle < 0.0)
            angle += 2.0 * CV_PI;

        int bin = static_cast<int>(std::floor(angle / (2.0 * CV_PI) * kBins));
        if (bin < 0)
            bin = 0;
        if (bin >= kBins)
            bin = kBins - 1;

        if (!occupied[bin])
        {
            occupied[bin] = true;
            ++occupied_count;
        }
    }

    return occupied_count >= 4;
}
}
 
 
int FindEllipse::m_curfindlinenum = 0;
FindEllipse::FindEllipse() :Shape(),
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
FindEllipse::~FindEllipse()
{

}
void FindEllipse::setcomparegap(int igap)
{
    m_icomparegap = igap;
}

void FindEllipse::setshow(int ishow)
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
void FindEllipse::setselectedgenum(int iedgenum)
{
    m_iselectedgenum = iedgenum;
}
void FindEllipse::clear()
{
    m_lines.clear();
    m_measurepoints.clear();
    m_measurepoints_.clear();
    m_has_display_roi = false;
    m_roi_x0 = 0;
    m_roi_y0 = 0;
    m_roi_x1 = 0;
    m_roi_y1 = 0;
    m_has_fit_result = false;
    m_fit_center_x = 0.0;
    m_fit_center_y = 0.0;
    m_fit_radius_x = 0.0;
    m_fit_radius_y = 0.0;
    m_fit_angle_deg = 0.0;
    m_fit_avgdist = 0.0;
    m_scan_candidate_lines = 0;
    m_scan_total_candidates = 0;
    m_scan_accepted_points_before_gate = 0;
    m_accepted_boundary_ratio_sum = 0.0;
    m_accepted_boundary_ratio_min = 999.0;
    m_accepted_boundary_ratio_max = -999.0;
    m_candidate_policy.clear();
}
void FindEllipse::Setgap(int gap)
{
    m_igap = gap;
    for (std::size_t i = 0; i < m_lines.size(); ++i)
    {
        m_lines[i].clear();
    }
    m_lines.clear();

    const int isize = ClampSizeToInt(getpath().ElementCount());
    if (m_igap > 0 && isize > 0 && m_has_display_roi)
    {
        LineShape aline1;
        const int igapadd = ComputeEllipseLineStep(m_igap, isize);
        const double cx = static_cast<double>(m_roi_x0 + m_roi_x1) * 0.5;
        const double cy = static_cast<double>(m_roi_y0 + m_roi_y1) * 0.5;
        const double width0 = std::abs(static_cast<double>(m_roi_x1 - m_roi_x0));
        const double height0 = std::abs(static_cast<double>(m_roi_y1 - m_roi_y0));
        const double rx = (width0 >= height0 ? width0 : height0) * 0.5;
        const double ry = (width0 <= height0 ? width0 : height0) * 0.5;
        const double outer_ratio = 1.05;

        for (int i = 0; i < isize; i += igapadd)
        {
            gp_Pnt apoint = getpath().ElementAt(i);
            const double dx = apoint.X() - cx;
            const double dy = apoint.Y() - cy;
            const double dist = std::sqrt(dx * dx + dy * dy);

            if (dist > 1.0)
            {
                const double nx = dx / dist;
                const double ny = dy / dist;
                const double boundary_dist = rx * ry / std::sqrt(
                    ry * ry * nx * nx + rx * rx * ny * ny);
                const double clamped_dist = std::min(
                    boundary_dist * outer_ratio,
                    dist);
                const int end_x = RoundToInt(cx + nx * clamped_dist);
                const int end_y = RoundToInt(cy + ny * clamped_dist);
                m_lines.push_back(aline1);
                m_lines[m_lines.size() - 1].setline(RoundToInt(cx), RoundToInt(cy), end_x, end_y);
            }
            else
            {
                m_lines.push_back(aline1);
                m_lines[m_lines.size() - 1].setline(RoundToInt(cx), RoundToInt(cy), RoundToInt(apoint.X()), RoundToInt(apoint.Y()));
            }
            m_lines[m_lines.size() - 1].setPercent(m_dsamplerate);
        }

        m_scan_lines_outside_roi_count = 0;
        m_scan_lines_cross_outside_ellipse_count = 0;
        m_scan_endpoint_norm_min = 999.0;
        m_scan_endpoint_norm_max = -999.0;

        for (auto& line : m_lines)
        {
            const int line_size = line.getlinesize();
            if (line_size < 2)
                continue;

            const double n_start = EllipseNorm(cx, cy, cx, cy, rx, ry);
            gp_Pnt p_end = line.getlinepoint(line_size - 1);
            const double n_end = EllipseNorm(p_end.X(), p_end.Y(), cx, cy, rx, ry);

            m_scan_endpoint_norm_min = std::min(m_scan_endpoint_norm_min, std::min(n_start, n_end));
            m_scan_endpoint_norm_max = std::max(m_scan_endpoint_norm_max, std::max(n_start, n_end));

            if (n_end > 1.05)
                m_scan_lines_cross_outside_ellipse_count++;
        }
    }
}
void FindEllipse::setellipse(int icentx, int icenty, int ipax, int ipay)
{
    Shape::setellipse(icentx, icenty, ipax, ipay);
    m_has_fit_result = false;
    m_fit_avgdist = 0.0;

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
        const double width0 = std::abs(static_cast<double>(ipax - icentx));
        const double height0 = std::abs(static_cast<double>(ipay - icenty));
        const double rx = (width0 >= height0 ? width0 : height0) * 0.5;
        const double ry = (width0 <= height0 ? width0 : height0) * 0.5;
        const double outer_ratio = 1.05;

        for (int i = 0; i < isize; i += igapadd)
        {
            gp_Pnt apoint = getpath().ElementAt(i);
            const double dx = apoint.X() - icx0;
            const double dy = apoint.Y() - icy0;
            const double dist = std::sqrt(dx * dx + dy * dy);

            if (dist > 1.0)
            {
                const double nx = dx / dist;
                const double ny = dy / dist;
                const double boundary_dist = rx * ry / std::sqrt(
                    ry * ry * nx * nx + rx * rx * ny * ny);
                const double clamped_dist = std::min(
                    boundary_dist * outer_ratio,
                    dist);
                const int end_x = RoundToInt(icx0 + nx * clamped_dist);
                const int end_y = RoundToInt(icy0 + ny * clamped_dist);
                m_lines.push_back(aline1);
                m_lines[m_lines.size() - 1].setline(icx0, icy0, end_x, end_y);
            }
            else
            {
                m_lines.push_back(aline1);
                m_lines[m_lines.size() - 1].setline(icx0, icy0, RoundToInt(apoint.X()), RoundToInt(apoint.Y()));
            }
            m_lines[m_lines.size() - 1].setPercent(m_dsamplerate);
        }
    }

    m_scan_geometry_policy = "ellipse_boundary_clamped_1_05";
    m_scan_lines_outside_roi_count = 0;
    m_scan_lines_cross_outside_ellipse_count = 0;
    m_scan_endpoint_norm_min = 999.0;
    m_scan_endpoint_norm_max = -999.0;

    if (m_has_display_roi)
    {
        const double cx = static_cast<double>(m_roi_x0 + m_roi_x1) * 0.5;
        const double cy = static_cast<double>(m_roi_y0 + m_roi_y1) * 0.5;
        const double width0 = std::abs(static_cast<double>(m_roi_x1 - m_roi_x0));
        const double height0 = std::abs(static_cast<double>(m_roi_y1 - m_roi_y0));
        const double rx = (width0 >= height0 ? width0 : height0) * 0.5;
        const double ry = (width0 <= height0 ? width0 : height0) * 0.5;

        for (auto& line : m_lines)
        {
            const int line_size = line.getlinesize();
            if (line_size < 2)
                continue;

            const double n_start = EllipseNorm(cx, cy, cx, cy, rx, ry);
            gp_Pnt p_end = line.getlinepoint(line_size - 1);
            const double n_end = EllipseNorm(p_end.X(), p_end.Y(), cx, cy, rx, ry);

            m_scan_endpoint_norm_min = std::min(m_scan_endpoint_norm_min, std::min(n_start, n_end));
            m_scan_endpoint_norm_max = std::max(m_scan_endpoint_norm_max, std::max(n_start, n_end));

            if (n_end > 1.05)
                m_scan_lines_cross_outside_ellipse_count++;
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
void FindEllipse::setellipse2(int icentx, int icenty, int ipax, int ipay,int idis)
{
    Shape::setellipse2(icentx, icenty, ipax, ipay, idis);
    m_has_fit_result = false;
    m_fit_avgdist = 0.0;

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

    m_scan_geometry_policy = "setellipse2_intersection_boundary";
    m_scan_lines_outside_roi_count = 0;
    m_scan_lines_cross_outside_ellipse_count = 0;
    m_scan_endpoint_norm_min = 999.0;
    m_scan_endpoint_norm_max = -999.0;

    if (m_has_display_roi)
    {
        const double cx = static_cast<double>(m_roi_x0 + m_roi_x1) * 0.5;
        const double cy = static_cast<double>(m_roi_y0 + m_roi_y1) * 0.5;
        const double width0 = std::abs(static_cast<double>(m_roi_x1 - m_roi_x0));
        const double height0 = std::abs(static_cast<double>(m_roi_y1 - m_roi_y0));
        const double rx = (width0 >= height0 ? width0 : height0) * 0.5;
        const double ry = (width0 <= height0 ? width0 : height0) * 0.5;

        for (auto& line : m_lines)
        {
            const int line_size = line.getlinesize();
            if (line_size < 2)
                continue;

            gp_Pnt p0 = line.getlinepoint(0);
            gp_Pnt p1 = line.getlinepoint(line_size - 1);

            const double n0 = EllipseNorm(p0.X(), p0.Y(), cx, cy, rx, ry);
            const double n1 = EllipseNorm(p1.X(), p1.Y(), cx, cy, rx, ry);

            m_scan_endpoint_norm_min = std::min(m_scan_endpoint_norm_min, std::min(n0, n1));
            m_scan_endpoint_norm_max = std::max(m_scan_endpoint_norm_max, std::max(n0, n1));

            if (n0 > 1.05 || n1 > 1.05)
                m_scan_lines_cross_outside_ellipse_count++;
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

void FindEllipse::translate(int ix,int iy)
{
    Translate(gp_Vec(ix, iy, 0)); 
}
void FindEllipse::Translate(const gp_Vec& translationVector)
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
void FindEllipse::drawpattern()
{ 
    m_modelpoints.setshow(8);
    m_modelpoints.drawshape(getpath());
    m_measurepoints.drawshape(getpath());
    m_measurepoints_.drawshape(getpath());

}
void FindEllipse::drawpatternx(double dmovx, double dmovy,
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
void FindEllipse::edgepattern(Image& image)
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
void FindEllipse::patternzeroposition()
{
    gp_Rectangle arect1 = m_modelpoints.boundingRect();
    m_modelpoints.Move(RoundToInt(-arect1.TopLeft().X()), RoundToInt(-arect1.TopLeft().Y()));
}
void FindEllipse::savepatternfile(const char* pchar)
{
    m_modelpoints.save(pchar);
}
void FindEllipse::loadpatternfile(const char* pchar)
{
    m_modelpoints.load(pchar);
}
gp_Rectangle FindEllipse::patternboundingrect()
{
    return m_modelpoints.boundingRect();
}
void FindEllipse::patterngap2gap(int inewgap)
{
    m_modelpoints.patterngap2gap(inewgap);
}
void FindEllipse::patternrootgrid(double itype, double drate, double ilevel)
{
    m_modelpoints.keysrootgrid(RoundToInt(itype), drate, RoundToInt(ilevel));
}
void FindEllipse::patterntranform(int igap, int itype, int isgap, int iline)
{
    m_modelpoints.patterntranform(igap, itype, isgap, iline);
}
void FindEllipse::patternzoom(double dx, double dy, double igap, double itype)
{
    m_modelpoints.patternzoom(RoundToInt(dx), RoundToInt(dy), RoundToInt(igap), RoundToInt(itype));
}
void FindEllipse::patternrotate(double dangle)
{
    m_modelpoints.Rotate(RoundToInt(dangle));
}
void FindEllipse::modelzoom(double dx, double dy)
{
    m_modelpoints.Zoom(RoundToInt(dx), RoundToInt(dy));
}
gp_Path& FindEllipse::getpatternpath()
{
    return m_modelpoints.getpath();
}
PointsShape& FindEllipse::getpattern()
{
    return m_modelpoints;
}
void FindEllipse::findpattern(void* pimage)
{
    Image* pgetimage = (Image*)pimage;
    if (pgetimage == nullptr)
        return;
    edgepattern(*pgetimage);
}
void FindEllipse::drawshape()
{
    Shape::drawshape();
}
void FindEllipse::drawshapex(
    double dmovx, double dmovy,
    double dangle,
    double dzoomx, double dzoomy)
{
    Shape::drawshapex(  dmovx, dmovy,
        dangle, dzoomx, dzoomy);
}

void FindEllipse::setlinesamplerate(double dsamplerate)
{
    m_dsamplerate = dsamplerate;
}
void FindEllipse::setlinegap(int igap)
{
    m_iSelectPointGap = igap;
}
void FindEllipse::setmethod(int imethod)
{
    m_iMethod = imethod;
}
void FindEllipse::setthre(int ithre)
{
    m_iThreshold = ithre;
}
int FindEllipse::thre()
{
    return m_iThreshold;
}
void FindEllipse::setgamarate(int igama)
{
    m_igamarate = igama;
}

void FindEllipse::setfindsetting(int ifindset)
{
    m_ifindset = ifindset;
}
void FindEllipse::setfilter(int ifilterborw, int ifiltermin, int ifiltermax)
{
    m_ifilterborw = ifilterborw;
    m_ifiltermin = ifiltermin;
    m_ifiltermax = ifiltermax;
}
void FindEllipse::MeasureT(void *pimage)
{
    (void)pimage;
}
void FindEllipse::Measure(Image& image)
{
    SetCxCrashBreadcrumb("FindEllipse::Measure:enter");
    m_has_fit_result = false;
    m_fit_avgdist = 0.0;
    m_measure_failure_stage.clear();
    m_measure_failure_reason.clear();

    m_scan_candidate_lines = 0;
    m_scan_total_candidates = 0;
    m_scan_accepted_points_before_gate = 0;
    m_accepted_boundary_ratio_sum = 0.0;
    m_accepted_boundary_ratio_min = 999.0;
    m_accepted_boundary_ratio_max = -999.0;
    m_candidate_policy = "ellipse_boundary_band_nearest_norm_loose_fallback";

    m_accepted_points_outside_ellipse_count = 0;
    m_accepted_point_norm_sum = 0.0;
    m_accepted_point_norm_count = 0;
    m_accepted_point_norm_min = 999.0;
    m_accepted_point_norm_max = -999.0;
    m_rejected_boundary_band_candidate_count = 0;
    m_rejected_boundary_band_norm_sum = 0.0;
    m_rejected_boundary_band_norm_min = 999.0;
    m_rejected_boundary_band_norm_max = -999.0;

    if (image.getmat().empty() || image.getWidth() <= 0 || image.getHeight() <= 0)
    {
        m_measure_failure_stage = "input_image_empty";
        m_measure_failure_reason = "Findellipse input image is empty or has invalid dimensions.";
        LogFindellipseMeasureProbe(
            "measure_preflight",
            "failed",
            m_measure_failure_reason);
        return;
    }

    {
        std::ostringstream oss;
        oss << "enter image=" << image.getWidth() << "x" << image.getHeight()
            << " roi=(" << m_roi_x0 << "," << m_roi_y0
            << ")-(" << m_roi_x1 << "," << m_roi_y1 << ")"
            << " gap=" << m_igap
            << " linegap=" << m_iSelectPointGap
            << " threshold=" << m_iThreshold
            << " method=" << m_iMethod
            << " existing_scan_lines=" << m_lines.size();
        LogFindellipseMeasureProbe("measure_enter", "running", oss.str());
    }

    SetCxCrashBreadcrumb("FindEllipse::Measure:ensure_resources");
    if (!ImageManager::EnsureAlgorithmRuntimeResources(
            image.getWidth(),
            image.getHeight()))
    {
        m_measure_failure_stage = "runtime_resources";
        m_measure_failure_reason =
            "Findellipse failed to initialize ImageManager algorithm runtime resources.";
        LogFindellipseMeasureProbe(
            "measure_preflight",
            "failed",
            m_measure_failure_reason);
        return;
    }
    g_pbackimage = ImageManager::GetBackImage(1);
    g_pbackfindobject = ImageManager::Getbackfindobject(1);

    SetCxCrashBreadcrumb("FindEllipse::Measure:roi_preflight");
    if (image.getWidth() < rect().TopLeft().X() + rect().Width()
        || image.getHeight() < rect().TopLeft().Y() + rect().Height())
    {
        m_measure_failure_stage = "roi_outside_image";
        m_measure_failure_reason = "Findellipse ROI rectangle is outside input image.";
        LogFindellipseMeasureProbe(
            "measure_preflight",
            "failed",
            m_measure_failure_reason);
        return;//error process
    }
    if (rect().TopLeft().X() < 0 || rect().TopLeft().Y() < 0)
    {
        m_measure_failure_stage = "roi_negative";
        m_measure_failure_reason = "Findellipse ROI rectangle has negative origin.";
        LogFindellipseMeasureProbe(
            "measure_preflight",
            "failed",
            m_measure_failure_reason);
        return;//error process
    }
    m_measurepoints.clear();
    int isize = ClampSizeToInt(m_lines.size());

    const double ellipse_cx = m_has_display_roi
        ? static_cast<double>(m_roi_x0 + m_roi_x1) * 0.5
        : 0.0;
    const double ellipse_cy = m_has_display_roi
        ? static_cast<double>(m_roi_y0 + m_roi_y1) * 0.5
        : 0.0;
    const double ellipse_rx = m_has_display_roi
        ? std::abs(static_cast<double>(m_roi_x1 - m_roi_x0)) * 0.5
        : 1.0;
    const double ellipse_ry = m_has_display_roi
        ? std::abs(static_cast<double>(m_roi_y1 - m_roi_y0)) * 0.5
        : 1.0;
    if (isize <= 0)
    {
        m_measure_failure_stage = "scan_lines_empty";
        m_measure_failure_reason = "Findellipse has no scan lines; check setellipse/setgap order.";
        LogFindellipseMeasureProbe(
            "measure_preflight",
            "failed",
            m_measure_failure_reason);
        return;
    }
    if (g_pbackimage == nullptr)
    {
        m_measure_failure_stage = "backimage_null";
        m_measure_failure_reason = "Findellipse back image is null.";
        LogFindellipseMeasureProbe(
            "measure_preflight",
            "failed",
            m_measure_failure_reason);
        return;
    }
    if (g_pbackimage == &image)
    {
        m_measure_failure_stage = "backimage_alias_input";
        m_measure_failure_reason = "Findellipse back image aliases the input image.";
        LogFindellipseMeasureProbe(
            "measure_preflight",
            "failed",
            m_measure_failure_reason);
        return;
    }
    if (g_pbackimage->getmat().empty())
    {
        m_measure_failure_stage = "backimage_empty";
        m_measure_failure_reason = "Findellipse back image Mat is empty.";
        LogFindellipseMeasureProbe(
            "measure_preflight",
            "failed",
            m_measure_failure_reason);
        return;
    }

    int ilineslen1 = 0;

    if (isize > 0)
        ilineslen1 = m_lines[0].getlinesize();

    int min_line_len = ilineslen1;
    int max_line_len = ilineslen1;
    int invalid_line_count = 0;
    int outside_endpoint_count = 0;
    for (int i = 0; i < isize; ++i)
    {
        const int line_len = m_lines[i].getlinesize();
        if (line_len <= 0)
        {
            ++invalid_line_count;
            continue;
        }
        min_line_len = std::min(min_line_len, line_len);
        max_line_len = std::max(max_line_len, line_len);

        gp_Pnt p0 = m_lines[i].getlinepoint(0);
        gp_Pnt p1 = m_lines[i].getlinepoint(line_len - 1);
        const bool p0_finite = std::isfinite(p0.X()) && std::isfinite(p0.Y());
        const bool p1_finite = std::isfinite(p1.X()) && std::isfinite(p1.Y());
        if (!p0_finite || !p1_finite)
        {
            ++invalid_line_count;
            continue;
        }

        if (p0.X() < 0.0 || p0.Y() < 0.0 ||
            p0.X() >= static_cast<double>(image.getWidth()) ||
            p0.Y() >= static_cast<double>(image.getHeight()) ||
            p1.X() < 0.0 || p1.Y() < 0.0 ||
            p1.X() >= static_cast<double>(image.getWidth()) ||
            p1.Y() >= static_cast<double>(image.getHeight()))
        {
            ++outside_endpoint_count;
        }
    }

    if (invalid_line_count > 0 || min_line_len <= 0)
    {
        m_measure_failure_stage = "scan_line_invalid";
        std::ostringstream oss;
        oss << "Findellipse generated invalid scan lines before linecopyex."
            << " invalid_line_count=" << invalid_line_count
            << " scan_lines=" << isize
            << " min_line_len=" << min_line_len
            << " max_line_len=" << max_line_len;
        m_measure_failure_reason = oss.str();
        LogFindellipseMeasureProbe("measure_preflight", "failed", m_measure_failure_reason);
        return;
    }

    int iprocessw = min_line_len;

    if (iprocessw <= 0 ||
        isize > g_pbackimage->getHeight() ||
        iprocessw > g_pbackimage->getWidth())
    {
        m_measure_failure_stage = "scan_buffer_too_small";
        std::ostringstream oss;
        oss << "Findellipse scan buffer is smaller than generated scan geometry."
            << " scan_lines=" << isize
            << " process_w=" << iprocessw
            << " back=" << g_pbackimage->getWidth() << "x" << g_pbackimage->getHeight();
        m_measure_failure_reason = oss.str();
        LogFindellipseMeasureProbe(
            "measure_preflight",
            "failed",
            m_measure_failure_reason);
        return;
    }

    {
        std::ostringstream oss;
        oss << "scan geometry ready scan_lines=" << isize
            << " process_w=" << iprocessw
            << " min_line_len=" << min_line_len
            << " max_line_len=" << max_line_len
            << " outside_endpoint_count=" << outside_endpoint_count
            << " back=" << g_pbackimage->getWidth() << "x" << g_pbackimage->getHeight();
        LogFindellipseMeasureProbe("measure_scan_geometry", "ready", oss.str());
    }

    SetCxCrashBreadcrumb("FindEllipse::Measure:linecopyex");
    for (int i = 0; i < isize; i++)
    {
        m_lines[i].linecopyex(image, *g_pbackimage, 0, i);
    }
    LogFindellipseMeasureProbe("measure_linecopyex", "finished", "Findellipse linecopyex completed.");

    SetCxCrashBreadcrumb("FindEllipse::Measure:preprocess_roi");
    g_pbackimage->setroi(0, 0, iprocessw, isize);

    g_pbackimage->roi_7blur_gap_mud_thre_bw(m_iThreshold, m_igamarate, m_iSelectPointGap, m_iMethod);
    LogFindellipseMeasureProbe("measure_preprocess", "finished", "Findellipse preprocess completed.");

    if ((m_ifindset & 0x01) && g_pbackfindobject != nullptr)
    {
        SetCxCrashBreadcrumb("FindEllipse::Measure:findobject");
        g_pbackfindobject->setrect(0, 0, iprocessw, isize);
        g_pbackfindobject->setbrow(m_ifilterborw);//21 22
        g_pbackfindobject->setminmaxarea(ClampLongLongToInt(static_cast<long long>(m_ifiltermin)), ClampLongLongToInt(static_cast<long long>(m_ifiltermax)));
        g_pbackfindobject->Measure(*g_pbackimage);
        LogFindellipseMeasureProbe("measure_findobject", "finished", "Findellipse FindObject filter completed.");
    }

    std::vector<int> irecordpoint;
    irecordpoint.reserve(128);
    bool bcollectBegin = false;

    int icurlinenum = 0;
    int icurlineposition = 0;

    int ifixvalue = 3;

    cv::Vec3b icolor = 0;
    SetCxCrashBreadcrumb("FindEllipse::Measure:candidate_collect");
    for (int inumy = 0 + ifixvalue; inumy < isize - ifixvalue; inumy++)
    {
        std::vector<int> candidate_positions;
        candidate_positions.reserve(8);
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
                            if (m_iselectedgenum == 0)
                            {
                                candidate_positions.push_back(icurlineposition);
                            }
                            else
                            {
                                gp_Pnt apoint = m_lines[inumy].getlinepoint(icurlineposition);
                                const double norm = EllipseNorm(
                                    apoint.X(), apoint.Y(),
                                    ellipse_cx, ellipse_cy,
                                    ellipse_rx, ellipse_ry);
                                m_accepted_point_norm_sum += norm;
                                m_accepted_point_norm_count++;
                                m_accepted_point_norm_min = std::min(m_accepted_point_norm_min, norm);
                                m_accepted_point_norm_max = std::max(m_accepted_point_norm_max, norm);
                                if (norm > 1.05)
                                    m_accepted_points_outside_ellipse_count++;
                                m_measurepoints.addpoint(apoint);
                                break;
                            }
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
                    if (m_iselectedgenum == 0)
                    {
                        candidate_positions.push_back(icurlineposition);
                    }
                    else
                    {
                        gp_Pnt apoint = m_lines[inumy].getlinepoint(icurlineposition);
                        const double norm = EllipseNorm(
                            apoint.X(), apoint.Y(),
                            ellipse_cx, ellipse_cy,
                            ellipse_rx, ellipse_ry);
                        m_accepted_point_norm_sum += norm;
                        m_accepted_point_norm_count++;
                        m_accepted_point_norm_min = std::min(m_accepted_point_norm_min, norm);
                        m_accepted_point_norm_max = std::max(m_accepted_point_norm_max, norm);
                        if (norm > 1.05)
                            m_accepted_points_outside_ellipse_count++;
                        m_measurepoints.addpoint(apoint);
                        break;
                    }
                }
            }
            irecordpoint.clear();
            bcollectBegin = false;
        }

        if (m_iselectedgenum == 0 && !candidate_positions.empty())
        {
            m_scan_candidate_lines++;
            m_scan_total_candidates += static_cast<int>(candidate_positions.size());

            constexpr double kBoundaryBandMin = 0.65;
            constexpr double kBoundaryBandLooseMin = 0.45;
            constexpr double kBoundaryBandMax = 1.08;
            int strict_boundary_position = -1;
            double strict_best_score = 999.0;
            double strict_best_norm = 0.0;
            int loose_boundary_position = -1;
            double loose_best_score = 999.0;
            double loose_best_norm = 0.0;
            for (const int candidate_position : candidate_positions)
            {
                if (candidate_position < 0 || candidate_position >= ilineslen1)
                    continue;

                gp_Pnt candidate_point = m_lines[inumy].getlinepoint(candidate_position);
                const double candidate_norm = EllipseNorm(
                    candidate_point.X(), candidate_point.Y(),
                    ellipse_cx, ellipse_cy,
                    ellipse_rx, ellipse_ry);

                if (!std::isfinite(candidate_norm) ||
                    candidate_norm < kBoundaryBandLooseMin ||
                    candidate_norm > kBoundaryBandMax)
                {
                    if (std::isfinite(candidate_norm))
                    {
                        ++m_rejected_boundary_band_candidate_count;
                        m_rejected_boundary_band_norm_sum += candidate_norm;
                        m_rejected_boundary_band_norm_min =
                            std::min(m_rejected_boundary_band_norm_min, candidate_norm);
                        m_rejected_boundary_band_norm_max =
                            std::max(m_rejected_boundary_band_norm_max, candidate_norm);
                    }
                    continue;
                }

                const double score = std::abs(candidate_norm - 1.0);
                if (candidate_norm >= kBoundaryBandMin)
                {
                    if (score < strict_best_score)
                    {
                        strict_best_score = score;
                        strict_best_norm = candidate_norm;
                        strict_boundary_position = candidate_position;
                    }
                }
                else if (score < loose_best_score)
                {
                    loose_best_score = score;
                    loose_best_norm = candidate_norm;
                    loose_boundary_position = candidate_position;
                }
            }

            const int boundary_position =
                strict_boundary_position >= 0 ? strict_boundary_position : loose_boundary_position;
            const double best_norm =
                strict_boundary_position >= 0 ? strict_best_norm : loose_best_norm;

            if (boundary_position < 0)
                continue;

            gp_Pnt apoint = m_lines[inumy].getlinepoint(boundary_position);

            const double norm = EllipseNorm(
                apoint.X(), apoint.Y(),
                ellipse_cx, ellipse_cy,
                ellipse_rx, ellipse_ry);
            m_accepted_point_norm_sum += norm;
            m_accepted_point_norm_count++;
            m_accepted_point_norm_min = std::min(m_accepted_point_norm_min, norm);
            m_accepted_point_norm_max = std::max(m_accepted_point_norm_max, norm);
            if (norm > 1.05)
                m_accepted_points_outside_ellipse_count++;

            m_measurepoints.addpoint(apoint);

            double boundary_ratio = best_norm;
            m_scan_accepted_points_before_gate++;
            m_accepted_boundary_ratio_sum += boundary_ratio;
            if (boundary_ratio < m_accepted_boundary_ratio_min)
                m_accepted_boundary_ratio_min = boundary_ratio;
            if (boundary_ratio > m_accepted_boundary_ratio_max)
                m_accepted_boundary_ratio_max = boundary_ratio;
        }

    }

    if (m_measurepoints.size() <= 0)
    {
        if (m_scan_total_candidates > 0 && m_scan_accepted_points_before_gate <= 0)
        {
            m_measure_failure_stage = "no_boundary_band_candidate";
            m_measure_failure_reason =
                "Findellipse found edge runs, but none are near the Gauge ellipse boundary band.";
            if (m_rejected_boundary_band_candidate_count > 0)
            {
                m_measure_failure_reason += " rejected_norm=" +
                    std::to_string(m_rejected_boundary_band_norm_min) + "/" +
                    std::to_string(
                        m_rejected_boundary_band_norm_sum /
                        static_cast<double>(m_rejected_boundary_band_candidate_count)) + "/" +
                    std::to_string(m_rejected_boundary_band_norm_max);
            }
        }
        else
        {
            m_measure_failure_stage = "threshold_no_edge";
            m_measure_failure_reason =
                "Findellipse preprocessing produced no accepted edge run; check threshold/method/linegap.";
        }
    }

    {
        std::ostringstream oss;
        oss << "measure finished points=" << m_measurepoints.size()
            << " candidate_lines=" << m_scan_candidate_lines
            << " total_candidates=" << m_scan_total_candidates
            << " accepted_before_gate=" << m_scan_accepted_points_before_gate
            << " failure_stage=" << m_measure_failure_stage;
        LogFindellipseMeasureProbe("measure_exit", "finished", oss.str());
    }
    
}

PointsShape& FindEllipse::getresultpoints()
{
    return m_measurepoints;
}

void FindEllipse::fitellipse()
{
    m_has_fit_result = false;
    m_fit_center_x = 0.0;
    m_fit_center_y = 0.0;
    m_fit_radius_x = 0.0;
    m_fit_radius_y = 0.0;
    m_fit_angle_deg = 0.0;
    m_fit_avgdist = 0.0;

    const int count = ClampSizeToInt(m_measurepoints.size());
    if (count < 5)
        return;

    std::vector<cv::Point2f> points;
    points.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i)
    {
        const double x = m_measurepoints.getx(i);
        const double y = m_measurepoints.gety(i);
        if (std::isfinite(x) && std::isfinite(y))
            points.emplace_back(static_cast<float>(x), static_cast<float>(y));
    }
    if (points.size() < 5)
        return;

    const double roi_center_x = static_cast<double>(m_roi_x0 + m_roi_x1) * 0.5;
    const double roi_center_y = static_cast<double>(m_roi_y0 + m_roi_y1) * 0.5;
    const double roi_radius_x = std::abs(static_cast<double>(m_roi_x1 - m_roi_x0)) * 0.5;
    const double roi_radius_y = std::abs(static_cast<double>(m_roi_y1 - m_roi_y0)) * 0.5;
    if (!HasSufficientEllipseAngularCoverage(
            points,
            roi_center_x,
            roi_center_y,
            roi_radius_x,
            roi_radius_y))
    {
        m_measure_failure_stage = "insufficient_boundary_coverage";
        m_measure_failure_reason =
            "Findellipse rejected edge set because accepted points do not cover enough of the Gauge ellipse.";
        return;
    }

    cv::RotatedRect fitted;
    try
    {
        fitted = cv::fitEllipse(points);
    }
    catch (const cv::Exception&)
    {
        return;
    }

    const double rx = std::abs(static_cast<double>(fitted.size.width)) * 0.5;
    const double ry = std::abs(static_cast<double>(fitted.size.height)) * 0.5;
    if (rx <= 0.0 || ry <= 0.0 ||
        !std::isfinite(rx) || !std::isfinite(ry) ||
        !std::isfinite(static_cast<double>(fitted.center.x)) ||
        !std::isfinite(static_cast<double>(fitted.center.y)))
    {
        return;
    }

    const double min_radius_ratio = std::min(
        rx / std::max(roi_radius_x, 1.0),
        ry / std::max(roi_radius_y, 1.0));
    const double max_radius_ratio = std::max(
        rx / std::max(roi_radius_x, 1.0),
        ry / std::max(roi_radius_y, 1.0));
    if (min_radius_ratio < 0.45 || max_radius_ratio > 3.0)
    {
        m_measure_failure_stage = "fit_too_small_for_gauge";
        m_measure_failure_reason =
            "Findellipse rejected local edge cluster because fitted ellipse is too small compared with the Gauge.";
        return;
    }

    m_fit_center_x = fitted.center.x;
    m_fit_center_y = fitted.center.y;
    m_fit_radius_x = rx;
    m_fit_radius_y = ry;
    m_fit_angle_deg = fitted.angle;

    const double angle = m_fit_angle_deg * CV_PI / 180.0;
    const double ca = std::cos(angle);
    const double sa = std::sin(angle);
    const double scale = (rx + ry) * 0.5;
    double sum = 0.0;
    int used = 0;
    for (const cv::Point2f& point : points)
    {
        const double dx = static_cast<double>(point.x) - m_fit_center_x;
        const double dy = static_cast<double>(point.y) - m_fit_center_y;
        const double local_x = ca * dx + sa * dy;
        const double local_y = -sa * dx + ca * dy;
        const double norm = std::sqrt(
            (local_x * local_x) / (rx * rx) +
            (local_y * local_y) / (ry * ry));
        if (std::isfinite(norm))
        {
            sum += std::abs(norm - 1.0) * scale;
            ++used;
        }
    }
    m_fit_avgdist = used > 0 ? sum / static_cast<double>(used) : 0.0;
    m_has_fit_result = true;
}

double FindEllipse::getresultcentx()
{
    return m_fit_center_x;
}

double FindEllipse::getresultcenty()
{
    return m_fit_center_y;
}

double FindEllipse::getresultradiusx()
{
    return m_fit_radius_x;
}

double FindEllipse::getresultradiusy()
{
    return m_fit_radius_y;
}

double FindEllipse::getresultangle()
{
    return m_fit_angle_deg;
}

double FindEllipse::getavgdist()
{
    return m_fit_avgdist;
}

double FindEllipse::hasfitresult()
{
    return m_has_fit_result ? 1.0 : 0.0;
}


void FindEllipse::measure(void* pimage)
{
    SetCxCrashBreadcrumb("FindEllipse::measure:void_ptr_enter");
    Image* pgetimage = (Image*)pimage;
    if (pgetimage == nullptr)
    {
        m_measure_failure_stage = "input_image_null";
        m_measure_failure_reason = "Findellipse measure received a null Image pointer.";
        LogFindellipseMeasureProbe("measure_wrapper", "failed", m_measure_failure_reason);
        return;
    }
    LogFindellipseMeasureProbe("measure_wrapper", "running", "Findellipse measure received Image pointer.");
    Measure(*pgetimage);
}
void FindEllipse::shapesetroi(void* pshape)
{
    if (pshape == nullptr)
        return;
    Shape::shapesetroi(pshape);
}
void FindEllipse::easycluster(int igapx, int igapy, int iclusternum)
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

bool FindEllipse::getdisplaysnapshot(FindEllipseDisplaySnapshot& out) const
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

    const PointsShape& points = const_cast<FindEllipse*>(this)->getresultpoints();
    out.has_measure_points = points.size() > 0;
    out.measure_points_count = static_cast<int>(points.size());
    out.has_fit_ellipse = m_has_fit_result;
    out.fit_center_x = m_fit_center_x;
    out.fit_center_y = m_fit_center_y;
    out.fit_radius_x = m_fit_radius_x;
    out.fit_radius_y = m_fit_radius_y;
    out.fit_angle_deg = m_fit_angle_deg;
    out.fit_avgdist = m_fit_avgdist;

    out.gap = m_igap;
    out.linegap = m_iSelectPointGap;
    out.threshold = m_iThreshold;
    out.method = m_iMethod;
    out.scan_line_count = static_cast<int>(m_lines.size());
    out.scan_line_length = m_lines.empty()
        ? 0
        : const_cast<LineShape&>(m_lines[0]).getlinesize();
    out.measure_failure_stage = m_measure_failure_stage;
    out.measure_failure_reason = m_measure_failure_reason;

    out.scan_candidate_lines = m_scan_candidate_lines;
    out.scan_total_candidates = m_scan_total_candidates;
    out.scan_accepted_points_before_gate = m_scan_accepted_points_before_gate;
    out.accepted_min_boundary_ratio = m_scan_accepted_points_before_gate > 0
        ? m_accepted_boundary_ratio_min
        : 0.0;
    out.accepted_max_boundary_ratio = m_scan_accepted_points_before_gate > 0
        ? m_accepted_boundary_ratio_max
        : 0.0;
    out.accepted_avg_boundary_ratio = m_scan_accepted_points_before_gate > 0
        ? m_accepted_boundary_ratio_sum / static_cast<double>(m_scan_accepted_points_before_gate)
        : 0.0;
    out.candidate_policy = m_candidate_policy;

    out.scan_lines_outside_roi_count = m_scan_lines_outside_roi_count;
    out.scan_lines_cross_outside_ellipse_count = m_scan_lines_cross_outside_ellipse_count;
    out.scan_endpoint_norm_min = m_scan_lines_cross_outside_ellipse_count >= 0
        ? m_scan_endpoint_norm_min
        : 0.0;
    out.scan_endpoint_norm_max = m_scan_lines_cross_outside_ellipse_count >= 0
        ? m_scan_endpoint_norm_max
        : 0.0;
    out.scan_endpoint_norm_avg = m_lines.empty()
        ? 0.0
        : (m_scan_endpoint_norm_min + m_scan_endpoint_norm_max) * 0.5;

    out.accepted_points_outside_ellipse_count = m_accepted_points_outside_ellipse_count;
    out.accepted_point_norm_min = m_accepted_point_norm_count > 0
        ? m_accepted_point_norm_min
        : 0.0;
    out.accepted_point_norm_max = m_accepted_point_norm_count > 0
        ? m_accepted_point_norm_max
        : 0.0;
    out.accepted_point_norm_avg = m_accepted_point_norm_count > 0
        ? m_accepted_point_norm_sum / m_accepted_point_norm_count
        : 0.0;

    out.rejected_boundary_band_candidate_count = m_rejected_boundary_band_candidate_count;
    out.rejected_boundary_band_norm_min = m_rejected_boundary_band_candidate_count > 0
        ? m_rejected_boundary_band_norm_min
        : 0.0;
    out.rejected_boundary_band_norm_max = m_rejected_boundary_band_candidate_count > 0
        ? m_rejected_boundary_band_norm_max
        : 0.0;
    out.rejected_boundary_band_norm_avg = m_rejected_boundary_band_candidate_count > 0
        ? m_rejected_boundary_band_norm_sum /
            static_cast<double>(m_rejected_boundary_band_candidate_count)
        : 0.0;

    out.scan_geometry_policy = m_scan_geometry_policy;

    return out.has_roi || out.has_measure_points || out.has_fit_ellipse;
}

void FindEllipse::PublishDisplayShapes(
    ICxShapeSink& sink,
    const std::string& owner_ref) const
{
    FindEllipseDisplaySnapshot snapshot;
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
            "FindEllipse",
            owner_ref,
            "setellipse",
            "roi",
            true,
            false,
            std::move(roi));
    }

    const PointsShape& points = const_cast<FindEllipse*>(this)->getresultpoints();
    if (points.size() > 0)
    {
        auto resultPoints = std::make_unique<PointsShape>();
        for (int i = 0; i < static_cast<int>(points.size()); ++i)
        {
            resultPoints->addpoint(points.getx(i), points.gety(i));
        }

        sink.UpsertShape(
            owner_ref + ".measure_points",
            "FindEllipse",
            owner_ref,
            "",
            "measure_points",
            false,
            true,
            std::move(resultPoints));
    }

    if (snapshot.has_fit_ellipse)
    {
        auto fitEllipse = std::make_unique<EllipseShape>(
            snapshot.fit_center_x,
            snapshot.fit_center_y,
            snapshot.fit_radius_x,
            snapshot.fit_radius_y);

        sink.UpsertShape(
            owner_ref + ".fit_ellipse",
            "FindEllipse",
            owner_ref,
            "",
            "result",
            false,
            true,
            std::move(fitEllipse));
    }
}
