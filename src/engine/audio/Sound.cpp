//
// Created by jglrxavpok on 18/04/2021.
//

#include "Sound.h"
#include "decoders/WavDecoder.h"
#include <memory>

Carrot::Sound::Sound(const std::string& filename, bool streaming): streaming(streaming) {
    if(filename.ends_with(".wav")) {
        decoder = make_unique<WavDecoder>(filename);
/*    } else if(filename.ends_with(".mp3")) {

    } else if(filename.ends_with(".ogg")) {
*/
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
    if(streaming) {
        return nullptr; // TODO
    } else {
        if(!endOfFile) {
            auto buffer = make_shared<AL::Buffer>();
            vector<float> samples = decoder->extractSamples(decoder->getSampleCount());
            buffer->upload(decoder->getFormat(), decoder->getFrequency(), samples.data(), samples.size() * sizeof(float));
            endOfFile = true;
            return buffer;
        } else {
            return nullptr;
        }
    }
}

void Carrot::Sound::rewind() {
    endOfFile = false;
    seek(0);
}

void Carrot::Sound::seek(size_t sampleIndex) {
    decoder->seek(sampleIndex);
}
