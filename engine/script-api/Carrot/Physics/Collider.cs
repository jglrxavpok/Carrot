using System.Runtime.CompilerServices;

namespace Carrot.Physics {
    public class RaycastInfo {
        public Vec3 WorldPoint = new Vec3();
        public Vec3 WorldNormal = new Vec3();
        
        /**
         * Length from start point to intersection point
         */
        public float T = -1.0f;
        
        public Collider Collider = null;
    }
    
    public class Collider: Reference {
        private Collider(ulong handle) : base(handle) { }

        /**
         * Performs a raycast against this collider (NOT a shape-cast !)
         * 'dir' is expected to be normalized
         * 'raycastInfo' is modified if there is a hit
         * returns true iff there is an intersection
         */
        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern bool Raycast(Vec3 start, Vec3 dir, float maxLength, ref RaycastInfo raycastInfo);

        /**
         * Returns the entity associated to this collider
         */
        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern Entity GetEntity();
    }
}
