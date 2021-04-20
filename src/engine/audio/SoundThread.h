//
// Created by jglrxavpok on 20/04/2021.
//

#pragma once

#include "Sound.h"
#include "SoundSource.h"
#include <memory>
#include <mutex>
#include <thread>

namespace Carrot {
    using namespace std;

    class SoundThread {
    private:
        bool running = false;
        std::thread backingThread;
        std::mutex sourceMutex;
        vector<shared_ptr<SoundSource>> currentSources;

        SoundThread();

        void threadCode();

        ~SoundThread();

    public:
        void registerSoundSource(const shared_ptr<SoundSource>& source);

        static SoundThread& instance() {
            static SoundThread threadInstance{};
            return threadInstance;
        }
    };
}
