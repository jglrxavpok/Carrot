//
// Created by jglrxavpok on 13/05/2021.
//

#include "ParticleBlueprint.h"
#include "engine/io/IO.h"
#include "engine/io/Resource.h"
#include "engine/render/ComputePipeline.h"
#include "engine/render/GBuffer.h"

struct Header {
    char magic[16];
    std::uint32_t version;
    std::uint32_t computeLength;
    std::uint32_t fragmentLength;
};

Carrot::ParticleBlueprint::ParticleBlueprint(std::vector<uint32_t>&& computeCode, std::vector<uint32_t>&& fragmentCode): computeShaderCode(computeCode), fragmentShaderCode(fragmentCode) {
    version = 1;
}

Carrot::ParticleBlueprint::ParticleBlueprint(const std::string& filename) {
    auto bytes = IO::readFile(filename);

    if(bytes.size() < sizeof(Header)) {
        throw std::runtime_error("File is too small!");
    }
    auto* header = (Header*)bytes.data();
    if(std::string("carrot particle") != header->magic) throw std::runtime_error("Invalid magic header.");
    if(header->version != 1) throw std::runtime_error("Unsupported version: " + std::to_string(version));

    if(bytes.size() != sizeof(Header) + header->computeLength + header->fragmentLength) {
        uint32_t expectedTotalSize = sizeof(Header) + header->computeLength + header->fragmentLength;
        throw std::runtime_error("File is too small (" + std::to_string(bytes.size()) + "), cannot fit compute and render shaders as advertised in header (" + std::to_string(expectedTotalSize) + ")!");
    }

    version = header->version;
    if(header->computeLength % sizeof(std::uint32_t) != 0) {
        throw std::runtime_error("computeLength is not a multiple of sizeof(uint32) !");
    }
    if(header->fragmentLength % sizeof(std::uint32_t) != 0) {
        throw std::runtime_error("fragmentLength is not a multiple of sizeof(uint32) !");
    }
    computeShaderCode.resize(header->computeLength / sizeof(std::uint32_t));
    fragmentShaderCode.resize(header->fragmentLength / sizeof(std::uint32_t));
    std::memcpy(computeShaderCode.data(), bytes.data() + sizeof(Header), header->computeLength);
    std::memcpy(fragmentShaderCode.data(), bytes.data() + sizeof(Header) + header->computeLength, header->fragmentLength);
}

std::ostream& Carrot::operator<<(std::ostream& out, const Carrot::ParticleBlueprint& blueprint) {
    out << Carrot::ParticleBlueprint::Magic << '\0';
    out.write(reinterpret_cast<const char *>(&blueprint.version), sizeof(blueprint.version));
    uint32_t computeSize = blueprint.computeShaderCode.size() * sizeof(uint32_t);
    uint32_t fragmentSize = blueprint.fragmentShaderCode.size() * sizeof(uint32_t);
    out.write(reinterpret_cast<const char *>(&computeSize), sizeof(uint32_t));
    out.write(reinterpret_cast<const char *>(&fragmentSize), sizeof(uint32_t));
    out.write(reinterpret_cast<const char *>(blueprint.computeShaderCode.data()), blueprint.computeShaderCode.size() * sizeof(uint32_t));
    out.write(reinterpret_cast<const char *>(blueprint.fragmentShaderCode.data()), blueprint.fragmentShaderCode.size() * sizeof(uint32_t));
    return out;
}

std::unique_ptr<Carrot::ComputePipeline> Carrot::ParticleBlueprint::buildComputePipeline(Carrot::Engine& engine, const vk::DescriptorBufferInfo particleBuffer, const vk::DescriptorBufferInfo statisticsBuffer) const {
    return ComputePipelineBuilder(engine)
            .shader(Carrot::IO::Resource({(std::uint8_t*)computeShaderCode.data(), computeShaderCode.size() * sizeof(std::uint32_t)}))
            .bufferBinding(vk::DescriptorType::eStorageBuffer, 0, 0, statisticsBuffer)
            .bufferBinding(vk::DescriptorType::eStorageBuffer, 0, 1, particleBuffer)
            .build();
}

std::unique_ptr<Carrot::Pipeline> Carrot::ParticleBlueprint::buildRenderingPipeline(Carrot::Engine& engine) const {
    Carrot::PipelineDescription desc;

    desc.subpassIndex = 0;
    desc.vertexFormat = VertexFormat::Particle;
    desc.materialStorageBufferBindingIndex = -1;
    desc.texturesBindingIndex = -1;
    desc.vertexShader = "resources/shaders/particles.vertex.glsl.spv";
    desc.fragmentShader = Carrot::IO::Resource({(std::uint8_t*)(fragmentShaderCode.data()), fragmentShaderCode.size() * sizeof(std::uint32_t)});

    return std::make_unique<Carrot::Pipeline>(engine.getVulkanDriver(), desc);
}