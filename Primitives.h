// Copyright (c) 2026 Nikita Hmelnitkii. MIT License - see LICENSE.
//
// Primitives.h - leaf types shared by every other ld header: CString
// (ptr,len), Span<T> (begin,end), install-name anchor helpers.

#ifndef LD_PRIMITIVES_H
#define LD_PRIMITIVES_H

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ld {

// ld-prime's non-owning (ptr, len) string handle - 16 bytes, ABI-stable.
// Not guaranteed null-terminated in the general case; installName is the
// exception (makeDylibFileInfo does malloc(len+1) + trailing nul).
struct CString {
    const char *data;
    size_t      len;

    constexpr CString() : data(nullptr), len(0) {}
    constexpr CString(const char *d, size_t n) : data(d), len(n) {}

    // Construct from a null-terminated C string (uses strlen).
    static CString fromCStr(const char *s) {
        return s ? CString{s, __builtin_strlen(s)} : CString{};
    }

    constexpr bool empty() const { return len == 0; }
    constexpr bool null()  const { return data == nullptr; }

    // Safe equality against another CString or a null-terminated C string.
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

static_assert(sizeof(CString) == 16, "CString ABI must be 16 bytes");

// Install-name anchor helpers (@rpath, @loader_path, @executable_path).

inline bool isRpathRelative(const char *installName) {
    return installName && __builtin_memcmp(installName, "@rpath/", 7) == 0;
}

inline bool isLoaderRelative(const char *installName) {
    return installName && __builtin_memcmp(installName, "@loader_path/", 13) == 0;
}

inline bool isExecutableRelative(const char *installName) {
    return installName && __builtin_memcmp(installName, "@executable_path/", 17) == 0;
}

inline bool installNameHasAnchor(const char *installName) {
    return isRpathRelative(installName)
        || isLoaderRelative(installName)
        || isExecutableRelative(installName);
}

// Strip a `@<anchor>/` prefix; returns the input unchanged when absent.
inline const char *stripInstallNameAnchor(const char *installName) {
    if (!installName) return nullptr;
    if (isRpathRelative(installName))      return installName + 7;
    if (isLoaderRelative(installName))     return installName + 13;
    if (isExecutableRelative(installName)) return installName + 17;
    return installName;
}

// Non-owning (begin, end) contiguous view. C++17 replacement for std::span.
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

// Build a Span<T> from base + two field offsets. Returns an empty span if
// the base or either offset is zero.
template <typename T>
inline Span<T> readSpan(const void *base, size_t beginOff, size_t endOff) {
    if (!base || beginOff == 0 || endOff == 0) return Span<T>{};
    const uint8_t *b = static_cast<const uint8_t *>(base);
    const T *begin = *reinterpret_cast<const T *const *>(b + beginOff);
    const T *end   = *reinterpret_cast<const T *const *>(b + endOff);
    return Span<T>{begin, end};
}

} // namespace ld

#endif
