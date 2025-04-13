/*
 * SPDX-FileCopyrightText: (C) 2021-2023 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "adriverfactory.h"

#include <asql_sqlite_export.h>

#include <QUrl>

namespace ASql {

class ASqlitePrivate;
class ASQL_SQLITE_EXPORT ASqlite : public ADriverFactory
{
public:
    /*!
     * \brief ASqlite contructs a driver factory with the connection info
     *
     * This class allows for creating Sqlite driver using the connection info.
     *
     * Example of connection info:
     * * Just a database path "sqlite:///db_path"
     */
    ASqlite(const QString &connectionInfo);
    ~ASqlite();

    static std::shared_ptr<ADriverFactory> factory(const QUrl &connectionInfo);
    static std::shared_ptr<ADriverFactory> factory(const QString &connectionInfo);
    static std::shared_ptr<ADriverFactory> factory(QStringView connectionInfo);
    static ADatabase database(const QString &connectionInfo);

    ADriver *createRawDriver() const final;
    std::shared_ptr<ADriver> createDriver() const final;
    ADatabase createDatabase() const final;

private:
    std::unique_ptr<ASqlitePrivate> d;
};

} // namespace ASql
