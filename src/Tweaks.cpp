#include "Tweaks.h"

#include "Config.h"
#include "Game.h"
#include "Hook.h"
#include "Log.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>

namespace tweaks {
namespace {
const char* const kStockCars[] = {
    "911turbo",  "997s",       "a3",         "a4",         "carreragt",
    "caymans",   "clio",       "clk500",     "cobaltss",   "corvette",
    "cts",       "db9",        "eclipsegt",  "elise",      "fordgt",
    "gallardo",  "gti",        "gto",        "is300",      "lancerevo8",
    "monaro",    "murcielago", "mustanggt",  "punto",      "rx7",
    "rx8",       "sl500",      "slr",        "supra",      "tt",
    "viper",
};


struct Car {
    uint32_t    base = 0;
    uint32_t    top  = 0;
    const char* name = nullptr;
};

Car    g_cars[config::kMaxCars];
size_t g_carCount = 0;

using TransmissionCtorFn = void*(__fastcall*)(void* self, void* edx, uint32_t key);
TransmissionCtorFn g_originalCtor = nullptr;

const char* NameOf(uint32_t hash) {
    for (size_t i = 0; i < g_carCount; ++i) {
        if (g_cars[i].base == hash || g_cars[i].top == hash) return g_cars[i].name;
    }
    return nullptr;
}

uint32_t TopFor(uint32_t baseHash) {
    for (size_t i = 0; i < g_carCount; ++i) {
        if (g_cars[i].base == baseHash) return g_cars[i].top;
    }
    return 0;
}

bool IsKnown(uint32_t hash) { return NameOf(hash) != nullptr; }

void AddCar(const char* name) {
    if (name == nullptr || *name == '\0' || g_carCount >= config::kMaxCars) return;

    char topName[config::kCarNameLength + 8];
    _snprintf_s(topName, sizeof(topName), _TRUNCATE, "%s_top", name);

    Car car;
    car.base = game::BStringHash(name);
    car.top  = game::BStringHash(topName);
    car.name = name;
    if (car.base == 0 || car.top == 0) return;

    for (size_t i = 0; i < g_carCount; ++i) {
        if (g_cars[i].base == car.base) return;
    }

    g_cars[g_carCount++] = car;
}

void LogCarList() {
    char line[1024];
    size_t used = 0;
    line[0] = 0;

    for (size_t i = 0; i < g_carCount; ++i) {
        const int written = _snprintf_s(line + used, sizeof(line) - used, _TRUNCATE,
                                        "%s%s", used ? " " : "", g_cars[i].name);
        if (written < 0) break;
        used += static_cast<size_t>(written);
    }

    log::Write("cars: %s", line);
}

void BuildCarTable() {
    g_carCount = 0;

    const config::Settings& settings = config::Get();

    if (settings.carsFromIni) {
        for (size_t i = 0; i < settings.carCount; ++i) {
            AddCar(settings.cars[i]);
        }
    } else {
        for (const char* name : kStockCars) AddCar(name);
    }
}

template <typename T>
bool Read(const void* base, uintptr_t offset, T* out) {
    return game::SafeRead(static_cast<const uint8_t*>(base) + offset, out, sizeof(T));
}

void* Deref(const void* base, uintptr_t offset) {
    void* value = nullptr;
    if (base == nullptr || !Read(base, offset, &value)) return nullptr;
    return value;
}

uint32_t BaseCarFor(const void* collection, bool* customised) {
    uint32_t key = 0;
    if (!Read(collection, game::kCollectionKey, &key)) return 0;

    if (IsKnown(key)) {
        *customised = false;
        return key;
    }

    const void* parent = Deref(collection, game::kCollectionParent);
    uint32_t parentKey = 0;
    if (parent == nullptr || !Read(parent, game::kCollectionKey, &parentKey)) return 0;
    if (!IsKnown(parentKey)) return 0;

    *customised = true;
    return parentKey;
}

const void* TopGearStorage(uint32_t topKey) {
    uint8_t scratch[64] = {};
    g_originalCtor(scratch, nullptr, topKey);

    const void* collection = Deref(scratch, game::kWrapperCollection);
    if (collection == nullptr) return nullptr;

    uint32_t key = 0;
    if (!Read(collection, game::kCollectionKey, &key) || key != topKey) return nullptr;

    return Deref(collection, game::kCollectionGearData);
}

void GrantSixthGear(void* wrapper) {
    void* collection = Deref(wrapper, game::kWrapperCollection);
    if (collection == nullptr) return;

    bool customised = false;
    const uint32_t baseCar = BaseCarFor(collection, &customised);
    if (baseCar == 0) return;

    if (!customised && !config::Get().includeStock) return;

    const uint32_t topKey = TopFor(baseCar);
    if (topKey == 0) return;

    void* data = Deref(collection, game::kCollectionGearData);
    if (data == nullptr) return;

    uint16_t header[2] = {};
    if (!Read(data, 0, &header)) return;
    const uint16_t capacity = header[0];
    const uint16_t count    = header[1];

    const void* topData = TopGearStorage(topKey);
    if (topData == nullptr) {
        log::Once("%s: no _top gear set found, skipped", NameOf(baseCar));
        return;
    }

    uint16_t topHeader[2] = {};
    if (!Read(topData, 0, &topHeader)) return;
    const uint16_t topCount = topHeader[1];

    if (topCount <= count) return;
    if (topCount > capacity) return;

    const size_t bytes = topCount * sizeof(float);
    if (!game::IsWritable(data, game::kGearHeaderSize + bytes)) return;

    float ratios[16] = {};
    if (!game::SafeRead(static_cast<const uint8_t*>(topData) + game::kGearHeaderSize,
                        ratios, bytes)) {
        return;
    }

    memcpy(static_cast<uint8_t*>(data) + game::kGearHeaderSize, ratios, bytes);
    static_cast<uint16_t*>(data)[1] = topCount;

    log::Once("%s: %u -> %u gears", NameOf(baseCar), count - 2u, topCount - 2u);
}

void* __fastcall TransmissionCtorDetour(void* self, void* edx, uint32_t key) {
    void* result = g_originalCtor(self, edx, key);

    if (config::Get().sixthGear) {
        __try {
            GrantSixthGear(self);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            log::Once("patch faulted on collection 0x%08X, left alone", key);
        }
    }

    if (config::Get().verbose) {
        log::Once("collection 0x%08X (%s)", key,
                  NameOf(key) ? NameOf(key) : "runtime or unlisted");
    }

    return result;
}
}

bool Install(char* reasonOut, size_t reasonSize) {
    BuildCarTable();
    log::Write("%u cars known (%s)",
               static_cast<unsigned>(g_carCount),
               config::Get().carsFromIni ? "from ini" : "built-in fallback list");
    LogCarList();

    if (!config::Get().sixthGear && !config::Get().verbose) {
        log::Write("nothing enabled, no hooks installed");
        return true;
    }

    if (!hook::InstallDetour(game::kTransmissionCtor,
                             reinterpret_cast<const void*>(&TransmissionCtorDetour),
                             game::kTransmissionCtorBytes,
                             game::kTransmissionCtorPrologue,
                             reinterpret_cast<void**>(&g_originalCtor))) {
        _snprintf_s(reasonOut, reasonSize, _TRUNCATE,
                    "could not hook the transmission constructor at 0x%08X",
                    static_cast<unsigned>(game::kTransmissionCtor));
        return false;
    }

    log::Write("ready");
    return true;
}
}
