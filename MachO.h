// Copyright (c) 2026 Nikita Hmelnitkii. MIT License - see LICENSE.
//
// MachO.h - non-owning parsers for mmap'd Mach-O buffers. Fat slice
// selection, load-command iteration, segment/section lookup, symtab.

#ifndef LD_MACHO_H
#define LD_MACHO_H

#include "Mach.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach/machine.h>

namespace ld {
namespace macho {

// Byte range in an mmap'd file. Empty view when base is null.
struct FileView {
    const uint8_t *base;
    size_t         size;

    constexpr bool valid() const { return base != nullptr && size != 0; }
};

// Find the slice for `cpuType` in a thin or fat Mach-O buffer. Handles
// fat_magic / fat_cigam / fat_magic_64 / fat_cigam_64. Empty on miss.
inline FileView findSliceForCpuType(const uint8_t *base, size_t size,
                                     cpu_type_t cpuType) {
    if (!base || size < 4) return {};

    uint32_t magic;
    __builtin_memcpy(&magic, base, 4);

    if (magic == MH_MAGIC_64) {
        const auto *mh = reinterpret_cast<const struct mach_header_64 *>(base);
        if (mh->cputype == cpuType) return {base, size};
        return {};
    }
    if (magic == MH_MAGIC) {
        const auto *mh = reinterpret_cast<const struct mach_header *>(base);
        if (mh->cputype == cpuType) return {base, size};
        return {};
    }

    // fat_magic / fat_cigam (32-bit arch records)
    if (magic == FAT_MAGIC || magic == FAT_CIGAM) {
        const bool swap = (magic == FAT_CIGAM);
        const auto *fh = reinterpret_cast<const fat_header *>(base);
        const uint32_t narch = swap ? __builtin_bswap32(fh->nfat_arch)
                                    : fh->nfat_arch;
        const auto *archs = reinterpret_cast<const fat_arch *>(
            base + sizeof(fat_header));
        for (uint32_t i = 0; i < narch; ++i) {
            const cpu_type_t ct = swap ? static_cast<cpu_type_t>(
                __builtin_bswap32(static_cast<uint32_t>(archs[i].cputype)))
                                       : archs[i].cputype;
            if (ct != cpuType) continue;
            const uint32_t off = swap ? __builtin_bswap32(archs[i].offset) : archs[i].offset;
            const uint32_t sz  = swap ? __builtin_bswap32(archs[i].size)   : archs[i].size;
            if (off + sz > size) return {};
            return {base + off, sz};
        }
        return {};
    }

    // fat_magic_64 / fat_cigam_64 (64-bit arch records)
    if (magic == FAT_MAGIC_64 || magic == FAT_CIGAM_64) {
        const bool swap = (magic == FAT_CIGAM_64);
        const auto *fh = reinterpret_cast<const fat_header *>(base);
        const uint32_t narch = swap ? __builtin_bswap32(fh->nfat_arch)
                                    : fh->nfat_arch;
        const auto *archs = reinterpret_cast<const fat_arch_64 *>(
            base + sizeof(fat_header));
        for (uint32_t i = 0; i < narch; ++i) {
            const cpu_type_t ct = swap ? static_cast<cpu_type_t>(
                __builtin_bswap32(static_cast<uint32_t>(archs[i].cputype)))
                                       : archs[i].cputype;
            if (ct != cpuType) continue;
            const uint64_t off = swap ? __builtin_bswap64(archs[i].offset) : archs[i].offset;
            const uint64_t sz  = swap ? __builtin_bswap64(archs[i].size)   : archs[i].size;
            if (off + sz > size) return {};
            return {base + off, static_cast<size_t>(sz)};
        }
        return {};
    }

    return {};
}

inline FileView findArm64Slice(const uint8_t *base, size_t size) {
    return findSliceForCpuType(base, size, CPU_TYPE_ARM64);
}
inline FileView findX86_64Slice(const uint8_t *base, size_t size) {
    return findSliceForCpuType(base, size, CPU_TYPE_X86_64);
}
inline FileView findArm64_32Slice(const uint8_t *base, size_t size) {
    return findSliceForCpuType(base, size, CPU_TYPE_ARM64_32);
}

inline uint32_t machHeader64NCmds(const FileView &slice) {
    if (!slice.valid() || slice.size < sizeof(struct mach_header_64)) return 0;
    const auto *mh = reinterpret_cast<const struct mach_header_64 *>(slice.base);
    return mh->magic == MH_MAGIC_64 ? mh->ncmds : 0;
}

// Iterate LC_* commands in a thin 64-bit slice. fn: bool(load_command*).
// Stops on false return or an implausible cmdsize.
template <typename Fn>
inline void forEachLoadCommand(const FileView &slice, Fn fn) {
    if (!slice.valid() || slice.size < sizeof(struct mach_header_64)) return;
    const auto *mh = reinterpret_cast<const struct mach_header_64 *>(slice.base);
    if (mh->magic != MH_MAGIC_64) return;
    const uint8_t *cmd = slice.base + sizeof(struct mach_header_64);
    const uint8_t *end = slice.base + slice.size;
    for (uint32_t i = 0; i < mh->ncmds; ++i) {
        if (cmd + sizeof(struct load_command) > end) return;
        const auto *lc = reinterpret_cast<const struct load_command *>(cmd);
        if (lc->cmdsize < sizeof(struct load_command)) return;
        if (cmd + lc->cmdsize > end) return;
        if (!fn(lc)) return;
        cmd += lc->cmdsize;
    }
}

// First LC matching `cmdType`; compares on stripped value so raw LC_*
// or ld::LoadCmd works. Null on miss.
inline const struct load_command *findLoadCommand(const FileView &slice,
                                                   uint32_t cmdType) {
    const uint32_t wantStripped = loadCmdStripFlags(cmdType);
    const struct load_command *hit = nullptr;
    forEachLoadCommand(slice, [&](const struct load_command *lc) {
        if (loadCmdStripFlags(lc->cmd) == wantStripped) { hit = lc; return false; }
        return true;
    });
    return hit;
}

// LC_SEGMENT_64 whose segname matches (first 16 bytes). Null on miss.
inline const struct segment_command_64 *findSegment64(const FileView &slice,
                                                       const char *segName) {
    if (!segName) return nullptr;
    const struct segment_command_64 *hit = nullptr;
    forEachLoadCommand(slice, [&](const struct load_command *lc) {
        if (lc->cmd != LC_SEGMENT_64) return true;
        const auto *seg = reinterpret_cast<const struct segment_command_64 *>(lc);
        if (__builtin_strncmp(seg->segname, segName, 16) == 0) {
            hit = seg;
            return false;
        }
        return true;
    });
    return hit;
}

// __SEG,__sect pair lookup. Null on miss.
inline const struct section_64 *findSection64(const FileView &slice,
                                               const char *segName,
                                               const char *sectName) {
    const auto *seg = findSegment64(slice, segName);
    if (!seg || !sectName) return nullptr;
    const auto *sects = reinterpret_cast<const struct section_64 *>(seg + 1);
    for (uint32_t i = 0; i < seg->nsects; ++i) {
        if (__builtin_strncmp(sects[i].sectname, sectName, 16) == 0) {
            return &sects[i];
        }
    }
    return nullptr;
}

// LC_UUID 16-byte value; false when absent.
inline bool readUUID(const FileView &slice, uint8_t outUUID[16]) {
    const auto *lc = findLoadCommand(slice, LC_UUID);
    if (!lc) return false;
    const auto *u = reinterpret_cast<const struct uuid_command *>(lc);
    __builtin_memcpy(outUUID, u->uuid, 16);
    return true;
}

// LC_SYMTAB -> (syms, strtab, nsyms). False on miss or OOB.
inline bool readSymtab(const FileView &slice,
                       const struct nlist_64 **outSyms,
                       const char **outStrTab,
                       uint32_t *outNSyms) {
    const auto *lc = findLoadCommand(slice, LC_SYMTAB);
    if (!lc) return false;
    const auto *st = reinterpret_cast<const struct symtab_command *>(lc);
    if (st->symoff + st->nsyms * sizeof(struct nlist_64) > slice.size) return false;
    if (st->stroff + st->strsize > slice.size) return false;
    if (outSyms)   *outSyms   = reinterpret_cast<const struct nlist_64 *>(
                                    slice.base + st->symoff);
    if (outStrTab) *outStrTab = reinterpret_cast<const char *>(
                                    slice.base + st->stroff);
    if (outNSyms)  *outNSyms  = st->nsyms;
    return true;
}

// VM address -> file offset via LC_SEGMENT_64 walk. 0 on miss.
inline size_t vmToFileOffset(const FileView &slice, uint64_t vmaddr) {
    size_t off = 0;
    forEachLoadCommand(slice, [&](const struct load_command *lc) {
        if (lc->cmd != LC_SEGMENT_64) return true;
        const auto *seg = reinterpret_cast<const struct segment_command_64 *>(lc);
        if (vmaddr >= seg->vmaddr && vmaddr < seg->vmaddr + seg->vmsize) {
            off = static_cast<size_t>(vmaddr - seg->vmaddr) + seg->fileoff;
            return false;
        }
        return true;
    });
    return off;
}

// __TEXT segment VM base; 0 on miss.
inline uint64_t textVMAddr(const FileView &slice) {
    const auto *seg = findSegment64(slice, "__TEXT");
    return seg ? seg->vmaddr : 0;
}

// Parse LC_BUILD_VERSION into a PlatformVersion and emit each tool
// record via fn(BuildToolVersion). platform=unknown on miss.
template <typename Fn>
inline PlatformVersion readBuildVersion(const FileView &slice, Fn fn) {
    PlatformVersion out{};
    const auto *lc = findLoadCommand(slice, LC_BUILD_VERSION);
    if (!lc) return out;
    const auto *bv = reinterpret_cast<const struct build_version_command *>(lc);
    out.platform   = static_cast<Platform>(bv->platform);
    out.minVersion = Version32{bv->minos};
    out.sdkVersion = Version32{bv->sdk};
    const auto *tools = reinterpret_cast<const struct build_tool_version *>(bv + 1);
    for (uint32_t i = 0; i < bv->ntools; ++i) {
        BuildToolVersion t{};
        t.tool    = static_cast<BuildTool>(tools[i].tool);
        t.version = Version32{tools[i].version};
        fn(t);
    }
    return out;
}

// LC_ID_DYLIB -> install name + compat/current versions. False on miss.
inline bool readIdDylib(const FileView &slice,
                         const char **outName,
                         Version32 *outCompat,
                         Version32 *outCurrent) {
    const auto *lc = findLoadCommand(slice, LC_ID_DYLIB);
    if (!lc) return false;
    const auto *dl = reinterpret_cast<const struct dylib_command *>(lc);
    if (outName)    *outName    = reinterpret_cast<const char *>(lc) + dl->dylib.name.offset;
    if (outCompat)  *outCompat  = Version32{dl->dylib.compatibility_version};
    if (outCurrent) *outCurrent = Version32{dl->dylib.current_version};
    return true;
}

// Iterate LC_RPATH entries. fn: bool(const char *path).
template <typename Fn>
inline void forEachRpath(const FileView &slice, Fn fn) {
    forEachLoadCommand(slice, [&](const struct load_command *lc) {
        if (loadCmdStripFlags(lc->cmd) != (LC_RPATH & 0x7FFFFFFFu)) return true;
        const auto *rp = reinterpret_cast<const struct rpath_command *>(lc);
        const char *path = reinterpret_cast<const char *>(lc) + rp->path.offset;
        return fn(path);
    });
}

// Iterate dylib-load records (regular/weak/reexport/upward/lazy).
// fn: bool(load_command*, installName, DylibAttr-bit).
template <typename Fn>
inline void forEachDylibLoad(const FileView &slice, Fn fn) {
    forEachLoadCommand(slice, [&](const struct load_command *lc) {
        if (!loadCmdIsDylib(lc->cmd)) return true;
        const auto *dl = reinterpret_cast<const struct dylib_command *>(lc);
        const char *path = reinterpret_cast<const char *>(lc) + dl->dylib.name.offset;
        const uint8_t attrBit = loadCmdToDylibAttrBit(lc->cmd);
        return fn(lc, path, attrBit);
    });
}

// LC_SOURCE_VERSION raw u64. Decode via decodeSourceVersion.
inline uint64_t readSourceVersion(const FileView &slice) {
    const auto *lc = findLoadCommand(slice, LC_SOURCE_VERSION);
    if (!lc) return 0;
    const auto *sv = reinterpret_cast<const struct source_version_command *>(lc);
    return sv->version;
}

// Decoded LC_SOURCE_VERSION: a.b.c.d.e with bit layout
// [a:24][b:10][c:10][d:10][e:10] (MSB..LSB).
struct SourceVersion {
    uint32_t a;  // top 24 bits
    uint16_t b;  // next 10 bits
    uint16_t c;  // next 10 bits
    uint16_t d;  // next 10 bits
    uint16_t e;  // low 10 bits
};

inline SourceVersion decodeSourceVersion(uint64_t v) {
    SourceVersion out{};
    out.e = static_cast<uint16_t>(v & 0x3FF);
    out.d = static_cast<uint16_t>((v >> 10) & 0x3FF);
    out.c = static_cast<uint16_t>((v >> 20) & 0x3FF);
    out.b = static_cast<uint16_t>((v >> 30) & 0x3FF);
    out.a = static_cast<uint32_t>((v >> 40) & 0xFFFFFF);
    return out;
}

// Linear byte search. Use for one-shot lookups only.
inline const uint8_t *findBytes(const FileView &slice,
                                 const void *needle, size_t needleLen) {
    if (!slice.valid() || needleLen == 0 || slice.size < needleLen) return nullptr;
    for (size_t i = 0; i + needleLen <= slice.size; ++i) {
        if (__builtin_memcmp(slice.base + i, needle, needleLen) == 0) {
            return slice.base + i;
        }
    }
    return nullptr;
}

} // namespace macho
} // namespace ld

#endif
