module base;

public typedef float2 vec2;
public typedef float3 vec3;
public typedef float4 vec4;
public typedef float3x3 mat3;
public typedef float4x4 mat4;

// Convenience methods for GLSL-like matrix/vector multiplication
public vector<float, N> operator*<int M, int N>(in matrix<float, M, N> m, in vector<float, M> v) {
    return mul(v, m);
}