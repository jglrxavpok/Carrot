//
// Created by jglrxavpok on 18/04/2021.
//

#include <engine/utils/Macros.h>
#include <engine/audio/AudioManager.h>
#include <core/io/Logging.hpp>
#include "SoundSource.h"
#include "AudioThread.h"

namespace Carrot::Audio {
    SoundSource::SoundSource() {

    }

    void SoundSource::play(std::unique_ptr<Sound>&& sound) {
        currentSound = std::move(sound);
        source.removeAllBuffers();
        for (int i = 0; i < BUFFERS_AT_ONCE; ++i) {
            auto buffer = currentSound->getNextBuffer();
            if(buffer == nullptr)
                break;
            source.queue(buffer);
        }
        source.play();

        GetAudioManager().registerSoundSource(shared_from_this());
    }

    void SoundSource::updateAudio() {
        source.unqueue(source.getProcessedBufferCount());

        if(source.getQueuedBufferCount() < BUFFERS_AT_ONCE) {
            bool eof = false;
            size_t loaded = 0;
            for (int i = 0; i < BUFFERS_AT_ONCE; ++i) {
                auto buffer = currentSound->getNextBuffer();
                if(buffer == nullptr) {
                    eof = true;
                    break;
                }
                loaded = i;
                source.queue(buffer);
            }

            if(eof && looping) {
                currentSound->rewind();
                for (int i = 0; i < BUFFERS_AT_ONCE - loaded; ++i) {
                    auto buffer = currentSound->getNextBuffer();
                    if(buffer == nullptr) {
                        break;
                    }
                    source.queue(buffer);
                }
            }
        }
    }

    bool SoundSource::isReadyForCleanup() {
        return cleanupPolicy == CleanupPolicy::OnSoundEnd && (!currentSound || currentSound->hasBeenFullyRead()) && !isPlaying() && !looping;
    }

    void SoundSource::setGain(float gain) {
        this->gain = gain;
        source.updateGain(gain);
    }

    void SoundSource::setPosition(const glm::vec3& position) {
        source.setPosition(position);
    }

    SoundSource::~SoundSource() {

    }
}
