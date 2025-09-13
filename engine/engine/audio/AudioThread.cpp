//
// Created by jglrxavpok on 20/04/2021.
//

#include "AudioThread.h"
#include <core/async/OSThreads.h>

namespace Carrot::Audio {
    AudioThread::AudioThread() {
        running = true;
        backingThread = std::jthread([&](){ threadCode(); });
        Carrot::Threads::setName(backingThread, "Audio");
    }

    void AudioThread::threadCode() {
        while(running) {
            std::shared_ptr<SoundSource> pNewSource;
            while(newSources.popSafe(pNewSource)) {
                bool add = true;
                for(auto& source : currentSources) {
                    if(source.get() == pNewSource.get()) {
                        add = false;
                        break;
                    }
                }
                if(add) {
                    currentSources.emplace_back(std::move(pNewSource));
                }
            }
            if (requestedStopAllSources) {
                for(auto& source : currentSources) {
                    source->stop();
                }
                requestedStopAllSources = false;
            }
            for(auto& source : currentSources) {
                source->updateAudio();
            }

            currentSources.erase(std::remove_if(currentSources.begin(), currentSources.end(), [](auto& s) { return s->isReadyForCleanup(); }), currentSources.end());
            std::this_thread::yield();
        }
    }

    void AudioThread::requestStopAllSources() {
        requestedStopAllSources = true;
    }

    void AudioThread::registerSoundSource(std::shared_ptr<SoundSource> source) {
        newSources.push(std::move(source));
    }

    AudioThread::~AudioThread() {
        running = false;
        backingThread.join();
    }
}