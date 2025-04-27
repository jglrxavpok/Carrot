//
// Created by jglrxavpok on 09/09/2023.
//

#include <core/utils/stringmanip.h>
#include <engine/audio/SFX.h>
#include <engine/audio/Sound.h>
#include <engine/utils/Macros.h>
#include <engine/Engine.h>
#include <core/scripting/csharp/Engine.h>
#include <engine/scripting/CSharpBindings.h>
#include <engine/scripting/CSharpReflectionHelper.h>
#include <engine/scripting/CSharpHelpers.ipp>
#include <core/scripting/csharp/CSClass.h>
#include <core/scripting/csharp/CSObject.h>
#include "AudioManager.h"

#include "Music.h"

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

    std::shared_ptr<Music> AudioManager::loadMusic(const Carrot::IO::Resource& musicFile) {
        verify(musicFile.isFile(), "Non file music not supported for now");
        auto path = GetVFS().resolve(Carrot::IO::VFS::Path { musicFile.getName() });
        return std::make_shared<Music>(musicFile);
    }

    void AudioManager::tick(double deltaTime) {
        // TODO: cleanup loadedSFXs map
    }

    void AudioManager::stopAllSounds() {
        thread.requestStopAllSources();
    }

    void AudioManager::registerSoundSource(std::shared_ptr<SoundSource> source) {
       verify(source != nullptr, "nullptr source, not allowed");
       thread.registerSoundSource(source);
    }
    using namespace Scripting;
    struct AudioManagerBindings {
        CSClass* SoundSourceClass;
        CSClass* SFXClass;
        CSClass* MusicClass;

        static AudioManagerBindings& self() {
            return *((AudioManagerBindings*)GetAudioManager().bindingsImpl);
        }

        using SFXHandle = std::shared_ptr<SFX>;
        using MusicHandle = std::shared_ptr<Music>;
        using SoundSourceHandle = std::shared_ptr<SoundSource>;

        static MonoObject* SFXLoad(MonoString* vfsPathCS) {
            char* vfsPathStr = mono_string_to_utf8(vfsPathCS);
            CLEANUP(mono_free(vfsPathStr));

            return GetCSharpBindings().requestCarrotObject<SFXHandle>(self().SFXClass, GetAudioManager().loadSFX(vfsPathStr)).toMono();
        }

        static void SFXPlay(MonoObject* sfxObj, MonoObject* soundSourceObj, float pitch) {
            auto& pSFX = getObject<SFXHandle>(sfxObj);
            auto& pSoundSource = getObject<SoundSourceHandle>(soundSourceObj);

            std::unique_ptr<Sound> newInstance = pSFX->createInstance();
            pSoundSource->setPitch(pitch);
            pSoundSource->play(std::move(newInstance));
        }

        static MonoObject* MusicLoad(MonoString* vfsPathCS) {
            char* vfsPathStr = mono_string_to_utf8(vfsPathCS);
            CLEANUP(mono_free(vfsPathStr));

            return GetCSharpBindings().requestCarrotObject<MusicHandle>(self().MusicClass, GetAudioManager().loadMusic(vfsPathStr)).toMono();
        }

        static void MusicPlay(MonoObject* musicObj) {
            auto& pMusic = getObject<MusicHandle>(musicObj);
            pMusic->play();
        }

        static void MusicSetLooping(MonoObject* musicObj, bool loop) {
            auto& pMusic = getObject<MusicHandle>(musicObj);
            pMusic->setLooping(loop);
        }

        static void MusicStop(MonoObject* musicObj) {
            auto& pMusic = getObject<MusicHandle>(musicObj);
            pMusic->stop();
        }

        static MonoObject* SoundSourceCreate() {
            return GetCSharpBindings().requestCarrotObject<SoundSourceHandle>(self().SoundSourceClass, std::make_shared<Carrot::Audio::SoundSource>()).toMono();
        }

        static void SoundSourceSetGain(MonoObject* soundSourceObj, float gain) {
            auto& pSoundSource = getObject<SoundSourceHandle>(soundSourceObj);
            pSoundSource->setGain(gain);
        }

        static void SoundSourceSetPosition(MonoObject* soundSourceObj, glm::vec3 pos) {
            auto& pSoundSource = getObject<SoundSourceHandle>(soundSourceObj);
            pSoundSource->setPosition(pos);
        }

        static bool SoundSourceIsPlaying(MonoObject* soundSourceObj) {
            auto& pSoundSource = getObject<SoundSourceHandle>(soundSourceObj);
            return pSoundSource->isPlaying();
        }

        AudioManagerBindings() {
            GetCSharpBindings().registerEngineAssemblyLoadCallback([&]() {
                LOAD_CLASS_NS("Carrot.Audio", SoundSource);
                LOAD_CLASS_NS("Carrot.Audio", SFX);
                LOAD_CLASS_NS("Carrot.Audio", Music);
            });

            mono_add_internal_call("Carrot.Audio.SFX::Load", (void*)SFXLoad);
            mono_add_internal_call("Carrot.Audio.SFX::Play", (void*)SFXPlay);

            mono_add_internal_call("Carrot.Audio.Music::Load", (void*)MusicLoad);
            mono_add_internal_call("Carrot.Audio.Music::Play", (void*)MusicPlay);
            mono_add_internal_call("Carrot.Audio.Music::Stop", (void*)MusicStop);
            mono_add_internal_call("Carrot.Audio.Music::SetLooping", (void*)MusicSetLooping);

            mono_add_internal_call("Carrot.Audio.SoundSource::Create", (void*)SoundSourceCreate);
            mono_add_internal_call("Carrot.Audio.SoundSource::SetGain", (void*)SoundSourceSetGain);
            mono_add_internal_call("Carrot.Audio.SoundSource::SetPosition", (void*)SoundSourceSetPosition);
            mono_add_internal_call("Carrot.Audio.SoundSource::IsPlaying", (void*)SoundSourceIsPlaying);
        }
    };

    AudioManager::~AudioManager() {
        delete (AudioManagerBindings*)bindingsImpl;
    }

    void AudioManager::initScripting() {
        bindingsImpl = new AudioManagerBindings;
    }

} // Carrot::Audio