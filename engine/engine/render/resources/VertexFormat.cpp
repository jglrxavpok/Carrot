//
// Created by jglrxavpok on 16/12/2020.
//

#include "VertexFormat.h"
#include "Vertex.h"
#include "engine/render/particles/Particles.h"

Carrot::VertexFormat Carrot::getVertexFormat(const std::string& name) {
    if(name == "Vertex") {
        return VertexFormat::Vertex;
    } else if(name == "SkinnedVertex") {
        return VertexFormat::SkinnedVertex;
    } else if(name == "ComputeSkinnedVertex") {
        return VertexFormat::ComputeSkinnedVertex;
    } else if(name == "ScreenSpace") {
        return VertexFormat::ScreenSpace;
    } else if(name == "SimpleVertex") {
        return VertexFormat::SimpleVertex;
    } else if(name == "SimpleVertexWithInstanceData") {
        return VertexFormat::SimpleVertexWithInstanceData;
    } else if(name == "Particle") {
        return VertexFormat::Particle;
    }
    return Carrot::VertexFormat::Invalid;
}

std::vector<vk::VertexInputBindingDescription> Carrot::getBindingDescriptions(Carrot::VertexFormat vertexFormat) {
    switch(vertexFormat) {
        case VertexFormat::Vertex:
            return Carrot::getVertexBindingDescription();
        case VertexFormat::SkinnedVertex:
            return Carrot::getSkinnedVertexBindingDescription();

        case VertexFormat::ComputeSkinnedVertex:
            return Carrot::getComputeSkinnedVertexBindingDescription();

        case VertexFormat::ScreenSpace:
            return Carrot::getScreenSpaceVertexBindingDescription();

        case VertexFormat::SimpleVertex:
            return Carrot::getSimpleVertexBindingDescription();

        case VertexFormat::SimpleVertexWithInstanceData:
            return Carrot::getSimpleVertexWithInstanceDataBindingDescription();

        case VertexFormat::Particle:
            return Carrot::getParticleBindingDescription();

        default:
            throw std::runtime_error("Invalid vertex format!");
    }
}

std::vector<vk::VertexInputAttributeDescription> Carrot::getAttributeDescriptions(VertexFormat vertexFormat) {
    switch(vertexFormat) {
        case VertexFormat::Vertex:
            return Carrot::getVertexAttributeDescriptions();

        case VertexFormat::SkinnedVertex:
            return Carrot::getSkinnedVertexAttributeDescriptions();

        case VertexFormat::ComputeSkinnedVertex:
            return Carrot::getComputeSkinnedVertexAttributeDescriptions();

        case VertexFormat::ScreenSpace:
            return Carrot::getScreenSpaceVertexAttributeDescriptions();

        case VertexFormat::SimpleVertex:
            return Carrot::getSimpleVertexAttributeDescriptions();

        case VertexFormat::SimpleVertexWithInstanceData:
            return Carrot::getSimpleVertexWithInstanceDataAttributeDescriptions();

        case VertexFormat::Particle:
            return Carrot::getParticleAttributeDescriptions();

        default:
            throw std::runtime_error("Invalid vertex format!");
    }
}

