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
         * RigidBody to ignore while doing raycast
         */
        public RigidBodyComponent IgnoreBody = null;

        /**s
         * Character to ignore while doing raycast
         */
        public CharacterComponent IgnoreCharacter = null;
    }
}
