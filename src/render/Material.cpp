//
// Created by jglrxavpok on 30/11/2020.
//

#include "Material.h"
#include "Pipeline.h"
#include <iostream>

Carrot::Material::Material(Carrot::Engine& engine): engine(engine) {

}

void Carrot::Material::bind(const string& shaderName, const string& textureName, uint32_t textureIndex, uint32_t bindingIndex) {
    if(!pipeline) {
        pipeline = engine.getOrCreatePipeline(shaderName);
        descriptorSets = pipeline->getDescriptorSets();
    }

    auto& textureView = engine.getOrCreateTextureView(textureName);

    vector<vk::WriteDescriptorSet> writes{engine.getSwapchainImageCount()};
    vk::DescriptorImageInfo imageInfo{
            .imageView = *textureView,
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
    };
    for(uint32_t i = 0; i < engine.getSwapchainImageCount(); i++) {
        auto& write = writes[i];
        write.dstSet = descriptorSets[i];
        write.dstBinding = bindingIndex;
        write.dstArrayElement = textureIndex;
        write.descriptorCount = 1;
        write.descriptorType = vk::DescriptorType::eSampledImage;
        write.pImageInfo = &imageInfo;
    }
    engine.getLogicalDevice().updateDescriptorSets(writes, {});
}

void Carrot::Material::bindForRender(const uint32_t imageIndex, vk::CommandBuffer& commands) const {
    pipeline->bind(commands);
    pipeline->bindDescriptorSets(commands, {descriptorSets[imageIndex]}, {0});
}

void Carrot::Material::bindDefaultValues() {
    bind("default", "default.png", 0, 1);
    bind("default", "default.png", 1, 1);
}
