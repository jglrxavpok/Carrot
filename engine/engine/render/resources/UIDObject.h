//
// Created by jglrxavpok on 23/05/2026.
//

#pragma once

namespace Carrot::Render {
    /// RADV (Vulkan driver for AMD) (and probably other drivers) stores the device addresses inside descriptors, so checking equality via the
    ///  Vulkan objects before re-binding this object could tell you that no rebind is necessary, but the underlying memory can be different.
    ///  Without this, it is a hard-to-reproduce, hard-to-debug crash waiting to happen.
    /// Therefore, Carrot objects that interact with Vulkan objects to check for equality should use the UID of the object instead
    class UIDObject {
    public:
        UIDObject();

        virtual ~UIDObject() = default;
        u64 getUID() const;

    private:
        u64 uid = 0;
    };
}
