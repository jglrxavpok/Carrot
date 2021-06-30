//
// Created by jglrxavpok on 30/11/2020.
//

#include "Material.h"
#include "engine/render/resources/Pipeline.h"
#include "IDTypes.h"
#include "DrawData.h"
#include <iostream>
#include <rapidjson/document.h>
#include <engine/io/IO.h>
#include <engine/render/GBuffer.h>

Carrot::Material::Material(Carrot::Engine& engine, const string& materialName): engine(engine) {
    rapidjson::Document description;
    description.Parse(Carrot::IO::readFileAsText("resources/materials/"+materialName+".json").c_str());

    string textureName = description["texture"].GetString();
    string pipelineName = description["pipeline"].GetString();

    // TODO: different render passes
    auto& renderPass = engine.getRenderer().getGRenderPass();
    pipeline = engine.getRenderer().getOrCreatePipeline(renderPass, pipelineName);

    if(description.HasMember("renderingPipeline")) {
        renderingPipeline = engine.getRenderer().getOrCreatePipeline(renderPass, description["renderingPipeline"].GetString());
    } else {
        renderingPipeline = pipeline;
    }

    if(description.HasMember("texture")) {
        string textureName = description["texture"].GetString();
        textureID = renderingPipeline->reserveTextureSlot(engine.getRenderer().getOrCreateTexture(textureName)->getView());
    }
    if(description.HasMember("ignoreInstanceColor")) {
        ignoreInstanceColor = description["ignoreInstanceColor"].GetBool();
    }
    id = renderingPipeline->reserveMaterialSlot(*this);
}

void Carrot::Material::bindForRender(const uint32_t imageIndex, vk::CommandBuffer& commands) const {
    renderingPipeline->bind(imageIndex, commands);
    vector<DrawData> data{1};
    data[0].materialIndex = id;
    commands.pushConstants<DrawData>(renderingPipeline->getPipelineLayout(), vk::ShaderStageFlagBits::eFragment, static_cast<uint32_t>(0), data);
}

Carrot::MaterialID Carrot::Material::getMaterialID() const {
    return id;
}

void Carrot::Material::setTextureID(Carrot::TextureID texID) {
    textureID = texID;
}

Carrot::TextureID Carrot::Material::getTextureID() const {
    return textureID;
}

bool Carrot::Material::ignoresInstanceColor() const {
    return ignoreInstanceColor;
}

const Carrot::Pipeline& Carrot::Material::getPipeline() const {
    return *pipeline;
}

const Carrot::Pipeline& Carrot::Material::getRenderingPipeline() const {
    return *renderingPipeline;
}