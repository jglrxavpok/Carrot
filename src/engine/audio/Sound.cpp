//
// Created by jglrxavpok on 18/04/2021.
//

#include "Sound.h"
#include "decoders/WavDecoder.h"
#include "decoders/MP3Decoder.h"
#include "decoders/VorbisDecoder.h"
#include <memory>

Carrot::Sound::Sound(const std::string& filename, bool streaming): streaming(streaming) {
    if(filename.ends_with(".wav")) {
        decoder = make_unique<WavDecoder>(filename);
    } else if(filename.ends_with(".mp3")) {
        decoder = make_unique<MP3Decoder>(filename);
    } else if(filename.ends_with(".ogg")) {
        decoder = make_unique<VorbisDecoder>(filename);
    } else {
        throw runtime_error("Unsupported audio file type");
    }
}

std::unique_ptr<Carrot::Sound> Carrot::Sound::loadMusic(const std::string& filename) {
    return std::make_unique<Carrot::Sound>(filename, true);
}

std::unique_ptr<Carrot::Sound> Carrot::Sound::loadSoundEffect(const std::string& filename) {
    return std::make_unique<Carrot::Sound>(filename, false);
}

std::shared_ptr<AL::Buffer> Carrot::Sound::getNextBuffer() {
    if(!endOfFile) {
        auto buffer = make_shared<AL::Buffer>();
        size_t sampleCount = streaming ? SAMPLES_AT_ONCE : decoder->getSampleCount();
        vector<float> samples = decoder->extractSamples(sampleCount);
        buffer->upload(decoder->getFormat(), decoder->getFrequency(), samples.data(), decoder->getChannelCount() * samples.size() * sizeof(float));
        if(samples.size() < sampleCount || !streaming) {
            endOfFile = true;
        }
        return buffer;
    } else {
        return nullptr;
    }
}

void Carrot::Sound::rewind() {
    endOfFile = false;
    seek(0);
}

void Carrot::Sound::seek(size_t sampleIndex) {
    decoder->seek(sampleIndex);
}
