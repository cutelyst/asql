// SPDX-FileCopyrightText: (C) 2026 Daniel Nicoletti <dantti12@gmail.com>
// SPDX-License-Identifier: MIT
//
// Visual Studio 2026 (VS 18, MSVC 14.5x, _MSC_VER >= 1950) removed
// stdext::make_checked_array_iterator. Qt 6.5.0–6.5.4 still call it from
// qvarlengtharray.h via QT_MAKE_CHECKED_ARRAY_ITERATOR. Provide a minimal
// stand-in that returns the raw pointer (same behaviour as Qt's non-MSVC path).
#pragma once

#if defined(__cplusplus) && defined(_MSC_VER) && _MSC_VER >= 1950
#    include <cstddef>

namespace stdext {
template <class T>
inline T *make_checked_array_iterator(T *ptr, std::size_t /*size*/, std::size_t offset = 0)
{
    return ptr + offset;
}

template <class T>
inline T *make_unchecked_array_iterator(T *ptr)
{
    return ptr;
}
} // namespace stdext
#endif
