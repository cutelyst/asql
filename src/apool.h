/* 
 * SPDX-FileCopyrightText: (C) 2020-2021 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef APOOL_H
#define APOOL_H

#include <QObject>
#include <QUrl>

#include <adatabase.h>
#include <adriverfactory.h>

#include <aqsqlexports.h>

class ASQL_EXPORT APool
{
public:
    static const char *defaultPool;

    Q_DECL_DEPRECATED_X("Use create instead.")
    static void addDatabase(const std::shared_ptr<ADriverFactory> &factory, const QString &poolName = QLatin1String(defaultPool));

    /*!
     * \brief create creates a new database pool
     *
     * Creates a new connection Pool that uses the factory to create new connections when they are required.
     *
     * \param factory is a driver factory that creates new connections
     * \param poolName is an identifier for such pools, for example "read-write" or "read-only-replicas"
     */
    static void create(const std::shared_ptr<ADriverFactory> &factory, const QString &poolName = QLatin1String(defaultPool));

    /*!
     * \brief remove removes the database pool
     *
     * Removes the \p poolName from the connection pool, it doesn't remove or close current connections.
     *
     * \param poolName
     */
    static void remove(const QString &poolName = QLatin1String(defaultPool));

    /*!
     * \brief database
     *
     * This method returns a new database object, unless an idle connection
     * (one that were previously dereferenced) is available on the pool.
     *
     * If the pool was not created or has reached it's maximum limit an invalid
     * database object is returned.
     *
     * \param poolName
     * \return ADatabase
     */
    static ADatabase database(const QString &poolName = QLatin1String(defaultPool));

    /*!
     * \brief currentConnections of the pool
     * \param poolName
     * \return the number of active connections on this pool
     */
    static int currentConnections(const QString &poolName = QLatin1String(defaultPool));

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
    static void database(std::function<void(ADatabase &database)>, QObject *receiver = nullptr, const QString &poolName = QLatin1String(defaultPool));

    /*!
     * \brief setMaxIdleConnections maximum number of idle connections of the pool
     *
     * The default value is 1, so if 2 connections are created when they are returned the
     * second one will be deleted.
     *
     * \param max
     * \param poolName
     */
    static void setMaxIdleConnections(int max, const QString &poolName = QLatin1String(defaultPool));

    /*!
     * \brief setMaxConnections maximum number of connections of the pool
     *
     * The default value is 0, which means ilimited, if a limit is set the \sa database method
     * will start returning invalid objects untill the current number of connections is reduced.
     *
     * Changing this value only affect new connections created.
     *
     * \param max
     * \param poolName
     */
    static void setMaxConnections(int max, const QString &poolName = QLatin1String(defaultPool));

    Q_DECL_DEPRECATED_X("Use setMaxIdleConnections instead.")
    static void setDatabaseMaxIdleConnections(int max, const QString &poolName = QLatin1String(defaultPool));

    Q_DECL_DEPRECATED_X("Use setMaxConnections instead.")
    static void setDatabaseMaximumConnections(int max, const QString &poolName = QLatin1String(defaultPool));

private:
    inline static void pushDatabaseBack(const QString &connectionName, ADriver *driver);
};

#endif // APOOL_H
