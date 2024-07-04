using System.Runtime.CompilerServices;

namespace Carrot.Components {
    public class AnimatedModelComponent: IComponent {
        public int AnimationIndex {
            get => _GetAnimationIndex();
            set => _SetAnimationIndex(value);
        }

        public float AnimationTime {
            get => _GetAnimationTime();
            set => _SetAnimationTime(value);
        }
        
        public AnimatedModelComponent(Entity owner) : base(owner) { }

        /**
         * Modifies AnimationIndex to point to the animation with the given name
         * Returns true if this works, false if the animation does not exist
         */
        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern bool SelectAnimation(string animationName);
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern int _GetAnimationIndex();
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern void _SetAnimationIndex(int newIndex);
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern float _GetAnimationTime();
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern void _SetAnimationTime(float newTime);
    }
}
