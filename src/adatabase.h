/*
 * SPDX-FileCopyrightText: (C) 2020-2023 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <asql_export.h>
#include <asqliterals.h>
#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>

#include <QObject>
#include <QVariantList>

namespace ASql {

class AResult;
class ADatabase;
class ATransaction;
class ADriver;
class ADriverFactory;

class ADatabaseNotification
{
public:
    QString name;
    QVariant payload;
    bool self;
};

/*!
 * \brief ACoroOpenData is the base interface used by coroutine awaitables to receive open results.
 *
 * The ASql engine holds a \c std::weak_ptr<ACoroOpenData> instead of a copied callback lambda.
 * When the open operation completes, the engine locks the weak_ptr and calls deliverOpen().
 * If the coroutine has already been destroyed, lock() returns nullptr and the call is skipped.
 */
class ASQL_EXPORT ACoroOpenData
{
public:
    virtual ~ACoroOpenData()                                    = default;
    virtual void deliverOpen(bool isOpen, const QString &error) = 0;
};

/*!
 * \brief AOpenFn is callable type accepted by driver open methods.
 *
 * Constructed from a \c std::weak_ptr<ACoroOpenData>; the driver holds only the weak reference
 * and calls deliverOpen() on the pointed-to object when the connection attempt completes.
 * If the awaitable has already been destroyed, lock() returns nullptr and the call is skipped.
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
 * \brief ACoroResult is the abstract delivery interface used by coroutine awaitables that receive
 * query results (AResult).
 *
 * The ASql engine holds a \c std::weak_ptr<ACoroResult> instead of a copied callback lambda.
 * When the query completes, the engine locks the weak_ptr and calls deliver().
 * If the coroutine has already been destroyed, lock() returns nullptr and the call is skipped.
 *
 * The concrete implementation lives in \c ACoroData<T> (see acoroexpected.h).
 */
class ASQL_EXPORT ACoroResult
{
public:
    virtual ~ACoroResult()           = default;
    virtual void deliver(AResult &v) = 0;
};

/*!
 * \brief AResultFn delivers query results into a coroutine awaitable.
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
 * \brief ACoroDataRef is a lightweight, movable reference to an ACoroExpected's
 * coroutine data. It holds a weak pointer to the underlying ACoroResult so the driver
 * can deliver query results without keeping the awaitable alive.
 *
 * Obtain an instance via AExpectedResult::ref() or AExpectedMultiResult::ref().
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

    // Implicit conversion so ACoroDataRef can be passed anywhere AResultFn is expected.
    operator AResultFn() const { return AResultFn{m_coroData}; }

private:
    std::weak_ptr<ACoroResult> m_coroData;
};

template <typename T>
class ACoroExpected;

template <typename T>
class ACoroMultiExpected;

class AExpectedOpen;

using AExpectedResult      = ACoroExpected<AResult>;
using AExpectedMultiResult = ACoroMultiExpected<AResult>;

using AExpectedTransaction = ACoroExpected<ATransaction>;
using AExpectedDatabase    = ACoroExpected<ADatabase>;

class APreparedQuery;
class ASQL_EXPORT ADatabase
{
    Q_GADGET
public:
    enum class State { Disconnected, Connecting, Connected };
    Q_ENUM(State)

    /*!
     * \brief ADatabase constructs an invalid database object
     */
    ADatabase();

    /*!
     * \brief ADatabase constructs a database object with the supplied driver
     */
    ADatabase(std::shared_ptr<ADriver> driver);

    /*!
     * \brief ADatabase constructs a database object with the supplied driver factory
     */
    ADatabase(const std::shared_ptr<ADriverFactory> &factory);

    /*!
     * \brief ADatabase constructs a database object from another database object
     */
    ADatabase(const ADatabase &other);

    /*!
     * \brief Move-constructs an ADatabase instance
     */
    ADatabase(ADatabase &&other) noexcept;

    virtual ~ADatabase();

    ADatabase &operator=(ADatabase &&other) noexcept
    {
        std::swap(d, other.d);
        return *this;
    }

    /*!
     * \brief isValid checks if this is a valid connection, invalid connections
     * are the ones created with invalid drivers, or using the empty constructor.
     * \return true if valid
     */
    [[nodiscard]] bool isValid() const;

    /*!
     * \brief driverName returns the driver identifier (for example \c QPSQL or \c QSQLITE).
     */
    [[nodiscard]] QString driverName() const;

    /*!
     * \brief Returns the underlying driver object for signal connections.
     */
    [[nodiscard]] ADriver *driver() const;

    /*!
     * \brief coOpen opens the database and returns a coroutine awaitable.
     *
     * co_await the returned object to suspend until the connection is established
     * (or fails). The result is \c std::expected<bool, QString>: \c true on success,
     * or an error string on failure.
     *
     * The operation is only initiated if the current state is Disconnected;
     * if already Connected the awaitable is immediately ready with \c true.
     *
     * \param receiver optional lifetime guard; if the QObject is destroyed
     *        before the connection completes the coroutine is destroyed.
     */
    [[nodiscard]] AExpectedOpen coOpen(QObject *receiver = nullptr);

    /*!
     * \brief state
     * \return database connection state
     */
    [[nodiscard]] State state() const;

    /*!
     * \brief isOpen returns if the database connection is open.
     * \return true if open
     */
    [[nodiscard]] bool isOpen() const;

    /*!
     * \brief begin a transaction with a RAII object, this operation usually succeeds,
     * but one can hook up a callback to check it's result.
     *
     * \param cb
     */
    [[nodiscard]] AExpectedTransaction begin(QObject *receiver = nullptr);

    /*!
     * \brief commit a transaction, this operation usually succeeds,
     * but one can hook up a callback to check it's result.
     *
     * \param cb
     */
    [[nodiscard]] AExpectedResult commit(QObject *receiver = nullptr);

    /*!
     * \brief rollback a transaction, this operation usually succeeds,
     * but one can hook up a callback to check it's result.
     *
     * \param cb
     */
    [[nodiscard]] AExpectedResult rollback(QObject *receiver = nullptr);

    /*!
     * \brief exec executes a \param query against this database connection.
     * co_await the returned awaitable; on failure the result is \c std::unexpected with an
     * error string.
     *
     * Postgres and SQLite allows for multiple commands to be sent, in this case you are
     * expected to co_await only once. Use \sa execMulti for such cases.
     *
     * \param query
     * \param receiver that tracks the lifetime of this query
     */
    [[nodiscard]] AExpectedResult exec(QStringView query, QObject *receiver = nullptr);

    /*!
     * \brief exec executes a UTF-8 \p query string literal against this database connection.
     *
     * The query is not copied. Pass a UTF-8 string literal such as \c u8"SELECT 1".
     * \c QUtf8StringView and other non-literal views are not accepted; use the QStringView
     * overload for dynamic SQL.
     *
     * \param query UTF-8 string literal, e.g. \c u8"SELECT 1"
     * \param receiver that tracks the lifetime of this query
     */
    template <std::size_t N>
    [[nodiscard]] AExpectedResult exec(const char8_t (&query)[N], QObject *receiver = nullptr);

    /*!
     * \brief exec executes a UTF-8 \p query string literal against this database connection.
     *
     * \note For dynamic SQL use the QStringView overload.
     */
    template <std::size_t N>
    [[nodiscard]] AExpectedResult exec(const char (&query)[N], QObject *receiver = nullptr);

    /*!
     *
     * \brief exec executes a \param query against this database connection.
     * co_await the returned awaitable; on failure the result is \c std::unexpected with an
     * error string.
     *
     * Postgres and SQLite allows for multiple commands to be sent, in this case you are
     * expected to co_await only once. Use \sa execMulti for such cases.
     *
     * \param query
     * \param receiver that tracks the lifetime of this query
     */
    [[nodiscard]] AExpectedResult
        exec(QStringView query, const QVariantList &params, QObject *receiver = nullptr);

    /*!
     * \brief exec executes a UTF-8 \p query string literal with bound \p params.
     *
     * \note For dynamic SQL use the QStringView overload.
     */
    template <std::size_t N>
    [[nodiscard]] AExpectedResult
        exec(const char8_t (&query)[N], const QVariantList &params, QObject *receiver = nullptr);

    /*!
     * \brief exec executes a UTF-8 \p query string literal with bound \p params.
     *
     * \note For dynamic SQL use the QStringView overload.
     */
    template <std::size_t N>
    [[nodiscard]] AExpectedResult
        exec(const char (&query)[N], const QVariantList &params, QObject *receiver = nullptr);

    /*!
     * \brief exec executes a prepared \param query against this database connection
     * with the following \p params to be bound,
     * once done AResult object will have the retrieved data if any, always
     * check for AResult::hasError() to see if the query was successful.
     *
     * \note For proper usage of APreparedQuery see it's documentation.
     * \note Postgres does not allow for multiple commands on prepared queries.
     *
     * \param query
     * \param cb
     */
    [[nodiscard]] AExpectedResult exec(const APreparedQuery &query, QObject *receiver = nullptr);

    /*!
     * \brief exec executes a prepared \param query against this database connection
     * with the following \p params to be bound,
     * once done AResult object will have the retrieved data if any, always
     * check for AResult::hasError() to see if the query was successful.
     *
     * \note For proper usage of APreparedQuery see it's documentation.
     * \note Postgres does not allow for multiple commands on prepared queries.
     *
     * \param query
     * \param cb
     */
    [[nodiscard]] AExpectedResult
        exec(const APreparedQuery &query, const QVariantList &params, QObject *receiver = nullptr);

    /*!
     * \brief exec executes a \param query against this database connection.
     * co_await the returned awaitable; on failure the result is \c std::unexpected with an
     * error string.
     *
     * Postgres allows for multiple commands to be sent, in this case you should co_await
     * for more results, check for AResult::lastResultSet() before that,
     * if one of the commands fails the subsequently ones will not be delivered, which
     * is why checking for AResult::lastResultSet() is important. This feature is
     * not supported by Postgres on the method that accepts params.
     *
     * \param query
     * \param receiver that tracks the lifetime of this query
     */
    [[nodiscard]] AExpectedMultiResult execMulti(QStringView query, QObject *receiver = nullptr);

    /*!
     * \brief execMulti executes a UTF-8 \p query string literal.
     *
     * \note For dynamic SQL use the QStringView overload.
     */
    template <std::size_t N>
    [[nodiscard]] AExpectedMultiResult execMulti(const char8_t (&query)[N],
                                                 QObject *receiver = nullptr);

    /*!
     * \brief execMulti executes a UTF-8 \p query string literal.
     *
     * \note For dynamic SQL use the QStringView overload.
     */
    template <std::size_t N>
    [[nodiscard]] AExpectedMultiResult execMulti(const char (&query)[N],
                                                 QObject *receiver = nullptr);

    /**
     * @brief setSingleRowMode
     *
     * Enables single row mode only for the last sent or queued query.
     */
    void setLastQuerySingleRowMode();

    /**
     * @brief enterPipelineMode will enable the pipeline mode on the driver, it's queue must be
     * empty and the connection must be open
     *
     * \param autoSyncMS if greater than zero it will setup a timer to send pipeline sync at each
     * ms.
     *
     * @return
     */
    bool enterPipelineMode(std::chrono::milliseconds timeout = std::chrono::milliseconds{0});

    /**
     * @brief exitPipelineMode disables the pipeline mode on the driver, its queue must be
     * empty and the connection must be open
     * @return
     */
    bool exitPipelineMode();

    enum class PipelineStatus {
        Off,
        On,
        Aborted,
    };
    /**
     * @brief pipelineStatus
     * @return the current pipeline mode status
     */
    [[nodiscard]] PipelineStatus pipelineStatus() const;

    /**
     * @brief pipelineSync sends a pipeline sync to delimiter the end of the current query set
     * @return
     */
    bool pipelineSync();

    /*!
     * \brief subscribeToNotification will start listening for notifications
     * described by name
     *
     * \note If the connection is broken the subscription is lost,
     * it's recommended to always subscribe when state changed to Connected.
     *
     * \note If a subscription is made in a transaction block it can be rolled
     * back, in which case it will not be effective.
     *
     * \param channel name of the channel
     * \param receiver optional lifetime guard for the LISTEN operation
     */
    void subscribeToNotification(const QString &channel, QObject *receiver = nullptr);

    /**
     * @brief subscribedToNotifications
     * @return a list of all notifications we subscribed to
     */
    [[nodiscard]] QStringList subscribedToNotifications() const;

    /*!
     * \brief unsubscribeFromNotification tell the database we are no longer
     * interested in being notified on this channel.
     *
     * \note If an unsubscription is made in a transaction block it can be rolled
     * back, in which case it will not be effective.
     *
     * \param channel name of the channel
     */
    void unsubscribeFromNotification(const QString &channel);

    /*!
     * \brief queueSize returns the driver's queue size
     * All calls to exec() methods are queued internally
     * and sent to the database.
     *
     * In pipeline mode they are also queued but sent to the database immediately.
     * \return
     */
    [[nodiscard]] int queueSize() const;

    ADatabase &operator=(const ADatabase &copy);

protected:
    friend class APool;
    std::shared_ptr<ADriver> d;

private:
    [[nodiscard]] AExpectedResult execUtf8(QUtf8StringView query, QObject *receiver = nullptr);
    [[nodiscard]] AExpectedResult
        execUtf8(QUtf8StringView query, const QVariantList &params, QObject *receiver = nullptr);
    [[nodiscard]] AExpectedMultiResult execMultiUtf8(QUtf8StringView query,
                                                     QObject *receiver = nullptr);
};

/*!
 * \brief ACoroDatabase is the abstract delivery interface used by coroutine awaitables that
 * receive a database connection (ADatabase).
 *
 * The concrete implementation lives in \c ACoroData<ADatabase> (see acoroexpected.h).
 */
class ASQL_EXPORT ACoroDatabase
{
public:
    virtual ~ACoroDatabase()          = default;
    virtual void deliver(ADatabase v) = 0;
};

/*!
 * \brief ADatabaseFn delivers a pooled database connection into a coroutine awaitable.
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

} // namespace ASql

Q_DECLARE_METATYPE(ASql::ADatabaseNotification)
