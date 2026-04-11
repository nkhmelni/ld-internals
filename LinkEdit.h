// Copyright (c) 2026 Nikita Hmelnitkii. MIT License - see LICENSE.
//
// LinkEdit.h - element types for ld-prime's linkedit-producing vectors.
// Strides verified against ld-1221.4 x86_64 accessors (sarq $N, %reg):
//   linkerOptions -> 16B (CString), largeAddends -> 8B (int64_t),
//   fileLOHs -> 8B (packed u64), altFileInfos -> 8B (ptr).

#ifndef LD_LINKEDIT_H
#define LD_LINKEDIT_H

#include "Primitives.h"

#include <cstddef>
#include <cstdint>

namespace ld {

// File-level -linker_option entry: one CString per argument.
// mach_o::HeaderWriter::LinkerOption is a distinct higher-level type used
// during LC_LINKER_OPTION emission (buffer + argc) and is not exposed here.
using LinkerOption = CString;
static_assert(sizeof(LinkerOption) == 16, "");

// Large-addend pool slot. Indexed when a Fixup's inline 21-bit addend
// overflows (bit 10 of kindAddend set). Stored in ascending order.
using LargeAddend = int64_t;
static_assert(sizeof(LargeAddend) == 8, "");

// Linker optimization hint - 8-byte packed record. Low byte is the kind;
// the remaining bits encode up to 3 instruction addresses. ld-prime does
// not expose the bit partition; decodeKind() is the only portable reader.
namespace LOH {

    // Values match ld64 src/ld/parsers/macho_relocatable_file.cpp LOH_ARM64_*.
    enum Kind : uint8_t {
        kNone          = 0x00,
        kAdrpAdrp      = 0x01,  // two ADRPs to the same page
        kAdrpLdr       = 0x02,  // ADRP + LDR
        kAdrpAddLdr    = 0x03,  // ADRP + ADD + LDR
        kAdrpLdrGotLdr = 0x04,  // ADRP + LDR(GOT) + LDR
        kAdrpAddStr    = 0x05,  // ADRP + ADD + STR
        kAdrpLdrGotStr = 0x06,  // ADRP + LDR(GOT) + STR
        kAdrpAdd       = 0x07,  // ADRP + ADD
        kAdrpLdrGot    = 0x08,  // ADRP + LDR(GOT)
    };

    inline constexpr uint8_t kMaxInstructions = 3;

    constexpr Kind decodeKind(uint64_t record) {
        return static_cast<Kind>(record & 0xFF);
    }
}

// ld::DependencyInfo entry written by -dependency_info. Tag byte at +0x00,
// owned std::string path at +0x08. Tag values match the dependency_info
// file format (also classic ld64 Options.h DEPEND_*). Verified on ld-1221.4:
//   addInputFileDependency  -> movb $0x10, ...
//   addOutputFileDependency -> movb $0x40, ...
namespace DependencyEntry {

    enum Kind : uint8_t {
        kVersion       = 0x00,
        kInput         = 0x10,
        kInputNotFound = 0x11,
        kInputLTO      = 0x12,
        kOutput        = 0x40,
    };

    inline constexpr size_t kTag  = 0x00;  // u8
    inline constexpr size_t kPath = 0x08;  // std::string (libc++ SSO, 24B)
}

} // namespace ld

#endif
