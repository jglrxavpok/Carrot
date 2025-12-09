using System.Runtime.CompilerServices;

namespace Carrot.Components
{
    public class LightComponent: IComponent
    {
        public float Intensity {
            get => _GetIntensity();
            set => _SetIntensity(value);
        }
        public bool Enabled {
            get => _GetEnabled();
            set => _SetEnabled(value);
        }
        
        public LightComponent(Entity owner) : base(owner)
        {
        }
        
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern float _GetIntensity();
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern void _SetIntensity(float newValue);
        
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern bool _GetEnabled();
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern void _SetEnabled(bool newValue);
    }
}