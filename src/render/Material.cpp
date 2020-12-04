//
// Created by jglrxavpok on 30/11/2020.
//

#include "Material.h"
#include "Pipeline.h"
#include "IDTypes.h"
#include "DrawData.h"
#include <iostream>
#include <rapidjson/document.h>
#include <io/IO.h>

Carrot::Material::Material(Carrot::Engine& engine, const string& materialName): engine(engine) {
    rapidjson::Document description;
    description.Parse(IO::readFileAsText("resources/materials/"+materialName+".json").c_str());

    textureID = description["textureIndex"].GetUint64();
    string pipelineName = description["pipeline"].GetString();
    pipeline = engine.getOrCreatePipeline(pipelineName);

    id = pipeline->reserveMaterialSlot(*this);
}

void Carrot::Material::bindForRender(const uint32_t imageIndex, vk::CommandBuffer& commands) const {
    pipeline->bind(imageIndex, commands);
    vector<DrawData> data{1};
    data[0].materialIndex = id;
    commands.pushConstants<DrawData>(pipeline->getPipelineLayout(), vk::ShaderStageFlagBits::eFragment, static_cast<uint32_t>(0), data);
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
