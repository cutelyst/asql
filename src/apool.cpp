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

        // Check for waiting clients
        while (!iPool.connectionQueue.empty()) {
            APoolQueuedClient client = iPool.connectionQueue.front();
            iPool.connectionQueue.pop();
            if ((client.checkReceiver && client.receiver.isNull()) || !client.cb) {
                continue;
            }

            ADatabase db{std::shared_ptr<ADriver>(driver, [connectionName](ADriver *driver) {
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
        delete driver;
    }
}

ADatabase APool::database(QStringView poolName)
{
    ADatabase db;
    auto it = m_connectionPool.find(poolName);
    if (it != m_connectionPool.end()) {
        APoolInternal &iPool = it.value();
        if (iPool.pool.empty()) {
            if (iPool.maximuConnections && iPool.connectionCount >= iPool.maximuConnections) {
                qCritical(ASQL_POOL) << "Maximum number of connections reached" << poolName
                                     << iPool.connectionCount << iPool.connectionQueue.size();
            } else {
                ++iPool.connectionCount;
                auto driver = iPool.driverFactory->createRawDriver();
                qDebug(ASQL_POOL) << "Creating a database connection for pool" << poolName
                                  << driver;
                db.d = std::shared_ptr<ADriver>(driver, [poolName = iPool.name](ADriver *driver) {
                    pushDatabaseBack(poolName, driver);
                });
            }
        } else {
            qDebug(ASQL_POOL) << "Reusing a database connection from pool" << poolName;
            ADriver *driver = iPool.pool.takeLast();
            db.d = std::shared_ptr<ADriver>(driver, [poolName = iPool.name](ADriver *driver) {
                pushDatabaseBack(poolName, driver);
            });
        }

        if (db.isOpen()) {
            if (iPool.reuseCb) {
                iPool.reuseCb(db);
            }
        } else {
            db.open(nullptr,
                    [setupCb = iPool.setupCb, db](bool isOpen, const QString &errorString) {
                if (isOpen && setupCb) {
                    setupCb(db);
                }
            });
        }
    } else {
        qCritical(ASQL_POOL) << "Database pool NOT FOUND" << poolName;
        db.open();
    }
    return db;
}

int APool::currentConnections(QStringView poolName)
{
    auto it = m_connectionPool.find(poolName);
    if (it != m_connectionPool.end()) {
        return it.value().connectionCount;
    }
    return 0;
}

void APool::database(QObject *receiver, ADatabaseFn cb, QStringView poolName)
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
                queued.cb            = cb;
                queued.receiver      = receiver;
                queued.checkReceiver = receiver;
                iPool.connectionQueue.emplace(std::move(queued));
                return;
            }
            ++iPool.connectionCount;
            qDebug(ASQL_POOL) << "Creating a database connection for pool" << poolName;
            db.d = std::shared_ptr<ADriver>(
                iPool.driverFactory->createRawDriver(),
                [poolName](ADriver *driver) { pushDatabaseBack(poolName, driver); });
        } else {
            qDebug(ASQL_POOL) << "Reusing a database connection from pool" << poolName;
            ADriver *priv = iPool.pool.takeLast();
            db.d          = std::shared_ptr<ADriver>(
                priv, [poolName](ADriver *driver) { pushDatabaseBack(poolName, driver); });
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
                    [setupCb = iPool.setupCb, db, cb](bool isOpen, const QString &errorString) {
                if (isOpen && setupCb) {
                    setupCb(db);
                }

                if (cb) {
                    cb(std::move(db));
                }
            });
        }
    } else {
        qCritical(ASQL_POOL) << "Database pool NOT FOUND" << poolName;
        if (cb) {
            db.open();
            cb(std::move(db));
        }
    }
}

AExpectedDatabase APool::coDatabase(QObject *receiver, QStringView poolName)
{
    AExpectedDatabase coro(receiver);
    database(receiver, coro.callback, poolName);
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
    auto cb = coro.callback;

    [](AResultFn cb, auto query, QObject *receiver, QStringView poolName) -> ACoroTerminator {
        auto db = co_await coDatabase(receiver, poolName);
        if (db) {
            auto result = co_await db->coExec(query, receiver);
            if (result) {
                cb(*result);
            }
        }

        AResult result;
        cb(result);
    }(cb, query, receiver, poolName);

    return coro;
}

AExpectedResult APool::exec(QUtf8StringView query, QObject *receiver, QStringView poolName)
{
    AExpectedResult coro(receiver);
    auto cb = coro.callback;

    [](AResultFn cb, auto query, QObject *receiver, QStringView poolName) -> ACoroTerminator {
        auto db = co_await coDatabase(receiver, poolName);
        if (db) {
            auto result = co_await db->coExec(query, receiver);
            if (result) {
                cb(*result);
            }
        }

        AResult result;
        cb(result);
    }(cb, query, receiver, poolName);

    return coro;
}

AExpectedMultiResult APool::execMulti(QStringView query, QObject *receiver, QStringView poolName)
{
    AExpectedMultiResult coro(receiver);
    auto cb = coro.callback;

    [](AResultFn cb, auto query, QObject *receiver, QStringView poolName) -> ACoroTerminator {
        auto db = co_await coDatabase(receiver, poolName);
        if (db) {
            auto awaiter = db->execMulti(query, receiver);

            auto result = co_await awaiter;
            while (result) {
                cb(*result);

                if (result->lastResultSet()) {
                    co_return;
                }
                result = co_await awaiter;
            };
        }

        AResult result;
        cb(result);
    }(cb, query, receiver, poolName);

    return coro;
}

AExpectedMultiResult
    APool::execMulti(QUtf8StringView query, QObject *receiver, QStringView poolName)
{
    AExpectedMultiResult coro(receiver);
    auto cb = coro.callback;

    [](AResultFn cb, auto query, QObject *receiver, QStringView poolName) -> ACoroTerminator {
        auto db = co_await coDatabase(receiver, poolName);
        if (db) {
            auto awaiter = db->execMulti(query, receiver);

            auto result = co_await awaiter;
            while (result) {
                cb(*result);

                if (result->lastResultSet()) {
                    co_return;
                }
                result = co_await awaiter;
            };
        }

        AResult result;
        cb(result);
    }(cb, query, receiver, poolName);

    return coro;
}

AExpectedResult APool::exec(const APreparedQuery &query, QObject *receiver, QStringView poolName)
{
    AExpectedResult coro(receiver);
    auto cb = coro.callback;

    [](AResultFn cb, auto query, QObject *receiver, QStringView poolName) -> ACoroTerminator {
        auto db = co_await coDatabase(receiver, poolName);
        if (db) {
            auto result = co_await db->coExec(query, receiver);
            if (result) {
                cb(*result);
            }
        }

        AResult result;
        cb(result);
    }(cb, query, receiver, poolName);

    return coro;
}

AExpectedResult APool::exec(QStringView query,
                            const QVariantList &params,
                            QObject *receiver,
                            QStringView poolName)
{
    AExpectedResult coro(receiver);
    auto cb = coro.callback;

    [](AResultFn cb, auto query, QVariantList params, QObject *receiver, QStringView poolName)
        -> ACoroTerminator {
        auto db = co_await coDatabase(receiver, poolName);
        if (db) {
            auto result = co_await db->coExec(query, params, receiver);
            if (result) {
                cb(*result);
            }
        }

        AResult result;
        cb(result);
    }(cb, query, params, receiver, poolName);

    return coro;
}

AExpectedResult APool::exec(QUtf8StringView query,
                            const QVariantList &params,
                            QObject *receiver,
                            QStringView poolName)
{
    AExpectedResult coro(receiver);
    auto cb = coro.callback;

    [](AResultFn cb, auto query, QVariantList params, QObject *receiver, QStringView poolName)
        -> ACoroTerminator {
        auto db = co_await coDatabase(receiver, poolName);
        if (db) {
            auto result = co_await db->coExec(query, params, receiver);
            if (result) {
                cb(*result);
            }
        }

        AResult result;
        cb(result);
    }(cb, query, params, receiver, poolName);

    return coro;
}

AExpectedResult APool::exec(const APreparedQuery &query,
                            const QVariantList &params,
                            QObject *receiver,
                            QStringView poolName)
{
    AExpectedResult coro(receiver);
    auto cb = coro.callback;

    [](AResultFn cb, auto query, QVariantList params, QObject *receiver, QStringView poolName)
        -> ACoroTerminator {
        auto db = co_await coDatabase(receiver, poolName);
        if (db) {
            auto result = co_await db->coExec(query, params, receiver);
            if (result) {
                cb(*result);
            }
        }

        AResult result;
        cb(result);
    }(cb, query, params, receiver, poolName);

    return coro;
}

AExpectedTransaction APool::begin(QObject *receiver, QStringView poolName)
{
    AExpectedTransaction coro(receiver);
    auto cb = coro.callback;

    // TODO fix me &coro will crash
    database(receiver, [cb, receiver, &coro](ADatabase db) {
        if (db.isValid()) {
            coro.database = db;
            db.begin(receiver, cb);
            return;
        }

        AResult result;
        cb(result);
    }, poolName);

    return coro;
}
