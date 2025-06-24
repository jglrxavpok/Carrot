//
// Created by jglrxavpok on 13/05/2021.
//

#include "ParticleBlueprint.h"

#include <engine/Engine.h>

#include "core/io/IO.h"
#include "core/io/Resource.h"
#include "engine/render/ComputePipeline.h"
#include "engine/render/GBuffer.h"

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

Carrot::ParticleBlueprint::ParticleBlueprint(const std::string& filename) {
    auto load = [this, filename]() {
        auto bytes = IO::readFile(filename);

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
    };

    load();

    pFileWatcher = Carrot::makeUnique<IO::FileWatcher>(Carrot::Allocator::getDefault(), [&, load](std::filesystem::path path) {
        readyForHotReload = true;
    }, std::vector{std::filesystem::path{filename}});
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

std::unique_ptr<Carrot::ComputePipeline> Carrot::ParticleBlueprint::buildComputePipeline(Carrot::Engine& engine, const vk::DescriptorBufferInfo particleBuffer, const vk::DescriptorBufferInfo statisticsBuffer) const {
    // TODO: replace with Carrot::Pipeline
    return ComputePipelineBuilder(engine)
            .shader(Carrot::IO::Resource({(std::uint8_t*)computeShaderCode.data(), computeShaderCode.size() * sizeof(std::uint32_t)}))
            .bufferBinding(vk::DescriptorType::eStorageBuffer, 0, 0, statisticsBuffer) // TODO: don't add
            .bufferBinding(vk::DescriptorType::eStorageBuffer, 1, 0, particleBuffer)
            .bufferBinding(vk::DescriptorType::eStorageBuffer, 1, 1, statisticsBuffer)
            .build();
}

std::unique_ptr<Carrot::Pipeline> Carrot::ParticleBlueprint::buildRenderingPipeline(Carrot::Engine& engine) const {
    Carrot::PipelineDescription desc{ Carrot::IO::Resource("resources/pipelines/particles.pipeline") };

    desc.type = PipelineType::Particles;
    desc.fragmentShader = Carrot::IO::Resource({(std::uint8_t*)(fragmentShaderCode.data()), fragmentShaderCode.size() * sizeof(std::uint32_t)});

    return std::make_unique<Carrot::Pipeline>(engine.getVulkanDriver(), desc);
}

bool Carrot::ParticleBlueprint::hasHotReloadPending() {
    return readyForHotReload;
}

void Carrot::ParticleBlueprint::clearHotReloadFlag() {
    readyForHotReload = false;
}
