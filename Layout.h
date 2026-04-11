// Copyright (c) 2026 Nikita Hmelnitkii. MIT License - see LICENSE.
//
// Layout.h - Builder -> LayoutExecutable -> Segments -> Sections -> Atoms -> Fixups.
// Segment stride: 0x50 (ld-1115), 0x58 (ld-1221+). Section stride: 0x88.
// Version-dependent offsets are packed into LayoutConstants.

#ifndef LD_LAYOUT_H
#define LD_LAYOUT_H

#include "Version.h"
#include "Atom.h"           // transitively: Fixup, Primitives, Mach, LinkEdit
#include "Options.h"
#include "Consolidator.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ld {

// Version-dependent offsets. Call layoutConstantsFor() once and cache.
// A zero offset means "not supported on this version"; accessors no-op.
struct LayoutConstants {
    bool   valid;

    // Segment layout.
    size_t segmentStride;
    size_t segSectionsBegin;
    size_t segSectionsEnd;

    // DynamicAtomFile fields.
    size_t dynamicFixupPool;
    size_t dynamicFileDylibFileInfo;
    size_t dynamicFileIsLTO;
    size_t dynamicFileLinkerOptions;
    size_t dynamicFileAltFileInfos;
    size_t dynamicFileLargeAddends;
    size_t dynamicFileFileLOHs;        // u64 packed records, 8B stride

    // LayoutExecutable indirects vector.
    size_t layoutIndirectBegin;
    size_t layoutIndirectEnd;

    // Consolidator pass-files vector.
    size_t passFilesBegin;
    size_t passFilesEnd;

    // AtomFile vtable - +0x08 shift on ld-1221+ (srcPath inserted at slot 1).
    size_t fileVtableSrcPath;
    size_t fileVtableDylibFileInfo;
    size_t fileVtableDependencyInfos;
    size_t fileVtableLinkerOptions;
    size_t fileVtableLargeAddends;
    size_t fileVtableIsLTO;
    size_t fileVtableIsDynamic;
    size_t fileVtableAtomLOHs;
    size_t fileVtableFileLOHs;

    // AtomFile_1 fields - +0x10 shift on ld-1221+.
    size_t atomFile1DylibFileInfo;
    size_t atomFile1FixupPool;
    size_t atomFile1LargeAddends;
    size_t atomFile1LinkerOptions;
    size_t atomFile1DependencyInfos;

    // Options flags - absolute offsets within Options.
    size_t optionsOptimizationsBase;
    size_t optionsDeadStrip;
    size_t optionsDeadStripDylibs;
    size_t optionsAllowDeadDuplicates;
    size_t optionsMergeZeroFillSections;

    // AtomFileConsolidator - dylib vectors and Options path navigation.
    size_t consolidatorOptions;              // always 0x00 on ld-prime
    size_t consolidatorInputDylibsBegin;     // always 0x2F0
    size_t consolidatorInputDylibsEnd;
    size_t consolidatorOutputDylibsBegin;    // feeds LC_LOAD_DYLIB
    size_t consolidatorOutputDylibsEnd;
    size_t consolidatorOutputDylibsCap;

    // LayoutExecutable - ld-1115 is flat; ld-1221+ inherits from
    // LayoutLinkedImage with very different offsets. Always use these
    // fields (not the `layout::` namespace constants, which are 1115-only).
    size_t layoutOptionsRef;         // 0x008 (1115) / 0x000 (1221+)
    size_t layoutConsolidatorRef;    // 0x108 / 0x0C8
    size_t layoutArch;               // 0x188 / 0x100
    size_t layoutSegmentsBegin;      // 0x120 / 0x3190 (1221/30) / 0x31A0 (1266)
    size_t layoutSegmentsEnd;
    size_t layoutSegmentsCap;
    size_t layoutLoadCmdsBase;       // 0 (1115) / 0x3108
    size_t layoutLoadCmdsCount;      // 0 / 0x3110
    size_t layoutDylibMapping;       // 0 / 0x0D8
    size_t layoutEntryAtom;          // 0 / 0x0D0
};

// Returns {.valid=false} for unrecognized versions.
inline LayoutConstants layoutConstantsFor(const LinkerVersion &v) {
    LayoutConstants lc = {};

    // Classic ld64 reaches Options directly via dylibs::doPass's first arg;
    // there's no Consolidator, so those fields stay zero.
    if (v.isClassic()) {
        lc.valid = true;
        lc.optionsOptimizationsBase = 0;
        lc.optionsDeadStripDylibs   = classic::options_offsets::Ld64_820::kDeadStripDylibs;
        return lc;
    }

    if (!v.isPrime()) return lc;
    lc.valid = true;

    // Stable across every ld-prime version.
    lc.consolidatorOptions          = consolidator::kOptionsPtr;
    lc.consolidatorInputDylibsBegin = consolidator::kInputDylibsBegin;

    if (v.major >= 1266) {
        lc.segmentStride = 0x58;
        lc.segSectionsBegin = 0x40; lc.segSectionsEnd = 0x48;
        lc.dynamicFixupPool = 0x218;
        lc.dynamicFileDylibFileInfo = 0x140;
        lc.dynamicFileIsLTO = 0x120;
        lc.dynamicFileLinkerOptions = 0x148;
        lc.dynamicFileAltFileInfos = 0x180;
        lc.dynamicFileLargeAddends = 0x230;
        lc.dynamicFileFileLOHs = 0x1D0;
        lc.layoutIndirectBegin = 0x3108;
        lc.layoutIndirectEnd   = 0x3110;
        lc.passFilesBegin = 0x878;
        lc.passFilesEnd   = 0x880;
        lc.fileVtableSrcPath = 0x08;
        lc.fileVtableDylibFileInfo = 0x28;
        lc.fileVtableDependencyInfos = 0x30;
        lc.fileVtableLinkerOptions = 0x38;
        lc.fileVtableLargeAddends = 0x40;
        lc.fileVtableIsLTO = 0x48;
        lc.fileVtableIsDynamic = 0x18;
        lc.fileVtableAtomLOHs = 0x68;
        lc.fileVtableFileLOHs = 0x70;
        lc.atomFile1DylibFileInfo   = 0x98;
        lc.atomFile1FixupPool       = 0xA0;
        lc.atomFile1LargeAddends    = 0xB0;
        lc.atomFile1LinkerOptions   = 0xE0;
        lc.atomFile1DependencyInfos = 0xF8;

        lc.optionsOptimizationsBase     = options_offsets::Prime_1266::kOptimizationsBase;
        lc.optionsDeadStrip             = options_offsets::Prime_1266::kDeadStrip;
        lc.optionsDeadStripDylibs       = options_offsets::Prime_1266::kDeadStripDylibs;
        lc.optionsAllowDeadDuplicates   = options_offsets::Prime_1266::kAllowDeadDuplicates;
        lc.optionsMergeZeroFillSections = options_offsets::Prime_1266::kMergeZeroFillSections;

        lc.consolidatorInputDylibsEnd    = consolidator_offsets::Prime_1221Plus::kInputDylibsEnd;
        lc.consolidatorOutputDylibsBegin = consolidator_offsets::Prime_1221Plus::kOutputDylibsBegin;
        lc.consolidatorOutputDylibsEnd   = consolidator_offsets::Prime_1221Plus::kOutputDylibsEnd;
        lc.consolidatorOutputDylibsCap   = consolidator_offsets::Prime_1221Plus::kOutputDylibsCap;

        // LayoutLinkedImage base + +0x10 drift in the derived tail.
        lc.layoutOptionsRef       = 0x000;
        lc.layoutConsolidatorRef  = 0x0C8;
        lc.layoutArch             = 0x100;
        lc.layoutSegmentsBegin    = 0x31A0;
        lc.layoutSegmentsEnd      = 0x31A8;
        lc.layoutSegmentsCap      = 0x31B0;
        lc.layoutLoadCmdsBase     = 0x3108;
        lc.layoutLoadCmdsCount    = 0x3110;
        lc.layoutDylibMapping     = 0x0D8;
        lc.layoutEntryAtom        = 0x0D0;
    } else if (v.major >= 1200) {
        lc.segmentStride = 0x58;
        lc.segSectionsBegin = 0x40; lc.segSectionsEnd = 0x48;
        lc.dynamicFixupPool = 0x1F8;
        lc.dynamicFileDylibFileInfo = 0x138;
        lc.dynamicFileIsLTO = 0x118;
        lc.dynamicFileLinkerOptions = 0x140;
        lc.dynamicFileAltFileInfos = 0x178;
        lc.dynamicFileLargeAddends = 0x210;
        lc.dynamicFileFileLOHs = 0x1C8;
        lc.layoutIndirectBegin = 0x3108;
        lc.layoutIndirectEnd   = 0x3110;
        lc.passFilesBegin = 0x878;
        lc.passFilesEnd   = 0x880;
        lc.fileVtableSrcPath = 0x08;
        lc.fileVtableDylibFileInfo = 0x28;
        lc.fileVtableDependencyInfos = 0x30;
        lc.fileVtableLinkerOptions = 0x38;
        lc.fileVtableLargeAddends = 0x40;
        lc.fileVtableIsLTO = 0x48;
        lc.fileVtableIsDynamic = 0x18;
        lc.fileVtableAtomLOHs = 0x68;
        lc.fileVtableFileLOHs = 0x70;
        lc.atomFile1DylibFileInfo   = 0x98;
        lc.atomFile1FixupPool       = 0xA0;
        lc.atomFile1LargeAddends    = 0xB0;
        lc.atomFile1LinkerOptions   = 0xE0;
        lc.atomFile1DependencyInfos = 0xF8;

        lc.optionsOptimizationsBase     = options_offsets::Prime_1221::kOptimizationsBase;
        lc.optionsDeadStrip             = options_offsets::Prime_1221::kDeadStrip;
        lc.optionsDeadStripDylibs       = options_offsets::Prime_1221::kDeadStripDylibs;
        lc.optionsAllowDeadDuplicates   = options_offsets::Prime_1221::kAllowDeadDuplicates;
        lc.optionsMergeZeroFillSections = options_offsets::Prime_1221::kMergeZeroFillSections;

        lc.consolidatorInputDylibsEnd    = consolidator_offsets::Prime_1221Plus::kInputDylibsEnd;
        lc.consolidatorOutputDylibsBegin = consolidator_offsets::Prime_1221Plus::kOutputDylibsBegin;
        lc.consolidatorOutputDylibsEnd   = consolidator_offsets::Prime_1221Plus::kOutputDylibsEnd;
        lc.consolidatorOutputDylibsCap   = consolidator_offsets::Prime_1221Plus::kOutputDylibsCap;

        // LayoutLinkedImage ctor is byte-identical between 1221 and 1230.
        lc.layoutOptionsRef       = 0x000;
        lc.layoutConsolidatorRef  = 0x0C8;
        lc.layoutArch             = 0x100;
        lc.layoutSegmentsBegin    = 0x3190;
        lc.layoutSegmentsEnd      = 0x3198;
        lc.layoutSegmentsCap      = 0x31A0;
        lc.layoutLoadCmdsBase     = 0x3108;
        lc.layoutLoadCmdsCount    = 0x3110;
        lc.layoutDylibMapping     = 0x0D8;
        lc.layoutEntryAtom        = 0x0D0;
    } else {
        lc.segmentStride = 0x50;
        lc.segSectionsBegin = 0x38; lc.segSectionsEnd = 0x40;
        lc.dynamicFixupPool = 0x1B8;
        lc.dynamicFileDylibFileInfo = 0x128;
        lc.dynamicFileIsLTO = 0x108;
        lc.dynamicFileLinkerOptions = 0x130;
        lc.dynamicFileAltFileInfos = 0x168;
        lc.dynamicFileLargeAddends = 0x1D0;
        lc.dynamicFileFileLOHs = 0;        // LOHs not emitted by 1115
        lc.layoutIndirectBegin = 0x3190;
        lc.layoutIndirectEnd   = 0x3198;
        lc.passFilesBegin = 0x788;
        lc.passFilesEnd   = 0x790;
        lc.fileVtableSrcPath         = 0;  // srcPath not present in 1115
        lc.fileVtableDylibFileInfo   = 0x20;
        lc.fileVtableDependencyInfos = 0x28;
        lc.fileVtableLinkerOptions   = 0x30;
        lc.fileVtableLargeAddends    = 0x38;
        lc.fileVtableIsLTO           = 0x40;
        lc.fileVtableIsDynamic       = 0x10;
        lc.fileVtableAtomLOHs        = 0;
        lc.fileVtableFileLOHs        = 0;
        lc.atomFile1DylibFileInfo   = 0x88;
        lc.atomFile1FixupPool       = 0x90;
        lc.atomFile1LargeAddends    = 0xA0;
        lc.atomFile1LinkerOptions   = 0xD0;
        lc.atomFile1DependencyInfos = 0xE8;

        // ld-1115 Options is flat - no Optimizations sub-struct.
        lc.optionsOptimizationsBase     = 0;
        lc.optionsDeadStrip             = options_offsets::Prime_1115::kDeadStrip;
        lc.optionsDeadStripDylibs       = options_offsets::Prime_1115::kDeadStripDylibs;
        lc.optionsAllowDeadDuplicates   = options_offsets::Prime_1115::kAllowDeadDuplicates;
        lc.optionsMergeZeroFillSections = options_offsets::Prime_1115::kMergeZeroFillSections;

        lc.consolidatorInputDylibsEnd    = consolidator_offsets::Prime_1115::kInputDylibsEnd;
        lc.consolidatorOutputDylibsBegin = consolidator_offsets::Prime_1115::kOutputDylibsBegin;
        lc.consolidatorOutputDylibsEnd   = consolidator_offsets::Prime_1115::kOutputDylibsEnd;
        lc.consolidatorOutputDylibsCap   = consolidator_offsets::Prime_1115::kOutputDylibsCap;

        // ld-1115 LayoutExecutable is flat (vtable at +0x00).
        lc.layoutOptionsRef       = 0x008;
        lc.layoutConsolidatorRef  = 0x108;
        lc.layoutArch             = 0x188;
        lc.layoutSegmentsBegin    = 0x120;
        lc.layoutSegmentsEnd      = 0x128;
        lc.layoutSegmentsCap      = 0x130;
        lc.layoutLoadCmdsBase     = 0;
        lc.layoutLoadCmdsCount    = 0;
        lc.layoutDylibMapping     = 0;
        lc.layoutEntryAtom        = 0;
    }
    return lc;
}

// Section fields - 0x88 stride, stable across all versions.
// kContentType holds ld::ContentType values (NOT AtomKind).
namespace section {
    inline constexpr size_t kStride      = 0x88;
    inline constexpr size_t kNamePtr     = 0x00;
    inline constexpr size_t kNameLen     = 0x08;
    inline constexpr size_t kSegNamePtr  = 0x10;
    inline constexpr size_t kSegNameLen  = 0x18;
    inline constexpr size_t kContentType = 0x2C;
    inline constexpr size_t kAlignment   = 0x30;  // u16 alignment power
    inline constexpr size_t kAddress     = 0x38;
    inline constexpr size_t kSize        = 0x40;
    inline constexpr size_t kFileOffset  = 0x48;
    inline constexpr size_t kAtomsBegin  = 0x60;
    inline constexpr size_t kAtomsEnd    = 0x68;
    inline constexpr size_t kSectionRO   = 0x78;
    inline constexpr size_t kSectionIdx  = 0x80;
}

// SectionRO_1 - inline metadata from .o files (0x2C bytes).
namespace SectionRO {
    inline constexpr size_t kAlignment   = 0x00;  // u32 alignment power
    inline constexpr size_t kContentType = 0x04;  // u32 ContentType
    inline constexpr size_t kSegName     = 0x08;  // char[18]
    inline constexpr size_t kSectName    = 0x1A;  // char[18]
    inline constexpr size_t kSize        = 0x2C;
}

// Segment fields - stride 0x50 (ld-1115) or 0x58 (1221+). Name and VM fields
// are stable; sections vector offset is version-dependent.
namespace segment {
    inline constexpr size_t kNamePtr      = 0x00;
    inline constexpr size_t kNameLen      = 0x08;
    inline constexpr size_t kVMAddr       = 0x10;
    inline constexpr size_t kVMSize       = 0x18;
    inline constexpr size_t kFileOff      = 0x20;
    inline constexpr size_t kFileSize     = 0x24;
    inline constexpr size_t kInitProt     = 0x2C;  // VM_PROT (5=RX, 3=RW)
    inline constexpr size_t kMaxProt      = 0x2D;  // VM_PROT
    inline constexpr size_t kSegmentOrder = 0x30;  // u32 sort key
    inline constexpr size_t kFixedAddr    = 0x38;  // u8, ld-1221+ only
    // Sections vector: +0x38/+0x40 (1115) or +0x40/+0x48 (1221+).
}

// ld-1115 legacy LayoutExecutable offsets. Do NOT use for new code on
// ld-1221+ - those versions inherit LayoutLinkedImage with completely
// different offsets. Prefer the layout* fields of LayoutConstants.
namespace layout {
    inline constexpr size_t kOptions         = 0x08;
    inline constexpr size_t kConsolidator    = 0x108;
    inline constexpr size_t kSegmentsBegin   = 0x120;
    inline constexpr size_t kSegmentsEnd     = 0x128;
    inline constexpr size_t kSegmentsCapacity = 0x130;
    inline constexpr size_t kArch            = 0x188;
}

// LinkeditBuilder fields. kLayout is the primary navigation entry point
// (read via builderGetLayout); the others are informational.
namespace builder {
    inline constexpr size_t kOptions       = 0x00;
    inline constexpr size_t kArch          = 0x08;
    inline constexpr size_t kLayout        = 0x10;
    inline constexpr size_t kAtomGroups    = 0x18;
    inline constexpr size_t kPlacement     = 0x20;
    inline constexpr size_t kSegmentsPtr   = 0x48;
    inline constexpr size_t kDylibMapping  = 0x50;
    inline constexpr size_t kIndirectBase  = 0x58;
    inline constexpr size_t kIndirectCount = 0x60;
}

// AtomPlacement (Atom+0x10).
namespace placement {
    inline constexpr size_t kOffset       = 0x00;
    inline constexpr size_t kSectionIndex = 0x0C;
}

// Navigation helpers.

inline const void *builderGetLayout(const void *bldr) {
    return readPtr(bldr, builder::kLayout);
}

// Version-aware segment vector accessors.
inline const uint8_t *layoutSegmentsBegin(const void *lay,
                                          const LayoutConstants &lc) {
    if (!lay || !lc.valid || lc.layoutSegmentsBegin == 0) return nullptr;
    return static_cast<const uint8_t *>(readPtr(lay, lc.layoutSegmentsBegin));
}

inline const uint8_t *layoutSegmentsEnd(const void *lay,
                                        const LayoutConstants &lc) {
    if (!lay || !lc.valid || lc.layoutSegmentsEnd == 0) return nullptr;
    return static_cast<const uint8_t *>(readPtr(lay, lc.layoutSegmentsEnd));
}

inline OptionsPtr layoutOptions(const void *lay, const LayoutConstants &lc) {
    if (!lay || !lc.valid) return nullptr;
    return readPtr(lay, lc.layoutOptionsRef);
}

inline ConsolidatorPtr layoutConsolidator(const void *lay, const LayoutConstants &lc) {
    if (!lay || !lc.valid) return nullptr;
    return readPtr(lay, lc.layoutConsolidatorRef);
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

// Segment field accessors.

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

// Segment name matching.

inline bool segmentNameMatches(const void *seg, const char *name, size_t nameLen) {
    size_t segLen = static_cast<size_t>(readU64(seg, segment::kNameLen));
    if (segLen != nameLen) return false;
    return memcmp(segmentName(seg), name, nameLen) == 0;
}

// Fast path for 6-char names (__DATA, __TEXT) - no memcmp.
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

// Find a section by segment + section name.
inline const void *findSection(const void *layoutExe,
                               const LayoutConstants &lc,
                               const char *segName, size_t segNameLen,
                               const char *sectName, size_t sectNameLen) {
    const uint8_t *segBegin = layoutSegmentsBegin(layoutExe, lc);
    const uint8_t *segEnd   = layoutSegmentsEnd(layoutExe, lc);
    if (!segBegin || !segEnd) return nullptr;

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

// Sanity check - call at hook entry before navigating; returns false on
// any plausibility failure (null, version mismatch, segment count OOB).
inline bool validateBuilder(const void *bldr, const LayoutConstants &lc) {
    if (!bldr || !lc.valid) return false;

    const void *lay = builderGetLayout(bldr);
    if (!lay) return false;

    const uint8_t *segBegin = layoutSegmentsBegin(lay, lc);
    const uint8_t *segEnd   = layoutSegmentsEnd(lay, lc);
    if (!segBegin || !segEnd || segBegin > segEnd) return false;

    size_t segCount = (segEnd - segBegin) / lc.segmentStride;
    if (segCount == 0 || segCount > 100) return false;

    return true;
}

// Mirrors Fixup.cpp's directTargetNoFollow: returns the raw target atom
// without following alias chains (see resolveFixupTargetFollowAliases).
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

// Same as resolveFixupTarget, but walks alias chains via atom+0x18.
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

// DynamicAtomFile typed vector accessors. Element strides match the
// sizeof() of the Span<T> element type; see LinkEdit.h.
inline Span<LinkerOption> dynamicFileLinkerOptions(const void *file,
                                                   const LayoutConstants &lc) {
    if (!file || !lc.valid || lc.dynamicFileLinkerOptions == 0) return {};
    return readSpan<LinkerOption>(file,
                                  lc.dynamicFileLinkerOptions,
                                  lc.dynamicFileLinkerOptions + 8);
}

inline Span<LargeAddend> dynamicFileLargeAddends(const void *file,
                                                 const LayoutConstants &lc) {
    if (!file || !lc.valid || lc.dynamicFileLargeAddends == 0) return {};
    return readSpan<LargeAddend>(file,
                                 lc.dynamicFileLargeAddends,
                                 lc.dynamicFileLargeAddends + 8);
}

inline Span<uint64_t> dynamicFileLOHs(const void *file,
                                      const LayoutConstants &lc) {
    if (!file || !lc.valid || lc.dynamicFileFileLOHs == 0) return {};
    return readSpan<uint64_t>(file,
                              lc.dynamicFileFileLOHs,
                              lc.dynamicFileFileLOHs + 8);
}

// Pointer-element vector (std::vector<const AltFileInfo*>). Opaque.
inline Span<const void *> dynamicFileAltFileInfos(const void *file,
                                                  const LayoutConstants &lc) {
    if (!file || !lc.valid || lc.dynamicFileAltFileInfos == 0) return {};
    return readSpan<const void *>(file,
                                  lc.dynamicFileAltFileInfos,
                                  lc.dynamicFileAltFileInfos + 8);
}

// Options version-aware flag accessors.

inline bool optionsDeadStripDylibs(OptionsPtr opts, const LayoutConstants &lc) {
    if (!lc.valid) return false;
    return optionsReadFlag(opts, lc.optionsDeadStripDylibs);
}

inline void setOptionsDeadStripDylibs(MutableOptionsPtr opts,
                                      const LayoutConstants &lc,
                                      bool enable) {
    if (!lc.valid) return;
    optionsWriteFlag(opts, lc.optionsDeadStripDylibs, enable);
}

inline bool optionsDeadStrip(OptionsPtr opts, const LayoutConstants &lc) {
    if (!lc.valid) return false;
    return optionsReadFlag(opts, lc.optionsDeadStrip);
}

inline void setOptionsDeadStrip(MutableOptionsPtr opts,
                                const LayoutConstants &lc,
                                bool enable) {
    if (!lc.valid) return;
    optionsWriteFlag(opts, lc.optionsDeadStrip, enable);
}

inline bool optionsMergeZeroFillSections(OptionsPtr opts, const LayoutConstants &lc) {
    if (!lc.valid) return false;
    return optionsReadFlag(opts, lc.optionsMergeZeroFillSections);
}

// Optimizations sub-struct base for ld-1221+. Returns nullptr on ld-1115
// (flat layout) or any invalid version.
inline const void *optionsOptimizations(OptionsPtr opts,
                                        const LayoutConstants &lc) {
    if (!opts || !lc.valid || lc.optionsOptimizationsBase == 0) return nullptr;
    return static_cast<const uint8_t *>(opts) + lc.optionsOptimizationsBase;
}

// Consolidator version-aware accessors.

inline OptionsPtr consolidatorOptions(ConsolidatorPtr cons,
                                      const LayoutConstants &lc) {
    if (!cons || !lc.valid) return nullptr;
    return readPtr(cons, lc.consolidatorOptions);
}

// Input dylib scratch vector - populated by activateLibraries() before
// buildDylibMapping runs. Elements are `ld::Dylib*`.
inline const void *const *consolidatorInputDylibsBegin(ConsolidatorPtr cons,
                                                       const LayoutConstants &lc) {
    if (!cons || !lc.valid) return nullptr;
    return static_cast<const void *const *>(
        readPtr(cons, lc.consolidatorInputDylibsBegin));
}

inline const void *const *consolidatorInputDylibsEnd(ConsolidatorPtr cons,
                                                     const LayoutConstants &lc) {
    if (!cons || !lc.valid || lc.consolidatorInputDylibsEnd == 0) return nullptr;
    return static_cast<const void *const *>(
        readPtr(cons, lc.consolidatorInputDylibsEnd));
}

// Output dylib vector - populated by buildDylibMapping after the
// erase-remove filter. Feeds LC_LOAD_DYLIB emission. Elements are
// DylibLoadInfo value-types; iterate with dylibLoadInfoStride().
inline const void *const *consolidatorOutputDylibsBegin(ConsolidatorPtr cons,
                                                        const LayoutConstants &lc) {
    if (!cons || !lc.valid || lc.consolidatorOutputDylibsBegin == 0) return nullptr;
    return static_cast<const void *const *>(
        readPtr(cons, lc.consolidatorOutputDylibsBegin));
}

inline const void *const *consolidatorOutputDylibsEnd(ConsolidatorPtr cons,
                                                      const LayoutConstants &lc) {
    if (!cons || !lc.valid || lc.consolidatorOutputDylibsEnd == 0) return nullptr;
    return static_cast<const void *const *>(
        readPtr(cons, lc.consolidatorOutputDylibsEnd));
}

inline size_t consolidatorInputDylibCount(ConsolidatorPtr cons,
                                          const LayoutConstants &lc) {
    auto begin = consolidatorInputDylibsBegin(cons, lc);
    auto end   = consolidatorInputDylibsEnd(cons, lc);
    if (!begin || !end || end < begin) return 0;
    return static_cast<size_t>(end - begin);
}

inline size_t consolidatorOutputDylibCount(ConsolidatorPtr cons,
                                           const LayoutConstants &lc) {
    auto begin = consolidatorOutputDylibsBegin(cons, lc);
    auto end   = consolidatorOutputDylibsEnd(cons, lc);
    if (!begin || !end || end < begin) return 0;
    return static_cast<size_t>(end - begin);
}

// Structural sanity check - call at hook entry before mutating state.
// Rejects null, invalid/classic version, null Options*, or implausible
// input dylib vector (begin > end, count > 100k).
inline bool validateConsolidator(ConsolidatorPtr cons,
                                 const LayoutConstants &lc) {
    if (!cons || !lc.valid) return false;

    OptionsPtr opts = consolidatorOptions(cons, lc);
    if (!opts) return false;

    auto begin = consolidatorInputDylibsBegin(cons, lc);
    auto end   = consolidatorInputDylibsEnd(cons, lc);
    if (!begin || !end || end < begin) return false;
    if (static_cast<size_t>(end - begin) > 100000) return false;

    return true;
}

} // namespace ld

#endif
