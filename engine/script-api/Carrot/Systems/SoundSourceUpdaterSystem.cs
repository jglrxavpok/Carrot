namespace Carrot.Systems {
    public class SoundSourceUpdaterSystem: LogicSystem {
        public SoundSourceUpdaterSystem(ulong handle) : base(handle) {
            AddComponent<TransformComponent>();
            AddComponent<SoundSourceComponent>();
        }
        
        public override void Tick(double deltaTime) {
            ForEachEntity<TransformComponent, SoundSourceComponent>((entity, transform, soundSource) => {
                soundSource.SetPosition(transform.WorldPosition);
            });
        }
    }
}
