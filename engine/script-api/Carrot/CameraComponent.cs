using System;
using Carrot.ComponentPropertyAttributes;

namespace Carrot {
    [InternalComponent]
    public class CameraComponent: IComponent {
        // TODO

        private CameraComponent(Entity owner) : base(owner) {
        }
    }
}
