/*
 * SPDX-FileCopyrightText: (C) 2021 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */
#include "ASqlite.hpp"

#include "ADriverSqlite.hpp"

#include <adatabase.h>

using namespace ASql;

namespace ASql {

class ASqlitePrivate
{
public:
    QString connection;
};

} // namespace ASql

ASqlite::ASqlite(const QString &connectionInfo)
    : d(std::make_unique<ASqlitePrivate>())
{
    d->connection = connectionInfo;
}

ASqlite::~ASqlite() = default;

std::shared_ptr<ADriverFactory> ASqlite::factory(const QUrl &connectionInfo)
{
    return ASqlite::factory(connectionInfo.toString(QUrl::None));
}

std::shared_ptr<ADriverFactory> ASqlite::factory(const QString &connectionInfo)
{
    auto ret = std::make_shared<ASqlite>(connectionInfo);
    return ret;
}

std::shared_ptr<ADriverFactory> ASqlite::factory(QStringView connectionInfo)
{
    return ASqlite::factory(connectionInfo.toString());
}

ADatabase ASqlite::database(const QString &connectionInfo)
{
    ADatabase ret(std::make_shared<ASqlite>(connectionInfo));
    return ret;
}

ADriver *ASqlite::createRawDriver() const
{
    auto ret = new ADriverSqlite(d->connection);
    return ret;
}

std::shared_ptr<ADriver> ASqlite::createDriver() const
{
    // auto ret = std::make_shared<ADriverSqlite>(d->connection);
    // return ret;
}

ADatabase ASqlite::createDatabase() const
{
    // return ADatabase(std::make_shared<ADriverSqlite>(d->connection));
}
