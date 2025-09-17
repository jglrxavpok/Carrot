Texture2D inputImage;
SamplerState nearestSampler;
SamplerState linearSampler;

enum ToneMappingOption: uint8_t {
    None,
    Reinhard,
    Aces,
};

float luminance(float3 v) {
    return dot(v, float3(0.2126f, 0.7152f, 0.0722f));
}

// https://www.pbr-book.org/3ed-2018/Color_and_Radiometry/The_SampledSpectrum_Class
float3 xyz2rgb(float3 xyz) {
    float3 rgb;
    rgb.r =  2.36461385*xyz.x - 0.89654057*xyz.y - 0.46807328*xyz.z;
    rgb.g = -0.51516621*xyz.x + 1.42640810*xyz.y + 0.08875810*xyz.z;
    rgb.b =  0.00520370*xyz.x - 0.01440816*xyz.y + 1.00920446*xyz.z;

    return rgb;
}

float3 rgb2xyz(float3 rgb) {
    float3 xyz;
    xyz.x = 0.49000*rgb.r + 0.31000*rgb.g + 0.20000*rgb.b;
    xyz.y = 0.17697*rgb.r + 0.82140*rgb.g + 0.01063*rgb.b;
    xyz.z = 0.00000*rgb.r + 0.01000*rgb.g + 0.99000*rgb.b;
    return xyz;
}

// https://64.github.io/tonemapping/
float3 changeLuminance(float3 color, float targetLuminance) {
    float l_in = luminance(color);
    return color * (targetLuminance / l_in);
}

float3 reinhard(float3 rgb) {
    float3 v = rgb2xyz(rgb);
    float l = luminance(v);
    float targetLuminance = l / (1.0f + l);
    return xyz2rgb(changeLuminance(v, targetLuminance));
}

// https://gist.github.com/SpineyPete/ebf9619f009318536c6da48209894fed
float3 aces(float3 color) {

    // ACES filmic tonemapper with highlight desaturation ("crosstalk").
    // Based on the curve fit by Krzysztof Narkowicz.
    // https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/

    const float slope = 12.0f; // higher values = slower rise.

    // Store grayscale as an extra channel.
    float4 x = float4(
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

    float4 tonemap = clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
    float t = x.a;

    t = t * t / (slope + t);

    // Return after desaturation step.
    return lerp(tonemap.rgb, tonemap.aaa, t);
}

[shader("pixel")]
float4 main(
    uniform ToneMappingOption option,
    in float2 uv) {
    float4 pixel = inputImage.Sample(nearestSampler, uv);
    switch(option) {
        case ToneMappingOption::None:
            return pixel;

        case ToneMappingOption::Reinhard:
            return float4(reinhard(pixel.rgb), pixel.a);

        case ToneMappingOption::Aces:
            return float4(aces(pixel.rgb), pixel.a);
    }
    return pixel;
}