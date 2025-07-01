//
// Created by jglrxavpok on 13/05/2021.
//

#include "ParticleBlueprint.h"

#include "core/io/IO.h"
#include "core/io/Resource.h"

struct Header {
    char magic[16];
    std::uint32_t version;
    std::uint32_t computeLength;
    std::uint32_t fragmentLength;
    bool opaque = false;
};

Carrot::ParticleBlueprint::ParticleBlueprint(std::vector<uint32_t>&& computeCode, std::vector<uint32_t>&& fragmentCode, bool opaque): computeShaderCode(computeCode), fragmentShaderCode(fragmentCode), opaque(opaque) {
    version = 1;
}

Carrot::ParticleBlueprint::ParticleBlueprint(const IO::Resource& file) {
    load(file);
    readyForHotReload = false;
}

void Carrot::ParticleBlueprint::load(const IO::Resource& file) {
    std::vector<u8> bytes;

    bytes.resize(file.getSize());
    file.read(bytes);

    if(bytes.size() < sizeof(Header)) {
        throw std::runtime_error("File is too small!");
    }
    auto* header = (Header*)bytes.data();
    if(std::string("carrot particle") != header->magic) throw std::runtime_error("Invalid magic header.");
    if(header->version < 0 || header->version > 2) throw std::runtime_error("Unsupported version: " + std::to_string(version));

    uint32_t expectedTotalSize = sizeof(Header) + header->computeLength + header->fragmentLength;
    if(bytes.size() != expectedTotalSize) {
        std::cout << "Size of header: " << sizeof(Header) << std::endl;
        throw std::runtime_error("File is too small (" + std::to_string(bytes.size()) + "), cannot fit compute and render shaders as advertised in header (" + std::to_string(expectedTotalSize) + ")!");
    }

    if(header->computeLength % sizeof(std::uint32_t) != 0) {
        throw std::runtime_error("computeLength is not a multiple of sizeof(uint32) !");
    }
    if(header->fragmentLength % sizeof(std::uint32_t) != 0) {
        throw std::runtime_error("fragmentLength is not a multiple of sizeof(uint32) !");
    }
    version = header->version;
    opaque = header->opaque;
    computeShaderCode.resize(header->computeLength / sizeof(std::uint32_t));
    fragmentShaderCode.resize(header->fragmentLength / sizeof(std::uint32_t));
    std::memcpy(computeShaderCode.data(), bytes.data() + sizeof(Header), header->computeLength);
    std::memcpy(fragmentShaderCode.data(), bytes.data() + sizeof(Header) + header->computeLength, header->fragmentLength);
}

std::ostream& Carrot::operator<<(std::ostream& out, const Carrot::ParticleBlueprint& blueprint) {
    Header h {
        .version = blueprint.version,
        .computeLength = static_cast<std::uint32_t>(blueprint.computeShaderCode.size() * sizeof(std::uint32_t)),
        .fragmentLength = static_cast<std::uint32_t>(blueprint.fragmentShaderCode.size() * sizeof(std::uint32_t)),
        .opaque = blueprint.opaque,
    };
    Carrot::strncpy(h.magic, Carrot::ParticleBlueprint::Magic, sizeof(h.magic));
    out.write(reinterpret_cast<const char *>(&h), sizeof(h));
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
