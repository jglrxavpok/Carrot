//
// Created by jglrxavpok on 18/04/2021.
//

#include "Sound.h"
#include "decoders/WavDecoder.h"
#include "decoders/MP3Decoder.h"
#include "decoders/VorbisDecoder.h"
#include <memory>

namespace Carrot::Audio {
    Sound::Sound(std::unique_ptr<AudioDecoder>&& _decoder, bool streaming): decoder(std::move(_decoder)), streaming(streaming) {}

    Sound::Sound(const std::string& filename, bool streaming): streaming(streaming) {
        if(filename.ends_with(".wav")) {
            decoder = std::make_unique<WavDecoder>(filename);
        } else if(filename.ends_with(".mp3")) {
            decoder = std::make_unique<MP3Decoder>(filename);
        } else if(filename.ends_with(".ogg")) {
            decoder = std::make_unique<VorbisDecoder>(filename);
        } else {
            throw std::runtime_error("Unsupported audio file type");
        }
    }

    std::shared_ptr<AL::Buffer> Sound::getNextBuffer() {
        if(!endOfFile) {
            auto buffer = std::make_shared<AL::Buffer>();
            size_t sampleCount = streaming ? SAMPLES_AT_ONCE : decoder->getSampleCount();
            std::vector<float> samples = decoder->extractSamples(sampleCount);
            buffer->upload(decoder->getFormat(), decoder->getFrequency(), samples.data(), samples.size() * sizeof(float));
            if(samples.size() < sampleCount * decoder->getChannelCount() || !streaming) {
                endOfFile = true;
            }
            return buffer;
        } else {
            return nullptr;
        }
    }

    void Sound::rewind() {
        endOfFile = false;
        seek(0);
    }

    void Sound::seek(size_t sampleIndex) {
        decoder->seek(sampleIndex);
    }

    NoDecoder Sound::copyInMemory() {
        return NoDecoder {
            decoder->getFormat(),
            std::move(decoder->extractSamples(decoder->getSampleCount())),
            decoder->getFrequency(),
            decoder->getChannelCount(),
        };
    }
}