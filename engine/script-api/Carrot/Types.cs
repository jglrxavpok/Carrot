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

        public float Dot(Vec2 other) {
            return X * other.X + Y * other.Y;
        }

        public static Vec2 operator +(Vec2 a, Vec2 b) {
            return new Vec2(a.X + b.X, a.Y + b.Y);
        }
        
        public static Vec2 operator -(Vec2 a, Vec2 b) {
            return new Vec2(a.X - b.X, a.Y - b.Y);
        }
        
        public static Vec2 operator -(Vec2 a) {
            return new Vec2(-a.X, -a.Y);
        }
        
        public static Vec2 operator *(Vec2 a, float s) {
            return new Vec2(a.X * s, a.Y * s);
        }
        
        public static Vec2 operator /(Vec2 a, float s) {
            return new Vec2(a.X / s, a.Y / s);
        }
        
        public static Vec2 operator *(float s, Vec2 a) {
            return new Vec2(a.X * s, a.Y * s);
        }
        
        public static Vec2 operator /(float s, Vec2 a) {
            return new Vec2(s / a.X, s / a.Y);
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

        public float Dot(Vec3 other) {
            return X * other.X + Y * other.Y + Z * other.Z;
        }

        public static Vec3 operator +(Vec3 a, Vec3 b) {
            return new Vec3(a.X + b.X, a.Y + b.Y, a.Z + b.Z);
        }
        
        public static Vec3 operator -(Vec3 a, Vec3 b) {
            return new Vec3(a.X - b.X, a.Y - b.Y, a.Z - b.Z);
        }
        
        public static Vec3 operator -(Vec3 a) {
            return new Vec3(-a.X, -a.Y, -a.Z);
        }
        
        public static Vec3 operator *(Vec3 a, float s) {
            return new Vec3(a.X * s, a.Y * s, a.Z * s);
        }
        
        public static Vec3 operator /(Vec3 a, float s) {
            return new Vec3(a.X / s, a.Y / s, a.Z / s);
        }
        
        public static Vec3 operator *(float s, Vec3 a) {
            return new Vec3(a.X * s, a.Y * s, a.Z * s);
        }
        
        public static Vec3 operator /(float s, Vec3 a) {
            return new Vec3(s / a.X, s / a.Y, s / a.Z);
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

        public override int GetHashCode() {
            unchecked {
                var hashCode = (int)data0;
                hashCode = (hashCode * 397) ^ (int)data1;
                hashCode = (hashCode * 397) ^ (int)data2;
                hashCode = (hashCode * 397) ^ (int)data3;
                return hashCode;
            }
        }
    }
}
