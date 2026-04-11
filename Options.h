// Copyright (c) 2026 Nikita Hmelnitkii. MIT License - see LICENSE.
//
// Options.h - raw offsets for ld::Options CLI state. Version-aware wrappers
// live in Layout.h. The Options struct has no vtable; layout drifts across
// ld-prime versions, so all accessors go through LayoutConstants.

#ifndef LD_OPTIONS_H
#define LD_OPTIONS_H

#include "Atom.h"   // readU8/readU16/readU32/writeU16 helpers

#include <cstddef>
#include <cstdint>

namespace ld {

// Opaque handle - Options has no vtable; access is by specific offsets only.
using OptionsPtr = const void *;
using MutableOptionsPtr = void *;

// Options::Optimizations sub-struct (ld-1221+). Packed u16 flag region at
// base+0x18..+0x44. Offsets below are RELATIVE to the sub-struct base; add
// LayoutConstants::optionsOptimizationsBase for the absolute Options offset.
// Byte-identical across ld-1221.4/1230.1/1266.8.
namespace Optimizations {

    inline constexpr size_t kDeadStrip                   = 0x18;  // u16 - `-dead_strip`
    inline constexpr size_t kLtoLibrarySet               = 0x1A;  // u16 - `-lto_library` present
    inline constexpr size_t kDeadStripDylibs             = 0x1C;  // u16 - `-dead_strip_dylibs`
    inline constexpr size_t kOrderInits                  = 0x1E;  // u16 - `-no_order_inits`
    inline constexpr size_t kImplicitDylibs              = 0x20;  // u16 - `-no_implicit_dylibs`
    inline constexpr size_t kAllowDeadDuplicates         = 0x22;  // u16 - `-allow_dead_duplicates`
    inline constexpr size_t kMaxCodeDedupPassesSet       = 0x24;  // u16 - has_value
    inline constexpr size_t kNoBranchIslands             = 0x26;  // u16 - `-no_branch_islands`
    inline constexpr size_t kDtraceDof                   = 0x28;  // u16 - `-no_dtrace_dof`
    inline constexpr size_t kDedupAllowObjcAddrRef       = 0x2A;  // u16
    inline constexpr size_t kDedupAllowSwiftAddrRef      = 0x2C;  // u16
    inline constexpr size_t kDedupAllowBlockLiteralRef   = 0x2E;  // u16
    inline constexpr size_t kDedupAllowCppVtableRef      = 0x30;  // u16
    inline constexpr size_t kDeduplicate                 = 0x32;  // u16 - `-deduplicate` / `-no_deduplicate`
    inline constexpr size_t kMaxCodeDedupPassesValue     = 0x34;  // u32 - dword value
    inline constexpr size_t kMergedLibrariesHookValue    = 0x3C;  // u32
    inline constexpr size_t kAddMergeableDebugHook       = 0x3E;  // u16
    inline constexpr size_t kMergeZeroFillSections       = 0x40;  // u16 - `-merge_zero_fill_sections`
    inline constexpr size_t kDebugVariant                = 0x42;  // u16
    inline constexpr size_t kRemoveSwiftReflection       = 0x44;  // u16
    inline constexpr size_t kOrderFileVector             = 0x48;  // vector<> - std::vector begin ptr
    inline constexpr size_t kLtoLibrary                  = 0xC0;  // CString (ptr, len)
    inline constexpr size_t kObjectPathLto               = 0xD0;  // CString
    inline constexpr size_t kCachePathLto                = 0xE0;  // CString
    inline constexpr size_t kNoTreatLtoCrossrefs         = 0xF0;  // u16
    inline constexpr size_t kPruneIntervalLtoValue       = 0xF4;  // u32
    inline constexpr size_t kPruneIntervalLtoSet         = 0xF8;  // u8  - has_value
    inline constexpr size_t kPruneAfterLtoValue          = 0xFC;  // u32
    inline constexpr size_t kPruneAfterLtoSet            = 0x100; // u8
    inline constexpr size_t kMaxRelCacheSizeLtoValue     = 0x104; // u32
    inline constexpr size_t kMaxRelCacheSizeLtoSet       = 0x108; // u8
    inline constexpr size_t kMcpu                        = 0x128; // CString
    inline constexpr size_t kCacheDir                    = 0x138; // CString
}

// Byte-granular read/write. The parser writes u16 but the arm64 reader uses
// ldrb (low byte only); byte semantics avoid clobbering neighbors at odd
// offsets (e.g. ld-1115 flag at 0x48D adjacent to another u16 at 0x48F).

inline bool optionsReadFlag(OptionsPtr opts, size_t offset) {
    if (!opts || offset == 0) return false;
    return readU8(opts, offset) != 0;
}

inline void optionsWriteFlag(MutableOptionsPtr opts, size_t offset, bool enable) {
    if (!opts || offset == 0) return;
    writeU8(opts, offset, enable ? uint8_t{1} : uint8_t{0});
}

// Per-version absolute offsets.
namespace options_offsets {

    // ld-1115.7.3 - flat layout, packed region +0x478..+0x4b6
    struct Prime_1115 {
        static constexpr size_t kDeadStrip              = 0x480;  // u16
        static constexpr size_t kAllowDeadDuplicates    = 0x48B;  // u16
        static constexpr size_t kDeadStripDylibs        = 0x48D;  // u16 - target
        static constexpr size_t kNoZeroFillSections     = 0x48F;  // u16
        static constexpr size_t kMergeZeroFillSections  = 0x493;  // u16
        static constexpr size_t kNoBranchIslands        = 0x497;  // u16
    };

    // ld-1221.4 / ld-1230.1 - Optimizations at +0x4f0
    struct Prime_1221 {
        static constexpr size_t kOptimizationsBase      = 0x4F0;
        static constexpr size_t kDeadStrip              = 0x4F0 + Optimizations::kDeadStrip;              // 0x508
        static constexpr size_t kDeadStripDylibs        = 0x4F0 + Optimizations::kDeadStripDylibs;        // 0x50C - target
        static constexpr size_t kAllowDeadDuplicates    = 0x4F0 + Optimizations::kAllowDeadDuplicates;    // 0x512
        static constexpr size_t kMergeZeroFillSections  = 0x4F0 + Optimizations::kMergeZeroFillSections;  // 0x530
    };

    // ld-1266.8 - Optimizations shifted to +0x510
    struct Prime_1266 {
        static constexpr size_t kOptimizationsBase      = 0x510;
        static constexpr size_t kDeadStrip              = 0x510 + Optimizations::kDeadStrip;              // 0x528
        static constexpr size_t kDeadStripDylibs        = 0x510 + Optimizations::kDeadStripDylibs;        // 0x52C - target
        static constexpr size_t kAllowDeadDuplicates    = 0x510 + Optimizations::kAllowDeadDuplicates;    // 0x532
        static constexpr size_t kMergeZeroFillSections  = 0x510 + Optimizations::kMergeZeroFillSections;  // 0x550
    };
}

static_assert(options_offsets::Prime_1115::kDeadStripDylibs == 0x48D, "");
static_assert(options_offsets::Prime_1221::kDeadStripDylibs == 0x50C, "");
static_assert(options_offsets::Prime_1266::kDeadStripDylibs == 0x52C, "");

// Classic ld64 (ld64-820.1, frozen). Options is a plain data class at global
// scope, no vtable, no version drift. Flags are direct members; offsets come
// from Options.h declaration order with trailing-bool packing.
// Hook target: ld::passes::dylibs::doPass(Options&, Internal&) - Options is
// x0 at entry, no Consolidator indirection needed.
namespace classic {

// Unscoped to match ld64's `class Options { enum OutputKind ... };`.
enum OutputKind : uint32_t {
    kDynamicExecutable = 0,   // MH_EXECUTE + dyld
    kStaticExecutable  = 1,   // MH_EXECUTE, no dyld
    kDynamicLibrary    = 2,   // MH_DYLIB
    kDynamicBundle     = 3,   // MH_BUNDLE
    kObjectFile        = 4,   // MH_OBJECT
    kDyld              = 5,   // MH_DYLINKER
    kPreload           = 6,   // MH_PRELOAD
    kKextBundle        = 7,   // MH_KEXT_BUNDLE
};

// ld64-820.1 Options offsets (version is frozen - no drift).
namespace options_offsets {
    struct Ld64_820 {
        static constexpr size_t kDeadStripDylibs = 0x3BC;  // bool fDeadStripDylibs
    };
}

} // namespace classic

static_assert(classic::options_offsets::Ld64_820::kDeadStripDylibs == 0x3BC, "");
static_assert(classic::kDynamicExecutable == 0, "");
static_assert(classic::kDynamicLibrary    == 2, "");
static_assert(classic::kKextBundle        == 7, "");

} // namespace ld

#endif
