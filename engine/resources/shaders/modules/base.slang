module base;

public typedef float2 vec2;
public typedef float3 vec3;
public typedef float4 vec4;
public typedef int32_t2 ivec2;
public typedef int32_t3 ivec3;
public typedef int32_t4 ivec4;
public typedef uint32_t2 uvec2;
public typedef uint32_t3 uvec3;
public typedef uint32_t4 uvec4;
public typedef float3x3 mat3;
public typedef float4x4 mat4;

public static const double M_PI = 3.14159265358979323846;
public static const double M_INV_PI = 1.0/M_PI;
public static const double M_PI_OVER_2 = M_PI/2.0;
public static const double M_PI_OVER_4 = M_PI/4.0;
public static const float M_PIf = float(M_PI);
public static const float M_INV_PIf = float(M_INV_PI);
public static const float M_PI_OVER_2f = float(M_PI_OVER_2);
public static const float M_PI_OVER_4f = float(M_PI_OVER_4);

// Convenience methods for GLSL-like matrix/vector multiplication
public vector<float, N> operator*<int M, int N>(in matrix<float, M, N> m, in vector<float, M> v) {
    return mul(v, m);
}

public matrix<float, N, N> operator *<int N>(in matrix<float, N, N> a, in matrix<float, N, N> b) {
    return mul(b, a);
}

// https://stackoverflow.com/questions/983999/simple-3x3-matrix-inverse-code-c
public matrix<float, 3, 3> inverse(in matrix<float, 3, 3> m) {
    // computes the inverse of a matrix m
    float det = m[0][0] * (m[1][1] * m[2][2] - m[2][1] * m[1][2]) -
                m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
                m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);

    float invdet = 1 / det;

    matrix<float, 3, 3> minv; // inverse of matrix m
    minv[0][0] = (m[1][1] * m[2][2] - m[2][1] * m[1][2]) * invdet;
    minv[0][1] = (m[0][2] * m[2][1] - m[0][1] * m[2][2]) * invdet;
    minv[0][2] = (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * invdet;
    minv[1][0] = (m[1][2] * m[2][0] - m[1][0] * m[2][2]) * invdet;
    minv[1][1] = (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * invdet;
    minv[1][2] = (m[1][0] * m[0][2] - m[0][0] * m[1][2]) * invdet;
    minv[2][0] = (m[1][0] * m[2][1] - m[2][0] * m[1][1]) * invdet;
    minv[2][1] = (m[2][0] * m[0][1] - m[0][0] * m[2][1]) * invdet;
    minv[2][2] = (m[0][0] * m[1][1] - m[1][0] * m[0][1]) * invdet;

    return minv;
}