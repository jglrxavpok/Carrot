//
// Created by jglrxavpok on 19/04/2021.
//

#pragma once

#include <engine/audio/OpenAL.hpp>
#include "AudioDecoder.h"
#include "dr_wav.h"

namespace Carrot {
    class WavDecoder: public AudioDecoder {
    private:
        drwav wav;

    public:
        explicit WavDecoder(const std::string& filename);

        size_t getSampleCount() override;

        uint64_t getFrequency() override;

        std::vector<float> extractSamples(std::size_t sampleCount) override;

        ALenum getFormat() override;

        void seek(std::size_t sampleIndex) override;

        uint64_t getChannelCount() override;

        ~WavDecoder() override;
    };
}
