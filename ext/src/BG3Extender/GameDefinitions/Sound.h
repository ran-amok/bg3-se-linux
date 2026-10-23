#pragma once

#include <GameDefinitions/Base/Base.h>
#include <GameDefinitions/EntitySystem.h>

BEGIN_SE()

struct SoundManager : public ProtectedGameObject<SoundManager>
{
#if defined(BG3SE_LINUX)
    // PORT (linux): upstream's `Destroy` is MSVC's one-slot scalar deleting destructor; Itanium gives
    // it two (D1, D0), and with one slot every call below landed one slot early (SetSwitch on
    // SeekEvent, GetRTPCValue on SetRTPCValue). Witness, ww::WwiseManager's vtable 0x7a04b40: slot 0
    // 0x6a29350 is ~WwiseManager (matched), slot 1 0x6a29460 calls it and tail-jumps to operator
    // delete. Same declaration as BasicModel's (Render.h).
    virtual ~SoundManager() = 0;
#else
    virtual void Destroy() = 0;
#endif
    virtual void Load() = 0;
    virtual void Unload() = 0;
    virtual void Update() = 0;
    virtual void SetObjectTransformation() = 0;
    virtual void SetObjectTransformation2() = 0;
    virtual void SetObjectAttenuationScale() = 0;
    virtual void SetObjectListener() = 0;
    virtual void ActivateListener() = 0;
    virtual void DeactivateListener() = 0;
    virtual void GetAttenuationDistance() = 0;
    virtual void SetEnvironmentSends() = 0;
    virtual void SetPanningRule() = 0;
    virtual void AddRumbleDevice() = 0;
    virtual void RemoveRumbleDevice() = 0;
    virtual void AddRumbleListener() = 0;
    virtual void RemoveRumbleListener() = 0;
    virtual void SeekEvent() = 0;
    virtual void SeekEvent2() = 0;

    virtual bool SetSwitch(const char* switchGroup, const char* switchState, SoundObjectId gameObjectId) = 0;
    virtual bool SetState(const char* stateGroup, const char* state) = 0;
    virtual bool SetRTPCValue(SoundObjectId gameObjectId, const char* name, float value, bool bypassInternalValueInterpolation = false) = 0;
    virtual float GetRTPCValue(SoundObjectId gameObjectId, const char* name) = 0;
    virtual void ResetRTPCValue(SoundObjectId gameObjectId, const char* name) = 0;
    virtual void CancelCallback(void* cookie) = 0;
    virtual void PauseSound(uint32_t playingId) = 0;
    virtual void ResumeSound(uint32_t playingId) = 0;
    virtual void StopSound(uint32_t playingId, uint32_t transitionDurationMs = 0) = 0;
    virtual void StopSounds(SoundObjectId gameObjectId, uint32_t transitionDurationMs = 0) = 0;
    virtual void StopAllSounds() = 0;
    virtual void StopAllHUDSounds(uint8_t playerIndex) = 0;
    virtual void StopAllCineHUDSounds(uint8_t playerIndex) = 0;
    virtual void StopAllMusicSounds() = 0;
    virtual void StopAllAmbientSounds(uint8_t playerIndex) = 0;
    virtual void PauseAllSounds() = 0;
    virtual void ResumeAllSounds() = 0;
    virtual void SetObstructionAndOcclusion(SoundObjectId emitterGameObjectId, uint8_t playerIndex, float obstructionLevel, float occlusionLevel) = 0;
    virtual int GetMainOutputChannelCount(bool playerIndex) = 0;
    virtual void SetListenerSpatialization(uint8_t playerIndex, bool spatialized, float* speakerVolumes) = 0;
    virtual bool RegisterDeviceStatusChangedCallback() = 0;
    virtual bool IsAnyControllerSpeakerConnected() = 0;
    virtual bool GetMemoryStats(void*) = 0;
    virtual bool GetSourcePlayPosition(uint32_t playingId, uint32_t* position) = 0;
    virtual SoundNameId GetIDFromString(char const* name) = 0;
    virtual void Suspend() = 0;
    virtual void Resume() = 0;
    virtual void Initialize(void*) = 0;
    virtual void RenderAudio(GameTime const&) = 0;
    virtual void Terminate() = 0;
    virtual void ClearBanks() = 0;
    virtual void ForceUnloadAllSound() = 0;
    virtual void ForceReload() = 0;
#if defined(BG3SE_LINUX)
    // PORT (linux): upstream lists this run in MSVC's vtable order, which differs from Itanium's in two
    // ways, and declares the bank calls with MSVC's calling convention. (1) MSVC puts each set of
    // overloaded virtuals together, at the first declaration's position, in reverse order; Itanium keeps
    // declaration order. So the Load/UnloadEvent_Blocking/_Deferred overloads and the three
    // PostEventInternal overloads sit differently here, and upstream's LoadEvent(id) was the
    // game's LoadEvent_Blocking(char const*), and PostEventByName its UpdateEndpointTypeStateInternal.
    // (2) A member function returning a class type (ls::TypeWrap<>) returns through a hidden pointer
    // under MSVC, which upstream spells as an out-reference plus a pointer return. Under Itanium the
    // TypeWrap is trivially copyable and comes back in rax: LoadBank_Blocking 0x30db8a0,
    // LoadBank_Deferred 0x30dbd30, PrepareBank_Blocking 0x6a29cf0, PrepareBank_Deferred 0x30dc100 take
    // the name in rsi and return the id in eax (0xffffffff on failure), RegisterSoundObjectInternal
    // 0x33476d0 returns the handle in rax. Witness: ww::WwiseManager's vtable 0x7a04b40 on a187af94, 81
    // slots, 80 matched function for function to the macOS oracle's _ZTVN2ww12WwiseManagerE+0x10 slot
    // for slot, the names below are the oracle's.
    virtual SoundObjectId RegisterSoundObject(char const*) = 0;                                  // 53
    virtual void UnregisterSoundObject(SoundObjectId soundObjectId) = 0;
    virtual bool RegisterBusMeteringCallback(FixedString const&) = 0;
    virtual bool LoadEventByName(char const*) = 0;                                               // 56
    virtual bool UnloadEventByName(char const*) = 0;
    virtual bool LoadEvent(SoundNameId eventId) = 0;
    virtual bool UnloadEvent(SoundNameId eventId) = 0;
    virtual bool AsyncLoadEventByName(char const*, void* callback) = 0;
    virtual bool AsyncUnloadEventByName(char const*, void* callback) = 0;
    virtual bool AsyncLoadEvent(SoundNameId eventId, void* callback) = 0;
    virtual bool AsyncUnloadEvent(SoundNameId eventId, void* callback) = 0;
    virtual SoundNameId LoadBank(const char* bankName) = 0;                                      // 64
    virtual bool UnloadBank(SoundNameId bankId) = 0;
    virtual SoundNameId AsyncLoadBank(const char* bankName, void* callback) = 0;
    virtual bool AsyncUnloadBank(SoundNameId bankId, void* callback) = 0;
    virtual SoundNameId PrepareBank(const char* name) = 0;
    virtual bool UnprepareBank(SoundNameId bankId) = 0;
    virtual SoundNameId AsyncPrepareBank(const char* name, void* callback) = 0;
    virtual bool AsyncUnpepareBank(SoundNameId bankId, void* callback) = 0;
    virtual bool LoadGameSync(int gameSyncType, const char* groupName, const char* gameSyncName) = 0;
    virtual bool UnloadGameSyncs(int gameSyncType, const char* groupName, const char* gameSyncName) = 0;
    virtual bool AsyncLoadGameSync(int gameSyncType, const char* groupName, const char* gameSyncName, void* callback) = 0;
    virtual bool AsyncUnloadGameSync(int gameSyncType, const char* groupName, const char* gameSyncName, void* callback) = 0;
    virtual void QueueSoundEvent(void* evt) = 0;                                                  // 76
    virtual bool UpdateEndpointTypeState() = 0;
    virtual bool PostEventByName(SoundObjectId gameObjectId, char const* eventName, float seekPosition, bool enableGetSourcePlayPosition, void* callback) = 0;
    virtual bool PostEventById(SoundObjectId gameObjectId, SoundNameId eventId, float seekPosition, bool enableGetSourcePlayPosition, void* callback) = 0;
    virtual bool PlayExternalSound(SoundObjectId gameObjectId, SoundNameId eventId, STDString const& path, uint8_t codecId, float seekPosition, bool enableGetSourcePlayPosition, void* callback) = 0;
#else
    virtual SoundObjectId* RegisterSoundObject(SoundObjectId& soundObjectId, char const*) = 0;
    virtual void UnregisterSoundObject(SoundObjectId soundObjectId) = 0;
    virtual bool RegisterBusMeteringCallback(FixedString const&) = 0;
    virtual bool LoadEvent(SoundNameId eventId) = 0;
    virtual bool LoadEventByName(char const*) = 0;
    virtual bool UnloadEvent(SoundNameId eventId) = 0;
    virtual bool UnloadEventByName(char const*) = 0;
    virtual bool AsyncLoadEvent(SoundNameId eventId, void* callback) = 0;
    virtual bool AsyncLoadEventByName(char const*, void* callback) = 0;
    virtual bool AsyncUnloadEvent(SoundNameId eventId, void* callback) = 0;
    virtual bool AsyncUnloadEventByName(char const*, void* callback) = 0;
    virtual SoundNameId* LoadBank(SoundNameId& bankId, const char* bankName) = 0;
    virtual bool UnloadBank(SoundNameId bankId) = 0;
    virtual SoundNameId* AsyncLoadBank(SoundNameId& bankId, const char* bankName, void* callback) = 0;
    virtual bool AsyncUnloadBank(SoundNameId bankId, void* callback) = 0;
    virtual SoundNameId* PrepareBank(SoundNameId& bankId, const char* name) = 0;
    virtual bool UnprepareBank(SoundNameId bankId) = 0;
    virtual SoundNameId* AsyncPrepareBank(SoundNameId& bankId, const char* name, void* callback) = 0;
    virtual bool AsyncUnpepareBank(SoundNameId bankId, void* callback) = 0;
    virtual bool LoadGameSync(int gameSyncType, const char* groupName, const char* gameSyncName) = 0;
    virtual bool UnloadGameSyncs(int gameSyncType, const char* groupName, const char* gameSyncName) = 0;
    virtual bool AsyncLoadGameSync(int gameSyncType, const char* groupName, const char* gameSyncName, void* callback) = 0;
    virtual bool AsyncUnloadGameSync(int gameSyncType, const char* groupName, const char* gameSyncName, void* callback) = 0;
    virtual bool PostEventById(SoundObjectId gameObjectId, SoundNameId eventId, float seekPosition, bool enableGetSourcePlayPosition, void* callback) = 0;
    virtual bool PostEventByName(SoundObjectId gameObjectId, char const* eventName, float seekPosition, bool enableGetSourcePlayPosition, void* callback) = 0;
    virtual void QueueSoundEvent(void* evt) = 0;
    virtual bool UpdateEndpointTypeState() = 0;
    virtual bool PlayExternalSound(SoundObjectId gameObjectId, SoundNameId eventId, STDString const& path, uint8_t codecId, float seekPosition, bool enableGetSourcePlayPosition, void* callback) = 0;
#endif


    UnknownSignal field_10;
    Module Module;
    Array<FixedString> OS_SoundbankSearchPath_SfxSoundbankSearchPaths;
    Array<FixedString> OS_SoundbankSearchPath2_LocaSoundbankSearchPaths;
    std::array<SoundObjectId, 4> ControllerSpeakerListener;
    std::array<SoundObjectId, 4> Listener;
    std::array<SoundObjectId, 4> RumbleListener;
    std::array<SoundObjectId, 4> PlayerEmitter;
    std::array<SoundObjectId, 4> Ambient;
    std::array<SoundObjectId, 4> HUD;
    std::array<SoundObjectId, 4> CineHUD;
    SoundObjectId MusicHandle;
    __int64 field_270;
};

struct WwiseManager : public SoundManager
{};

END_NS()
