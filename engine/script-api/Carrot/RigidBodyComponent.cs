using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using Carrot.ComponentPropertyAttributes;
using Carrot.Physics;
using Action = Carrot.Input.Action;

namespace Carrot {
    /**
     * Represents the rigidbody attached to an entity
     */
    [InternalComponent]
    public class RigidBodyComponent: IComponent {
        public Vec3 Velocity {
            get => _GetVelocity();
            set => _SetVelocity(value);
        }
        public Vec3 AngularVelocityInLocalSpace {
            get => _GetAngularVelocityInLocalSpace();
        }

        private class Listeners {
            public List<Action<Entity>> contactAddedListeners = new List<Action<Entity>>();
            public List<Action<Entity>> contactPersistedListeners = new List<Action<Entity>>();
        }

        private bool RegisteredForContacts {
            get => _GetRegisteredForContacts();
            set => _SetRegisteredForContacts(value);
        }

        private Listeners CallbackHolders {
            get => _GetCallbacksHolder();
            set => _SetCallbacksHolder(value);
        }

        private RigidBodyComponent(Entity owner) : base(owner) {
            if (CallbackHolders == null) {
                CallbackHolders = new Listeners();
            }
        }

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

        public void RegisterContactAddedListener(Action<Entity> listener) {
            CallbackHolders.contactAddedListeners.Add(listener);
            if (!RegisteredForContacts) {
                RegisteredForContacts = true;
                _RegisterForContacts();
            }
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern Vec3 _GetAngularVelocityInLocalSpace();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern Vec3 _GetVelocity();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern void _SetVelocity(Vec3 v);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern void _RegisterForContacts();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern bool _GetRegisteredForContacts();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern void _SetRegisteredForContacts(bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern Listeners _GetCallbacksHolder();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern void _SetCallbacksHolder(Listeners value);

        /**
         * Gets the velocity of a given point of this rigidbody
         */
        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern Vec3 GetPointVelocity(Vec3 point);

        /**
         * Adds the given force at a point on the rigidbody
         */
        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern void AddForceAtPoint(Vec3 force, Vec3 point);

        /**
         * Adds the given force at the center of mass, in the coordinate system of the rigidbody
         */
        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern void AddRelativeForce(Vec3 force);
        
        // called by engine when contact are found
        private void _OnContactAdded(Entity otherBody) {
            foreach (var action in CallbackHolders.contactAddedListeners) {
                action(otherBody);
            }
        }
        
        private void _OnContactPersisted(Entity otherBody) {
            foreach (var action in CallbackHolders.contactPersistedListeners) {
                action(otherBody);
            }
        }
    }
}
