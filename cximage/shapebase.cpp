#include "pch.h"

#include "shapebase.h"
#include "Findline.h"
#include "Sysctl.h"
#if defined USE_AI
#include "mlpackrun.h"
#endif

#include <nanoflann.hpp>
#include <vector>
#include <map>
#include <cmath>
#include <algorithm>
#include <iostream>


//#include "mlpackrun.h"
//#include "CONST_PointDistancetable.h"
  
void cross_line(double l1x1,double l1y1,double l1x2,double l1y2,
               double l2x1,double l2y1,double l2x2,double l2y2,
               double &cx,double &cy
               )
{
    double k1;
    double b1;
    double k2;
    double b2;
    if(l1y1!=l1y2
    &&l1x2!=l1x1)
    {
        //k1 //b1
        k1=(l1y1-l1y2)/(l1x1-l1x2);
        b1=(l1y1*l1x2-l1y2*l1x1)/(l1x2-l1x1);
    }
    if(l2y1!=l2y2
    &&l2x2!=l2x1)
    {
        //k2 //b2
        k2=(l2x1-l2x2)/(l2y1-l2y2);
        b2=(l2y1*l2x2-l2y2*l2x1)/(l2x2-l2x1);
    }

    double gl1x,gl1y,gl2x,gl2y;
    if(l1y1==l1y2)
    {
        gl1y = l1y1;
        if(l2y1!=l2y2)
        {
            cy = gl1y;
            cx = (cy - b2)/k2;
        }
        else
        {
            //PX
        }
    }
    else if(l2y1==l2y2)
    {
        gl2y = l2y1;
        if(l1y1!=l1y2)
        {
            cy = gl2y;
            cx = (cy - b1)/k1;
        }
        else
        {
            //PX
        }
    }
    else if(l1x2==l1x1)
    {
        gl1x = l1x1;
        if(l2x1!=l2x2)
        {
            cx = gl1x;
            cy = k2*cx+b2;
        }
        else
        {
            //PX
        }
    }
    else if(l2x2==l2x1)
    {
        gl2x = l2x1;
        if(l1x1!=l1x2)
        {
            cx = gl2x;
            cy = k1*cx+b1;
        }
        else
        {
            //PX
        }
    }
    else
    {
        if(k1!=k2)
        {
            //cx
            cx = (b2-b1)/(k1-k2);
            cy = (b1*k2-b2*k1)/(k2-k1);
        }
        else
        {
            //PX
        }
    } 
}

ShapeBase::ShapeBase():
    m_ishow(1),
    m_shapetype(ShapeType::Path),
    m_antialiased(false),
    m_transformed(false),
    m_color(Quantity_NOC_GREEN1),
    m_dminpercent(0.01),
    m_icount(100),
    m_ishowlines(1)
{
}
ShapeBase::~ShapeBase() = default;

CxShapeHit ShapeBase::hitTest(double x, double y, double tolerance) const
{
    (void)x;
    (void)y;
    (void)tolerance;
    return {};
}

void ShapeBase::enumerateHandles(std::vector<CxShapeHandle>& out) const
{
    (void)out;
}

void ShapeBase::dragHandle(CxShapeHandleRole role, int vertex_index, double x, double y)
{
    (void)role;
    (void)vertex_index;
    (void)x;
    (void)y;
}

void ShapeBase::translateBy(double dx, double dy)
{
    (void)dx;
    (void)dy;
}

bool ShapeBase::snapshot(CxShapeGeometrySnapshot& out) const
{
    out.kind = kind();
    out.points.clear();
    exportPoints(out.points);
    if (out.points.empty())
    {
        bool closed = false;
        exportPolyline(out.points, closed);
        out.closed = closed;
    }
    CxShapePoint center;
    double radius = 0.0, inner_radius = 0.0;
    if (exportCircle(center, radius, inner_radius))
    {
        out.center = center;
        out.radius = radius;
        out.inner_radius = inner_radius;
    }
    return true;
}

void ShapeBase::exportPolyline(std::vector<CxShapePoint>& out, bool& closed) const
{
    (void)out;
    closed = false;
}

bool ShapeBase::exportCircle(CxShapePoint& center, double& radius, double& inner_radius) const
{
    (void)center;
    (void)radius;
    (void)inner_radius;
    return false;
}

bool ShapeBase::exportLine(CxShapePoint& p0, CxShapePoint& p1) const
{
    (void)p0;
    (void)p1;
    return false;
}

bool ShapeBase::exportEllipse(CxShapePoint& center, double& radius_x, double& radius_y, double& angle) const
{
    (void)center;
    (void)radius_x;
    (void)radius_y;
    (void)angle;
    return false;
}

void ShapeBase::exportPoints(std::vector<CxShapePoint>& out) const
{
    (void)out;
}

int ShapeBase::show()
{
    return m_ishow;
}

void ShapeBase::setshow(int ishow)
{
    m_ishow = ishow; 
}

//! [3]
void ShapeBase::setShape(int ashape)
{
    switch (ashape)
    {
        case 0: m_shapetype = ShapeBase::Line;break;
        case 1: m_shapetype = ShapeBase::Points;break;
        case 2: m_shapetype = ShapeBase::Polyline;break;
        case 3: m_shapetype = ShapeBase::Polygon;break;
        case 4: m_shapetype = ShapeBase::Rect;break;
        case 5: m_shapetype = ShapeBase::RoundedRect;break;
        case 6: m_shapetype = ShapeBase::Ellipse;break;
        case 7: m_shapetype = ShapeBase::Arc;break;
        case 8: m_shapetype = ShapeBase::Chord;break;
        case 9: m_shapetype = ShapeBase::Pie;break;
        case 10: m_shapetype = ShapeBase::Path;break;
        case 11: m_shapetype = ShapeBase::Text;break;
        case 12: m_shapetype = ShapeBase::Pixmap;break;
        default: m_shapetype = ShapeBase::Path;break;
    }
}

void ShapeBase::setpenw(int iw)
{
}
//! [3]
void ShapeBase::setPercent(double dvalue)
{//0.002->500sample 
    if (dvalue < 0.001 || dvalue>0.1)
        return;
   m_dminpercent = dvalue;
   m_icount=1/m_dminpercent ;
}

//! [4]
void ShapeBase::setPen(int ir, int ig ,int ib)
{

}

//! [4]

//! [5]
void ShapeBase::setBrush(int ir,int ig, int ib)
{
}
//! [5]

//! [6]
void ShapeBase::setAntialiased(int bantialiased)
{
    m_antialiased = bantialiased>0?true:false;
}
//! [6]

//! [7]
void ShapeBase::setTransformed(int btransformed)
{
    m_transformed = btransformed>0?true:false;
}
//

QRootGrid::QRootGrid()
    : m_ilevel(0),
      m_itype(0),
      m_iclasstype(0),
      m_name(),
      m_point(0, 0, 0)
{
    m_ishow = 1;
}

QRootGrid::~QRootGrid() = default;

void QRootGrid::release()
{
    m_glist.clear();
    m_plist.clear();
    m_ilevel = 0;
    m_itype = 0;
    m_iclasstype = 0;
    m_name.clear();
    m_point = gp_Pnt(0, 0, 0);
}

void QRootGrid::addpoint(gp_Pnt& apoint)
{
    if (m_plist.empty())
    {
        m_point = apoint;
    }
    m_plist.push_back(apoint);
}

int QRootGrid::getlevel()
{
    return m_ilevel;
}

void QRootGrid::addrootpointlist(std::list<gp_Pnt>& keypoints, int itype)
{
    m_itype = itype;
    for (gp_Pnt& point : keypoints)
    {
        addpoint(point);
    }
}

void QRootGrid::addpoint(gp_Pnt& arootpoint, gp_Pnt& apoint)
{
    QRootGrid child;
    child.setlevel(m_ilevel + 1);
    child.settype(m_itype);
    child.m_point = arootpoint;
    child.addpoint(apoint);
    m_glist.push_back(child);
}

void QRootGrid::drawshape(gp_Path& painter)
{
    drawgrid(painter);
}

void QRootGrid::drawgrid(gp_Path& painter)
{
    for (const gp_Pnt& point : m_plist)
    {
        painter.AddPoint(point);
    }

    for (QRootGrid& child : m_glist)
    {
        child.drawgrid(painter);
    }
}

void QRootGrid::drawlayer(gp_Path& painter, int ilevel)
{
    if (m_ilevel == ilevel)
    {
        for (const gp_Pnt& point : m_plist)
        {
            painter.AddPoint(point);
        }
    }

    for (QRootGrid& child : m_glist)
    {
        child.drawlayer(painter, ilevel);
    }
}

void QRootGrid::gridclasstype()
{
    m_iclasstype = static_cast<int>(m_plist.size());
}

//
/*
void ShapeBase::drawshape(QPainter &painter)
{
    static const gp_Pnt points[4] = {
        gp_Pnt(10, 80),
        gp_Pnt(20, 10),
        gp_Pnt(80, 30),
        gp_Pnt(90, 70)
    };

    QRect rect(10, 20, 80, 60);

    QPainterPath path;
    path.moveTo(20, 80);
    path.lineTo(20, 30);
    path.cubicTo(80, 0, 50, 50, 80, 80);

    int startAngle = 20 * 16;
    int arcLength = 120 * 16;
//! [8]

//! [9]
    painter.setPen(m_pen);
    painter.setBrush(m_brush);
    if (m_antialiased)
        painter.setRenderHint(QPainter::Antialiasing, true);
//! [9]

//! [10]
    for (int x = 0; x <400; x += 100)
    {
        for (int y = 0; y < 400; y += 100)
        {
            painter.save();
            painter.translate(x, y);

            if (m_transformed)
            {
                painter.translate(50, 50);
                painter.rotate(60.0);
                painter.scale(0.6, 0.9);
                painter.translate(-50, -50);
            }


//! [12]
            switch (m_shapetype)
            {
                case Line:
                    painter.drawLine(rect.bottomLeft(), rect.topRight());
                    break;
                case Points:
                    painter.drawPoints(points, 4);
                    break;
                case Polyline:
                    painter.drawPolyline(points, 4);
                    break;
                case Polygon:
                    painter.drawPolygon(points, 4);
                    break;
                case Rect:
                    painter.drawRect(rect);
                    break;
                case RoundedRect:
                    painter.drawRoundedRect(rect, 25, 25, Qt::RelativeSize);
                    break;
                case Ellipse:
                    painter.drawEllipse(rect);
                    break;
                case Arc:
                    painter.drawArc(rect, startAngle, arcLength);
                    break;
                case Chord:
                    painter.drawChord(rect, startAngle, arcLength);
                    break;
                case Pie:
                    painter.drawPie(rect, startAngle, arcLength);
                    break;
                case Path:
                    if(1==m_ishow)
                        painter.drawPath(path);
                    else if(2==m_ishow)
                    {
                        //int icount = 1/m_dminpercent ;
                        for(int i=0;i<m_icount;i++)
                        {
                           gp_PntF fpoint = path.pointAtPercent(m_dminpercent*i);
                            painter.drawPoint(fpoint);
                        }
                    }
                    else if(3==m_ishow)
                    {

                        //int icount = 1/m_dminpercent ;
                        for(int i=0;i<m_icount;i++)
                        {
                           gp_PntF fpoint = path.pointAtPercent(m_dminpercent*i);
                            painter.drawPoint(fpoint);
                        }

                        painter.setPen(QPen(QColor(255,0,0)));
                        int icount = path.ElementCount();
                        for(int i=0;i<icount;i++)
                        {
                             gp_Pnt aele = path.ElementAt(i);
                             painter.drawPoint(aele.x,aele.y);
                        }
                        painter.setPen(m_pen);
                    }
                    break;
                case Text:
                    painter.drawText(rect,
                                     Qt::AlignCenter,
                                     (" "));
                    break;
                case Pixmap:
                    painter.drawPixmap(10, 10, m_pixmap);
            }
//! [12] //! [13]
            painter.restore();
        }
    }

    painter.setRenderHint(QPainter::Antialiasing, false);
    //painter.setPen(palette().dark().color());
    painter.setBrush(Qt::NoBrush);
    //painter.drawRect(QRect(0, 0, width() - 1, height() - 1));
}
*/
LineShape::LineShape()
{
   m_line.setLine(gp_Pnt(100,100,0), gp_Pnt(200,200,0));
   m_path.AddPoint(m_line.StartPoint());
   m_path.AddPoint(m_line.EndPoint());
}
double LineShape::getlinedistance()
{ 
    int x = m_line.StartPoint().X();
    int y = m_line.StartPoint().Y();
    int x0 = m_line.EndPoint().X();
    int y0 = m_line.EndPoint().Y();

    return sqrt((x - x0) * (x - x0) + (y - y0) * (y - y0));
}

void LineShape::setline(int ix0,int iy0,int ix1,int iy1)
{
    m_line.setLine(ix0,iy0,ix1,iy1);

    gp_Path path;
    path.AddPoint(m_line.StartPoint());
    path.AddPoint(m_line.EndPoint());
    m_path = path;
   
}


 void LineShape::Move(int ix, int iy)
{
    // 平移路径
    gp_Vec translationVector(ix, iy, 0); // 沿X轴平移5，沿Y轴平移3，沿Z轴平移2
    m_path.Translate(translationVector);

}
void LineShape::Rotate(double dangle)
{
    gp_Pnt apoint(0, 0, 0);
    m_path.RotateAroundPoint(apoint, dangle);
}
void LineShape::Zoom(double dx0, double dy0)
{
    gp_Pnt apoint(0, 0, 0);
    m_path.ScaleAroundPoint(apoint, dx0, dy0);
}
LineShape::~LineShape()
{
}
void LineShape::clear()
{
    m_path.Clear();
}
void LineShape::copy(LineShape &aline)
{
    m_line = aline.m_line;
    m_path.CopyPath(aline.m_path);
    m_icount = aline.m_icount;

}
void LineShape::setcolor(int ir, int ig, int ib)
{
    m_color = Quantity_Color(ir / 255.0, ig / 255.0, ib / 255.0, Quantity_TypeOfColor::Quantity_TOC_RGB);

    m_path.setcolor(ir, ig, ib);
}
void LineShape::setshow(int bshow)
{
    ShapeBase::setshow(bshow);
    
    m_path.PathShow(bshow);
}
void LineShape::setpenw(int iw)
   
{
   
}

bool LineShape::exportLine(CxShapePoint& p0, CxShapePoint& p1) const
{
    gp_Pnt start = m_line.StartPoint();
    gp_Pnt end = m_line.EndPoint();
    p0.x = start.X();
    p0.y = start.Y();
    p1.x = end.X();
    p1.y = end.Y();
    return true;
}

void LineShape::exportPoints(std::vector<CxShapePoint>& out) const
{
    out.clear();
    gp_Pnt start = m_line.StartPoint();
    gp_Pnt end = m_line.EndPoint();
    out.push_back({ start.X(), start.Y() });
    out.push_back({ end.X(), end.Y() });
}

CxShapeHit LineShape::hitTest(double x, double y, double tolerance) const
{
    gp_Pnt start = m_line.StartPoint();
    gp_Pnt end = m_line.EndPoint();
    const double x0 = start.X();
    const double y0 = start.Y();
    const double x1 = end.X();
    const double y1 = end.Y();

    const double len = std::sqrt((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
    if (len < 1.0)
        return {};

    const double t_sq = tolerance * tolerance;
    double dist;

    dist = (x - x0) * (x - x0) + (y - y0) * (y - y0);
    if (dist <= t_sq)
        return { true, CxShapeHandleRole::Start, -1, std::sqrt(dist) };

    dist = (x - x1) * (x - x1) + (y - y1) * (y - y1);
    if (dist <= t_sq)
        return { true, CxShapeHandleRole::End, -1, std::sqrt(dist) };

    const double cx = (x0 + x1) * 0.5;
    const double cy = (y0 + y1) * 0.5;
    dist = (x - cx) * (x - cx) + (y - cy) * (y - cy);
    if (dist <= t_sq)
        return { true, CxShapeHandleRole::Center, -1, std::sqrt(dist) };

    const double dx = x1 - x0;
    const double dy = y1 - y0;
    const double tdx = dx / len;
    const double tdy = dy / len;
    const double px = x - x0;
    const double py = y - y0;
    const double proj = px * tdx + py * tdy;
    const double clamped_proj = std::max(0.0, std::min(len, proj));
    const double near_x = x0 + tdx * clamped_proj;
    const double near_y = y0 + tdy * clamped_proj;
    const double body_dist = std::sqrt((x - near_x) * (x - near_x) + (y - near_y) * (y - near_y));

    if (body_dist <= tolerance)
        return { true, CxShapeHandleRole::Body, -1, body_dist };

    return {};
}

void LineShape::enumerateHandles(std::vector<CxShapeHandle>& out) const
{
    gp_Pnt start = m_line.StartPoint();
    gp_Pnt end = m_line.EndPoint();
    const double x0 = start.X();
    const double y0 = start.Y();
    const double x1 = end.X();
    const double y1 = end.Y();
    const double cx = (x0 + x1) * 0.5;
    const double cy = (y0 + y1) * 0.5;

    out.push_back({ CxShapeHandleRole::Start, -1, { x0, y0 }, "P0" });
    out.push_back({ CxShapeHandleRole::End, -1, { x1, y1 }, "P1" });
    out.push_back({ CxShapeHandleRole::Center, -1, { cx, cy }, "C" });
}

void LineShape::dragHandle(CxShapeHandleRole role, int vertex_index, double x, double y)
{
    (void)vertex_index;
    gp_Pnt start = m_line.StartPoint();
    gp_Pnt end = m_line.EndPoint();
    const double x0 = start.X();
    const double y0 = start.Y();
    const double x1 = end.X();
    const double y1 = end.Y();

    switch (role)
    {
    case CxShapeHandleRole::Start:
        m_line.setLine(gp_Pnt(x, y, 0), end);
        break;
    case CxShapeHandleRole::End:
        m_line.setLine(start, gp_Pnt(x, y, 0));
        break;
    case CxShapeHandleRole::Center:
    case CxShapeHandleRole::Body:
    {
        const double cx_old = (x0 + x1) * 0.5;
        const double cy_old = (y0 + y1) * 0.5;
        const double dx = x - cx_old;
        const double dy = y - cy_old;
        m_line.setLine(gp_Pnt(x0 + dx, y0 + dy, 0), gp_Pnt(x1 + dx, y1 + dy, 0));
        break;
    }
    default:
        break;
    }

    gp_Path path;
    path.AddPoint(m_line.StartPoint());
    path.AddPoint(m_line.EndPoint());
    m_path = path;
}

void LineShape::translateBy(double dx, double dy)
{
    gp_Pnt start = m_line.StartPoint();
    gp_Pnt end = m_line.EndPoint();
    m_line.setLine(gp_Pnt(start.X() + dx, start.Y() + dy, 0),
                   gp_Pnt(end.X() + dx, end.Y() + dy, 0));

    gp_Path path;
    path.AddPoint(m_line.StartPoint());
    path.AddPoint(m_line.EndPoint());
    m_path = path;
}
 
void LineShape::drawshape(gp_Path&painter)
{

            if(0)
            if (m_transformed)
            {
                // 平移路径
                gp_Vec translationVector(5, 3, 2); // 沿X轴平移5，沿Y轴平移3，沿Z轴平移2
                painter.Translate(translationVector);
                // 旋转路径
                gp_Pnt rotationCenter(0, 0, 0);
                double rotationAngle = 90; // 旋转90度
                painter.RotateAroundPoint(rotationCenter, rotationAngle);
                // 缩放路径
                gp_Pnt scaleCenter(0, 0, 0);
                double scaleX = 2.0;
                double scaleY = 1.5;
                painter.ScaleAroundPoint(scaleCenter, scaleX, scaleY);
            }
            if(1==m_ishow)
                painter.AddPath(m_path);
            else if(2==m_ishow)
            {
                //int icount = 1/m_dminpercent ;
                for(int i=0;i<m_icount;i++)
                {
                    gp_Pnt fpoint = m_path.PointAtPercent(m_dminpercent*i);
                    painter.AddPoint(fpoint);
                }
            }
            else if(3==m_ishow)
            {
               // int icount = 1/m_dminpercent ;
                for(int i=0;i<m_icount;i++)
                {
                    gp_Pnt fpoint = m_path.PointAtPercent(m_dminpercent*i);
                    painter.AddPoint(fpoint);
                }
                //painter.setPen(QPen(QColor(255,0,0)));
                int icount = m_path.ElementCount();
                for(int i=0;i<icount;i++)
                {
                    gp_Pnt aele = m_path.ElementAt(i);
                     painter.AddPoint(aele);
                }
               // painter.setPen(m_pen);
            }
            else if(5==m_ishow)
            {
                gp_Pnt aele = m_path.ElementAt(0);
                gp_Rectangle arect(aele,3,3);
                painter.AddArc(arect.TopLeft(), arect.Width(),0,360);
                //painter.setPen(m_pen);
            }


 }
void LineShape::drawshapex(gp_Path&painter,double dmovx,double dmovy,
                           double dangle,double dzoomx,double dzoomy)
{
//! [9]
 //   painter.setPen(m_pen);
 //   painter.setBrush(m_brush);
//    if (m_antialiased)
//        painter.setRenderHint(QPainter::Antialiasing, true);

            if(1==m_ishow)
             {
                 //int icount = 1/m_dminpercent ;
                for(int i=0;i<m_icount;i++)
                {
                    gp_Pnt fpoint = m_path.PointAtPercent(m_dminpercent*i); 
                    transformPoint(fpoint, dzoomx, dzoomy, dmovx, dmovy); 
                     painter.AddPoint(fpoint);
                }
            }
            else if(2==m_ishow)
            {
               // int icount = 1/m_dminpercent ;
                for(int i=0;i<m_icount;i++)
                {
                    gp_Pnt fpoint = m_path.PointAtPercent(m_dminpercent*i);
                    painter.AddPoint(fpoint);
                }
            }
            else if(3==m_ishow)
            {

                //int icount = 1/m_dminpercent ;
                for(int i=0;i<m_icount;i++)
                {
                    gp_Pnt fpoint = m_path.PointAtPercent(m_dminpercent*i);
                    painter.AddPoint(fpoint);
                }

               // painter.setPen(QPen(QColor(255,0,0)));
                int icount = m_path.ElementCount();
                for(int i=0;i<icount;i++)
                {
                    gp_Pnt aele = m_path.ElementAt(i);
                     painter.AddPoint(aele);
                }
               // painter.setPen(m_pen);
            }
            else if(5==m_ishow)
            {
                gp_Pnt aele = m_path.ElementAt(0); 
                painter.AddCircle(aele,3);
               // painter.setPen(m_pen);
            }


 }
 
void LineShape::linecopy(Image&srcImage, Image&desImage)
{
   // int icount = 1/m_dminpercent ;
    for(int i=0;i<m_icount;i++)
    {
        gp_Pnt fpoint = m_path.PointAtPercent(m_dminpercent*i);
       desImage.setPixel(fpoint.X(),fpoint.Y() ,srcImage.pixel(fpoint.X(), fpoint.Y()));
    }
}
void LineShape::linecopyex(Image& srcImage, Image& desImage, int ix, int iy)
{
    const int sampleCount = m_icount;
    if (sampleCount <= 0 || srcImage.getWidth() <= 0 ||
        srcImage.getHeight() <= 0 || desImage.getWidth() <= 0 ||
        desImage.getHeight() <= 0)
        return;

    const cv::Vec3b emptyPixel(0, 0, 0);
    auto copySample = [&](int sample, int destinationX, int destinationY)
    {
        if (destinationX < 0 || destinationY < 0 ||
            destinationX >= desImage.getWidth() ||
            destinationY >= desImage.getHeight())
            return;

        const gp_Pnt sourcePoint =
            m_path.PointAtPercent(m_dminpercent * sample);
        const int sourceX = static_cast<int>(std::lround(sourcePoint.X()));
        const int sourceY = static_cast<int>(std::lround(sourcePoint.Y()));
        if (sourceX < 0 || sourceY < 0 ||
            sourceX >= srcImage.getWidth() ||
            sourceY >= srcImage.getHeight())
        {
            desImage.setPixel(destinationX, destinationY, emptyPixel);
            return;
        }
        desImage.setPixel(destinationX, destinationY,
                          srcImage.pixel(sourceX, sourceY));
    };

    if (ix == 0)
    {
        for (int sample = 0; sample < sampleCount; ++sample)
            copySample(sample, sample, iy);
    }
    else if (iy == 0)
    {
        for (int sample = 0; sample < sampleCount; ++sample)
            copySample(sample, ix, sample);
    }
}
gp_Pnt LineShape::getlinepoint(int inum)
{
    if(inum)
        return m_path.PointAtPercent(m_dminpercent*inum);
    return gp_Pnt(0, 0, 0);
}
int LineShape::getlinesize()
{
    int icount = m_icount ;//1/m_dminpercent ;
    return icount;
}

void LineShape::lineaex(int irate)
{
    /*
    gp_Pnt apointb = m_path.ElementAt(1);
    int icurmodule = BackImageManager::GetCurMode();
    Image*pimage = BackImageManager::GetTransferImage(icurmodule);
    gp_Path path = m_path;
    for(int ir = 0;ir<irate;ir++)
    {
        gp_Pnt apointa = path.ElementAt(0); 
        path = pimage->ImageTransfer(path,-apointa.X(),-apointa.Y(),0);
         path = pimage->ImageTransfer(path,0,0,180);
         path = pimage->ImageTransfer(path,apointa.X(),apointa.Y(),0);
         QPainterPath xpath;
         xpath.moveTo(path.ElementAt(1));
         xpath.lineTo(path.ElementAt(0));
         path = xpath;
    }
    gp_PntF apointa = path.ElementAt(0);
    QPainterPath bpath;
    bpath.moveTo(apointa.X(),apointa.Y());
    bpath.lineTo(apointb.X(),apointb.Y());
    m_path = bpath;
    */
}
void LineShape::linebex(int irate)
{
    /*
    gp_PntF apointb = m_path.ElementAt(0);
    int icurmodule = BackImageManager::GetCurMode();
    ImageBase *pimage = BackImageManager::GetTransferImage(icurmodule);
    QPainterPath path = m_path;
    for(int ir = 0;ir<irate;ir++)
    {
        gp_PntF apointa = path.ElementAt(1);
        path = pimage->ImageTransfer(path,-apointa.X(),-apointa.Y(),0);
         path = pimage->ImageTransfer(path,0,0,180);
         path = pimage->ImageTransfer(path,apointa.X(),apointa.Y(),0);
         QPainterPath xpath;
         xpath.moveTo(path.ElementAt(1));
         xpath.lineTo(path.ElementAt(0));
         path = xpath;
    }
    gp_PntF apointa = path.ElementAt(1);
    QPainterPath bpath;
    bpath.moveTo(apointa.X(),apointa.Y());
    bpath.lineTo(apointb.X(),apointb.Y());
    m_path = bpath;
    */
}
void LineShape::linecv()
{
    /*
    int icurmodule = BackImageManager::GetCurMode();
    ImageBase *pimage = BackImageManager::GetTransferImage(icurmodule);

    gp_PntF apointa = m_path.ElementAt(0);
    gp_PntF apointb = m_path.ElementAt(1);

    gp_PntF apointc =gp_PntF(apointa.X()*0.5+apointb.X()*0.5
                              ,apointa.Y()*0.5+apointb.Y()*0.5);

    QPainterPath path = m_path;
    path = pimage->ImageTransfer(path,-apointa.X(),-apointa.Y(),0);
    path = pimage->ImageTransfer(path,0,0,90);
    path = pimage->ImageTransfer(path,apointc.X(),apointc.Y(),0);

    m_path = path;
    */
}


PointsShape::PointsShape()
{ 
}

void PointsShape::MoveAB(int ix, int iy)
{
    // 平移路径
    gp_Vec translationVector(ix, iy, 0); // 沿X轴平移，沿Y轴平移，沿Z轴平移
    m_pathA.Translate(translationVector);
    m_pathB.Translate(translationVector);
}
void PointsShape::RotateAB(double dangle)
{
    gp_Pnt apoint(0, 0, 0);
    m_pathA.RotateAroundPoint(apoint, dangle);
    m_pathB.RotateAroundPoint(apoint, dangle);
}
void PointsShape::ZoomAB(double dx0, double dy0)
{
    gp_Pnt apoint(0, 0, 0);
    m_pathA.ScaleAroundPoint(apoint, dx0, dy0);
    m_pathB.ScaleAroundPoint(apoint, dx0, dy0);
}
void PointsShape::setcolor(int ir, int ig, int ib)
{
    m_color = Quantity_Color(ir / 255.0, ig / 255.0, ib / 255.0, Quantity_TypeOfColor::Quantity_TOC_RGB);

    m_path.setcolor(ir, ig, ib);
}
void PointsShape::setcolorA(int ir, int ig, int ib)
{ 
    m_pathA.setcolor(ir, ig, ib);
}
void PointsShape::setcolorB(int ir, int ig, int ib)
{ 
    m_pathB.setcolor(ir, ig, ib);
}
void PointsShape::Move(int ix,int iy)
{
    // 平移路径
    gp_Vec translationVector(ix, iy, 0); // 沿X轴平移5，沿Y轴平移3，沿Z轴平移2
    m_path.Translate(translationVector);
 
}
void PointsShape::Rotate(double dangle)
{ 
    gp_Pnt apoint(0,0,0); 
    m_path.RotateAroundPoint(apoint, dangle);  
}
void PointsShape::Zoom(double dx0,double dy0)
{
    gp_Pnt apoint(0, 0, 0);
    m_path.ScaleAroundPoint(apoint, dx0, dy0);
}
void PointsShape::setshow(int ishow)
{
    if (2 == ishow)
    {
        setcolorA(180, 0, 0);
        setcolorB(100, 180, 0);
        setcolor(0, 0, 180);
        MakePointShapeAB();
    }
    else if (1 == ishow)
    { 
        MakePointShape();
    }
    else if (16 == ishow)
    {
        int ishownum = 0;
        for (int i = 0; i < m_paths.size(); i++)
        { 
            if (m_paths[i].ElementCount() > 0)
            {
                if (0 == ishownum)
                {
                    m_paths[i].setcolor(0, 0, 200);
                    ishownum++;
                }  
                else if (1 == ishownum)
                {
                    m_paths[i].setcolor(0, 100, 100);
                    ishownum++;
                }  
                else if (2 == ishownum)
                {
                    m_paths[i].setcolor(0, 200, 0);
                    ishownum++;
                }  
                else if (3 == ishownum)
                {
                    m_paths[i].setcolor(0, 200, 200);
                    ishownum = 0;
                }   
            }
            m_paths[i].MakePointShape();
        }
    }
    else if (8 == ishow)
    {
        MakeEdgeShape();
    }
    else if (32 == ishow)
    {
        MakeShape();
    }
    ShapeBase::setshow(ishow);

}
void PointsShape::pointsABadd(void *pointsB)
{
    PointsShape *apointsb =(PointsShape *) pointsB;
    PointsShape anewpoints;
    int isum0= size();
    int isum1 = apointsb->size();
     double dx0,dy0,dx1,dy1;
     int isum2 = isum0 > isum1 ? isum0:isum1;
    for (int i = 0 ;i<isum2;i++)
    {
        if(i<isum0)
        {
            dx0= getx(i);
            dy0= gety(i);
            anewpoints.addpoint(dx0,dy0);
        }
        if(i<isum1)
        {
            dx1= apointsb->getx(i);
            dy1= apointsb->gety(i);
            anewpoints.addpoint(dx1,dy1);
        }
    }
    clear();
    int isum= anewpoints.size();
    for (int i = 0 ;i<isum;i++)
    {
         dx0= anewpoints.getx(i);
         dy0= anewpoints.gety(i);
         addpoint(dx0,dy0);
    }
}
void PointsShape::pointslineadd(void *pointsB)
{
    PointsShape *apointsb =(PointsShape *) pointsB;
    PointsShape anewpoints;
    int isum0= size();
    int isum1 = apointsb->size();
     double dx0,dy0,dx1,dy1;
    int j= 0;int i = 0;int ien1 = 0;int ien2 = 0;
    while (ien1>=0||ien2>=0)
    {
        if(i<isum0)
        {
            dx0= getx(i);
            dy0= gety(i);
            anewpoints.addpoint(dx0,dy0);
            i++;
            dx0= getx(i);
            dy0= gety(i);
            anewpoints.addpoint(dx0,dy0);
            i++;
        }
        else
            ien1 = -1;

        if(j<isum1)
        {
            dx1= apointsb->getx(j);
            dy1= apointsb->gety(j);
            anewpoints.addpoint(dx1,dy1);
            j++;
            dx1= apointsb->getx(j);
            dy1= apointsb->gety(j);
            anewpoints.addpoint(dx1,dy1);
            j++;
        }
        else
            ien2 = -1;
    }
    clear();
    int isum= anewpoints.size();
    for (int i = 0 ;i<isum;i++)
    {
         dx0= anewpoints.getx(i);
         dy0= anewpoints.gety(i);
         addpoint(dx0,dy0);
    }
}
void PointsShape::getfindlinemodel(void* findline)
{ 
    Findline* pfindline = (Findline*)findline;
    if (pfindline == nullptr)
        return;
    pfindline->getpattern();
             
}
void PointsShape::addpoints(PointsShape &points)
{
    m_path.AddPath(points.getpath());
}
void PointsShape::addpoint(int apointx, int apointy)
{
    gp_Pnt apoint(apointx, apointy, 0);
    m_path.AddPoint(apoint);
}
void PointsShape::addpoint(Standard_Real& apointx, Standard_Real& apointy)
{
    gp_Pnt apoint(apointx, apointy, 0);
    m_path.AddPoint(apoint);
}
void PointsShape::addpointa(Standard_Real& apointx, Standard_Real& apointy)
{
    gp_Pnt apoint(apointx, apointy, 0);
    m_pathA.AddPoint(apoint);
}
void PointsShape::addpointb(Standard_Real& apointx, Standard_Real& apointy)
{
    gp_Pnt apoint(apointx, apointy, 0);
    m_pathB.AddPoint(apoint);
}
gp_Rectangle PointsShape::boundingRect()
{
    return m_path.boundingRect();
}
gp_Rectangle PointsShape::boundingRectAB()
{
    gp_Rectangle arect = m_pathA.boundingRect();
    gp_Rectangle brect = m_pathB.boundingRect();

    Standard_Real iaTLx = arect.TopLeft().X();
    Standard_Real iaTLy = arect.TopLeft().Y();

    Standard_Real ibTLx = brect.TopLeft().X();
    Standard_Real ibTLy = brect.TopLeft().Y();

    Standard_Real iaBRx = arect.BottomRight().X();
    Standard_Real iaBRy = arect.BottomRight().Y();

    Standard_Real ibBRx = brect.BottomRight().X();
    Standard_Real ibBRy = brect.BottomRight().Y();

    Standard_Real iminx = iaTLx <= ibTLx ? iaTLx : ibTLx;
    Standard_Real iminy = iaTLy <= ibTLy ? iaTLy : ibTLy;
    
    Standard_Real imaxx = iaBRx >= ibBRx ? iaBRx : ibBRx;
    Standard_Real imaxy = iaBRy >= ibBRy ? iaBRy : ibBRy;
    
    gp_Rectangle crect(gp_Pnt(iminx, iminy,0), gp_Pnt(imaxx, imaxy, 0));
      
    return crect;
}
gp_Rectangle PointsShape::boundingRectA()
{
    gp_Rectangle arect = m_pathA.boundingRect(); 
       
    return arect;
}
gp_Rectangle PointsShape::boundingRectB()
{ 
    gp_Rectangle brect = m_pathB.boundingRect();
        
    return brect;
}

gp_Rectangle PointsShape::controlPointRect()
{
    return m_path.boundingRect();
}
void PointsShape::addpoint(gp_Pnt&apoint)
{ 
   m_path.AddPoint(apoint); 
} 
void PointsShape::AdaptiveDistfilter(int k)
{
#if defined USE_AI
    gp_Path& path  =  getpath(); 
    if (1)
    {
        size_t numPoints = path.getpoints().size();
        if (numPoints <= 3)
            return;
        arma::mat points(2, numPoints);
        for (size_t i = 0; i < numPoints; ++i)
        {
            points(0, i) = path.getpoints()[i].X(); // 第一行是 x 坐标
            points(1, i) = path.getpoints()[i].Y(); // 第二行是 y 坐标
        }
        path.Clear();
        // 自适应过滤
        auto filteredIndices = mlpackclass::AdaptiveFilterWithCoincidenceHandling_(points,k);
        for (auto idx : filteredIndices)
            path.AddPoint(gp_Pnt(points(0, idx), points(1, idx), 0));
    }
#endif
}

void PointsShape::FftWaveletTransform(double distanceThreshold , double waveletThreshold )
{
#if defined USE_AI___
    gp_Path& path = getpath();

        size_t numPoints = path.getpoints().size();
        arma::mat points(2, numPoints);
        for (size_t i = 0; i < numPoints; ++i)
        {
            points(0, i) = path.getpoints()[i].X(); // 第一行是 x 坐标
            points(1, i) = path.getpoints()[i].Y(); // 第二行是 y 坐标
        }
    
    // 设置距离阈值和小波分析的能量阈值
    //double distanceThreshold = 1.0;
    //double waveletThreshold = 0.5; // 小波能量阈值

    // 对点云进行分类
    auto clusters = mlpackclass::ClusterPointCloud_(points, distanceThreshold);

 
    // 输出每个聚类的索引和基于小波分析的路径分段结果
    mlpackclass::OutputClustersWithWaveletAnalysis(points, clusters, waveletThreshold);
     
#endif
}
void PointsShape::MakeEdgeShape()
{
    m_path.MakeEdgeShape();
}
void PointsShape::MakeShape()
{
    m_path.MakeShape();
}
void PointsShape::MakePointShape()
{
    m_path.MakePointShape();
}
void PointsShape::MakePointShapeAB()
{
    m_pathA.MakePointShape();
    m_pathB.MakePointShape();

}
/*
void PointsShape::addText(Standard_Real x, Standard_Real y,QFont &f,QString &text)
{
    m_path.addText(x, y, f, text);
}*/
/*
void PointsShape::Distfilter()
{
    if (1)
    { 
        size_t numPoints = m_pathA.getpoints().size();
        arma::mat points(2, numPoints); 
        for (size_t i = 0; i < numPoints; ++i)
        {
            points(0, i) = m_pathA.getpoints()[i].X(); // 第一行是 x 坐标
            points(1, i) = m_pathA.getpoints()[i].Y(); // 第二行是 y 坐标
        }
        m_pathA.Clear();
        // 自适应过滤
        auto filteredIndices = AdaptiveFilterWithCoincidenceHandling<NearestNeighborSort, EuclideanDistance>(points);
        for (auto idx : filteredIndices)
            m_pathA.AddPoint(gp_Pnt(points(0, idx), points(1, idx), 0)); 
    }
    if (1)
    { 
        size_t numPoints = m_pathB.getpoints().size();
        arma::mat points(2, numPoints); 
        for (size_t i = 0; i < numPoints; ++i)
        {
            points(0, i) = m_pathB.getpoints()[i].X(); // 第一行是 x 坐标
            points(1, i) = m_pathB.getpoints()[i].Y(); // 第二行是 y 坐标
        }
        m_pathB.Clear();
        // 自适应过滤
        auto filteredIndices = AdaptiveFilterWithCoincidenceHandling<NearestNeighborSort, EuclideanDistance>(points);
        for (auto idx : filteredIndices)
            m_pathB.AddPoint(gp_Pnt(points(0, idx), points(1, idx), 0));

    }
   

}
*/

gp_Pnt PointsShape::getpointscent() const
{ 
    return m_path.centroid();
}
void PointsShape::clear()
{
    m_path.Clear();
    m_pathA.Clear();
    m_pathB.Clear();
    for (int i = 0; i < m_paths.size(); i++)
        m_paths[i].Clear();
    m_paths.clear();
}
void PointsShape::copy(PointsShape &points)
{
    clear();
    m_path.CopyPath(points.getpath());
    m_pathA.CopyPath(points.getpathA());
    m_pathB.CopyPath(points.getpathB());
    
    //for (int i = 0; i < m_paths.size(); i++)
    //    m_paths[i].CopyPath(points.);
    // m_paths.clear();
}
int PointsShape::size() const
{
    return m_path.ElementCount();
}

int PointsShape::script_size()
{
    return size();
}

int PointsShape::ABsize() const
{
    return m_pathA.ElementCount();
}
void PointsShape::drawshape(gp_Path&painter)
{
//! [9]
 //   painter.setPen(m_pen);
 //   painter.setBrush(m_brush);
//    if (m_antialiased)
//        painter.setRenderHint(QPainter::Antialiasing, true);
//! [9]
//! [10]
//            if (m_transformed)
//            {
//                painter.translate(50, 50);
//                painter.rotate(60.0);
//                painter.scale(0.6, 0.9);
//                painter.translate(-50, -50);
//            }
            if(1==m_ishow)
            {// draw dot
                int icount = m_path.ElementCount();
                for(int i=0;i<icount;i++)
                {
                    gp_Pnt aele = m_path.ElementAt(i);
                     painter.AddPoint(aele);
                }
            }
            else if(2==m_ishow)
            {// draw x
                int icount = m_path.ElementCount();
                for(int i=0;i<icount;i++)
                {
                    gp_Pnt aele = m_path.ElementAt(i);
                    painter.AddCross(aele, 3); 
                }
            }
            else if(16==m_ishow)
            {//四边形
               // painter.setPen(QPen(QColor(0,255,0)));
                 
                int icount = m_path.ElementCount();
                for(int i=0;i<icount-4;i=i+4)
                {
                 }
            }
            else if(3==m_ishow)
                painter.AddPath(m_path);
            else if(4==m_ishow)
            {
                //int icount = 1/m_dminpercent ;
                for(int i=0;i<m_icount;i++)
                {
                    gp_Pnt fpoint = m_path.PointAtPercent(m_dminpercent*i);
                    painter.AddPoint(fpoint);
                }

               // painter.setPen(QPen(QColor(255,0,0)));
                int icount = m_path.ElementCount();
                for(int i=0;i<icount;i++)
                {
                    gp_Pnt aele = m_path.ElementAt(i);
                     painter.AddCross(aele,3); 
                }
               // painter.setPen(m_pen);
            }           
            else if(8==m_ishow)
            {  
                int icount = m_path.ElementCount();

                for(int i=0;i<icount-1;i++)
                {
                    gp_Pnt aele = m_path.ElementAt(i); 
                    //painter.setPen(QPen(QColor(255,0,0)));
                    painter.AddPoint(aele);
                    i++;
                    aele = m_path.ElementAt(i);
                    //painter.setPen(QPen(QColor(0,0,255)));
                    painter.AddPoint(aele);
                } 
            }
 }
void PointsShape::drawshapex(gp_Path&painter,
                             double dmovx,
                             double dmovy,
                             double dangle,
                             double dzoomx,
                             double dzoomy)
{
//! [9]
 //   painter.setPen(m_pen);
 //   painter.setBrush(m_brush);

            if(1==m_ishow)
            {// draw dot
                int icount = m_path.ElementCount();
                for(int i=0;i<icount;i++)
                {
                    gp_Pnt aele = m_path.ElementAt(i); 
                    transformPoint(aele, dzoomx, dzoomy, dmovx, dmovy); 
                     painter.AddPoint(aele);
                }
            }
            else if(2==m_ishow)
            {// draw x
                int icount = m_path.ElementCount();
                for(int i=0;i<icount;i++)
                {
                    gp_Pnt aele = m_path.ElementAt(i);
                    transformPoint(aele, dzoomx, dzoomy, dmovx, dmovy);
                    painter.AddCross(aele,3); 
                }
            }
            else if(5==m_ishow)
            {// draw x
                int icount = m_path.ElementCount();
                int iab=0;
                for(int i=0;i<icount;i++)
                {
                    gp_Pnt aele = m_path.ElementAt(i);
                    if(0==iab)
                    {
                        iab=1;
                       // painter.setPen(QPen(QColor(0,255,0)));
                    }
                    else {iab=0;
                       // painter.setPen(QPen(QColor(255,0,0)));

                    }
                    transformPoint(aele, dzoomx, dzoomy, dmovx, dmovy);
                    m_path.AddCross(aele, 3); 


                }
               // painter.setPen(m_pen);
            }
            else if(16==m_ishow)
            {//四边形
               // painter.setPen(QPen(QColor(0,255,0)));
                int icount = m_path.ElementCount();
                for (int i=0;i<icount;)
                {
                    double dx1=0,dy1=0,dx2=0,dy2=0,dx3=0,dy3=0,dx4=0,dy4=0;
                    if(i<icount)
                    {
                        gp_Pnt aele1 = m_path.ElementAt(i);
                        transformPoint(aele1, dzoomx, dzoomy, dmovx, dmovy);
                        painter.AddPoint(aele1);
                    }
                    i=i+1;
                    if(i<icount)
                    {
                        gp_Pnt aele2 = m_path.ElementAt(i);
                        transformPoint(aele2, dzoomx, dzoomy, dmovx, dmovy);
                        painter.AddPoint(aele2);
                    }
                    i=i+1;
                    if(i<icount)
                    {
                        gp_Pnt aele3 = m_path.ElementAt(i);
                        transformPoint(aele3, dzoomx, dzoomy, dmovx, dmovy);
                        painter.AddPoint(aele3);
                    }
                     i=i+1;
                     if(i<icount)
                     {
                        gp_Pnt aele4 = m_path.ElementAt(i); 
                        transformPoint(aele4, dzoomx, dzoomy, dmovx, dmovy);
                        painter.AddPoint(aele4);
                     }
                     i=i+1;
                      
                 }
                //painter.setPen(m_pen);

            }
            else if(17==m_ishow)
            {//对边
               // painter.setPen(QPen(QColor(0,255,0)));
                int icount = m_path.ElementCount();
                for (int i=0;i<icount;)
                {
                    gp_Pnt aele1, aele2;
                    if(i<icount)
                    {
                        gp_Pnt aele1 = m_path.ElementAt(i);
                        transformPoint(aele1, dzoomx, dzoomy, dmovx, dmovy); 
                    }
                    i=i+1;
                    if(i<icount)
                    {
                        gp_Pnt aele2 = m_path.ElementAt(i);
                        transformPoint(aele2, dzoomx, dzoomy, dmovx, dmovy);
                    }
                    i=i+1; 
                    painter.AddLine(aele1, aele2);
                 }
                //painter.setPen(m_pen); 
            }

            else if(3==m_ishow)
                painter.AddPath(m_path);
            else if(4==m_ishow)
            {
                int icount = m_path.ElementCount();
                if(icount>=2)
                {
                    gp_Pnt aele0 = m_path.ElementAt(0);
                    gp_Pnt aele1 = m_path.ElementAt(1);

                    transformPoint(aele0, dzoomx, dzoomy, dmovx, dmovy);
                    transformPoint(aele1, dzoomx, dzoomy, dmovx, dmovy);
                    painter.AddLine(aele0, aele1);
                }
                //painter.setPen(QPen(QColor(0,255,0)));
                for(int i=0;i<icount;i++)
                {
                    gp_Pnt aele = m_path.ElementAt(i);

                    transformPoint(aele, dzoomx, dzoomy, dmovx, dmovy);
                    painter.AddCross(aele,3); 
                }
                //painter.setPen(m_pen);
            }
            else if(6==m_ishow)
            {
                int icount = m_path.ElementCount();
                for(int i=0;i<icount;)
                {
                    gp_Pnt aele0;
                    gp_Pnt aele1;
                    if(i<icount)
                        aele0 = m_path.ElementAt(i);
                    i++;
                    if(i<icount)
                        aele1 = m_path.ElementAt(i);
                    i++;
                    transformPoint(aele0, dzoomx, dzoomy, dmovx, dmovy);
                    transformPoint(aele1, dzoomx, dzoomy, dmovx, dmovy);

                    painter.AddLine(aele0, aele1);
                }
                //painter.setPen(QPen(QColor(0,255,0)));
                for(int i=0;i<icount;i++)
                {
                    gp_Pnt aele = m_path.ElementAt(i);
                    transformPoint(aele, dzoomx, dzoomy, dmovx, dmovy);
                    painter.AddCross(aele, 3); 
                }
                //painter.setPen(m_pen);
            }
            else if(7==m_ishow)
            {
                int icount = m_path.ElementCount();
                for(int i=0;i<icount;)
                {
                    gp_Pnt aele0;gp_Pnt aele1;
                    if(i<icount)
                        aele0 = m_path.ElementAt(i);
                    i++; 
                    if(i<icount)
                        aele1 = m_path.ElementAt(i);
                    i++; 

                    painter.AddLine(aele0, aele1);
                }
                //painter.setPen(m_pen);
            }


            else if(8==m_ishow)
            {
               // painter.save();

                int icount = m_path.ElementCount();

                for(int i=0;i<icount-1;i++)
                {
                    gp_Pnt aele = m_path.ElementAt(i); 
                    transformPoint(aele, dzoomx, dzoomy, dmovx, dmovy);
                    //painter.setPen(QPen(QColor(255,0,0)));
                    painter.AddPoint(aele);
                    i++;
                    aele = m_path.ElementAt(i);
                    transformPoint(aele, dzoomx, dzoomy, dmovx, dmovy); 
                    //painter.setPen(QPen(QColor(0,0,255)));
                    painter.AddPoint(aele);
                }
              // painter.restore();
            }
 }

void PointsShape::load(const char * pchar)
{
    clear();
    FILE    *rf = fopen(pchar, "rb");
    if(nullptr==rf)
        return;
    fseek(rf,0,SEEK_END);
    int filesize = ftell(rf);
    char *pcharget =new char[filesize+10];
    memset(pcharget,0,filesize+10);
    rewind(rf);
    fread((char*)(pcharget),filesize,1,rf);
    std::string qstr = pcharget;
    std::vector<std::string> strnumlist = splitstring(qstr,',');
     
    for(int i=0;i<strnumlist.size()-3;i++)
    {
        int ix = std::stoi(strnumlist.at(i)); 
        i++;
        int iy = std::stoi(strnumlist.at(i));
        addpoint(ix,iy);
        i++;
        ix = std::stoi(strnumlist.at(i));
        i++;
        iy = std::stoi(strnumlist.at(i));
        addpoint(ix,iy);
    } 
    delete []pcharget;
        fclose(rf);
}
void PointsShape::save(const char * pchar)
{

    int isize = m_path.ElementCount();
    if(isize<=0)
        return;
    FILE    *rf = fopen(pchar, "w+");
   if ( rf == nullptr)
       return;
   rewind(rf);
   gp_Pnt aele = m_path.ElementAt(0);
    int ix =aele.X();
    int iy =aele.Y();
   fprintf(rf,"%d,",ix);
   fprintf(rf,"%d",iy);

   for(int i=1;i<isize;i++)
   {
       aele = m_path.ElementAt(i);
       ix =aele.X();
       iy =aele.Y();
       fprintf(rf,",");
       fprintf(rf,"%d,",ix);
       fprintf(rf,"%d",iy);
   }

   fclose(rf);
}

void PointsShape::loadAB(const char* pchar)
{
    clear();
    FILE* rf = fopen(pchar, "rb");
    if (nullptr == rf)
        return;
    fseek(rf, 0, SEEK_END);
    int filesize = ftell(rf);
    char* pcharget = new char[filesize + 10];
    memset(pcharget, 0, filesize + 10);
    rewind(rf);
    fread((char*)(pcharget), filesize, 1, rf);
    std::string qstr = pcharget;
    std::vector<std::string> strnumlist = splitstring(qstr, ',');

    for (int i = 0; i < strnumlist.size() - 3; i++)
    {
        double dx = std::stod(strnumlist.at(i));
        i++;
        double dy = std::stod(strnumlist.at(i));
        addpointa(dx, dy);
        i++;
        dx = std::stod(strnumlist.at(i));
        i++;
        dy = std::stod(strnumlist.at(i));
        addpointb(dx, dy);
    }
    delete[]pcharget;
    fclose(rf);
}
void PointsShape::saveAB(const char* pchar)
{ 
    int isize = m_pathA.ElementCount();
    if (isize <= 0)
        return;
    FILE* rf = fopen(pchar, "w+");
    if (rf == nullptr)
        return;
    rewind(rf);
    gp_Pnt aele0 = m_pathA.ElementAt(0);
    gp_Pnt aele1 = m_pathB.ElementAt(0);
    double ix = aele0.X();
    double iy = aele0.Y();
    fprintf(rf, "%f,", ix);
    fprintf(rf, "%f,", iy);
    ix = aele1.X();
    iy = aele1.Y();
    fprintf(rf, "%f,", ix);
    fprintf(rf, "%f", iy);
    for (int i = 1; i < isize; i++)
    {
        aele0 = m_pathA.ElementAt(i);
        aele1 = m_pathB.ElementAt(i);
        ix = aele0.X();
        iy = aele0.Y();
        fprintf(rf, ",");
        fprintf(rf, "%f,", ix);
        fprintf(rf, "%f", iy);
        ix = aele1.X();
        iy = aele1.Y();
        fprintf(rf, ",");
        fprintf(rf, "%f,", ix);
        fprintf(rf, "%f", iy);
    }

    fclose(rf);
}
void PointsShape::ABtoShape(std::vector<cv::Point2f>& points)
{
    int isize0 = m_pathA.ElementCount();
    int isize1 = m_pathB.ElementCount();

    if (isize0 != isize1)
        return;
    points.clear();
    for (int i = 0; i < isize0; i++)
    {
        gp_Pnt aele0 = m_pathA.ElementAt(i);
        gp_Pnt aele1 = m_pathB.ElementAt(i);
        gp_Pnt ptCent = gp_Pnt(0.5 * aele0.X() + 0.5 * aele1.X(), 0.5 * aele0.Y() + 0.5 * aele1.Y(), 0);
        points.push_back(cv::Point2f(0.5 * aele0.X() + 0.5 * aele1.X(), 0.5 * aele0.Y() + 0.5 * aele1.Y()));
    }
}
void PointsShape::patterngap2gap(int newgap)
{ 
    int isize = m_path.ElementCount();
    if(isize<=0)
        return;
    gp_Path path; 
    gp_Pnt aele0 = m_path.ElementAt(0);
    gp_Pnt aele1 = m_path.ElementAt(0);

    int ix0,iy0;
    int ix1,iy1;
    double dcx0,dcy0;
    int inx0,iny0;
    int inx1,iny1;
   //fprintf(rf,"%d,",ix);
   //fprintf(rf,"%d",iy);
   for(int i=0;i<isize;)
   {
       aele0 = m_path.ElementAt(i);
       ix0 = aele0.X();
       iy0 = aele0.Y();
       aele1 = m_path.ElementAt(i+1);
       ix1 =aele1.X();
       iy1 =aele1.Y();
       dcx0 = ix0*0.5 + ix1*0.5;
       dcy0 = iy0*0.5 + iy1*0.5;
       if(ix0==ix1)
       {
           inx0 = ix0;
           inx1 = ix1;
           iny0 = dcy0 - newgap;
           iny1 = dcy0 + newgap;

           path.AddPoint(gp_Pnt(inx0,iny0,0));
           i++;
           path.AddPoint(gp_Pnt(inx1,iny1,0));
           i++;
       }
       else if(iy0==iy1)
       {
           iny0 = iy0;
           iny1 = iy1;
           inx0 = dcx0 - newgap;
           inx1 = dcx0 + newgap;

           path.AddPoint(gp_Pnt(inx0,iny0,0));
           i++;
           path.AddPoint(gp_Pnt(inx1,iny1,0));
           i++;
       }
       else {
           assert(0);
       }
       //fprintf(rf,",");
       //fprintf(rf,"%d,",ix);
       //fprintf(rf,"%d",iy);
   }
   m_path = path;
   //fclose(rf);
}
void PointsShape::patternABgap2gap(double newgaprate)
{
    int isize0 = m_pathA.ElementCount();
    int isize1 = m_pathB.ElementCount();
    if (isize0 <= 0 && isize0!=isize1)
        return;
    gp_Path pathA;
    gp_Path pathB;
    gp_Pnt aele0 = m_pathA.ElementAt(0);
    gp_Pnt aele1 = m_pathB.ElementAt(0);

    double ix0, iy0;
    double ix1, iy1;
    double dcx0, dcy0;
    double dgpx0, dgpy0;
    double inx0, iny0;
    double inx1, iny1;

    for (int i = 0; i < isize0;i++)
    {
        aele0 = m_pathA.ElementAt(i);
        ix0 = aele0.X();
        iy0 = aele0.Y();
        aele1 = m_pathB.ElementAt(i);
        ix1 = aele1.X();
        iy1 = aele1.Y();
        dcx0 = ix0 * 0.5 + ix1 * 0.5;
        dcy0 = iy0 * 0.5 + iy1 * 0.5;
        dgpx0 = (ix0 - ix1) * 0.5;
        dgpy0 = (iy0 - iy1) * 0.5;

        inx0 = dcx0 + dgpx0 * newgaprate;
        iny0 = dcy0 + dgpy0 * newgaprate;
        inx1 = dcx0 - dgpx0 * newgaprate;
        iny1 = dcy0 - dgpy0 * newgaprate;

        pathA.AddPoint(gp_Pnt(inx0, iny0, 0)); 
        pathB.AddPoint(gp_Pnt(inx1, iny1, 0));


    }
    m_pathA.Clear();
    m_pathB.Clear();
    m_pathA.CopyPath(pathA);
    m_pathB.CopyPath(pathB); 
    //fclose(rf);
}
void PointsShape::patternABsample(int irate)
{
    if (irate < 1)
        return;
    int isize0 = m_pathA.ElementCount();
    int isize1 = m_pathB.ElementCount();
    if (isize0 <= 0 && isize0 != isize1)
        return;
    gp_Path pathA;
    gp_Path pathB;
    gp_Pnt aele0 = m_pathA.ElementAt(0);
    gp_Pnt aele1 = m_pathB.ElementAt(0);

    double ix0, iy0;
    double ix1, iy1;
    double dcx0, dcy0;
    double dgpx0, dgpy0;
    double inx0, iny0;
    double inx1, iny1;
    int isample = irate;
    for (int i = 0; i < isize0; i++)
    {
        if (isample == 1)
        {
            aele0 = m_pathA.ElementAt(i);
            ix0 = aele0.X();
            iy0 = aele0.Y();
            aele1 = m_pathB.ElementAt(i);
            ix1 = aele1.X();
            iy1 = aele1.Y();

            inx0 = ix0;
            iny0 = iy0;
            inx1 = ix1;
            iny1 = iy1;

            pathA.AddPoint(gp_Pnt(inx0, iny0, 0));
            pathB.AddPoint(gp_Pnt(inx1, iny1, 0)); 
        }

        isample = isample - 1; 
        if (isample == 0)
            isample = irate;
    }
    m_pathA.Clear();
    m_pathB.Clear();
    m_pathA.CopyPath(pathA);
    m_pathB.CopyPath(pathB);
    //fclose(rf);
}
 

void PointsShape::patterntokeys(gp_Path&keypointsA, gp_Path&keypointsA_,
    gp_Path&keypointsB, gp_Path&keypointsB_,
    int ibackgroundtype)
{//ibackgroundtype=0无背景=1黑背景=2白背景
    int i = 0;
    while(i<m_path.ElementCount())
    {
         gp_Pnt aele0 = m_path.ElementAt(i);
         i++;
         if (i >= m_path.ElementCount()) break;
         gp_Pnt aele1 = m_path.ElementAt(i);
         i++;
         int ix0 = aele0.X();
         int iy0 = aele0.Y();
         int ix1 = aele1.X();
         int iy1 = aele1.Y();

         int ix = aele0.X() + aele1.X();
         int iy = aele0.Y() + aele1.Y();
         if( ix0 == ix1 && iy0 > iy1 )
         {
             if(0==ibackgroundtype)
             keypointsA_.AddPoint(gp_Pnt(ix/2,iy/2,0));
             else if(1==ibackgroundtype)
             keypointsA_.AddPoint(gp_Pnt(ix0,iy0,0));
             else if(2==ibackgroundtype)
             keypointsA_.AddPoint(gp_Pnt(ix1,iy1,0));
         }
         else if( ix0 == ix1 && iy0 < iy1 )
         {
             if(0==ibackgroundtype)
             keypointsA.AddPoint(gp_Pnt(ix/2,iy/2,0));
             else if(1==ibackgroundtype)
             keypointsA.AddPoint(gp_Pnt(ix1,iy1,0));
             else if(2==ibackgroundtype)
             keypointsA.AddPoint(gp_Pnt(ix0,iy0,0));
         }
         else if(iy0 == iy1 && ix0 > ix1)
         {
             if(0==ibackgroundtype)
             keypointsB_.AddPoint(gp_Pnt(ix/2,iy/2,0));
             else if(1==ibackgroundtype)
             keypointsB_.AddPoint(gp_Pnt(ix1,iy1,0));
             else if(2==ibackgroundtype)
             keypointsB_.AddPoint(gp_Pnt(ix0,iy0,0));
         }
         else if(iy0 == iy1 && ix0 < ix1)
         {
             if(0==ibackgroundtype)
             keypointsB.AddPoint(gp_Pnt(ix/2,iy/2,0));
             else if(1==ibackgroundtype)
             keypointsB.AddPoint(gp_Pnt(ix0,iy0,0));
             else if(2==ibackgroundtype)
             keypointsB.AddPoint(gp_Pnt(ix1,iy1,0));
         }
    }

}
void PointsShape::keystopattern(gp_Path&keypointsA, gp_Path&keypointsA_,
    gp_Path&keypointsB, gp_Path&keypointsB_,
    int igap,int itype,int isgap,int iline)
{

    clear();
    int icount0 = keypointsA.ElementCount();
    int icount1 = keypointsA_.ElementCount();
    int icount2 =  keypointsB.ElementCount();
    int icount3 =  keypointsB_.ElementCount();
    if(1==itype)
    {
        for(int i=0;i<icount0;i++)
        {
            gp_Pnt apoint = keypointsA.ElementAt(i);
            Standard_Real ix0 = apoint.X();
            Standard_Real iy0 = apoint.Y()-igap;
            Standard_Real ix1 = apoint.X();
            Standard_Real iy1 = apoint.Y()+igap;
            addpointa(ix0,iy0);
            addpointb(ix1,iy1);
        }
        for(int i=0;i<icount1;i++)
        {
            gp_Pnt apoint = keypointsA_.ElementAt(i);
            Standard_Real ix0 = apoint.X();
            Standard_Real iy0 = apoint.Y()+igap;
            Standard_Real ix1 = apoint.X();
            Standard_Real iy1 = apoint.Y()-igap;
            addpointa(ix0,iy0);
            addpointb(ix1,iy1);
        }
        for(int i=0;i<icount2;i++)
        {
            gp_Pnt apoint = keypointsB.ElementAt(i);
            Standard_Real ix0 = apoint.X()-igap;
            Standard_Real iy0 = apoint.Y();
            Standard_Real ix1 = apoint.X()+igap;
            Standard_Real iy1 = apoint.Y();
            addpointa(ix0,iy0);
            addpointb(ix1,iy1);
        }
        for(int i=0;i<icount3;i++)
        {
            gp_Pnt apoint = keypointsB_.ElementAt(i);
            Standard_Real ix0 = apoint.X()+igap;
            Standard_Real iy0 = apoint.Y();
            Standard_Real ix1 = apoint.X()-igap;
            Standard_Real iy1 = apoint.Y();
            addpointa(ix0,iy0);
            addpointb(ix1,iy1);
        }
    }
    else if(2==itype)
    {
        for(int i=0;i<icount0;i++)
        {
            gp_Pnt apoint = keypointsA.ElementAt(i);
            Standard_Real ix0 = apoint.X();
            Standard_Real iy0 = apoint.Y()-igap;
            Standard_Real ix1 = apoint.X();
            Standard_Real iy1 = apoint.Y()+igap;
            addpointa(ix0,iy0);
            addpointb(ix1,iy1);
            Standard_Real ix2 = apoint.X();
            Standard_Real iy2 = iy0-igap;
            Standard_Real ix3 = apoint.X();
            Standard_Real iy3 = iy1+igap;
            addpointa(ix2,iy2);
            addpointb(ix3,iy3);
        }
        for(int i=0;i<icount1;i++)
        {
            gp_Pnt apoint = keypointsA_.ElementAt(i);
            Standard_Real ix0 = apoint.X();
            Standard_Real iy0 = apoint.Y()+igap;
            Standard_Real ix1 = apoint.X();
            Standard_Real iy1 = apoint.Y()-igap;
            addpointa(ix0,iy0);
            addpointb(ix1,iy1);
            Standard_Real ix2 = apoint.X();
            Standard_Real iy2 = iy0+igap;
            Standard_Real ix3 = apoint.X();
            Standard_Real iy3 = iy1-igap;
            addpointa(ix2,iy2);
            addpointb(ix3,iy3);
        }
        for(int i=0;i<icount2;i++)
        {
            gp_Pnt apoint = keypointsB.ElementAt(i);
            Standard_Real ix0 = apoint.X()-igap;
            Standard_Real iy0 = apoint.Y();
            Standard_Real ix1 = apoint.X()+igap;
            Standard_Real iy1 = apoint.Y();
            addpointa(ix0,iy0);
            addpointb(ix1,iy1);
            Standard_Real ix2 = ix0-igap;
            Standard_Real iy2 = apoint.Y();
            Standard_Real ix3 = ix1+igap;
            Standard_Real iy3 = apoint.Y();
            addpointa(ix2,iy2);
            addpointb(ix3,iy3);
        }
        for(int i=0;i<icount3;i++)
        {
            gp_Pnt apoint = keypointsB_.ElementAt(i);
            Standard_Real ix0 = apoint.X()+igap;
            Standard_Real iy0 = apoint.Y();
            Standard_Real ix1 = apoint.X()-igap;
            Standard_Real iy1 = apoint.Y();
            addpointa(ix0,iy0);
            addpointb(ix1,iy1);
            Standard_Real ix2 = ix0+igap;
            Standard_Real iy2 = apoint.Y();
            Standard_Real ix3 = ix1-igap;
            Standard_Real iy3 = apoint.Y();
            addpointa(ix2,iy2);
            addpointb(ix3,iy3);
        }
    }
    else if(100==itype)
    {
        for(int i=0;i<icount0;i++)
        {
            gp_Pnt apoint = keypointsA.ElementAt(i);
            Standard_Real ix0 = apoint.X();
            Standard_Real iy0 = apoint.Y()-igap;
            Standard_Real ix1 = apoint.X();
            Standard_Real iy1 = apoint.Y()+isgap;//+igap;
            addpointa(ix0,iy0);
            addpointb(ix1,iy1);
            Standard_Real ix2 = apoint.X();
            Standard_Real iy2 = iy0-igap;
            Standard_Real ix3 = apoint.X();
            Standard_Real iy3 = iy1;//+igap;
            addpointa(ix2,iy2);
            addpointb(ix3,iy3);
        }
        for(int i=0;i<icount1;i++)
        {
            gp_Pnt apoint = keypointsA_.ElementAt(i);
            Standard_Real ix0 = apoint.X();
            Standard_Real iy0 = apoint.Y()+igap;
            Standard_Real ix1 = apoint.X();
            Standard_Real iy1 = apoint.Y()-isgap;//-igap;
            addpointa(ix0,iy0);
            addpointb(ix1,iy1);
            Standard_Real ix2 = apoint.X();
            Standard_Real iy2 = iy0+igap;
            Standard_Real ix3 = apoint.X();
            Standard_Real iy3 = iy1;//-igap;
            addpointa(ix2,iy2);
            addpointb(ix3,iy3);
        }
        for(int i=0;i<icount2;i++)
        {
            gp_Pnt apoint = keypointsB.ElementAt(i);
            Standard_Real ix0 = apoint.X()-igap;
            Standard_Real iy0 = apoint.Y();
            Standard_Real ix1 = apoint.X()+isgap;//+igap;
            Standard_Real iy1 = apoint.Y();
            addpointa(ix0,iy0);
            addpointb(ix1,iy1);
            Standard_Real ix2 = ix0-igap;
            Standard_Real iy2 = apoint.Y();
            Standard_Real ix3 = ix1;//+igap;
            Standard_Real iy3 = apoint.Y();
            addpointa(ix2,iy2);
            addpointb(ix3,iy3);
        }
        for(int i=0;i<icount3;i++)
        {
            gp_Pnt apoint = keypointsB_.ElementAt(i);
            Standard_Real ix0 = apoint.X()+igap;
            Standard_Real iy0 = apoint.Y();
            Standard_Real ix1 = apoint.X()-isgap;//-igap;
            Standard_Real iy1 = apoint.Y();
            addpointa(ix0,iy0);
            addpointb(ix1,iy1);
            Standard_Real ix2 = ix0+igap;
            Standard_Real iy2 = apoint.Y();
            Standard_Real ix3 = ix1;//-igap;
            Standard_Real iy3 = apoint.Y();
            addpointa(ix2,iy2);
            addpointb(ix3,iy3);
        }

    }
    else if(200==itype)
    {
        for(int i=0;i<icount0;i++)
        {
            gp_Pnt apoint = keypointsA.ElementAt(i);
            Standard_Real ix0 = apoint.X();
            Standard_Real iy0 = apoint.Y()-isgap;//-igap;
            Standard_Real ix1 = apoint.X();
            Standard_Real iy1 = apoint.Y()+igap;
            addpointa(ix0,iy0);
            addpointb(ix1,iy1);
            Standard_Real ix2 = apoint.X();
            Standard_Real iy2 = iy0;//-igap;
            Standard_Real ix3 = apoint.X();
            Standard_Real iy3 = iy1+igap;
            addpointa(ix2,iy2);
            addpointb(ix3,iy3);
        }
        for(int i=0;i<icount1;i++)
        {
            gp_Pnt apoint = keypointsA_.ElementAt(i);
            Standard_Real ix0 = apoint.X();
            Standard_Real iy0 = apoint.Y()+isgap;//+igap;
            Standard_Real ix1 = apoint.X();
            Standard_Real iy1 = apoint.Y()-igap;
            addpointa(ix0,iy0);
            addpointb(ix1,iy1);
            Standard_Real ix2 = apoint.X();
            Standard_Real iy2 = iy0;//+igap;
            Standard_Real ix3 = apoint.X();
            Standard_Real iy3 = iy1-igap;
            addpointa(ix2,iy2);
            addpointb(ix3,iy3);
        }
        for(int i=0;i<icount2;i++)
        {
            gp_Pnt apoint = keypointsB.ElementAt(i);
            Standard_Real ix0 = apoint.X()-isgap;//-igap;
            Standard_Real iy0 = apoint.Y();
            Standard_Real ix1 = apoint.X()+igap;
            Standard_Real iy1 = apoint.Y();
            addpointa(ix0,iy0);
            addpointb(ix1,iy1);
            Standard_Real ix2 = ix0;//-igap;
            Standard_Real iy2 = apoint.Y();
            Standard_Real ix3 = ix1+igap;
            Standard_Real iy3 = apoint.Y();
            addpointa(ix2,iy2);
            addpointb(ix3,iy3);
        }
        for(int i=0;i<icount3;i++)
        {
            gp_Pnt apoint = keypointsB_.ElementAt(i);
            Standard_Real ix0 = apoint.X()+isgap;//+igap;
            Standard_Real iy0 = apoint.Y();
            Standard_Real ix1 = apoint.X()-igap;
            Standard_Real iy1 = apoint.Y();
            addpointa(ix0,iy0);
            addpointb(ix1,iy1);
            Standard_Real ix2 = ix0;//+igap;
            Standard_Real iy2 = apoint.Y();
            Standard_Real ix3 = ix1-igap;
            Standard_Real iy3 = apoint.Y();
            addpointa(ix2,iy2);
            addpointb(ix3,iy3);
        }
    }
    else if(1000==itype)
    {
        for(int i=0;i<icount0;i++)
        {
            gp_Pnt apoint = keypointsA.ElementAt(i);
            Standard_Real ix0 = apoint.X();
            Standard_Real iy0 = apoint.Y()-igap;
            Standard_Real ix1 = apoint.X();
            Standard_Real iy1 = apoint.Y()+isgap;//+igap;
            addpointa(ix0,iy0);
            addpointb(ix1,iy1);
            for(int j=0;j<iline;j++)
            {
                iy0 = iy0-igap;
                addpointa(ix0,iy0);
                addpointb(ix1,iy1);
            }
        }
        for(int i=0;i<icount1;i++)
        {
            gp_Pnt apoint = keypointsA_.ElementAt(i);
            Standard_Real ix0 = apoint.X();
            Standard_Real iy0 = apoint.Y()+igap;
            Standard_Real ix1 = apoint.X();
            Standard_Real iy1 = apoint.Y()-isgap;//-igap;
            addpointa(ix0,iy0);
            addpointb(ix1,iy1);
            for(int j=0;j<iline;j++)
            {
                iy0 = iy0+igap;
                addpointa(ix0,iy0);
                addpointb(ix1,iy1);
            }
        }
        for(int i=0;i<icount2;i++)
        {
            gp_Pnt apoint = keypointsB.ElementAt(i);
            Standard_Real ix0 = apoint.X()-igap;
            Standard_Real iy0 = apoint.Y();
            Standard_Real ix1 = apoint.X()+isgap;//+igap;
            Standard_Real iy1 = apoint.Y();
            addpointa(ix0,iy0);
            addpointb(ix1,iy1);
            for(int j=0;j<iline;j++)
            {
                ix0 = ix0-igap;
                addpointa(ix0,iy0);
                addpointb(ix1,iy1);
            }
        }
        for(int i=0;i<icount3;i++)
        {
            gp_Pnt apoint = keypointsB_.ElementAt(i);
            Standard_Real ix0 = apoint.X()+igap;
            Standard_Real iy0 = apoint.Y();
            Standard_Real ix1 = apoint.X()-isgap;//-igap;
            Standard_Real iy1 = apoint.Y();
            addpointa(ix0,iy0);
            addpointb(ix1,iy1);
            for(int j=0;j<iline;j++)
            {
                ix0 = ix0+igap;
                addpointa(ix0,iy0);
                addpointb(ix1,iy1);
            }
        }

    }
    else if(2000==itype)
    {
        for(int i=0;i<icount0;i++)
        {
            gp_Pnt apoint = keypointsA.ElementAt(i);
            Standard_Real ix0 = apoint.X();
            Standard_Real iy0 = apoint.Y();//-igap;
            Standard_Real ix1 = apoint.X();
            Standard_Real iy1 = apoint.Y()+igap;
            addpointa(ix0,iy0);
            addpointb(ix1,iy1);

            for(int j=0;j<iline;j++)
            {
                iy1 = iy1+igap;
                addpointa(ix0,iy0);
                addpointb(ix1,iy1);
            }
        }
        for(int i=0;i<icount1;i++)
        {
            gp_Pnt apoint = keypointsA_.ElementAt(i);
            Standard_Real ix0 = apoint.X();
            Standard_Real iy0 = apoint.Y();//+igap;
            Standard_Real ix1 = apoint.X();
            Standard_Real iy1 = apoint.Y()-igap;
            addpointa(ix0,iy0);
            addpointb(ix1,iy1);
            for(int j=0;j<iline;j++)
            {
                iy1 = iy1-igap;
                addpointa(ix0,iy0);
                addpointb(ix1,iy1);
            }
        }
        for(int i=0;i<icount2;i++)
        {
            gp_Pnt apoint = keypointsB.ElementAt(i);
            Standard_Real ix0 = apoint.X();//-igap;
            Standard_Real iy0 = apoint.Y();
            Standard_Real ix1 = apoint.X()+igap;
            Standard_Real iy1 = apoint.Y();
            addpointa(ix0,iy0);
            addpointb(ix1,iy1);
            for(int j=0;j<iline;j++)
            {
                ix1 = ix1+igap;
                addpointa(ix0,iy0);
                addpointb(ix1,iy1);
            }
        }
        for(int i=0;i<icount3;i++)
        {
            gp_Pnt apoint = keypointsB_.ElementAt(i);
            Standard_Real ix0 = apoint.X();//+igap;
            Standard_Real iy0 = apoint.Y();
            Standard_Real ix1 = apoint.X()-igap;
            Standard_Real iy1 = apoint.Y();
            addpointa(ix0,iy0);
            addpointb(ix1,iy1);
            for(int j=0;j<iline;j++)
            {
                ix1 = ix1-igap;
                addpointa(ix0,iy0);
                addpointb(ix1,iy1);
            }
        }

    }

}
void PointsShape::keyszoom(gp_Path &keypointsA,gp_Path &keypointsA_,
                   gp_Path &keypointsB,gp_Path &keypointsB_,
                    double dxz,double dyz){}
void PointsShape::patternzoom(double dxz,double dyz,int igap,int itype,int iline)
{
    if(0==itype)
    {
        gp_Path keypointsA;
        gp_Path keypointsA_;
        gp_Path keypointsB;
        gp_Path keypointsB_;
        patterntokeys(keypointsA,keypointsA_,keypointsB,keypointsB_);
        keyszoom(keypointsA,keypointsA_,keypointsB,keypointsB_,dxz,dyz);
        keystopattern(keypointsA,keypointsA_,keypointsB,keypointsB_,igap);
    }
    else if(1==itype)
    {//黑色线和形体模型缩小（底色为白）
        gp_Path keypointsA;
        gp_Path keypointsA_;
        gp_Path keypointsB;
        gp_Path keypointsB_;
        patterntokeys(keypointsA,keypointsA_,keypointsB,keypointsB_,1);
        keyszoom(keypointsA,keypointsA_,keypointsB,keypointsB_,dxz,dyz);
        keystopattern(keypointsA,keypointsA_,keypointsB,keypointsB_,igap,1000,0,iline);
    }
    else if(2==itype)
    {//白色线和形体模型缩小（底色为黑）
        gp_Path keypointsA;
        gp_Path keypointsA_;
        gp_Path keypointsB;
        gp_Path keypointsB_;
        patterntokeys(keypointsA,keypointsA_,keypointsB,keypointsB_,2);
        keyszoom(keypointsA,keypointsA_,keypointsB,keypointsB_,dxz,dyz);
        keystopattern(keypointsA,keypointsA_,keypointsB,keypointsB_,igap,2000,0,iline);
    }
 }
void PointsShape::keysrootgrid(int ibackgroundtype,
    double drate,
    int ilevel)
{

}

void PointsShape::crosslinea(void *aline)
{

}
void PointsShape::crosslineb(void *bline)
{

}
void PointsShape::crosspoint(int inum0,int inum1,int inum2,int inum3)
{

}

void PointsShape::findrightgrid(int inum,int igap,int &iminxnum,int &iminynum,int &imin3)
{
    gp_Pnt aele0 = m_path.ElementAt(inum);

    double dvaluex0 = aele0.X();
    double dvaluey0 = aele0.Y();

    double dminx = 999;
    double dminy = 999;

     iminxnum = -1;
     iminynum = -1;

    int icount = m_path.ElementCount();

    for(int i=0;i<icount;i++)
    {
       // if(i>inum)
        {
            gp_Pnt aele = m_path.ElementAt(i);

             double dvaluex1 = aele.X();
             double dvaluey1 = aele.Y();
                 double dabsvaluex = (dvaluex1 - dvaluex0)>0?(dvaluex1 - dvaluex0):(dvaluex0 - dvaluex1);
                 double dabsvaluey = (dvaluey1 - dvaluey0)>0?(dvaluey1 - dvaluey0):(dvaluey0 - dvaluey1);

                 if(dvaluex1>dvaluex0)
                 {
                     if(dabsvaluey<igap)
                     {
                        if(dminx>dabsvaluex)
                        {
                           iminxnum = i ;
                           dminx = dabsvaluex;
                        }
                     }
                 }

                 if(dvaluey1>dvaluey0)
                 {
                     if(dabsvaluex<igap)
                     {
                         if(dminy>dabsvaluey)
                         {
                            iminynum = i ;
                            dminy = dabsvaluey;
                         }

                     }
                 }


        }
    }
    if(iminxnum<0||iminynum<0)
        return;
    gp_Pnt aeleX = m_path.ElementAt(iminxnum);
    gp_Pnt aeleY = m_path.ElementAt(iminynum);

    double dx0 = aeleX.X();
    double dy0 = aeleY.Y();
     imin3 = -1;
    for(int i=0;i<icount;i++)
    {
        if(inum!=i&&iminxnum!=i&&iminynum!=i)
        {
            gp_Pnt aele = m_path.ElementAt(i);

             double dvaluex1 = aele.X();
             double dvaluey1 = aele.Y();

             double dabsvaluex = (dvaluex1 - dx0)>0?(dvaluex1 - dx0):(dx0-dvaluex1);
             double dabsvaluey = (dvaluey1 - dy0)>0?(dvaluey1 - dy0):(dy0-dvaluey1);
             if(dabsvaluey<igap&&dabsvaluex<igap)
             {
                   imin3 = i ;
             }
         }
    }
}

void PointsShape::gridpoints(void *points)
{
    //findrightgrid(int inum,int igap);
    PointsShape *tpoints = (PointsShape *)points;
    tpoints->clear();
    int ilasthnum=-1;
    int inextnum = 0;
    int icount = m_path.ElementCount();

    double dmindd = 999999;
    int iminddnum = -1;
    for (int i=0;i<icount;i++)
    {
        gp_Pnt aele = m_path.ElementAt(i);
        double dvaluex = aele.X();
        double dvaluey = aele.Y();
        double dds = dvaluex*dvaluex + dvaluey*dvaluey;
        if(dmindd>dds)
        {
            iminddnum = i;
            dmindd = dds;
        }

    }

    inextnum = iminddnum;

    int icalnum = 0;
    while(inextnum>=0)
    {
        int iminxnum = -1;
        int iminynum = -1;
        int imin3 =-1;
        findrightgrid(inextnum,9,iminxnum,iminynum,imin3);
        if(-1!=iminxnum&&-1!=iminynum&&-1!=imin3)
        {
            icalnum = icalnum + 1;
            gp_Pnt aele = m_path.ElementAt(inextnum);
            double dvaluex = aele.X();
            double dvaluey = aele.Y();
            tpoints->addpoint(dvaluex,dvaluey);
            gp_Pnt aele0 = m_path.ElementAt(iminxnum);
            double dvaluex1 = aele0.X();
            double dvaluey1 = aele0.Y();
            tpoints->addpoint(dvaluex1,dvaluey1);
            gp_Pnt aele1 = m_path.ElementAt(imin3);
            double dvaluex2 = aele1.X();
            double dvaluey2 = aele1.Y();
            tpoints->addpoint(dvaluex2,dvaluey2);
            gp_Pnt aele2 = m_path.ElementAt(iminynum);
            double dvaluex3 = aele2.X();
            double dvaluey3 = aele2.Y();
            tpoints->addpoint(dvaluex3,dvaluey3);
            inextnum = iminxnum;
            if(-1==ilasthnum)
                ilasthnum = iminynum;
        }
        else
        {
            if(inextnum != ilasthnum)
            {
                inextnum = ilasthnum ;
                ilasthnum = -1;
            }
            else {
                inextnum = -1;
            }

        }
    }

}

void PointsShape::gridrightline(void *gridpoints)
{
    PointsShape *tpoints = (PointsShape *)gridpoints;
    tpoints->clear();
    int icount = m_path.ElementCount();
    for (int i=0;i<icount;)
    {
        gp_Pnt aele0 = m_path.ElementAt(i);
        double dvaluex0 = aele0.X();
        double dvaluey0 = aele0.Y();
        tpoints->addpoint(dvaluex0,dvaluey0);
        i=i+3;
        if(i<icount)
        {
            gp_Pnt aele1 = m_path.ElementAt(i);
            double dvaluex1 = aele1.X();
            double dvaluey1 = aele1.Y();
            tpoints->addpoint(dvaluex1,dvaluey1);
        }
        i=i+1;
    }

}
void PointsShape::patterntranform(int igap,int itype,int isgap,int iline)
{//模型转换
    gp_Path keypointsA;
    gp_Path keypointsA_;
    gp_Path keypointsB;
    gp_Path keypointsB_;
    patterntokeys(keypointsA,keypointsA_,keypointsB,keypointsB_);
    keystopattern(keypointsA,keypointsA_,keypointsB,keypointsB_,igap,itype,isgap,iline);
}
void PointsShape::doublepattern(double igap,int idirect,PointsShape& apoints)
{//more and more furture in here

    double dx ;
    double dy  ;
    switch (idirect) {
        case 12:// 12 o'clock /up
        {
            int icount = m_path.ElementCount();
            for(int i=0;i<icount;i++)
            {
                 gp_Pnt aele = m_path.ElementAt(i);
                 dx = aele.X();
                 dy = aele.Y() + igap;
                 apoints.addpointa(dx,dy);
                 dx = aele.X();
                 dy = aele.Y() - igap;
                 apoints.addpointb(dx,dy);
            }
        }
            break;
        case 6:// 6 o'clock /down
        {
            int icount = m_path.ElementCount();
            for(int i=0;i<icount;i++)
            {
                gp_Pnt aele = m_path.ElementAt(i);
                dx = aele.X();
                dy = aele.Y() - igap;
                apoints.addpointa(dx,dy);
                dx = aele.X();
                dy = aele.Y() + igap;
                apoints.addpointb(dx,dy);
            }
        }
            break;
        case 3:// 3 o'clock /left
        {
            int icount = m_path.ElementCount();
            for(int i=0;i<icount;i++)
            {
                 gp_Pnt aele = m_path.ElementAt(i);
                 dx = aele.X() - igap;
                 dy = aele.Y();
                 apoints.addpointa(dx,dy);
                 dx = aele.X() + igap;
                 dy = aele.Y();
                 apoints.addpointb(dx,dy);
            }
        }
            break;
        case 9:// 6 o'clock /right
        {
            int icount = m_path.ElementCount();
            for(int i=0;i<icount;i++)
            {
                gp_Pnt aele = m_path.ElementAt(i);
                dx = aele.X() + igap;
                dy = aele.Y();
                apoints.addpointa(dx,dy);
                dx = aele.X() - igap;
                dy = aele.Y();
                apoints.addpointb(dx,dy);
            }
        }
            break;

    }

}
void PointsShape::onepattern(double igap,int idirect,PointsShape& apoints)
{//more and more furture in here

    double dx ;
    double dy  ;
    switch (idirect) {
        case 12:// 12 o'clock /up
        {
            int icount = m_path.ElementCount();
            for(int i=0;i<icount;i++)
            {
                 gp_Pnt aele = m_path.ElementAt(i);
                 dx = aele.X();
                 dy = aele.Y();
                 apoints.addpointa(dx,dy);
                 dx = aele.X();
                 dy = aele.Y() - igap;
                 apoints.addpointb(dx,dy);
            }
        }
            break;
        case 6:// 6 o'clock /down
        {
            int icount = m_path.ElementCount();
            for(int i=0;i<icount;i++)
            {
                gp_Pnt aele = m_path.ElementAt(i);
                dx = aele.X();
                dy = aele.Y();
                apoints.addpointa(dx,dy);
                dx = aele.X();
                dy = aele.Y() + igap;
                apoints.addpointb(dx,dy);
            }
        }
            break;
        case 3:// 3 o'clock /left
        {
            int icount = m_path.ElementCount();
            for(int i=0;i<icount;i++)
            {
                 gp_Pnt aele = m_path.ElementAt(i);
                 dx = aele.X();
                 dy = aele.Y();
                 apoints.addpointa(dx,dy);
                 dx = aele.X() + igap;
                 dy = aele.Y();
                 apoints.addpointb(dx,dy);
            }
        }
            break;
        case 9:// 6 o'clock /right
        {
            int icount = m_path.ElementCount();
            for(int i=0;i<icount;i++)
            {
                gp_Pnt aele = m_path.ElementAt(i);
                dx = aele.X();
                dy = aele.Y();
                apoints.addpointa(dx,dy);
                dx = aele.X() - igap;
                dy = aele.Y();
                apoints.addpointb(dx,dy);
            }
        }
            break;

    }

}
void PointsShape::doublesample(int isamplerate,PointsShape& apoints)
{
    double dx ;
    double dy  ;
    int icount = m_path.ElementCount();
    int idoublepointscount = icount/2;
    for(int i=0;i<idoublepointscount;i++)
    {
        if(i%isamplerate==0)
        {
            int inum = i*2;
            gp_Pnt aele = m_path.ElementAt(inum);
            dx = aele.X()  ;
            dy = aele.Y()  ;
            apoints.addpointa(dx,dy);
            aele = m_path.ElementAt(inum+1);
            dx = aele.X()  ;
            dy = aele.Y()  ;
            apoints.addpointb(dx,dy);
        }
     }
}
void PointsShape::resampleAB(int inum)
{ 
    if ( inum <= 0)
    {
        return;
    }
    double dx;
    double dy;
    int icount0 = m_pathA.ElementCount();
    int icount1 = m_pathB.ElementCount();
    if (icount0 <= inum)
        return;

    int isamplerate = icount0 / inum;
    gp_Path m_path_A;
    gp_Path m_path_B;
     
    for (int i = 0; i < icount0; i++)
    {
        if (i % isamplerate == 0)
        {
            gp_Pnt aele = m_pathA.ElementAt(i);
            dx = aele.X();
            dy = aele.Y();
            m_path_A.AddPoint(aele);
            aele = m_pathB.ElementAt(i);
            dx = aele.X();
            dy = aele.Y();
            m_path_B.AddPoint(aele);
        }
    }

    m_pathA.Clear();
    m_pathB.Clear();
    m_pathA.CopyPath(m_path_A);
    m_pathB.CopyPath(m_path_B);
 

}
void PointsShape::calibration(double dx,double dy,double dangle)
{
    gp_Vec translationVector(dx, dy, 0); // 沿X轴平移5，沿Y轴平移3，沿Z轴平移2
    m_path.Translate(translationVector);
    gp_Pnt apoint(0, 0, 0);
    m_path.RotateAroundPoint(apoint, dangle); 

}
double PointsShape::getx(int inum) const
{
    int icount = m_path.ElementCount();
    if(icount>inum&&inum>=0)
    {
        gp_Pnt aele = m_path.ElementAt(inum);
        return aele.X();
    }
    else {
        return -99999;
    }
}

double PointsShape::script_getx(int inum)
{
    return getx(inum);
}

double PointsShape::gety(int inum) const
{
    int icount = m_path.ElementCount();
    if(icount>inum&&inum>=0)
    {
        gp_Pnt aele = m_path.ElementAt(inum);
        return aele.Y();
    }
    else {
        return -99999;
    }

}

void PointsShape::exportPoints(std::vector<CxShapePoint>& out) const
{
    out.clear();
    const int count = m_path.ElementCount();
    for (int i = 0; i < count; ++i)
    {
        gp_Pnt p = m_path.ElementAt(i);
        out.push_back({p.X(), p.Y()});
    }
}

CxShapeHit PointsShape::hitTest(double x, double y, double tolerance) const
{
    const double t_sq = tolerance * tolerance;
    const int count = m_path.ElementCount();
    for (int i = 0; i < count; ++i)
    {
        gp_Pnt p = m_path.ElementAt(i);
        const double dx = x - p.X();
        const double dy = y - p.Y();
        const double dist_sq = dx * dx + dy * dy;
        if (dist_sq <= t_sq)
            return { true, CxShapeHandleRole::Vertex, i, std::sqrt(dist_sq) };
    }
    return {};
}

void PointsShape::enumerateHandles(std::vector<CxShapeHandle>& out) const
{
    const int count = m_path.ElementCount();
    for (int i = 0; i < count; ++i)
    {
        gp_Pnt p = m_path.ElementAt(i);
        out.push_back({ CxShapeHandleRole::Vertex, i, { p.X(), p.Y() }, "V" + std::to_string(i) });
    }
}

void PointsShape::dragHandle(CxShapeHandleRole role, int vertex_index, double x, double y)
{
    if (role != CxShapeHandleRole::Vertex)
        return;

    const int count = m_path.ElementCount();
    if (vertex_index < 0 || vertex_index >= count)
        return;

    gp_Pnt p = m_path.ElementAt(vertex_index);
    gp_Vec delta(x - p.X(), y - p.Y(), 0);

    gp_Path new_path;
    for (int i = 0; i < count; ++i)
    {
        gp_Pnt pt = m_path.ElementAt(i);
        if (i == vertex_index)
            new_path.AddPoint(gp_Pnt(x, y, 0));
        else
            new_path.AddPoint(pt);
    }
    m_path = new_path;
}

void PointsShape::translateBy(double dx, double dy)
{
    gp_Path new_path;
    const int count = m_path.ElementCount();
    for (int i = 0; i < count; ++i)
    {
        gp_Pnt p = m_path.ElementAt(i);
        new_path.AddPoint(gp_Pnt(p.X() + dx, p.Y() + dy, 0));
    }
    m_path = new_path;
}

double PointsShape::script_gety(int inum)
{
    return gety(inum);
}

//圆条件
#define CXCORE_CIRCLE_STEP_DISTANCE_RESIDUAL(y,x,x0,y0,R2) ((x-x0)*(x-x0)+(y-y0)*(y-y0)-R2)

#define _DISTANCEX(y,x,x0,y0,R2) PointDistance[abs(x-x0)][abs(y-y0)]-R2
//椭圆条件
#define _EDISTANCE(y,x,x0,y0,x1,y1,R2) sqrt((x-x0)*(x-x0)+(y-y0)*(y-y0))+sqrt((x-x1)*(x-x1)+(y-y1)*(y-y1))-R2;




//////////////////////////////////////////////////////////////////////////
//循迹方程
//根据输入的轨迹方向 和 约束条件 输出 {轨迹方向   [中心]---------[S点] 顺时针  |[S点]---------[中心] 逆时针}
//圆弧循迹 输入圆心 和 圆上点 和 半径
int PointsShape::CircleStep(int apha,int x,int y,int i_x0,int i_y0,int  i_R_2)
{
    float Eightsum[8];
    apha=apha%360;
    Eightsum[0]=(x-i_x0)*(x-i_x0)+(y+1-i_y0)*(y+1-i_y0)-i_R_2;
    Eightsum[1]=(x+1-i_x0)*(x+1-i_x0)+(y+1-i_y0)*(y+1-i_y0)-i_R_2;
    Eightsum[2]=(x+1-i_x0)*(x+1-i_x0)+(y-i_y0)*(y-i_y0)-i_R_2;
    Eightsum[3]=(x+1-i_x0)*(x+1-i_x0)+(y-1-i_y0)*(y-1-i_y0)-i_R_2;
    Eightsum[4]=(x-i_x0)*(x-i_x0)+(y-1-i_y0)*(y-1-i_y0)-i_R_2;
    Eightsum[5]=(x-1-i_x0)*(x-1-i_x0)+(y-1-i_y0)*(y-1-i_y0)-i_R_2;
    Eightsum[6]=(x-1-i_x0)*(x-1-i_x0)+(y-i_y0)*(y-i_y0)-i_R_2;
    Eightsum[7]=(x-1-i_x0)*(x-1-i_x0)+(y+1-i_y0)*(y+1-i_y0)-i_R_2;
    for(int i=0;i<8;i++)
    {
        //Direction
        if((Eightsum[i]>=0&&Eightsum[i+1]<=0)
                ||(Eightsum[i+1]>=0&&Eightsum[i]<=0))//Eightsum[i]*Eightsum[i+1]<=0)
        {
                if(Eightsum[i]<0)
                    if(i*45!=apha)
                    return i*45;
                else
                    if(Eightsum[i+1]<0)
                        if(i*45+45!=apha)
                            return i*45+45;
        }
        if(7==i)
            if(Eightsum[i]*Eightsum[0]<=0)
                if(Eightsum[i]<0)
                    if(i*45!=apha)
                    return i*45;
                else
                    if(Eightsum[0]<0)
                        if(apha!=0)
                        return 0;
    }
    return 0;

}

//////////////////////////////////////////////////////////////////////////
//循迹方程
//根据输入的轨迹方向 和 约束条件 输出 轨迹方向
//椭圆弧循迹 输入圆心 和 圆上点 和 半径
int  PointsShape::EllipseStep(int apha, int x, int y, int i_ax, int i_ay, int i_bx, int i_by, int  i_dis,int &imx,int &imy)
{
    double Eightsum[8];
    apha=apha%360;
    Eightsum[0]=_EDISTANCE(y+1,x,i_ax,i_ay,i_bx,i_by,i_dis);
    Eightsum[1]=_EDISTANCE(y+1,x+1,i_ax,i_ay,i_bx,i_by,i_dis);
    Eightsum[2]=_EDISTANCE(y,x+1,i_ax,i_ay,i_bx,i_by,i_dis);
    Eightsum[3]=_EDISTANCE(y-1,x+1,i_ax,i_ay,i_bx,i_by,i_dis);
    Eightsum[4]=_EDISTANCE(y-1,x,i_ax,i_ay,i_bx,i_by,i_dis);
    Eightsum[5]=_EDISTANCE(y-1,x-1,i_ax,i_ay,i_bx,i_by,i_dis);
    Eightsum[6]=_EDISTANCE(y,x-1,i_ax,i_ay,i_bx,i_by,i_dis);
    Eightsum[7]=_EDISTANCE(y+1,x-1,i_ax,i_ay,i_bx,i_by,i_dis);
    for(int i=0;i<8;i++)
    {
        //Direction
        if(Eightsum[i]*Eightsum[i+1]<=0)//)||Eightsum[i]>0&&Eightsum[i+1]<0)
        {
            if(Eightsum[i]<0)
                if(i*45!=apha)
                    return i*45;
                else
                    if(Eightsum[i+1]<0)
                        if(i*45+45!=apha)
                            return i*45+45;
        }
        if(7==i)
            if(Eightsum[i]*Eightsum[0]<=0)
                if(Eightsum[i]<0)
                    if(i*45!=apha)
                        return i*45;
                    else
                        if(Eightsum[0]<0)
                            if(apha!=0)
                                return 0;
    }
    return 0;
}

void PointsShape::arccircle(int i_x,int i_y,int i_x1,int i_y1,int i_x0,int i_y0)
{
    clear();
    int i_R_2=(i_x-i_x0)*(i_x-i_x0)+(i_y-i_y0)*(i_y-i_y0);

    float R=sqrt((float)(i_R_2));
    int i_circle_pointsum=R>(int)R?(R+1)*2*PI:R*2*PI;

    int x=i_x;
    int y=i_y;
    //get first find point
    int apha=0;//循迹方向
    int igetsum=0;
    int inix=i_x1;
    int iniy=i_y1;
    int iR4 = R*4;
    for(int i=0;i<i_circle_pointsum;i++)
    {
        apha=(CircleStep(apha,x,y,i_x0,i_y0,i_R_2))%360;
        switch(apha)
        {
        case 0:
            y=y+1;
            break;
        case 45:
            x=x+1;
            y=y+1;
            break;
        case 90:
            x=x+1;
            break;
        case 135:
            x=x+1;
            y=y-1;
            break;
        case 180:
            y=y-1;
            break;
        case 225:
            y=y-1;
            x=x-1;
            break;
        case 270:
            x=x-1;
            break;
        case 315:
            x=x-1;
            y=y+1;
            break;
        default:;
            //assert("THIS IS ERROR");
        }
        gp_Pnt  apoint0(x,y,0);
        addpoint(apoint0);
/*
        if(0==i)
        {
            inix=x;
            iniy=y;
        }
*/
        igetsum=igetsum+1;
        apha=(180+apha)%360;
       // if(i>iR4)
        {
            if(x==inix&&y==iniy)
                break;
            if(x==inix+1&&y==iniy)
                break;
            if(x==inix-1&&y==iniy)
                break;
            if(x==inix&&y==iniy-1)
                break;
            if(x==inix&&y==iniy+1)
                break;
            if(x==inix-1&&y==iniy-1)
                break;
            if(x==inix+1&&y==iniy+1)
                break;
        }

    }

}
//////////////////////////////////////////////////////////////////////////
void PointsShape::circlepoints(int i_x,int i_y,int i_x0,int i_y0)
{
    clear();
    int i_R_2=(i_x-i_x0)*(i_x-i_x0)+(i_y-i_y0)*(i_y-i_y0);

    float R=sqrt((float)(i_R_2));
    int i_circle_pointsum=R>(int)R?(R+1)*2*PI:R*2*PI;

    int x=i_x;
    int y=i_y;
    //get first find point
    int apha=0;//循迹方向
    int igetsum=0;
    int inix=i_x;
    int iniy=i_y;
    int iR4 = R*4;
    for(int i=0;i<i_circle_pointsum;i++)
    {
        apha=(CircleStep(apha,x,y,i_x0,i_y0,i_R_2))%360;
        switch(apha)
        {
        case 0:
            y=y+1;
            break;
        case 45:
            x=x+1;
            y=y+1;
            break;
        case 90:
            x=x+1;
            break;
        case 135:
            x=x+1;
            y=y-1;
            break;
        case 180:
            y=y-1;
            break;
        case 225:
            y=y-1;
            x=x-1;
            break;
        case 270:
            x=x-1;
            break;
        case 315:
            x=x-1;
            y=y+1;
            break;
        default:;
            //assert("THIS IS ERROR");
        }
        gp_Pnt  apoint0(x,y,0);
        addpoint(apoint0);
/*
        if(0==i)
        {
            inix=x;
            iniy=y;
        }
        */
        igetsum=igetsum+1;
        apha=(180+apha)%360;
        if(i>iR4)
        {
            if(x==inix&&y==iniy)
                break;
            if(x==inix+1&&y==iniy)
                break;
            if(x==inix-1&&y==iniy)
                break;
            if(x==inix&&y==iniy-1)
                break;
            if(x==inix&&y==iniy+1)
                break;
            if(x==inix-1&&y==iniy-1)
                break;
            if(x==inix+1&&y==iniy+1)
                break;
        }

    }

}
void PointsShape::halfcircle(int i_x,int i_y,int i_x1,int i_y1)
{
    clear();
    int i_x0 = (i_x+i_x1)>>1;
    int i_y0 = (i_y+i_y1)>>1;
    int i_R_2=(i_x-i_x0)*(i_x-i_x0)+(i_y-i_y0)*(i_y-i_y0);

    float R=sqrt((float)(i_R_2));
    int i_circle_pointsum=R>(int)R?(R+1)*PI:R*PI;

    int x=i_x;
    int y=i_y;
    //get first find point
    int apha=0;//循迹方向
    int igetsum=0;
    int inix=0;
    int iniy=0;
    int iR4 = R*2 ;

        {
            inix=i_x1;
            iniy=i_y1;
        }
    for(int i=0;i<i_circle_pointsum;i++)
    {
        apha=(CircleStep(apha,x,y,i_x0,i_y0,i_R_2))%360;
        switch(apha)
        {
        case 0:
            y=y+1;
            break;
        case 45:
            x=x+1;
            y=y+1;
            break;
        case 90:
            x=x+1;
            break;
        case 135:
            x=x+1;
            y=y-1;
            break;
        case 180:
            y=y-1;
            break;
        case 225:
            y=y-1;
            x=x-1;
            break;
        case 270:
            x=x-1;
            break;
        case 315:
            x=x-1;
            y=y+1;
            break;
        default:;
            //assert("THIS IS ERROR");
        }
        gp_Pnt  apoint0(x,y,0);
        addpoint(apoint0);


        igetsum=igetsum+1;
        apha=(180+apha)%360;
        if(i>iR4)
        {
            if(x==inix&&y==iniy)
                break;
            if(x==inix+1&&y==iniy)
                break;
            if(x==inix-1&&y==iniy)
                break;
            if(x==inix&&y==iniy-1)
                break;
            if(x==inix&&y==iniy+1)
                break;
            if(x==inix-1&&y==iniy-1)
                break;
            if(x==inix+1&&y==iniy+1)
                break;
        }

    }

}
//////////////////////////////////////////////////////////////////////////
void PointsShape::ellipsepointsx(int i_x, int i_y, int i_x0, int i_y0, int ix1, int iy1)
{
    clear();
    int i_R_2=(i_x-i_x0)*(i_x-i_x0)+(i_y-i_y0)*(i_y-i_y0);
    int x_=i_x-i_x0;
    int y_=i_y-i_y0;
    float R=sqrt((float)(i_R_2));
    double fdis = sqrt((i_x - ix1) * (i_x - ix1) + (i_y - iy1) * (i_y - iy1))
        + sqrt((i_x0 - ix1) * (i_x0 - ix1) + (i_y0 - iy1) * (i_y0 - iy1));
        
    //求长边和短边
    if(fdis <R)
        return;
    int iedge1 = fdis;
    int iedge2 = 2*sqrt((float)(fdis * fdis /4 - R*R/4)) ;

    //int iedge1_2= idis*idis;
    //
    int i_circle_pointsum=iedge1*2+iedge2*2;

    //
    int curX,curY;

    //zero memory
    int Direct_y=0,Direct_x=0;
    int D_y=0,D_x=0;
    int x= ix1;
    int y= iy1;
    //get first find point
    int apha=0;//循迹方向
    int igetsum=0;
    int inix=0;
    int iniy=0;
    int imx, imy;
    for(int i=0;i<i_circle_pointsum;i++)
    {
        apha=(EllipseStep(apha,x,y,i_x,i_y,i_x0,i_y0, fdis,imx,imy))%360;
        switch(apha)
        {
        case 0:
            y=y+1;
            break;
        case 45:
            x=x+1;
            y=y+1;
            break;
        case 90:
            x=x+1;
            break;
        case 135:
            x=x+1;
            y=y-1;
            break;
        case 180:
            y=y-1;
            break;
        case 225:
            y=y-1;
            x=x-1;
            break;
        case 270:
            x=x-1;
            break;
        case 315:
            x=x-1;
            y=y+1;
            break;
        default:;
           // AfxMessageBox("THIS IS ERROR");
        }
        if(x==inix&&y==iniy)
            break;

        gp_Pnt  apoint0(x,y,0);
        addpoint(apoint0);
       // pPointlist[i].x=x;
       // pPointlist[i].y=y;
        if(0==i)
        {
            inix=x;//pPointlist[i].X();
            iniy=y;//pPointlist[i].Y();
        }
        igetsum=igetsum+1;
        apha=(180+apha)%360;

    }

}
//////////////////////////////////////////////////////////////////////////
 // Ellipse
void PointsShape::OBBCenterAngleSort(void * points)
{
    PointsShape* tpoints = (PointsShape*)points;;
    if (tpoints == nullptr)
        return;
    gp_Pnt apnt = m_path.OBBCenterAngleSort();
    tpoints->clear();
    tpoints->addpoint(apnt);
    tpoints->setcolor(0, 250, 0);
    tpoints->setshow(1);
}
//////////////////////////////////////////////////////////////////////////
void PointsShape::PointsMaxLen(void* points)
{
    PointsShape* tpoints = (PointsShape*)points;;
    if (tpoints == nullptr)
        return;
    gp_Pnt apnt = m_path.OBBCenterAngleSort();
    tpoints->clear();
    tpoints->addpoint(apnt);
    tpoints->setcolor(0, 250, 0);
    tpoints->setshow(1);
}

//////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////
void PointsShape::arcpoints(int i_x,int i_y,int i_x0,int i_y0,int ipoinsum)
{
    clear();
    int i_R_2=(i_x-i_x0)*(i_x-i_x0)+(i_y-i_y0)*(i_y-i_y0);
    int x_=i_x-i_x0;
    int y_=i_y-i_y0;
    float R=sqrt((float)(i_R_2));
    int i_circle_pointsum=R>(int)R?(R+1)*PI*2:R*PI*2;
    if(i_circle_pointsum<ipoinsum)
        ipoinsum = i_circle_pointsum -1;

    int x=i_x;
    int y=i_y;
    //get first find point
    int apha=0;
    int igetsum=0;
    int inix=0;
    int iniy=0;
    for(int i=0;i<ipoinsum;i++)
    {
        apha=(CircleStep(apha,x,y,i_x0,i_y0,i_R_2))%360;
        switch(apha)
        {
        case 0:
            y=y+1;
            break;
        case 45:
            x=x+1;
            y=y+1;
            break;
        case 90:
            x=x+1;
            break;
        case 135:
            x=x+1;
            y=y-1;
            break;
        case 180:
            y=y-1;
            break;
        case 225:
            y=y-1;
            x=x-1;
            break;
        case 270:
            x=x-1;
            break;
        case 315:
            x=x-1;
            y=y+1;
            break;
        default:;
//            AfxMessageBox("THIS IS ERROR");
        }

        gp_Pnt  apoint0(x,y,0);
        addpoint(apoint0);

        if(0==i)
        {
            inix=x;
            iniy=y;
        }
        igetsum=igetsum+1;
        apha=(180+apha)%360;

    }


}

void PointsShape::sample(double drate)
{
    if(drate<=0)
        return;
  PointsShape anewpoints;
  int igap = 1/drate;

        int icount = m_path.ElementCount();
        int ineedp=0;
        for(int i=0;i<icount;i++)
        {
            if(ineedp==igap)
            {
             gp_Pnt aele = m_path.ElementAt(i);
             double dvaluex = aele.X();
             double dvaluey = aele.Y();
             anewpoints.addpoint(dvaluex,dvaluey);
             ineedp=0;
            }
            ineedp++;
        }
        clear();
        addpoints(anewpoints);

}

void PointsShape::part(int ipart0,int ipart1)
{
    int icount = m_path.ElementCount();
    if(ipart0>icount||ipart1>icount)
        return;
  PointsShape anewpoints;
        int ineedp=0;
        if(ipart0<ipart1)
        {
            for(int i=0;i<icount;i++)
            {
                if(i>=ipart0&&i<=ipart1)
                {
                 gp_Pnt aele = m_path.ElementAt(i);


                 double dvaluex = aele.X();
                 double dvaluey = aele.Y();

                 anewpoints.addpoint(dvaluex,dvaluey);
                }
            }

        }
        else
        {
            for(int i=0;i<icount;i++)
            {
                if(i<=ipart1||i>ipart0)
                {
                 gp_Pnt aele = m_path.ElementAt(i);


                 double dvaluex = aele.X();
                 double dvaluey = aele.Y();

                 anewpoints.addpoint(dvaluex,dvaluey);
                }
            }

        }
        clear();
        addpoints(anewpoints);

}
void PointsShape::ratetopoint(double dx,double dy,double drate)
{
  PointsShape anewpoints;
        int icount = m_path.ElementCount();
        for(int i=0;i<icount;i++)
        {
             gp_Pnt aele = m_path.ElementAt(i);
            double dvaluex;
            double dvaluey;
            if(drate>0)
            {
                dvaluex = aele.X() + (aele.X() - dx)*drate;
                dvaluey = aele.Y() + (aele.Y() - dy)*drate;
           }
           else
           {
                dvaluex = dx + (dx -aele.X() )*drate;
                dvaluey = dy + (dy -aele.Y() )*drate;
            }
            anewpoints.addpoint(dvaluex,dvaluey);
        }
        clear();
        addpoints(anewpoints);
}
 
struct Point_2D {
    float x, y;
    Point_2D(float x = 0, float y = 0) : x(x), y(y) {}
};
struct Vector2 {
    float x, y;

    Vector2(float x = 0, float y = 0) : x(x), y(y) {}

    float length() const {
        return std::sqrt(x * x + y * y);
    }

    Vector2 normalized() const {
        float len = length();
        if (len == 0) return Vector2(0, 0);
        return Vector2(x / len, y / len);
    }

    float dot(const Vector2& other) const {
        return x * other.x + y * other.y;
    }
};
struct PointCloud {
    std::vector<Point_2D> pts;

    inline size_t kdtree_get_point_count() const {
        return pts.size();
    }

    inline float kdtree_get_pt(const size_t idx, const size_t dim) const {
        if (dim == 0) return pts[idx].x;
        else          return pts[idx].y;
    }

    template <typename BBOX>
    bool kdtree_get_bbox(BBOX&   bb) const {
        return false; // 不提供包围盒
    }
};
using namespace nanoflann;

typedef KDTreeSingleIndexAdaptor<L2_Simple_Adaptor<float, PointCloud>,PointCloud,2>  MyKdTree;
struct PointCloud2D {
    std::vector<Point_2D> pts;

    inline size_t kdtree_get_point_count() const { return pts.size(); }

    inline float kdtree_get_pt(const size_t idx, const size_t dim) const {
        return dim == 0 ? pts[idx].x : pts[idx].y;
    }

    template <typename BBOX>
    bool kdtree_get_bbox(BBOX& /*bb*/) const { return false; }
};
typedef KDTreeSingleIndexAdaptor<L2_Simple_Adaptor<float, PointCloud2D>, PointCloud2D, 2> MyKdTree2D;



struct Neighbor {
    size_t index;
    float distance;
    float angle;

    Neighbor(size_t i, float d, float a)
        : index(i), distance(d), angle(a) {
    }
};
// 排序依据
bool compareByDistance(const Neighbor& a, const Neighbor& b) {
    return a.distance < b.distance;
}
bool compareByAngle(const Neighbor& a, const Neighbor& b) {
    return a.angle < b.angle;
}

float lineangle(int ix0, int iy0, int ix1, int iy1,
                int ix2, int iy2, int ix3, int iy3)
{ 
    gp_Pnt startP0(ix0,iy0,0), endP0(ix1,iy1,0);
    gp_Pnt startP1(ix2,iy2,0), endP1(ix3,iy3,0);

    gp_Vec vec = gp_Vec(startP0, endP0).Normalized();
    gp_Dir dirction1(vec / vec.Magnitude()); //方向
    gp_Vec vec2 = gp_Vec(startP1, endP1).Normalized();
    gp_Dir dirction2(vec2 / vec2.Magnitude()); //方向
    // 计算旋转角度（弧度）
    float dotProduct = (float)dirction1.Dot(dirction2); // 两个向量的点乘

    float theta = acos(dotProduct) * (180.0 / M_PI);

    return theta;
}
float lineangle(gp_Dir dirction1,gp_Dir dirction2)
{
    // 计算旋转角度（弧度）
    float dotProduct = (float)dirction1.Dot(dirction2); // 两个向量的点乘

    float theta = acos(dotProduct) * (180.0 / M_PI);

    return theta;
}
void findnode(std::vector<std::vector<size_t>>& neighbors_list, 
    std::vector<size_t> &list,
    int inum)
{
    int isize = neighbors_list[inum].size();
    for (int is=0; is < isize; is++)
    {
        list.push_back(neighbors_list[inum][is]);
        findnode(neighbors_list, list, neighbors_list[inum][is]);
    }
}
std::vector<std::vector<size_t>> FindKNearestAndSort
(const std::vector<Point_2D>& points,size_t k,
    const std::vector<Vector2>& directions)
{  
    size_t num_points = points.size();
    std::vector<std::vector<size_t>> vectlist;
    std::vector<std::vector<size_t>> neighbors_list(num_points);
    std::vector<size_t> nodesonvalue(num_points);

    // 构建点云和 kd-tree
    PointCloud cloud;
    cloud.pts = points;

    MyKdTree index(3, cloud, KDTreeSingleIndexAdaptorParams(10));
    index.buildIndex();
     

    for (size_t i = 0; i < num_points; ++i) 
    {
        // 获取当前查询点和方向
        const Point_2D& query = points[i];
        Vector2 dir = directions[i].normalized();
        float fradius = directions[i].length();

        // 查询最多 k 个最近点（也可以换成 radius search）
        std::vector<unsigned int> indices(k);
        std::vector<float> dists2(k);

        // 使用 kNN 搜索
        nanoflann::KNNResultSet<float, unsigned int> result_set(k);
        result_set.init(indices.data(), dists2.data());
        index.findNeighbors(result_set, &query.x, nanoflann::SearchParameters());
        size_t found = result_set.size();

        // 收集邻居并计算距离、角度
        std::vector<Neighbor> neighbors;
        for (size_t j = 0; j < found; ++j) 
        {
            size_t neighbor_idx = indices[j];
            if (neighbor_idx == i) continue; // 跳过自己


            gp_Pnt startP0(query.x, query.y, 0), endP0(query.x + directions[i].x, query.y + directions[i].y, 0);

            gp_Vec vec = gp_Vec(startP0, endP0).Normalized();
            gp_Dir dirction1(vec / vec.Magnitude()); //方向


            gp_Pnt startP1(query.x, query.y, 0), endP1(points[neighbor_idx].x, points[neighbor_idx].y, 0);
            gp_Vec vec2 = gp_Vec(startP1, endP1).Normalized();
            gp_Dir dirction2(vec2 / vec2.Magnitude()); //方向

            float dotProduct = (float)dirction1.Dot(dirction2); // 两个向量的点乘

            float theta = acos(dotProduct) * (180.0 / M_PI);


            float dists = sqrt(dists2[j]);
            if(theta < 45 && dists< fradius)
                neighbors.push_back(Neighbor(neighbor_idx, dists, theta));
                                                  // 实际距离    // 角度（弧度）
        }
 
        // 排序策略一：按距离升序
        std::sort(neighbors.begin(), neighbors.end(), compareByDistance);
        std::vector<size_t> sorted_by_distance;
        for (size_t nidx = 0; nidx < neighbors.size(); ++nidx)
        {
            const Neighbor& n = neighbors[nidx];
            sorted_by_distance.push_back(n.index);
            nodesonvalue[n.index] = 2;
        }

        if (0)
        {  
            // 排序策略二：按角度升序
            std::sort(neighbors.begin(), neighbors.end(), compareByAngle);
            std::vector<size_t> sorted_by_angle;
            for (size_t nidx = 0; nidx < neighbors.size(); ++nidx)
            {
                const Neighbor& n = neighbors[nidx];
                sorted_by_angle.push_back(n.index);
            }
        }

        // 可以根据需要返回其中一个排序结果，或者两者都保留
         neighbors_list[i] = sorted_by_distance; // 默认使用距离排序+

    }

    //查找到初始节点且串联
    
    for (size_t i = 0; i < num_points; ++i) 
    { 
        if (1 == nodesonvalue[i])
        { 
            std::vector<size_t> alist; 
            findnode(neighbors_list, alist,i); 
            vectlist.push_back(alist);
        }
    }

    return vectlist;
}
bool comparePoints(const Point_2D& a, const Point_2D& b) {
    if (a.x != b.x)
        return a.x < b.x;
    else
        return a.y < b.y;
}
bool operator==(const Point_2D& a, const Point_2D& b) {
      return a.x == b.x && a.y == b.y;
   }
std::vector<size_t> GetRootIndices(const std::vector<int>& hasParent0)
{
    std::vector<size_t> rootIndices;
    for (size_t i = 0; i < hasParent0.size(); ++i) 
    {
        if (-1==hasParent0[i]) 
        {
            rootIndices.push_back(i);
        }
    }
    return rootIndices;
}
std::vector<std::vector<size_t>> FindAngleAndDistanceFilteredNeighbors(
    const std::vector<Point_2D>& points,
    const std::vector<Vector2>& directions,
    size_t k,
    float max_angle_degrees,
    float max_distance,
    std::vector<int>& hasParent0) // 输出：每个点是否被其他点选为邻居
{

    std::vector<std::vector<size_t>> neighborChains(points.size());
    hasParent0.assign(points.size(), -1);

    // 构建 kd-tree
    PointCloud2D cloud;
    cloud.pts = points;

    MyKdTree2D index(2, cloud, KDTreeSingleIndexAdaptorParams(10));
    index.buildIndex();

    float max_angle_rad = max_angle_degrees * M_PI / 180.0f;

    for (unsigned int i = 0; i < points.size(); ++i) 
    {
        const Point_2D& query = points[i];
        Vector2 dir = directions[i].normalized();

        std::vector<unsigned int> indices(k);
        std::vector<float> dists2(k);

        nanoflann::KNNResultSet<float, unsigned int> result_set(k);
        result_set.init(indices.data(), dists2.data());
        index.findNeighbors(result_set, &query.x, nanoflann::SearchParameters());
        size_t found = result_set.size();

        std::vector<Neighbor> filtered_neighbors;

        for (size_t j = 0; j < found; ++j) 
        {
            size_t neighbor_idx = indices[j];
            if (neighbor_idx == i) continue;

            // 向量差
            Vector2 vec(query.x - points[neighbor_idx].x,query.y - points[neighbor_idx].y);
            vec = vec.normalized();

            float dot = dir.dot(vec);
            dot = std::max(-1.0f, dot);
            dot = std::min(1.0f, dot);
            float angle_rad = std::acos(dot);

                gp_Pnt startP0(query.x, query.y, 0), endP0(query.x + directions[i].x, query.y + directions[i].y, 0);
                gp_Vec vec0 = gp_Vec(startP0, endP0).Normalized();
                gp_Dir dirction1(vec0 / vec0.Magnitude()); //方向 
                gp_Pnt startP1(query.x, query.y, 0), endP1(points[neighbor_idx].x, points[neighbor_idx].y, 0);
                gp_Vec vec2 = gp_Vec(startP1, endP1).Normalized();
                gp_Dir dirction2(vec2 / vec2.Magnitude()); //方向 
                float dotProduct = (float)dirction1.Dot(dirction2); // 两个向量的点乘 
                float theta = acos(dotProduct) * (180.0 / M_PI); 
                 
            float distance = std::sqrt(dists2[j]);
            // 条件过滤
            if (theta < max_angle_degrees
                && theta > -max_angle_degrees
                && distance <= max_distance) 
            {
            //if (angle_rad <= max_angle_rad && distance <= max_distance) {
                filtered_neighbors.push_back(Neighbor(neighbor_idx, abs(theta), distance));
            } 
        }
        // 按Distance排序
        std::sort(filtered_neighbors.begin(), filtered_neighbors.end(), compareByDistance); 
        // 提取索引并标记父邻点
        std::vector<size_t> sorted_indices;
        for (size_t nidx = 0; nidx < filtered_neighbors.size(); ++nidx)
        { 
            const Neighbor& n = filtered_neighbors[nidx];
            sorted_indices.push_back(n.index);
            hasParent0[n.index] = i; // 被别人选为邻居了
            break;
        }
        if (sorted_indices.size() <= 0)
        { 
            // 按angle排序
            std::sort(filtered_neighbors.begin(), filtered_neighbors.end(), compareByAngle);
            for (size_t nidx = 0; nidx < filtered_neighbors.size(); ++nidx)
            {
                const Neighbor& n = filtered_neighbors[nidx];
                sorted_indices.push_back(n.index);
                hasParent0[n.index] = i; // 被别人选为邻居了
                break;
            }
        }  
        neighborChains[i] = sorted_indices;
    }
    return neighborChains;
}
std::vector<std::vector<size_t>> FindAngleAndDistanceFilteredNeighbors_Radius(
    const std::vector<Point_2D>& points,
    const std::vector<Vector2>& directions,
    float radius,                  // 半径（欧氏距离）
    float max_angle_degrees,       // 最大夹角（度）
    float max_distance,            // 可选：最大距离限制（<= radius）
    std::vector<int>& hasParent0
)
{
    size_t num_points = points.size();
    std::vector<std::vector<size_t>> neighborChains(num_points);
    hasParent0.assign(points.size(), -1);
    // 构建 kd-tree
    PointCloud2D cloud;
    cloud.pts = points;

    MyKdTree2D index(2, cloud, KDTreeSingleIndexAdaptorParams(10));
    index.buildIndex();

    float max_angle_rad = max_angle_degrees * M_PI / 180.0f;

    for (size_t i = 0; i < num_points; ++i) 
    {
        const Point_2D& query = points[i];
        Vector2 dir = directions[i].normalized();

        // radiusSearch 返回的是 pair<index, squared distance>
 
        std::vector<ResultItem<unsigned int, float>> ret_matches;
        nanoflann::SearchParameters params;
       
        // 执行 radiusSearch
        size_t found = index.radiusSearch(&query.x, radius * radius, ret_matches, params);

        std::vector<Neighbor> filtered_neighbors;

        for (size_t j = 0; j < found; ++j) 
        {
            size_t neighbor_idx = ret_matches[j].first;
            if (neighbor_idx == i) continue;

            float distance = std::sqrt(ret_matches[j].second);

            gp_Pnt startP0(query.x, query.y, 0), endP0(query.x + directions[i].x, query.y + directions[i].y, 0);
            gp_Vec vec0 = gp_Vec(startP0, endP0).Normalized();
            gp_Dir dirction1(vec0 / vec0.Magnitude()); //方向 
            gp_Pnt startP1(query.x, query.y, 0), endP1(points[neighbor_idx].x, points[neighbor_idx].y, 0);
            gp_Vec vec2 = gp_Vec(startP1, endP1).Normalized();
            gp_Dir dirction2(vec2 / vec2.Magnitude()); //方向 
            float dotProduct = (float)dirction1.Dot(dirction2); // 两个向量的点乘 
            float theta = acos(dotProduct) * (180.0 / M_PI);

            // 条件过滤
            if (theta <= max_angle_degrees
                && theta >= -max_angle_degrees
                && distance <= max_distance)
            {
                //if (angle_rad <= max_angle_rad && distance <= max_distance) {
                filtered_neighbors.emplace_back(neighbor_idx, distance, abs(theta));
            }
        }

        // 按Distance排序
        std::sort(filtered_neighbors.begin(), filtered_neighbors.end(), compareByDistance);
        // 提取索引并标记父邻点
        std::vector<size_t> sorted_indices;
        bool bselect = false;
        for (const auto& n : filtered_neighbors)
        {
            if (false == bselect)
            {
                bselect = true;
                sorted_indices.push_back(n.index);
            }
            hasParent0[n.index] = i; // 被别人选为邻居了
           
        }
        if (sorted_indices.size() <= 0)
        {
            // 按angle排序
            std::sort(filtered_neighbors.begin(), filtered_neighbors.end(), compareByAngle);
              bselect = false;
            for (const auto& n : filtered_neighbors)
            {
                if (false == bselect)
                {
                    bselect = true;
                    sorted_indices.push_back(n.index);
                }
                hasParent0[n.index] = i; // 被别人选为邻居了
                
            }
        }

        neighborChains[i] = sorted_indices;
    }

    return neighborChains;
}

//
std::vector<std::vector<size_t>> FindAngleAndDistanceFilteredNeighbors_adaptive_Radius(
    const std::vector<Point_2D>& points,
    const std::vector<Vector2>& directions,
    float gap_radius,                  // 半径增量（欧氏距离）
    float max_radius,                  // 最大半径（欧氏距离）　
    float max_angle_degrees,       // 最大夹角（度） float max_distance,// 可选：最大距离限制（<= radius）
    std::vector<int>& hasParent0
    )
{
    size_t num_points = points.size();
    std::vector<std::vector<size_t>> neighborChains(num_points);
    hasParent0.assign(points.size(), -1);
    // 构建 kd-tree
    PointCloud2D cloud;
    cloud.pts = points;

    MyKdTree2D index(2, cloud, KDTreeSingleIndexAdaptorParams(10));
    index.buildIndex();
    float max_angle_rad = max_angle_degrees * M_PI / 180.0f;
    for (size_t i = 0; i < num_points; ++i)
    {
        const Point_2D& query = points[i];
        Vector2 dir = directions[i].normalized();
        // radiusSearch 返回的是 pair<index, squared distance>
        std::vector<ResultItem<unsigned int, float>> ret_matches;
        nanoflann::SearchParameters params;
        std::vector<Neighbor> filtered_neighbors;
        int iradtimes = max_radius / gap_radius;
        for (int inum = 1; inum < iradtimes; inum++)//逐步加大扫描半径
        {
            float fcurradius = gap_radius * inum;
            // 执行 radiusSearch
            size_t found = index.radiusSearch(&query.x, fcurradius * fcurradius, ret_matches, params);
            
            for (size_t j = 0; j < found; ++j)
            {
                size_t neighbor_idx = ret_matches[j].first;
                if (neighbor_idx == i) continue;
                float distance = std::sqrt(ret_matches[j].second);

                gp_Pnt startP0(query.x, query.y, 0), endP0(query.x + directions[i].x, query.y + directions[i].y, 0);
                gp_Vec vec0 = gp_Vec(startP0, endP0).Normalized();
                gp_Dir dirction1(vec0 / vec0.Magnitude()); //方向 
                gp_Pnt startP1(query.x, query.y, 0), endP1(points[neighbor_idx].x, points[neighbor_idx].y, 0);
                gp_Vec vec2 = gp_Vec(startP1, endP1).Normalized();
                gp_Dir dirction2(vec2 / vec2.Magnitude()); //方向 
                float dotProduct = (float)dirction1.Dot(dirction2); // 两个向量的点乘 
                float theta = acos(dotProduct) * (180.0 / M_PI);

                // 条件过滤
                if (theta <= max_angle_degrees
                    && theta >= -max_angle_degrees)//&& distance <= max_distance
                {
                    //if (angle_rad <= max_angle_rad && distance <= max_distance) {
                    filtered_neighbors.emplace_back(neighbor_idx, distance, abs(theta));
                }
            }
            if (found != 0 && filtered_neighbors.size() != 0)
                break; 
        }
        // 按Distance排序
        std::sort(filtered_neighbors.begin(), filtered_neighbors.end(), compareByDistance);
        // 提取索引并标记父邻点
        std::vector<size_t> sorted_indices;
        bool bselect = false;
        for (const auto& n : filtered_neighbors)
        {
            if (false == bselect)
            {
                bselect = true;
                sorted_indices.push_back(n.index);
            }
            hasParent0[n.index] = i; // 被别人选为邻居了
        }
        if (sorted_indices.size() <= 0)
        {// 按angle排序
            std::sort(filtered_neighbors.begin(), filtered_neighbors.end(), compareByAngle);
            bselect = false;
            for (const auto& n : filtered_neighbors)
            {
                if (false == bselect)
                {
                    bselect = true;
                    sorted_indices.push_back(n.index);
                }
                hasParent0[n.index] = i; // 被别人选为邻居了
            }
        }
        neighborChains[i] = sorted_indices;
    }
    return neighborChains;
}

std::vector<std::vector<size_t>> FindAngleAndDistanceFilteredNeighbors_adaptive_Radius_adaptive_angle_no_(
    const std::vector<Point_2D>& points,
    const std::vector<Vector2>& directions,
    float gap_radius,                  // 半径增量（欧氏距离）
    float max_radius,                  // 最大半径（欧氏距离）　
    float max_angle_degrees,       // 最大夹角（度） float max_distance,// 可选：最大距离限制（<= radius）
    std::vector<int>& hasParent0
)
{
    size_t num_points = points.size();
    std::vector<std::vector<size_t>> neighborChains(num_points);
    hasParent0.assign(points.size(), -1);
    // 构建 kd-tree
    PointCloud2D cloud;
    cloud.pts = points;

    MyKdTree2D index(2, cloud, KDTreeSingleIndexAdaptorParams(10));
    index.buildIndex();
    float max_angle_rad = max_angle_degrees * M_PI / 180.0f;
    for (size_t i = 0; i < num_points; ++i)
    {
        const Point_2D& query = points[i];
        Vector2 dir = directions[i].normalized();
        // radiusSearch 返回的是 pair<index, squared distance>
        std::vector<ResultItem<unsigned int, float>> ret_matches;
        nanoflann::SearchParameters params;
        std::vector<Neighbor> filtered_neighbors;
        int iradtimes = max_radius / gap_radius;
        for (int inum = 1; inum < iradtimes; inum++)
        {
            float fcurradius = gap_radius * inum;
            // 执行 radiusSearch
            size_t found = index.radiusSearch(&query.x, fcurradius * fcurradius, ret_matches, params);

            for (size_t j = 0; j < found; ++j)
            {
                size_t neighbor_idx = ret_matches[j].first;
                if (neighbor_idx == i) continue;
                float distance = std::sqrt(ret_matches[j].second);

                gp_Pnt startP0(query.x, query.y, 0), endP0(query.x + directions[i].x, query.y + directions[i].y, 0);
                gp_Vec vec0 = gp_Vec(startP0, endP0).Normalized();
                gp_Dir dirction1(vec0 / vec0.Magnitude()); //方向 
                gp_Pnt startP1(query.x, query.y, 0), endP1(points[neighbor_idx].x, points[neighbor_idx].y, 0);
                gp_Vec vec2 = gp_Vec(startP1, endP1).Normalized();
                gp_Dir dirction2(vec2 / vec2.Magnitude()); //方向 
                float dotProduct = (float)dirction1.Dot(dirction2); // 两个向量的点乘 
                float theta = acos(dotProduct) * (180.0 / M_PI);

                // 条件过滤
                if (theta <= max_angle_degrees
                    && theta >= -max_angle_degrees)//&& distance <= max_distance
                {
                    //if (angle_rad <= max_angle_rad && distance <= max_distance) {
                    filtered_neighbors.emplace_back(neighbor_idx, distance, abs(theta));
                }
            }
            if (found != 0 && filtered_neighbors.size() != 0)
                break;
        }
        // 按Distance排序
        std::sort(filtered_neighbors.begin(), filtered_neighbors.end(), compareByDistance);
        // 提取索引并标记父邻点
        std::vector<size_t> sorted_indices;
        bool bselect = false;
        for (const auto& n : filtered_neighbors)
        {
            if (false == bselect)
            {
                bselect = true;
                sorted_indices.push_back(n.index);
            }
            hasParent0[n.index] = i; // 被别人选为邻居了
        }
        if (sorted_indices.size() <= 0)
        {// 按angle排序
            std::sort(filtered_neighbors.begin(), filtered_neighbors.end(), compareByAngle);
            bselect = false;
            for (const auto& n : filtered_neighbors)
            {
                if (false == bselect)
                {
                    bselect = true;
                    sorted_indices.push_back(n.index);
                }
                hasParent0[n.index] = i; // 被别人选为邻居了
            }
        }
        neighborChains[i] = sorted_indices;
    }
    return neighborChains;
}

std::vector<std::vector<size_t>> BuildChainsFromRoots(const std::vector<std::vector<size_t>>& neighborChains,
    const std::vector<size_t>& rootIndices,
    std::vector<int>& hasParent0)
{
    std::vector<std::vector<size_t>> pointChains;
    std::unordered_set<size_t> visited;
    std::vector<int>  haschain;
    haschain.assign(hasParent0.size(), 0);
    /*
    for (size_t root : rootIndices)
    {
        if (visited.count(root)) 
            continue;
        size_t current = root;
        for (int i = 0; i < hasParent0.size(); i++)
        {
            hasParent0[i] = ;
        }
    }
    */

    for (size_t root : rootIndices) 
    {
        if (visited.count(root)) 
            continue;

        std::vector<size_t> chain;
        std::unordered_set<size_t> local_visited;

        size_t current = root;
        while (true) 
        {
            if (local_visited.count(current)) 
                break; // 防止环
            if (1 != haschain[current])
            {//防止重复堆积
                chain.push_back(current);
                haschain[current] = 1;
            }
            visited.insert(current);
            local_visited.insert(current);

            if (neighborChains[current].empty()) 
                break;

            bool found_next = false;
            for (size_t next : neighborChains[current]) 
            {
                if (local_visited.find(next) == local_visited.end()) 
                {
                    current = next;
                    found_next = true;
                    break;
                }
            }
            if (!found_next) 
                break;
        }
        if (!chain.empty()) 
        {
            pointChains.push_back(chain);
        }
    }
    return pointChains;
}


void PointsShape::Sort()
{
    std::vector<Point_2D> points;
    int icount = m_path.ElementCount();
    for (int i = 0; i < icount; i++)
    {
        gp_Pnt aele = m_path.ElementAt(i);
        Point_2D apts;
        apts.x = aele.X();
        apts.y = aele.Y();
        points.push_back(apts);
    }
    // 使用方式：
    std::sort(points.begin(), points.end(), comparePoints);
    // 使用 unique 去重
    auto last = std::unique(points.begin(), points.end());
    points.erase(last, points.end()); 
    m_path.Clear();
    for (int i = 0; i < points.size(); i++)
    {
        m_path.AddPoint(gp_Pnt(points[i].x, points[i].y, 0));
    }
}

void PointsShape::ClusterPointCloud(double distanceThreshold, double dk)
{
#if defined USE_AI
    gp_Path& path = getpath();
    size_t numPoints = path.getpoints().size();
    if (numPoints <= 3)
        return;
    arma::mat points(2, numPoints);
    for (size_t i = 0; i < numPoints; ++i)
    {
        points(0, i) = path.getpoints()[i].X(); // 第一行是 x 坐标
        points(1, i) = path.getpoints()[i].Y(); // 第二行是 y 坐标
    }
    // 对点云进行分类
    auto clusters = mlpackclass::ClusterPointCloud_(points, distanceThreshold, dk);
    for (int i = 0; i < m_paths.size(); i++)
        m_paths[i].Clear();
    m_paths.clear();
    for (const auto& [clusterId, clusterPoints] : clusters)
    {
        // 提取 x 和 y 坐标
        gp_Path apath;
        m_paths.push_back(apath);
        for (size_t i = 0; i < clusterPoints.size(); ++i)
        {
            m_paths[m_paths.size() - 1].AddPoint(gp_Pnt(points(0, clusterPoints[i]), points(1, clusterPoints[i]), 0));
        }
    }
#endif
}

void PointsShape::SortPointsA(int idirectionsx,int idirectionsy)
{
    Sort();
    std::vector<Point_2D> points;
  // 创建点集
  //  PointCloud points; 
    int icount = m_path.ElementCount();
    for (int i = 0; i < icount; i++)
    {
        gp_Pnt aele = m_path.ElementAt(i);
        Point_2D apts;
        apts.x = aele.X();
        apts.y = aele.Y();
        points.push_back(apts);
    } 

     

    std::vector<Vector2> directions;

    for (const auto& p : points) 
    {
        Vector2 v(idirectionsx, idirectionsy); // 方向向量增量
        directions.push_back(v);
    }

    auto clusters = FindKNearestAndSort(points, 2, directions);


    // 对点云进行分类
    for (int i = 0; i < m_paths.size(); i++)
        m_paths[i].Clear();
    m_paths.clear();
    for (size_t i = 0; i < clusters.size(); ++i)
    {
        // 提取 x 和 y 坐标
        gp_Path apath;
        m_paths.push_back(apath);
        for (size_t j = 0; j < clusters[i].size(); ++j)
        {
            gp_Pnt aele = m_path.ElementAt(clusters[i][j]);
            m_paths[m_paths.size() - 1].AddPoint(aele);
        }
    }
}
void PointsShape::SortPoints(int idirectionsx, int idirectionsy, int idisgap,int ianglescale)
{ 
    float fdist = sqrt(idirectionsx * idirectionsx + idirectionsy * idirectionsy);
    Sort();
    // 创建点集
    //  PointCloud points; 
    int icount = m_path.ElementCount();
    std::vector<Point_2D> points(icount);
    for (int i = 0; i < icount; i++)
    {
        gp_Pnt aele = m_path.ElementAt(i);
        Point_2D apts;
        apts.x = aele.X();
        apts.y = aele.Y();
        points[i] = apts;
    }

    std::vector<Vector2> directions;
    for (const auto& p : points) {
        Vector2 v(idirectionsx, idirectionsy);// (-p.y, p.x); // 假设法线方向垂直于点本身
        directions.push_back(v.normalized());
    }

    std::vector<int> hasParent0;
    
   // auto neighborChains = FindAngleAndDistanceFilteredNeighbors(points, directions, 10, 90.0f, fdist, hasParent0);
   // auto neighborChains = FindAngleAndDistanceFilteredNeighbors_Radius(points, directions, fdist, 90.0f, 100, hasParent0);
    auto neighborChains = FindAngleAndDistanceFilteredNeighbors_adaptive_Radius(points, directions, idisgap, fdist, ianglescale, hasParent0);
    auto rootIndices = GetRootIndices(hasParent0);
    auto pointChains = BuildChainsFromRoots(neighborChains, rootIndices, hasParent0);

    //std::cout << "Root Indices:\n";
    //for (size_t r : rootIndices)
    //    std::cout << r << " ";
    //std::cout << "\n";

    //std::cout << "Point Chains:\n";
    //for (const auto& chain : pointChains) {
    //    for (size_t idx : chain)
    //        std::cout << idx << " ";
    //    std::cout << "\n";
    //}

    // 对点云进行分类
    for (int i = 0; i < m_paths.size(); i++)
        m_paths[i].Clear();
    m_paths.clear();

    for (const auto& chain : pointChains) {
        // 提取 x 和 y 坐标
        gp_Path apath;
        m_paths.push_back(apath);
        for (int idx=0;idx<chain.size();idx++)
        {
            gp_Pnt aele = m_path.ElementAt(chain[idx]);
            m_paths[m_paths.size() - 1].AddPoint(aele);
        }
    }
}
void PointsShape::getSubPixelEdge(void* pimg)
{

}
void PointsShape::ClusterPointsXX(std::vector<PointsShape>& seekpoints)
{
    // 创建点集
    PointCloud cloud;

    int icount = m_path.ElementCount(); 
    for (int i = 0; i < icount; i++)
    {
        gp_Pnt aele = m_path.ElementAt(i);
        Point_2D apts;
        apts.x = aele.X();
        apts.y = aele.Y(); 
        cloud.pts.push_back(apts);
    }

     // 构建 kd-tree
     using KDTree = nanoflann::KDTreeSingleIndexAdaptor<
       nanoflann::L2_Simple_Adaptor<float, PointCloud>,
       PointCloud,2>;

     KDTree index(2, cloud, {});
     index.buildIndex();

     // 创建 map: Point -> index
     std::map<std::pair<float, float>, int> pointToIndex;
     for (size_t i = 0; i < cloud.pts.size(); ++i) 
     {
          pointToIndex[{cloud.pts[i].x, cloud.pts[i].y}] = static_cast<int>(i);
     }

     // 查询某个点的索引
     if (0)
     {
          auto it = pointToIndex.find({ 1.0f, 1.0f });
          if (it != pointToIndex.end())
          {
              std::cout << "Point (1,1) has index: " << it->second << std::endl;
          } 
     }

        // 进行最近邻搜索
        const Point_2D queryPt = { 1.1f, 1.1f };
        size_t ret_index;
        float out_dist_sqr;
        nanoflann::KNNResultSet<float> resultSet(1);
        resultSet.init(&ret_index, &out_dist_sqr);

        //，&query_point.x 就是指向该点 (x, y) 坐标数组的指针
        index.findNeighbors(resultSet, &queryPt.x, nanoflann::SearchParameters());

        std::cout << "Nearest neighbor index: " << ret_index
            << ", distance: " << std::sqrt(out_dist_sqr) << std::endl;
}


void PointsShape::ClusterPoints(double max_distance) 
{
    // 构建 kd-tree
    PointCloud2D cloud; 
    gp_Path& path = getpath();
    size_t numPoints = path.getpoints().size(); 
    for (size_t i = 0; i < numPoints; ++i)
    {
        Point_2D apoint(path.getpoints()[i].X(), path.getpoints()[i].Y());
        cloud.pts.push_back(apoint);
    }
     
    int num_points = cloud.pts.size();
    // 存储每个点的邻居信息
    std::unordered_map<int, std::vector<int>> neighborsMap;

    MyKdTree2D index(2, cloud, KDTreeSingleIndexAdaptorParams(10));
    index.buildIndex(); 
    for (int i = 0; i < num_points; ++i)
    {
        const Point_2D& query = cloud.pts[i];
        // radiusSearch 返回的是 pair<index, squared distance>
        std::vector<ResultItem<unsigned int, float>> ret_matches;
        nanoflann::SearchParameters params;
 
        // 执行 radiusSearch
        int found = index.radiusSearch(&query.x, max_distance * max_distance, ret_matches, params);

        for (int j = 0; j < found; ++j)
        {
            int neighbor_idx = ret_matches[j].first;
            if (neighbor_idx == i) continue;
            float distance = std::sqrt(ret_matches[j].second);
            neighborsMap[i].push_back(neighbor_idx); 
        } 
    }
    // 开始聚类
    std::unordered_map<int, std::vector<int>> clusters;
    std::vector<int> pointLabels(cloud.pts.size(), -1); // 每个点的标签
    int clusterId = 0;
    for (int i = 0; i < cloud.pts.size(); ++i)
    {
        if (pointLabels[i] != -1) continue; // 已经被分配到某个聚类中
        // BFS 进行聚类
        std::vector<int> queue;
        queue.push_back(i);
        pointLabels[i] = clusterId;

        while (!queue.empty())
        {
            int currentPoint = queue.back();
            queue.pop_back();

            for (int neighbor : neighborsMap[currentPoint])
            {
                if (pointLabels[neighbor] == -1) // 尚未被分配
                {
                    pointLabels[neighbor] = clusterId;
                    queue.push_back(neighbor);
                }
            }
        }

        // 添加到聚类结果
        std::vector<int> clusterPoints;
        for (int j = 0; j < cloud.pts.size(); ++j)
        {
            if (pointLabels[j] == clusterId)
            {
                clusterPoints.push_back(j);
            }
        }
        clusters[clusterId] = clusterPoints;

        // 更新聚类ID
        ++clusterId;
    }
    //return clusters;
    for (int i = 0; i < m_paths.size(); i++)
        m_paths[i].Clear();
    m_paths.clear();
    for (const auto& [clusterId, clusterPoints] : clusters)
    {
        // 提取 x 和 y 坐标
        gp_Path apath;
        m_paths.push_back(apath);
        for (int i = 0; i < clusterPoints.size(); ++i)
        {
            m_paths[m_paths.size() - 1].AddPoint(gp_Pnt(cloud.pts[clusterPoints[i]].x, cloud.pts[clusterPoints[i]].y,0));
        }
    }

     
}
void PointsShape::FilterPoints(double max_distance, double filter_points)
{
    ClusterPoints(max_distance);
    gp_Path& apath = getpath();
    apath.Clear();
    for (int i = 0; i < m_paths.size(); i++)
    {
        gp_Path& path = m_paths[i];
        size_t numPoints = path.getpoints().size();
        if (filter_points < numPoints)
        {
            apath.AddPath(path);
        }
    } 
    for (int i = 0; i < m_paths.size(); i++)
        m_paths[i].Clear();
    m_paths.clear();
}
 


 
#include <utility> // for std::pair
#include <limits>

 

// --- 预定义的固定高斯权重数组 ---
// 这些权重是根据高斯函数计算并归一化得到的。
// 为了清晰，这里列出 n=5 和 n=7 的权重 (中心对称)。

// n=5 的高斯权重 (假设 sigma 使得权重集中在中心)
// 例如，使用 sigma ≈ 1.0, 计算后归一化:
const std::vector<float> GAUSSIAN_WEIGHTS_5 = {
    0.05448868f,  // 边缘
    0.24420134f,  // 次边缘
    0.40261996f,  // 中心
    0.24420134f,  // 次边缘
    0.05448868f   // 边缘
    // 总和 ≈ 1.0
};

// n=7 的高斯权重 (假设 sigma ≈ 1.5, 计算后归一化)
// 例如，使用 sigma ≈ 1.5, 计算后归一化:
const std::vector<float> GAUSSIAN_WEIGHTS_7 = {
    0.0185557f,   // 边缘
    0.0745056f,   // 次边缘
    0.1857117f,   // 次中心
    0.2424539f,   // 中心
    0.1857117f,   // 次中心
    0.0745056f,   // 次边缘
    0.0185557f    // 边缘
    // 总和 ≈ 1.0
};






// --- 核心函数：处理整个点数组，返回拐点索引 ---
// 返回值: std::vector<std::pair<int, char>> (索引, 'P' 或 'V')
// 参数:
//   points: 输入的整个点数组
//   n: 高斯平滑窗口大小 (必须是 5 或 7)
//   look_range: 拐点检测邻域范围
Point_2D findInflectionPoints(const std::vector<Point_2D>& points,
    int n = 5, // 默认使用 n=5
    int look_range = 1,
    double danglegaprate = 0.1)
{
    // 输入验证：n 必须是 5 或 7
    if (n != 5 && n != 7) {
        std::cerr << "Error: Window size n must be 5 or 7.\n";
        return {};
    }
    if (points.size() < static_cast<size_t>(2 * n) || look_range < 1) {
        return {}; // 点不够或参数无效
    }



    //   (x值)
    std::vector<double> nosmoothed_xs;
    double nosmoothed_x = 0.0f;
    nosmoothed_xs.reserve(points.size());
    for (size_t i = 0; i < points.size(); ++i) {
        nosmoothed_x = points[i].x;
        nosmoothed_xs.push_back(nosmoothed_x);
    }
    //    (y值)
    std::vector<double> nosmoothed_ys;
    double nosmoothed_y = 0.0f;
    nosmoothed_ys.reserve(points.size());
    for (size_t i = 0; i < points.size(); ++i) {
        nosmoothed_y = points[i].y;
        nosmoothed_ys.push_back(nosmoothed_y);
    }

    std::vector<double>::iterator XmaxNum = std::max_element(nosmoothed_xs.begin(), nosmoothed_xs.end());
    int XindexMax = distance(nosmoothed_xs.begin(), XmaxNum);


    std::vector<double>::iterator YmaxNum = std::max_element(nosmoothed_ys.begin(), nosmoothed_ys.end());
    int YindexMax = distance(nosmoothed_ys.begin(), YmaxNum);



    std::vector<double>::iterator XminNum = std::min_element(nosmoothed_xs.begin(), nosmoothed_xs.end());
    int XindexMin = distance(nosmoothed_xs.begin(), XminNum);


    std::vector<double>::iterator YminNum = std::min_element(nosmoothed_ys.begin(), nosmoothed_ys.end());
    int YindexMin = distance(nosmoothed_ys.begin(), YminNum);



    Point_2D agetpoint;
    if (XindexMax > n && XindexMax < points.size() - n)
    {
        agetpoint = points[XindexMax];
        return agetpoint;
    }
    if (YindexMax > n && YindexMax < points.size() - n)
    {
        agetpoint = points[YindexMax];
        return agetpoint;
    }
    if (XindexMin > n && XindexMin < points.size() - n)
    {
        agetpoint = points[XindexMin];
        return agetpoint;
    }
    if (YindexMin > n && YindexMin < points.size() - n)
    {
        agetpoint = points[YindexMin];
        return agetpoint;
    }


    if (1)
    {
        // 2. 选择预定义的高斯权重
        const std::vector<float>& gaussian_weights =
            (n == 5) ? GAUSSIAN_WEIGHTS_5 : GAUSSIAN_WEIGHTS_7;

        // 3. 对整个序列进行高斯平滑 (y值)
        std::vector<float> smoothed_ys;
        smoothed_ys.reserve(points.size());

        // 使用滑动窗口进行平滑
        for (size_t i = 0; i < points.size(); ++i) {
            // 检查窗口是否能填满 n 个点
            if (i < static_cast<size_t>(n - 1)) {
                // 窗口未满，无法平滑，填充 0.0f
                smoothed_ys.push_back(0.0f);
                continue;
            }

            // 计算当前窗口 [i-n+1, i] 的平滑值
            float smoothed_y = 0.0f;
            for (int j = 0; j < n; ++j) {
                size_t Pointidx = i - n + 1 + j; // 原始数组中的索引
                smoothed_y += points[Pointidx].y * gaussian_weights[j];
            }
            smoothed_ys.push_back(smoothed_y);
        }
         
        // 3.1 对整个序列进行高斯平滑 (x值)
        std::vector<float> smoothed_xs;
        smoothed_xs.reserve(points.size());

        // 使用滑动窗口进行平滑
        for (size_t i = 0; i < points.size(); ++i) {
            // 检查窗口是否能填满 n 个点
            if (i < static_cast<size_t>(n - 1)) {
                // 窗口未满，无法平滑，填充 0.0f
                smoothed_xs.push_back(0.0f);
                continue;
            }

            // 计算当前窗口 [i-n+1, i] 的平滑值
            float smoothed_x = 0.0f;
            for (int j = 0; j < n; ++j) {
                size_t Pointidx = i - n + 1 + j; // 原始数组中的索引
                smoothed_x += points[Pointidx].x * gaussian_weights[j];
            }
            smoothed_xs.push_back(smoothed_x);
        }
 
        // 4. 在平滑后的序列中检测拐点 (局部极大值和极小值)
        Point_2D inflection_points;

        std::vector <gp_Dir> vector_dir;
        // 从索引 (n-1) 开始，smoothed_ys smoothed_xs 才有有效值
        for (size_t k = n - 1; k < points.size() - 1; ++k)
        {
            /*
            float current_x0 = 0;
            float current_y0 = 0;
            float current_x1 = 0;
            float current_y1 = 0;
            float fnum = 0;
            for (int i = 0; i < ianglegap / 2; i++)
            {
                current_x0 = current_x0 + smoothed_xs[k - i];
                current_y0 = current_y0 + smoothed_ys[k - i];
                fnum = fnum + 1;
            }
            current_x0 = current_x0 / fnum;
            current_y0 = current_y0 / fnum;
            fnum = 0;
            for (int i = 1; i < ianglegap / 2; i++)
            {
                current_x1 = current_x1 + smoothed_xs[k + i];
                current_y1 = current_y1 + smoothed_ys[k + i];
                fnum = fnum + 1;
            }
            current_x1 = current_x1 / fnum;
            current_y1 = current_y1 / fnum;
            */
            float current_x0 = smoothed_xs[k];
            float current_y0 = smoothed_ys[k];

            float current_x1 = smoothed_xs[k + 1];
            float current_y1 = smoothed_ys[k + 1];

            gp_Pnt startP0(current_x0, current_y0, 0),
                endP0(current_x1, current_y1, 0);

            gp_Vec vec = gp_Vec(startP0, endP0).Normalized();
            gp_Dir dirction1(vec / vec.Magnitude()); //方向
            vector_dir.push_back(dirction1);
        }

        std::vector <float> vector_dist;
        // 从索引 (n-1) 开始，smoothed_ys smoothed_xs 才有有效值
        for (size_t k = n - 1; k < points.size() - 1; ++k)
        {
            float current_x0 = smoothed_xs[k];
            float current_y0 = smoothed_ys[k];

            float current_x1 = smoothed_xs[k + 1];
            float current_y1 = smoothed_ys[k + 1];

            float dx = current_x1 - current_x0;
            float dy = current_y1 - current_y0;
            float distance = std::sqrt(dx * dx + dy * dy);

            vector_dist.push_back(distance);
        }

        //
        std::vector <float> vector_angle;
        for (int ik = 0; ik < vector_dir.size() - 1; ++ik)
        {
            gp_Dir dirction1 = vector_dir[ik];
            gp_Dir dirction2 = vector_dir[ik + 1];
            float dotProduct = (float)dirction1.Dot(dirction2); // 两个向量的点乘 
            float theta = acos(dotProduct) * (180.0 / M_PI);

            vector_angle.push_back(theta);// 角度（弧度）
        }
        float fmaxvalue = 0;
        int imaxnum = 0;
        float faddmaxvalue = 0;
        int ianglenum = danglegaprate * vector_angle.size();
        for (int it = 0; it < vector_angle.size() - ianglenum; it++)
        {
            if (abs(vector_angle[it + ianglenum] - vector_angle[it]) > fmaxvalue)
            {
                fmaxvalue = abs(vector_angle[it + ianglenum] - vector_angle[it]);

                float faddvalue = 0;
                for (int ik = it; ik < it + ianglenum; ik++)
                {
                    float distance = vector_dist[ik];
                    faddvalue = faddvalue + vector_angle[ik] * distance;
                }
                if (faddvalue > faddmaxvalue)
                {
                    faddmaxvalue = faddvalue;
                    imaxnum = it;
                }
            }

        }
        if (imaxnum >= 0 && (imaxnum + ianglenum) < points.size())
            inflection_points = points[imaxnum + ianglenum];
     
        return inflection_points;
    }
    return agetpoint;
}

  
void PointsShape::FindCrossPoints(void* points)
{
    PointsShape* tpoints = (PointsShape*)points;
    if (tpoints == nullptr)
        return;
    gp_Path& path = getpath();
    size_t numPoints = path.getpoints().size();
    if (numPoints < 2)
        return;
    std::vector<Point_2D> points2d;
    for (size_t i = 0; i < numPoints; ++i)
    {
        Point_2D apoint(path.getpoints()[i].X(), path.getpoints()[i].Y());
        double dvaluex0 = apoint.x ; 
        double dvaluey0 = apoint.y;
        points2d.push_back(apoint);
    }
    Point_2D tpoint2d = findInflectionPoints(points2d, 5, 1); // n=5, look_range=1

    tpoints->clear();
    tpoints->addpoint(points2d[0].x, points2d[0].y);
    tpoints->addpoint(tpoint2d.x, tpoint2d.y);
    tpoints->addpoint(tpoint2d.x, tpoint2d.y);
    tpoints->addpoint(points2d[points2d.size()-1].x, points2d[points2d.size() - 1].y);
 
}
TwoPointsShape::TwoPointsShape():
m_insidewidth(2),
m_movoffset(0),
m_iheadtail(0)
{
    m_ishow = 4;
}
void TwoPointsShape::clear()
{
    m_linemap.clear(); 
    m_path.Clear();
}
void TwoPointsShape::makepath(int ivh)
{
    m_path.Clear();

    if (0 == ivh)
    {
        for (const auto& entry : m_linemap)
        {
            const int ix = entry.first;
            const gp_Pnt& range = entry.second;
            const double centerY = (range.X() + range.Y()) / 2.0;
            m_path.AddPoint(gp_Pnt(ix, centerY, 0));
        }
    }
    else if (1 == ivh)
    {
        for (const auto& entry : m_linemap)
        {
            const int iy = entry.first;
            const gp_Pnt& range = entry.second;
            const double centerX = (range.X() + range.Y()) / 2.0;
            m_path.AddPoint(gp_Pnt(centerX, iy, 0));
        }
    }
}
void TwoPointsShape::addpoint(int ivaluea ,int ivalueb)
{
    gp_Pnt resultpoint(0, 0, 0);
    const auto found = m_linemap.find(ivaluea);
    if (found != m_linemap.end())
    {
        resultpoint = found->second;
    }

    const int imin = static_cast<int>(resultpoint.X());
    const int imax = static_cast<int>(resultpoint.Y());
    if (0 == imin || ivalueb < imin)
        resultpoint.SetX(ivalueb);
    else
        resultpoint.SetX(imin);
    if (0 == imax || ivalueb > imax)
        resultpoint.SetY(ivalueb);
    else
        resultpoint.SetY(imax);
    m_linemap[ivaluea] = resultpoint;

}
int TwoPointsShape::size()
{
    return static_cast<int>(m_linemap.size());
}
void TwoPointsShape::drawshape(gp_Path& painter)
{
    painter.AddPath(m_path);
}
void TwoPointsShape::setedgeoi(int insidewidth,int offset,int iheadtail)
{
    m_insidewidth = insidewidth;
    m_movoffset = offset;
    m_iheadtail = iheadtail;
}
void TwoPointsShape::edgeimage(cv::Mat& aImage,int itype)
{
    const int inum = m_iheadtail > 0 ? m_iheadtail : -m_iheadtail;
    const int icount = static_cast<int>(m_path.ElementCount());
    if (0 == icount || aImage.empty())
    {
        return;
    }

    const int startIndex = (icount > (inum * 2)) ? inum : 0;
    const int endIndex = (icount > (inum * 2)) ? (icount - inum) : icount;

    switch (itype)
    {
    case 0:
    case 2:
    {
        const cv::Vec3b primary = (itype == 0) ? cv::Vec3b(255, 255, 255) : cv::Vec3b(0, 0, 0);
        const cv::Vec3b secondary = (itype == 0) ? cv::Vec3b(0, 0, 0) : cv::Vec3b(255, 255, 255);
        for (int i = startIndex; i < endIndex; ++i)
        {
            const gp_Pnt point = m_path.ElementAt(i);
            const int px = static_cast<int>(point.X());
            const int py = static_cast<int>(point.Y());
            for (int offset = 0; offset < m_insidewidth; ++offset)
            {
                const int targetY = py + offset + m_movoffset;
                if (targetY >= 0 && targetY < aImage.rows && px >= 0 && px < aImage.cols)
                    aImage.at<cv::Vec3b>(targetY, px) = primary;
            }
            for (int offset = 1; offset < m_insidewidth; ++offset)
            {
                const int targetY = py - offset + m_movoffset;
                if (targetY >= 0 && targetY < aImage.rows && px >= 0 && px < aImage.cols)
                    aImage.at<cv::Vec3b>(targetY, px) = secondary;
            }
        }
    }
    break;
    case 1:
    case 3:
    {
        const cv::Vec3b primary = (itype == 1) ? cv::Vec3b(0, 0, 0) : cv::Vec3b(255, 255, 255);
        const cv::Vec3b secondary = (itype == 1) ? cv::Vec3b(255, 255, 255) : cv::Vec3b(0, 0, 0);
        for (int i = startIndex; i < endIndex; ++i)
        {
            const gp_Pnt point = m_path.ElementAt(i);
            const int px = static_cast<int>(point.X());
            const int py = static_cast<int>(point.Y());
            for (int offset = 0; offset < m_insidewidth; ++offset)
            {
                const int targetX = (itype == 1) ? px + m_movoffset + offset : px + m_movoffset - offset;
                if (targetX >= 0 && targetX < aImage.cols && py >= 0 && py < aImage.rows)
                    aImage.at<cv::Vec3b>(py, targetX) = primary;
            }
            for (int offset = 1; offset < m_insidewidth; ++offset)
            {
                const int targetX = (itype == 1) ? px + m_movoffset - offset : px + m_movoffset + offset;
                if (targetX >= 0 && targetX < aImage.cols && py >= 0 && py < aImage.rows)
                    aImage.at<cv::Vec3b>(py, targetX) = secondary;
            }
        }
    }
        break;
    default:
        break;
    }
}
void TwoPointsShape::Move(int ix, int iy)
{
    // 平移路径
    gp_Vec translationVector(ix, iy, 0); // 沿X轴平移5，沿Y轴平移3，沿Z轴平移2
    m_path.Translate(translationVector);

}
void TwoPointsShape::Rotate(double dangle)
{
    gp_Pnt apoint(0, 0, 0);
    m_path.RotateAroundPoint(apoint, dangle);
}
void TwoPointsShape::Zoom(double dx0, double dy0)
{
    gp_Pnt apoint(0, 0, 0);
    m_path.ScaleAroundPoint(apoint, dx0, dy0);
} 
void RectsShape::setcolor(int ir, int ig, int ib)
{
    m_color = Quantity_Color(ir / 255.0, ig / 255.0, ib / 255.0, Quantity_TypeOfColor::Quantity_TOC_RGB);
 
    m_path.setcolor(ir, ig, ib);
}
void RectsShape::addrect(gp_Rectangle& arect)
{
    m_rects.push_back(arect);
}
void RectsShape::addrect(gp_Rectangle& arect, std::string& astring) 
{
    m_rects.push_back(arect);
    m_strlist.push_back(astring);
}
void RectsShape::clear()
{
    m_rects.clear();
    m_strlist.clear();
    m_angles.clear();
    m_path.Clear();
}
void RectsShape::setrect(int inum,int ix,int iy,int iw,int ih)
{
    if (inum < 0)
        return;
    const size_t index = static_cast<size_t>(inum);
    if (index >= m_rects.size())
        return;

    m_rects[index] = gp_Rectangle(gp_Pnt(ix, iy, 0), gp_Pnt(ix + iw, iy + ih, 0));
}
void RectsShape::MakeShape()
{
    m_path.Clear(); 
    for (int i = 0; i < m_rects.size(); i++)
    {
        if (m_ispecshow >= 0 && i != m_ispecshow)
            continue;
        m_path.AddRect(m_rects[i]); 
    }
    m_path.MakeRectsShape();
}
void RectsShape::MakeShape(int inum)
{
    m_path.Clear();
    //for (int i = 0; i < m_rects.size(); i++)
    
    if (inum == -1)
    {
        if(m_rects.size()>0)
        m_path.AddRect(m_rects[m_rects.size()-1]);
    }
    else
    if(inum< m_rects.size())
    {
        m_path.AddRect(m_rects[inum]);
    }
    m_path.MakeRectsShape();
}
void RectsShape::MakePointShape()
{
    m_path.MakePointShape();
}

Area::Area(RectType type)
    : gp_Rectangle(gp_Pnt(0, 0, 0), gp_Pnt(0, 0, 0)),
      m_id(0),
      m_type(type),
      m_name()
{
}

QAreas::QAreas(RectsShape& rects)
    : m_rects(rects)
{
}

void QAreas::GenMap()
{
    m_areas.clear();
    const int rectCount = m_rects.size();
    for (int i = 0; i < rectCount; ++i)
    {
        Area area(Area::orgarea);
        area.setid(i + 1);
        m_areas.push_back(area);
    }
}

void QAreas::relation()
{
    if (m_areas.empty())
    {
        GenMap();
    }
}

void QAreas::regroup()
{
    if (m_areas.empty())
    {
        GenMap();
    }
}

void QAreas::sort()
{
    std::sort(m_areas.begin(), m_areas.end(),
        [](Area& left, Area& right)
        {
            return left.ID() < right.ID();
        });
}
