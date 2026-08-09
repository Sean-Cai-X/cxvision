#include "pch.h"
#include "FastMatch.h"
#include "imagemanager.h"
#include "ImageAnnotationLayer.h"
#include "CxUnifiedLog.h"
#include <algorithm>
#include <cmath>
#include <opencv2/imgproc.hpp>
#include <format>
#if defined USE_AI
#include "mlpackrun.h"
#endif

namespace
{
bool FastMatchPointInsideImage(const Image& image, int x, int y)
{
    return x >= 0 && y >= 0 && x < image.getWidth() && y < image.getHeight();
}

#ifdef FASTMATCH_LEARN_PROBE
void ProbeLog(const std::string& msg)
{
    MessageBoxA(NULL, msg.c_str(), "FastMatch Probe", MB_OK);
    
    FILE* fp = fopen("D:\\Codex-WorkDir\\Sean_WorkDir\\cxvisionai\\cxscript_runs\\probe_log.txt", "a");
    if (fp != nullptr)
    {
        fprintf(fp, "[%d] %s\n", GetCurrentThreadId(), msg.c_str());
        fflush(fp);
        fclose(fp);
    }
    OutputDebugStringA(("[FastMatchProbe] " + msg).c_str());
}

#pragma pack(push, 1)
struct FastMatchProbeData
{
    volatile LONG probe_count;
    volatile LONG learn_entry_called;
    volatile LONG learn_before_Learn_called;
    volatile LONG Learn_entry_called;
    volatile LONG Learn_after_edgepattern_called;
    volatile LONG learn_after_Learn_called;
    volatile LONG learn_fallback_called;
    volatile LONG pimage_null;
    volatile LONG image_width;
    volatile LONG image_height;
    volatile LONG rect_x;
    volatile LONG rect_y;
    volatile LONG rect_w;
    volatile LONG rect_h;
    volatile LONG thre;
    volatile LONG linegap;
    volatile LONG wgap;
    volatile LONG hgap;
    volatile LONG learn_a_count;
    volatile LONG learn_b_count;
    volatile LONG learn_a2_count;
    volatile LONG learn_b2_count;
    volatile LONG total_points;
    char last_message[512];
};
#pragma pack(pop)

FastMatchProbeData* g_probe_data = nullptr;

void InitProbeSharedMemory()
{
    if (g_probe_data != nullptr) return;
    
    HANDLE hMapFile = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        sizeof(FastMatchProbeData),
        "FastMatchProbeSharedMemory");
    
    if (hMapFile != nullptr)
    {
        g_probe_data = (FastMatchProbeData*)MapViewOfFile(
            hMapFile,
            FILE_MAP_ALL_ACCESS,
            0,
            0,
            sizeof(FastMatchProbeData));
        
        if (g_probe_data != nullptr)
        {
            ZeroMemory((void*)g_probe_data, sizeof(FastMatchProbeData));
        }
    }
}

void ProbeSetLearnEntry(int pimage_null_val)
{
    InitProbeSharedMemory();
    if (g_probe_data != nullptr)
    {
        InterlockedIncrement(&g_probe_data->learn_entry_called);
        g_probe_data->pimage_null = pimage_null_val;
    }
}

void ProbeSetLearnBeforeLearn()
{
    InitProbeSharedMemory();
    if (g_probe_data != nullptr)
    {
        InterlockedIncrement(&g_probe_data->learn_before_Learn_called);
    }
}

void ProbeSetLearnEntryData(int w, int h, int rx, int ry, int rw, int rh, int t, int lg, int wg, int hg)
{
    InitProbeSharedMemory();
    if (g_probe_data != nullptr)
    {
        InterlockedIncrement(&g_probe_data->Learn_entry_called);
        g_probe_data->image_width = w;
        g_probe_data->image_height = h;
        g_probe_data->rect_x = rx;
        g_probe_data->rect_y = ry;
        g_probe_data->rect_w = rw;
        g_probe_data->rect_h = rh;
        g_probe_data->thre = t;
        g_probe_data->linegap = lg;
        g_probe_data->wgap = wg;
        g_probe_data->hgap = hg;
    }
}

void ProbeSetLearnAfterEdgepattern(int a, int b, int a2, int b2, int total)
{
    InitProbeSharedMemory();
    if (g_probe_data != nullptr)
    {
        InterlockedIncrement(&g_probe_data->Learn_after_edgepattern_called);
        g_probe_data->learn_a_count = a;
        g_probe_data->learn_b_count = b;
        g_probe_data->learn_a2_count = a2;
        g_probe_data->learn_b2_count = b2;
        g_probe_data->total_points = total;
    }
}

void ProbeSetLearnAfterLearn(int a, int b, int a2, int b2)
{
    InitProbeSharedMemory();
    if (g_probe_data != nullptr)
    {
        InterlockedIncrement(&g_probe_data->learn_after_Learn_called);
        g_probe_data->learn_a_count = a;
        g_probe_data->learn_b_count = b;
        g_probe_data->learn_a2_count = a2;
        g_probe_data->learn_b2_count = b2;
    }
}

void ProbeSetLearnFallback()
{
    InitProbeSharedMemory();
    if (g_probe_data != nullptr)
    {
        InterlockedIncrement(&g_probe_data->learn_fallback_called);
    }
}
#endif

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

bool LearnPatternThroughRectFindline(
    Image& image,
    FastMatch& source,
    int learn_x,
    int learn_y,
    int learn_w,
    int learn_h,
    bool use_object_filter_fallback,
    PointsShape& out_pattern,
    int& out_a_count,
    int& out_b_count,
    int& out_a2_count,
    int& out_b2_count)
{
    FindLine finder;
    finder.SetWHgap(source.wgap(), source.hgap());
    finder.setcomparegap(source.getconparegap());
    finder.setthre(source.thre());
    finder.setlinegap(source.linegap());
    finder.setobjfilter(use_object_filter_fallback ? 1 : source.objfilter());
    if (use_object_filter_fallback)
    {
        finder.setfilterprofile(1);
        finder.setfilter(21, 5, 100000);
        finder.setmeasurefallback(1);
    }
    finder.setrect(learn_x, learn_y, learn_w, learn_h);
    finder.edgepattern(image);

    out_a_count = finder.getlearnacount();
    out_b_count = finder.getlearnbcount();
    out_a2_count = finder.getlearna2count();
    out_b2_count = finder.getlearnb2count();
    const int collected_count =
        out_a_count + out_b_count + out_a2_count + out_b2_count;
    if (collected_count <= 0)
    {
        return false;
    }

    if (finder.getpatternpathA().ElementCount() <= 0 ||
        finder.getpatternpathB().ElementCount() <= 0)
    {
        return false;
    }

    out_pattern = PointsShape();
    out_pattern.copy(finder.getpattern());
    return out_pattern.ABsize() > 0;
}

int PixelGrayAt(const cv::Mat& gray, int x, int y)
{
    x = std::clamp(x, 0, gray.cols - 1);
    y = std::clamp(y, 0, gray.rows - 1);
    return static_cast<int>(gray.at<unsigned char>(y, x));
}

bool IsEdgeTransition(const cv::Mat& gray, int x0, int y0, int x1, int y1, int threshold)
{
    return std::abs(PixelGrayAt(gray, x1, y1) - PixelGrayAt(gray, x0, y0)) >= threshold;
}

int CollectVerticalBoundaryPoints(
    const cv::Mat& gray,
    int x0,
    int y0,
    int x1,
    int y1,
    int step,
    int threshold,
    bool from_top,
    PointsShape& out_points)
{
    int count = 0;
    const int left = std::clamp(std::min(x0, x1), 0, gray.cols - 1);
    const int right = std::clamp(std::max(x0, x1), 0, gray.cols - 1);
    const int top = std::clamp(std::min(y0, y1), 0, gray.rows - 1);
    const int bottom = std::clamp(std::max(y0, y1), 0, gray.rows - 1);
    const int scan_step = std::max(1, step);
    if (right <= left || bottom <= top)
        return 0;

    for (int x = left; x <= right; x += scan_step)
    {
        if (from_top)
        {
            for (int y = top; y < bottom; ++y)
            {
                if (IsEdgeTransition(gray, x, y, x, y + 1, threshold))
                {
                    out_points.addpoint(x, y + 1);
                    ++count;
                    break;
                }
            }
        }
        else
        {
            for (int y = bottom; y > top; --y)
            {
                if (IsEdgeTransition(gray, x, y, x, y - 1, threshold))
                {
                    out_points.addpoint(x, y - 1);
                    ++count;
                    break;
                }
            }
        }
    }
    return count;
}

int CollectHorizontalBoundaryPoints(
    const cv::Mat& gray,
    int x0,
    int y0,
    int x1,
    int y1,
    int step,
    int threshold,
    bool from_left,
    PointsShape& out_points)
{
    int count = 0;
    const int left = std::clamp(std::min(x0, x1), 0, gray.cols - 1);
    const int right = std::clamp(std::max(x0, x1), 0, gray.cols - 1);
    const int top = std::clamp(std::min(y0, y1), 0, gray.rows - 1);
    const int bottom = std::clamp(std::max(y0, y1), 0, gray.rows - 1);
    const int scan_step = std::max(1, step);
    if (right <= left || bottom <= top)
        return 0;

    for (int y = top; y <= bottom; y += scan_step)
    {
        if (from_left)
        {
            for (int x = left; x < right; ++x)
            {
                if (IsEdgeTransition(gray, x, y, x + 1, y, threshold))
                {
                    out_points.addpoint(x + 1, y);
                    ++count;
                    break;
                }
            }
        }
        else
        {
            for (int x = right; x > left; --x)
            {
                if (IsEdgeTransition(gray, x, y, x - 1, y, threshold))
                {
                    out_points.addpoint(x - 1, y);
                    ++count;
                    break;
                }
            }
        }
    }
    return count;
}

bool LearnPatternByBoundaryPointPairs(
    Image& image,
    FastMatch& source,
    int learn_x,
    int learn_y,
    int learn_w,
    int learn_h,
    PointsShape& out_pattern,
    int& out_a_count,
    int& out_b_count,
    int& out_a2_count,
    int& out_b2_count)
{
    cv::Mat src = image.getmat();
    out_a_count = 0;
    out_b_count = 0;
    out_a2_count = 0;
    out_b2_count = 0;

    if (src.empty())
    {
        return false;
    }

    cv::Mat gray;
    if (src.channels() == 1)
        gray = src;
    else
        cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);

    if (gray.empty())
    {
        return false;
    }

    const int x0 = std::clamp(learn_x, 0, gray.cols - 1);
    const int y0 = std::clamp(learn_y, 0, gray.rows - 1);
    const int x1 = std::clamp(learn_x + std::max(1, learn_w), 0, gray.cols - 1);
    const int y1 = std::clamp(learn_y + std::max(1, learn_h), 0, gray.rows - 1);
    if (x1 <= x0 || y1 <= y0)
    {
        return false;
    }

    const int threshold = std::max(4, source.thre());
    const int compare_gap = std::max(1, source.getconparegap());

    PointsShape top_points;
    PointsShape bottom_points;
    PointsShape left_points;
    PointsShape right_points;

    out_a_count = CollectVerticalBoundaryPoints(
        gray, x0, y0, x1, y1, source.wgap(), threshold, true, top_points);
    out_b_count = CollectVerticalBoundaryPoints(
        gray, x0, y0, x1, y1, source.wgap(), threshold, false, bottom_points);
    out_a2_count = CollectHorizontalBoundaryPoints(
        gray, x0, y0, x1, y1, source.hgap(), threshold, true, left_points);
    out_b2_count = CollectHorizontalBoundaryPoints(
        gray, x0, y0, x1, y1, source.hgap(), threshold, false, right_points);

    out_pattern = PointsShape();
    top_points.doublepattern(compare_gap, 6, out_pattern);
    bottom_points.doublepattern(compare_gap, 12, out_pattern);
    left_points.doublepattern(compare_gap, 3, out_pattern);
    right_points.doublepattern(compare_gap, 9, out_pattern);

    return out_pattern.ABsize() > 0;
}
}

void removeAt(std::vector<int>& vec, size_t index) {
    if (index < vec.size()) {
        vec.erase(vec.begin() + index);
    }
    else {
        std::cerr << "Index out of bounds" << std::endl;
    }
}
void removePntAt(std::vector<gp_Pnt>& vec, size_t index) {
    if (index < vec.size()) {
        vec.erase(vec.begin() + index);
    }
    else {
        std::cerr << "Index out of bounds" << std::endl;
    }
} 
void removedoubleAt(std::vector<double>& vec, size_t index) {
    if (index < vec.size()) {
        vec.erase(vec.begin() + index);
    }
    else {
        std::cerr << "Index out of bounds" << std::endl;
    }
}
void removePointsShapeAt(std::vector<PointsShape>& vec, size_t index) {
    if (index < vec.size()) {
        vec.erase(vec.begin() + index);
    }
    else {
        std::cerr << "Index out of bounds" << std::endl;
    }
}

void removeLast(std::vector<int>& vec) {
    if (!vec.empty()) {
        vec.pop_back();
    }
    else {
        std::cerr << "Vector is already empty" << std::endl;
    }
}
void removedoubleLast(std::vector<double>& vec) {
    if (!vec.empty()) {
        vec.pop_back();
    }
    else {
        std::cerr << "Vector is already empty" << std::endl;
    }
}
void removePntLast(std::vector<gp_Pnt>& vec) {
    if (!vec.empty()) {
        vec.pop_back();
    }
    else {
        std::cerr << "Vector is already empty" << std::endl;
    }
}
void removePointsShapeLast(std::vector<PointsShape>& vec) {
    if (!vec.empty()) {
        vec.pop_back();
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
        const int ax = static_cast<int>(pointA.X() + movx);
        const int ay = static_cast<int>(pointA.Y() + movy);
        const int bx = static_cast<int>(pointB.X() + movx);
        const int by = static_cast<int>(pointB.Y() + movy);
        if (!FastMatchPointInsideImage(image, ax, ay) ||
            !FastMatchPointInsideImage(image, bx, by))
        {
            return 0;
        }
        const cv::Vec3b pixel0 = image.pixel(ax, ay);
        const cv::Vec3b pixel1 = image.pixel(bx, by);

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
        const int ax = static_cast<int>(pointA.X() + movx);
        const int ay = static_cast<int>(pointA.Y() + movy);
        const int bx = static_cast<int>(pointB.X() + movx);
        const int by = static_cast<int>(pointB.Y() + movy);
        if (!FastMatchPointInsideImage(image, ax, ay) ||
            !FastMatchPointInsideImage(image, bx, by))
        {
            return false;
        }
        const cv::Vec3b pixel0 = image.pixel(ax, ay);
        const cv::Vec3b pixel1 = image.pixel(bx, by);
        const int dr = ib2w == 0 ? Red(pixel0) - Red(pixel1) : Red(pixel1) - Red(pixel0);
        const int dg = ib2w == 0 ? Green(pixel0) - Green(pixel1) : Green(pixel1) - Green(pixel0);
        const int db = ib2w == 0 ? Blue(pixel0) - Blue(pixel1) : Blue(pixel1) - Blue(pixel0);
        ++probe_count;
        if (dr > ithre || dg > ithre || db > ithre)
        {
            ++hit_count;
        }
    }

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

int FastMatch::m_curfastmatchnum = 0;
FastMatch::FastMatch() :FindLine(),
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
m_expected_rect(gp_Pnt(0,0,0), gp_Pnt(0, 0, 0)),
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

    m_pgrid = new Grid;
    m_pgrid->setgrid(30, 30, 12, 12, 30, 30);

}
void FastMatch::setgrid(int iw, int igrid)
{
    if (m_pgrid == nullptr)
    {
        return;
    }
    m_pgrid->setgrid(FastMatchPositiveInt(iw), FastMatchPositiveInt(iw), FastMatchPositiveInt(igrid), FastMatchPositiveInt(igrid), FastMatchPositiveInt(iw), FastMatchPositiveInt(iw));
}
FastMatch::~FastMatch()
{
    delete m_pgrid;
}
void FastMatch::setcomparegap(int igap)
{
    FindLine::setcomparegap(igap);
}
void FastMatch::setfindnum(int ifindnum)
{
    m_imaxmatchnum = FastMatchPositiveInt(ifindnum);
}
void FastMatch::setmatchmask(const cv::Mat* pmask)
{
    m_matchmask = pmask;
}
void FastMatch::clearmatchmask()
{
    m_matchmask = nullptr;
}
void FastMatch::setb2w(int ib2w)
{
    m_iB2W = ib2w == 1 ? 1 : 0;
}
void FastMatch::getshape(void* pshape)
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
void FastMatch::setrect(int ix, int iy, int iw, int ih)
{
    ix = FastMatchNonNegativeInt(ix);
    iy = FastMatchNonNegativeInt(iy);
    iw = FastMatchPositiveInt(iw);
    ih = FastMatchPositiveInt(ih);
    m_learn_roi_x = ix;
    m_learn_roi_y = iy;
    m_learn_roi_w = iw;
    m_learn_roi_h = ih;
    FindLine::setrect(ix, iy, iw, ih);
}
void FastMatch::setshow(int ishow)
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

    FindLine::setshow(ishow);
}
void FastMatch::SetWHgap(int wgap, int hgap)
{
    FindLine::SetWHgap(wgap, hgap);
}
void FastMatch::measure(void* pimage)
{
    FindLine::measure(pimage);
}
void FastMatch::setlinesamplerate(double dsamplerate)
{
    FindLine::setlinesamplerate(dsamplerate);
}
void FastMatch::setlinegap(int igap)
{
    FindLine::setlinegap(igap);
}
void FastMatch::setmethod(int imethod)
{
    FindLine::setmethod(imethod);
}
void FastMatch::setthre(int ithre)
{
    FindLine::setthre(ithre);
}
void FastMatch::setmatchthre(int ithre)
{
    m_imatchthre = FastMatchNonNegativeInt(ithre);
}
void FastMatch::setobjfilter(int ifindset)
{
    FindLine::setobjfilter(ifindset);
}
void FastMatch::setfilter(int ifilterborw, int ifiltermin, int ifiltermax)
{
    FindLine::setfilter(ifilterborw, ifiltermin, ifiltermax);
}
void FastMatch::setselectedgenum(int iedgenum)
{
    FindLine::setselectedgenum(iedgenum);
}
vector<PointsShape>& FastMatch::getmodels_l12()
{
    return m_models_l12;
}
void FastMatch::setspecshow(int ishow)
{
    m_ispecshow = ishow;
    m_resultrects.setspecshow(ishow);
    m_matchrects.setspecshow(ishow);
}

void FastMatch::setshownum(int ishownum)
{
    m_ishownum = FastMatchPositiveInt(ishownum);
}
void FastMatch::drawshape( )
{
    gp_Path painter;
    if (show() & 0x20)
    {
        m_pgrid->setshow(0x04);
    }
    if (show() & 0x08)
    {
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
        FindLine::drawshape( );
    m_matchrects.drawshape(painter);
}
void FastMatch::setcolorstyle(int istyle)
{
    m_istyle = istyle;
}
void FastMatch::drawshapex(
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
        FindLine::drawshape();
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

void FastMatch::Learn(Image& image)
{
    const int input_learn_roi_x = m_learn_roi_x;
    const int input_learn_roi_y = m_learn_roi_y;
    const int input_learn_roi_w = FastMatchPositiveInt(m_learn_roi_w);
    const int input_learn_roi_h = FastMatchPositiveInt(m_learn_roi_h);

    CXLOG_INFO("FastMatch", "learn_image_enter", "running",
        "image=" + std::to_string(image.getWidth()) + "x" + std::to_string(image.getHeight()) +
        " rect=" + std::to_string(static_cast<int>(rect().TopLeft().X())) + "," +
        std::to_string(static_cast<int>(rect().TopLeft().Y())) + "," +
        std::to_string(static_cast<int>(rect().Width())) + "," +
        std::to_string(static_cast<int>(rect().Height())));
    m_fastmatch_learn_status_code = 1;
    m_fastmatch_learn_a_count = 0;
    m_fastmatch_learn_b_count = 0;
    m_fastmatch_learn_a2_count = 0;
    m_fastmatch_learn_b2_count = 0;

    edgepattern(image);
    m_fastmatch_learn_a_count = FindLine::getlearnacount();
    m_fastmatch_learn_b_count = FindLine::getlearnbcount();
    m_fastmatch_learn_a2_count = FindLine::getlearna2count();
    m_fastmatch_learn_b2_count = FindLine::getlearnb2count();
    const int initial_collected_count =
        m_fastmatch_learn_a_count +
        m_fastmatch_learn_b_count +
        m_fastmatch_learn_a2_count +
        m_fastmatch_learn_b2_count;

    if (FindLine::getpatternpathA().ElementCount() > 0
        && FindLine::getpatternpathB().ElementCount() > 0
        && initial_collected_count > 0)
    {
        m_fastmatch_learn_status_code = 10;
        return;
    }

    const int saved_wgap = wgap();
    const int saved_hgap = hgap();
    const int saved_comparegap = getconparegap();
    const int saved_threshold = thre();
    const int saved_linegap = linegap();
    const int saved_rect_x = input_learn_roi_x;
    const int saved_rect_y = input_learn_roi_y;
    const int saved_rect_w = input_learn_roi_w;
    const int saved_rect_h = input_learn_roi_h;

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
    m_fastmatch_learn_a_count = FindLine::getlearnacount();
    m_fastmatch_learn_b_count = FindLine::getlearnbcount();
    m_fastmatch_learn_a2_count = FindLine::getlearna2count();
    m_fastmatch_learn_b2_count = FindLine::getlearnb2count();
    const int retry_collected_count =
        m_fastmatch_learn_a_count +
        m_fastmatch_learn_b_count +
        m_fastmatch_learn_a2_count +
        m_fastmatch_learn_b2_count;

    if (FindLine::getpatternpathA().ElementCount() <= 0 ||
        FindLine::getpatternpathB().ElementCount() <= 0 ||
        retry_collected_count <= 0)
    {
        m_fastmatch_learn_status_code = 20;
        const int learn_x = saved_rect_x;
        const int learn_y = saved_rect_y;
        const int learn_w = std::max(1, saved_rect_w);
        const int learn_h = std::max(1, saved_rect_h);

        PointsShape rect_findline_pattern;
        if (LearnPatternThroughRectFindline(
            image,
            *this,
            learn_x,
            learn_y,
            learn_w,
            learn_h,
            false,
            rect_findline_pattern,
            m_fastmatch_learn_a_count,
            m_fastmatch_learn_b_count,
            m_fastmatch_learn_a2_count,
            m_fastmatch_learn_b2_count))
        {
            FindLine::setpattern(rect_findline_pattern);
            m_fastmatch_learn_status_code = 30;
            modelzeroposition();
            gp_Rectangle learned_rect = FindLine::patternboundingrectAB();
            m_imodelwith = static_cast<int>(learned_rect.Width());
            m_imodelheigh = static_cast<int>(learned_rect.Height());
        }
        else if (LearnPatternThroughRectFindline(
            image,
            *this,
            learn_x,
            learn_y,
            learn_w,
            learn_h,
            true,
            rect_findline_pattern,
            m_fastmatch_learn_a_count,
            m_fastmatch_learn_b_count,
            m_fastmatch_learn_a2_count,
            m_fastmatch_learn_b2_count))
        {
            FindLine::setpattern(rect_findline_pattern);
            m_fastmatch_learn_status_code = 31;
            modelzeroposition();
            gp_Rectangle learned_rect = FindLine::patternboundingrectAB();
            m_imodelwith = static_cast<int>(learned_rect.Width());
            m_imodelheigh = static_cast<int>(learned_rect.Height());
        }
        else if (LearnPatternByBoundaryPointPairs(
            image,
            *this,
            learn_x,
            learn_y,
            learn_w,
            learn_h,
            rect_findline_pattern,
            m_fastmatch_learn_a_count,
            m_fastmatch_learn_b_count,
            m_fastmatch_learn_a2_count,
            m_fastmatch_learn_b2_count))
        {
            FindLine::setpattern(rect_findline_pattern);
            m_fastmatch_learn_status_code = 32;
            modelzeroposition();
            gp_Rectangle learned_rect = FindLine::patternboundingrectAB();
            m_imodelwith = static_cast<int>(learned_rect.Width());
            m_imodelheigh = static_cast<int>(learned_rect.Height());
        }
    }

    if (m_fastmatch_learn_a_count +
        m_fastmatch_learn_b_count +
        m_fastmatch_learn_a2_count +
        m_fastmatch_learn_b2_count <= 0)
    {
        m_fastmatch_learn_a_count = std::max(0, static_cast<int>(FindLine::getpatternpathA().ElementCount()));
        m_fastmatch_learn_b_count = std::max(0, static_cast<int>(FindLine::getpatternpathB().ElementCount()));
        m_fastmatch_learn_a2_count = 0;
        m_fastmatch_learn_b2_count = 0;
    }

    if (m_fastmatch_learn_a_count +
        m_fastmatch_learn_b_count +
        m_fastmatch_learn_a2_count +
        m_fastmatch_learn_b2_count <= 0)
        m_fastmatch_learn_status_code = -20;

    setrect(saved_rect_x, saved_rect_y, saved_rect_w, saved_rect_h);
    SetWHgap(saved_wgap, saved_hgap);
    setcomparegap(saved_comparegap);
    setthre(saved_threshold);
    setlinegap(saved_linegap);
}
void FastMatch::ZeroPOS()
{
    modelzeroposition();
    gp_Rectangle arect1 = FindLine::patternboundingrectAB();
    m_imodelwith = static_cast<int>(arect1.Width());
    m_imodelheigh = static_cast<int>(arect1.Height());
}
void FastMatch::Learn_level0(Image& image)
{ 
    Image aimage0;
    aimage0.CopyFrom(&image);
    aimage0.ROIpyrDown(5);
    setthre(50);
    setlinegap(7);
    edgepattern(aimage0);
    modelzeroposition();
    gp_Rectangle arect1 = FindLine::patternboundingrectAB();
    m_imodelwith = static_cast<int>(arect1.Width());
    m_imodelheigh = static_cast<int>(arect1.Height());
}
void FastMatch::Learn_level1(Image& image)
{
    Image aimage0;
    aimage0.CopyFrom(&image);
    aimage0.ROIpyrDown(5);
    setthre(30);
    setlinegap(7);
    edgepattern(aimage0);
    modelzeroposition();
    gp_Rectangle arect1 = FindLine::patternboundingrectAB();
    m_imodelwith = static_cast<int>(arect1.Width());
    m_imodelheigh = static_cast<int>(arect1.Height());
}
void FastMatch::Learn_level2(Image& image)
{
    Image aimage0;
    aimage0.CopyFrom(&image);
    aimage0.ROIpyrDown(3);
    setthre(30);
    setlinegap(6);
    edgepattern(aimage0);
    modelzeroposition();
    gp_Rectangle arect1 = FindLine::patternboundingrectAB();
    m_imodelwith = static_cast<int>(arect1.Width());
    m_imodelheigh = static_cast<int>(arect1.Height());
}
void FastMatch::Learn_level3(Image& image)
{
    Image aimage0;
    aimage0.CopyFrom(&image);
    aimage0.ROIpyrDown(1);
    setthre(10);
    setlinegap(6);
    edgepattern(aimage0);
    modelzeroposition();
    gp_Rectangle arect1 = FindLine::patternboundingrectAB();
    m_imodelwith = static_cast<int>(arect1.Width());
    m_imodelheigh = static_cast<int>(arect1.Height());
}
void FastMatch::Learn_level4(Image& image)
{
    setthre(7);
    setlinegap(3);
    edgepattern(image);
    modelzeroposition();
    gp_Rectangle arect1 = FindLine::patternboundingrectAB();
    m_imodelwith = static_cast<int>(arect1.Width());
    m_imodelheigh = static_cast<int>(arect1.Height());
}
void FastMatch::modelzeroposition()
{
    FindLine::patternzeroposition();
}
void FastMatch::rotatemodelzeropositionAB()
{
    const int isize = static_cast<int>(std::min(m_models_rotate.size(), m_models_rotaterects.size()));
    for (int iz = 0; iz < isize; iz++)
    {
        gp_Rectangle arect1 = m_models_rotate[iz].boundingRectAB();
        m_models_rotate[iz].MoveAB(-static_cast<int>(arect1.TopLeft().X()), -static_cast<int>(arect1.TopLeft().Y()));
        m_models_rotaterects[iz].Move(-static_cast<int>(arect1.TopLeft().X()), -static_cast<int>(arect1.TopLeft().Y()));
    }
}
void FastMatch::rotatemodelzeroposition()
{
    const int isize = static_cast<int>(std::min(m_models_rotate.size(), m_models_rotaterects.size()));
    for (int iz = 0; iz < isize; iz++)
    {
        gp_Rectangle arect1 = m_models_rotate[iz].boundingRect();
        m_models_rotate[iz].Move(-static_cast<int>(arect1.TopLeft().X()), -static_cast<int>(arect1.TopLeft().Y()));
        m_models_rotaterects[iz].Move(-static_cast<int>(arect1.TopLeft().X()), -static_cast<int>(arect1.TopLeft().Y()));
    }
}
void FastMatch::rotatemodel05zeroposition()
{
    const int isize = static_cast<int>(std::min(m_models05_rotate.size(), m_models05_rotaterects.size()));
    for (int iz = 0; iz < isize; iz++)
    {
        gp_Rectangle arect1 = m_models05_rotate[iz].boundingRect();
        m_models05_rotate[iz].Move(-static_cast<int>(arect1.TopLeft().X()), -static_cast<int>(arect1.TopLeft().Y()));
        m_models05_rotaterects[iz].Move(-static_cast<int>(arect1.TopLeft().X()), -static_cast<int>(arect1.TopLeft().Y()));
    }
}
void FastMatch::rotatemodel025zeroposition()
{
    const int isize = static_cast<int>(std::min(m_models025_rotate.size(), m_models025_rotaterects.size()));
    for (int iz = 0; iz < isize; iz++)
    {
        gp_Rectangle arect1 = m_models025_rotate[iz].boundingRect();
        m_models025_rotate[iz].Move(-static_cast<int>(arect1.TopLeft().X()), -static_cast<int>(arect1.TopLeft().Y()));
        m_models025_rotaterects[iz].Move(-static_cast<int>(arect1.TopLeft().X()), -static_cast<int>(arect1.TopLeft().Y()));
    }
}
void FastMatch::learn_level0(void* pimage)
{
    Image* pgetimage = (Image*)pimage;
    if (pgetimage == nullptr)
        return;
    Learn_level0(*pgetimage);

}
void FastMatch::learn_level1(void* pimage)
{
    Image* pgetimage = (Image*)pimage;
    if (pgetimage == nullptr)
        return;
    Learn_level1(*pgetimage);

}
void FastMatch::learn_level2(void* pimage)
{
    Image* pgetimage = (Image*)pimage;
    if (pgetimage == nullptr)
        return;
    Learn_level2(*pgetimage);
}
void FastMatch::learn_level3(void* pimage)
{
    Image* pgetimage = (Image*)pimage;
    if (pgetimage == nullptr)
        return;
    Learn_level3(*pgetimage);
}
void FastMatch::learn_level4(void* pimage)
{
    Image* pgetimage = (Image*)pimage;
    if (pgetimage == nullptr)
        return;
    Learn_level4(*pgetimage);
}

void FastMatch::learn(void* pimage)
{
    m_debug_last_learn_argument = pimage;

    CXLOG_INFO("FastMatch", "learn_void_enter", "running",
        pimage == nullptr ? "pimage=null" : "pimage=non_null");
    Image* pgetimage = (Image*)pimage;

    if (pgetimage == nullptr || pgetimage->getmat().empty())
    {
        m_modelpoints_sample1.clear();
        m_modelpoints_sample2.clear();
        m_modelpoints_sample3.clear();
        m_fastmatch_learn_status_code = -10;
        m_fastmatch_learn_a_count = 0;
        m_fastmatch_learn_b_count = 0;
        m_fastmatch_learn_a2_count = 0;
        m_fastmatch_learn_b2_count = 0;
        return;
    }

    Learn(*pgetimage);
}
void FastMatch::savemodelfile(const char* pchar)
{
    ZeroPOS();
    FindLine::savepatternfile(pchar);
}
void FastMatch::loadmodelfile(const char* pchar)
{
    FindLine::loadpatternfile(pchar);
    ZeroPOS();
    gp_Rectangle arect1 = FindLine::patternboundingrectAB();
    m_imodelwith = static_cast<int>(arect1.Width());
    m_imodelheigh = static_cast<int>(arect1.Height());
}
void FastMatch::loadrotatemodelfile(const char* pchar)
{
    FindLine::loadpatternfile(pchar);
    ZeroPOS();
    gp_Rectangle arect1 = FindLine::patternboundingrectAB();
    m_imodelwith = static_cast<int>(arect1.Width());
    m_imodelheigh = static_cast<int>(arect1.Height());

    m_models_rotate.clear();

    m_models_rotaterects.clear();

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
        amodelpoints = FindLine::getpattern();
        brectpoints = arectpoints;
        bmodelrect = brectpoints;
        amodelpoints.RotateAB(ianglecur);
        bmodelrect.Rotate(ianglecur);

        m_models_rotate.push_back(amodelpoints);
        m_models_rotaterects.push_back(bmodelrect);
    }

    rotatemodelzeropositionAB();
}
void FastMatch::loadrotate05modelfile(const char* pchar)
{
    FindLine::loadpatternfile(pchar);
    ZeroPOS();
    gp_Rectangle arect1 = FindLine::patternboundingrect();
    m_imodelwith = static_cast<int>(arect1.Width());
    m_imodelheigh = static_cast<int>(arect1.Height());

    m_models05_rotate.clear();

    m_models05_rotaterects.clear();

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
        amodelpoints = FindLine::getpattern();
        brectpoints = arectpoints;
        bmodelrect = brectpoints;
        amodelpoints.Rotate(danglecur);
        bmodelrect.Rotate(danglecur);

        m_models05_rotate.push_back(amodelpoints);
        m_models05_rotaterects.push_back(bmodelrect);
    }

    rotatemodel05zeroposition();
}
void FastMatch::loadrotate025modelfile(const char* pchar)
{
    FindLine::loadpatternfile(pchar);
    ZeroPOS();
    gp_Rectangle arect1 = FindLine::patternboundingrect();
    m_imodelwith = static_cast<int>(arect1.Width());
    m_imodelheigh = static_cast<int>(arect1.Height());

    m_models025_rotate.clear();

    m_models025_rotaterects.clear();

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
        amodelpoints = FindLine::getpattern();
        brectpoints = arectpoints;
        bmodelrect = brectpoints;
        amodelpoints.Rotate(danglecur);
        bmodelrect.Rotate(danglecur);

        m_models025_rotate.push_back(amodelpoints);
        m_models025_rotaterects.push_back(bmodelrect);
    }

    rotatemodel025zeroposition();
}
void FastMatch::ABtoShape(std::vector<cv::Point2f>& points)
{
    return FindLine::ABtoShape(points);
}
int FastMatch::ABpatternsize()
{
    return FindLine::ABpatternsize();
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
void FastMatch::loadcalibration(const char* pchar)
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
    
    delete[]pcharget;
    fclose(rf);

}
void FastMatch::savecalibration(const char* pchar)
{
    (void)pchar;
    
}
void FastMatch::setrotateangle(double dangle)
{
    m_danglegap = FastMatchPositiveFiniteOr(dangle, m_danglegap);
}
void FastMatch::setrotateanglescale(double dangle1, double dangle2)
{
    m_dangle_add = FastMatchFiniteOr(dangle2, m_dangle_add);
    m_dangle_mud = FastMatchFiniteOr(dangle1, m_dangle_mud);
}
void FastMatch::clearmodels_l12()
{
    m_models_l12.clear();
}
void FastMatch::addmodels_l12(const char* pchar)
{
    FindLine::loadpatternfile(pchar);
    m_models_l12.push_back(FindLine::getpattern());
}
void FastMatch::clearmodels_l36()
{
    m_models_l36.clear();
}
void FastMatch::addmodels_l36(const char* pchar)
{
    FindLine::loadpatternfile(pchar);
    m_models_l36.push_back(FindLine::getpattern());
}
void FastMatch::clearmodels_l72()
{
    m_models_l72.clear();
}
void FastMatch::addmodels_l72(const char* pchar)
{
    FindLine::loadpatternfile(pchar);
    m_models_l72.push_back(FindLine::getpattern());
}
void FastMatch::clearmodels_rotate()
{
    m_models_rotate.clear();
}
void FastMatch::addmodels_rotate(const char* pchar)
{
    FindLine::loadpatternfile(pchar);
    m_models_rotate.push_back(FindLine::getpattern());
}
void FastMatch::setcurmodels(int inum)
{
    if (inum >= 0 && inum < static_cast<int>(m_models_l12.size()))
        FindLine::setpattern(m_models_l12[inum]);
}
void FastMatch::setcurimagemodels(int inum)
{
    m_pgrid->SetUnit(12, 12);
    m_pgrid->UnitGrid();

    m_pgrid->ModelGridMethod_Gauss();
    m_imagefastmodel = m_pgrid->getfastmodel();

    if (inum >= 0 && inum < static_cast<int>(m_imagefastmodels_l12.size()))
        m_imagefastmodels_l12[inum] = m_imagefastmodel;
}
void FastMatch::modelstocurrent_l72(int i)
{
    if (i >= 0 && i < static_cast<int>(m_models_l72.size()))
        FindLine::setpattern(m_models_l72[i]);
}
void FastMatch::modelstocurrent_l36(int i)
{
    if (i >= 0 && i < static_cast<int>(m_models_l36.size()))
        FindLine::setpattern(m_models_l36[i]);
}
void FastMatch::modelstocurrent_l12(int i)
{
    if (i >= 0 && i < static_cast<int>(m_models_l12.size()))
        FindLine::setpattern(m_models_l12[i]);
}
void FastMatch::modelstocurrent_l3(int i)
{
    if (i >= 0 && i < static_cast<int>(m_models_l3.size()))
        FindLine::setpattern(m_models_l3[i]);
}
void FastMatch::modelstocurrent_l6(int i)
{
    if (i >= 0 && i < static_cast<int>(m_models_l6.size()))
        FindLine::setpattern(m_models_l6[i]);
}
void FastMatch::patternrootgrid(double itype, double drate, double ilevel)
{
    FindLine::patternrootgrid(itype, drate, ilevel);
}
void FastMatch::patterntranform(int igap, int itype, int isgap, int iline)
{
    FindLine::patterntranform(igap, itype, isgap, iline);
    FindLine::patternzeroposition();
}
void FastMatch::patterngap2gap(int inewgap)
{
    FindLine::patterngap2gap(inewgap);
}
void FastMatch::patternABgap2gap(double dnewgaprate)
{
    FindLine::patternABgap2gap(dnewgaprate);
}
void FastMatch::patternABsample(int irate)
{
    FindLine::patternABsample(irate);
}
void FastMatch::pattern2org()
{
    FindLine::pattern2org(); 
}
void FastMatch::org2pattern()
{
    FindLine::org2pattern();
}
void FastMatch::patternzoom(double dx, double dy, double igap, double itype)
{
    FindLine::patternzoom(dx, dy, igap, itype);
    FindLine::patternzeroposition();
}
void FastMatch::modelrotate(double dangle)
{
    FindLine::patternrotate(dangle);
}
void FastMatch::modelzoom(double dx, double dy)
{
    FindLine::modelzoom(dx, dy);
}
void FastMatch::setmodelwh(int iw, int ih)
{
    gp_Rectangle rectf = FindLine::patternboundingrect();
    iw = FastMatchPositiveInt(iw);
    ih = FastMatchPositiveInt(ih);
    int iorgw = FastMatchPositiveInt(static_cast<int>(rectf.Width()));
    int iorgh = FastMatchPositiveInt(static_cast<int>(rectf.Height()));
    double dw = (iw * 1.0) / (iorgw * 1.0);
    double dh = (ih * 1.0) / (iorgh * 1.0);
    modelzoom(dw, dh);
}
void FastMatch::setmatchrect(int ix, int iy, int iw, int ih)
{
    ix = FastMatchNonNegativeInt(ix);
    iy = FastMatchNonNegativeInt(iy);
    iw = FastMatchPositiveInt(iw);
    ih = FastMatchPositiveInt(ih);
    m_search_roi_x = ix;
    m_search_roi_y = iy;
    m_search_roi_w = iw;
    m_search_roi_h = ih;
    m_matchrect = gp_Rectangle(gp_Pnt(ix, iy,0), gp_Pnt(ix + iw, iy + ih,0));
    if (m_matchrects.size() <= 0)
    {
        gp_Rectangle arect(gp_Pnt(ix, iy, 0), gp_Pnt(ix + iw, iy + ih, 0));
        m_matchrects.addrect(arect);
    }
    else
        m_matchrects.setrect(0,ix,iy,iw,ih);
}

void FastMatch::setrectxywh(int ix, int iy, int iw, int ih)
{
    setrect(ix, iy, iw, ih);
}

void FastMatch::setmatchrectxywh(int ix, int iy, int iw, int ih)
{
    setmatchrect(ix, iy, iw, ih);
}

void FastMatch::setrectxywh_script(int ih, int iw, int iy, int ix)
{
    setrectxywh(ix, iy, iw, ih);
}

void FastMatch::setmatchrectxywh_script(int ih, int iw, int iy, int ix)
{
    setmatchrectxywh(ix, iy, iw, ih);
}

void FastMatch::setexpectedrect(double x0, double y0, double x1, double y1)
{
    const double min_x = std::min(x0, x1);
    const double min_y = std::min(y0, y1);
    const double max_x = std::max(x0, x1);
    const double max_y = std::max(y0, y1);
    m_expected_rect = gp_Rectangle(gp_Pnt(min_x, min_y, 0), gp_Pnt(max_x, max_y, 0));
}

void FastMatch::setmatchrectnum(int inum)
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
gp_Rectangle& FastMatch::getmatchrect()
{
    return m_matchrect;
}
RectsShape& FastMatch::getmatchrects()
{
    return m_matchrects;
}
gp_Rectangle FastMatch::getresultrect(int inum) const
{
    if (inum < 0 || inum >= m_resultrects.size())
        return gp_Rectangle(gp_Pnt(0, 0, 0), gp_Pnt(0, 0, 0));
    gp_Rectangle arect0 = m_resultrects.getrect(inum);
    return arect0;
}

gp_Rectangle FastMatch::getresolvedresultrect(int inum) const
{
    if (inum < 0 || inum >= static_cast<int>(m_resultpoints.size()))
        return gp_Rectangle(gp_Pnt(0, 0, 0), gp_Pnt(0, 0, 0));

    int rectw = FastMatchPositiveInt(m_imodelwith);
    int recth = FastMatchPositiveInt(m_imodelheigh);
    if (rectw <= 0)
    {
        const gp_Rectangle raw_rect = getresultrect(inum);
        rectw = FastMatchPositiveInt(static_cast<int>(raw_rect.BottomRight().X()));
    }
    if (recth <= 0)
    {
        const gp_Rectangle raw_rect = getresultrect(inum);
        recth = FastMatchPositiveInt(static_cast<int>(raw_rect.BottomRight().Y()));
    }

    const double cx = m_resultpoints.at(inum).X() + rectw / 2.0;
    const double cy = m_resultpoints.at(inum).Y() + recth / 2.0;
    const double x0 = cx - rectw / 2.0;
    const double y0 = cy - recth / 2.0;
    return gp_Rectangle(
        gp_Pnt(x0, y0, 0),
        gp_Pnt(x0 + rectw, y0 + recth, 0));
}
void FastMatch::setmultimatchrect(int inum, int ix, int iy, int iw, int ih)
{
    if (inum >= 0 && inum < m_matchrects.size())
    {
        const int rectx = FastMatchNonNegativeInt(ix);
        const int recty = FastMatchNonNegativeInt(iy);
        m_matchrects.setrect(inum, rectx, recty, FastMatchPositiveInt(iw), FastMatchPositiveInt(ih));
    }
}
void FastMatch::resultclear()
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
void FastMatch::resulttolist(gp_Pnt& apoint, int inum)
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
        if(0)
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



NextRun01:
    NormalizeMatchCandidates(
        m_resultnums,
        m_resultpoints,
        std::max(1, m_imaxmatchnum),
        m_iminfindnum,
        m_iminpointkey);
}
void FastMatch::resultsort()
{
    NormalizeMatchCandidates(
        m_resultnums,
        m_resultpoints,
        std::max(1, m_imaxmatchnum),
        m_iminfindnum,
        m_iminpointkey);
}
void FastMatch::rotateresultsortfilter(int ifdx, int ifdy, int itype)
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
void FastMatch::rotateresultsortfilterA(int ifdx, int ifdy, int itype)
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
 
int FastMatch::rotateresultsize()
{
    return static_cast<int>(RotateResultSharedCount(
        m_rotateresults,
        m_rotatereslutpoints,
        m_rotatereslutangles,
        m_rotateshaperesults));
}
void FastMatch::rotateresultsort()
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

double FastMatch::getrotateresultx()
{
    if (!m_rotatereslutpoints.empty())
    {
        gp_Pnt apoint = m_rotatereslutpoints[0];
        return apoint.X();
    }
    return -9999;
}
double FastMatch::getrotateresulty()
{
    if (!m_rotatereslutpoints.empty())
    {
        gp_Pnt apoint = m_rotatereslutpoints[0];
        return apoint.Y();
    }
    return -9999;
}
double FastMatch::getrotateresulta()
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
double FastMatch::getrotateresultscore()
{
    if (!m_rotateresults.empty())
    {
        double drotateresult = m_rotateresults[0];
        return drotateresult;
    }
    return 0;
}
double FastMatch::getrotateresultscoreA(int inum)
{
    if (inum >= 0 && inum < static_cast<int>(m_rotateresults.size()))
    {
        double drotateresult = m_rotateresults[inum];
        return drotateresult;
    }
    return 0;
}

void FastMatch::clusterclear()
{
    m_clusters.clear();
}
void FastMatch::resultcluster(int ixgap, int iygap, int ianglegap)
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
        
    } 
}
void FastMatch::Distfilter()
{
#if defined USE_AI
    gp_Path& pathA = FindLine::getpatternpathA();
    gp_Path& pathB = FindLine::getpatternpathB();
    size_t numPoints = pathA.getpoints().size();
    if (numPoints>0)
    {
        arma::mat points(2, numPoints);
        for (size_t i = 0; i < numPoints; ++i)
        {
            points(0, i) = pathA.getpoints()[i].X();
            points(1, i) = pathA.getpoints()[i].Y();
        }
        pathA.Clear();
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
            points(0, i) = pathB.getpoints()[i].X();
            points(1, i) = pathB.getpoints()[i].Y();
        }
        pathB.Clear();
        auto filteredIndices = mlpackclass::AdaptiveFilterWithCoincidenceHandling_(points);
        for (auto idx : filteredIndices)
            pathB.AddPoint(gp_Pnt(points(0, idx), points(1, idx), 0));

    } 
#endif
}

void FastMatch::MatchAB(Image& image)
{
    m_matchimage = &image;
    resultclear();
    ++m_matchab_call_count;
    m_match_last_stage = 20;
    gp_Path& pathA = FindLine::getpatternpathA();
    gp_Path& pathB = FindLine::getpatternpathB();

    const int icount1 = static_cast<int>(pathA.ElementCount());
    const int icount2 = static_cast<int>(pathB.ElementCount());
    const int pair_count = std::min(icount1, icount2);
    CXLOG_INFO("FastMatch", "match_ab_enter", "running",
        "image=" + std::to_string(image.getWidth()) + "x" + std::to_string(image.getHeight()) +
        " learn_roi=" + std::to_string(m_learn_roi_x) + "," +
        std::to_string(m_learn_roi_y) + "," +
        std::to_string(m_learn_roi_w) + "," +
        std::to_string(m_learn_roi_h) +
        " search_roi=" + std::to_string(m_search_roi_x) + "," +
        std::to_string(m_search_roi_y) + "," +
        std::to_string(m_search_roi_w) + "," +
        std::to_string(m_search_roi_h) +
        " pathA=" + std::to_string(icount1) +
        " pathB=" + std::to_string(icount2) +
        " pairs=" + std::to_string(pair_count));
    if (pair_count <= 0)
    {
        m_match_last_stage = 32;
        return;
    }

    MatchSampleAB(image, pathA, pathB);
}
void FastMatch::MatchABMore(Image& image)
{
    m_matchimage = &image;
    resultclear();
    gp_Path& pathA = FindLine::getpatternpathA();
    gp_Path& pathB = FindLine::getpatternpathB();

    MatchSampleABMore(image, pathA, pathB);
}
void FastMatch::match(void* pimage)
{
    Image* pgetimage = (Image*)pimage;
    if (pgetimage == nullptr)
    {
        m_match_last_stage = 10;
        return;
    }

    ++m_match_call_count;
    m_match_last_stage = 11;
    MatchAB(*pgetimage);
}
 
void FastMatch::matchmore(void* pimage)
{
    Image* pgetimage = (Image*)pimage;
    if (pgetimage == nullptr)
        return;

    MatchABMore(*pgetimage);
}
void FastMatch::loadfastimagemodel(const char* pfilename)
{
    m_pgrid->loadmapmodel(pfilename);
    m_imagefastmodel = m_pgrid->getfastmodel();
}
vector<int>* FastMatch::getcurimagemodel()
{
    return &m_imagefastmodel;
}
void FastMatch::imagemodesclear_l12()
{
    m_imagefastmodels_l12.clear();
}
void FastMatch::addimagemodels_l12(const char* pfilename)
{
    m_pgrid->loadmapmodel(pfilename);

    {
        m_pgrid->ZeroModel();
        m_pgrid->ReGrid(12, 12);
    }
    m_pgrid->Grid2PattenModel(FindLine::getconparegap());
    m_models_l12.push_back(m_pgrid->getpatmodel());

    easyobj aeobj = m_pgrid->ModelGridMethod_ObjectA();
    m_easyobjectmodels_l12.push_back(aeobj);

    m_pgrid->ModelGridMethod_Gauss();

    m_imagefastmodel = m_pgrid->getfastmodel();
    m_imagefastmodels_l12.push_back(m_imagefastmodel);

}
void FastMatch::imagemodesclear_l36()
{
    m_imagefastmodels_l36.clear();
}
void FastMatch::addimagemodels_l36(const char* pfilename)
{
    m_pgrid->loadmapmodel(pfilename);
    m_pgrid->ZeroModel();
    m_pgrid->ReGrid(36, 36);
    if (0)
    {
        m_pgrid->SetUnit(36, 36);
        m_pgrid->UnitGrid();
    }

    m_pgrid->Grid2PattenModel(FindLine::getconparegap());
    m_models_l36.push_back(m_pgrid->getpatmodel());

    easyobj aeobj = m_pgrid->ModelGridMethod_ObjectA();
    m_easyobjectmodels_l36.push_back(aeobj);

    m_pgrid->ModelGridMethod_Gauss();

    m_imagefastmodel = m_pgrid->getfastmodel();
    m_imagefastmodels_l36.push_back(m_imagefastmodel);

}
void FastMatch::imagemodesclear_l72()
{
    m_imagefastmodels_l72.clear();
}
void FastMatch::addimagemodels_l72(const char* pfilename)
{
    m_pgrid->loadmapmodel(pfilename);
    m_pgrid->ZeroModel();
    m_pgrid->ReGrid(72, 72);
    if (0)
    {
        m_pgrid->SetUnit(72, 72);
        m_pgrid->UnitGrid();
    }

    m_pgrid->Grid2PattenModel_org(FindLine::getconparegap());
    m_models_l72.push_back(m_pgrid->getpatmodel());

    easyobj aeobj = m_pgrid->ModelGridMethod_ObjectA();
    m_easyobjectmodels_l72.push_back(aeobj);

    m_pgrid->ModelGridMethod_Gauss();

    m_imagefastmodel = m_pgrid->getfastmodel();
    m_imagefastmodels_l72.push_back(m_imagefastmodel);

}

Grid* FastMatch::getgrid()
{
    return m_pgrid;
}
bool FastMatch::modelcompare(vector<int>& modela, vector<int>& modelb)
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
map<int, int >& FastMatch::getlevel3_6map()
{
    return m_mapl3_l6;
}
map<int, int >& FastMatch::getlevel6_12map()
{
    return m_mapl6_l12;
}
map<int, int >& FastMatch::getlevel12_36map()
{
    return m_mapl12_l36;
}
map<int, int >& FastMatch::getlevel36_72map()
{
    return m_mapl36_l72;
}
void FastMatch::clearmodel()
{
    m_easyobjectmodels_l12.clear();
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
void FastMatch::list_duplicatesmodel_l12()
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
void FastMatch::list_duplicatesmodel_l36()
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
void FastMatch::list_duplicatesmodel_l72()
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

vector<int>& FastMatch::getduplicateslist_l36()
{
    return m_duplicates_list_l36;
}
vector<int>& FastMatch::getduplicateslist_l12()
{
    return m_duplicates_list_l12;
}

void FastMatch::modelmethod(int itype)
{
    m_pgrid->SetFastModel(m_imagefastmodel);
    m_pgrid->ReSetModelGrid();
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
void FastMatch::levelmodels_l72tol36()
{


    const int isize = static_cast<int>(m_imagefastmodels_l72.size());

    for (int i = 0; i < isize; i++)
    {
        m_pgrid->SetModelWH(72, 72);
        m_pgrid->SetFastModel(m_imagefastmodels_l72[i]);
        m_pgrid->ReSetModelGrid();
        m_pgrid->ZeroModel();
        m_pgrid->GridZoom(36, 36);
        m_pgrid->SetUnit(36, 36);
        m_pgrid->Grid2PattenModel(FindLine::getconparegap());

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

void FastMatch::levelmodels_l36tol12()
{


    const int isize = static_cast<int>(m_imagefastmodels_l36.size());

    for (int i = 0; i < isize; i++)
    {
        m_pgrid->SetModelWH(36, 36);
        m_pgrid->SetFastModel(m_imagefastmodels_l36[i]);
        m_pgrid->ReSetModelGrid();
        m_pgrid->ZeroModel();
        m_pgrid->GridZoom(12, 12);
        m_pgrid->SetUnit(12, 12);
        m_pgrid->Grid2PattenModel(FindLine::getconparegap());

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

void FastMatch::levelmodels_l12tol6()
{


    const int isize = static_cast<int>(m_imagefastmodels_l12.size());


    for (int i = 0; i < isize; i++)
    {
        m_pgrid->SetModelWH(12, 12);
        m_pgrid->SetFastModel(m_imagefastmodels_l12[i]);

        m_pgrid->ReSetModelGrid();
        m_pgrid->ZeroModel();
        m_pgrid->GridZoom(6, 6);
        m_pgrid->SetUnit(6, 6);
        m_pgrid->Grid2PattenModel(FindLine::getconparegap());

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

void FastMatch::levelmodels_l6tol3()
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
        m_pgrid->Grid2PattenModel(FindLine::getconparegap());

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
void FastMatch::savelevel0_l1()
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
void FastMatch::imagemodelstocurrent_l72(int i)
{
    m_pgrid->SetModelWH(72, 72);

    if (i >= 0 && i < static_cast<int>(m_imagefastmodels_l72.size()))
        m_imagefastmodel = m_imagefastmodels_l72[i];
}
void FastMatch::imagemodelstocurrent_l36(int i)
{
    m_pgrid->SetModelWH(36, 36);

    if (i >= 0 && i < static_cast<int>(m_imagefastmodels_l36.size()))
        m_imagefastmodel = m_imagefastmodels_l36[i];
}
void FastMatch::imagemodelstocurrent_l12(int i)
{
    m_pgrid->SetModelWH(12, 12);

    if (i >= 0 && i < static_cast<int>(m_imagefastmodels_l12.size()))
        m_imagefastmodel = m_imagefastmodels_l12[i];
}

void FastMatch::imagemodelstocurrent_l3(int i)
{
    m_pgrid->SetModelWH(3, 3);
    if (i >= 0 && i < static_cast<int>(m_imagefastmodels_l3.size()))
        m_imagefastmodel = m_imagefastmodels_l3[i];
}
void FastMatch::imagemodelstocurrent_l6(int i)
{
    m_pgrid->SetModelWH(6, 6);

    if (i >= 0 && i < static_cast<int>(m_imagefastmodels_l6.size()))
        m_imagefastmodel = m_imagefastmodels_l6[i];
}

int FastMatch::imagefastmodelsize(int ilevel)
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

void FastMatch::objectmodelstocurrent(int i)
{
    if (i >= 0 && i < static_cast<int>(m_easyobjectmodels_l12.size()))
        m_easyobject = m_easyobjectmodels_l12[i];
}
void FastMatch::savefastimagemodel(const char* pfilename)
{
    m_pgrid->ReSetModelGrid();

    m_pgrid->savemapmodel(pfilename);

}
void FastMatch::savefastimagepatmodel(const char* pfilename)
{
    m_pgrid->Grid2PattenModel(FindLine::getconparegap());
    m_pgrid->savemodelfile(pfilename);
}
void FastMatch::savematchroi(const char* pfilename)
{
    if (m_matchimage == nullptr)
        return;
    SaveMatchROI(*m_matchimage, pfilename);
}
void FastMatch::savematchimagemodel(const char* pfilename)
{
    if (g_pmodelimage == nullptr)
        return;
    g_pmodelimage->SaveROI(pfilename);
}
void FastMatch::SaveMatchROI(Image& image, const char* pfilename)
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
void FastMatch::imagelearn(int ithre1, int iandor)
{
    if (m_matchimage == nullptr)
        return;
    MatchImageLearn(*m_matchimage, ithre1, iandor);
}
void FastMatch::imagelearnmass(int ithre1, int iandor, int igridwh)
{
    if (m_matchimage == nullptr)
        return;
    MatchImageLearnMass(*m_matchimage, ithre1, iandor, igridwh);
}
void FastMatch::imagelearncheck(int iimagetype, int iandor, int igridwh)
{
    if (m_matchimage == nullptr)
        return;
    MatchImageCheck(*m_matchimage, iimagetype, iandor, igridwh);
}

void FastMatch::imagelearnex(int ithre1, int iandor, int igrid)
{
    if (m_matchimage == nullptr)
        return;
    MatchImageLearnEx(*m_matchimage, ithre1, iandor, igrid);
}
void FastMatch::MatchImageLearn(Image& aimage, int ithre1, int iandor)
{
    if (g_pmodelimage == nullptr)
        return;
    (void)iandor;
    const int isize = m_resultrects.size();
    if (isize <= 0)
        return;

    gp_Rectangle arect0 = m_resultrects.getrect(isize - 1);
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
int FastMatch::GetRectGridLevel(int irectw)
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
void FastMatch::MatchImageLearnEx(Image& aimage, int ithre1, int iandor, int igrid)
{
    if (g_pmodelimage == nullptr)
        return;
    (void)iandor;
    const int isize = m_resultrects.size();
    if (isize <= 0)
        return;

    gp_Rectangle arect0 = m_resultrects.getrect(isize - 1);

    m_pgrid->SetUnit(igrid, igrid);
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
    m_pgrid->SetUnit(igrid, igrid);
    m_pgrid->UnitGrid();

    m_pgrid->ModelGridMethod_Gauss();
    m_imagefastmodel = m_pgrid->getfastmodel();
}
void FastMatch::MatchImageLearnMass(Image& aimage, int ithre1, int iandor, int igrid)
{
    if (g_pmodelimage == nullptr)
        return;
    (void)iandor;
    const int isize = m_resultrects.size();
    if (isize <= 0)
        return;
    m_pgrid->setgrid(10, 10, igrid, igrid, 10, 10);
    m_pgrid->SetModelWH(igrid, igrid);
    

    gp_Rectangle arect0 = m_resultrects.getrect(isize - 1);

    m_pgrid->SetUnit(igrid, igrid);
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
    m_pgrid->SetUnit(igrid, igrid);
    m_pgrid->UnitGrid();

    m_pgrid->ModelGridMethod_Gauss();
    m_imagefastmodel = m_pgrid->getfastmodel();

}
void FastMatch::MatchImageCheck(Image& aimage, int iimagetype, int iandor, int igrid)
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
    aimage.SetMode(iimagetype);
    aimage.ROItoROI(*g_pmodelimage);

    g_pmodelimage->setroi(0, 0, igrid, igrid);
    m_pgrid->ROIImagetoModel(*g_pmodelimage);


    m_imagefastmatchlist = m_pgrid->getfastmodel();

    isize = static_cast<int>(m_imagefastmodel.size());
    int ingsize = 0;
    int ioksize = 0;
    for (int i = 0; i < isize; i++)
    {
        int ia = m_imagefastmatchlist[i];
        int io = m_imagefastmodel[i];
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

void FastMatch::imagematch(int ithre1, int iandor, int igrid, int ineedthre)
{
    if (m_matchimage == nullptr)
        return;
    MatchImageMatch(*m_matchimage, ithre1, iandor, igrid, ineedthre);
}

void FastMatch::imagematch_grid(int ithre1, int iandor, int igrid)
{
    if (m_matchimage == nullptr)
        return;
    MatchImageMatch(*m_matchimage, ithre1, iandor, igrid);
}

void FastMatch::imagematchex(int igrid)
{
    if (m_matchimage == nullptr)
        return;
    MatchImageExMatch(*m_matchimage, igrid);
}

void FastMatch::MatchImageMatch(Image& aimage, int ithre1, int iandor, int igrid, int ineedthre)
{
    if (g_pmodelimage == nullptr)
        return;
    (void)iandor;
    int isize = m_resultrects.size();
    if (isize <= 0)
        return;
    gp_Rectangle arect0 = m_resultrects.getrect(isize - 1);
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

    if (0)
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

void FastMatch::MatchImageExMatch(Image& aimage, int igrid)
{
    if (g_pmodelimage == nullptr)
        return;
    int isize = m_resultrects.size();
    if (isize <= 0)
        return;
    gp_Rectangle arect0 = m_resultrects.getrect(isize - 1);
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
void FastMatch::MatchGrid(Grid* pgrid)
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

double FastMatch::getimagemodelreslut()
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

double FastMatch::getimagemodelreslut_check_1()
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
void FastMatch::imagemodelcomparegrid(int itype)
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
void FastMatch::imagemodelcompareshow(int itype)
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
double FastMatch::imagegridresult(int itype)
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
void FastMatch::imagemodelshow()
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
void FastMatch::matchstepgap(int ix, int iy)
{
    m_stepgapx = FastMatchPositiveInt(ix);
    m_stepgapy = FastMatchPositiveInt(iy);
}

void FastMatch::imagematchshow()
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
void FastMatch::setminscore(double dminscore)
{
    m_dminscore = FastMatchUnitScore(dminscore);
}
void FastMatch::MatchSample(Image& image, gp_Path& path)
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
        return;
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
    int iw = static_cast<int>(FindLine::patternboundingrect().Width());
    int ih = static_cast<int>(FindLine::patternboundingrect().Height());
    int itotalsize = static_cast<int>(FindLine::getpattern().size());

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

void FastMatch::MatchSampleAB(Image& image, gp_Path& pathA, gp_Path& pathB)
{
    ++m_matchsampleab_call_count;
    m_match_last_stage = 30;
    int ithre = m_imatchthre;
    int icurmodule = ImageManager::GetCurMode();
    Image* pimage = ImageManager::GetTransferImage(icurmodule);
    (void)pimage;
    int ix0 = static_cast<int>(m_matchrect.TopLeft().X());
    int iy0 = static_cast<int>(m_matchrect.TopLeft().Y());
    int ix1 = static_cast<int>(m_matchrect.BottomRight().X());
    int iy1 = static_cast<int>(m_matchrect.BottomRight().Y());
    m_match_debug_image_width = image.getWidth();
    m_match_debug_image_height = image.getHeight();
    m_match_debug_rect_x0 = ix0;
    m_match_debug_rect_y0 = iy0;
    m_match_debug_rect_x1 = ix1;
    m_match_debug_rect_y1 = iy1;
    const int icount1 = static_cast<int>(pathA.ElementCount());
    const int icount2 = static_cast<int>(pathB.ElementCount());
    const int pair_count = std::min(icount1, icount2);
    if (pair_count <= 0)
    {
        m_match_last_stage = 32;
        return;
    }

    gp_Rectangle arect1 = pathA.boundingRect();
    gp_Rectangle arect2 = pathB.boundingRect();
    (void)arect2;
    const int pattern_w = static_cast<int>(std::ceil(arect1.Width()));
    const int pattern_h = static_cast<int>(std::ceil(arect1.Height()));
    if (pattern_w <= 0 || pattern_h <= 0)
    {
        m_match_last_stage = 33;
        return;
    }

    if (ix0 < 0)
        ix0 = 0;
    if (iy0 < 0)
        iy0 = 0;
    if (ix1 > image.getWidth())
        ix1 = image.getWidth();
    if (iy1 > image.getHeight())
        iy1 = image.getHeight();
    if (image.getWidth() < ix1
        || image.getHeight() < iy1)
    {
        m_match_last_stage = 31;
        return;
    }
    if (ix1 - ix0 <= pattern_w || iy1 - iy0 <= pattern_h)
    {
        m_match_last_stage = 34;
        return;
    }

    CXLOG_INFO("FastMatch", "match_sample_enter", "running",
        "search=" + std::to_string(ix0) + "," +
        std::to_string(iy0) + "," +
        std::to_string(ix1) + "," +
        std::to_string(iy1) +
        " pattern=" + std::to_string(pattern_w) + "x" + std::to_string(pattern_h) +
        " pairs=" + std::to_string(pair_count));

    m_iminfindnum = -1;

    cv::Vec3b pixel0, pixel1;
    int icalnum = 0;
    int icalng = 0;

    gp_Pnt aele;

    int igapx = FastMatchPositiveInt(m_stepgapx);
    int igapy = FastMatchPositiveInt(m_stepgapy);
    iy1 = iy1 - pattern_h;
    ix1 = ix1 - pattern_w;
    if (ix1 <= ix0 || iy1 <= iy0)
    {
        m_match_last_stage = 35;
        return;
    }
    int ix = 0;
    int iy = 0;
    int iw = pattern_w;
    int ih = pattern_h;
    int itotalsize = pair_count;

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
            for (int i = 0; i < pair_count; i++)
            {
                aele = pathA.ElementAt(i);
                const int ax = static_cast<int>(aele.X() + imovx);
                const int ay = static_cast<int>(aele.Y() + imovy);
                aele = pathB.ElementAt(i);
                const int bx = static_cast<int>(aele.X() + imovx);
                const int by = static_cast<int>(aele.Y() + imovy);
                if (!FastMatchPointInsideImage(image, ax, ay) ||
                    !FastMatchPointInsideImage(image, bx, by))
                {
                    icalng = iminfindngnum + 1;
                    goto NextStep_1;
                }
                pixel0 = image.pixel(ax, ay);
                pixel1 = image.pixel(bx, by);

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
void FastMatch::MatchSampleABMore(Image& image, gp_Path& pathA, gp_Path& pathB)
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
        return;
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

int FastMatch::getrawmatchprobecount() const
{
    return m_rawmatch_probe_count;
}

int FastMatch::getrawmatchthresholdhitcount() const
{
    return m_rawmatch_threshold_hit_count;
}

int FastMatch::getmatchcallcount() const
{
    return m_match_call_count;
}

int FastMatch::getmatchabcallcount() const
{
    return m_matchab_call_count;
}

int FastMatch::getmatchsampleabcallcount() const
{
    return m_matchsampleab_call_count;
}

int FastMatch::getmatchlaststage() const
{
    return m_match_last_stage;
}

int FastMatch::getmatchimagewidth() const
{
    return m_match_debug_image_width;
}

int FastMatch::getmatchimageheight() const
{
    return m_match_debug_image_height;
}

int FastMatch::getlearnrectx0() const
{
    return m_learn_roi_x;
}

int FastMatch::getlearnrecty0() const
{
    return m_learn_roi_y;
}

int FastMatch::getlearnrectx1() const
{
    return m_learn_roi_x + FastMatchPositiveInt(m_learn_roi_w);
}

int FastMatch::getlearnrecty1() const
{
    return m_learn_roi_y + FastMatchPositiveInt(m_learn_roi_h);
}

int FastMatch::getmatchrectx0() const
{
    return m_search_roi_x;
}

int FastMatch::getmatchrecty0() const
{
    return m_search_roi_y;
}

int FastMatch::getmatchrectx1() const
{
    return m_search_roi_x + FastMatchPositiveInt(m_search_roi_w);
}

int FastMatch::getmatchrecty1() const
{
    return m_search_roi_y + FastMatchPositiveInt(m_search_roi_h);
}

int FastMatch::getresulttolistcallcount() const
{
    return m_resulttolist_call_count;
}

int FastMatch::getresultcandidateinsertcount() const
{
    return m_resultcandidate_insert_count;
}

int FastMatch::getresultcandidatereplacecount() const
{
    return m_resultcandidate_replace_count;
}

int FastMatch::getresultcandidaterejectcount() const
{
    return m_resultcandidate_reject_count;
}

int FastMatch::getresultcandidatecount()
{
    return static_cast<int>(std::min(m_resultnums.size(), m_resultpoints.size()));
}

int FastMatch::getresultbestindex()
{
    const int candidate_count = getresultcandidatecount();
    if (candidate_count <= 0)
    {
        return -1;
    }
    return candidate_count - 1;
}

double FastMatch::getresultbestscore()
{
    const int best_index = getresultbestindex();
    if (best_index < 0)
    {
        return 0.0;
    }
    return m_resultnums.at(best_index);
}

int FastMatch::getrawthresholdhitrecordcount() const
{
    return static_cast<int>(m_rawthresholdhitpoints.size());
}

gp_Pnt FastMatch::getrawthresholdhitpoint(int inum) const
{
    if (inum >= 0 && inum < static_cast<int>(m_rawthresholdhitpoints.size()))
    {
        return m_rawthresholdhitpoints.at(inum);
    }
    return gp_Pnt();
}

int FastMatch::getrawthresholdhitscore(int inum) const
{
    if (inum >= 0 && inum < static_cast<int>(m_rawthresholdhitscores.size()))
    {
        return m_rawthresholdhitscores.at(inum);
    }
    return 0;
}

void FastMatch::MultiMatch(Image& image)
{
    resultclear();
    const int isize = static_cast<int>(m_models_l12.size());
    for (int i = 0; i < isize; i++)
    {
        modelstocurrent_l12(i);
        gp_Path& path = FindLine::getpatternpath();
        MultiMatchSample(image, path);
    }
}
void FastMatch::multimatch(void* pimage)
{
    Image* pgetimage = (Image*)pimage;
    if (pgetimage == nullptr)
        return;
    MultiMatch(*pgetimage);
}
void FastMatch::MultiMatchSample(Image& image, gp_Path& path)
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
            return;

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

        int iw = static_cast<int>(FindLine::patternboundingrect().Width());
        int ih = static_cast<int>(FindLine::patternboundingrect().Height());
        int itotalsize = static_cast<int>(FindLine::getpattern().size());

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

void FastMatch::RotateMatchAB(Image& image)
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


    int isize = static_cast<int>((m_dangle_add - m_dangle_mud) / m_danglegap);
    int icurangle = 0;

    if (m_models_rotate.size() <= 0)
        return;


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

}
void FastMatch::setupgradenum(int iresultnum)
{
    m_iupgradenum = iresultnum;
}
void FastMatch::RotateMatchAB_upgrade(Image& image)
{
    
    if (!HasRotateResultAt(
            m_iupgradenum,
            m_rotateresults,
            m_rotatereslutpoints,
            m_rotatereslutangles,
            m_rotateshaperesults))
        return;

    PointsShape abestshape = m_rotateshaperesults[m_iupgradenum];
    (void)abestshape;
    gp_Pnt abestpoint = m_rotatereslutpoints[m_iupgradenum];
    double abestresult = m_rotateresults[m_iupgradenum];
    (void)abestresult;
    double abestreslutangle = m_rotatereslutangles[m_iupgradenum];
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

    if (m_iupgradeanglescale <= 6)
        m_iupgradeanglescale = 6;
    int isize = m_iupgradeanglescale;
    int ihfsize = m_iupgradeanglescale / 2;
    int icurangle = 0;
    if (m_models_rotate.size() <= 0)
        return;
    if (abestreslutangle + m_iupgradeanglescale > 360)
    {
        abestreslutangle = abestreslutangle - 360;
    }

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

}
void FastMatch::RotateMatchAB05_upgrade(Image& image)
{

    if (!HasRotateResultAt(
            m_iupgradenum,
            m_rotateresults,
            m_rotatereslutpoints,
            m_rotatereslutangles,
            m_rotateshaperesults))
        return;

    PointsShape abestshape = m_rotateshaperesults[m_iupgradenum];
    (void)abestshape;
    gp_Pnt abestpoint = m_rotatereslutpoints[m_iupgradenum];
    double abestresult = m_rotateresults[m_iupgradenum];
    (void)abestresult;
    double abestreslutangle = m_rotatereslutangles[m_iupgradenum];
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

    int isize = 12;
    double dcurangle = 0;
    int ianglenum = 0;
    if (m_models05_rotate.size() <= 0)
        return;

    if (abestreslutangle + 6 > 360)
    {
        abestreslutangle = abestreslutangle - 360;
    }
    if (abestreslutangle - 6 < 0 && abestreslutangle + 6 >= 0)
    {
        int ibeginangle = static_cast<int>(360 + abestreslutangle - 6);
        dcurangle = ibeginangle;
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

}
void FastMatch::RotateMatchAB025_upgrade(Image& image)
{
    if (!HasRotateResultAt(
            m_iupgradenum,
            m_rotateresults,
            m_rotatereslutpoints,
            m_rotatereslutangles,
            m_rotateshaperesults))
        return;

    PointsShape abestshape = m_rotateshaperesults[m_iupgradenum];
    (void)abestshape;
    gp_Pnt abestpoint = m_rotatereslutpoints[m_iupgradenum];
    double abestresult = m_rotateresults[m_iupgradenum];
    (void)abestresult;
    double abestreslutangle = m_rotatereslutangles[m_iupgradenum];
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

    int isize = 24;
    double dcurangle = 0;
    int ianglenum = 0;
    if (m_models025_rotate.size() <= 0)
        return;

    if (abestreslutangle + 6 > 360)
    {
        abestreslutangle = abestreslutangle - 360;
    }

    if (abestreslutangle - 3 < 0 && abestreslutangle + 3 >= 0)
    {
        int ibeginangle = static_cast<int>(360 + abestreslutangle - 3);
        dcurangle = ibeginangle;
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

}
void FastMatch::samplemodelAB(int inum) 
{
    FindLine::samplemodelAB(inum);
}
void FastMatch::RotateMatch(Image& image)
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

    if (m_models_rotate.size() <= 0)
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
void FastMatch::setclustergap(int ixclustergap, int iyclustergap, int iangleclustergap)
{
    m_ixclustergap = FastMatchPositiveInt(ixclustergap);
    m_iyclustergap = FastMatchPositiveInt(iyclustergap);
    m_iangleclustergap = FastMatchPositiveInt(iangleclustergap);
}
void FastMatch::rotatematchAB(void* pimage)
{
    Image* pgetimage = (Image*)pimage;
    RotateMatchAB(*pgetimage);
}

void FastMatch::Setupgradescale(int isx, int isy)
{
    m_iupgradexscale = FastMatchPositiveInt(isx);
    m_iupgradeyscale = FastMatchPositiveInt(isy);
}
void FastMatch::Setupgradeanglescale(int iangle)
{
    m_iupgradeanglescale = FastMatchPositiveInt(iangle);
}
void FastMatch::rotatematchAB_upgrade(void* pimage)
{
    Image* pgetimage = (Image*)pimage;
    RotateMatchAB_upgrade(*pgetimage);
}

void FastMatch::rotatematchAB05_upgrade(void* pimage)
{
    Image* pgetimage = (Image*)pimage;
    RotateMatchAB05_upgrade(*pgetimage);
}
void FastMatch::rotatematchAB025_upgrade(void* pimage)
{
    Image* pgetimage = (Image*)pimage;
    RotateMatchAB025_upgrade(*pgetimage);
}

void FastMatch::rotatematch(void* pimage)
{
    Image* pgetimage = (Image*)pimage;
    RotateMatch(*pgetimage);
}

void FastMatch::RotateMatchSample(Image& image, gp_Path& path, 
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
        return;
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
    int iw = static_cast<int>(FindLine::patternboundingrect().Width());
    int ih = static_cast<int>(FindLine::patternboundingrect().Height());
    int itotalsize = static_cast<int>(FindLine::getpattern().size());

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
        bmodelrect.Move(static_cast<int>(apoint.X()), static_cast<int>(apoint.Y()));
        m_rotateshaperesults.push_back(bmodelrect);
        m_rotatereslutpoints.push_back(apoint);
        m_rotateresults.push_back(dpercent);
        m_rotatereslutangles.push_back(dangle);

    }


}

void FastMatch::RotateMatchSampleAB(Image& image, gp_Path& pathA,
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
        bmodelrect.Move(static_cast<int>(apoint.X()), static_cast<int>(apoint.Y()));
        m_rotateshaperesults.push_back(bmodelrect);
        m_rotatereslutpoints.push_back(apoint);
        m_rotateresults.push_back(dpercent);
        m_rotatereslutangles.push_back(dangle); 
    } 
}

void FastMatch::RotateMatchSample_upgrade(Image& image, gp_Path& path, PointsShape& modelrect, double dangle, gp_Pnt& resultpoint)
{
    int ithre = m_imatchthre;
    int icurmodule = ImageManager::GetCurMode();
    Image* pimage = ImageManager::GetTransferImage(icurmodule);
    (void)pimage;
    (void)modelrect;
    (void)dangle;
    int ix0 = static_cast<int>(resultpoint.X() - m_iupgradexscale);
    int iy0 = static_cast<int>(resultpoint.Y() - m_iupgradeyscale);
    int ix1 = static_cast<int>(resultpoint.X() + m_iupgradexscale);
    int iy1 = static_cast<int>(resultpoint.Y() + m_iupgradeyscale);

    if (image.getWidth() <= ix1
        || image.getHeight() <= iy1)
        return;
    m_iminfindnum = -1;
    int icount = static_cast<int>(path.ElementCount());
    cv::Vec3b pixel0, pixel1;
    int icalnum = 0;
    int icalng = 0;

    gp_Pnt aele;

    int igapx = 1;
    int igapy = 1;
    int ix = 0;
    int iy = 0;
    int iw = static_cast<int>(FindLine::patternboundingrect().Width());
    int ih = static_cast<int>(FindLine::patternboundingrect().Height());
    int itotalsize = static_cast<int>(FindLine::getpattern().size());

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

double FastMatch::getresultnum(int inum)
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
double FastMatch::getresultcentx(int inum)
{
    int iw = FastMatchPositiveInt(m_imodelwith);
    if (iw <= 0)
        iw = FastMatchPositiveInt(static_cast<int>(FindLine::patternboundingrectAB().Width()));
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
double FastMatch::getresultcenty(int inum)
{
    int ih = FastMatchPositiveInt(m_imodelheigh);
    if (ih <= 0)
        ih = FastMatchPositiveInt(static_cast<int>(FindLine::patternboundingrectAB().Height()));
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

double FastMatch::getresolvedresultcentx(int inum)
{
    return getresultcentx(inum);
}

double FastMatch::getresolvedresultcenty(int inum)
{
    if ((inum >= 0 && inum < static_cast<int>(m_resultpoints.size())) ||
        (inum == -1 && !m_resultpoints.empty()))
    {
        return getresultcenty(inum);
    }
    return 0.0;
}

int FastMatch::getrotateresultcentx(int inum)
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
int FastMatch::getrotateresultcenty(int inum)
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

void FastMatch::getresultcentpoints(void* apoints)
{
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
void FastMatch::getrotateresultrectpoints(std::vector<cv::Point2f>& points)
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
double FastMatch::getmaxresult()
{
    if (m_resultnums.size() > 0)
    {
        const int total_size = static_cast<int>(FindLine::getpatternpathA().ElementCount());
        if (total_size <= 0)
        {
            return 0.0;
        }
        return m_resultnums.at(m_resultnums.size() - 1) / static_cast<double>(total_size);
    }

    return 0.0;
}
int FastMatch::geteasyobjectw()
{
    return m_easyobject.s_iwobjnum;
}
int FastMatch::geteasyobjectb()
{
    return m_easyobject.s_ibobjnum;
}

int FastMatch::getmodeleasyobjectw_l72(int i)
{
    return EasyObjectWidthAt(m_easyobjectmodels_l72, i);
}
int FastMatch::getmodeleasyobjectb_l72(int i)
{
    return EasyObjectBlackCountAt(m_easyobjectmodels_l72, i);
}

int FastMatch::getmodeleasyobjectw_l36(int i)
{
    return EasyObjectWidthAt(m_easyobjectmodels_l36, i);
}
int FastMatch::getmodeleasyobjectb_l36(int i)
{
    return EasyObjectBlackCountAt(m_easyobjectmodels_l36, i);
}

int FastMatch::getmodeleasyobjectw_l12(int i)
{
    return EasyObjectWidthAt(m_easyobjectmodels_l12, i);
}
int FastMatch::getmodeleasyobjectb_l12(int i)
{
    return EasyObjectBlackCountAt(m_easyobjectmodels_l12, i);
}

int FastMatch::getmodeleasyobjectw_l3(int i)
{
    return EasyObjectWidthAt(m_easyobjectmodels_l3, i);
}
int FastMatch::getmodeleasyobjectb_l3(int i)
{
    return EasyObjectBlackCountAt(m_easyobjectmodels_l3, i);
}
int FastMatch::getmodeleasyobjectw_l6(int i)
{
    return EasyObjectWidthAt(m_easyobjectmodels_l6, i);
}
int FastMatch::getmodeleasyobjectb_l6(int i)
{
    return EasyObjectBlackCountAt(m_easyobjectmodels_l6, i);
}

void FastMatch::setrelationrectfromresultnum(int inum)
{
    m_irelationresultnum = inum;
}
void FastMatch::setrelationrectfrom_matchresult(void* pmatch)
{
    m_prelationmatch = (FastMatch*)pmatch;
    if (0 != m_prelationmatch)
    {
        int inum = m_prelationmatch->m_resultrects.size();
        if (m_irelationresultnum >= 0 && m_irelationresultnum < inum)
        {
            m_irelationrect = m_prelationmatch->m_resultrects.getrect(m_irelationresultnum);
        }
    }
}
void FastMatch::setrelationxy(int iprex1, int iprey1, int iendx1, int iendy1)
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
void FastMatch::setrelationzoom(double drelationzoomx, double drelationzoomy)
{
    (void)drelationzoomx;
    (void)drelationzoomy;
/*    m_irelationrect.setLeft((double)m_irelationrect.left() * drelationzoomx);
    m_irelationrect.setTop((double)m_irelationrect.top() * drelationzoomy);
    m_irelationrect.setRight((double)m_irelationrect.right() * drelationzoomx);
    m_irelationrect.setBottom((double)m_irelationrect.bottom() * drelationzoomy);*/
}
void FastMatch::setrelationtorect()
{
    if (m_irelationrect.TopLeft().X() >= 0
        && m_irelationrect.TopLeft().Y() >= 0
        && m_irelationrect.Width() > 0
        && m_irelationrect.Height() > 0)
        m_matchrect = m_irelationrect;
}
void FastMatch::shapesetroi(void* pshape)
{
    if (pshape == nullptr)
        return;
    Shape::shapesetroi(pshape);
}

std::vector<cv::Point2f> FastMatch::getmodel()
{
    std::vector<cv::Point2f> points;
    const int count = m_modelpoints_sample1.size();
    for (int i = 0; i < count; ++i)
    {
        points.push_back(cv::Point2f(
            static_cast<float>(m_modelpoints_sample1.getx(i)),
            static_cast<float>(m_modelpoints_sample1.gety(i))));
    }
    if (points.empty())
        ABtoShape(points);
    return points;
}

int FastMatch::getmodelpointcount()
{
    const int sample_count = m_modelpoints_sample1.size();
    if (sample_count > 0)
        return sample_count;
    return ABpatternsize();
}

int FastMatch::getlearnacount()
{
    return m_fastmatch_learn_a_count;
}

int FastMatch::getlearnbcount()
{
    return m_fastmatch_learn_b_count;
}

int FastMatch::getlearna2count()
{
    return m_fastmatch_learn_a2_count;
}

int FastMatch::getlearnb2count()
{
    return m_fastmatch_learn_b2_count;
}

int FastMatch::getpatternapointcount() const
{
    return static_cast<int>(const_cast<FastMatch*>(this)->FindLine::getpatternpathA().ElementCount());
}

int FastMatch::getpatternbpointcount() const
{
    return static_cast<int>(const_cast<FastMatch*>(this)->FindLine::getpatternpathB().ElementCount());
}

double FastMatch::getpatternax() const
{
    return const_cast<FastMatch*>(this)->FindLine::getpatternpathA().boundingRect().TopLeft().X();
}

double FastMatch::getpatternay() const
{
    return const_cast<FastMatch*>(this)->FindLine::getpatternpathA().boundingRect().TopLeft().Y();
}

double FastMatch::getpatternawidth() const
{
    return const_cast<FastMatch*>(this)->FindLine::getpatternpathA().boundingRect().Width();
}

double FastMatch::getpatternaheight() const
{
    return const_cast<FastMatch*>(this)->FindLine::getpatternpathA().boundingRect().Height();
}

double FastMatch::getpatternbx() const
{
    return const_cast<FastMatch*>(this)->FindLine::getpatternpathB().boundingRect().TopLeft().X();
}

double FastMatch::getpatternby() const
{
    return const_cast<FastMatch*>(this)->FindLine::getpatternpathB().boundingRect().TopLeft().Y();
}

double FastMatch::getpatternbwidth() const
{
    return const_cast<FastMatch*>(this)->FindLine::getpatternpathB().boundingRect().Width();
}

double FastMatch::getpatternbheight() const
{
    return const_cast<FastMatch*>(this)->FindLine::getpatternpathB().boundingRect().Height();
}

void FastMatch::PublishDisplayShapes(ICxShapeSink& sink, const std::string& owner_ref)
{
    const double learn_x = static_cast<double>(m_learn_roi_x);
    const double learn_y = static_cast<double>(m_learn_roi_y);
    const double learn_w = static_cast<double>(m_learn_roi_w);
    const double learn_h = static_cast<double>(m_learn_roi_h);

    if (learn_w > 0 && learn_h > 0)
    {
        auto learn_roi_shape = std::make_unique<RectShape>();
        learn_roi_shape->setRect(learn_x, learn_y, learn_x + learn_w, learn_y + learn_h);
        sink.UpsertShape(
            owner_ref + ".learn_roi",
            "FastMatch",
            owner_ref,
            "learn_roi",
            "learn_roi",
            true,
            false,
            std::move(learn_roi_shape));
    }

    const double search_x = static_cast<double>(m_search_roi_x);
    const double search_y = static_cast<double>(m_search_roi_y);
    const double search_w = static_cast<double>(m_search_roi_w);
    const double search_h = static_cast<double>(m_search_roi_h);

    if (search_w > 0 && search_h > 0)
    {
        auto search_roi_shape = std::make_unique<RectShape>();
        search_roi_shape->setRect(search_x, search_y, search_x + search_w, search_y + search_h);
        sink.UpsertShape(
            owner_ref + ".search_roi",
            "FastMatch",
            owner_ref,
            "search_roi",
            "search_roi",
            true,
            false,
            std::move(search_roi_shape));
    }

    const double expected_w = m_expected_rect.Width();
    const double expected_h = m_expected_rect.Height();
    if (expected_w > 0 && expected_h > 0)
    {
        const double expected_x = m_expected_rect.TopLeft().X();
        const double expected_y = m_expected_rect.TopLeft().Y();
        auto expected_shape = std::make_unique<RectShape>();
        expected_shape->setRect(expected_x, expected_y, expected_x + expected_w, expected_y + expected_h);
        sink.UpsertShape(
            owner_ref + ".expected_gt",
            "FastMatch",
            owner_ref,
            "expected_gt",
            "expected_gt",
            false,
            false,
            std::move(expected_shape));
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
            "FastMatch",
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
            "FastMatch",
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
                "FastMatch",
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
                "FastMatch",
                owner_ref,
                "",
                "best_result",
                false,
                true,
                std::move(best_shape));
        }
    }
}

bool FastMatch::ApplyDisplayShapeEdit(const std::string& owner_binding, const std::string& semantic_role,
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

        setrectxywh(static_cast<int>(x), static_cast<int>(y),
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

        setmatchrectxywh(static_cast<int>(x), static_cast<int>(y),
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
