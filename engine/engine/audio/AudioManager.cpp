//
// Created by jglrxavpok on 09/09/2023.
//

#include <core/utils/stringmanip.h>
#include <engine/audio/SFX.h>
#include <engine/audio/Sound.h>
#include "AudioManager.h"

namespace Carrot::Audio {

    AudioManager::AudioManager(): alDevice(AL::openDefaultDevice()), alContext(alDevice.createContext()) {
        alContext.makeCurrent();
    }

    std::shared_ptr<SFX> AudioManager::loadSFX(const Carrot::IO::Resource& soundFile) {
        const std::string& key = soundFile.getName();
        std::shared_ptr<SFX> keepAlive; // keep reference to new SFX instance at least until this function ends
        std::weak_ptr<SFX> sfx = loadedSFX.getOrCompute(key, [&]() {
            keepAlive = std::make_shared<SFX>(soundFile);
            return keepAlive;
        });
        if(auto ptr = sfx.lock()) {
            return ptr;
        } else {
            loadedSFX.remove(key);
            return loadSFX(soundFile);
        }
    }

    std::unique_ptr<Sound> AudioManager::loadMusic(const Carrot::IO::Resource& musicFile) {
        verify(musicFile.isFile(), "Non file music not supported for now");
        auto path = GetVFS().resolve(Carrot::IO::VFS::Path { musicFile.getName() });
        return std::make_unique<Sound>(Carrot::toString(path.u8string()), true /*streaming*/);
    }

    void AudioManager::tick(double deltaTime) {
        // TODO: cleanup loadedSFXs map
    }

    void AudioManager::registerSoundSource(std::shared_ptr<SoundSource> source) {
       verify(source != nullptr, "nullptr source, not allowed");
       thread.registerSoundSource(source);
    }

} // Carrot::Audio