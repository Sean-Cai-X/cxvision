#include "pch.h"
#include "CircleShape.h"

CircleShape::CircleShape()
    : m_cx(0.0), m_cy(0.0), m_radius(50.0), m_innerRadius(0.0)
{
}

CircleShape::CircleShape(double cx, double cy, double radius, double inner_radius)
    : m_cx(cx), m_cy(cy), m_radius(std::max(1.0, radius)), m_innerRadius(inner_radius)
{
}

void CircleShape::setCenter(double cx, double cy)
{
    m_cx = cx;
    m_cy = cy;
}

void CircleShape::setRadius(double r)
{
    m_radius = std::max(1.0, r);
}

void CircleShape::setInnerRadius(double r)
{
    m_innerRadius = std::max(0.0, std::min(r, m_radius - 1.0));
}

CxShapeHit CircleShape::hitTest(double x, double y, double tolerance) const
{
    const double dx = x - m_cx;
    const double dy = y - m_cy;
    const double dist_sq = dx * dx + dy * dy;
    const double t_sq = tolerance * tolerance;

    if (dist_sq <= t_sq)
        return { true, CxShapeHandleRole::Center, -1, std::sqrt(dist_sq) };

    const double dist = std::sqrt(dist_sq);
    const double radius_dist = std::abs(dist - m_radius);
    
    if (radius_dist <= tolerance)
        return { true, CxShapeHandleRole::Radius, -1, radius_dist };

    if (m_innerRadius > 0)
    {
        const double inner_dist = std::abs(dist - m_innerRadius);
        if (inner_dist <= tolerance)
            return { true, CxShapeHandleRole::InnerRadius, -1, inner_dist };
    }

    if (dist >= m_innerRadius - tolerance && dist <= m_radius + tolerance)
        return { true, CxShapeHandleRole::Body, -1, std::min(std::abs(dist - m_innerRadius), std::abs(dist - m_radius)) };

    return {};
}

void CircleShape::enumerateHandles(std::vector<CxShapeHandle>& out) const
{
    out.push_back({ CxShapeHandleRole::Center, -1, { m_cx, m_cy }, "C" });
    out.push_back({ CxShapeHandleRole::Radius, -1, { m_cx + m_radius, m_cy }, "Rout" });
    
    if (m_innerRadius > 0)
        out.push_back({ CxShapeHandleRole::InnerRadius, -1, { m_cx + m_innerRadius, m_cy }, "Rin" });
}

void CircleShape::dragHandle(CxShapeHandleRole role, int vertex_index, double x, double y)
{
    (void)vertex_index;

    switch (role)
    {
    case CxShapeHandleRole::Center:
    case CxShapeHandleRole::Body:
        m_cx = x;
        m_cy = y;
        break;
    case CxShapeHandleRole::Radius:
    {
        const double dx = x - m_cx;
        const double dy = y - m_cy;
        const double r = std::max(1.0, std::sqrt(dx * dx + dy * dy));
        m_radius = r;
        if (m_innerRadius >= m_radius)
            m_innerRadius = std::max(0.0, m_radius - 1.0);
        break;
    }
    case CxShapeHandleRole::InnerRadius:
    {
        const double dx = x - m_cx;
        const double dy = y - m_cy;
        const double r = std::min(m_radius - 1.0, std::max(0.0, std::sqrt(dx * dx + dy * dy)));
        m_innerRadius = r;
        break;
    }
    default:
        break;
    }
}

void CircleShape::translateBy(double dx, double dy)
{
    m_cx += dx;
    m_cy += dy;
}

void CircleShape::exportPoints(std::vector<CxShapePoint>& out) const
{
    out.clear();
    out.push_back({ m_cx, m_cy });
    out.push_back({ m_cx + m_radius, m_cy });
}

void CircleShape::drawshape(gp_Path& painter)
{
    painter.AddCircle(gp_Pnt(m_cx, m_cy, 0), m_radius);
    
    if (m_innerRadius > 0)
        painter.AddCircle(gp_Pnt(m_cx, m_cy, 0), m_innerRadius);
}

bool CircleShape::exportCircle(CxShapePoint& center, double& radius, double& inner_radius) const
{
    center.x = m_cx;
    center.y = m_cy;
    radius = m_radius;
    inner_radius = m_innerRadius;
    return true;
}

bool CircleShape::snapshot(CxShapeGeometrySnapshot& out) const
{
    out.kind = CxShapeKind::Circle;
    out.points.clear();
    out.points.push_back({ m_cx, m_cy });
    out.points.push_back({ m_cx + m_radius, m_cy });
    out.center = { m_cx, m_cy };
    out.radius = m_radius;
    out.inner_radius = m_innerRadius;
    out.closed = true;
    return true;
}