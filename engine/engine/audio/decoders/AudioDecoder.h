//
// Created by jglrxavpok on 18/04/2021.
//

#pragma once

#include <utility>
#include <vector>
#include <string>

namespace Carrot {
    class AudioDecoder {
    protected:
        std::string filename;

    public:
        explicit AudioDecoder(const std::string& filename): filename(filename) {};

        virtual ALenum getFormat() = 0;
        virtual std::size_t getSampleCount() = 0;
        virtual std::uint64_t getFrequency() = 0;
        virtual std::uint64_t getChannelCount() = 0;
        virtual void seek(size_t sampleIndex) = 0;

        /**
         * Reads 'sampleCount' samples, and advances cursor by that many samples.
         * Returned vector can be less than 'sampleCount' if reached the end of the file.
         * If the audio supports multiple channels, should attempt to return sampleCount*channelCount samples, interleaved
         */
        virtual std::vector<float> extractSamples(size_t sampleCount) = 0; // TODO: avoid allocations

        virtual ~AudioDecoder() = default;
    };
}
