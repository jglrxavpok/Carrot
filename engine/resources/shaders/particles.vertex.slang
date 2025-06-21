import modules.base;
import modules.camera;
import modules.particles;

struct VSOutput {
    float4 hPos : SV_Position;
    nointerpolation uint32_t particleIndex;
    float2 fragPosition;
}

[vk::binding(0,0)]
ParameterBlock<Camera> camera;
ParameterBlock<Particles> particles;

float3 quad(int vertexID) {
    // TODO: optimize
    switch (vertexID % 6) {
    case 0:
    case 4:
        return float3(0.5, 0, 0.5);
    case 1:
        return float3(0.5, 0, -0.5);

    case 2:
    case 5:
        return float3(-0.5, 0, -0.5);

    case 3:
        return float3(-0.5, 0, 0.5);
    }
    return float3(0.0);
}

[shader("vertex")]
VSOutput vertex(int vertexID: SV_VertexID) {
    VSOutput o;

    vec3 inPosition = quad(vertexID);
    o.fragPosition = inPosition.xz * 2;

    inPosition = inPosition.zxy;

    uint particleIndex = vertexID / 6;
    o.particleIndex = particleIndex;

    const Particle particle = particles.list[particleIndex];

    const CameraBufferObject cbo = camera.CurrentFrame();
    mat3 inverseRotation = mat3(cbo.inverseView);
    inPosition = inverseRotation * inPosition;

    float size = particle.size;
    inPosition *= size;

    const Emitter emitter = particles.emitters.Load(particle.emitterID);
    inPosition += (emitter.transform * vec4(particle.position, 1)).xyz;

    vec4 viewPosition = cbo.view * vec4(inPosition, 1.0);
    o.hPos = cbo.jitteredProjection * viewPosition;

    return o;
}