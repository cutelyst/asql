/* 
 * SPDX-FileCopyrightText: (C) 2020 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "apool.h"
#include "adatabase_p.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(ASQL_POOL, "asql.pool", QtInfoMsg)

struct APoolQueuedClient {
    std::function<void (ADatabase &)> cb;
    QPointer<QObject> receiver;
    bool checkReceiver;
};

struct APoolInternal {
    QString connectionInfo;
    QVector<ADatabasePrivate *> pool;
    QQueue<APoolQueuedClient> connectionQueue;
    int maxIdleConnections = 1;
    int maximuConnections = 0;
    int connectionCount = 0;
};

static thread_local QHash<QString, APoolInternal> m_connectionPool;

const char *APool::defaultConnection = const_cast<char *>("asql_default_pool");

void APool::addDatabase(const QString &connectionInfo, const QString &connectionName)
{
    if (!m_connectionPool.contains(connectionName)) {
        APoolInternal pool;
        pool.connectionInfo = connectionInfo;
        m_connectionPool.insert(connectionName, pool);
    } else {
        qWarning(ASQL_POOL) << "Ignoring addDatabase, connectionName already available" << connectionName;
    }
}

void APool::pushDatabaseBack(const QString &connectionName, ADatabasePrivate *priv)
{
    auto it = m_connectionPool.find(connectionName);
    if (it != m_connectionPool.end()) {
        APoolInternal &iPool = it.value();
        if (priv->driver->state() == ADatabase::Disconnected) {
            qDebug(ASQL_POOL) << "Deleting database connection as is not open" << priv->driver->isOpen();
            delete priv;
            --iPool.connectionCount;
            return;
        }

        // Check for waiting clients
        while (!iPool.connectionQueue.isEmpty()) {
            APoolQueuedClient client = iPool.connectionQueue.dequeue();
            if ((client.checkReceiver && client.receiver.isNull()) || !client.cb) {
                continue;
            }

            ADatabase db;
            db.d = QSharedPointer<ADatabasePrivate>(priv, [connectionName] (ADatabasePrivate *priv) {
                    pushDatabaseBack(connectionName, priv);
            });
            client.cb(db);
            return;
        }

        if (iPool.pool.size() >= iPool.maxIdleConnections) {
            qDebug(ASQL_POOL) << "Deleting database connection due max idle connections" << iPool.maxIdleConnections << iPool.pool.size();
            delete priv;
            --iPool.connectionCount;
        } else {
            qDebug(ASQL_POOL) << "Returning database connection to pool" << connectionName << priv;
            iPool.pool.push_back(priv);
        }
    } else {
        delete priv;
    }
}

ADatabase APool::database(const QString &connectionName)
{
    ADatabase db;
    auto it = m_connectionPool.find(connectionName);
    if (it != m_connectionPool.end()) {
        APoolInternal &iPool = it.value();
        if (iPool.pool.empty()) {
            if (iPool.maximuConnections && iPool.connectionCount >= iPool.maximuConnections) {
                qWarning(ASQL_POOL) << "Maximum number of connections reached" << connectionName << iPool.connectionCount << iPool.maximuConnections;
            } else {
                ++iPool.connectionCount;
                qDebug(ASQL_POOL) << "Creating a database connection for pool" << connectionName << iPool.connectionInfo;
                db.d = QSharedPointer<ADatabasePrivate>(new ADatabasePrivate(iPool.connectionInfo), [connectionName] (ADatabasePrivate *priv) {
                    pushDatabaseBack(connectionName, priv);
                });
            }
        } else {
            qDebug(ASQL_POOL) << "Reusing a database connection from pool" << connectionName;
            ADatabasePrivate *priv = iPool.pool.takeLast();
            db.d = QSharedPointer<ADatabasePrivate>(priv, [connectionName] (ADatabasePrivate *priv) {
                pushDatabaseBack(connectionName, priv);
            });
        }
    } else {
        qCritical(ASQL_POOL) << "Database connection NOT FOUND in pool" << connectionName;
    }
    db.open();
    return db;
}

void APool::database(std::function<void (ADatabase &)> cb, QObject *receiver, const QString &connectionName)
{
    ADatabase db;
    auto it = m_connectionPool.find(connectionName);
    if (it != m_connectionPool.end()) {
        APoolInternal &iPool = it.value();
        if (iPool.pool.empty()) {
            if (iPool.maximuConnections && iPool.connectionCount >= iPool.maximuConnections) {
                qInfo(ASQL_POOL) << "Maximum number of connections reached, reached" << connectionName << iPool.connectionCount << iPool.maximuConnections;
                APoolQueuedClient queued;
                queued.cb = cb;
                queued.receiver = receiver;
                queued.checkReceiver = receiver;
                iPool.connectionQueue.enqueue(queued);
                return;
            }
            ++iPool.connectionCount;
            qDebug(ASQL_POOL) << "Creating a database connection for pool" << connectionName << iPool.connectionInfo;
            db.d = QSharedPointer<ADatabasePrivate>(new ADatabasePrivate(iPool.connectionInfo), [connectionName] (ADatabasePrivate *priv) {
                    pushDatabaseBack(connectionName, priv);
            });
        } else {
            qDebug(ASQL_POOL) << "Reusing a database connection from pool" << connectionName;
            ADatabasePrivate *priv = iPool.pool.takeLast();
            db.d = QSharedPointer<ADatabasePrivate>(priv, [connectionName] (ADatabasePrivate *priv) {
                    pushDatabaseBack(connectionName, priv);
            });
        }
    } else {
        qCritical(ASQL_POOL) << "Database connection NOT FOUND in pool" << connectionName;
    }
    db.open();

    if (cb) {
        cb(db);
    }
}

void APool::setDatabaseMaxIdleConnections(int max, const QString &connectionName)
{
    auto it = m_connectionPool.find(connectionName);
    if (it != m_connectionPool.end()) {
        it.value().maxIdleConnections = max;
    } else {
        qCritical(ASQL_POOL) << "Database connection NOT FOUND in pool" << connectionName;
    }
}

void APool::setDatabaseMaximumConnections(int max, const QString &connectionName)
{
    auto it = m_connectionPool.find(connectionName);
    if (it != m_connectionPool.end()) {
        it.value().maximuConnections = max;
    } else {
        qCritical(ASQL_POOL) << "Database connection NOT FOUND in pool" << connectionName;
    }
}
