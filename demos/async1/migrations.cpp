/*
 * SPDX-FileCopyrightText: (C) 2020-2026 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "../../src/acoroexpected.h"
#include "../../src/adatabase.h"
#include "../../src/amigrations.h"
#include "../../src/apg.h"
#include "../../src/apool.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QScopeGuard>

using namespace ASql;
using namespace Qt::StringLiterals;

ACoroTerminator runMigrationsDemo(std::shared_ptr<QObject> finished)
{
    auto _ = qScopeGuard([finished] {});

    auto db = co_await APool::database();
    if (!db) {
        qDebug() << "Could not get a connection:" << db.error();
        co_return;
    }

    AMigrations mig;
    mig.fromString(uR"V0G0N(
-- 1 up
create table messages (message text);
insert into messages values ('I ♥ Cutelyst!');
-- 1 down
drop table messages;
-- 2 up
create table log (message text);
insert into log values ('logged');
-- 2 down
drop table log;
)V0G0N"_s);

    qDebug() << "MIG latest" << mig.latest() << "active" << mig.active();

    const auto loaded = co_await mig.load(*db, u"demo"_s);
    if (!loaded) {
        qDebug() << "LOAD error:" << loaded.error();
        co_return;
    }

    const auto migrated = co_await mig.migrate(2);
    if (!migrated) {
        qDebug() << "MIGRATE error:" << migrated.error();
        co_return;
    }

    qDebug() << "Migrated to version" << mig.active();
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    APool::create(APg::factory(u"postgres:///"_s));

    QEventLoop loop;
    {
        auto finished = std::make_shared<QObject>();
        QObject::connect(finished.get(), &QObject::destroyed, &loop, &QEventLoop::quit);
        runMigrationsDemo(finished);
    }
    loop.exec();

    return 0;
}
