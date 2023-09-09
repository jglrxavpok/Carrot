//
// Created by jglrxavpok on 09/09/2023.
//

#pragma once

#include "AL/al.h"
#include <engine/audio/decoders/AudioDecoder.h>

namespace Carrot::Audio {

    /**
     * The NoDecoder stores the entire sample list in memory and performs no actual decoding
     */
    class NoDecoder: public AudioDecoder {
    public:
        NoDecoder(): AudioDecoder("<no file>") {};
        explicit NoDecoder(ALenum format, std::vector<float> samples, std::uint64_t frequency, std::uint64_t channelCount);

        NoDecoder(const NoDecoder& toCopy) = default;
        NoDecoder(NoDecoder&& toMove) = default;
        NoDecoder& operator=(const NoDecoder& toCopy) = default;
        NoDecoder& operator=(NoDecoder&& toMove) = default;

        ALenum getFormat() override;

        size_t getSampleCount() override;

        uint64_t getFrequency() override;

        uint64_t getChannelCount() override;

        void seek(size_t sampleIndex) override;

        std::vector<float> extractSamples(size_t sampleCount) override;

    private:
        ALenum format = 0;
        std::uint64_t frequency = 1;
        std::uint64_t channelCount = 1;
        std::uint64_t sampleCount = 0; // not the same as samples.size(), 'samples' contains interleaved channel samples

        std::shared_ptr<std::vector<float>> pSamples;
        std::size_t cursor = 0;
    };

} // Carrot::Audio
