using System;

namespace Carrot.ComponentPropertyAttributes {
    /**
     * Specify the default value for a component field.
     * This is done because the default value in C# is not known before instantiating a component.
     */
    public class FloatDefaultAttribute: Attribute {
        public FloatDefaultAttribute(float value) {
            this.Value = value;
        }
        
        public float Value { get;  }
        
    }
}
