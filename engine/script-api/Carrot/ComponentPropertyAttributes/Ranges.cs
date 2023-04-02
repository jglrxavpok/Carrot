using System;

namespace Carrot.ComponentPropertyAttributes {
    /**
     * Attribute that can be added to fields of components to limit the range inside the editor (no effect at runtime)
     */
    public class IntRangeAttribute : Attribute {
        public IntRangeAttribute(int min, int max) {
            this.Min = min;
            this.Max = max;
        }

        public int Min { get; }
        public int Max { get; }
    }
    
    /**
     * Attribute that can be added to fields of components to limit the range inside the editor (no effect at runtime)
     */
    public class FloatRangeAttribute : Attribute {
        public FloatRangeAttribute(float min, float max) {
            this.Min = min;
            this.Max = max;
        }

        public float Min { get; }
        public float Max { get; }
    }
}
