/*
 * SPDX-FileCopyrightText: (C) 2020-2026 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */
#pragma once

/*!
 * \file asql_coro_delivery.h
 * \brief Internal coroutine delivery types — not part of the stable public API.
 *
 * Drivers and the ASql core use these to deliver open/query/pool results into
 * coroutine awaitables via weak_ptr. Application code should use ADatabase /
 * APool / ACoroExpected only.
 */

#include <asql_export.h>
#include <memory>
#include <optional>

#include <QString>

namespace ASql {

class AResult;
class ADatabase;

namespace detail {

/*!
 * \brief Base interface used by coroutine awaitables to receive open results.
 */
class ASQL_EXPORT ACoroOpenData
{
public:
    virtual ~ACoroOpenData()                                    = default;
    virtual void deliverOpen(bool isOpen, const QString &error) = 0;
};

/*!
 * \brief Callable held by drivers for open completion.
 */
class ASQL_EXPORT AOpenFn
{
public:
    AOpenFn() = default;

    AOpenFn(std::weak_ptr<ACoroOpenData> coroData)
        : m_coroData(std::move(coroData))
    {
    }

    void operator()(bool isOpen, const QString &error) const
    {
        if (m_coroData.has_value()) {
            if (auto data = m_coroData->lock()) {
                data->deliverOpen(isOpen, error);
            }
        }
    }

    explicit operator bool() const { return m_coroData.has_value() && !m_coroData->expired(); }

private:
    std::optional<std::weak_ptr<ACoroOpenData>> m_coroData;
};

/*!
 * \brief Abstract delivery interface for query results.
 */
class ASQL_EXPORT ACoroResult
{
public:
    virtual ~ACoroResult()           = default;
    virtual void deliver(AResult &v) = 0;
};

/*!
 * \brief Callable that delivers query results into a coroutine awaitable.
 */
class ASQL_EXPORT AResultFn
{
public:
    AResultFn() = default;

    explicit AResultFn(std::weak_ptr<ACoroResult> coroData)
        : m_coroData(std::move(coroData))
    {
    }

    void operator()(AResult &result) const
    {
        if (auto data = m_coroData.lock()) {
            data->deliver(result);
        }
    }

    explicit operator bool() const { return !m_coroData.expired(); }

private:
    std::weak_ptr<ACoroResult> m_coroData;
};

/*!
 * \brief Lightweight weak reference used by drivers to deliver query results.
 */
class ASQL_EXPORT ACoroDataRef
{
public:
    ACoroDataRef() = default;

    explicit ACoroDataRef(std::weak_ptr<ACoroResult> coroData)
        : m_coroData(std::move(coroData))
    {
    }

    void deliverResult(AResult &result) const
    {
        if (auto data = m_coroData.lock()) {
            data->deliver(result);
        }
    }

    explicit operator bool() const { return !m_coroData.expired(); }

    operator AResultFn() const { return AResultFn{m_coroData}; }

private:
    std::weak_ptr<ACoroResult> m_coroData;
};

/*!
 * \brief Abstract delivery interface for pooled database connections.
 */
class ASQL_EXPORT ACoroDatabase
{
public:
    virtual ~ACoroDatabase()          = default;
    virtual void deliver(ADatabase v) = 0;
};

/*!
 * \brief Callable that delivers a pooled connection into a coroutine awaitable.
 */
class ASQL_EXPORT ADatabaseFn
{
public:
    ADatabaseFn() = default;

    explicit ADatabaseFn(std::weak_ptr<ACoroDatabase> coroData)
        : m_coroData(std::move(coroData))
    {
    }

    void operator()(ADatabase db) const
    {
        if (auto data = m_coroData.lock()) {
            data->deliver(std::move(db));
        }
    }

    explicit operator bool() const { return !m_coroData.expired(); }

private:
    std::weak_ptr<ACoroDatabase> m_coroData;
};

} // namespace detail

// Compatibility aliases used throughout the core and drivers.
using ACoroOpenData = detail::ACoroOpenData;
using AOpenFn       = detail::AOpenFn;
using ACoroResult   = detail::ACoroResult;
using AResultFn     = detail::AResultFn;
using ACoroDataRef  = detail::ACoroDataRef;
using ACoroDatabase = detail::ACoroDatabase;
using ADatabaseFn   = detail::ADatabaseFn;

} // namespace ASql
