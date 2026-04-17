/*
 * SPDX-FileCopyrightText: (C) 2026 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "adriverfactory.h"

#include <asql_odbc_export.h>

#include <QUrl>

namespace ASql {

class AOdbcPrivate;

/*!
 * \brief AOdbc provides an asynchronous unixODBC database driver factory.
 *
 * This class allows creating ODBC-backed ADatabase instances. Because
 * ODBC does not expose asynchronous I/O primitives, all database
 * operations are offloaded to a dedicated worker thread (the same
 * approach used by the SQLite driver), keeping the Qt event loop free.
 *
 * \section Connection string format
 *
 * Pass a standard ODBC connection string, or QUrl with scheme
 * \c "odbc://" or \c "odbcs://" for encrypted connections. The host part of the URL is used as the
 * \c SERVER parameter, and the port is used as the \c PORT parameter. The path part of the URL is
 * used as the \c DATABASE parameter, unless a \c DATABASE query item is present, in which case the
 * path is ignored. User credentials can be specified using the \c UID and \c PWD query items, or
 * directly in the URL. Query items can be used to specify any additional ODBC parameters.
 *
 * Examples:
 * \code
 * // Connect via a URL with ODBC scheme
 * AOdbc::factory(QUrl{u"odbc://user:password@mydsn/database?DRIVER={ODBC Driver 18 for SQL
 * Server}"_s});
 *
 * // Connect directly with a ODBC connection string
 * AOdbc::factory(u"DRIVER={ODBC Driver 18 for SQL
 * Server};SERVER=localhost;DATABASE=mydb;UID=user;PWD=password"_s);
 * \endcode
 */
class ASQL_ODBC_EXPORT AOdbc : public ADriverFactory
{
public:
    /*!
     * \brief Constructs an AOdbc factory with the given \p connectionInfo.
     */
    AOdbc(const QString &connectionInfo);
    ~AOdbc();

    static std::shared_ptr<ADriverFactory> factory(const QUrl &connectionInfo);
    static std::shared_ptr<ADriverFactory> factory(const QString &connectionInfo);
    static std::shared_ptr<ADriverFactory> factory(QStringView connectionInfo);
    static ADatabase database(const QString &connectionInfo);
    static ADatabase database(const QUrl &connectionInfo);

    ADriver *createRawDriver() const final;
    std::shared_ptr<ADriver> createDriver() const final;
    ADatabase createDatabase() const final;

private:
    std::unique_ptr<AOdbcPrivate> d;
};

} // namespace ASql
