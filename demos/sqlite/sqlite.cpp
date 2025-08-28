/*
 * SPDX-FileCopyrightText: (C) 2025 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "../../src/ASqlite.hpp"
#include "../../src/acache.h"
#include "../../src/acoroexpected.h"
#include "../../src/adatabase.h"
#include "../../src/amigrations.h"
#include "../../src/apool.h"
#include "../../src/apreparedquery.h"
#include "../../src/aresult.h"
#include "../../src/atransaction.h"

#include <thread>

#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QScopeGuard>
#include <QTimer>

using namespace ASql;
using namespace Qt::StringLiterals;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    APool::create(ASqlite::factory(u"sqlite://?MEMORY"_s));
    APool::setMaxIdleConnections(10);

    if (false) {
        auto callTerminatorEarly = []() -> ACoroTerminator {
            auto _ = qScopeGuard([] { qDebug() << "coro exited"; });
            qDebug() << "coro started";

            auto result = co_await APool::exec(u"SELECT error()"_s);
            if (result) {
                qDebug() << "coro result has value" << result->toJsonObject();
            } else {
                qDebug() << "coro result error" << result.error();
            }

            result = co_await APool::exec(u"SELECT 123 num"_s);
            if (result) {
                qDebug() << "coro result has value" << result->toJsonObject();
            } else {
                qDebug() << "coro result error" << result.error();
            }

            result = co_await APool::exec(u"SELECT 'a', 'b', 321"_s);
            if (result) {
                qDebug() << "coro result has value" << result->columnNames()
                         << result->toJsonObject();
            } else {
                qDebug() << "coro result error" << result.error();
            }

            result = co_await APool::exec(u"SELECT ? d, ? a, ? c"_s,
                                          {
                                              1,
                                              true,
                                              u"foo"_s,
                                          });
            if (result) {
                qDebug() << "coro result has value" << result->columnNames()
                         << result->toJsonObject();
            } else {
                qDebug() << "coro result error" << result.error();
            }

            result =
                co_await APool::exec(u"SELECT *, random() FROM (VALUES (1), (2), (3), (4), (5))"_s);
            if (result) {
                qDebug() << "coro result has value" << result->columnNames()
                         << result->toJsonArrayObject();
            } else {
                qDebug() << "coro result error" << result.error();
            }

            result = co_await APool::exec(u"SELECT * FROM foo"_s);
            if (result) {
                qDebug() << "coro result has value" << result->columnNames()
                         << result->toJsonArrayObject();
            } else {
                qDebug() << "coro result error" << result.error();
            }

            result = co_await APool::exec(u"SELECT date()"_s);
            if (result) {
                qDebug() << "coro result has value" << result->columnNames()
                         << result->toJsonObject();
            } else {
                qDebug() << "coro result error" << result.error();
            }

            qApp->quit();
        };
        callTerminatorEarly();
    }

    if (false) {
        auto callTerminatorEarly = []() -> ACoroTerminator {
            auto _ = qScopeGuard([] { qDebug() << "coro exited"; });
            qDebug() << "coro started prepared queries";

            // TODO coDatabase returns a db handle that is not open yet.
            // because database() method calls open() and do not wait it.
            auto db = co_await APool::coDatabase();
            if (db) {
                qDebug() << "coro db isOpen" << db->isOpen();
            } else {
                qDebug() << "coro db error" << db.error();
                co_return;
            }

            for (int i = 0; i < 3; ++i) {
                auto result = co_await db->exec(APreparedQueryLiteral(u"SELECT random()"_s));
                if (result) {
                    qDebug() << i << "coro result has value" << result->toJsonObject();
                } else {
                    qDebug() << i << "coro result error" << result.error();
                }
            }

            qApp->quit();
        };
        callTerminatorEarly();
    }

    if (true) {
        auto multipleQueries = []() -> ACoroTerminator {
            auto _ = qScopeGuard([] { qDebug() << "coro exited"; });
            qDebug() << "coro started prepared queries";

            bool last      = true;
            auto awaitable = APool::exec(u"SELECT date(); SELECT 123; SELECT 456"_s);
            do {
                auto result = co_await awaitable;
                if (result) {
                    qDebug() << "coro result has value" << result->columnNames()
                             << result->toJsonObject();
                    last = result->lastResultSet();
                } else {
                    qDebug() << "coro result error" << result.error();
                    break;
                }
            } while (!last);

            qApp->quit();
        };

        // auto db = APool::database();
        // db.exec(u"SELECT date(); SELECT 123; SELECT 456"_s, nullptr, [](AResult &result) {
        //     qDebug() << "1result" << result.query();
        //     qDebug() << "2result" << result.lastResultSet();
        //     qDebug() << "3result" << result.toJsonObject();
        //     if (result.lastResultSet()) {
        //         qApp->quit();
        //     }
        // });
        multipleQueries();
    }

    app.exec();
}
