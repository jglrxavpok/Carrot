using System.Runtime.CompilerServices;

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
        private extern Vec3 _GetVelocity();
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern void _SetVelocity(Vec3 v);
    }
}
