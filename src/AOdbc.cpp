/*
 * SPDX-FileCopyrightText: (C) 2025 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */
#include "AOdbc.hpp"

#include "ADriverOdbc.hpp"

#include <adatabase.h>

using namespace ASql;

namespace ASql {

class AOdbcPrivate
{
public:
    QString connection;
};

} // namespace ASql

AOdbc::AOdbc(const QString &connectionInfo)
    : d(std::make_unique<AOdbcPrivate>())
{
    d->connection = connectionInfo;
}

AOdbc::~AOdbc() = default;

std::shared_ptr<ADriverFactory> AOdbc::factory(const QUrl &connectionInfo)
{
    return AOdbc::factory(connectionInfo.toString(QUrl::None));
}

std::shared_ptr<ADriverFactory> AOdbc::factory(const QString &connectionInfo)
{
    return std::make_shared<AOdbc>(connectionInfo);
}

std::shared_ptr<ADriverFactory> AOdbc::factory(QStringView connectionInfo)
{
    return AOdbc::factory(connectionInfo.toString());
}

ADatabase AOdbc::database(const QString &connectionInfo)
{
    ADatabase ret(std::make_shared<AOdbc>(connectionInfo));
    return ret;
}

ADriver *AOdbc::createRawDriver() const
{
    return new ADriverOdbc(d->connection);
}

std::shared_ptr<ADriver> AOdbc::createDriver() const
{
    return std::make_shared<ADriverOdbc>(d->connection);
}

ADatabase AOdbc::createDatabase() const
{
    return ADatabase(std::make_shared<ADriverOdbc>(d->connection));
}
