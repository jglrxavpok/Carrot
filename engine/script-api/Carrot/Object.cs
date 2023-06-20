using System;

namespace Carrot {
    /**
     * Object that can be kept in C# code
     */
    public class Object {
        private UInt64 _handle = 0;
        
        protected Object(ulong handle) {
            this._handle = handle;
        }
    }
    
    /**
     * Reference to an engine object, but not safe to keep inside code
     */
    public class Reference {
        private UInt64 _handle = 0;
        
        protected Reference(ulong handle) {
            this._handle = handle;
        }
    }
}
