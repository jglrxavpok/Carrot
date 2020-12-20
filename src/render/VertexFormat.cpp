//
// Created by jglrxavpok on 16/12/2020.
//

#include "VertexFormat.h"
#include "Vertex.h"

Carrot::VertexFormat Carrot::getVertexFormat(const std::string& name) {
    if(name == "Vertex") {
        return VertexFormat::Vertex;
    } else if(name == "SkinnedVertex") {
        return VertexFormat::SkinnedVertex;
    }
    return Carrot::VertexFormat::Invalid;
}

std::vector<vk::VertexInputBindingDescription> Carrot::getBindingDescriptions(Carrot::VertexFormat vertexFormat) {
    switch(vertexFormat) {
        case VertexFormat::Vertex:
            return Carrot::Vertex::getBindingDescription();
        case VertexFormat::SkinnedVertex:
            return Carrot::SkinnedVertex::getBindingDescription();
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
        default:
            throw std::runtime_error("Invalid vertex format!");
    }
}
