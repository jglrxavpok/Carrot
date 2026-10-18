module base;

import glsl;

public static const double M_PI = 3.14159265358979323846;
public static const double M_INV_PI = 1.0/M_PI;
public static const double M_PI_OVER_2 = M_PI/2.0;
public static const double M_PI_OVER_4 = M_PI/4.0;
public static const float M_PIf = float(M_PI);
public static const float M_INV_PIf = float(M_INV_PI);
public static const float M_PI_OVER_2f = float(M_PI_OVER_2);
public static const float M_PI_OVER_4f = float(M_PI_OVER_4);

public static const float3x3 IdentityMatrix3x3 = float3x3(
    1, 0, 0,
    0, 1, 0,
    0, 0, 1,
);

public static const float4x4 IdentityMatrix = float4x4(
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1,
);

public void computeTangents(float3 normal, out float3 tangent, out float3 bitangent) {
    float3 toUse = abs(normal.x) > 0.5f ? vec3(0, 0, 1) : vec3(1, 0, 0); // avoid cross product with a colinear vector
    tangent = cross(normal, toUse);
    bitangent = cross(normal, tangent);
}

public float luminance(float3 rgb) {
    return dot(rgb, float3(0.2126F, 0.7152F, 0.0722F));
}

public float4 transformSphere(float4 sphere, float4x4 transform) {
    float4 hCenter = float4(sphere.xyz, 1.0f);
    hCenter = transform * hCenter;
    const float3 center = hCenter.xyz / hCenter.w;
    return float4(center, length((transform * float4(sphere.w, 0, 0, 0)).xyz));
}

public float getSignedDistanceToPlane(vec3 planeNormal, float planeDistanceFromOrigin, vec3 point) {
    return dot(planeNormal, point) + planeDistanceFromOrigin;
}
