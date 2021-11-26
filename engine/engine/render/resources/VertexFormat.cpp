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
            return Carrot::Vertex::getBindingDescription();
        case VertexFormat::SkinnedVertex:
            return Carrot::SkinnedVertex::getBindingDescription();

        case VertexFormat::ComputeSkinnedVertex:
            return Carrot::ComputeSkinnedVertex::getBindingDescription();

        case VertexFormat::ScreenSpace:
            return Carrot::ScreenSpaceVertex::getBindingDescription();

        case VertexFormat::SimpleVertex:
            return Carrot::SimpleVertex::getBindingDescription();

        case VertexFormat::SimpleVertexWithInstanceData:
            return Carrot::SimpleVertexWithInstanceData::getBindingDescription();

        case VertexFormat::Particle:
            return Carrot::Particle::getBindingDescription();

        default:
            throw std::runtime_error("Invalid vertex format!");
    }
}

std::vector<vk::VertexInputAttributeDescription> Carrot::getAttributeDescriptions(VertexFormat vertexFormat) {
    switch(vertexFormat) {
        case VertexFormat::Vertex:
            return Carrot::Vertex::getAttributeDescriptions();

        case VertexFormat::SkinnedVertex:
            return Carrot::SkinnedVertex::getAttributeDescriptions();

        case VertexFormat::ComputeSkinnedVertex:
            return Carrot::ComputeSkinnedVertex::getAttributeDescriptions();

        case VertexFormat::ScreenSpace:
            return Carrot::ScreenSpaceVertex::getAttributeDescriptions();

        case VertexFormat::SimpleVertex:
            return Carrot::SimpleVertex::getAttributeDescriptions();

        case VertexFormat::SimpleVertexWithInstanceData:
            return Carrot::SimpleVertexWithInstanceData::getAttributeDescriptions();

        case VertexFormat::Particle:
            return Carrot::Particle::getAttributeDescriptions();

        default:
            throw std::runtime_error("Invalid vertex format!");
    }
}

