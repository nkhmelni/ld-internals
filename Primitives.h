// Copyright (c) 2026 Nikita Hmelnitkii. MIT License - see LICENSE.
//
// Primitives.h - basic types shared across all ld layout headers.
// CString, Span, unaligned memory access, install-name parsing, path helpers.

#ifndef LD_PRIMITIVES_H
#define LD_PRIMITIVES_H

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ld {

// 16-byte non-owning (ptr, len) string. Not generally null-terminated.
struct CString {
    const char *data;
    size_t      len;

    constexpr CString() : data(nullptr), len(0) {}
    constexpr CString(const char *d, size_t n) : data(d), len(n) {}

    static CString fromCStr(const char *s) {
        return s ? CString{s, __builtin_strlen(s)} : CString{};
    }

    constexpr bool empty() const { return len == 0; }
    constexpr bool null()  const { return data == nullptr; }

    bool equals(const char *rhs) const {
        if (!data) return rhs == nullptr;
        if (!rhs)  return false;
        size_t rlen = __builtin_strlen(rhs);
        return len == rlen && memcmp(data, rhs, len) == 0;
    }
    bool equals(CString rhs) const {
        return len == rhs.len
            && (len == 0 || memcmp(data, rhs.data, len) == 0);
    }

    bool startsWith(const char *prefix) const {
        if (!data || !prefix) return false;
        size_t plen = __builtin_strlen(prefix);
        return len >= plen && memcmp(data, prefix, plen) == 0;
    }
};

static_assert(sizeof(CString) == 16, "");

enum class InstallNameAnchor : uint8_t {
    None            = 0,
    Rpath           = 1,
    LoaderPath      = 2,
    ExecutablePath  = 3,
};

enum class InstallNameKind : uint8_t {
    Invalid   = 0,
    Loose     = 1,
    Framework = 2,
};

// Non-owning view into a parsed install name. All CString members
// alias the original path storage.
struct DylibInstallName {
    const char       *path;
    InstallNameKind   kind;
    InstallNameAnchor anchor;
    CString           body;
    CString frameworkName;
    CString frameworkVersion;
    CString frameworkInstallDir;
    CString leafName;

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

inline const char *lastSlashInRange(const char *begin, const char *end) {
    for (const char *p = end; p > begin; ) {
        --p;
        if (*p == '/') return p;
    }
    return nullptr;
}

inline const char *findSubstringInRange(const char *begin, const char *end,
                                         const char *needle, size_t needleLen) {
    if (end - begin < static_cast<ptrdiff_t>(needleLen)) return nullptr;
    for (const char *p = begin; p + needleLen <= end; ++p) {
        if (__builtin_memcmp(p, needle, needleLen) == 0) return p;
    }
    return nullptr;
}

// Framework leaf name must match both sides of .framework/ -- a
// mismatch falls through to Loose.
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

    static constexpr char kFrameworkMarker[] = ".framework/";
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
            out.kind          = InstallNameKind::Framework;
            out.frameworkName = CString{nameBegin, nameLen};
            out.frameworkInstallDir = CString{body,
                static_cast<size_t>(nameBegin - body)};

            const char *afterMarker = marker + kFrameworkMarkerLen;
            static constexpr char kVersionsMarker[] = "Versions/";
            static constexpr size_t kVersionsMarkerLen = sizeof(kVersionsMarker) - 1;
            if (static_cast<size_t>(leafBegin - afterMarker) > kVersionsMarkerLen
                && __builtin_memcmp(afterMarker, kVersionsMarker,
                                    kVersionsMarkerLen) == 0)
            {
                const char *verBegin = afterMarker + kVersionsMarkerLen;
                const char *verEnd   = leafBegin - 1;
                if (verEnd > verBegin) {
                    out.frameworkVersion =
                        CString{verBegin, static_cast<size_t>(verEnd - verBegin)};
                }
            }
            return out;
        }
    }

    out.kind = InstallNameKind::Loose;
    return out;
}

inline CString pathBasename(const char *path) {
    if (!path) return {};
    const char *end = path + __builtin_strlen(path);
    const char *slash = lastSlashInRange(path, end);
    if (slash == nullptr) return CString{path, static_cast<size_t>(end - path)};
    return CString{slash + 1, static_cast<size_t>(end - slash - 1)};
}

inline CString pathDirname(const char *path) {
    if (!path) return {};
    const char *end = path + __builtin_strlen(path);
    const char *slash = lastSlashInRange(path, end);
    if (slash == nullptr) return {};
    return CString{path, static_cast<size_t>(slash - path + 1)};
}

inline CString pathExtension(const char *path) {
    CString base = pathBasename(path);
    if (base.len == 0) return {};
    for (size_t i = base.len; i > 0; --i) {
        if (base.data[i - 1] == '.') {
            return CString{base.data + i - 1, base.len - i + 1};
        }
    }
    return {};
}

template <typename T>
struct Span {
    const T *begin;
    const T *end;

    constexpr Span() : begin(nullptr), end(nullptr) {}
    constexpr Span(const T *b, const T *e) : begin(b), end(e) {}

    constexpr size_t size() const {
        return begin && end && end >= begin
             ? static_cast<size_t>(end - begin)
             : 0;
    }
    constexpr bool empty() const { return size() == 0; }
    constexpr bool valid() const { return begin != nullptr && end >= begin; }

    constexpr const T &operator[](size_t i) const { return begin[i]; }
    constexpr const T *cbegin() const { return begin; }
    constexpr const T *cend()   const { return end; }
};

template <typename T>
inline Span<T> readSpan(const void *base, size_t beginOff, size_t endOff) {
    if (!base || beginOff == 0 || endOff == 0) return Span<T>{};
    const uint8_t *b = static_cast<const uint8_t *>(base);
    const T *begin = *reinterpret_cast<const T *const *>(b + beginOff);
    const T *end   = *reinterpret_cast<const T *const *>(b + endOff);
    return Span<T>{begin, end};
}

// Unaligned memory access via __builtin_memcpy. Compiles to a single
// load or store on arm64 and x86_64.
inline uint8_t  readU8 (const void *b, size_t o) { return static_cast<const uint8_t*>(b)[o]; }
inline uint16_t readU16(const void *b, size_t o) { uint16_t v; __builtin_memcpy(&v, static_cast<const uint8_t*>(b)+o, 2); return v; }
inline uint32_t readU32(const void *b, size_t o) { uint32_t v; __builtin_memcpy(&v, static_cast<const uint8_t*>(b)+o, 4); return v; }
inline uint64_t readU64(const void *b, size_t o) { uint64_t v; __builtin_memcpy(&v, static_cast<const uint8_t*>(b)+o, 8); return v; }
inline const void *readPtr(const void *b, size_t o) { const void *v; __builtin_memcpy(&v, static_cast<const uint8_t*>(b)+o, sizeof(v)); return v; }

inline void writeU8 (void *b, size_t o, uint8_t  v) { static_cast<uint8_t*>(b)[o] = v; }
inline void writeU16(void *b, size_t o, uint16_t v) { __builtin_memcpy(static_cast<uint8_t*>(b)+o, &v, 2); }
inline void writeU32(void *b, size_t o, uint32_t v) { __builtin_memcpy(static_cast<uint8_t*>(b)+o, &v, 4); }
inline void writeU64(void *b, size_t o, uint64_t v) { __builtin_memcpy(static_cast<uint8_t*>(b)+o, &v, 8); }
inline void writePtr(void *b, size_t o, const void *v) { __builtin_memcpy(static_cast<uint8_t*>(b)+o, &v, sizeof(v)); }

} // namespace ld

#endif
