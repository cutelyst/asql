/*
 * SPDX-FileCopyrightText: (C) 2021 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */
#include "apg.h"
#include "adriverpg.h"

class APgPrivate
{
public:
    QString connection;
};

APg::APg(const QString &connectionInfo)
    : d(new APgPrivate)
{
    d->connection = connectionInfo;
}

APg::~APg()
{
    delete d;
}

std::shared_ptr<ADriverFactory> APg::factory(const QUrl &connectionInfo)
{
    return APg::factory(connectionInfo.toString(QUrl::None));
}

std::shared_ptr<ADriverFactory> APg::factory(const QString &connectionInfo)
{
    auto ret = std::make_shared<APg>(connectionInfo);
    return ret;
}

ADatabase APg::database(const QString &connectionInfo)
{
    ADatabase ret(std::make_shared<APg>(connectionInfo));
    return ret;
}

ADriver *APg::createRawDriver() const
{
    auto ret = new ADriverPg(d->connection);
    return ret;
}

std::shared_ptr<ADriver> APg::createDriver() const
{
    auto ret = std::make_shared<ADriverPg>(d->connection);
    return ret;
}

ADatabase APg::createDatabase() const
{
    return ADatabase(std::make_shared<ADriverPg>(d->connection));
}
