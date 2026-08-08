//
// Created by jglrxavpok on 26/01/2022.
//

#pragma once

#include <memory>
#include <thread>
#include <engine/utils/Macros.h>
#include <engine/task/TaskScheduler.h>
#include <engine/render/Model.h>

namespace Carrot {
    class RenderableParticleBlueprint;
}

namespace Carrot {
    template<typename TElement>
    struct AsyncResourceTraits;

    template<typename TElement, typename ValueContainer = std::shared_ptr<TElement>>
    using AsyncTaskType = std::function<ValueContainer(TaskHandle& taskHandle)>;

    template<typename TElement, typename ValueContainer = std::shared_ptr<TElement>>
    concept IsResourceSerialisable = requires(TElement resource, AsyncResourceTraits<TElement> traits, const Carrot::IO::VFS::Path& resourcePath) {
        { resource.getFilePath() } -> std::convertible_to<Carrot::IO::VFS::Path>;
        { traits.makeLoadingTask(resourcePath) } -> std::convertible_to<AsyncTaskType<TElement, ValueContainer>>;
    };

    template<typename T, bool WaitOnAccess, typename ValueContainer = std::shared_ptr<T>>
    class AsyncResource {
    public:

    public:
        AsyncResource() {
            storage = std::make_shared<Storage>();
        };

        AsyncResource(const AsyncResource& toCopy) {
            *this = toCopy;
        }

        AsyncResource(AsyncResource&& toMove) = default;

        //! Creates an already-loaded async resource
        AsyncResource(ValueContainer alreadyLoaded) {
            storage = std::make_shared<Storage>();
            storage->value = alreadyLoaded;
            storage->initialized = true;
            storage->running = false;
        }

        explicit AsyncResource(AsyncTaskType<T, ValueContainer>&& loadingTask) {
            storage = std::make_shared<Storage>();
            storage->initializer = std::move(loadingTask);
            TaskDescription desc {
                .name = "Loading AsyncResource",
                .task = asTask(storage),
                // TODO: joiner counter?
            };
            GetTaskScheduler().schedule(std::move(desc), TaskScheduler::AssetLoading);
        }

        bool isEmpty() const {
            return !storage->initializer && !storage->initialized;
        }

        // Not empty and finished loading
        bool isReady() const {
            return storage->initialized && !!storage->value;
        }

        void forceWait() const {
            verify(storage->initializer || storage->running || storage->initialized, "No initializer task!");
            while(!isReady()) {
                std::this_thread::yield();
            }
        }

    public: // access
        const T* operator->() const {
            throwOrWait();
            return storage->value.get();
        }

        T* operator->() {
            throwOrWait();
            return storage->value.get();
        }

        const T& operator*() const {
            throwOrWait();
            return *storage->value;
        }

        T& operator*() {
            throwOrWait();
            return *storage->value;
        }

        ValueContainer get() {
            throwOrWait();
            return storage->value;
        }

        ValueContainer get() const {
            throwOrWait();
            return storage->value;
        }

    public:
        AsyncResource& operator=(const AsyncResource& toCopy) {
            storage = toCopy.storage;
            return *this;
        }

        AsyncResource& operator=(AsyncResource&& toMove) {
            storage = toMove.storage;

            toMove.storage = nullptr;
            return *this;
        }

        void startLoadFromPath(const Carrot::IO::VFS::Path& path) requires IsResourceSerialisable<T, ValueContainer> {
            *this = AsyncResource(AsyncResourceTraits<T>::makeLoadingTask(path));
        }

    public: // interface with Carrot::DocumentElement

        // Returns a DocumentElement representing this resource.
        // This will block if resource is still loading
        Carrot::DocumentElement serialise() const requires IsResourceSerialisable<T, ValueContainer> {
            Carrot::DocumentElement result;
            result = "";
            if (isEmpty()) {
                return result;
            }

            forceWait();
            result = storage->value->getFilePath().toString();
            return result;
        }

        // Starts loading the resource from the given DocumentElement (as generated from serialise)
        void startLoad(const Carrot::DocumentElement& resourceDocument) requires IsResourceSerialisable<T, ValueContainer> {
            const Carrot::IO::VFS::Path path{ resourceDocument.getAsString() };
            startLoadFromPath(path);
        }

        // Checks if parent has a member named 'name', and if so, calls startLoad on this member
        bool optionalStartLoad(const Carrot::DocumentElement& parent, const std::string& name) requires IsResourceSerialisable<T, ValueContainer> {
            auto view = parent.getAsObject();
            if (auto iter = view.find(name); iter.isValid()) {
                if (iter->second.getAsString().empty()) {
                    return false;
                }
                startLoad(iter->second);
                return true;
            }
            return false;
        }

    private:
        struct Storage {
            ValueContainer value;
            std::atomic<bool> initialized = false;
            std::atomic<bool> running = false;
            AsyncTaskType<T, ValueContainer> initializer;
        };

        void throwOrWait() const {
            if constexpr(WaitOnAccess) {
                while(!isReady()) {
                    std::this_thread::yield();
                }
            } else {
                verify(isReady(), "Not ready");
            }
        }

        /// Creates a coroutine compatible with the TaskScheduler
        TaskProc asTask(std::shared_ptr<AsyncResource::Storage> targetStorage) {
            verify(targetStorage->initializer, "Needs an initializer");
            verify(!targetStorage->initialized, "Resource must not already be initialized");
            verify(!targetStorage->value, "Resource must not already be initialized");
            return [targetStorage](TaskHandle& taskHandle) {
                targetStorage->running = true;
                targetStorage->value = targetStorage->initializer(taskHandle);
                targetStorage->initializer = {};
                targetStorage->running = false;
                targetStorage->initialized = true;
            };
        }

    private:
        std::shared_ptr<Storage> storage;
    };

    using AsyncModelResource = AsyncResource<Carrot::Model, false>;
    using AsyncTextureResource = AsyncResource<Carrot::Render::Texture, false>;
    using AsyncParticleBlueprint = AsyncResource<Carrot::RenderableParticleBlueprint, false>;

    template<>
    struct AsyncResourceTraits<Carrot::Render::Texture> {
        static AsyncTaskType<Carrot::Render::Texture> makeLoadingTask(const Carrot::IO::VFS::Path& path);
    };
}
