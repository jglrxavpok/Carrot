//
// Created by jglrxavpok on 01/10/2023.
//

#include "AnimatedModel.h"

namespace Carrot::Render {
    AnimatedModel::Handle::Handle(const std::shared_ptr<AnimatedModel>& parent): parent(parent) {

    }

    AnimatedModel::Handle::~Handle() {

    }

    AnimatedModel& AnimatedModel::Handle::getParent() {
        return *parent;
    }

    const AnimatedModel& AnimatedModel::Handle::getParent() const {
        return *parent;
    }

    AnimatedInstanceData& AnimatedModel::Handle::getData() {
        return data;
    }

    AnimatedModel::AnimatedModel(const std::shared_ptr<Model>& model): model(model), animatedInstances(GetEngine(), model, MaxInstances) {}

    Carrot::Model& AnimatedModel::getModel() {
        return *model;
    }

    const Carrot::Model& AnimatedModel::getModel() const {
        return *model;
    }

    std::shared_ptr<AnimatedModel::Handle> AnimatedModel::requestHandle() {
        handles.push_back(std::make_shared<AnimatedModel::Handle>(this->shared_from_this()));
        return handles.back();
    }

    void AnimatedModel::onFrame(const Context& renderContext) {
        // remove stale references, and reindex (index of handle inside 'handles' array is its index inside 'animatedInstances')
        std::erase_if(handles, [](const std::shared_ptr<Handle>& handle) {
            return handle.use_count() <= 1;
        });

        // copy to GPU
        for (int i = 0; i < handles.size(); ++i) {
            animatedInstances.getInstance(i) = handles[i]->getData();
        }

        // TODO: allow for custom pass
        animatedInstances.render(renderContext, Carrot::Render::PassEnum::OpaqueGBuffer, handles.size());
    }
} // Carrot::Render