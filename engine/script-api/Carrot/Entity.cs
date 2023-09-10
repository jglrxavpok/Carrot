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

        public override string ToString() { 
            return GetName();
        }

        public static bool operator==(Entity a, Entity b) {
            if (ReferenceEquals(a, b)) {
                return true;
            }
            if (ReferenceEquals(a, null)) {
                return false;
            }
            if (ReferenceEquals(b, null)) {
                return false;
            }

            return a._userPointer == b._userPointer && a._id == b._id;
        }

        public static bool operator!=(Entity a, Entity b) {
            return !(a == b);
        }

        public override bool Equals(object obj) {
            if (obj is Entity e) {
                return this == e;
            }

            return false;
        }


        /**
         * Does this object point to an existing entity?
         * The entity can be a null entity, or it may have been removed from the current world
         */
        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern bool Exists();
        
        /**
         * Mark this entity for removal, will be removed next tick
         */
        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern void Remove();
        
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
         * Duplicate this entity and its children
         * Components in the duplicated hierarchy that reference entities from said hierarchy, will have their reference
         *  updated to point to the duplicated entities. 
         */
        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern Entity Duplicate();

        /**
         * Returns the component on this entity corresponding to the given type, or null if none.
         */
        public T GetComponent<T>() where T : IComponent {
            return (T)GetComponent(typeof(T));
        }

        /**
         * Convenience method to get the transform of an entity. May return null
         */
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
