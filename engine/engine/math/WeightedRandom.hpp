//
// Created by jglrxavpok on 12/08/2021.
//

#pragma once

#include <engine/utils/Macros.h>
#include <engine/utils/RNG.h>

namespace Carrot::RNG {

    template<typename Item>
    class WeightedRandom {
    public:
        WeightedRandom(const WeightedRandom& toCopy) = default;
        WeightedRandom(WeightedRandom& toMove) = default;

        explicit WeightedRandom(std::vector<std::pair<Item, float>> items): items(std::move(items)) {
            assert(this->items.size() != 0);
            totalWeight = 0.0f;
            for(const auto& p : this->items) {
                totalWeight += p.second;
            }
        }

        WeightedRandom(std::initializer_list<std::pair<Item, float>> items): items(items) {
            assert(this->items.size() != 0);
            totalWeight = 0.0f;
            for(const auto& p : this->items) {
                totalWeight += p.second;
            }
        };

        Item next() {
            float w = 0.0f;
            float r = Carrot::RNG::randomFloat(0.0f, totalWeight);
            for (const auto& [item, weight] : items) {
                w += weight;
                if(w > r) {
                    return item;
                }
            }
            return items.back().first;
        }

    private:
        float totalWeight = 0.0f;
        std::vector<std::pair<Item, float>> items;
    };
}