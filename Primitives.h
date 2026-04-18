// Copyright (c) 2026 Nikita Hmelnitkii. MIT License - see LICENSE.
//
// Primitives.h - shared utilities: non-owning string view, Span, fixed
// unaligned memory access, and path helpers. Every layout header builds
// on these primitives.

#ifndef LD_PRIMITIVES_H
#define LD_PRIMITIVES_H

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ld {

// Non-owning (ptr, len) string view. 16 bytes, not guaranteed null-
// terminated. Mirrors the (ptr, size_t) pair libc++ std::string_view
// decays into and matches the layout of C++ string-vector elements
// stored in ld-prime's linkedit tables.
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

// Bounded reverse scan for '/'. Null on miss.
inline const char *lastSlashInRange(const char *begin, const char *end) {
    for (const char *p = end; p > begin; ) {
        --p;
        if (*p == '/') return p;
    }
    return nullptr;
}

// Bounded forward substring search. Null on miss.
inline const char *findSubstringInRange(const char *begin, const char *end,
                                        const char *needle, size_t needleLen) {
    if (end - begin < static_cast<ptrdiff_t>(needleLen)) return nullptr;
    for (const char *p = begin; p + needleLen <= end; ++p) {
        if (__builtin_memcmp(p, needle, needleLen) == 0) return p;
    }
    return nullptr;
}

inline CString pathBasename(const char *path) {
    if (!path) return {};
    const char *end = path + __builtin_strlen(path);
    const char *slash = lastSlashInRange(path, end);
    if (slash == nullptr) return CString{path, static_cast<size_t>(end - path)};
    return CString{slash + 1, static_cast<size_t>(end - slash - 1)};
}

// Directory portion including the trailing slash. Empty when `path`
// contains no slash.
inline CString pathDirname(const char *path) {
    if (!path) return {};
    const char *end = path + __builtin_strlen(path);
    const char *slash = lastSlashInRange(path, end);
    if (slash == nullptr) return {};
    return CString{path, static_cast<size_t>(slash - path + 1)};
}

// Extension including the leading dot. Empty when there is no dot in
// the basename.
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

// Half-open (begin, end) range over a contiguous T*. Matches libc++'s
// layout for std::vector<T> iterators as stored in ld-prime's Layout
// objects, allowing direct readSpan<T>(file, beginOffset, endOffset).
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

// Unaligned memory access. __builtin_memcpy lowers to a single load
// or store on arm64 and x86_64.
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
