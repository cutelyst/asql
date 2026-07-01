/*
 * SPDX-FileCopyrightText: (C) 2020-2025 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "apool.h"

#include "acoroexpected.h"
#include "adriver.h"
#include "adriverfactory.h"
#include "apreparedquery.h"
#include "atransaction.h"

#include <expected>

#include <QLoggingCategory>
#include <QObject>
#include <QPointer>
#include <queue>

Q_LOGGING_CATEGORY(ASQL_POOL, "asql.pool", QtInfoMsg)

using namespace ASql;

struct APoolQueuedClient {
    ADatabaseFn cb;
    QPointer<QObject> receiver;
    bool checkReceiver;
};

struct APoolInternal {
    QString name;
    std::shared_ptr<ADriverFactory> driverFactory;
    QVector<ADriver *> pool;
    std::queue<APoolQueuedClient> connectionQueue;
    ADatabaseFn setupCb;
    ADatabaseFn reuseCb;
    int maxIdleConnections = 1;
    int maximuConnections  = 0;
    int connectionCount    = 0;
};

static thread_local QHash<QStringView, APoolInternal> m_connectionPool;

const QStringView APool::defaultPool = u"asql_default_pool";

void APool::create(std::shared_ptr<ADriverFactory> factory, QStringView poolName)
{
    APool::create(std::move(factory), poolName.toString());
}

void APool::create(std::shared_ptr<ADriverFactory> factory, const QString &poolName)
{
    if (!m_connectionPool.contains(poolName)) {
        APoolInternal pool;
        pool.name          = poolName;
        pool.driverFactory = std::move(factory);
        m_connectionPool.emplace(pool.name, std::move(pool));
    } else {
        qWarning(ASQL_POOL) << "Ignoring addDatabase, connectionName already available" << poolName;
    }
}

void APool::remove(QStringView poolName)
{
    m_connectionPool.remove(poolName);
}

QStringList APool::pools()
{
    QStringList keys;
    for (const auto &conn : std::as_const(m_connectionPool)) {
        keys << conn.name;
    }
    return keys;
}

void APool::pushDatabaseBack(QStringView connectionName, ADriver *driver)
{
    auto it = m_connectionPool.find(connectionName);
    if (it != m_connectionPool.end()) {
        APoolInternal &iPool = it.value();
        if (driver->state() == ADatabase::State::Disconnected) {
            qDebug(ASQL_POOL) << "Deleting database connection as is not open" << driver->isOpen();
            driver->deleteLater();
            --iPool.connectionCount;
            return;
        }

        if (driver->state() == ADatabase::State::Connecting) {
            qDebug(ASQL_POOL) << "Connection still opening, skipping pool return" << driver;
            return;
        }

        // Check for waiting clients
        while (!iPool.connectionQueue.empty()) {
            APoolQueuedClient client = iPool.connectionQueue.front();
            iPool.connectionQueue.pop();
            if ((client.checkReceiver && client.receiver.isNull()) || !client.cb) {
                continue;
            }

            ADatabase db{std::shared_ptr<ADriver>(
                driver, [connectionName = QString(connectionName)](ADriver *driver) {
                pushDatabaseBack(connectionName, driver);
            })};
            client.cb(std::move(db));
            return;
        }

        if (iPool.pool.size() >= iPool.maxIdleConnections) {
            qDebug(ASQL_POOL) << "Deleting database connection due max idle connections"
                              << iPool.maxIdleConnections << iPool.pool.size();
            driver->deleteLater();
            --iPool.connectionCount;
        } else {
            qDebug(ASQL_POOL) << "Returning database connection to pool" << connectionName
                              << driver;
            iPool.pool.push_back(driver);
        }
    } else {
        qCritical(ASQL_POOL) << "Database pool NOT FOUND on pushDatabaseBack" << connectionName;
        delete driver;
    }
}

int APool::currentConnections(QStringView poolName)
{
    auto it = m_connectionPool.find(poolName);
    if (it != m_connectionPool.end()) {
        return it.value().connectionCount;
    }
    return 0;
}

void APool::databaseCallback(QObject *receiver, ADatabaseFn cb, QStringView poolName)
{
    ADatabase db;
    auto it = m_connectionPool.find(poolName);
    if (it != m_connectionPool.end()) {
        APoolInternal &iPool = it.value();
        if (iPool.pool.empty()) {
            if (iPool.maximuConnections && iPool.connectionCount >= iPool.maximuConnections) {
                qInfo(ASQL_POOL) << "Maximum number of connections reached, queuing" << poolName
                                 << iPool.connectionCount << iPool.connectionQueue.size();
                APoolQueuedClient queued;
                queued.cb            = std::move(cb);
                queued.receiver      = receiver;
                queued.checkReceiver = receiver;
                iPool.connectionQueue.emplace(std::move(queued));
                return;
            }
            ++iPool.connectionCount;
            qDebug(ASQL_POOL) << "Creating a database connection for pool" << poolName;
            const QString poolKey = poolName.toString();
            db.d                  = std::shared_ptr<ADriver>(
                iPool.driverFactory->createRawDriver(),
                [poolKey](ADriver *driver) { pushDatabaseBack(poolKey, driver); });
        } else {
            qDebug(ASQL_POOL) << "Reusing a database connection from pool" << poolName;
            ADriver *priv         = iPool.pool.takeLast();
            const QString poolKey = poolName.toString();
            db.d                  = std::shared_ptr<ADriver>(
                priv, [poolKey](ADriver *driver) { pushDatabaseBack(poolKey, driver); });
        }

        if (db.isOpen()) {
            if (iPool.reuseCb) {
                iPool.reuseCb(db);
            }
            if (cb) {
                cb(std::move(db));
            }
        } else {
            db.open(receiver,
                    [setupCb = iPool.setupCb, db, cb = std::move(cb)](
                        bool isOpen, const QString &errorString) mutable {
                if (isOpen) {
                    if (setupCb) {
                        setupCb(db);
                    }
                    if (cb) {
                        cb(std::move(db));
                    }
                } else if (cb) {
                    Q_UNUSED(errorString);
                    cb(std::move(db));
                }
            });
        }
    } else {
        qCritical(ASQL_POOL) << "Database pool NOT FOUND" << poolName;
        if (cb) {
            cb({});
        }
    }
}

AExpectedDatabase APool::database(QObject *receiver, QStringView poolName)
{
    AExpectedDatabase coro(receiver);
    databaseCallback(receiver, ADatabaseFn{std::weak_ptr<ACoroDatabase>{coro.m_data}}, poolName);
    return coro;
}

void APool::setMaxIdleConnections(int max, QStringView poolName)
{
    auto it = m_connectionPool.find(poolName);
    if (it != m_connectionPool.end()) {
        it.value().maxIdleConnections = max;
    } else {
        qCritical(ASQL_POOL) << "Failed to set maximum idle connections: Database pool NOT FOUND"
                             << poolName;
    }
}

int APool::maxIdleConnections(QStringView poolName)
{
    return m_connectionPool.value(poolName).maxIdleConnections;
}

void APool::setMaxConnections(int max, QStringView poolName)
{
    auto it = m_connectionPool.find(poolName);
    if (it != m_connectionPool.end()) {
        it.value().maximuConnections = max;
    } else {
        qCritical(ASQL_POOL) << "Failed to set maximum connections: Database pool NOT FOUND"
                             << poolName;
    }
}

int APool::maxConnections(QStringView poolName)
{
    return m_connectionPool.value(poolName).maximuConnections;
}

void APool::setSetupCallback(ADatabaseFn cb, QStringView poolName)
{
    auto it = m_connectionPool.find(poolName);
    if (it != m_connectionPool.end()) {
        it.value().setupCb = cb;
    } else {
        qCritical(ASQL_POOL) << "Failed to set maximum connections: Database pool NOT FOUND"
                             << poolName;
    }
}

void APool::setReuseCallback(ADatabaseFn cb, QStringView poolName)
{
    auto it = m_connectionPool.find(poolName);
    if (it != m_connectionPool.end()) {
        it.value().reuseCb = cb;
    } else {
        qCritical(ASQL_POOL) << "Failed to set maximum connections: Database pool NOT FOUND"
                             << poolName;
    }
}

AExpectedResult APool::exec(QStringView query, QObject *receiver, QStringView poolName)
{
    AExpectedResult coro(receiver);
    auto ref = coro.ref();

    [](ACoroDataRef ref, auto query, QObject *receiver, QStringView poolName) -> ACoroTerminator {
        auto db = co_await database(receiver, poolName);
        if (db) {
            auto result = co_await db->exec(query, receiver);
            if (result) {
                ref.deliverResult(*result);
            } else {
                AResult error = resultError(result.error());
                ref.deliverResult(error);
            }
            co_return;
        }

        AResult error = resultError(db.error());
        ref.deliverResult(error);
    }(std::move(ref), query, receiver, poolName);

    return coro;
}

AExpectedResult APool::execUtf8(QUtf8StringView query, QObject *receiver, QStringView poolName)
{
    AExpectedResult coro(receiver);
    auto ref = coro.ref();

    [](ACoroDataRef ref, auto query, QObject *receiver, QStringView poolName) -> ACoroTerminator {
        auto db = co_await database(receiver, poolName);
        if (db) {
            auto result = co_await db->execUtf8(query, receiver);
            if (result) {
                ref.deliverResult(*result);
            } else {
                AResult error = resultError(result.error());
                ref.deliverResult(error);
            }
            co_return;
        }

        AResult error = resultError(db.error());
        ref.deliverResult(error);
    }(std::move(ref), query, receiver, poolName);

    return coro;
}

AExpectedMultiResult APool::execMulti(QStringView query, QObject *receiver, QStringView poolName)
{
    AExpectedMultiResult coro(receiver);
    auto ref = coro.ref();

    [](ACoroDataRef ref, auto query, QObject *receiver, QStringView poolName) -> ACoroTerminator {
        auto db = co_await database(receiver, poolName);
        if (db) {
            auto awaiter = db->execMulti(query, receiver);

            auto result = co_await awaiter;
            while (result) {
                ref.deliverResult(*result);

                if (result->lastResultSet()) {
                    co_return;
                }
                result = co_await awaiter;
            };

            AResult error = resultError(result.error());
            ref.deliverResult(error);
            co_return;
        }

        AResult error = resultError(db.error());
        ref.deliverResult(error);
    }(std::move(ref), query, receiver, poolName);

    return coro;
}

AExpectedMultiResult
    APool::execMultiUtf8(QUtf8StringView query, QObject *receiver, QStringView poolName)
{
    AExpectedMultiResult coro(receiver);
    auto ref = coro.ref();

    [](ACoroDataRef ref, auto query, QObject *receiver, QStringView poolName) -> ACoroTerminator {
        auto db = co_await database(receiver, poolName);
        if (db) {
            auto awaiter = db->execMultiUtf8(query, receiver);

            auto result = co_await awaiter;
            while (result) {
                ref.deliverResult(*result);

                if (result->lastResultSet()) {
                    co_return;
                }
                result = co_await awaiter;
            };

            AResult error = resultError(result.error());
            ref.deliverResult(error);
            co_return;
        }

        AResult error = resultError(db.error());
        ref.deliverResult(error);
    }(std::move(ref), query, receiver, poolName);

    return coro;
}

AExpectedResult APool::exec(const APreparedQuery &query, QObject *receiver, QStringView poolName)
{
    AExpectedResult coro(receiver);
    auto ref = coro.ref();

    [](ACoroDataRef ref, auto query, QObject *receiver, QStringView poolName) -> ACoroTerminator {
        auto db = co_await database(receiver, poolName);
        if (db) {
            auto result = co_await db->exec(query, receiver);
            if (result) {
                ref.deliverResult(*result);
            } else {
                AResult error = resultError(result.error());
                ref.deliverResult(error);
            }
            co_return;
        }

        AResult error = resultError(db.error());
        ref.deliverResult(error);
    }(std::move(ref), query, receiver, poolName);

    return coro;
}

AExpectedResult APool::exec(QStringView query,
                            const QVariantList &params,
                            QObject *receiver,
                            QStringView poolName)
{
    AExpectedResult coro(receiver);
    auto ref = coro.ref();

    [](ACoroDataRef ref, auto query, QVariantList params, QObject *receiver, QStringView poolName)
        -> ACoroTerminator {
        auto db = co_await database(receiver, poolName);
        if (db) {
            auto result = co_await db->exec(query, params, receiver);

            if (result) {
                ref.deliverResult(*result);
            } else {
                AResult error = resultError(result.error());
                ref.deliverResult(error);
            }
            co_return;
        }

        AResult error = resultError(db.error());
        ref.deliverResult(error);
    }(std::move(ref), query, params, receiver, poolName);

    return coro;
}

AExpectedResult APool::execUtf8(QUtf8StringView query,
                                const QVariantList &params,
                                QObject *receiver,
                                QStringView poolName)
{
    AExpectedResult coro(receiver);
    auto ref = coro.ref();

    [](ACoroDataRef ref, auto query, QVariantList params, QObject *receiver, QStringView poolName)
        -> ACoroTerminator {
        auto db = co_await database(receiver, poolName);
        if (db) {
            auto result = co_await db->execUtf8(query, params, receiver);
            if (result) {
                ref.deliverResult(*result);
            } else {
                AResult error = resultError(result.error());
                ref.deliverResult(error);
            }
            co_return;
        }

        AResult error = resultError(db.error());
        ref.deliverResult(error);
    }(std::move(ref), query, params, receiver, poolName);

    return coro;
}

AExpectedResult APool::exec(const APreparedQuery &query,
                            const QVariantList &params,
                            QObject *receiver,
                            QStringView poolName)
{
    AExpectedResult coro(receiver);
    auto ref = coro.ref();

    [](ACoroDataRef ref, auto query, QVariantList params, QObject *receiver, QStringView poolName)
        -> ACoroTerminator {
        auto db = co_await database(receiver, poolName);
        if (db) {
            auto result = co_await db->exec(query, params, receiver);
            if (result) {
                ref.deliverResult(*result);
            } else {
                AResult error = resultError(result.error());
                ref.deliverResult(error);
            }
            co_return;
        }

        AResult error = resultError(db.error());
        ref.deliverResult(error);
    }(std::move(ref), query, params, receiver, poolName);

    return coro;
}
AExpectedTransaction APool::begin(QObject *receiver, QStringView poolName)
{
    AExpectedTransaction coro(receiver);
    [](auto chainData, QObject *receiver, QStringView poolName) -> ACoroTerminator {
        auto db = co_await database(receiver, poolName);
        if (db) {
            auto result = co_await db->begin(receiver);
            if (result) {
                chainData->deliverDirect(std::move(*result));
                co_return;
            }
            chainData->deliverDirect(std::unexpected(result.error()));
            co_return;
        }
        chainData->deliverDirect(std::unexpected(db.error()));
    }(coro.m_data, receiver, poolName);
    return coro;
}
