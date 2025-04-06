using System;
using System.Runtime.CompilerServices;
using Carrot.ComponentPropertyAttributes;

namespace Carrot {
    [InternalComponent]
    public class TextComponent: IComponent {

        public string Text {
            get => _GetText();
            set => _SetText(value);
        }
        
        public Vec4 Color {
            get => _GetColor();
            set => _SetColor(value);
        }

        private TextComponent(Entity owner) : base(owner) {
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern string _GetText();
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern void _SetText(string newText);
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern Vec4 _GetColor();
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern void _SetColor(Vec4 v);
    }
}
