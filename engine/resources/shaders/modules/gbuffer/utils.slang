implementing gbuffer;
import glsl;

uint16_t2 toUints(float2 v) {
    return uint16_t2(round(clamp((v + 1) / 2, 0, 1) * 65535.0f));
}

float2 toFloats(uint16_t2 v) {
    return float2((v/65535.0f)*2-1);
}

// Based on https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/

float2 OctWrap(float2 v)
{
    return (1.0 - abs(v.yx)) * (v.xy >= 0.0 ? 1.0 : -1.0);
}

float2 Encode(float3 n)
{
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    n.xy = n.z >= 0.0 ? n.xy : OctWrap(n.xy);
    n.xy = n.xy * 0.5 + 0.5;
    return n.xy;
}

float3 Decode(float2 f)
{
    f = f * 2.0 - 1.0;

    // https://twitter.com/Stubbesaurus/status/937994790553227264
    float3 n = float3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = saturate(-n.z);
    n.xy += n.xy >= 0.0 ? -t : t;
    return normalize(n);
}

void decompressTBN(uint16_t4 compressed, out mat3 outTBN) {
    outTBN[2] = Decode(toFloats(compressed.xy));  // normal
    outTBN[0] = Decode(toFloats(compressed.zw)); // tangent

    outTBN[1] = cross(outTBN[0], outTBN[2]);
}

public void compressTBN(mat3 tbn, out uint16_t4 compressed) {
    vec3 tangent = normalize(tbn[0]);
    vec3 normal = normalize(tbn[2]);

    compressed.xy = toUints(Encode(normal));
    compressed.zw = toUints(Encode(tangent));
}