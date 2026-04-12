// Copyright (c) 2026 Nikita Hmelnitkii. MIT License - see LICENSE.
//
// Fixup.h - relocation record layouts.

#ifndef LD_FIXUP_H
#define LD_FIXUP_H

#include <cstddef>
#include <cstdint>

namespace ld {

// Prime fixup record.
struct Fixup {

    uint32_t offsetInAtom;  // +0x00
    uint32_t targetRef;     // +0x04  binding[31:30] passIndex[29:24] targetIndex[23:0]
    uint32_t kindAddend;    // +0x08  kind[9:0] hasLargeAddend[10] addend[31:11]
    uint32_t extras;        // +0x0C  arm64e auth data or extra addend

    static constexpr uint32_t kKindBits = 10;
    static constexpr uint32_t kKindMask = (1u << kKindBits) - 1;

    uint16_t kind() const { return static_cast<uint16_t>(kindAddend & kKindMask); }
    void setKind(uint16_t k) { kindAddend = (kindAddend & ~kKindMask) | (k & kKindMask); }

    static constexpr uint16_t kKindMax_1115 = 0x149;

    // Kind dispatch ranges (bits [9:7]):
    //   0x000 generic, 0x080 arm64, 0x100 x86_64, 0x180 arm64e v2
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

        // dispatch range 0x180 (1230+)
        kindArm64eAuth2      = 0x182,  // arm64e auth v2
    };

    // 2-bit binding: 0 = direct (targetIndex), !=0 = pass-generated.
    static constexpr uint32_t kBindingShift = 30;
    static constexpr uint32_t kBindingBits  = 2;
    static constexpr uint32_t kBindingMask  = (1u << kBindingBits) - 1;
    uint8_t binding() const { return static_cast<uint8_t>((targetRef >> kBindingShift) & kBindingMask); }

    static constexpr uint32_t kTargetIndexMask = 0x00FFFFFF;
    uint32_t targetIndex() const { return targetRef & kTargetIndexMask; }
    uint8_t  passIndex()   const { return static_cast<uint8_t>((targetRef >> 24) & 0x3F); }

    // Signed 21-bit inline addend, or pool index when bit 10 is set.
    static constexpr uint32_t kAddendShift    = 11;
    static constexpr uint32_t kLargeAddendBit = 10;
    bool hasLargeAddend() const { return (kindAddend >> kLargeAddendBit) & 1; }
    int32_t addend() const {
        static_assert(-1 >> 1 == -1, "arithmetic right shift required");
        return static_cast<int32_t>(kindAddend) >> kAddendShift;
    }

    // Extras: 32-bit extra addend, OR (auth info byte << 24) | (disc << 16).
    int32_t  extraAddend()       const { return static_cast<int32_t>(extras); }
    uint16_t authDiscriminator() const { return static_cast<uint16_t>(extras >> 16); }
    uint8_t  authInfoByte()      const { return static_cast<uint8_t>(extras >> 24); }

    // arm64e auth info byte (ptrauth_key): bits 0..1 = key, bit 2 = addrDiv.
    static constexpr uint8_t kAuthKeyMask  = 0x03;
    static constexpr uint8_t kAuthAddrDiv  = 0x04;
    uint8_t authKey() const { return static_cast<uint8_t>(authInfoByte() & kAuthKeyMask); }
    bool authHasAddrDiversity() const { return (authInfoByte() & kAuthAddrDiv) != 0; }

    // Union of absolute-pointer and got-relative kinds - "occupies a
    // pointer slot in the final image".
    bool occupiesPointerSlot() const {
        uint16_t k = kind();
        if (kindIsAbsolutePointer(k)) return true;
        return k == kindPtr32ToGot || k == kindPtr64ToGot;
    }

    // Coarse pointer-kind filter. Range [2, 0x0E] includes some
    // non-pointer kinds that callers may need to filter further.
    bool isPointerKind() const {
        uint16_t k = kind();
        if (k >= kindPtr64 && k <= 0x0E) return true;
        return k == kindArm64AuthPtr || k == kindArm64eAuth2;
    }

    // Dispatch-range classifiers (kindAddend & 0x380).
    static constexpr bool kindIsGeneric(uint16_t k)  { return (k & 0x380) == 0x000; }
    static constexpr bool kindIsArm64(uint16_t k)    { return (k & 0x380) == 0x080; }
    static constexpr bool kindIsX86_64(uint16_t k)   { return (k & 0x380) == 0x100; }
    static constexpr bool kindIsArm64e_v2(uint16_t k){ return (k & 0x380) == 0x180; }

    static constexpr bool kindIsArm64Branch(uint16_t k) {
        return k == kindArm64Branch26 || k == kindArm64Branch26Add;
    }
    static constexpr bool kindIsArm64AdrpAny(uint16_t k) {
        return k == kindArm64Adrp || k == kindArm64AdrpAdd
            || k == kindArm64AdrpGot || k == kindArm64AdrpTlv
            || k == kindArm64AdrpAddPair;
    }
    static constexpr bool kindIsArm64Lo12Any(uint16_t k) {
        return k == kindArm64Lo12 || k == kindArm64Lo12Add
            || k == kindArm64Lo12Got || k == kindArm64Lo12Tlv;
    }
    static constexpr bool kindIsAuth(uint16_t k) {
        return k == kindArm64AdrpLdrAuth || k == kindArm64AuthPtr
            || k == kindArm64eAuth2;
    }
    static constexpr bool kindIsGot(uint16_t k) {
        return k == kindPtr32ToGot || k == kindPtr64ToGot
            || k == kindPcrel32ToGot || k == kindSwiftRel32ToGot
            || k == kindArm64AdrpGot || k == kindArm64Lo12Got
            || k == kindArm64AdrpLdrGot;
    }
    static constexpr bool kindIsTlv(uint16_t k) {
        return k == kindTlvOffset || k == kindArm64AdrpTlv
            || k == kindArm64Lo12Tlv || k == kindX86TlvLoad;
    }

    static constexpr bool kindIsBranchAny(uint16_t k) {
        return kindIsArm64Branch(k) || k == kindX86Call || k == kindX86Branch8;
    }
    // True iff the fixup writes a pointer-sized absolute value.
    static constexpr bool kindIsAbsolutePointer(uint16_t k) {
        return k == kindPtr64 || k == kindPtr32
            || k == kindArm64AuthPtr || k == kindArm64eAuth2;
    }
    static constexpr bool kindIsDelta(uint16_t k) {
        return k == kindDiff32 || k == kindDiff64;
    }
    static constexpr bool kindIsImageOffset(uint16_t k) {
        return k == kindImageOffset32;
    }
};

inline const char *primeFixupKindName(uint16_t k) {
    switch (k) {
    case Fixup::kindNone:             return "none";
    case Fixup::kindKeepAlive:        return "keep_alive";
    case Fixup::kindPtr64:            return "ptr64";
    case Fixup::kindPtr32:            return "ptr32";
    case Fixup::kindDiff32:           return "diff32";
    case Fixup::kindDiff64:           return "diff64";
    case Fixup::kindImageOffset32:    return "image_off32";
    case Fixup::kindPcrel32ToGot:     return "pcrel32_got";
    case Fixup::kindPcrelDelta32:     return "pcrel_delta32";
    case Fixup::kindSwiftRel32ToGot:  return "swift_rel32_got";
    case Fixup::kindDylibExportClient:return "dylib_export_client";
    case Fixup::kindAlias:            return "alias";
    case Fixup::kindTlvOffset:        return "tlv_off";
    case Fixup::kindPtr32ToGot:       return "ptr32_got";
    case Fixup::kindPtr64ToGot:       return "ptr64_got";
    case Fixup::kindDylibIndex:       return "dylib_index";
    case Fixup::kindArm64Branch26:    return "arm64_b26";
    case Fixup::kindArm64Branch26Add: return "arm64_b26_add";
    case Fixup::kindArm64Adrp:        return "arm64_adrp";
    case Fixup::kindArm64AdrpAdd:     return "arm64_adrp_add";
    case Fixup::kindArm64Lo12:        return "arm64_lo12";
    case Fixup::kindArm64Lo12Add:     return "arm64_lo12_add";
    case Fixup::kindArm64AdrpGot:     return "arm64_adrp_got";
    case Fixup::kindArm64Lo12Got:     return "arm64_lo12_got";
    case Fixup::kindArm64AdrpLdr:     return "arm64_adrp_ldr";
    case Fixup::kindArm64AdrpLdrAuth: return "arm64_adrp_ldr_auth";
    case Fixup::kindArm64AdrpLdrGot:  return "arm64_adrp_ldr_got";
    case Fixup::kindArm64AdrpTlv:     return "arm64_adrp_tlv";
    case Fixup::kindArm64Lo12Tlv:     return "arm64_lo12_tlv";
    case Fixup::kindArm64AuthPtr:     return "arm64_auth_ptr";
    case Fixup::kindArm64AdrpAddPair: return "arm64_adrp_add_pair";
    case Fixup::kindX86Call:          return "x86_call";
    case Fixup::kindX86RipRel:        return "x86_rip";
    case Fixup::kindX86GotUse:        return "x86_got_use";
    case Fixup::kindX86RipRel1:       return "x86_rip1";
    case Fixup::kindX86RipRel2:       return "x86_rip2";
    case Fixup::kindX86RipRel4:       return "x86_rip4";
    case Fixup::kindX86GotLoad:       return "x86_got_load";
    case Fixup::kindX86TlvLoad:       return "x86_tlv_load";
    case Fixup::kindX86Branch8:       return "x86_b8";
    case Fixup::kindArm64eAuth2:      return "arm64e_auth2";
    default: return "?";
    }
}

// 32-byte record written into LC_DYLD_CHAINED_FIXUPS.
namespace mach_o {

struct EmittedFixup {
    uint64_t sectionFixupOffset;  // +0x00
    uint64_t segmentVMAddr;       // +0x08
    uint32_t isBind;              // +0x10 (u32 for alignment)
    uint32_t authDiscriminator;   // +0x14
    uint64_t target;              // +0x18
};

static_assert(sizeof(EmittedFixup) == 32, "EmittedFixup must be 32 bytes");
static_assert(offsetof(EmittedFixup, sectionFixupOffset) == 0x00, "");
static_assert(offsetof(EmittedFixup, segmentVMAddr)      == 0x08, "");
static_assert(offsetof(EmittedFixup, isBind)             == 0x10, "");
static_assert(offsetof(EmittedFixup, authDiscriminator)  == 0x14, "");
static_assert(offsetof(EmittedFixup, target)             == 0x18, "");

} // namespace mach_o

static_assert(sizeof(Fixup) == 16, "Fixup must be exactly 16 bytes");
static_assert(offsetof(Fixup, offsetInAtom) == 0x00, "");
static_assert(offsetof(Fixup, targetRef)    == 0x04, "");
static_assert(offsetof(Fixup, kindAddend)   == 0x08, "");
static_assert(offsetof(Fixup, extras)       == 0x0C, "");

// Classic fixup record.
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
        kindNone                              = 0,
        kindNoneFollowOn                      = 1,
        kindNoneGroupSubordinate              = 2,
        kindNoneGroupSubordinateFDE           = 3,
        kindNoneGroupSubordinateLSDA          = 4,
        kindNoneGroupSubordinatePersonality   = 5,
        kindSetTargetAddress                  = 6,
        kindSubtractTargetAddress             = 7,
        kindAddAddend                         = 8,
        kindSubtractAddend                    = 9,
        kindSetTargetImageOffset              = 10,
        kindSetTargetSectionOffset            = 11,
        kindSetTargetTLVTemplateOffset        = 12,
        kindStore8                            = 13,
        kindStoreLittleEndian16               = 14,
        kindStoreLittleEndianLow24of32        = 15,
        kindStoreLittleEndian32               = 16,
        kindStoreLittleEndian64               = 17,
        kindStoreBigEndian16                  = 18,
        kindStoreBigEndianLow24of32           = 19,
        kindStoreBigEndian32                  = 20,
        kindStoreBigEndian64                  = 21,
        kindStoreX86BranchPCRel8              = 22,
        kindStoreX86BranchPCRel32             = 23,
        kindStoreX86PCRel8                    = 24,
        kindStoreX86PCRel16                   = 25,
        kindStoreX86PCRel32                   = 26,
        kindStoreX86PCRel32_1                 = 27,
        kindStoreX86PCRel32_2                 = 28,
        kindStoreX86PCRel32_4                 = 29,
        kindStoreX86PCRel32GOTLoad            = 30,
        kindStoreX86PCRel32GOTLoadNowLEA      = 31,
        kindStoreX86PCRel32GOT                = 32,
        kindStoreX86PCRel32TLVLoad            = 33,
        kindStoreX86PCRel32TLVLoadNowLEA      = 34,
        kindStoreX86Abs32TLVLoad              = 35,
        kindStoreX86Abs32TLVLoadNowLEA        = 36,
        kindStoreARMBranch24                  = 37,
        kindStoreThumbBranch22                = 38,
        kindStoreARMLoad12                    = 39,
        kindStoreARMLow16                     = 40,
        kindStoreARMHigh16                    = 41,
        kindStoreThumbLow16                   = 42,
        kindStoreThumbHigh16                  = 43,
        kindStoreARM64Branch26                = 44,
        kindStoreARM64Page21                  = 45,
        kindStoreARM64PageOff12               = 46,
        kindStoreARM64GOTLoadPage21           = 47,
        kindStoreARM64GOTLoadPageOff12        = 48,
        kindStoreARM64GOTLeaPage21            = 49,
        kindStoreARM64GOTLeaPageOff12         = 50,
        kindStoreARM64TLVPLoadPage21          = 51,
        kindStoreARM64TLVPLoadPageOff12       = 52,
        kindStoreARM64TLVPLoadNowLeaPage21    = 53,
        kindStoreARM64TLVPLoadNowLeaPageOff12 = 54,
        kindStoreARM64PointerToGOT            = 55,
        kindStoreARM64PCRelToGOT              = 56,
    };

    static constexpr bool kindIsAbsoluteStore(uint8_t k) {
        return k == kindStoreLittleEndian16
            || k == kindStoreLittleEndian32
            || k == kindStoreLittleEndian64
            || k == kindStoreBigEndian16
            || k == kindStoreBigEndian32
            || k == kindStoreBigEndian64
            || k == kindStoreARM64PointerToGOT;
    }

    static constexpr bool kindIsX86PCRel(uint8_t k) {
        return k >= kindStoreX86BranchPCRel8 && k <= kindStoreX86Abs32TLVLoadNowLEA;
    }

    static constexpr bool kindIsARM64(uint8_t k) {
        return k >= kindStoreARM64Branch26 && k <= kindStoreARM64PCRelToGOT;
    }
};

static_assert(sizeof(Fixup) == 16, "");
static_assert(offsetof(Fixup, u)             == 0x00, "");
static_assert(offsetof(Fixup, offsetInAtom)  == 0x08, "");
static_assert(offsetof(Fixup, flags)         == 0x0C, "");

} // namespace classic
} // namespace ld

#endif
