using System.Runtime.CompilerServices;

namespace Carrot.Audio {
    public class SoundSource: Object {
        protected SoundSource(ulong handle) : base(handle) { }
        
        /**
         * Creates a new instance of SoundSource (required because it has to talk to the engine code for this)
         */
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern SoundSource Create();
        
        /**
         * Changes the gain for this source
         */
        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern void SetGain(float gain);
        
        /**
         * Changes the world position of this source
         */
        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern void SetPosition(Vec3 position);
        
        /**
         * Is this source currently playing something?
         */
        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern bool IsPlaying();
    }
}
