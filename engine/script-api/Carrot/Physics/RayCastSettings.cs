namespace Carrot.Physics {
    public class RayCastSettings {
        public RayCastSettings() {
            Origin = new Vec3();
            Dir = new Vec3();
        }
        
        public Vec3 Origin;
        public Vec3 Dir;
        public float MaxLength = 0.0f;

        /**
         * RigidBodies to ignore while doing raycast, can be null
         */
        public RigidBodyComponent[] IgnoreBodies = null;

        /**
         * Characters to ignore while doing raycast, can be null
         */
        public CharacterComponent[] IgnoreCharacters = null;

        /**
         * Name of layers to ignore while doing raycast, can be null
         */
        public string[] IgnoreLayers = null;
    }
}
