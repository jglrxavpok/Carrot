using System.Runtime.CompilerServices;

namespace Carrot.Audio {
    public class SFX: Object {
        protected SFX(ulong handle) : base(handle) { }

        /**
         * Loads the given SFX.
         * Loading an already loaded SFX will return the same instance of SFX.
         */
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern SFX Load(string vfsPath);

        /**
         * Plays this SFX through the given source
         */
        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern void Play(SoundSource source);
    }
}
