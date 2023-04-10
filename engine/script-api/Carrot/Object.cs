using System;

namespace Carrot {
    public class Object {
        private UInt64 _handle = 0;
        
        protected Object(ulong handle) {
            this._handle = handle;
        }
    }
}
