# NFSMW Overdrive

An ASI plugin for *Need for Speed: Most Wanted* (2005, PC).

**Five-speed cars get their sixth gear from the first transmission package
instead of the last.** Fit a Race gearbox to a Cobalt SS and it pulls sixth,
rather than making you wait for the Ultimate package.

Cars with a stock gearbox are untouched, and upgrading anything else — engine,
nitrous, tyres — does not trigger it.

**The Ultimate transmission package brings the straight-cut gear whine.** MW
only gives the M3 GTR's straight-cut gearbox sound to a car whose engine audio
has it switched on in the VLT, so a car either has it or never will. With this
plugin, beat Webster, fit the Ultimate transmission, and the whine kicks in.

## Install

Drop `NFSMWOverdrive.asi` and `NFSMWOverdrive.ini` into the game's
`scripts` folder. Uninstall by deleting the two files.

You need an ASI loader, which you already have if you run any other MW mods.
Plays nicely alongside Unlimiter, Bartender, ExtraOptions and the Widescreen
Fix.

## Settings

`NFSMWOverdrive.ini`, read once at launch.

| Key | Default | |
| --- | --- | --- |
| `[Gears] Enabled` | `1` | the feature |
| `[Gears] IncludeStock` | `0` | also give the 6th to cars with no gearbox upgrade |
| `[Tranny] Enabled` | `1` | the straight-cut whine with the Ultimate transmission |

## Cars

There is no car list. At startup the plugin reads every car's `pvehicle`
entry in the VLT, which lists the transmissions the game uses for that car:
the stock set first, the fully upgraded set last. Every car with two different
sets gets the 6th gear, so add-on cars work as soon as they have an upgraded
gear set, whatever it is called.

That also covers cars a name list could never find. The Impreza's stock gear
set is not called `imprezawrx`, so it was missing from the old list; it is
picked up now. Cars whose upgraded set has no extra gear, or that already have
six, are left alone. The M3 GTR, SL65, Camaro and 911 GT2 ship fully specced
with a single gear set, so there is nothing to draw from.

The Debug build writes `NFSMWOverdrive.log` beside the `.asi`, listing every
gear set pair it found and then each car as you drive it.

## How it works

Every upgradable car ships two transmission sets, named in its `pvehicle`
`transmission` list: the stock one and the fully upgraded one (`<car>_top` for
most stock cars). On a 5-speed car the stock `GEAR_RATIO` array holds seven entries (reverse,
neutral, five gears) and the `_top` array holds eight.

There is **no `NUM_RATIOS` field** anywhere in `attributes.bin` — MW takes the
gear count straight from that array's length. The sixth gear exists only in the
`_top` set, which is exactly why vanilla hands it over with the last package.

Finding a customised car by its collection key does not work: the moment a part
is fitted, MW synthesises a collection at runtime whose key is in no VLT file
and changes every session. What is stable is the parent link:

```
+0x10   parent collection    customised Cobalt -> "cobaltss"
                             stock "traffic"   -> "default"
+0x18   GEAR_RATIO storage   [u16 capacity][u16 count][hdr][floats]
+0x20   this collection's key
```

`count` is 7 on a 5-speed and 8 on a 6-speed; `capacity` is 9 either way — the
class maximum — so the extra slot already exists. The plugin walks `+0x10` to
identify the car, reads `<car>_top`'s ratios, copies them in and bumps `count`.
Nothing is reallocated, and it refuses if the count would not fit, the memory
is not writable, or the car already has enough gears.

Because MW builds these collections per class, a runtime transmission
collection only appears when the *transmission* was upgraded — which is what
makes the trigger exact rather than approximate.

Verified in-game: Cobalt SS on a Race gearbox pulls 6th; stock Cobalt stays at
5; RX-7 with nitrous and a stock gearbox stays at 5.

## The straight-cut whine

The sound is `Csis::CAR_TRANNY`, from the global bank `CAR_TRANNY.abk`, which
sits in the same always-loaded bank list as the traffic, rain and nitrous
banks. `CARSFX_GinsuEngine::InitializeEngine` only creates it for the player's
car, and only when the car's `engineaudio` collection has `Tranny` set; the M3
GTR is the one stock car that does. Its volume follows your road speed.

The plugin wraps `InitializeEngine`. When your car's transmission is at its
highest package (`Physics::Upgrades::GetLevel` equals `GetMaxLevel` for
`PUT_TRANSMISSION`), it raises `Tranny` for that one call, so the game builds
the whine exactly as it does for the GTR, then puts the flag back. Nothing in
the VLT changes, and AI cars sharing the same engine audio are not affected.

The sound is set up when your car's audio is created, so a newly fitted
Ultimate transmission is heard the next time the car loads.

Earlier versions tried `Csis:Whine`. That is `CARSFX_Shift::PlayGearWhine`,
the reverse-gear whine, not the straight-cut gearbox.

## Compatibility

Reference build: `speed.exe` v1.3, 6,029,312 bytes, MD5
`C0516B485065FABDD69579816B5DF763`.

Every patch checks the bytes it replaces first: each call it redirects must
still call the game's own function, and each vtable slot must still hold the
game's own `InitializeEngine`. Anything that does not match is left alone, so
a different executable is never changed.

## Build

Visual Studio, `Release|Win32`. Output is `Release/NFSMWOverdrive.asi`. The
`Debug|Win32` build is the same plugin with a log.

| | |
| --- | --- |
| `src/dllmain.cpp` | entry point |
| `src/NFSMW/transmission.hpp` | the game classes, with EA's names and layouts |
| `src/NFSMW/transmission.cpp` | the ini, the 6th gear and the straight-cut whine |
