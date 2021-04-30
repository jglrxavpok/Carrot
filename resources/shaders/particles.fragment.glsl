#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "includes/gbuffer.glsl"
#include "includes/particles.glsl"

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec3 outViewPosition;
layout(location = 2) out vec3 outNormal;
layout(location = 3) out uint outIntProperties;

layout(location = 0) in flat uint particleIndex;

void main() {
    // TODO
    vec4 c = vec4(0.0);
    c.b = mod(particleIndex / 100.0, 1.0);
    c.a = 1.0;
    outColor = c;
    outIntProperties = 0;
}