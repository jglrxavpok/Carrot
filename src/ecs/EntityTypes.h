//
// Created by jglrxavpok on 27/02/2021.
//

#pragma once
#include <memory>
#include <cstddef>

typedef std::uint32_t EntityID;
typedef EntityID Entity;
typedef std::shared_ptr<uint32_t> Entity_Ptr;
typedef std::weak_ptr<Entity> Entity_WeakPtr;