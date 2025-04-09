//
// Created by jglrxavpok on 09/09/2023.
//

#pragma once

#include <core/io/Resource.h>
#include <engine/audio/Sound.h>
#include <engine/audio/decoders/NoDecoder.h>

#include "SoundSource.h"

namespace Carrot::Audio {

    /**
     * A Music is a long audio clip that will be streamed for storage.
     * It is also its own SoundSource, so it can be played directly
     */
    class Music {
    public:
        explicit Music(const Carrot::IO::Resource& audioFile);

        void play();
        void stop();
        void setLooping(bool b);

    private:
        std::unique_ptr<Sound> createInstance();

        std::string filepath; // to instantiate Sound
        std::shared_ptr<SoundSource> pSourceSource;
        Carrot::IO::Resource originatingResource;
    };

} // Carrot::Audio
