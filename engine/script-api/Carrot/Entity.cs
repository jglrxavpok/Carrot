using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using static Carrot.Utilities;

namespace Carrot {

    using EntityEnumerator = IEnumerator<Entity>;

    /**
    * Represents an entity inside the scene
    */
    public class Entity {
        private EntityID _id;
        private UInt64 _userPointer;
        
        private Entity(EntityID id, UInt64 userPointer) {
            this._id = id;
            this._userPointer = userPointer;
        }
        
        /**
         * Name of this entity
         */
        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern string GetName();

        /**
         * Children of this entity.
         */
        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern EntityEnumerator GetChildren();

        /**
         * Returns the parent of this entity, or null if none.
         */
        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern Entity GetParent();
        
        /**
         * Changes the parent of this entity. 'null' is valid and is used to remove its parent.
         * Does NOT fix local transform to keep the same world transform
         */
        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern void SetParent(Entity newParent);
        
        /**
         * Changes the parent of this entity. 'null' is valid and is used to remove its parent.
         * Difference with SetParent: this modifies the entity transform to keep the same world transform after changing parent
         */
        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern void ReParent(Entity newParent);

        /**
         * Returns the component on this entity corresponding to the given type, or null if none.
         */
        public T GetComponent<T>() where T : IComponent {
            return (T)GetComponent(typeof(T));
        }

        public TransformComponent GetTransform() { 
            return GetComponent<TransformComponent>();
        }

        private IComponent GetComponent(Type type) {
            return GetComponent(type.Namespace, type.Name);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern IComponent GetComponent(string namespaceName, string className);
    }
}
