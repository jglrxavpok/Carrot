//
// Created by jglrxavpok on 13/05/2021.
//

#include "ParticleBlueprint.h"

#include <core/containers/Vector.hpp>

#include "core/io/IO.h"
#include "core/io/Resource.h"

struct PreHeader {
    char magic[16];
    u32 version;
};

struct Header_v1 {
    std::uint32_t computeLength;
    std::uint32_t fragmentLength;
    bool opaque = false;
};

constexpr u32 MaxTexturesPerShader = 32; // if changed, need version modification
struct TextureEntry {
    u32 nameOffset = 0; // offset in stringPool
    u32 nameLength = 0; // if 0, no texture is used for this slot
};
struct Header_v2 {
    std::uint32_t computeLength = 0;
    std::uint32_t fragmentLength = 0;
    bool opaque = false;
    TextureEntry textureEntries[Carrot::ParticleBlueprint::MaxTexturesPerShader];
    u32 stringPoolSize = 0;
    char stringPool[];
};

static_assert(std::is_trivially_copyable_v<Header_v1>);
static_assert(std::is_trivially_copyable_v<Header_v2>);

Carrot::ParticleBlueprint::ParticleBlueprint(std::vector<uint32_t>&& computeCode, std::vector<uint32_t>&& fragmentCode, bool opaque, const std::unordered_map<std::string, i32>& textureIndices)
: computeShaderCode(computeCode)
, fragmentShaderCode(fragmentCode)
, opaque(opaque)
, textureIndices(textureIndices)
{
    version = 2;
}

Carrot::ParticleBlueprint::ParticleBlueprint(const IO::Resource& file) {
    load(file);
    readyForHotReload = false;
}

void Carrot::ParticleBlueprint::load(const IO::Resource& file) {
    std::vector<u8> bytes;

    bytes.resize(file.getSize());
    file.read(bytes);

    if(bytes.size() < sizeof(PreHeader)) {
        throw std::runtime_error("File is too small!");
    }

    auto* preheader = (PreHeader*)bytes.data();
    if(std::string("carrot particle") != preheader->magic) throw std::runtime_error("Invalid magic header.");
    if(preheader->version < 0 || preheader->version > 2) throw std::runtime_error("Unsupported version: " + std::to_string(version));

    u32 computeLength;
    u32 fragmentLength;
    u32 headerSize;
    if (preheader->version == 1) {
        auto* header = (Header_v1*)(bytes.data()+sizeof(PreHeader));
        computeLength = header->computeLength;
        fragmentLength = header->fragmentLength;
        opaque = header->opaque;
        headerSize = sizeof(Header_v1);
    } else if (preheader->version == 2) {
        auto* header = (Header_v2*)(bytes.data()+sizeof(PreHeader));
        computeLength = header->computeLength;
        fragmentLength = header->fragmentLength;
        opaque = header->opaque;
        headerSize = sizeof(Header_v2) + header->stringPoolSize;

        for (i32 textureEntryIndex = 0; textureEntryIndex < MaxTexturesPerShader; textureEntryIndex++) {
            const TextureEntry& entry = header->textureEntries[textureEntryIndex];
            if (entry.nameLength == 0) {
                continue;
            }

            std::string name{std::string_view{header->stringPool + entry.nameOffset, entry.nameLength}};
            textureIndices[name] = textureEntryIndex;
        }
    } else {
        throw std::runtime_error("Unsupported version");
    }

    uint32_t expectedTotalSize = sizeof(PreHeader) + headerSize + computeLength + fragmentLength;
    if(bytes.size() != expectedTotalSize) {
        std::cout << "Size of header: " << headerSize << std::endl;
        throw std::runtime_error("File is too small (" + std::to_string(bytes.size()) + "), cannot fit compute and render shaders as advertised in header (" + std::to_string(expectedTotalSize) + ")!");
    }

    if(computeLength % sizeof(std::uint32_t) != 0) {
        throw std::runtime_error("computeLength is not a multiple of sizeof(uint32) !");
    }
    if(fragmentLength % sizeof(std::uint32_t) != 0) {
        throw std::runtime_error("fragmentLength is not a multiple of sizeof(uint32) !");
    }
    version = preheader->version;
    computeShaderCode.resize(computeLength / sizeof(std::uint32_t));
    fragmentShaderCode.resize(fragmentLength / sizeof(std::uint32_t));
    std::memcpy(computeShaderCode.data(), bytes.data() + sizeof(PreHeader) + headerSize, computeLength);
    std::memcpy(fragmentShaderCode.data(), bytes.data() + sizeof(PreHeader) + headerSize + computeLength, fragmentLength);
}

std::ostream& Carrot::operator<<(std::ostream& out, const Carrot::ParticleBlueprint& blueprint) {
    Header_v2 h {
        .computeLength = static_cast<std::uint32_t>(blueprint.computeShaderCode.size() * sizeof(std::uint32_t)),
        .fragmentLength = static_cast<std::uint32_t>(blueprint.fragmentShaderCode.size() * sizeof(std::uint32_t)),
        .opaque = blueprint.opaque,
    };

    Vector<char> stringPool;
    for (const auto& [imagePath, imageIndex] : blueprint.textureIndices) {
        const u64 offset = stringPool.size();
        stringPool.resize(stringPool.size() + imagePath.size());

        memcpy(&stringPool[offset], imagePath.c_str(), imagePath.size());

        verify(imageIndex < MaxTexturesPerShader, "Too many textures in particle shader");
        h.textureEntries[imageIndex].nameOffset = offset;
        h.textureEntries[imageIndex].nameLength = imagePath.size();
    }

    h.stringPoolSize = stringPool.size();

    PreHeader preHeader {
        .version = 2,
    };
    Carrot::strncpy(preHeader.magic, Carrot::ParticleBlueprint::Magic, sizeof(preHeader.magic));
    out.write(reinterpret_cast<const char*>(&preHeader), sizeof(preHeader));
    out.write(reinterpret_cast<const char *>(&h), sizeof(h));
    out.write(reinterpret_cast<const char *>(stringPool.data()), stringPool.size());
    out.write(reinterpret_cast<const char*>(blueprint.computeShaderCode.data()), h.computeLength);
    out.write(reinterpret_cast<const char*>(blueprint.fragmentShaderCode.data()), h.fragmentLength);
    return out;
}

bool Carrot::ParticleBlueprint::hasHotReloadPending() {
    return readyForHotReload;
}

void Carrot::ParticleBlueprint::clearHotReloadFlag() {
    readyForHotReload = false;
}

const std::unordered_map<std::string, i32>& Carrot::ParticleBlueprint::getTextureIndices() const {
    return textureIndices;
}
