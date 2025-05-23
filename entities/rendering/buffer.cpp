/*
 * buffer.cpp
 */

#include "buffer.h"

#include <array>

// Projection methods

Projection Projection::sort_x() const
{
    std::array arr = {p1, p2, p3};

    std::ranges::sort(arr, [](const Vec3 &a, const Vec3 &b) { return a.x < b.x; });

    return {arr[0], arr[1], arr[2], color};
}

float Projection::limit_y1(const float x) const
{
    if (x <= p1.x) return p1.y;
    if (x >= p3.x) return p3.y;

    if (x <= p2.x)
    {
        const float denominator = p2.x - p1.x;

        if (std::fabs(denominator) < 1e-7f)
            return p1.y;

        const float t = (x - p1.x) / denominator;
        return lerp(p1.y, p2.y, t);
    }
    else
    {
        const float denominator = p3.x - p2.x;

        if (std::fabs(denominator) < 1e-7f)
            return p2.y;

        const float t = (x - p2.x) / denominator;
        return lerp(p2.y, p3.y, t);
    }
}

float Projection::limit_y2(const float x) const
{
    if (x <= p1.x)
        return p1.y;
    if (x >= p3.x)
        return p3.y;

    const float denominator = p3.x - p1.x;

    if (std::fabs(denominator) < 1e-7f)
        return p1.y;

    const float t = (x - p1.x) / denominator;
    return lerp(p1.y, p3.y, t);
}

Vec3 Projection::normal() const
{
    const Vec3 v1 = p2 - p1;
    const Vec3 v2 = p3 - p1;
    const Vec3 n = Vec3::cross(v1, v2);

    return n.normalize();
}

// Buffer methods

Buffer::Buffer(const unsigned int x, const unsigned int y, const float logical_x, const float logical_y) : x(x), y(y), logical_x(logical_x), logical_y(logical_y)
{
    if (x == 0 || y == 0)
    {
        throw std::runtime_error("zero buffer size");
    }

    dx = logical_x / static_cast<float>(x);
    dy = logical_y / static_cast<float>(y);

    pixels.resize(x * y);

    clear();
}

void Buffer::clear()
{
    for (auto &p : pixels)
    {
        p.z = std::numeric_limits<float>::max();
        p.c = ' ';
        p.material = std::nullopt;
    }
}

int Buffer::index_x(const float real_x) const
{
    int index = static_cast<int>(real_x / dx);
    index = clamp(index, 0, static_cast<int>(x) - 1);

    return index;
}

int Buffer::index_y(const float real_y) const
{
    int iy = static_cast<int>(real_y / dy);
    iy = clamp(iy, 0, static_cast<int>(y) - 1);

    return iy;
}

float Buffer::depth(const Projection &projection, const Vec3 &normal, const int pixel_x, const int pixel_y) const
{
    const float center_x = (static_cast<float>(pixel_x) + 0.5f) * dx;
    const float center_y = (static_cast<float>(pixel_y) + 0.5f) * dy;

    if (std::fabs(normal.z) < 1e-7f)
    {
        return projection.p1.z;
    }

    const float d_z = normal.x * (center_x - projection.p1.x) + normal.y * (center_y - projection.p1.y);
    const float z  = projection.p1.z - d_z / normal.z;

    return z;
}

void Buffer::draw_projection(const Projection &projection, const char c, int material)
{
    const Projection triangle = projection.sort_x();

    const float x_i = triangle.p1.x + dx * 0.5f;
    const float x_f = triangle.p3.x - dx * 0.5f;
    if (x_f < 0.f || x_i > logical_x)
        return;

    const int x_start = index_x(x_i);
    const int x_end   = index_x(x_f);

    const Vec3 normal = triangle.normal();

    for (int pixel_x = x_start; pixel_x <= x_end; pixel_x++)
    {
        const float rx = (static_cast<float>(pixel_x) + 0.5f) * dx;

        float y1 = triangle.limit_y1(rx);
        float y2 = triangle.limit_y2(rx);

        const float y_min = std::min(y1, y2);
        const float y_max = std::max(y1, y2);

        if (y_max < 0.f || y_min > logical_y)
            continue;

        const float y_start_val = y_min + dy * 0.5f;
        const float y_end_val = y_max - dy * 0.5f;

        const int y_start = index_y(y_start_val);
        const int y_end = index_y(y_end_val);

        for (int pixel_y = y_start; pixel_y <= y_end; pixel_y++)
        {
            Pixel &pixel = pixels[pixel_y * x + pixel_x];

            if (const float z = depth(triangle, normal, pixel_x, pixel_y); z < pixel.z)
            {
                pixel.z = z;
                pixel.c = c;
                pixel.material = material;
            }
        }
    }
}

void Buffer::printw() const
{
    for (unsigned int row = 0; row < y; row++)
    {
        ::move(static_cast<int>(row), 0);
        int prev_color = -1;

        for (unsigned int col = 0; col < x; col++)
        {
            const Pixel &pixel = pixels[row * x + col];

            if (const int color = pixel.material ? (pixel.material.value() + 1) : 0; color != prev_color)
            {
                if (prev_color > 0)
                {
                    attroff(COLOR_PAIR(prev_color));
                }

                if (color > 0)
                {
                    attron(COLOR_PAIR(color));
                }

                prev_color = color;
            }

            ::printw("%c", pixel.c);
        }

        if (prev_color > 0)
        {
            attroff(COLOR_PAIR(prev_color));
        }
    }
}