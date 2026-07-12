#include "pch.h"
#include "EllipseShape.h"

EllipseShape::EllipseShape()
    : m_cx(0.0), m_cy(0.0), m_rx(50.0), m_ry(30.0)
{
}

EllipseShape::EllipseShape(double center_x, double center_y, double radius_x, double radius_y)
    : m_cx(center_x), m_cy(center_y),
      m_rx(std::max(1.0, radius_x)), m_ry(std::max(1.0, radius_y))
{
}

void EllipseShape::setCenter(double cx, double cy)
{
    m_cx = cx;
    m_cy = cy;
}

void EllipseShape::setRadiusX(double rx)
{
    m_rx = std::max(1.0, rx);
}

void EllipseShape::setRadiusY(double ry)
{
    m_ry = std::max(1.0, ry);
}

static double MinDistanceToSegment(
    double px, double py,
    double x0, double y0,
    double x1, double y1)
{
    const double dx = x1 - x0;
    const double dy = y1 - y0;
    const double len2 = dx * dx + dy * dy;

    if (len2 < 1.0e-12)
    {
        const double dx_p = px - x0;
        const double dy_p = py - y0;
        return std::sqrt(dx_p * dx_p + dy_p * dy_p);
    }

    double t = ((px - x0) * dx + (py - y0) * dy) / len2;
    t = std::max(0.0, std::min(1.0, t));

    const double near_x = x0 + t * dx;
    const double near_y = y0 + t * dy;
    const double dx_n = px - near_x;
    const double dy_n = py - near_y;

    return std::sqrt(dx_n * dx_n + dy_n * dy_n);
}

static double MinDistanceToClosedPolyline(
    const std::vector<CxShapePoint>& points,
    double x, double y)
{
    if (points.size() < 2)
        return std::numeric_limits<double>::max();

    double minDist = std::numeric_limits<double>::max();
    const int n = static_cast<int>(points.size());

    for (int i = 0; i < n; ++i)
    {
        const int j = (i + 1) % n;
        const double dist = MinDistanceToSegment(
            x, y,
            points[i].x, points[i].y,
            points[j].x, points[j].y);
        minDist = std::min(minDist, dist);
    }

    return minDist;
}

void EllipseShape::EnumerateBoundaryPoints(
    std::vector<CxShapePoint>& out,
    int segments) const
{
    out.clear();
    const int count = std::max(24, segments);

    for (int i = 0; i < count; ++i)
    {
        const double angle = 2.0 * PI * static_cast<double>(i) / static_cast<double>(count);
        out.push_back({
            m_cx + m_rx * std::cos(angle),
            m_cy + m_ry * std::sin(angle)
        });
    }
}

CxShapeHit EllipseShape::hitTest(double x, double y, double tolerance) const
{
    const double dx = x - m_cx;
    const double dy = y - m_cy;
    const double t_sq = tolerance * tolerance;

    if (dx * dx + dy * dy <= t_sq)
        return { true, CxShapeHandleRole::Center, -1, std::sqrt(dx * dx + dy * dy) };

    const double rx_handle_dist_sq = (x - (m_cx + m_rx)) * (x - (m_cx + m_rx)) + (y - m_cy) * (y - m_cy);
    if (rx_handle_dist_sq <= t_sq)
        return { true, CxShapeHandleRole::RadiusX, -1, std::sqrt(rx_handle_dist_sq) };

    const double ry_handle_dist_sq = (x - m_cx) * (x - m_cx) + (y - (m_cy + m_ry)) * (y - (m_cy + m_ry));
    if (ry_handle_dist_sq <= t_sq)
        return { true, CxShapeHandleRole::RadiusY, -1, std::sqrt(ry_handle_dist_sq) };

    std::vector<CxShapePoint> boundary;
    EnumerateBoundaryPoints(boundary, 96);
    const double boundaryDistance = MinDistanceToClosedPolyline(boundary, x, y);

    if (boundaryDistance <= tolerance)
    {
        return { true, CxShapeHandleRole::Body, -1, boundaryDistance };
    }

    return {};
}

void EllipseShape::enumerateHandles(std::vector<CxShapeHandle>& out) const
{
    out.push_back({ CxShapeHandleRole::Center, -1, { m_cx, m_cy }, "C" });
    out.push_back({ CxShapeHandleRole::RadiusX, -1, { m_cx + m_rx, m_cy }, "Rx" });
    out.push_back({ CxShapeHandleRole::RadiusY, -1, { m_cx, m_cy + m_ry }, "Ry" });
}

void EllipseShape::dragHandle(CxShapeHandleRole role, int vertex_index, double x, double y)
{
    (void)vertex_index;

    switch (role)
    {
    case CxShapeHandleRole::Center:
    case CxShapeHandleRole::Body:
        m_cx = x;
        m_cy = y;
        break;
    case CxShapeHandleRole::RadiusX:
    {
        const double rx = std::max(1.0, std::abs(x - m_cx));
        m_rx = rx;
        break;
    }
    case CxShapeHandleRole::RadiusY:
    {
        const double ry = std::max(1.0, std::abs(y - m_cy));
        m_ry = ry;
        break;
    }
    default:
        break;
    }
}

void EllipseShape::translateBy(double dx, double dy)
{
    m_cx += dx;
    m_cy += dy;
}

void EllipseShape::exportPoints(std::vector<CxShapePoint>& out) const
{
    out.clear();
    out.push_back({ m_cx, m_cy });
    out.push_back({ m_cx + m_rx, m_cy });
    out.push_back({ m_cx, m_cy + m_ry });
}

void EllipseShape::drawshape(gp_Path& painter)
{
    gp_Pnt p0(m_cx - m_rx, m_cy - m_ry, 0);
    gp_Pnt p1(m_cx + m_rx, m_cy + m_ry, 0);
    painter.AddRectangularEllipse(p0, p1);
}

bool EllipseShape::exportEllipse(CxShapePoint& center, double& radius_x, double& radius_y, double& angle) const
{
    center.x = m_cx;
    center.y = m_cy;
    radius_x = m_rx;
    radius_y = m_ry;
    angle = 0.0;
    return true;
}

bool EllipseShape::snapshot(CxShapeGeometrySnapshot& out) const
{
    out.kind = CxShapeKind::Ellipse;
    out.points.clear();
    out.points.push_back({ m_cx, m_cy });
    out.points.push_back({ m_cx + m_rx, m_cy });
    out.points.push_back({ m_cx, m_cy + m_ry });
    out.center = { m_cx, m_cy };
    out.radius_x = m_rx;
    out.radius_y = m_ry;
    out.angle = 0.0;
    out.closed = true;
    return true;
}