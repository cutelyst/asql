/*
 * SPDX-FileCopyrightText: (C) 2020-2026 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */
#pragma once

namespace ASql {

template <std::size_t N>
AExpectedResult APool::exec(const char8_t (&query)[N], QObject *receiver, QStringView poolName)
{
    static_assert(N > 1, "UTF-8 SQL literal must not be empty");
    return execUtf8(
        detail::utf8LiteralView(reinterpret_cast<const char *>(query), qsizetype(N - 1)),
        receiver,
        poolName);
}

template <std::size_t N>
AExpectedResult APool::exec(const char (&query)[N], QObject *receiver, QStringView poolName)
{
    static_assert(N > 1, "UTF-8 SQL literal must not be empty");
    return execUtf8(detail::utf8LiteralView(query, qsizetype(N - 1)), receiver, poolName);
}

template <std::size_t N>
AExpectedMultiResult
    APool::execMulti(const char8_t (&query)[N], QObject *receiver, QStringView poolName)
{
    static_assert(N > 1, "UTF-8 SQL literal must not be empty");
    return execMultiUtf8(
        detail::utf8LiteralView(reinterpret_cast<const char *>(query), qsizetype(N - 1)),
        receiver,
        poolName);
}

template <std::size_t N>
AExpectedMultiResult
    APool::execMulti(const char (&query)[N], QObject *receiver, QStringView poolName)
{
    static_assert(N > 1, "UTF-8 SQL literal must not be empty");
    return execMultiUtf8(detail::utf8LiteralView(query, qsizetype(N - 1)), receiver, poolName);
}

template <std::size_t N>
AExpectedResult APool::exec(const char8_t (&query)[N],
                            const QVariantList &params,
                            QObject *receiver,
                            QStringView poolName)
{
    static_assert(N > 1, "UTF-8 SQL literal must not be empty");
    return execUtf8(
        detail::utf8LiteralView(reinterpret_cast<const char *>(query), qsizetype(N - 1)),
        params,
        receiver,
        poolName);
}

template <std::size_t N>
AExpectedResult APool::exec(const char (&query)[N],
                            const QVariantList &params,
                            QObject *receiver,
                            QStringView poolName)
{
    static_assert(N > 1, "UTF-8 SQL literal must not be empty");
    return execUtf8(detail::utf8LiteralView(query, qsizetype(N - 1)), params, receiver, poolName);
}

} // namespace ASql
