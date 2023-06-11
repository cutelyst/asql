/*
 * SPDX-FileCopyrightText: (C) 2020-2021 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "apool.h"

#include "adriver.h"
#include "adriverfactory.h"

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

void APool::create(const std::shared_ptr<ADriverFactory> &factory, QStringView poolName)
{
    APool::create(factory, poolName.toString());
}

void APool::create(const std::shared_ptr<ADriverFactory> &factory, const QString &poolName)
{
    if (!m_connectionPool.contains(poolName)) {
        APoolInternal pool;
        pool.name          = poolName;
        pool.driverFactory = factory;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        m_connectionPool.emplace(pool.name, std::move(pool));
#else
        m_connectionPool.insert(pool.name, pool);
#endif
    } else {
        qWarning(ASQL_POOL) << "Ignoring addDatabase, connectionName already available" << poolName;
    }
}

void APool::remove(QStringView poolName)
{
    m_connectionPool.remove(poolName);
}

void APool::pushDatabaseBack(QStringView connectionName, ADriver *driver)
{
    auto it = m_connectionPool.find(connectionName);
    if (it != m_connectionPool.end()) {
        APoolInternal &iPool = it.value();
        if (driver->state() == ADatabase::State::Disconnected) {
            qDebug(ASQL_POOL) << "Deleting database connection as is not open" << driver->isOpen();
            delete driver;
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
            qDebug(ASQL_POOL) << "Deleting database connection due max idle connections" << iPool.maxIdleConnections << iPool.pool.size();
            delete driver;
            --iPool.connectionCount;
        } else {
            qDebug(ASQL_POOL) << "Returning database connection to pool" << connectionName << driver;
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
                qCritical(ASQL_POOL) << "Maximum number of connections reached" << poolName << iPool.connectionCount << iPool.connectionQueue.size();
            } else {
                ++iPool.connectionCount;
                auto driver = iPool.driverFactory->createRawDriver();
                qDebug(ASQL_POOL) << "Creating a database connection for pool" << poolName << driver;
                db.d = std::shared_ptr<ADriver>(driver, [poolName](ADriver *driver) {
                    pushDatabaseBack(poolName, driver);
                });

                if (iPool.setupCb) {
                    iPool.setupCb(db);
                }
            }
        } else {
            qDebug(ASQL_POOL) << "Reusing a database connection from pool" << poolName;
            ADriver *driver = iPool.pool.takeLast();
            db.d            = std::shared_ptr<ADriver>(driver, [poolName](ADriver *driver) {
                pushDatabaseBack(poolName, driver);
            });

            if (iPool.reuseCb) {
                iPool.reuseCb(db);
            }
        }
    } else {
        qCritical(ASQL_POOL) << "Database pool NOT FOUND" << poolName;
    }
    db.open();
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
    auto it = m_connectionPool.find(poolName);
    if (it != m_connectionPool.end()) {
        APoolInternal &iPool = it.value();
        if (iPool.pool.empty()) {
            if (iPool.maximuConnections && iPool.connectionCount >= iPool.maximuConnections) {
                qInfo(ASQL_POOL) << "Maximum number of connections reached, queuing" << poolName << iPool.connectionCount << iPool.connectionQueue.size();
                APoolQueuedClient queued;
                queued.cb            = cb;
                queued.receiver      = receiver;
                queued.checkReceiver = receiver;
                iPool.connectionQueue.emplace(std::move(queued));
                return;
            }
            ++iPool.connectionCount;
            qDebug(ASQL_POOL) << "Creating a database connection for pool" << poolName;
            ADatabase db{std::shared_ptr<ADriver>(iPool.driverFactory->createRawDriver(), [poolName](ADriver *driver) {
                pushDatabaseBack(poolName, driver);
            })};

            if (iPool.setupCb) {
                iPool.setupCb(db);
            }

            db.open();
            if (cb) {
                cb(std::move(db));
            }
        } else {
            qDebug(ASQL_POOL) << "Reusing a database connection from pool" << poolName;
            ADriver *priv = iPool.pool.takeLast();
            ADatabase db{std::shared_ptr<ADriver>(priv, [poolName](ADriver *driver) {
                pushDatabaseBack(poolName, driver);
            })};

            if (iPool.reuseCb) {
                iPool.reuseCb(db);
            }

            db.open();
            if (cb) {
                cb(std::move(db));
            }
        }
    } else {
        qCritical(ASQL_POOL) << "Database pool NOT FOUND" << poolName;

        ADatabase db;
        db.open();
        if (cb) {
            cb(std::move(db));
        }
    }
}

void APool::setMaxIdleConnections(int max, QStringView poolName)
{
    auto it = m_connectionPool.find(poolName);
    if (it != m_connectionPool.end()) {
        it.value().maxIdleConnections = max;
    } else {
        qCritical(ASQL_POOL) << "Failed to set maximum idle connections: Database pool NOT FOUND" << poolName;
    }
}

void APool::setMaxConnections(int max, QStringView poolName)
{
    auto it = m_connectionPool.find(poolName);
    if (it != m_connectionPool.end()) {
        it.value().maximuConnections = max;
    } else {
        qCritical(ASQL_POOL) << "Failed to set maximum connections: Database pool NOT FOUND" << poolName;
    }
}

void APool::setSetupCallback(ADatabaseFn cb, QStringView poolName)
{
    auto it = m_connectionPool.find(poolName);
    if (it != m_connectionPool.end()) {
        it.value().setupCb = cb;
    } else {
        qCritical(ASQL_POOL) << "Failed to set maximum connections: Database pool NOT FOUND" << poolName;
    }
}

void APool::setReuseCallback(ADatabaseFn cb, QStringView poolName)
{
    auto it = m_connectionPool.find(poolName);
    if (it != m_connectionPool.end()) {
        it.value().reuseCb = cb;
    } else {
        qCritical(ASQL_POOL) << "Failed to set maximum connections: Database pool NOT FOUND" << poolName;
    }
}
