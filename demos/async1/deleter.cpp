/*
 * SPDX-FileCopyrightText: (C) 2020-2026 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

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

ACoroTerminator runDeleterDemo(std::shared_ptr<QObject> finished)
{
    auto _ = qScopeGuard([finished] {});

    {
        auto db = co_await APool::database();
        if (!db) {
            qDebug() << "Could not get a connection:" << db.error();
            co_return;
        }

        auto result = co_await db->exec(u"SELECT now()"_s);
        if (result) {
            qDebug() << "SELECT" << result->toJsonObject();
        }
    }

    {
        // Returning the previous connection triggers the reuse hook on checkout.
        auto db = co_await APool::database();
        if (db) {
            auto result = co_await db->exec(u"SELECT now()"_s);
            if (result) {
                qDebug() << "SELECT after reuse" << result->toJsonObject();
            }
        }
    }

    auto *cancelator = new QObject;
    QTimer::singleShot(100, cancelator, [cancelator] { cancelator->deleteLater(); });
    auto result = co_await APool::exec(u"SELECT pg_sleep(5), now()"_s, cancelator);
    if (result) {
        qDebug() << "cancelled query succeeded:" << result->toJsonObject();
    } else {
        qDebug() << "cancelled query:" << result.error();
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    APool::create(APg::factory(u"postgres:///?target_session_attrs=read-write"_s));
    APool::setSetupHook([](ADatabase db) -> ACoroTerminator {
        qDebug() << "setup db";
        auto result = co_await db.exec(u"SET TIME ZONE 'Europe/Rome'"_s);
        if (result) {
            qDebug() << "SETUP" << result->toJsonObject();
        }
    });

    APool::setReuseHook([](ADatabase db) -> ACoroTerminator {
        qDebug() << "reuse db";
        auto result = co_await db.exec(u"DISCARD ALL"_s);
        if (result) {
            qDebug() << "REUSE" << result->toJsonObject();
        }
    });

    QEventLoop loop;
    {
        auto finished = std::make_shared<QObject>();
        QObject::connect(finished.get(), &QObject::destroyed, &loop, &QEventLoop::quit);
        runDeleterDemo(finished);
    }
    loop.exec();

    return 0;
}
