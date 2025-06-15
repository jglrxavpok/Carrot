module camera;
import base;

public struct CameraBufferObject {
    public mat4 view;
    public mat4 inverseView;

    public mat4 jitteredProjection;
    public mat4 nonJitteredProjection;

    public mat4 inverseJitteredProjection;
    public mat4 inverseNonJitteredProjection;

    public vec4 frustumPlanes[6]; /* xyz = normal, w = distance from origin*/
}

public struct Camera {
    public CameraBufferObject cbo;
    public CameraBufferObject previousFrameCBO;
}