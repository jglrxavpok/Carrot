using System.Runtime.CompilerServices;

namespace Carrot.Input {
    public static class Aim {
        /**
         * Computes the (worldspace) direction based on the position on screen.
         * Can be used for raycast based on where the mouse is, for example
         */
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern Vec3 GetDirectionFromScreen(CameraComponent camera, Vec2 screenPositionInPixels);
    }
}
