#extension GL_EXT_nonuniform_qualifier : enable

#include "includes/materials.glsl"
#include "includes/billboards.glsl"

MATERIAL_SYSTEM_SET(1)

layout(location = 0) in vec2 uv;
layout(location = 1) in vec3 viewPosition;

#include <includes/gbuffer.glsl>
#include "includes/gbuffer_output.glsl"

void main() {
    vec4 texColor = texture(sampler2D(textures[billboard.textureID], linearSampler), uv) * vec4(billboard.color, 1.0);
    if(texColor.a < 0.01) {
        discard;
    }

    GBuffer o = initGBuffer(mat4(1.0));
    o.albedo = vec4(texColor.rgb, 1.0);
    o.viewPosition = viewPosition;
    o.viewTBN = mat3(vec3(1,0,0), vec3(0,0,1), vec3(0,1,0));

    outputGBuffer(o, mat4(1.0));
}