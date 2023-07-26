namespace Carrot {
    public class IComponent {
        protected Entity owner;

        public IComponent(Entity owner) {
            this.owner = owner;
        }

        public Entity GetEntity() {
            return owner;
        }
    }
}
