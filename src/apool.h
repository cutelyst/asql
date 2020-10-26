/* 
 * SPDX-FileCopyrightText: (C) 2020 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef APOOL_H
#define APOOL_H

#include <QObject>
#include <QUrl>

#include <adatabase.h>

#include <aqsqlexports.h>

class ASQL_EXPORT APool
{
public:
    static const char *defaultConnection;

    /*!
     * \brief addDatabase connection to pool
     *
     * Every time database is called, a new database object is returned, unless an
     * idle connection (one that were previously dereferenced) is available.
     *
     * \param connectionInfo is a driver url such as postgresql://user:pass@host:port/dbname
     * \param connectionName is an identifier for such connections, for example "read-write" or "read-only-replicas"
     */
    static void addDatabase(const QString &connectionInfo, const QString &connectionName = QLatin1String(defaultConnection));
    static ADatabase database(const QString &connectionName = QLatin1String(defaultConnection));

    /*!
     * \brief retrieves a database object
     *
     * This method is only useful if the pool has a limit of maximum connections allowed,
     * when the limit is reached instead of immediately returning a database object it will
     * queue the request and once an object is freed the callback is issued.
     *
     * \param receiver
     * \param connectionName
     */
    static void database(std::function<void(ADatabase &database)>, QObject *receiver = nullptr, const QString &connectionName = QLatin1String(defaultConnection));


    static void setDatabaseMaxIdleConnections(int max, const QString &connectionName = QLatin1String(defaultConnection));
    static void setDatabaseMaximumConnections(int max, const QString &connectionName = QLatin1String(defaultConnection));

private:
    inline static void pushDatabaseBack(const QString &connectionName, ADatabasePrivate *priv);
};

#endif // APOOL_H
