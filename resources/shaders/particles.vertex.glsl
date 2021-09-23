#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(set = 2, binding = 0) uniform CameraBufferObject {
    mat4 projection;
    mat4 view;
    mat4 inverseView;
    mat4 inverseProjection;
} cbo;

#include "includes/particles.glsl"

layout(location = 0) out flat uint outParticleIndex;
layout(location = 1) out vec2 outFragPosition;

vec3 quad(int vertexID) {
    // TODO: optimize
    switch(vertexID % 6) {
        case 0:
        case 4:
            return vec3(0.5,0,0.5);
        case 1:
            return vec3(0.5,0,-0.5);

        case 2:
        case 5:
            return vec3(-0.5,0,-0.5);

        case 3:
            return vec3(-0.5,0,0.5);
    }
    return vec3(0.0);
}

void main() {
    vec3 inPosition = quad(gl_VertexIndex);
    outFragPosition = inPosition.xz*2;

    inPosition = inPosition.zxy;

    //uint particleIndex = gl_InstanceIndex;
    uint particleIndex = gl_VertexIndex / 6;
    outParticleIndex = particleIndex;

    #define particle particles[nonuniformEXT(particleIndex)]

    mat3 inverseRotation = mat3(cbo.inverseView);
    inPosition = inverseRotation * inPosition;

    float size = particle.size;
    inPosition *= size;

    inPosition += particle.position;

    vec4 viewPosition = cbo.view * vec4(inPosition, 1.0);
    gl_Position = cbo.projection * viewPosition;
}