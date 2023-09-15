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
        private readonly BitArray _components = new BitArray(GetMaxComponentCount());
        private readonly int[] _indices = new int[GetMaxComponentCount()];

        public Signature() {
            
        }

        public void AddComponent<T>() where T : IComponent {
            _components[GetComponentIndex<T>()] = true;
            _Reindex();
        }

        public bool HasComponent<T>() where T : IComponent {
            return _components[GetComponentIndex<T>()];
        }

        /**
         * Returns the index of the component inside this signature:
         * the index is the number of 1s inside _components before reaching the index of the given component
         */
        public int GetIndex<T>() where T : IComponent {
            int compID = GetComponentIndex<T>();
            if (!_components[compID]) {
                throw new ArgumentException($"Component {typeof(T).FullName} is not present inside signature");
            }
            return _indices[compID];
        }

        private static int GetComponentIndex<T>() where T : IComponent {
            var type = typeof(T);
            return GetComponentIndex(type.Namespace, type.Name);
        }

        private void _Reindex() {
            int index = 0;
            for (int i = 0; i < GetMaxComponentCount(); i++) {
                if (_components[i]) {
                    _indices[i] = index++;
                } else {
                    _indices[i] = -1;
                }
            }
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern int GetComponentIndex(string typeNamespace, string typeClass);
    }
    
    public class EntityWithComponents {
        public readonly Entity Entity;
        public readonly IComponent[] Components;

        public EntityWithComponents(Entity e, IComponent[] comps) {
            Entity = e;
            Components = comps;
        }
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
            int index0 = _signature.GetIndex<T0>();
            foreach (var entity in LoadEntities()) {
                action(entity.Entity, (T0)entity.Components[index0]);
            }
        }

        /**
         * DO NOT STORE THE COMPONENTS
         */
        public void ForEachEntity<T0, T1>(Action<Entity, T0, T1> action) where T0 : IComponent where T1: IComponent {
            int index0 = _signature.GetIndex<T0>();
            int index1 = _signature.GetIndex<T1>();
            foreach (var entity in LoadEntities()) {
                action(entity.Entity, (T0) entity.Components[index0], (T1) entity.Components[index1]);
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
            int index0 = _signature.GetIndex<T0>();
            int index1 = _signature.GetIndex<T1>();
            int index2 = _signature.GetIndex<T2>();
            foreach (var entity in LoadEntities()) {
                action(entity.Entity, (T0) entity.Components[index0], (T1) entity.Components[index1], (T2) entity.Components[index2]);
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
            int index0 = _signature.GetIndex<T0>();
            int index1 = _signature.GetIndex<T1>();
            int index2 = _signature.GetIndex<T2>();
            int index3 = _signature.GetIndex<T3>();
            foreach (var entity in LoadEntities()) {
                action(entity.Entity, (T0) entity.Components[index0], (T1) entity.Components[index1], (T2) entity.Components[index2], (T3) entity.Components[index3]);
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
            where T4: IComponent {
            int index0 = _signature.GetIndex<T0>();
            int index1 = _signature.GetIndex<T1>();
            int index2 = _signature.GetIndex<T2>();
            int index3 = _signature.GetIndex<T3>();
            int index4 = _signature.GetIndex<T4>();
            foreach (var entity in LoadEntities()) {
                action(entity.Entity, (T0) entity.Components[index0], (T1) entity.Components[index1], (T2) entity.Components[index2], (T3) entity.Components[index3], (T4) entity.Components[index4]);
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
        private extern EntityWithComponents[] LoadEntities();
        
        /**
         * Ask engine to send list of entities with matching components
         */
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern Entity[] _Query(string[] types);
    }
    
    public class ECS {
        
    }
}
