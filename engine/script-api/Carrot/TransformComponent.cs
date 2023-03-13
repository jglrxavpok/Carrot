using System;
using System.Runtime.CompilerServices;

namespace Carrot {
    public class TransformComponent: IComponent {

        public Vec3 LocalPosition {
            get => _GetLocalPosition();
            set => _SetLocalPosition(value);
        }

        private TransformComponent(Entity owner) : base(owner) {
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern Vec3 _GetLocalPosition();
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern void _SetLocalPosition(Vec3 newPosition);
    }
}
