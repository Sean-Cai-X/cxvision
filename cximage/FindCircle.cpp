
#include "pch.h"

#include "FindCircle.h"
#include "CircleShape.h"
#include "PolylineShape.h"
#include "ImageAnnotationLayer.h"
#include "../cxgeom/include/CxSetCircleBuild.h"
#include "occtinclude.h"
#include "imagemanager.h"
#include "findobject.h"
#include "CxAlgorithmTraceSink.h"
#include "CxUnifiedLog.h"

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

void LogFindCircleMeasureProbe(
    const char* phase,
    const char* status,
    const std::string& message)
{
    CXLOG_INFO("FindCircle", phase, status, message);
    CxUnifiedLog::Instance().Flush();
}

std::string FindCircleMeasureMessage(
    const char* detail,
    int image_w,
    int image_h,
    int image_channels,
    int cx,
    int cy,
    int px,
    int py,
    int line_count,
    int line_length,
    int back_w,
    int back_h)
{
    return std::string("detail=") + detail +
        ", image=" + std::to_string(image_w) + "x" + std::to_string(image_h) +
        "x" + std::to_string(image_channels) +
        ", circle=(" + std::to_string(cx) + "," + std::to_string(cy) +
        ")->(" + std::to_string(px) + "," + std::to_string(py) + ")" +
        ", scan_line_count=" + std::to_string(line_count) +
        ", scan_line_length=" + std::to_string(line_length) +
        ", back=" + std::to_string(back_w) + "x" + std::to_string(back_h);
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
    (void)process_width;
    (void)line_count;
    // bit0 is the explicit FindObject prefilter switch. Compact-domain
    // bypass is handled by the caller and must not be encoded here as an
    // unreachable implicit enable path.
    return (findset & 0x01) != 0;
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

int FindCircle::m_curfindlinenum = 0;
FindCircle::FindCircle() :Shape(),
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
FindCircle::~FindCircle()
{

}
void FindCircle::setcomparegap(int igap)
{
    m_icomparegap = igap;
}

void FindCircle::setshow(int ishow)
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
void FindCircle::setselectedgenum(int iedgenum)
{
    m_iselectedgenum = iedgenum;
}
void FindCircle::getshape(void* pshape)
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

void FindCircle::setcirclegap(int ivalue)
{
    m_idisgap = std::max(0, ivalue);

    setcircle2(m_icentx, m_icenty, m_ipax, m_ipay, m_idisgap);
}
void FindCircle::clear()
{
    m_lines.clear();
}
void FindCircle::Setgap(int gap)
{
    m_igap = std::max(1, gap);

    if (m_measure_geometry_request.valid)
    {
        m_measure_geometry_request.gap_degrees = m_igap;
        MarkCircleMeasureGeometryDirty();
        m_measure_geometry_request.version = m_measure_geometry_version;
    }
    else
    {
        MarkCircleMeasureGeometryDirty();
    }
}
void FindCircle::setcircle(int icentx, int icenty, int ipax, int ipay)
{
    m_icentx = icentx;
    m_icenty = icenty;
    m_ipax = ipax;
    m_ipay = ipay;

    UpdateCircleMeasureGeometryRequest(false);

    BuildCircleMeasureGeometryFromRequest(m_measure_geometry_request);

    m_measure_geometry_ready = !m_lines.empty();
    m_measure_geometry_dirty = !m_measure_geometry_ready;

    if (m_measure_geometry_ready)
    {
        m_measure_geometry_built_version = m_measure_geometry_request.version;
    }
}
void FindCircle::setcircle2(int icentx, int icenty, int ipax, int ipay, int idis)
{
    m_icentx = icentx;
    m_icenty = icenty;
    m_ipax = ipax;
    m_ipay = ipay;
    m_idisgap = idis;

    UpdateCircleMeasureGeometryRequest(true);

    BuildCircleMeasureGeometryFromRequest(m_measure_geometry_request);

    m_measure_geometry_ready = !m_lines.empty();
    m_measure_geometry_dirty = !m_measure_geometry_ready;

    if (m_measure_geometry_ready)
    {
        m_measure_geometry_built_version = m_measure_geometry_request.version;
    }
}
void FindCircle::translate(int ix,int iy)
{
    Translate(gp_Vec(ix, iy, 0)); 
}
void FindCircle::Translate(const gp_Vec& translationVector)
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
void FindCircle::drawpattern()
{ 
    m_modelpoints.setshow(8);
    m_modelpoints.drawshape(getpath());
    m_measurepoints.drawshape(getpath());
    m_measurepoints_.drawshape(getpath());

}
void FindCircle::drawpatternx(double dmovx, double dmovy,
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
void FindCircle::edgepattern(Image& image)
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
void FindCircle::patternzeroposition()
{
    gp_Rectangle arect1 = m_modelpoints.boundingRect();
    m_modelpoints.Move(RoundToInt(-arect1.TopLeft().X()), RoundToInt(-arect1.TopLeft().Y()));
}
void FindCircle::savepatternfile(const char* pchar)
{
    m_modelpoints.save(pchar);
}
void FindCircle::loadpatternfile(const char* pchar)
{
    m_modelpoints.load(pchar);
}
gp_Rectangle FindCircle::patternboundingrect()
{
    return m_modelpoints.boundingRect();
}
void FindCircle::patterngap2gap(int inewgap)
{
    m_modelpoints.patterngap2gap(inewgap);
}
void FindCircle::patternrootgrid(double itype, double drate, double ilevel)
{
    m_modelpoints.keysrootgrid(RoundToInt(itype), drate, RoundToInt(ilevel));
}
void FindCircle::patterntranform(int igap, int itype, int isgap, int iline)
{
    m_modelpoints.patterntranform(igap, itype, isgap, iline);
}
void FindCircle::patternzoom(double dx, double dy, double igap, double itype)
{
    m_modelpoints.patternzoom(RoundToInt(dx), RoundToInt(dy), RoundToInt(igap), RoundToInt(itype));
}
void FindCircle::patternrotate(double dangle)
{
    m_modelpoints.Rotate(RoundToInt(dangle));
}
void FindCircle::modelzoom(double dx, double dy)
{
    m_modelpoints.Zoom(RoundToInt(dx), RoundToInt(dy));
}
gp_Path& FindCircle::getpatternpath()
{
    return m_modelpoints.getpath();
}
PointsShape& FindCircle::getpattern()
{
    return m_modelpoints;
}
void FindCircle::findpattern(void* pimage)
{
    Image* pgetimage = (Image*)pimage;
    if (pgetimage == nullptr)
        return;
    edgepattern(*pgetimage);
}
void FindCircle::drawshape()
{
    Shape::drawshape();
}
void FindCircle::drawshapex(
    double dmovx, double dmovy,
    double dangle,
    double dzoomx, double dzoomy)
{
    Shape::drawshapex(  dmovx, dmovy,
        dangle, dzoomx, dzoomy);
}

void FindCircle::setlinesamplerate(double dsamplerate)
{
    m_dsamplerate = dsamplerate;

    if (m_measure_geometry_request.valid)
    {
        m_measure_geometry_request.sample_rate = m_dsamplerate;

        MarkCircleMeasureGeometryDirty();
        m_measure_geometry_request.version = m_measure_geometry_version;
    }
}
void FindCircle::setlinegap(int igap)
{
    m_iSelectPointGap = std::max(1, igap);

    if (m_measure_geometry_request.valid)
    {
        m_measure_geometry_request.linegap = m_iSelectPointGap;
        MarkCircleMeasureGeometryDirty();
        m_measure_geometry_request.version = m_measure_geometry_version;
    }
    else
    {
        MarkCircleMeasureGeometryDirty();
    }
}
void FindCircle::setmethod(int imethod)
{
    m_iMethod = imethod;
}
void FindCircle::setthre(int ithre)
{
    m_iThreshold = ithre;
}
int FindCircle::thre()
{
    return m_iThreshold;
}
void FindCircle::setgamarate(int igama)
{
    m_igamarate = igama;
}

void FindCircle::setfindsetting(int ifindset)
{
    m_ifindset = ifindset;
}
void FindCircle::setfilter(int ifilterborw, int ifiltermin, int ifiltermax)
{
    m_ifilterborw = ifilterborw;
    m_ifiltermin = ifiltermin;
    m_ifiltermax = ifiltermax;
}
void FindCircle::MeasureT(void *pimage)
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
void FindCircle::Measure(Image& image)
{
    LogFindCircleMeasureProbe(
        "measure_image_enter",
        "running",
        FindCircleMeasureMessage(
            "FindCircle::Measure(Image&) enter",
            image.getmat().empty() ? 0 : image.getmat().cols,
            image.getmat().empty() ? 0 : image.getmat().rows,
            image.getmat().empty() ? 0 : image.getmat().channels(),
            m_icentx,
            m_icenty,
            m_ipax,
            m_ipay,
            ClampSizeToInt(m_lines.size()),
            m_lines.empty() ? 0 : m_lines[0].getlinesize(),
            g_pbackimage != nullptr ? g_pbackimage->getWidth() : 0,
            g_pbackimage != nullptr ? g_pbackimage->getHeight() : 0));

    m_measurepoints.clear();
    m_dresultcentx = 0.0;
    m_dresultcenty = 0.0;
    m_dradius = 0.0;
    m_avgdist = 0.0;
    m_last_prefilter_used = 0;

    m_lastMeasureGeometryDebug.image_ready =
        image.getmat().empty() ? false : true;

    m_lastMeasureGeometryDebug.image_width =
        image.getWidth();

    m_lastMeasureGeometryDebug.image_height =
        image.getHeight();

    m_lastMeasureGeometryDebug.image_channels =
        image.getmat().empty() ? 0 : image.getmat().channels();

    m_lastMeasureGeometryDebug.backimage_ready =
        (g_pbackimage != nullptr);

    m_lastMeasureGeometryDebug.findobject_ready =
        (g_pbackfindobject != nullptr);

    m_lastMeasureGeometryDebug.measure_source =
        "original_circle_measure_pipeline";

    if (image.getmat().empty())
    {
        LogFindCircleMeasureProbe(
            "measure_image_fail",
            "failed",
            "failure_stage=image_mat_empty");
        return;
    }

    const gp_Rectangle roi = rect();

    const double left = roi.TopLeft().X();
    const double top = roi.TopLeft().Y();
    const double right = left + roi.Width();
    const double bottom = top + roi.Height();

    if (left < 0.0 || top < 0.0)
    {
        LogFindCircleMeasureProbe(
            "measure_image_fail",
            "failed",
            FindCircleMeasureMessage(
                "failure_stage=circle_roi_negative",
                image.getWidth(),
                image.getHeight(),
                image.getmat().channels(),
                m_icentx,
                m_icenty,
                m_ipax,
                m_ipay,
                ClampSizeToInt(m_lines.size()),
                m_lines.empty() ? 0 : m_lines[0].getlinesize(),
                g_pbackimage != nullptr ? g_pbackimage->getWidth() : 0,
                g_pbackimage != nullptr ? g_pbackimage->getHeight() : 0));
        return;
    }

    if (right >= static_cast<double>(image.getWidth()) ||
        bottom >= static_cast<double>(image.getHeight()))
    {
        LogFindCircleMeasureProbe(
            "measure_image_fail",
            "failed",
            FindCircleMeasureMessage(
                "failure_stage=circle_roi_outside_image",
                image.getWidth(),
                image.getHeight(),
                image.getmat().channels(),
                m_icentx,
                m_icenty,
                m_ipax,
                m_ipay,
                ClampSizeToInt(m_lines.size()),
                m_lines.empty() ? 0 : m_lines[0].getlinesize(),
                g_pbackimage != nullptr ? g_pbackimage->getWidth() : 0,
                g_pbackimage != nullptr ? g_pbackimage->getHeight() : 0));
        return;
    }

    const int isize = ClampSizeToInt(m_lines.size());

    m_lastMeasureGeometryDebug.scan_line_count = isize;

    if (isize <= 0)
    {
        m_lastMeasureGeometryDebug.failure_stage =
            "circle_scan_lines_empty";

        m_lastMeasureGeometryDebug.detail =
            "FindCircle Measure has zero scan lines; check setcircle/setcircle2, Setgap and geometry cache.";

        LogFindCircleMeasureProbe(
            "measure_geometry_fail",
            "failed",
            "failure_stage=circle_scan_lines_empty");
        return;
    }

    if (g_pbackimage == nullptr || g_pbackimage == &image ||
        g_pbackimage->getmat().empty())
    {
        m_lastMeasureGeometryDebug.failure_stage =
            "circle_scan_workspace_unavailable";

        m_lastMeasureGeometryDebug.detail =
            "FindCircle Measure requires an independent ImageManager BackImage workspace.";

        LogFindCircleMeasureProbe(
            "measure_image_fail",
            "failed",
            FindCircleMeasureMessage(
                "failure_stage=circle_scan_workspace_unavailable",
                image.getWidth(),
                image.getHeight(),
                image.getmat().channels(),
                m_icentx,
                m_icenty,
                m_ipax,
                m_ipay,
                isize,
                m_lines.empty() ? 0 : m_lines[0].getlinesize(),
                g_pbackimage != nullptr ? g_pbackimage->getWidth() : 0,
                g_pbackimage != nullptr ? g_pbackimage->getHeight() : 0));
        return;
    }

    int ilineslen1 = 0;
    if (isize > 0)
        ilineslen1 = m_lines[0].getlinesize();

    int iprocessw = ilineslen1;

    m_lastMeasureGeometryDebug.scan_line_length =
        ilineslen1;

    LogFindCircleMeasureProbe(
        "measure_before_capacity_check",
        "running",
        FindCircleMeasureMessage(
            "before workspace capacity check",
            image.getWidth(),
            image.getHeight(),
            image.getmat().channels(),
            m_icentx,
            m_icenty,
            m_ipax,
            m_ipay,
            isize,
            iprocessw,
            g_pbackimage->getWidth(),
            g_pbackimage->getHeight()));

    m_lastMeasureGeometryDebug.process_width =
        iprocessw;

    if (iprocessw <= 0)
    {
        m_lastMeasureGeometryDebug.failure_stage =
            "circle_process_width_zero";

        m_lastMeasureGeometryDebug.detail =
            "FindCircle scan lines exist, but scan line length is zero.";

        LogFindCircleMeasureProbe(
            "measure_geometry_fail",
            "failed",
            "failure_stage=circle_process_width_zero");
        return;
    }

    if (isize + 5 > g_pbackimage->getHeight() ||
        iprocessw + 3 > g_pbackimage->getWidth())
    {
        m_lastMeasureGeometryDebug.failure_stage =
            "circle_scan_workspace_capacity_exceeded";

        m_lastMeasureGeometryDebug.detail =
            "FindCircle Measure skipped because scan geometry exceeds the BackImage workspace.";

        LogFindCircleMeasureProbe(
            "measure_capacity_fail",
            "failed",
            FindCircleMeasureMessage(
                "failure_stage=circle_scan_workspace_capacity_exceeded",
                image.getWidth(),
                image.getHeight(),
                image.getmat().channels(),
                m_icentx,
                m_icenty,
                m_ipax,
                m_ipay,
                isize,
                iprocessw,
                g_pbackimage->getWidth(),
                g_pbackimage->getHeight()));
        return;
    }

    const int stage_limit = ReadCircleMeasureStageLimit();

    LogFindCircleMeasureProbe(
        "measure_stage_limit",
        "running",
        "stage_limit=" + std::to_string(stage_limit));

    if (stage_limit == 1)
        return;

    if (stage_limit == 2)
        return;

    LogFindCircleMeasureProbe(
        "measure_before_linecopyex",
        "running",
        FindCircleMeasureMessage(
            "before linecopyex",
            image.getWidth(),
            image.getHeight(),
            image.getmat().channels(),
            m_icentx,
            m_icenty,
            m_ipax,
            m_ipay,
            isize,
            iprocessw,
            g_pbackimage->getWidth(),
            g_pbackimage->getHeight()));
    for (int i = 0; i < isize; i++)
    {
        m_lines[i].linecopyex(image, *g_pbackimage, 0, i);
    }
    LogFindCircleMeasureProbe(
        "measure_after_linecopyex",
        "running",
        "linecopyex complete");
    if (stage_limit == 3)
        return;

      LogFindCircleMeasureProbe(
          "measure_before_backimage_roi",
          "running",
          "setroi=(0,0," + std::to_string(iprocessw + 3) + "," +
              std::to_string(isize + 5) + ")");
      g_pbackimage->setroi(0, 0, iprocessw+3, isize+5);
      LogFindCircleMeasureProbe(
          "measure_before_blur_threshold",
          "running",
          "threshold=" + std::to_string(m_iThreshold) +
              ", gamma=" + std::to_string(m_igamarate) +
              ", linegap=" + std::to_string(m_iSelectPointGap) +
              ", method=" + std::to_string(m_iMethod));
      g_pbackimage->roi_7blur_gap_mud_thre_bw(m_iThreshold, m_igamarate, m_iSelectPointGap, m_iMethod);
      LogFindCircleMeasureProbe(
          "measure_after_blur_threshold",
          "running",
          "blur/threshold complete");
      if (stage_limit == 4)
          return;

      const bool compact_domain = isize <= 24 || ilineslen1 <= 24;
      if (!compact_domain && g_pbackfindobject != nullptr && ShouldApplyCircleObjectPrefilter(m_ifindset, iprocessw, isize))
      {
          LogFindCircleMeasureProbe(
              "measure_before_object_prefilter",
              "running",
              "ifindset=" + std::to_string(m_ifindset));
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
          LogFindCircleMeasureProbe(
              "measure_after_object_prefilter",
              "running",
              "object prefilter complete");
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

      LogFindCircleMeasureProbe(
          "measure_before_sampling_loop",
          "running",
          FindCircleMeasureMessage(
              "before sampling loop",
              image.getWidth(),
              image.getHeight(),
              image.getmat().channels(),
              m_icentx,
              m_icenty,
              m_ipax,
              m_ipay,
              isize,
              iprocessw,
              g_pbackimage->getWidth(),
              g_pbackimage->getHeight()));

      int irecordpoint[100];
      int irecordnum = 0;
      bool bcollectBegin = false;
      int idarkgapnum = 0;

    int icurlinenum = 0;
    int icurlineposition = 0;

      m_budget_state = CxAlgorithmBudgetState();

      int ifixvalue = ComputeCircleScanTrim(isize);
      int iscanlines = isize - ifixvalue;
      iscanlines = std::max(iscanlines, ComputeCircleMinScanLines(isize));
      if (iscanlines < 0)
          iscanlines = 0;
      const int left_margin = ComputeCircleEdgeMargin(ilineslen1, m_iSelectPointGap);
      const int right_margin = ComputeCircleEdgeMargin(ilineslen1, m_iSelectPointGap);
      const int max_edge_width = ComputeCircleMaxEdgeWidth(ilineslen1);

      const auto begin_time = std::chrono::steady_clock::now();
      int scan_lines_processed = 0;
      int total_samples = 0;
      int valid_points_count = 0;

      auto now = std::chrono::steady_clock::now();
      int elapsed_ms = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(now - begin_time).count());

      CxAlgorithmTraceScope::Emit({
          "FindCircle",
          "measure",
          "begin",
          "FindCircle measure begin",
          0,
          0,
          0,
          elapsed_ms
      });

      auto budgetExceeded = [&]() -> bool {
          auto now = std::chrono::steady_clock::now();
          auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - begin_time).count();

          m_budget_state.elapsed_ms = static_cast<int>(elapsed_ms);
          m_budget_state.scan_line_count = scan_lines_processed;
          m_budget_state.sample_count = total_samples;

          if (scan_lines_processed > m_budget.max_scan_lines) {
              m_budget_state.exceeded = true;
              m_budget_state.exceeded_kind = "scan_line_budget_exceeded";
              m_lastMeasureGeometryDebug.failure_stage = "scan_line_budget_exceeded";
              m_lastMeasureGeometryDebug.detail = "FindCircle Measure scan lines exceeded budget: " + std::to_string(scan_lines_processed) + " > " + std::to_string(m_budget.max_scan_lines);
              return true;
          }

          if (total_samples > m_budget.max_samples) {
              m_budget_state.exceeded = true;
              m_budget_state.exceeded_kind = "sample_budget_exceeded";
              m_lastMeasureGeometryDebug.failure_stage = "sample_budget_exceeded";
              m_lastMeasureGeometryDebug.detail = "FindCircle Measure samples exceeded budget: " + std::to_string(total_samples) + " > " + std::to_string(m_budget.max_samples);
              return true;
          }

          if (elapsed_ms > m_budget.max_elapsed_ms) {
              m_budget_state.exceeded = true;
              m_budget_state.exceeded_kind = "algorithm_budget_exceeded";
              m_lastMeasureGeometryDebug.failure_stage = "algorithm_budget_exceeded";
              m_lastMeasureGeometryDebug.detail = "FindCircle Measure time exceeded budget: " + std::to_string(elapsed_ms) + "ms > " + std::to_string(m_budget.max_elapsed_ms) + "ms";
              return true;
          }

          return false;
      };
 
    if (1) 
    {
        cv::Vec3b icolor = 0;
          for (int inumy = 0; inumy < iscanlines; inumy++)
          {
              ++scan_lines_processed;

              if (budgetExceeded()) {
                  auto elapsed_ms = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin_time).count());
                  CxAlgorithmTraceScope::Emit({
                      "FindCircle",
                      "measure",
                      "abort",
                      "budget exceeded: " + m_lastMeasureGeometryDebug.failure_stage,
                      scan_lines_processed,
                      total_samples,
                      valid_points_count,
                      elapsed_ms
                  });
                  m_measurepoints.clear();
                  return;
              }

              irecordnum = 0;
              icurlinenum = 0;
              bcollectBegin = false;
              idarkgapnum = 0;
              int best_line_position = -1;
             
            for (int inumx = 0; inumx < ilineslen1; inumx++)
            { 
                ++total_samples;

                if ((total_samples % 4096) == 0) {
                    if (budgetExceeded()) {
                        auto elapsed_ms = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin_time).count());
                        CxAlgorithmTraceScope::Emit({
                            "FindCircle",
                            "measure",
                            "abort",
                            "budget exceeded: " + m_lastMeasureGeometryDebug.failure_stage,
                            scan_lines_processed,
                            total_samples,
                            valid_points_count,
                            elapsed_ms
                        });
                        m_measurepoints.clear();
                        return;
                    }

                    auto elapsed_ms = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin_time).count());
                    std::cout << "[DEBUG MEASURE] progress elapsed_ms=" << elapsed_ms << ", valid=" << valid_points_count << "\n";
                    CxAlgorithmTraceScope::Emit({
                        "FindCircle",
                        "measure",
                        "progress",
                        "sampling circle edge",
                        scan_lines_processed,
                        total_samples,
                        valid_points_count,
                        elapsed_ms
                    });
                }

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
                                    valid_points_count = m_measurepoints.size();
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
                            valid_points_count = m_measurepoints.size();
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
                valid_points_count = m_measurepoints.size();
            }

        }

    }

    m_lastMeasureGeometryDebug.measure_points_count =
        m_measurepoints.size();

    m_lastMeasureGeometryDebug.valid_points_count =
        m_measurepoints.size();

    if (m_measurepoints.size() > 0)
    {
        m_lastMeasureGeometryDebug.failure_stage =
            "result_points_available";

        m_lastMeasureGeometryDebug.detail =
            "FindCircle original Measure produced result points.";
    }
    else
    {
        m_lastMeasureGeometryDebug.failure_stage =
            "circle_measure_no_result_points";

        m_lastMeasureGeometryDebug.detail =
            "FindCircle original Measure completed, but produced zero result points.";
    }

    m_lastMeasureGeometryDebug.scan_lines_processed = scan_lines_processed;
    m_lastMeasureGeometryDebug.total_samples = total_samples;
    auto end_time = std::chrono::steady_clock::now();
    m_lastMeasureGeometryDebug.elapsed_ms =
        static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - begin_time).count());

    m_lastMeasureGeometryDebug.budget_max_scan_lines = m_budget.max_scan_lines;
    m_lastMeasureGeometryDebug.budget_max_samples = m_budget.max_samples;
    m_lastMeasureGeometryDebug.budget_max_elapsed_ms = m_budget.max_elapsed_ms;

    CxAlgorithmTraceScope::Emit({
        "FindCircle",
        "measure",
        "end",
        "FindCircle measure end",
        scan_lines_processed,
        total_samples,
        static_cast<int>(m_measurepoints.size()),
        m_lastMeasureGeometryDebug.elapsed_ms
    });
    LogFindCircleMeasureProbe(
        "measure_image_exit",
        "finished",
        "scan_lines_processed=" + std::to_string(scan_lines_processed) +
            ", total_samples=" + std::to_string(total_samples) +
            ", measure_points=" + std::to_string(m_measurepoints.size()) +
            ", failure_stage=" + m_lastMeasureGeometryDebug.failure_stage);
}

void FindCircle::MeasureBalanced(Image& image)
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

PointsShape& FindCircle::getresultpoints()
{
    return m_measurepoints;
}

const PointsShape& FindCircle::getresultpoints() const
{
    return m_measurepoints;
}
 double distance(const gp_Pnt& a, const gp_Pnt& b)
{
    const double dx = a.X() - b.X();
    const double dy = a.Y() - b.Y();
    return std::sqrt(dx * dx + dy * dy);
}

void FindCircle::fitcircle()
{ 
   // for (const auto& apoint : m_measurepoints) {
   //     m_curgroundBias.push_back(apoint);
   //     m_groundBias.push_back(apoint);
   // }
    
    auto begin_time = std::chrono::steady_clock::now();
    int initial_points = m_measurepoints.size();

    CxAlgorithmTraceScope::Emit({
        "FindCircle",
        "fitcircle",
        "begin",
        "FindCircle fitcircle begin",
        0,
        0,
        initial_points,
        0
    });

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

        auto elapsed_ms = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin_time).count());
        CxAlgorithmTraceScope::Emit({
            "FindCircle",
            "fitcircle",
            "fail",
            "FindCircle fitcircle failed: insufficient points",
            0,
            0,
            initial_points,
            elapsed_ms
        });
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

        auto elapsed_ms = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin_time).count());
        CxAlgorithmTraceScope::Emit({
            "FindCircle",
            "fitcircle",
            "fail",
            "FindCircle fitcircle failed: degenerate or non-finite result",
            0,
            0,
            initial_points,
            elapsed_ms
        });
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

     auto elapsed_ms = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin_time).count());
     CxAlgorithmTraceScope::Emit({
         "FindCircle",
         "fitcircle",
         "end",
         "FindCircle fitcircle end radius=" + std::to_string(m_dradius),
         0,
         0,
         initial_points,
         elapsed_ms
     });
}

void FindCircle::fitcirclefiltered()
{
    m_fitfilter_input_count = m_measurepoints.size();
    m_fitfilter_kept_count = 0;
    m_fitfilter_rejected_count = 0;
    m_fitfilter_sigma = 0.0;
    m_fitfilter_threshold = 0.0;

    const int point_count = m_measurepoints.size();
    const int min_fit_points = ComputeCircleMinFitPoints(static_cast<int>(m_lines.size()));
    if (point_count < std::max(5, min_fit_points))
    {
        m_dresultcentx = 0.0;
        m_dresultcenty = 0.0;
        m_dradius = 0.0;
        m_avgdist = 0.0;
        return;
    }

    // The scan lines are stored in angular order.  Use a circular five-tap
    // Gaussian neighbourhood only to obtain a stable initial circle; the
    // final fit still uses the original, non-smoothed measurement points.
    static constexpr double gaussian[5] = {
        0.06136, 0.24477, 0.38774, 0.24477, 0.06136
    };
    std::vector<cv::Point2f> smoothed;
    smoothed.reserve(static_cast<std::size_t>(point_count));
    for (int i = 0; i < point_count; ++i)
    {
        double x = 0.0;
        double y = 0.0;
        for (int offset = -2; offset <= 2; ++offset)
        {
            const int neighbour = (i + offset + point_count) % point_count;
            const double weight = gaussian[offset + 2];
            x += weight * m_measurepoints.getx(neighbour);
            y += weight * m_measurepoints.gety(neighbour);
        }
        smoothed.emplace_back(static_cast<float>(x), static_cast<float>(y));
    }

    auto initial_points = smoothed;
    const auto [initial_center, initial_radius_value] = Image::CircleFit_(initial_points);
    const double initial_radius = static_cast<double>(initial_radius_value);
    if (!std::isfinite(initial_center.x) || !std::isfinite(initial_center.y) ||
        !std::isfinite(initial_radius) || initial_radius <= 0.0)
    {
        m_dresultcentx = 0.0;
        m_dresultcenty = 0.0;
        m_dradius = 0.0;
        m_avgdist = 0.0;
        return;
    }

    std::vector<double> signed_residuals;
    signed_residuals.reserve(static_cast<std::size_t>(point_count));
    double residual_mean = 0.0;
    for (int i = 0; i < point_count; ++i)
    {
        const double dx = m_measurepoints.getx(i) - static_cast<double>(initial_center.x);
        const double dy = m_measurepoints.gety(i) - static_cast<double>(initial_center.y);
        const double residual = std::sqrt(dx * dx + dy * dy) - initial_radius;
        signed_residuals.push_back(residual);
        residual_mean += residual;
    }
    residual_mean /= static_cast<double>(point_count);

    double variance = 0.0;
    for (double residual : signed_residuals)
    {
        const double centered = residual - residual_mean;
        variance += centered * centered;
    }
    variance /= static_cast<double>(point_count);
    m_fitfilter_sigma = std::sqrt(std::max(0.0, variance));
    m_fitfilter_threshold = 2.0 * m_fitfilter_sigma;

    PointsShape filtered_points;
    for (int i = 0; i < point_count; ++i)
    {
        if (std::abs(signed_residuals[static_cast<std::size_t>(i)] - residual_mean) <=
            m_fitfilter_threshold)
        {
            gp_Pnt point;
            point.SetX(m_measurepoints.getx(i));
            point.SetY(m_measurepoints.gety(i));
            filtered_points.addpoint(point);
        }
    }

    m_fitfilter_kept_count = filtered_points.size();
    m_fitfilter_rejected_count = point_count - m_fitfilter_kept_count;
    if (m_fitfilter_kept_count < min_fit_points)
    {
        m_dresultcentx = 0.0;
        m_dresultcenty = 0.0;
        m_dradius = 0.0;
        m_avgdist = 0.0;
        return;
    }

    m_measurepoints = filtered_points;
    fitcircle();
}

void FindCircle::FitResultMeasure(void* pimage)
{
    Image* pgetimage = static_cast<Image*>(pimage);

    if (pgetimage == nullptr)
    {
        return;
    }

    if (pgetimage->getmat().empty())
    {
        return;
    }

    if (!canfitresultmeasure())
    {
        return;
    }

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

    if (!pre_fit_valid)
        return;

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
void FindCircle::setfitmeasuregap(int igap)
{
    m_fitmeasuregap = std::max(1, igap);
}

double FindCircle::getresultcentx()
{
    return m_dresultcentx;
}
double FindCircle::getresultcenty()
{
    return m_dresultcenty;
}
double FindCircle::getradius()
{
    return m_dradius;
}

double FindCircle::getavgdist()
{
    return m_avgdist;
}

int FindCircle::getvalidpointcount()
{
    return static_cast<int>(m_measurepoints.size());
}

bool FindCircle::hasfitresult()
{
    return m_measurepoints.size() >= 3 &&
           m_dradius > 0.0 &&
           std::isfinite(m_dresultcentx) &&
           std::isfinite(m_dresultcenty) &&
           std::isfinite(m_dradius) &&
           std::isfinite(m_avgdist);
}

double FindCircle::getresultcentx() const { return m_dresultcentx; }
double FindCircle::getresultcenty() const { return m_dresultcenty; }
double FindCircle::getradius() const { return m_dradius; }
bool FindCircle::hasfitresult() const
{
    return m_measurepoints.size() >= 3 &&
           m_dradius > 0.0 &&
           std::isfinite(m_dresultcentx) &&
           std::isfinite(m_dresultcenty) &&
           std::isfinite(m_dradius) &&
           std::isfinite(m_avgdist);
}

bool FindCircle::canfitresultmeasure()
{
    return hasfitresult() && m_fitmeasuregap > 0;
}

GeomAdaptor_Curve FindCircle::GetCurve(gp_Pnt center_p, Standard_Real radius)
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
gp_Pnt FindCircle::FindClosestPointOnCurve(GeomAdaptor_Curve myCurve,gp_Pnt externalPoint)
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
void FindCircle::measure(void* pimage)
{
    LogFindCircleMeasureProbe(
        "measure_void_enter",
        "running",
        std::string("pimage_null=") + (pimage == nullptr ? "true" : "false"));

    Image* pgetimage = static_cast<Image*>(pimage);

    if (pgetimage == nullptr)
    {
        m_measurepoints.clear();
        m_dresultcentx = 0.0;
        m_dresultcenty = 0.0;
        m_dradius = 0.0;
        m_avgdist = 0.0;
        LogFindCircleMeasureProbe(
            "measure_void_fail",
            "failed",
            "failure_stage=image_pointer_null");
        return;
    }

    if (pgetimage->getmat().empty())
    {
        m_measurepoints.clear();
        m_dresultcentx = 0.0;
        m_dresultcenty = 0.0;
        m_dradius = 0.0;
        m_avgdist = 0.0;
        LogFindCircleMeasureProbe(
            "measure_void_fail",
            "failed",
            "failure_stage=image_mat_empty");
        return;
    }

    if (!ImageManager::EnsureAlgorithmRuntimeResources(
            pgetimage->getWidth(),
            pgetimage->getHeight()))
    {
        m_measurepoints.clear();
        m_dresultcentx = 0.0;
        m_dresultcenty = 0.0;
        m_dradius = 0.0;
        m_avgdist = 0.0;
        LogFindCircleMeasureProbe(
            "measure_void_fail",
            "failed",
            "failure_stage=circle_scan_workspace_unavailable, reason=EnsureAlgorithmRuntimeResources failed");
        return;
    }
    g_pbackimage = ImageManager::GetBackImage(1);
    g_pbackfindobject = ImageManager::Getbackfindobject(1);

    if (g_pbackimage == nullptr || g_pbackimage == pgetimage ||
        g_pbackimage->getmat().empty())
    {
        m_measurepoints.clear();
        m_dresultcentx = 0.0;
        m_dresultcenty = 0.0;
        m_dradius = 0.0;
        m_avgdist = 0.0;
        m_lastMeasureGeometryDebug.failure_stage =
            "circle_scan_workspace_unavailable";
        m_lastMeasureGeometryDebug.detail =
            "FindCircle.measure requires an independent ImageManager BackImage workspace.";
        LogFindCircleMeasureProbe(
            "measure_void_fail",
            "failed",
            "failure_stage=circle_scan_workspace_unavailable, backimage_null=" +
                std::string(g_pbackimage == nullptr ? "true" : "false") +
                ", backimage_alias_input=" +
                std::string(g_pbackimage == pgetimage ? "true" : "false"));
        return;
    }

    if (!EnsureCircleMeasureGeometryReady())
    {
        m_measurepoints.clear();
        m_dresultcentx = 0.0;
        m_dresultcenty = 0.0;
        m_dradius = 0.0;
        m_avgdist = 0.0;
        LogFindCircleMeasureProbe(
            "measure_void_fail",
            "failed",
            "failure_stage=circle_measure_geometry_not_ready");
        return;
    }

    LogFindCircleMeasureProbe(
        "measure_void_before_measure_image",
        "running",
        FindCircleMeasureMessage(
            "before Measure(Image&)",
            pgetimage->getWidth(),
            pgetimage->getHeight(),
            pgetimage->getmat().channels(),
            m_icentx,
            m_icenty,
            m_ipax,
            m_ipay,
            ClampSizeToInt(m_lines.size()),
            m_lines.empty() ? 0 : m_lines[0].getlinesize(),
            g_pbackimage->getWidth(),
            g_pbackimage->getHeight()));
    Measure(*pgetimage);
    LogFindCircleMeasureProbe(
        "measure_void_after_measure_image",
        "finished",
        "measure_points=" + std::to_string(m_measurepoints.size()) +
            ", failure_stage=" + m_lastMeasureGeometryDebug.failure_stage);
}
void FindCircle::automeasure(void* pimage)
{
    (void)pimage;
 
}
void FindCircle::shapesetroi(void* pshape)
{
    if (pshape == nullptr)
        return;
    Shape::shapesetroi(pshape);
}
void FindCircle::easycluster(int igapx, int igapy, int iclusternum)
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

void FindCircle::MarkCircleMeasureGeometryDirty()
{
    m_measure_geometry_dirty = true;
    m_measure_geometry_ready = false;
    ++m_measure_geometry_version;
}

void FindCircle::UpdateCircleMeasureGeometryRequest(bool hasInnerGap)
{
    m_measure_geometry_request.valid = true;

    m_measure_geometry_request.center_x = m_icentx;
    m_measure_geometry_request.center_y = m_icenty;
    m_measure_geometry_request.pass_x = m_ipax;
    m_measure_geometry_request.pass_y = m_ipay;

    m_measure_geometry_request.has_inner_gap = hasInnerGap;
    m_measure_geometry_request.inner_gap =
        hasInnerGap ? std::max(0, m_idisgap) : 0;

    m_measure_geometry_request.gap_degrees = m_igap;
    m_measure_geometry_request.linegap = m_iSelectPointGap;
    m_measure_geometry_request.sample_rate = m_dsamplerate;

    MarkCircleMeasureGeometryDirty();

    m_measure_geometry_request.version = m_measure_geometry_version;
}

bool FindCircle::EnsureCircleMeasureGeometryReady()
{
    if (!m_measure_geometry_request.valid)
    {
        m_lastMeasureGeometryDebug.request_valid = false;
        m_lastMeasureGeometryDebug.geometry_ready = false;
        m_lastMeasureGeometryDebug.failure_stage =
            "circle_measure_request_invalid";
        m_lastMeasureGeometryDebug.detail =
            "FindCircle measure request is invalid; call setcircle or setcircle2 before measure.";
        return false;
    }

    if (!m_measure_geometry_dirty &&
        m_measure_geometry_ready &&
        m_measure_geometry_built_version ==
            m_measure_geometry_request.version)
    {
        return true;
    }

    const bool ok =
        BuildCircleMeasureGeometryFromRequest(
            m_measure_geometry_request);

    m_measure_geometry_ready = ok;
    m_measure_geometry_dirty = !ok;

    if (ok)
    {
        m_measure_geometry_built_version =
            m_measure_geometry_request.version;
    }
    else
    {
        m_lastMeasureGeometryDebug.failure_stage =
            "circle_measure_geometry_build_failed";
        m_lastMeasureGeometryDebug.detail =
            "BuildCircleMeasureGeometryFromRequest failed.";
    }

    return ok;
}

bool FindCircle::BuildCircleMeasureGeometryFromRequest(
    const FindCircleMeasureGeometryRequest& request)
{
    if (!request.valid)
        return false;

    BuildCircleMeasureGeometryCore(request);

    const bool ok = !m_lines.empty();

    m_lastMeasureGeometryDebug.request_valid = request.valid;
    m_lastMeasureGeometryDebug.geometry_ready = ok;
    m_lastMeasureGeometryDebug.geometry_dirty = false;
    m_lastMeasureGeometryDebug.geometry_version =
        m_measure_geometry_version;
    m_lastMeasureGeometryDebug.geometry_built_version =
        request.version;

    m_lastMeasureGeometryDebug.center_x = request.center_x;
    m_lastMeasureGeometryDebug.center_y = request.center_y;
    m_lastMeasureGeometryDebug.pass_x = request.pass_x;
    m_lastMeasureGeometryDebug.pass_y = request.pass_y;
    m_lastMeasureGeometryDebug.inner_gap = request.inner_gap;
    m_lastMeasureGeometryDebug.gap_degrees = request.gap_degrees;
    m_lastMeasureGeometryDebug.linegap = request.linegap;
    m_lastMeasureGeometryDebug.scan_line_count =
        static_cast<int>(m_lines.size());

    if (!m_lines.empty())
    {
        m_lastMeasureGeometryDebug.scan_line_length =
            m_lines[0].getlinesize();
        m_lastMeasureGeometryDebug.process_width =
            m_lastMeasureGeometryDebug.scan_line_length;
    }

    if (!ok)
    {
        m_lastMeasureGeometryDebug.failure_stage =
            "circle_scan_lines_empty";
        m_lastMeasureGeometryDebug.detail =
            "FindCircle geometry build produced zero scan lines; check setcircle/setcircle2 request and gap.";
    }

    return ok;
}

void FindCircle::BuildCircleMeasureGeometryCore(
    const FindCircleMeasureGeometryRequest& request)
{
    Shape::clear();

    m_icentx = request.center_x;
    m_icenty = request.center_y;
    m_ipax = request.pass_x;
    m_ipay = request.pass_y;
    m_idisgap = request.inner_gap;

    if (request.has_inner_gap)
    {
        Shape::setcircle2(
            request.center_x,
            request.center_y,
            request.pass_x,
            request.pass_y,
            request.inner_gap);
    }
    else
    {
        Shape::setcircle(
            request.center_x,
            request.center_y,
            request.pass_x,
            request.pass_y);
    }

    for (std::size_t i = 0; i < m_lines.size(); ++i)
    {
        m_lines[i].clear();
    }

    m_lines.clear();

    const int isize =
        ClampSizeToInt(getpath().ElementCount());

    const cxgeom::CxSetCircleBuildMeta scan_meta =
        BuildCircleScanMeta(
            request.center_x,
            request.center_y,
            request.pass_x,
            request.pass_y,
            request.gap_degrees);

    if (request.gap_degrees <= 0 || isize <= 0)
        return;

    const int igapadd =
        ComputeCircleLineStep(
            isize,
            request.gap_degrees,
            scan_meta);

    if (request.has_inner_gap)
    {
        int iadd = 0;

        for (int i = 0; i < isize; )
        {
            LineShape ray;
            const gp_Pnt apoint = getpath().ElementAt(i);

            ray.setline(
                request.center_x,
                request.center_y,
                RoundToInt(apoint.X()),
                RoundToInt(apoint.Y()));

            std::vector<gp_Pnt> acrosspoints =
                ray.getpath().IntersectPaths(getpath2());

            if (!acrosspoints.empty())
            {
                LineShape scanLine;
                scanLine.setline(
                    RoundToInt(acrosspoints[0].X()),
                    RoundToInt(acrosspoints[0].Y()),
                    RoundToInt(apoint.X()),
                    RoundToInt(apoint.Y()));

                scanLine.setPercent(request.sample_rate);
                m_lines.push_back(scanLine);
            }

            ++iadd;
            i += igapadd;
        }
    }
    else
    {
        int iadd = 0;

        for (int i = 0; i < isize; )
        {
            LineShape scanLine;
            const gp_Pnt apoint = getpath().ElementAt(i);

            scanLine.setline(
                request.center_x,
                request.center_y,
                RoundToInt(apoint.X()),
                RoundToInt(apoint.Y()));

            scanLine.setPercent(request.sample_rate);
            m_lines.push_back(scanLine);

            ++iadd;
            i += igapadd;
        }
    }
}

void FindCircle::PublishDisplayShapes(ICxShapeSink& sink, const std::string& owner_ref) const
{
    const int cx = getcirclecentx();
    const int cy = getcirclecenty();
    const int px = getcirclepax();
    const int py = getcirclepay();

    const double roi_radius = std::hypot(
        static_cast<double>(px - cx),
        static_cast<double>(py - cy));

    auto roi_circle = std::make_unique<CircleShape>(
        static_cast<double>(cx),
        static_cast<double>(cy),
        roi_radius);
    sink.UpsertShape(
        owner_ref + ".roi_circle",
        "FindCircle",
        owner_ref,
        "setcircle",
        "roi",
        true,
        false,
        std::move(roi_circle));

    const int linegap = m_iSelectPointGap;
    const double outer_radius = roi_radius + static_cast<double>(linegap);
    auto outer_scan = std::make_unique<CircleShape>(
        static_cast<double>(cx),
        static_cast<double>(cy),
        outer_radius);
    sink.UpsertShape(
        owner_ref + ".outer_scan_circle",
        "FindCircle",
        owner_ref,
        "",
        "scan",
        false,
        false,
        std::move(outer_scan));

    const FindCircleMeasureGeometryDebug& debug = lastmeasuregeometrydebug();
    if (debug.has_inner_gap && debug.inner_gap > 0)
    {
        const double inner_radius = std::max(1.0, roi_radius - static_cast<double>(debug.inner_gap));
        auto inner_scan = std::make_unique<CircleShape>(
            static_cast<double>(cx),
            static_cast<double>(cy),
            inner_radius);
        sink.UpsertShape(
            owner_ref + ".inner_scan_circle",
            "FindCircle",
            owner_ref,
            "",
            "scan",
            false,
            false,
            std::move(inner_scan));
    }

    const PointsShape& measure_points = getresultpoints();
    if (measure_points.size() > 0)
    {
        auto pts_shape = std::make_unique<PointsShape>();
        for (int i = 0; i < measure_points.size(); ++i)
        {
            pts_shape->addpoint(measure_points.getx(i), measure_points.gety(i));
        }
        sink.UpsertShape(
            owner_ref + ".measure_points",
            "FindCircle",
            owner_ref,
            "",
            "measure_points",
            false,
            true,
            std::move(pts_shape));
    }

    if (hasfitresult())
    {
        auto fit_circle = std::make_unique<CircleShape>(
            getresultcentx(),
            getresultcenty(),
            getradius());
        sink.UpsertShape(
            owner_ref + ".fit_circle",
            "FindCircle",
            owner_ref,
            "",
            "result",
            false,
            true,
            std::move(fit_circle));
    }
}

void FindCircle::setmaxelapsedms(int value)
{
    m_budget.max_elapsed_ms = value;
}

void FindCircle::setmaxscanlines(int value)
{
    m_budget.max_scan_lines = value;
}

void FindCircle::setmaxsamples(int value)
{
    m_budget.max_samples = value;
}

bool FindCircle::budgetexceeded() const
{
    return m_budget_state.exceeded;
}

int FindCircle::getelapsedms() const
{
    return m_budget_state.elapsed_ms;
}

int FindCircle::getscanlinecount() const
{
    return m_budget_state.scan_line_count;
}

int FindCircle::getsamplecount() const
{
    return m_budget_state.sample_count;
}

const std::string& FindCircle::getfailurestage() const
{
    return m_lastMeasureGeometryDebug.failure_stage;
}
