//
// Created by jglrxavpok on 18/04/2021.
//

#include "SoundSource.h"
#include "SoundThread.h"

Carrot::SoundSource::SoundSource() {

}

void Carrot::SoundSource::play(std::unique_ptr<Carrot::Sound>&& sound) {
    currentSound = move(sound);
    for (int i = 0; i < BUFFERS_AT_ONCE; ++i) {
        auto buffer = currentSound->getNextBuffer();
        if(buffer == nullptr)
            break;
        source.queue(buffer);
    }
    source.play();

    SoundThread::instance().registerSoundSource(shared_from_this());
}

void Carrot::SoundSource::updateAudio() {
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

bool Carrot::SoundSource::isReadyForCleanup() {
    return cleanupPolicy == CleanupPolicy::OnSoundEnd && (!currentSound || currentSound->hasBeenFullyRead()) && !isPlaying() && !looping;
}

Carrot::SoundSource::~SoundSource() {

}
