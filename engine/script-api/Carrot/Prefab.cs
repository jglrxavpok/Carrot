using System;
using System.Runtime.CompilerServices;

namespace Carrot {
    public class Prefab {
        private UInt64 _pointer;
        
        private Prefab(UInt64 pointer) {
            this._pointer = pointer;
        }

        /// <summary>
        /// Loads the given prefab, for later instantiation
        /// </summary>
        /// <param name="path"></param>
        /// <returns></returns>
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern Prefab Load(string path);

        /// <summary>
        /// Instantiates this prefab into the scene. Returns the entity representing the newly created instance of the prefab
        /// </summary>
        /// <returns></returns>
        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern Entity Instantiate();
    }
}
