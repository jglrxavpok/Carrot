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
    
    /**
     * Input Action that corresponds to a trigger-like input
     */
    public class FloatInputAction: Action {
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern FloatInputAction Create(string name);
        
        protected FloatInputAction(ulong handle) : base(handle) { }

        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern float GetValue();
    }
    
    /**
     * Input Action that corresponds to a joystick-like input
     */
    public class Vec2InputAction: Action {
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern Vec2InputAction Create(string name);
        
        protected Vec2InputAction(ulong handle) : base(handle) { }

        public Vec2 GetValue() {
            Vec2 result = new Vec2();
            _GetValue(ref result);
            return result;
        }
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern void _GetValue(ref Vec2 output);
    }
}
