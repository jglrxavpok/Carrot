using System;

namespace Carrot.ComponentPropertyAttributes {
    /**
     * Specify the default value for a component field.
     * This is done because the default value in C# is not known before instantiating a component.
     */
    public class IntDefaultAttribute: Attribute {
        public IntDefaultAttribute(int value) {
            this.Value = value;
        }
        
        public int Value { get;  }
        
    }
}
