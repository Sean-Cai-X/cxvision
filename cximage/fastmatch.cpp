#include "pch.h"

#include "FastMatch.h"
#include "imagemanager.h"
#include "ImageAnnotationLayer.h"
#include <algorithm>
#include <cmath>
#include <opencv2/imgproc.hpp>
#include <format> // C++20 formatting
#if defined USE_AI
#include "mlpackrun.h"
#endif

namespace
{
int FastMatchPositiveInt(int value, int fallback = 1)
{
    return value > 0 ? value : fallback;
}

int FastMatchNonNegativeInt(int value)
{
    return std::max(0, value);
}

double FastMatchFiniteOr(double value, double fallback = 0.0)
{
    return std::isfinite(value) ? value : fallback;
}

double FastMatchPositiveFiniteOr(double value, double fallback = 1.0)
{
    return std::isfinite(value) && value > 0.0 ? value : fallback;
}

double FastMatchUnitScore(double value, double fallback = 0.4)
{
    if (!std::isfinite(value))
    {
        value = fallback;
    }
    return std::clamp(value, 0.0, 1.0);
}
}

void removeAt(std::vector<int>& vec, size_t index) {
    if (index < vec.size()) {
        // erase item at index
        vec.erase(vec.begin() + index);
    }
    else {
        std::cerr << "Index out of bounds" << std::endl;
    }
}
void removePntAt(std::vector<gp_Pnt>& vec, size_t index) {
    if (index < vec.size()) {
        // erase item at index
        vec.erase(vec.begin() + index);
    }
    else {
        std::cerr << "Index out of bounds" << std::endl;
    }
} 
void removedoubleAt(std::vector<double>& vec, size_t index) {
    if (index < vec.size()) {
        // erase item at index
        vec.erase(vec.begin() + index);
    }
    else {
        std::cerr << "Index out of bounds" << std::endl;
    }
}
void removePointsShapeAt(std::vector<PointsShape>& vec, size_t index) {
    if (index < vec.size()) {
        // erase item at index
        vec.erase(vec.begin() + index);
    }
    else {
        std::cerr << "Index out of bounds" << std::endl;
    }
}

void removeLast(std::vector<int>& vec) {
    if (!vec.empty()) {
        vec.pop_back(); // remove last element
    }
    else {
        std::cerr << "Vector is already empty" << std::endl;
    }
}
void removedoubleLast(std::vector<double>& vec) {
    if (!vec.empty()) {
        vec.pop_back(); // remove last element
    }
    else {
        std::cerr << "Vector is already empty" << std::endl;
    }
}
void removePntLast(std::vector<gp_Pnt>& vec) {
    if (!vec.empty()) {
        vec.pop_back(); // remove last element
    }
    else {
        std::cerr << "Vector is already empty" << std::endl;
    }
}
void removePointsShapeLast(std::vector<PointsShape>& vec) {
    if (!vec.empty()) {
        vec.pop_back(); // remove last element
    }
    else {
        std::cerr << "Vector is already empty" << std::endl;
    }
}

int EvaluateMatchSampleABScore(
    Image& image,
    gp_Path& pathA,
    gp_Path& pathB,
    int movx,
    int movy,
    int ithre,
    int ib2w,
    int iminfindngnum)
{
    const int icount = std::min(
        static_cast<int>(pathA.ElementCount()),
        static_cast<int>(pathB.ElementCount()));
    int icalnum = 0;
    int icalng = 0;
    for (int i = 0; i < icount - 1; ++i)
    {
        const gp_Pnt pointA = pathA.ElementAt(i);
        const gp_Pnt pointB = pathB.ElementAt(i);
        const cv::Vec3b pixel0 = image.pixel(
            static_cast<int>(pointA.X() + movx),
            static_cast<int>(pointA.Y() + movy));
        const cv::Vec3b pixel1 = image.pixel(
            static_cast<int>(pointB.X() + movx),
            static_cast<int>(pointB.Y() + movy));

        if (ib2w == 0)
        {
            const int ir = Red(pixel0) - Red(pixel1);
            const int ig = Green(pixel0) - Green(pixel1);
            const int ib = Blue(pixel0) - Blue(pixel1);
            if (ir > ithre || ig > ithre || ib > ithre)
            {
                ++icalnum;
            }
            else if (++icalng > iminfindngnum)
            {
                break;
            }
        }
        else
        {
            const int ir = Red(pixel1) - Red(pixel0);
            const int ig = Green(pixel1) - Green(pixel0);
            const int ib = Blue(pixel1) - Blue(pixel0);
            if (ir > ithre || ig > ithre || ib > ithre)
            {
                ++icalnum;
            }
            else if (++icalng > iminfindngnum)
            {
                break;
            }
        }
    }
    return icalnum;
}

bool MatchSampleABAnchorPass(
    Image& image,
    gp_Path& pathA,
    gp_Path& pathB,
    int movx,
    int movy,
    int ithre,
    int ib2w)
{
    const int icount = std::min(
        static_cast<int>(pathA.ElementCount()),
        static_cast<int>(pathB.ElementCount()));
    if (icount <= 0)
    {
        return false;
    }

    const int anchor_count = std::min(5, icount);
    const int anchor_step = std::max(1, icount / anchor_count);
    int hit_count = 0;
    int probe_count = 0;
    for (int anchor_index = 0; anchor_index < anchor_count; ++anchor_index)
    {
        int sample_index = anchor_index * anchor_step;
        if (anchor_index + 1 == anchor_count)
        {
            sample_index = icount - 1;
        }

        const gp_Pnt pointA = pathA.ElementAt(sample_index);
        const gp_Pnt pointB = pathB.ElementAt(sample_index);
        const cv::Vec3b pixel0 = image.pixel(
            static_cast<int>(pointA.X() + movx),
            static_cast<int>(pointA.Y() + movy));
        const cv::Vec3b pixel1 = image.pixel(
            static_cast<int>(pointB.X() + movx),
            static_cast<int>(pointB.Y() + movy));
        const int dr = ib2w == 0 ? Red(pixel0) - Red(pixel1) : Red(pixel1) - Red(pixel0);
        const int dg = ib2w == 0 ? Green(pixel0) - Green(pixel1) : Green(pixel1) - Green(pixel0);
        const int db = ib2w == 0 ? Blue(pixel0) - Blue(pixel1) : Blue(pixel1) - Blue(pixel0);
        ++probe_count;
        if (dr > ithre || dg > ithre || db > ithre)
        {
            ++hit_count;
        }
    }

    // A single positive anchor keeps recall high while rejecting clear background.
    return probe_count == 0 || hit_count > 0;
}

bool FastMatchMaskPixelPass(const cv::Mat* mask, int x, int y)
{
    if (mask == nullptr || mask->empty())
    {
        return true;
    }
    if (x < 0 || y < 0 || x >= mask->cols || y >= mask->rows)
    {
        return false;
    }
    if (mask->depth() != CV_8U)
    {
        return true;
    }

    const int channels = mask->channels();
    const uchar* pixel = mask->ptr<uchar>(y) + x * channels;
    for (int channel = 0; channel < channels; ++channel)
    {
        if (pixel[channel] != 0)
        {
            return true;
        }
    }
    return false;
}

bool FastMatchCandidateMaskPass(const cv::Mat* mask, gp_Path& path, int movx, int movy)
{
    if (mask == nullptr || mask->empty())
    {
        return true;
    }

    const gp_Rectangle bounds = path.boundingRect();
    const int center_x = static_cast<int>(bounds.TopLeft().X() + bounds.Width() / 2 + movx);
    const int center_y = static_cast<int>(bounds.TopLeft().Y() + bounds.Height() / 2 + movy);
    return FastMatchMaskPixelPass(mask, center_x, center_y);
}
gp_Pnt RefineMatchSampleABPoint(
    Image& image,
    gp_Path& pathA,
    gp_Path& pathB,
    const gp_Pnt& coarse_point,
    int coarse_score,
    int ithre,
    int ib2w,
    int iminfindngnum,
    int stepx,
    int stepy,
    int& refined_score)
{
    gp_Pnt best_point = coarse_point;
    int best_score = coarse_score;
    int cur_step_x = std::max(1, stepx / 2);
    int cur_step_y = std::max(1, stepy / 2);

    while (true)
    {
        gp_Pnt stage_best = best_point;
        int stage_score = best_score;
        for (int dy = -cur_step_y; dy <= cur_step_y; dy += cur_step_y)
        {
            for (int dx = -cur_step_x; dx <= cur_step_x; dx += cur_step_x)
            {
                const int probe_x = static_cast<int>(best_point.X()) + dx;
                const int probe_y = static_cast<int>(best_point.Y()) + dy;
                const int score = EvaluateMatchSampleABScore(
                    image,
                    pathA,
                    pathB,
                    probe_x,
                    probe_y,
                    ithre,
                    ib2w,
                    iminfindngnum);
                if (score > stage_score)
                {
                    stage_score = score;
                    stage_best = gp_Pnt(probe_x, probe_y, 0);
                }
            }
        }

        best_point = stage_best;
        best_score = stage_score;

        if (cur_step_x == 1 && cur_step_y == 1)
        {
            break;
        }

        cur_step_x = std::max(1, cur_step_x / 2);
        cur_step_y = std::max(1, cur_step_y / 2);
    }

    refined_score = best_score;
    return best_point;
}

gp_Pnt GetRotateFilterAnchor(
    const gp_Pnt& point,
    const PointsShape& shape,
    int itype)
{
    if (itype == 0)
    {
        return point;
    }
    return shape.getpointscent();
}

int EasyObjectWidthAt(const std::vector<easyobj>& models, int index)
{
    if (index < 0 || index >= static_cast<int>(models.size()))
    {
        return 0;
    }
    return models[index].s_iwobjnum;
}

int EasyObjectBlackCountAt(const std::vector<easyobj>& models, int index)
{
    if (index < 0 || index >= static_cast<int>(models.size()))
    {
        return 0;
    }
    return models[index].s_ibobjnum;
}

size_t RotateResultSharedCount(
    const std::vector<double>& results,
    const std::vector<gp_Pnt>& points,
    const std::vector<double>& angles,
    const std::vector<PointsShape>& shapes)
{
    return std::min(std::min(results.size(), points.size()), std::min(angles.size(), shapes.size()));
}

bool HasRotateResultAt(
    int index,
    const std::vector<double>& results,
    const std::vector<gp_Pnt>& points,
    const std::vector<double>& angles,
    const std::vector<PointsShape>& shapes)
{
    return index >= 0 && index < static_cast<int>(RotateResultSharedCount(results, points, angles, shapes));
}

void FilterRotateResultsByDistance(
    std::vector<double>& results,
    std::vector<gp_Pnt>& points,
    std::vector<double>& angles,
    std::vector<PointsShape>& shapes,
    int ifdx,
    int ifdy,
    int itype,
    bool full_scan)
{
    const size_t shared_count = std::min(std::min(results.size(), points.size()), std::min(angles.size(), shapes.size()));
    if (shared_count == 0)
    {
        return;
    }
    results.resize(shared_count);
    points.resize(shared_count);
    angles.resize(shared_count);
    shapes.resize(shared_count);

    if (!full_scan)
    {
        for (int i = 1; i < static_cast<int>(results.size());)
        {
            const gp_Pnt lhs = GetRotateFilterAnchor(points.at(i - 1), shapes.at(i - 1), itype);
            const gp_Pnt rhs = GetRotateFilterAnchor(points.at(i), shapes.at(i), itype);
            const int iabsx = std::abs(static_cast<int>(lhs.X() - rhs.X()));
            const int iabsy = std::abs(static_cast<int>(lhs.Y() - rhs.Y()));
            if (iabsx < ifdx && iabsy < ifdy)
            {
                for (int j = i; j + 1 < static_cast<int>(results.size()); ++j)
                {
                    results[j] = results.at(j + 1);
                    points[j] = points.at(j + 1);
                    angles[j] = angles.at(j + 1);
                    shapes[j] = shapes.at(j + 1);
                }
                removedoubleLast(results);
                removePntLast(points);
                removedoubleLast(angles);
                removePointsShapeLast(shapes);
            }
            else
            {
                ++i;
            }
        }
        return;
    }

    for (int i = 0; i < static_cast<int>(results.size()); ++i)
    {
        for (int j = 0; j < static_cast<int>(results.size());)
        {
            if (i == j)
            {
                ++j;
                continue;
            }

            const gp_Pnt lhs = GetRotateFilterAnchor(points.at(i), shapes.at(i), itype);
            const gp_Pnt rhs = GetRotateFilterAnchor(points.at(j), shapes.at(j), itype);
            const int iabsx = std::abs(static_cast<int>(lhs.X() - rhs.X()));
            const int iabsy = std::abs(static_cast<int>(lhs.Y() - rhs.Y()));
            if (iabsx < ifdx && iabsy < ifdy)
            {
                removedoubleAt(results, j);
                removedoubleAt(angles, j);
                removePntAt(points, j);
                removePointsShapeAt(shapes, j);
                if (j < i)
                {
                    --i;
                }
            }
            else
            {
                ++j;
            }
        }
    }
}

void NormalizeMatchCandidates(
    std::vector<int>& scores,
    std::vector<gp_Pnt>& points,
    int keep_limit,
    int& min_score,
    gp_Pnt& min_point)
{
    const size_t shared_count = std::min(scores.size(), points.size());
    if (scores.size() != shared_count)
        scores.resize(shared_count);
    if (points.size() != shared_count)
        points.resize(shared_count);

    struct Candidate
    {
        int score;
        gp_Pnt point;
    };

    std::vector<Candidate> candidates;
    candidates.reserve(shared_count);
    for (size_t i = 0; i < shared_count; ++i)
    {
        candidates.push_back({ scores[i], points[i] });
    }

    std::stable_sort(
        candidates.begin(),
        candidates.end(),
        [](const Candidate& lhs, const Candidate& rhs)
        {
            if (lhs.score != rhs.score)
                return lhs.score < rhs.score;
            if (lhs.point.X() != rhs.point.X())
                return lhs.point.X() < rhs.point.X();
            return lhs.point.Y() < rhs.point.Y();
        });

    const size_t normalized_keep = static_cast<size_t>(std::max(1, keep_limit));
    while (candidates.size() > normalized_keep)
    {
        candidates.erase(candidates.begin());
    }

    scores.clear();
    points.clear();
    scores.reserve(candidates.size());
    points.reserve(candidates.size());
    for (const Candidate& candidate : candidates)
    {
        scores.push_back(candidate.score);
        points.push_back(candidate.point);
    }

    min_score = -1;
    min_point = gp_Pnt();
    if (!candidates.empty())
    {
        min_score = candidates.front().score;
        min_point = candidates.front().point;
    }
}

int fastmatch::m_curfastmatchnum = 0;
fastmatch::fastmatch() :Findline(),
m_matchimage(0),
m_matchmask(nullptr),
m_imaxmatchnum(10),
m_iminfindnum(-1),
m_imatchthre(5),
m_ispecshow(-1),
m_iB2W(0),
m_imatchoffset(1),
m_dminscore(0.4),
m_prelationmatch(0),
m_irelationresultnum(0),
m_drelationzoomx(1),
m_drelationzoomy(1),
m_danglegap(2),
m_dangle_add(60),
m_dangle_mud(-60),
m_stepgapx(1),
m_stepgapy(1),
m_ixclustergap(5),
m_iyclustergap(5),
m_iangleclustergap(5),
m_istyle(0),
m_rootgridA(0),
m_iupgradexscale(5),
m_iupgradeyscale(5),
m_iupgradeanglescale(6),
m_matchrect(gp_Pnt(0,0,0), gp_Pnt(0, 0, 0)),
m_irelationrect(gp_Pnt(0, 0, 0), gp_Pnt(0, 0, 0))
{
    setcolor(0, 0, 255);
    std::ostringstream stream;
    stream  << "fmatch " << m_curfastmatchnum;
    std::string strname = stream.str();
    (void)strname;
    m_curfastmatchnum = m_curfastmatchnum + 1;
    m_matchrects.setshow(2);
    m_imatchrectnum = 1;
    gp_Rectangle rect(gp_Pnt(20, 20,0), gp_Pnt(300, 200,0));
    m_matchrects.addrect(rect);

    int icurmodule = ImageManager::GetCurMode();
    g_pmodelimage = ImageManager::GetModelImage(icurmodule);
    //m_rootgridA = ImageManager::GetRootGridA(icurmodule);

    m_pgrid = new Grid;
    m_pgrid->setgrid(30, 30, 12, 12, 30, 30);

}
void fastmatch::setgrid(int iw, int igrid)
{
    if (m_pgrid == nullptr)
    {
        return;
    }
    m_pgrid->setgrid(FastMatchPositiveInt(iw), FastMatchPositiveInt(iw), FastMatchPositiveInt(igrid), FastMatchPositiveInt(igrid), FastMatchPositiveInt(iw), FastMatchPositiveInt(iw));
}
fastmatch::~fastmatch()
{
    delete m_pgrid;
}
void fastmatch::setcomparegap(int igap)
{
    Findline::setcomparegap(igap);
}
void fastmatch::setfindnum(int ifindnum)
{
    m_imaxmatchnum = FastMatchPositiveInt(ifindnum);
}
void fastmatch::setmatchmask(const cv::Mat* pmask)
{
    m_matchmask = pmask;
}
void fastmatch::clearmatchmask()
{
    m_matchmask = nullptr;
}
void fastmatch::setb2w(int ib2w)
{
    m_iB2W = ib2w == 1 ? 1 : 0;
}
void fastmatch::getshape(void* pshape)
{
    Shape* pshape0 = (Shape*)pshape;
    if (pshape0 == nullptr)
        return;

    const gp_Rectangle arect = rect();
    pshape0->setrect(static_cast<int>(arect.TopLeft().X()),
        static_cast<int>(arect.TopLeft().Y()),
        static_cast<int>(arect.Width()),
        static_cast<int>(arect.Height()));
}
void fastmatch::setrect(int ix, int iy, int iw, int ih)
{
    Findline::setrect(ix, iy, iw, ih);
}
void fastmatch::setshow(int ishow)
{ 
    if (ishow == 8)
    {
        m_resultrects.setcolor(0, 255,0);
        m_resultrects.setshow(1);
        m_resultrects.MakeShape(-1); 
    }
    else if(ishow==-8)
    {
        m_resultrects.setcolor(0, 255, 0);
        m_resultrects.setshow(1);
        m_resultrects.MakeShape();
        ishow = 8;
    }
    else if (ishow == 16)
    {
        const int irsize = static_cast<int>(m_rotateshaperesults.size());
        if (irsize > 0)
        {
            m_rotateshaperesults[0].setcolor(0, 255, 0);
            m_rotateshaperesults[0].setshow(32);
        }
    }
    else if (ishow == 32)
    {
        const int irsize = static_cast<int>(m_rotateshaperesults.size());
           for (int i = 0; i < irsize; i++)
           {
               m_rotateshaperesults[i].setcolor(0, 255, 0);
               m_rotateshaperesults[i].setshow(32);
           } 
    }

    Findline::setshow(ishow);
}
void fastmatch::SetWHgap(int wgap, int hgap)
{
    Findline::SetWHgap(wgap, hgap);
}
void fastmatch::measure(void* pimage)
{
    Findline::measure(pimage);
}
void fastmatch::setlinesamplerate(double dsamplerate)
{
    Findline::setlinesamplerate(dsamplerate);
}
void fastmatch::setlinegap(int igap)
{
    Findline::setlinegap(igap);
}
void fastmatch::setmethod(int imethod)
{
    Findline::setmethod(imethod);
}
void fastmatch::setthre(int ithre)
{
    Findline::setthre(ithre);
}
void fastmatch::setmatchthre(int ithre)
{
    m_imatchthre = FastMatchNonNegativeInt(ithre);
}
void fastmatch::setobjfilter(int ifindset)
{
    Findline::setobjfilter(ifindset);
}
void fastmatch::setfilter(int ifilterborw, int ifiltermin, int ifiltermax)
{
    Findline::setfilter(ifilterborw, ifiltermin, ifiltermax);
}
void fastmatch::setselectedgenum(int iedgenum)
{
    Findline::setselectedgenum(iedgenum);
}
vector<PointsShape>& fastmatch::getmodels_l12()
{
    return m_models_l12;
}
void fastmatch::setspecshow(int ishow)
{
    m_ispecshow = ishow;
    m_resultrects.setspecshow(ishow);
    m_matchrects.setspecshow(ishow);
}

void fastmatch::setshownum(int ishownum)
{
    m_ishownum = FastMatchPositiveInt(ishownum);
}
void fastmatch::drawshape( )
{
    gp_Path painter;
    if (show() & 0x20)
    {
        m_pgrid->setshow(0x04);
     // m_pgrid->drawshape(painter);
    }
    if (show() & 0x08)
    {
     // drawpattern(painter);
    }
    if (show() & 0x10)
    {
        m_modelpoints_sample1.setshow(8);
        m_modelpoints_sample1.drawshape(painter);
    }
    if (show() & 0x02)
    {
        m_resultrects.setshow(2);
        m_resultrects.drawshape(painter);
    }
    if (show() & 0x04)
    {
        m_resultrects.setshow(4);
        m_resultrects.drawshape(painter);
    }
    else if (show() & 0x40)
    {
        /*int irsize = m_rotateresults.size();
        for(int i=0;i<irsize;i=i+4)
        {
           PointsShape apoints= m_rotateshaperesults[i];
           apoints.setshow(16);
           apoints.drawshape(painter);
        }
        */
        const int iclustersize = static_cast<int>(m_clusters.size());
        for (int ic = 0; ic < iclustersize; ic++)
        {
            if (m_clusters[ic].empty())
            {
                continue;
            }
            const int id = m_clusters[ic][0];
            if (id < 0 || id >= static_cast<int>(m_rotateshaperesults.size()))
            {
                continue;
            }
            PointsShape apoints = m_rotateshaperesults[id];
            apoints.setshow(16);
            apoints.drawshape(painter);
        }
    }
    else if (show() & 0x80)
    {
        const int isize = static_cast<int>(std::min(m_models_rotate.size(), m_models_rotaterects.size()));
        for (int iz = 0; iz < isize; iz++)
        {
            if (-1 == m_ispecshow || iz == m_ispecshow)
            {
                m_models_rotate[iz].setshow(8);
                m_models_rotate[iz].drawshape(painter);
                m_models_rotaterects[iz].setshow(16);
                m_models_rotaterects[iz].drawshape(painter);
            }
        }
    }
    else
        Findline::drawshape( );
    m_matchrects.drawshape(painter);
}
void fastmatch::setcolorstyle(int istyle)
{
    m_istyle = istyle;
}
void fastmatch::drawshapex(
    double dmovx,
    double dmovy,
    double dangle,
    double dzoomx,
    double dzoomy)
{
    if (show() & 0x20)
    {
        m_pgrid->setshow(0x04);
        m_pgrid->drawshape();
    }
    if (show() & 0x08)
    {
        drawpattern();
    }
    if (show() & 0x10)
    {
        m_modelpoints_sample1.setshow(8);
        gp_Path painter;
        m_modelpoints_sample1.drawshape(painter);
    }
    if (show() & 0x02)
    {
        m_resultrects.setshow(2);
        gp_Path painter;
        m_resultrects.drawshapex(painter, dmovx, dmovy, dangle, dzoomx, dzoomy);
    }
    if (show() & 0x04)
    {
        m_resultrects.setshow(4);
        gp_Path painter;
        m_resultrects.drawshapex(painter, dmovx, dmovy, dangle, dzoomx, dzoomy);
    }
    else if (show() & 0x40)
    {
        const int irsize = static_cast<int>(RotateResultSharedCount(
            m_rotateresults,
            m_rotatereslutpoints,
            m_rotatereslutangles,
            m_rotateshaperesults));
        for (int i = 0; i < irsize; ++i)
        {
            if (i > m_ishownum || i < 0 || i >= static_cast<int>(m_rotateshaperesults.size()))
            {
                continue;
            }

            PointsShape rotated_points = m_rotateshaperesults[i];
            rotated_points.setPen(0, 255, 0);
            rotated_points.setshow(16);
            gp_Path painter;
            rotated_points.drawshapex(painter, dmovx, dmovy, dangle, dzoomx, dzoomy);
        }
    }
    else if (show() & 0x80)
    {
        const int isize = static_cast<int>(std::min(m_models_rotate.size(), m_models_rotaterects.size()));
        for (int iz = 0; iz < isize; ++iz)
        {
            if (-1 == m_ispecshow || iz == m_ispecshow)
            {
                m_models_rotate[iz].setshow(8);
                gp_Path painter;
                m_models_rotate[iz].drawshape(painter);
                m_models_rotaterects[iz].setshow(16);
                m_models_rotaterects[iz].drawshape(painter);
            }
        }
    }
    else
    {
        Findline::drawshape();
    }

    gp_Rectangle amatchrect(gp_Pnt(0, 0, 0), gp_Pnt(0, 0, 0));
    amatchrect.setrect(
        gp_Pnt(m_matchrect.TopLeft().X() * dzoomx + dmovx,
               m_matchrect.TopLeft().Y() * dzoomy + dmovy,
               0),
        gp_Pnt((m_matchrect.TopLeft().X() + m_matchrect.Width()) * dzoomx + dmovx,
               (m_matchrect.TopLeft().Y() + m_matchrect.Height()) * dzoomy + dmovy,
               0));
}

void fastmatch::Learn(Image& image)
{
    edgepattern(image);
    if (Findline::getpatternpathA().ElementCount() > 0
        && Findline::getpatternpathB().ElementCount() > 0)
    {
        return;
    }

    const int saved_wgap = wgap();
    const int saved_hgap = hgap();
    const int saved_comparegap = getconparegap();
    const int saved_threshold = thre();
    const int saved_linegap = linegap();
    const int saved_rect_x = static_cast<int>(rect().TopLeft().X());
    const int saved_rect_y = static_cast<int>(rect().TopLeft().Y());
    const int saved_rect_w = static_cast<int>(rect().Width());
    const int saved_rect_h = static_cast<int>(rect().Height());

    const int retry_wgap = saved_wgap > 1 ? 1 : saved_wgap;
    const int retry_hgap = saved_hgap > 1 ? 1 : saved_hgap;
    const int retry_comparegap = saved_comparegap > 5 ? 5 : (saved_comparegap < 2 ? 2 : saved_comparegap);
    const int retry_threshold = saved_threshold > 8 ? 8 : (saved_threshold < 6 ? 6 : saved_threshold);
    const int retry_linegap = 1;
    const int retry_margin = 2;
    const int retry_rect_x = saved_rect_x > retry_margin ? saved_rect_x - retry_margin : 0;
    const int retry_rect_y = saved_rect_y > retry_margin ? saved_rect_y - retry_margin : 0;
    const int retry_rect_right = (saved_rect_x + saved_rect_w + retry_margin) < image.getWidth() ?
        (saved_rect_x + saved_rect_w + retry_margin) : image.getWidth();
    const int retry_rect_bottom = (saved_rect_y + saved_rect_h + retry_margin) < image.getHeight() ?
        (saved_rect_y + saved_rect_h + retry_margin) : image.getHeight();
    const int retry_rect_w = retry_rect_right - retry_rect_x;
    const int retry_rect_h = retry_rect_bottom - retry_rect_y;

    setrect(retry_rect_x, retry_rect_y, retry_rect_w, retry_rect_h);
    SetWHgap(retry_wgap, retry_hgap);
    setcomparegap(retry_comparegap);
    setthre(retry_threshold);
    setlinegap(retry_linegap);
    edgepattern(image);

    setrect(saved_rect_x, saved_rect_y, saved_rect_w, saved_rect_h);
    SetWHgap(saved_wgap, saved_hgap);
    setcomparegap(saved_comparegap);
    setthre(saved_threshold);
    setlinegap(saved_linegap);
}
void fastmatch::ZeroPOS()
{
    modelzeroposition();
    gp_Rectangle arect1 = Findline::patternboundingrectAB();
    m_imodelwith = static_cast<int>(arect1.Width());
    m_imodelheigh = static_cast<int>(arect1.Height());
}
void fastmatch::Learn_level0(Image& image)//5pyrDown   thre >50
{ 
    Image aimage0;
    aimage0.CopyFrom(&image);
    aimage0.ROIpyrDown(5);
    setthre(50);
    setlinegap(7);
    edgepattern(aimage0);
    modelzeroposition();
    gp_Rectangle arect1 = Findline::patternboundingrectAB();
    m_imodelwith = static_cast<int>(arect1.Width());
    m_imodelheigh = static_cast<int>(arect1.Height());
}
void fastmatch::Learn_level1(Image& image)//5pyrDown   thre >30
{
    Image aimage0;
    aimage0.CopyFrom(&image);
    aimage0.ROIpyrDown(5);
    setthre(30);
    setlinegap(7);
    edgepattern(aimage0);
    modelzeroposition();
    gp_Rectangle arect1 = Findline::patternboundingrectAB();
    m_imodelwith = static_cast<int>(arect1.Width());
    m_imodelheigh = static_cast<int>(arect1.Height());
}
void fastmatch::Learn_level2(Image& image)//3pyrDown   thre >30
{
    Image aimage0;
    aimage0.CopyFrom(&image);
    aimage0.ROIpyrDown(3);
    setthre(30);
    setlinegap(6);
    edgepattern(aimage0);
    modelzeroposition();
    gp_Rectangle arect1 = Findline::patternboundingrectAB();
    m_imodelwith = static_cast<int>(arect1.Width());
    m_imodelheigh = static_cast<int>(arect1.Height());
}
void fastmatch::Learn_level3(Image& image)//1pyrDown  thre >10
{
    Image aimage0;
    aimage0.CopyFrom(&image);
    aimage0.ROIpyrDown(1);
    setthre(10);
    setlinegap(6);
    edgepattern(aimage0);
    modelzeroposition();
    gp_Rectangle arect1 = Findline::patternboundingrectAB();
    m_imodelwith = static_cast<int>(arect1.Width());
    m_imodelheigh = static_cast<int>(arect1.Height());
}
void fastmatch::Learn_level4(Image& image)//thre >7
{
    setthre(7);
    setlinegap(3);
    edgepattern(image);
    modelzeroposition();
    gp_Rectangle arect1 = Findline::patternboundingrectAB();
    m_imodelwith = static_cast<int>(arect1.Width());
    m_imodelheigh = static_cast<int>(arect1.Height());
}
void fastmatch::modelzeroposition()
{
    Findline::patternzeroposition();
}
void fastmatch::rotatemodelzeropositionAB()
{
    const int isize = static_cast<int>(std::min(m_models_rotate.size(), m_models_rotaterects.size()));
    for (int iz = 0; iz < isize; iz++)
    {
        gp_Rectangle arect1 = m_models_rotate[iz].boundingRectAB();
        m_models_rotate[iz].MoveAB(-static_cast<int>(arect1.TopLeft().X()), -static_cast<int>(arect1.TopLeft().Y()));
        m_models_rotaterects[iz].Move(-static_cast<int>(arect1.TopLeft().X()), -static_cast<int>(arect1.TopLeft().Y()));
    }
}
void fastmatch::rotatemodelzeroposition()
{
    const int isize = static_cast<int>(std::min(m_models_rotate.size(), m_models_rotaterects.size()));
    for (int iz = 0; iz < isize; iz++)
    {
        gp_Rectangle arect1 = m_models_rotate[iz].boundingRect();
        m_models_rotate[iz].Move(-static_cast<int>(arect1.TopLeft().X()), -static_cast<int>(arect1.TopLeft().Y()));
        m_models_rotaterects[iz].Move(-static_cast<int>(arect1.TopLeft().X()), -static_cast<int>(arect1.TopLeft().Y()));
    }
}
void fastmatch::rotatemodel05zeroposition()
{
    const int isize = static_cast<int>(std::min(m_models05_rotate.size(), m_models05_rotaterects.size()));
    for (int iz = 0; iz < isize; iz++)
    {
        gp_Rectangle arect1 = m_models05_rotate[iz].boundingRect();
        m_models05_rotate[iz].Move(-static_cast<int>(arect1.TopLeft().X()), -static_cast<int>(arect1.TopLeft().Y()));
        m_models05_rotaterects[iz].Move(-static_cast<int>(arect1.TopLeft().X()), -static_cast<int>(arect1.TopLeft().Y()));
    }
}
void fastmatch::rotatemodel025zeroposition()
{
    const int isize = static_cast<int>(std::min(m_models025_rotate.size(), m_models025_rotaterects.size()));
    for (int iz = 0; iz < isize; iz++)
    {
        gp_Rectangle arect1 = m_models025_rotate[iz].boundingRect();
        m_models025_rotate[iz].Move(-static_cast<int>(arect1.TopLeft().X()), -static_cast<int>(arect1.TopLeft().Y()));
        m_models025_rotaterects[iz].Move(-static_cast<int>(arect1.TopLeft().X()), -static_cast<int>(arect1.TopLeft().Y()));
    }
}
void fastmatch::learn_level0(void* pimage)//5pyrDown   thre >50
{
    Image* pgetimage = (Image*)pimage;
    if (pgetimage == nullptr)
        return;
    Learn_level0(*pgetimage);

}
void fastmatch::learn_level1(void* pimage)//2pyrDown   thre >30
{
    Image* pgetimage = (Image*)pimage;
    if (pgetimage == nullptr)
        return;
    Learn_level1(*pgetimage);

}
void fastmatch::learn_level2(void* pimage)//pyrDown thre >10
{
    Image* pgetimage = (Image*)pimage;
    if (pgetimage == nullptr)
        return;
    Learn_level2(*pgetimage);
}
void fastmatch::learn_level3(void* pimage)//pyrDown thre >10
{
    Image* pgetimage = (Image*)pimage;
    if (pgetimage == nullptr)
        return;
    Learn_level3(*pgetimage);
}
void fastmatch::learn_level4(void* pimage)//pyrDown thre >10
{
    Image* pgetimage = (Image*)pimage;
    if (pgetimage == nullptr)
        return;
    Learn_level4(*pgetimage);
}
void fastmatch::learn(void* pimage)
{
    Image* pgetimage = (Image*)pimage;
    if (pgetimage == nullptr)
        return;
    Learn(*pgetimage);
}
void fastmatch::savemodelfile(const char* pchar)
{
    ZeroPOS();
    Findline::savepatternfile(pchar);
}
void fastmatch::loadmodelfile(const char* pchar)
{
    Findline::loadpatternfile(pchar);
    ZeroPOS();
    gp_Rectangle arect1 = Findline::patternboundingrectAB();
    m_imodelwith = static_cast<int>(arect1.Width());
    m_imodelheigh = static_cast<int>(arect1.Height());
}
void fastmatch::loadrotatemodelfile(const char* pchar)
{
    //red(white 1) gap blue(black 0) model
    Findline::loadpatternfile(pchar);
    ZeroPOS();
    //samplemodelAB(m_imodelsamplenum);
    gp_Rectangle arect1 = Findline::patternboundingrectAB();
    m_imodelwith = static_cast<int>(arect1.Width());
    m_imodelheigh = static_cast<int>(arect1.Height());

    m_models_rotate.clear();//5 degree

    m_models_rotaterects.clear();//4 points

    int ianglecur = 0;
    PointsShape amodelpoints;
    PointsShape arectpoints;
    PointsShape brectpoints;
    PointsShape bmodelrect;
    gp_Pnt apoint(0, 0, 0);
    gp_Pnt bpoint(m_imodelwith, 0, 0);
    gp_Pnt cpoint(m_imodelwith, m_imodelheigh, 0);
    gp_Pnt dpoint(0, m_imodelheigh, 0);
    arectpoints.addpoint(apoint);
    arectpoints.addpoint(bpoint);
    arectpoints.addpoint(cpoint);
    arectpoints.addpoint(dpoint);

    for (int i = 0; i < 360; i++)
    {
        ianglecur = i;
        amodelpoints = Findline::getpattern();
        brectpoints = arectpoints;
        // QFont afont("Fixedsys", 16);
        // string astr = string("%1").arg(i);
        bmodelrect = brectpoints;
        // bmodelrect.addText(0,-8,afont,astr);
        amodelpoints.RotateAB(ianglecur);
        bmodelrect.Rotate(ianglecur);

        m_models_rotate.push_back(amodelpoints);
        m_models_rotaterects.push_back(bmodelrect);
    }

    rotatemodelzeropositionAB();
}
void fastmatch::loadrotate05modelfile(const char* pchar)
{
    //red(white 1) gap blue(black 0) model
    Findline::loadpatternfile(pchar);
    ZeroPOS();
    gp_Rectangle arect1 = Findline::patternboundingrect();
    m_imodelwith = static_cast<int>(arect1.Width());
    m_imodelheigh = static_cast<int>(arect1.Height());

    m_models05_rotate.clear();//5 degree

    m_models05_rotaterects.clear();//4 points

    double danglecur = 0;
    PointsShape amodelpoints;
    PointsShape arectpoints;
    PointsShape brectpoints;
    PointsShape bmodelrect;
    gp_Pnt apoint(0, 0, 0);
    gp_Pnt bpoint(m_imodelwith, 0, 0);
    gp_Pnt cpoint(m_imodelwith, m_imodelheigh, 0);
    gp_Pnt dpoint(0, m_imodelheigh, 0);
    arectpoints.addpoint(apoint);
    arectpoints.addpoint(bpoint);
    arectpoints.addpoint(cpoint);
    arectpoints.addpoint(dpoint);

    for (int i = 0; i < 720; i++)
    {
        danglecur = i * 0.5;
        amodelpoints = Findline::getpattern();
        brectpoints = arectpoints;
        bmodelrect = brectpoints;
        amodelpoints.Rotate(danglecur);
        bmodelrect.Rotate(danglecur);

        m_models05_rotate.push_back(amodelpoints);
        m_models05_rotaterects.push_back(bmodelrect);
    }

    rotatemodel05zeroposition();
}
void fastmatch::loadrotate025modelfile(const char* pchar)
{
    //red(white 1) gap blue(black 0) model
    Findline::loadpatternfile(pchar);
    ZeroPOS();
    gp_Rectangle arect1 = Findline::patternboundingrect();
    m_imodelwith = static_cast<int>(arect1.Width());
    m_imodelheigh = static_cast<int>(arect1.Height());

    m_models025_rotate.clear();//5 degree

    m_models025_rotaterects.clear();//4 points

    double danglecur = 0;
    PointsShape amodelpoints;
    PointsShape arectpoints;
    PointsShape brectpoints;
    PointsShape bmodelrect;
    gp_Pnt apoint(0, 0, 0);
    gp_Pnt bpoint(m_imodelwith, 0, 0);
    gp_Pnt cpoint(m_imodelwith, m_imodelheigh, 0);
    gp_Pnt dpoint(0, m_imodelheigh, 0);
    arectpoints.addpoint(apoint);
    arectpoints.addpoint(bpoint);
    arectpoints.addpoint(cpoint);
    arectpoints.addpoint(dpoint);

    for (int i = 0; i < 1440; i++)
    {
        danglecur = i * 0.25;
        amodelpoints = Findline::getpattern();
        brectpoints = arectpoints;
        bmodelrect = brectpoints;
        amodelpoints.Rotate(danglecur);
        bmodelrect.Rotate(danglecur);

        m_models025_rotate.push_back(amodelpoints);
        m_models025_rotaterects.push_back(bmodelrect);
    }

    rotatemodel025zeroposition();
}
void fastmatch::ABtoShape(std::vector<cv::Point2f>& points)
{
    return Findline::ABtoShape(points);
}
int fastmatch::ABpatternsize()
{
    return Findline::ABpatternsize();
}
std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);

    while (getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }

    return tokens;
}
void fastmatch::loadcalibration(const char* pchar)
{ 
    clear();
    FILE* rf = nullptr;
    fopen_s(&rf, pchar, "rb");
    if (nullptr == rf)
        return;
    fseek(rf, 0, SEEK_END);
    int filesize = ftell(rf);
    char* pcharget = new char[filesize + 10];
    memset(pcharget, 0, filesize + 10);
    rewind(rf);
    fread((char*)(pcharget), filesize, 1, rf);
    string astr = pcharget;
    vector<string> strcalnumlist = split(astr, '|');
    /*
        QStringList strnumlist = qstr.split(",");
        for(int i=0;i<strnumlist.size()-3;i++)
        {
            qreal ix = strnumlist.at(i).toInt();
            i++;
            qreal iy = strnumlist.at(i).toInt();
            addpointa(ix,iy);
            i++;
            ix = strnumlist.at(i).toInt();
            i++;
            iy = strnumlist.at(i).toInt();
            addpointb(ix,iy);
        } 
    */
    delete[]pcharget;
    fclose(rf);

}
void fastmatch::savecalibration(const char* pchar)
{
    (void)pchar;
    /*
      int isize = m_path.elementCount();
      if(isize<=0)
          return;
      FILE    *rf = fopen(pchar, "w+");
     if ( rf == nullptr)
         return;
     rewind(rf);
     QPainterPath::Element aele = m_path.elementAt(0);
      int ix =aele.x;
      int iy =aele.y;
     fprintf(rf,"%d,",ix);
     fprintf(rf,"%d",iy);

     for(int i=1;i<isize;i++)
     {
         aele = m_path.elementAt(i);
         ix =aele.x;
         iy =aele.y;
         fprintf(rf,",");
         fprintf(rf,"%d,",ix);
         fprintf(rf,"%d",iy);
     } 
     fclose(rf);
  */
}
void fastmatch::setrotateangle(double dangle)
{
    m_danglegap = FastMatchPositiveFiniteOr(dangle, m_danglegap);
}
void fastmatch::setrotateanglescale(double dangle1, double dangle2)
{
    m_dangle_add = FastMatchFiniteOr(dangle2, m_dangle_add);
    m_dangle_mud = FastMatchFiniteOr(dangle1, m_dangle_mud);
}
void fastmatch::clearmodels_l12()
{
    m_models_l12.clear();
}
void fastmatch::addmodels_l12(const char* pchar)
{
    Findline::loadpatternfile(pchar);
    m_models_l12.push_back(Findline::getpattern());
}
void fastmatch::clearmodels_l36()
{
    m_models_l36.clear();
}
void fastmatch::addmodels_l36(const char* pchar)
{
    Findline::loadpatternfile(pchar);
    m_models_l36.push_back(Findline::getpattern());
}
void fastmatch::clearmodels_l72()
{
    m_models_l72.clear();
}
void fastmatch::addmodels_l72(const char* pchar)
{
    Findline::loadpatternfile(pchar);
    m_models_l72.push_back(Findline::getpattern());
}
void fastmatch::clearmodels_rotate()
{
    m_models_rotate.clear();
}
void fastmatch::addmodels_rotate(const char* pchar)
{
    Findline::loadpatternfile(pchar);
    m_models_rotate.push_back(Findline::getpattern());
}
void fastmatch::setcurmodels(int inum)
{
    if (inum >= 0 && inum < static_cast<int>(m_models_l12.size()))
        Findline::setpattern(m_models_l12[inum]);
}
void fastmatch::setcurimagemodels(int inum)
{
    m_pgrid->SetUnit(12, 12);
    m_pgrid->UnitGrid();

    m_pgrid->ModelGridMethod_Gauss();
    m_imagefastmodel = m_pgrid->getfastmodel();

    if (inum >= 0 && inum < static_cast<int>(m_imagefastmodels_l12.size()))
        m_imagefastmodels_l12[inum] = m_imagefastmodel;
}
void fastmatch::modelstocurrent_l72(int i)
{
    if (i >= 0 && i < static_cast<int>(m_models_l72.size()))
        Findline::setpattern(m_models_l72[i]);
}
void fastmatch::modelstocurrent_l36(int i)
{
    if (i >= 0 && i < static_cast<int>(m_models_l36.size()))
        Findline::setpattern(m_models_l36[i]);
}
void fastmatch::modelstocurrent_l12(int i)
{
    if (i >= 0 && i < static_cast<int>(m_models_l12.size()))
        Findline::setpattern(m_models_l12[i]);
}
void fastmatch::modelstocurrent_l3(int i)
{
    if (i >= 0 && i < static_cast<int>(m_models_l3.size()))
        Findline::setpattern(m_models_l3[i]);
}
void fastmatch::modelstocurrent_l6(int i)
{
    if (i >= 0 && i < static_cast<int>(m_models_l6.size()))
        Findline::setpattern(m_models_l6[i]);
}
void fastmatch::patternrootgrid(double itype, double drate, double ilevel)
{
    Findline::patternrootgrid(itype, drate, ilevel);
}
void fastmatch::patterntranform(int igap, int itype, int isgap, int iline)
{
    Findline::patterntranform(igap, itype, isgap, iline);
    Findline::patternzeroposition();
}
void fastmatch::patterngap2gap(int inewgap)
{
    Findline::patterngap2gap(inewgap);
}
void fastmatch::patternABgap2gap(double dnewgaprate)
{
    Findline::patternABgap2gap(dnewgaprate);
}
void fastmatch::patternABsample(int irate)
{
    Findline::patternABsample(irate);
}
void fastmatch::pattern2org()
{
    Findline::pattern2org(); 
}
void fastmatch::org2pattern()
{
    Findline::org2pattern();
}
void fastmatch::patternzoom(double dx, double dy, double igap, double itype)
{
    Findline::patternzoom(dx, dy, igap, itype);
    Findline::patternzeroposition();
}
void fastmatch::modelrotate(double dangle)
{
    Findline::patternrotate(dangle);
}
void fastmatch::modelzoom(double dx, double dy)
{
    Findline::modelzoom(dx, dy);
}
void fastmatch::setmodelwh(int iw, int ih)
{
    gp_Rectangle rectf = Findline::patternboundingrect();
    iw = FastMatchPositiveInt(iw);
    ih = FastMatchPositiveInt(ih);
    int iorgw = FastMatchPositiveInt(static_cast<int>(rectf.Width()));
    int iorgh = FastMatchPositiveInt(static_cast<int>(rectf.Height()));
    double dw = (iw * 1.0) / (iorgw * 1.0);
    double dh = (ih * 1.0) / (iorgh * 1.0);
    modelzoom(dw, dh);
}
void fastmatch::setmatchrect(int ix, int iy, int iw, int ih)
{
    ix = FastMatchNonNegativeInt(ix);
    iy = FastMatchNonNegativeInt(iy);
    iw = FastMatchPositiveInt(iw);
    ih = FastMatchPositiveInt(ih);
    m_matchrect = gp_Rectangle(gp_Pnt(ix, iy,0), gp_Pnt(ix + iw, iy + ih,0));
    if (m_matchrects.size() <= 0)
    {
        gp_Rectangle arect(gp_Pnt(ix, iy, 0), gp_Pnt(ix + iw, iy + ih, 0));
        m_matchrects.addrect(arect);
    }
    else
        m_matchrects.setrect(0,ix,iy,iw,ih);
}

void fastmatch::setmatchrectnum(int inum)
{
    if (inum <= 0)
        inum = 1;
    m_matchrects.clear();
    if (inum > 0)
        for (int i = 0; i < inum; i++)
        {
            gp_Rectangle arect(gp_Pnt(100, 100,0), gp_Pnt(200, 200,0));
            std::ostringstream stream;
            stream << i ; 
            string str = stream.str();
            m_matchrects.addrect(arect, str);
        }
}
gp_Rectangle& fastmatch::getmatchrect()
{
    return m_matchrect;
}
RectsShape& fastmatch::getmatchrects()
{
    return m_matchrects;
}
gp_Rectangle fastmatch::getresultrect(int inum) const
{
    if (inum < 0 || inum >= m_resultrects.size())
        return gp_Rectangle(gp_Pnt(0, 0, 0), gp_Pnt(0, 0, 0));
    gp_Rectangle arect0 = m_resultrects.getrect(inum);
    return arect0;
}

gp_Rectangle fastmatch::getresolvedresultrect(int inum) const
{
    gp_Rectangle arect0 = getresultrect(inum);
    gp_Pnt origin = rect().TopLeft();
    const int rectw = FastMatchPositiveInt(static_cast<int>(arect0.Width()));
    const int recth = FastMatchPositiveInt(static_cast<int>(arect0.Height()));
    return gp_Rectangle(
        gp_Pnt(arect0.TopLeft().X() + origin.X(), arect0.TopLeft().Y() + origin.Y(), 0),
        rectw,
        recth);
}
void fastmatch::setmultimatchrect(int inum, int ix, int iy, int iw, int ih)
{
    if (inum >= 0 && inum < m_matchrects.size())
    {
        const int rectx = FastMatchNonNegativeInt(ix);
        const int recty = FastMatchNonNegativeInt(iy);
        m_matchrects.setrect(inum, rectx, recty, FastMatchPositiveInt(iw), FastMatchPositiveInt(ih));
    }
}
void fastmatch::resultclear()
{
    m_iminfindnum = -1;
    m_resultpoints.clear();
    m_resultnums.clear();
    m_rawmatch_probe_count = 0;
    m_rawmatch_threshold_hit_count = 0;
    m_resulttolist_call_count = 0;
    m_resultcandidate_insert_count = 0;
    m_resultcandidate_replace_count = 0;
    m_resultcandidate_reject_count = 0;
    m_rawthresholdhitpoints.clear();
    m_rawthresholdhitscores.clear();
  
}
void fastmatch::resulttolist(gp_Pnt& apoint, int inum)
{
    ++m_resulttolist_call_count;
    if (inum > m_iminfindnum)
    {
        const int keep_limit = std::max(1, m_imaxmatchnum);
        const int merge_half_w = std::max(1, keep_limit > 1
            ? std::max(m_stepgapx * 2, m_imodelwith / 8)
            : m_imodelwith / 2);
        const int merge_half_h = std::max(1, keep_limit > 1
            ? std::max(m_stepgapy * 2, m_imodelheigh / 8)
            : m_imodelheigh / 2);
        const int replace_margin = keep_limit > 1 ? 1 : 0;
        //have same area
        const size_t shared_count = std::min(m_resultnums.size(), m_resultpoints.size());
        if (m_resultnums.size() != shared_count)
            m_resultnums.resize(shared_count);
        if (m_resultpoints.size() != shared_count)
            m_resultpoints.resize(shared_count);

        for (int i = 0; i < static_cast<int>(shared_count); ++i)
        {
            gp_Pnt apoint0 = m_resultpoints.at(i);
            int ivalue = m_resultnums.at(i);
            const int idx = std::abs(static_cast<int>(apoint0.X() - apoint.X()));
            const int idy = std::abs(static_cast<int>(apoint0.Y() - apoint.Y()));
            if (idx <= merge_half_w
                && idy <= merge_half_h)
            {
                if (ivalue + replace_margin < inum)
                {
                    m_resultpoints[i] = apoint;
                    m_resultnums[i] = inum;
                    ++m_resultcandidate_replace_count;
                    goto NextRun01;
                }
                else
                {
                    ++m_resultcandidate_reject_count;
                    return;
                }
            }
        }
        if(0)//!!!!!!
        if (m_iminfindnum != -1
            && m_resultnums.size() > m_iminfindnum)
        {
            const int result_index = m_resultnums[m_iminfindnum];
            removeAt(m_resultnums, result_index);
              removePntAt(m_resultpoints, result_index);
 
          }
 
          m_resultpoints.push_back(apoint);
          m_resultnums.push_back(inum);
          ++m_resultcandidate_insert_count;

          NormalizeMatchCandidates(
              m_resultnums,
              m_resultpoints,
              keep_limit,
              m_iminfindnum,
              m_iminpointkey);
      }
      else
          return;


    //
    //

NextRun01:
    NormalizeMatchCandidates(
        m_resultnums,
        m_resultpoints,
        std::max(1, m_imaxmatchnum),
        m_iminfindnum,
        m_iminpointkey);
}
void fastmatch::resultsort()
{
    NormalizeMatchCandidates(
        m_resultnums,
        m_resultpoints,
        std::max(1, m_imaxmatchnum),
        m_iminfindnum,
        m_iminpointkey);
}
void fastmatch::rotateresultsortfilter(int ifdx, int ifdy, int itype)
{
    FilterRotateResultsByDistance(
        m_rotateresults,
        m_rotatereslutpoints,
        m_rotatereslutangles,
        m_rotateshaperesults,
        ifdx,
        ifdy,
        itype,
        false);
}
void fastmatch::rotateresultsortfilterA(int ifdx, int ifdy, int itype)
{
    FilterRotateResultsByDistance(
        m_rotateresults,
        m_rotatereslutpoints,
        m_rotatereslutangles,
        m_rotateshaperesults,
        ifdx,
        ifdy,
        itype,
        true);
}
 
int fastmatch::rotateresultsize()
{
    return static_cast<int>(RotateResultSharedCount(
        m_rotateresults,
        m_rotatereslutpoints,
        m_rotatereslutangles,
        m_rotateshaperesults));
}
void fastmatch::rotateresultsort()
{
    const size_t shared_count = RotateResultSharedCount(
        m_rotateresults,
        m_rotatereslutpoints,
        m_rotatereslutangles,
        m_rotateshaperesults);
    if (shared_count == 0)
        return;
    m_rotateresults.resize(shared_count);
    m_rotatereslutpoints.resize(shared_count);
    m_rotatereslutangles.resize(shared_count);
    m_rotateshaperesults.resize(shared_count);
    const int isize = static_cast<int>(shared_count);
    double dprevalue = m_rotateresults.at(0);
    gp_Pnt prepoint = m_rotatereslutpoints.at(0);
    double predangle = m_rotatereslutangles.at(0);
    PointsShape preshape = m_rotateshaperesults.at(0);
    int iprenum = 0;
    double dnxtvalue = m_rotateresults.at(0);
    gp_Pnt nxtpoint = m_rotatereslutpoints.at(0);
    double nxtdangle = m_rotatereslutangles.at(0);
    PointsShape nxtshape = m_rotateshaperesults.at(0);
    int inxtnum = 0;

    if (isize > 1)
    for (int i = 1; i < isize; ++i)
    {
        dprevalue = m_rotateresults.at(i - 1);
        prepoint = m_rotatereslutpoints.at(i - 1);
        predangle = m_rotatereslutangles.at(i - 1);
        preshape = m_rotateshaperesults.at(i - 1);
        iprenum = i - 1;
        dnxtvalue = m_rotateresults.at(i);
        nxtpoint = m_rotatereslutpoints.at(i);
        nxtdangle = m_rotatereslutangles.at(i);
        nxtshape = m_rotateshaperesults.at(i);
        inxtnum = i;
        if (dprevalue < dnxtvalue)
        {
            m_rotateresults[iprenum] = dnxtvalue;
            m_rotatereslutpoints[iprenum] = nxtpoint;
            m_rotatereslutangles[iprenum] = nxtdangle;
            m_rotateshaperesults[iprenum] = nxtshape;

            m_rotateresults[inxtnum] = dprevalue;
            m_rotatereslutpoints[inxtnum] = prepoint;
            m_rotatereslutangles[inxtnum] = predangle;
            m_rotateshaperesults[inxtnum] = preshape;

            for (int j = i - 1; j > 0; j--)
            {
                dprevalue = m_rotateresults.at(j - 1);
                prepoint = m_rotatereslutpoints.at(j - 1);
                predangle = m_rotatereslutangles.at(j - 1);
                preshape = m_rotateshaperesults.at(j - 1);

                iprenum = j - 1;

                dnxtvalue = m_rotateresults.at(j);
                nxtpoint = m_rotatereslutpoints.at(j);
                nxtdangle = m_rotatereslutangles.at(j);
                nxtshape = m_rotateshaperesults.at(j);

                inxtnum = j;

                if (dprevalue < dnxtvalue)
                {
                    m_rotateresults[iprenum] = dnxtvalue;
                    m_rotatereslutpoints[iprenum] = nxtpoint;
                    m_rotatereslutangles[iprenum] = nxtdangle;
                    m_rotateshaperesults[iprenum] = nxtshape;

                    m_rotateresults[inxtnum] = dprevalue;
                    m_rotatereslutpoints[inxtnum] = prepoint;
                    m_rotatereslutangles[inxtnum] = predangle;
                    m_rotateshaperesults[inxtnum] = preshape;
                }
            }
        }
    }
}

double fastmatch::getrotateresultx()
{
    if (!m_rotatereslutpoints.empty())
    {
        gp_Pnt apoint = m_rotatereslutpoints[0];
        return apoint.X();
    }
    return -9999;
}
double fastmatch::getrotateresulty()
{
    if (!m_rotatereslutpoints.empty())
    {
        gp_Pnt apoint = m_rotatereslutpoints[0];
        return apoint.Y();
    }
    return -9999;
}
double fastmatch::getrotateresulta()
{
    if (!m_rotatereslutangles.empty())
    {
        double drotatedangle = m_rotatereslutangles[0];
        if (drotatedangle > 270)
        {
            drotatedangle = drotatedangle - 360;
        }
        return drotatedangle;
    }
    return -999;
}
double fastmatch::getrotateresultscore()
{
    if (!m_rotateresults.empty())
    {
        double drotateresult = m_rotateresults[0];
        return drotateresult;
    }
    return 0;
}
double fastmatch::getrotateresultscoreA(int inum)
{
    if (inum >= 0 && inum < static_cast<int>(m_rotateresults.size()))
    {
        double drotateresult = m_rotateresults[inum];
        return drotateresult;
    }
    return 0;
}

void fastmatch::clusterclear()
{
    m_clusters.clear();
}
void fastmatch::resultcluster(int ixgap, int iygap, int ianglegap)
{ 
    const size_t shared_count = std::min(m_rotatereslutpoints.size(), m_rotatereslutangles.size());
    if (shared_count == 0)
    {
        return;
    }
    const int ipointsize = static_cast<int>(shared_count);
    gp_Pnt apoint;
    bool binsert = false;
    const int cluster_count = static_cast<int>(m_clusters.size());
    if (0 == cluster_count)
    {
        Cluster newcluster;
        newcluster.push_back(0);
        m_clusters.push_back(newcluster);

    }
    for (int ir = 1; ir < ipointsize; ir++)
    {
        binsert = false;
        apoint = m_rotatereslutpoints[ir];
        const int ix0 = static_cast<int>(apoint.X());
        const int iy0 = static_cast<int>(apoint.Y());
        double dangle0 = m_rotatereslutangles[ir];
        const int current_cluster_count = static_cast<int>(m_clusters.size());
        for (int ic = 0; ic < current_cluster_count; ic++)
        {
            const int cluster_size = static_cast<int>(m_clusters[ic].size());
            for (int cluster_index = 0; cluster_index < cluster_size; cluster_index++)
            {
                int ipos = m_clusters[ic][cluster_index];
                if (ipos < 0 || ipos >= ipointsize)
                {
                    continue;
                }
                gp_Pnt bpoint = m_rotatereslutpoints[ipos];
                const int ix1 = static_cast<int>(bpoint.X());
                const int iy1 = static_cast<int>(bpoint.Y());
                double dangle1 = m_rotatereslutangles[ipos];

                int ixgap0 = ix1 - ix0 > 0 ? ix1 - ix0 : ix0 - ix1;
                int iygap0 = iy1 - iy0 > 0 ? iy1 - iy0 : iy0 - iy1;
                double danglegap0 = dangle1 - dangle0 > 0 ? dangle1 - dangle0 : dangle0 - dangle1;
                if (ixgap0 < ixgap
                    && iygap0 < iygap
                    && danglegap0 < ianglegap)
                {
                    //鎻掑叆鍫嗘爤
                    m_clusters[ic].push_back(ir);
                    binsert = true;
                    const int cluster_size = static_cast<int>(m_clusters[ic].size());
                    for (int sorted_index = 0; sorted_index < cluster_size - 1; ++sorted_index)
                    {
                        const int ipos0 = m_clusters[ic][sorted_index];
                        const int ipos1 = m_clusters[ic][sorted_index + 1];
                        if (ipos0 < 0 || ipos0 >= ipointsize || ipos1 < 0 || ipos1 >= ipointsize)
                        {
                            continue;
                        }

                        const double sort_angle0 = m_rotatereslutangles[ipos0];
                        const double sort_angle1 = m_rotatereslutangles[ipos1];
                        if (sort_angle0 < sort_angle1)
                        {
                            m_clusters[ic][sorted_index] = ipos1;
                            m_clusters[ic][sorted_index + 1] = ipos0;
                        }
                    }

                }
            }
        }
        if (false == binsert)
        {
            Cluster newcluster;
            newcluster.push_back(ir);
            m_clusters.push_back(newcluster);
        } 
        /*
        for(int ic=1;ic<=iclustersize;ic++)
        {
            if(false==binsert)
            {
                int icsize = m_clusters[ic].size();
                for(int in=0;in<icsize;in++)
                {
                    if(false==binsert)
                    {
                        int ipos=m_clusters[ic][in];
                        gp_Pnt bpoint = m_rotatereslutpoints[ipos];
                        int ix1 =bpoint.X();
                        int iy1 =bpoint.Y();
                        double dangle1 = m_rotatereslutangles[ipos];
                        int ixgap0 = ix1-ix0>0?ix1-ix0:ix0-ix1;
                        int iygap0 = iy1-iy0>0?iy1-iy0:iy0-iy1;
                        double danglegap0 = dangle1-dangle0>0?dangle1-dangle0:dangle0-dangle1;
                        if(ixgap0<ixgap
                          &&iygap0<iygap
                          &&danglegap0<ianglegap)
                        {
                            m_clusters[ic].push_back(ir);
                            binsert=true;
                        }
                    }
                }
            }
            if(true==binsert)
            {
                int icsize = m_clusters[ic].size();
                for(int in=0;in<icsize-1;in++)
                {
                        int ipos0=m_clusters[ic][in];
                        int ipos1=m_clusters[ic][in+1];
                        double dangle0 = m_rotatereslutangles[ipos0];
                        double dangle1 = m_rotatereslutangles[ipos1];
                        if(dangle0<dangle1)
                        {
                            m_clusters[ic][in]=ipos1;
                            m_clusters[ic][in+1]=ipos0;
                        }
                }
            }
        }
        if(false==binsert)
        {
            QCluster newcluster;
            newcluster.push_back(ir);
            m_clusters.push_back(newcluster);
        }*/
    } 
}
void fastmatch::Distfilter()
{
#if defined USE_AI
    gp_Path& pathA = Findline::getpatternpathA();
    gp_Path& pathB = Findline::getpatternpathB();
    size_t numPoints = pathA.getpoints().size();
    if (numPoints>0)
    {
        arma::mat points(2, numPoints);
        for (size_t i = 0; i < numPoints; ++i)
        {
            points(0, i) = pathA.getpoints()[i].X(); // 绗竴琛屾槸 x 鍧愭爣
            points(1, i) = pathA.getpoints()[i].Y(); // 绗簩琛屾槸 y 鍧愭爣
        }
        pathA.Clear();
        // 鑷€傚簲杩囨护
        auto filteredIndices = mlpackclass::AdaptiveFilterWithCoincidenceHandling_(points);
        for (auto idx : filteredIndices)
            pathA.AddPoint(gp_Pnt(points(0, idx), points(1, idx), 0));
    }
    numPoints = pathB.getpoints().size();
    if (numPoints > 0)
    {
        arma::mat points(2, numPoints);
        for (size_t i = 0; i < numPoints; ++i)
        {
            points(0, i) = pathB.getpoints()[i].X(); // 绗竴琛屾槸 x 鍧愭爣
            points(1, i) = pathB.getpoints()[i].Y(); // 绗簩琛屾槸 y 鍧愭爣
        }
        pathB.Clear();
        // 鑷€傚簲杩囨护
        auto filteredIndices = mlpackclass::AdaptiveFilterWithCoincidenceHandling_(points);
        for (auto idx : filteredIndices)
            pathB.AddPoint(gp_Pnt(points(0, idx), points(1, idx), 0));

    } 
#endif
}

void fastmatch::MatchAB(Image& image)
{
    m_matchimage = &image;
    resultclear();
    gp_Path& pathA = Findline::getpatternpathA();
    gp_Path& pathB = Findline::getpatternpathB();

   // Distfilter();
    MatchSampleAB(image, pathA, pathB);
}
void fastmatch::MatchABMore(Image& image)
{
    m_matchimage = &image;
    resultclear();
    gp_Path& pathA = Findline::getpatternpathA();
    gp_Path& pathB = Findline::getpatternpathB();

    // Distfilter();
    MatchSampleABMore(image, pathA, pathB);
}
void fastmatch::match(void* pimage)
{
    Image* pgetimage = (Image*)pimage;
    if (pgetimage == nullptr)
        return;

    MatchAB(*pgetimage);
}
 
void fastmatch::matchmore(void* pimage)
{
    Image* pgetimage = (Image*)pimage;
    if (pgetimage == nullptr)
        return;

    MatchABMore(*pgetimage);
}
void fastmatch::loadfastimagemodel(const char* pfilename)
{
    m_pgrid->loadmapmodel(pfilename);
    m_imagefastmodel = m_pgrid->getfastmodel();
}
vector<int>* fastmatch::getcurimagemodel()
{
    return &m_imagefastmodel;
}
void fastmatch::imagemodesclear_l12()
{
    m_imagefastmodels_l12.clear();
}
void fastmatch::addimagemodels_l12(const char* pfilename)
{
    m_pgrid->loadmapmodel(pfilename);

    // if(1)//test
    // {
    //     m_pgrid->SetUnit(12,12);
    //     m_pgrid->UnitGrid();
    // }
    // else
    {
        m_pgrid->ZeroModel();
        m_pgrid->ReGrid(12, 12);
    }
    m_pgrid->Grid2PattenModel(Findline::getconparegap());
    m_models_l12.push_back(m_pgrid->getpatmodel());

    easyobj aeobj = m_pgrid->ModelGridMethod_ObjectA();
    m_easyobjectmodels_l12.push_back(aeobj);

    m_pgrid->ModelGridMethod_Gauss();

    m_imagefastmodel = m_pgrid->getfastmodel();
    m_imagefastmodels_l12.push_back(m_imagefastmodel);

}
void fastmatch::imagemodesclear_l36()
{
    m_imagefastmodels_l36.clear();
}
void fastmatch::addimagemodels_l36(const char* pfilename)
{
    m_pgrid->loadmapmodel(pfilename);
    m_pgrid->ZeroModel();
    m_pgrid->ReGrid(36, 36);
    if (0)//test
    {
        m_pgrid->SetUnit(36, 36);
        m_pgrid->UnitGrid();
    }

    m_pgrid->Grid2PattenModel(Findline::getconparegap());
    m_models_l36.push_back(m_pgrid->getpatmodel());

    easyobj aeobj = m_pgrid->ModelGridMethod_ObjectA();
    m_easyobjectmodels_l36.push_back(aeobj);

    m_pgrid->ModelGridMethod_Gauss();

    m_imagefastmodel = m_pgrid->getfastmodel();
    m_imagefastmodels_l36.push_back(m_imagefastmodel);

}
void fastmatch::imagemodesclear_l72()
{
    m_imagefastmodels_l72.clear();
}
void fastmatch::addimagemodels_l72(const char* pfilename)
{
    m_pgrid->loadmapmodel(pfilename);
    m_pgrid->ZeroModel();
    m_pgrid->ReGrid(72, 72);
    if (0)//test
    {
        m_pgrid->SetUnit(72, 72);
        m_pgrid->UnitGrid();
    }

    m_pgrid->Grid2PattenModel_org(Findline::getconparegap());
    m_models_l72.push_back(m_pgrid->getpatmodel());

    easyobj aeobj = m_pgrid->ModelGridMethod_ObjectA();
    m_easyobjectmodels_l72.push_back(aeobj);

    m_pgrid->ModelGridMethod_Gauss();

    m_imagefastmodel = m_pgrid->getfastmodel();
    m_imagefastmodels_l72.push_back(m_imagefastmodel);

}

Grid* fastmatch::getgrid()
{
    return m_pgrid;
}
bool fastmatch::modelcompare(vector<int>& modela, vector<int>& modelb)
{
    const int isize0 = static_cast<int>(modela.size());
    const int isize1 = static_cast<int>(modelb.size());
    if (isize0 != isize1)
        return false;
    for (int i = 0; i < isize0; i++)
    {
        if (modela[i] != modelb[i])
            return false;
    }
    return true;
}
map<int, int >& fastmatch::getlevel3_6map()
{
    return m_mapl3_l6;
}
map<int, int >& fastmatch::getlevel6_12map()
{
    return m_mapl6_l12;
}
map<int, int >& fastmatch::getlevel12_36map()
{
    return m_mapl12_l36;
}
map<int, int >& fastmatch::getlevel36_72map()
{
    return m_mapl36_l72;
}
void fastmatch::clearmodel()
{
    m_easyobjectmodels_l12.clear();//
    m_models_l12.clear();
    m_imagefastmodels_l12.clear();
    m_mapl12_l36.clear();

    m_easyobjectmodels_l6.clear();
    m_models_l6.clear();
    m_imagefastmodels_l6.clear();
    m_mapl6_l12.clear();

    m_easyobjectmodels_l3.clear();
    m_models_l3.clear();
    m_imagefastmodels_l3.clear();
    m_mapl3_l6.clear();
}
void fastmatch::list_duplicatesmodel_l12()
{
    m_duplicates_list_l12.clear();
    const int isize = static_cast<int>(m_imagefastmodels_l12.size());

    for (int i = 0; i < isize; i++)
    {
        m_duplicates_list_l12.push_back(0);
    }
    int iduplicatesnum = 1;
    for (int i = 0; i < isize; i++)
    {
        if (0 == m_duplicates_list_l12[i])
        {
            for (int j = 0; j < isize; j++)
            {
                if (i != j)
                {
                    if (modelcompare(m_imagefastmodels_l12[i], m_imagefastmodels_l12[j]))
                    {
                        int isetvalue = 0;
                        if (m_duplicates_list_l12[i] == 0 && m_duplicates_list_l12[j] == 0)
                        {
                            m_duplicates_list_l12[i] = iduplicatesnum;
                            m_duplicates_list_l12[j] = iduplicatesnum;
                            iduplicatesnum = iduplicatesnum + 1;
                        }
                        else if (m_duplicates_list_l12[i] != 0)
                        {
                            isetvalue = m_duplicates_list_l12[i];
                            m_duplicates_list_l12[j] = isetvalue;
                        }
                        else if (m_duplicates_list_l12[j] != 0)
                        {
                            isetvalue = m_duplicates_list_l12[j];
                            m_duplicates_list_l12[i] = isetvalue;
                        }
                    }
                }
            }
        }
    }

}
void fastmatch::list_duplicatesmodel_l36()
{
    m_duplicates_list_l36.clear();
    const int isize = static_cast<int>(m_imagefastmodels_l36.size());

    for (int i = 0; i < isize; i++)
    {
        m_duplicates_list_l36.push_back(0);
    }
    int iduplicatesnum = 1;
    for (int i = 0; i < isize; i++)
    {
        if (0 == m_duplicates_list_l36[i])
        {
            for (int j = 0; j < isize; j++)
            {
                if (i != j)
                {
                    if (modelcompare(m_imagefastmodels_l36[i], m_imagefastmodels_l36[j]))
                    {
                        int isetvalue = 0;
                        if (m_duplicates_list_l36[i] == 0 && m_duplicates_list_l36[j] == 0)
                        {
                            m_duplicates_list_l36[i] = iduplicatesnum;
                            m_duplicates_list_l36[j] = iduplicatesnum;
                            iduplicatesnum = iduplicatesnum + 1;
                        }
                        else if (m_duplicates_list_l36[i] != 0)
                        {
                            isetvalue = m_duplicates_list_l36[i];
                            m_duplicates_list_l36[j] = isetvalue;
                        }
                        else if (m_duplicates_list_l36[j] != 0)
                        {
                            isetvalue = m_duplicates_list_l36[j];
                            m_duplicates_list_l36[i] = isetvalue;
                        }
                    }
                }
            }
        }
    }

}
void fastmatch::list_duplicatesmodel_l72()
{
    m_duplicates_list_l72.clear();
    const int isize = static_cast<int>(m_imagefastmodels_l72.size());

    for (int i = 0; i < isize; i++)
    {
        m_duplicates_list_l72.push_back(0);
    }
    int iduplicatesnum = 1;
    for (int i = 0; i < isize; i++)
    {
        if (0 == m_duplicates_list_l72[i])
        {
            for (int j = 0; j < isize; j++)
            {
                if (i != j)
                {
                    if (modelcompare(m_imagefastmodels_l72[i], m_imagefastmodels_l72[j]))
                    {
                        int isetvalue = 0;
                        if (m_duplicates_list_l72[i] == 0 && m_duplicates_list_l72[j] == 0)
                        {
                            m_duplicates_list_l72[i] = iduplicatesnum;
                            m_duplicates_list_l72[j] = iduplicatesnum;
                            iduplicatesnum = iduplicatesnum + 1;
                        }
                        else if (m_duplicates_list_l72[i] != 0)
                        {
                            isetvalue = m_duplicates_list_l72[i];
                            m_duplicates_list_l72[j] = isetvalue;
                        }
                        else if (m_duplicates_list_l72[j] != 0)
                        {
                            isetvalue = m_duplicates_list_l72[j];
                            m_duplicates_list_l72[i] = isetvalue;
                        }
                    }
                }
            }
        }
    }

}

vector<int>& fastmatch::getduplicateslist_l36()
{
    return m_duplicates_list_l36;
}
vector<int>& fastmatch::getduplicateslist_l12()
{
    return m_duplicates_list_l12;
}

void fastmatch::modelmethod(int itype)
{
    //m_pgrid->ReGrid(72,72);
    m_pgrid->SetFastModel(m_imagefastmodel);
    m_pgrid->ReSetModelGrid();
    //
    switch (itype) {
    case 0:
        m_pgrid->ReSetModelGrid();
        break;
    case 1:
        m_pgrid->ModelGridMethod_Gauss();
        break;
    case 2:
        m_pgrid->ModelGridMethod_Object();
        break;
    case 3:
        m_pgrid->ModelGridMethod_ObjectA();
        break;
    case 4:
        m_pgrid->ModelGridMethod_Inside();
        break;
    case 5:
        m_pgrid->ModelGridMethod_Outside();
        break;
    default:
        m_pgrid->ReSetModelGrid();
        break;
    }
    m_imagefastmodel = m_pgrid->getfastmodel();

}
void fastmatch::levelmodels_l72tol36()
{
    //model level 12 load from file so no need to clear in here


    const int isize = static_cast<int>(m_imagefastmodels_l72.size());

    for (int i = 0; i < isize; i++)
    {
        m_pgrid->SetModelWH(72, 72);
        m_pgrid->SetFastModel(m_imagefastmodels_l72[i]);
        m_pgrid->ReSetModelGrid();
        m_pgrid->ZeroModel();
        m_pgrid->GridZoom(36, 36);
        m_pgrid->SetUnit(36, 36);
        //
        m_pgrid->Grid2PattenModel(Findline::getconparegap());//m_icomparegap
        // m_pgrid->ZeroModel();

        easyobj aeobj = m_pgrid->ModelGridMethod_ObjectA();

        m_pgrid->ModelGridMethod_Gauss();

        m_imagefastmodel = m_pgrid->getfastmodel();

        const int imsize = static_cast<int>(m_imagefastmodels_l36.size());
        int isame = -1;
        for (int im = 0; im < imsize; im++)
        {
            if (modelcompare(m_imagefastmodels_l36[im], m_imagefastmodel))
            {
                m_mapl36_l72[i] = im;
                isame = 1;
                break;
            }
        }
        if (-1 == isame)
        {
            m_imagefastmodels_l36.push_back(m_imagefastmodel);
            m_models_l36.push_back(m_pgrid->getpatmodel());
            m_easyobjectmodels_l36.push_back(aeobj);
            m_mapl36_l72[i] = static_cast<int>(m_imagefastmodels_l36.size()) - 1;
        }
    }
}

void fastmatch::levelmodels_l36tol12()
{
    //model level 12 load from file so no need to clear in here


    const int isize = static_cast<int>(m_imagefastmodels_l36.size());

    for (int i = 0; i < isize; i++)
    {
        m_pgrid->SetModelWH(36, 36);
        m_pgrid->SetFastModel(m_imagefastmodels_l36[i]);
        m_pgrid->ReSetModelGrid();
        m_pgrid->ZeroModel();
        m_pgrid->GridZoom(12, 12);
        m_pgrid->SetUnit(12, 12);
        //
        m_pgrid->Grid2PattenModel(Findline::getconparegap());//m_icomparegap
        // m_pgrid->ZeroModel();

        easyobj aeobj = m_pgrid->ModelGridMethod_ObjectA();

        m_pgrid->ModelGridMethod_Gauss();

        m_imagefastmodel = m_pgrid->getfastmodel();

        const int imsize = static_cast<int>(m_imagefastmodels_l12.size());
        int isame = -1;
        for (int im = 0; im < imsize; im++)
        {
            if (modelcompare(m_imagefastmodels_l12[im], m_imagefastmodel))
            {
                m_mapl12_l36[i] = im;
                isame = 1;
                break;
            }
        }
        if (-1 == isame)
        {
            m_imagefastmodels_l12.push_back(m_imagefastmodel);
            m_models_l12.push_back(m_pgrid->getpatmodel());
            m_easyobjectmodels_l12.push_back(aeobj);
            m_mapl12_l36[i] = static_cast<int>(m_imagefastmodels_l12.size()) - 1;

        }
    }
}

void fastmatch::levelmodels_l12tol6()
{

    //    m_imagefastmodels_l12_l2.clear();

    const int isize = static_cast<int>(m_imagefastmodels_l12.size());
    //    for(int i=0;i<isize;i++)
    //    {
    //        m_pgrid->SetModelWH(12,12);
    //        m_pgrid->SetFastModel(m_imagefastmodels_l12[i]);
    //        m_pgrid->ReSetModelGrid();
    //        m_pgrid->ZeroModel();
    //        m_pgrid->SetUnit(12,12);
    //        m_imagefastmodel = m_pgrid->getfastmodel();
    //        m_imagefastmodels_l12_l2.push_back(m_imagefastmodel);
    //    }


    for (int i = 0; i < isize; i++)
    {
        m_pgrid->SetModelWH(12, 12);
        m_pgrid->SetFastModel(m_imagefastmodels_l12[i]);

        m_pgrid->ReSetModelGrid();
        m_pgrid->ZeroModel();
        m_pgrid->GridZoom(6, 6);
        m_pgrid->SetUnit(6, 6);
        //
        m_pgrid->Grid2PattenModel(Findline::getconparegap());//m_icomparegap
        //m_pgrid->ZeroModel();

        easyobj aeobj = m_pgrid->ModelGridMethod_ObjectA();

        m_pgrid->ModelGridMethod_Gauss();

        m_imagefastmodel = m_pgrid->getfastmodel();

        const int imsize = static_cast<int>(m_imagefastmodels_l6.size());
        int isame = -1;
        for (int im = 0; im < imsize; im++)
        {
            if (modelcompare(m_imagefastmodels_l6[im], m_imagefastmodel))
            {
                m_mapl6_l12[i] = im;
                isame = 1;
                break;
            }
        }
        if (-1 == isame)
        {
            m_imagefastmodels_l6.push_back(m_imagefastmodel);
            m_models_l6.push_back(m_pgrid->getpatmodel());
            m_easyobjectmodels_l6.push_back(aeobj);
            m_mapl6_l12[i] = static_cast<int>(m_imagefastmodels_l6.size()) - 1;
        }
    }
}

void fastmatch::levelmodels_l6tol3()
{
    const int isize = static_cast<int>(m_imagefastmodels_l6.size());
    for (int i = 0; i < isize; i++)
    {
        m_pgrid->SetModelWH(6, 6);
        m_pgrid->SetFastModel(m_imagefastmodels_l6[i]);

        m_pgrid->ReSetModelGrid();
        m_pgrid->ZeroModel();
        m_pgrid->GridZoom(3, 3);
        m_pgrid->SetUnit(3, 3);
        //
        m_pgrid->Grid2PattenModel(Findline::getconparegap());//m_icomparegap
        //m_pgrid->ZeroModel();

        easyobj aeobj = m_pgrid->ModelGridMethod_ObjectA();

        m_pgrid->ModelGridMethod_Gauss();

        m_imagefastmodel = m_pgrid->getfastmodel();

        const int imsize = static_cast<int>(m_imagefastmodels_l3.size());
        int isame = -1;
        for (int im = 0; im < imsize; im++)
        {
            if (modelcompare(m_imagefastmodels_l3[im], m_imagefastmodel))
            {
                m_mapl3_l6[i] = im;
                isame = 1;
                break;
            }
        }
        if (-1 == isame)
        {
            m_models_l3.push_back(m_pgrid->getpatmodel());
            m_imagefastmodels_l3.push_back(m_imagefastmodel);
            m_easyobjectmodels_l3.push_back(aeobj);
            m_mapl3_l6[i] = static_cast<int>(m_imagefastmodels_l3.size()) - 1;
        }

    }
}
void fastmatch::savelevel0_l1()
{
    const int isize0 = static_cast<int>(m_models_l3.size());
    for (int i = 0; i < isize0; i++)
    { 
        std::ostringstream stream;
        stream << "./model/3x3/_" << i  << ".pat";
        string strfilename = stream.str();
        m_models_l3[i].save(strfilename.c_str());
    }
    const int isize1 = static_cast<int>(m_models_l6.size());
    for (int i = 0; i < isize1; i++)
    { 
        std::ostringstream stream;
        stream << "./model/6x6/_" << i << ".pat";
        string strfilename = stream.str();
        m_models_l6[i].save(strfilename.c_str());
    }

    int isize = static_cast<int>(m_imagefastmodels_l3.size());
    for (int i = 0; i < isize; i++)
    {
        m_pgrid->SetModelWH(3, 3);
        m_pgrid->SetFastModel(m_imagefastmodels_l3[i]);
        m_pgrid->ReSetModelGrid(); 
        std::ostringstream stream;
        stream << "./model/3x3/_" << i << ".imp";
        string strfilename = stream.str();
        m_pgrid->savemapmodel(strfilename.c_str());
    }

    isize = static_cast<int>(m_imagefastmodels_l6.size());
    for (int i = 0; i < isize; i++)
    {
        m_pgrid->SetModelWH(6, 6);
        m_pgrid->SetFastModel(m_imagefastmodels_l6[i]);
        m_pgrid->ReSetModelGrid();
        std::ostringstream stream;
        stream << "./model/6x6/_" << i << ".imp";
        string strfilename = stream.str(); 
        m_pgrid->savemapmodel(strfilename.c_str());
    }

    isize = static_cast<int>(m_imagefastmodels_l12.size());
    for (int i = 0; i < isize; i++)
    {
        m_pgrid->SetModelWH(12, 12);
        m_pgrid->SetFastModel(m_imagefastmodels_l12[i]);
        m_pgrid->ReSetModelGrid();
        m_pgrid->Grid2PattenModel(2);
        std::ostringstream stream;
        stream << "./model/12x12/_" << i << ".pat";
        string strfilename = stream.str(); 
        m_pgrid->savemodelfile(strfilename.c_str());

        std::ostringstream stream0;
        stream0 << "./model/12x12/_" << i << ".imp";
        strfilename = stream0.str();
        m_pgrid->savemapmodel(strfilename.c_str());
    }
}
void fastmatch::imagemodelstocurrent_l72(int i)
{
    m_pgrid->SetModelWH(72, 72);

    if (i >= 0 && i < static_cast<int>(m_imagefastmodels_l72.size()))
        m_imagefastmodel = m_imagefastmodels_l72[i];
}
void fastmatch::imagemodelstocurrent_l36(int i)
{
    m_pgrid->SetModelWH(36, 36);

    if (i >= 0 && i < static_cast<int>(m_imagefastmodels_l36.size()))
        m_imagefastmodel = m_imagefastmodels_l36[i];
}
void fastmatch::imagemodelstocurrent_l12(int i)
{
    m_pgrid->SetModelWH(12, 12);

    if (i >= 0 && i < static_cast<int>(m_imagefastmodels_l12.size()))
        m_imagefastmodel = m_imagefastmodels_l12[i];
}

void fastmatch::imagemodelstocurrent_l3(int i)
{
    m_pgrid->SetModelWH(3, 3);
    if (i >= 0 && i < static_cast<int>(m_imagefastmodels_l3.size()))
        m_imagefastmodel = m_imagefastmodels_l3[i];
}
void fastmatch::imagemodelstocurrent_l6(int i)
{
    m_pgrid->SetModelWH(6, 6);

    if (i >= 0 && i < static_cast<int>(m_imagefastmodels_l6.size()))
        m_imagefastmodel = m_imagefastmodels_l6[i];
}

int fastmatch::imagefastmodelsize(int ilevel)
{
    switch (ilevel)
    {
    case 0:
        return static_cast<int>(m_imagefastmodels_l3.size());
    case 1:
        return static_cast<int>(m_imagefastmodels_l6.size());
    case 2:
        return static_cast<int>(m_imagefastmodels_l12.size());
    case 3:
        return static_cast<int>(m_imagefastmodels_l36.size());
    case 4:
        return static_cast<int>(m_imagefastmodels_l72.size());

    default:
        return static_cast<int>(m_imagefastmodels_l12.size());
    }
    return 0;
}

void fastmatch::objectmodelstocurrent(int i)
{
    if (i >= 0 && i < static_cast<int>(m_easyobjectmodels_l12.size()))
        m_easyobject = m_easyobjectmodels_l12[i];
}
void fastmatch::savefastimagemodel(const char* pfilename)
{
    m_pgrid->ReSetModelGrid();

    m_pgrid->savemapmodel(pfilename);

}
void fastmatch::savefastimagepatmodel(const char* pfilename)
{
    m_pgrid->Grid2PattenModel(Findline::getconparegap());
    m_pgrid->savemodelfile(pfilename);
}
void fastmatch::savematchroi(const char* pfilename)
{
    if (m_matchimage == nullptr)
        return;
    SaveMatchROI(*m_matchimage, pfilename);
}
void fastmatch::savematchimagemodel(const char* pfilename)
{
    if (g_pmodelimage == nullptr)
        return;
    g_pmodelimage->SaveROI(pfilename);
}
void fastmatch::SaveMatchROI(Image& image, const char* pfilename)
{
    const int isize = m_resultrects.size();
    if (isize <= 0)
        return;
    gp_Rectangle arect = m_resultrects.getrect(isize - 1);
    const int roiw = FastMatchPositiveInt(static_cast<int>(arect.Width()));
    const int roih = FastMatchPositiveInt(static_cast<int>(arect.Height()));
 
    image.setroi(
        static_cast<int>(arect.TopLeft().X()) - m_imatchoffset,
        static_cast<int>(arect.TopLeft().Y()),
        roiw,
        roih);
    image.SaveROI(pfilename);
}
void fastmatch::imagelearn(int ithre1, int iandor)
{
    if (m_matchimage == nullptr)
        return;
    MatchImageLearn(*m_matchimage, ithre1, iandor);
}
void fastmatch::imagelearnmass(int ithre1, int iandor, int igridwh)
{
    if (m_matchimage == nullptr)
        return;
    MatchImageLearnMass(*m_matchimage, ithre1, iandor, igridwh);
}
void fastmatch::imagelearncheck(int iimagetype, int iandor, int igridwh)
{
    if (m_matchimage == nullptr)
        return;
    MatchImageCheck(*m_matchimage, iimagetype, iandor, igridwh);
}

void fastmatch::imagelearnex(int ithre1, int iandor, int igrid)
{
    if (m_matchimage == nullptr)
        return;
    MatchImageLearnEx(*m_matchimage, ithre1, iandor, igrid);
}
void fastmatch::MatchImageLearn(Image& aimage, int ithre1, int iandor)
{
    if (g_pmodelimage == nullptr)
        return;
    (void)iandor;
    const int isize = m_resultrects.size();
    if (isize <= 0)
        return;

    gp_Rectangle arect0 = m_resultrects.getrect(isize - 1);
    //gp_Rectangle arect = m_pgrid->CentRect(arect0);
    m_pgrid->SetUnit(12, 12);
    gp_Rectangle arect = m_pgrid->CentRect_Condition(arect0, 3);
    const int roiw = FastMatchPositiveInt(static_cast<int>(arect.Width()));
    const int roih = FastMatchPositiveInt(static_cast<int>(arect.Height()));

    aimage.setroi(
        static_cast<int>(arect.TopLeft().X()) - m_imatchoffset,
        static_cast<int>(arect.TopLeft().Y()),
        roiw,
        roih);
    g_pmodelimage->setroi(0, 0, roiw, roih);
    aimage.SetMode(3);
    aimage.ROItoROI(*g_pmodelimage);
    g_pmodelimage->ROIEasyThre(ithre1); 
    m_pgrid->ROIImagetoModel(*g_pmodelimage);
    m_pgrid->SetUnit(12, 12);
    m_pgrid->UnitGrid();

    m_pgrid->ModelGridMethod_Gauss();
    m_imagefastmodel = m_pgrid->getfastmodel();
}
int fastmatch::GetRectGridLevel(int irectw)
{
    int igrid = 12;
    if (irectw <= 12)
    {
        igrid = 12;
    }
    else if (irectw > 12 && irectw <= 36)
    {
        igrid = 36;
    }
    else if (irectw > 36 && irectw <= 72)
    {
        igrid = 72;
    }
    else if (irectw > 72 && irectw <= 144)
    {
        igrid = 144;
    }

    return igrid;
}
void fastmatch::MatchImageLearnEx(Image& aimage, int ithre1, int iandor, int igrid)
{
    if (g_pmodelimage == nullptr)
        return;
    (void)iandor;
    const int isize = m_resultrects.size();
    if (isize <= 0)
        return;

    gp_Rectangle arect0 = m_resultrects.getrect(isize - 1);

    // int iw = arect0.Width();
    // int ih = arect0.Height();
    // int imaxlen = iw>ih?iw:ih;
    // {
    m_pgrid->SetUnit(igrid, igrid);//(12,12);
    gp_Rectangle arect = m_pgrid->CentRect_Condition(arect0, 3);
    const int roiw = FastMatchPositiveInt(static_cast<int>(arect.Width()));
    const int roih = FastMatchPositiveInt(static_cast<int>(arect.Height()));
    aimage.setroi(
        static_cast<int>(arect.TopLeft().X()) - m_imatchoffset,
        static_cast<int>(arect.TopLeft().Y()),
        roiw,
        roih);
    g_pmodelimage->setroi(0, 0, roiw, roih);
    aimage.SetMode(3);
    aimage.ROItoROI(*g_pmodelimage); 
    g_pmodelimage->ROIEasyThre(ithre1);
    m_pgrid->ROIImagetoModel(*g_pmodelimage);
    m_pgrid->SetUnit(igrid, igrid);//(12,12);
    m_pgrid->UnitGrid();

    m_pgrid->ModelGridMethod_Gauss();
    m_imagefastmodel = m_pgrid->getfastmodel();
    //}
}
void fastmatch::MatchImageLearnMass(Image& aimage, int ithre1, int iandor, int igrid)
{
    if (g_pmodelimage == nullptr)
        return;
    (void)iandor;
    const int isize = m_resultrects.size();
    if (isize <= 0)
        return;
    m_pgrid->setgrid(10, 10, igrid, igrid, 10, 10);
    m_pgrid->SetModelWH(igrid, igrid);
    /*   gp_Rectangle arect0 = m_resultrects.getrect(isize - 1);
       //gp_Rectangle arect = m_pgrid->CentRect(arect0);
       m_pgrid->SetUnit(igridwh,igridwh);
       gp_Rectangle arect = m_pgrid->CentRect_Condition(arect0,3);

       aimage.setroi(arect.X()-m_imatchoffset,arect.Y(),arect.Width(),arect.Height());
       g_pmodelimage->setroi(0,0,arect.Width(),arect.Height());
       aimage.SetMode(3);
       aimage.ROItoROI(g_pmodelimage);
       g_pmodelimage->ROIColorTable();
       g_pmodelimage->ROIColorTableBlur(0,ithre1);
       g_pmodelimage->ROIColorTableEasyThre(iandor);
       m_pgrid->ROIImagetoModel(*g_pmodelimage);
       m_pgrid->SetUnit(igridwh,igridwh);
       m_pgrid->UnitGrid();

       m_pgrid->ModelGridMethod_Gauss();
       m_imagefastmodel = m_pgrid->getfastmodel();
       */

    gp_Rectangle arect0 = m_resultrects.getrect(isize - 1);

    m_pgrid->SetUnit(igrid, igrid);//(12,12);
    gp_Rectangle arect = m_pgrid->CentRect_Condition(arect0, 3);
    const int roiw = FastMatchPositiveInt(static_cast<int>(arect.Width()));
    const int roih = FastMatchPositiveInt(static_cast<int>(arect.Height()));
    aimage.setroi(
        static_cast<int>(arect.TopLeft().X()) - m_imatchoffset,
        static_cast<int>(arect.TopLeft().Y()),
        roiw,
        roih);
    g_pmodelimage->setroi(0, 0, roiw, roih);
    aimage.SetMode(3);//linux 0 ,win 3
    aimage.ROItoROI(*g_pmodelimage); 
    g_pmodelimage->ROIEasyThre(ithre1);
    m_pgrid->ROIImagetoModel(*g_pmodelimage);
    m_pgrid->SetUnit(igrid, igrid);//(12,12);
    m_pgrid->UnitGrid();

    m_pgrid->ModelGridMethod_Gauss();
    m_imagefastmodel = m_pgrid->getfastmodel();

}
void fastmatch::MatchImageCheck(Image& aimage, int iimagetype, int iandor, int igrid)
{
    if (g_pmodelimage == nullptr)
        return;
    (void)iandor;
    int isize = m_resultrects.size();
    if (isize <= 0)
        return;

    m_pgrid->setgrid(10, 10, igrid, igrid, 10, 10);
    m_pgrid->SetModelWH(igrid, igrid);

    gp_Rectangle arect0 = m_resultrects.getrect(isize - 1);

    m_pgrid->SetUnit(igrid, igrid);

    //zero image roi
    g_pmodelimage->setroi(0, 0, igrid, igrid);
    g_pmodelimage->colorizeROI(0,0,0);

    const int roiw = FastMatchPositiveInt(static_cast<int>(arect0.Width()));
    const int roih = FastMatchPositiveInt(static_cast<int>(arect0.Height()));
    aimage.setroi(
        static_cast<int>(arect0.TopLeft().X()) - m_imatchoffset,
        static_cast<int>(arect0.TopLeft().Y()),
        roiw,
        roih);
    g_pmodelimage->setroi(0, 0, roiw, roih);
    aimage.SetMode(iimagetype);//linux 0 ,win 30
    aimage.ROItoROI(*g_pmodelimage);

    g_pmodelimage->setroi(0, 0, igrid, igrid);
    m_pgrid->ROIImagetoModel(*g_pmodelimage);

    //    m_pgrid->ROIImagetoModel_gray(*g_pmodelimage);
    //    m_pgrid->SetUnit(igrid,igrid);//(12,12);
    //    m_pgrid->EdgeGrid();
    //    amatch.setthre(21);
    //    amatch.setcompgap(1);
    //    amatch.setmethod(0);
    //    amatch.setlinegap(3);
    //    amatch.setwhgap(1,1);
    //    amatch.setlinesample(0.004);
    //    amatch.setfilter(21,0,10000);
    //    amatch.learn(aimage3);
    //    amatch.savemodel("ready.pat");
    //here !!!
    //    !!!
    //    m_pgrid->ModelGridMethod_Gauss();
    //    m_imagefastmodel = m_pgrid->getfastmodel();

    m_imagefastmatchlist = m_pgrid->getfastmodel();

    isize = static_cast<int>(m_imagefastmodel.size());
    int ingsize = 0;
    int ioksize = 0;
    for (int i = 0; i < isize; i++)
    {
        int ia = m_imagefastmatchlist[i];
        int io = m_imagefastmodel[i];
        //
        if (ia != io && io == 1)
        {
            if (ia > 0)
                ingsize = ingsize + ia;
            else
                ingsize = ingsize - ia;
        }
        else if (ia == 1 && io != 1)
        {
            if (io > 0)
                ingsize = ingsize + io;
            else
                ingsize = ingsize - io;
        }
        else if (io == ia
            && io == 1)
            ioksize++;
    }
    m_imagemodelresult_OK = ioksize;
    m_imagemodelresult_NG = ingsize;
}

void fastmatch::imagematch(int ithre1, int iandor, int igrid, int ineedthre)
{
    if (m_matchimage == nullptr)
        return;
    MatchImageMatch(*m_matchimage, ithre1, iandor, igrid, ineedthre);
}

void fastmatch::imagematch_grid(int ithre1, int iandor, int igrid)
{
    if (m_matchimage == nullptr)
        return;
    MatchImageMatch(*m_matchimage, ithre1, iandor, igrid);
}

void fastmatch::imagematchex(int igrid)
{
    if (m_matchimage == nullptr)
        return;
    MatchImageExMatch(*m_matchimage, igrid);
}

void fastmatch::MatchImageMatch(Image& aimage, int ithre1, int iandor, int igrid, int ineedthre)
{
    if (g_pmodelimage == nullptr)
        return;
    (void)iandor;
    int isize = m_resultrects.size();
    if (isize <= 0)
        return;
    gp_Rectangle arect0 = m_resultrects.getrect(isize - 1);
    //gp_Rectangle arect = m_pgrid->CentRect(arect0);
    //gp_Rectangle arect = m_pgrid->CentRect_Condition(arect0,3);
    const int roiw = FastMatchPositiveInt(static_cast<int>(arect0.Width()));
    const int roih = FastMatchPositiveInt(static_cast<int>(arect0.Height()));
    aimage.setroi(
        static_cast<int>(arect0.TopLeft().X()) - m_imatchoffset,
        static_cast<int>(arect0.TopLeft().Y()),
        roiw,
        roih);
    g_pmodelimage->setroi(0, 0, roiw, roih);
    aimage.SetMode(3);
    aimage.ROItoROI(*g_pmodelimage);

    if (1 == ineedthre)
    {
        g_pmodelimage->ROIEasyThre(ithre1); 
    }
    m_pgrid->ROIImagetoModel(*g_pmodelimage);
    m_pgrid->ZeroModel();
    m_pgrid->ReGrid(igrid, igrid);

    if (0)//test1
    {
        m_pgrid->SetUnit(igrid, igrid);
        m_pgrid->UnitGrid();
    }
    m_easyobject = m_pgrid->ModelGridMethod_ObjectA();

    m_pgrid->ModelGridMethod_Gauss();
    m_imagefastmatchlist = m_pgrid->getfastmodel();

    isize = static_cast<int>(m_imagefastmodel.size());
    int ingsize = 0;
    int ioksize = 0;
    for (int i = 0; i < isize; i++)
    {
        int ia = m_imagefastmatchlist[i];
        int io = m_imagefastmodel[i];

        //
        if (ia != io && io == 1)
        {
            if (ia > 0)
                ingsize = ingsize + ia;
            else
                ingsize = ingsize - ia;
        }
        else if (ia == 1 && io != 1)
        {
            if (io > 0)
                ingsize = ingsize + io;
            else
                ingsize = ingsize - io;
        }
        else if (io == ia
            && io == 1)
            ioksize++;
    }
    m_imagemodelresult_OK = ioksize;
    m_imagemodelresult_NG = ingsize;
}

void fastmatch::MatchImageExMatch(Image& aimage, int igrid)
{
    if (g_pmodelimage == nullptr)
        return;
    int isize = m_resultrects.size();
    if (isize <= 0)
        return;
    gp_Rectangle arect0 = m_resultrects.getrect(isize - 1);
    //gp_Rectangle arect = m_pgrid->CentRect(arect0);
    m_pgrid->SetUnit(igrid, igrid);
    gp_Rectangle arect(gp_Pnt(0,0,0),gp_Pnt(0,0,0));
    switch (igrid) {
    case 3:
        arect = gp_Rectangle(gp_Pnt(arect0.TopLeft().X() + 1, arect0.TopLeft().Y() + 1,0), 3, 3);
        break;
    case 6:
        arect = gp_Rectangle(gp_Pnt(arect0.TopLeft().X() + 1, arect0.TopLeft().Y() + 1,0), 6, 6);
        break;
    case 12:
        arect = m_pgrid->CentRect_Condition(arect0, 3);
        break;
    default:
        break;
    }
    const int roiw = FastMatchPositiveInt(static_cast<int>(arect.Width()));
    const int roih = FastMatchPositiveInt(static_cast<int>(arect.Height()));
    aimage.setroi(
        static_cast<int>(arect.TopLeft().X()) - m_imatchoffset,
        static_cast<int>(arect.TopLeft().Y()),
        roiw,
        roih);
    g_pmodelimage->setroi(0, 0, roiw, roih);
    aimage.SetMode(3);
    aimage.ROItoROI(*g_pmodelimage);

    //g_pmodelimage->ROIEasyThre(ithre1);
    m_pgrid->ROIImagetoModel(*g_pmodelimage);

    m_easyobject = m_pgrid->ModelGridMethod_ObjectA();

    m_pgrid->ModelGridMethod_Gauss();
    m_imagefastmatchlist = m_pgrid->getfastmodel();

    isize = std::min(
        static_cast<int>(m_imagefastmodel.size()),
        static_cast<int>(m_imagefastmatchlist.size()));
    int ingsize = 0;
    int ioksize = 0;
    for (int i = 0; i < isize; i++)
    {
        int ia = m_imagefastmatchlist[i];
        int io = m_imagefastmodel[i];

        //
        if (ia != io && io == 1)
        {
            if (ia > 0)
                ingsize = ingsize + ia;
            else
                ingsize = ingsize - ia;
        }
        else if (ia == 1 && io != 1)
        {
            if (io > 0)
                ingsize = ingsize + io;
            else
                ingsize = ingsize - io;
        }
        else if (io == ia
            && io == 1)
            ioksize++;
    }
    m_imagemodelresult_OK = ioksize;
    m_imagemodelresult_NG = ingsize;
}
void fastmatch::MatchGrid(Grid* pgrid)
{
    m_pgrid->GridFastModel(pgrid);

    m_easyobject = m_pgrid->ModelGridMethod_ObjectA();

    m_pgrid->ModelGridMethod_Gauss();
    m_imagefastmatchlist = m_pgrid->getfastmodel();

    const int isize = std::min(
        static_cast<int>(m_imagefastmodel.size()),
        static_cast<int>(m_imagefastmatchlist.size()));
    int ingsize = 0;
    int ioksize = 0;
    for (int i = 0; i < isize; i++)
    {
        int ia = m_imagefastmatchlist[i];
        int io = m_imagefastmodel[i];

        //
        if (ia != io && io == 1)
        {
            if (ia > 0)
                ingsize = ingsize + ia;
            else
                ingsize = ingsize - ia;
        }
        else if (ia == 1 && io != 1)
        {
            if (io > 0)
                ingsize = ingsize + io;
            else
                ingsize = ingsize - io;
        }
        else if (io == ia
            && io == 1)
            ioksize++;
    }
    m_imagemodelresult_OK = ioksize;
    m_imagemodelresult_NG = ingsize;
}

double fastmatch::getimagemodelreslut()
{
    if (0 == m_imagemodelresult_NG
        && 0 != m_imagemodelresult_OK)
        return 100;
    if (0 == m_imagemodelresult_OK
        && 0 != m_imagemodelresult_NG)
        return 0;
    if (0 == m_imagemodelresult_OK
        && 0 == m_imagemodelresult_NG)
        return 0;
    if (m_imagemodelresult_OK < 0
        || m_imagemodelresult_NG <= 0)
        return 0;

    const int itotalsize = static_cast<int>(m_imagefastmodel.size());
    (void)itotalsize;
    double dresult = (m_imagemodelresult_OK * 1.0) / (1.0 * m_imagemodelresult_NG);
    return dresult;
}

double fastmatch::getimagemodelreslut_check_1()
{
    if (0 == m_imagemodelresult_NG
        && 0 != m_imagemodelresult_OK)
        return 100;
    if (0 == m_imagemodelresult_OK
        && 0 != m_imagemodelresult_NG)
        return 0;
    if (0 == m_imagemodelresult_OK
        && 0 == m_imagemodelresult_NG)
        return 0;
    if (m_imagemodelresult_OK < 0
        || m_imagemodelresult_NG <= 0)
        return 0;

    const int itotalsize = static_cast<int>(m_imagefastmodel.size());
    (void)itotalsize;
    double dresult = (m_imagemodelresult_OK * 1.0) / (1.0 * m_imagemodelresult_NG);
    return dresult;
}
void fastmatch::imagemodelcomparegrid(int itype)
{
    const int imatchsize = static_cast<int>(m_imagefastmatchlist.size());
    if (imatchsize <= 0)
        return;
    const int isize = static_cast<int>(m_imagefastmodel.size());
    if (imatchsize != isize)
        return;

    switch (itype)
    {
    case 0:
        for (int i = 0; i < isize; i++)
        {
            int ia = m_imagefastmatchlist[i];
            int io = m_imagefastmodel[i];

            if (io == 1 && ia != io)
            {
                m_pgrid->setfastlistvalue(i, -1);
            }
            else if (ia == 1 && io != 1)
            {
                m_pgrid->setfastlistvalue(i, -2);
            }
            else if (io == 1 && ia != 1)
            {
                m_pgrid->setfastlistvalue(i, -3);
            }
            else if (io == 1 && io == ia)
                m_pgrid->setfastlistvalue(i, 1);
            else
                m_pgrid->setfastlistvalue(i, 0);
        }

        break;
    case 1:
        for (int i = 0; i < isize; i++)
        {
            int ia = m_imagefastmatchlist[i];
            int io = m_imagefastmodel[i];

            if (io == 0 && ia != io)
                m_pgrid->setfastlistvalue(i, -1);
            else if (ia == 1 && io != 0)
                m_pgrid->setfastlistvalue(i, 0);
            else if (ia == 1 && io == 0)
                m_pgrid->setfastlistvalue(i, -1);
            else if (ia == 0 && io != 0)
            {
                if (io == -1)
                    m_pgrid->setfastlistvalue(i, 0);
                else
                    m_pgrid->setfastlistvalue(i, io);
            }
            else
                m_pgrid->setfastlistvalue(i, 0);
        }

        break;
    case 2:

        break;


    default:

        break;
    }


}
void fastmatch::imagemodelcompareshow(int itype)
{
    const int imatchsize = static_cast<int>(m_imagefastmatchlist.size());
    if (imatchsize <= 0)
        return;
    const int isize = static_cast<int>(m_imagefastmodel.size());
    if (imatchsize != isize)
        return;
    switch (itype)
    {
    case 0:
        for (int i = 0; i < isize; i++)
        {
            int ia = m_imagefastmatchlist[i];
            int io = m_imagefastmodel[i];

            if (io == 1 && ia != io)
            {
                m_pgrid->setfastlistvalue(i, -1);
            }
            else if (ia == 1 && io != 1)
            {
                m_pgrid->setfastlistvalue(i, -2);
            }
            else if (io == 1 && ia != 1)
            {
                m_pgrid->setfastlistvalue(i, -3);
            }
            else if (io == 1 && io == ia)
                m_pgrid->setfastlistvalue(i, 1);
            else
                m_pgrid->setfastlistvalue(i, 0);
        }

        break;
    case 1:
        for (int i = 0; i < isize; i++)
        {
            int ia = m_imagefastmatchlist[i];
            int io = m_imagefastmodel[i];

            if (io == 0 && ia != io)
                m_pgrid->setfastlistvalue(i, -1);
            else if (ia == 1 && io != 0)
                m_pgrid->setfastlistvalue(i, 0);
            else if (ia == 1 && io == 0)
                m_pgrid->setfastlistvalue(i, -1);
            else if (ia == 0 && io != 0)
            {
                if (io == -1)
                    m_pgrid->setfastlistvalue(i, 0);
                else
                    m_pgrid->setfastlistvalue(i, io);
            }
            else
                m_pgrid->setfastlistvalue(i, 0);
        }

        break;
    case 2:

        break;


    default:

        break;
    }
}
double fastmatch::imagegridresult(int itype)
{
    switch (itype) {
    case 0:

        break;
    case 1:

        break;


    default:

        break;
    }
    return 0;
}
void fastmatch::imagemodelshow()
{
    m_pgrid->SetFastModel(m_imagefastmodel);
    const int isize = static_cast<int>(m_imagefastmodel.size());
    if (isize <= 0)
        return;
    for (int i = 0; i < isize; i++)
    {
        int io = m_imagefastmodel[i];
        m_pgrid->setfastlistvalue(i, io);
    }
}
void fastmatch::matchstepgap(int ix, int iy)
{
    m_stepgapx = FastMatchPositiveInt(ix);
    m_stepgapy = FastMatchPositiveInt(iy);
}

void fastmatch::imagematchshow()
{
    m_pgrid->SetFastModel(m_imagefastmodel);
    const int imatchsize = static_cast<int>(m_imagefastmatchlist.size());
    if (imatchsize <= 0)
        return;
    const int isize = static_cast<int>(m_imagefastmodel.size());
    if (isize != imatchsize)
        return;
    for (int i = 0; i < isize; i++)
    {
        int io = m_imagefastmatchlist[i];
        m_pgrid->setfastlistvalue(i, io);
    }
}
void fastmatch::setminscore(double dminscore)
{
    m_dminscore = FastMatchUnitScore(dminscore);
}
void fastmatch::MatchSample(Image& image, gp_Path& path)
{
    int ithre = m_imatchthre;
    int icurmodule = ImageManager::GetCurMode();
    Image* pimage = ImageManager::GetTransferImage(icurmodule);
    (void)pimage;
    int ix0 = static_cast<int>(m_matchrect.TopLeft().X());
    int iy0 = static_cast<int>(m_matchrect.TopLeft().Y());
    int ix1 = static_cast<int>(m_matchrect.TopLeft().X() + m_matchrect.Width());
    int iy1 = static_cast<int>(m_matchrect.TopLeft().Y() + m_matchrect.Height());

    if (image.getWidth() <= ix1
        || image.getHeight() <= iy1)
        return;//error process
    m_iminfindnum = -1;
    const int icount = static_cast<int>(path.ElementCount());
    cv::Vec3b pixel0, pixel1;
    int icalnum = 0;
    int icalng = 0;

    gp_Pnt aele;

    int igapx = m_stepgapx;
    int igapy = m_stepgapy;
    gp_Rectangle arect1 = path.boundingRect();
    iy1 = iy1 - static_cast<int>(arect1.Height());
    ix1 = ix1 - static_cast<int>(arect1.Width());
    int ix = 0;
    int iy = 0;
    int iw = static_cast<int>(Findline::patternboundingrect().Width());
    int ih = static_cast<int>(Findline::patternboundingrect().Height());
    int itotalsize = static_cast<int>(Findline::getpattern().size());

    m_dminscore = FastMatchUnitScore(m_dminscore, 0.4);
    const int iminfindngnum = static_cast<int>((1 - m_dminscore) * itotalsize / 2);
    const int iminfindoknum = static_cast<int>(m_dminscore * itotalsize / 2);



    for (iy = iy0; iy < iy1;)
    {
        for (ix = ix0; ix < ix1;)
        {
            int imovx = ix;
            int imovy = iy;

            icalnum = 0;
            icalng = 0;
            for (int i = 0; i < icount - 1; i++)
            {
                aele = path.ElementAt(i);
                pixel0 = image.pixel(
                    static_cast<int>(aele.X() + imovx),
                    static_cast<int>(aele.Y() + imovy));
                i++;
                aele = path.ElementAt(i);
                pixel1 = image.pixel(
                    static_cast<int>(aele.X() + imovx),
                    static_cast<int>(aele.Y() + imovy));

                if (0 == m_iB2W)
                {
                    int ir = Red(pixel0) - Red(pixel1);
                    int ig = Green(pixel0) - Green(pixel1);
                    int ib = Blue(pixel0) - Blue(pixel1);
                    if (ir > ithre || ig > ithre || ib > ithre)
                    {
                        icalnum++;
                    }
                    else
                    {
                        icalng++;
                        if (icalng > iminfindngnum)
                            goto NextStep_1;
                    }
                }
                else if (1 == m_iB2W)
                {
                    int ir = Red(pixel1) - Red(pixel0);
                    int ig = Green(pixel1) - Green(pixel0);
                    int ib = Blue(pixel1) - Blue(pixel0);
                    if (ir > ithre || ig > ithre || ib > ithre)
                    {
                        icalnum++;
                    }
                    else
                    {
                        icalng++;

                        if (icalng > iminfindngnum)
                            goto NextStep_1;
                    }
                }
            }

        NextStep_1:
            ix += igapx;
            if (icalnum > m_iminfindnum
                && icalnum > iminfindoknum)
            {
                gp_Pnt apoint(imovx, imovy, 0);
                if (m_rawthresholdhitpoints.size() < 32)
                {
                    m_rawthresholdhitpoints.push_back(apoint);
                    m_rawthresholdhitscores.push_back(icalnum);
                }
                resulttolist(apoint, icalnum);
            }

        }
        iy += igapy;
    }

    resultsort();
    m_resultrects.clear();
    const int icountresult = static_cast<int>(m_resultnums.size());
    for (int i = 0; i < icountresult; i++)
    {
        int ivalue = m_resultnums.at(i);
        gp_Pnt apoint = m_resultpoints.at(i);
        double dpercent = (2.0 * ivalue) / (1.0 * itotalsize);
        gp_Rectangle arect(
            gp_Pnt(apoint.X(), apoint.Y(), 0),
            gp_Pnt(static_cast<double>(iw), static_cast<double>(ih), 0));
        std::ostringstream stream;
        stream << dpercent;
        std::string astr = stream.str();
        m_resultrects.addrect(arect, astr);
    }

}

void fastmatch::MatchSampleAB(Image& image, gp_Path& pathA, gp_Path& pathB)
{
    int ithre = m_imatchthre;
    int icurmodule = ImageManager::GetCurMode();
    Image* pimage = ImageManager::GetTransferImage(icurmodule);
    (void)pimage;
    int ix0 = static_cast<int>(m_matchrect.TopLeft().X());
    int iy0 = static_cast<int>(m_matchrect.TopLeft().Y());
    int ix1 = static_cast<int>(m_matchrect.BottomRight().X());
    int iy1 = static_cast<int>(m_matchrect.BottomRight().Y());
    ZeroPOS();
    if (image.getWidth() <= ix1
        || image.getHeight() <= iy1)
        return;//error process
    m_iminfindnum = -1;
    const int icount1 = static_cast<int>(pathA.ElementCount());
    const int icount2 = static_cast<int>(pathB.ElementCount());
    (void)icount2;

    cv::Vec3b pixel0, pixel1;
    int icalnum = 0;
    int icalng = 0;

    gp_Pnt aele;

    int igapx = m_stepgapx;
    int igapy = m_stepgapy;
    gp_Rectangle arect1 = pathA.boundingRect();
    gp_Rectangle arect2 = pathB.boundingRect();
    iy1 = iy1 - static_cast<int>(arect1.Height());
    ix1 = ix1 - static_cast<int>(arect1.Width());
    int ix = 0;
    int iy = 0;
    int iw = static_cast<int>(pathA.boundingRect().Width());
    int ih = static_cast<int>(pathA.boundingRect().Height());
    int itotalsize = static_cast<int>(pathA.ElementCount());

    m_dminscore = FastMatchUnitScore(m_dminscore, 0.4);
    const int iminfindngnum = static_cast<int>((1 - m_dminscore) * itotalsize / 2);
    const int iminfindoknum = static_cast<int>(m_dminscore * itotalsize / 2);



    for (iy = iy0; iy < iy1;)
    {
        for (ix = ix0; ix < ix1;)
        {
            ++m_rawmatch_probe_count;
            int imovx = ix;
            int imovy = iy;

            icalnum = 0;
            icalng = 0;
            if (!FastMatchCandidateMaskPass(m_matchmask, pathA, imovx, imovy))
            {
                ix += igapx;
                continue;
            }
            if (!MatchSampleABAnchorPass(image, pathA, pathB, imovx, imovy, ithre, m_iB2W))
            {
                ix += igapx;
                continue;
            }
            for (int i = 0; i < icount1 - 1; i++)
            {
                aele = pathA.ElementAt(i);
                pixel0 = image.pixel(
                    static_cast<int>(aele.X() + imovx),
                    static_cast<int>(aele.Y() + imovy));
                //i++;
                aele = pathB.ElementAt(i);
                pixel1 = image.pixel(
                    static_cast<int>(aele.X() + imovx),
                    static_cast<int>(aele.Y() + imovy));

                if (0 == m_iB2W)
                {
                    int ir = Red(pixel0) - Red(pixel1);
                    int ig = Green(pixel0) - Green(pixel1);
                    int ib = Blue(pixel0) - Blue(pixel1);
                    if (ir > ithre || ig > ithre || ib > ithre)
                    {
                        icalnum++;
                    }
                    else
                    {
                        icalng++;
                        if (icalng > iminfindngnum)
                            goto NextStep_1;
                    }
                }
                else if (1 == m_iB2W)
                {
                    int ir = Red(pixel1) - Red(pixel0);
                    int ig = Green(pixel1) - Green(pixel0);
                    int ib = Blue(pixel1) - Blue(pixel0);
                    if (ir > ithre || ig > ithre || ib > ithre)
                    {
                        icalnum++;
                    }
                    else
                    {
                        icalng++;

                        if (icalng > iminfindngnum)
                            goto NextStep_1;
                    }
                }
            }

        NextStep_1:
            ix += igapx;
            if (icalnum > m_iminfindnum
                && icalnum > iminfindoknum)
            {
                ++m_rawmatch_threshold_hit_count;
                gp_Pnt refined_point(imovx, imovy, 0);
                int refined_score = icalnum;
                refined_point = RefineMatchSampleABPoint(
                    image,
                    pathA,
                    pathB,
                    refined_point,
                    icalnum,
                    ithre,
                    m_iB2W,
                    iminfindngnum,
                    igapx,
                    igapy,
                    refined_score);
                if (m_rawthresholdhitpoints.size() < 32)
                {
                    m_rawthresholdhitpoints.push_back(refined_point);
                    m_rawthresholdhitscores.push_back(refined_score);
                }
                resulttolist(refined_point, refined_score);
            }

        }
        iy += igapy;
    }

    resultsort();
    m_resultrects.clear();
    const int icountresult = static_cast<int>(m_resultnums.size());
    for (int i = 0; i < icountresult; i++)
    {
        int ivalue = m_resultnums.at(i);
        gp_Pnt apoint = m_resultpoints.at(i);
        double dpercent =  ivalue  / (1.0 * itotalsize);
        gp_Rectangle arect(
            gp_Pnt(apoint.X(), apoint.Y(), 0),
            gp_Pnt(apoint.X() + iw, apoint.Y() + ih, 0));
        std::ostringstream stream;
        stream << dpercent;
        std::string astr = stream.str();
        m_resultrects.addrect(arect, astr);
    }

}
void fastmatch::MatchSampleABMore(Image& image, gp_Path& pathA, gp_Path& pathB)
{
    int ithre = m_imatchthre;
    int icurmodule = ImageManager::GetCurMode();
    Image* pimage = ImageManager::GetTransferImage(icurmodule);
    (void)pimage;
    ZeroPOS();
    gp_Rectangle arect1 = pathA.boundingRect();
    gp_Rectangle arect2 = pathB.boundingRect();
    (void)arect2;
    int ix = 0;
    int iy = 0;
    int iw = static_cast<int>(pathA.boundingRect().Width());
    int ih = static_cast<int>(pathA.boundingRect().Height());
    int ix0, iy0, ix1, iy1;

    int igapx = m_stepgapx;
    int igapy = m_stepgapy;
    if (m_resultrects.size() > 0)
    {
        int iresultnum = static_cast<int>(m_resultrects.size()) - 1;
        ix0 = static_cast<int>(m_resultrects.getrect(iresultnum).TopLeft().X() - 1.5 * igapx);
        iy0 = static_cast<int>(m_resultrects.getrect(iresultnum).TopLeft().Y() - 1.5 * igapy);
        ix1 = static_cast<int>(m_resultrects.getrect(iresultnum).BottomRight().X() + 1.5 * igapx);
        iy1 = static_cast<int>(m_resultrects.getrect(iresultnum).BottomRight().Y() + 1.5 * igapx);

         igapx = 1;
         igapy = 1;
    }
    else
    {
         ix0 = static_cast<int>(m_matchrect.TopLeft().X());
         iy0 = static_cast<int>(m_matchrect.TopLeft().Y());
         ix1 = static_cast<int>(m_matchrect.BottomRight().X());
         iy1 = static_cast<int>(m_matchrect.BottomRight().Y());
    }
 



    if (image.getWidth() <= ix1
        || image.getHeight() <= iy1)
        return;//error process
    m_iminfindnum = -1;
    const int icount1 = static_cast<int>(pathA.ElementCount());
    const int icount2 = static_cast<int>(pathB.ElementCount());
    (void)icount2;

    cv::Vec3b pixel0, pixel1;
    int icalnum = 0;
    int icalng = 0;

    gp_Pnt aele;

    iy1 = iy1 - static_cast<int>(arect1.Height());
    ix1 = ix1 - static_cast<int>(arect1.Width());
    int itotalsize = static_cast<int>(pathA.ElementCount());

    m_dminscore = FastMatchUnitScore(m_dminscore, 0.4);
    const int iminfindngnum = static_cast<int>((1 - m_dminscore) * itotalsize / 2);
    const int iminfindoknum = static_cast<int>(m_dminscore * itotalsize / 2);



    for (iy = iy0; iy < iy1;)
    {
        for (ix = ix0; ix < ix1;)
        {
            ++m_rawmatch_probe_count;
            int imovx = ix;
            int imovy = iy;

            icalnum = 0;
            icalng = 0;
            for (int i = 0; i < icount1 - 1; i++)
            {
                aele = pathA.ElementAt(i);
                pixel0 = image.pixel(
                    static_cast<int>(aele.X() + imovx),
                    static_cast<int>(aele.Y() + imovy));
                //i++;
                aele = pathB.ElementAt(i);
                pixel1 = image.pixel(
                    static_cast<int>(aele.X() + imovx),
                    static_cast<int>(aele.Y() + imovy));

                if (0 == m_iB2W)
                {
                    int ir = Red(pixel0) - Red(pixel1);
                    int ig = Green(pixel0) - Green(pixel1);
                    int ib = Blue(pixel0) - Blue(pixel1);
                    if (ir > ithre || ig > ithre || ib > ithre)
                    {
                        icalnum++;
                    }
                    else
                    {
                        icalng++;
                        if (icalng > iminfindngnum)
                            goto NextStep_1;
                    }
                }
                else if (1 == m_iB2W)
                {
                    int ir = Red(pixel1) - Red(pixel0);
                    int ig = Green(pixel1) - Green(pixel0);
                    int ib = Blue(pixel1) - Blue(pixel0);
                    if (ir > ithre || ig > ithre || ib > ithre)
                    {
                        icalnum++;
                    }
                    else
                    {
                        icalng++;

                        if (icalng > iminfindngnum)
                            goto NextStep_1;
                    }
                }
            }

        NextStep_1:
            ix += igapx;
            if (icalnum > m_iminfindnum
                && icalnum > iminfindoknum)
            {
                ++m_rawmatch_threshold_hit_count;
                gp_Pnt apoint(ix, iy, 0);
                resulttolist(apoint, icalnum);
            }

        }
        iy += igapy;
    }

    resultsort();
    m_resultrects.clear();
    const int icountresult = static_cast<int>(m_resultnums.size());
    for (int i = 0; i < icountresult; i++)
    {
        int ivalue = m_resultnums.at(i);
        gp_Pnt apoint = m_resultpoints.at(i);
        double dpercent = ivalue / (1.0 * itotalsize);
        gp_Rectangle arect(
            gp_Pnt(apoint.X(), apoint.Y(), 0),
            gp_Pnt(apoint.X() + iw, apoint.Y() + ih, 0));
        std::ostringstream stream;
        stream << dpercent;
        std::string astr = stream.str();
        m_resultrects.addrect(arect, astr);
    }

}

int fastmatch::getrawmatchprobecount() const
{
    return m_rawmatch_probe_count;
}

int fastmatch::getrawmatchthresholdhitcount() const
{
    return m_rawmatch_threshold_hit_count;
}

int fastmatch::getresulttolistcallcount() const
{
    return m_resulttolist_call_count;
}

int fastmatch::getresultcandidateinsertcount() const
{
    return m_resultcandidate_insert_count;
}

int fastmatch::getresultcandidatereplacecount() const
{
    return m_resultcandidate_replace_count;
}

int fastmatch::getresultcandidaterejectcount() const
{
    return m_resultcandidate_reject_count;
}

int fastmatch::getresultcandidatecount()
{
    return static_cast<int>(std::min(m_resultnums.size(), m_resultpoints.size()));
}

int fastmatch::getresultbestindex()
{
    const int candidate_count = getresultcandidatecount();
    if (candidate_count <= 0)
    {
        return -1;
    }
    return candidate_count - 1;
}

double fastmatch::getresultbestscore()
{
    const int best_index = getresultbestindex();
    if (best_index < 0)
    {
        return 0.0;
    }
    return m_resultnums.at(best_index);
}

int fastmatch::getrawthresholdhitrecordcount() const
{
    return static_cast<int>(m_rawthresholdhitpoints.size());
}

gp_Pnt fastmatch::getrawthresholdhitpoint(int inum) const
{
    if (inum >= 0 && inum < static_cast<int>(m_rawthresholdhitpoints.size()))
    {
        return m_rawthresholdhitpoints.at(inum);
    }
    return gp_Pnt();
}

int fastmatch::getrawthresholdhitscore(int inum) const
{
    if (inum >= 0 && inum < static_cast<int>(m_rawthresholdhitscores.size()))
    {
        return m_rawthresholdhitscores.at(inum);
    }
    return 0;
}

void fastmatch::MultiMatch(Image& image)
{
    resultclear();
    const int isize = static_cast<int>(m_models_l12.size());
    for (int i = 0; i < isize; i++)
    {
        modelstocurrent_l12(i);
        gp_Path& path = Findline::getpatternpath();
        MultiMatchSample(image, path);
    }
}
void fastmatch::multimatch(void* pimage)
{
    Image* pgetimage = (Image*)pimage;
    if (pgetimage == nullptr)
        return;
    MultiMatch(*pgetimage);
}
void fastmatch::MultiMatchSample(Image& image, gp_Path& path)
{
    int ithre = m_imatchthre;
    int icurmodule = ImageManager::GetCurMode();
    Image* pimage = ImageManager::GetTransferImage(icurmodule);
    (void)pimage;

    const int imatchnum = static_cast<int>(m_matchrects.size());
    for (int ik = 0; ik < imatchnum; ik++)
    {
        int ix0 = static_cast<int>(m_matchrects.getrect(ik).TopLeft().X());
        int iy0 = static_cast<int>(m_matchrects.getrect(ik).TopLeft().Y());
        int ix1 = static_cast<int>(m_matchrects.getrect(ik).TopLeft().X() + m_matchrects.getrect(ik).Width());
        int iy1 = static_cast<int>(m_matchrects.getrect(ik).TopLeft().Y() + m_matchrects.getrect(ik).Height());

        if (image.getWidth() < ix1
            || image.getHeight() < iy1)
            return;//error process

        m_iminfindnum = -1;
        const int icount = static_cast<int>(path.ElementCount());

        cv::Vec3b pixel0, pixel1;

        int icalnum = 0;
        int icalng = 0;
        gp_Pnt aele;

        int igapx = m_stepgapx;
        int igapy = m_stepgapy;
        gp_Rectangle arect1 = path.boundingRect();
        iy1 = iy1 - static_cast<int>(arect1.Height());
        ix1 = ix1 - static_cast<int>(arect1.Width());
        //g_pmodelimage->ImageClear(0);
        int ix = 0;
        int iy = 0;

        int iw = static_cast<int>(Findline::patternboundingrect().Width());
        int ih = static_cast<int>(Findline::patternboundingrect().Height());
        int itotalsize = static_cast<int>(Findline::getpattern().size());

        m_dminscore = FastMatchUnitScore(m_dminscore, 0.4);
        const int iminfindngnum = static_cast<int>((1 - m_dminscore) * itotalsize);
        const int iminfindoknum = static_cast<int>(m_dminscore * itotalsize);

        for (iy = iy0; iy < iy1;)
        {
            for (ix = ix0; ix < ix1;)
            {
                int imovx = ix;
                int imovy = iy;

                icalnum = 0;
                for (int i = 0; i < icount - 1; i++)
                {
                    aele = path.ElementAt(i);
                    pixel0 = image.pixel(
                        static_cast<int>(aele.X() + imovx),
                        static_cast<int>(aele.Y() + imovy));
                    i++;
                    aele = path.ElementAt(i);
                    pixel1 = image.pixel(
                        static_cast<int>(aele.X() + imovx),
                        static_cast<int>(aele.Y() + imovy));
                    //iresult =  qGray(pixel0) - qGray(pixel1)  ;

                    if (0 == m_iB2W)
                    {
                        int ir = Red(pixel0) - Red(pixel1);
                        int ig = Green(pixel0) - Green(pixel1);
                        int ib = Blue(pixel0) - Blue(pixel1);
                        if (ir > ithre || ig > ithre || ib > ithre)
                        {
                            icalnum++;
                        }
                        else
                        {
                            icalng++;
                            if (icalng > iminfindngnum)
                                goto NextStep_2;
                        }
                    }
                    else if (1 == m_iB2W)
                    {
                        int ir = Red(pixel1) - Red(pixel0);
                        int ig = Green(pixel1) - Green(pixel0);
                        int ib = Blue(pixel1) - Blue(pixel0);
                        if (ir > ithre || ig > ithre || ib > ithre)
                        {
                            icalnum++;
                        }
                        else
                        {
                            icalng++;
                            if (icalng > iminfindngnum)
                                goto NextStep_2;
                        }
                    }
                }
            NextStep_2:
                ix += igapx;

                if (icalnum > m_iminfindnum
                    && icalnum > iminfindoknum)
                {
                    gp_Pnt apoint(ix, iy,0);
                    resulttolist(apoint, icalnum);
                }

            }
            iy += igapy;
        }

        resultsort();
        m_resultrects.clear();

        const int icountresult = static_cast<int>(m_resultnums.size());
        for (int i = 0; i < icountresult; i++)
        {
            int ivalue = m_resultnums.at(i);
            gp_Pnt apoint = m_resultpoints.at(i);
            double dpercent = (2.0 * ivalue) / (1.0 * itotalsize);
            gp_Rectangle arect(
                gp_Pnt(apoint.X(), apoint.Y(), 0),
                gp_Pnt(apoint.X() + iw, apoint.Y() + ih, 0));
            std::ostringstream stream;
            stream  << dpercent ;
            std::string astr = stream.str();
            m_resultrects.addrect(arect, astr);
        }

    }

}

void fastmatch::RotateMatchAB(Image& image)
{
    m_matchimage = &image;
    resultclear();
   
    for (int it = 0; it < static_cast<int>(m_rotateshaperesults.size()); it++)
    {
        m_rotateshaperesults[it].clear(); 
    } 
    m_rotateshaperesults.clear();
    m_rotatereslutpoints.clear();
    m_rotateresults.clear();
    m_rotatereslutangles.clear();
    m_clusters.clear();

    // int isize = 360/m_danglegap;

    int isize = static_cast<int>((m_dangle_add - m_dangle_mud) / m_danglegap);
    int icurangle = 0;

    if (m_models_rotate.size() <= 0)//normal size 360
        return;

    // double m_dangle_add;//10
    // double m_dangle_mud;//-10
    // 0 - 360
    // 0 - 360/m_danglegap

    if (m_dangle_mud < 0 && m_dangle_add >= 0)
    {
        int ibeginangle = static_cast<int>(360 + m_dangle_mud);
        icurangle = ibeginangle;
        while (icurangle < 360)
        {
            resultclear();
            gp_Path& pathA = m_models_rotate[icurangle].getpathA();
            gp_Path& pathB = m_models_rotate[icurangle].getpathB();
            PointsShape& arectpoints = m_models_rotaterects[icurangle];
            RotateMatchSampleAB(image, pathA, pathB, arectpoints, icurangle);
            icurangle = static_cast<int>(icurangle + m_danglegap);
        }
        icurangle = 0;
        while (icurangle < m_dangle_add)
        {
            resultclear();
            gp_Path& pathA = m_models_rotate[icurangle].getpathA();
            gp_Path& pathB = m_models_rotate[icurangle].getpathB();
            PointsShape& arectpoints = m_models_rotaterects[icurangle];
            RotateMatchSampleAB(image, pathA, pathB, arectpoints, icurangle);
            icurangle = static_cast<int>(icurangle + m_danglegap);
        }
    }
    else if (m_dangle_mud < m_dangle_add && m_dangle_mud >= 0)
    {
        int ibeginangle = static_cast<int>(m_dangle_mud);
        icurangle = ibeginangle;
        for (int i = 0; i < isize; i++)
        {
            resultclear();
            gp_Path& pathA = m_models_rotate[icurangle].getpathA();
            gp_Path& pathB = m_models_rotate[icurangle].getpathB();
            PointsShape& arectpoints = m_models_rotaterects[icurangle];
            RotateMatchSampleAB(image, pathA, pathB, arectpoints, icurangle);
            icurangle = static_cast<int>(icurangle + m_danglegap);
        }
    }

    rotateresultsort();
    if (m_danglegap > 1.0
        && !m_rotateresults.empty()
        && m_iupgradeanglescale > 0)
    {
        setupgradenum(0);
        RotateMatchAB_upgrade(image);
    }
    //   resultcluster(m_ixclustergap,m_iyclustergap,m_iangleclustergap);

}
void fastmatch::setupgradenum(int iresultnum)
{
    m_iupgradenum = iresultnum;
}
void fastmatch::RotateMatchAB_upgrade(Image& image)
{
    /*
        int m_stepgapx;
        int m_stepgapy;

        double m_danglegap;//5

        double m_dangle_add;//10 
        double m_dangle_mud;//-10 
    */
    //result
    if (!HasRotateResultAt(
            m_iupgradenum,
            m_rotateresults,
            m_rotatereslutpoints,
            m_rotatereslutangles,
            m_rotateshaperesults))
        return;

    PointsShape abestshape = m_rotateshaperesults[m_iupgradenum];//4 points
    (void)abestshape;
    gp_Pnt abestpoint = m_rotatereslutpoints[m_iupgradenum];//
    double abestresult = m_rotateresults[m_iupgradenum];//
    (void)abestresult;
    double abestreslutangle = m_rotatereslutangles[m_iupgradenum];//
    //
    m_matchimage = &image;
    resultclear();
    for (int it = 0; it < static_cast<int>(m_rotateshaperesults.size()); it++)
    {
        m_rotateshaperesults[it].clear();
    }
    m_rotateshaperesults.clear();
    m_rotatereslutpoints.clear();
    m_rotateresults.clear();
    m_rotatereslutangles.clear();
    m_clusters.clear();

    // int isize = 360/m_danglegap;
    if (m_iupgradeanglescale <= 6)
        m_iupgradeanglescale = 6;
    int isize = m_iupgradeanglescale;//6;//-3  abestreslutangle +3
    int ihfsize = m_iupgradeanglescale / 2;
    int icurangle = 0;
    if (m_models_rotate.size() <= 0)//normal size 360
        return;
    if (abestreslutangle + m_iupgradeanglescale > 360)
    {
        abestreslutangle = abestreslutangle - 360;
    }

    // double m_dangle_add;//10
    // double m_dangle_mud;//-10
    // 0 - 360
    // 0 - 360/m_danglegap
    if (abestreslutangle - ihfsize < 0 && abestreslutangle + ihfsize >= 0)
    {
        int ibeginangle = static_cast<int>(360 + abestreslutangle - ihfsize);
        icurangle = ibeginangle;
        while (icurangle < 360)
        {
            resultclear();
            gp_Path& path = m_models_rotate[icurangle].getpath();
            PointsShape& arectpoints = m_models_rotaterects[icurangle];

            RotateMatchSample_upgrade(image, path, arectpoints, icurangle, abestpoint);
            icurangle = icurangle + 1;
        }
        icurangle = 0;
        while (icurangle < abestreslutangle + ihfsize)
        {
            resultclear();
            gp_Path& path = m_models_rotate[icurangle].getpath();
            PointsShape& arectpoints = m_models_rotaterects[icurangle];
            RotateMatchSample_upgrade(image, path, arectpoints, icurangle, abestpoint);
            icurangle = icurangle + 1;
        }
    }
    else if (abestreslutangle - ihfsize >= 0)
    {
        int ibeginangle = static_cast<int>(abestreslutangle - ihfsize);
        icurangle = ibeginangle;
        for (int i = 0; i < isize; i++)
        {
            resultclear();
            gp_Path& path = m_models_rotate[icurangle].getpath();
            PointsShape& arectpoints = m_models_rotaterects[icurangle];
            RotateMatchSample_upgrade(image, path, arectpoints, icurangle, abestpoint);
            icurangle = icurangle + 1;
        }
    }

    rotateresultsort();
    //   resultcluster(m_ixclustergap,m_iyclustergap,m_iangleclustergap);

}
void fastmatch::RotateMatchAB05_upgrade(Image& image)
{

    //result
    if (!HasRotateResultAt(
            m_iupgradenum,
            m_rotateresults,
            m_rotatereslutpoints,
            m_rotatereslutangles,
            m_rotateshaperesults))
        return;

    PointsShape abestshape = m_rotateshaperesults[m_iupgradenum];//4 points
    (void)abestshape;
    gp_Pnt abestpoint = m_rotatereslutpoints[m_iupgradenum];//
    double abestresult = m_rotateresults[m_iupgradenum];//
    (void)abestresult;
    double abestreslutangle = m_rotatereslutangles[m_iupgradenum];//
    //
    m_matchimage = &image;
    resultclear();
    for (int it = 0; it < static_cast<int>(m_rotateshaperesults.size()); it++)
    {
        m_rotateshaperesults[it].clear();
    }
    m_rotateshaperesults.clear();
    m_rotatereslutpoints.clear();
    m_rotateresults.clear();
    m_rotatereslutangles.clear();
    m_clusters.clear();

    // int isize = 360/m_danglegap;
    int isize = 12;//-3  abestreslutangle +3 ,0.5
    double dcurangle = 0;
    int ianglenum = 0;
    if (m_models05_rotate.size() <= 0)//normal size 360
        return;

    if (abestreslutangle + 6 > 360)
    {
        abestreslutangle = abestreslutangle - 360;
    }
    // double m_dangle_add;//10
    // double m_dangle_mud;//-10
    // 0 - 360
    // 0 - 360/m_danglegap
    if (abestreslutangle - 6 < 0 && abestreslutangle + 6 >= 0)
    {
        int ibeginangle = static_cast<int>(360 + abestreslutangle - 6);
        dcurangle = ibeginangle;//0.5
        ianglenum = ibeginangle * 2;
        while (ianglenum < 720)
        {
            resultclear();
            gp_Path& path = m_models05_rotate[ianglenum].getpath();
            PointsShape& arectpoints = m_models05_rotaterects[ianglenum];
            RotateMatchSample_upgrade(image, path, arectpoints, dcurangle, abestpoint);
            dcurangle = dcurangle + 0.5;
            ianglenum = ianglenum + 1;
        }
        dcurangle = 0;
        ianglenum = 0;
        while (dcurangle < abestreslutangle + 6)
        {
            resultclear();
            gp_Path& path = m_models05_rotate[ianglenum].getpath();
            PointsShape& arectpoints = m_models05_rotaterects[ianglenum];
            RotateMatchSample_upgrade(image, path, arectpoints, dcurangle, abestpoint);
            dcurangle = dcurangle + 0.5;
            ianglenum = ianglenum + 1;
        }
    }
    else if (abestreslutangle - 6 >= 0)
    {
        int ibeginangle = static_cast<int>(abestreslutangle - 6);
        dcurangle = ibeginangle;
        ianglenum = ibeginangle * 2;
        for (int i = 0; i < isize; i++)
        {
            resultclear();
            gp_Path& path = m_models05_rotate[ianglenum].getpath();
            PointsShape& arectpoints = m_models05_rotaterects[ianglenum];
            RotateMatchSample_upgrade(image, path, arectpoints, dcurangle, abestpoint);
            dcurangle = dcurangle + 0.5;
            ianglenum = ianglenum + 1;
        }
    }

    rotateresultsort();
    //   resultcluster(m_ixclustergap,m_iyclustergap,m_iangleclustergap);

}
void fastmatch::RotateMatchAB025_upgrade(Image& image)
{
    //result
    if (!HasRotateResultAt(
            m_iupgradenum,
            m_rotateresults,
            m_rotatereslutpoints,
            m_rotatereslutangles,
            m_rotateshaperesults))
        return;

    PointsShape abestshape = m_rotateshaperesults[m_iupgradenum];//4 points
    (void)abestshape;
    gp_Pnt abestpoint = m_rotatereslutpoints[m_iupgradenum];//
    double abestresult = m_rotateresults[m_iupgradenum];//
    (void)abestresult;
    double abestreslutangle = m_rotatereslutangles[m_iupgradenum];//
    //
    m_matchimage = &image;
    resultclear();
    for (int it = 0; it < static_cast<int>(m_rotateshaperesults.size()); it++)
    {
        m_rotateshaperesults[it].clear();
    }
    m_rotateshaperesults.clear();
    m_rotatereslutpoints.clear();
    m_rotateresults.clear();
    m_rotatereslutangles.clear();
    m_clusters.clear();

    // int isize = 360/m_danglegap;
    int isize = 24;//-3  abestreslutangle +3 ,0.25
    double dcurangle = 0;
    int ianglenum = 0;
    if (m_models025_rotate.size() <= 0)//normal size 360
        return;

    if (abestreslutangle + 6 > 360)
    {
        abestreslutangle = abestreslutangle - 360;
    }

    if (abestreslutangle - 3 < 0 && abestreslutangle + 3 >= 0)
    {
        int ibeginangle = static_cast<int>(360 + abestreslutangle - 3);
        dcurangle = ibeginangle;//0.5
        ianglenum = ibeginangle * 4;
        while (ianglenum < 1440)
        {
            resultclear();
            gp_Path& path = m_models025_rotate[ianglenum].getpath();
            PointsShape& arectpoints = m_models025_rotaterects[ianglenum];
            RotateMatchSample_upgrade(image, path, arectpoints, dcurangle, abestpoint);
            dcurangle = dcurangle + 0.25;
            ianglenum = ianglenum + 1;
        }
        dcurangle = 0;
        ianglenum = 0;
        while (dcurangle < abestreslutangle + 3)
        {
            resultclear();
            gp_Path& path = m_models025_rotate[ianglenum].getpath();
            PointsShape& arectpoints = m_models025_rotaterects[ianglenum];
            RotateMatchSample_upgrade(image, path, arectpoints, dcurangle, abestpoint);
            dcurangle = dcurangle + 0.25;
            ianglenum = ianglenum + 1;
        }
    }
    else if (abestreslutangle - 3 >= 0)
    {
        int ibeginangle = static_cast<int>(abestreslutangle - 3);
        dcurangle = ibeginangle;
        ianglenum = ibeginangle * 4;
        for (int i = 0; i < isize; i++)
        {
            resultclear();
            gp_Path& path = m_models025_rotate[ianglenum].getpath();
            PointsShape& arectpoints = m_models025_rotaterects[ianglenum];
            RotateMatchSample_upgrade(image, path, arectpoints, dcurangle, abestpoint);
            dcurangle = dcurangle + 0.25;
            ianglenum = ianglenum + 1;
        }
    }

    rotateresultsort();
    //   resultcluster(m_ixclustergap,m_iyclustergap,m_iangleclustergap);

}
void fastmatch::samplemodelAB(int inum) 
{
    Findline::samplemodelAB(inum);
}
void fastmatch::RotateMatch(Image& image)
{
    m_matchimage = &image;
    resultclear();
    for (int it = 0; it < static_cast<int>(m_rotateshaperesults.size()); it++)
    {
        m_rotateshaperesults[it].clear();
    }
    m_rotateshaperesults.clear();
    m_rotatereslutpoints.clear();
    m_rotateresults.clear();
    m_rotatereslutangles.clear();
    m_clusters.clear();

    int isize = static_cast<int>(360 / m_danglegap);
    int icurangle = 0;

    //  double m_dangle_add;//10
    //  double m_dangle_mud;//-10
    if (m_models_rotate.size() <= 0)//normal size 360
        return;



    for (int i = 0; i < isize; i++)
    {
        resultclear();

        gp_Path& path = m_models_rotate[icurangle].getpath();
        PointsShape& arectpoints = m_models_rotaterects[icurangle];
        RotateMatchSample(image, path, arectpoints, icurangle);

        icurangle = static_cast<int>(icurangle + m_danglegap);
    }


    resultcluster(m_ixclustergap, m_iyclustergap, m_iangleclustergap);
}
void fastmatch::setclustergap(int ixclustergap, int iyclustergap, int iangleclustergap)
{
    m_ixclustergap = FastMatchPositiveInt(ixclustergap);
    m_iyclustergap = FastMatchPositiveInt(iyclustergap);
    m_iangleclustergap = FastMatchPositiveInt(iangleclustergap);
}
void fastmatch::rotatematchAB(void* pimage)
{
    Image* pgetimage = (Image*)pimage;
    RotateMatchAB(*pgetimage);
}

void fastmatch::Setupgradescale(int isx, int isy)
{
    m_iupgradexscale = FastMatchPositiveInt(isx);
    m_iupgradeyscale = FastMatchPositiveInt(isy);
}
void fastmatch::Setupgradeanglescale(int iangle)
{
    m_iupgradeanglescale = FastMatchPositiveInt(iangle);
}
void fastmatch::rotatematchAB_upgrade(void* pimage)
{
    Image* pgetimage = (Image*)pimage;
    RotateMatchAB_upgrade(*pgetimage);
}

void fastmatch::rotatematchAB05_upgrade(void* pimage)
{
    Image* pgetimage = (Image*)pimage;
    RotateMatchAB05_upgrade(*pgetimage);
}
void fastmatch::rotatematchAB025_upgrade(void* pimage)
{
    Image* pgetimage = (Image*)pimage;
    RotateMatchAB025_upgrade(*pgetimage);
}

void fastmatch::rotatematch(void* pimage)
{
    Image* pgetimage = (Image*)pimage;
    RotateMatch(*pgetimage);
}

void fastmatch::RotateMatchSample(Image& image, gp_Path& path, 
    PointsShape& modelrect, 
    double dangle)
{
    int ithre = m_imatchthre;
    int icurmodule = ImageManager::GetCurMode();
    Image* pimage = ImageManager::GetTransferImage(icurmodule);
    (void)pimage;
    (void)modelrect;
    (void)dangle;
    int ix0 = static_cast<int>(m_matchrect.TopLeft().X());
    int iy0 = static_cast<int>(m_matchrect.TopLeft().Y());
    int ix1 = static_cast<int>(m_matchrect.TopLeft().X() + m_matchrect.Width());
    int iy1 = static_cast<int>(m_matchrect.TopLeft().Y() + m_matchrect.Height());

    if (image.getWidth() <= ix1
        || image.getHeight() <= iy1)
        return;//error process
    m_iminfindnum = -1;
    const int icount = static_cast<int>(path.ElementCount());
    cv::Vec3b pixel0, pixel1;
    int icalnum = 0;
    int icalng = 0;

    gp_Pnt aele;

    int igapx = m_stepgapx;
    int igapy = m_stepgapy;
    gp_Rectangle arect1 = path.boundingRect();
    iy1 = iy1 - static_cast<int>(arect1.Height());
    ix1 = ix1 - static_cast<int>(arect1.Width());
    int ix = 0;
    int iy = 0;
    int iw = static_cast<int>(Findline::patternboundingrect().Width());
    int ih = static_cast<int>(Findline::patternboundingrect().Height());
    int itotalsize = static_cast<int>(Findline::getpattern().size());

    m_dminscore = FastMatchUnitScore(m_dminscore, 0.4);
    const int iminfindngnum = static_cast<int>((1 - m_dminscore) * itotalsize / 2);
    const int iminfindoknum = static_cast<int>(m_dminscore * itotalsize / 2);



    for (iy = iy0; iy < iy1;)
    {
        for (ix = ix0; ix < ix1;)
        {
            int imovx = ix;
            int imovy = iy;

            icalnum = 0;
            icalng = 0;
            for (int i = 0; i < icount - 1; i++)
            {
                aele = path.ElementAt(i);
                pixel0 = image.pixel(
                    static_cast<int>(aele.X() + imovx),
                    static_cast<int>(aele.Y() + imovy));
                i++;
                aele = path.ElementAt(i);
                pixel1 = image.pixel(
                    static_cast<int>(aele.X() + imovx),
                    static_cast<int>(aele.Y() + imovy));

                if (0 == m_iB2W)
                {
#ifdef COLORMATCH
                    int ir = Red(pixel0) - Red(pixel1);
                    int ig = Green(pixel0) - Green(pixel1);
                    int ib = Blue(pixel0) - Blue(pixel1);
                    if (ir > ithre || ig > ithre || ib > ithre)
#else
                    int ir = Red(pixel0) - Red(pixel1);
                    if (ir > ithre)
#endif
                    {
                        icalnum++;
                    }
                    else
                    {
                        icalng++;
                        if (icalng > iminfindngnum)
                            goto NextStep_1;
                    }
                }
                else if (1 == m_iB2W)
                {
#ifdef COLORMATCH
                    int ir = Red(pixel1) - Red(pixel0);
                    int ig = Green(pixel1) - Green(pixel0);
                    int ib = Blue(pixel1) - Blue(pixel0);
                    if (ir > ithre || ig > ithre || ib > ithre)
#else
                    int ir = Red(pixel1) - Red(pixel0);
                    if (ir > ithre)
#endif
                    {
                        icalnum++;
                    }
                    else
                    {
                        icalng++;

                        if (icalng > iminfindngnum)
                            goto NextStep_1;
                    }
                }
            }

        NextStep_1:
            ix += igapx;
            if (icalnum > m_iminfindnum
                && icalnum > iminfindoknum)
            {
                gp_Pnt apoint(imovx, imovy, 0);
                if (m_rawthresholdhitpoints.size() < 32)
                {
                    m_rawthresholdhitpoints.push_back(apoint);
                    m_rawthresholdhitscores.push_back(icalnum);
                }
                resulttolist(apoint, icalnum);
            }

        }
        iy += igapy;
    }

    resultsort();
    m_resultrects.clear();
    int icountresult = static_cast<int>(m_resultnums.size());
    for (int i = 0; i < icountresult; i++)
    {
        int ivalue = m_resultnums.at(i);
        gp_Pnt apoint = m_resultpoints.at(i);
        double dpercent = (2.0 * ivalue) / (1.0 * itotalsize);
        gp_Rectangle arect(
            gp_Pnt(apoint.X(), apoint.Y(), 0),
            static_cast<int>(iw),
            static_cast<int>(ih));
        std::ostringstream stream;
        stream << dangle <<"   " << dpercent;
        std::string astr = stream.str();
        m_resultrects.addrect(arect, astr);
        PointsShape bmodelrect = modelrect;
        // QFont afont("Fixedsys", 30);
        // bmodelrect.addText(0,0,afont,astr);
        bmodelrect.Move(static_cast<int>(apoint.X()), static_cast<int>(apoint.Y()));
        m_rotateshaperesults.push_back(bmodelrect);
        m_rotatereslutpoints.push_back(apoint);
        m_rotateresults.push_back(dpercent);
        m_rotatereslutangles.push_back(dangle);

    }


}

void fastmatch::RotateMatchSampleAB(Image& image, gp_Path& pathA,
    gp_Path& pathB, PointsShape& modelrect,
    double dangle)
{
    int ithre = m_imatchthre;
    int icurmodule = ImageManager::GetCurMode();
    Image* pimage = ImageManager::GetTransferImage(icurmodule);
    (void)pimage;
    int ix0 = static_cast<int>(m_matchrect.TopLeft().X());
    int iy0 = static_cast<int>(m_matchrect.TopLeft().Y());
    int ix1 = static_cast<int>(m_matchrect.TopLeft().X() + m_matchrect.Width());
    int iy1 = static_cast<int>(m_matchrect.TopLeft().Y() + m_matchrect.Height());
    
    if (image.getWidth() <= ix1
        || image.getHeight() <= iy1)
    {
        if(image.getWidth() <= ix1)
            ix1 = image.getWidth();
        if (image.getHeight() <= iy1)
            iy1 = image.getHeight();
    }
      //  return;//error process
    m_iminfindnum = -1;
    int icount1 = static_cast<int>(pathA.ElementCount());
    int icount2 = static_cast<int>(pathB.ElementCount());
    (void)icount2;

    cv::Vec3b pixel0, pixel1;
    int icalnum = 0;
    int icalng = 0;

    gp_Pnt aele;

    int igapx = m_stepgapx;
    int igapy = m_stepgapy;
    gp_Rectangle arect1 = pathA.boundingRect();
    gp_Rectangle arect2 = pathB.boundingRect();
    (void)arect2;
    iy1 = static_cast<int>(iy1 - arect1.Height());
    ix1 = static_cast<int>(ix1 - arect1.Width());
    int ix = 0;
    int iy = 0;
    int iw = static_cast<int>(pathA.boundingRect().Width());
    int ih = static_cast<int>(pathA.boundingRect().Height());
    int itotalsize = static_cast<int>(pathA.ElementCount());

    m_dminscore = FastMatchUnitScore(m_dminscore, 0.4);
    int iminfindngnum = static_cast<int>((1 - m_dminscore) * itotalsize / 2);
    int iminfindoknum = static_cast<int>(m_dminscore * itotalsize / 2);



    for (iy = iy0; iy < iy1;)
    {
        for (ix = ix0; ix < ix1;)
        {
            int imovx = ix;
            int imovy = iy;

            icalnum = 0;
            icalng = 0;
            if (!FastMatchCandidateMaskPass(m_matchmask, pathA, imovx, imovy))
            {
                ix += igapx;
                continue;
            }
            if (!MatchSampleABAnchorPass(image, pathA, pathB, imovx, imovy, ithre, m_iB2W))
            {
                ix += igapx;
                continue;
            }
            for (int i = 0; i < icount1 - 1; i++)
            {
                aele = pathA.ElementAt(i);
                pixel0 = image.pixel(static_cast<int>(aele.X() + imovx), static_cast<int>(aele.Y() + imovy));
                //i++;
                aele = pathB.ElementAt(i);
                pixel1 = image.pixel(static_cast<int>(aele.X() + imovx), static_cast<int>(aele.Y() + imovy));

                if (0 == m_iB2W)
                {
                    int ir = Red(pixel0) - Red(pixel1);
                    int ig = Green(pixel0) - Green(pixel1);
                    int ib = Blue(pixel0) - Blue(pixel1);
                    if (ir > ithre || ig > ithre || ib > ithre)
                    {
                        icalnum++;
                    }
                    else
                    {
                        icalng++;
                        if (icalng > iminfindngnum)
                            goto NextStep_1_R;
                    }
                }
                else if (1 == m_iB2W)
                { 
                    int ir = Red(pixel1) - Red(pixel0);
                    int ig = Green(pixel1) - Green(pixel0);
                    int ib = Blue(pixel1) - Blue(pixel0);
                    if (ir > ithre || ig > ithre || ib > ithre)
                    {
                        icalnum++;
                    }
                    else
                    {
                        icalng++;

                        if (icalng > iminfindngnum)
                            goto NextStep_1_R;
                    }
                }
            }

        NextStep_1_R:
            ix += igapx;
            if (icalnum > m_iminfindnum
                && icalnum > iminfindoknum)
            {
                gp_Pnt refined_point(imovx, imovy, 0);
                int refined_score = icalnum;
                refined_point = RefineMatchSampleABPoint(
                    image,
                    pathA,
                    pathB,
                    refined_point,
                    icalnum,
                    ithre,
                    m_iB2W,
                    iminfindngnum,
                    igapx,
                    igapy,
                    refined_score);
                resulttolist(refined_point, refined_score);
            }

        }
        iy += igapy;
    }

    resultsort();
    m_resultrects.clear();
    int icountresult = static_cast<int>(m_resultnums.size());
    for (int i = 0; i < icountresult; i++)
    {
        int ivalue = m_resultnums.at(i);
        gp_Pnt apoint = m_resultpoints.at(i);
        double dpercent =  ivalue / (1.0 * itotalsize); 
        gp_Rectangle arect(
            gp_Pnt(apoint.X(), apoint.Y(), 0),
            gp_Pnt(apoint.X() + static_cast<int>(iw), apoint.Y() + static_cast<int>(ih), 0));
        std::ostringstream stream;
        stream << dangle << "   " << dpercent;
        std::string astr = stream.str(); 
        m_resultrects.addrect(arect, astr);
        PointsShape bmodelrect = modelrect;
        // QFont afont("Fixedsys", 30);
        // bmodelrect.addText(0,0,afont,astr);
        bmodelrect.Move(static_cast<int>(apoint.X()), static_cast<int>(apoint.Y()));
        m_rotateshaperesults.push_back(bmodelrect);
        m_rotatereslutpoints.push_back(apoint);
        m_rotateresults.push_back(dpercent);
        m_rotatereslutangles.push_back(dangle); 
    } 
}

void fastmatch::RotateMatchSample_upgrade(Image& image, gp_Path& path, PointsShape& modelrect, double dangle, gp_Pnt& resultpoint)
{
    int ithre = m_imatchthre;
    int icurmodule = ImageManager::GetCurMode();
    Image* pimage = ImageManager::GetTransferImage(icurmodule);
    (void)pimage;
    (void)modelrect;
    (void)dangle;
    int ix0 = static_cast<int>(resultpoint.X() - m_iupgradexscale);//upgradexscale = 5
    int iy0 = static_cast<int>(resultpoint.Y() - m_iupgradeyscale);//upgradeyscale = 5
    int ix1 = static_cast<int>(resultpoint.X() + m_iupgradexscale);
    int iy1 = static_cast<int>(resultpoint.Y() + m_iupgradeyscale);

    if (image.getWidth() <= ix1
        || image.getHeight() <= iy1)
        return;//error process
    m_iminfindnum = -1;
    int icount = static_cast<int>(path.ElementCount());
    cv::Vec3b pixel0, pixel1;
    int icalnum = 0;
    int icalng = 0;

    gp_Pnt aele;

    int igapx = 1;
    int igapy = 1;
    //  gp_Rectangle arect1 = path.boundingRect();
    //  iy1 = iy1 - arect1.Height();
    //  ix1 = ix1 - arect1.Width();
    int ix = 0;
    int iy = 0;
    int iw = static_cast<int>(Findline::patternboundingrect().Width());
    int ih = static_cast<int>(Findline::patternboundingrect().Height());
    int itotalsize = static_cast<int>(Findline::getpattern().size());

    m_dminscore = FastMatchUnitScore(m_dminscore, 0.4);
    int iminfindngnum = static_cast<int>((1 - m_dminscore) * itotalsize / 2);
    int iminfindoknum = static_cast<int>(m_dminscore * itotalsize / 2);

    for (iy = iy0; iy < iy1;)
    {
        for (ix = ix0; ix < ix1;)
        {
            int imovx = ix;
            int imovy = iy;
            icalnum = 0;
            icalng = 0;
            for (int i = 0; i < icount - 1; i++)
            {
                aele = path.ElementAt(i);
                pixel0 = image.pixel(static_cast<int>(aele.X() + imovx), static_cast<int>(aele.Y() + imovy));
                i++;
                aele = path.ElementAt(i);
                pixel1 = image.pixel(static_cast<int>(aele.X() + imovx), static_cast<int>(aele.Y() + imovy));

                if (0 == m_iB2W)
                {
#ifdef COLORMATCH
                    int ir = Red(pixel0) - Red(pixel1);
                    int ig = Green(pixel0) - Green(pixel1);
                    int ib = Blue(pixel0) - Blue(pixel1);
                    if (ir > ithre || ig > ithre || ib > ithre)
#else
                    int ir = Red(pixel0) - Red(pixel1);
                    if (ir > ithre)
#endif
                    {
                        icalnum++;
                    }
                    else
                    {
                        icalng++;
                        if (icalng > iminfindngnum)
                            goto NextStep_1;
                    }
                }
                else if (1 == m_iB2W)
                {
#ifdef COLORMATCH
                    int ir = qRed(pixel1) - qRed(pixel0);
                    int ig = qGreen(pixel1) - qGreen(pixel0);
                    int ib = qBlue(pixel1) - qBlue(pixel0);
                    if (ir > ithre || ig > ithre || ib > ithre)
#else
                    int ir = Red(pixel1) - Red(pixel0);
                    if (ir > ithre)
#endif
                    {
                        icalnum++;
                    }
                    else
                    {
                        icalng++;

                        if (icalng > iminfindngnum)
                            goto NextStep_1;
                    }
                }
            }

        NextStep_1:
            ix += igapx;
            if (icalnum > m_iminfindnum
                && icalnum > iminfindoknum)
            {
                gp_Pnt apoint(ix, iy,0);
                resulttolist(apoint, icalnum);
            }

        }
        iy += igapy;
    }

    resultsort();
    m_resultrects.clear();
    int icountresult = static_cast<int>(m_resultnums.size());
    for (int i = 0; i < icountresult; i++)
    {
        int ivalue = m_resultnums.at(i);
        gp_Pnt apoint = m_resultpoints.at(i);
        double dpercent = (2.0 * ivalue) / (1.0 * itotalsize);
        gp_Rectangle arect(
            gp_Pnt(apoint.X(), apoint.Y(), 0),
            static_cast<int>(iw),
            static_cast<int>(ih));
        std::ostringstream stream;
        stream << dangle << "  " << dpercent;
        std::string astr = stream.str(); 
        m_resultrects.addrect(arect, astr);
        PointsShape bmodelrect = modelrect; 
        bmodelrect.Move(static_cast<int>(apoint.X()), static_cast<int>(apoint.Y()));
        m_rotateshaperesults.push_back(bmodelrect);
        m_rotatereslutpoints.push_back(apoint);
        m_rotateresults.push_back(dpercent);
        m_rotatereslutangles.push_back(dangle);
    }


}

double fastmatch::getresultnum(int inum)
{
    if (inum >= 0 && inum < static_cast<int>(m_resultnums.size()))
    {
        return m_resultnums.at(inum);
    }
    if (inum == -1 && !m_resultnums.empty())
    {
        return m_resultnums.back();
    }
    return 0.0;
}
double fastmatch::getresultcentx(int inum)
{
    const int iw = static_cast<int>(Findline::patternboundingrectAB().Width());
    if (inum >= 0 && inum < static_cast<int>(m_resultpoints.size()))
    {
        return m_resultpoints.at(inum).X() + (iw / 2);
    }
    if (inum == -1 && !m_resultpoints.empty())
    {
        return m_resultpoints.back().X() + (iw / 2);
    }
    return 0.0;
}
double fastmatch::getresultcenty(int inum)
{
    const int ih = static_cast<int>(Findline::patternboundingrectAB().Height());
    if (inum >= 0 && inum < static_cast<int>(m_resultpoints.size()))
    {
        return m_resultpoints.at(inum).Y() + (ih / 2);
    }
    if (inum == -1 && !m_resultpoints.empty())
    {
        return m_resultpoints.back().Y() + (ih / 2);
    }
    return 0.0;
}

double fastmatch::getresolvedresultcentx(int inum)
{
    return getresultcentx(inum) + rect().TopLeft().X();
}

double fastmatch::getresolvedresultcenty(int inum)
{
    if ((inum >= 0 && inum < static_cast<int>(m_resultpoints.size())) ||
        (inum == -1 && !m_resultpoints.empty()))
    {
        return getresultcenty(inum) + rect().TopLeft().Y();
    }
    return 0.0;
}

int fastmatch::getrotateresultcentx(int inum)
{
    if (!HasRotateResultAt(
        inum,
        m_rotateresults,
        m_rotatereslutpoints,
        m_rotatereslutangles,
        m_rotateshaperesults))
    {
        return -9999;
    }
    return static_cast<int>(m_rotateshaperesults[inum].getpointscent().X());
}
int fastmatch::getrotateresultcenty(int inum)
{
    if (!HasRotateResultAt(
        inum,
        m_rotateresults,
        m_rotatereslutpoints,
        m_rotatereslutangles,
        m_rotateshaperesults))
    {
        return -9999;
    }
    return static_cast<int>(m_rotateshaperesults[inum].getpointscent().Y());
}

void fastmatch::getresultcentpoints(void* apoints)
{//m_rotateshaperesults
//m_rotateshaperesults[inum].getpointscent()
    PointsShape* points = (PointsShape*)apoints;
    if (nullptr == points)
        return;
    points->clear();
    const int isize = static_cast<int>(RotateResultSharedCount(
        m_rotateresults,
        m_rotatereslutpoints,
        m_rotatereslutangles,
        m_rotateshaperesults));
    for (int i = 0; i < isize; i++)
    {
        double dx = m_rotateshaperesults[i].getpointscent().X();
        double dy = m_rotateshaperesults[i].getpointscent().Y();

        points->addpoint(dx, dy);
    }

}
void fastmatch::getrotateresultrectpoints(std::vector<cv::Point2f>& points)
{ 
    points.clear();
    int isize = static_cast<int>(m_rotateshaperesults.size());
    if(isize>0)
    {
        for (int i = 0; i < static_cast<int>(m_rotateshaperesults[0].size()); i++)
        {
            double dx = m_rotateshaperesults[0].getx(i);
            double dy = m_rotateshaperesults[0].gety(i);
            points.push_back(cv::Point2f(static_cast<float>(dx), static_cast<float>(dy)));
        }
    }
}
double fastmatch::getmaxresult()
{
    if (m_resultnums.size() > 0)
    {
        const int total_size = static_cast<int>(Findline::getpatternpathA().ElementCount());
        if (total_size <= 0)
        {
            return 0.0;
        }
        return m_resultnums.at(m_resultnums.size() - 1) / static_cast<double>(total_size);
    }

    return 0.0;
}
int fastmatch::geteasyobjectw()
{
    return m_easyobject.s_iwobjnum;
}
int fastmatch::geteasyobjectb()
{
    return m_easyobject.s_ibobjnum;
}

int fastmatch::getmodeleasyobjectw_l72(int i)
{
    return EasyObjectWidthAt(m_easyobjectmodels_l72, i);
}
int fastmatch::getmodeleasyobjectb_l72(int i)
{
    return EasyObjectBlackCountAt(m_easyobjectmodels_l72, i);
}

int fastmatch::getmodeleasyobjectw_l36(int i)
{
    return EasyObjectWidthAt(m_easyobjectmodels_l36, i);
}
int fastmatch::getmodeleasyobjectb_l36(int i)
{
    return EasyObjectBlackCountAt(m_easyobjectmodels_l36, i);
}

int fastmatch::getmodeleasyobjectw_l12(int i)
{
    return EasyObjectWidthAt(m_easyobjectmodels_l12, i);
}
int fastmatch::getmodeleasyobjectb_l12(int i)
{
    return EasyObjectBlackCountAt(m_easyobjectmodels_l12, i);
}

int fastmatch::getmodeleasyobjectw_l3(int i)
{
    return EasyObjectWidthAt(m_easyobjectmodels_l3, i);
}
int fastmatch::getmodeleasyobjectb_l3(int i)
{
    return EasyObjectBlackCountAt(m_easyobjectmodels_l3, i);
}
int fastmatch::getmodeleasyobjectw_l6(int i)
{
    return EasyObjectWidthAt(m_easyobjectmodels_l6, i);
}
int fastmatch::getmodeleasyobjectb_l6(int i)
{
    return EasyObjectBlackCountAt(m_easyobjectmodels_l6, i);
}

void fastmatch::setrelationrectfromresultnum(int inum)
{
    m_irelationresultnum = inum;
}
void fastmatch::setrelationrectfrom_matchresult(void* pmatch)
{
    m_prelationmatch = (fastmatch*)pmatch;
    if (0 != m_prelationmatch)
    {
        int inum = m_prelationmatch->m_resultrects.size();
        if (m_irelationresultnum >= 0 && m_irelationresultnum < inum)
        {
            m_irelationrect = m_prelationmatch->m_resultrects.getrect(m_irelationresultnum);
        }
    }
}
void fastmatch::setrelationxy(int iprex1, int iprey1, int iendx1, int iendy1)
{
    (void)iprex1;
    (void)iprey1;
    (void)iendx1;
    (void)iendy1;
 /*   m_irelationrect.setLeft(m_irelationrect.left() + iprex1);
    m_irelationrect.setTop(m_irelationrect.top() + iprey1);
    m_irelationrect.setRight(m_irelationrect.right() + iendx1);
    m_irelationrect.setBottom(m_irelationrect.bottom() + iendy1);
    */
}
void fastmatch::setrelationzoom(double drelationzoomx, double drelationzoomy)
{
    (void)drelationzoomx;
    (void)drelationzoomy;
/*    m_irelationrect.setLeft((double)m_irelationrect.left() * drelationzoomx);
    m_irelationrect.setTop((double)m_irelationrect.top() * drelationzoomy);
    m_irelationrect.setRight((double)m_irelationrect.right() * drelationzoomx);
    m_irelationrect.setBottom((double)m_irelationrect.bottom() * drelationzoomy);*/
}
void fastmatch::setrelationtorect()
{
    if (m_irelationrect.TopLeft().X() >= 0
        && m_irelationrect.TopLeft().Y() >= 0
        && m_irelationrect.Width() > 0
        && m_irelationrect.Height() > 0)
        m_matchrect = m_irelationrect;
}
void fastmatch::shapesetroi(void* pshape)
{
    if (pshape == nullptr)
        return;
    Shape::shapesetroi(pshape);
}

std::vector<cv::Point2f> fastmatch::getmodel() const
{
    std::vector<cv::Point2f> points;
    const int count = m_modelpoints_sample1.size();
    for (int i = 0; i < count; ++i)
    {
        points.push_back(cv::Point2f(
            static_cast<float>(m_modelpoints_sample1.getx(i)),
            static_cast<float>(m_modelpoints_sample1.gety(i))));
    }
    return points;
}

int fastmatch::getmodelpointcount()
{
    return m_modelpoints_sample1.size();
}

void fastmatch::PublishDisplayShapes(ICxShapeSink& sink, const std::string& owner_ref)
{
    const gp_Rectangle learn_rect = rect();
    const double learn_x = learn_rect.TopLeft().X();
    const double learn_y = learn_rect.TopLeft().Y();
    const double learn_w = learn_rect.Width();
    const double learn_h = learn_rect.Height();

    if (learn_w > 0 && learn_h > 0)
    {
        auto learn_roi_shape = std::make_unique<RectShape>();
        learn_roi_shape->setRect(learn_x, learn_y, learn_x + learn_w, learn_y + learn_h);
        sink.UpsertShape(
            owner_ref + ".learn_roi",
            "fastmatch",
            owner_ref,
            "learn_roi",
            "learn_roi",
            true,
            false,
            std::move(learn_roi_shape));
    }

    const gp_Rectangle search_rect = m_matchrect;
    const double search_x = search_rect.TopLeft().X();
    const double search_y = search_rect.TopLeft().Y();
    const double search_w = search_rect.Width();
    const double search_h = search_rect.Height();

    if (search_w > 0 && search_h > 0)
    {
        auto search_roi_shape = std::make_unique<RectShape>();
        search_roi_shape->setRect(search_x, search_y, search_x + search_w, search_y + search_h);
        sink.UpsertShape(
            owner_ref + ".search_roi",
            "fastmatch",
            owner_ref,
            "search_roi",
            "search_roi",
            true,
            false,
            std::move(search_roi_shape));
    }

    const auto& model_points = getmodel();
    if (!model_points.empty())
    {
        auto model_shape = std::make_unique<PointsShape>();
        for (const auto& pt : model_points)
        {
            model_shape->addpoint(pt.x, pt.y);
        }
        sink.UpsertShape(
            owner_ref + ".model_points",
            "fastmatch",
            owner_ref,
            "",
            "model",
            false,
            false,
            std::move(model_shape));
    }

    const int candidate_count = getresultcandidatecount();
    if (candidate_count > 0)
    {
        auto candidates_shape = std::make_unique<PointsShape>();
        for (int i = 0; i < candidate_count; ++i)
        {
            candidates_shape->addpoint(getresolvedresultcentx(i), getresolvedresultcenty(i));
        }
        sink.UpsertShape(
            owner_ref + ".candidate_centers",
            "fastmatch",
            owner_ref,
            "",
            "measure_points",
            false,
            true,
            std::move(candidates_shape));
    }

    const RectsShape* result_rects = getresultrects();
    if (result_rects != nullptr && result_rects->size() > 0)
    {
        for (int i = 0; i < result_rects->size(); ++i)
        {
            const gp_Rectangle r = getresolvedresultrect(i);
            auto result_shape = std::make_unique<RectShape>();
            result_shape->setRect(r.TopLeft().X(), r.TopLeft().Y(),
                                 r.BottomRight().X(), r.BottomRight().Y());
            sink.UpsertShape(
                owner_ref + ".result_boxes." + std::to_string(i),
                "fastmatch",
                owner_ref,
                "result",
                "result",
                false,
                true,
                std::move(result_shape));
        }
    }

    if (candidate_count > 0)
    {
        const int best_index = getresultbestindex();
        if (best_index >= 0 && best_index < candidate_count)
        {
            auto best_shape = std::make_unique<PointsShape>();
            best_shape->addpoint(getresolvedresultcentx(best_index), getresolvedresultcenty(best_index));
            sink.UpsertShape(
                owner_ref + ".best_center",
                "fastmatch",
                owner_ref,
                "",
                "best_result",
                false,
                true,
                std::move(best_shape));
        }
    }
}

bool fastmatch::ApplyDisplayShapeEdit(const std::string& owner_binding, const std::string& semantic_role,
                                      double x0, double y0, double x1, double y1, std::string& reason)
{
    if (owner_binding == "learn_roi")
    {
        const double w = std::abs(x1 - x0);
        const double h = std::abs(y1 - y0);
        const double x = std::min(x0, x1);
        const double y = std::min(y0, y1);

        if (w < 2.0 || h < 2.0)
        {
            reason = "learn ROI too small (min 2px)";
            return false;
        }

        setrect(static_cast<int>(x), static_cast<int>(y),
                static_cast<int>(w), static_cast<int>(h));
        reason = "learn ROI updated";
        return true;
    }
    else if (owner_binding == "search_roi")
    {
        const double w = std::abs(x1 - x0);
        const double h = std::abs(y1 - y0);
        const double x = std::min(x0, x1);
        const double y = std::min(y0, y1);

        if (w < 2.0 || h < 2.0)
        {
            reason = "search ROI too small (min 2px)";
            return false;
        }

        setmatchrect(static_cast<int>(x), static_cast<int>(y),
                     static_cast<int>(w), static_cast<int>(h));
        reason = "search ROI updated";
        return true;
    }
    else if (owner_binding == "result")
    {
        reason = "result elements are not editable";
        return false;
    }

    reason = "unknown owner_binding: " + owner_binding;
    return false;
}
