// Copyright (c) 2026 Nikita Hmelnitkii. MIT License - see LICENSE.
//
// Layout.h - Builder → Layout → Segments → Sections → Atoms → Fixups.
// Segment stride: 0x50 (ld-1115), 0x58 (ld-1221+). Sections: 0x88. Atoms: +0x60/+0x68.

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
    // segment layout
    size_t segmentStride;
    size_t segSectionsBegin;
    size_t segSectionsEnd;
    // DynamicAtomFile fields
    size_t dynamicFixupPool;
    size_t dynamicFileDylibFileInfo;
    size_t dynamicFileIsLTO;
    size_t dynamicFileLinkerOptions;
    size_t dynamicFileAltFileInfos;
    size_t dynamicFileLargeAddends;
    // layout indirects
    size_t layoutIndirectBegin;
    size_t layoutIndirectEnd;
    // consolidator passFiles
    size_t passFilesBegin;
    size_t passFilesEnd;
    // AtomFile vtable (shifted +0x08 in ld-1221 due to srcPath insertion)
    size_t fileVtableSrcPath;          // 0 in ld-1115 (not present)
    size_t fileVtableDylibFileInfo;
    size_t fileVtableDependencyInfos;
    size_t fileVtableLinkerOptions;
    size_t fileVtableLargeAddends;
    size_t fileVtableIsLTO;
    size_t fileVtableIsDynamic;
    size_t fileVtableAtomLOHs;         // 0 in ld-1115 (not present)
    size_t fileVtableFileLOHs;         // 0 in ld-1115 (not present)
    // AtomFile_1 fields (shifted +0x10 in ld-1221)
    size_t atomFile1DylibFileInfo;
    size_t atomFile1FixupPool;
    size_t atomFile1LargeAddends;
    size_t atomFile1LinkerOptions;
    size_t atomFile1DependencyInfos;
};

// returns {.valid=false} for unrecognized or classic (ld64) versions.
inline LayoutConstants layoutConstantsFor(const LinkerVersion &v) {
    LayoutConstants lc = {};
    if (!v.isPrime()) return lc;
    lc.valid = true;

    if (v.major >= 1266) {
        // ld-1266+: DynamicAtomFile fields shifted further
        lc.segmentStride = 0x58;
        lc.segSectionsBegin = 0x40; lc.segSectionsEnd = 0x48;
        lc.dynamicFixupPool = 0x218;
        lc.dynamicFileDylibFileInfo = 0x140;
        lc.dynamicFileIsLTO = 0x120;
        lc.dynamicFileLinkerOptions = 0x148;
        lc.dynamicFileAltFileInfos = 0x180;
        lc.dynamicFileLargeAddends = 0x230;
        lc.layoutIndirectBegin = 0x3108; lc.layoutIndirectEnd = 0x3110;
        lc.passFilesBegin = 0x878; lc.passFilesEnd = 0x880;
        lc.fileVtableSrcPath = 0x08;
        lc.fileVtableDylibFileInfo = 0x28;
        lc.fileVtableDependencyInfos = 0x30;
        lc.fileVtableLinkerOptions = 0x38;
        lc.fileVtableLargeAddends = 0x40;
        lc.fileVtableIsLTO = 0x48;
        lc.fileVtableIsDynamic = 0x18;
        lc.fileVtableAtomLOHs = 0x68;
        lc.fileVtableFileLOHs = 0x70;
        lc.atomFile1DylibFileInfo = 0x98;
        lc.atomFile1FixupPool = 0xA0;
        lc.atomFile1LargeAddends = 0xB0;
        lc.atomFile1LinkerOptions = 0xE0;
        lc.atomFile1DependencyInfos = 0xF8;
    } else if (v.major >= 1200) {
        // ld-1221/1230: srcPath() inserted at AtomFile vtable slot 1
        lc.segmentStride = 0x58;
        lc.segSectionsBegin = 0x40; lc.segSectionsEnd = 0x48;
        lc.dynamicFixupPool = 0x1F8;
        lc.dynamicFileDylibFileInfo = 0x138;
        lc.dynamicFileIsLTO = 0x118;
        lc.dynamicFileLinkerOptions = 0x140;
        lc.dynamicFileAltFileInfos = 0x178;
        lc.dynamicFileLargeAddends = 0x210;
        lc.layoutIndirectBegin = 0x3108; lc.layoutIndirectEnd = 0x3110;
        lc.passFilesBegin = 0x878; lc.passFilesEnd = 0x880;
        lc.fileVtableSrcPath = 0x08;
        lc.fileVtableDylibFileInfo = 0x28;
        lc.fileVtableDependencyInfos = 0x30;
        lc.fileVtableLinkerOptions = 0x38;
        lc.fileVtableLargeAddends = 0x40;
        lc.fileVtableIsLTO = 0x48;
        lc.fileVtableIsDynamic = 0x18;
        lc.fileVtableAtomLOHs = 0x68;
        lc.fileVtableFileLOHs = 0x70;
        lc.atomFile1DylibFileInfo = 0x98;
        lc.atomFile1FixupPool = 0xA0;
        lc.atomFile1LargeAddends = 0xB0;
        lc.atomFile1LinkerOptions = 0xE0;
        lc.atomFile1DependencyInfos = 0xF8;
    } else {
        // ld-1115
        lc.segmentStride = 0x50;
        lc.segSectionsBegin = 0x38; lc.segSectionsEnd = 0x40;
        lc.dynamicFixupPool = 0x1B8;
        lc.dynamicFileDylibFileInfo = 0x128;
        lc.dynamicFileIsLTO = 0x108;
        lc.dynamicFileLinkerOptions = 0x130;
        lc.dynamicFileAltFileInfos = 0x168;
        lc.dynamicFileLargeAddends = 0x1D0;
        lc.layoutIndirectBegin = 0x3190; lc.layoutIndirectEnd = 0x3198;
        lc.passFilesBegin = 0x788; lc.passFilesEnd = 0x790;
        lc.fileVtableSrcPath = 0;  // not present in ld-1115
        lc.fileVtableDylibFileInfo = 0x20;
        lc.fileVtableDependencyInfos = 0x28;
        lc.fileVtableLinkerOptions = 0x30;
        lc.fileVtableLargeAddends = 0x38;
        lc.fileVtableIsLTO = 0x40;
        lc.fileVtableIsDynamic = 0x10;
        lc.fileVtableAtomLOHs = 0;   // not present in ld-1115
        lc.fileVtableFileLOHs = 0;   // not present in ld-1115
        lc.atomFile1DylibFileInfo = 0x88;
        lc.atomFile1FixupPool = 0x90;
        lc.atomFile1LargeAddends = 0xA0;
        lc.atomFile1LinkerOptions = 0xD0;
        lc.atomFile1DependencyInfos = 0xE8;
    }
    return lc;
}

// section fields (stride 0x88, stable)

namespace section {
    inline constexpr size_t kStride      = 0x88;
    inline constexpr size_t kNamePtr     = 0x00;
    inline constexpr size_t kNameLen     = 0x08;
    inline constexpr size_t kSegNamePtr  = 0x10;
    inline constexpr size_t kSegNameLen  = 0x18;
    inline constexpr size_t kContentType = 0x2C;
    inline constexpr size_t kAlignment   = 0x30;  // u16: alignment power (ldrh)
    inline constexpr size_t kAddress     = 0x38;
    inline constexpr size_t kSize        = 0x40;
    inline constexpr size_t kFileOffset  = 0x48;
    inline constexpr size_t kAtomsBegin  = 0x60;
    inline constexpr size_t kAtomsEnd    = 0x68;
    inline constexpr size_t kSectionRO   = 0x78;
    inline constexpr size_t kSectionIdx  = 0x80;

    // NOTE: the contentType field at +0x2C stores Atom ContentType values
    // (ld::ContentType enum), NOT AtomKind values. Use ContentType::kGot,
    // ContentType::kStub, etc. for section type comparisons.
}

// SectionRO_1 - inline metadata from .o files (0x2C bytes)

namespace SectionRO {
    inline constexpr size_t kAlignment   = 0x00;  // u32: alignment power
    inline constexpr size_t kContentType = 0x04;  // u32: ContentType enum value
    inline constexpr size_t kSegName     = 0x08;  // char[18]
    inline constexpr size_t kSectName    = 0x1A;  // char[18]
    inline constexpr size_t kSize        = 0x2C;
}

// segment field offsets (ld-1115 stride 0x50, ld-1221+ stride 0x58).
// name + vm fields stable; sections vector is version-dependent.

namespace segment {
    inline constexpr size_t kNamePtr      = 0x00;
    inline constexpr size_t kNameLen      = 0x08;
    inline constexpr size_t kVMAddr       = 0x10;  // uint64: virtual memory address
    inline constexpr size_t kVMSize       = 0x18;  // uint64: aligned virtual memory size
    inline constexpr size_t kFileOff      = 0x20;  // uint32: file offset
    inline constexpr size_t kFileSize     = 0x24;  // uint32: file size
    inline constexpr size_t kInitProt     = 0x2C;  // uint8: VM_PROT (e.g. 5=RX, 3=RW)
    inline constexpr size_t kMaxProt      = 0x2D;  // uint8: VM_PROT
    inline constexpr size_t kSegmentOrder = 0x30;  // u32: sort key
    inline constexpr size_t kFixedAddr   = 0x38;  // u8: fixed-address flag (ld-1221+ only)
    // sections vector at +0x38/+0x40 (ld-1115) or +0x40/+0x48 (ld-1221+)
}

// LayoutExecutable fields

namespace layout {
    inline constexpr size_t kOptions      = 0x08;
    inline constexpr size_t kConsolidator = 0x108;
    inline constexpr size_t kSegmentsBegin   = 0x120;
    inline constexpr size_t kSegmentsEnd     = 0x128;
    inline constexpr size_t kSegmentsCapacity = 0x130;
    inline constexpr size_t kArch         = 0x188;
}

// LinkeditBuilder fields

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

// AtomPlacement (Atom+0x10)

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

inline uint8_t sectionContentType(const void *sec) {
    return readU8(sec, section::kContentType);
}

inline uint64_t sectionAddress(const void *sec) {
    return readU64(sec, section::kAddress);
}

inline uint64_t sectionSize(const void *sec) {
    return readU64(sec, section::kSize);
}

inline AtomPtr const *sectionAtomsBegin(const void *sec) {
    return static_cast<AtomPtr const *>(readPtr(sec, section::kAtomsBegin));
}

inline AtomPtr const *sectionAtomsEnd(const void *sec) {
    return static_cast<AtomPtr const *>(readPtr(sec, section::kAtomsEnd));
}

// segment field accessors

inline uint64_t segmentVMAddr(const void *seg) {
    return readU64(seg, segment::kVMAddr);
}

inline uint64_t segmentVMSize(const void *seg) {
    return readU64(seg, segment::kVMSize);
}

inline uint32_t segmentFileOff(const void *seg) {
    return readU32(seg, segment::kFileOff);
}

inline uint32_t segmentFileSize(const void *seg) {
    return readU32(seg, segment::kFileSize);
}

inline uint8_t segmentInitProt(const void *seg) {
    return readU8(seg, segment::kInitProt);
}

inline uint8_t segmentMaxProt(const void *seg) {
    return readU8(seg, segment::kMaxProt);
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
