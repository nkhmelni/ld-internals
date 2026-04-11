// Copyright (c) 2026 Nikita Hmelnitkii. MIT License - see LICENSE.
//
// Atom.h - atom layout, BIND/REBASE classification, file/dylib accessors.
// Atom_1 = 0x30 (.o files), DynamicAtom = 0x58 (GOT/stubs). Vtables stable.

#ifndef LD_ATOM_H
#define LD_ATOM_H

#include "Fixup.h"
#include "Primitives.h"
#include "Mach.h"
#include "LinkEdit.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ld {

// Unaligned memory access primitives.

inline uint8_t  readU8 (const void *b, size_t o) { return static_cast<const uint8_t*>(b)[o]; }
inline uint16_t readU16(const void *b, size_t o) { uint16_t v; __builtin_memcpy(&v, static_cast<const uint8_t*>(b)+o, 2); return v; }
inline uint32_t readU32(const void *b, size_t o) { uint32_t v; __builtin_memcpy(&v, static_cast<const uint8_t*>(b)+o, 4); return v; }
inline uint64_t readU64(const void *b, size_t o) { uint64_t v; __builtin_memcpy(&v, static_cast<const uint8_t*>(b)+o, 8); return v; }
inline const void *readPtr(const void *b, size_t o) { const void *v; __builtin_memcpy(&v, static_cast<const uint8_t*>(b)+o, sizeof(v)); return v; }

inline void writeU8 (void *b, size_t o, uint8_t  v) { static_cast<uint8_t*>(b)[o] = v; }
inline void writeU16(void *b, size_t o, uint16_t v) { __builtin_memcpy(static_cast<uint8_t*>(b)+o, &v, 2); }
inline void writeU32(void *b, size_t o, uint32_t v) { __builtin_memcpy(static_cast<uint8_t*>(b)+o, &v, 4); }
inline void writeU64(void *b, size_t o, uint64_t v) { __builtin_memcpy(static_cast<uint8_t*>(b)+o, &v, 8); }
inline void writePtr(void *b, size_t o, const void *v) { __builtin_memcpy(static_cast<uint8_t*>(b)+o, &v, sizeof(v)); }

// atom kind (+0x08). BIND vs REBASE via makeBindTarget bitmask 0x103F.
namespace AtomKind {
    inline constexpr uint8_t kRegular0     = 0x00;
    inline constexpr uint8_t kRegular1     = 0x01;
    inline constexpr uint8_t kRegular2     = 0x02;
    inline constexpr uint8_t kRegular3     = 0x03;
    inline constexpr uint8_t kRegular4     = 0x04;
    inline constexpr uint8_t kRegular5     = 0x05;
    inline constexpr uint8_t kExternal     = 0x06;
    inline constexpr uint8_t kDylibExport0 = 0x07;
    inline constexpr uint8_t kDylibExport1 = 0x08;
    inline constexpr uint8_t kDylibExport2 = 0x09;
    inline constexpr uint8_t kExternal2    = 0x0A;
    inline constexpr uint8_t kDylibWeak    = 0x0B;
    inline constexpr uint8_t kAlias        = 0x0C;
    inline constexpr uint8_t kTentative0   = 0x0D;
    inline constexpr uint8_t kTentative1   = 0x0E;
    inline constexpr uint8_t kTentative2   = 0x0F;
    inline constexpr uint8_t kTentative3   = 0x10;
    inline constexpr uint8_t kProxy        = 0x12;
    inline constexpr uint8_t kStub         = 0x13;

    constexpr bool isBind(uint8_t k)        { return k <= 18 && !((0x103Fu >> k) & 1); }
    constexpr bool isRebase(uint8_t k)      { return k > 18 || ((0x103Fu >> k) & 1); }
    constexpr bool isProxy(uint8_t k)       { return k == kProxy; }
    constexpr bool isDylibExport(uint8_t k) { return k >= kDylibExport0 && k <= kDylibExport2; }
    constexpr bool isTentative(uint8_t k)   { return k >= kTentative0 && k <= kTentative3; }
}

// atom scope (+0x09) - ld::file_format::Scope values.
namespace AtomScope {
    inline constexpr uint8_t kTranslationUnit  = 0;
    inline constexpr uint8_t kHidden           = 1;
    inline constexpr uint8_t kGlobalAutoHidden = 2;
    inline constexpr uint8_t kGlobal           = 3;
    inline constexpr uint8_t kGlobalNeverStrip = 4;
}

// atom content type (+0x0A) - section assignment driver. Values 0x3B-0x3F
// are internal markers (segmentStart/End, sectionStart/End, custom).
namespace ContentType {
    inline constexpr uint8_t kInvalid          = 0x00;
    inline constexpr uint8_t kNone             = 0x01;
    inline constexpr uint8_t kFunction         = 0x02;  // __TEXT,__text
    inline constexpr uint8_t kStub             = 0x03;  // __TEXT,__stubs
    inline constexpr uint8_t kAuthStub         = 0x04;  // __TEXT,__auth_stubs
    inline constexpr uint8_t kObjcStub         = 0x05;  // __TEXT,__objc_stubs
    inline constexpr uint8_t kStubHelper       = 0x06;  // __TEXT,__stub_helper
    inline constexpr uint8_t kResolverHelper   = 0x07;  // __TEXT,__resolver_help
    inline constexpr uint8_t kBranchIsland     = 0x08;
    inline constexpr uint8_t kConstText        = 0x09;  // __TEXT,__const
    inline constexpr uint8_t kCString          = 0x0A;  // __TEXT,__cstring
    inline constexpr uint8_t kObjcClassName    = 0x0B;  // __TEXT,__objc_classname
    inline constexpr uint8_t kObjcMethodName   = 0x0C;  // __TEXT,__objc_methname
    inline constexpr uint8_t kObjcMethodType   = 0x0D;  // __TEXT,__objc_methtype
    inline constexpr uint8_t kOsLogStrings     = 0x0E;  // __TEXT,__oslogstring
    inline constexpr uint8_t kObjcMethodList   = 0x0F;  // __TEXT,__objc_methlist
    inline constexpr uint8_t kUtf16String      = 0x10;  // __TEXT,__ustring
    inline constexpr uint8_t kLiteral4         = 0x11;  // __TEXT,__literal4
    inline constexpr uint8_t kLiteral8         = 0x12;  // __TEXT,__literal8
    inline constexpr uint8_t kLiteral16        = 0x13;  // __TEXT,__literal16
    inline constexpr uint8_t kDtrace           = 0x14;
    inline constexpr uint8_t kAuthGot          = 0x15;  // __DATA,__auth_got
    inline constexpr uint8_t kGot              = 0x16;  // __DATA,__got
    inline constexpr uint8_t kLazyPointer      = 0x17;  // __DATA,__la_symbol_ptr
    inline constexpr uint8_t kLazyPointerSR    = 0x18;  // __DATA,__la_resolver
    inline constexpr uint8_t kAuthPtr          = 0x19;  // __DATA,__auth_ptr
    inline constexpr uint8_t kConstData        = 0x1A;  // __DATA,__const
    inline constexpr uint8_t kData             = 0x1B;  // __DATA,__data
    inline constexpr uint8_t kCfstring         = 0x1C;  // __DATA,__cfstring
    inline constexpr uint8_t kCfObj2           = 0x1D;  // __DATA,__const_cfobj2
    inline constexpr uint8_t kLSDA            = 0x1E;   // __TEXT,__gcc_except_tab
    inline constexpr uint8_t kCFI             = 0x1F;   // __TEXT,__eh_frame
    inline constexpr uint8_t kCompactUnwind    = 0x20;
    inline constexpr uint8_t kObjcClassRef     = 0x21;  // __DATA,__objc_classrefs
    inline constexpr uint8_t kObjcSuperRef     = 0x22;  // __DATA,__objc_superrefs
    inline constexpr uint8_t kObjcSelectorRef  = 0x23;  // __DATA,__objc_selrefs
    inline constexpr uint8_t kObjcProtocolRef  = 0x24;  // __DATA,__objc_protorefs
    inline constexpr uint8_t kObjcIvar         = 0x25;  // __DATA,__objc_ivar
    inline constexpr uint8_t kObjcData         = 0x26;  // __DATA,__objc_data
    inline constexpr uint8_t kObjcConst        = 0x27;  // __DATA,__objc_const
    inline constexpr uint8_t kObjcClassList    = 0x28;  // __DATA,__objc_classlist
    inline constexpr uint8_t kObjcCatList      = 0x29;  // __DATA,__objc_catlist
    inline constexpr uint8_t kObjcProtoList    = 0x2A;  // __DATA,__objc_protolist
    inline constexpr uint8_t kObjcImageInfo    = 0x2B;  // __DATA,__objc_imageinfo
    inline constexpr uint8_t kObjcNLClsList    = 0x2C;  // __DATA,__objc_nlclslist
    inline constexpr uint8_t kObjcNLCatList    = 0x2D;  // __DATA,__objc_nlcatlist
    inline constexpr uint8_t kObjcIntObj       = 0x2E;  // __DATA,__objc_intobj
    inline constexpr uint8_t kObjcFloatObj     = 0x2F;  // __DATA,__objc_floatobj
    inline constexpr uint8_t kObjcDoubleObj    = 0x30;  // __DATA,__objc_doubleobj
    inline constexpr uint8_t kObjcDateObj      = 0x31;  // __DATA,__objc_dateobj
    inline constexpr uint8_t kObjcDictObj      = 0x32;  // __DATA,__objc_dictobj
    inline constexpr uint8_t kObjcArrayObj     = 0x33;  // __DATA,__objc_arrayobj
    inline constexpr uint8_t kObjcArrayData    = 0x34;  // __DATA,__objc_arraydata
    inline constexpr uint8_t kModInitPtr       = 0x35;  // __DATA,__mod_init_func
    inline constexpr uint8_t kModTermPtr       = 0x36;  // __DATA,__mod_term_func
    inline constexpr uint8_t kInitOffset       = 0x37;  // __TEXT,__init_offsets
    inline constexpr uint8_t kStaticInit       = 0x38;  // __TEXT,__StaticInit
    inline constexpr uint8_t kThreadVars       = 0x39;  // __DATA,__thread_vars
    inline constexpr uint8_t kThreadPtrs       = 0x3A;  // __DATA,__thread_ptrs
    inline constexpr uint8_t kCustom           = 0x3F;
    inline constexpr uint8_t kThreadData       = 0x40;  // __DATA,__thread_data
    inline constexpr uint8_t kThreadBss        = 0x41;  // __DATA,__thread_bss
    inline constexpr uint8_t kCommon           = 0x42;  // __DATA,__common
    inline constexpr uint8_t kBss              = 0x43;  // __DATA,__bss
    inline constexpr uint8_t kDelayInitStub    = 0x44;  // __TEXT,__delay_stubs
    inline constexpr uint8_t kDelayInitHelper  = 0x45;  // __TEXT,__delay_helper
    inline constexpr uint8_t kObjcCatList2     = 0x46;  // __DATA,__objc_catlist2
    inline constexpr uint8_t kObjcClassROList  = 0x48;  // __DATA,__objc_clsrolist
}

// atom flags (+0x0D)
namespace AtomFlags {
    inline constexpr uint8_t kLive      = 0x01;
    inline constexpr uint8_t kAlias     = 0x04;
    inline constexpr uint8_t kActivated = 0x20;
}

// DynamicAtom flags (+0x4D)
namespace DynamicAtomFlags {
    inline constexpr uint8_t kDontDeadStrip           = 0x01;
    inline constexpr uint8_t kDontDeadStripIfRefsLive = 0x02;
    inline constexpr uint8_t kCold                    = 0x04;
    inline constexpr uint8_t kHasLinkConstraints      = 0x08;
    inline constexpr uint8_t kHasLOHs                 = 0x10;  // DynamicAtom::hasLOHs()
}

// Opaque atom handle. All reads go through offset-based accessors.
using AtomPtr = const void *;

inline uint8_t atomKind(AtomPtr a)        { return readU8(a, 0x08); }
inline uint8_t atomScope(AtomPtr a)       { return readU8(a, 0x09); }
inline uint8_t atomContentType(AtomPtr a) { return readU8(a, 0x0A); }
inline uint8_t atomFlags(AtomPtr a)       { return readU8(a, 0x0D); }

inline bool    atomIsProxy(AtomPtr a)     { return AtomKind::isProxy(atomKind(a)); }
inline bool    atomIsBind(AtomPtr a)      { return AtomKind::isBind(atomKind(a)); }
inline bool    atomIsRebase(AtomPtr a)    { return AtomKind::isRebase(atomKind(a)); }
inline bool    atomIsAlias(AtomPtr a)     { return (atomFlags(a) & AtomFlags::kAlias) != 0; }
inline AtomPtr atomAliasTarget(AtomPtr a) { return static_cast<AtomPtr>(readPtr(a, 0x18)); }
inline const void *atomPlacement(AtomPtr a) { return readPtr(a, 0x10); }

// Vtable dispatch helpers. `slot` is the byte offset within the vtable.
template <typename R>
inline R atomVCall(AtomPtr a, size_t slot) {
    const void *vt = readPtr(a, 0);
    using Fn = R (*)(const void *);
    Fn fn; __builtin_memcpy(&fn, static_cast<const uint8_t*>(vt) + slot, sizeof(fn));
    return fn(a);
}

template <typename R>
inline R fileVCall(const void *f, size_t slot) {
    const void *vt = readPtr(f, 0);
    using Fn = R (*)(const void *);
    Fn fn; __builtin_memcpy(&fn, static_cast<const uint8_t*>(vt) + slot, sizeof(fn));
    return fn(f);
}

template <typename R, typename A>
inline R fileVCallArg(const void *f, size_t slot, A arg) {
    const void *vt = readPtr(f, 0);
    using Fn = R (*)(const void *, A);
    Fn fn; __builtin_memcpy(&fn, static_cast<const uint8_t*>(vt) + slot, sizeof(fn));
    return fn(f, arg);
}

// ld::SymbolString - 16-byte header in front of a null-terminated C string,
// owned by SymbolStringZone. `InternedString` is a legacy alias kept so
// existing call sites compile against both names.
namespace SymbolString {
    inline constexpr size_t kOwnerZone = 0x00;  // ptr to ChunkBumpAllocatorZone
    inline constexpr size_t kHash      = 0x08;  // pre-hashed string value
    inline constexpr size_t kCStr      = 0x10;  // null-terminated C string
}
namespace InternedString {
    inline constexpr size_t kHeader = 0x00;
    inline constexpr size_t kHash   = 0x08;
    inline constexpr size_t kCStr   = 0x10;
}

inline const void *atomNameRaw(AtomPtr a) { return atomVCall<const void*>(a, 0x08); }
inline const char *atomName(AtomPtr a) {
    const void *raw = atomNameRaw(a);
    return raw ? static_cast<const char*>(raw) + InternedString::kCStr : nullptr;
}

// atomFixups() - vtable slot +0x58, stable across all versions.
struct FixupSpan { Fixup *ptr; uint32_t count; };
static_assert(sizeof(FixupSpan) == 16, "");

inline FixupSpan atomFixups(AtomPtr a) {
    const void *vt = readPtr(a, 0);
    using Fn = FixupSpan (*)(const void *);
    Fn fn; __builtin_memcpy(&fn, static_cast<const uint8_t*>(vt) + 0x58, sizeof(fn));
    return fn(a);
}

// atom vtable slots 0-15 (stable)
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
    inline constexpr size_t kRawContent    = 0x50;
    inline constexpr size_t kFixups        = 0x58;
    inline constexpr size_t kIsDynamicAtom = 0x60;
    inline constexpr size_t kDylibFileInfo = 0x68;
    inline constexpr size_t kHasLinkConstraints = 0x70;
    inline constexpr size_t kHasLOHs       = 0x78;
}

inline bool isDynamicAtom(AtomPtr a) { return atomVCall<bool>(a, vtable::kIsDynamicAtom); }
inline bool atomHasLOHs(AtomPtr a)   { return atomVCall<bool>(a, vtable::kHasLOHs); }

// Atom base class fields (stable across versions).
namespace atom {
    inline constexpr size_t kVtable      = 0x00;
    inline constexpr size_t kKind        = 0x08;
    inline constexpr size_t kScope       = 0x09;
    inline constexpr size_t kContentType = 0x0A;
    inline constexpr size_t kFlags       = 0x0D;
    inline constexpr size_t kPlacement   = 0x10;
    inline constexpr size_t kAliasTarget = 0x18;

    // Atom_1 (0x30 bytes)
    inline constexpr size_t kAtom1_RO        = 0x20;
    inline constexpr size_t kAtom1_DebugInfo = 0x28;
    inline constexpr size_t kAtom1_Size      = 0x30;

    // DynamicAtom (0x58 bytes)
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

// AtomRO_1 - read-only data for Atom_1 (+0x20). Fixups: pool + RO[0x08]*16,
// count = RO[0x04].
namespace AtomRO {
    inline constexpr size_t kOrdinal       = 0x00;
    inline constexpr size_t kFixupCount    = 0x04;
    inline constexpr size_t kFixupStart    = 0x08;  // pool index (pool + val*16)
    inline constexpr size_t kNameOffset    = 0x0C;  // 0xFFFFFF = unnamed
    inline constexpr size_t kDescriptor    = 0x10;  // kind[7:3] scope[2:0] contentType[14:8]
                                                     // cold[15] sectionIndex[28:21]
    inline constexpr size_t kContentSize   = 0x14;
    inline constexpr size_t kContentOffset = 0x18;  // -1 = zero-fill
    inline constexpr size_t kDylibIndex    = 0x1C;  // 0xFF = none
    inline constexpr size_t kAlignLow      = 0x1D;
    inline constexpr size_t kAlignHigh     = 0x1E;
    inline constexpr size_t kExtDylibIndex = 0x20;  // 1-based, 0 = none
    inline constexpr size_t kSize          = 0x28;
}

// AtomFile base fields (stable across versions).
namespace AtomFile {
    inline constexpr size_t kVtable       = 0x00;
    inline constexpr size_t kAtomsBegin   = 0x08;
    inline constexpr size_t kAtomsEnd     = 0x10;
    inline constexpr size_t kAtomsCapacity= 0x18;
    inline constexpr size_t kConsolidator = 0x38;
    inline constexpr size_t kPath         = 0x78;
    inline constexpr size_t kPathLen      = 0x80;
}

// AtomFile vtable. ld-1221+ inserts srcPath at slot 1 (+0x08 shift to all
// subsequent slots) and appends atomLOHs/fileLOHs at the tail.
namespace AtomFileVtable {
    inline constexpr size_t kPath = 0x00;

    // ld-1115 slot numbering (add +0x08 for ld-1221+)
    inline constexpr size_t kAtomsGroup_1115         = 0x08;
    inline constexpr size_t kIsDynamicAtomFile_1115  = 0x10;
    inline constexpr size_t kIsInvalidAtomFile_1115  = 0x18;
    inline constexpr size_t kDylibFileInfo_1115      = 0x20;
    inline constexpr size_t kDependencyInfos_1115    = 0x28;
    inline constexpr size_t kLinkerOptions_1115      = 0x30;
    inline constexpr size_t kLargeAddends_1115       = 0x38;
    inline constexpr size_t kIsLTO_1115              = 0x40;
    inline constexpr size_t kAltFileInfoAtIndex_1115 = 0x48;
    inline constexpr size_t kAtomLinkConstraints_1115= 0x50;
    inline constexpr size_t kFileLinkConstraints_1115= 0x58;

    // ld-1221+ only (not present in ld-1115)
    inline constexpr size_t kSrcPath_1221            = 0x08;
    inline constexpr size_t kAtomLOHs_1221           = 0x68;
    inline constexpr size_t kFileLOHs_1221           = 0x70;
}

// DynamicAtomFile field offsets. Three tiers:
//   ld-1115, ld-1221/1230 (+srcPath shift), ld-1266 (+further shift).
namespace DynamicAtomFile {
    // fixup pool
    inline constexpr size_t kFixupPool_1115          = 0x1B8;
    inline constexpr size_t kFixupPool_1221          = 0x1F8;
    inline constexpr size_t kFixupPool_1266          = 0x218;
    // dylib file info
    inline constexpr size_t kDylibFileInfo_1115      = 0x128;
    inline constexpr size_t kDylibFileInfo_1221      = 0x138;
    inline constexpr size_t kDylibFileInfo_1266      = 0x140;
    // isLTO flag (byte)
    inline constexpr size_t kIsLTO_1115              = 0x108;
    inline constexpr size_t kIsLTO_1221              = 0x118;
    inline constexpr size_t kIsLTO_1266              = 0x120;
    // linker options span (16-byte element begin/end)
    inline constexpr size_t kLinkerOptions_1115      = 0x130;
    inline constexpr size_t kLinkerOptions_1221      = 0x140;
    inline constexpr size_t kLinkerOptions_1266      = 0x148;
    // alt file infos (ptr-element vector begin/end)
    inline constexpr size_t kAltFileInfos_1115       = 0x168;
    inline constexpr size_t kAltFileInfos_1221       = 0x178;
    inline constexpr size_t kAltFileInfos_1266       = 0x180;
    // link constraints (ptr-element vector begin/end)
    inline constexpr size_t kLinkConstraints_1221    = 0x190;
    // atom link constraint ranges (begin/end)
    inline constexpr size_t kAtomLCRanges_1221       = 0x1A8;
    // file link constraint start/count (2x uint32)
    inline constexpr size_t kFileLCStart_1221        = 0x1C0;
    inline constexpr size_t kFileLCCount_1221        = 0x1C4;
    // file LOHs span (8-byte element begin/end) - ld-1221+ only
    inline constexpr size_t kFileLOHs_1221           = 0x1C8;
    inline constexpr size_t kFileLOHs_1266           = 0x1D0;
    // atom LOH ranges (begin/end) - ld-1221+ only
    inline constexpr size_t kAtomLOHRanges_1221      = 0x1E0;
    inline constexpr size_t kAtomLOHRanges_1266      = 0x1E8;
    // large addends span (8-byte element begin/end)
    inline constexpr size_t kLargeAddends_1115       = 0x1D0;
    inline constexpr size_t kLargeAddends_1221       = 0x210;
    inline constexpr size_t kLargeAddends_1266       = 0x230;
}

// AtomFile_1 field offsets. Two tiers: ld-1115 / ld-1221+ (+0x10 shift).
namespace AtomFile1 {
    // dylib file info
    inline constexpr size_t kDylibFileInfo_1115      = 0x88;
    inline constexpr size_t kDylibFileInfo_1221      = 0x98;
    // fixup RO pool
    inline constexpr size_t kFixupPool_1115          = 0x90;
    inline constexpr size_t kFixupPool_1221          = 0xA0;
    // large addends span (begin/count)
    inline constexpr size_t kLargeAddends_1115       = 0xA0;
    inline constexpr size_t kLargeAddends_1221       = 0xB0;
    // linker options span (16-byte element begin/end)
    inline constexpr size_t kLinkerOptions_1115      = 0xD0;
    inline constexpr size_t kLinkerOptions_1221      = 0xE0;
    // dependency file infos span (ptr-element begin/end)
    inline constexpr size_t kDependencyInfos_1115    = 0xE8;
    inline constexpr size_t kDependencyInfos_1221    = 0xF8;
}

// DylibFileInfo - 0x90 bytes, stable across versions.
// CString = {ptr, len}. Vectors = {begin, end, cap}.
namespace DylibFileInfo {
    inline constexpr size_t kCompatVersion        = 0x00;
    inline constexpr size_t kCurrentVersion       = 0x04;
    inline constexpr size_t kInstallName          = 0x08;  // heap-allocated, null-terminated
    inline constexpr size_t kInstallNameLen        = 0x10;
    inline constexpr size_t kParentFrameworkPtr   = 0x18;  // borrowed CString (umbrella parent)
    inline constexpr size_t kParentFrameworkLen   = 0x20;
    inline constexpr size_t kPlatformsBegin       = 0x28;  // vector<u32> platform IDs
    inline constexpr size_t kPlatformsEnd         = 0x30;
    inline constexpr size_t kPlatformsCapacity    = 0x38;
    inline constexpr size_t kAllowedClientsBegin  = 0x40;  // vector<CString> LC_SUB_CLIENT
    inline constexpr size_t kAllowedClientsEnd    = 0x48;
    inline constexpr size_t kAllowedClientsCap    = 0x50;
    inline constexpr size_t kReexportedDylibsBegin= 0x58;  // vector<CString> re-exports
    inline constexpr size_t kReexportedDylibsEnd  = 0x60;
    inline constexpr size_t kReexportedDylibsCap  = 0x68;
    inline constexpr size_t kUpwardDylibsBegin    = 0x70;  // vector<CString> upward deps
    inline constexpr size_t kUpwardDylibsEnd      = 0x78;
    inline constexpr size_t kUpwardDylibsCap      = 0x80;
    inline constexpr size_t kHasWeakDefs          = 0x88;
    inline constexpr size_t kHasTLVars            = 0x89;
    inline constexpr size_t kSize                 = 0x90;
}

inline const void *atomFile(AtomPtr a)          { return atomVCall<const void*>(a, vtable::kFile); }
inline const void *atomDylibFileInfo(AtomPtr a) { return atomVCall<const void*>(a, vtable::kDylibFileInfo); }

// Raw content - vtable slot kRawContent, returns {ptr, size}.
struct RawContent { const uint8_t *ptr; uint64_t size; };
inline RawContent atomRawContent(AtomPtr a) {
    const void *vt = readPtr(a, 0);
    using Fn = RawContent (*)(const void *);
    Fn fn; __builtin_memcpy(&fn, static_cast<const uint8_t*>(vt) + vtable::kRawContent, sizeof(fn));
    return fn(a);
}

// Install name is guaranteed null-terminated by makeDylibFileInfo's
// malloc(len+1) + trailing nul. Returns nullptr only when dfi is null.
inline const char *dylibInstallName(const void *dfi) {
    return dfi ? static_cast<const char*>(readPtr(dfi, DylibFileInfo::kInstallName)) : nullptr;
}

// CString form (ptr, len) - avoids strlen when the length is already known.
inline CString dylibInstallNameCStr(const void *dfi) {
    if (!dfi) return {};
    return CString{
        static_cast<const char*>(readPtr(dfi, DylibFileInfo::kInstallName)),
        readU64(dfi, DylibFileInfo::kInstallNameLen)
    };
}

// Parent framework - non-empty only for sub-frameworks (e.g. libGLImage
// under OpenGL.framework). Borrowed CString, NOT guaranteed null-terminated.
inline CString dylibParentFramework(const void *dfi) {
    if (!dfi) return {};
    return CString{
        static_cast<const char*>(readPtr(dfi, DylibFileInfo::kParentFrameworkPtr)),
        readU64(dfi, DylibFileInfo::kParentFrameworkLen)
    };
}

// LC_ID_DYLIB compat / current versions (Version32).
inline Version32 dylibCompatVersion(const void *dfi) {
    if (!dfi) return {};
    return Version32{readU32(dfi, DylibFileInfo::kCompatVersion)};
}
inline Version32 dylibCurrentVersion(const void *dfi) {
    if (!dfi) return {};
    return Version32{readU32(dfi, DylibFileInfo::kCurrentVersion)};
}

inline bool dylibHasWeakDefs(const void *dfi) {
    return dfi && readU8(dfi, DylibFileInfo::kHasWeakDefs) != 0;
}
inline bool dylibHasTLVars(const void *dfi) {
    return dfi && readU8(dfi, DylibFileInfo::kHasTLVars) != 0;
}

// LC_BUILD_VERSION platform IDs present in the dylib.
inline Span<uint32_t> dylibPlatformIds(const void *dfi) {
    if (!dfi) return {};
    return Span<uint32_t>{
        static_cast<const uint32_t*>(readPtr(dfi, DylibFileInfo::kPlatformsBegin)),
        static_cast<const uint32_t*>(readPtr(dfi, DylibFileInfo::kPlatformsEnd))
    };
}

// LC_SUB_CLIENT allowed-client list.
inline Span<CString> dylibAllowedClients(const void *dfi) {
    if (!dfi) return {};
    return Span<CString>{
        static_cast<const CString*>(readPtr(dfi, DylibFileInfo::kAllowedClientsBegin)),
        static_cast<const CString*>(readPtr(dfi, DylibFileInfo::kAllowedClientsEnd))
    };
}

// LC_REEXPORT_DYLIB install names.
inline Span<CString> dylibReexports(const void *dfi) {
    if (!dfi) return {};
    return Span<CString>{
        static_cast<const CString*>(readPtr(dfi, DylibFileInfo::kReexportedDylibsBegin)),
        static_cast<const CString*>(readPtr(dfi, DylibFileInfo::kReexportedDylibsEnd))
    };
}

// LC_LOAD_UPWARD_DYLIB references.
inline Span<CString> dylibUpwardDeps(const void *dfi) {
    if (!dfi) return {};
    return Span<CString>{
        static_cast<const CString*>(readPtr(dfi, DylibFileInfo::kUpwardDylibsBegin)),
        static_cast<const CString*>(readPtr(dfi, DylibFileInfo::kUpwardDylibsEnd))
    };
}

inline const char *filePath(const void *file) {
    return file ? fileVCall<const char*>(file, AtomFileVtable::kPath) : nullptr;
}

// Version-dependent file vtable accessors. Pass the slot from LayoutConstants.
inline const char *fileSrcPath(const void *file, size_t vtSlot) {
    if (!file || vtSlot == 0) return nullptr;
    return fileVCall<const char*>(file, vtSlot);
}
inline bool fileIsDynamic(const void *file, size_t vtSlot) {
    return file ? fileVCall<bool>(file, vtSlot) : false;
}
inline bool fileIsLTO(const void *file, size_t vtSlot) {
    return file ? fileVCall<bool>(file, vtSlot) : false;
}
inline const void *fileDylibFileInfo(const void *file, size_t vtSlot) {
    return file ? fileVCall<const void*>(file, vtSlot) : nullptr;
}
inline const char *fileDylibInstallName(const void *file, size_t vtSlot) {
    return dylibInstallName(fileDylibFileInfo(file, vtSlot));
}

// Install name from DylibFileInfo; falls back to file path.
inline const char *atomDylibPath(AtomPtr a) {
    const char *n = dylibInstallName(atomDylibFileInfo(a));
    return n ? n : filePath(atomFile(a));
}

// Classic ld64 Atom vtable. ARM64 ABI has 2 destructor slots at the top.
namespace classic { namespace vtable {
    inline constexpr size_t kDestructorD1 = 0x00;  // ~Atom() complete object
    inline constexpr size_t kDestructorD0 = 0x08;  // ~Atom() deleting
    inline constexpr size_t kFile         = 0x10;
    inline constexpr size_t kTUSource     = 0x18;
    inline constexpr size_t kName         = 0x20;  // returns const char* directly
    inline constexpr size_t kObjectAddr   = 0x28;
    inline constexpr size_t kSize         = 0x30;
    inline constexpr size_t kCopyContent  = 0x38;
}}

} // namespace ld
#endif
