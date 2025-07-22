namespace Carrot.Components
{
    /**
     * Component used by NavMeshTestSystem to generate a path between two points
     */
    public class NavMeshTestComponent: IComponent
    {
        /**
         * Entity that is used as a marker for the start of the test
         */
        public Entity Start;
        
        /**
         * Entity that is used as a marker for the end of the test
         */
        public Entity End;
        
        public NavMeshTestComponent(Entity owner) : base(owner)
        {
        }
    }
}