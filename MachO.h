// Copyright (c) 2026 Nikita Hmelnitkii. MIT License - see LICENSE.
//
// MachO.h - Mach-O format types, load-command iteration, segment and
// section lookup, dylib install-name parsing, and fat-slice selection.
// Numeric values mirror <mach-o/loader.h> and <mach-o/fat.h>; enums
// are renamed so both representations can coexist in one translation
// unit without macro collisions.

#ifndef LD_MACHO_H
#define LD_MACHO_H

#include "Primitives.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <mach/machine.h>
#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>

namespace ld {

// Mach-O CPU slice identity. Subtype lives in the low 24 bits; the top
// byte carries PAC ABI information on arm64e.
struct Architecture {
    cpu_type_t    cpuType;
    cpu_subtype_t cpuSubtype;
    const char   *name;
    const char   *tripleArch;

    constexpr bool isArm64()    const { return cpuType == CPU_TYPE_ARM64; }
    constexpr bool isArm64_32() const { return cpuType == CPU_TYPE_ARM64_32; }
    constexpr bool isX86_64()   const { return cpuType == CPU_TYPE_X86_64; }
    constexpr bool isI386()     const { return cpuType == CPU_TYPE_I386; }
    constexpr bool isArm32()    const { return cpuType == CPU_TYPE_ARM; }
    constexpr bool isArm64e()   const {
        return cpuType == CPU_TYPE_ARM64
            && (cpuSubtype & ~CPU_SUBTYPE_MASK) == CPU_SUBTYPE_ARM64E;
    }
};

namespace arch {
    inline constexpr Architecture kArm64   = { CPU_TYPE_ARM64,  CPU_SUBTYPE_ARM64_ALL,  "arm64",   "arm64"   };
    inline constexpr Architecture kArm64e  = { CPU_TYPE_ARM64,  CPU_SUBTYPE_ARM64E,     "arm64e",  "arm64e"  };
    inline constexpr Architecture kX86_64  = { CPU_TYPE_X86_64, CPU_SUBTYPE_X86_64_ALL, "x86_64",  "x86_64"  };
    inline constexpr Architecture kX86_64h = { CPU_TYPE_X86_64, CPU_SUBTYPE_X86_64_H,   "x86_64h", "x86_64h" };
    inline constexpr Architecture kI386    = { CPU_TYPE_I386,   CPU_SUBTYPE_I386_ALL,   "i386",    "i386"    };
    inline constexpr Architecture kArmv7   = { CPU_TYPE_ARM,    CPU_SUBTYPE_ARM_V7,     "armv7",   "thumbv7"  };
    inline constexpr Architecture kArmv7s  = { CPU_TYPE_ARM,    CPU_SUBTYPE_ARM_V7S,    "armv7s",  "thumbv7s" };
    inline constexpr Architecture kArmv7k  = { CPU_TYPE_ARM,    CPU_SUBTYPE_ARM_V7K,    "armv7k",  "thumbv7k" };
}

// Resolve a (cputype, cpusubtype) pair to a stable Architecture*.
// Returns nullptr for unknown combinations.
inline const Architecture *architectureFromMachO(cpu_type_t t, cpu_subtype_t s) {
    const cpu_subtype_t base = s & ~CPU_SUBTYPE_MASK;
    switch (t) {
    case CPU_TYPE_ARM64:
        return base == CPU_SUBTYPE_ARM64E ? &arch::kArm64e : &arch::kArm64;
    case CPU_TYPE_X86_64:
        return base == CPU_SUBTYPE_X86_64_H ? &arch::kX86_64h : &arch::kX86_64;
    case CPU_TYPE_I386: return &arch::kI386;
    case CPU_TYPE_ARM:
        switch (base) {
        case CPU_SUBTYPE_ARM_V7:  return &arch::kArmv7;
        case CPU_SUBTYPE_ARM_V7S: return &arch::kArmv7s;
        case CPU_SUBTYPE_ARM_V7K: return &arch::kArmv7k;
        default: return nullptr;
        }
    default: return nullptr;
    }
}

// LC_BUILD_VERSION platform. 1..24 appear on disk; freestanding and any
// are linker-internal sentinels.
enum class Platform : uint32_t {
    unknown                = 0,
    macOS                  = 1,
    iOS                    = 2,
    tvOS                   = 3,
    watchOS                = 4,
    bridgeOS               = 5,
    macCatalyst            = 6,
    iOS_simulator          = 7,
    tvOS_simulator         = 8,
    watchOS_simulator      = 9,
    driverKit              = 10,
    visionOS               = 11,
    visionOS_simulator     = 12,
    firmware               = 13,
    sepOS                  = 14,
    macOS_exclaveCore      = 15,
    macOS_exclaveKit       = 16,
    iOS_exclaveCore        = 17,
    iOS_exclaveKit         = 18,
    tvOS_exclaveCore       = 19,
    tvOS_exclaveKit        = 20,
    watchOS_exclaveCore    = 21,
    watchOS_exclaveKit     = 22,
    visionOS_exclaveCore   = 23,
    visionOS_exclaveKit    = 24,
    freestanding           = 100,
    any                    = 0xFFFFFFFF,
};

inline const char *platformName(Platform p) {
    switch (p) {
    case Platform::unknown:             return "unknown";
    case Platform::macOS:               return "macOS";
    case Platform::iOS:                 return "iOS";
    case Platform::tvOS:                return "tvOS";
    case Platform::watchOS:             return "watchOS";
    case Platform::bridgeOS:            return "bridgeOS";
    case Platform::macCatalyst:         return "macCatalyst";
    case Platform::iOS_simulator:       return "iOS-simulator";
    case Platform::tvOS_simulator:      return "tvOS-simulator";
    case Platform::watchOS_simulator:   return "watchOS-simulator";
    case Platform::driverKit:           return "driverKit";
    case Platform::visionOS:            return "visionOS";
    case Platform::visionOS_simulator:  return "visionOS-simulator";
    case Platform::firmware:            return "firmware";
    case Platform::sepOS:               return "sepOS";
    case Platform::macOS_exclaveCore:   return "macOS-exclaveCore";
    case Platform::macOS_exclaveKit:    return "macOS-exclaveKit";
    case Platform::iOS_exclaveCore:     return "iOS-exclaveCore";
    case Platform::iOS_exclaveKit:      return "iOS-exclaveKit";
    case Platform::tvOS_exclaveCore:    return "tvOS-exclaveCore";
    case Platform::tvOS_exclaveKit:     return "tvOS-exclaveKit";
    case Platform::watchOS_exclaveCore: return "watchOS-exclaveCore";
    case Platform::watchOS_exclaveKit:  return "watchOS-exclaveKit";
    case Platform::visionOS_exclaveCore:return "visionOS-exclaveCore";
    case Platform::visionOS_exclaveKit: return "visionOS-exclaveKit";
    case Platform::freestanding:        return "freestanding";
    case Platform::any:                 return "any";
    }
    return "invalid";
}

inline bool platformIsSimulator(Platform p) {
    return p == Platform::iOS_simulator
        || p == Platform::tvOS_simulator
        || p == Platform::watchOS_simulator
        || p == Platform::visionOS_simulator;
}

inline bool platformIsExclave(Platform p) {
    switch (p) {
    case Platform::macOS_exclaveCore:
    case Platform::macOS_exclaveKit:
    case Platform::iOS_exclaveCore:
    case Platform::iOS_exclaveKit:
    case Platform::tvOS_exclaveCore:
    case Platform::tvOS_exclaveKit:
    case Platform::watchOS_exclaveCore:
    case Platform::watchOS_exclaveKit:
    case Platform::visionOS_exclaveCore:
    case Platform::visionOS_exclaveKit:
        return true;
    default:
        return false;
    }
}

// Packed XXXX.YY.ZZ version as 0xXXXXYYZZ. LC_ID_DYLIB and
// LC_BUILD_VERSION both use this encoding.
struct Version32 {
    uint32_t raw;

    constexpr Version32() : raw(0) {}
    constexpr explicit Version32(uint32_t r) : raw(r) {}
    constexpr Version32(uint16_t major, uint8_t minor, uint8_t patch)
        : raw(static_cast<uint32_t>((major << 16) | (minor << 8) | patch)) {}

    constexpr uint16_t major() const { return static_cast<uint16_t>(raw >> 16); }
    constexpr uint8_t  minor() const { return static_cast<uint8_t>((raw >> 8) & 0xFF); }
    constexpr uint8_t  patch() const { return static_cast<uint8_t>(raw & 0xFF); }

    constexpr bool operator==(Version32 o) const { return raw == o.raw; }
    constexpr bool operator!=(Version32 o) const { return raw != o.raw; }
    constexpr bool operator< (Version32 o) const { return raw <  o.raw; }
    constexpr bool operator<=(Version32 o) const { return raw <= o.raw; }
    constexpr bool operator> (Version32 o) const { return raw >  o.raw; }
    constexpr bool operator>=(Version32 o) const { return raw >= o.raw; }
};
static_assert(sizeof(Version32) == 4, "");

// operator== compares platform only (build-version record set key).
struct PlatformVersion {
    Platform  platform;
    Version32 minVersion;
    Version32 sdkVersion;

    constexpr PlatformVersion() : platform(Platform::unknown), minVersion(), sdkVersion() {}
    constexpr explicit PlatformVersion(Platform p) : platform(p), minVersion(), sdkVersion() {}
    constexpr PlatformVersion(Platform p, Version32 v) : platform(p), minVersion(v), sdkVersion(v) {}
    constexpr PlatformVersion(Platform p, Version32 m, Version32 s)
        : platform(p), minVersion(m), sdkVersion(s) {}

    constexpr bool operator==(const PlatformVersion &o) const { return platform == o.platform; }
    constexpr bool operator!=(const PlatformVersion &o) const { return !(*this == o); }
    constexpr bool operator< (const PlatformVersion &o) const { return platform <  o.platform; }
};

// Linker output kind. Dense 0..7; not equal to MH_* values.
enum OutputKind : uint32_t {
    kDynamicExecutable = 0,   // MH_EXECUTE + dyld
    kStaticExecutable  = 1,   // MH_EXECUTE static
    kDynamicLibrary    = 2,   // MH_DYLIB
    kDynamicBundle     = 3,   // MH_BUNDLE
    kObjectFile        = 4,   // MH_OBJECT
    kDyld              = 5,   // MH_DYLINKER
    kPreload           = 6,   // MH_PRELOAD
    kKextBundle        = 7,   // MH_KEXT_BUNDLE
};

inline const char *outputKindName(OutputKind k) {
    switch (k) {
    case kDynamicExecutable: return "executable";
    case kStaticExecutable:  return "static-executable";
    case kDynamicLibrary:    return "dylib";
    case kDynamicBundle:     return "bundle";
    case kObjectFile:        return "object";
    case kDyld:              return "dyld";
    case kPreload:           return "preload";
    case kKextBundle:        return "kext";
    }
    return "unknown";
}

// LC_BUILD_VERSION tool record (TOOL_* in <mach-o/loader.h>).
enum class BuildTool : uint32_t {
    unknown = 0,
    clang   = 1,
    swift   = 2,
    ld      = 3,
    lld     = 4,
};

struct BuildToolVersion {
    BuildTool tool;
    Version32 version;
};
static_assert(sizeof(BuildToolVersion) == 8, "");

// Code-signature blob magic (CSMAGIC_*).
enum class CSMagic : uint32_t {
    CodeDirectory     = 0xfade0c02,
    EmbeddedSignature = 0xfade0cc0,
    DetachedSignature = 0xfade0cc1,
    Requirement       = 0xfade0c00,
    Requirements      = 0xfade0c01,
    Entitlements      = 0xfade7171,
    EntitlementsDER   = 0xfade7172,
    BlobWrapper       = 0xfade0b01,
};

// CSCodeDirectory::hashType.
enum class CSHashType : uint8_t {
    none         = 0,
    sha1         = 1,
    sha256       = 2,
    sha256_trunc = 3,
    sha384       = 4,
    sha512       = 5,
};

// VM protection bits used by segment_command_64::{max,init}prot.
// Values match <mach/vm_prot.h>.
namespace vmprot {
    inline constexpr uint8_t kNone    = 0;
    inline constexpr uint8_t kRead    = 1;
    inline constexpr uint8_t kWrite   = 2;
    inline constexpr uint8_t kExecute = 4;
}

// Segment flags stored in segment_command_64::flags (SG_* in loader.h).
namespace sgflag {
    inline constexpr uint32_t kHighVM             = 0x01;
    inline constexpr uint32_t kFvmLib             = 0x02;
    inline constexpr uint32_t kNoReloc            = 0x04;
    inline constexpr uint32_t kProtectedVersion1  = 0x08;
    inline constexpr uint32_t kReadOnly           = 0x10;
}

// Load-command type tags. Names differ from the LC_* macros so both
// can coexist; numeric values are identical once LC_REQ_DYLD is folded
// in via loadCmdStripFlags.
enum class LoadCmd : uint32_t {
    ReqDyldFlag          = 0x80000000u,

    Segment              = 0x01,
    Segment64            = 0x19,
    Symtab               = 0x02,
    Symseg               = 0x03,
    Thread               = 0x04,
    UnixThread           = 0x05,
    Dysymtab             = 0x0B,
    LoadDylib            = 0x0C,
    IdDylib              = 0x0D,
    LoadDylinker         = 0x0E,
    IdDylinker           = 0x0F,
    PreboundDylib        = 0x10,
    Routines             = 0x11,
    Routines64           = 0x1A,
    SubFramework         = 0x12,
    SubClient            = 0x14,
    SubUmbrella          = 0x13,
    SubLibrary           = 0x15,
    TwolevelHints        = 0x16,
    PrebindCksum         = 0x17,
    Uuid                 = 0x1B,
    CodeSignature        = 0x1D,
    SegmentSplitInfo     = 0x1E,
    LazyLoadDylib        = 0x20,
    EncryptionInfo       = 0x21,
    EncryptionInfo64     = 0x2C,
    FunctionStarts       = 0x26,
    DyldEnvironment      = 0x27,
    DataInCode           = 0x29,
    SourceVersion        = 0x2A,
    DylibCodeSignDrs     = 0x2B,
    LinkerOption         = 0x2D,
    LinkerOptimizationHint = 0x2E,
    Note                 = 0x31,
    BuildVersion         = 0x32,
    Main                 = 0x28u | 0x80000000u,
    LoadWeakDylib        = 0x18u | 0x80000000u,
    ReexportDylib        = 0x1Fu | 0x80000000u,
    LoadUpwardDylib      = 0x23u | 0x80000000u,
    Rpath                = 0x1Cu | 0x80000000u,
    DyldInfo             = 0x22,
    DyldInfoOnly         = 0x22u | 0x80000000u,
    DyldExportsTrie      = 0x33u | 0x80000000u,
    DyldChainedFixups    = 0x34u | 0x80000000u,
    FilesetEntry         = 0x35u | 0x80000000u,
    AtomInfo             = 0x36,
    FunctionVariants     = 0x37,
    FunctionVariantFixups= 0x38,
    TargetTriple         = 0x39,
    VersionMinMacosx     = 0x24,
    VersionMinIphoneos   = 0x25,
    VersionMinWatchos    = 0x30,
    VersionMinTvos       = 0x2F,
};

inline constexpr bool loadCmdRequiresDyld(uint32_t cmd) {
    return (cmd & 0x80000000u) != 0;
}
inline constexpr uint32_t loadCmdStripFlags(uint32_t cmd) {
    return cmd & 0x7FFFFFFFu;
}

// True for any LC_LOAD_* variant that references a dylib file.
inline constexpr bool loadCmdIsDylib(uint32_t cmd) {
    const uint32_t c = loadCmdStripFlags(cmd);
    return c == static_cast<uint32_t>(LoadCmd::LoadDylib)
        || c == loadCmdStripFlags(static_cast<uint32_t>(LoadCmd::LoadWeakDylib))
        || c == loadCmdStripFlags(static_cast<uint32_t>(LoadCmd::ReexportDylib))
        || c == loadCmdStripFlags(static_cast<uint32_t>(LoadCmd::LoadUpwardDylib))
        || c == static_cast<uint32_t>(LoadCmd::LazyLoadDylib);
}

// Attribute bit matching the LC_LOAD_* variant (see DylibAttr in Consolidator.h).
// Returns 0 for LC_LOAD_DYLIB or an unrelated command.
inline constexpr uint8_t loadCmdToDylibAttrBit(uint32_t cmd) {
    const uint32_t c = loadCmdStripFlags(cmd);
    if (c == static_cast<uint32_t>(LoadCmd::LoadDylib))                          return 0x00;
    if (c == loadCmdStripFlags(static_cast<uint32_t>(LoadCmd::LoadWeakDylib)))   return 0x01;
    if (c == loadCmdStripFlags(static_cast<uint32_t>(LoadCmd::ReexportDylib)))   return 0x02;
    if (c == loadCmdStripFlags(static_cast<uint32_t>(LoadCmd::LoadUpwardDylib))) return 0x04;
    return 0x00;
}

// Install-name parsing. Anchor prefix plus body; frameworks recognised
// structurally via "/<name>.framework/" inside the body.
enum class InstallNameAnchor : uint8_t {
    None            = 0,
    Rpath           = 1,
    LoaderPath      = 2,
    ExecutablePath  = 3,
};

enum class InstallNameKind : uint8_t {
    Invalid   = 0,
    Loose     = 1,   // regular dylib path
    Framework = 2,   // Foo.framework/[Versions/V/]Foo
};

// Non-owning view into a parsed install name. All CString members
// alias the original path storage.
struct DylibInstallName {
    const char       *path;
    InstallNameKind   kind;
    InstallNameAnchor anchor;
    CString           body;
    CString           frameworkName;
    CString           frameworkVersion;
    CString           frameworkInstallDir;
    CString           leafName;

    constexpr bool isFramework() const { return kind == InstallNameKind::Framework; }
    constexpr bool isLoose()     const { return kind == InstallNameKind::Loose;     }
    constexpr bool hasAnchor()   const { return anchor != InstallNameAnchor::None;  }
};

namespace internal {
    struct AnchorDescriptor { const char *prefix; uint8_t len; InstallNameAnchor kind; };
    inline constexpr AnchorDescriptor kAnchorRpath      { "@rpath/",           7,  InstallNameAnchor::Rpath };
    inline constexpr AnchorDescriptor kAnchorLoader     { "@loader_path/",     13, InstallNameAnchor::LoaderPath };
    inline constexpr AnchorDescriptor kAnchorExecutable { "@executable_path/", 17, InstallNameAnchor::ExecutablePath };
}

inline bool isRpathRelative(const char *installName) {
    return installName && __builtin_memcmp(installName, internal::kAnchorRpath.prefix, internal::kAnchorRpath.len) == 0;
}
inline bool isLoaderRelative(const char *installName) {
    return installName && __builtin_memcmp(installName, internal::kAnchorLoader.prefix, internal::kAnchorLoader.len) == 0;
}
inline bool isExecutableRelative(const char *installName) {
    return installName && __builtin_memcmp(installName, internal::kAnchorExecutable.prefix, internal::kAnchorExecutable.len) == 0;
}
inline bool installNameHasAnchor(const char *installName) {
    return isRpathRelative(installName) || isLoaderRelative(installName) || isExecutableRelative(installName);
}

inline internal::AnchorDescriptor classifyInstallNameAnchor(const char *installName) {
    if (!installName) return { "", 0, InstallNameAnchor::None };
    if (isRpathRelative(installName))      return internal::kAnchorRpath;
    if (isLoaderRelative(installName))     return internal::kAnchorLoader;
    if (isExecutableRelative(installName)) return internal::kAnchorExecutable;
    return { "", 0, InstallNameAnchor::None };
}

inline const char *stripInstallNameAnchor(const char *installName) {
    auto a = classifyInstallNameAnchor(installName);
    return installName ? installName + a.len : nullptr;
}

// Framework classification requires leaf name to match the name before
// .framework/; any mismatch falls through to Loose (matches ld64).
inline DylibInstallName parseDylibInstallName(const char *installName) {
    DylibInstallName out = {};
    out.path   = installName;
    out.kind   = InstallNameKind::Invalid;
    out.anchor = InstallNameAnchor::None;

    if (!installName || !*installName) return out;

    auto anchor = classifyInstallNameAnchor(installName);
    out.anchor  = anchor.kind;

    const char *body    = installName + anchor.len;
    const size_t bodyLen = __builtin_strlen(body);
    const char *bodyEnd = body + bodyLen;
    out.body = CString{body, bodyLen};

    const char *lastSlash = lastSlashInRange(body, bodyEnd);
    const char *leafBegin = lastSlash ? lastSlash + 1 : body;
    if (leafBegin == bodyEnd) return out;
    out.leafName = CString{leafBegin, static_cast<size_t>(bodyEnd - leafBegin)};

    static constexpr char kFrameworkMarker[]    = ".framework/";
    static constexpr size_t kFrameworkMarkerLen = sizeof(kFrameworkMarker) - 1;

    const char *marker = findSubstringInRange(body, bodyEnd,
                                              kFrameworkMarker, kFrameworkMarkerLen);
    if (marker) {
        const char *nameBegin = lastSlashInRange(body, marker);
        nameBegin = nameBegin ? nameBegin + 1 : body;
        const size_t nameLen = static_cast<size_t>(marker - nameBegin);

        if (nameLen > 0
            && out.leafName.len == nameLen
            && __builtin_memcmp(out.leafName.data, nameBegin, nameLen) == 0)
        {
            out.kind                = InstallNameKind::Framework;
            out.frameworkName       = CString{nameBegin, nameLen};
            out.frameworkInstallDir = CString{body, static_cast<size_t>(nameBegin - body)};

            const char *afterMarker = marker + kFrameworkMarkerLen;
            static constexpr char kVersionsMarker[]    = "Versions/";
            static constexpr size_t kVersionsMarkerLen = sizeof(kVersionsMarker) - 1;
            if (static_cast<size_t>(leafBegin - afterMarker) > kVersionsMarkerLen
                && __builtin_memcmp(afterMarker, kVersionsMarker,
                                    kVersionsMarkerLen) == 0)
            {
                const char *verBegin = afterMarker + kVersionsMarkerLen;
                const char *verEnd   = leafBegin - 1;
                if (verEnd > verBegin) {
                    out.frameworkVersion = CString{verBegin, static_cast<size_t>(verEnd - verBegin)};
                }
            }
            return out;
        }
    }

    out.kind = InstallNameKind::Loose;
    return out;
}

namespace mach_o {

// Non-owning view into an mmap'd Mach-O buffer.
struct FileView {
    const uint8_t *base;
    size_t         size;

    constexpr bool valid() const { return base != nullptr && size != 0; }
};

// Thin or fat buffer, any magic/endian. Empty view on miss.
inline FileView findSliceForCpuType(const uint8_t *base, size_t size,
                                    cpu_type_t cpuType) {
    if (!base || size < 4) return {};

    uint32_t magic;
    __builtin_memcpy(&magic, base, 4);

    if (magic == MH_MAGIC_64) {
        const auto *mh = reinterpret_cast<const struct mach_header_64 *>(base);
        return mh->cputype == cpuType ? FileView{base, size} : FileView{};
    }
    if (magic == MH_MAGIC) {
        const auto *mh = reinterpret_cast<const struct mach_header *>(base);
        return mh->cputype == cpuType ? FileView{base, size} : FileView{};
    }

    if (magic == FAT_MAGIC || magic == FAT_CIGAM) {
        const bool swap = (magic == FAT_CIGAM);
        const auto *fh = reinterpret_cast<const fat_header *>(base);
        const uint32_t narch = swap ? __builtin_bswap32(fh->nfat_arch) : fh->nfat_arch;
        const auto *archs = reinterpret_cast<const fat_arch *>(base + sizeof(fat_header));
        for (uint32_t i = 0; i < narch; ++i) {
            const cpu_type_t ct = swap
                ? static_cast<cpu_type_t>(__builtin_bswap32(static_cast<uint32_t>(archs[i].cputype)))
                : archs[i].cputype;
            if (ct != cpuType) continue;
            const uint32_t off = swap ? __builtin_bswap32(archs[i].offset) : archs[i].offset;
            const uint32_t sz  = swap ? __builtin_bswap32(archs[i].size)   : archs[i].size;
            if (off + sz > size) return {};
            return {base + off, sz};
        }
        return {};
    }

    if (magic == FAT_MAGIC_64 || magic == FAT_CIGAM_64) {
        const bool swap = (magic == FAT_CIGAM_64);
        const auto *fh = reinterpret_cast<const fat_header *>(base);
        const uint32_t narch = swap ? __builtin_bswap32(fh->nfat_arch) : fh->nfat_arch;
        const auto *archs = reinterpret_cast<const fat_arch_64 *>(base + sizeof(fat_header));
        for (uint32_t i = 0; i < narch; ++i) {
            const cpu_type_t ct = swap
                ? static_cast<cpu_type_t>(__builtin_bswap32(static_cast<uint32_t>(archs[i].cputype)))
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

// Iterate load commands of a thin 64-bit slice. Callback returns false
// to stop early. Malformed cmdsize terminates iteration.
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

// Return the first load command whose stripped type matches `cmdType`.
// Accepts raw LC_* macros or LoadCmd enum values. Null on miss.
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

// Copy the LC_UUID payload. False when the command is absent.
inline bool readUUID(const FileView &slice, uint8_t outUUID[16]) {
    const auto *lc = findLoadCommand(slice, LC_UUID);
    if (!lc) return false;
    const auto *u = reinterpret_cast<const struct uuid_command *>(lc);
    __builtin_memcpy(outUUID, u->uuid, 16);
    return true;
}

// Resolve LC_SYMTAB into pointers + count. Bounds-checks against the
// slice size. Any out parameter may be null.
inline bool readSymtab(const FileView &slice,
                       const struct nlist_64 **outSyms,
                       const char **outStrTab,
                       uint32_t *outNSyms) {
    const auto *lc = findLoadCommand(slice, LC_SYMTAB);
    if (!lc) return false;
    const auto *st = reinterpret_cast<const struct symtab_command *>(lc);
    if (st->symoff + st->nsyms * sizeof(struct nlist_64) > slice.size) return false;
    if (st->stroff + st->strsize > slice.size) return false;
    if (outSyms)   *outSyms   = reinterpret_cast<const struct nlist_64 *>(slice.base + st->symoff);
    if (outStrTab) *outStrTab = reinterpret_cast<const char *>(slice.base + st->stroff);
    if (outNSyms)  *outNSyms  = st->nsyms;
    return true;
}

// 0 on miss; ambiguous with a true offset of 0, confirm separately.
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

inline uint64_t textVMAddr(const FileView &slice) {
    const auto *seg = findSegment64(slice, "__TEXT");
    return seg ? seg->vmaddr : 0;
}

// Parse LC_BUILD_VERSION. Emits each tool record via fn(BuildToolVersion).
// Returns PlatformVersion{Platform::unknown} when the command is absent.
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

// Resolve LC_ID_DYLIB into install name + compat/current versions.
// False when the command is absent (e.g. executables and bundles).
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

template <typename Fn>
inline void forEachRpath(const FileView &slice, Fn fn) {
    forEachLoadCommand(slice, [&](const struct load_command *lc) {
        if (loadCmdStripFlags(lc->cmd) != (LC_RPATH & 0x7FFFFFFFu)) return true;
        const auto *rp = reinterpret_cast<const struct rpath_command *>(lc);
        const char *path = reinterpret_cast<const char *>(lc) + rp->path.offset;
        return fn(path);
    });
}

// Iterate every dylib-load command (regular/weak/reexport/upward/lazy).
// The DylibAttr bit mirrors the variant; LC_LOAD_DYLIB reports 0.
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

// Raw LC_SOURCE_VERSION value. Zero when absent. Decode with
// decodeSourceVersion.
inline uint64_t readSourceVersion(const FileView &slice) {
    const auto *lc = findLoadCommand(slice, LC_SOURCE_VERSION);
    if (!lc) return 0;
    const auto *sv = reinterpret_cast<const struct source_version_command *>(lc);
    return sv->version;
}

// Decoded LC_SOURCE_VERSION: a.b.c.d.e packed as [a:24][b:10][c:10][d:10][e:10].
struct SourceVersion {
    uint32_t a;
    uint16_t b;
    uint16_t c;
    uint16_t d;
    uint16_t e;
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

// Linear byte search across the slice. O(size); intended for one-shot
// marker lookups, not for hot paths.
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

} // namespace mach_o

} // namespace ld

#endif
