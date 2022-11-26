/**
 * Performs a Gaussian blur in a single dimension. 'blurScale' is expected to be (1/textureWidth, 0) or (0, 1/textureHeight)
 */
/*vec4 _singleDimensionGaussianBlur(sampler2D toSample, vec2 uv, vec2 blurScale) {
    vec4    color  = texture(toSample, (uv + (vec2(-3.0) * blurScale.xy))) * ( 1.0/64.0);
            color += texture(toSample, (uv + (vec2(-2.0) * blurScale.xy))) * ( 6.0/64.0);
            color += texture(toSample, (uv + (vec2(-1.0) * blurScale.xy))) * (15.0/64.0);
            color += texture(toSample, (uv + (vec2(+0.0) * blurScale.xy))) * (20.0/64.0);
            color += texture(toSample, (uv + (vec2(+1.0) * blurScale.xy))) * (15.0/64.0);
            color += texture(toSample, (uv + (vec2(+2.0) * blurScale.xy))) * ( 6.0/64.0);
            color += texture(toSample, (uv + (vec2(+3.0) * blurScale.xy))) * ( 1.0/64.0);
    return color;
}

vec4 gaussianBlurH(sampler2D toSample, vec2 uv) {
    vec2 texSize = textureSize(toSample, 0);
    return _singleDimensionGaussianBlur(toSample, uv, vec2(1.0/texSize.x, 0.0));
}

vec4 gaussianBlurV(sampler2D toSample, vec2 uv) {
    vec2 texSize = textureSize(toSample, 0);
    return _singleDimensionGaussianBlur(toSample, uv, vec2(0.0, 1.0/texSize.y));
}*/

vec4 averageSample3x3(texture2D toSample, sampler s, vec2 uv) {
    vec4 color = vec4(0.0);
    vec2 invTexSize = 1.0 / textureSize(toSample, 0);
    [[unroll]] for(int dx = -1; dx <= 1; dx++) {
        [[unroll]] for(int dy = -1; dy <= 1; dy++) {
            color += texture(sampler2D(toSample, s), uv + invTexSize * vec2(dx, dy));
        }
    }
    return color / 9;
}