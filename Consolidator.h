// Copyright (c) 2026 Nikita Hmelnitkii. MIT License - see LICENSE.
//
// Consolidator.h - ld::AtomFileConsolidator layout.
//
// Central resolve-phase state: Options*, input AtomFiles, passFiles, input
// and output Dylib vectors. Allocated via `new(size, align=128)` in
// Linker::run(). Invariant: `Consolidator + 0x00 = Options const*` across
// every ld-prime version. Version-aware accessors live in Layout.h.

#ifndef LD_CONSOLIDATOR_H
#define LD_CONSOLIDATOR_H

#include "Atom.h"      // readPtr
#include "Options.h"
#include "Version.h"   // ld::LinkerVersion

#include <cstddef>

namespace ld {

using ConsolidatorPtr = const void *;
using MutableConsolidatorPtr = void *;

// AtomFileConsolidator fields stable across all ld-prime versions.
namespace consolidator {
    inline constexpr size_t kOptionsPtr          = 0x000;  // canonical Options const*
    inline constexpr size_t kOptionsPtrDuplicate = 0x080;  // back-pointer, diagnostic only
    inline constexpr size_t kInputDylibsBegin    = 0x2F0;  // ld::Dylib* scratch vector
    // Version-dependent fields live in LayoutConstants (Layout.h).
}

// buildDylibMapping filters the input scratch vector into a
// std::vector<DylibLoadInfo>. Element is a value-type with version-dependent
// stride - iterate via dylibLoadInfoStride(), not pointer arithmetic.
//
// Partial field map (verified in $_9<Dylib> and recursivePropagateWeakLink):
//   +0x00 Dylib*            back-ptr to input dylib (holds install name)
//   +0x08 Options const*    secondary back-ref
//   +0x10 uint8_t            attributes: weak / reexport / upward bits
//   +0x18 uint32_t           LC_LOAD_DYLIB ordinal (1-based)
namespace DylibLoadInfo {
    inline constexpr size_t kSize_1115  = 0x38;  // 56 bytes
    inline constexpr size_t kSize_1221  = 0x50;  // 80 bytes
    inline constexpr size_t kDylibPtr   = 0x00;  // ld::Dylib*
    inline constexpr size_t kOptionsPtr = 0x08;  // Options const*
    inline constexpr size_t kAttributes = 0x10;  // u8 bitfield
    inline constexpr size_t kOrdinal    = 0x18;  // u32 LC_LOAD_DYLIB ordinal
}

// Version-appropriate stride for iterating the output dylib vector.
inline size_t dylibLoadInfoStride(const ld::LinkerVersion &v) {
    return v.isPrime() && v.major >= 1200
         ? DylibLoadInfo::kSize_1221
         : DylibLoadInfo::kSize_1115;
}

// Per-version absolute offsets.
namespace consolidator_offsets {

    struct Prime_1115 {
        static constexpr size_t kInputDylibsBegin  = 0x2F0;
        static constexpr size_t kInputDylibsEnd    = 0x2F8;
        static constexpr size_t kOutputDylibsBegin = 0x728;  // feeds LC_LOAD_DYLIB
        static constexpr size_t kOutputDylibsEnd   = 0x730;
        static constexpr size_t kOutputDylibsCap   = 0x738;
        static constexpr size_t kTotalSize         = 0x800;
    };

    // ld-1221.4 / 1230.1 / 1266.8 - byte-identical.
    struct Prime_1221Plus {
        static constexpr size_t kInputDylibsBegin  = 0x2F0;
        static constexpr size_t kInputDylibsEnd    = 0x300;
        static constexpr size_t kOutputDylibsBegin = 0x7F0;  // feeds LC_LOAD_DYLIB
        static constexpr size_t kOutputDylibsEnd   = 0x7F8;
        static constexpr size_t kOutputDylibsCap   = 0x800;
    };
}

// Canonical Options* loader (+0x00, stable across all versions).
inline OptionsPtr consolidatorOptionsRaw(ConsolidatorPtr cons) {
    if (!cons) return nullptr;
    return readPtr(cons, consolidator::kOptionsPtr);
}

// Diagnostic-only alt path through the +0x80 duplicate slot.
inline OptionsPtr consolidatorOptionsAltRaw(ConsolidatorPtr cons) {
    if (!cons) return nullptr;
    return readPtr(cons, consolidator::kOptionsPtrDuplicate);
}

inline bool consolidatorOptionsPathsAgree(ConsolidatorPtr cons) {
    OptionsPtr a = consolidatorOptionsRaw(cons);
    if (!a) return false;
    OptionsPtr b = consolidatorOptionsAltRaw(cons);
    return a == b;
}

} // namespace ld

#endif
