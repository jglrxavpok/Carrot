using System;

namespace Carrot.ComponentPropertyAttributes {
    
    /**
     * Select which ID is used for user-facing interfaces (ie inside the editor)
     */
    public class DisplayNameAttribute: Attribute {
        public DisplayNameAttribute(string value) {
            this.Value = value;
        }
        
        public string Value { get;  }
    }
}
