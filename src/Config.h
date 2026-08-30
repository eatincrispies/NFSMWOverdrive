#pragma once

namespace config {

struct Settings {
    // Give 5-speed cars their 6th gear from the first transmission package
    // instead of the last.
    bool sixthGear = true;

    // Also patch cars with no transmission upgrade at all. Off by default -
    // a showroom-stock car keeping five gears is the point.
    bool includeStock = false;

    // Writes NFSMWOverdrive.log next to the .asi.
    bool log = false;

    // Verbose diagnostics: every transmission collection the game builds, plus
    // a header dump of any the plugin does not recognise.
    bool verbose = false;
};

// Reads the ini. A missing file or missing keys keep the defaults above, so
// the plugin works with no ini at all.
const Settings& Load(const char* iniPath);
const Settings& Get();

} // namespace config
