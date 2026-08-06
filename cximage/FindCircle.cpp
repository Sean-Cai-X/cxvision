
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
    if ((ishow & 0x04) != 0)
    {
        for (std::size_t i = 0; i < m_lines.size(); ++i)
        {
            // A scan line represents only an opportunity.  Detected points
            // and fitted geometry are published as separate result evidence.
            m_lines[i].setcolor(140, 230, 255);
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

void FindCircle::setannulus(
    int icentx,
    int icenty,
    int iinnerRadius,
    int iouterRadius)
{
    const int outerRadius = std::max(1, iouterRadius);
    const int innerRadius = std::max(0, std::min(iinnerRadius, outerRadius - 1));

    if (innerRadius > 0)
    {
        setcircle2(
            icentx,
            icenty,
            icentx + outerRadius,
            icenty,
            outerRadius - innerRadius);
        return;
    }

    setcircle(icentx, icenty, icentx + outerRadius, icenty);
}

void FindCircle::setscanarc(int startDegrees, int endDegrees, int enabled)
{
    const auto normalize = [](int degrees) {
        int result = degrees % 360;
        return result < 0 ? result + 360 : result;
    };

    // A range spanning a whole revolution means no angular crop.  Evaluate
    // this from the caller values before modulo normalization; otherwise
    // 0..360 becomes 0..0 and the legacy one-degree fallback loses the
    // intended full-circle scan.
    const bool full_turn = std::abs(endDegrees - startDegrees) >= 360;
    if (enabled == 0 || full_turn)
    {
        m_scan_arc_start_degrees = 0;
        m_scan_arc_end_degrees = 360;
        m_has_scan_arc_window = false;
    }
    else
    {
        m_scan_arc_start_degrees = normalize(startDegrees);
        m_scan_arc_end_degrees = normalize(endDegrees);
        if (m_scan_arc_start_degrees == m_scan_arc_end_degrees)
            m_scan_arc_end_degrees = normalize(m_scan_arc_start_degrees + 1);
        m_has_scan_arc_window = true;
    }

    if (m_measure_geometry_request.valid)
    {
        m_measure_geometry_request.arc_start_degrees = m_scan_arc_start_degrees;
        m_measure_geometry_request.arc_end_degrees = m_scan_arc_end_degrees;
        m_measure_geometry_request.has_arc_window = m_has_scan_arc_window;
        MarkCircleMeasureGeometryDirty();
        m_measure_geometry_request.version = m_measure_geometry_version;
        BuildCircleMeasureGeometryFromRequest(m_measure_geometry_request);
        m_measure_geometry_ready = !m_lines.empty();
        m_measure_geometry_dirty = !m_measure_geometry_ready;
        if (m_measure_geometry_ready)
            m_measure_geometry_built_version = m_measure_geometry_request.version;
    }
}

void FindCircle::cxscript_setcircle(int ipay, int ipax, int icenty, int icentx)
{
    setcircle(icentx, icenty, ipax, ipay);
}

void FindCircle::cxscript_setcircle2(int idis, int ipay, int ipax, int icenty, int icentx)
{
    setcircle2(icentx, icenty, ipax, ipay, idis);
}

void FindCircle::cxscript_setannulus(int iouterRadius, int iinnerRadius, int icenty, int icentx)
{
    setannulus(icentx, icenty, iinnerRadius, iouterRadius);
}

void FindCircle::cxscript_setscanarc(int enabled, int endDegrees, int startDegrees)
{
    setscanarc(startDegrees, endDegrees, enabled);
}

int FindCircle::getscanarcstart() { return m_scan_arc_start_degrees; }
int FindCircle::getscanarcend() { return m_scan_arc_end_degrees; }
int FindCircle::hasscanarc() { return m_has_scan_arc_window ? 1 : 0; }

int FindCircle::getannulusouter()
{
    if (!m_measure_geometry_request.valid)
        return 0;

    const double dx = static_cast<double>(
        m_measure_geometry_request.pass_x - m_measure_geometry_request.center_x);
    const double dy = static_cast<double>(
        m_measure_geometry_request.pass_y - m_measure_geometry_request.center_y);
    return std::max(0, RoundToInt(std::sqrt(dx * dx + dy * dy)));
}

int FindCircle::getannulusinner()
{
    if (!m_measure_geometry_request.valid ||
        !m_measure_geometry_request.has_inner_gap)
        return 0;

    return std::max(
        0,
        getannulusouter() - std::max(0, m_measure_geometry_request.inner_gap));
}

int FindCircle::getannuluswidth()
{
    return std::max(0, getannulusouter() - getannulusinner());
}

int FindCircle::hasannulus()
{
    return m_measure_geometry_request.valid &&
           m_measure_geometry_request.has_inner_gap &&
           m_measure_geometry_request.inner_gap > 0 &&
           getannulusouter() > getannulusinner()
        ? 1
        : 0;
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
    m_last_measure_input_request = m_measure_geometry_request;

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

    /*
     * The circle scan image is a rectangular workspace representation of a
     * closed angular domain.  Clear the full workspace ROI before writing the
     * current scan rows; otherwise stale rows from a previous run can leak into
     * the tail padding used by blur/gap/FindObject.
     */
    {
        cv::Mat& backMat = g_pbackimage->getmat();
        if (!backMat.empty())
        {
            const int clearWidth =
                std::min(iprocessw + 3, backMat.cols);
            const int clearHeight =
                std::min(isize + 5, backMat.rows);
            if (clearWidth > 0 && clearHeight > 0)
            {
                backMat(cv::Rect(0, 0, clearWidth, clearHeight))
                    .setTo(cv::Scalar(0, 0, 0));
            }
        }
    }

    for (int i = 0; i < isize; i++)
    {
        m_lines[i].linecopyex(image, *g_pbackimage, 0, i);
    }

    /*
     * Closed-circle padding for the BackImage / FindObject path.
     *
     * linecopyex writes rows [0, isize).  The subsequent ROI is [0, isize+5),
     * and Image::roi_7blur_gap_mud_thre_bw treats that ROI as an ordinary
     * rectangle.  Leaving rows [isize, isize+5) empty turns the circular seam
     * into a hard image border, so the last angular side can lose candidates.
     * Copy the first scan rows into the tail padding to preserve angular
     * continuity for blur/gap thresholding and optional FindObject filtering.
     */
    {
        cv::Mat& backMat = g_pbackimage->getmat();
        const int wrapRows = std::min(5, isize);
        const int copyWidth =
            std::min(iprocessw, backMat.empty() ? 0 : backMat.cols);
        if (!backMat.empty() && copyWidth > 0)
        {
            for (int pad = 0; pad < wrapRows; ++pad)
            {
                const int dstRow = isize + pad;
                const int srcRow = pad;
                if (srcRow < 0 || srcRow >= backMat.rows ||
                    dstRow < 0 || dstRow >= backMat.rows)
                {
                    continue;
                }
                backMat(cv::Rect(0, srcRow, copyWidth, 1))
                    .copyTo(backMat(cv::Rect(0, dstRow, copyWidth, 1)));
            }
        }
    }

    LogFindCircleMeasureProbe(
        "measure_after_linecopyex",
        "running",
        "linecopyex complete; circular tail padding rows=" +
            std::to_string(std::min(5, isize)));
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

      m_budget_state = CxAlgorithmBudgetState();

      /*
       * Circle gauge is a closed sampling domain.  The old fixed trim removed
       * the tail scan columns (for example 57 scan lines -> 54 processed),
       * which makes the visible start/end arc lose result points even when the
       * geometry itself is valid.  Process every generated scan line here; if a
       * future geometry builder emits a real duplicate close point, that should
       * be detected by geometry identity instead of a hard-coded tail cut.
       */
      int iscanlines = isize;
      if (iscanlines < 0)
          iscanlines = 0;
      const int left_margin = ComputeCircleEdgeMargin(ilineslen1, m_iSelectPointGap);
      const int right_margin = ComputeCircleEdgeMargin(ilineslen1, m_iSelectPointGap);
      const int endpoint_guard = std::max(
          right_margin,
          std::min(std::max(6, m_iSelectPointGap * 2),
                   std::max(1, ilineslen1 / 12)));
      const int max_edge_width = ComputeCircleMaxEdgeWidth(ilineslen1);

      const auto begin_time = std::chrono::steady_clock::now();
      int scan_lines_processed = 0;
      int total_samples = 0;
      int valid_points_count = 0;
      int candidate_runs_total = 0;
      int candidate_runs_max_per_line = 0;
      int selected_edge_hits = 0;
      int selected_edge_misses = 0;
      double selected_edge_radius_sum = 0.0;
      double selected_edge_radius_min = std::numeric_limits<double>::infinity();
      double selected_edge_radius_max = 0.0;
      std::vector<int> accepted_line_positions(
          static_cast<std::size_t>(std::max(0, iscanlines)),
          -1);

      auto record_selected_radius = [&](const gp_Pnt& point) {
          const double dx = point.X() - static_cast<double>(m_icentx);
          const double dy = point.Y() - static_cast<double>(m_icenty);
          const double radius = std::sqrt(dx * dx + dy * dy);
          if (!std::isfinite(radius))
              return;
          selected_edge_radius_sum += radius;
          selected_edge_radius_min = std::min(selected_edge_radius_min, radius);
          selected_edge_radius_max = std::max(selected_edge_radius_max, radius);
      };

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
              bcollectBegin = false;
              idarkgapnum = 0;

              struct CircleLineCandidate
              {
                  int ordinal = 0;
                  int position = -1;
                  int score = std::numeric_limits<int>::max();
                  double radius_from_center = 0.0;
              };
              std::vector<CircleLineCandidate> line_candidates;

              auto score_candidate = [&](int candidate_position) -> int {
                  /*
                   * Default "any edge" used to mean "take max line position",
                   * so the outermost/endpoint-adjacent response always won.
                   * That is unsafe on circular gauges: cap/image border texture
                   * can become a false edge.  Keep the old outer-side tendency,
                   * but prefer the outer band just inside the scan endpoint.
                   */
                  const int target_outer_position =
                      std::max(left_margin, ilineslen1 - 1 - endpoint_guard);
                  const int endpoint_distance =
                      (ilineslen1 - 1) - candidate_position;
                  int score = std::abs(candidate_position - target_outer_position);
                  if (endpoint_distance < endpoint_guard)
                  {
                      score += (endpoint_guard - endpoint_distance + 1) * 1000;
                  }
                  if (compact_domain)
                  {
                      score = candidate_position;
                  }
                  return score;
              };

              auto append_line_candidate = [&](int record_count) {
                  if (record_count <= 0 || record_count > max_edge_width)
                  {
                      return;
                  }

                  int candidate_position =
                      ComputeCircleEdgeOffset(record_count, m_ineedfixs) +
                      irecordpoint[(record_count >> 1)];
                  candidate_position =
                      ClampCircleEdgePosition(candidate_position, ilineslen1, right_margin);

                  if (candidate_position < left_margin ||
                      candidate_position > (ilineslen1 - 1 - right_margin))
                  {
                      return;
                  }

                  CircleLineCandidate candidate;
                  candidate.ordinal =
                      static_cast<int>(line_candidates.size()) + 1;
                  candidate.position = candidate_position;
                  candidate.score = score_candidate(candidate_position);
                  const gp_Pnt candidate_point =
                      m_lines[inumy].getlinepoint(candidate_position);
                  const double dx =
                      candidate_point.X() - static_cast<double>(m_icentx);
                  const double dy =
                      candidate_point.Y() - static_cast<double>(m_icenty);
                  candidate.radius_from_center = std::sqrt(dx * dx + dy * dy);
                  line_candidates.push_back(candidate);
              };

              auto flush_line_candidate = [&]() {
                  if (bcollectBegin)
                  {
                      append_line_candidate(irecordnum);
                  }
                  irecordnum = 0;
                  bcollectBegin = false;
                  idarkgapnum = 0;
              };

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

                /*
                 * Image::roi_7blur_gap_mud_thre_bw writes threshold output to
                 * y + 1.  The source scan line at row N therefore produces its
                 * binary edge response at row N+1.  Reading row N loses the
                 * last scan line response entirely (it lands at row isize),
                 * which is visible as the missing final circle gauge side.
                 */
                const int sample_y = std::min(
                    inumy + 1,
                    g_pbackimage->getHeight() - 1);
                icolor = g_pbackimage->pixel(inumx, sample_y);
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
                        append_line_candidate(irecordnum);
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
                flush_line_candidate();
              }

            candidate_runs_total += static_cast<int>(line_candidates.size());
            candidate_runs_max_per_line =
                std::max(candidate_runs_max_per_line,
                         static_cast<int>(line_candidates.size()));

            /*
             * Edge selection must be independent from the internal scan-line
             * direction.  A circle gauge line may be built from Rout->Rin or
             * Rin->Rout depending on geometry/arc construction.  Sort by
             * actual radial distance so Edge 1/2/3 means the 1st/2nd/3rd
             * candidate from the center outward, matching FindLine's stable
             * "candidate column" semantics.
             */
            std::sort(
                line_candidates.begin(),
                line_candidates.end(),
                [](const CircleLineCandidate& lhs,
                   const CircleLineCandidate& rhs) {
                    if (lhs.radius_from_center != rhs.radius_from_center)
                        return lhs.radius_from_center < rhs.radius_from_center;
                    return lhs.position < rhs.position;
                });
            for (std::size_t i = 0; i < line_candidates.size(); ++i)
            {
                line_candidates[i].ordinal = static_cast<int>(i) + 1;
            }

            if (m_iselectedgenum == 0)
            {
                /*
                 * All edges must mirror FindLine's "All edges" semantics:
                 * every eligible boundary crossing on each radial Gauge line
                 * is an accepted/displayed result point.  The previous code
                 * picked a single best-scored candidate per radial line, which
                 * made FindCircle look like a selected-edge run even when the
                 * UI was set to All edges.
                 */
                if (line_candidates.empty())
                {
                    ++selected_edge_misses;
                }
                else
                {
                    int representative_position = -1;
                    for (const CircleLineCandidate& candidate : line_candidates)
                    {
                        gp_Pnt apoint =
                            m_lines[inumy].getlinepoint(candidate.position);
                        m_measurepoints.addpoint(apoint);
                        record_selected_radius(apoint);
                        ++selected_edge_hits;
                        if (representative_position < 0)
                            representative_position = candidate.position;
                    }
                    if (inumy >= 0 && inumy < iscanlines)
                    {
                        accepted_line_positions[
                            static_cast<std::size_t>(inumy)] =
                            representative_position;
                    }
                    valid_points_count = m_measurepoints.size();
                }
                continue;
            }

            int selected_line_position = -1;
            if (m_iselectedgenum == -1)
            {
                // -1 is the true terminal boundary on this radial Gauge
                // line. Candidate counts may differ from one angle to the
                // next, so it cannot be represented by a fixed Edge N.
                if (!line_candidates.empty())
                {
                    selected_line_position = line_candidates.back().position;
                    ++selected_edge_hits;
                }
                else
                {
                    ++selected_edge_misses;
                }
            }
            else if (m_iselectedgenum > 0)
            {
                if (m_iselectedgenum <= static_cast<int>(line_candidates.size()))
                {
                    selected_line_position =
                        line_candidates[static_cast<std::size_t>(m_iselectedgenum - 1)].position;
                    ++selected_edge_hits;
                }
                else
                {
                    ++selected_edge_misses;
                }
            }
            if (selected_line_position >= 0)
            {
                gp_Pnt apoint = m_lines[inumy].getlinepoint(selected_line_position);
                m_measurepoints.addpoint(apoint);
                record_selected_radius(apoint);
                if (inumy >= 0 && inumy < iscanlines)
                    accepted_line_positions[static_cast<std::size_t>(inumy)] = selected_line_position;
                valid_points_count = m_measurepoints.size();
            }

        }

        /*
         * Closed-circle seam repair.
         *
         * The generated scan lines are circular, but this loop is linear.
         * Scan line 0 and the last scan line are neighbors in angle; treating
         * them as unrelated endpoints leaves visible missing points exactly at
         * the gauge start/end seam.  The visual seam can cover a few generated
         * scan lines, not only the exact first/last line, so repair only a
         * small circular seam window using the nearest accepted neighboring
         * radial position.  Middle scan lines are not synthesized here, so
         * normal rejection semantics stay intact.
        */
        if (!compact_domain &&
            iscanlines >= 4 &&
            static_cast<int>(accepted_line_positions.size()) == iscanlines)
        {
            int seam_repaired = 0;
            auto wrapIndex = [&](int index) -> int {
                int wrapped = index % iscanlines;
                if (wrapped < 0)
                    wrapped += iscanlines;
                return wrapped;
            };
            auto acceptedAt = [&](int index) -> int {
                return accepted_line_positions[
                    static_cast<std::size_t>(wrapIndex(index))];
            };
            auto repairSeamScan = [&](int scan_index) {
                if (scan_index < 0 || scan_index >= iscanlines)
                    return;
                if (accepted_line_positions[static_cast<std::size_t>(scan_index)] >= 0)
                    return;

                int sum = 0;
                int count = 0;
                const int max_search = std::min(8, std::max(2, iscanlines / 12));
                for (int offset = 1; offset <= max_search && count < 2; ++offset)
                {
                    const int p = acceptedAt(scan_index - offset);
                    if (p >= 0)
                    {
                        sum += p;
                        ++count;
                        break;
                    }
                }
                for (int offset = 1; offset <= max_search && count < 2; ++offset)
                {
                    const int p = acceptedAt(scan_index + offset);
                    if (p >= 0)
                    {
                        sum += p;
                        ++count;
                        break;
                    }
                }
                if (count <= 0)
                    return;

                const int repaired_position =
                    ClampCircleEdgePosition(sum / count, ilineslen1, right_margin);
                gp_Pnt apoint = m_lines[static_cast<std::size_t>(scan_index)].getlinepoint(repaired_position);
                m_measurepoints.addpoint(apoint);
                record_selected_radius(apoint);
                accepted_line_positions[static_cast<std::size_t>(scan_index)] = repaired_position;
                if (m_iselectedgenum != 0)
                {
                    ++selected_edge_hits;
                    if (selected_edge_misses > 0)
                        --selected_edge_misses;
                }
                ++seam_repaired;
            };

            const int seam_window =
                std::min(4, std::max(1, iscanlines / 32));
            for (int offset = 0; offset < seam_window; ++offset)
            {
                repairSeamScan(offset);
                repairSeamScan(iscanlines - 1 - offset);
            }

            if (seam_repaired > 0)
            {
                valid_points_count = m_measurepoints.size();
                CxAlgorithmTraceScope::Emit({
                    "FindCircle",
                    "measure",
                    "seam_repair",
                    "closed circle seam repaired points=" + std::to_string(seam_repaired),
                    scan_lines_processed,
                    total_samples,
                    valid_points_count,
                    static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - begin_time).count())
                });
            }
        }

    }

    m_lastMeasureGeometryDebug.measure_points_count =
        m_measurepoints.size();

    m_lastMeasureGeometryDebug.valid_points_count =
        m_measurepoints.size();

    m_lastMeasureGeometryDebug.selected_edge_index = m_iselectedgenum;
    m_lastMeasureGeometryDebug.candidate_runs_total = candidate_runs_total;
    m_lastMeasureGeometryDebug.candidate_runs_max_per_line =
        candidate_runs_max_per_line;
    m_lastMeasureGeometryDebug.selected_edge_hits = selected_edge_hits;
    m_lastMeasureGeometryDebug.selected_edge_misses = selected_edge_misses;
    if (m_measurepoints.size() > 0 &&
        std::isfinite(selected_edge_radius_min))
    {
        m_lastMeasureGeometryDebug.selected_edge_radius_avg =
            selected_edge_radius_sum /
            static_cast<double>(std::max(1, m_measurepoints.size()));
        m_lastMeasureGeometryDebug.selected_edge_radius_min =
            selected_edge_radius_min;
        m_lastMeasureGeometryDebug.selected_edge_radius_max =
            selected_edge_radius_max;
    }
    else
    {
        m_lastMeasureGeometryDebug.selected_edge_radius_avg = 0.0;
        m_lastMeasureGeometryDebug.selected_edge_radius_min = 0.0;
        m_lastMeasureGeometryDebug.selected_edge_radius_max = 0.0;
    }

    if (m_measurepoints.size() > 0)
    {
        m_lastMeasureGeometryDebug.failure_stage =
            "result_points_available";

        m_lastMeasureGeometryDebug.detail =
            "FindCircle original Measure produced result points; selected_edge=" +
            std::to_string(m_iselectedgenum) +
            ", candidate_runs_total=" + std::to_string(candidate_runs_total) +
            ", candidate_runs_max_per_line=" +
            std::to_string(candidate_runs_max_per_line) +
            ", selected_edge_hits=" + std::to_string(selected_edge_hits) +
            ", selected_edge_misses=" + std::to_string(selected_edge_misses) +
            ", selected_edge_radius_avg=" +
            std::to_string(m_lastMeasureGeometryDebug.selected_edge_radius_avg) +
            ", selected_edge_radius_min=" +
            std::to_string(m_lastMeasureGeometryDebug.selected_edge_radius_min) +
            ", selected_edge_radius_max=" +
            std::to_string(m_lastMeasureGeometryDebug.selected_edge_radius_max);
    }
    else
    {
        m_lastMeasureGeometryDebug.failure_stage =
            "circle_measure_no_result_points";

        m_lastMeasureGeometryDebug.detail =
            "FindCircle original Measure completed, but produced zero result points; selected_edge=" +
            std::to_string(m_iselectedgenum) +
            ", candidate_runs_total=" + std::to_string(candidate_runs_total) +
            ", candidate_runs_max_per_line=" +
            std::to_string(candidate_runs_max_per_line) +
            ", selected_edge_hits=" + std::to_string(selected_edge_hits) +
            ", selected_edge_misses=" + std::to_string(selected_edge_misses) +
            ", selected_edge_radius_avg=" +
            std::to_string(m_lastMeasureGeometryDebug.selected_edge_radius_avg) +
            ", selected_edge_radius_min=" +
            std::to_string(m_lastMeasureGeometryDebug.selected_edge_radius_min) +
            ", selected_edge_radius_max=" +
            std::to_string(m_lastMeasureGeometryDebug.selected_edge_radius_max);
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
            ", selected_edge=" + std::to_string(m_iselectedgenum) +
            ", candidate_runs_total=" + std::to_string(candidate_runs_total) +
            ", candidate_runs_max_per_line=" + std::to_string(candidate_runs_max_per_line) +
            ", selected_edge_hits=" + std::to_string(selected_edge_hits) +
            ", selected_edge_misses=" + std::to_string(selected_edge_misses) +
            ", selected_edge_radius_avg=" +
            std::to_string(m_lastMeasureGeometryDebug.selected_edge_radius_avg) +
            ", selected_edge_radius_min=" +
            std::to_string(m_lastMeasureGeometryDebug.selected_edge_radius_min) +
            ", selected_edge_radius_max=" +
            std::to_string(m_lastMeasureGeometryDebug.selected_edge_radius_max) +
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
    const bool saved_has_inner_gap =
        m_measure_geometry_request.valid &&
        m_measure_geometry_request.has_inner_gap &&
        m_measure_geometry_request.inner_gap > 0;
    const int saved_inner_gap = saved_has_inner_gap
        ? std::max(1, m_measure_geometry_request.inner_gap)
        : 0;
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
        // Candidate fitting may temporarily use an ordinary circle, but the
        // original annulus is the UI/runtime geometry contract.  Restore it
        // before PublishDisplayShapes() so Rin/Rout remain available.
        if (saved_has_inner_gap)
            setcircle2(saved_centx, saved_centy, saved_pax, saved_pay, saved_inner_gap);
        else
            setcircle(saved_centx, saved_centy, saved_pax, saved_pay);
        // The candidate loop temporarily uses ordinary-circle geometry.  The
        // request visible to the display/evidence layer must be the restored
        // request, not the last candidate's snapshot.
        m_last_measure_input_request = m_measure_geometry_request;
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
    if (saved_has_inner_gap)
        setcircle2(saved_centx, saved_centy, saved_pax, saved_pay, saved_inner_gap);
    else
        setcircle(saved_centx, saved_centy, saved_pax, saved_pay);
    // Keep the native scan geometry and the published request in lockstep
    // after candidate evaluation.  Otherwise PublishDisplayShapes() can
    // describe an annulus while getscanline() still exposes the last ordinary
    // circle candidate.
    m_last_measure_input_request = m_measure_geometry_request;
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
void FindCircle::measureRobust(void* pimage)
{
    if (pimage == nullptr)
    {
        m_measurepoints.clear();
        m_dresultcentx = 0.0;
        m_dresultcenty = 0.0;
        m_dradius = 0.0;
        m_avgdist = 0.0;
        m_lastMeasureGeometryDebug.failure_stage = "circle_input_image_null";
        return;
    }

    Image* pgetimage = static_cast<Image*>(pimage);

    if (pgetimage->getmat().empty())
    {
        m_measurepoints.clear();
        m_dresultcentx = 0.0;
        m_dresultcenty = 0.0;
        m_dradius = 0.0;
        m_avgdist = 0.0;
        m_lastMeasureGeometryDebug.failure_stage = "circle_input_image_empty";
        return;
    }

    if (!ImageManager::EnsureAlgorithmRuntimeResources(
            pgetimage->getWidth(),
            pgetimage->getHeight()))
    {
        m_measurepoints.clear();
        m_lastMeasureGeometryDebug.failure_stage = "circle_scan_workspace_unavailable";
        return;
    }

    g_pbackimage = ImageManager::GetBackImage(1);
    g_pbackfindobject = ImageManager::Getbackfindobject(1);

    if (g_pbackimage == nullptr || g_pbackimage == pgetimage ||
        g_pbackimage->getmat().empty())
    {
        m_measurepoints.clear();
        m_lastMeasureGeometryDebug.failure_stage = "circle_scan_workspace_unavailable";
        return;
    }

    if (!EnsureCircleMeasureGeometryReady())
    {
        m_measurepoints.clear();
        m_lastMeasureGeometryDebug.failure_stage = "circle_measure_geometry_not_ready";
        return;
    }

    MeasureRobust(*pgetimage);
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
    m_measure_geometry_request.arc_start_degrees = m_scan_arc_start_degrees;
    m_measure_geometry_request.arc_end_degrees = m_scan_arc_end_degrees;
    m_measure_geometry_request.has_arc_window = m_has_scan_arc_window;
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
    m_lastMeasureGeometryDebug.has_inner_gap = request.has_inner_gap;
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

    const auto normalize_angle = [](double degrees) {
        double result = std::fmod(degrees, 360.0);
        return result < 0.0 ? result + 360.0 : result;
    };
    const auto is_in_scan_arc = [&request, &normalize_angle](double degrees) {
        if (!request.has_arc_window)
            return true;
        const double angle = normalize_angle(degrees);
        const double start = normalize_angle(request.arc_start_degrees);
        const double end = normalize_angle(request.arc_end_degrees);
        return start <= end ? (angle >= start && angle <= end)
                            : (angle >= start || angle <= end);
    };

    if (request.has_inner_gap)
    {
        int iadd = 0;

        // An annulus scan is defined by the two radii, not by a best-effort
        // path intersection.  Construct both endpoints from the same radial
        // direction so every native scan line starts on Rin and ends on Rout.
        // This also keeps the first/last angular samples valid and avoids an
        // OCC intersection returning the centre path or no point at the seam.
        const double dx = static_cast<double>(request.pass_x - request.center_x);
        const double dy = static_cast<double>(request.pass_y - request.center_y);
        const double outer_radius = std::sqrt(dx * dx + dy * dy);
        const double inner_radius =
            outer_radius - static_cast<double>(request.inner_gap);

        for (int i = 0; i < isize; )
        {
            const gp_Pnt apoint = getpath().ElementAt(i);
            const double ux = apoint.X() - static_cast<double>(request.center_x);
            const double uy = apoint.Y() - static_cast<double>(request.center_y);
            const double direction_length = std::sqrt(ux * ux + uy * uy);

            if (direction_length > 1e-6 &&
                is_in_scan_arc(std::atan2(uy, ux) * 180.0 / CV_PI) &&
                outer_radius > 1.0 &&
                inner_radius > 0.0 &&
                inner_radius < outer_radius)
            {
                LineShape scanLine;
                const double nx = ux / direction_length;
                const double ny = uy / direction_length;
                scanLine.setline(
                    RoundToInt(request.center_x + nx * inner_radius),
                    RoundToInt(request.center_y + ny * inner_radius),
                    RoundToInt(request.center_x + nx * outer_radius),
                    RoundToInt(request.center_y + ny * outer_radius));

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

            const double ux = apoint.X() - static_cast<double>(request.center_x);
            const double uy = apoint.Y() - static_cast<double>(request.center_y);
            if (!is_in_scan_arc(std::atan2(uy, ux) * 180.0 / CV_PI))
            {
                ++iadd;
                i += igapadd;
                continue;
            }

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
    const FindCircleMeasureGeometryRequest& publish_request =
        (m_measure_geometry_request.valid &&
         m_measure_geometry_request.has_inner_gap &&
         m_measure_geometry_request.inner_gap > 0)
            ? m_measure_geometry_request
            : (m_last_measure_input_request.valid
                ? m_last_measure_input_request
                : m_measure_geometry_request);
    const int cx = publish_request.valid
        ? publish_request.center_x
        : getcirclecentx();
    const int cy = publish_request.valid
        ? publish_request.center_y
        : getcirclecenty();
    const int px = publish_request.valid
        ? publish_request.pass_x
        : getcirclepax();
    const int py = publish_request.valid
        ? publish_request.pass_y
        : getcirclepay();

    const double roi_radius = std::hypot(
        static_cast<double>(px - cx),
        static_cast<double>(py - cy));

    const int linegap = m_iSelectPointGap;
    const bool has_annulus_scan =
        publish_request.valid &&
        publish_request.has_inner_gap &&
        publish_request.inner_gap > 0 &&
        roi_radius > static_cast<double>(publish_request.inner_gap);

    /*
     * Display semantics must match the sampling semantics:
     *
     * - setcircle(): scan domain is the ordinary narrow gauge band around the
     *   ROI circle, so display roi +/- linegap.
     * - setcircle2(): the ROI circle is already the outer sampling boundary
     *   and inner_gap is the annulus width, so display [outer=roi_radius,
     *   inner=roi_radius-inner_gap].  Do not add linegap again here, otherwise
     *   UI/evidence shows a narrow band near the outer edge while the script
     *   requested a true inner-to-outer annulus.
     */
    const double outer_radius = has_annulus_scan
        ? roi_radius
        : roi_radius + static_cast<double>(linegap);
    const int inner_gap = (has_annulus_scan)
        ? publish_request.inner_gap
        : linegap;
    const double inner_radius =
        std::max(1.0, roi_radius - static_cast<double>(inner_gap));

    /*
     * The editable FindCircle ROI must carry the whole annulus geometry.
     * Image View only draws handles for editable shapes, so storing only the
     * outer circle here leaves the inner-radius handle disconnected from
     * Key Parameter Controls and global_* export.
     */
    auto roi_circle = std::make_unique<CircleShape>(
        static_cast<double>(cx),
        static_cast<double>(cy),
        has_annulus_scan ? outer_radius : roi_radius,
        has_annulus_scan ? inner_radius : 0.0);
    roi_circle->setScanSector(
        publish_request.has_arc_window,
        publish_request.arc_start_degrees,
        publish_request.arc_end_degrees);
    sink.UpsertShape(
        owner_ref + ".roi_circle",
        "FindCircle",
        owner_ref,
        "setcircle",
        "roi",
        true,
        false,
        std::move(roi_circle));

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

    if (has_annulus_scan)
    {
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

bool FindCircle::getscanline(
    int scan_index,
    CxShapePoint& p0,
    CxShapePoint& p1) const
{
    if (scan_index < 0 ||
        scan_index >= static_cast<int>(m_lines.size()))
    {
        return false;
    }

    return m_lines[static_cast<std::size_t>(scan_index)].exportLine(p0, p1);
}

int FindCircle::getsamplecount() const
{
    return m_budget_state.sample_count;
}

const std::string& FindCircle::getfailurestage() const
{
    return m_lastMeasureGeometryDebug.failure_stage;
}

void FindCircle::ClearMeasureState()
{
    m_measurepoints.clear();
    m_circle_fit_candidate_sequences.clear();
    m_circle_best_sequence_index = -1;
    m_circle_edge_band_candidates.clear();
    m_circle_feature_graph = CircleFeatureGraph();
}

namespace {

double CircleDotProduct(const std::vector<double>& a, const std::vector<double>& b)
{
    if (a.size() != b.size()) return 0.0;
    double sum = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i) sum += a[i] * b[i];
    return sum;
}

double CircleMean(const std::vector<double>& v)
{
    if (v.empty()) return 0.0;
    double sum = 0.0;
    for (double x : v) sum += x;
    return sum / static_cast<double>(v.size());
}

double CircleCalculateNCC(const std::vector<double>& a, const std::vector<double>& b)
{
    if (a.empty() || b.empty()) return 0.0;
    std::size_t n = std::min(a.size(), b.size());
    if (n < 2) return 0.0;

    std::vector<double> a_aligned(a.begin(), a.begin() + n);
    std::vector<double> b_aligned(b.begin(), b.begin() + n);

    double mean_a = CircleMean(a_aligned);
    double mean_b = CircleMean(b_aligned);

    std::vector<double> a_centered(n), b_centered(n);
    for (std::size_t i = 0; i < n; ++i)
    {
        a_centered[i] = a_aligned[i] - mean_a;
        b_centered[i] = b_aligned[i] - mean_b;
    }

    double numerator = CircleDotProduct(a_centered, b_centered);
    double denom_a = std::sqrt(CircleDotProduct(a_centered, a_centered));
    double denom_b = std::sqrt(CircleDotProduct(b_centered, b_centered));

    if (denom_a < 1e-8 || denom_b < 1e-8) return 0.0;
    double ncc = numerator / (denom_a * denom_b);
    return std::max(-1.0, std::min(1.0, ncc));
}

std::vector<double> ExtractCircleProfileAt(const cv::Mat& gray,
                                            int cx, int cy, int radius,
                                            int half_width,
                                            double angle_deg)
{
    std::vector<double> profile;
    double angle_rad = angle_deg * 3.14159265358979323846 / 180.0;
    double tangent_x = -std::sin(angle_rad);
    double tangent_y = std::cos(angle_rad);

    int w = gray.cols;
    int h = gray.rows;

    for (int offset = -half_width; offset <= half_width; ++offset)
    {
        double px = cx + radius * std::cos(angle_rad) + tangent_x * offset;
        double py = cy + radius * std::sin(angle_rad) + tangent_y * offset;

        int ix = static_cast<int>(std::lround(px));
        int iy = static_cast<int>(std::lround(py));

        if (ix >= 0 && ix < w && iy >= 0 && iy < h)
        {
            profile.push_back(static_cast<double>(gray.at<uchar>(iy, ix)));
        }
        else
        {
            profile.push_back(0.0);
        }
    }
    return profile;
}

} // anonymous namespace

void FindCircle::MeasureRobust(Image& image)
{
    // [DIAG] Log key algorithm parameters at MeasureRobust entry
    std::cout << "[DIAG] FindCircle::MeasureRobust entry: threshold=" << m_iThreshold
              << " method=" << m_iMethod
              << " linegap=" << m_iSelectPointGap
              << " gamma=" << m_igamarate
              << " center=(" << m_icentx << "," << m_icenty
              << ")" << " radius=" << m_dradius
              << std::endl;

    ClearMeasureState();
    m_circle_fit_candidate_sequences.clear();
    m_circle_best_sequence_index = -1;
    m_circle_edge_band_candidates.clear();
    m_circle_feature_graph = CircleFeatureGraph();

    if (!EnsureCircleMeasureGeometryReady())
    {
        LogFindCircleMeasureProbe("robust_measure", "skipped", "geometry not ready");
        return;
    }

    if (!ImageManager::EnsureAlgorithmRuntimeResources(
            image.getWidth(),
            image.getHeight()))
    {
        LogFindCircleMeasureProbe("robust_measure", "failed", "EnsureAlgorithmRuntimeResources failed");
        return;
    }

    g_pbackimage = ImageManager::GetBackImage(1);

    if (g_pbackimage == nullptr || g_pbackimage == &image ||
        g_pbackimage->getmat().empty())
    {
        LogFindCircleMeasureProbe("robust_measure", "failed", "backimage unavailable");
        return;
    }

    {
        const int isize = ClampSizeToInt(m_lines.size());
        if (isize <= 0)
        {
            return;
        }

        int ilineslen = 0;
        if (isize > 0)
            ilineslen = m_lines[0].getlinesize();

        if (ilineslen <= 0)
        {
            return;
        }

        for (int i = 0; i < isize; i++)
        {
            m_lines[i].linecopyex(image, *g_pbackimage, 0, i);
        }

        g_pbackimage->setroi(0, 0, ilineslen, isize);
        g_pbackimage->roi_7blur_gap_mud_thre_bw(m_iThreshold, m_igamarate, m_iSelectPointGap, m_iMethod);
    }

    CollectCircleEdgeBandsRobust(image);

    if (m_circle_edge_band_candidates.empty())
    {
        LogFindCircleMeasureProbe("robust_measure", "no_candidates", "no edge bands found");
        return;
    }

    BuildCircleFeatureGraph();

    if (m_circle_feature_graph.nodes.empty())
    {
        LogFindCircleMeasureProbe("robust_measure", "no_graph", "feature graph empty");
        return;
    }

    FindCircleComponentsInGraph();

    SelectBestCircleSequence();

    if (m_circle_best_sequence_index < 0 ||
        m_circle_best_sequence_index >= static_cast<int>(m_circle_fit_candidate_sequences.size()))
    {
        LogFindCircleMeasureProbe("robust_measure", "no_sequence", "no valid sequence selected");
        return;
    }

    ConvertCircleSequenceToMeasurePoints(m_circle_best_sequence_index);

    LogFindCircleMeasureProbe("robust_measure", "success",
        "robust measurement completed with " +
        std::to_string(m_circle_fit_candidate_sequences[m_circle_best_sequence_index].node_count) +
        " points");
}

void FindCircle::CollectCircleEdgeBandsRobust(Image& image)
{
    m_circle_edge_band_candidates.clear();

    if (g_pbackimage == nullptr || g_pbackimage->getmat().empty())
    {
        if (g_pbackfindobject != nullptr)
        {
            g_pbackfindobject->Measure(*g_pbackimage);
        }
        if (g_pbackimage == nullptr || g_pbackimage->getmat().empty())
        {
            return;
        }
    }

    cv::Mat gray;
    if (image.getmat().channels() == 1)
        gray = image.getmat().clone();
    else if (image.getmat().channels() == 3)
        cv::cvtColor(image.getmat(), gray, cv::COLOR_BGR2GRAY);
    else
        gray = image.getmat();

    cv::Mat binary = g_pbackimage->getmat();

    int w = binary.cols;
    int h = binary.rows;
    int cx = m_icentx;
    int cy = m_icenty;

    if (w <= 0 || h <= 0)
        return;

    int half_width = m_igap;
    if (half_width < 1) half_width = 3;

    for (int line_idx = 0; line_idx < static_cast<int>(m_lines.size()); ++line_idx)
    {
        auto& line = m_lines[line_idx];
        int candidate_index = 0;
        int line_size = line.getlinesize();

        bool in_foreground = false;
        int seg_start = -1;
        int seg_end = -1;

        for (int pt_idx = 0; pt_idx < line_size; ++pt_idx)
        {
            gp_Pnt pt = line.getlinepoint(pt_idx);
            int ix = static_cast<int>(std::lround(pt.X()));
            int iy = static_cast<int>(std::lround(pt.Y()));

            if (ix < 0 || ix >= w || iy < 0 || iy >= h)
                continue;

            bool is_foreground = binary.at<uchar>(iy, ix) > 0;

            if (is_foreground && !in_foreground)
            {
                in_foreground = true;
                seg_start = pt_idx;
            }
            else if (!is_foreground && in_foreground)
            {
                in_foreground = false;
                seg_end = pt_idx;

                int seg_len = seg_end - seg_start;
                if (seg_len >= 2)
                {
                    int seg_center = (seg_start + seg_end) / 2;
                    gp_Pnt center_pt = line.getlinepoint(seg_center);

                    CircleEdgeBandCandidate candidate;
                    candidate.scan_index = line_idx;
                    candidate.candidate_index = candidate_index++;
                    candidate.start_angle = seg_start;
                    candidate.end_angle = seg_end;
                    candidate.center_angle = seg_center;
                    candidate.x = center_pt.X();
                    candidate.y = center_pt.Y();
                    candidate.arc_length = static_cast<double>(seg_len);
                    candidate.response_strength = static_cast<double>(seg_len);
                    candidate.polarity = 1.0;
                    candidate.edge_rank = seg_len >= 5 ? 0 : (seg_len >= 3 ? 1 : 2);
                    candidate.valid = true;

                    double dx = candidate.x - cx;
                    double dy = candidate.y - cy;
                    double dist = std::sqrt(dx * dx + dy * dy);
                    double angle_deg = std::atan2(dy, dx) * 180.0 / 3.14159265358979323846;

                    candidate.profile = ExtractCircleProfileAt(
                        gray, cx, cy,
                        static_cast<int>(std::lround(dist)),
                        half_width, angle_deg);

                    if (candidate.profile.size() >= 5)
                    {
                        m_circle_edge_band_candidates.push_back(candidate);
                    }
                }
            }
        }

        if (in_foreground)
        {
            seg_end = line_size;
            int seg_len = seg_end - seg_start;
            if (seg_len >= 2)
            {
                int seg_center = (seg_start + seg_end) / 2;
                gp_Pnt center_pt = line.getlinepoint(seg_center);

                CircleEdgeBandCandidate candidate;
                candidate.scan_index = line_idx;
                candidate.candidate_index = candidate_index++;
                candidate.start_angle = seg_start;
                candidate.end_angle = seg_end;
                candidate.center_angle = seg_center;
                candidate.x = center_pt.X();
                candidate.y = center_pt.Y();
                candidate.arc_length = static_cast<double>(seg_len);
                candidate.response_strength = static_cast<double>(seg_len);
                candidate.polarity = 1.0;
                candidate.edge_rank = 0;
                candidate.valid = true;

                double dx = candidate.x - cx;
                double dy = candidate.y - cy;
                double dist = std::sqrt(dx * dx + dy * dy);
                double angle_deg = std::atan2(dy, dx) * 180.0 / 3.14159265358979323846;

                candidate.profile = ExtractCircleProfileAt(
                    gray, cx, cy,
                    static_cast<int>(std::lround(dist)),
                    half_width, angle_deg);

                if (candidate.profile.size() >= 5)
                {
                    m_circle_edge_band_candidates.push_back(candidate);
                }
            }
        }
    }
}

void FindCircle::BuildCircleFeatureGraph()
{
    m_circle_feature_graph = CircleFeatureGraph();

    for (std::size_t i = 0; i < m_circle_edge_band_candidates.size(); ++i)
    {
        CircleFeatureNode node;
        node.id = static_cast<int>(i);
        node.candidate = m_circle_edge_band_candidates[i];
        m_circle_feature_graph.nodes.push_back(node);
    }

    if (m_circle_feature_graph.nodes.size() < 2)
        return;

    double T_space = 15.0;
    double T_width = 10.0;
    double T_ncc = 0.75;

    for (std::size_t i = 0; i < m_circle_feature_graph.nodes.size(); ++i)
    {
        for (std::size_t j = i + 1; j < m_circle_feature_graph.nodes.size(); ++j)
        {
            const auto& node_a = m_circle_feature_graph.nodes[i];
            const auto& node_b = m_circle_feature_graph.nodes[j];

            int scan_diff = std::abs(node_a.candidate.scan_index - node_b.candidate.scan_index);
            if (scan_diff != 1)
                continue;

            double dx = node_a.candidate.x - node_b.candidate.x;
            double dy = node_a.candidate.y - node_b.candidate.y;
            double spatial_dist = std::sqrt(dx * dx + dy * dy);

            if (spatial_dist > T_space)
                continue;

            double width_diff = std::abs(node_a.candidate.arc_length - node_b.candidate.arc_length);
            if (width_diff > T_width)
                continue;

            if (node_a.candidate.polarity != node_b.candidate.polarity)
                continue;

            double ncc = CircleCalculateNCC(node_a.candidate.profile, node_b.candidate.profile);
            if (ncc <= T_ncc)
                continue;

            CircleFeatureEdge edge;
            edge.node_a = static_cast<int>(i);
            edge.node_b = static_cast<int>(j);
            edge.ncc_score = ncc;
            edge.angular_distance = spatial_dist;
            edge.valid = true;

            m_circle_feature_graph.edges.push_back(edge);
            m_circle_feature_graph.nodes[i].neighbors.push_back(static_cast<int>(j));
            m_circle_feature_graph.nodes[j].neighbors.push_back(static_cast<int>(i));
        }
    }
}

void FindCircle::FindCircleComponentsInGraph()
{
    m_circle_feature_graph.next_component_id = 0;

    for (auto& node : m_circle_feature_graph.nodes)
    {
        node.visited = false;
        node.component_id = -1;
    }

    for (std::size_t i = 0; i < m_circle_feature_graph.nodes.size(); ++i)
    {
        if (m_circle_feature_graph.nodes[i].visited)
            continue;

        int component_id = m_circle_feature_graph.next_component_id++;
        std::vector<int> stack;
        stack.push_back(static_cast<int>(i));

        while (!stack.empty())
        {
            int current = stack.back();
            stack.pop_back();

            if (m_circle_feature_graph.nodes[current].visited)
                continue;

            m_circle_feature_graph.nodes[current].visited = true;
            m_circle_feature_graph.nodes[current].component_id = component_id;

            for (int neighbor : m_circle_feature_graph.nodes[current].neighbors)
            {
                if (!m_circle_feature_graph.nodes[neighbor].visited)
                {
                    stack.push_back(neighbor);
                }
            }
        }
    }
}

void FindCircle::SelectBestCircleSequence()
{
    m_circle_fit_candidate_sequences.clear();

    if (m_circle_feature_graph.nodes.empty())
        return;

    int max_component_id = m_circle_feature_graph.next_component_id;
    if (max_component_id <= 0)
        return;

    std::vector<std::vector<int>> component_nodes(max_component_id);

    for (const auto& node : m_circle_feature_graph.nodes)
    {
        if (node.component_id >= 0 && node.component_id < max_component_id)
        {
            component_nodes[node.component_id].push_back(node.id);
        }
    }

    for (const auto& node_ids : component_nodes)
    {
        if (node_ids.size() < 2)
            continue;

        CircleFitCandidateSequence seq;

        std::vector<const CircleFeatureNode*> sorted_nodes;
        for (int id : node_ids)
        {
            sorted_nodes.push_back(&m_circle_feature_graph.nodes[id]);
        }

        std::sort(sorted_nodes.begin(), sorted_nodes.end(),
            [](const CircleFeatureNode* a, const CircleFeatureNode* b)
            {
                return a->candidate.scan_index < b->candidate.scan_index;
            });

        double total_ncc = 0.0;
        double total_response = 0.0;

        for (const auto* node : sorted_nodes)
        {
            cv::Point2d pt(node->candidate.x, node->candidate.y);
            seq.points.push_back(pt);
            total_response += node->candidate.response_strength;
        }

        int edge_count = 0;
        for (const auto& edge : m_circle_feature_graph.edges)
        {
            if (edge.valid &&
                std::find(node_ids.begin(), node_ids.end(), edge.node_a) != node_ids.end() &&
                std::find(node_ids.begin(), node_ids.end(), edge.node_b) != node_ids.end())
            {
                total_ncc += edge.ncc_score;
                edge_count++;
            }
        }

        seq.node_count = static_cast<int>(sorted_nodes.size());
        seq.avg_ncc = edge_count > 0 ? total_ncc / edge_count : 0.0;
        seq.total_response = total_response;
        seq.score = seq.node_count * 100.0 + seq.avg_ncc * 50.0 + total_response;
        seq.valid = true;

        m_circle_fit_candidate_sequences.push_back(seq);
    }

    std::sort(m_circle_fit_candidate_sequences.begin(), m_circle_fit_candidate_sequences.end(),
        [](const CircleFitCandidateSequence& a, const CircleFitCandidateSequence& b)
        {
            return a.score > b.score;
        });

    if (m_circle_fit_candidate_sequences.size() > 5)
    {
        m_circle_fit_candidate_sequences.resize(5);
    }

    if (!m_circle_fit_candidate_sequences.empty())
    {
        m_circle_best_sequence_index = 0;
    }
    else
    {
        m_circle_best_sequence_index = -1;
    }
}

void FindCircle::ConvertCircleSequenceToMeasurePoints(int sequence_index)
{
    if (sequence_index < 0 ||
        sequence_index >= static_cast<int>(m_circle_fit_candidate_sequences.size()))
        return;

    const auto& seq = m_circle_fit_candidate_sequences[sequence_index];
    m_measurepoints.clear();

    for (const auto& pt : seq.points)
    {
        m_measurepoints.addpoint(gp_Pnt(pt.x, pt.y, 0.0));
    }
}
