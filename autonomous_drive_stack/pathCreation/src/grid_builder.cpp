#include "prove/grid_builder.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace prove::planning {
namespace {
using Real = long double;

bool finite(Point2 p) { return std::isfinite(p.x) && std::isfinite(p.y); }
bool equal(Point2 a, Point2 b) { return a.x == b.x && a.y == b.y; }
Real cross(Point2 a, Point2 b, Point2 c) {
    return (Real(b.x) - a.x) * (Real(c.y) - a.y) - (Real(b.y) - a.y) * (Real(c.x) - a.x);
}
bool onSegment(Point2 a, Point2 b, Point2 p) {
    return cross(a, b, p) == 0 && p.x >= std::min(a.x, b.x) && p.x <= std::max(a.x, b.x) &&
           p.y >= std::min(a.y, b.y) && p.y <= std::max(a.y, b.y);
}
int sign(Real a) { return (a > 0) - (a < 0); }
bool intersects(Point2 a, Point2 b, Point2 c, Point2 d) {
    Real x = cross(a,b,c), y = cross(a,b,d), z = cross(c,d,a), w = cross(c,d,b);
    if ((x == 0 && onSegment(a,b,c)) || (y == 0 && onSegment(a,b,d)) ||
        (z == 0 && onSegment(c,d,a)) || (w == 0 && onSegment(c,d,b))) return true;
    return sign(x) * sign(y) < 0 && sign(z) * sign(w) < 0;
}

bool validPolygon(const Polygon& p) {
    if (p.size() < 4 || !equal(p.front(), p.back())) return false;
    for (Point2 v : p) if (!finite(v)) return false;
    const std::size_t n = p.size() - 1;
    Real twice_area = 0;
    for (std::size_t i = 0; i < n; ++i) {
        if (equal(p[i], p[i + 1])) return false;
        // Translation reduces cancellation when coordinates have a large origin.
        twice_area += (Real(p[i].x) - p[0].x) * (Real(p[i + 1].y) - p[0].y) -
                      (Real(p[i].y) - p[0].y) * (Real(p[i + 1].x) - p[0].x);
        const Point2 prev = p[(i + n - 1) % n], cur = p[i], next = p[i + 1];
        if (cross(prev, cur, next) == 0) {
            const Real dot = (Real(cur.x) - prev.x) * (Real(next.x) - cur.x) +
                             (Real(cur.y) - prev.y) * (Real(next.y) - cur.y);
            if (dot <= 0) return false;
        }
        for (std::size_t j = i + 1; j < n; ++j) {
            if (j == i + 1 || (i == 0 && j == n - 1)) continue;
            if (intersects(p[i], p[i + 1], p[j], p[j + 1])) return false;
        }
    }
    return twice_area != 0;
}

bool inside(Point2 point, const Polygon& p) {
    bool result = false;
    for (std::size_t i = 0; i + 1 < p.size(); ++i) {
        Point2 a = p[i], b = p[i + 1];
        if (onSegment(a, b, point)) return true;
        if ((a.y > point.y) != (b.y > point.y)) {
            const Real at_x = Real(a.x) + (Real(b.x) - a.x) * (Real(point.y) - a.y) / (Real(b.y) - a.y);
            if (Real(point.x) < at_x) result = !result;
        }
    }
    return result;
}

struct Square { double x0, y0, x1, y1; };
bool contains(Square s, Point2 p) { return p.x >= s.x0 && p.x <= s.x1 && p.y >= s.y0 && p.y <= s.y1; }
bool edgesTouch(const Polygon& p, Square s) {
    const Point2 a{s.x0,s.y0}, b{s.x1,s.y0}, c{s.x1,s.y1}, d{s.x0,s.y1};
    for (std::size_t i = 0; i + 1 < p.size(); ++i) {
        Point2 u = p[i], v = p[i + 1];
        if (contains(s,u) || contains(s,v) || intersects(u,v,a,b) || intersects(u,v,b,c) ||
            intersects(u,v,c,d) || intersects(u,v,d,a)) return true;
    }
    return false;
}
bool insideArea(Square s, const Polygon& p) {
    if (edgesTouch(p, s)) return false;
    return inside({s.x0,s.y0},p) && inside({s.x1,s.y0},p) &&
           inside({s.x1,s.y1},p) && inside({s.x0,s.y1},p);
}
bool hitsPolygon(Square s, const Polygon& p) {
    return edgesTouch(p,s) || inside({s.x0,s.y0},p) || inside({s.x1,s.y0},p) ||
           inside({s.x1,s.y1},p) || inside({s.x0,s.y1},p);
}
bool hitsCircle(Square s, CircleObstacle c) {
    const Real x = std::max(Real(s.x0), std::min(Real(c.center.x), Real(s.x1)));
    const Real y = std::max(Real(s.y0), std::min(Real(c.center.y), Real(s.y1)));
    const Real dx = x - c.center.x, dy = y - c.center.y;
    return dx * dx + dy * dy <= Real(c.radius) * c.radius;
}
} // namespace

CorridorResult closeCorridor(const std::vector<Point2>& left, const std::vector<Point2>& right) {
    CorridorResult out;
    if (left.size() < 2 || right.size() < 2) { out.reason = "need at least two points per boundary"; return out; }
    Polygon polygon = left;
    polygon.insert(polygon.end(), right.rbegin(), right.rend());
    polygon.push_back(polygon.front());
    if (!validPolygon(polygon)) { out.reason = "boundaries do not form a valid closed corridor"; return out; }
    out.success = true;
    out.polygon = std::move(polygon);
    return out;
}

GridBuildResult buildOccupancyGrid(const GridGeometry& g, const Polygon& drivable,
                                   const std::vector<CircleObstacle>& circles,
                                   const std::vector<Polygon>& polygons,
                                   const std::vector<BoxObstacle>& boxes) {
    GridBuildResult out;
    // A bound prevents a malformed message from requesting unbounded allocation.
    constexpr std::size_t max_cells = 10000000;
    if (!g.width || !g.height || g.width > max_cells / g.height ||
        !std::isfinite(g.resolution) || g.resolution <= 0 ||
        !std::isfinite(g.origin_x) || !std::isfinite(g.origin_y) ||
        !std::isfinite(g.origin_x + g.width * g.resolution) ||
        !std::isfinite(g.origin_y + g.height * g.resolution) ||
        g.origin_x + g.resolution == g.origin_x || g.origin_y + g.resolution == g.origin_y) {
        out.reason = "invalid grid geometry"; return out;
    }
    if (!validPolygon(drivable)) { out.reason = "invalid drivable polygon"; return out; }
    for (auto circle : circles) {
        if (!finite(circle.center) || !std::isfinite(circle.radius) || circle.radius <= 0) {
            out.reason = "invalid circle obstacle"; return out;
        }
    }
    for (const auto& polygon : polygons) {
        if (!validPolygon(polygon)) { out.reason = "invalid polygon obstacle"; return out; }
    }
    std::vector<Polygon> box_polygons;
    box_polygons.reserve(boxes.size());
    for (const auto& box : boxes) {
        if (!finite(box.center) || !std::isfinite(box.length) || !std::isfinite(box.width) ||
            !std::isfinite(box.yaw) || box.length <= 0 || box.width <= 0) {
            out.reason = "invalid box obstacle"; return out;
        }
        const double c = std::cos(box.yaw), s = std::sin(box.yaw);
        const double l = box.length / 2, w = box.width / 2;
        Polygon p;
        for (const auto& corner : {Point2{-l,-w}, Point2{l,-w}, Point2{l,w}, Point2{-l,w}}) {
            p.push_back({box.center.x + c * corner.x - s * corner.y,
                         box.center.y + s * corner.x + c * corner.y});
        }
        p.push_back(p.front());
        if (!validPolygon(p)) { out.reason = "invalid box obstacle"; return out; }
        box_polygons.push_back(std::move(p));
    }
    OccupancyGrid grid;
    grid.width = g.width; grid.height = g.height; grid.resolution = g.resolution;
    grid.origin_x = g.origin_x; grid.origin_y = g.origin_y;
    grid.cells.assign(g.width * g.height, -1);
    for (std::size_t y = 0; y < g.height; ++y) {
        for (std::size_t x = 0; x < g.width; ++x) {
            const Square cell{g.origin_x + x * g.resolution, g.origin_y + y * g.resolution,
                              g.origin_x + (x + 1) * g.resolution, g.origin_y + (y + 1) * g.resolution};
            if (edgesTouch(drivable, cell)) { grid.cells[y * g.width + x] = 100; continue; }
            if (!insideArea(cell, drivable)) continue;
            bool blocked = false;
            for (const auto& circle : circles) if (hitsCircle(cell, circle)) { blocked = true; break; }
            if (!blocked) for (const auto& polygon : polygons) if (hitsPolygon(cell, polygon)) { blocked = true; break; }
            if (!blocked) for (const auto& polygon : box_polygons) if (hitsPolygon(cell, polygon)) { blocked = true; break; }
            grid.cells[y * g.width + x] = blocked ? 100 : 0;
        }
    }
    out.success = true;
    out.grid = std::move(grid);
    return out;
}
} // namespace prove::planning
