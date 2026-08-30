#pragma once

// ---------------------------------------------------------------------------
//  NFS Most Wanted (2005) PC - the addresses TransmissionTweaks depends on.
//
//  Everything here was read out of speed.exe and GLOBAL/attributes.bin by
//  following the code and data that uses it. Reference build:
//
//      speed.exe   6,029,312 bytes   MD5 C0516B485065FABDD69579816B5DF763
//
//  VerifyBuild() re-checks the bytes at every address before anything is
//  patched, so a different exe fails at load rather than crashing later.
// ---------------------------------------------------------------------------

#include <stddef.h>
#include <stdint.h>

namespace game {

// bStringHash - Jenkins lookup2, initval 0xABCDEF00. MW hashes every VLT class,
// field and collection name with it. Confirmed by round-tripping names against
// constants already in the binary: "transmission" -> 0x07A7A3E5,
// "engine" -> 0xF1F5FBC7, "default" -> 0xEEC2271A.
constexpr uintptr_t kBStringHash = 0x005CC240;

constexpr uint32_t kKeyTransmission = 0x07A7A3E5;  // "transmission"
constexpr uint32_t kKeyDefault      = 0xEEC2271A;  // "default"

// Attrib::Gen::transmission::transmission(collectionKey), __thiscall.
//
//   6A FF             push -1
//   68 58 CB 87 00    push 0x0087CB58        ; SEH frame, no relative operands
//   ...
//   8B 44 24 14       mov  eax, [esp+0x14]   ; the collection key argument
//
// Every transmission collection in the game is built here - player, AI, cops,
// traffic - which makes it the one place to catch a car's gear table.
constexpr uintptr_t kTransmissionCtor        = 0x006A5590;
constexpr uint8_t   kTransmissionCtorBytes[] = { 0x6A, 0xFF, 0x68, 0x58, 0xCB, 0x87, 0x00 };
constexpr size_t    kTransmissionCtorPrologue = sizeof(kTransmissionCtorBytes);

// Attrib::Collection layout, measured at runtime rather than inferred. A
// customised car's collection was dumped against the stock "traffic" one as a
// control, which is what made these unambiguous:
//
//      +0x10  parent collection    customised Cobalt -> "cobaltss"
//                                  stock "traffic"   -> "default"
//      +0x18  GEAR_RATIO storage
//      +0x20  this collection's own key
//
// The storage is an 8-byte header then the ratios:
//
//      [0..1] capacity (u16)   9, the class maximum
//      [2..3] count    (u16)   7 on a 5-speed, 8 on a 6-speed
//      [4..7] element size / max
//      [8..]  count floats
//
// There is no NUM_RATIOS field anywhere in attributes.bin - MW takes the gear
// count straight from this array's length (Attrib::Array::GetLength at
// 0x00452940, read by the physics at 0x00673400). That is the whole reason a
// 6th gear normally waits for the Ultimate package: only <car>_top's array is
// long enough. Capacity is 9 either way, so the extra slot already exists and
// nothing needs reallocating.
constexpr uintptr_t kCollectionParent   = 0x10;
constexpr uintptr_t kCollectionGearData = 0x18;
constexpr uintptr_t kCollectionKey      = 0x20;
constexpr uintptr_t kGearHeaderSize     = 8;

// The transmission wrapper keeps its collection here.
constexpr uintptr_t kWrapperCollection = 0x04;

bool VerifyBuild(char* reasonOut, size_t reasonSize);

uint32_t BStringHash(const char* text);

bool SafeRead(const void* address, void* out, size_t size);
bool IsReadable(const void* address, size_t size);
bool IsWritable(const void* address, size_t size);

} // namespace game
