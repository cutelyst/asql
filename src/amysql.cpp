/*
 * SPDX-FileCopyrightText: (C) 2025 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */
#include "amysql.h"

#include "adrivermysql.h"

using namespace ASql;

namespace ASql {

class AMysqlPrivate
{
public:
    QString connection;
};

} // namespace ASql

AMysql::AMysql(const QString &connectionInfo)
    : d(std::make_unique<AMysqlPrivate>())
{
    d->connection = connectionInfo;
}

AMysql::~AMysql() = default;

std::shared_ptr<ADriverFactory> AMysql::factory(const QUrl &connectionInfo)
{
    return AMysql::factory(connectionInfo.toString(QUrl::None));
}

std::shared_ptr<ADriverFactory> AMysql::factory(const QString &connectionInfo)
{
    auto ret = std::make_shared<AMysql>(connectionInfo);
    return ret;
}

std::shared_ptr<ADriverFactory> AMysql::factory(QStringView connectionInfo)
{
    return AMysql::factory(connectionInfo.toString());
}

ADatabase AMysql::database(const QString &connectionInfo)
{
    ADatabase ret(std::make_shared<AMysql>(connectionInfo));
    return ret;
}

ADriver *AMysql::createRawDriver() const
{
    auto ret = new ADriverMysql(d->connection);
    return ret;
}

std::shared_ptr<ADriver> AMysql::createDriver() const
{
    auto ret = std::make_shared<ADriverMysql>(d->connection);
    return ret;
}

ADatabase AMysql::createDatabase() const
{
    return ADatabase(std::make_shared<ADriverMysql>(d->connection));
}
