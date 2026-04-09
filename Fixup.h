// Copyright (c) 2026 Nikita Hmelnitkii. MIT License - see LICENSE.
//
// Fixup.h - linker fixup/relocation descriptor
//
// ld-prime (ld-1115.7.3, ld-1230.1): 16 bytes, layout stable across versions.
// ld64 classic (ld64-820.1): 16 bytes, completely different field arrangement.
//
// All offsets verified against arm64 disassembly - see tests/ for validation.

#ifndef LD_FIXUP_H
#define LD_FIXUP_H

#include <cstddef>
#include <cstdint>

namespace ld {

// ld-prime fixup - overlaid directly on the linker's in-memory fixup arrays.
struct Fixup {

    uint32_t offsetInAtom;  // +0x00
    uint32_t targetRef;     // +0x04: binding[31:30] | passIndex[29:24] | targetIndex[23:0]
    uint32_t kindAddend;    // +0x08: kind[9:0] | hasLargeAddend[10] | addend[31:11]
    uint32_t extras;        // +0x0C: arm64e auth data or extra addend

    // kind - 10-bit fixup type

    static constexpr uint32_t kKindBits = 10;
    static constexpr uint32_t kKindMask = (1u << kKindBits) - 1;

    uint16_t kind() const { return static_cast<uint16_t>(kindAddend & kKindMask); }

    void setKind(uint16_t k) {
        kindAddend = (kindAddend & ~kKindMask) | (k & kKindMask);
    }

    enum Kind : uint16_t {
        kindNone          = 0x000,  // applyFixup errors on this in final output
        kindKeepAlive     = 0x001,  // applyFixup no-op, skipped by buildChainedFixups
        kindPtr64         = 0x002,
        kindPtr32         = 0x003,
        kindDiff32        = 0x004,
        kindDiff64        = 0x005,
        kindArm64AuthPtr  = 0x08D,  // arm64e authenticated pointer
        kindArm64eAuth2   = 0x182,  // ld-1230.1+
    };

    // binding - 2-bit target resolution type

    static constexpr uint32_t kBindingShift = 30;
    static constexpr uint32_t kBindingBits  = 2;
    static constexpr uint32_t kBindingMask  = (1u << kBindingBits) - 1;

    // 0 = direct atom ref (targetIndex into source file's atoms).
    // non-zero = pass-generated atom (passIndex selects which pass).
    uint8_t binding() const {
        return static_cast<uint8_t>((targetRef >> kBindingShift) & kBindingMask);
    }

    // target index - bits [23:0], valid when binding == 0

    static constexpr uint32_t kTargetIndexMask = 0x00FFFFFF;

    uint32_t targetIndex() const { return targetRef & kTargetIndexMask; }

    // pass index - bits [29:24], valid when binding != 0

    uint8_t passIndex() const {
        return static_cast<uint8_t>((targetRef >> 24) & 0x3F);
    }

    // addend - signed 21-bit, or large-addend table index when bit 10 is set

    static constexpr uint32_t kAddendShift    = 11;
    static constexpr uint32_t kLargeAddendBit = 10;

    bool hasLargeAddend() const { return (kindAddend >> kLargeAddendBit) & 1; }

    int32_t addend() const {
        // arithmetic right shift - implementation-defined in C++ but
        // guaranteed on all Apple targets. matches sbfx in the linker.
        return static_cast<int32_t>(kindAddend) >> kAddendShift;
    }

    // extras - extra addend or arm64e auth data

    int32_t  extraAddend()       const { return static_cast<int32_t>(extras); }
    uint16_t authDiscriminator() const { return static_cast<uint16_t>(extras >> 16); }
    uint8_t  authInfoByte()      const { return static_cast<uint8_t>(extras >> 24); }

    // pointer-kind classification - true for kinds that emit chained fixup entries.
    // range [2, 0x0E] derived from: sub w9, w8, #2; cmp w9, #0xC; b.hi skip
    bool isPointerKind() const {
        uint16_t k = kind();
        if (k >= kindPtr64 && k <= 0x0E) return true;
        return k == kindArm64AuthPtr || k == kindArm64eAuth2;
    }
};

static_assert(sizeof(Fixup) == 16, "Fixup must be exactly 16 bytes");
static_assert(offsetof(Fixup, offsetInAtom) == 0x00, "");
static_assert(offsetof(Fixup, targetRef)    == 0x04, "");
static_assert(offsetof(Fixup, kindAddend)   == 0x08, "");
static_assert(offsetof(Fixup, extras)       == 0x0C, "");

// ld64 classic fixup - completely different layout from ld-prime.
// 8-bit kind, 3-bit binding, 8-byte union at +0x00.

namespace classic {

struct Fixup {

    union {
        const void *target;
        const char *name;
        uint64_t    addend;
        uint32_t    bindingIndex;
    } u;                        // +0x00

    uint32_t offsetInAtom;      // +0x08
    uint32_t flags;             // +0x0C: kind[7:0] | cluster[11:8] | weakImport[12] | binding[15:13]

    static constexpr uint32_t kKindMask      = 0xFF;
    static constexpr uint32_t kClusterShift  = 8;
    static constexpr uint32_t kClusterMask   = 0xF;
    static constexpr uint32_t kWeakImportBit = 12;
    static constexpr uint32_t kBindingShift  = 13;
    static constexpr uint32_t kBindingMask   = 0x7;

    uint8_t kind()       const { return static_cast<uint8_t>(flags & kKindMask); }
    uint8_t clusterSize()const { return static_cast<uint8_t>((flags >> kClusterShift) & kClusterMask); }
    bool    weakImport() const { return (flags >> kWeakImportBit) & 1; }
    uint8_t binding()    const { return static_cast<uint8_t>((flags >> kBindingShift) & kBindingMask); }

    void setKind(uint8_t k) { flags = (flags & ~kKindMask) | k; }

    enum Binding : uint8_t {
        bindingNone             = 0,
        bindingByNameUnbound    = 1,
        bindingDirectlyBound    = 2,
        bindingByContentBound   = 3,
        bindingsIndirectlyBound = 4,
    };

    enum Kind : uint8_t {
        kindNone = 0,
        // ~40 more values exist in ld64 - add as needed.
    };
};

static_assert(sizeof(Fixup) == 16, "");
static_assert(offsetof(Fixup, u)             == 0x00, "");
static_assert(offsetof(Fixup, offsetInAtom)  == 0x08, "");
static_assert(offsetof(Fixup, flags)         == 0x0C, "");

} // namespace classic
} // namespace ld

#endif
