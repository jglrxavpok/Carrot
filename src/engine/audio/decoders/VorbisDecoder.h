//
// Created by jglrxavpok on 22/04/2021.
//

#pragma once

#include <engine/audio/OpenAL.hpp>
#include "AudioDecoder.h"
#include "stb_vorbis.h"

namespace Carrot {
    class VorbisDecoder: public AudioDecoder {
    private:
        stb_vorbis* vorbis = nullptr;
        stb_vorbis_info info{};

    public:
        explicit VorbisDecoder(const std::string& filename);

        std::size_t getSampleCount() override;

        std::uint64_t getFrequency() override;

        std::vector<float> extractSamples(size_t sampleCount) override;

        ALenum getFormat() override;

        void seek(std::size_t sampleIndex) override;

        std::uint64_t getChannelCount() override;

        ~VorbisDecoder() override;
    };
}
