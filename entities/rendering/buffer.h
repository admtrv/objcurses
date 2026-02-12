/*
 * buffer.h
 */

#pragma once

#include <optional>
#include <vector>
#include <cmath>
#include <algorithm>
#include <stdexcept>

#ifdef _WIN32
#include <ncurses/ncurses.h>
// for msys2 on windows platform use

#elif
#include <ncurses.h>
#endif

#include <iostream>

#include "utils/mathematics.h"
#include "utils/algorithms.h"

// screen pixel
class Pixel {
public:
    float z;                        // depth (z-coordinate)
    char c;                         // character
    std::optional<int> material;    // material index

    Pixel() : z(std::numeric_limits<float>::max()), c(' '), material(std::nullopt) {}
    Pixel(const float z, const char c, const std::optional<int> material = std::nullopt) : z(z), c(c), material(material) {}
};

// projection of triangle onto screen
class Projection {
public:
    Vec3 p1, p2, p3; // vertices of triangle
    char color;      // color of triangle

    Projection(const Vec3 &p1, const Vec3 &p2, const Vec3 &p3, const char color) : p1(p1), p2(p2), p3(p3), color(color) {}

    [[nodiscard]] Projection sort_x() const;
    [[nodiscard]] float limit_y1(float x) const;
    [[nodiscard]] float limit_y2(float x) const;
    [[nodiscard]] Vec3 normal() const;
};

// screen buffer
class Buffer {
public:
    unsigned int x, y;          // character buffer size
    float logical_x, logical_y; // logical buffer size
    float dx, dy;               // logical character size
    std::vector<Pixel> pixels;  // pixel Buffer

    Buffer(unsigned int x, unsigned int y, float logical_x, float logical_y);

    void clear();
    void draw_projection(const Projection &projection, char c, int material);
    void printw() const;

private:
    [[nodiscard]] int index_x(float real_x) const;
    [[nodiscard]] int index_y(float real_y) const;
    [[nodiscard]] float depth(const Projection &projection, const Vec3 &normal, int pixel_x, int pixel_y) const;

};
