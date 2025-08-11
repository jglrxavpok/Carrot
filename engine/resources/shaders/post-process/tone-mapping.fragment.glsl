#include <includes/lighting_utils.glsl>

layout(set = 0, binding = 0) uniform texture2D inputImage;
layout(set = 0, binding = 1) uniform sampler nearestSampler;
layout(set = 0, binding = 2) uniform sampler linearSampler;

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 outColor;

// https://www.pbr-book.org/3ed-2018/Color_and_Radiometry/The_SampledSpectrum_Class
vec3 xyz2rgb(vec3 xyz) {
    vec3 rgb;
    rgb.r =  2.36461385*xyz.x - 0.89654057*xyz.y - 0.46807328*xyz.z;
    rgb.g = -0.51516621*xyz.x + 1.42640810*xyz.y + 0.08875810*xyz.z;
    rgb.b =  0.00520370*xyz.x - 0.01440816*xyz.y + 1.00920446*xyz.z;

    return rgb;
}

vec3 rgb2xyz(vec3 rgb) {
    vec3 xyz;
    xyz.x = 0.49000*rgb.r + 0.31000*rgb.g + 0.20000*rgb.b;
    xyz.y = 0.17697*rgb.r + 0.82140*rgb.g + 0.01063*rgb.b;
    xyz.z = 0.00000*rgb.r + 0.01000*rgb.g + 0.99000*rgb.b;
    return xyz;
}

// https://64.github.io/tonemapping/
vec3 changeLuminance(vec3 color, float targetLuminance) {
    float l_in = luminance(color);
    return color * (targetLuminance / l_in);
}

vec3 reinhard(vec3 v) {
    float l = luminance(v);
    float targetLuminance = l / (1.0f + l);
    return changeLuminance(v, targetLuminance);
}

// https://gist.github.com/SpineyPete/ebf9619f009318536c6da48209894fed
vec3 Tonemap_Aces(vec3 color) {

    // ACES filmic tonemapper with highlight desaturation ("crosstalk").
    // Based on the curve fit by Krzysztof Narkowicz.
    // https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/

    const float slope = 12.0f; // higher values = slower rise.

    // Store grayscale as an extra channel.
    vec4 x = vec4(
    // RGB
    color.r, color.g, color.b,
    // Luminosity
    (color.r * 0.299) + (color.g * 0.587) + (color.b * 0.114)
    );

    // ACES Tonemapper
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;

    vec4 tonemap = clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
    float t = x.a;

    t = t * t / (slope + t);

    // Return after desaturation step.
    return mix(tonemap.rgb, tonemap.aaa, t);
}

void main() {
    vec4 fragmentColor = texture(sampler2D(inputImage, linearSampler), uv);

    vec3 rgb = fragmentColor.rgb;
    vec3 xyz = rgb2xyz(rgb);
    vec3 reinhard = reinhard(xyz);
    vec3 aces = Tonemap_Aces(rgb);
    //outColor = fragmentColor;
    outColor = vec4(xyz2rgb(reinhard), fragmentColor.a);
    //outColor = vec4(aces, fragmentColor.a);
    //outColor = vec4(rgb, fragmentColor.a);

//    outColor.rgb = pow(outColor.rgb, vec3(1.0 / 2.2));
}