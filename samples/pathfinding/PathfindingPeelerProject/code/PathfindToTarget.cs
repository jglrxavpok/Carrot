using Carrot;

namespace PathfindingPeelerProject {
    public class NavProgress {
        public int CurrentWaypoint = 0;
        public int NextWaypoint = 1;
        public float ProgressTowardsNextWaypoint = 0.0f;
    }
    
    public class PathfindToTargetComponent: IComponent {
        public Entity Target = null;
        public Entity Navmesh = null;

        public bool Go = false;

        internal NavPath navPath = new NavPath();
        internal NavProgress navProgress = new NavProgress();
        
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
                TransformComponent targetTransform = pathfindComponent.Target.GetComponent<TransformComponent>();
                Vec3 targetPosition = targetTransform.WorldPosition;
                
                if (pathfindComponent.Go) {
                    pathfindComponent.Go = false;
                    

                    pathfindComponent.navProgress = new NavProgress();
                    pathfindComponent.navPath = pathfindComponent.Navmesh.GetComponent<NavMeshComponent>().PathFind(currentPosition, targetPosition);
                }

                /*
                targetTransform.LocalPosition = pathfindComponent.Navmesh.GetComponent<NavMeshComponent>()
                    .GetClosestPointInMesh(currentPosition);
*/
                
                if (pathfindComponent.navPath.Waypoints.Length > 0) {
                    Vec3[] waypoints = pathfindComponent.navPath.Waypoints;
                    NavProgress progress = pathfindComponent.navProgress;

                    if (progress.CurrentWaypoint+1 < waypoints.Length) {
                        float distance = new Vec3(
                            waypoints[progress.CurrentWaypoint + 1].X - waypoints[progress.CurrentWaypoint].X,
                            waypoints[progress.CurrentWaypoint + 1].Y - waypoints[progress.CurrentWaypoint].Y,
                            waypoints[progress.CurrentWaypoint + 1].Z - waypoints[progress.CurrentWaypoint].Z
                        ).Length();
                        float targetSpeed = 5.0f;
                        transformComponent.LocalPosition = waypoints[progress.CurrentWaypoint].Lerp(waypoints[progress.CurrentWaypoint+1], progress.ProgressTowardsNextWaypoint);
                        progress.ProgressTowardsNextWaypoint += (float)deltaTime * targetSpeed / distance;
                        if(progress.ProgressTowardsNextWaypoint >= 1.0f) {
                            progress.ProgressTowardsNextWaypoint = 0.0f;
                            progress.CurrentWaypoint++;
                        }
                    }
                }

            });
        }
    }
}
