using System.Runtime.CompilerServices;
using Carrot.ComponentPropertyAttributes;

namespace Carrot.Components {
    [InternalComponent]
    public class ModelComponent: IComponent {
        public Vec4 Color {
            get => _GetColor();
            set => _SetColor(value);
        }
        
        public ModelComponent(Entity owner) : base(owner) { }
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern Vec4 _GetColor();
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern void _SetColor(Vec4 v);
    }
}
