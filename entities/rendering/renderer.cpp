/*
 * renderer.cpp
 */

#include "renderer.h"

char Renderer::luminance_char(const Vec3 &normal, const Vec3 &light, const std::string &scale)
{
    const float sim = (Vec3::cosine_similarity(normal, light) + 1.0f) * 0.5f;
    const int idx = std::clamp(static_cast<int>(std::round(sim * static_cast<float>(scale.size() - 1))), 0, static_cast<int>(scale.size() - 1));
    return scale[idx];
}

void Renderer::render(Buffer &buf, const Object &obj, const Camera &cam, const Light  &light, bool static_light, bool color_support)
{
    const float az_cos = std::cos(cam.azimuth);
    const float az_sin = std::sin(cam.azimuth);
    const float al_cos = std::cos(cam.altitude);
    const float al_sin = std::sin(cam.altitude);

    // pre-computed rotations
    auto rot_y = [az_cos, az_sin](const Vec3 &v) {
        return Vec3::rotate_y(v, std::atan2(-az_sin, az_cos));
    };

    auto rot_x = [al_cos, al_sin](const Vec3 &v) {
        return Vec3::rotate_x(v, std::atan2(-al_sin, al_cos));
    };

    const float lx = buf.logical_x;
    const float ly = buf.logical_y;

    // first pass - rotate, project, collect bounds
    const size_t vcount = obj.vertices.size();

    std::vector<Vec3> rverts(vcount);   // rotated vertices
    std::vector<Vec3> sverts(vcount);   // screen coords (without offset)

    float min_x = std::numeric_limits<float>::max();
    float max_x = -std::numeric_limits<float>::max();
    float min_y = std::numeric_limits<float>::max();
    float max_y = -std::numeric_limits<float>::max();

    for (size_t i = 0; i < vcount; i++)
    {
        const Vec3 rv = rot_x(rot_y(obj.vertices[i]));
        rverts[i] = rv;

        const Vec3 sv = Vec3::to_screen(rv, cam.zoom, lx, ly);
        sverts[i] = sv;

        min_x = std::min(min_x, sv.x);
        max_x = std::max(max_x, sv.x);
        min_y = std::min(min_y, sv.y);
        max_y = std::max(max_y, sv.y);
    }

    // offset that centers the bounding box in logical space
    const float off_x = 0.0f;
    const float off_y = (ly - (max_y - min_y)) * 0.5f - min_y;
    const Vec3 offset(off_x, off_y, 0.0f);

    // second pass - draw faces
    for (const auto &face : obj.faces)
    {
        const Vec3 &rv1 = rverts[face.indices[0]];
        const Vec3 &rv2 = rverts[face.indices[1]];
        const Vec3 &rv3 = rverts[face.indices[2]];

        // back-face culling in camera space
        Vec3 normal_cam = Vec3::cross(rv2 - rv1, rv3 - rv1).normalize();

        if (normal_cam.z >= 0.0f)
        {
            continue;
        }

        const Vec3 normal_view = -normal_cam;

        // screen coordinates with centering offset
        const Vec3 s1 = sverts[face.indices[0]] + offset;
        const Vec3 s2 = sverts[face.indices[1]] + offset;
        const Vec3 s3 = sverts[face.indices[2]] + offset;

        // shading
        const Vec3 n_light = static_light ? Vec3::cross(obj.vertices[face.indices[1]] - obj.vertices[face.indices[0]], obj.vertices[face.indices[2]] - obj.vertices[face.indices[0]]).normalize() : normal_view;
        const char lum = luminance_char(n_light, light.direction, CHARS_LUM);

        buf.draw_projection(Projection(s1, s2, s3, lum), lum, (color_support && face.material) ? *face.material : -1);
    }
}