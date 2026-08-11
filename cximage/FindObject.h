#ifndef FINDOBJECT_H
#define FINDOBJECT_H
 

#include "shapebase.h"
#include "Shape.h"
#include "Image.h"
#include "imagemanager.h"
 

 
/*G_SearchPointGroup Search Matrix */

typedef vector<int> vectorint;
typedef vector<cv::Vec3b> vectorpixel;
typedef vectorint::iterator vectintitor;

class Grid;
class Match;
class ICxShapeSink;
class FindObject :public Shape
{
    enum ObjectAnalysisType
    {
        ANALYSIS_NO = 0,
        ANLAYSIS_COLLECTION = 1,
        ANLAYSIS_OVER = 2,
        ANLAYSIS_OK = 3,
        ANLAYSIS_READY = 4,
        ANLAYSIS_NOVALID = 5
    };
    enum ObjectSearchType
    {
        Search_O = 0,
        Search_OL = 1,
        Search_OR = 2,
        Search_OU = 3,
        Search_OD = 4,
        Search_OX = 5
    };

public:
    FindObject();
    ~FindObject();

    void setcolor(int ir, int ig, int ib);
    void setshow(int ishow);
    void getshape(void* pshape);
    virtual void setrect(int ix, int iy, int iw, int ih);
    virtual void drawshape();
    void drawshapex(double dmovx,double dmovy,
        double dangle,double dzoomx,double dzoomy);
    void Measure(Image& image);
    void MeasureFast(Image& image);
    void MeasureConnectedComponents(Image& image);
    void MeasurePeakLocalBFS(Image& image);
    void MeasureGrid(Grid* grid);
    void MeasureX(Image& image);
    void MeasureXFast(Image& image);
    void MeasureXConnectedComponents(Image& image);
    void MeasureXPeakLocalBFS(Image& image);
    void measure(void* pimage);
    void measurefast(void* pimage);
    void measurecc(void* pimage);
    void measurexbfs(void* pimage);
    void measurex(void* pimage);
    void measurexfast(void* pimage);
    void measurexcc(void* pimage);
    void measurexpeakbfs(void* pimage);

    void edgeimage(void* pimage);
    void setedgeoi(int iw, int ioffset, int iheadtail);

    void Edge(int inum);
    void Object(int inum);
    void setfilteredge(int iw);
    void setbrow(int iborw);
    void setdistance(int idist);
    void setrect_script(int ih, int iw, int iy, int ix)
    {
        setrect(ix, iy, iw, ih);
    }
    void sethsogap(int ihgap, int isgap, int iogap);
    void sethsogap_script(int iogap, int isgap, int ihgap)
    {
        sethsogap(ihgap, isgap, iogap);
    }
    void setminmaxarea(int imin, int imax);
    void setminmaxarea_script(int imax, int imin)
    {
        setminmaxarea(imin, imax);
    }
    void setminmaxwh(int iminw, int imaxw, int iminh, int imaxh);
    void setminmaxwh_script(int imaxh, int iminh, int imaxw, int iminw)
    {
        setminmaxwh(iminw, imaxw, iminh, imaxh);
    }
    int getresultcentx(int inum);
    int getresultcenty(int inum);
    int getresultx(int inum);
    int getresulty(int inum);
    int getresultw(int inum);
    int getresulth(int inum);
    int getresultsize(int inum);
    int getresultobjsnum();
    int getdebugcomponentcount();
    int getdebugacceptedcount();
    int getdebugrejectedcount();
    int getdebugmaxcomponentarea();
    int getdebugmaxcomponentw();
    int getdebugmaxcomponenth();
    const std::string& getdebugalgorithmbranch() const;
    void PublishDisplayShapes(ICxShapeSink& sink, const std::string& owner_ref) const;
    void setsearchtype(int itype);
    void setoffset(int ix0, int ix1, int iy0, int iy1);
    RectsShape& getresultrects() { return m_rectresults; }
    void setobjectgrid(int iw, int ih, int ixgrid);
    int getobjectgridw();
    int getobjectgridh();
    void setbackground(int iedge, int ibackgroundmethod = 1);
    void objectgrid(void* pimage);
    void resultsrectfilter();
    void objectsort();
    gp_Rectangle getgrid(int inum);
    gp_Rectangle getgridex(int inum);

    typedef vector<TwoPointsShape> tpvect;
    typedef tpvect::iterator tpvectitor;

    tpvect m_cent_h_bw_points_v;
    tpvect m_cent_v_bw_points_v;

    tpvect m_cent_h_wb_points_v;
    tpvect m_cent_v_wb_points_v;
    //   void B2Wedge_h();
    //   void W2Bedge_h();
    //   void B2Wedge_h();
    //   void W2Bedge_h();

private:
    int m_istyle;
    Image* g_pmapimage;
    Image* g_pbackobjectimage;
    Image* m_pgetimage;

    gp_Pnt* m_objlistscanorA;
    gp_Pnt* m_objlistcollectorA;

    RectsShape m_rectresults;
    RectsShape m_rectgrids;

    PointsShape m_keypoint;
    PointsShape m_fitwh;

    PointsShape m_curedge;
    PointsShape m_curobject;

    vectorint m_scanid;
    vectorint m_vrow;
    vectorpixel m_vborw;
    vectorint m_vobjnum;

    int m_icurobj;
    int m_iobjnum;

    int m_ifilterNedge;
    int m_iborw;

    int m_idistance;
    int64 m_iminarea;
    int64 m_imaxarea;
    int64 m_iminobjw;
    int64 m_iminobjh;
    int64 m_imaxobjw;
    int64 m_imaxobjh;
    int64 m_totalarea;

    int m_ioffsetx0;
    int m_ioffsetx1;
    int m_ioffsety0;
    int m_ioffsety1;

    int m_ihgap;
    int m_isgap;
    int m_iogap;

    gp_Pnt* m_SearchPointGroup;
    ObjectSearchType m_searchtype;

    int m_icopyw;
    int m_icopyh;
    int m_icopywgrid;

    int m_background_edge;
    int m_background_method;
     
    static int m_curfindobjectnum; 
  //  fastmatch* m_prelationmatch;
    int m_irelationresultnum;

    double m_drelationzoomx;
    double m_drelationzoomy;
    gp_Rectangle m_irelationrect;
    int m_imagethre;
    int m_imagethreincrease;
    int m_imagecomparegap;
    int m_imagefindBorW;
    int m_imageedge_5o7;
    int m_debug_component_count;
    int m_debug_accepted_count;
    int m_debug_rejected_count;
    int m_debug_max_component_area;
    int m_debug_max_component_w;
    int m_debug_max_component_h;
    std::string m_debug_algorithm_branch;
    bool RefreshAlgorithmRuntimeResources(int image_width, int image_height);
    void FinalizeRegionGrowthDebugCounters();
    void ObserveDebugComponent(int area, int width, int height);
    bool IsSamePixel(const cv::Vec3b& lhs, const cv::Vec3b& rhs) const;

    std::vector<cv::Point> DetectPeakSeeds(const cv::Mat& distance_map,
                                           double min_peak_distance) const;
    cv::Rect ComputeLocalSearchROI(const cv::Point& peak,
                                   const cv::Mat& distance_map,
                                   int max_edge_width) const;
    void RunPreemptiveLocalBFS(const cv::Point& peak,
                               const cv::Rect& roi,
                               const cv::Mat& binary_image,
                               cv::Mat& label_map,
                               int component_id,
                               std::vector<cv::Rect>& out_bboxes,
                               int& out_area,
                               bool is_white_region) const;
    void CollectComponentFromLabels(const cv::Mat& labels,
                                    int component_id,
                                    const cv::Rect& roi,
                                    int& out_x, int& out_y,
                                    int& out_w, int& out_h) const;
    void AcceptPeakComponent(int local_x, int local_y,
                             int comp_w, int comp_h, int area,
                             bool is_white_region);
public:
    void setrelationrectfromresultnum(int inum);
    void setrelationrectfrom_matchresult(void* pmatch);
    void setrelationrectfrom_objectresult(void* pmatch);
    void setrelationxy(int iprex1, int iprey1, int iendx1, int iendy1);
    void setrelationzoom(double drelationzoomx, double drelationzoomy);
    void setrelationtorect();
    void setcolorstyle(int istyle);


    void SetImageROIthre(int ithre);
    void SetImageROIincrease(int increase);
    void SetImageROIcomparegap(int icomparegap);
    void SetImageROIfindBorW(int ifindBorW);
    void SetImageROIedge_5o7(int i5o7);

    void ImageROIthre(void* pimage);
    void ImageROIedge(void* pimage);
    void ImageROIedgeH(void* pimage);

    void shapesetroi(void* pshape);

    int ncurscan = -1;
    int nscansize = 0;
    int ncursearchseek = -1;
    int nsearchseeksize = 0;

 
    //[Service][Analysis][ Pixel ][Edge]
 
    int GetServiceValue(cv::Vec4b rgbx)
    {
        return rgbx[0];
    }
    int GetPixelValue(cv::Vec4b rgbx)
    {
        return rgbx[1];
    }
    int GetAnalysisValue(cv::Vec4b rgbx)
    {
        return rgbx[2];
    }
    int GetEdgeValue(cv::Vec4b rgbx)
    {
        return rgbx[3];
    }
    struct MapState
    {
        int service;
        int pixel;
        int analysis;
        int edge;
    };
    MapState DecodeMapState(const cv::Vec4b& rgbx) const
    {
        return {
            static_cast<int>(rgbx[0]),
            static_cast<int>(rgbx[1]),
            static_cast<int>(rgbx[2]),
            static_cast<int>(rgbx[3])
        };
    }
    cv::Vec4b SetServiceValue(cv::Vec4b rgbx, int ivalue)
    {
        rgbx[0] = static_cast<uchar>(ivalue);
        return rgbx;
    }
    cv::Vec4b SetPixelValue(cv::Vec4b rgbx, int  ivalue)
    {
        rgbx[1] = static_cast<uchar>(ivalue);
        return rgbx;
    }
    cv::Vec4b SetAnalysisValue(cv::Vec4b rgbx, int ivalue)
    {
        rgbx[2] = static_cast<uchar>(ivalue);
        return rgbx;
    }
    cv::Vec4b SetEdgeValue(cv::Vec4b rgbx, int ivalue)
    {
        rgbx[3] = static_cast<uchar>(ivalue);
        return rgbx;
    }




    void  PUSH_SCANOR(int ix,int iy)
    {   
        if (m_objlistscanorA == nullptr)
            return;
        if (ncurscan + 1 >= OBJSCANNNUM)
            return;
        ncurscan++; 
        m_objlistscanorA[ncurscan].SetX(ix);
        m_objlistscanorA[ncurscan].SetY(iy);
        nscansize++;
    }
    void CLEAR_SCANOR()
    {
        ncurscan = -1;
        nscansize = 0;
    }
    void PUSH_SEARCHSEEK(int ix,int iy)
    {
        if (m_objlistcollectorA == nullptr)
            return;
        if (ncursearchseek + 1 >= OBJCOLLECTIONNUM)
            return;
        ncursearchseek++; 
        m_objlistcollectorA[ncursearchseek].SetX(ix);
        m_objlistcollectorA[ncursearchseek].SetY(iy);
        nsearchseeksize++;
    }
    void CLEAR_SEARCHSEEK()
    {
        ncursearchseek = -1;
        nsearchseeksize = 0;
    }
    void MAPCLEAR() 
    {
        if (g_pmapimage == nullptr)
            return;
        g_pmapimage->setroi(
            static_cast<int>(rect().TopLeft().X()),
            static_cast<int>(rect().TopLeft().Y()),
            static_cast<int>(rect().Width()),
            static_cast<int>(rect().Height())); \
        g_pmapimage->colorizeROI(0,0,0);
    }
    cv::Vec4b MAP(int ix, int iy) {
        if (g_pmapimage == nullptr ||
            ix < 0 || iy < 0 ||
            ix >= g_pmapimage->getWidth() ||
            iy >= g_pmapimage->getHeight())
        {
            return cv::Vec4b(0, 0, 0, 0);
        }
        return g_pmapimage->pixelvalue(ix, iy);
    }

    int MAP_pixel(int ix,int iy){
       return GetPixelValue(MAP(ix, iy));
    } 
    int MAP_service(int ix, int iy){
        return GetServiceValue(MAP(ix, iy));
    }
    int MAP_analysis(int ix, int iy) {
        return GetAnalysisValue(MAP(ix, iy));
    }
    int MAP_edge(int ix, int iy) {
        return GetEdgeValue(MAP(ix, iy));
    }

    void SetMAP(int ix, int iy, cv::Vec4b ivalue)
    {
        if (g_pmapimage == nullptr ||
            ix < 0 || iy < 0 ||
            ix >= g_pmapimage->getWidth() ||
            iy >= g_pmapimage->getHeight())
        {
            return;
        }
        g_pmapimage->setpixelvalue(ix, iy, ivalue);
    }
    void SetMAP_pixel(int ix, int iy, int  ivalue)
    {
        SetMAP(ix, iy, SetPixelValue(MAP(ix, iy), ivalue));
    }
    void SetMAP_service(int ix, int iy, int ivalue)
    {
        SetMAP(ix, iy, SetServiceValue(MAP(ix, iy), ivalue));
    }
    void SetMAP_analysis(int ix, int iy, int ivalue)
    {
        SetMAP(ix, iy, SetAnalysisValue(MAP(ix, iy), ivalue));
    }
    void SetMAP_edge(int ix, int iy, int ivalue)
    {
        SetMAP(ix, iy, SetEdgeValue(MAP(ix, iy), ivalue));
    }
    void SetMAP_service_pixel(int ix, int iy, int iservice, int ipixel)
    {
        cv::Vec4b map_value = MAP(ix, iy);
        map_value[0] = static_cast<uchar>(iservice);
        map_value[1] = static_cast<uchar>(ipixel);
        SetMAP(ix, iy, map_value);
    }



};
 
 
















#endif //FINDOBJECT_H
