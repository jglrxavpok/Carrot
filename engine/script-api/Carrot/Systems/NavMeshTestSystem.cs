using System;
using Carrot.Components;

namespace Carrot.Systems
{
    public class NavMeshTestSystem: LogicSystem
    {
        public NavMeshTestSystem(ulong handle) : base(handle)
        {
            AddComponent<NavMeshTestComponent>();
            AddComponent<NavMeshComponent>();
        }

        public override void Tick(double deltaTime)
        {
            ForEachEntity<NavMeshTestComponent, NavMeshComponent>((entity, navMeshTest, navMesh) =>
            {
                bool valid = true;
                if (navMeshTest.Start == null)
                {
                    Console.WriteLine($"[NavMeshTest] 'Start' of entity {entity.GetName()} is not set");
                    valid = false;
                }
                if (navMeshTest.End == null)
                {
                    Console.WriteLine($"[NavMeshTest] 'End' of entity {entity.GetName()} is not set");
                    valid = false;
                }

                if (!valid)
                {
                    return;
                }

                NavPath path = navMesh.PathFind(navMeshTest.Start.GetTransform().WorldPosition,
                    navMeshTest.End.GetTransform().WorldPosition);
                float z = navMeshTest.Start.GetTransform().WorldPosition.Z;

                for (int i = 0; i < path.Waypoints.Length-1; i++)
                {
                    Vec3 start = path.Waypoints[i];
                    Vec3 end = path.Waypoints[i + 1];

                    start.Z = z;
                    end.Z = z;
                    Debug.DrawLine(start, end, new Color(0, 1, 0, 1));
                }

            });
        }
    }
}