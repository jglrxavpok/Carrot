using System.Runtime.CompilerServices;

namespace Carrot.Input {
    /**
     * I/O Action that can be remapped by the user.
     * Default bindings are given via SuggestBinding
     */
    public class Action : Object {
        // TODO
        
        protected Action(ulong handle) : base(handle) { }
        
        /**
         * Suggests a new binding for this action
         * An action can have multiple default bindings.
         */
        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern void SuggestBinding(string bindingStr);
    }
    
    /**
     * Input Action that corresponds to a button-like input
     */
    public class BoolInputAction: Action {
        // TODO
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern BoolInputAction Create(string name);
        
        protected BoolInputAction(ulong handle) : base(handle) { }

        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern bool IsPressed();

        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern bool WasJustPressed();

        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern bool WasJustReleased();
    }
}
