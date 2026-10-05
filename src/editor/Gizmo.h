// Gizmo.h - Manipuladores visuales de traslación / rotación / escala
#pragma once
#include "core/Math3D.h"

enum GizmoMode { GIZMO_SELECT = 0, GIZMO_MOVE = 1, GIZMO_ROTATE = 2, GIZMO_SCALE = 3 };

// Manijas: 1 = X, 2 = Y, 3 = Z, 4 = centro (libre / escala uniforme)
struct GizmoFrame {
    Vec3 origin;
    Vec3 axis[3];
    Vec3 camRight, camUp;
    float size;
    GizmoFrame() : size(1.0f) { axis[0] = Vec3(1, 0, 0); axis[1] = Vec3(0, 1, 0); axis[2] = Vec3(0, 0, 1); }
};

namespace Gizmo {
// Dibuja con la matriz de vista cargada en GL_MODELVIEW. pick=true usa colores de selección.
void draw(const GizmoFrame& f, int mode, int hover, bool pick, float lineScale);
}
