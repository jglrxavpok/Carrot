using System.Runtime.CompilerServices;

namespace Carrot.Input {
    public static class Utils {
        /**
         * Computes the (worldspace) direction based on the position on screen.
         * Can be used for raycast based on where the mouse is, for example
         */
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern Vec3 GetDirectionFromScreen(CameraComponent camera, Vec2 screenPositionInPixels);

        /**
         * Locks the cursor to the center of the screen, until the game is closed, or UngrabCursor is called.
         * To help with edition, the game should call this every frame the cursor needs to be grabbed.
         */
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void GrabCursor();
        
        /**
         * Unlocks the cursor from the center of the screen. Undoes GrabCursor(). Does nothing if the cursor is not grabbed
         */
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void UngrabCursor();
    }
    
}
