#define M_PI 3.14159265358979323846
#define M_INV_PI (1.0/M_PI)
#define M_PI_OVER_2 (M_PI/2.0)
#define M_PI_OVER_4 (M_PI/4.0)

float getNear(mat4 perspectiveMatrix) {
    float m22 = perspectiveMatrix[2][2];
    float m32 = perspectiveMatrix[3][2];
    return m22 / (m32 + 1.0);
}

float getFar(mat4 perspectiveMatrix) {
    float m22 = perspectiveMatrix[2][2];
    float m32 = perspectiveMatrix[3][2];
    return m22 / (m32 - 1.0);
}

vec2 getNearFar(mat4 perspectiveMatrix) {
    float m22 = perspectiveMatrix[2][2];
    float m32 = perspectiveMatrix[3][2];
    return vec2(getNear(perspectiveMatrix), getFar(perspectiveMatrix));
}