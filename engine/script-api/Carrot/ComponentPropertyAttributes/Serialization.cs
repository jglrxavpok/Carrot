using System;

namespace Carrot.ComponentPropertyAttributes {
    
    /**
     * Select which ID is used for serialization
     */
    public class SerializationNameAttribute: Attribute {
        public SerializationNameAttribute(string value) {
            this.Value = value;
        }
        
        public string Value { get;  }
    }
}
