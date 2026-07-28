#pragma once

#include "system.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace aper {

enum class PaintRole {
    paper,
    ink,
    palette,
};

class Paint {
  public:
    static Paint paper();
    static Paint ink();
    static Paint palette(std::uint8_t fill);

    PaintRole role() const { return role_; }
    std::uint8_t fill() const { return fill_; }

  private:
    Paint(PaintRole role, std::uint8_t fill);

    PaintRole role_ = PaintRole::paper;
    std::uint8_t fill_ = 0;
};

class Viewport {
  public:
    Viewport(Point minimum, Point maximum);
    Viewport(double x, double y, double width, double height);

    Point minimum() const { return minimum_; }
    Point maximum() const { return maximum_; }
    double x() const { return minimum_.real(); }
    double y() const { return minimum_.imag(); }
    double width() const { return maximum_.real() - minimum_.real(); }
    double height() const { return maximum_.imag() - minimum_.imag(); }
    double aspect_ratio() const { return width() / height(); }
    bool intersects(std::span<const Point> polygon) const;

  private:
    Point minimum_;
    Point maximum_;
};

struct DrawingPolygon {
    std::vector<Point> vertices;
    Paint fill = Paint::paper();
    Paint stroke = Paint::ink();
};

struct DrawingArrow {
    Point start;
    Point end;
    Paint paint = Paint::ink();
    double width = 0.0;
    double head_size = 0.0;
};

struct DrawingMetadata {
    std::string title;
    std::string subject;
};

class Drawing {
  public:
    explicit Drawing(Viewport viewport, DrawingMetadata metadata = {});

    const Viewport& viewport() const { return viewport_; }
    const DrawingMetadata& metadata() const { return metadata_; }
    Paint background() const { return background_; }
    std::span<const DrawingPolygon> polygons() const { return polygons_; }
    std::span<const DrawingArrow> arrows() const { return arrows_; }
    bool empty() const { return polygons_.empty() && arrows_.empty(); }

    void set_background(Paint paint);
    void add(DrawingPolygon polygon);
    void add(DrawingArrow arrow);

  private:
    Viewport viewport_;
    DrawingMetadata metadata_;
    Paint background_ = Paint::paper();
    std::vector<DrawingPolygon> polygons_;
    std::vector<DrawingArrow> arrows_;
};

class PatchView {
  public:
    PatchView(const TilingSystem& system, std::string_view seed, unsigned depth);
    PatchView(TilingSystem&&, std::string_view, unsigned) = delete;

    Drawing drawing() const;

  private:
    const TilingSystem& system_;
    std::string seed_;
    unsigned depth_;
};

class RuleView {
  public:
    explicit RuleView(const TilingSystem& system);
    RuleView(TilingSystem&&) = delete;

    Drawing drawing() const;

  private:
    const TilingSystem& system_;
};

} // namespace aper
