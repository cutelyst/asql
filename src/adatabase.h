/*
 * SPDX-FileCopyrightText: (C) 2020-2023 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <asql_export.h>
#include <chrono>
#include <functional>
#include <memory>

#include <QObject>
#include <QVariantList>

namespace ASql {

class AResult;
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

using AResultFn       = std::function<void(AResult &result)>;
using ANotificationFn = std::function<void(const ADatabaseNotification &payload)>;

template <typename T>
class ACoroExpected;

using AExpectedResult      = ACoroExpected<AResult>;
using AExpectedTransaction = ACoroExpected<ATransaction>;

class APreparedQuery;
class ASQL_EXPORT ADatabase
{
    Q_GADGET
public:
    enum class State { Disconnected, Connecting, Connected };
    Q_ENUM(State)

    /*!
     * \brief ADatabase contructs an invalid database object
     */
    ADatabase();

    /*!
     * \brief ADatabase contructs an database object with the supplied driver
     */
    ADatabase(const std::shared_ptr<ADriver> &driver);

    /*!
     * \brief ADatabase contructs an database object with the supplied driver
     */
    ADatabase(std::shared_ptr<ADriver> &&driver);

    /*!
     * \brief ADatabase contructs an database object with the supplied driver factory
     */
    ADatabase(const std::shared_ptr<ADriverFactory> &factory);

    /*!
     * \brief ADatabase contructs an database object from another database object
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
    [[nodiscard]] bool isValid();

    /*!
     * \brief open the database, the callback is called once the operation is done
     * either by success or failure, with \param describing the error.
     *
     * The callback function is only called if current state is Disconnected.
     *
     * \param cb
     */
    void open(QObject *receiver                                         = nullptr,
              std::function<void(bool isOpen, const QString &error)> cb = {});

    /*!
     * \brief state
     * \return database connection state
     */
    [[nodiscard]] State state() const;

    /*!
     * \brief onStateChanged the callback is called once connection state changes
     *
     * Only one callback can be registered per database
     *
     * \param cb
     */
    void onStateChanged(QObject *receiver,
                        std::function<void(State state, const QString &status)> cb);

    /*!
     * \brief isOpen returns if the database connection is open.
     * \return true if open
     */
    [[nodiscard]] bool isOpen() const;

    /*!
     * \brief begin a transaction, this operation usually succeeds,
     * but one can hook up a callback to check it's result.
     *
     * \param cb
     */
    void begin(QObject *receiver = nullptr, AResultFn cb = {});

    AExpectedTransaction coBegin(QObject *receiver = nullptr);

    /*!
     * \brief commit a transaction, this operation usually succeeds,
     * but one can hook up a callback to check it's result.
     *
     * \param cb
     */
    void commit(QObject *receiver = nullptr, AResultFn cb = {});

    /*!
     * \brief rollback a transaction, this operation usually succeeds,
     * but one can hook up a callback to check it's result.
     *
     * \param cb
     */
    void rollback(QObject *receiver = nullptr, AResultFn cb = {});

    AExpectedResult coExec(QStringView query, QObject *receiver = nullptr);

    AExpectedResult
        coExec(QStringView query, const QVariantList &params, QObject *receiver = nullptr);

    AExpectedResult coExec(QUtf8StringView query, QObject *receiver = nullptr);

    AExpectedResult
        coExec(QUtf8StringView query, const QVariantList &params, QObject *receiver = nullptr);

    AExpectedResult coExec(const APreparedQuery &query, QObject *receiver = nullptr);

    AExpectedResult coExec(const APreparedQuery &query,
                           const QVariantList &params,
                           QObject *receiver = nullptr);

    /*!
     * \brief exec excutes a \param query against this database connection,
     * once done AResult object will have the retrieved data if any, always
     * check for AResult::error() to see if the query was successful.
     *
     * Postgres allows for multiple commands to be sent, in this case the callback
     * will be called once per command you must then check for AResult::lastResultSet(),
     * if one of the commands fails the subsequently ones will not be delivered, which
     * is why checking for AResult::lastResultSet() is important. This feature is
     * not supported by Postgres on the method that accepts params.
     *
     * \param query
     * \param cb
     */
    void exec(QStringView query, QObject *receiver = nullptr, AResultFn cb = {});

    /*!
     * \brief exec excutes a \param query against this database connection,
     * once done AResult object will have the retrieved data if any, always
     * check for AResult::error() to see if the query was successful.
     *
     * Postgres allows for multiple commands to be sent, in this case the callback
     * will be called once per command you must then check for AResult::lastResultSet(),
     * if one of the commands fails the subsequently ones will not be delivered, which
     * is why checking for AResult::lastResultSet() is important. This feature is
     * not supported by Postgres on the method that accepts params.
     *
     * \note Since ASql might queue queries only use this method for strings that can outlive
     * the query execution, such as string literals
     *
     * \param query
     * \param cb
     */
    void exec(QUtf8StringView query, QObject *receiver = nullptr, AResultFn cb = {});

    /*!
     * \brief exec executes a prepared \param query against this database connection,
     * once done AResult object will have the retrieved data if any, always
     * check for AResult::error() to see if the query was successful.
     *
     * \note For proper usage of APreparedQuery see it's documentation.
     * \note Postgres does not allow for multiple commands on prepared queries.
     *
     * \param query
     * \param cb
     */
    void exec(const APreparedQuery &query, QObject *receiver = nullptr, AResultFn cb = {});

    /*!
     * \brief exec executes a \param query against this database connection,
     * once done AResult object will have the retrieved data if any, always
     * check for AResult::error() to see if the query was successful.
     *
     * \param query
     * \param params
     * \param cb
     */
    void exec(QStringView query,
              const QVariantList &params,
              QObject *receiver = nullptr,
              AResultFn cb      = {});

    /*!
     * \brief exec executes a \param query against this database connection,
     * once done AResult object will have the retrieved data if any, always
     * check for AResult::error() to see if the query was successful.
     *
     * \note Since ASql might queue queries only use this method for strings that can outlive
     * the query execution, such as string literals
     *
     * \param query
     * \param params
     * \param cb
     */
    void exec(QUtf8StringView query,
              const QVariantList &params,
              QObject *receiver = nullptr,
              AResultFn cb      = {});

    /*!
     * \brief exec executes a prepared \param query against this database connection
     * with the following \p params to be bound,
     * once done AResult object will have the retrieved data if any, always
     * check for AResult::error() to see if the query was successful.
     *
     * \note For proper usage of APreparedQuery see it's documentation.
     * \note Postgres does not allow for multiple commands on prepared queries.
     *
     * \param query
     * \param cb
     */
    void exec(const APreparedQuery &query,
              const QVariantList &params,
              QObject *receiver = nullptr,
              AResultFn cb      = {});

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
     * @brief exitPipelineModewill disables the pipeline mode on the driver, it's queue must be
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
     * \param cb
     */
    void subscribeToNotification(const QString &channel, QObject *receiver, ANotificationFn cb);

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
};

} // namespace ASql
