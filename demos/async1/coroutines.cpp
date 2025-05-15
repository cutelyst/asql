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

    AResultFn resultFn;
    QElapsedTimer t;
    if (false) {
        auto coroBench = [&t, &resultFn]() -> ACoroTerminator {
            auto db      = APool::database();
            auto counter = std::make_shared<int>(0);

            const int maxQueries = 100'000;

            // warm up
            t.start();
            for (int i = 0; i < maxQueries; ++i) {
                auto result = co_await db.coExec(u"SELECT 1", nullptr);
                // qDebug() << "coroutine" << *counter << t.elapsed();
                if (*counter == maxQueries - 1) {
                    qDebug() << "coroutine warminig" << t.elapsed();
                }
                *counter = *counter + 1;
            }

            *counter = 0;
            t.restart();

            for (int i = 0; i < maxQueries; ++i) {
                auto result = co_await db.coExec(u"SELECT 1", nullptr);
                // qDebug() << "coroutine" << *counter << t.elapsed();
                if (*counter == maxQueries - 1) {
                    qDebug() << "coroutine warm" << t.elapsed();
                }
                *counter = *counter + 1;
            }

            *counter = 0;
            t.restart();

            // Biggest non coro issue seems to be queueing queries
            // this recursive mode that prevents queueing doesn't make much of a
            // performance difference but is still rather more complex to write.
            // After all coro machinery still uses AResultFn lambdas to be notified
            // of a result and resume execution.
            resultFn = [counter, db, &resultFn, &t](AResult &result) mutable {
                // qDebug() << "lambda" << *counter << t.elapsed();
                if (*counter == maxQueries - 1) {
                    qDebug() << "recursive  lambda" << t.elapsed();

                    *counter = 0;
                    t.restart();
                    for (int i = 0; i < maxQueries; ++i) {
                        db.exec(u"SELECT 1", nullptr, [counter, db, &t](AResult &result) mutable {
                            // qDebug() << "lambda" << *counter << t.elapsed();
                            if (*counter == maxQueries - 1) {
                                qDebug() << "queued lambda" << t.elapsed();
                            }
                            *counter = *counter + 1;
                        });
                    }

                    return;
                }
                *counter = *counter + 1;

                db.exec(u"SELECT 1", nullptr, resultFn);
            };

            db.exec(u"SELECT 1", nullptr, resultFn);
        }();
    }

    if (false) {
        auto callEx = []() -> ACoroTerminator {
            qDebug() << "coro started";

            auto db = APool::database();
            db.exec(u"SELECT 2", nullptr, [](AResult &result) {
                if (result.hasError()) {
                    qDebug() << "Error" << result.errorString();
                } else {
                    qDebug() << "1s loop" << result.toHash();
                }
            });

            auto obj = new QObject;
            QTimer::singleShot(500, obj, [obj] {
                qDebug() << "Delete Obj";
                delete obj;
            });

            {
                auto result = co_await db.coExec(u8"SELECT now(), pg_sleep(3)", obj);
                if (result.has_value()) {
                    qDebug() << "coro result has value" << result->toJsonObject();
                } else {
                    qDebug() << "coro result error" << result.error();
                }
                obj->setProperty("crash", true);
            }

            {
                auto result = co_await db.coExec(u8"SELECT now(), pg_sleep(1)", nullptr);
                if (result.has_value()) {
                    qDebug() << "coro result has value" << result->toJsonObject();
                } else {
                    qDebug() << "coro result error" << result.error();
                }
                obj->setProperty("crash", true);
            }
        };

        callEx();
    }

    if (false) {
        auto callTransaction = []() -> ACoroTerminator {
            auto _ = qScopeGuard([] { qDebug() << "coro exited"; });
            qDebug() << "coro started";

            auto db = co_await APool::coDatabase();

            auto transaction = co_await db->coBegin();
            qDebug() << "transaction started";

            auto result = co_await db->coExec(u8"SELECT now()");
            if (result.has_value()) {
                qDebug() << "coro result has value" << result->toJsonObject();
            } else {
                qDebug() << "coro result error" << result.error();
            }

            auto result2 = co_await db->coExec(u8"SELECT now()");
            if (result2.has_value()) {
                qDebug() << "coro result2 has value" << result2->toJsonObject();
            } else {
                qDebug() << "coro result2 error" << result2.error();
            }

            auto commit = co_await transaction->coCommit();
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
            auto result = co_await db.coExec(u"SELECT now()"_s, nullptr);
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

                auto result = co_await db->coExec(u"SELECT now() at time zone 'UTC'"_s, nullptr);
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

    if (false) {
        auto callPool = []() -> ACoroTerminator {
            auto _ = qScopeGuard([] { qDebug() << "coro pool exited"; });
            qDebug() << "coro pool started";

            auto db = co_await APool::coDatabase();
            if (db.has_value()) {
                qDebug() << "coro pool has value" << db->isOpen();
            } else {
                qDebug() << "coro pool error" << db.error();
            }

            auto result = co_await db->coExec(u"SELECT now(), pg_sleep(1)"_s, nullptr);
            if (result.has_value()) {
                qDebug() << "coro result has value" << result->toJsonObject();
            } else {
                qDebug() << "coro result error" << result.error();
            }
        };

        callPool();
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

            auto result = co_await db->coExec(u8"SELECT now(), pg_sleep(2)", obj);
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

            auto result = co_await db->coExec(u8"SELECT now()", obj);
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
            auto _ = qScopeGuard([] { qDebug() << "coro pool exited"; });
            qDebug() << "coro exec pool started";

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
        auto callPoolBegin = []() -> ACoroTerminator {
            auto _ = qScopeGuard([] { qDebug() << "coro pool exited"; });
            qDebug() << "coro exec pool started";

            auto t = co_await APool::begin(nullptr);
            if (t.has_value()) {
                qDebug() << "coro exec t has value" << t->database().isOpen();
            } else {
                qDebug() << "coro exec t error" << t.error();
            }

            auto result = co_await t->database().coExec(u8"SELECT now()", nullptr);
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

    if (true) {
        auto testPoolSync = []() {
            {
                auto db1 = APool::database(u"pool");
                qDebug() << db1.isValid();
                qDebug() << APool::currentConnections(u"pool");

                auto db2 = APool::database(u"pool");
                qDebug() << db2.isValid();
                qDebug() << APool::currentConnections(u"pool");

                auto db3 = APool::database(u"pool");
                qDebug() << db3.isValid();
                qDebug() << APool::currentConnections(u"pool");
            }

            auto db4 = APool::database(u"pool");
            qDebug() << db4.isValid();
            qDebug() << APool::currentConnections(u"pool");
        };
        testPoolSync();
    }

    app.exec();
}
