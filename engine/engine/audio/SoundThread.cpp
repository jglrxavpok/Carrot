//
// Created by jglrxavpok on 20/04/2021.
//

#include "SoundThread.h"
#include <core/async/OSThreads.h>

Carrot::SoundThread::SoundThread() {
    running = true;
    backingThread = std::thread([&](){ threadCode(); });
    Carrot::Threads::setName(backingThread, "Audio");
}

void Carrot::SoundThread::threadCode() {
    while(running) {
        std::lock_guard lk(sourceMutex);
        for(auto& source : currentSources) {
            source->updateAudio();
        }

        currentSources.erase(std::remove_if(currentSources.begin(), currentSources.end(), [](auto& s) { return s->isReadyForCleanup(); }), currentSources.end());
        std::this_thread::yield();
    }
}

void Carrot::SoundThread::registerSoundSource(const std::shared_ptr<Carrot::SoundSource>& source) {
    std::lock_guard lk(sourceMutex);
    currentSources.push_back(source);
}

Carrot::SoundThread& Carrot::SoundThread::instance() {
    static SoundThread threadInstance{};
    return threadInstance;
}

Carrot::SoundThread::~SoundThread() {
    running = false;
    backingThread.join();
}
