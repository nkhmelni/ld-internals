// Copyright (c) 2026 Nikita Hmelnitkii. MIT License - see LICENSE.
//
// Layout.h - navigation from LinkeditBuilder::build() to atoms
//
// Navigation chain:
//   build(Error*, LinkeditBuilder*, Linkedit*)
//     -> LinkeditBuilder+0x10     -> LayoutExecutable*
//       -> Layout+0x120/0x128     -> segments begin/end
//         -> Segment+0x38 or 0x40 -> sections begin/end
//           -> Section+0x60/0x68  -> atoms begin/end
//             -> Atom vtable+0x58 -> fixups()
//
// Version differences:
//   SegmentLayout stride:       0x50 (ld-1115) vs 0x58 (ld-1230)
//   SegmentLayout sections:     +0x38/+0x40 (ld-1115) vs +0x40/+0x48 (ld-1230)
//   SectionLayout:              0x88 (both), atoms at +0x60/+0x68 (both)
//   LayoutExecutable segments:  +0x120/+0x128 (both)

#ifndef LD_LAYOUT_H
#define LD_LAYOUT_H

#include "Version.h"
#include "Atom.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ld {

// version-dependent offsets - call layoutConstantsFor() once and cache.

struct LayoutConstants {
    bool   valid;
    size_t segmentStride;
    size_t segSectionsBegin;
    size_t segSectionsEnd;
    size_t dynamicFixupPool;
    size_t layoutIndirectBegin;
    size_t layoutIndirectEnd;
    size_t passFilesBegin;       // consolidator + offset
    size_t passFilesEnd;
};

// returns {.valid=false} for unrecognized or classic (ld64) versions.
inline LayoutConstants layoutConstantsFor(const LinkerVersion &v) {
    if (!v.isPrime()) return {false, 0, 0, 0, 0, 0, 0, 0, 0};

    if (v.major >= 1200) {
        return {true, 0x58, 0x40, 0x48, 0x1F8, 0x3108, 0x3110, 0x878, 0x880};
    }
    return {true, 0x50, 0x38, 0x40, 0x1B8, 0x3190, 0x3198, 0x788, 0x790};
}

// section field offsets - stable across both ld-prime versions (stride 0x88)

namespace section {
    inline constexpr size_t kStride      = 0x88;
    inline constexpr size_t kNamePtr     = 0x00;
    inline constexpr size_t kNameLen     = 0x08;
    inline constexpr size_t kSegNamePtr  = 0x10;
    inline constexpr size_t kSegNameLen  = 0x18;
    inline constexpr size_t kContentType = 0x2C;
    inline constexpr size_t kAlignment   = 0x30;
    inline constexpr size_t kAddress     = 0x38;
    inline constexpr size_t kSize        = 0x40;
    inline constexpr size_t kFileOffset  = 0x48;
    inline constexpr size_t kAtomsBegin  = 0x60;
    inline constexpr size_t kAtomsEnd    = 0x68;
    inline constexpr size_t kSectionRO   = 0x78;
    inline constexpr size_t kSectionIdx  = 0x80;

    inline constexpr uint8_t kTypeProxy    = 0x12;
    inline constexpr uint8_t kTypeAlias    = 0x0C;
    inline constexpr uint8_t kTypeZerofill = 0x01;
    inline constexpr uint8_t kTypeStub     = 0x13;
}

// SectionRO_1 - inline metadata from .o files

namespace SectionRO {
    inline constexpr size_t kContentType = 0x00;
    inline constexpr size_t kAlignment   = 0x04;
    inline constexpr size_t kSegName     = 0x08;  // char[16]
    inline constexpr size_t kSectName    = 0x1A;  // char[16]
}

// segment field offsets - name at +0x00/+0x08 stable, sections version-dependent

namespace segment {
    inline constexpr size_t kNamePtr = 0x00;
    inline constexpr size_t kNameLen = 0x08;
}

// LayoutExecutable field offsets - segments vector at +0x120 stable

namespace layout {
    inline constexpr size_t kOptions      = 0x08;
    inline constexpr size_t kConsolidator = 0x108;
    inline constexpr size_t kSegmentsBegin   = 0x120;
    inline constexpr size_t kSegmentsEnd     = 0x128;
    inline constexpr size_t kSegmentsCapacity = 0x130;
    inline constexpr size_t kArch         = 0x188;
}

// LinkeditBuilder::build() calling convention (ARM64):
//   x0 = mach_o::Error*  (hidden struct return)
//   x1 = LinkeditBuilder* (this)
//   x2 = Linkedit*        (heap-allocated output)

namespace builder {
    inline constexpr size_t kOptions      = 0x00;
    inline constexpr size_t kArch         = 0x08;
    inline constexpr size_t kLayout       = 0x10;  // LayoutExecutable* - stable
    inline constexpr size_t kAtomGroups   = 0x18;
    inline constexpr size_t kPlacement    = 0x20;
    inline constexpr size_t kSegmentsPtr  = 0x48;
    inline constexpr size_t kDylibMapping = 0x50;
    inline constexpr size_t kIndirectBase = 0x58;
    inline constexpr size_t kIndirectCount = 0x60;
}

// AtomPlacement - pointed to by Atom+0x10

namespace placement {
    inline constexpr size_t kOffset       = 0x00;
    inline constexpr size_t kSectionIndex = 0x0C;
}

// navigation helpers

inline const void *builderGetLayout(const void *bldr) {
    return readPtr(bldr, builder::kLayout);
}

inline const uint8_t *layoutSegmentsBegin(const void *lay) {
    return static_cast<const uint8_t *>(readPtr(lay, layout::kSegmentsBegin));
}

inline const uint8_t *layoutSegmentsEnd(const void *lay) {
    return static_cast<const uint8_t *>(readPtr(lay, layout::kSegmentsEnd));
}

inline const char *segmentName(const void *seg) {
    return static_cast<const char *>(readPtr(seg, segment::kNamePtr));
}

inline const uint8_t *segSectionsBegin(const void *seg, const LayoutConstants &lc) {
    return static_cast<const uint8_t *>(readPtr(seg, lc.segSectionsBegin));
}

inline const uint8_t *segSectionsEnd(const void *seg, const LayoutConstants &lc) {
    return static_cast<const uint8_t *>(readPtr(seg, lc.segSectionsEnd));
}

inline const char *sectionName(const void *sec) {
    return static_cast<const char *>(readPtr(sec, section::kNamePtr));
}

inline size_t sectionNameLen(const void *sec) {
    return static_cast<size_t>(readU64(sec, section::kNameLen));
}

inline const char *sectionSegName(const void *sec) {
    return static_cast<const char *>(readPtr(sec, section::kSegNamePtr));
}

inline AtomPtr const *sectionAtomsBegin(const void *sec) {
    return static_cast<AtomPtr const *>(readPtr(sec, section::kAtomsBegin));
}

inline AtomPtr const *sectionAtomsEnd(const void *sec) {
    return static_cast<AtomPtr const *>(readPtr(sec, section::kAtomsEnd));
}

// segment name matching

inline bool segmentNameMatches(const void *seg, const char *name, size_t nameLen) {
    size_t segLen = static_cast<size_t>(readU64(seg, segment::kNameLen));
    if (segLen != nameLen) return false;
    return memcmp(segmentName(seg), name, nameLen) == 0;
}

// fast path for 6-char names (__DATA, __TEXT) - avoids memcmp
inline bool segmentNameIs(const void *seg, const char (&name)[7]) {
    const char *n = segmentName(seg);
    if (!n) return false;
    uint32_t w32a, w32b;
    __builtin_memcpy(&w32a, n, 4);
    __builtin_memcpy(&w32b, name, 4);
    if (w32a != w32b) return false;
    uint16_t w16a, w16b;
    __builtin_memcpy(&w16a, n + 4, 2);
    __builtin_memcpy(&w16b, name + 4, 2);
    return w16a == w16b;
}

inline bool sectionNameIs(const void *sec, const char *name, size_t nameLen) {
    if (sectionNameLen(sec) != nameLen) return false;
    return memcmp(sectionName(sec), name, nameLen) == 0;
}

// find a section by segment + section name

inline const void *findSection(const void *layoutExe,
                               const LayoutConstants &lc,
                               const char *segName, size_t segNameLen,
                               const char *sectName, size_t sectNameLen) {
    const uint8_t *segBegin = layoutSegmentsBegin(layoutExe);
    const uint8_t *segEnd   = layoutSegmentsEnd(layoutExe);

    for (const uint8_t *seg = segBegin; seg < segEnd; seg += lc.segmentStride) {
        if (!segmentNameMatches(seg, segName, segNameLen)) continue;

        const uint8_t *secBegin = segSectionsBegin(seg, lc);
        const uint8_t *secEnd   = segSectionsEnd(seg, lc);

        for (const uint8_t *sec = secBegin; sec < secEnd; sec += section::kStride) {
            if (sectionNameIs(sec, sectName, sectNameLen))
                return sec;
        }
    }
    return nullptr;
}

// convenience overloads

inline const void *findSection(const void *layoutExe,
                               const LayoutConstants &lc,
                               const char (&segName)[7],
                               const char *sectName, size_t sectNameLen) {
    return findSection(layoutExe, lc, segName, 6, sectName, sectNameLen);
}

inline const void *findSection(const void *layoutExe,
                               const LayoutConstants &lc,
                               const char *segName,
                               const char *sectName) {
    return findSection(layoutExe, lc, segName, strlen(segName),
                       sectName, strlen(sectName));
}

// validation - call once at hook entry before navigating.
// returns false if the pointers look wrong (version mismatch, etc.).
inline bool validateBuilder(const void *bldr, const LayoutConstants &lc) {
    if (!bldr || !lc.valid) return false;

    const void *lay = builderGetLayout(bldr);
    if (!lay) return false;

    const uint8_t *segBegin = layoutSegmentsBegin(lay);
    const uint8_t *segEnd   = layoutSegmentsEnd(lay);
    if (!segBegin || !segEnd || segBegin > segEnd) return false;

    size_t segCount = (segEnd - segBegin) / lc.segmentStride;
    if (segCount == 0 || segCount > 100) return false;

    return true;
}

// fixup target resolution - mirrors directTargetNoFollow from Fixup.cpp

inline AtomPtr resolveFixupTarget(const Fixup &f, AtomPtr containingAtom,
                                  const LayoutConstants &lc) {
    const void *file = atomFile(containingAtom);
    if (!file) return nullptr;

    uint32_t targetIdx = f.targetIndex();

    if (f.binding() == 0) {
        const void *const *begin = static_cast<const void *const *>(
            readPtr(file, AtomFile::kAtomsBegin));
        const void *const *end = static_cast<const void *const *>(
            readPtr(file, AtomFile::kAtomsEnd));
        if (!begin || !end) return nullptr;
        if (targetIdx >= static_cast<size_t>(end - begin)) return nullptr;
        return begin[targetIdx];
    }

    if (!lc.valid || lc.passFilesBegin == 0) return nullptr;

    const void *consolidator = readPtr(file, AtomFile::kConsolidator);
    if (!consolidator) return nullptr;

    const void *const *passBegin = static_cast<const void *const *>(
        readPtr(consolidator, lc.passFilesBegin));
    const void *const *passEnd = static_cast<const void *const *>(
        readPtr(consolidator, lc.passFilesEnd));
    if (!passBegin || !passEnd) return nullptr;

    uint8_t passIdx = f.passIndex();
    if (passIdx >= static_cast<size_t>(passEnd - passBegin)) return nullptr;

    const void *passFile = passBegin[passIdx];
    if (!passFile) return nullptr;

    const void *const *pBegin = static_cast<const void *const *>(
        readPtr(passFile, AtomFile::kAtomsBegin));
    const void *const *pEnd = static_cast<const void *const *>(
        readPtr(passFile, AtomFile::kAtomsEnd));
    if (!pBegin || !pEnd) return nullptr;
    if (targetIdx >= static_cast<size_t>(pEnd - pBegin)) return nullptr;

    return pBegin[targetIdx];
}

// same as above, but follows alias chains via atom+0x18
inline AtomPtr resolveFixupTargetFollowAliases(const Fixup &f,
                                               AtomPtr containingAtom,
                                               const LayoutConstants &lc) {
    AtomPtr target = resolveFixupTarget(f, containingAtom, lc);
    while (target && (atomFlags(target) & AtomFlags::kAlias)) {
        AtomPtr next = static_cast<AtomPtr>(readPtr(target, atom::kAliasTarget));
        if (!next) break;
        target = next;
    }
    return target;
}

} // namespace ld

#endif
