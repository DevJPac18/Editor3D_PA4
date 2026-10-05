#include "render/Camera.h"
#include <cmath>

void Camera::reset() {
    target = Vec3(0.6f, 0.9f, 0.0f);
    distance = 13.0f;
    yaw = 28.0f;
    pitch = 24.0f;
    fov = 45.0f;
    nearPlane = 0.05f;
    farPlane = 400.0f;
}

Vec3 Camera::eye() const {
    float p = deg2rad(clampf(pitch, -89.5f, 89.5f)), y = deg2rad(yaw);
    return target + Vec3(std::cos(p) * std::sin(y), std::sin(p), std::cos(p) * std::cos(y)) * distance;
}

void Camera::orbit(float dx, float dy) {
    yaw -= dx * 0.4f;
    pitch = clampf(pitch + dy * 0.4f, -89.5f, 89.5f);
}

void Camera::pan(float dx, float dy, float vpH) {
    float k = 2.0f * distance * std::tan(deg2rad(fov) * 0.5f) / (vpH > 1 ? vpH : 1);
    target += right() * (-dx * k) + up() * (dy * k);
}

void Camera::zoom(float steps) {
    distance = clampf(distance * std::pow(0.88f, steps), 0.3f, 250.0f);
}

void Camera::focus(const Vec3& p, float radius) {
    target = p;
    distance = radius * 3.0f < 1.5f ? 1.5f : radius * 3.0f;
}
