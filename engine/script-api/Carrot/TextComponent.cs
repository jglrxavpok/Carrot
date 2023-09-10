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

        private TextComponent(Entity owner) : base(owner) {
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern string _GetText();
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern void _SetText(string newText);
    }
}
