#include "pch.h"

 
#include "findobject.h"
#include "occtinclude.h"
#include "imagemanager.h"

#include <opencv2/opencv.hpp>		
#include <opencv2/core/version.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/video/tracking.hpp>

/*
#define  PUSH_SCANOR(ix,iy) \
{ncurscan++; \
    m_objlistscanorA[ncurscan].SetX(ix);\
    m_objlistscanorA[ncurscan].SetY(iy);\
    nscansize++;}

#define CLEAR_SCANOR() \
{ncurscan = -1;\
    nscansize = 0;}

#define  PUSH_SEARCHSEEK(ix,iy) \
{ncursearchseek++; \
    m_objlistcollectorA[ncursearchseek].SetX(ix);\
    m_objlistcollectorA[ncursearchseek].SetY(iy);\
    nsearchseeksize++;}

#define  CLEAR_SEARCHSEEK() \
{ncursearchseek = -1; \
    nsearchseeksize = 0;}
*/

typedef unsigned char  BYTE;

#define HI4bit(w)         static_cast<BYTE>((w >> 4) & 0x0F)//  ((BYTE)((qintptr)(w) >> 4)&0x0f)
#define LO4bit(w)         static_cast<BYTE>(w & 0x0F)// ((BYTE)((qintptr)(w) & 0x0f)) 
#define LOBYTE(w)         static_cast<BYTE>(w & 0xFF)//  ((BYTE)((qintptr)(w) & 0xff))
/*
//  喜  姆 式 墙  ivalue   前     纸冢 B, G     ivalue0  暮      纸冢 R, A         
//    龋    ivalue   取      色通  值         16 位
unsigned int highPart = (static_cast<unsigned int>(rgbx[0]) << 24) |(static_cast<unsigned int>(rgbx[1]) << 16);

// 然 螅  ivalue0   取 臁lpha 通  值
unsigned int lowPart = (static_cast<unsigned int>(rgbx[2]) << 8) |static_cast<unsigned int>(rgbx[3]);

//  喜       值
unsigned int combinedValue = highPart | lowPart;

//    喜    值      ivalue1
ivalue1[0] = (combinedValue >> 24) & 0xFF; //   色
ivalue1[1] = (combinedValue >> 16) & 0xFF; //   色
ivalue1[2] = (combinedValue >> 8) & 0xFF;  //   色
ivalue1[3] = combinedValue & 0xFF;         // Alpha
*/

 
/*
#define MAPCLEAR() \
{g_pmapimage->setroi(rect().TopLeft().X(), rect().TopLeft().Y(), rect().Width(), rect().Height()); \
g_pmapimage->colorizeROI(0,0,0);}

#define MAP(ix,iy) g_pmapimage->pixelvalue(ix,iy)
#define MAP_pixel(ix,iy) GetPixelValue(MAP(ix,iy))
#define MAP_service(ix,iy) GetServiceValue(MAP(ix,iy))
#define MAP_analysis(ix,iy) GetAnalysisValue(MAP(ix,iy))
#define MAP_edge(ix,iy) GetEdgeValue(MAP(ix,iy))

#define SetMAP(ix,iy,ivalue)  g_pmapimage->setpixelvalue(ix,iy,ivalue)
#define SetMAP_pixel(ix,iy,ivalue) SetMAP(ix,iy,SetPixelValue(MAP(ix,iy),ivalue))
#define SetMAP_service(ix,iy,ivalue) SetMAP(ix,iy,SetServiceValue(MAP(ix,iy),ivalue))
#define SetMAP_analysis(ix,iy,ivalue) SetMAP(ix,iy,SetAnalysisValue(MAP(ix,iy),ivalue))
#define SetMAP_edge(ix,iy,ivalue) SetMAP(ix,iy,SetEdgeValue(MAP(ix,iy),ivalue))
*/
static gp_Pnt G_SearchPointGroup[224] =
{
    //0
   {1,0,0},{0,-1,0},{-1,0,0},{0,1,0},//4
   {1,1,0},{-1,1,0},{-1,-1,0},{1,-1,0},//8
   {2,-1,0},{2,0,0},{2,1,0},{2,2,0},{1,2,0},{0,2,0},{-1,2,0},{-2,2,0},{-2,1,0},{-2,0,0},{-2,-1,0},{-2,-2,0},{-1,-2,0},{0,-2,0},{1,-2,0},{2,-2,0},//24
   {3,-2,0},{3,-1,0},{3,0,0},{3,1,0},{3,2,0},{3,3,0},{2,3,0},{1,3,0},{0,3,0},{-1,3,0},{-2,3,0},{-3,3,0},{-3,2,0},{-3,1,0},{-3,0,0},{-3,-1,0},{-3,-2,0},{-3,-3,0},{-2,-3,0},{-1,-3,0},{0,-3,0},{1,-3,0},{2,-3,0},{3,-3,0},//48
   {4,-3,0},{4,-2,0},{4,-1,0},{4,0,0},{4,1,0},{4,2,0},{4,3,0},{4,4,0},{3,4,0},{2,4,0},{1,4,0},{0,4,0},{-1,4,0},{-2,4,0},{-3,4,0},{-4,4,0},{-4,3,0},{-4,2,0},{-4,1,0},{-4,0,0},{-4,-1,0},{-4,-2,0},{-4,-3,0},{-4,-4,0},{-3,-4,0},{-2,-4,0},{-1,-4,0},{0,-4,0},{1,-4,0},{2,-4,0},{3,-4,0},{4,-4,0},//80
   {5,-4,0},{5,-3,0},{5,-2,0},{5,-1,0},{5,0,0},{5,1,0},{5,2,0},{5,3,0},{5,4,0},{5,5,0},{4,5,0},{3,5,0},{2,5,0},{1,5,0},{0,5,0},{-1,5,0},{-2,5,0},{-3,5,0},{-4,5,0},{-5,5,0},{-5,4,0},{-5,3,0},{-5,2,0},{-5,1,0},{-5,0,0},{-5,-1,0},{-5,-2,0},{-5,-3,0},{-5,-4,0},{-5,-5,0},{-4,-5,0},{-3,-5,0},{-2,-5,0},{-1,-5,0},{0,-5,0},{1,-5,0},{2,-5,0},{3,-5,0},{4,-5,0},{5,-5,0},//120
   {6,-5,0},{6,-4,0},{6,-3,0},{6,-2,0},{6,-1,0},{6,0,0},{6,1,0},{6,2,0},{6,3,0},{6,4,0},{6,5,0},{6,6,0},{5,6,0},{4,6,0},{3,6,0},{2,6,0},{1,6,0},{0,6,0},{-1,6,0},{-2,6,0},{-3,6,0},{-4,6,0},{-5,6,0},{-6,6,0},{-6,5,0},{-6,4,0},{-6,3,0},{-6,2,0},{-6,1,0},{-6,0,0},{-6,-1,0},{-6,-2,0},{-6,-3,0},{-6,-4,0},{-6,-5,0},{-6,-6,0},{-5,-6,0},{-4,-6,0},{-3,-6,0},{-2,-6,0},{-1,-6,0},{0,-6,0},{1,-6,0},{2,-6,0},{3,-6,0},{4,-6,0},{5,-6,0},{6,-6,0},//168
   {7,-6,0},{7,-5,0},{7,-4,0},{7,-3,0},{7,-2,0},{7,-1,0},{7,0,0},{7,1,0},{7,2,0},{7,3,0},{7,4,0},{7,5,0},{7,6,0},{7,7,0},{6,7,0},{5,7,0},{4,7,0},{3,7,0},{2,7,0},{1,7,0},{0,7,0},{-1,7,0},{-2,7,0},{-3,7,0},{-4,7,0},{-5,7,0},{-6,7,0},{-7,7,0},{-7,6,0},{-7,5,0},{-7,4,0},{-7,3,0},{-7,2,0},{-7,1,0},{-7,0,0},{-7,-1,0},{-7,-2,0},{-7,-3,0},{-7,-4,0},{-7,-5,0},{-7,-6,0},{-7,-7,0},{-6,-7,0},{-5,-7,0},{-4,-7,0},{-3,-7,0},{-2,-7,0},{-1,-7,0},{0,-7,0},{1,-7,0},{2,-7,0},{3,-7,0},{4,-7,0},{5,-7,0},{6,-7,0},{7,-7,0},//224
};
static gp_Pnt G_SearchPointGroup_OOD[16] =
{
    //0
 {0,-1,0},{0,1,0},{1,0,0},{-1,0,0},{1,1,0},{-1,1,0},{-1,-1,0},{1,-1,0},
        {0,2,0},{0,3,0},{0,4,0},{0,5,0},//8
    {0,8,0},{0,11,0},{0,15,0},{0,20,0}

};
static gp_Pnt G_SearchPointGroup_OD[16] =
{
    //0
    {0,-1,0},{0,1,0},{1,0,0},{-1,0,0},{1,1,0},{-1,1,0},{-1,-1,0},{1,-1,0},
        {0,2,0},{0,3,0},{0,4,0},{0,5,0},//8
    {0,6,0},{0,7,0},{0,8,0},{0,9,0}

};
static gp_Pnt G_SearchPointGroup_ODD[16] =
{
    //0
   {0,-1,0},{0,1,0},{1,0,0},{-1,0,0},{1,1,0},{-1,1,0},{-1,-1,0},{1,-1,0},
        {0,3,0},{0,6,0},{0,9,0},{0,12,0},//8
    {0,15,0},{0,18,0},{0,21,0},{0,24,0}

};
static gp_Pnt G_SearchPointGroup_L[16] =
{
    //0
  {1,0,0},{0,1,0},{0,-1,0},{-1,0,0},{2,0,0},{3,0,0},{4,0,0},{5,0,0},//8
    {6,0,0},{7,0,0},{8,0,0},{9,0,0},{10,0,0},{11,0,0},{12,0,0},{13,0,0}

};
static gp_Pnt G_SearchPointGroup_LL[16] =
{
    //0
    {1,0,0},{0,1,0},{0,-1,0},{-1,0,0},{2,0,0},{4,0,0},{6,0,0},{8,0,0},//8
    {10,0,0},{12,0,0},{14,0,0},{16,0,0},{18,0,0},{20,0,0},{22,0,0},{24,0,0}

};
static gp_Pnt G_SearchPointGroup_LLL[16] =
{
    //0
    {1,0,0},{0,1,0},{0,-1,0},{-1,0,0},{3,0,0},{6,0,0},{9,0,0},{12,0,0},//8
    {15,0,0},{18,0,0},{21,0,0},{24,0,0},{27,0,0},{30,0,0},{33,0,0},{36,0,0}

};
static gp_Pnt G_SearchPointGroup_LMAX[16] =
{
    //0
    {1,0,0},{0,1,0},{0,-1,0},{-1,0,0},{5,0,0},{10,0,0},{15,0,0},{20,0,0},//8
    {25,0,0},{30,0,0},{35,0,0},{40,0,0},{45,0,0},{50,0,0},{55,0,0},{60,0,0}

};
static gp_Pnt G_SearchPointGroup_R[16] =
{
    //0
    {-1,0,0},{1,0,0},{0,1,0},{0,-1,0},{-2,0,0},{-3,0,0},{-4,0,0},{-5,0,0},//8
    {-6,0,0},{-7,0,0},{-8,0,0},{-9,0,0},{-10,0,0},{-11,0,0},{-12,0,0},{-13,0,0}

};
static gp_Pnt G_SearchPointGroup_U[16] =
{
    //0
    {0,-1,0},{0,1,0},{1,0,0},{-1,0,0},{0,-2,0},{0,-3,0},{0,-4,0},{0,-5,0},//8
    {0,-6,0},{0,-7,0},{0,-8,0},{0,-9,0},{0,-10,0},{0,-11,0},{0,-12,0},{0,-13,0}

};
static gp_Pnt G_SearchPointGroup_D[16] =
{
    //0
    {0,-1,0},{0,1,0},{1,0,0},{-1,0,0},{0,2,0},{0,3,0},{0,4,0},{0,5,0},//8
    {0,6,0},{0,7,0},{0,8,0},{0,9,0},{0,10,0},{0,11,0},{0,12,0},{0,13,0}
};
static gp_Pnt G_SearchPointGroup_DD[16] =
{
    //0
    {0,-1,0},{0,1,0},{1,0,0},{-1,0,0},{0,2,0},{0,4,0},{0,6,0},{0,8,0},//8
    {0,10,0},{0,12,0},{0,14,0},{0,16,0},{0,18,0},{0,20,0},{0,22,0},{0,24,0}
};
static gp_Pnt G_SearchPointGroup_DDD[16] =
{
    //0
    {0,-1,0},{0,1,0},{1,0,0},{-1,0,0},{0,3,0},{0,6,0},{0,9,0},{0,12,0},//8
    {0,15,0},{0,18,0},{0,21,0},{0,24,0},{0,27,0},{0,30,0},{0,33,0},{0,36,0}

};
static gp_Pnt G_SearchPointGroup_X[16] =
{
    //0
    {0,-1,0},{0,1,0},{1,0,0},{-1,0,0},{0,2,0},{0,3,0},{0,4,0},{0,5,0},//8
    {0,6,0},{0,7,0},{0,8,0},{0,9,0},{0,10,0},{0,11,0},{0,12,0},{0,13,0}

};

int FindObject::m_curfindobjectnum = 0;
FindObject::FindObject() :
    m_iborw(3),
    m_ifilterNedge(0),
    m_idistance(16),
    m_icurobj(0),
    m_iobjnum(0),
    m_iminarea(5),
    m_imaxarea(99999),
    m_iminobjw(0),
    m_iminobjh(0),
    m_imaxobjw(9999),
    m_imaxobjh(9999),
    m_ihgap(50),
    m_isgap(20),
    m_iogap(30),
    m_pgetimage(0),
    m_icopyw(30),
    m_icopyh(30),
    m_icopywgrid(20),
    m_background_edge(2),
    m_background_method(1),
    m_ioffsetx0(0),
    m_ioffsetx1(0),
    m_ioffsety0(0),
    m_ioffsety1(0),
    m_imagethre(18),
    m_imagethreincrease(0),
    m_imagecomparegap(2),
    m_imagefindBorW(0),
    m_imageedge_5o7(5),
    m_irelationrect(gp_Pnt(0,0,0), gp_Pnt(0, 0, 0))
{ 
    string strname = string("fobject%1");
    setname(strname.c_str());
    m_curfindobjectnum = m_curfindobjectnum + 1;

    setcolor(0, 205, 180);
    int icurmodule = ImageManager::GetCurMode();
    g_pbackobjectimage = ImageManager::GetBackObjectImage(icurmodule);
    g_pmapimage = ImageManager::GetMapImage(icurmodule);

    m_objlistscanorA = ImageManager::GetListScan(icurmodule);
    m_objlistcollectorA = ImageManager::GetListCollect(icurmodule);
    m_SearchPointGroup = G_SearchPointGroup;
    m_searchtype = ObjectSearchType::Search_O;
}
FindObject::~FindObject()
{
}
void FindObject::setbrow(int iborw)
{
    m_iborw = iborw;
}
void FindObject::setcolor(int ir, int ig, int ib)
{
    m_rectresults.setcolor(ir, ig, ib);
}
void FindObject::setshow(int ishow)
{
    if (1 == ishow)
    {
        m_rectresults.setcolor(255,0,0);
        m_rectresults.setshow(1);
        m_rectresults.MakeShape(); 
    }
    Shape::setshow(ishow);
}
void FindObject::getshape(void* pshape)
{
    Shape* pshape0 = (Shape*)pshape;
    if (pshape0 == nullptr)
        return;

    const gp_Rectangle arect = rect();
    pshape0->setrect(arect.TopLeft().X(),
        arect.TopLeft().Y(),
        arect.Width(),
        arect.Height());
}
void FindObject::setrect(int ix, int iy, int iw, int ih)
{
    Shape::setrect(ix, iy, iw, ih);
}
void FindObject::drawshape()
{
    /*
    if (show() & 0x02)
    {
        m_rectresults.drawshape();
    }
    if (show() & 0x04)
    {
        m_curedge.drawshape(painter);
        m_curobject.drawshape(painter);
    }
    if (show() & 0x08)
    {
        int isize = m_cent_h_bw_points_v.size();
        for (int i = 0; i < isize; i++)
            m_cent_h_bw_points_v[i].drawshape(painter);
        isize = m_cent_v_bw_points_v.size();
        for (int i = 0; i < isize; i++)
            m_cent_v_bw_points_v[i].drawshape(painter);
        isize = m_cent_h_wb_points_v.size();
        for (int i = 0; i < isize; i++)
            m_cent_h_wb_points_v[i].drawshape(painter);
        isize = m_cent_v_wb_points_v.size();
        for (int i = 0; i < isize; i++)
            m_cent_v_wb_points_v[i].drawshape(painter);
    }*/
    Shape::drawshape();
}
int FindObject::getresultcentx(int inum)
{
    if (inum >= 0 && inum < m_rectresults.size())
        return m_rectresults.getrect(inum).BottomRight().X();
    else
        return 0;
}
int FindObject::getresultcenty(int inum)
{
    if (inum >= 0 && inum < m_rectresults.size())
        return m_rectresults.getrect(inum).BottomRight().Y();
    else
        return 0;
}
int FindObject::getresultx(int inum)
{
    if (inum < m_rectresults.size() && inum >= 0)
        return m_rectresults.getrect(inum).BottomRight().X();
    else
        return 0;
}
int FindObject::getresulty(int inum)
{
    if (inum < m_rectresults.size() && inum >= 0)
        return m_rectresults.getrect(inum).BottomRight().Y();
    else
        return 0;
}
int FindObject::getresultw(int inum)
{
    if (inum >= 0 && inum < m_rectresults.size())
        return m_rectresults.getrect(inum).Width();
    else
        return 0;
}
int FindObject::getresulth(int inum)
{
    if (inum >= 0 && inum < m_rectresults.size())
        return m_rectresults.getrect(inum).Height();
    else
        return 0;
}
int FindObject::getresultsize(int inum)
{
    if (inum >= 0 && inum < m_rectresults.size() && inum < static_cast<int>(m_vobjnum.size()))
        return m_vobjnum.at(inum);
    else
        return 0;
}
int FindObject::getresultobjsnum()
{
    return m_rectresults.size();
}
void FindObject::setdistance(int idist)
{
    switch (m_searchtype)
    {
    case ObjectSearchType::Search_O:
    {
        if (idist > 3 && idist < 1520)
            m_idistance = idist;
        else if (idist < 4)
            m_idistance = 4;
        else if (idist > 1519)
            m_idistance = 1520;
    } 
        break;
    case ObjectSearchType::Search_OL:
    case ObjectSearchType::Search_OR:
    case ObjectSearchType::Search_OU:
    case ObjectSearchType::Search_OD:
    case ObjectSearchType::Search_OX:
    {
        if (idist > 0 && idist < 16)
            m_idistance = idist;
        else if (idist < 0)
            m_idistance = 4;
        else if (idist > 16)
            m_idistance = 16;
    } 
        break;
    default:
    {
        m_idistance = 16; 
    }
        break;
    }
}
void FindObject::setoffset(int ix0, int ix1, int iy0, int iy1)
{
    m_ioffsetx0 = ix0;
    m_ioffsetx1 = ix1;
    m_ioffsety0 = iy0;
    m_ioffsety1 = iy1;
}
void FindObject::setsearchtype(int itype)
{
    switch (itype) {
    case 0:
        m_searchtype = ObjectSearchType::Search_O;
        m_SearchPointGroup = G_SearchPointGroup;
        break;
    case 1:
        m_searchtype = ObjectSearchType::Search_OL;
        m_SearchPointGroup = G_SearchPointGroup_L;
        break;
    case 11:
        m_searchtype = ObjectSearchType::Search_OL;
        m_SearchPointGroup = G_SearchPointGroup_LL;
        break;
    case 111:
        m_searchtype = ObjectSearchType::Search_OL;
        m_SearchPointGroup = G_SearchPointGroup_LLL;
        break;
    case 1111:
        m_searchtype = ObjectSearchType::Search_OL;
        m_SearchPointGroup = G_SearchPointGroup_LMAX;
        break;

    case 2:
        m_searchtype = ObjectSearchType::Search_OR;
        m_SearchPointGroup = G_SearchPointGroup_R;
        break;
    case 3:
        m_searchtype = ObjectSearchType::Search_OU;
        m_SearchPointGroup = G_SearchPointGroup_U;

        break;
    case 4:
        m_searchtype = ObjectSearchType::Search_OD;
        m_SearchPointGroup = G_SearchPointGroup_D;

        break;
    case 44:
        m_searchtype = ObjectSearchType::Search_OD;
        m_SearchPointGroup = G_SearchPointGroup_DD;

        break;
    case 444:
        m_searchtype = ObjectSearchType::Search_OD;
        m_SearchPointGroup = G_SearchPointGroup_DDD;

        break;

    case 40:
        m_searchtype = ObjectSearchType::Search_OD;
        m_SearchPointGroup = G_SearchPointGroup_OD;

        break;
    case 440:
        m_searchtype = ObjectSearchType::Search_OD;
        m_SearchPointGroup = G_SearchPointGroup_ODD;

        break;
    case 441:
        m_searchtype = ObjectSearchType::Search_OD;
        m_SearchPointGroup = G_SearchPointGroup_OOD;

        break;

    case 5:
        m_searchtype = ObjectSearchType::Search_OX;
        m_SearchPointGroup = G_SearchPointGroup_X;

        break;
    case 6:

        break;

    default:
        m_searchtype = ObjectSearchType::Search_O;
        m_SearchPointGroup = G_SearchPointGroup;

        break;
    }
}
void FindObject::Measure(Image& image)
{
    m_pgetimage = &image;
    if (image.getmat().empty())
        return;
    if (image.getWidth() < rect().TopLeft().X() + rect().Width()
        || image.getHeight() < rect().TopLeft().Y() + rect().Height())
        return;//error process
    int iw = rect().Width();
    int ih = rect().Height();
    int ix = rect().TopLeft().X();
    int iy = rect().TopLeft().Y();
    if (iw <= 0 || ih <= 0 || ix < 0 || iy < 0)
        return;
    if (g_pmapimage == nullptr ||
        m_objlistscanorA == nullptr ||
        m_objlistcollectorA == nullptr)
        return;
    if (g_pmapimage->getWidth() < ix + iw ||
        g_pmapimage->getHeight() < iy + ih)
        return;
    int ix1 = ix + iw;
    int iy1 = iy + ih;

    int m_isearchfirstx = rect().TopLeft().X();
    int m_isearchfirsty = rect().TopLeft().Y();

    int nx0 = 0;
    int ny0 = 0;
    int nx = 0;
    int ny = 0;
    int nservice = 0;
    cv::Vec3b abyte, bytenext;

    int nScanerID = 1;
    int icurScanerNUM = 0;
    bool boverflow = false;

    ncurscan = -1;
    nscansize = 0;

    ncursearchseek = -1;
    nsearchseeksize = 0;

    int ishowfont = 0;
    int mapservice = 0;
    int mapanalysis = 0;
    int mapedge = 0;

    int iminx = 9999;
    int iminy = 9999;
    int imaxx = 0;
    int imaxy = 0;

    int iborw = 0;

    switch (m_iborw)
    {
    case 901:
    {
        int isize = m_cent_h_bw_points_v.size();
        for (int i = 0; i < isize; i++)
            m_cent_h_bw_points_v[i].clear();
        m_cent_h_bw_points_v.clear();
    }
    break;
    case 902:
    {
        int isize = m_cent_v_bw_points_v.size();
        for (int i = 0; i < isize; i++)
            m_cent_v_bw_points_v[i].clear();
        m_cent_v_bw_points_v.clear();
    }
    break;
    case 903:
    {
        int isize = m_cent_h_wb_points_v.size();
        for (int i = 0; i < isize; i++)
            m_cent_h_wb_points_v[i].clear();
        m_cent_h_wb_points_v.clear();
    }
    break;
    case 904:
    {
        int isize = m_cent_v_wb_points_v.size();
        for (int i = 0; i < isize; i++)
            m_cent_v_wb_points_v[i].clear();
        m_cent_v_wb_points_v.clear();
    }
    break;
    }

    m_icurobj = 0;
    m_totalarea = 0;

    m_scanid.clear();
    m_vborw.clear();
    m_vrow.clear();
    m_vobjnum.clear();
    m_rectresults.clear();
    m_keypoint.clear();
    m_fitwh.clear();;
    MAPCLEAR();

    CLEAR_SEARCHSEEK();
    PUSH_SEARCHSEEK(m_isearchfirstx, m_isearchfirsty);

    for (int icurSeekNum = 0; icurSeekNum < nsearchseeksize;)//searchseek.size();)
    {
    FORBEGIN: 
        if (icurSeekNum < 0 || icurSeekNum >= nsearchseeksize)
            break;
        mapservice = MAP_service(m_objlistcollectorA[icurSeekNum].X(), m_objlistcollectorA[icurSeekNum].Y());

        if (mapservice > 0)
        {
            icurSeekNum++;

            if (icurSeekNum >= nsearchseeksize)
                break;
            else
                goto FORBEGIN;
        }
        iminx = 9999;
        iminy = 9999;
        imaxx = 0;
        imaxy = 0;

        CLEAR_SCANOR();
        PUSH_SCANOR(m_objlistcollectorA[icurSeekNum].X(), m_objlistcollectorA[icurSeekNum].Y());
        if (nscansize <= 0)
            break;
        iminx = static_cast<int>(m_objlistcollectorA[icurSeekNum].X());
        iminy = static_cast<int>(m_objlistcollectorA[icurSeekNum].Y());
        imaxx = iminx;
        imaxy = iminy;
        SetMAP_service(m_objlistcollectorA[icurSeekNum].X(), m_objlistcollectorA[icurSeekNum].Y(), nScanerID);
        int itestx0 = MAP_service(m_objlistcollectorA[icurSeekNum].X(), m_objlistcollectorA[icurSeekNum].Y());
        icurScanerNUM = 0;

    CURSCANERBEGIN:
        while (icurScanerNUM != nscansize)
        {
            nx0 = m_objlistscanorA[icurScanerNUM].X();
            ny0 = m_objlistscanorA[icurScanerNUM].Y();
            abyte = image.pixel(nx0, ny0);
            mapanalysis = MAP_analysis(nx0, ny0);

            if (mapanalysis == FindObject::ANLAYSIS_OVER)
                goto NEXTFINDSTEP;
            for (int i = 0; i < m_idistance; i++)
            {
                nx = nx0 + m_SearchPointGroup[i].X();
                ny = ny0 + m_SearchPointGroup[i].Y();
                if (nx < ix
                    || ny < iy
                    || nx >= ix1
                    || ny >= iy1)
                    continue;
                //mapanalysis = static_cast<BYTE>((MAP(nx, ny)[0] >> 4) & 0x0F);
                mapanalysis = MAP_analysis(nx, ny);
                mapservice = MAP_service(nx, ny);
                mapedge = MAP_edge(nx, ny);
                if (mapanalysis == ANLAYSIS_OVER
                    || mapservice > 0
                    || (mapedge == mapservice && 0 != mapedge))
                    continue;
                bytenext = image.pixel(nx, ny);
                 
                if (abyte == bytenext)
                {
                    PUSH_SCANOR(nx, ny);
                    SetMAP_service(nx, ny, nScanerID);
                    iborw = (abyte[0] == 0) ? 0 : 1;
                    SetMAP_pixel(nx, ny, iborw);
                    if (iminx > nx)
                        iminx = nx;
                    if (iminy > ny)
                        iminy = ny;
                    if (imaxx < nx)
                        imaxx = nx;
                    if (imaxy < ny)
                        imaxy = ny;
                }
                else
                {
                    PUSH_SEARCHSEEK(nx, ny);
                    SetMAP_edge(nx, ny, nScanerID); 
                }
            }
            SetMAP_analysis(nx0, ny0, ANLAYSIS_OVER); 
        NEXTFINDSTEP:
            icurScanerNUM++;
        }
    CURSCANEREND:
        int iobjw = (imaxx - iminx <= 0) ? 1 : imaxx - iminx;
        int iobjh = (imaxy - iminy <= 0) ? 1 : imaxy - iminy;
        if ((m_ifilterNedge > 0
            && (iminx > m_ifilterNedge
                && imaxx<iw - m_ifilterNedge
                && iminy>m_ifilterNedge
                && imaxy < ih - m_ifilterNedge))
            || m_ifilterNedge == 0)
        {
            if (nscansize > m_iminarea
                && nscansize < m_imaxarea
                && iobjw < m_imaxobjw
                && iobjw >= m_iminobjw
                && iobjh < m_imaxobjh
                && iobjh >= m_iminobjh)
            {
                m_vrow.push_back(nscansize);

                abyte = image.pixel(m_objlistscanorA[0].X(), m_objlistscanorA[0].Y());

                m_vborw.push_back(abyte);

                boverflow = false;
                {
                    switch (m_iborw)
                    {
                    case 3://any white or black
                    {
                        gp_Rectangle arectresult(gp_Pnt(iminx, iminy,0), gp_Pnt(imaxx, imaxy,0));
                        gp_Pnt apoint(m_objlistscanorA[0].X(), m_objlistscanorA[0].Y(),0);
                        m_keypoint.addpoint(apoint);

                        m_rectresults.addrect(arectresult);
                        m_scanid.push_back(nScanerID);
                        m_vobjnum.push_back(nscansize);
                        m_iobjnum++;
                    }
                    ishowfont++;
                    m_icurobj++;

                    break;
                    case 0:break;// no
                    case 1://select w
                    {
                        {
                            gp_Rectangle arectresult(gp_Pnt(iminx, iminy,0), gp_Pnt(imaxx, imaxy,0));
                            if (m_vborw[m_icurobj][0] > 0)
                            {
                                gp_Pnt apoint(m_objlistscanorA[0].X(), m_objlistscanorA[0].Y(),0);
                                m_keypoint.addpoint(apoint);
                                m_rectresults.addrect(arectresult);
                                m_scanid.push_back(nScanerID);
                                m_vobjnum.push_back(nscansize);
                                m_totalarea = m_totalarea + nscansize;
                                m_iobjnum++;
                            }
                            ishowfont++;
                            m_icurobj++;

                        }
                    }
                    break;
                    case 2://select black
                    {
                        {
                            gp_Rectangle arectresult(gp_Pnt(iminx, iminy,0), gp_Pnt(imaxx, imaxy,0));
                            if (m_vborw[m_icurobj][0] < 255)
                            {
                                gp_Pnt apoint(m_objlistscanorA[0].X(), m_objlistscanorA[0].Y(),0);
                                m_keypoint.addpoint(apoint);

                                m_rectresults.addrect(arectresult);
                                m_scanid.push_back(nScanerID);
                                m_vobjnum.push_back(nscansize);

                                m_totalarea = m_totalarea + nscansize;
                                m_iobjnum++;
                            }
                            ishowfont++;
                            m_icurobj++;

                        }
                    }
                    break;

                    case 11://test
                    {

                        gp_Rectangle arectresult(gp_Pnt(iminx, iminy,0), gp_Pnt(imaxx, imaxy,0));
                        if (m_vborw[m_icurobj][0] > 0)
                        {
                            for (int ir = 0; ir < nscansize; ir++)
                            {
                                image.setPixel(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y(), cv::Vec3b(0, 0, 0));
                            }
                            gp_Pnt apoint(m_objlistscanorA[0].X(), m_objlistscanorA[0].Y(),0);
                            m_keypoint.addpoint(apoint);
                            m_rectresults.addrect(arectresult);
                            m_scanid.push_back(nScanerID);
                            m_vobjnum.push_back(nscansize);

                            m_totalarea = m_totalarea + nscansize;
                            m_iobjnum++;
                        }
                        ishowfont++;
                        m_icurobj++;

                    }
                    break;
                    case 101://test
                    {

                        gp_Rectangle arectresult(gp_Pnt(iminx, iminy,0), gp_Pnt(imaxx, imaxy,0));
                        if (m_vborw[m_icurobj][0] > 0)
                        {
                            for (int ir = 0; ir < nscansize; ir++)
                            {
                                image.setPixel(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y(), cv::Vec3b(255, 255, 255));
                            }
                            gp_Pnt apoint(m_objlistscanorA[0].X(), m_objlistscanorA[0].Y(),0);
                            m_keypoint.addpoint(apoint);
                            m_rectresults.addrect(arectresult);
                            m_scanid.push_back(nScanerID);
                            m_vobjnum.push_back(nscansize);

                            m_totalarea = m_totalarea + nscansize;
                            m_iobjnum++;
                        }
                        ishowfont++;
                        m_icurobj++;

                    }
                    break;
                    case 12://test
                    {
                        {
                            gp_Rectangle arectresult(gp_Pnt(iminx, iminy,0), gp_Pnt(imaxx, imaxy,0));
                            if (m_vborw[m_icurobj][0] < 255)
                            {
                                for (int ir = 0; ir < nscansize; ir++)
                                {
                                    image.setPixel(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y(), cv::Vec3b(0, 0, 0));
                                }
                                gp_Pnt apoint(m_objlistscanorA[0].X(), m_objlistscanorA[0].Y(),0);
                                m_keypoint.addpoint(apoint);

                                m_rectresults.addrect(arectresult);
                                m_scanid.push_back(nScanerID);
                                m_vobjnum.push_back(nscansize);

                                m_totalarea = m_totalarea + nscansize;
                                m_iobjnum++;
                            }
                            ishowfont++;
                            m_icurobj++;

                        }

                    }
                    break;
                    case 102://test
                    {
                        {
                            gp_Rectangle arectresult(gp_Pnt(iminx, iminy,0), gp_Pnt( imaxx, imaxy,0));
                            if (m_vborw[m_icurobj][0] < 255)
                            {
                                for (int ir = 0; ir < nscansize; ir++)
                                {
                                    image.setPixel(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y(), cv::Vec3b(255, 255, 255));
                                }
                                gp_Pnt apoint(m_objlistscanorA[0].X(), m_objlistscanorA[0].Y(),0);
                                m_keypoint.addpoint(apoint);

                                m_rectresults.addrect(arectresult);
                                m_scanid.push_back(nScanerID);
                                m_vobjnum.push_back(nscansize);

                                m_totalarea = m_totalarea + nscansize;
                                m_iobjnum++;
                            }
                            ishowfont++;
                            m_icurobj++;

                        }
                    }
                    break;
                    case 13://test
                    {
                        {
                            if (m_vborw[m_icurobj][0] < 255)
                            {
                                for (int ir = 0; ir < nscansize; ir++)
                                {
                                    image.setPixel(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y(), cv::Vec3b(255, 0, 0));
                                }
                            }
                            if (m_vborw[m_icurobj][0] > 0)
                            {
                                for (int ir = 0; ir < nscansize; ir++)
                                {
                                    image.setPixel(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y(), cv::Vec3b(0, 0, 255));
                                }
                            }
                            ishowfont++;
                            m_icurobj++;

                        }
                    }
                    break;
                    case 901:
                    {
                        if (m_vborw[m_icurobj][0] < 255)
                        {
                            TwoPointsShape atpshape;
                            for (int ir = 0; ir < nscansize; ir++)
                            {
                                atpshape.addpoint(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y());
                            }
                            m_cent_h_bw_points_v.push_back(atpshape);
                        }
                        if (m_vborw[m_icurobj][0] > 0)
                        {
                            TwoPointsShape atpshape;
                            for (int ir = 0; ir < nscansize; ir++)
                            {
                                atpshape.addpoint(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y());
                            }
                            m_cent_h_bw_points_v.push_back(atpshape);
                        }
                        ishowfont++;
                        m_icurobj++;
                    }
                    break;
                    case 902:
                    {
                        if (m_vborw[m_icurobj][0] < 255)
                        {
                            TwoPointsShape atpshape;
                            for (int ir = 0; ir < nscansize; ir++)
                            {
                                atpshape.addpoint(m_objlistscanorA[ir].Y(), m_objlistscanorA[ir].X());
                            }
                            m_cent_v_bw_points_v.push_back(atpshape);
                        }
                        if (m_vborw[m_icurobj][0] > 0)
                        {
                            TwoPointsShape atpshape;
                            for (int ir = 0; ir < nscansize; ir++)
                            {
                                atpshape.addpoint(m_objlistscanorA[ir].Y(), m_objlistscanorA[ir].X());
                            }
                            m_cent_v_bw_points_v.push_back(atpshape);
                        }
                        ishowfont++;
                        m_icurobj++;
                    }
                    break;
                    case 903:
                    {
                        if (m_vborw[m_icurobj][0] < 255)
                        {
                            TwoPointsShape atpshape;
                            for (int ir = 0; ir < nscansize; ir++)
                            {
                                atpshape.addpoint(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y());
                            }
                            m_cent_h_wb_points_v.push_back(atpshape);
                        }
                        if (m_vborw[m_icurobj][0] > 0)
                        {
                            TwoPointsShape atpshape;
                            for (int ir = 0; ir < nscansize; ir++)
                            {
                                atpshape.addpoint(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y());
                            }
                            m_cent_h_wb_points_v.push_back(atpshape);
                        }
                        ishowfont++;
                        m_icurobj++;
                    }
                    break;
                    case 904:
                    {
                        if (m_vborw[m_icurobj][0] < 255)
                        {
                            TwoPointsShape atpshape;
                            for (int ir = 0; ir < nscansize; ir++)
                            {
                                atpshape.addpoint(m_objlistscanorA[ir].Y(), m_objlistscanorA[ir].X());
                            }
                            m_cent_v_wb_points_v.push_back(atpshape);
                        }
                        if (m_vborw[m_icurobj][0] > 0)
                        {
                            TwoPointsShape atpshape;
                            for (int ir = 0; ir < nscansize; ir++)
                            {
                                atpshape.addpoint(m_objlistscanorA[ir].Y(), m_objlistscanorA[ir].X());
                            }
                            m_cent_v_wb_points_v.push_back(atpshape);
                        }
                        ishowfont++;
                        m_icurobj++;
                    }
                    break;
                    }
                }
            }
            else
            {
                //full color no selected object
                switch (m_iborw)
                {
                    // no
                case 21://select w
                {
                    for (int ir = 0; ir < nscansize; ir++)
                    {
                        image.setPixel(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y(), cv::Vec3b(0, 0, 0));
                    }
                }
                break;
                case 22://select black
                {
                    for (int ir = 0; ir < nscansize; ir++)
                    {
                        image.setPixel(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y(), cv::Vec3b(255, 255, 255));
                    }
                }
                break;
                case 23://test
                {
                    for (int ir = 0; ir < nscansize; ir++)
                    {
                        image.setPixel(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y(), cv::Vec3b(255, 0, 0));
                    }
                }
                break;

                }
            }
        }
        else
        {
            //full color no selected object
            switch (m_iborw)
            {
                // no
            case 21://select w
            {
                for (int ir = 0; ir < nscansize; ir++)
                {
                    image.setPixel(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y(), cv::Vec3b(0, 0, 0));
                }
            }
            break;
            case 22://select black
            {
                for (int ir = 0; ir < nscansize; ir++)
                {
                    image.setPixel(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y(), cv::Vec3b(255, 255, 255));
                }
            }
            break;
            case 23://test
            {
                for (int ir = 0; ir < nscansize; ir++)
                {
                    image.setPixel(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y(), cv::Vec3b(255, 0, 0));
                }
            }
            break;
            }
        }

        iminx = 9999;
        iminy = 9999;
        imaxx = 0;
        imaxy = 0;

        nScanerID++;
        icurSeekNum++;
    FOREND:
        ;
    }


    switch (m_iborw)
    {
    case 901:
    {
        int isize = m_cent_h_bw_points_v.size();
        for (int i = 0; i < isize; i++)
            m_cent_h_bw_points_v[i].makepath(0);

    }
    break;
    case 902:
    {
        int isize = m_cent_v_bw_points_v.size();
        for (int i = 0; i < isize; i++)
            m_cent_v_bw_points_v[i].makepath(1);
    }
    break;
    case 903:
    {
        int isize = m_cent_h_wb_points_v.size();
        for (int i = 0; i < isize; i++)
            m_cent_h_wb_points_v[i].makepath(0);

    }
    break;
    case 904:
    {
        int isize = m_cent_v_wb_points_v.size();
        for (int i = 0; i < isize; i++)
            m_cent_v_wb_points_v[i].makepath(1);
    }
    break;
    }


    CLEAR_SCANOR();
    CLEAR_SEARCHSEEK();

}
void FindObject::MeasureGrid(Grid* pgrid)
{

}

void FindObject::Edge(int inum)
{
    if (inum >= m_scanid.size()
        || inum < 0)
        return;
    int imapservice = m_scanid[inum];
    int ix = getresultx(inum);
    int iy = getresulty(inum);
    int iw = getresultw(inum);
    int ih = getresulth(inum);

    m_curedge.setshow(3);
    m_curedge.clear();

    for (int iy0 = 0; iy0 < ih; iy0++)
    {
        int ibeginx = 0;
        int iendx = iw - 1;
        for (int ix0 = 0; ix0 < iw; ix0++)
        {
            int imapservicecur = MAP_service(ix0 + ix, iy0 + iy);
            if (imapservicecur == imapservice)
            {

                ibeginx = ix0;
                break;
            }
        }
        for (int ix0 = iw - 1; ix0 >= 0; ix0--)
        {
            int imapservicecur = MAP_service(ix0 + ix, iy0 + iy);
            if (imapservicecur == imapservice)
            {
                iendx = ix0;
                break;
            }
        }
        Standard_Real qrx1 = ibeginx + ix;
        Standard_Real qry1 = iy0 + iy;
        Standard_Real qrx2 = iendx + ix;
        Standard_Real qry2 = iy0 + iy;

        //LineShape aline;
        //aline.setline(ibeginx+ix,iy0+iy,iendx+ix,iy0+iy);
        //m_curedge.push_back(aline);
        //int inum = m_curedge.size() - 1;
        //m_curedge[inum].setPercent(m_dsamplerate);

        m_curedge.addpointa(qrx1, qry1);
        m_curedge.addpointb(qrx2, qry2);
    }
}
void FindObject::Object(int inum)
{
    if (inum >= m_scanid.size()
        || inum < 0)
        return;
    int imapservice = m_scanid[inum];
    int ix = getresultx(inum);
    int iy = getresulty(inum);
    int iw = getresultw(inum);
    int ih = getresulth(inum);

    m_curobject.setshow(3);
    m_curobject.clear();

    for (int iy0 = 0; iy0 < ih; iy0++)
    {
        for (int ix0 = 0; ix0 < iw; ix0++)
        {
            int imapservicecur = MAP_service(ix0 + ix, iy0 + iy);
            if (imapservicecur == imapservice)
            {
                Standard_Real qrx1 = ix0;
                Standard_Real qry1 = iy0;

                m_curobject.addpoint(qrx1, qry1);

            }
        } 
        //LineShape aline;
        //aline.setline(ibeginx+ix,iy0+iy,iendx+ix,iy0+iy);
        //m_curedge.push_back(aline);
        //int inum = m_curedge.size() - 1;
        //m_curedge[inum].setPercent(m_dsamplerate);

    }
}

void FindObject::measure(void* pimage)
{
    Image* pgetimage = (Image*)pimage;
    if (pgetimage == nullptr)
        return;
    Measure(*pgetimage);
}

void FindObject::sethsogap(int ihgap, int isgap, int iogap)
{
    m_ihgap = ihgap;
    m_isgap = isgap;
    m_iogap = iogap;
}
void FindObject::setminmaxarea(int imin, int imax)
{
    m_iminarea = imin;
    m_imaxarea = imax;
}
void FindObject::MeasureX(Image& image)
{
    m_pgetimage = &image;
    if (image.getWidth() < rect().TopLeft().X() + rect().Width()
        || image.getHeight() < rect().TopLeft().Y() + rect().Height())
        return;//error process
    int iw = rect().Width();
    int ih = rect().Height();
    int ix = rect().TopLeft().X();
    int iy = rect().TopLeft().Y();
    int ix1 = ix + iw;
    int iy1 = iy + ih;

    int m_isearchfirstx = rect().TopLeft().X();
    int m_isearchfirsty = rect().TopLeft().Y();

    int nx0 = 0;
    int ny0 = 0;
    int nx = 0;
    int ny = 0;
    int nservice = 0;
    cv::Vec3b abyte, bytenext;

    int nScanerID = 1;
    int icurScanerNUM = 0;
    bool boverflow = false;

    ncurscan = -1;
    nscansize = 0;

    ncursearchseek = -1;
    nsearchseeksize = 0;

    int ishowfont = 0;
    int mapservice = 0;
    int mapanalysis = 0;
    int mapedge = 0;

    int iminx = 9999;
    int iminy = 9999;
    int imaxx = 0;
    int imaxy = 0;

    int iborw = 0;

    m_icurobj = 0;
    m_iobjnum = 0;
    m_totalarea = 0;

    m_scanid.clear();
    m_vborw.clear();
    m_vrow.clear();
    m_vobjnum.clear();
    m_rectresults.clear();
    m_keypoint.clear();
    m_fitwh.clear();;
    MAPCLEAR();

    CLEAR_SEARCHSEEK();
    PUSH_SEARCHSEEK(m_isearchfirstx, m_isearchfirsty);

    for (int icurSeekNum = 0; icurSeekNum < nsearchseeksize;)//searchseek.size();)
    {
    FORBEGIN:
        mapservice = MAP_service(m_objlistcollectorA[icurSeekNum].X(), m_objlistcollectorA[icurSeekNum].Y());

        if (mapservice > 0)
        {
            icurSeekNum++;

            if (icurSeekNum >= nsearchseeksize)
                break;
            else
                goto FORBEGIN;
        }
        CLEAR_SCANOR();
        PUSH_SCANOR(m_objlistcollectorA[icurSeekNum].X(), m_objlistcollectorA[icurSeekNum].Y());
        SetMAP_service(m_objlistcollectorA[icurSeekNum].X(), m_objlistcollectorA[icurSeekNum].Y(), nScanerID);
        icurScanerNUM = 0;

    CURSCANERBEGIN:
        while (icurScanerNUM != nscansize)
        {
            nx0 = m_objlistscanorA[icurScanerNUM].X();
            ny0 = m_objlistscanorA[icurScanerNUM].Y();
            abyte = image.pixel(nx0, ny0);

            mapanalysis = MAP_analysis(nx0, ny0);

            if (mapanalysis == FindObject::ANLAYSIS_OVER)
                goto NEXTFINDSTEP;
            for (int i = 0; i < m_idistance; i++)
            {
                nx = nx0 + m_SearchPointGroup[i].X();
                ny = ny0 + m_SearchPointGroup[i].Y();
                if (nx <= ix
                    || ny <= iy
                    || nx >= ix1
                    || ny >= iy1)
                    continue;
                mapanalysis = MAP_analysis(nx, ny);
                mapservice = MAP_service(nx, ny);
                mapedge = MAP_edge(nx, ny);
                if (mapanalysis == ANLAYSIS_OVER
                    || mapservice!=0
                    ||(mapedge == mapservice &&0!= mapedge))
                    continue;
                bytenext = image.pixel(nx, ny);

                if(abs(abyte[0] - bytenext[0])<m_ihgap &&
                    abs(abyte[1] - bytenext[1])< m_isgap &&
                    abs(abyte[2] - bytenext[2])< m_iogap)
                {
                    PUSH_SCANOR(nx, ny);
                    SetMAP_service(nx, ny, nScanerID);
                    iborw = (abyte[0] == 0) ? 0 : 1;
                    SetMAP_pixel(nx, ny, iborw);
                    if (iminx > nx)
                        iminx = nx;
                    if (iminy > ny)
                        iminy = ny;
                    if (imaxx < nx)
                        imaxx = nx;
                    if (imaxy < ny)
                        imaxy = ny;
                }
                else
                {
                    PUSH_SEARCHSEEK(nx, ny);
                    SetMAP_edge(nx, ny, nScanerID);
                     
                }
            }
            SetMAP_analysis(nx0, ny0, ANLAYSIS_OVER);
        NEXTFINDSTEP:
            icurScanerNUM++;
        }
    CURSCANEREND:
        int iobjw = imaxx - iminx;
        int iobjh = imaxy - iminy;
        if ((m_ifilterNedge > 0
            && (iminx > m_ifilterNedge
                && imaxx<iw - m_ifilterNedge
                && iminy>m_ifilterNedge
                && imaxy < ih - m_ifilterNedge))
            || m_ifilterNedge == 0)
        {
            if (nscansize > m_iminarea
                && nscansize < m_imaxarea
                && iobjw < m_imaxobjw
                && iobjw >= m_iminobjw
                && iobjh < m_imaxobjh
                && iobjh >= m_iminobjh)
            {
                m_vrow.push_back(nscansize);

                abyte = image.pixel(m_objlistscanorA[0].X(), m_objlistscanorA[0].Y());

                m_vborw.push_back(abyte);

                boverflow = false;
                {
                    switch (m_iborw)
                    {
                    case 3://any white or black
                    {
                        gp_Rectangle arectresult(gp_Pnt(iminx, iminy,0), gp_Pnt(imaxx, imaxy,0));
                        gp_Pnt apoint(m_objlistscanorA[0].X(), m_objlistscanorA[0].Y(),0);
                        m_keypoint.addpoint(apoint);

                        m_rectresults.addrect(arectresult);
                        m_scanid.push_back(nScanerID);
                        m_iobjnum++;
                    }
                    ishowfont++;
                    m_icurobj++;


                    break;
                    case 13://test
                    {
                        {
                            if (m_vborw[m_icurobj][0] < 255)
                            {
                                for (int ir = 0; ir < nscansize; ir++)
                                {
                                    image.setPixel(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y(), cv::Vec3b(255, 0, 0));
                                }
                            }
                            if (m_vborw[m_icurobj][0] > 0)
                            {
                                for (int ir = 0; ir < nscansize; ir++)
                                {
                                    image.setPixel(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y(), cv::Vec3b(0, 0, 255));
                                }
                            }
                            ishowfont++;
                            m_icurobj++;

                        }
                    }
                    break;

                    }
                }
            }
            else
            {
                //full color no selected object
                switch (m_iborw)
                {
                    // no
                case 21://select w
                {
                    for (int ir = 0; ir < nscansize; ir++)
                    {
                        image.setPixel(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y(), cv::Vec3b(0, 0, 0));
                    }
                }
                break;
                case 22://select black
                {
                    for (int ir = 0; ir < nscansize; ir++)
                    {
                        image.setPixel(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y(), cv::Vec3b(255, 255, 255));
                    }
                }
                break;
                case 23://test
                {
                    for (int ir = 0; ir < nscansize; ir++)
                    {
                        image.setPixel(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y(), cv::Vec3b(255, 0, 0));
                    }
                }
                break;

                }
            }
        }
        else
        {
            //full color no selected object
            switch (m_iborw)
            {
                // no
            case 21://select w
            {
                for (int ir = 0; ir < nscansize; ir++)
                {
                    image.setPixel(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y(), cv::Vec3b(0, 0, 0));
                }
            }
            break;
            case 22://select black
            {
                for (int ir = 0; ir < nscansize; ir++)
                {
                    image.setPixel(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y(), cv::Vec3b(255, 255, 255));
                }
            }
            break;
            case 23://test
            {
                for (int ir = 0; ir < nscansize; ir++)
                {
                    image.setPixel(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y(), cv::Vec3b(255, 0, 0));
                }
            }
            break;
            }
        }
        iminx = 9999;
        iminy = 9999;
        imaxx = 0;
        imaxy = 0;

        nScanerID++;
        icurSeekNum++;
    FOREND:
        ;
    }

    CLEAR_SCANOR();
    CLEAR_SEARCHSEEK();

}
void FindObject::measurex(void* pimage)
{
    Image* pgetimage = (Image*)pimage;
    if (pgetimage == nullptr)
        return;

    MeasureX(*pgetimage);
}

void FindObject::setminmaxwh(int iminw, int imaxw, int iminh, int imaxh)
{
    m_imaxobjw = imaxw;
    m_iminobjw = iminw;
    m_imaxobjh = imaxh;
    m_iminobjh = iminh;
}
void FindObject::setobjectgrid(int iw, int ih, int ixgrid)
{
    m_icopyw = iw;
    m_icopyh = ih;
    m_icopywgrid = ixgrid;
}
int FindObject::getobjectgridw()
{
    return m_icopyw;
}
int FindObject::getobjectgridh()
{
    return m_icopyh;
}

void FindObject::setbackground(int iedge, int ibackgroundmethod)
{
    m_background_edge = iedge;
    m_background_method = ibackgroundmethod;
}
void FindObject::resultsrectfilter()
{
    m_rectresults.removecontains_c();
}
void FindObject::objectgrid(void* pimage)
{
 /*   Image* pgetimage = (Image*)pimage;
    m_rectgrids.clear();
    int isize = m_rectresults.size();
    int inumw = 0;
    int inumh = 0;
    int ibeginx = 0;
    int ibeginy = 0; 
    for (int inum = 0; inum < isize; inum++)
    {
        if (inumw >= m_icopywgrid)
        {
            inumw = 0;
            inumh++;
        } 
        int iobjx0 = getresultx(inum);
        int iobjy0 = getresulty(inum);
        int iobjw = getresultw(inum);
        int iobjh = getresulth(inum); 
        iobjx0 = iobjx0 + m_ioffsetx0;
        iobjy0 = iobjy0 + m_ioffsety0;
        iobjw = iobjw + m_ioffsetx1;
        iobjh = iobjh + m_ioffsety1; 
        ibeginx = inumw * m_icopyw;
        ibeginy = inumh * m_icopyh; 
        int igridsum = (iobjw + 10) / m_icopyw + ((iobjw + 10) % m_icopyw > 0 ? 1 : 0);
        pgetimage->setroi(iobjx0, iobjy0, iobjw + 1, iobjh + 1);
        cv::Vec3b acolor = pgetimage->ROIBackground(m_background_edge, m_background_edge, m_background_edge, m_background_edge, m_background_method);
        g_pbackobjectimage->setroi(ibeginx, ibeginy, m_icopyw * igridsum, m_icopyh);
        g_pbackobjectimage->set(acolor); 
        g_pbackobjectimage->SetROI(ibeginx + 10, ibeginy + 10, iobjw + 1, iobjh + 1); 
        pgetimage->SetMode(3);
        pgetimage->ROItoROI(g_pbackobjectimage); 
        gp_Rectangle arect(ibeginx, ibeginy, m_icopyw * igridsum, m_icopyh);
        m_rectgrids.addrect(arect); 
        inumw = inumw + igridsum;
    }
*/

}
void FindObject::objectsort()
{
    m_rectresults.sort();
}
gp_Rectangle FindObject::getgrid(int inum)
{
    //m_rectgrids
    int igridh = inum / m_icopywgrid;
    int igridw = inum % m_icopywgrid;

    int ibeginx = igridw * m_icopyw;
    int ibeginy = igridh * m_icopyh;

    gp_Rectangle arect(gp_Pnt(ibeginx, ibeginy,0), gp_Pnt( m_icopyw, m_icopyh,0));

    return arect;

}
gp_Rectangle FindObject::getgridex(int inum)
{
    if (inum < m_rectgrids.size()
        && inum >= 0)
        return m_rectgrids.getrect(inum);
    else
        return gp_Rectangle(gp_Pnt(0, 0,0), gp_Pnt( 0, 0,0));
}
void FindObject::setedgeoi(int iw, int ioffset, int iheadtail)
{
    switch (m_iborw)
    {
    case 901:
    {
        int isize = m_cent_h_bw_points_v.size();
        for (int i = 0; i < isize; i++)
            m_cent_h_bw_points_v[i].setedgeoi(iw, ioffset, iheadtail);
    }
    break;
    case 902:
    {
        int isize = m_cent_v_bw_points_v.size();
        for (int i = 0; i < isize; i++)
            m_cent_v_bw_points_v[i].setedgeoi(iw, ioffset, iheadtail);
    }
    break;
    case 903:
    {
        int isize = m_cent_h_wb_points_v.size();
        for (int i = 0; i < isize; i++)
            m_cent_h_wb_points_v[i].setedgeoi(iw, ioffset, iheadtail);
    }
    break;
    case 904:
    {
        int isize = m_cent_v_wb_points_v.size();
        for (int i = 0; i < isize; i++)
            m_cent_v_wb_points_v[i].setedgeoi(iw, ioffset, iheadtail);
    }
    break;
    }
}
void FindObject::edgeimage(void* pimage)
{
    if (0 == pimage)
        return;
    Image* paimage = (Image*)pimage;
    switch (m_iborw)
    {
    case 901:
    {
        int isize = m_cent_h_bw_points_v.size();
        for (int i = 0; i < isize; i++)
            m_cent_h_bw_points_v[i].edgeimage(paimage->getmat(), 0);
    }
    break;
    case 902:
    {
        int isize = m_cent_v_bw_points_v.size();
        for (int i = 0; i < isize; i++)
            m_cent_v_bw_points_v[i].edgeimage(paimage->getmat(), 1);
    }
    break;
    case 903:
    {
        int isize = m_cent_h_wb_points_v.size();
        for (int i = 0; i < isize; i++)
            m_cent_h_wb_points_v[i].edgeimage(paimage->getmat(), 2);
    }
    break;
    case 904:
    {
        int isize = m_cent_v_wb_points_v.size();
        for (int i = 0; i < isize; i++)
            m_cent_v_wb_points_v[i].edgeimage(paimage->getmat(), 3);
    }
    break;
    }
}
void FindObject::setcolorstyle(int istyle)
{
    m_istyle = istyle;
}
void FindObject::drawshapex(
    double dmovx,
    double dmovy,
    double dangle,
    double dzoomx,
    double dzoomy)
{
    /*
    if (show() & 0x02)
    {
        m_rectresults.drawshapex( dmovx,
            dmovy,
            dangle,
            dzoomx,
            dzoomy);
    }
    if (show() & 0x04)
    {
        m_curedge.drawshape();
        m_curobject.drawshape();
    }
    if (show() & 0x08)
    {
        int isize = m_cent_h_bw_points_v.size();
        for (int i = 0; i < isize; i++)
            m_cent_h_bw_points_v[i].drawshape();
        isize = m_cent_v_bw_points_v.size();
        for (int i = 0; i < isize; i++)
            m_cent_v_bw_points_v[i].drawshape();
        isize = m_cent_h_wb_points_v.size();
        for (int i = 0; i < isize; i++)
            m_cent_h_wb_points_v[i].drawshape();
        isize = m_cent_v_wb_points_v.size();
        for (int i = 0; i < isize; i++)
            m_cent_v_wb_points_v[i].drawshape();
    }
    gp_Rectangle arect;
    arect.setX(rect().x() * dzoomx + dmovx);
    arect.setY(rect().y() * dzoomy + dmovy);
    arect.setWidth(rect().Width() * dzoomx + dmovx);
    arect.setHeight(rect().height() * dzoomy + dmovy);
    */
}

void FindObject::setrelationrectfromresultnum(int inum)
{
    m_irelationresultnum = inum;
}
void FindObject::setrelationrectfrom_matchresult(void* pmatch)
{
    /* m_prelationmatch = (FastMatch*)pmatch;
    if (0 != m_prelationmatch)
    {
        int inum = m_prelationmatch->getresultrects()->size();
        if (m_irelationresultnum >= 0 && m_irelationresultnum < inum)
        {
            m_irelationrect = m_prelationmatch->getresultrects()->getrect(m_irelationresultnum);
        }
    }
    */
}
void FindObject::setrelationxy(int iprex1, int iprey1, int iendx1, int iendy1)
{
    /*    m_irelationrect.setLeft(m_irelationrect.left() + iprex1);
    m_irelationrect.setTop(m_irelationrect.top() + iprey1);
    m_irelationrect.setRight(m_irelationrect.right() + iendx1);
    m_irelationrect.setBottom(m_irelationrect.bottom() + iendy1);
    */

}
void FindObject::setrelationzoom(double drelationzoomx, double drelationzoomy)
{
    /*
    m_irelationrect.setLeft((double)m_irelationrect.left() * drelationzoomx);
    m_irelationrect.setTop((double)m_irelationrect.top() * drelationzoomy);
    m_irelationrect.setRight((double)m_irelationrect.right() * drelationzoomx);
    m_irelationrect.setBottom((double)m_irelationrect.bottom() * drelationzoomy);
*/
}
void FindObject::setrelationtorect()
{/*
    if (m_irelationrect.x() >= 0
        && m_irelationrect.y() >= 0
        && m_irelationrect.Width() > 0
        && m_irelationrect.height() > 0)
        Shape::setrect(m_irelationrect.x(),
            m_irelationrect.y(),
            m_irelationrect.Width(),
            m_irelationrect.height());
*/
}
void FindObject::SetImageROIthre(int ithre)
{
    m_imagethre = ithre;
}
void FindObject::SetImageROIincrease(int increase)
{
    m_imagethreincrease = increase;
}
void FindObject::SetImageROIcomparegap(int icomparegap)
{
    m_imagecomparegap = icomparegap;
}
void FindObject::SetImageROIfindBorW(int ifindBorW)
{
    m_imagefindBorW = ifindBorW;
}
void FindObject::SetImageROIedge_5o7(int i5o7)
{
    m_imageedge_5o7 = i5o7;
}
void FindObject::ImageROIthre(void* pimage)
{
    Image* pgetimage = (Image*)pimage;
//    pgetimage->SetROI(rect().x(), rect().y(), rect().Width(), rect().height());
//    pgetimage->ROIImageThre(m_imagethre);
}
void FindObject::ImageROIedge(void* pimage)
{
 /*   Image* pgetimage = (Image*)pimage;
    pgetimage->SetROI(rect().x(), rect().y(), rect().Width(), rect().height());
    if (5 == m_imageedge_5o7)
        pgetimage->ROIImage_5Blur_Gap_mud_thre_BW(m_imagethre, m_imagethreincrease, m_imagecomparegap, m_imagefindBorW);
    else if (7 == m_imageedge_5o7)
        pgetimage->ROIImage_7Blur_Gap_mud_thre_BW(m_imagethre, m_imagethreincrease, m_imagecomparegap, m_imagefindBorW);
*/
}
void FindObject::ImageROIedgeH(void* pimage)
{
/*  Image* pgetimage = (Image*)pimage;
    pgetimage->SetROI(rect().x(), rect().y(), rect().Width(), rect().height());
    if (5 == m_imageedge_5o7)
        pgetimage->ROIImage_5Blur_Gap_mud_thre_BW_H(m_imagethre, m_imagethreincrease, m_imagecomparegap, m_imagefindBorW);
    else if (7 == m_imageedge_5o7)
        pgetimage->ROIImage_7Blur_Gap_mud_thre_BW_H(m_imagethre, m_imagethreincrease, m_imagecomparegap, m_imagefindBorW);
*/
}
void FindObject::shapesetroi(void* pshape)
{
    if (pshape == nullptr)
        return;
    Shape::shapesetroi(pshape);
}
