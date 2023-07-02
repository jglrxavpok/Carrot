using System;
using System.Collections;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using static Carrot.Utilities;

namespace Carrot {

    using ComponentID = UInt64;
    
    /**
     * Represents the list of components available to a System
     */
    public class Signature {
        private BitArray _components = new BitArray(GetMaxComponentCount());

        public Signature() {
            
        }

        public void AddComponent<T>() where T : IComponent {
            _components[GetComponentID<T>()] = true;
        }

        public bool HasComponent<T>() where T : IComponent {
            return _components[GetComponentID<T>()];
        }

        private static int GetComponentID<T>() where T : IComponent {
            var type = typeof(T);
            return GetComponentID(type.Namespace, type.Name);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern int GetComponentID(string typeNamespace, string typeClass);
    }
    
    public abstract class System: Object {
        private Signature _signature = new Signature();
        
        protected System(ulong handle): base(handle) {
        }
        
        public void AddComponent<T>() where T: IComponent {
            _signature.AddComponent<T>();
        }
        
        public abstract void Tick(double deltaTime);
        
        // TODO: Render
        
        // because C# does not have variadic generics, this is how we will have to do for now...
        
        #region Act on entities
        
        /**
         * DO NOT STORE THE COMPONENTS
         */
        public void ForEachEntity<T0>(Action<Entity, T0> action) where T0: IComponent {
            //TODO("check signature compatibility");
            foreach (var entity in LoadEntities()) {
                action(entity, entity.GetComponent<T0>());
            }
        }

        /**
         * DO NOT STORE THE COMPONENTS
         */
        public void ForEachEntity<T0, T1>(Action<Entity, T0, T1> action) where T0 : IComponent where T1: IComponent {
            //TODO("check signature compatibility");
            foreach (var entity in LoadEntities()) {
                action(entity, entity.GetComponent<T0>(), entity.GetComponent<T1>());
            }
        }

        /**
         * DO NOT STORE THE COMPONENTS
         */
        public void ForEachEntity<T0, T1, T2>(Action<Entity, T0, T1, T2> action) 
            where T0 : IComponent 
            where T1: IComponent 
            where T2: IComponent 
        {
            //TODO("check signature compatibility");
            foreach (var entity in LoadEntities()) {
                action(entity, entity.GetComponent<T0>(), entity.GetComponent<T1>(), entity.GetComponent<T2>());
            }
        }

        /**
         * DO NOT STORE THE COMPONENTS
         */
        public void ForEachEntity<T0, T1, T2, T3>(Action<Entity, T0, T1, T2, T3> action) 
            where T0 : IComponent 
            where T1: IComponent 
            where T2: IComponent 
            where T3: IComponent 
        {
            //TODO("check signature compatibility");
            foreach (var entity in LoadEntities()) {
                action(entity, entity.GetComponent<T0>(), entity.GetComponent<T1>(), entity.GetComponent<T2>(), entity.GetComponent<T3>());
            }
        }

        /**
         * DO NOT STORE THE COMPONENTS
         */
        public void ForEachEntity<T0, T1, T2, T3, T4>(Action<Entity, T0, T1, T2, T3, T4> action) 
            where T0 : IComponent 
            where T1: IComponent 
            where T2: IComponent 
            where T3: IComponent 
            where T4: IComponent 
        {
            //TODO("check signature compatibility");
            foreach (var entity in LoadEntities()) {
                action(entity, entity.GetComponent<T0>(), entity.GetComponent<T1>(), entity.GetComponent<T2>(), entity.GetComponent<T3>(), entity.GetComponent<T4>());
            }
        }
        
        /**
         * Queries the entities matching the list of components.
         * Use this if you need to access entities which do NOT match this system signature
         * (ie does not match the components given by AddComponent). Otherwise, you should use ForEachEntity 
         */
        public Entity[] Query<T0>() 
            where T0 : IComponent {
            string[] componentTypes = new string[] {
                typeof(T0).Namespace,typeof(T0).Name 
            };
            return _Query(componentTypes);
        }
        
        /**
         * Queries the entities matching the list of components.
         * Use this if you need to access entities which do NOT match this system signature
         * (ie does not match the components given by AddComponent). Otherwise, you should use ForEachEntity 
         */
        public Entity[] Query<T0, T1>() 
            where T0 : IComponent 
            where T1 : IComponent 
        {
            string[] componentTypes = new string[] {
                typeof(T0).Namespace, typeof(T0).Name,
                typeof(T1).Namespace, typeof(T1).Name,
            };
            return _Query(componentTypes);
        }
        
        /**
         * Queries the entities matching the list of components.
         * Use this if you need to access entities which do NOT match this system signature
         * (ie does not match the components given by AddComponent). Otherwise, you should use ForEachEntity 
         */
        public Entity[] Query<T0, T1, T2>() 
            where T0 : IComponent 
            where T1 : IComponent 
            where T2 : IComponent 
        {
            string[] componentTypes = new string[] {
                typeof(T0).Namespace, typeof(T0).Name,
                typeof(T1).Namespace, typeof(T1).Name,
                typeof(T2).Namespace, typeof(T2).Name,
            };
            return _Query(componentTypes);
        }
        
        /**
         * Queries the entities matching the list of components.
         * Use this if you need to access entities which do NOT match this system signature
         * (ie does not match the components given by AddComponent). Otherwise, you should use ForEachEntity 
         */
        public Entity[] Query<T0, T1, T2, T3>() 
            where T0 : IComponent 
            where T1 : IComponent 
            where T2 : IComponent 
            where T3 : IComponent 
        {
            string[] componentTypes = new string[] {
                typeof(T0).Namespace, typeof(T0).Name,
                typeof(T1).Namespace, typeof(T1).Name,
                typeof(T2).Namespace, typeof(T2).Name,
                typeof(T3).Namespace, typeof(T3).Name,
            };
            return _Query(componentTypes);
        }
        
        /**
         * Queries the entities matching the list of components.
         * Use this if you need to access entities which do NOT match this system signature
         * (ie does not match the components given by AddComponent). Otherwise, you should use ForEachEntity 
         */
        public Entity[] Query<T0, T1, T2, T3, T4>() 
            where T0 : IComponent 
            where T1 : IComponent 
            where T2 : IComponent 
            where T3 : IComponent 
            where T4 : IComponent 
        {
            string[] componentTypes = new string[] {
                typeof(T0).Namespace, typeof(T0).Name,
                typeof(T1).Namespace, typeof(T1).Name,
                typeof(T2).Namespace, typeof(T2).Name,
                typeof(T3).Namespace, typeof(T3).Name,
                typeof(T4).Namespace, typeof(T4).Name,
            };
            return _Query(componentTypes);
        }
        #endregion

        /**
         * Ask the engine to find an entity with a matching name.
         * Can get quite slow if there are many entities.
         */
        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern Entity FindEntityByName(string name);

        /**
         * Ask engine to send list of entities for this system
         */
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern Entity[] LoadEntities();
        
        /**
         * Ask engine to send list of entities with matching components
         */
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern Entity[] _Query(string[] types);
    }
    
    public class ECS {
        
    }
}
