#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "transmission.hpp"

namespace Memory {

    namespace {

        int SafeCopy(void* destination, const void* source, size_t length) {
            __try {
                std::memcpy(destination, source, length);
                return 1;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                return 0;
            }
        }

    }

    bool Read(uintptr_t va, void* out, size_t length) {
        return SafeCopy(out, reinterpret_cast<const void*>(va), length) != 0;
    }

    bool Write(uintptr_t va, const void* bytes, size_t length) {
        void* target = reinterpret_cast<void*>(va);
        DWORD oldProtect = 0;
        if (!VirtualProtect(target, length, PAGE_EXECUTE_READWRITE, &oldProtect)) return false;
        const bool copied = SafeCopy(target, bytes, length) != 0;
        FlushInstructionCache(GetCurrentProcess(), target, length);
        DWORD unused = 0;
        VirtualProtect(target, length, oldProtect, &unused);
        return copied;
    }

}

namespace Patch {

    namespace {

        constexpr int kMostPatches = 16;

        struct Original {
            uintptr_t va;
            uint32_t  value;
        };

        Original gOriginals[kMostPatches] = {};
        int      gCount = 0;

    }

    bool ReplaceDword(uintptr_t va, uint32_t expected, uint32_t value) {
        uint32_t current = 0;
        if (gCount >= kMostPatches || !Memory::Read(va, &current, sizeof(current)) || current != expected
            || !Memory::Write(va, &value, sizeof(value)))
            return false;
        gOriginals[gCount++] = { va, current };
        return true;
    }

    bool RedirectCall(uintptr_t va, uintptr_t function, const void* replacement) {
        const uint32_t expected = static_cast<uint32_t>(function - (va + 5));
        const uint32_t relative = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(replacement) - (va + 5));
        uint8_t opcode = 0;
        return Memory::Read(va, &opcode, sizeof(opcode)) && opcode == 0xE8 && ReplaceDword(va + 1, expected, relative);
    }

    void RestoreAll() {
        while (gCount > 0) {
            const Original& original = gOriginals[--gCount];
            Memory::Write(original.va, &original.value, sizeof(original.value));
        }
    }

}

namespace Game {

    template <typename Return, typename... Args>
    Return ThisCall(uintptr_t function, Args... args) {
        return reinterpret_cast<Return (__thiscall*)(Args...)>(function)(args...);
    }

    template <typename Return, typename... Args>
    Return Call(uintptr_t function, Args... args) {
        return reinterpret_cast<Return (__cdecl*)(Args...)>(function)(args...);
    }

    template <typename Method>
    uint32_t MethodAddress(Method method) {
        static_assert(sizeof(method) == sizeof(void*), "only plain member functions have a single code address");
        uint32_t address = 0;
        std::memcpy(&address, &method, sizeof(address));
        return address;
    }

}

namespace Settings {

    namespace {

        constexpr const char* kGears   = "Gears";
        constexpr const char* kTranny  = "Tranny";
        constexpr const char* kEnabled = "Enabled";
        constexpr const char* kStock   = "IncludeStock";

        bool IniPath(void* module, char (&path)[MAX_PATH]) {
            const DWORD length = GetModuleFileNameA(static_cast<HMODULE>(module), path, MAX_PATH);
            char* dot = length && length < MAX_PATH ? std::strrchr(path, '.') : nullptr;
            if (!dot || dot < std::strrchr(path, '\\') || static_cast<size_t>(dot - path) + 5 > MAX_PATH) return false;
            std::strcpy(dot, ".ini");
            return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
        }

        char* Trim(char* text) {
            while (*text == ' ' || *text == '\t') ++text;
            char* end = text + std::strlen(text);
            while (end > text && (end[-1] == ' ' || end[-1] == '\t')) --end;
            *end = 0;
            return text;
        }

        bool IsTrue(const char* text) {
            return _stricmp(text, "1") == 0 || _stricmp(text, "true") == 0 || _stricmp(text, "yes") == 0 || _stricmp(text, "on") == 0;
        }

        bool ReadFlag(const char* path, const char* section, const char* key, bool fallback) {
            char text[32] = "";
            if (GetPrivateProfileStringA(section, key, "", text, sizeof(text), path) == 0) return fallback;
            char* value = Trim(text);
            return *value ? IsTrue(value) : fallback;
        }

    }

    bool Load(void* module, Overdrive& overdrive) {
        char path[MAX_PATH] = "";
        if (!IniPath(module, path)) return false;
        overdrive.sixthGear = ReadFlag(path, kGears, kEnabled, true);
        overdrive.includeStock = ReadFlag(path, kGears, kStock, false);
        overdrive.tranny = ReadFlag(path, kTranny, kEnabled, true);
        return true;
    }

}

namespace {

    constexpr const char* kVersion = "V1.2.0";

    constexpr uintptr_t kAttrib_FindCollection               = 0x00455FD0u;
    constexpr uintptr_t kAttrib_Database_sThis               = 0x0090DCBCu;
    constexpr uintptr_t kDatabase_GetClass                   = 0x00455BC0u;
    constexpr uintptr_t kClass_GetFirstCollection            = 0x00456B00u;
    constexpr uintptr_t kClass_GetNextCollection             = 0x00456B20u;
    constexpr uintptr_t kClass_GetCollection                 = 0x00455960u;
    constexpr uintptr_t kCollection_GetData                  = 0x00454190u;
    constexpr uintptr_t kAttributeStructPtrTransmission_Ctor = 0x006A5590u;
    constexpr uintptr_t kTransmissionConstructSites[]        = { 0x006B0D8Au, 0x006B1C90u, 0x006B21A8u, 0x006B2710u,
                                                                 0x006B3193u, 0x006B3A36u, 0x006B4043u, 0x006B445Du };
    constexpr uintptr_t kPhysicsUpgrades_GetLevel            = 0x00672D30u;
    constexpr uintptr_t kPhysicsUpgrades_GetMaxLevel         = 0x00672E80u;
    constexpr uintptr_t kCARSFX_GinsuEngine_InitializeEngine = 0x004EE1C0u;
    constexpr uintptr_t kInitializeEngineSlots[]             = { 0x008995B4u, 0x008995FCu, 0x00899644u };

    constexpr Attrib::Key kTransmissionClass     = 0x07A7A3E5u;
    constexpr Attrib::Key kPVehicleClass         = 0x4A97EC8Fu;
    constexpr Attrib::Key kPVehicle_transmission = 0x07A7A3E5u;

    constexpr int kMostGearSets          = 1024;
    constexpr int kMostTransmissionLevels = 16;
    constexpr int kMostCollections       = 8192;

    static_assert(sizeof(Attrib::Array) == 0x08, "Attrib::Array header is 8 bytes");
    static_assert(sizeof(Attrib::RefSpec) == 0x0C, "Attrib::RefSpec is 0x0C bytes");
    static_assert(offsetof(Attrib::Collection, mParent) == 0x10, "Attrib::Collection::mParent");
    static_assert(offsetof(Attrib::Collection, mLayout) == 0x18, "Attrib::Collection::mLayout");
    static_assert(offsetof(Attrib::Collection, mKey) == 0x20, "Attrib::Collection::mKey");
    static_assert(sizeof(Attrib::Instance) == 0x14, "Attrib::Instance is 0x14 bytes");
    static_assert(offsetof(Attrib::Instance, mCollection) == 0x04, "Attrib::Instance::mCollection");
    static_assert(offsetof(Attrib::Instance, mLayoutPtr) == 0x08, "Attrib::Instance::mLayoutPtr");
    static_assert(offsetof(Attrib::Gen::engineaudio::_LayoutStruct, CarID) == 0x94, "engineaudio CarID");
    static_assert(offsetof(Attrib::Gen::engineaudio::_LayoutStruct, Tranny) == 0xA3, "engineaudio Tranny");
    static_assert(offsetof(EAX_CarState, mAttributes) == 0x200, "EAX_CarState::mAttributes");
    static_assert(offsetof(EAX_CarState, mContext) == 0x228, "EAX_CarState::mContext");
    static_assert(offsetof(CSTATE_Base, m_pCar) == 0x34, "CSTATE_Base::m_pCar");
    static_assert(offsetof(EAXCar, mEngineInfo) == 0xD0, "EAXCar::mEngineInfo");
    static_assert(offsetof(CARSFX_GinsuEngine, m_pStateBase) == 0x14, "SndBase::m_pStateBase");
    static_assert(offsetof(CARSFX_GinsuEngine, m_pEAXCar) == 0x18, "SndBase::m_pEAXCar");

#if defined(_DEBUG)
    FILE* gLog = nullptr;

    void OpenLog(void* module) {
        char path[MAX_PATH] = "";
        const DWORD length = GetModuleFileNameA(static_cast<HMODULE>(module), path, MAX_PATH);
        char* dot = length && length < MAX_PATH ? std::strrchr(path, '.') : nullptr;
        if (!dot || dot < std::strrchr(path, '\\')) return;
        std::strcpy(dot, ".log");
        gLog = std::fopen(path, "w");
    }

    void WriteLog(const char* format, ...) {
        if (!gLog) return;
        va_list args;
        va_start(args, format);
        std::vfprintf(gLog, format, args);
        va_end(args);
        std::fputc('\n', gLog);
        std::fflush(gLog);
    }

    void CloseLog() {
        if (gLog) std::fclose(gLog);
        gLog = nullptr;
    }

#define LOG(...) WriteLog(__VA_ARGS__)
#else
    void OpenLog(void*) {}
    void CloseLog() {}
    void WriteLog(const char*, ...);

#define LOG(...) ((void)sizeof((WriteLog(__VA_ARGS__), 0)))
#endif

    struct GearSet {
        Attrib::Key stock = 0;
        Attrib::Key top = 0;
        bool        reported = false;
    };

    Settings::Overdrive gSettings;
    GearSet             gGearSets[kMostGearSets];
    int                 gGearSetCount = 0;
    bool                gGearSetsBuilt = false;

    GearSet* FindGearSet(Attrib::Key stock) {
        for (int i = 0; i < gGearSetCount; ++i)
            if (gGearSets[i].stock == stock) return &gGearSets[i];
        return nullptr;
    }

    void BuildGearSets() {
        gGearSetsBuilt = true;
        const Attrib::Class* pvehicles = Attrib::Database::Get().GetClass(kPVehicleClass);
        if (pvehicles == nullptr) return;
        int visited = 0;
        for (Attrib::Key key = pvehicles->GetFirstCollection(); key != 0 && visited < kMostCollections && gGearSetCount < kMostGearSets;
             key = pvehicles->GetNextCollection(key), ++visited) {
            const Attrib::Collection* vehicle = pvehicles->GetCollection(key);
            const Attrib::RefSpec* stock = vehicle ? static_cast<const Attrib::RefSpec*>(vehicle->GetData(kPVehicle_transmission, 0)) : nullptr;
            if (stock == nullptr || stock->mCollectionKey == 0) continue;
            const Attrib::RefSpec* top = stock;
            for (unsigned int level = 1; level < kMostTransmissionLevels; ++level) {
                const Attrib::RefSpec* next = static_cast<const Attrib::RefSpec*>(vehicle->GetData(kPVehicle_transmission, level));
                if (next == nullptr) break;
                top = next;
            }
            if (top->mCollectionKey == 0 || top->mCollectionKey == stock->mCollectionKey || FindGearSet(stock->mCollectionKey)) continue;
            GearSet& gearSet = gGearSets[gGearSetCount++];
            gearSet.stock = stock->mCollectionKey;
            gearSet.top = top->mCollectionKey;
            LOG("[Gears] pvehicle 0x%08X transmission stock 0x%08X top 0x%08X", key, gearSet.stock, gearSet.top);
        }
        LOG("[Gears] %d pvehicle collections, %d with an upgraded gear set", visited, gGearSetCount);
    }

    void GrantSixthGear(const Attrib::Gen::transmission& transmission) {
        if (!gGearSetsBuilt) BuildGearSets();
        const Attrib::Collection* collection = transmission.mCollection;
        if (collection == nullptr || collection->mLayout == nullptr) return;
        GearSet* gearSet = FindGearSet(collection->mKey);
        const bool upgraded = gearSet == nullptr;
        if (upgraded) gearSet = collection->mParent ? FindGearSet(collection->mParent->mKey) : nullptr;
        if (gearSet == nullptr || (!upgraded && !gSettings.includeStock)) return;

        Attrib::Array& gears = static_cast<Attrib::Gen::transmission::_LayoutStruct*>(collection->mLayout)->GEAR_RATIO;
        const Attrib::Collection* top = Attrib::FindCollection(kTransmissionClass, gearSet->top);
        if (top == nullptr || top->mKey != gearSet->top || top->mLayout == nullptr) {
            if (!gearSet->reported) LOG("[Gears] transmission 0x%08X: top gear set 0x%08X not found, skipped", gearSet->stock, gearSet->top);
            gearSet->reported = true;
            return;
        }
        const Attrib::Array& topGears = static_cast<Attrib::Gen::transmission::_LayoutStruct*>(top->mLayout)->GEAR_RATIO;
        if (topGears.mCount <= gears.mCount || topGears.mCount > gears.mAlloc) return;

        LOG("[Gears] transmission 0x%08X (stock 0x%08X) GEAR_RATIO mCount[+0x2] %u -> %u (%u -> %u forward gears) from 0x%08X", collection->mKey,
            gearSet->stock, gears.mCount, topGears.mCount, gears.mCount - 2u, topGears.mCount - 2u, gearSet->top);
        std::memcpy(gears.Data(), const_cast<Attrib::Array&>(topGears).Data(), topGears.mCount * sizeof(float));
        gears.mCount = topGears.mCount;
    }

    int GrantSixthGearGuarded(const Attrib::Gen::transmission& transmission) {
        __try {
            GrantSixthGear(transmission);
            return 1;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return 0;
        }
    }

    bool HasUltimateTransmission(const Attrib::Gen::pvehicle& vehicle) {
        const int most = Physics::Upgrades::GetMaxLevel(vehicle, Physics::Upgrades::PUT_TRANSMISSION);
        return most > 0 && Physics::Upgrades::GetLevel(vehicle, Physics::Upgrades::PUT_TRANSMISSION) >= most;
    }

    bool SetTranny(Attrib::Gen::engineaudio::_LayoutStruct* audio, bool tranny) {
        return Memory::Write(reinterpret_cast<uintptr_t>(&audio->Tranny), &tranny, sizeof(tranny));
    }

}

const Attrib::Collection* Attrib::FindCollection(Key classKey, Key collectionKey) {
    return Game::Call<const Collection*>(kAttrib_FindCollection, classKey, collectionKey);
}

const void* Attrib::Collection::GetData(Key attributeKey, unsigned int index) const {
    return Game::ThisCall<const void*>(kCollection_GetData, this, attributeKey, index);
}

Attrib::Key Attrib::Class::GetFirstCollection() const {
    return Game::ThisCall<Key>(kClass_GetFirstCollection, this);
}

Attrib::Key Attrib::Class::GetNextCollection(Key prev) const {
    return Game::ThisCall<Key>(kClass_GetNextCollection, this, prev);
}

const Attrib::Collection* Attrib::Class::GetCollection(Key collectionKey) const {
    return Game::ThisCall<const Collection*>(kClass_GetCollection, this, collectionKey);
}

Attrib::Database& Attrib::Database::Get() {
    return **reinterpret_cast<Database**>(kAttrib_Database_sThis);
}

Attrib::Class* Attrib::Database::GetClass(Key classKey) const {
    return Game::ThisCall<Class*>(kDatabase_GetClass, this, classKey);
}

int Physics::Upgrades::GetLevel(const Attrib::Gen::pvehicle& vehicle, Type type) {
    return Game::Call<int>(kPhysicsUpgrades_GetLevel, &vehicle, type);
}

int Physics::Upgrades::GetMaxLevel(const Attrib::Gen::pvehicle& vehicle, Type type) {
    return Game::Call<int>(kPhysicsUpgrades_GetMaxLevel, &vehicle, type);
}

template <>
AttributeStructPtr<Attrib::Gen::transmission>* AttributeStructPtr<Attrib::Gen::transmission>::Construct(unsigned int key) {
    return Game::ThisCall<AttributeStructPtr*>(kAttributeStructPtrTransmission_Ctor, this, key);
}

template <>
AttributeStructPtr<Attrib::Gen::transmission>* AttributeStructPtr<Attrib::Gen::transmission>::ConstructHook(unsigned int key) {
    AttributeStructPtr* result = Construct(key);
    if (gSettings.sixthGear && !GrantSixthGearGuarded(*this)) LOG("[Gears] transmission 0x%08X faulted, left alone", key);
    return result;
}

void CARSFX_GinsuEngine::InitializeEngine() {
    Game::ThisCall<void>(kCARSFX_GinsuEngine_InitializeEngine, this);
}

void CARSFX_GinsuEngine::InitializeEngineHook() {
    EAX_CarState* car = m_pStateBase ? GetPhysCar() : nullptr;
    Attrib::Gen::engineaudio::_LayoutStruct* audio = m_pEAXCar ? m_pEAXCar->GetAttributes().GetLayout() : nullptr;
    const bool raised = gSettings.tranny && car && audio && car->mContext == Sound::CONTEXT_PLAYER && !audio->Tranny
                     && HasUltimateTransmission(car->mAttributes) && SetTranny(audio, true);
    if (raised)
        LOG("[Tranny] CARSFX_GinsuEngine 0x%p Ultimate transmission, engineaudio CarID[+0x94]=%u Tranny[+0xA3] 0 -> 1 for InitializeEngine", this,
            audio->CarID);
    InitializeEngine();
    if (raised) SetTranny(audio, false);
}

bool Overdrive::Init(void* module) {
    OpenLog(module);
    LOG("NFSMWOverdrive %s", kVersion);
    if (!Settings::Load(module, gSettings)) {
        LOG("NFSMWOverdrive.ini not found, nothing changed");
        return false;
    }
    LOG("[Gears] Enabled=%d IncludeStock=%d [Tranny] Enabled=%d", gSettings.sixthGear, gSettings.includeStock, gSettings.tranny);

    int gears = 0;
    if (gSettings.sixthGear)
        for (uintptr_t site : kTransmissionConstructSites) {
            const bool redirected = Patch::RedirectCall(site, kAttributeStructPtrTransmission_Ctor,
                                                        reinterpret_cast<const void*>(Game::MethodAddress(
                                                            &AttributeStructPtr<Attrib::Gen::transmission>::ConstructHook)));
            gears += redirected;
            LOG("AttributeStructPtr<transmission> call[0x%08X] -> ConstructHook = %d", site, redirected);
        }

    int tranny = 0;
    if (gSettings.tranny)
        for (uintptr_t slot : kInitializeEngineSlots) {
            const bool replaced = Patch::ReplaceDword(slot, kCARSFX_GinsuEngine_InitializeEngine,
                                                      Game::MethodAddress(&CARSFX_GinsuEngine::InitializeEngineHook));
            tranny += replaced;
            LOG("CARSFX_GinsuEngine::InitializeEngine vtable slot[0x%08X] -> InitializeEngineHook = %d", slot, replaced);
        }
    return gears > 0 || tranny > 0;
}

void Overdrive::Restore() {
    Patch::RestoreAll();
    CloseLog();
}
