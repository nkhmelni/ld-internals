// Copyright (c) 2026 Nikita Hmelnitkii. MIT License - see LICENSE.
//
// Atom.h - atom and file layouts, vtable slots, SymbolString,
// DylibFileInfo, and the classic type system.

#ifndef LD_ATOM_H
#define LD_ATOM_H

#include "Fixup.h"
#include "Primitives.h"
#include "MachO.h"
#include "Version.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ld {

// Atom kind byte at +0x08. BIND/REBASE classified by bitmask 0x103F.
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

    inline const char *name(uint8_t k) {
        switch (k) {
        case kRegular0:     return "regular0";
        case kRegular1:     return "regular1";
        case kRegular2:     return "regular2";
        case kRegular3:     return "regular3";
        case kRegular4:     return "regular4";
        case kRegular5:     return "regular5";
        case kExternal:     return "external";
        case kDylibExport0: return "dylib_export0";
        case kDylibExport1: return "dylib_export1";
        case kDylibExport2: return "dylib_export2";
        case kExternal2:    return "external2";
        case kDylibWeak:    return "dylib_weak";
        case kAlias:        return "alias";
        case kTentative0:   return "tentative0";
        case kTentative1:   return "tentative1";
        case kTentative2:   return "tentative2";
        case kTentative3:   return "tentative3";
        case kProxy:        return "proxy";
        case kStub:         return "stub";
        default:            return "?";
        }
    }
}

// Scope byte at +0x09.
namespace AtomScope {
    inline constexpr uint8_t kTranslationUnit  = 0;
    inline constexpr uint8_t kHidden           = 1;
    inline constexpr uint8_t kGlobalAutoHidden = 2;
    inline constexpr uint8_t kGlobal           = 3;
    inline constexpr uint8_t kGlobalNeverStrip = 4;
}

// Content type byte at +0x0A. Determines section placement.
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

inline const char *contentTypeName(uint8_t ct) {
    switch (ct) {
    case ContentType::kInvalid:          return "invalid";
    case ContentType::kNone:             return "none";
    case ContentType::kFunction:         return "function";
    case ContentType::kStub:             return "stub";
    case ContentType::kAuthStub:         return "auth_stub";
    case ContentType::kObjcStub:         return "objc_stub";
    case ContentType::kStubHelper:       return "stub_helper";
    case ContentType::kResolverHelper:   return "resolver_helper";
    case ContentType::kBranchIsland:     return "branch_island";
    case ContentType::kConstText:        return "const_text";
    case ContentType::kCString:          return "cstring";
    case ContentType::kObjcClassName:    return "objc_classname";
    case ContentType::kObjcMethodName:   return "objc_methname";
    case ContentType::kObjcMethodType:   return "objc_methtype";
    case ContentType::kOsLogStrings:     return "oslogstring";
    case ContentType::kObjcMethodList:   return "objc_methlist";
    case ContentType::kUtf16String:      return "ustring";
    case ContentType::kLiteral4:         return "literal4";
    case ContentType::kLiteral8:         return "literal8";
    case ContentType::kLiteral16:        return "literal16";
    case ContentType::kDtrace:           return "dtrace";
    case ContentType::kAuthGot:          return "auth_got";
    case ContentType::kGot:              return "got";
    case ContentType::kLazyPointer:      return "lazy_ptr";
    case ContentType::kLazyPointerSR:    return "lazy_resolver";
    case ContentType::kAuthPtr:          return "auth_ptr";
    case ContentType::kConstData:        return "const_data";
    case ContentType::kData:             return "data";
    case ContentType::kCfstring:         return "cfstring";
    case ContentType::kCfObj2:           return "cfobj2";
    case ContentType::kLSDA:             return "lsda";
    case ContentType::kCFI:              return "eh_frame";
    case ContentType::kCompactUnwind:    return "compact_unwind";
    case ContentType::kObjcClassRef:     return "objc_classrefs";
    case ContentType::kObjcSuperRef:     return "objc_superrefs";
    case ContentType::kObjcSelectorRef:  return "objc_selrefs";
    case ContentType::kObjcProtocolRef:  return "objc_protorefs";
    case ContentType::kObjcIvar:         return "objc_ivar";
    case ContentType::kObjcData:         return "objc_data";
    case ContentType::kObjcConst:        return "objc_const";
    case ContentType::kObjcClassList:    return "objc_classlist";
    case ContentType::kObjcCatList:      return "objc_catlist";
    case ContentType::kObjcProtoList:    return "objc_protolist";
    case ContentType::kObjcImageInfo:    return "objc_imageinfo";
    case ContentType::kObjcNLClsList:    return "objc_nlclslist";
    case ContentType::kObjcNLCatList:    return "objc_nlcatlist";
    case ContentType::kObjcIntObj:       return "objc_intobj";
    case ContentType::kObjcFloatObj:     return "objc_floatobj";
    case ContentType::kObjcDoubleObj:    return "objc_doubleobj";
    case ContentType::kObjcDateObj:      return "objc_dateobj";
    case ContentType::kObjcDictObj:      return "objc_dictobj";
    case ContentType::kObjcArrayObj:     return "objc_arrayobj";
    case ContentType::kObjcArrayData:    return "objc_arraydata";
    case ContentType::kModInitPtr:       return "mod_init";
    case ContentType::kModTermPtr:       return "mod_term";
    case ContentType::kInitOffset:       return "init_offsets";
    case ContentType::kStaticInit:       return "static_init";
    case ContentType::kThreadVars:       return "thread_vars";
    case ContentType::kThreadPtrs:       return "thread_ptrs";
    case ContentType::kCustom:           return "custom";
    case ContentType::kThreadData:       return "thread_data";
    case ContentType::kThreadBss:        return "thread_bss";
    case ContentType::kCommon:           return "common";
    case ContentType::kBss:              return "bss";
    case ContentType::kDelayInitStub:    return "delay_stub";
    case ContentType::kDelayInitHelper:  return "delay_helper";
    case ContentType::kObjcCatList2:     return "objc_catlist2";
    case ContentType::kObjcClassROList:  return "objc_clsrolist";
    default: return "?";
    }
}

// Flags byte at +0x0D.
namespace AtomFlags {
    inline constexpr uint8_t kLive      = 0x01;
    inline constexpr uint8_t kAlias     = 0x04;
    inline constexpr uint8_t kActivated = 0x20;
}

// DynamicAtom flags byte at +0x4D.
namespace DynamicAtomFlags {
    inline constexpr uint8_t kDontDeadStrip           = 0x01;
    inline constexpr uint8_t kDontDeadStripIfRefsLive = 0x02;
    inline constexpr uint8_t kCold                    = 0x04;
    inline constexpr uint8_t kHasLinkConstraints      = 0x08;
    inline constexpr uint8_t kHasLOHs                 = 0x10;
}

using AtomPtr        = const void *;
using AtomFilePtr    = const void *;
using SegmentPtr     = const void *;
using SectionPtr     = const void *;
using LayoutPtr      = const void *;
using BuilderPtr     = const void *;
using DylibFileInfoPtr = const void *;

inline uint8_t atomKind(AtomPtr a)        { return a ? readU8(a, 0x08) : 0; }
inline uint8_t atomScope(AtomPtr a)       { return a ? readU8(a, 0x09) : 0; }
inline uint8_t atomContentType(AtomPtr a) { return a ? readU8(a, 0x0A) : 0; }
inline uint8_t atomFlags(AtomPtr a)       { return a ? readU8(a, 0x0D) : 0; }

inline bool    atomIsProxy(AtomPtr a)     { return a && AtomKind::isProxy(atomKind(a)); }
inline bool    atomIsBind(AtomPtr a)      { return a && AtomKind::isBind(atomKind(a)); }
inline bool    atomIsRebase(AtomPtr a)    { return a && AtomKind::isRebase(atomKind(a)); }
inline bool    atomIsAlias(AtomPtr a)     { return a && (atomFlags(a) & AtomFlags::kAlias) != 0; }
inline AtomPtr atomAliasTarget(AtomPtr a) { return a ? static_cast<AtomPtr>(readPtr(a, 0x18)) : nullptr; }
inline const void *atomPlacement(AtomPtr a) { return a ? readPtr(a, 0x10) : nullptr; }

// Vtable dispatch. Slot is the byte offset into the vtable.
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

// SymbolString: 16-byte header + null-terminated UTF-8 body.
//   +0x00  int64   self-relative offset to C string (typically 0x10)
//   +0x08  uint64  44-bit hash | 20-bit length
//   +0x10  char[]  string body
namespace SymbolString {
    inline constexpr size_t kSelfRelCStr = 0x00;
    inline constexpr size_t kHashSize    = 0x08;
    inline constexpr size_t kCStr        = 0x10;
    inline constexpr uint64_t kHashMask  = 0x0FFFFFFFFFFFULL;
    inline constexpr uint32_t kSizeShift = 44;
}

inline const char *symbolStringCStr(const void *s) {
    if (!s) return nullptr;
    int64_t off;
    __builtin_memcpy(&off, s, sizeof(off));
    return static_cast<const char *>(s) + off;
}

inline uint64_t symbolStringHash(const void *s) {
    return s ? (readU64(s, SymbolString::kHashSize) & SymbolString::kHashMask) : 0;
}

inline uint32_t symbolStringSize(const void *s) {
    return s ? static_cast<uint32_t>(readU64(s, SymbolString::kHashSize) >> SymbolString::kSizeShift) : 0;
}

inline const void *atomNameRaw(AtomPtr a) {
    return a ? atomVCall<const void*>(a, 0x08) : nullptr;
}
inline const char *atomName(AtomPtr a) {
    return symbolStringCStr(atomNameRaw(a));
}

// Fixup span returned by vtable slot +0x58.
struct FixupSpan { Fixup *ptr; uint32_t count; };
static_assert(sizeof(FixupSpan) == 16, "");

inline FixupSpan atomFixups(AtomPtr a) {
    if (!a) return FixupSpan{nullptr, 0};
    const void *vt = readPtr(a, 0);
    using Fn = FixupSpan (*)(const void *);
    Fn fn; __builtin_memcpy(&fn, static_cast<const uint8_t*>(vt) + 0x58, sizeof(fn));
    return fn(a);
}

// fn: bool(const Fixup &). Returning false stops iteration.
template <typename Fn>
inline void forEachFixup(AtomPtr a, Fn fn) {
    FixupSpan s = atomFixups(a);
    for (uint32_t i = 0; i < s.count; ++i) {
        if (!fn(s.ptr[i])) return;
    }
}

// Prime Atom vtable. Slots 0x00..0x80 stable; 0x88+ shifted on 1266.
namespace vtable {
    inline constexpr size_t kFile          = 0x00;
    inline constexpr size_t kName          = 0x08;
    inline constexpr size_t kAtomOrdinal   = 0x10;
    inline constexpr size_t kAlignment     = 0x18;
    inline constexpr size_t kSection       = 0x20;
    inline constexpr size_t kSectionCustom = 0x28;  // section(bool& outIsCustom)
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
    inline constexpr size_t kIsThumb       = 0x80;

    // 1266 tail (hasDataInCode + forEachDataInCode inserted at 0x88/0x90).
    inline constexpr size_t kHasDataInCode_1266     = 0x88;
    inline constexpr size_t kForEachDataInCode_1266 = 0x90;
    inline constexpr size_t kDebugFileInfo_1266     = 0x98;
    inline constexpr size_t kSetDebugFileInfo_1266  = 0xA0;
    inline constexpr size_t kFileRO_1266            = 0xA8;

    // 1221 tail (debugFileInfo/setDebugFileInfo/fileRO at 0x88/0x90/0x98).
    inline constexpr size_t kDebugFileInfo_1221     = 0x88;
    inline constexpr size_t kSetDebugFileInfo_1221  = 0x90;
    inline constexpr size_t kFileRO_1221            = 0x98;
}

// Tail-slot lookup. 0 for classic. fileRO is Atom_1-only on 1053
// (DynamicAtom has no fileRO slot there).
inline size_t atomVtableSlotDebugFileInfo(const LinkerVersion &v) {
    if (!v.isPrime()) return 0;
    if (v.major < 1100) return 0x78;
    return v.major >= 1266 ? vtable::kDebugFileInfo_1266
                           : vtable::kDebugFileInfo_1221;
}
inline size_t atomVtableSlotFileRO(const LinkerVersion &v) {
    if (!v.isPrime()) return 0;
    if (v.major < 1100) return 0x88;
    return v.major >= 1266 ? vtable::kFileRO_1266
                           : vtable::kFileRO_1221;
}

inline bool isDynamicAtom(AtomPtr a) { return a && atomVCall<bool>(a, vtable::kIsDynamicAtom); }

// Gated on 1053: slot 0x78 holds debugFileInfo there, not hasLOHs.
inline bool atomHasLOHs(AtomPtr a, const LinkerVersion &v) {
    if (!a || !v.isPrime() || v.major < 1100) return false;
    return atomVCall<bool>(a, vtable::kHasLOHs);
}
inline bool atomIsThumb(AtomPtr a, const LinkerVersion &v) {
    if (!a || !v.isPrime() || v.major < 1100) return false;
    return atomVCall<bool>(a, vtable::kIsThumb);
}

inline uint32_t atomOrdinal(AtomPtr a)              { return a ? atomVCall<uint32_t>(a, vtable::kAtomOrdinal) : 0; }
inline const void *atomSection(AtomPtr a)           { return a ? atomVCall<const void *>(a, vtable::kSection) : nullptr; }
inline bool atomIsCustomSect(AtomPtr a)             { return a && atomVCall<bool>(a, vtable::kIsCustomSect); }
inline bool atomDontDeadStrip(AtomPtr a)            { return a && atomVCall<bool>(a, vtable::kDontDeadStrip); }
inline bool atomDontDeadStripIfRefsLive(AtomPtr a)  { return a && atomVCall<bool>(a, vtable::kDontDeadStripIfRefsLive); }
inline bool atomCold(AtomPtr a)                     { return a && atomVCall<bool>(a, vtable::kCold); }
inline bool atomHasLinkConstraints(AtomPtr a)       { return a && atomVCall<bool>(a, vtable::kHasLinkConstraints); }

struct AtomAlignment {
    uint16_t powerOf2;
    uint16_t modulus;
    constexpr uint8_t trailingZeros() const { return static_cast<uint8_t>(powerOf2); }
};
static_assert(sizeof(AtomAlignment) == 4, "");

inline AtomAlignment atomAlignment(AtomPtr a) {
    if (!a) return AtomAlignment{0, 0};
    const void *vt = readPtr(a, 0);
    using Fn = AtomAlignment (*)(const void *);
    Fn fn; __builtin_memcpy(&fn, static_cast<const uint8_t *>(vt) + vtable::kAlignment, sizeof(fn));
    return fn(a);
}


// Atom base-class field offsets (stable across all versions).
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

// Atom_1 read-only descriptor at atom+0x20. Fixups are at
// pool + kFixupStart * 16, count kFixupCount.
namespace AtomRO {
    inline constexpr size_t kOrdinal       = 0x00;
    inline constexpr size_t kFixupCount    = 0x04;
    inline constexpr size_t kFixupStart    = 0x08;  // pool index (pool + val*16)
    inline constexpr size_t kNameOffset    = 0x0C;  // 0xFFFFFF = unnamed
    inline constexpr size_t kDescriptor    = 0x10;  // see atomRoDescriptor*() helpers
    inline constexpr size_t kContentSize   = 0x14;
    inline constexpr size_t kContentOffset = 0x18;  // -1 = zero-fill
    inline constexpr size_t kDylibIndex    = 0x1C;  // 0xFF = none
    inline constexpr size_t kAlignLow      = 0x1D;
    inline constexpr size_t kAlignHigh     = 0x1E;
    inline constexpr size_t kExtDylibIndex = 0x20;  // 1-based, 0 = none
    inline constexpr size_t kSize          = 0x28;

    // Packed descriptor at +0x10:
    //   [2:0] scope | [7:3] kind | [14:8] contentType | [15] cold | [28:21] sectIdx
    inline constexpr uint32_t kDescScopeMask   = 0x00000007;
    inline constexpr uint32_t kDescKindShift   = 3;
    inline constexpr uint32_t kDescKindMask    = 0x1F;     // 5 bits
    inline constexpr uint32_t kDescCtypeShift  = 8;
    inline constexpr uint32_t kDescCtypeMask   = 0x7F;     // 7 bits
    inline constexpr uint32_t kDescColdBit     = 15;
    inline constexpr uint32_t kDescSectIdxShift= 21;
    inline constexpr uint32_t kDescSectIdxMask = 0xFF;     // 8 bits
}

inline uint32_t atomRoDescriptor(const void *ro) {
    return ro ? readU32(ro, AtomRO::kDescriptor) : 0;
}
inline uint8_t atomRoScope(const void *ro) {
    return static_cast<uint8_t>(atomRoDescriptor(ro) & AtomRO::kDescScopeMask);
}
inline uint8_t atomRoKind(const void *ro) {
    return static_cast<uint8_t>(
        (atomRoDescriptor(ro) >> AtomRO::kDescKindShift) & AtomRO::kDescKindMask);
}
inline uint8_t atomRoContentType(const void *ro) {
    return static_cast<uint8_t>(
        (atomRoDescriptor(ro) >> AtomRO::kDescCtypeShift) & AtomRO::kDescCtypeMask);
}
inline bool atomRoIsCold(const void *ro) {
    return ro && ((atomRoDescriptor(ro) >> AtomRO::kDescColdBit) & 1);
}
inline uint8_t atomRoSectionIndex(const void *ro) {
    return static_cast<uint8_t>(
        (atomRoDescriptor(ro) >> AtomRO::kDescSectIdxShift) & AtomRO::kDescSectIdxMask);
}

inline uint8_t atomRawAlignmentPow2(AtomPtr a, bool isDynamic) {
    return isDynamic ? readU8(a, atom::kDynamic_Alignment)
                     : readU8(readPtr(a, atom::kAtom1_RO), AtomRO::kAlignLow);
}

// AtomFile base fields (stable across all versions).
namespace AtomFile {
    inline constexpr size_t kVtable       = 0x00;
    inline constexpr size_t kAtomsBegin   = 0x08;
    inline constexpr size_t kAtomsEnd     = 0x10;
    inline constexpr size_t kAtomsCapacity= 0x18;
    inline constexpr size_t kConsolidator = 0x38;
    inline constexpr size_t kPath         = 0x78;
    inline constexpr size_t kPathLen      = 0x80;
}

// Atoms vector view. Works for every AtomFile subclass.
inline Span<AtomPtr> atomFileAtoms(const void *file) {
    if (!file) return {};
    return Span<AtomPtr>{
        static_cast<const AtomPtr *>(readPtr(file, AtomFile::kAtomsBegin)),
        static_cast<const AtomPtr *>(readPtr(file, AtomFile::kAtomsEnd))
    };
}

inline const void *atomFileConsolidator(const void *file) {
    return file ? readPtr(file, AtomFile::kConsolidator) : nullptr;
}

// 1115+ direct access. Use the (pathOff, lenOff) overload for 1053.
inline CString atomFilePath(const void *file) {
    if (!file) return {};
    return CString{
        static_cast<const char *>(readPtr(file, AtomFile::kPath)),
        readU64(file, AtomFile::kPathLen)
    };
}

inline CString atomFilePath(const void *file, size_t pathOff, size_t lenOff) {
    if (!file || pathOff == 0 || lenOff == 0) return {};
    return CString{
        static_cast<const char *>(readPtr(file, pathOff)),
        readU64(file, lenOff)
    };
}

// AtomFile vtable slot 0 is always path(). All other slots diverge per
// version and live in LayoutConstants::fileVtable*.
namespace AtomFileVtable {
    inline constexpr size_t kPath = 0x00;
}

// DynamicAtomFile field offsets. Three tiers: 1115, 1221/1230, 1266.
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
    // atomAndDataInCode vector (begin/end/cap; 24-byte elements). 1266+ only.
    inline constexpr size_t kAtomAndDataInCode_1266  = 0x200;
    // large addends span (8-byte element begin/end)
    inline constexpr size_t kLargeAddends_1115       = 0x1D0;
    inline constexpr size_t kLargeAddends_1221       = 0x210;
    inline constexpr size_t kLargeAddends_1266       = 0x230;
}

// AtomFile_1 field offsets. Two tiers: 1115 / 1221+ (+0x10 shift).
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

// DylibFileInfo. 0x88 on 1053, 0x90 on 1115+ (adds hasWeakDefs/hasTLVars).
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

inline const void *atomFile(AtomPtr a)          { return a ? atomVCall<const void*>(a, vtable::kFile) : nullptr; }
inline const void *atomDylibFileInfo(AtomPtr a) { return a ? atomVCall<const void*>(a, vtable::kDylibFileInfo) : nullptr; }

// size is u32 (zero-extended on arm64).
struct RawContent { const uint8_t *ptr; uint32_t size; };
static_assert(sizeof(RawContent) == 16, "");

inline RawContent atomRawContent(AtomPtr a) {
    if (!a) return RawContent{nullptr, 0};
    const void *vt = readPtr(a, 0);
    using Fn = RawContent (*)(const void *);
    Fn fn; __builtin_memcpy(&fn, static_cast<const uint8_t*>(vt) + vtable::kRawContent, sizeof(fn));
    return fn(a);
}

inline const char *dylibInstallName(const void *dfi) {
    return dfi ? static_cast<const char*>(readPtr(dfi, DylibFileInfo::kInstallName)) : nullptr;
}

inline CString dylibInstallNameCStr(const void *dfi) {
    if (!dfi) return {};
    return CString{
        static_cast<const char*>(readPtr(dfi, DylibFileInfo::kInstallName)),
        readU64(dfi, DylibFileInfo::kInstallNameLen)
    };
}

inline CString dylibParentFramework(const void *dfi) {
    if (!dfi) return {};
    return CString{
        static_cast<const char*>(readPtr(dfi, DylibFileInfo::kParentFrameworkPtr)),
        readU64(dfi, DylibFileInfo::kParentFrameworkLen)
    };
}

inline Version32 dylibCompatVersion(const void *dfi) {
    if (!dfi) return {};
    return Version32{readU32(dfi, DylibFileInfo::kCompatVersion)};
}
inline Version32 dylibCurrentVersion(const void *dfi) {
    if (!dfi) return {};
    return Version32{readU32(dfi, DylibFileInfo::kCurrentVersion)};
}

// 1053 omits the trailing flag bytes; gate avoids reading past +0x88.
inline bool dylibHasWeakDefs(const void *dfi, const LinkerVersion &v) {
    if (!dfi || !v.isPrime() || v.major < 1100) return false;
    return readU8(dfi, DylibFileInfo::kHasWeakDefs) != 0;
}
inline bool dylibHasTLVars(const void *dfi, const LinkerVersion &v) {
    if (!dfi || !v.isPrime() || v.major < 1100) return false;
    return readU8(dfi, DylibFileInfo::kHasTLVars) != 0;
}

inline Span<uint32_t> dylibPlatformIds(const void *dfi) {
    if (!dfi) return {};
    return Span<uint32_t>{
        static_cast<const uint32_t*>(readPtr(dfi, DylibFileInfo::kPlatformsBegin)),
        static_cast<const uint32_t*>(readPtr(dfi, DylibFileInfo::kPlatformsEnd))
    };
}

inline Span<CString> dylibAllowedClients(const void *dfi) {

    if (!dfi) return {};
    return Span<CString>{
        static_cast<const CString*>(readPtr(dfi, DylibFileInfo::kAllowedClientsBegin)),
        static_cast<const CString*>(readPtr(dfi, DylibFileInfo::kAllowedClientsEnd))
    };
}

inline Span<CString> dylibReexports(const void *dfi) {
    if (!dfi) return {};
    return Span<CString>{
        static_cast<const CString*>(readPtr(dfi, DylibFileInfo::kReexportedDylibsBegin)),
        static_cast<const CString*>(readPtr(dfi, DylibFileInfo::kReexportedDylibsEnd))
    };
}

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

// Version-dependent file vtable accessors.
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

inline const char *atomDylibPath(AtomPtr a) {
    const char *n = dylibInstallName(atomDylibFileInfo(a));
    return n ? n : filePath(atomFile(a));
}

// Classic ld64 types.
namespace classic {

// Atom vtable (ARM64 ABI: 2 destructor slots at top).
namespace vtable {
    inline constexpr size_t kDestructorD1  = 0x00;
    inline constexpr size_t kDestructorD0  = 0x08;
    inline constexpr size_t kFile          = 0x10;
    inline constexpr size_t kOriginalFile  = 0x18;
    inline constexpr size_t kTUSource      = 0x20;
    inline constexpr size_t kName          = 0x28;
    inline constexpr size_t kObjectAddr    = 0x30;
    inline constexpr size_t kSize          = 0x38;
    inline constexpr size_t kCopyContent   = 0x40;
    inline constexpr size_t kFixupsBegin   = 0x60;
    inline constexpr size_t kFixupsEnd     = 0x68;
}

// Atom object layout. Bitfield block at +0x1F packs definition, scope,
// contentType, mode, etc. Read as an unaligned u32 via memcpy.
namespace atom {
    inline constexpr size_t kVptr              = 0x00;
    inline constexpr size_t kSectionRef        = 0x08;  // const Section*
    inline constexpr size_t kAddress           = 0x10;  // u64 (sect offset or final VM addr)
    inline constexpr size_t kOutputSymbolIndex = 0x18;  // u32 (UINT32_MAX = unbound)
    inline constexpr size_t kAlignmentModulus  = 0x1C;  // u16
    inline constexpr size_t kAlignmentPow2     = 0x1E;  // u8
    inline constexpr size_t kBitfields         = 0x1F;  // 5 packed bytes (unaligned u32)
    inline constexpr size_t kSize              = 0x28;

    // Scope occupies bits 18..19 of the u32 at +0x1F.
    inline constexpr uint32_t kScopeShift = 18;
    inline constexpr uint32_t kScopeMask  = 0x3;
}

namespace section {
    inline constexpr size_t kSegmentName = 0x00;  // const char*
    inline constexpr size_t kSectionName = 0x08;  // const char*
    inline constexpr size_t kType        = 0x10;  // Section::Type (i32)
    inline constexpr size_t kHidden      = 0x14;  // bool
    inline constexpr size_t kSize        = 0x18;
}

// ld::Internal layout. Has a vtable at +0x00; sections and dylibs are
// std::vector<> triples (begin/end/cap, 8-byte pointer stride).
namespace internal {
    inline constexpr size_t kVptr                 = 0x00;
    inline constexpr size_t kSectionsBegin        = 0x08;   // FinalSection**
    inline constexpr size_t kSectionsEnd          = 0x10;
    inline constexpr size_t kSectionsCapacity     = 0x18;
    inline constexpr size_t kDylibsBegin          = 0x20;   // ld::dylib::File**
    inline constexpr size_t kDylibsEnd            = 0x28;
    inline constexpr size_t kDylibsCapacity       = 0x30;
    inline constexpr size_t kIndirectBindingTable = 0x128;  // const Atom**
}

// FinalSection : Section. Inherits the (segName,sectName,type,hidden)
// prefix; adds atoms vector + VM placement.
namespace final_section {
    inline constexpr size_t kAtomsBegin  = 0x18;  // const Atom**
    inline constexpr size_t kAtomsEnd    = 0x20;
    inline constexpr size_t kAtomsCap    = 0x28;
    inline constexpr size_t kAddress     = 0x30;  // u64 final VM addr
    inline constexpr size_t kSize        = 0x40;  // u64 section size
}

// dylib::File. installPath is the LC_ID_DYLIB string. Flag bytes are
// mutated by ld::passes::dylibs::doPass when -dead_strip_dylibs fires.
namespace dylib_file {
    inline constexpr size_t kVptr              = 0x00;
    inline constexpr size_t kInstallPath       = 0x28;  // const char*
    inline constexpr size_t kDeadStripable     = 0x44;  // u8
    inline constexpr size_t kExplicitlyLinked  = 0x45;  // u8
    inline constexpr size_t kForcedWeakLinked  = 0x48;  // u8
    inline constexpr size_t kImplicitLink      = 0x4B;  // u8 (set by doPass for survivors)
}

enum class Scope : uint8_t {
    TranslationUnit  = 0,
    LinkageUnit      = 1,
    Global           = 2,
};

enum class Definition : uint8_t {
    Regular          = 0,
    Tentative        = 1,  // tentative (common) definition
    Absolute         = 2,  // absolute symbol, not relocatable
    Proxy            = 3,  // placeholder for imported symbol
};

enum class Combine : uint8_t {
    Never                = 0,
    ByName               = 1,
    ByNameAndContent     = 2,
    ByNameAndReferences  = 3,
};

enum class SymbolTableInclusion : uint8_t {
    NotIn                       = 0,
    NotInFinalLinkedImages      = 1,
    In                          = 2,
    InAndNeverStrip             = 3,
    InAsAbsolute                = 4,
    InWithRandomAutoStripLabel  = 5,
};

enum class WeakImportState : uint8_t {
    Unset = 0,
    True  = 1,
    False = 2,
};

enum class ContentType : uint8_t {
    Unclassified           = 0,
    ZeroFill               = 1,
    CString                = 2,
    CFI                    = 3,
    LSDA                   = 4,
    SectionStart           = 5,
    SectionEnd             = 6,
    BranchIsland           = 7,
    LazyPointer            = 8,
    Stub                   = 9,
    NonLazyPointer         = 10,
    LazyDylibPointer       = 11,
    StubHelper             = 12,
    InitializerPointers    = 13,
    TerminatorPointers     = 14,
    LTOtemporary           = 15,
    Resolver               = 16,
    TLV                    = 17,
    TLVZeroFill            = 18,
    TLVInitialValue        = 19,
    TLVInitializerPointers = 20,
    TLVPointer             = 21,
};

enum class SectionType : uint8_t {
    Unclassified             = 0,
    Code                     = 1,
    PageZero                 = 2,
    ImportProxies            = 3,
    LinkEdit                 = 4,
    MachHeader               = 5,
    Stack                    = 6,
    Literal4                 = 7,
    Literal8                 = 8,
    Literal16                = 9,
    Constants                = 10,
    TempLTO                  = 11,
    TempAlias                = 12,
    CString                  = 13,
    NonStdCString            = 14,
    CStringPointer           = 15,
    UTF16Strings             = 16,
    CFString                 = 17,
    ObjC1Classes             = 18,
    CFI                      = 19,
    LSDA                     = 20,
    DtraceDOF                = 21,
    UnwindInfo               = 22,
    ObjCClassRefs            = 23,
    ObjC2CategoryList        = 24,
    ObjC2ClassList           = 25,
    ZeroFill                 = 26,
    TentativeDefs            = 27,
    LazyPointer              = 28,
    Stub                     = 29,
    StubObjC                 = 30,
    NonLazyPointer           = 31,
    DyldInfo                 = 32,
    LazyDylibPointer         = 33,
    StubHelper               = 34,
    InitializerPointers      = 35,
    TerminatorPointers       = 36,
    StubClose                = 37,
    LazyPointerClose         = 38,
    AbsoluteSymbols          = 39,
    ThreadStarts             = 40,
    ChainStarts              = 41,
    TLVDefs                  = 42,
    TLVZeroFill              = 43,
    TLVInitialValues         = 44,
    TLVInitializerPointers   = 45,
    TLVPointers              = 46,
    FirstSection             = 47,
    LastSection              = 48,
    Debug                    = 49,
    SectCreate               = 50,
    InitOffsets              = 51,
    Interposing              = 52,
    RebaseRLE                = 53,
};

enum class Cluster : uint8_t {
    k1of1 = 0,
    k1of2 = 1, k2of2 = 2,
    k1of3 = 3, k2of3 = 4, k3of3 = 5,
    k1of4 = 6, k2of4 = 7, k3of4 = 8, k4of4 = 9,
    k1of5 = 10, k2of5 = 11, k3of5 = 12, k4of5 = 13, k5of5 = 14,
};

using AtomPtr          = const void *;
using SectionPtr       = const void *;
using FinalSectionPtr  = const void *;
using InternalPtr      = const void *;
using DylibFilePtr     = const void *;
using FilePtr          = const void *;

template <typename R>
inline R atomVCall(AtomPtr a, size_t slot) {
    const void *vt = readPtr(a, atom::kVptr);
    using Fn = R (*)(const void *);
    Fn fn; __builtin_memcpy(&fn, static_cast<const uint8_t *>(vt) + slot, sizeof(fn));
    return fn(a);
}

inline FilePtr    atomFile(AtomPtr a)      { return a ? atomVCall<const void *>(a, vtable::kFile)       : nullptr; }
inline FilePtr    atomOriginalFile(AtomPtr a){ return a ? atomVCall<const void *>(a, vtable::kOriginalFile) : nullptr; }
inline const char *atomName(AtomPtr a)     { return a ? atomVCall<const char *>(a, vtable::kName)       : nullptr; }
inline uint64_t   atomObjectAddr(AtomPtr a){ return a ? atomVCall<uint64_t>(a, vtable::kObjectAddr)     : 0; }
inline uint64_t   atomSize(AtomPtr a)      { return a ? atomVCall<uint64_t>(a, vtable::kSize)           : 0; }

inline const ld::classic::Fixup *atomFixupsBegin(AtomPtr a) {
    return a ? atomVCall<const ld::classic::Fixup *>(a, vtable::kFixupsBegin) : nullptr;
}
inline const ld::classic::Fixup *atomFixupsEnd(AtomPtr a) {
    return a ? atomVCall<const ld::classic::Fixup *>(a, vtable::kFixupsEnd) : nullptr;
}

// Direct-field accessors for the stable Atom prefix.
inline SectionPtr atomSection(AtomPtr a)            { return a ? readPtr(a, atom::kSectionRef) : nullptr; }
inline uint64_t   atomAddress(AtomPtr a)            { return a ? readU64(a, atom::kAddress) : 0; }
inline uint32_t   atomOutputSymbolIndex(AtomPtr a)  { return a ? readU32(a, atom::kOutputSymbolIndex) : 0; }
inline uint16_t   atomAlignmentModulus(AtomPtr a)   { return a ? readU16(a, atom::kAlignmentModulus) : 0; }
inline uint8_t    atomAlignmentPow2(AtomPtr a)      { return a ? readU8(a, atom::kAlignmentPow2) : 0; }

// Packed bitfield word at +0x1F (unaligned u32).
inline uint32_t atomBitfieldWord(AtomPtr a) {
    if (!a) return 0;
    uint32_t v;
    __builtin_memcpy(&v, static_cast<const uint8_t *>(a) + atom::kBitfields, sizeof(v));
    return v;
}
inline Scope atomScope(AtomPtr a) {
    return static_cast<Scope>(
        (atomBitfieldWord(a) >> atom::kScopeShift) & atom::kScopeMask);
}

inline Span<AtomPtr> finalSectionAtoms(FinalSectionPtr fs) {
    if (!fs) return {};
    return Span<AtomPtr>{
        static_cast<const AtomPtr *>(readPtr(fs, final_section::kAtomsBegin)),
        static_cast<const AtomPtr *>(readPtr(fs, final_section::kAtomsEnd))
    };
}
inline uint64_t finalSectionAddress(FinalSectionPtr fs) {
    return fs ? readU64(fs, final_section::kAddress) : 0;
}
inline uint64_t finalSectionSize(FinalSectionPtr fs) {
    return fs ? readU64(fs, final_section::kSize) : 0;
}
inline const char *finalSectionSegName(FinalSectionPtr fs) {
    return fs ? static_cast<const char *>(readPtr(fs, section::kSegmentName)) : nullptr;
}
inline const char *finalSectionSectName(FinalSectionPtr fs) {
    return fs ? static_cast<const char *>(readPtr(fs, section::kSectionName)) : nullptr;
}

inline Span<FinalSectionPtr> internalSections(InternalPtr ip) {
    if (!ip) return {};
    return Span<FinalSectionPtr>{
        static_cast<const FinalSectionPtr *>(readPtr(ip, internal::kSectionsBegin)),
        static_cast<const FinalSectionPtr *>(readPtr(ip, internal::kSectionsEnd))
    };
}
inline Span<DylibFilePtr> internalDylibs(InternalPtr ip) {
    if (!ip) return {};
    return Span<DylibFilePtr>{
        static_cast<const DylibFilePtr *>(readPtr(ip, internal::kDylibsBegin)),
        static_cast<const DylibFilePtr *>(readPtr(ip, internal::kDylibsEnd))
    };
}

inline const char *dylibInstallPath(DylibFilePtr df) {
    return df ? static_cast<const char *>(readPtr(df, dylib_file::kInstallPath)) : nullptr;
}
inline bool dylibIsDeadStripable(DylibFilePtr df) {
    return df && readU8(df, dylib_file::kDeadStripable) != 0;
}
inline bool dylibIsExplicitlyLinked(DylibFilePtr df) {
    return df && readU8(df, dylib_file::kExplicitlyLinked) != 0;
}
inline bool dylibIsImplicitLink(DylibFilePtr df) {
    return df && readU8(df, dylib_file::kImplicitLink) != 0;
}
inline bool dylibIsForcedWeakLinked(DylibFilePtr df) {
    return df && readU8(df, dylib_file::kForcedWeakLinked) != 0;
}

template <typename Fn>
inline void forEachAtom(InternalPtr ip, Fn fn) {
    auto secs = internalSections(ip);
    for (size_t i = 0, n = secs.size(); i < n; ++i) {
        FinalSectionPtr fs = secs[i];
        if (!fs) continue;
        auto atoms = finalSectionAtoms(fs);
        for (size_t j = 0, m = atoms.size(); j < m; ++j) {
            if (atoms[j] && !fn(fs, atoms[j])) return;
        }
    }
}

template <typename Fn>
inline void forEachDylib(InternalPtr ip, Fn fn) {
    auto dys = internalDylibs(ip);
    for (size_t i = 0, n = dys.size(); i < n; ++i) {
        if (dys[i] && !fn(dys[i])) return;
    }
}

} // namespace classic

} // namespace ld
#endif
