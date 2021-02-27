//
// Created by jglrxavpok on 27/02/2021.
//

#include "Signature.hpp"

std::unordered_map<Carrot::ComponentID, std::size_t> Carrot::Signature::hash2index{};
std::mutex Carrot::Signature::mappingAccess{};