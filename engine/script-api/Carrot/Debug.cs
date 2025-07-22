using System.Collections.Generic;

namespace Carrot
{
    public static class Debug
    {
        public struct LineDesc
        {
            public Vec3 A;
            public Vec3 B;
            public Color Color;

            public LineDesc(Vec3 a, Vec3 b, Color color)
            {
                A = a;
                B = b;
                Color = color;
            }
        }
        
        private static List<LineDesc> _lines = new List<LineDesc>();
        
        /**
         * Draws a world-space line from 'a' to 'b', with the given color
         */
        public static void DrawLine(Vec3 a, Vec3 b, Color lineColor)
        {
            _lines.Add(new LineDesc(a, b, lineColor));
        }

        /**
         * Gets the lines drawn by C# code and prepares them for the engine.
         * Not intended for use in C#
         */
        public static LineDesc[] GetDrawnLines()
        {
            LineDesc[] array = new LineDesc[_lines.Count];
            _lines.CopyTo(array);
            _lines.Clear();
            return array;
        }
    }
}