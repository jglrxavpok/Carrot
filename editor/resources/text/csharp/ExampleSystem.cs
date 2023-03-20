using Carrot;
using System;

public class ExampleSystem : LogicSystem {

    public ExampleSystem(ulong handle): base(handle) {
        AddComponent<TransformComponent>();
    }

    public override void Tick(double deltaTime) {
        ForEachEntity<TransformComponent>((entity, transform) => {
                Console.WriteLine("-- Entity: "+entity.GetName());
                var transformLocalPosition = transform.LocalPosition;
                Console.WriteLine("X = " + transformLocalPosition.X);
                transformLocalPosition.X += (float)deltaTime;
                Console.WriteLine("X' = " + transformLocalPosition.X);
                transform.LocalPosition = transformLocalPosition;
        });
    }
}