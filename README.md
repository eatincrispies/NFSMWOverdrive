# NFSMW Overdrive

An ASI plugin for *Need for Speed: Most Wanted* (2005, PC).

**Five-speed cars get their sixth gear from the first transmission package
instead of the last.** Fit a Race gearbox to a Cobalt SS and it pulls sixth,
rather than making you wait for the Ultimate package.

Cars with a stock gearbox are untouched, and upgrading anything else — engine,
nitrous, tyres — does not trigger it.

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
| `[Debug] Log` | `0` | writes `NFSMWOverdrive.log` beside the `.asi` |
| `[Debug] Verbose` | `0` | logs every transmission collection built |

## Affected cars

The 31 with an upgraded gear set in the game's data:

`911turbo` `997s` `a3` `a4` `carreragt` `caymans` `clio` `clk500` `cobaltss`
`corvette` `cts` `db9` `eclipsegt` `elise` `fordgt` `gallardo` `gti` `gto`
`is300` `lancerevo8` `monaro` `murcielago` `mustanggt` `punto` `rx7` `rx8`
`sl500` `slr` `supra` `tt` `viper`

Cars absent from that list — the M3 GTR, SL65, Camaro, 911 GT2, and every cop,
traffic and semi entry — ship fully specced with no upgraded set to draw from.
Cars that already have six gears are left alone.

## How it works

Every car ships two attribute sets: `<car>` and `<car>_top`, the fully-upgraded
one. On a 5-speed car the stock `GEAR_RATIO` array holds seven entries (reverse,
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

## Compatibility

Reference build: `speed.exe`, 6,029,312 bytes, MD5
`C0516B485065FABDD69579816B5DF763`.

The plugin verifies the bytes at every address it touches before patching
anything, and disables itself with a message box on a mismatch — a different
executable cannot be corrupted by it.

## Build

Visual Studio, `Release|Win32`. Output is `Release/NFSMWOverdrive.asi`.

| | |
| --- | --- |
| `src/Game.h` | addresses and structure offsets, with the disassembly they came from |
| `src/Tweaks.cpp` | the hook and the gear patch |
| `src/Hook.cpp` | 5-byte JMP detour with trampoline and byte check |
| `src/Config.cpp` | ini parsing |
| `src/Log.cpp` | optional logging |

### A note on the straight-cut whine

Earlier versions carried an unfinished attempt at giving any car the M3 GTR's
gear whine. It was removed because it never worked.

For anyone tempted to try: `sub_4EC2F0` is **not** the whine. It passes the
string `"Csis:Whine"` to a logging call, which is misleading — instrumenting it
shows it never executes at all, including in a GTR that is audibly whining.
The byte at `*(this+72)+49` is not a gate for it either; the GTR whines with
that byte reading `0`.

The likeliest explanation, untested: `CAR_TRANNY.abk` appears exactly once in
the whole VLT, in the global bank manifest immediately after `CAR_99_ENG_MB_EE.abk`
— the GTR's engine bank — and has no per-car reference anywhere. That suggests
the whine is part of the GTR's engine audio package rather than a separately
gated layer, which would make it a data problem, not an ASI one. A five-minute
VltEd test — point another car's engine audio at the GTR's and listen — would
confirm or kill that before anyone writes code.
