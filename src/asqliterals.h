/*
 * SPDX-FileCopyrightText: (C) 2020-2026 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <cstddef>

#include <QtCore/qutf8stringview.h>

namespace ASql::detail {

inline QUtf8StringView utf8LiteralView(const char *data, qsizetype size) noexcept
{
    return QUtf8StringView(data, size);
}

} // namespace ASql::detail
