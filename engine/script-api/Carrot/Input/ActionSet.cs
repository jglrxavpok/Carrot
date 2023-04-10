using System.Runtime.CompilerServices;

namespace Carrot.Input {
    /**
     * Action sets represent a group of actions that can be activated/deactivated all at once.
     * Use it to regroup actions logically
     */
    public class ActionSet: Object {
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern ActionSet Create(string name);

        protected ActionSet(ulong handle) : base(handle) { }

        public void Add(Action action) {
            _AddToActionSet(action);
        }

        /**
         * Activates the actions inside this set
         */
        public void Activate() {
            _ActivateActionSet();
        }
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern ActionSet _ActivateActionSet();
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern ActionSet _AddToActionSet(Action action);
    }
}
