#include "apool.h"
#include "adatabase_p.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(ASQL_POOL, "asql.pool", QtInfoMsg)

typedef struct {
    QString connectionInfo;
    QVector<ADatabasePrivate *> pool;
    int maxIdleConnections = 1;
    int maximuConnections = 0;
    int connectionCount = 0;
} APoolInternal;

static thread_local QHash<QString, APoolInternal> m_connectionPool;

const char *APool::defaultConnection = const_cast<char *>("asql_default_pool");

APool::APool(QObject *parent) : QObject(parent)
{

}

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

static void pushDatabaseBack(const QString &connectionName, ADatabasePrivate *priv)
{
    auto it = m_connectionPool.find(connectionName);
    if (it != m_connectionPool.end()) {
        APoolInternal &iPool = it.value();
        if (iPool.pool.size() >= iPool.maxIdleConnections || priv->driver->state() == ADatabase::Disconnected) {
            qDebug(ASQL_POOL) << "Deleting database connection due max idle connections or it is not open" << iPool.maxIdleConnections << iPool.pool.size() << priv->driver->isOpen();
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
    auto it = m_connectionPool.find(connectionName);
    if (it != m_connectionPool.end()) {
        APoolInternal &iPool = it.value();
        ADatabase db;
        if (iPool.pool.empty()) {
            if (iPool.maximuConnections && iPool.connectionCount >= iPool.maximuConnections) {
                qWarning(ASQL_POOL) << "Maximum number of connections reached" << connectionName << iPool.connectionCount << iPool.maximuConnections;
                return db;
            }
            ++iPool.connectionCount;
            qDebug(ASQL_POOL) << "Creating a database connection for pool" << connectionName << iPool.connectionInfo;
            db.d = QSharedPointer<ADatabasePrivate>(new ADatabasePrivate(iPool.connectionInfo), [connectionName] (ADatabasePrivate *priv) {
                    pushDatabaseBack(connectionName, priv);
            });
            db.open();
        } else {
            qDebug(ASQL_POOL) << "Reusing a database connection from pool" << connectionName;
            ADatabasePrivate *priv = iPool.pool.takeLast();
            db.d = QSharedPointer<ADatabasePrivate>(priv, [connectionName] (ADatabasePrivate *priv) {
                    pushDatabaseBack(connectionName, priv);
            });
        }
        return db;
    } else {
        qCritical(ASQL_POOL) << "Database connection NOT FOUND in pool" << connectionName;
        return ADatabase();
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
