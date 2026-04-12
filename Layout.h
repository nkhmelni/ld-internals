// Copyright (c) 2026 Nikita Hmelnitkii. MIT License - see LICENSE.
//
// Layout.h - LayoutConstants, segment/section/atom navigation, fixup
// resolution, and Consolidator/Builder accessors.

#ifndef LD_LAYOUT_H
#define LD_LAYOUT_H

#include "Version.h"
#include "Atom.h"
#include "LinkEdit.h"
#include "Options.h"
#include "Consolidator.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ld {

// Version-dependent offsets. Build once via layoutConstantsFor() and
// cache. A zero offset means "not present on this version".
//
// LinkeditBuilder tier summary:
//   1115: builder 0x70 bytes, Layout at +0x10.
//   1167/1221/1230: builder 0x90 bytes, Layout at +0x10.
//   1266: builder 0xA0 bytes (new 8-byte slot at +0x08), Layout at +0x18.
struct LayoutConstants {
    bool          valid;
    LinkerVersion version;     // set by layoutConstantsFor(); {Unknown,0,0,0} otherwise

    // Segment layout. Prot/order offsets differ: 1115 vs 1221+.
    size_t segmentStride;
    size_t segSectionsBegin;
    size_t segSectionsEnd;
    size_t segMaxProt;          // 0x2C (1115) / 0x2E (1221+)
    size_t segInitProt;         // 0x2D (1115) / 0x2F (1221+)
    size_t segSegmentOrder;     // 0x30 (1115) / 0x3C (1221+)
    size_t segFixedAddr;        // 0 (1115, absent) / 0x38 (1221+)
    size_t segLinkedSeg;        // 0 (1115, absent) / 0x30 (1221+)

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

    // AtomFile vtable - +0x08 shift on 1221+ (srcPath inserted at slot 1).
    size_t fileVtableSrcPath;
    size_t fileVtableDylibFileInfo;
    size_t fileVtableDependencyInfos;
    size_t fileVtableLinkerOptions;
    size_t fileVtableLargeAddends;
    size_t fileVtableIsLTO;
    size_t fileVtableIsDynamic;
    size_t fileVtableAtomLOHs;
    size_t fileVtableFileLOHs;

    // AtomFile_1 fields - +0x10 shift on 1221+.
    size_t atomFile1DylibFileInfo;
    size_t atomFile1FixupPool;
    size_t atomFile1LargeAddends;
    size_t atomFile1LinkerOptions;
    size_t atomFile1DependencyInfos;

    // Options flags - absolute offsets within Options.
    size_t optionsOptimizationsBase;  // 0 on 1115 (flat layout)
    size_t optionsDeadStrip;
    size_t optionsDeadStripDylibs;
    size_t optionsAllowDeadDuplicates;
    size_t optionsMergeZeroFillSections;

    // AtomFileConsolidator vectors.
    size_t consolidatorOptions;                 // always 0x00 on ld-prime
    size_t consolidatorInputAtomFilesBegin;     // vector<const AtomFile*>
    size_t consolidatorInputAtomFilesEnd;
    size_t consolidatorOutputDylibsBegin;       // vector<DylibLoadInfo>
    size_t consolidatorOutputDylibsEnd;
    size_t consolidatorOutputDylibsCap;

    // LinkeditBuilder field offsets (version-tiered).
    size_t builderOptionsOffset;     // Options* first field, always 0x00
    size_t builderFlagsOffset;       // uint32 (chained-fixups-disabled etc)
    size_t builderLayoutOffset;      // Layout* - 0x10 (<=1230) / 0x18 (1266)
    size_t builderAtomGroupOffset;   // root AtomGroup - +0x08 past layout

    // LayoutExecutable. 1115 is flat; 1221+ inherits LayoutLinkedImage.
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

// Returns {valid=false} for unrecognised versions.
inline LayoutConstants layoutConstantsFor(const LinkerVersion &v) {
    LayoutConstants lc = {};
    lc.version = v;

    // Classic: Options passed directly, no Consolidator.
    if (v.isClassic()) {
        lc.valid = true;
        lc.optionsOptimizationsBase = 0;
        lc.optionsDeadStripDylibs   = classic::options_offsets::Ld64_820::kDeadStripDylibs;
        return lc;
    }

    if (!v.isPrime()) return lc;
    lc.valid = true;

    // Stable across every ld-prime version.
    lc.consolidatorOptions = consolidator::kOptionsPtr;

    // LinkeditBuilder: 1266 inserted a new slot at +0x08, shifting
    // flags/layout/atomGroup down by 8.
    if (v.major >= 1266) {
        lc.builderOptionsOffset   = 0x00;
        lc.builderFlagsOffset     = 0x10;
        lc.builderLayoutOffset    = 0x18;
        lc.builderAtomGroupOffset = 0x20;
    } else {
        lc.builderOptionsOffset   = 0x00;
        lc.builderFlagsOffset     = 0x08;
        lc.builderLayoutOffset    = 0x10;
        lc.builderAtomGroupOffset = 0x18;
    }

    if (v.major >= 1266) {
        lc.segmentStride = 0x58;
        lc.segSectionsBegin = 0x40; lc.segSectionsEnd = 0x48;
        lc.segMaxProt = 0x2E; lc.segInitProt = 0x2F;
        lc.segSegmentOrder = 0x3C; lc.segFixedAddr = 0x38; lc.segLinkedSeg = 0x30;
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

        lc.consolidatorInputAtomFilesBegin = consolidator_offsets::Prime_1221Plus::kInputAtomFilesBegin;
        lc.consolidatorInputAtomFilesEnd   = consolidator_offsets::Prime_1221Plus::kInputAtomFilesEnd;
        lc.consolidatorOutputDylibsBegin   = consolidator_offsets::Prime_1221Plus::kOutputDylibsBegin;
        lc.consolidatorOutputDylibsEnd     = consolidator_offsets::Prime_1221Plus::kOutputDylibsEnd;
        lc.consolidatorOutputDylibsCap     = consolidator_offsets::Prime_1221Plus::kOutputDylibsCap;

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
    } else if (v.major >= 1167) {
        // 1167/1221/1230 tier - DynamicAtomFile/AtomFile_1 layout is
        // shared; 1167 overrides the Options/Consolidator/Layout tail at
        // the bottom of this branch.
        lc.segmentStride = 0x58;
        lc.segSectionsBegin = 0x40; lc.segSectionsEnd = 0x48;
        lc.segMaxProt = 0x2E; lc.segInitProt = 0x2F;
        lc.segSegmentOrder = 0x3C; lc.segFixedAddr = 0x38; lc.segLinkedSeg = 0x30;
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

        lc.consolidatorInputAtomFilesBegin = consolidator_offsets::Prime_1221Plus::kInputAtomFilesBegin;
        lc.consolidatorInputAtomFilesEnd   = consolidator_offsets::Prime_1221Plus::kInputAtomFilesEnd;
        lc.consolidatorOutputDylibsBegin   = consolidator_offsets::Prime_1221Plus::kOutputDylibsBegin;
        lc.consolidatorOutputDylibsEnd     = consolidator_offsets::Prime_1221Plus::kOutputDylibsEnd;
        lc.consolidatorOutputDylibsCap     = consolidator_offsets::Prime_1221Plus::kOutputDylibsCap;

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

        // 1167 uses the pre-rewrite LayoutExecutable without cached
        // tail fields. Zeroed slots cause accessors to return null.
        if (v.major < 1221) {
            lc.optionsOptimizationsBase     = options_offsets::Prime_1167::kOptimizationsBase;
            lc.optionsDeadStrip             = options_offsets::Prime_1167::kDeadStrip;
            lc.optionsDeadStripDylibs       = options_offsets::Prime_1167::kDeadStripDylibs;
            lc.optionsAllowDeadDuplicates   = options_offsets::Prime_1167::kAllowDeadDuplicates;
            lc.optionsMergeZeroFillSections = options_offsets::Prime_1167::kMergeZeroFillSections;

            lc.consolidatorInputAtomFilesBegin = consolidator_offsets::Prime_1167::kInputAtomFilesBegin;
            lc.consolidatorInputAtomFilesEnd   = consolidator_offsets::Prime_1167::kInputAtomFilesEnd;
            lc.consolidatorOutputDylibsBegin   = consolidator_offsets::Prime_1167::kOutputDylibsBegin;
            lc.consolidatorOutputDylibsEnd     = consolidator_offsets::Prime_1167::kOutputDylibsEnd;
            lc.consolidatorOutputDylibsCap     = consolidator_offsets::Prime_1167::kOutputDylibsCap;

            lc.passFilesBegin        = 0x828;
            lc.passFilesEnd          = 0x830;
            lc.layoutConsolidatorRef = 0x110;
            lc.layoutSegmentsBegin   = 0x128;
            lc.layoutSegmentsEnd     = 0x130;
            lc.layoutSegmentsCap     = 0x138;

            lc.layoutArch          = 0;
            lc.layoutLoadCmdsBase  = 0;
            lc.layoutLoadCmdsCount = 0;
            lc.layoutIndirectBegin = 0;
            lc.layoutIndirectEnd   = 0;
            lc.layoutDylibMapping  = 0;
            lc.layoutEntryAtom     = 0;
        }
    } else {
        lc.segmentStride = 0x50;
        lc.segSectionsBegin = 0x38; lc.segSectionsEnd = 0x40;
        lc.segMaxProt = 0x2C; lc.segInitProt = 0x2D;
        lc.segSegmentOrder = 0x30; lc.segFixedAddr = 0; lc.segLinkedSeg = 0;
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

        // ld-1115: flat Options, no Optimizations sub-struct.
        lc.optionsOptimizationsBase     = 0;
        lc.optionsDeadStrip             = options_offsets::Prime_1115::kDeadStrip;
        lc.optionsDeadStripDylibs       = options_offsets::Prime_1115::kDeadStripDylibs;
        lc.optionsAllowDeadDuplicates   = options_offsets::Prime_1115::kAllowDeadDuplicates;
        lc.optionsMergeZeroFillSections = options_offsets::Prime_1115::kMergeZeroFillSections;

        lc.consolidatorInputAtomFilesBegin = consolidator_offsets::Prime_1115::kInputAtomFilesBegin;
        lc.consolidatorInputAtomFilesEnd   = consolidator_offsets::Prime_1115::kInputAtomFilesEnd;
        lc.consolidatorOutputDylibsBegin   = consolidator_offsets::Prime_1115::kOutputDylibsBegin;
        lc.consolidatorOutputDylibsEnd     = consolidator_offsets::Prime_1115::kOutputDylibsEnd;
        lc.consolidatorOutputDylibsCap     = consolidator_offsets::Prime_1115::kOutputDylibsCap;

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

// Section fields - 0x88 stride, stable across versions. kContentType
// holds ld::ContentType values (NOT AtomKind).
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

// SectionRO_1 - inline .o-file metadata (0x2C bytes).
namespace SectionRO {
    inline constexpr size_t kAlignment   = 0x00;  // u32 alignment power
    inline constexpr size_t kContentType = 0x04;  // u32 ContentType
    inline constexpr size_t kSegName     = 0x08;  // char[18]
    inline constexpr size_t kSectName    = 0x1A;  // char[18]
    inline constexpr size_t kSize        = 0x2C;
}

// Segment fields. Stable fields use these constants directly; fields
// that shifted between ld-1115 and ld-1221+ go through LayoutConstants
// (segMaxProt, segInitProt, segSegmentOrder, segFixedAddr, segLinkedSeg).
namespace segment {
    // Stable across all versions.
    inline constexpr size_t kNamePtr      = 0x00;
    inline constexpr size_t kNameLen      = 0x08;
    inline constexpr size_t kVMAddr       = 0x10;
    inline constexpr size_t kVMSize       = 0x18;
    inline constexpr size_t kFileOff      = 0x20;  // u32
    inline constexpr size_t kFileSize     = 0x24;  // u32
    inline constexpr size_t kSegFlags     = 0x28;  // u32 (SG_READ_ONLY etc)
    // Version-dependent (use LayoutConstants accessors):
    //   maxProt:      0x2C (1115) / 0x2E (1221+)
    //   initProt:     0x2D (1115) / 0x2F (1221+)
    //   linkedSeg:    absent (1115) / 0x30 (1221+)
    //   fixedAddr:    absent (1115) / 0x38 (1221+)
    //   segmentOrder: 0x30 (1115) / 0x3C (1221+)
    // Sections vector: +0x38/+0x40 (1115) or +0x40/+0x48 (1221+).
}

// AtomPlacement (Atom+0x10). Holds the atom's offset within its final
// section + a u32 section index into the LinkedImage section table.
namespace placement {
    inline constexpr size_t kOffset       = 0x00;  // u64 offset within section
    inline constexpr size_t kSectionIndex = 0x0C;  // u32 section index
}

inline uint64_t atomPlacementOffset(AtomPtr a) {
    const void *p = atomPlacement(a);
    return p ? readU64(p, placement::kOffset) : 0;
}
inline uint32_t atomPlacementSectionIndex(AtomPtr a) {
    const void *p = atomPlacement(a);
    return p ? readU32(p, placement::kSectionIndex) : 0;
}

// Builder navigation (version-aware).
inline const void *builderGetLayout(const void *bldr, const LayoutConstants &lc) {
    if (!bldr || !lc.valid) return nullptr;
    return readPtr(bldr, lc.builderLayoutOffset);
}
inline const void *builderGetOptions(const void *bldr, const LayoutConstants &lc) {
    if (!bldr || !lc.valid) return nullptr;
    return readPtr(bldr, lc.builderOptionsOffset);
}
inline const void *builderGetAtomGroup(const void *bldr, const LayoutConstants &lc) {
    if (!bldr || !lc.valid) return nullptr;
    return readPtr(bldr, lc.builderAtomGroupOffset);
}
inline uint32_t builderGetFlags(const void *bldr, const LayoutConstants &lc) {
    if (!bldr || !lc.valid) return 0;
    return readU32(bldr, lc.builderFlagsOffset);
}

// Version-aware segment-vector accessors.
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

inline uint64_t sectionFileOffset(const void *sec) {
    return readU64(sec, section::kFileOffset);
}

inline uint16_t sectionAlignmentPow2(const void *sec) {
    return readU16(sec, section::kAlignment);
}

inline uint32_t sectionIndex(const void *sec) {
    return readU32(sec, section::kSectionIdx);
}

inline const void *sectionRO(const void *sec) {
    return readPtr(sec, section::kSectionRO);
}

// SectionRO (0x2C bytes inline in .o files). Returns 0 / empty for null.
inline uint32_t sectionRoAlignment(const void *ro)   { return ro ? readU32(ro, SectionRO::kAlignment)   : 0; }
inline uint32_t sectionRoContentType(const void *ro) { return ro ? readU32(ro, SectionRO::kContentType) : 0; }
inline const char *sectionRoSegName(const void *ro)  { return ro ? static_cast<const char *>(ro) + SectionRO::kSegName  : nullptr; }
inline const char *sectionRoSectName(const void *ro) { return ro ? static_cast<const char *>(ro) + SectionRO::kSectName : nullptr; }

inline uint64_t sectionVMEnd(const void *sec) {
    return sectionAddress(sec) + sectionSize(sec);
}

inline bool sectionContainsVM(const void *sec, uint64_t vmaddr) {
    const uint64_t base = sectionAddress(sec);
    return vmaddr >= base && vmaddr < base + sectionSize(sec);
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

// Version-aware segment field accessors.
inline uint8_t segmentInitProt(const void *seg, const LayoutConstants &lc) {
    return readU8(seg, lc.segInitProt);
}
inline uint8_t segmentMaxProt(const void *seg, const LayoutConstants &lc) {
    return readU8(seg, lc.segMaxProt);
}
inline uint32_t segmentOrder(const void *seg, const LayoutConstants &lc) {
    return readU32(seg, lc.segSegmentOrder);
}
inline bool segmentFixedAddr(const void *seg, const LayoutConstants &lc) {
    return lc.segFixedAddr != 0 && readU8(seg, lc.segFixedAddr) != 0;
}
inline const void *segmentLinkedSeg(const void *seg, const LayoutConstants &lc) {
    return lc.segLinkedSeg != 0 ? readPtr(seg, lc.segLinkedSeg) : nullptr;
}

inline uint32_t segmentFlags(const void *seg) {
    return readU32(seg, segment::kSegFlags);
}

// Segment name matching.

inline bool segmentNameMatches(const void *seg, const char *name, size_t nameLen) {
    size_t segLen = static_cast<size_t>(readU64(seg, segment::kNameLen));
    if (segLen != nameLen) return false;
    return memcmp(segmentName(seg), name, nameLen) == 0;
}

// 6-char name fast-path (__DATA / __TEXT) - avoids memcmp.
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

// Segment / section traversal helpers.
inline uint64_t segmentVMEnd(const void *seg) {
    return segmentVMAddr(seg) + segmentVMSize(seg);
}

inline bool segmentContainsVM(const void *seg, uint64_t vmaddr) {
    const uint64_t base = segmentVMAddr(seg);
    return vmaddr >= base && vmaddr < base + segmentVMSize(seg);
}

// Find a segment by name. Null on miss.
inline const void *findSegment(const void *layoutExe,
                               const LayoutConstants &lc,
                               const char *name, size_t nameLen) {
    const uint8_t *begin = layoutSegmentsBegin(layoutExe, lc);
    const uint8_t *end   = layoutSegmentsEnd(layoutExe, lc);
    if (!begin || !end) return nullptr;
    for (const uint8_t *seg = begin; seg < end; seg += lc.segmentStride) {
        if (segmentNameMatches(seg, name, nameLen)) return seg;
    }
    return nullptr;
}

// Count sections within a segment (computed span, not stored size).
inline size_t segmentSectionCount(const void *seg, const LayoutConstants &lc) {
    if (!seg || !lc.valid) return 0;
    const uint8_t *b = segSectionsBegin(seg, lc);
    const uint8_t *e = segSectionsEnd(seg, lc);
    if (!b || !e || e < b) return 0;
    return static_cast<size_t>(e - b) / section::kStride;
}

// Iterate every segment in the LayoutExecutable. fn: bool(seg).
template <typename Fn>
inline void forEachSegment(const void *layoutExe, const LayoutConstants &lc, Fn fn) {
    const uint8_t *begin = layoutSegmentsBegin(layoutExe, lc);
    const uint8_t *end   = layoutSegmentsEnd(layoutExe, lc);
    if (!begin || !end) return;
    for (const uint8_t *seg = begin; seg < end; seg += lc.segmentStride) {
        if (!fn(static_cast<const void *>(seg))) return;
    }
}

// Iterate atom pointers in a section. fn: bool(AtomPtr).
template <typename Fn>
inline void forEachAtomInSection(const void *sec, Fn fn) {
    AtomPtr const *begin = sectionAtomsBegin(sec);
    AtomPtr const *end   = sectionAtomsEnd(sec);
    if (!begin || !end) return;
    for (AtomPtr const *p = begin; p < end; ++p) {
        if (*p && !fn(*p)) return;
    }
}

// Iterate every section across every segment. fn: bool(seg, sec).
template <typename Fn>
inline void forEachSection(const void *layoutExe, const LayoutConstants &lc, Fn fn) {
    const uint8_t *begin = layoutSegmentsBegin(layoutExe, lc);
    const uint8_t *end   = layoutSegmentsEnd(layoutExe, lc);
    if (!begin || !end) return;
    for (const uint8_t *seg = begin; seg < end; seg += lc.segmentStride) {
        const uint8_t *sb = segSectionsBegin(seg, lc);
        const uint8_t *se = segSectionsEnd(seg, lc);
        if (!sb || !se || se < sb) continue;
        for (const uint8_t *sec = sb; sec < se; sec += section::kStride) {
            if (!fn(static_cast<const void *>(seg), static_cast<const void *>(sec)))
                return;
        }
    }
}

// Iterate every atom in the LayoutExecutable. fn: bool(seg, sec, atom).
template <typename Fn>
inline void forEachAtom(const void *layoutExe,
                        const LayoutConstants &lc, Fn fn) {
    forEachSection(layoutExe, lc, [&](const void *seg, const void *sec) {
        bool cont = true;
        forEachAtomInSection(sec, [&](AtomPtr a) {
            cont = fn(seg, sec, a);
            return cont;
        });
        return cont;
    });
}

// Find a section by (segment name, section name). Null on miss.
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

// VM protection predicates (mach vm_prot_t).
inline bool segmentIsReadable(const void *seg, const LayoutConstants &lc)   { return (segmentInitProt(seg, lc) & 0x1) != 0; }
inline bool segmentIsWritable(const void *seg, const LayoutConstants &lc)   { return (segmentInitProt(seg, lc) & 0x2) != 0; }
inline bool segmentIsExecutable(const void *seg, const LayoutConstants &lc) { return (segmentInitProt(seg, lc) & 0x4) != 0; }

// Find an atom by name across every section. O(atoms); use sparingly.
inline AtomPtr findAtomByName(const void *layoutExe,
                              const LayoutConstants &lc,
                              const char *name) {
    if (!name) return nullptr;
    AtomPtr hit = nullptr;
    forEachSection(layoutExe, lc, [&](const void *, const void *sec) {
        AtomPtr const *ab = sectionAtomsBegin(sec);
        AtomPtr const *ae = sectionAtomsEnd(sec);
        if (!ab || !ae) return true;
        for (AtomPtr const *p = ab; p < ae; ++p) {
            const char *n = atomName(*p);
            if (n && __builtin_strcmp(n, name) == 0) { hit = *p; return false; }
        }
        return true;
    });
    return hit;
}

// Parsed install name for the dylib a file points at.
inline DylibInstallName fileDylibInstallNameParsed(const void *file,
                                                    size_t vtSlot) {
    return parseDylibInstallName(fileDylibInstallName(file, vtSlot));
}

inline DylibInstallName dylibInstallNameParsed(const void *dfi) {
    return parseDylibInstallName(dylibInstallName(dfi));
}

// LayoutExecutable Architecture*. Null when layoutArch is zero.
inline const Architecture *layoutArchitecture(const void *lay,
                                              const LayoutConstants &lc) {
    if (!lay || !lc.valid || lc.layoutArch == 0) return nullptr;
    return static_cast<const Architecture *>(readPtr(lay, lc.layoutArch));
}

// Structural sanity check. Rejects null, OOB segment count,
// unreachable Options*, or implausible section counts.
inline bool validateBuilder(const void *bldr, const LayoutConstants &lc) {
    if (!bldr || !lc.valid) return false;

    const void *lay = builderGetLayout(bldr, lc);
    if (!lay) return false;

    const uint8_t *segBegin = layoutSegmentsBegin(lay, lc);
    const uint8_t *segEnd   = layoutSegmentsEnd(lay, lc);
    if (!segBegin || !segEnd || segBegin > segEnd) return false;

    size_t segCount = (segEnd - segBegin) / lc.segmentStride;
    if (segCount == 0 || segCount > 100) return false;

    // Prime builders must have a reachable Options*.
    if (lc.version.isPrime() && !layoutOptions(lay, lc)) return false;

    // Each segment's section count must be plausible (<=256).
    for (const uint8_t *seg = segBegin; seg < segEnd; seg += lc.segmentStride) {
        size_t sc = segmentSectionCount(seg, lc);
        if (sc > 256) return false;
    }

    return true;
}

// Raw target resolution, no alias chain following.
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

// Target resolution with alias chain following.
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

// DynamicAtomFile typed vector accessors. Strides = sizeof(element).
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

// Pointer-element vector (std::vector<const AltFileInfo*>).
inline Span<const void *> dynamicFileAltFileInfos(const void *file,
                                                  const LayoutConstants &lc) {
    if (!file || !lc.valid || lc.dynamicFileAltFileInfos == 0) return {};
    return readSpan<const void *>(file,
                                  lc.dynamicFileAltFileInfos,
                                  lc.dynamicFileAltFileInfos + 8);
}

// Fixup pool for a DynamicAtomFile (flat ld::Fixup array). Elements are
// indexed by DynamicAtom::fixupStart (atom+0x3C) * sizeof(Fixup).
inline const Fixup *dynamicFileFixupPool(const void *file,
                                         const LayoutConstants &lc) {
    if (!file || !lc.valid || lc.dynamicFixupPool == 0) return nullptr;
    return static_cast<const Fixup *>(readPtr(file, lc.dynamicFixupPool));
}

// Fixup pool for an AtomFile_1 (.o file). Atom_1 stores start/count in
// AtomRO, not in the atom struct; iterate via atom1FileFixups.
inline const Fixup *atomFile1FixupPool(const void *file,
                                       const LayoutConstants &lc) {
    if (!file || !lc.valid || lc.atomFile1FixupPool == 0) return nullptr;
    return static_cast<const Fixup *>(readPtr(file, lc.atomFile1FixupPool));
}

// Atom_1 fixup span: (pool + ro.start*16, ro.count). Empty on null.
inline FixupSpan atom1FileFixups(const void *file, const void *ro,
                                 const LayoutConstants &lc) {
    if (!file || !ro || !lc.valid) return FixupSpan{nullptr, 0};
    const Fixup *pool = atomFile1FixupPool(file, lc);
    if (!pool) return FixupSpan{nullptr, 0};
    uint32_t start = readU32(ro, AtomRO::kFixupStart);
    uint32_t count = readU32(ro, AtomRO::kFixupCount);
    return FixupSpan{const_cast<Fixup *>(pool + start), count};
}

// Version-aware Options flag accessors.
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

// Optimizations sub-struct base. Null on ld-1115 (flat Options).
inline const void *optionsOptimizations(OptionsPtr opts,
                                        const LayoutConstants &lc) {
    if (!opts || !lc.valid || lc.optionsOptimizationsBase == 0) return nullptr;
    return static_cast<const uint8_t *>(opts) + lc.optionsOptimizationsBase;
}

// True when all layout tail fields are populated (false for 1167).
inline bool layoutConstantsHaveDiagnosticPath(const LayoutConstants &lc) {
    return lc.valid
        && lc.layoutArch          != 0
        && lc.layoutLoadCmdsBase  != 0
        && lc.layoutLoadCmdsCount != 0
        && lc.layoutDylibMapping  != 0
        && lc.layoutEntryAtom     != 0;
}

// Iterate Consolidator::passFiles. Each entry is an AtomFile*
// (typically DynamicAtomFile*). No-op on classic / invalid lc.
template <typename Fn>
inline void forEachPassFile(ConsolidatorPtr cons,
                            const LayoutConstants &lc, Fn fn) {
    if (!cons || !lc.valid || lc.passFilesBegin == 0 || lc.passFilesEnd == 0)
        return;
    const void *const *begin = static_cast<const void *const *>(
        readPtr(cons, lc.passFilesBegin));
    const void *const *end = static_cast<const void *const *>(
        readPtr(cons, lc.passFilesEnd));
    if (!begin || !end || end < begin) return;
    for (const void *const *p = begin; p < end; ++p) {
        if (*p && !fn(*p)) return;
    }
}

// Version-aware Consolidator accessors.
inline OptionsPtr consolidatorOptions(ConsolidatorPtr cons,
                                      const LayoutConstants &lc) {
    if (!cons || !lc.valid) return nullptr;
    return readPtr(cons, lc.consolidatorOptions);
}

// Input AtomFile pointer vector. Stride 8.
inline const void *const *consolidatorInputAtomFilesBegin(ConsolidatorPtr cons,
                                                          const LayoutConstants &lc) {
    if (!cons || !lc.valid || lc.consolidatorInputAtomFilesBegin == 0) return nullptr;
    return static_cast<const void *const *>(
        readPtr(cons, lc.consolidatorInputAtomFilesBegin));
}

inline const void *const *consolidatorInputAtomFilesEnd(ConsolidatorPtr cons,
                                                        const LayoutConstants &lc) {
    if (!cons || !lc.valid || lc.consolidatorInputAtomFilesEnd == 0) return nullptr;
    return static_cast<const void *const *>(
        readPtr(cons, lc.consolidatorInputAtomFilesEnd));
}

// Output DylibLoadInfo vector. Iterate with dylibLoadInfoStride().
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

inline size_t consolidatorInputAtomFileCount(ConsolidatorPtr cons,
                                             const LayoutConstants &lc) {
    auto begin = consolidatorInputAtomFilesBegin(cons, lc);
    auto end   = consolidatorInputAtomFilesEnd(cons, lc);
    if (!begin || !end || end < begin) return 0;
    return static_cast<size_t>(end - begin);
}

// DylibFileInfo for an input AtomFile* slot.
inline const void *inputAtomFileDylibFileInfo(const void *slot,
                                              const LayoutConstants &lc) {
    const void *file = inputAtomFilePtr(slot);
    if (!file) return nullptr;
    return fileDylibFileInfo(file, lc.fileVtableDylibFileInfo);
}

// Install name of a single input AtomFile slot.
inline const char *inputAtomFileInstallName(const void *slot,
                                            const LayoutConstants &lc) {
    return dylibInstallName(inputAtomFileDylibFileInfo(slot, lc));
}

// Iterate every input AtomFile. fn: bool(atomFile, dylibFileInfo).
template <typename Fn>
inline void forEachInputAtomFile(ConsolidatorPtr cons,
                                 const LayoutConstants &lc, Fn fn) {
    const void *const *begin = consolidatorInputAtomFilesBegin(cons, lc);
    const void *const *end   = consolidatorInputAtomFilesEnd(cons, lc);
    if (!begin || !end || end < begin) return;
    const uint8_t *p = reinterpret_cast<const uint8_t *>(begin);
    const uint8_t *e = reinterpret_cast<const uint8_t *>(end);
    for (; p < e; p += input_atomfile::kStride) {
        const void *file = inputAtomFilePtr(p);
        if (!file) continue;
        const void *dfi = fileDylibFileInfo(file, lc.fileVtableDylibFileInfo);
        if (!fn(file, dfi)) return;
    }
}

inline size_t consolidatorOutputDylibCount(ConsolidatorPtr cons,
                                           const LayoutConstants &lc) {
    auto begin = consolidatorOutputDylibsBegin(cons, lc);
    auto end   = consolidatorOutputDylibsEnd(cons, lc);
    if (!begin || !end || end < begin) return 0;
    return static_cast<size_t>(end - begin);
}

// Output dylib iteration. The vector holds DylibLoadInfo value elements
// whose byte stride varies by version. Use outputDylibSlotAt() to pull
// a slot; element count = byteSpan / dylibLoadInfoStride(v).
inline size_t consolidatorOutputDylibByteSpan(ConsolidatorPtr cons,
                                              const LayoutConstants &lc) {
    if (!cons || !lc.valid) return 0;
    const uint8_t *begin = static_cast<const uint8_t *>(
        readPtr(cons, lc.consolidatorOutputDylibsBegin));
    const uint8_t *end = static_cast<const uint8_t *>(
        readPtr(cons, lc.consolidatorOutputDylibsEnd));
    if (!begin || !end || end < begin) return 0;
    return static_cast<size_t>(end - begin);
}

inline size_t consolidatorOutputDylibTypedCount(ConsolidatorPtr cons,
                                                const LinkerVersion &v,
                                                const LayoutConstants &lc) {
    size_t span = consolidatorOutputDylibByteSpan(cons, lc);
    size_t stride = dylibLoadInfoStride(v);
    return stride ? span / stride : 0;
}

// Pointer to the i-th DylibLoadInfo slot; nullptr if OOB or invalid.
inline const void *outputDylibSlotAt(ConsolidatorPtr cons,
                                     const LinkerVersion &v,
                                     const LayoutConstants &lc,
                                     size_t i) {
    if (!cons || !lc.valid) return nullptr;
    size_t stride = dylibLoadInfoStride(v);
    if (stride == 0) return nullptr;
    size_t count  = consolidatorOutputDylibByteSpan(cons, lc) / stride;
    if (i >= count) return nullptr;
    const uint8_t *begin = static_cast<const uint8_t *>(
        readPtr(cons, lc.consolidatorOutputDylibsBegin));
    return begin ? begin + i * stride : nullptr;
}

// Install name of the i-th output dylib slot via its back-pointer to the
// input Dylib / AtomFile / DylibFileInfo. Null on any broken hop.
inline const char *outputDylibInstallName(ConsolidatorPtr cons,
                                          const LinkerVersion &v,
                                          const LayoutConstants &lc,
                                          size_t i) {
    const void *slot  = outputDylibSlotAt(cons, v, lc, i);
    if (!slot) return nullptr;
    const void *dylib = dylibLoadInfoDylib(slot);
    if (!dylib) return nullptr;
    const void *dfi = fileDylibFileInfo(dylib, lc.fileVtableDylibFileInfo);
    return dylibInstallName(dfi);
}

// Structural sanity check for a Consolidator. Rejects null, invalid or
// classic version, null Options*, or an implausible input vector.
inline bool validateConsolidator(ConsolidatorPtr cons,
                                 const LayoutConstants &lc) {
    if (!cons || !lc.valid) return false;

    OptionsPtr opts = consolidatorOptions(cons, lc);
    if (!opts) return false;

    auto begin = consolidatorInputAtomFilesBegin(cons, lc);
    auto end   = consolidatorInputAtomFilesEnd(cons, lc);
    if (!begin || !end || end < begin) return false;
    if (static_cast<size_t>(end - begin) > 100000) return false;

    return true;
}

} // namespace ld

#endif
