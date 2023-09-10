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
        
        public Vec2 Lerp(Vec3 other, float t) {
            float oneMinusT = 1.0f - t;
            return new Vec2(
                other.X * t + X * oneMinusT,
                other.Y * t + Y * oneMinusT
            );
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

        public Vec3 Lerp(Vec3 other, float t) {
            float oneMinusT = 1.0f - t;
            return new Vec3(
                other.X * t + X * oneMinusT,
                other.Y * t + Y * oneMinusT,
                other.Z * t + Z * oneMinusT
            );
        }
    }

    public struct EntityID {
        private UInt32 data0;
        private UInt32 data1;
        private UInt32 data2;
        private UInt32 data3;
        
        public static bool operator==(EntityID a, EntityID b) {
            return
                a.data0 == b.data0
                && a.data1 == b.data1
                && a.data2 == b.data2
                && a.data3 == b.data3;
        }

        public static bool operator!=(EntityID a, EntityID b) {
            return !(a == b);
        }

        public override bool Equals(object obj) {
            if (obj is EntityID e) {
                return this == e;
            }

            return false;
        }
    }
}
