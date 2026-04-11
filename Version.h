// Copyright (c) 2026 Nikita Hmelnitkii. MIT License - see LICENSE.
//
// Version.h - runtime detection of the linker family and version by
// scanning the loaded ld image for its embedded "PROJECT:ld..." string.

#ifndef LD_VERSION_H
#define LD_VERSION_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <mach-o/dyld.h>
#include <mach-o/loader.h>

namespace ld {

enum class Family : uint8_t {
    Unknown,
    Prime,   // ld-prime (ld-1100+), closed-source
    Classic, // ld64, open-source-based
};

struct LinkerVersion {
    Family   family;
    uint32_t major;
    uint16_t minor;
    uint16_t patch;

    bool isPrime()   const { return family == Family::Prime; }
    bool isClassic() const { return family == Family::Classic; }

    bool operator==(const LinkerVersion &o) const {
        return family == o.family && major == o.major
            && minor == o.minor && patch == o.patch;
    }
    bool operator!=(const LinkerVersion &o) const { return !(*this == o); }

    bool operator<(const LinkerVersion &o) const {
        if (family != o.family) return family < o.family;
        if (major != o.major) return major < o.major;
        if (minor != o.minor) return minor < o.minor;
        return patch < o.patch;
    }
};

namespace version {
    inline constexpr LinkerVersion Prime_1115 = {Family::Prime, 1115, 7, 3};
    inline constexpr LinkerVersion Prime_1221 = {Family::Prime, 1221, 4, 0};
    inline constexpr LinkerVersion Prime_1230 = {Family::Prime, 1230, 1, 0};
    inline constexpr LinkerVersion Prime_1266 = {Family::Prime, 1266, 8, 0};
    inline constexpr LinkerVersion ld64_820   = {Family::Classic, 820, 1, 0};
}

// Parse "ld-1115.7.3" or "ld64-820.1" into a LinkerVersion.
inline LinkerVersion parseProjectString(const char *s) {
    LinkerVersion v = {Family::Unknown, 0, 0, 0};
    if (!s) return v;

    const char *p = s;
    if (p[0] == 'l' && p[1] == 'd') {
        if (p[2] == '6' && p[3] == '4' && p[4] == '-') {
            v.family = Family::Classic;
            p += 5;
        } else if (p[2] == '-') {
            v.family = Family::Prime;
            p += 3;
        } else {
            return v;
        }
    } else {
        return v;
    }

    char *end = nullptr;
    v.major = static_cast<uint32_t>(strtoul(p, &end, 10));
    if (end == p || v.major == 0) return {Family::Unknown, 0, 0, 0};
    p = end;
    if (*p == '.') {
        v.minor = static_cast<uint16_t>(strtoul(++p, &end, 10));
        p = end;
    }
    if (*p == '.') {
        v.patch = static_cast<uint16_t>(strtoul(++p, &end, 10));
    }
    return v;
}

// Search a bounded region for "PROJECT:ld..." and parse the version.
inline LinkerVersion findProjectString(const uint8_t *start, const uint8_t *end) {
    static const char needle[] = "PROJECT:ld";
    constexpr size_t nlen = sizeof(needle) - 1;

    for (const uint8_t *p = start; p + nlen <= end; ++p) {
        if (memcmp(p, needle, nlen) != 0) continue;

        const char *proj = reinterpret_cast<const char *>(p) + nlen;
        const char *limit = reinterpret_cast<const char *>(end);

        while (proj < limit && *proj == ' ') ++proj;
        if (proj >= limit) continue;

        size_t remaining = static_cast<size_t>(limit - proj);
        char buf[64];
        int n = snprintf(buf, sizeof(buf), "ld%.*s",
                         static_cast<int>(remaining < sizeof(buf) - 3
                                          ? remaining : sizeof(buf) - 3),
                         proj);
        if (n <= 0) continue;

        for (size_t k = 0; k < sizeof(buf) && buf[k] != '\0'; ++k) {
            char c = buf[k];
            if (c != '-' && c != '.' && c != '_'
                && !((c >= '0' && c <= '9')
                  || (c >= 'a' && c <= 'z')
                  || (c >= 'A' && c <= 'Z'))) {
                buf[k] = '\0';
                break;
            }
        }
        LinkerVersion v = parseProjectString(buf);
        if (v.family != Family::Unknown) return v;
    }
    return {Family::Unknown, 0, 0, 0};
}

// Detect the linker version from the running process. Single-threaded
// only (safe inside LinkeditBuilder::build or any hook installed there).
inline LinkerVersion detectVersion() {
    const uint32_t imageCount = _dyld_image_count();

    auto checkImage = [](uint32_t i) -> LinkerVersion {
        const char *name = _dyld_get_image_name(i);
        if (!name) return {Family::Unknown, 0, 0, 0};
        size_t len = strlen(name);

        // Basename filter: "ld", "ld-*", "ld64*", "ld.sh". Anything else
        // is rejected up-front to avoid scanning unrelated images.
        const char *slash = nullptr;
        for (size_t k = 0; k < len; ++k) if (name[k] == '/') slash = name + k;
        const char *base = slash ? slash + 1 : name;
        bool isLinker =
            (strcmp(base, "ld") == 0) ||
            (strncmp(base, "ld-", 3) == 0) ||
            (strncmp(base, "ld64", 4) == 0) ||
            (strcmp(base, "ld.sh") == 0);
        if (!isLinker) return {Family::Unknown, 0, 0, 0};

        const auto *mh = reinterpret_cast<const struct mach_header_64 *>(
            _dyld_get_image_header(i));
        if (!mh || mh->magic != MH_MAGIC_64)
            return {Family::Unknown, 0, 0, 0};

        const uint8_t *cmds =
            reinterpret_cast<const uint8_t *>(mh) + sizeof(struct mach_header_64);
        intptr_t slide = _dyld_get_image_vmaddr_slide(i);

        const struct load_command *lc =
            reinterpret_cast<const struct load_command *>(cmds);
        for (uint32_t j = 0; j < mh->ncmds; ++j) {
            if (lc->cmdsize < sizeof(struct load_command)) break;
            if (lc->cmd == LC_SEGMENT_64) {
                const auto *seg =
                    reinterpret_cast<const struct segment_command_64 *>(lc);

                // Only __TEXT and __DATA carry the PROJECT string.
                if (strncmp(seg->segname, "__TEXT", 6) != 0
                    && strncmp(seg->segname, "__DATA", 6) != 0) {
                    lc = reinterpret_cast<const struct load_command *>(
                        reinterpret_cast<const uint8_t *>(lc) + lc->cmdsize);
                    continue;
                }

                uint64_t searchSize = seg->filesize < seg->vmsize
                                      ? seg->filesize : seg->vmsize;
                const uint8_t *start =
                    reinterpret_cast<const uint8_t *>(seg->vmaddr + slide);

                LinkerVersion v = findProjectString(start, start + searchSize);
                if (v.family != Family::Unknown) return v;
            }
            lc = reinterpret_cast<const struct load_command *>(
                reinterpret_cast<const uint8_t *>(lc) + lc->cmdsize);
        }
        return {Family::Unknown, 0, 0, 0};
    };

    // Fast path: image 0 is the main executable (the ld binary itself).
    if (imageCount > 0) {
        LinkerVersion v = checkImage(0);
        if (v.family != Family::Unknown) return v;
    }

    for (uint32_t i = 1; i < imageCount; ++i) {
        LinkerVersion v = checkImage(i);
        if (v.family != Family::Unknown) return v;
    }
    return {Family::Unknown, 0, 0, 0};
}

} // namespace ld

#endif
