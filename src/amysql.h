/*
 * SPDX-FileCopyrightText: (C) 2025 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "adriverfactory.h"

#include <asql_mysql_export.h>

#include <QUrl>

namespace ASql {

class AMysqlPrivate;
class ASQL_MYSQL_EXPORT AMysql : public ADriverFactory
{
public:
    /*!
     * \brief AMysql constructs a driver factory with the connection info
     *
     * This class allows for creating MySQL driver instances using the connection info.
     *
     * Example of connection info:
     * * Just a database "mysql:///dbname"  (localhost, default port)
     * * Username and database "mysql://username@localhost/dbname"
     * * Username, password, host, port and database
     *   "mysql://username:password@hostname:3306/dbname"
     */
    AMysql(const QString &connectionInfo);
    ~AMysql();

    static std::shared_ptr<ADriverFactory> factory(const QUrl &connectionInfo);
    static std::shared_ptr<ADriverFactory> factory(const QString &connectionInfo);
    static std::shared_ptr<ADriverFactory> factory(QStringView connectionInfo);
    static ADatabase database(const QString &connectionInfo);

    ADriver *createRawDriver() const final;
    std::shared_ptr<ADriver> createDriver() const final;
    ADatabase createDatabase() const final;

private:
    std::unique_ptr<AMysqlPrivate> d;
};

} // namespace ASql
