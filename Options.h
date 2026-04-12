// Copyright (c) 2026 Nikita Hmelnitkii. MIT License - see LICENSE.
//
// Options.h - ld::Options field offsets.
// No vtable; layout varies per version.

#ifndef LD_OPTIONS_H
#define LD_OPTIONS_H

#include "Primitives.h"

#include <cstddef>
#include <cstdint>

namespace ld {

using OptionsPtr = const void *;
using MutableOptionsPtr = void *;

// Optimizations sub-struct (ld-1167+). Offsets are relative to the
// sub-struct base. Add LayoutConstants::optionsOptimizationsBase for
// the absolute position within Options.
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

// Byte-granular flag access. Uses byte semantics to avoid clobbering
// adjacent packed fields.
inline bool optionsReadFlag(OptionsPtr opts, size_t offset) {
    if (!opts || offset == 0) return false;
    return readU8(opts, offset) != 0;
}

inline void optionsWriteFlag(MutableOptionsPtr opts, size_t offset, bool enable) {
    if (!opts || offset == 0) return;
    writeU8(opts, offset, enable ? uint8_t{1} : uint8_t{0});
}

// Per-version absolute Options offsets.
namespace options_offsets {

    // ld-1115.7.3 - flat layout, packed region +0x478..+0x4B6.
    struct Prime_1115 {
        static constexpr size_t kDeadStrip              = 0x480;  // u16
        static constexpr size_t kAllowDeadDuplicates    = 0x48B;  // u16
        static constexpr size_t kDeadStripDylibs        = 0x48D;  // u16
        static constexpr size_t kNoZeroFillSections     = 0x48F;  // u16
        static constexpr size_t kMergeZeroFillSections  = 0x493;  // u16
        static constexpr size_t kNoBranchIslands        = 0x497;  // u16
    };

    // 1167.5 -- Optimizations at +0x4D0.
    struct Prime_1167 {
        static constexpr size_t kOptimizationsBase      = 0x4D0;
        static constexpr size_t kDeadStrip              = 0x4D0 + Optimizations::kDeadStrip;              // 0x4E8
        static constexpr size_t kDeadStripDylibs        = 0x4D0 + Optimizations::kDeadStripDylibs;        // 0x4EC - target
        static constexpr size_t kAllowDeadDuplicates    = 0x4D0 + Optimizations::kAllowDeadDuplicates;    // 0x4F2
        static constexpr size_t kMergeZeroFillSections  = 0x4D0 + Optimizations::kMergeZeroFillSections;  // 0x510
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
static_assert(options_offsets::Prime_1167::kDeadStripDylibs == 0x4EC, "");
static_assert(options_offsets::Prime_1221::kDeadStripDylibs == 0x50C, "");
static_assert(options_offsets::Prime_1266::kDeadStripDylibs == 0x52C, "");

// Classic ld64 (ld64-820.1). Plain data class, no vtable.
namespace classic {

enum OutputKind : uint32_t {
    kDynamicExecutable = 0,
    kStaticExecutable  = 1,
    kDynamicLibrary    = 2,
    kDynamicBundle     = 3,
    kObjectFile        = 4,
    kDyld              = 5,
    kPreload           = 6,
    kKextBundle        = 7,
};

enum UndefinedPolicy : uint32_t {
    kUndefinedError         = 0,
    kUndefinedWarning       = 1,
    kUndefinedSuppress      = 2,
    kUndefinedDynamicLookup = 3,
};

enum WeakRefTreatment : uint32_t {
    kWeakReferenceMismatchError   = 0,
    kWeakReferenceMismatchWeak    = 1,
    kWeakReferenceMismatchNonWeak = 2,
};

enum BitcodeMode : uint32_t {
    kBitcodeProcess = 0,
    kBitcodeAsData  = 1,
    kBitcodeMarker  = 2,
    kBitcodeStrip   = 3,
};

enum CommonsMode : uint32_t {
    kCommonsIgnoreDylibs         = 0,
    kCommonsOverriddenByDylibs   = 1,
    kCommonsConflictsDylibsError = 2,
};

// ld64-820.1 Options field offsets.
namespace options_offsets {
    struct Ld64_820 {
        static constexpr size_t kOutputFile            = 0x000;  // const char*
        static constexpr size_t kArchitecture          = 0x020;  // cpu_type_t
        static constexpr size_t kSubArchitecture       = 0x024;  // cpu_subtype_t
        static constexpr size_t kArchitectureName      = 0x030;  // const char*
        static constexpr size_t kOutputKind            = 0x038;  // enum OutputKind
        static constexpr size_t kDeadStrip             = 0x050;  // bool
        static constexpr size_t kDeadStripDylibs       = 0x3BC;  // bool
        static constexpr size_t kMergeZeroFillSections = 0x3E4;  // bool
        static constexpr size_t kAllowDeadDuplicates   = 0x452;  // bool
        static constexpr size_t kDebugInfoStripping    = 0x460;  // enum
        static constexpr size_t kPlatforms             = 0x470;  // ld::VersionSet
    };
}

inline bool classicOptionsDeadStripDylibs(const void *opts) {
    if (!opts) return false;
    return static_cast<const uint8_t *>(opts)[options_offsets::Ld64_820::kDeadStripDylibs] != 0;
}

inline void classicOptionsSetDeadStripDylibs(void *opts, bool enable) {
    if (!opts) return;
    static_cast<uint8_t *>(opts)[options_offsets::Ld64_820::kDeadStripDylibs] =
        enable ? uint8_t{1} : uint8_t{0};
}

using InternalPtr = const void *;

} // namespace classic

static_assert(classic::options_offsets::Ld64_820::kDeadStripDylibs == 0x3BC, "");
static_assert(classic::kDynamicExecutable == 0, "");
static_assert(classic::kDynamicLibrary    == 2, "");
static_assert(classic::kKextBundle        == 7, "");

} // namespace ld

#endif
