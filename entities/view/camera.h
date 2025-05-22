/*
 * camera.h
 */

#pragma once

#include <algorithm>
#include "utils/mathematics.h"
#include "utils/algorithms.h"
#include "config.h"

class Camera {
public:

    float azimuth;      // rad
    float altitude;     // rad
    float zoom;         // 1.0 == unit cube

    // constructors
    Camera() : azimuth(0.0f), altitude(0.0f), zoom(std::clamp(1.0f, ZOOM_MIN, ZOOM_MAX)) {}
    Camera(float azimuth, float altitude, float zoom) :
        azimuth(rad_norm(azimuth)),
        altitude(std::clamp(altitude, -PI / 2, PI / 2)),
        zoom(std::clamp(zoom, ZOOM_MIN, ZOOM_MAX)) {}

    void rotate_left(float degree = ANGLE_STEP)
    {
        azimuth  = rad_norm(azimuth  + deg2rad(degree));
    }
    void rotate_right(float degree = ANGLE_STEP)
    {
        azimuth  = rad_norm(azimuth  - deg2rad(degree));
    }
    void rotate_up(float degree = ANGLE_STEP)
    {
        altitude = std::min(altitude + deg2rad(degree),  PI / 2);
    }
    void rotate_down(float degree = ANGLE_STEP)
    {
        altitude = std::max(altitude - deg2rad(degree), -PI / 2);
    }

    void zoom_in(float step = ZOOM_STEP)
    {
        zoom = std::min(zoom + step, ZOOM_MAX);
    }
    void zoom_out(float step = ZOOM_STEP)
    {
        zoom = std::max(zoom - step, ZOOM_MIN);
    }
};
