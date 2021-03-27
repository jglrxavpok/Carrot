#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(input_attachment_index = 0, binding = 0) uniform subpassInput albedo;
layout(input_attachment_index = 1, binding = 1) uniform subpassInput depth;
layout(input_attachment_index = 2, binding = 2) uniform subpassInput viewPos;
layout(input_attachment_index = 3, binding = 3) uniform subpassInput viewNormals;
layout(set = 0, binding = 4) uniform texture2D rayTracedLighting;
layout(set = 0, binding = 5) uniform texture2D uiRendered;
layout(set = 0, binding = 6) uniform sampler linearSampler;

layout(set = 0, binding = 7) uniform CameraBufferObject {
    mat4 projection;
    mat4 view;
    mat4 inverseView;
    mat4 inverseProjection;
} cbo;

layout(set = 0, binding = 8) uniform Debug {
    #include "debugparams.glsl"
} debug;

layout(set = 0, binding = 9) uniform texture2D skyboxTexture;

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 outColor;

const vec3 sunDirection = vec3(1,1,1);

void main() {
    vec4 outColorWorld;

    if(debug.showSingleGChannel >= 0) {
        switch(debug.showSingleGChannel) {
            case 0:
                outColorWorld = subpassLoad(albedo);
            break;

            case 1:
                outColorWorld = vec4(subpassLoad(viewPos).rgb, 1.0);
            break;

            case 2:
                outColorWorld = vec4(subpassLoad(viewNormals).rgb, 1.0);
            break;

            case 3:
                outColorWorld = subpassLoad(depth);
            break;

            case 4:
                outColorWorld = texture(sampler2D(rayTracedLighting, linearSampler), uv);
            break;

            case 5:
                outColorWorld = vec4(0.0,0.0,0.0,1.0);

            case 6:
                outColorWorld = texture(sampler2D(skyboxTexture, linearSampler), uv);
            break;
        }

    } else {
        const vec3 nSunDirection = normalize(sunDirection);
        vec4 fragmentColor = subpassLoad(albedo);
        vec3 normal = (cbo.inverseView * subpassLoad(viewNormals)).xyz;
        vec3 lighting = texture(sampler2D(rayTracedLighting, linearSampler), uv).rgb;
        vec3 skyboxRGB = texture(sampler2D(skyboxTexture, linearSampler), uv).rgb;

        float currDepth = subpassLoad(depth).r;
        if(currDepth < 1.0) {
            outColorWorld = vec4(fragmentColor.rgb*lighting, fragmentColor.a);
        } else {
            outColorWorld = vec4(skyboxRGB, fragmentColor.a);
        }
    }

    vec4 outColorUI = texture(sampler2D(uiRendered, linearSampler), uv);
    float alpha01 = (1 - outColorUI.a)*outColorWorld.a + outColorUI.a;
    vec4 renderedColor = vec4(((1-outColorUI.a) * outColorWorld.a * outColorWorld.rgb + outColorUI.a*outColorUI.rgb) / alpha01, 1.0);
    outColor = vec4(renderedColor.rgb, 1.0);
}