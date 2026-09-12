#pragma once

#include <stddef.h>

namespace config {

constexpr size_t kMaxCars       = 128;
constexpr size_t kCarNameLength = 32;

struct Settings {
    bool sixthGear    = true;
    bool includeStock = false;
    bool log          = false;
    bool verbose      = false;

    char   cars[kMaxCars][kCarNameLength] = {};
    size_t carCount     = 0;
    bool   carsFromIni  = false;
};

const Settings& Load(const char* iniPath);
const Settings& Get();

}
