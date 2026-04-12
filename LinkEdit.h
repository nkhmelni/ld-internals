// Copyright (c) 2026 Nikita Hmelnitkii. MIT License - see LICENSE.
//
// LinkEdit.h - element types for linkedit-producing vectors.

#ifndef LD_LINKEDIT_H
#define LD_LINKEDIT_H

#include "Primitives.h"

#include <cstddef>
#include <cstdint>

namespace ld {

using LinkerOption = CString;
static_assert(sizeof(LinkerOption) == 16, "");

// Large-addend pool slot (indexed when the inline 21-bit addend overflows).
using LargeAddend = int64_t;
static_assert(sizeof(LargeAddend) == 8, "");

// Linker optimization hint -- 8-byte packed record.
namespace LOH {

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

// Dependency info entry. Values match the dep_info file format.
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
