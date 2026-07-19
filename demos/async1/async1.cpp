/*
 * SPDX-FileCopyrightText: (C) 2020-2026 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "../../src/acache.h"
#include "../../src/acoroexpected.h"
#include "../../src/adatabase.h"
#include "../../src/apg.h"
#include "../../src/apool.h"
#include "../../src/aresult.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QJsonObject>
#include <QScopeGuard>
#include <QTimer>

using namespace ASql;
using namespace Qt::StringLiterals;

ACoroTerminator runAsync1(std::shared_ptr<QObject> finished)
{
    auto _ = qScopeGuard([finished] {});

    auto db = co_await APool::database();
    if (!db) {
        qDebug() << "Could not get a connection:" << db.error();
        co_return;
    }

    auto result = co_await db->exec(u8"SELECT 'I ♥ Cutelyst!' AS utf8");
    if (result) {
        qDebug() << "utf8" << result->toHash();
    }

    result = co_await db->exec(u"SELECT generate_series(1, 5) AS number"_s);
    if (result) {
        for (const auto &row : *result) {
            qDebug() << "series" << row.value(0) << row.value(u"number"_s);
        }
    }

    result = co_await db->exec(u"select jsonb_build_object('foo', now());"_s);
    if (result && result->size()) {
        qDebug() << "json" << (*result)[0][0].toJsonValue();
    }

    auto *cache = new ACache;
    cache->setDatabase(*db);

    result = co_await cache->exec(u"SELECT now()"_s);
    if (result) {
        qDebug() << "CACHED 1" << result->toJsonObject();
    }

    result = co_await cache->exec(u"SELECT now()"_s);
    if (result) {
        qDebug() << "CACHED 2 (hit)" << result->toJsonObject();
    }

    cache->clear(u"SELECT now()"_s);
    result = co_await cache->exec(u"SELECT now()"_s);
    if (result) {
        qDebug() << "CACHED 3 (after clear)" << result->toJsonObject();
    }

    cache->deleteLater();
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    APool::create(APg::factory(u"postgres:///"_s));
    APool::setMaxIdleConnections(10);

    QEventLoop loop;
    {
        auto finished = std::make_shared<QObject>();
        QObject::connect(finished.get(), &QObject::destroyed, &loop, &QEventLoop::quit);
        runAsync1(finished);
    }
    loop.exec();

    return 0;
}
