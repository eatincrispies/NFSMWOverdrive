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

// ---------------------------------------------------------------------------
//  How MW stores gear tables, and why this works
//
//  Every car ships two attribute sets: <car>, the stock one, and <car>_top,
//  the fully-upgraded one. On a 5-speed car the stock GEAR_RATIO array holds
//  seven entries (reverse, neutral, five gears) and the _top array holds eight.
//  MW reads the gear count straight off that array's length, so the sixth gear
//  exists only in the _top set - which is why vanilla hands it over with the
//  last transmission package and not before.
//
//  The catch is that you cannot find a customised car by its collection key.
//  The moment a part is fitted, MW synthesises a new collection at runtime with
//  a key that is in no VLT file and changes every session - the same Cobalt
//  came through as 0x47B5D3C3, then 0xA3AFA91B, then 0x8CB46EF8. What is stable
//  is the parent link at +0x10, which points at the collection the runtime one
//  was derived from.
//
//  That gives a clean three-way split, all of it observed in-game:
//
//      key is <car>            no transmission upgrade
//      runtime key, parent <car>   some transmission package fitted
//      key is <car>_top        the last package
//
//  And because MW builds these per class, a runtime transmission collection
//  only appears when the transmission itself was upgraded. Fitting nitrous or
//  an engine package leaves it alone, so the trigger is exact rather than
//  approximate.
// ---------------------------------------------------------------------------

// The cars that have a <car>_top counterpart in the stock attributes.bin,
// verified by hashing "<name>_top" and searching the file. Everything absent
// from this list - bmwm3gtr, sl65, camaro, 911gt2, the cop, traffic and semi
// entries - ships fully specced with no _top at all.
const char* const kCarNames[] = {
    "911turbo",  "997s",       "a3",         "a4",         "carreragt",
    "caymans",   "clio",       "clk500",     "cobaltss",   "corvette",
    "cts",       "db9",        "eclipsegt",  "elise",      "fordgt",
    "gallardo",  "gti",        "gto",        "is300",      "lancerevo8",
    "monaro",    "murcielago", "mustanggt",  "punto",      "rx7",
    "rx8",       "sl500",      "slr",        "supra",      "tt",
    "viper",
};

constexpr size_t kCarCount = sizeof(kCarNames) / sizeof(kCarNames[0]);

struct Car {
    uint32_t base = 0;
    uint32_t top  = 0;
};

Car    g_cars[kCarCount];
size_t g_carCount = 0;

using TransmissionCtorFn = void*(__fastcall*)(void* self, void* edx, uint32_t key);
TransmissionCtorFn g_originalCtor = nullptr;

const char* NameOf(uint32_t hash) {
    for (size_t i = 0; i < g_carCount; ++i) {
        if (g_cars[i].base == hash || g_cars[i].top == hash) return kCarNames[i];
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

void BuildCarTable() {
    g_carCount = 0;
    for (const char* name : kCarNames) {
        char topName[64];
        _snprintf_s(topName, sizeof(topName), _TRUNCATE, "%s_top", name);

        Car car;
        car.base = game::BStringHash(name);
        car.top  = game::BStringHash(topName);
        if (car.base == 0 || car.top == 0) continue;

        g_cars[g_carCount++] = car;
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

// Which car is this collection made of, and had anything been fitted to it?
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

// Builds a throwaway wrapper for <car>_top purely to read its ratios. The
// constructor only looks collections up and stores pointers, so a stack buffer
// is safe and needs no teardown.
const void* TopGearStorage(uint32_t topKey) {
    uint8_t scratch[64] = {};
    g_originalCtor(scratch, nullptr, topKey);

    const void* collection = Deref(scratch, game::kWrapperCollection);
    return collection ? Deref(collection, game::kCollectionGearData) : nullptr;
}

void GrantSixthGear(void* wrapper) {
    void* collection = Deref(wrapper, game::kWrapperCollection);
    if (collection == nullptr) return;

    bool customised = false;
    const uint32_t baseCar = BaseCarFor(collection, &customised);
    if (baseCar == 0) return;

    // A car with a stock gearbox keeps its five gears - that is the point.
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
    if (topData == nullptr) return;

    uint16_t topHeader[2] = {};
    if (!Read(topData, 0, &topHeader)) return;
    const uint16_t topCount = topHeader[1];

    if (topCount <= count) return;      // already has at least as many gears
    if (topCount > capacity) return;    // would not fit; refuse rather than scribble

    const size_t bytes = topCount * sizeof(float);
    if (!game::IsWritable(data, game::kGearHeaderSize + bytes)) return;

    // Take the whole top ratio set. The sixth gear only exists as part of it,
    // and the ratios below it are spaced for it.
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

} // namespace

bool Install(char* reasonOut, size_t reasonSize) {
    BuildCarTable();
    log::Write("%u cars with a _top gear set", static_cast<unsigned>(g_carCount));

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

} // namespace tweaks
