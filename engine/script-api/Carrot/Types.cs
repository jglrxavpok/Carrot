using System;

namespace Carrot {
    public struct Vec2 {
        public float X;
        public float Y;

        public Vec2(float x, float y) {
            this.X = x;
            this.Y = y;
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
    }

    public struct EntityID {
        private UInt32 data0;
        private UInt32 data1;
        private UInt32 data2;
        private UInt32 data3;
    }
}
