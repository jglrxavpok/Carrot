module compute_fullscreen;

import modules.base;
import modules.gbuffer;
import modules.camera;


/**
 * Represents the information of a given pixel, extracted from the GBuffer.
 * If 'isSky' is true, the other fields are undefined.
 */
public struct PixelInfo {
    public bool isSky;

    public GBuffer gBuffer;
    public float3 worldNormal;
    public float3 worldTangent;
    public float3 worldPosition;
    public float3 cameraPos; // helper to avoid writing it in code in each fullscreen shader

    public __init(in CameraBufferObject camera, in GBuffer gBuffer) {
        loadFrom(camera, gBuffer);
    }

    public __init(in CameraBufferObject camera, in GBufferInputs gBufferInputs, float2 uv) {
        loadFrom(camera, gBufferInputs.read(uv));
    }

    [mutating]
    private void loadFrom(in CameraBufferObject camera, in GBuffer gBufferIn) {
        float depth = gBufferIn.readDepth;
        if(depth >= 1.0) {
            isSky = true;
            return;
        }

        isSky = false;
        gBuffer = gBufferIn;
        
        cameraPos = (camera.inverseView * float4(0, 0, 0, 1)).xyz;
        const float4 hWorldPos = camera.inverseView * float4(gBuffer.viewPosition, 1);
        worldPosition = hWorldPos.xyz / hWorldPos.w;

        const float3x3 cboNormalView = transpose(inverse(float3x3(camera.view)));
        const float3x3 inverseNormalView = inverse(cboNormalView);
        worldNormal = inverseNormalView * gBuffer.viewTBN[2];
        worldTangent = inverseNormalView * gBuffer.viewTBN[0];
    }
}