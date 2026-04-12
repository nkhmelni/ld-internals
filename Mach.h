// Copyright (c) 2026 Nikita Hmelnitkii. MIT License - see LICENSE.
//
// Mach.h - Mach-O constants. Values match <mach-o/loader.h>.

#ifndef LD_MACH_H
#define LD_MACH_H

#include <cstdint>

#include <mach/machine.h>

namespace ld {

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
    // Sentinels - not written to LC_BUILD_VERSION.
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

// Packed XXXX.YY.ZZ -> 0xXXXXYYZZ. Used by LC_ID_DYLIB, LC_BUILD_VERSION.
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

// operator== compares platform only (platform is the set key).
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
    // PAC ABI version lives in the high byte.
    constexpr bool isArm64e() const {
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
    inline constexpr Architecture kArmv7   = { CPU_TYPE_ARM, CPU_SUBTYPE_ARM_V7,  "armv7",  "thumbv7"  };
    inline constexpr Architecture kArmv7s  = { CPU_TYPE_ARM, CPU_SUBTYPE_ARM_V7S, "armv7s", "thumbv7s" };
    inline constexpr Architecture kArmv7k  = { CPU_TYPE_ARM, CPU_SUBTYPE_ARM_V7K, "armv7k", "thumbv7k" };
}

// (cputype, cpusubtype) -> Architecture*; nullptr on unknown.
inline const Architecture *architectureFromMachO(cpu_type_t t, cpu_subtype_t s) {
    const cpu_subtype_t base = s & ~CPU_SUBTYPE_MASK;
    switch (t) {
    case CPU_TYPE_ARM64:
        return base == CPU_SUBTYPE_ARM64E ? &arch::kArm64e : &arch::kArm64;
    case CPU_TYPE_X86_64:
        return base == CPU_SUBTYPE_X86_64_H ? &arch::kX86_64h : &arch::kX86_64;
    case CPU_TYPE_I386:   return &arch::kI386;
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

enum OutputKind : uint32_t {
    kDynamicExecutable = 0,   // MH_EXECUTE + dyld
    kStaticExecutable  = 1,
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

// CSCodeDirectory.hashType values.
enum class CSHashType : uint8_t {
    none         = 0,
    sha1         = 1,
    sha256       = 2,
    sha256_trunc = 3,
    sha384       = 4,
    sha512       = 5,
};

// Load-command type IDs. Names differ from <mach-o/loader.h> LC_* macros
// to let both coexist in one TU; numeric values match. Bit 0x80000000 is
// LC_REQ_DYLD - dyld fails the load when an unknown command has it set.
enum class LoadCmd : uint32_t {
    ReqDyldFlag          = 0x80000000u,

    Segment64            = 0x19,
    Symtab               = 0x02,
    Dysymtab             = 0x0B,
    LoadDylib            = 0x0C,
    IdDylib              = 0x0D,
    LoadDylinker         = 0x0E,
    IdDylinker           = 0x0F,
    PreboundDylib        = 0x10,
    Routines64           = 0x1A,
    SubFramework         = 0x12,
    SubClient            = 0x14,
    SubUmbrella          = 0x13,
    SubLibrary           = 0x15,
    TwolevelHints        = 0x16,
    Uuid                 = 0x1B,
    CodeSignature        = 0x1D,
    SegmentSplitInfo     = 0x1E,
    LazyLoadDylib        = 0x20,
    EncryptionInfo64     = 0x2C,
    FunctionStarts       = 0x26,
    DataInCode           = 0x29,
    SourceVersion        = 0x2A,
    DylibCodeSignDrs     = 0x2B,
    LinkerOption         = 0x2D,
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

// True for LoadDylib / LoadWeakDylib / ReexportDylib / LoadUpwardDylib /
// LazyLoadDylib. Works on raw LC_* macros or LoadCmd enum values.
inline constexpr bool loadCmdIsDylib(uint32_t cmd) {
    const uint32_t c = loadCmdStripFlags(cmd);
    return c == static_cast<uint32_t>(LoadCmd::LoadDylib)
        || c == loadCmdStripFlags(static_cast<uint32_t>(LoadCmd::LoadWeakDylib))
        || c == loadCmdStripFlags(static_cast<uint32_t>(LoadCmd::ReexportDylib))
        || c == loadCmdStripFlags(static_cast<uint32_t>(LoadCmd::LoadUpwardDylib))
        || c == static_cast<uint32_t>(LoadCmd::LazyLoadDylib);
}

// Equivalent DylibAttr bit; 0 = regular LoadDylib.
inline constexpr uint8_t loadCmdToDylibAttrBit(uint32_t cmd) {
    const uint32_t c = loadCmdStripFlags(cmd);
    if (c == static_cast<uint32_t>(LoadCmd::LoadDylib))                               return 0x00;
    if (c == loadCmdStripFlags(static_cast<uint32_t>(LoadCmd::LoadWeakDylib)))        return 0x01;
    if (c == loadCmdStripFlags(static_cast<uint32_t>(LoadCmd::ReexportDylib)))        return 0x02;
    if (c == loadCmdStripFlags(static_cast<uint32_t>(LoadCmd::LoadUpwardDylib)))      return 0x04;
    return 0x00;
}

// VM protection bits (<mach/vm_prot.h>).
namespace vmprot {
    inline constexpr uint8_t kNone    = 0;
    inline constexpr uint8_t kRead    = 1;
    inline constexpr uint8_t kWrite   = 2;
    inline constexpr uint8_t kExecute = 4;
}

} // namespace ld

#endif
