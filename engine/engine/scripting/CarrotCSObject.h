//
// Created by jglrxavpok on 08/04/2023.
//

#pragma once

#include <memory>
#include <core/scripting/csharp/forward.h>
#include <core/scripting/csharp/CSClass.h>
#include <core/scripting/csharp/CSObject.h>

namespace Carrot::Scripting {

    class CarrotCSObjectBase {
    protected:
        CarrotCSObjectBase() = default;

    public:
        virtual MonoObject* toMono() const = 0;
        virtual bool isAlive() const = 0;
        virtual ~CarrotCSObjectBase() = default;
    };

    /**
     * C# object wrapper that references a C++ object that needs to be destroyed once C# code no longer uses it
     */
    template<typename T>
    class CarrotCSObject: public CarrotCSObjectBase {
    public:
        /**
         * Construct a CarrotCSObject: creates the C++ object and a C# object with a pointer to the C++ object
         * @tparam Args Argument types for the constructor of the C++ object
         * @param csType C# class of the new object to instantiate, expects a constructor taking a single UInt64 argument
         * @param args arguments for the C++ constructor
         */
        template<typename... Args>
        explicit CarrotCSObject(Scripting::CSClass* csType, Args&&... args) {
            cppObj = std::make_unique<T>(std::forward<Args>(args)...);

            std::uint64_t handle = reinterpret_cast<std::uint64_t>(cppObj.get());
            void* csArgs[1] = {
                    &handle,
            };
            csObj = csType->newObject(csArgs)->toMono(); // we don't want to hold a GC handle of this object
            gcHandle = mono_gchandle_new_weakref(csObj, true);
        }

        virtual bool isAlive() const override {
            return mono_gchandle_get_target(gcHandle) != nullptr;
        }

        T* get() const {
            return cppObj.get();
        }

        virtual MonoObject* toMono() const override {
            return csObj;
        }

    private:
        std::unique_ptr<T> cppObj;
        MonoObject* csObj = nullptr;
        std::uint32_t gcHandle = 0;
    };
}