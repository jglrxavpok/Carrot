using System.Runtime.CompilerServices;

namespace Carrot {
    public class NavPath {
        public Vec3[] Waypoints = new Vec3[0];
    }
    
    public class NavMeshComponent: IComponent {
        public NavMeshComponent(Entity owner) : base(owner) { }

        /// Closest point to 'p', which lies inside the navmesh
        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern Vec3 GetClosestPointInMesh(Vec3 p);
        
        /// Computes path from 'pointA' to 'pointB', first transforming pointA and pointB via a similar method to GetClosestPointInMesh first.
        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern NavPath PathFind(Vec3 a, Vec3 b);
    }
}
