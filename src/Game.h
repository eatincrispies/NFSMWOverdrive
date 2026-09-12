#pragma once

#include <stddef.h>
#include <stdint.h>

namespace game {
constexpr uintptr_t kBStringHash = 0x005CC240;

constexpr uint32_t kKeyTransmission = 0x07A7A3E5;
constexpr uint32_t kKeyDefault      = 0xEEC2271A;

constexpr uintptr_t kTransmissionCtor        = 0x006A5590;
constexpr uint8_t   kTransmissionCtorBytes[] = { 0x6A, 0xFF, 0x68, 0x58, 0xCB, 0x87, 0x00 };
constexpr size_t    kTransmissionCtorPrologue = sizeof(kTransmissionCtorBytes);

constexpr uintptr_t kCollectionParent   = 0x10;
constexpr uintptr_t kCollectionGearData = 0x18;
constexpr uintptr_t kCollectionKey      = 0x20;
constexpr uintptr_t kGearHeaderSize     = 8;

constexpr uintptr_t kWrapperCollection = 0x04;

bool VerifyBuild(char* reasonOut, size_t reasonSize);

uint32_t BStringHash(const char* text);

bool SafeRead(const void* address, void* out, size_t size);
bool IsReadable(const void* address, size_t size);
bool IsWritable(const void* address, size_t size);
}
