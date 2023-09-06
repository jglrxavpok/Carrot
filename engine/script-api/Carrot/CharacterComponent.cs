using System.Runtime.CompilerServices;
using Carrot.Physics;

namespace Carrot {
    public class CharacterComponent: IComponent {
        public Vec3 Velocity {
            get => _GetVelocity();
            set => _SetVelocity(value);
        }
        
        public CharacterComponent(Entity owner) : base(owner) { }
        
        /**
         * Performs a raycast against the physics world
         * 'settings.dir' is expected to be normalized
         * 'raycastInfo' is modified if there is a hit
         * returns true iff there is an intersection
         */
        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern bool Raycast(RayCastSettings settings, RaycastInfo raycastInfo);

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
