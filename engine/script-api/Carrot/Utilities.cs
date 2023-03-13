using System;
using System.Runtime.CompilerServices;

namespace Carrot {
    public static class Utilities {
        public static void TODO(string message = "Something was not implemented") {
            throw new NotImplementedException(message);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern int GetMaxComponentCount();
    }
}
