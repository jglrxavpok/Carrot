#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int32 : enable
#extension GL_EXT_buffer_reference : enable

layout (local_size_x = 1) in;
layout (local_size_y = 1) in;

layout(push_constant) uniform P {
    uint64_t address;
} push;

layout(buffer_reference) buffer PointerToUint { uint32_t v; };

void main() {
    PointerToUint target = PointerToUint(push.address);
    target.v = 0;
}