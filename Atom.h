// Copyright (c) 2026 Nikita Hmelnitkii. MIT License - see LICENSE.
//
// Atom.h - ld::Atom class layout and BIND/REBASE classification
//
// The Atom is the fundamental unit in Apple's linker - every symbol, literal,
// stub, etc. is an Atom. Three concrete subclasses in ld-prime:
//   Atom_1 (0x30 bytes) - deserialized from .o files
//   Atom_2              - variant of Atom_1
//   DynamicAtom (0x58)  - linker-generated atoms
//
// Vtable slots 0-11 (+0x00 through +0x58) are stable across both known
// ld-prime versions (ld-1115.7.3, ld-1230.1). Divergence starts at slot 15.

#ifndef LD_ATOM_H
#define LD_ATOM_H

#include "Fixup.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ld {

// memory access primitives - all struct access goes through these.

inline uint8_t readU8(const void *base, size_t off) {
    return static_cast<const uint8_t *>(base)[off];
}

inline uint32_t readU32(const void *base, size_t off) {
    uint32_t v;
    __builtin_memcpy(&v, static_cast<const uint8_t *>(base) + off, 4);
    return v;
}

inline uint64_t readU64(const void *base, size_t off) {
    uint64_t v;
    __builtin_memcpy(&v, static_cast<const uint8_t *>(base) + off, 8);
    return v;
}

inline const void *readPtr(const void *base, size_t off) {
    const void *v;
    __builtin_memcpy(&v, static_cast<const uint8_t *>(base) + off, sizeof(v));
    return v;
}

// atom kind - the byte at Atom+0x08 that determines BIND vs REBASE.
// derived from makeBindTarget's bitmask 0x103F: bits set at positions
// 0-5 and 12 are regular (REBASE). everything else <= 18 is BIND.

namespace AtomKind {
    inline constexpr uint8_t kRegular0     = 0x00;
    inline constexpr uint8_t kRegular1     = 0x01;
    inline constexpr uint8_t kRegular2     = 0x02;
    inline constexpr uint8_t kRegular3     = 0x03;
    inline constexpr uint8_t kRegular4     = 0x04;
    inline constexpr uint8_t kRegular5     = 0x05;
    inline constexpr uint8_t kExternal     = 0x06;
    inline constexpr uint8_t kDylibExport0 = 0x07;  // BIND with dylib ordinal
    inline constexpr uint8_t kDylibExport1 = 0x08;
    inline constexpr uint8_t kDylibExport2 = 0x09;
    inline constexpr uint8_t kExternal2    = 0x0A;
    inline constexpr uint8_t kDylibWeak    = 0x0B;  // BIND with weak flag
    inline constexpr uint8_t kAlias        = 0x0C;  // follows alias chain (REBASE)
    inline constexpr uint8_t kTentative0   = 0x0D;
    inline constexpr uint8_t kTentative1   = 0x0E;
    inline constexpr uint8_t kTentative2   = 0x0F;
    inline constexpr uint8_t kTentative3   = 0x10;
    inline constexpr uint8_t kProxy        = 0x12;  // dylib import - definitive BIND
    inline constexpr uint8_t kStub         = 0x13;  // stub/thunk

    constexpr bool isBind(uint8_t kind) {
        if (kind > 18) return false;
        constexpr uint32_t regularMask = 0x103F; // bits 0-5, 12
        return !((regularMask >> kind) & 1);
    }

    constexpr bool isRebase(uint8_t kind) {
        if (kind > 18) return true;
        constexpr uint32_t regularMask = 0x103F;
        return (regularMask >> kind) & 1;
    }

    constexpr bool isProxy(uint8_t kind) { return kind == kProxy; }

    constexpr bool isDylibExport(uint8_t kind) {
        return kind >= kDylibExport0 && kind <= kDylibExport2;
    }
}

namespace AtomFlags {
    inline constexpr uint8_t kLive      = 0x01;
    inline constexpr uint8_t kAlias     = 0x04;
    inline constexpr uint8_t kActivated = 0x20;
}

// opaque atom pointer and field accessors

using AtomPtr = const void *;

inline uint8_t atomKind(AtomPtr a)  { return readU8(a, 0x08); }
inline uint8_t atomScope(AtomPtr a) { return readU8(a, 0x09); }
inline uint8_t atomFlags(AtomPtr a) { return readU8(a, 0x0D); }

inline bool atomIsProxy(AtomPtr a)  { return AtomKind::isProxy(atomKind(a)); }
inline bool atomIsBind(AtomPtr a)   { return AtomKind::isBind(atomKind(a)); }
inline bool atomIsRebase(AtomPtr a) { return AtomKind::isRebase(atomKind(a)); }

// vtable dispatch - calls into linker code

template <typename R>
inline R atomVCall(AtomPtr a, size_t slot) {
    const void *vtbl = readPtr(a, 0);
    using Fn = R (*)(const void *);
    Fn fn;
    __builtin_memcpy(&fn, static_cast<const uint8_t *>(vtbl) + slot, sizeof(fn));
    return fn(a);
}

inline const char *atomName(AtomPtr a) {
    return atomVCall<const char *>(a, 0x08);
}

// fixups() returns {ptr, count} in registers: ARM64 x0:w1, x86_64 rax:edx.
struct FixupSpan {
    Fixup   *ptr;
    uint32_t count;
};

static_assert(sizeof(FixupSpan) == 16,
    "must be 16 bytes to match register return ABI");

inline FixupSpan atomFixups(AtomPtr a) {
    const void *vtbl = readPtr(a, 0);
    using Fn = FixupSpan (*)(const void *);
    Fn fn;
    __builtin_memcpy(&fn, static_cast<const uint8_t *>(vtbl) + 0x58, sizeof(fn));
    return fn(a);
}

// vtable slot offsets (ld-prime) - slots 0-11 stable across versions

namespace vtable {
    inline constexpr size_t kFile          = 0x00;
    inline constexpr size_t kName          = 0x08;
    inline constexpr size_t kAtomOrdinal   = 0x10;
    inline constexpr size_t kAlignment     = 0x18;
    inline constexpr size_t kSection       = 0x20;
    inline constexpr size_t kSectionCustom = 0x28;
    inline constexpr size_t kIsCustomSect  = 0x30;
    inline constexpr size_t kDontDeadStrip = 0x38;
    inline constexpr size_t kDontDeadStripIfRefsLive = 0x40;
    inline constexpr size_t kCold          = 0x48;
    inline constexpr size_t kRawContent    = 0x50;  // returns {ptr, size}
    inline constexpr size_t kFixups        = 0x58;  // returns {ptr, count}
    inline constexpr size_t kIsDynamicAtom = 0x60;
    inline constexpr size_t kDylibFileInfo = 0x68;
    inline constexpr size_t kHasLinkConstraints = 0x70;
    // slots 15+ diverge between ld versions
}

// atom base class field offsets - stable across ld-prime versions

namespace atom {
    inline constexpr size_t kVtable      = 0x00;
    inline constexpr size_t kKind        = 0x08;
    inline constexpr size_t kScope       = 0x09;
    inline constexpr size_t kContentType = 0x0A;
    inline constexpr size_t kFlags       = 0x0D;
    inline constexpr size_t kPlacement   = 0x10;
    inline constexpr size_t kAliasTarget = 0x18;

    // Atom_1 subclass (0x30 bytes)
    inline constexpr size_t kAtom1_RO        = 0x20;
    inline constexpr size_t kAtom1_DebugInfo = 0x28;
    inline constexpr size_t kAtom1_Size      = 0x30;

    // DynamicAtom subclass (0x58 bytes)
    inline constexpr size_t kDynamic_File       = 0x20;
    inline constexpr size_t kDynamic_Name       = 0x28;
    inline constexpr size_t kDynamic_ContentPtr = 0x30;
    inline constexpr size_t kDynamic_ContentSz  = 0x38;
    inline constexpr size_t kDynamic_FixupStart = 0x3C;
    inline constexpr size_t kDynamic_FixupCount = 0x40;
    inline constexpr size_t kDynamic_Alignment  = 0x44;
    inline constexpr size_t kDynamic_Ordinal    = 0x48;
    inline constexpr size_t kDynamic_CustomSect = 0x4C;
    inline constexpr size_t kDynamic_Flags      = 0x4D;
    inline constexpr size_t kDynamic_DebugInfo  = 0x50;
    inline constexpr size_t kDynamic_Size       = 0x58;
}

// AtomRO_1 - read-only serialized data for Atom_1 (pointed to by Atom_1+0x20)

namespace AtomRO {
    inline constexpr size_t kOrdinal       = 0x00;
    inline constexpr size_t kFixupStart    = 0x04;
    inline constexpr size_t kFixupCount    = 0x08;
    inline constexpr size_t kNameOffset    = 0x0C;  // 0xFFFFFF = unnamed
    inline constexpr size_t kContentSize   = 0x14;
    inline constexpr size_t kContentOffset = 0x18;  // -1 = no content
    inline constexpr size_t kDylibIndex    = 0x1C;  // 0xFF = none
    inline constexpr size_t kAlignLow      = 0x1D;
    inline constexpr size_t kAlignHigh     = 0x1E;
}

// DynamicAtomFile fixup pool offset - differs between ld versions

namespace DynamicAtomFile {
    inline constexpr size_t kFixupPool_1115 = 0x1B8;
    inline constexpr size_t kFixupPool_1230 = 0x1F8;
}

// AtomFile - returned by Atom vtable slot 0 (file()).
// atoms array entries may point to atoms from other files after resolution.

namespace AtomFile {
    inline constexpr size_t kAtomsBegin   = 0x08;
    inline constexpr size_t kAtomsEnd     = 0x10;
    inline constexpr size_t kAtomsCapacity= 0x18;
    inline constexpr size_t kConsolidator = 0x38;
    inline constexpr size_t kPath         = 0x78;
    inline constexpr size_t kPathLen      = 0x80;
}

// AtomFile vtable - slot 0 stable, slots 1+ shifted in ld-1221

namespace AtomFileVtable {
    inline constexpr size_t kPath = 0x00;
}

// DylibFileInfo (0x90 bytes) - Atom vtable slot 13, NULL for DynamicAtom

namespace DylibFileInfo {
    inline constexpr size_t kCompatVersion  = 0x00;
    inline constexpr size_t kCurrentVersion = 0x04;
    inline constexpr size_t kInstallName    = 0x08;
    inline constexpr size_t kInstallNameLen = 0x10;
    inline constexpr size_t kHasWeakDefs    = 0x88;
    inline constexpr size_t kHasTLVars      = 0x89;
    inline constexpr size_t kSize           = 0x90;
}

// atom -> file / dylib accessors

inline const void *atomFile(AtomPtr a) {
    return atomVCall<const void *>(a, vtable::kFile);
}

inline const void *atomDylibFileInfo(AtomPtr a) {
    return atomVCall<const void *>(a, vtable::kDylibFileInfo);
}

inline const char *dylibInstallName(const void *dfi) {
    if (!dfi) return nullptr;
    return static_cast<const char *>(readPtr(dfi, DylibFileInfo::kInstallName));
}

inline const char *filePath(const void *file) {
    if (!file) return nullptr;
    const void *vtbl = readPtr(file, 0);
    using Fn = const char *(*)(const void *);
    Fn fn;
    __builtin_memcpy(&fn, static_cast<const uint8_t *>(vtbl), sizeof(fn));
    return fn(file);
}

// tries install name (Atom_1), falls back to file path (DynamicAtom)
inline const char *atomDylibPath(AtomPtr a) {
    const char *name = dylibInstallName(atomDylibFileInfo(a));
    if (name) return name;
    return filePath(atomFile(a));
}

// ld64 classic vtable - different slot ordering, fixupsBegin/End iterators

namespace classic {
namespace vtable {
    inline constexpr size_t kDestructor  = 0x00;
    inline constexpr size_t kFile        = 0x08;
    inline constexpr size_t kName        = 0x10;
    inline constexpr size_t kObjectAddr  = 0x18;
    inline constexpr size_t kSize        = 0x20;
    inline constexpr size_t kCopyContent = 0x28;
    // fixupsBegin/End are near the middle - not stable across ld64 versions
}
}

} // namespace ld

#endif
