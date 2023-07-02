using System;

namespace Carrot {
    public struct Vec2 {
        public float X;
        public float Y;

        public Vec2(float x, float y) {
            this.X = x;
            this.Y = y;
        }

        public float LengthSquared() {
            return X * X + Y * Y;
        }

        public float Length() {
            return (float)Math.Sqrt(LengthSquared());
        }

        public Vec2 Normalize() {
            float length = Length();
            X /= length;
            Y /= length;
            return this;
        }
    }
    
    public struct Vec3 {
        public float X;
        public float Y;
        public float Z;
        
        public Vec3(float x, float y, float z) {
            this.X = x;
            this.Y = y;
            this.Z = z;
        }
        
        public float LengthSquared() {
            return X * X + Y * Y + Z * Z;
        }

        public float Length() {
            return (float)Math.Sqrt(LengthSquared());
        }
        
        public Vec3 Normalize() {
            float length = Length();
            X /= length;
            Y /= length;
            Z /= length;
            return this;
        }
    }

    public struct EntityID {
        private UInt32 data0;
        private UInt32 data1;
        private UInt32 data2;
        private UInt32 data3;
    }
}
