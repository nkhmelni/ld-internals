// Copyright (c) 2026 Nikita Hmelnitkii. MIT License - see LICENSE.
//
// Fixup.h - cross-atom relocation records. Prime and classic variants
// are both 16 bytes.

#ifndef LD_FIXUP_H
#define LD_FIXUP_H

#include <cstddef>
#include <cstdint>

namespace ld {

// ld-prime fixup. 10-bit kind selects an instruction-class dispatch range.
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
    static constexpr uint16_t kKindMax_1266 = 0x18C;

    // Kind dispatch ranges (bits [9:7]):
    //   0x000 generic, 0x080 arm64, 0x0C0 arm64 was_, 0x100 x86_64,
    //   0x140 x86_64 was_, 0x180 arm32/thumb
    enum Kind : uint16_t {
        // Generic kinds (0x000..0x041).
        kindNone             = 0x000,
        kindKeepAlive        = 0x001,  // no-op, skipped by buildChainedFixups
        kindPtr64            = 0x002,
        kindPtr32            = 0x003,
        kindDiff32           = 0x004,
        kindDiff64           = 0x005,
        kindImageOffset32    = 0x006,
        kindPcrel32ToGot     = 0x007,
        kindPcrelDelta32     = 0x008,
        kindSwiftRel32ToGot  = 0x009,
        kindDylibExportClient= 0x00A,
        kindAlias            = 0x00B,
        kindTlvOffset        = 0x00C,
        kindPtr32ToGot       = 0x00D,
        kindPtr64ToGot       = 0x00E,
        kindDylibIndex       = 0x041,

        // arm64 active (0x080..0x08F).
        kindArm64Branch26         = 0x080,
        kindArm64Branch26Add      = 0x081,
        kindArm64Adrp             = 0x082,
        kindArm64AdrpAdd          = 0x083,
        kindArm64Lo12             = 0x084,
        kindArm64Lo12Add          = 0x085,
        kindArm64AdrpGot          = 0x086,
        kindArm64Lo12Got          = 0x087,
        kindArm64AdrpLdr          = 0x088,
        kindArm64AdrpLdrAdd       = 0x089,
        kindArm64AdrpLdrGot       = 0x08A,
        kindArm64AdrpTlv          = 0x08B,
        kindArm64Lo12Tlv          = 0x08C,
        kindArm64AuthPtr          = 0x08D,
        kindArm64AddGot           = 0x08E,
        kindArm64AdrpAddGot       = 0x08F,

        // arm64 optimizer-transformed "was_" (0x0C0..0x0D1).
        kindArm64WasAdrpLdrGotLoadGot = 0x0C0,
        kindArm64WasAdrpLdrGotLeaGot  = 0x0C1,
        kindArm64WasAdrpLdrGotElideGot= 0x0C2,
        kindArm64WasAdrpLdrGotBlHelper= 0x0C3,
        kindArm64WasAdrpGotUseGot     = 0x0C4,
        kindArm64WasAdrpGotElideGot   = 0x0C5,
        kindArm64WasAdrpGotBlHelper   = 0x0C6,
        kindArm64WasLo12GotLoadGot    = 0x0C7,
        kindArm64WasLo12GotLeaGot     = 0x0C8,
        kindArm64WasLo12GotElideGot   = 0x0C9,
        kindArm64WasB26DtraceProbe    = 0x0CA,
        kindArm64WasB26DtraceEnable   = 0x0CB,
        kindArm64WasAdrpTlvLoadGot    = 0x0CC,
        kindArm64WasAdrpTlvElideGot   = 0x0CD,
        kindArm64WasLo12TlvLoadGot    = 0x0CE,
        kindArm64WasLo12TlvElideGot   = 0x0CF,
        kindArm64WasAdrpAddGotLeaGot  = 0x0D0,
        kindArm64WasAdrpAddGotBlHelper= 0x0D1,

        // x86_64 active (0x100..0x109).
        kindX86Call          = 0x100,  // CALL (BRANCH32)
        kindX86RipRel        = 0x101,
        kindX86RipGot        = 0x102,
        kindX86RipRel1       = 0x103,
        kindX86RipRel2       = 0x104,
        kindX86RipRel4       = 0x105,
        kindX86RipGotLoad    = 0x106,
        kindX86RipTlvLoad    = 0x107,
        kindX86Branch8       = 0x108,
        kindX86RipRel1Got    = 0x109,

        // x86_64 optimizer-transformed "was_" (0x140..0x149; 0x141 reserved).
        kindX86WasRipGotLoadLoadGot   = 0x140,
        kindX86WasRipGotLoadElideGot  = 0x142,
        kindX86WasRipGotLoadCallHelper= 0x143,
        kindX86WasRipGotLeaCallHelper = 0x144,
        kindX86WasRipGotCmpCallHelper = 0x145,
        kindX86WasCallDtraceProbe     = 0x146,
        kindX86WasCallDtraceEnable    = 0x147,
        kindX86WasRipTlvLoadGot       = 0x148,
        kindX86WasRipTlvElideGot      = 0x149,

        // ARM32 / Thumb (0x180..0x18C).
        kindArm32Branch24  = 0x180,
        kindThumbBranch22  = 0x181,
        kindArm32Ptr       = 0x182,
        kindArm32PtrDiff   = 0x183,
        kindArm32Pcrel     = 0x184,
        kindArm32Hi16Pc    = 0x185,
        kindArm32Lo16Pc    = 0x186,
        kindThumbHi16Pc    = 0x187,
        kindThumbLo16Pc    = 0x188,
        kindArm32Hi16      = 0x189,
        kindArm32Lo16      = 0x18A,
        kindThumbHi16      = 0x18B,
        kindThumbLo16      = 0x18C,
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

    // Absolute pointers plus their GOT-indirect variants.
    bool occupiesPointerSlot() const {
        uint16_t k = kind();
        if (kindIsAbsolutePointer(k)) return true;
        return k == kindPtr32ToGot || k == kindPtr64ToGot;
    }

    // Coarse range filter; refine with kindIsAbsolutePointer/kindIsGot.
    bool isPointerKind() const {
        uint16_t k = kind();
        if (k >= kindPtr64 && k <= 0x0E) return true;
        return k == kindArm64AuthPtr;
    }

    // Dispatch range selected by bits [9:7] of the kind.
    static constexpr bool kindIsGeneric(uint16_t k)   { return (k & 0x380) == 0x000; }
    static constexpr bool kindIsArm64(uint16_t k)     { return (k & 0x380) == 0x080; }
    static constexpr bool kindIsX86_64(uint16_t k)    { return (k & 0x380) == 0x100; }
    static constexpr bool kindIsArm32(uint16_t k)     { return (k & 0x380) == 0x180; }

    // "was_" kinds: optimizer-transformed GOT/TLV/dtrace relaxations.
    static constexpr bool kindIsArm64Was(uint16_t k)  { return k >= 0x0C0 && k <= 0x0D1; }
    static constexpr bool kindIsX86Was(uint16_t k)    { return k >= 0x140 && k <= 0x149 && k != 0x141; }
    static constexpr bool kindIsOptimizerWas(uint16_t k) { return kindIsArm64Was(k) || kindIsX86Was(k); }

    static constexpr bool kindIsThumb(uint16_t k) {
        return k == kindThumbBranch22 || k == kindThumbHi16Pc || k == kindThumbLo16Pc
            || k == kindThumbHi16 || k == kindThumbLo16;
    }

    static constexpr bool kindIsArm64Branch(uint16_t k) {
        return k == kindArm64Branch26 || k == kindArm64Branch26Add;
    }
    static constexpr bool kindIsArm64AdrpAny(uint16_t k) {
        return k == kindArm64Adrp || k == kindArm64AdrpAdd
            || k == kindArm64AdrpGot || k == kindArm64AdrpTlv
            || k == kindArm64AdrpLdr || k == kindArm64AdrpLdrAdd
            || k == kindArm64AdrpLdrGot || k == kindArm64AdrpAddGot;
    }
    static constexpr bool kindIsArm64Lo12Any(uint16_t k) {
        return k == kindArm64Lo12 || k == kindArm64Lo12Add
            || k == kindArm64Lo12Got || k == kindArm64Lo12Tlv;
    }
    static constexpr bool kindIsAuth(uint16_t k) {
        return k == kindArm64AuthPtr;
    }
    static constexpr bool kindIsGot(uint16_t k) {
        return k == kindPtr32ToGot || k == kindPtr64ToGot
            || k == kindPcrel32ToGot || k == kindSwiftRel32ToGot
            || k == kindArm64AdrpGot || k == kindArm64Lo12Got
            || k == kindArm64AdrpLdrGot || k == kindArm64AddGot
            || k == kindArm64AdrpAddGot
            || k == kindX86RipGot || k == kindX86RipGotLoad
            || k == kindX86RipRel1Got;
    }
    static constexpr bool kindIsTlv(uint16_t k) {
        return k == kindTlvOffset || k == kindArm64AdrpTlv
            || k == kindArm64Lo12Tlv || k == kindX86RipTlvLoad;
    }
    static constexpr bool kindIsDtrace(uint16_t k) {
        return k == kindArm64WasB26DtraceProbe || k == kindArm64WasB26DtraceEnable
            || k == kindX86WasCallDtraceProbe  || k == kindX86WasCallDtraceEnable;
    }

    static constexpr bool kindIsBranchAny(uint16_t k) {
        return kindIsArm64Branch(k) || k == kindX86Call || k == kindX86Branch8
            || k == kindArm32Branch24 || k == kindThumbBranch22;
    }
    // True iff the fixup writes a pointer-sized absolute value.
    static constexpr bool kindIsAbsolutePointer(uint16_t k) {
        return k == kindPtr64 || k == kindPtr32 || k == kindArm64AuthPtr;
    }
    static constexpr bool kindIsDelta(uint16_t k) {
        return k == kindDiff32 || k == kindDiff64;
    }
    static constexpr bool kindIsImageOffset(uint16_t k) {
        return k == kindImageOffset32;
    }
    // Internal metadata kinds, never reach applyFixup.
    static constexpr bool kindIsInternalOnly(uint16_t k) {
        return k == kindNone || k == kindKeepAlive
            || k == kindDylibExportClient || k == kindAlias
            || k == kindDylibIndex;
    }
};

inline const char *primeFixupKindName(uint16_t k) {
    switch (k) {
    case Fixup::kindNone:             return "none";
    case Fixup::kindKeepAlive:        return "keepAlive";
    case Fixup::kindPtr64:            return "ptr64";
    case Fixup::kindPtr32:            return "ptr32";
    case Fixup::kindDiff32:           return "diff32";
    case Fixup::kindDiff64:           return "diff64";
    case Fixup::kindImageOffset32:    return "imageOffset32";
    case Fixup::kindPcrel32ToGot:     return "pcrel32_to_got";
    case Fixup::kindPcrelDelta32:     return "pcrel_delta32";
    case Fixup::kindSwiftRel32ToGot:  return "swift_rel32_to_got";
    case Fixup::kindDylibExportClient:return "dylibExportClient";
    case Fixup::kindAlias:            return "alias of";
    case Fixup::kindTlvOffset:        return "tlvOffset";
    case Fixup::kindPtr32ToGot:       return "ptr32_to_got";
    case Fixup::kindPtr64ToGot:       return "ptr64_to_got";
    case Fixup::kindDylibIndex:       return "dylibIndex";
    case Fixup::kindArm64Branch26:    return "arm64_b26";
    case Fixup::kindArm64Branch26Add: return "arm64_b26_addend";
    case Fixup::kindArm64Adrp:        return "arm64_adrp";
    case Fixup::kindArm64AdrpAdd:     return "arm64_adrp_addend";
    case Fixup::kindArm64Lo12:        return "arm64_lo12";
    case Fixup::kindArm64Lo12Add:     return "arm64_lo12_addend";
    case Fixup::kindArm64AdrpGot:     return "arm64_adrp_got";
    case Fixup::kindArm64Lo12Got:     return "arm64_ld12_got";
    case Fixup::kindArm64AdrpLdr:     return "arm64_adrp_lo12";
    case Fixup::kindArm64AdrpLdrAdd:  return "arm64_adrp_lo12_addend";
    case Fixup::kindArm64AdrpLdrGot:  return "arm64_adrp_ldr_got";
    case Fixup::kindArm64AdrpTlv:     return "arm64_adrp_tlv";
    case Fixup::kindArm64Lo12Tlv:     return "arm64_ld12_tlv";
    case Fixup::kindArm64AuthPtr:     return "arm64_auth_ptr";
    case Fixup::kindArm64AddGot:      return "arm64_add_got";
    case Fixup::kindArm64AdrpAddGot:  return "arm64_adrp_add_got";
    case Fixup::kindArm64WasAdrpLdrGotLoadGot:  return "arm64_was_adrp_ldr_got_load_got";
    case Fixup::kindArm64WasAdrpLdrGotLeaGot:   return "arm64_was_adrp_ldr_got_lea_got";
    case Fixup::kindArm64WasAdrpLdrGotElideGot: return "arm64_was_adrp_ldr_got_elide_got";
    case Fixup::kindArm64WasAdrpLdrGotBlHelper: return "arm64_was_adrp_ldr_got_bl_helper";
    case Fixup::kindArm64WasAdrpGotUseGot:      return "arm64_was_adrp_got_use_got";
    case Fixup::kindArm64WasAdrpGotElideGot:    return "arm64_was_adrp_got_elide_got";
    case Fixup::kindArm64WasAdrpGotBlHelper:    return "arm64_was_adrp_got_bl_helper";
    case Fixup::kindArm64WasLo12GotLoadGot:     return "arm64_was_ld12_got_load_got";
    case Fixup::kindArm64WasLo12GotLeaGot:      return "arm64_was_ld12_got_lea_got";
    case Fixup::kindArm64WasLo12GotElideGot:    return "arm64_was_ld12_got_elide_got";
    case Fixup::kindArm64WasB26DtraceProbe:     return "arm64_was_b26_dtrace_probe";
    case Fixup::kindArm64WasB26DtraceEnable:    return "arm64_was_b26_dtrace_enable";
    case Fixup::kindArm64WasAdrpTlvLoadGot:     return "arm64_was_adrp_tlv_load_got";
    case Fixup::kindArm64WasAdrpTlvElideGot:    return "arm64_was_adrp_tlv_elide_got";
    case Fixup::kindArm64WasLo12TlvLoadGot:     return "arm64_was_ld12_tlv_load_got";
    case Fixup::kindArm64WasLo12TlvElideGot:    return "arm64_was_ld12_tlv_elide_got";
    case Fixup::kindArm64WasAdrpAddGotLeaGot:   return "arm64_was_adrp_add_got_lea_got";
    case Fixup::kindArm64WasAdrpAddGotBlHelper: return "arm64_was_adrp_add_got_bl_helper";
    case Fixup::kindX86Call:          return "x86_64_call";
    case Fixup::kindX86RipRel:        return "x86_64_rip";
    case Fixup::kindX86RipGot:        return "x86_64_rip_got";
    case Fixup::kindX86RipRel1:       return "x86_64_rip1";
    case Fixup::kindX86RipRel2:       return "x86_64_rip2";
    case Fixup::kindX86RipRel4:       return "x86_64_rip4";
    case Fixup::kindX86RipGotLoad:    return "x86_64_rip_got_load";
    case Fixup::kindX86RipTlvLoad:    return "x86_64_rip_tlv_load";
    case Fixup::kindX86Branch8:       return "x86_64_branch8";
    case Fixup::kindX86RipRel1Got:    return "x86_64_rip1_got";
    case Fixup::kindX86WasRipGotLoadLoadGot:    return "x86_64_was_rip_got_load_load_got";
    case Fixup::kindX86WasRipGotLoadElideGot:   return "x86_64_was_rip_got_load_elide_got";
    case Fixup::kindX86WasRipGotLoadCallHelper: return "x86_64_was_rip_got_load_call_helper";
    case Fixup::kindX86WasRipGotLeaCallHelper:  return "x86_64_was_rip_got_lea_call_helper";
    case Fixup::kindX86WasRipGotCmpCallHelper:  return "x86_64_was_rip_got_cmp_call_helper";
    case Fixup::kindX86WasCallDtraceProbe:      return "x86_64_was_call_dtrace_probe";
    case Fixup::kindX86WasCallDtraceEnable:     return "x86_64_was_call_dtrace_enable";
    case Fixup::kindX86WasRipTlvLoadGot:        return "x86_64_was_rip_tlv_load_got";
    case Fixup::kindX86WasRipTlvElideGot:       return "x86_64_was_rip_tlv_elide_got";
    case Fixup::kindArm32Branch24:    return "arm32_b24";
    case Fixup::kindThumbBranch22:    return "thumb_b22";
    case Fixup::kindArm32Ptr:         return "arm32_ptr";
    case Fixup::kindArm32PtrDiff:     return "arm32_ptrdiff";
    case Fixup::kindArm32Pcrel:       return "arm32_pcrel";
    case Fixup::kindArm32Hi16Pc:      return "arm32_hi16_pc";
    case Fixup::kindArm32Lo16Pc:      return "arm32_lo16_pc";
    case Fixup::kindThumbHi16Pc:      return "thumb_hi16_pc";
    case Fixup::kindThumbLo16Pc:      return "thumb_lo16_pc";
    case Fixup::kindArm32Hi16:        return "arm32_hi16";
    case Fixup::kindArm32Lo16:        return "arm32_lo16";
    case Fixup::kindThumbHi16:        return "thumb_hi16";
    case Fixup::kindThumbLo16:        return "thumb_lo16";
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

// ld64 classic fixup. `binding` in flags selects the union interpretation.
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

inline const char *classicFixupKindName(uint8_t k) {
    switch (k) {
    case Fixup::kindNone:                              return "none";
    case Fixup::kindNoneFollowOn:                      return "none_follow_on";
    case Fixup::kindNoneGroupSubordinate:              return "none_group_sub";
    case Fixup::kindNoneGroupSubordinateFDE:           return "none_group_sub_fde";
    case Fixup::kindNoneGroupSubordinateLSDA:          return "none_group_sub_lsda";
    case Fixup::kindNoneGroupSubordinatePersonality:   return "none_group_sub_pers";
    case Fixup::kindSetTargetAddress:                  return "set_target_addr";
    case Fixup::kindSubtractTargetAddress:             return "sub_target_addr";
    case Fixup::kindAddAddend:                         return "add_addend";
    case Fixup::kindSubtractAddend:                    return "sub_addend";
    case Fixup::kindSetTargetImageOffset:              return "set_image_off";
    case Fixup::kindSetTargetSectionOffset:            return "set_sect_off";
    case Fixup::kindSetTargetTLVTemplateOffset:        return "set_tlv_tmpl_off";
    case Fixup::kindStore8:                            return "store8";
    case Fixup::kindStoreLittleEndian16:               return "store_le16";
    case Fixup::kindStoreLittleEndianLow24of32:        return "store_le_low24";
    case Fixup::kindStoreLittleEndian32:               return "store_le32";
    case Fixup::kindStoreLittleEndian64:               return "store_le64";
    case Fixup::kindStoreBigEndian16:                  return "store_be16";
    case Fixup::kindStoreBigEndianLow24of32:           return "store_be_low24";
    case Fixup::kindStoreBigEndian32:                  return "store_be32";
    case Fixup::kindStoreBigEndian64:                  return "store_be64";
    case Fixup::kindStoreX86BranchPCRel8:              return "x86_bpc8";
    case Fixup::kindStoreX86BranchPCRel32:             return "x86_bpc32";
    case Fixup::kindStoreX86PCRel8:                    return "x86_pc8";
    case Fixup::kindStoreX86PCRel16:                   return "x86_pc16";
    case Fixup::kindStoreX86PCRel32:                   return "x86_pc32";
    case Fixup::kindStoreX86PCRel32_1:                 return "x86_pc32_1";
    case Fixup::kindStoreX86PCRel32_2:                 return "x86_pc32_2";
    case Fixup::kindStoreX86PCRel32_4:                 return "x86_pc32_4";
    case Fixup::kindStoreX86PCRel32GOTLoad:            return "x86_pc32_got_ld";
    case Fixup::kindStoreX86PCRel32GOTLoadNowLEA:      return "x86_pc32_got_ld_lea";
    case Fixup::kindStoreX86PCRel32GOT:                return "x86_pc32_got";
    case Fixup::kindStoreX86PCRel32TLVLoad:            return "x86_pc32_tlv_ld";
    case Fixup::kindStoreX86PCRel32TLVLoadNowLEA:      return "x86_pc32_tlv_ld_lea";
    case Fixup::kindStoreX86Abs32TLVLoad:              return "x86_abs32_tlv_ld";
    case Fixup::kindStoreX86Abs32TLVLoadNowLEA:        return "x86_abs32_tlv_ld_lea";
    case Fixup::kindStoreARMBranch24:                  return "arm_b24";
    case Fixup::kindStoreThumbBranch22:                return "thumb_b22";
    case Fixup::kindStoreARMLoad12:                    return "arm_ld12";
    case Fixup::kindStoreARMLow16:                     return "arm_lo16";
    case Fixup::kindStoreARMHigh16:                    return "arm_hi16";
    case Fixup::kindStoreThumbLow16:                   return "thumb_lo16";
    case Fixup::kindStoreThumbHigh16:                  return "thumb_hi16";
    case Fixup::kindStoreARM64Branch26:                return "arm64_b26";
    case Fixup::kindStoreARM64Page21:                  return "arm64_page21";
    case Fixup::kindStoreARM64PageOff12:               return "arm64_pageoff12";
    case Fixup::kindStoreARM64GOTLoadPage21:           return "arm64_got_page21";
    case Fixup::kindStoreARM64GOTLoadPageOff12:        return "arm64_got_pageoff12";
    case Fixup::kindStoreARM64GOTLeaPage21:            return "arm64_got_lea_page21";
    case Fixup::kindStoreARM64GOTLeaPageOff12:         return "arm64_got_lea_pageoff12";
    case Fixup::kindStoreARM64TLVPLoadPage21:          return "arm64_tlv_page21";
    case Fixup::kindStoreARM64TLVPLoadPageOff12:       return "arm64_tlv_pageoff12";
    case Fixup::kindStoreARM64TLVPLoadNowLeaPage21:    return "arm64_tlv_lea_page21";
    case Fixup::kindStoreARM64TLVPLoadNowLeaPageOff12: return "arm64_tlv_lea_pageoff12";
    case Fixup::kindStoreARM64PointerToGOT:            return "arm64_ptr_to_got";
    case Fixup::kindStoreARM64PCRelToGOT:              return "arm64_pcrel_to_got";
    default: return "?";
    }
}

} // namespace classic
} // namespace ld

#endif
