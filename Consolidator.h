// Copyright (c) 2026 Nikita Hmelnitkii. MIT License - see LICENSE.
//
// Consolidator.h - AtomFileConsolidator field offsets and DylibLoadInfo
// record layout. Version-dependent offsets live in LayoutConstants.

#ifndef LD_CONSOLIDATOR_H
#define LD_CONSOLIDATOR_H

#include "Primitives.h"
#include "Options.h"
#include "Version.h"

#include <cstddef>

namespace ld {

using ConsolidatorPtr = const void *;
using MutableConsolidatorPtr = void *;

// Stable top-of-struct fields.
namespace consolidator {
    inline constexpr size_t kOptionsPtr          = 0x000;
    inline constexpr size_t kOptionsPtrDuplicate = 0x080;
}

// Element slot in the input AtomFile pointer vector. Stride 8.
namespace input_atomfile {
    inline constexpr size_t kFilePtr = 0x00;
    inline constexpr size_t kStride  = 0x08;
}

inline const void *inputAtomFilePtr(const void *slot) {
    if (!slot) return nullptr;
    const void *p;
    __builtin_memcpy(&p, slot, sizeof(p));
    return p;
}

// Output dylib record. One per LC_LOAD_DYLIB. Stride 0x50.
namespace DylibLoadInfo {
    inline constexpr size_t kStride     = 0x50;
    inline constexpr size_t kDylibPtr   = 0x00;
    inline constexpr size_t kOptionsPtr = 0x08;
    inline constexpr size_t kAttributes = 0x10;
    inline constexpr size_t kOrdinal    = 0x18;
}

// DylibLoadInfo attribute flags at +0x10.
enum class DylibAttr : uint8_t {
    None           = 0x00,
    Weak           = 0x01,  // emit LC_LOAD_WEAK_DYLIB
    Reexport       = 0x02,  // emit LC_REEXPORT_DYLIB
    Upward         = 0x04,  // emit LC_LOAD_UPWARD_DYLIB
    Implicit       = 0x08,  // auto-linked, not explicit
    DeadStrippable = 0x10,  // eligible for -dead_strip_dylibs
};

inline constexpr uint8_t operator|(DylibAttr a, DylibAttr b) {
    return static_cast<uint8_t>(a) | static_cast<uint8_t>(b);
}
inline constexpr bool hasAttr(uint8_t raw, DylibAttr a) {
    return (raw & static_cast<uint8_t>(a)) != 0;
}

// Currently 0x50 for all known versions.
inline size_t dylibLoadInfoStride(const LinkerVersion &) {
    return DylibLoadInfo::kStride;
}

inline const void *dylibLoadInfoDylib(const void *slot) {
    return slot ? readPtr(slot, DylibLoadInfo::kDylibPtr) : nullptr;
}
inline OptionsPtr dylibLoadInfoOptions(const void *slot) {
    return slot ? readPtr(slot, DylibLoadInfo::kOptionsPtr) : nullptr;
}
inline uint8_t dylibLoadInfoAttributesRaw(const void *slot) {
    return slot ? readU8(slot, DylibLoadInfo::kAttributes) : 0;
}
inline uint32_t dylibLoadInfoOrdinal(const void *slot) {
    return slot ? readU32(slot, DylibLoadInfo::kOrdinal) : 0;
}

inline bool dylibLoadInfoIsWeak(const void *slot)          { return hasAttr(dylibLoadInfoAttributesRaw(slot), DylibAttr::Weak); }
inline bool dylibLoadInfoIsReexport(const void *slot)      { return hasAttr(dylibLoadInfoAttributesRaw(slot), DylibAttr::Reexport); }
inline bool dylibLoadInfoIsUpward(const void *slot)        { return hasAttr(dylibLoadInfoAttributesRaw(slot), DylibAttr::Upward); }
inline bool dylibLoadInfoIsImplicit(const void *slot)      { return hasAttr(dylibLoadInfoAttributesRaw(slot), DylibAttr::Implicit); }
inline bool dylibLoadInfoIsDeadStrippable(const void *slot){ return hasAttr(dylibLoadInfoAttributesRaw(slot), DylibAttr::DeadStrippable); }

// Per-version Consolidator vector offsets.
namespace consolidator_offsets {

    struct Prime_1115 {
        static constexpr size_t kInputAtomFilesBegin = 0x4A8;
        static constexpr size_t kInputAtomFilesEnd   = 0x4B0;
        static constexpr size_t kOutputDylibsBegin   = 0x728;
        static constexpr size_t kOutputDylibsEnd     = 0x730;
        static constexpr size_t kOutputDylibsCap     = 0x738;
    };

    // 1221 / 1230 / 1266 share this layout.
    struct Prime_1221Plus {
        static constexpr size_t kInputAtomFilesBegin = 0x4B8;
        static constexpr size_t kInputAtomFilesEnd   = 0x4C0;
        static constexpr size_t kOutputDylibsBegin   = 0x7F0;
        static constexpr size_t kOutputDylibsEnd     = 0x7F8;
        static constexpr size_t kOutputDylibsCap     = 0x800;
    };

    // 1167.5 -- inputAtomFiles matches 1221 but outputDylibs differs.
    struct Prime_1167 {
        static constexpr size_t kInputAtomFilesBegin = 0x4B8;
        static constexpr size_t kInputAtomFilesEnd   = 0x4C0;
        static constexpr size_t kOutputDylibsBegin   = 0x7B8;
        static constexpr size_t kOutputDylibsEnd     = 0x7C0;
        static constexpr size_t kOutputDylibsCap     = 0x7C8;
    };
}

inline OptionsPtr consolidatorOptionsRaw(ConsolidatorPtr cons) {
    return cons ? readPtr(cons, consolidator::kOptionsPtr) : nullptr;
}

// Duplicate Options* at +0x80; both slots hold the same value.
inline OptionsPtr consolidatorOptionsAltRaw(ConsolidatorPtr cons) {
    return cons ? readPtr(cons, consolidator::kOptionsPtrDuplicate) : nullptr;
}

inline bool consolidatorOptionsPathsAgree(ConsolidatorPtr cons) {
    OptionsPtr a = consolidatorOptionsRaw(cons);
    if (!a) return false;
    return a == consolidatorOptionsAltRaw(cons);
}

} // namespace ld

#endif
