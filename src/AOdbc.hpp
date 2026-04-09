/*
 * SPDX-FileCopyrightText: (C) 2025 Daniel Nicoletti <dantti12@gmail.com>
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
 * Pass a standard ODBC connection string prefixed with \c "odbc:".
 * The \c "odbc:" (or \c "odbc://") prefix is stripped before the
 * string is forwarded to \c SQLDriverConnect.
 *
 * Examples:
 * \code
 * // Connect via a DSN configured in odbcinst.ini / odbc.ini
 * AOdbc::factory(u"odbc:DSN=mydsn;UID=user;PWD=secret"_s);
 *
 * // Connect directly with a driver name
 * AOdbc::factory(u"odbc:DRIVER={PostgreSQL};SERVER=localhost;DATABASE=mydb;UID=postgres;PWD=secret"_s);
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

    ADriver *createRawDriver() const final;
    std::shared_ptr<ADriver> createDriver() const final;
    ADatabase createDatabase() const final;

private:
    std::unique_ptr<AOdbcPrivate> d;
};

} // namespace ASql
