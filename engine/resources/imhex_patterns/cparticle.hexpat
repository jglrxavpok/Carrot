struct PreHeader {
    char magic[16];
    u32 version;
};

struct TextureEntry {
    u32 nameOffset;
    u32 nameLength;
};

const u32 MaxTexturesPerShader = 32;

struct Header_v2 {
    u32 computeLength;
    u32 fragmentLength;
    bool opaque;
    u8 pad[3];
    TextureEntry textureEntries[MaxTexturesPerShader];
    u32 stringPoolSize;
    char stringPool[stringPoolSize];
};

struct ParticleData {
    PreHeader preHeader;
    Header_v2 header;
    char computeShader[header.computeLength];
    char fragmentShader[header.fragmentLength];
};

ParticleData data @ 0x00;