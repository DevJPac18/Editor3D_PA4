// Camera.h - Cámara orbital del editor (objetivo, distancia, yaw, pitch)
#pragma once
#include "core/Math3D.h"

class Camera {
public:
    Vec3 target;
    float distance, yaw, pitch, fov, nearPlane, farPlane;

    Camera() { reset(); }
    void reset();

    Vec3 eye() const;
    Vec3 forward() const { return normalize(target - eye()); }
    Vec3 right() const { return normalize(cross(forward(), Vec3(0, 1, 0))); }
    Vec3 up() const { return cross(right(), forward()); }
    Mat4 view() const { return Mat4::lookAt(eye(), target, Vec3(0, 1, 0)); }
    Mat4 projection(float aspect) const { return Mat4::perspective(fov, aspect, nearPlane, farPlane); }

    void orbit(float dxPixels, float dyPixels);
    void pan(float dxPixels, float dyPixels, float viewportHeight);
    void zoom(float steps);
    void focus(const Vec3& p, float radius);
    void setView(float yawDeg, float pitchDeg) { yaw = yawDeg; pitch = pitchDeg; }
};
