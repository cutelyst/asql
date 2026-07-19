/*
 * SPDX-FileCopyrightText: (C) 2022-2026 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "../../src/acoroexpected.h"
#include "../../src/adatabase.h"
#include "../../src/apg.h"
#include "../../src/apool.h"
#include "../../src/apreparedquery.h"
#include "../../src/aresult.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QLoggingCategory>
#include <QScopeGuard>

using namespace ASql;
using namespace Qt::StringLiterals;
using namespace std::chrono_literals;

ACoroTerminator runPipelines(std::shared_ptr<QObject> finished)
{
    auto _ = qScopeGuard([finished] {});

    {
        auto db = co_await APool::database();
        if (!db) {
            qDebug() << "Could not get a connection:" << db.error();
            co_return;
        }

        // Pipeline mode requires an open connection and an empty query queue.
        qDebug() << "PIPELINE ENTER" << db->enterPipelineMode();
        qDebug() << "PIPELINE STATUS" << int(db->pipelineStatus());

        for (int i = 0; i < 10; ++i) {
            auto result = co_await db->exec(u"SELECT now(), $1"_s, {i});
            if (!result) {
                qDebug() << "PIPELINE SELECT error" << i << result.error();
                co_return;
            }

            if (result->size()) {
                qDebug() << "PIPELINE SELECT value" << i << (*result)[0][1].toInt()
                         << (*result)[0].value(0);
            }
        }

        // Call periodically (or after a batch) unless enterPipelineMode set autoSync.
        qDebug() << "PIPELINE SYNC" << db->pipelineSync();
    }

    {
        auto db = co_await APool::database();
        if (!db) {
            qDebug() << "Could not get a connection:" << db.error();
            co_return;
        }

        qDebug() << "2 PIPELINE ENTER" << db->enterPipelineMode(2s);
        qDebug() << "2 PIPELINE STATUS" << int(db->pipelineStatus());

        for (int i = 0; i < 3; ++i) {
            auto result = co_await db->exec(APreparedQuery(u"SELECT now(), $1"_s), {i});
            if (!result) {
                qDebug() << "2 PIPELINE SELECT error" << i << result.error();
                co_return;
            }

            if (result->size()) {
                qDebug() << "2 PIPELINE SELECT value" << i << (*result)[0][1].toInt()
                         << (*result)[0].value(0);
            }

            auto staticResult = co_await db->exec(APreparedQueryLiteral(u"SELECT now(), $1"), {-i});
            if (!staticResult) {
                qDebug() << "2 PIPELINE SELECT error" << -i << staticResult.error();
                co_return;
            }

            if (staticResult->size()) {
                qDebug() << "2 PIPELINE SELECT value" << -i << (*staticResult)[0][1].toInt()
                         << (*staticResult)[0].value(0);
            }
        }
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    APool::create(APg::factory(u"postgres:///?target_session_attrs=read-write"_s));
    APool::setMaxIdleConnections(10);

    QEventLoop loop;
    {
        auto finished = std::make_shared<QObject>();
        QObject::connect(finished.get(), &QObject::destroyed, &loop, &QEventLoop::quit);
        runPipelines(finished);
    }
    loop.exec();

    return 0;
}
