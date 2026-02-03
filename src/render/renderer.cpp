#include "renderer.h"
#include <cmath>
#include <cstring>

namespace opensaints {

// Math helpers implementation

void RenderMath::identity(float* m) {
    std::memset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

void RenderMath::perspective(float* m, float fov, float aspect, float near, float far) {
    std::memset(m, 0, 16 * sizeof(float));

    float tanHalfFov = std::tan(fov * 0.5f);

    m[0] = 1.0f / (aspect * tanHalfFov);
    m[5] = 1.0f / tanHalfFov;
    m[10] = -(far + near) / (far - near);
    m[11] = -1.0f;
    m[14] = -(2.0f * far * near) / (far - near);
}

void RenderMath::lookAt(float* m, const float* eye, const float* target, const float* up) {
    // Forward vector (from target to eye for right-handed)
    float f[3] = {
        target[0] - eye[0],
        target[1] - eye[1],
        target[2] - eye[2]
    };

    // Normalize forward
    float fLen = std::sqrt(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
    f[0] /= fLen;
    f[1] /= fLen;
    f[2] /= fLen;

    // Right vector = forward x up
    float r[3] = {
        f[1] * up[2] - f[2] * up[1],
        f[2] * up[0] - f[0] * up[2],
        f[0] * up[1] - f[1] * up[0]
    };

    // Normalize right
    float rLen = std::sqrt(r[0] * r[0] + r[1] * r[1] + r[2] * r[2]);
    r[0] /= rLen;
    r[1] /= rLen;
    r[2] /= rLen;

    // Recalculate up = right x forward
    float u[3] = {
        r[1] * f[2] - r[2] * f[1],
        r[2] * f[0] - r[0] * f[2],
        r[0] * f[1] - r[1] * f[0]
    };

    // Build matrix (column-major)
    m[0] = r[0];  m[4] = r[1];  m[8]  = r[2];  m[12] = -(r[0] * eye[0] + r[1] * eye[1] + r[2] * eye[2]);
    m[1] = u[0];  m[5] = u[1];  m[9]  = u[2];  m[13] = -(u[0] * eye[0] + u[1] * eye[1] + u[2] * eye[2]);
    m[2] = -f[0]; m[6] = -f[1]; m[10] = -f[2]; m[14] = (f[0] * eye[0] + f[1] * eye[1] + f[2] * eye[2]);
    m[3] = 0;     m[7] = 0;     m[11] = 0;     m[15] = 1.0f;
}

void RenderMath::translate(float* m, float x, float y, float z) {
    identity(m);
    m[12] = x;
    m[13] = y;
    m[14] = z;
}

void RenderMath::rotateY(float* m, float angle) {
    identity(m);
    float c = std::cos(angle);
    float s = std::sin(angle);
    m[0] = c;
    m[2] = -s;
    m[8] = s;
    m[10] = c;
}

void RenderMath::multiply(float* result, const float* a, const float* b) {
    float temp[16];
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            temp[i * 4 + j] =
                a[i * 4 + 0] * b[0 * 4 + j] +
                a[i * 4 + 1] * b[1 * 4 + j] +
                a[i * 4 + 2] * b[2 * 4 + j] +
                a[i * 4 + 3] * b[3 * 4 + j];
        }
    }
    std::memcpy(result, temp, 16 * sizeof(float));
}

} // namespace opensaints
