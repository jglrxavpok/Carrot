//
// Created by jglrxavpok on 04/05/2021.
//

#pragma once

#include <type_traits>
#include <list>

namespace Carrot {

    template<typename T>
    struct DefaultInit {
        T operator()() {
            return T{};
        }
    };

    template<typename A, typename B, typename AInitialiser = DefaultInit<A>, typename BInitialiser = DefaultInit<B>> requires (!std::is_same_v<A, B>)
    class BidirectionalMap {
    private:
        using element_type = std::shared_ptr<std::pair<A, B>>;
        using list_type = std::list<element_type>;
        list_type elements{};

    public:
        explicit BidirectionalMap() {};

    public:
        typename list_type::iterator find(A&& key) {
            return std::find_if(elements.begin(), elements.end(), [&](auto& element) { return element->first == key; });
        };

        typename list_type::iterator find(B&& key) {
            return std::find_if(elements.begin(), elements.end(), [&](auto& element) { return element->second == key; });
        };

        typename list_type::iterator begin() {
            return elements.begin();
        }

        typename list_type::iterator end() {
            return elements.end();
        }

    public:
        typename list_type::iterator insert(A&& key, B&& value) {
            auto existing = find(key);
            if(existing != end()) {
                (*existing)->second = value;
            }
            elements.push_back(std::make_shared(key, value));
            return elements.end() - 1;
        }

        typename list_type::iterator insert(B&& key, A&& value) {
            auto existing = find(key);
            if(existing != end()) {
                (*existing)->first = value;
            }
            elements.push_back(std::make_shared(value, key));
            return elements.end() - 1;
        }

    public:
        A& operator[](B&& key) {
            auto it = find(key);
            if(it == end()) {
                it = insert(key, AInitialiser());
            }
            return (*it)->first;
        };

        B& operator[](A&& key) {
            auto it = find(key);
            if(it == end()) {
                it = insert(key, BInitialiser());
            }
            return (*it)->second;
        };
    };
}