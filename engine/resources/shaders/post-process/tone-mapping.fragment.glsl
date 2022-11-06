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
float luminance(vec3 v) {
    return dot(v, vec3(0.2126f, 0.7152f, 0.0722f));
}

vec3 changeLuminance(vec3 color, float targetLuminance) {
    float l_in = luminance(color);
    return color * (targetLuminance / l_in);
}

vec3 reinhard(vec3 v) {
    float l = luminance(v);
    float targetLuminance = l / (1.0f + l);
    return changeLuminance(v, targetLuminance);
}

void main() {
    vec4 fragmentColor = texture(sampler2D(inputImage, linearSampler), uv);

    vec3 rgb = fragmentColor.rgb;
    vec3 xyz = rgb2xyz(rgb);
    vec3 reinhard = reinhard(xyz);

    outColor = vec4(xyz2rgb(reinhard), fragmentColor.a);
}