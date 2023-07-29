using Carrot;

namespace PathfindingPeelerProject {
    public class PathfindToTargetComponent: IComponent {
        public Entity Target = null;
        public Entity Navmesh = null;
        
        public PathfindToTargetComponent(Entity owner) : base(owner) { }
    }

    public class PathfindToTargetSystem : LogicSystem {
        public PathfindToTargetSystem(ulong handle) : base(handle) {
            AddComponent<TransformComponent>();
            AddComponent<PathfindToTargetComponent>();
        }
        
        public override void Tick(double deltaTime) {
            ForEachEntity<TransformComponent, PathfindToTargetComponent>((entity, transformComponent, pathfindComponent) => {
                if (pathfindComponent.Target == null) {
                    return;
                }
                
                if (pathfindComponent.Navmesh == null) {
                    return;
                }

                Vec3 currentPosition = transformComponent.WorldPosition;
                Vec3 targetPosition = pathfindComponent.Target.GetComponent<TransformComponent>().WorldPosition;
                
                // TODO
            });
        }
    }
}
