using Carrot.Audio;
using Carrot.ComponentPropertyAttributes;

namespace Carrot {
    public class SoundSourceComponent: IComponent {
        [FloatRange(min:0.0f, max:1.0f)]
        [FloatDefault(1.0f)]
        public float Gain;

        private const int MaxSources = 8;
        private readonly SoundSource[] _sources = new SoundSource[MaxSources];
        
        public SoundSourceComponent(Entity owner) : base(owner) { }
        
        public void Play(SFX sound, float pitch = 1.0f) {
            SoundSource source = _FindFreeSource();
            if (source != null) {
                source.SetGain(Gain);
                sound.Play(source, pitch);
            }
        }

        public void SetPosition(Vec3 newPosition) {
            foreach (var source in _sources) {
                source?.SetPosition(newPosition);
            }
        }

        /**
         * Looks for a free SoundSource inside this component.
         * Returns null if none.
         */
        private SoundSource _FindFreeSource() {
            for(int i = 0; i < _sources.Length; i++) {
                SoundSource source = _sources[i];
                if (source != null) {
                    if (!source.IsPlaying()) {
                        return source;
                    }
                } else {
                    _sources[i] = SoundSource.Create();
                    return _sources[i];
                }
            }

            return null;
        }
    }
}
