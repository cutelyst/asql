/*
 * SPDX-FileCopyrightText: (C) 2020 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "../../src/acache.h"
#include "../../src/acoroexpected.h"
#include "../../src/adatabase.h"
#include "../../src/amigrations.h"
#include "../../src/apg.h"
#include "../../src/apool.h"
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

    APool::create(APg::factory(u"postgres:///?target_session_attrs=read-write"_s));
    APool::setMaxIdleConnections(10);

    APool::create(APg::factory(u"postgres:///?target_session_attrs=read-write"_s), u"pool"_s);
    APool::setMaxIdleConnections(2, u"pool");
    APool::setMaxConnections(3, u"pool");

    if (false) {
        auto callTransaction = []() -> ACoroTerminator {
            auto _ = qScopeGuard([] { qDebug() << "coro exited"; });
            qDebug() << "coro started";

            auto db = co_await APool::coDatabase();

            auto transaction = co_await db->beginTransaction();
            qDebug() << "transaction started";

            auto result = co_await db->exec(u8"SELECT now()");
            if (result.has_value()) {
                qDebug() << "coro result has value" << result->toJsonObject();
            } else {
                qDebug() << "coro result error" << result.error();
            }

            auto result2 = co_await db->exec(u8"SELECT now()");
            if (result2.has_value()) {
                qDebug() << "coro result2 has value" << result2->toJsonObject();
            } else {
                qDebug() << "coro result2 error" << result2.error();
            }

            auto commit = co_await transaction->commit();
            if (commit.has_value()) {
                qDebug() << "coro commit has value" << commit->toJsonObject();
            } else {
                qDebug() << "coro commit error" << commit.error();
            }
        };

        callTransaction();
    }

    if (false) {
        auto cache = new ACache;
        cache->setDatabase(APool::database());

        auto callCache = [cache]() -> ACoroTerminator {
            auto _ = qScopeGuard([] { qDebug() << "coro cache exited"; });
            qDebug() << "coro cache started";

            auto result = co_await cache->coExec(u"SELECT now(), pg_sleep(1)"_s, nullptr);
            if (result.has_value()) {
                qDebug() << "coro result has value" << result->toJsonObject();
            } else {
                qDebug() << "coro result error" << result.error();
            }

            auto resultCached = co_await cache->coExec(u"SELECT now(), pg_sleep(1)"_s, nullptr);
            if (resultCached.has_value()) {
                qDebug() << "coro resultCached has value" << resultCached->toJsonObject();
            } else {
                qDebug() << "coro resultCached error" << resultCached.error();
            }
        };

        callCache();
    }

    if (false) {
        auto callInvalid = []() -> ACoroTerminator {
            auto _ = qScopeGuard([] { qDebug() << "coro db invalid exited"; });
            qDebug() << "coro db invalid started";

            ADatabase db;
            auto result = co_await db.exec(u"SELECT now()"_s, nullptr);
            if (result.has_value()) {
                qDebug() << "coro result has value" << result->toJsonObject();
            } else {
                qDebug() << "coro result error" << result.error();
            }
        };

        callInvalid();
    }

    if (false) {
        auto callOuter = []() -> ACoroTerminator {
            auto _ = qScopeGuard([] { qDebug() << "coro outer exited"; });
            qDebug() << "coro outer started";

            auto callInner = []() -> ACoroTerminator {
                auto _ = qScopeGuard([] { qDebug() << "coro inner exited"; });
                qDebug() << "coro inner started";

                auto db = co_await APool::coDatabase();
                if (!db) {
                    qDebug() << "coro db error" << db.error();
                    co_return;
                }

                auto result = co_await db->exec(u"SELECT now() at time zone 'UTC'"_s, nullptr);
                if (result.has_value()) {
                    qDebug() << "coro result has value" << result->toJsonArrayObject();
                    for (const auto &row : *result) {
                        qDebug() << "coro result row" << row.toJsonObject();
                    }
                } else {
                    qDebug() << "coro result error" << result.error();
                }
            };

            // Cannot be awaited :)
            callInner();
            co_return;
        };

        callOuter();
    }

    if (true) {
        auto callPool = [=]() -> ATask<QJsonObject> {
            auto _ = qScopeGuard([] { qDebug() << "coro pool exited"; });
            qDebug() << "coro pool started";

            auto db = co_await APool::coDatabase();
            if (db.has_value()) {
                qDebug() << "coro pool has value" << db->isOpen();
            } else {
                qDebug() << "coro pool error" << db.error();
            }

            auto obj = new QObject;
            // QTimer::singleShot(2000, obj, [obj] {
            //     qDebug() << "Delete Obj later";
            //     delete obj;
            // });
            // co_yield obj; // so that this promise is destroyed if this object is destroyed

            auto result = co_await db->exec(u"SELECT now(), pg_sleep(1)"_s, obj);
            if (result.has_value()) {
                qDebug() << "coro result has value" << result->toJsonObject();
                co_return result->toJsonObject();
            } else {
                qDebug() << "coro result error" << result.error();
            }

            co_return {};
        };

        auto callPoolTask = [=]() -> ACoroTerminator {
            auto _ = qScopeGuard([] { qDebug() << "coro callPoolTask exited"; });
            qDebug() << "coro callPoolTask started";
            auto task = callPool();
            ;
            qDebug() << "coro callPoolTask awaitable";

            auto obj = new QObject;
            QTimer::singleShot(200, obj, [obj] {
                qDebug() << "Delete callPoolTask obj later";
                delete obj;
            });
            co_yield obj; // so that this promise is destroyed if this object is destroyed

            auto result = co_await task;
            qDebug() << "coro callPoolTask result" << result;
        };

        callPoolTask();
    }

    if (false) {
        auto callTerminatorEarly = []() -> ACoroTerminator {
            auto _ = qScopeGuard([] { qDebug() << "coro exited"; });
            qDebug() << "coro started";

            auto obj = new QObject;
            QTimer::singleShot(500, obj, [obj] {
                qDebug() << "Delete Obj";
                delete obj;
            });
            co_yield obj; // so that this promise is destroyed if this object is destroyed

            auto db = co_await APool::coDatabase();

            auto result = co_await db->exec(u8"SELECT now(), pg_sleep(2)", obj);
            if (result.has_value()) {
                qDebug() << "coro result has value" << result->toJsonObject();
            } else {
                qDebug() << "coro result error" << result.error();
            }
        };
        callTerminatorEarly();

        auto callTerminatorLater = []() -> ACoroTerminator {
            auto _ = qScopeGuard([] { qDebug() << "coro exited"; });
            qDebug() << "coro started";

            auto obj = new QObject;
            QTimer::singleShot(2000, obj, [obj] {
                qDebug() << "Delete Obj later";
                delete obj;
            });
            co_yield obj; // so that this promise is destroyed if this object is destroyed

            auto db = co_await APool::coDatabase();

            auto result = co_await db->exec(u8"SELECT now()", obj);
            if (result.has_value()) {
                qDebug() << "coro result has value" << result->toJsonObject();
            } else {
                qDebug() << "coro result error" << result.error();
            }
        };

        callTerminatorLater();
    }

    if (false) {
        auto callPool = []() -> ACoroTerminator {
            auto _ = qScopeGuard([] { qDebug() << "callPool exited"; });
            qDebug() << "callPool started";

            auto result = co_await APool::exec(u8"SELECT now()", nullptr, u"invalid");
            if (result.has_value()) {
                qDebug() << "coro exec result has value" << result->toJsonObject();
            } else {
                qDebug() << "coro exec result error" << result.error();
            }

            result = co_await APool::exec(u8"SELECT now()", nullptr);
            if (result.has_value()) {
                qDebug() << "coro exec result has value" << result->toJsonObject();
            } else {
                qDebug() << "coro exec result error" << result.error();
            }

            auto obj = new QObject;
            QTimer::singleShot(500, obj, [obj] {
                qDebug() << "Delete Obj later";
                delete obj;
            });
            co_yield obj; // so that this promise is destroyed if this object is destroyed

            result = co_await APool::exec(u8"SELECT now(), pg_sleep(1)", obj);
            if (result.has_value()) {
                qDebug() << "coro exec result has value" << result->toJsonObject();
            } else {
                qDebug() << "coro exec result error" << result.error();
            }
        };

        callPool();
    }

    if (false) {
        auto destroyedLambda = []() -> ACoroTerminator {
            auto _ = qScopeGuard([] { qDebug() << "destroyedLambda exited"; });
            qDebug() << "destroyedLambda started";

            auto obj = new QObject;
            QTimer::singleShot(500, obj, [obj] {
                qDebug() << "Delete Obj later";
                delete obj;
            });
            // Do NOT yield obj here, as obj is a cancelator,
            // and it should prevent this coroutine to resume

            auto result = co_await APool::exec(u8"SELECT now(), pg_sleep(1)", obj);
            if (result.has_value()) {
                qDebug() << "destroyedLambda exec result has value" << result->toJsonObject();
            } else {
                qDebug() << "destroyedLambda exec result error" << result.error();
            }
        };

        destroyedLambda();
    }

    if (false) {
        auto callPoolBegin = []() -> ACoroTerminator {
            auto _ = qScopeGuard([] { qDebug() << "coro pool exited"; });
            qDebug() << "coro exec pool started";

            auto t = co_await APool::begin(nullptr);
            if (t.has_value()) {
                qDebug() << "coro exec t has value" << t->database().isOpen();
            } else {
                qDebug() << "coro exec t error" << t.error();
            }

            auto result = co_await t->database().exec(u8"SELECT now()", nullptr);
            if (result.has_value()) {
                qDebug() << "coro exec result has value" << result->toJsonObject();
            } else {
                qDebug() << "coro exec result error" << result.error();
            }

            auto obj = new QObject;
            QTimer::singleShot(500, obj, [obj] {
                qDebug() << "Delete Obj later";
                delete obj;
            });
            co_yield obj; // so that this promise is destroyed if this object is destroyed

            std::ignore = APool::exec(u8"SELECT now(), pg_sleep(1)" /*, obj*/);
        };

        callPoolBegin();
    }

    if (false) {
        auto callJsonBegin = []() -> ACoroTerminator {
            auto _ = qScopeGuard([] { qDebug() << "coro pool exited"; });
            qDebug() << "coro exec pool started";

            auto result = co_await APool::exec(u8"SELECT jsonb_build_object()", nullptr);
            if (result.has_value()) {
                auto row = result->begin();
                qDebug() << "coro exec result has value" << result->toJsonObject();
                qDebug() << "coro exec result has value" << row.value(0);
            } else {
                qDebug() << "coro exec result error" << result.error();
            }

            result = co_await APool::exec(u8"SELECT jsonb_build_array()", nullptr);
            if (result.has_value()) {
                auto row = result->begin();
                qDebug() << "coro exec result has value" << result->toJsonObject();
                qDebug() << "coro exec result has value" << row.value(0);
            } else {
                qDebug() << "coro exec result error" << result.error();
            }

            result = co_await APool::exec(u8"SELECT json_build_object()", nullptr);
            if (result.has_value()) {
                auto row = result->begin();
                qDebug() << "coro exec result has value" << result->toJsonObject();
                qDebug() << "coro exec result has value" << row.value(0);
            } else {
                qDebug() << "coro exec result error" << result.error();
            }

            result = co_await APool::exec(u8"SELECT json_build_array()", nullptr);
            if (result.has_value()) {
                auto row = result->begin();
                qDebug() << "coro exec result has value" << result->toJsonObject();
                qDebug() << "coro exec result has value" << row.value(0);
            } else {
                qDebug() << "coro exec result error" << result.error();
            }

            result = co_await APool::exec(u8"SELECT 'b12e975f-9717-4edb-b37e-d1e3827e6b3b'::uuid",
                                          nullptr);
            if (result.has_value()) {
                auto row = result->begin();
                qDebug() << "coro exec result has value" << result->toJsonObject();
                qDebug() << "coro exec result has value" << row.value(0);
            } else {
                qDebug() << "coro exec result error" << result.error();
            }
        };

        callJsonBegin();
    }

    if (false) {
        auto testPoolSync = []() -> ACoroTerminator {
            {
                auto db1 = co_await APool::coDatabase(nullptr, u"pool");
                qDebug() << db1->isValid();
                qDebug() << APool::currentConnections(u"pool");

                auto db2 = co_await APool::coDatabase(nullptr, u"pool");
                qDebug() << db2->isValid();
                qDebug() << APool::currentConnections(u"pool");

                auto db3 = co_await APool::coDatabase(nullptr, u"pool");
                qDebug() << db3->isValid();
                qDebug() << APool::currentConnections(u"pool");
            }

            auto awaiter = APool::exec(u8"SELECT 1, now(), pg_sleep(1);SELECT 2, now(), "
                                       u8"pg_sleep(1);SELECT 3, now(), pg_sleep(1)",
                                       nullptr);
            if (awaiter.await_ready()) {
                qDebug() << "Awaiter ready";
            }

            auto result = co_await awaiter;
            if (result.has_value()) {
                qDebug() << "awaiter result has value" << result->lastResultSet()
                         << result->toJsonObject();
            } else {
                qDebug() << "awaiter result error" << result.error();
            }

            result = co_await awaiter;
            if (result.has_value()) {
                qDebug() << "awaiter result has value" << result->lastResultSet()
                         << result->toJsonObject();
            } else {
                qDebug() << "awaiter result error" << result.error();
            }

            result = co_await awaiter;
            if (result.has_value()) {
                qDebug() << "awaiter result has value" << result->lastResultSet()
                         << result->toJsonObject();
            } else {
                qDebug() << "awaiter result error" << result.error();
            }

            qDebug() << "calling finished awaiter ";
            result = co_await awaiter;
            if (result.has_value()) {
                qDebug() << "awaiter result has value" << result->toJsonObject();
            } else {
                qDebug() << "awaiter result error" << result.error();
            }
            qDebug() << "awaiter finished";

            // auto db4 = APool::database(u"pool");
            // qDebug() << db4.isValid();
            // qDebug() << APool::currentConnections(u"pool");
        };
        testPoolSync();
    }

    app.exec();
}
