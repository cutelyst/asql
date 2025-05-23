/*
 * SPDX-FileCopyrightText: (C) 2021-2023 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "adriverfactory.h"

#include <asql_pg_export.h>

#include <QUrl>

namespace ASql {

class APgPrivate;
class ASQL_PG_EXPORT APg : public ADriverFactory
{
public:
    /*!
     * \brief APg contructs a driver factory with the connection info
     *
     * This class allows for creating Postgres driver using the connection info.
     *
     * Example of connection info:
     * * Just a database db1 "postgresql:///db1"
     * * Username and database "postgresql://username@/db2"
     * * Username, host, database and options
     * "postgresql://username@example.com/db3/bng?target_session_attrs=read-write"
     */
    APg(const QString &connectionInfo);
    ~APg();

    static std::shared_ptr<ADriverFactory> factory(const QUrl &connectionInfo);
    static std::shared_ptr<ADriverFactory> factory(const QString &connectionInfo);
    static std::shared_ptr<ADriverFactory> factory(QStringView connectionInfo);
    static ADatabase database(const QString &connectionInfo);

    ADriver *createRawDriver() const final;
    std::shared_ptr<ADriver> createDriver() const final;
    ADatabase createDatabase() const final;

private:
    std::unique_ptr<APgPrivate> d;
};

} // namespace ASql
