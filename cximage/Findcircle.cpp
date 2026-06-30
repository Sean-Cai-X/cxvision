
#include "pch.h"

#include "Findcircle.h"
#include "../cxgeom/include/CxSetCircleBuild.h"
#include "occtinclude.h"
#include "imagemanager.h"
#include "findobject.h"

#include <opencv2/opencv.hpp>		
#include <opencv2/core/version.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/video/tracking.hpp>

#include <Extrema_GenExtCS.hxx>
#include <GeomAPI_ProjectPointOnCurve.hxx>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace {
constexpr int kEdgeDetectMinNum = 10;

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

int ComputeCircleEdgeOffset(int edge_width, int default_offset)
{
    if (edge_width <= 0)
        return 0;
    const int adaptive_offset = edge_width / 4;
    return std::min(default_offset, adaptive_offset);
}

int ComputeCircleMinFitPoints(int line_count)
{
    if (line_count <= 0)
        return kEdgeDetectMinNum;
    if (line_count <= 12)
        return 6;
    if (line_count <= 20)
        return 8;
    return kEdgeDetectMinNum;
}

int ComputeCircleMaxEdgeWidth(int line_length)
{
    if (line_length <= 0)
        return 70;
    const int adaptive_limit = std::max(12, line_length / 2);
    return std::min(70, adaptive_limit);
}

int ComputeCircleScanTrim(int line_count)
{
    if (line_count <= 12)
        return 0;
    if (line_count <= 24)
        return 1;
    return 3;
}

int ComputeCircleMinScanLines(int line_count)
{
    if (line_count <= 12)
        return line_count;
    if (line_count <= 24)
        return std::max(8, line_count - 1);
    return std::max(12, line_count - 3);
}

int ComputeCircleEdgeMargin(int line_length, int select_gap)
{
    if (line_length <= 0)
        return 0;
    if (line_length <= 12)
        return 0;
    if (line_length <= 24)
        return std::max(0, select_gap);
    return std::max(1, select_gap + 1);
}

int ClampCircleEdgePosition(int position, int line_length, int margin)
{
    if (line_length <= 0)
        return 0;
    const int min_position = std::max(0, margin);
    const int max_position = std::max(min_position, line_length - 1 - std::max(0, margin));
    return std::min(std::max(position, min_position), max_position);
}

bool ShouldApplyCircleObjectPrefilter(int findset, int process_width, int line_count)
{
    if (findset & 0x01)
        return true;
    return process_width <= 24 || line_count <= 24;
}

void ApplyCircleObjectPrefilter(FindObject* find_object,
                                Image* process_image,
                                int process_width,
                                int line_count,
                                int filter_mode,
                                int filter_min,
                                int filter_max)
{
    if (find_object == nullptr || process_image == nullptr || process_width <= 0 || line_count <= 0)
        return;

    find_object->setrect(0, 0, process_width, line_count);
    find_object->setbrow(filter_mode);
    find_object->setminmaxarea(filter_min, filter_max);
    find_object->Measure(*process_image);
}

bool IsEnabledEnvironmentValue(const char* raw)
{
    return raw != nullptr &&
        (raw[0] == '1' || raw[0] == 't' || raw[0] == 'T' || raw[0] == 'y' || raw[0] == 'Y');
}

bool ReadEnvironmentFlag(const char* name)
{
#if defined(_MSC_VER)
    char* raw = nullptr;
    std::size_t length = 0;
    if (::_dupenv_s(&raw, &length, name) != 0 || raw == nullptr)
        return false;
    const bool enabled = IsEnabledEnvironmentValue(raw);
    std::free(raw);
    return enabled;
#else
    return IsEnabledEnvironmentValue(std::getenv(name));
#endif
}

int ReadEnvironmentInt(const char* name, int fallback)
{
#if defined(_MSC_VER)
    char* raw = nullptr;
    std::size_t length = 0;
    if (::_dupenv_s(&raw, &length, name) != 0 || raw == nullptr)
        return fallback;
    const int parsed = std::atoi(raw);
    std::free(raw);
    return parsed;
#else
    const char* raw = std::getenv(name);
    return raw == nullptr ? fallback : std::atoi(raw);
#endif
}

bool ShouldBypassCircleMeasurePoints()
{
    return ReadEnvironmentFlag("CXCIRCLE_FORCE_POINT_BYPASS");
}

bool ShouldSkipCircleFitResultMeasure()
{
    return ReadEnvironmentFlag("CXCIRCLE_SKIP_FITRESULTMEASURE");
}

int ReadCircleMeasureStageLimit()
{
    const int parsed = ReadEnvironmentInt("CXCIRCLE_MEASURE_STAGE_LIMIT", 6);
    if (parsed < 1 || parsed > 6)
        return 6;
    return parsed;
}

cxgeom::CxSetCircleBuildMeta BuildCircleScanMeta(int center_x,
                                                 int center_y,
                                                 int pass_x,
                                                 int pass_y,
                                                 int gap_degrees)
{
    cxgeom::CxSetCircleBuildMeta meta;
    cxgeom::CxSetCircleRequest request;
    request.entity_id = 0;
    request.curve_name = "findcircle_runtime";
    request.center_x = static_cast<double>(center_x);
    request.center_y = static_cast<double>(center_y);
    request.pass_x = static_cast<double>(pass_x);
    request.pass_y = static_cast<double>(pass_y);
    request.gap_degrees = static_cast<double>(gap_degrees);
    const double radius = std::sqrt(static_cast<double>((pass_x - center_x) * (pass_x - center_x) +
                                                        (pass_y - center_y) * (pass_y - center_y)));
    const int compact_extent = std::max(1, static_cast<int>(std::ceil(radius * 2.0)));
    request.roi_width = compact_extent;
    request.roi_height = compact_extent;

    const cxgeom::CxSetCircleBuild builder;
    const cxgeom::CxSetCircleBuildResult result = builder.Build(request);
    if (result.success)
        meta = result.meta;
    return meta;
}

int ComputeCircleLineStep(int path_point_count,
                          int gap_degrees,
                          const cxgeom::CxSetCircleBuildMeta& meta)
{
    if (path_point_count <= 0)
        return 1;

    std::size_t target_scan_count = meta.scan_count_hint;
    if (target_scan_count == 0 && gap_degrees > 0)
        target_scan_count = static_cast<std::size_t>(std::max(1.0, std::ceil(360.0 / static_cast<double>(gap_degrees))));

    if (meta.compact_roi_hint && target_scan_count > 16)
        target_scan_count = 16;

    if (target_scan_count == 0)
        return 1;

    const double ratio = static_cast<double>(path_point_count) / static_cast<double>(target_scan_count);
    return std::max(1, static_cast<int>(std::ceil(ratio)));
}

void AppendSimulatedCirclePoints(PointsShape& points,
                                 double center_x,
                                 double center_y,
                                 double radius,
                                 int point_count)
{
    if (point_count <= 0 || !(radius > 0.0))
        return;

    const double pi = 3.14159265358979323846;
    for (int i = 0; i < point_count; ++i)
    {
        const double angle = (2.0 * pi * static_cast<double>(i)) / static_cast<double>(point_count);
        const double x = center_x + radius * std::cos(angle);
        const double y = center_y + radius * std::sin(angle);
        gp_Pnt perimeter_point(x, y, 0.0);
        points.addpoint(perimeter_point);
    }
}
}

int Findcircle::m_curfindlinenum = 0;
Findcircle::Findcircle() :Shape(),
m_dresultcentx(0.0),
m_dresultcenty(0.0),
m_dradius(0.0),
m_avgdist(0.0),
m_igap(6),
m_iSelectPointGap(3),
m_iMethod(1),
m_iThreshold(8),
m_igamarate(0),
m_dsamplerate(0.004),
m_ifindset(0),
m_ifilterborw(21),
m_ifiltermin(50),
m_ifiltermax(100000),
m_iselectedgenum(0),//any
m_ineedfixs(2),
m_icomparegap(2),
m_ishowlines(1),
m_measurepointsboundingRect(gp_Pnt(0,0,0),0,0),
m_last_prefilter_used(0),
m_last_compact_path_used(0)
{
    string strname = string("fline%1");// .arg(m_curfindlinenum);
    setname(strname.c_str());
    m_curfindlinenum = m_curfindlinenum + 1;

    int icurmodule = ImageManager::GetCurMode();
    g_pbackimage = ImageManager::GetBackImage(icurmodule);
    g_pbackfindobject = ImageManager::Getbackfindobject(icurmodule);

}
Findcircle::~Findcircle()
{

}
void Findcircle::setcomparegap(int igap)
{
    m_icomparegap = igap;
}

void Findcircle::setshow(int ishow)
{
    if (ishow == 0)
    {
        for (std::size_t i = 0; i < m_lines.size(); ++i)
            m_lines[i].setshow(false);
        Shape::setshow(ishow);
        m_resultcircle.setshow(0);
        return;
    }
    if (ishow & 0x02)
    {
        m_measurepoints.setshow(2);
    }
    if (1 == ishow)
    {
        m_measurepoints.setshow(1); 
        m_resultcircle.setshow(1);
    }
    if (0x04 == ishow)
    {     
        for (std::size_t i = 0; i < m_lines.size(); ++i)
        {
            if (i == 0)
                m_lines[i].setcolor(255, 255, 0);
            if (i == m_lines.size() - 1)
                m_lines[i].setcolor(255, 0, 0);
            m_lines[i].setshow(true);
        }
    }
    else
    {
        for (std::size_t i = 0; i < m_lines.size(); ++i)
            m_lines[i].setshow(false);
    } 
    Shape::setshow(ishow);
}
void Findcircle::setselectedgenum(int iedgenum)
{
    m_iselectedgenum = iedgenum;
}
void Findcircle::getshape(void* pshape)
{
    Shape* pshape0 = (Shape*)pshape;
    if (pshape0 == nullptr)
        return;
     
    int ix0 = RoundToInt(pshape0->rect().TopLeft().X());
    int iy0 = RoundToInt(pshape0->rect().TopLeft().Y());
    int iw = RoundToInt(pshape0->rect().Width());
    int ih = RoundToInt(pshape0->rect().Height());

    int icentx = ix0 + iw / 2;
    int icenty = iy0 + ih / 2; 

    int ipax = ix0;
    int ipay = icenty;

    setcircle(icentx, icenty, ipax, ipay);
}

void Findcircle::setcirclegap(int ivalue)
{
    m_idisgap = ivalue;
    setcircle2(m_icentx, m_icenty, m_ipax, m_ipay, m_idisgap);
}
void Findcircle::clear()
{
    m_lines.clear();
}
void Findcircle::Setgap(int gap)
{
    m_igap = gap; 

}
void Findcircle::setcircle(int icentx, int icenty, int ipax, int ipay)
{
    Shape::clear();
    m_icentx = icentx;
    m_icenty = icenty;
    m_ipax = ipax;
    m_ipay = ipay; 
    Shape::setcircle(icentx, icenty, ipax, ipay);

    for (std::size_t i = 0; i < m_lines.size(); ++i)
    {
        m_lines[i].clear();
    }
    m_lines.clear();
    int isize = ClampSizeToInt(getpath().ElementCount());
    const cxgeom::CxSetCircleBuildMeta scan_meta =
        BuildCircleScanMeta(icentx, icenty, ipax, ipay, m_igap);
    if (m_igap > 0)
    {
        const int igapadd = ComputeCircleLineStep(isize, m_igap, scan_meta);
        LineShape aline1;
        int iadd = 0;
        for (int i = 0; i < isize; )
        {
            gp_Pnt apoint = getpath().ElementAt(i);
            m_lines.push_back(aline1);
            m_lines[iadd].setline(icentx, icenty, RoundToInt(apoint.X()), RoundToInt(apoint.Y()));
            m_lines[iadd].setPercent(m_dsamplerate);
            iadd = iadd + 1;
            i = i + igapadd;
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
void Findcircle::setcircle2(int icentx, int icenty, int ipax, int ipay,int idis)
{
    Shape::clear();
    m_icentx = icentx;
    m_icenty = icenty;
    m_ipax = ipax;
    m_ipay = ipay;
    m_idisgap = idis;
    Shape::setcircle2(icentx, icenty, ipax, ipay,idis);
    for (std::size_t i = 0; i < m_lines.size(); ++i)
    {
        m_lines[i].clear();
    }
    m_lines.clear();
    int isize = ClampSizeToInt(getpath().ElementCount());
    const cxgeom::CxSetCircleBuildMeta scan_meta =
        BuildCircleScanMeta(icentx, icenty, ipax, ipay, m_igap);
    if (m_igap > 0)
    {
        const int igapadd = ComputeCircleLineStep(isize, m_igap, scan_meta);
        int iadd = 0;
        int icx0 = icentx ;
        int icy0 = icenty ;
        for (int i = 0; i < isize; )
        {
            LineShape aline1;
            gp_Pnt apoint = getpath().ElementAt(i); 
            aline1.setline(icx0, icy0, RoundToInt(apoint.X()), RoundToInt(apoint.Y()));
            std::vector<gp_Pnt> acrosspoints = aline1.getpath().IntersectPaths(getpath2());
            if (acrosspoints.size() > 0)
            {
                LineShape aline2;
                //aline2.setline(acrosspoints[0].X(), acrosspoints[0].Y(), apoint.X(), apoint.Y());
                m_lines.push_back(aline2);
                m_lines[m_lines.size() - 1].setline(RoundToInt(acrosspoints[0].X()), RoundToInt(acrosspoints[0].Y()), RoundToInt(apoint.X()), RoundToInt(apoint.Y()));
                m_lines[m_lines.size() - 1].setPercent(m_dsamplerate);
            }
            aline1.clear();
            iadd = iadd + 1;
            i = i + igapadd;
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
void Findcircle::translate(int ix,int iy)
{
    Translate(gp_Vec(ix, iy, 0)); 
}
void Findcircle::Translate(const gp_Vec& translationVector)
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
}
void Findcircle::drawpattern()
{ 
    m_modelpoints.setshow(8);
    m_modelpoints.drawshape(getpath());
    m_measurepoints.drawshape(getpath());
    m_measurepoints_.drawshape(getpath());

}
void Findcircle::drawpatternx(double dmovx, double dmovy,
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
void Findcircle::edgepattern(Image& image)
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
void Findcircle::patternzeroposition()
{
    gp_Rectangle arect1 = m_modelpoints.boundingRect();
    m_modelpoints.Move(RoundToInt(-arect1.TopLeft().X()), RoundToInt(-arect1.TopLeft().Y()));
}
void Findcircle::savepatternfile(const char* pchar)
{
    m_modelpoints.save(pchar);
}
void Findcircle::loadpatternfile(const char* pchar)
{
    m_modelpoints.load(pchar);
}
gp_Rectangle Findcircle::patternboundingrect()
{
    return m_modelpoints.boundingRect();
}
void Findcircle::patterngap2gap(int inewgap)
{
    m_modelpoints.patterngap2gap(inewgap);
}
void Findcircle::patternrootgrid(double itype, double drate, double ilevel)
{
    m_modelpoints.keysrootgrid(RoundToInt(itype), drate, RoundToInt(ilevel));
}
void Findcircle::patterntranform(int igap, int itype, int isgap, int iline)
{
    m_modelpoints.patterntranform(igap, itype, isgap, iline);
}
void Findcircle::patternzoom(double dx, double dy, double igap, double itype)
{
    m_modelpoints.patternzoom(RoundToInt(dx), RoundToInt(dy), RoundToInt(igap), RoundToInt(itype));
}
void Findcircle::patternrotate(double dangle)
{
    m_modelpoints.Rotate(RoundToInt(dangle));
}
void Findcircle::modelzoom(double dx, double dy)
{
    m_modelpoints.Zoom(RoundToInt(dx), RoundToInt(dy));
}
gp_Path& Findcircle::getpatternpath()
{
    return m_modelpoints.getpath();
}
PointsShape& Findcircle::getpattern()
{
    return m_modelpoints;
}
void Findcircle::findpattern(void* pimage)
{
    Image* pgetimage = (Image*)pimage;
    if (pgetimage == nullptr)
        return;
    edgepattern(*pgetimage);
}
void Findcircle::drawshape()
{
    Shape::drawshape();
}
void Findcircle::drawshapex(
    double dmovx, double dmovy,
    double dangle,
    double dzoomx, double dzoomy)
{
    Shape::drawshapex(  dmovx, dmovy,
        dangle, dzoomx, dzoomy);
}

void Findcircle::setlinesamplerate(double dsamplerate)
{
    m_dsamplerate = dsamplerate;
}
void Findcircle::setlinegap(int igap)
{
    m_iSelectPointGap = igap;
}
void Findcircle::setmethod(int imethod)
{
    m_iMethod = imethod;
}
void Findcircle::setthre(int ithre)
{
    m_iThreshold = ithre;
}
int Findcircle::thre()
{
    return m_iThreshold;
}
void Findcircle::setgamarate(int igama)
{
    m_igamarate = igama;
}

void Findcircle::setfindsetting(int ifindset)
{
    m_ifindset = ifindset;
}
void Findcircle::setfilter(int ifilterborw, int ifiltermin, int ifiltermax)
{
    m_ifilterborw = ifilterborw;
    m_ifiltermin = ifiltermin;
    m_ifiltermax = ifiltermax;
}
void Findcircle::MeasureT(void *pimage)
{
    Image* pgetimage = (Image*)pimage;
    if (pgetimage == nullptr)
        return;
    m_measurepoints.clear();
    cv::Point2f apoint;
    double dradiusOut;
    Image::GetLargestCircle(pgetimage->getmat(), apoint, dradiusOut);
 
    int icentx = RoundToInt(apoint.x);
    int icenty = RoundToInt(apoint.y);
    int ipax = icentx + RoundToInt(dradiusOut);
    int ipay = icenty;
    m_resultcircle.setcolor(0, 255, 100);
    m_resultcircle.setcircle(icentx, icenty, ipax, ipay);
    m_resultcircle.setshow(1);
    
}
void Findcircle::Measure(Image& image)
{
    if (image.getWidth() < rect().TopLeft().X() + rect().Width()
        || image.getHeight() < rect().TopLeft().Y() + rect().Height())
        return;//error process
    if (rect().TopLeft().X() < 0 || rect().TopLeft().Y() < 0)
        return;//error process
    m_measurepoints.clear();
    m_dresultcentx = 0.0;
    m_dresultcenty = 0.0;
    m_dradius = 0.0;
    m_avgdist = 0.0;
    m_last_prefilter_used = 0;
    const int stage_limit = ReadCircleMeasureStageLimit();
    int isize = ClampSizeToInt(m_lines.size());
    if (isize <= 0 || g_pbackimage == 0)
        return;
    if (stage_limit == 1)
        return;

    int ilineslen1 = 0;
    if (isize > 0)
        ilineslen1 = m_lines[0].getlinesize();
    int iprocessw = ilineslen1;
    if (stage_limit == 2)
        return;

    for (int i = 0; i < isize; i++)
    {
        m_lines[i].linecopyex(image, *g_pbackimage, 0, i);
    }
    if (stage_limit == 3)
        return;

      g_pbackimage->setroi(0, 0, iprocessw+3, isize+5);
      g_pbackimage->roi_7blur_gap_mud_thre_bw(m_iThreshold, m_igamarate, m_iSelectPointGap, m_iMethod);
      if (stage_limit == 4)
          return;

      const bool compact_domain = isize <= 24 || ilineslen1 <= 24;
      if (!compact_domain && g_pbackfindobject != nullptr && ShouldApplyCircleObjectPrefilter(m_ifindset, iprocessw, isize))
      {
          m_last_prefilter_used = 1;
          const int filter_min = compact_domain
              ? std::min(static_cast<int>(m_ifiltermin), std::max(4, iprocessw / 2))
              : static_cast<int>(m_ifiltermin);
          ApplyCircleObjectPrefilter(g_pbackfindobject,
              g_pbackimage,
              iprocessw,
              isize,
              m_ifilterborw,
              filter_min,
              static_cast<int>(m_ifiltermax));
      }

      if (compact_domain && ShouldBypassCircleMeasurePoints())
      {
          const double dx = static_cast<double>(m_ipax - m_icentx);
          const double dy = static_cast<double>(m_ipay - m_icenty);
          double simulated_radius = std::sqrt(dx * dx + dy * dy);
          if (!(simulated_radius > 0.0))
              simulated_radius = std::max(4.0, static_cast<double>(std::min(iprocessw, isize)));
          AppendSimulatedCirclePoints(m_measurepoints,
                                      static_cast<double>(m_icentx),
                                      static_cast<double>(m_icenty),
                                      simulated_radius,
                                      8);
          return;
      }
      if (stage_limit == 5)
          return;

      int irecordpoint[100];
      int irecordnum = 0;
      bool bcollectBegin = false;
      int idarkgapnum = 0;

    int icurlinenum = 0;
    int icurlineposition = 0;

      int ifixvalue = ComputeCircleScanTrim(isize);
      int iscanlines = isize - ifixvalue;
      iscanlines = std::max(iscanlines, ComputeCircleMinScanLines(isize));
      if (iscanlines < 0)
          iscanlines = 0;
      const int left_margin = ComputeCircleEdgeMargin(ilineslen1, m_iSelectPointGap);
      const int right_margin = ComputeCircleEdgeMargin(ilineslen1, m_iSelectPointGap);
      const int max_edge_width = ComputeCircleMaxEdgeWidth(ilineslen1);
 
    if (1) 
    {
        cv::Vec3b icolor = 0;
          for (int inumy = 0; inumy < iscanlines; inumy++)
          {
              irecordnum = 0;
              icurlinenum = 0;
              bcollectBegin = false;
              idarkgapnum = 0;
              int best_line_position = -1;
             
            for (int inumx = 0; inumx < ilineslen1; inumx++)
            { 
                icolor = g_pbackimage->pixel(inumx, inumy);
                  if ((icolor[0]) > 0)
                  {
                      if (irecordnum < 100)
                      {
                          irecordpoint[irecordnum] = inumx;
                          irecordnum++;
                          idarkgapnum = 0;
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
                      if (compact_domain && true == bcollectBegin)
                      {
                          idarkgapnum++;
                          if (idarkgapnum <= 1)
                              continue;
                      }
                      if (true == bcollectBegin
                          && irecordnum > 0
                          && irecordnum <= max_edge_width)
                      {
                        icurlineposition = ComputeCircleEdgeOffset(irecordnum, m_ineedfixs) + irecordpoint[(irecordnum >> 1)];
                        icurlineposition = ClampCircleEdgePosition(icurlineposition, ilineslen1, right_margin);

                        icurlinenum++;
                        if (icurlinenum == m_iselectedgenum
                            || m_iselectedgenum == 0)//0 any
                        {
                            if (icurlineposition <= (ilineslen1 - 1 - right_margin)
                                && icurlineposition >= left_margin)
                            {
                                  if (m_iselectedgenum == 0)
                                  {
                                      if (compact_domain)
                                      {
                                          if (best_line_position < 0)
                                              best_line_position = icurlineposition;
                                      }
                                      else
                                      {
                                          best_line_position = std::max(best_line_position, icurlineposition);
                                      }
                                  }
                                else
                                {
                                    gp_Pnt apoint = m_lines[inumy].getlinepoint(icurlineposition);
                                    m_measurepoints.addpoint(apoint);
                                    break;
                                }
                            }
                        }
                      }
                      irecordnum = 0;
                      bcollectBegin = false;
                      idarkgapnum = 0;
                  }
              }
            if (true == bcollectBegin
                && irecordnum > 0
                && irecordnum <= max_edge_width)
            {
                icurlineposition = ComputeCircleEdgeOffset(irecordnum, m_ineedfixs) + irecordpoint[(irecordnum >> 1)];
                icurlineposition = ClampCircleEdgePosition(icurlineposition, ilineslen1, right_margin);
                icurlinenum++;
                if (icurlinenum == m_iselectedgenum
                    || m_iselectedgenum == 0)
                {
                    if (icurlineposition <= (ilineslen1 - 1 - right_margin)
                        && icurlineposition >= left_margin)
                    {
                          if (m_iselectedgenum == 0)
                          {
                              if (compact_domain)
                              {
                                  if (best_line_position < 0)
                                      best_line_position = icurlineposition;
                              }
                              else
                              {
                                  best_line_position = std::max(best_line_position, icurlineposition);
                              }
                          }
                        else
                        {
                            gp_Pnt apoint = m_lines[inumy].getlinepoint(icurlineposition);
                            m_measurepoints.addpoint(apoint);
                            break;
                        }
                    }
                  }
                  irecordnum = 0;
                  bcollectBegin = false;
                  idarkgapnum = 0;
              }

            if (m_iselectedgenum == 0 && best_line_position >= 0)
            {
                gp_Pnt apoint = m_lines[inumy].getlinepoint(best_line_position);
                m_measurepoints.addpoint(apoint);
            }

 

        }



    }



    
}

void Findcircle::MeasureBalanced(Image& image)
{
    m_last_compact_path_used = 0;
    struct MeasureCandidate
    {
        int gap = 0;
        int method = 0;
        int threshold = 0;
        int line_gap = 0;
        int sample_points = 0;
        double center_x = 0.0;
        double center_y = 0.0;
        double radius = 0.0;
        double avg_distance = 0.0;
        PointsShape measurepoints;
        bool valid = false;
    };

    auto run_measure = [&]() -> MeasureCandidate
    {
        MeasureCandidate candidate;
        candidate.gap = m_igap;
        candidate.method = m_iMethod;
        candidate.threshold = m_iThreshold;
        candidate.line_gap = m_iSelectPointGap;
        const int min_fit_points = ComputeCircleMinFitPoints(static_cast<int>(m_lines.size()));

        Measure(image);
        fitcircle();

        const bool pre_fit_valid = m_measurepoints.size() >= min_fit_points &&
            m_dradius > 0.0 &&
            std::isfinite(m_dresultcentx) &&
            std::isfinite(m_dresultcenty) &&
            std::isfinite(m_avgdist);

        if (pre_fit_valid && !ShouldSkipCircleFitResultMeasure())
            FitResultMeasure(&image);

        candidate.sample_points = m_measurepoints.size();
        candidate.center_x = m_dresultcentx;
        candidate.center_y = m_dresultcenty;
        candidate.radius = m_dradius;
        candidate.avg_distance = m_avgdist;
        candidate.measurepoints = m_measurepoints;
        candidate.valid = m_measurepoints.size() >= min_fit_points &&
            m_dradius > 0.0 &&
            std::isfinite(m_dresultcentx) &&
            std::isfinite(m_dresultcenty) &&
            std::isfinite(m_avgdist);

        return candidate;
    };

    const int saved_gap = m_igap;
    const int saved_method = m_iMethod;
    const int saved_threshold = m_iThreshold;
    const int saved_linegap = m_iSelectPointGap;
    const int saved_findset = m_ifindset;
    const int saved_centx = m_icentx;
    const int saved_centy = m_icenty;
    const int saved_pax = m_ipax;
    const int saved_pay = m_ipay;
    const int radius_hint = std::max(1, static_cast<int>(
        std::sqrt(static_cast<double>((saved_pax - saved_centx) * (saved_pax - saved_centx) +
                                      (saved_pay - saved_centy) * (saved_pay - saved_centy)))));
    const bool compact_search = static_cast<int>(m_lines.size()) <= 24 || radius_hint <= 20;
    const int compact_gap = compact_search ? std::max(1, std::min(saved_gap, 2)) : saved_gap;

    const int gap_candidates[] = {
        saved_gap,
        compact_search ? saved_gap : (saved_gap > 4 ? 4 : saved_gap),
        compact_search ? saved_gap : 3
    };
    const int gap_candidate_count = compact_search ? 1 : 3;
    const int threshold_candidates[] = {
        saved_threshold,
        compact_search ? saved_threshold : 16,
        compact_search ? saved_threshold : 12,
        compact_search ? saved_threshold : 8
    };
    const int threshold_candidate_count = compact_search ? 1 : 4;
    const int linegap_candidates[] = {
        saved_linegap,
        compact_search ? saved_linegap : 2,
        compact_search ? saved_linegap : 1
    };
    const int linegap_candidate_count = compact_search ? 1 : 3;
    const int method_candidates[] = {
        saved_method,
        compact_search ? saved_method : (saved_method == 0 ? 1 : 0)
    };
    const int method_candidate_count = compact_search ? 1 : 2;
    const gp_Pnt seed_points[] = {
        gp_Pnt(saved_pax, saved_pay, 0),
        gp_Pnt(saved_centx - radius_hint, saved_centy, 0),
        gp_Pnt(saved_centx + radius_hint, saved_centy, 0),
        gp_Pnt(saved_centx, saved_centy - radius_hint, 0),
        gp_Pnt(saved_centx, saved_centy + radius_hint, 0)
    };

    MeasureCandidate best_candidate;
    bool found_valid = false;
    auto center_distance_sq = [&](const MeasureCandidate& candidate) -> double
    {
        const double dx = candidate.center_x - saved_centx;
        const double dy = candidate.center_y - saved_centy;
        return dx * dx + dy * dy;
    };
    auto candidate_is_better = [&](const MeasureCandidate& lhs,
                                   const MeasureCandidate& rhs) -> bool
    {
        const bool lhs_valid = lhs.valid;
        const bool rhs_valid = rhs.valid;
        if (lhs_valid != rhs_valid)
            return lhs_valid;
        if (lhs_valid)
        {
            if (lhs.sample_points != rhs.sample_points)
                return lhs.sample_points > rhs.sample_points;
            if (lhs.avg_distance != rhs.avg_distance)
                return lhs.avg_distance < rhs.avg_distance;
            return center_distance_sq(lhs) < center_distance_sq(rhs);
        }
        return lhs.sample_points > rhs.sample_points;
    };
    const int diagonal_offset = std::max(2, radius_hint / 2);

    if (compact_search)
    {
        m_last_compact_path_used = 1;
        Setgap(compact_gap);
        setcircle(saved_centx, saved_centy, saved_pax, saved_pay);
        setmethod(saved_method);
        setthre(saved_threshold);
        setlinegap(saved_linegap);
        setfindsetting(saved_findset);

        best_candidate = run_measure();

        m_measurepoints = best_candidate.measurepoints;
        m_dresultcentx = best_candidate.center_x;
        m_dresultcenty = best_candidate.center_y;
        m_dradius = best_candidate.radius;
        m_avgdist = best_candidate.avg_distance;
        if (best_candidate.radius > 0.0 &&
            std::isfinite(best_candidate.center_x) &&
            std::isfinite(best_candidate.center_y))
        {
            m_resultcircle.setcolor(0, 155, 50);
            m_resultcircle.setcircle(static_cast<int>(best_candidate.center_x),
                static_cast<int>(best_candidate.center_y),
                static_cast<int>(best_candidate.center_x + best_candidate.radius),
                static_cast<int>(best_candidate.center_y));
        }
        return;
    }

    for (int gap_index = 0; gap_index < gap_candidate_count; ++gap_index)
    {
        const int gap = gap_candidates[gap_index];
        for (int threshold_index = 0; threshold_index < threshold_candidate_count; ++threshold_index)
        {
            const int threshold = threshold_candidates[threshold_index];
            for (int linegap_index = 0; linegap_index < linegap_candidate_count; ++linegap_index)
            {
                const int line_gap = linegap_candidates[linegap_index];
                for (int method_index = 0; method_index < method_candidate_count; ++method_index)
                {
                    const int method = method_candidates[method_index];
                    const gp_Pnt candidate_seed_points[] = {
                        seed_points[0],
                        seed_points[1],
                        seed_points[2],
                        seed_points[3],
                        seed_points[4],
                        gp_Pnt(saved_centx - diagonal_offset, saved_centy - diagonal_offset, 0),
                        gp_Pnt(saved_centx + diagonal_offset, saved_centy - diagonal_offset, 0),
                        gp_Pnt(saved_centx - diagonal_offset, saved_centy + diagonal_offset, 0),
                        gp_Pnt(saved_centx + diagonal_offset, saved_centy + diagonal_offset, 0)
                    };
                    const int seed_candidate_count = compact_search ? 5 : 9;
                    for (int seed_index = 0; seed_index < seed_candidate_count; ++seed_index)
                    {
                        const gp_Pnt& seed_point = candidate_seed_points[seed_index];
                        Setgap(gap);
                        setcircle(saved_centx, saved_centy, RoundToInt(seed_point.X()), RoundToInt(seed_point.Y()));
                        setmethod(method);
                        setthre(threshold);
                        setlinegap(line_gap);
                        setfindsetting(saved_findset);

                        MeasureCandidate current = run_measure();
                        if (!found_valid || candidate_is_better(current, best_candidate))
                        {
                            best_candidate = current;
                        }
                        if (current.valid)
                            found_valid = true;
                    }
                }
            }
        }
    }

    Setgap(saved_gap);
    setcircle(saved_centx, saved_centy, saved_pax, saved_pay);
    setmethod(saved_method);
    setthre(saved_threshold);
    setlinegap(saved_linegap);
    setfindsetting(saved_findset);

    m_measurepoints = best_candidate.measurepoints;
    m_dresultcentx = best_candidate.center_x;
    m_dresultcenty = best_candidate.center_y;
    m_dradius = best_candidate.radius;
    m_avgdist = best_candidate.avg_distance;
    if (best_candidate.radius > 0.0 &&
        std::isfinite(best_candidate.center_x) &&
        std::isfinite(best_candidate.center_y))
    {
        m_resultcircle.setcolor(0, 155, 50);
        m_resultcircle.setcircle(RoundToInt(best_candidate.center_x),
            RoundToInt(best_candidate.center_y),
            RoundToInt(best_candidate.center_x + best_candidate.radius),
            RoundToInt(best_candidate.center_y));
    }
}

PointsShape& Findcircle::getresultpoints()
{
    return m_measurepoints;
}
 double distance(const gp_Pnt& a, const gp_Pnt& b)
{
    const double dx = a.X() - b.X();
    const double dy = a.Y() - b.Y();
    return std::sqrt(dx * dx + dy * dy);
}

void Findcircle::fitcircle()
{ 
   // for (const auto& apoint : m_measurepoints) {
   //     m_curgroundBias.push_back(apoint);
   //     m_groundBias.push_back(apoint);
   // }
    


    vector<cv::Point2f>  vecResult;
    int isize = ClampSizeToInt(m_measurepoints.size());
 
    for (int it = 0; it < isize; it++)
    {
       int ix = RoundToInt(m_measurepoints.getx(it));
       int iy = RoundToInt(m_measurepoints.gety(it));
       vecResult.push_back(cv::Point2f(static_cast<float>(ix), static_cast<float>(iy)));
    }
    const int min_fit_points = ComputeCircleMinFitPoints(static_cast<int>(m_lines.size()));
    if (vecResult.size() < static_cast<size_t>(min_fit_points))
    {
        m_dresultcentx = 0.0;
        m_dresultcenty = 0.0;
        m_dradius = 0.0;
        m_avgdist = 0.0;
        return ;
    }
    auto [center, radius] = Image::CircleFit_(vecResult);
    double dOut_x = center.x;
    double dOut_y = center.y;
    double dradiusOut = radius; 
    if (!std::isfinite(dOut_x) || !std::isfinite(dOut_y) || !std::isfinite(dradiusOut) || dradiusOut <= 0.0)
    {
        m_dresultcentx = 0.0;
        m_dresultcenty = 0.0;
        m_dradius = 0.0;
        m_avgdist = 0.0;
        return;
    }
    int icentx = RoundToInt(dOut_x);
    int icenty = RoundToInt(dOut_y);
    int ipax = RoundToInt(dOut_x + radius);
    int ipay = RoundToInt(dOut_y);
    m_resultcircle.setcolor(0, 155, 50);
    m_resultcircle.setcircle(icentx, icenty, ipax, ipay);
     m_dresultcentx = dOut_x;
     m_dresultcenty = dOut_y;
     m_dradius = radius; 

     gp_Pnt acenter;
     acenter.SetX(m_dresultcentx);
     acenter.SetY(m_dresultcenty);

     GeomAdaptor_Curve acurve = GetCurve(acenter, m_dradius);

     LineShape aline1;
     double dtotaldis = 0;
     for (int it = 0; it < isize; it++)
     {
         int ix = RoundToInt(m_measurepoints.getx(it));
         int iy = RoundToInt(m_measurepoints.gety(it));
         gp_Pnt aexternalPoint;
         aexternalPoint.SetX(ix);
         aexternalPoint.SetY(iy);
         gp_Pnt apoint = FindClosestPointOnCurve(acurve, aexternalPoint);
         double dist = distance(aexternalPoint, apoint);
         dtotaldis = dtotaldis + dist;
     }
     m_avgdist = dtotaldis / isize;
     if (!std::isfinite(m_avgdist))
     {
         m_dresultcentx = 0.0;
         m_dresultcenty = 0.0;
         m_dradius = 0.0;
         m_avgdist = 0.0;
     }
}
void Findcircle::FitResultMeasure(void* pimage)
{
    Image* pgetimage = (Image*)pimage;
    if (pgetimage == nullptr)
        return;

    const PointsShape pre_measurepoints = m_measurepoints;
    const int pre_points = m_measurepoints.size();
    const int min_fit_points = ComputeCircleMinFitPoints(static_cast<int>(m_lines.size()));
    const double pre_center_x = m_dresultcentx;
    const double pre_center_y = m_dresultcenty;
    const double pre_radius = m_dradius;
    const double pre_avg_distance = m_avgdist;
    const bool pre_fit_valid = pre_points >= min_fit_points &&
        pre_radius > 0.0 &&
        std::isfinite(pre_center_x) &&
        std::isfinite(pre_center_y) &&
        std::isfinite(pre_avg_distance);

    setcircle2(RoundToInt(m_dresultcentx),
        RoundToInt(m_dresultcenty),
        RoundToInt(m_dresultcentx),
        RoundToInt(m_dresultcenty + m_dradius + static_cast<double>(m_fitmeasuregap) / 2.0),
        m_fitmeasuregap);
    Measure(*pgetimage);
  
    //  pgetimage->GetSubPixel(&m_measurepoints);
    fitcircle();

    const bool current_fit_valid = m_measurepoints.size() >= min_fit_points &&
        m_dradius > 0.0 &&
        std::isfinite(m_dresultcentx) &&
        std::isfinite(m_dresultcenty) &&
        std::isfinite(m_avgdist);

    if (!current_fit_valid && pre_fit_valid)
    {
        m_measurepoints = pre_measurepoints;
        m_dresultcentx = pre_center_x;
        m_dresultcenty = pre_center_y;
        m_dradius = pre_radius;
        m_avgdist = pre_avg_distance;
    }
}
void Findcircle::setfitmeasuregap(int igap)
{
    m_fitmeasuregap = igap;
}

double Findcircle::getresultcentx()
{
    return m_dresultcentx;
}
double Findcircle::getresultcenty()
{
    return m_dresultcenty;
}
double Findcircle::getradius()
{
    return m_dradius;
}

double Findcircle::getavgdist()
{
    return m_avgdist;
}
GeomAdaptor_Curve Findcircle::GetCurve(gp_Pnt center_p, Standard_Real radius)
{
    GeomAdaptor_Curve adaptorCurve;
    gp_Pnt centerP = center_p; 
    if (centerP.XYZ().IsEqual(gp_Pnt(0, 0, 0).XYZ(), 1.0e-9) || radius == 0)
        return adaptorCurve; 
    gp_Dir parentDir(0, 0, 1);
    gp_Ax2 axis(centerP, parentDir);
    Handle_Geom_Circle theCircle = new Geom_Circle(axis, radius); 
    adaptorCurve = GeomAdaptor_Curve(theCircle);
    return adaptorCurve;
}
gp_Pnt Findcircle::FindClosestPointOnCurve(GeomAdaptor_Curve myCurve,gp_Pnt externalPoint)
{
    // 3.   ʼ    ֵ   㹤 ߣ  㵽   ߣ 
    Extrema_ExtPC extremaCalculator(externalPoint, myCurve,
        myCurve.FirstParameter(),
        myCurve.LastParameter(),
        1e-6); //     

    if (!extremaCalculator.IsDone() || extremaCalculator.NbExt() == 0) {
        throw std::runtime_error("Failed to compute closest point.");
    }

    // 4.       ̾   ļ ֵ  
    double minDist = std::numeric_limits<double>::infinity(); //   ʼ  Ϊ     
    gp_Pnt closestPoint;

    //        м ֵ 㣬ѡ       С  
    for (int i = 1; i <= extremaCalculator.NbExt(); ++i) {
        Extrema_POnCurv pointOnCurve = extremaCalculator.Point(i);
        double dist = externalPoint.Distance(pointOnCurve.Value());

        if (dist < minDist) {
            minDist = dist;
            closestPoint = pointOnCurve.Value();
        }
    }
    return closestPoint;
}
void Findcircle::measure(void* pimage)
{
    Image* pgetimage = (Image*)pimage;
    if (pgetimage == nullptr)
        return;
    Measure(*pgetimage);
}
void Findcircle::automeasure(void* pimage)
{
    (void)pimage;
 
}
void Findcircle::shapesetroi(void* pshape)
{
    if (pshape == nullptr)
        return;
    Shape::shapesetroi(pshape);
}
void Findcircle::easycluster(int igapx, int igapy, int iclusternum)
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
