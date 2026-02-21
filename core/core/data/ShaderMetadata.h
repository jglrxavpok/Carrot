//
// Created by jglrxavpok on 27/11/2021.
//

#pragma once

#include <filesystem>
#include <array>
#include <string>
#include <core/io/Resource.h>
#include <core/utils/JSON.h>
#include <core/utils/Lookup.hpp>
#include <vulkan/vulkan_core.h>

namespace ShaderCompiler {
    static Carrot::Lookup DescriptorTypeNames = std::array {
        Carrot::LookupEntry(VK_DESCRIPTOR_TYPE_SAMPLER, "sampler"),
        Carrot::LookupEntry(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, "combined_image_sampler"),
        Carrot::LookupEntry(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, "sampled_image"),
        Carrot::LookupEntry(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, "acceleration_structure"),
        Carrot::LookupEntry(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, "uniform_buffer"),
        Carrot::LookupEntry(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, "storage_buffer"),
        Carrot::LookupEntry(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, "storage_image"),
    };

    struct BindingSlot {
        u32 setID = 0;
        u32 bindingID = 0;
        VkDescriptorType type{};
    };

    struct Metadata {
        std::vector<std::filesystem::path> sourceFiles; // all source files used to create this shader (original .glsl + included files)
        std::array<std::string, 5> commandArguments; // command to launch to recompile this shader

        // Initializes an empty metadata struct
        explicit Metadata() = default;

        // Initializes from JSON data
        explicit Metadata(const rapidjson::Value& json);

        // Serializes this struct to JSON. Same format as taken as input by the constructor
        void writeJSON(rapidjson::Document& document) const;

    };
}
