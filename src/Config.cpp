#include "Config.h"

#include <windows.h>
#include <string.h>

namespace config {
namespace {
Settings g_settings;

bool Truthy(const char* value) {
    return _stricmp(value, "1")   == 0 || _stricmp(value, "true") == 0 ||
           _stricmp(value, "yes") == 0 || _stricmp(value, "on")   == 0;
}

bool ReadBool(const char* section, const char* key, bool fallback,
              const char* iniPath) {
    char value[32] = {};
    if (GetPrivateProfileStringA(section, key, "", value, sizeof(value), iniPath) == 0 ||
        value[0] == '\0') {
        return fallback;
    }
    return Truthy(value);
}

void ReadCars(Settings& settings, const char* iniPath) {
    char section[16384] = {};
    if (GetPrivateProfileSectionA("Cars", section, sizeof(section), iniPath) == 0) {
        return;
    }

    settings.carsFromIni = true;

    for (const char* entry = section;
         *entry != '\0' && settings.carCount < kMaxCars;
         entry += strlen(entry) + 1) {

        const char* equals = strchr(entry, '=');
        if (equals != nullptr && !Truthy(equals + 1)) continue;

        size_t name = equals ? static_cast<size_t>(equals - entry) : strlen(entry);
        while (name > 0 && (entry[name - 1] == ' ' || entry[name - 1] == '\t')) {
            --name;
        }
        if (name == 0 || name >= kCarNameLength) continue;

        memcpy(settings.cars[settings.carCount], entry, name);
        settings.cars[settings.carCount][name] = '\0';
        ++settings.carCount;
    }
}
}

const Settings& Load(const char* iniPath) {
    g_settings.sixthGear    = ReadBool("Gears", "Enabled",      true,  iniPath);
    g_settings.includeStock = ReadBool("Gears", "IncludeStock", false, iniPath);
    g_settings.log          = ReadBool("Debug", "Log",          false, iniPath);
    g_settings.verbose      = ReadBool("Debug", "Verbose",      false, iniPath);

    g_settings.carCount    = 0;
    g_settings.carsFromIni = false;
    ReadCars(g_settings, iniPath);

    return g_settings;
}

const Settings& Get() { return g_settings; }

}
