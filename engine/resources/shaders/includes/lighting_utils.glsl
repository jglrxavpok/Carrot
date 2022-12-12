float luminance(vec3 v) {
    return dot(v, vec3(0.2126f, 0.7152f, 0.0722f));
}

// https://diharaw.github.io/post/adventures_in_hybrid_rendering/
float variance(vec2 moments) {
    return max(0.0f, moments.g - moments.r * moments.r);
}