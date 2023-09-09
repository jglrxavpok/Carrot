//
// Created by jglrxavpok on 09/09/2023.
//

#pragma once

#include <core/io/Resource.h>
#include <engine/audio/Sound.h>
#include <engine/audio/decoders/NoDecoder.h>

namespace Carrot::Audio {

    /**
     * A Sound Effect is a short audio clip that will be played entirely once 'play' is called.
     * Sound effects are decoded and their samples are stored completely in memory for the lifetime of the SFX instance
     */
    class SFX {
    public:
        explicit SFX(const Carrot::IO::Resource& audioFile);

        /**
         * Creates a new instance of this sound.
         * Creating a new instance is cheap, no decoding nor file read is required
         */
        std::unique_ptr<Sound> createInstance();

    private:
        Carrot::IO::Resource originatingResource;
        NoDecoder decoderTemplate;
    };

} // Carrot::Audio
