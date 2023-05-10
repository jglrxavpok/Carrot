#extension GL_EXT_nonuniform_qualifier : enable

#include <includes/camera.glsl>
DEFINE_CAMERA_SET(0)

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
    gl_Position = cbo.jitteredProjection * viewPosition;
}