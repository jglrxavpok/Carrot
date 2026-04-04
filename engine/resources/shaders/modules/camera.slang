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
    public float2 jitter;

    // Checks if the sphere with given center and radius is not outside the frustum (ie fully or partially inside frustum)
    public bool frustumCheck(float3 center, float radius) {
        for(int i = 0; i < 6; i++) {
            if(getSignedDistanceToPlane(frustumPlanes[i].xyz, frustumPlanes[i].w, center) < -radius) {
                return false;
            }
        }
        return true;
    }
}

public struct Camera {
    ConstantBuffer<CameraBufferObject> cbo;
    ConstantBuffer<CameraBufferObject> previousFrameCBO;

    public CameraBufferObject CurrentFrame() {
        return cbo;
    }

    public CameraBufferObject PreviousFrame() {
        return previousFrameCBO;
    }
}