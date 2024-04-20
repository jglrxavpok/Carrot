#ifndef _MATH_GLSL
#define _MATH_GLSL

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

vec3 barycentrics(vec3 a, vec3 b, vec3 c, vec3 p) {
    vec3 offsetA = a - p;
    vec3 offsetB = b - p;
    vec3 offsetC = c - p;

    float invTriangleArea = 2.0 / length(cross(b-a, c-a));
    float u = length(cross(p-b, p-c)) / 2.0;
    float v = length(cross(p-a, p-c)) / 2.0;
    u *= invTriangleArea;
    v *= invTriangleArea;

    return vec3(u, v, 1-u-v);
}

// Transforms a sphere by the given transform
// xyz is sphere center
// w is sphere radius
vec4 transformSphere(vec4 sphere, mat4 transform) {
    vec4 hCenter = vec4(sphere.xyz, 1.0f);
    hCenter = transform * hCenter;
    const vec3 center = hCenter.xyz / hCenter.w;
    return vec4(center, length((transform * vec4(sphere.w, 0, 0, 0)).xyz));
}

float getSignedDistanceToPlane(vec3 planeNormal, float planeDistanceFromOrigin, vec3 point) {
    return dot(planeNormal, point) + planeDistanceFromOrigin;
}

#endif // _MATH_GLSL