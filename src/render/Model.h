//
// Created by jglrxavpok on 30/11/2020.
//

#pragma once
#include <string>
#include <vector>
#include <memory>
#include <map>
#include "vulkan/includes.h"

using namespace std;

namespace Carrot {
    class Mesh;
    class Engine;
    class Material;

    class Model {
    private:
        Carrot::Engine& engine;
        vector<shared_ptr<Material>> materials{};
        // TODO: textures
        map<Material*, vector<shared_ptr<Mesh>>> meshes{};

    public:
        explicit Model(Carrot::Engine& engine, const string& filename);

        void draw(const uint32_t imageIndex, vk::CommandBuffer& commands);
    };
}
