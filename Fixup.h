// Copyright (c) 2026 Nikita Hmelnitkii. MIT License - see LICENSE.
//
// Fixup.h - fixup/relocation descriptor. 16 bytes for both ld-prime and ld64 classic.

#ifndef LD_FIXUP_H
#define LD_FIXUP_H

#include <cstddef>
#include <cstdint>

namespace ld {

// ld-prime fixup — overlaid on in-memory fixup arrays.
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

    // max kind value: 0x149 in ld-1115 (329 total). kindHasAddendInExtras
    // and kindHasArm64Extras use jump tables up to this value.
    // Later versions (ld-1230+) add kinds beyond 0x149 (e.g. 0x182).
    static constexpr uint16_t kKindMax_1115 = 0x149;

    // applyFixup dispatches on bits [9:7] (kindAddend & 0x380):
    //   0x000 → generic  (0x000..0x041)
    //   0x080 → arm64    (0x080..0x0D1)
    //   0x100 → x86_64   (0x100..0x149)

    enum Kind : uint16_t {
        // --- generic kinds (0x000..0x041) ---
        kindNone             = 0x000,  // applyFixup errors on this in output
        kindKeepAlive        = 0x001,  // no-op, skipped by buildChainedFixups
        kindPtr64            = 0x002,  // 64-bit absolute pointer
        kindPtr32            = 0x003,  // 32-bit absolute pointer
        kindDiff32           = 0x004,  // 32-bit signed difference
        kindDiff64           = 0x005,  // 64-bit signed difference
        kindImageOffset32    = 0x006,  // 32-bit offset from image base
        kindPcrel32ToGot     = 0x007,  // pcrel32 to GOT slot
        kindPcrelDelta32     = 0x008,  // 32-bit pcrel delta
        kindSwiftRel32ToGot  = 0x009,  // Swift pcrel32 to GOT
        kindDylibExportClient= 0x00A,  // dylib export client reference
        kindAlias            = 0x00B,  // alias fixup
        kindTlvOffset        = 0x00C,  // TLV slot offset
        kindPtr32ToGot       = 0x00D,  // 32-bit ptr to GOT entry
        kindPtr64ToGot       = 0x00E,  // 64-bit ptr to GOT entry
        kindDylibIndex       = 0x041,  // dylib index

        // --- arm64 kinds (0x080..0x0D1) ---
        kindArm64Branch26    = 0x080,  // B/BL instruction
        kindArm64Branch26Add = 0x081,  // B/BL with explicit addend
        kindArm64Adrp        = 0x082,  // ADRP instruction
        kindArm64AdrpAdd     = 0x083,  // ADRP with explicit addend
        kindArm64Lo12        = 0x084,  // LDR/STR/ADD lo12 offset
        kindArm64Lo12Add     = 0x085,  // lo12 with addend
        kindArm64AdrpGot     = 0x086,  // ADRP to GOT page
        kindArm64Lo12Got     = 0x087,  // LDR from GOT slot offset
        kindArm64AdrpLdr     = 0x088,  // combined ADRP+lo12 pair
        kindArm64AdrpLdrAuth = 0x089,  // arm64e auth ADRP/lo12 pair
        kindArm64AdrpLdrGot  = 0x08A,  // combined GOT ADRP+LDR pair
        kindArm64AdrpTlv     = 0x08B,  // ADRP to TLV page
        kindArm64Lo12Tlv     = 0x08C,  // LDR from TLV slot
        kindArm64AuthPtr     = 0x08D,  // arm64e authenticated pointer
        kindArm64AdrpAddPair = 0x08E,  // ADRP/ADD pair (non-GOT/TLV)
        // 0x0C0..0x0D1: optimizer-transformed arm64 GOT/TLV kinds

        // --- x86_64 kinds (0x100..0x149) ---
        kindX86Call          = 0x101,  // CALL instruction (BRANCH32)
        kindX86RipRel        = 0x102,  // RIP-relative MOV/LEA (SIGNED)
        kindX86GotUse        = 0x103,  // LEA from GOT
        kindX86RipRel1       = 0x104,  // RIP-rel with -1 displacement
        kindX86RipRel2       = 0x105,  // RIP-rel with -2 displacement
        kindX86RipRel4       = 0x106,  // RIP-rel with -4 displacement
        kindX86GotLoad       = 0x107,  // movq from GOT
        kindX86TlvLoad       = 0x108,  // thread-local load
        kindX86Branch8       = 0x109,  // short 8-bit branch

        // --- ld-1230+ only (dispatch range 0x180, outside ld-1115 tables) ---
        kindArm64eAuth2      = 0x182,  // arm64e auth v2
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
        // arithmetic right shift — implementation-defined but guaranteed on Apple targets.
        static_assert(-1 >> 1 == -1, "arithmetic right shift required");
        return static_cast<int32_t>(kindAddend) >> kAddendShift;
    }

    // extras - extra addend or arm64e auth data

    int32_t  extraAddend()       const { return static_cast<int32_t>(extras); }
    uint16_t authDiscriminator() const { return static_cast<uint16_t>(extras >> 16); }
    uint8_t  authInfoByte()      const { return static_cast<uint8_t>(extras >> 24); }

    // pointer-kind classification — coarse filter matching buildChainedFixups.
    // range [2, 0x0E] includes some non-pointer kinds (0x0A-0x0C) that are
    // filtered downstream. matches the binary: sub w9, w8, #2; cmp w9, #0xC
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

// ld64 classic fixup — different layout: 8-bit kind, 3-bit binding, 8-byte union.

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
