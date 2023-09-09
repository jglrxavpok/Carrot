//
// Created by jglrxavpok on 09/09/2023.
//

#pragma once

#include <core/async/ParallelMap.hpp>
#include <engine/audio/AudioThread.h>
#include <core/io/Resource.h>

namespace Carrot::Audio {
    class SFX;

    class AudioManager {
    public:
        /**
         * Inits the manager, inits the sound engine (OpenAL at the time of writing)
         */
        AudioManager();

        /**
         * Loads the wanted SFX. Meant to be kept in memory and played multiple times at once
         * See SFX doc
         */
        std::shared_ptr<SFX> loadSFX(const Carrot::IO::Resource& soundFile);

        /**
         * Loads the wanted music. Meant to be streamed from the given resource, and have a single instance playing.
         */
        std::unique_ptr<Sound> loadMusic(const Carrot::IO::Resource& musicFile);

        /**
         * Ticks this manager, mostly for book-keeping reasons
         */
        void tick(double deltaTime);

    private:
        void registerSoundSource(std::shared_ptr<SoundSource> source);

    private:
        AL::Device alDevice;
        AL::Context alContext;

        AudioThread thread;
        Async::ParallelMap<std::string, std::weak_ptr<SFX>> loadedSFX;

        friend class SoundSource;
    };

} // Carrot::Audio
