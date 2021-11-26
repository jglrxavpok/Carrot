float getNear(mat4 perspectiveMatrix) {
    float m22 = perspectiveMatrix[2][2];
    float m32 = perspectiveMatrix[3][2];
    return m22 / (m32 + 1.0);
}

float getFar(mat4 perspectiveMatrix) {
    float m22 = perspectiveMatrix[2][2];
    float m32 = perspectiveMatrix[3][2];
    return m22 / (m32 - 1.0);
}

vec2 getNearFar(mat4 perspectiveMatrix) {
    float m22 = perspectiveMatrix[2][2];
    float m32 = perspectiveMatrix[3][2];
    return vec2(getNear(perspectiveMatrix), getFar(perspectiveMatrix));
}