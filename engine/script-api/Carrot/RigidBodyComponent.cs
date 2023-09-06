using System;
using System.Runtime.CompilerServices;
using Carrot.Physics;

namespace Carrot {
    /**
     * Represents the rigidbody attached to an entity
     */
    public class RigidBodyComponent: IComponent {
        public Vec3 Velocity {
            get => _GetVelocity();
            set => _SetVelocity(value);
        }
            
        private RigidBodyComponent(Entity owner) : base(owner) { }

        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern UInt64 GetColliderCount();

        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern Collider GetCollider(UInt64 colliderIndex);

        /**
         * Performs a raycast against the physics world
         * 'settings.dir' is expected to be normalized
         * 'raycastInfo' is modified if there is a hit
         * returns true iff there is an intersection
         */
        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern bool Raycast(RayCastSettings settings, RaycastInfo raycastInfo);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern Vec3 _GetVelocity();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern void _SetVelocity(Vec3 v);
    }
}
