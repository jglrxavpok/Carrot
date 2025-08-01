using System;
using System.Runtime.CompilerServices;
using Carrot.ComponentPropertyAttributes;

namespace Carrot {
    [InternalComponent]
    public class TransformComponent: IComponent {

        public Vec3 LocalPosition {
            get => _GetLocalPosition();
            set => _SetLocalPosition(value);
        }
        
        public Vec3 LocalScale {
            get => _GetLocalScale();
            set => _SetLocalScale(value);
        }

        /**
         * Allows to set the rotation with euler angles (in radians).
         * Order is around X axis, Y axis, and Z axis
         */
        public Vec3 EulerAngles {
            get => _GetEulerAngles();
            set => _SetEulerAngles(value);
        }
        
        /**
         * World position of this entity (ie takes into account parents' position)
         * Writing to LocalPosition immediately updates WorldPosition too
         */
        public Vec3 WorldPosition {
            get => _GetWorldPosition();
            // TODO: setter
        }
        
        private TransformComponent(Entity owner) : base(owner) {
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern Vec3 _GetLocalPosition();
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern void _SetLocalPosition(Vec3 newPosition);
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern Vec3 _GetLocalScale();
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern void _SetLocalScale(Vec3 newScale);
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern Vec3 _GetEulerAngles();
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern void _SetEulerAngles(Vec3 newScale);
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern Vec3 _GetWorldPosition();
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern Vec3[] _GetWorldUpForwardVectors();

        public void GetWorldUpForwardVectors(ref Vec3 up, ref Vec3 forward)
        {
            Vec3[] array = _GetWorldUpForwardVectors();
            up = array[0];
            forward = array[1];
        }
    }
}
