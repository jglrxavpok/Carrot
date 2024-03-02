using System.Runtime.CompilerServices;

namespace Carrot {
    public class KinematicsComponent: IComponent {
        public Vec3 LocalSpaceVelocity {
            get => _GetLocalVelocity();
            set => _SetLocalVelocity(value);
        }
        
        public KinematicsComponent(Entity owner) : base(owner) { }
        
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern Vec3 _GetLocalVelocity();
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern void _SetLocalVelocity(Vec3 newPosition);
    }
}
