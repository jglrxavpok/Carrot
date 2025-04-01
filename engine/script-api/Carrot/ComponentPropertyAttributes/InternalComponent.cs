using System;

namespace Carrot.ComponentPropertyAttributes {
    /**
     * Used for hardcoded components: components which have a C++ counterpart that should be used inside the editor, instead
     *  of this C# wrapper.
     *
     * Note that these components are instantiated when queried, they cannot store state!
     */
    public class InternalComponentAttribute: Attribute {
        
    }
}
