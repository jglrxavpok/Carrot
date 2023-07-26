using System.Runtime.CompilerServices;

namespace Carrot {
    public class CharacterComponent: IComponent {
        public Vec3 Velocity {
            get => _GetVelocity();
            set => _SetVelocity(value);
        }
        
        public CharacterComponent(Entity owner) : base(owner) { }

        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern void EnablePhysics();

        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern void DisablePhysics();

        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern bool IsOnGround();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern Vec3 _GetVelocity();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern void _SetVelocity(Vec3 velocity);
    }
}
