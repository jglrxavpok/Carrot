using System.Runtime.CompilerServices;

namespace Carrot.Audio {
    public class Music: Object {
        protected Music(ulong handle) : base(handle) { }

        /**
         * Loads the given music.
         */
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern Music Load(string vfsPath);

        /**
         * Plays this Music
         */
        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern void Play();

        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern void SetLooping(bool loop);
    }
}
