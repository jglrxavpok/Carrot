#extension GL_EXT_nonuniform_qualifier : enable

#include "includes/gbuffer.glsl"
#include "includes/particles.glsl"

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec3 outViewPosition;
layout(location = 2) out vec3 outNormal;
layout(location = 3) out uint outIntProperties;

layout(location = 0) in flat uint particleIndex;
layout(location = 1) in vec2 inFragPosition;

void main() {
    if(length(inFragPosition) >= 1.0) {
        discard;
    }
    // TODO
    vec4 c = vec4(0.0);
    c.b = mod(sin(particleIndex) / 100.0, 1.0);
    c.a = abs(particleIndex);
    c.r = exp(particleIndex);
    outColor = c;
    outIntProperties = 0;
}