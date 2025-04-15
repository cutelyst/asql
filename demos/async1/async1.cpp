/*
 * SPDX-FileCopyrightText: (C) 2020 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "../../src/acache.h"
#include "../../src/adatabase.h"
#include "../../src/amigrations.h"
#include "../../src/apg.h"
#include "../../src/apool.h"
#include "../../src/apreparedquery.h"
#include "../../src/aresult.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QTimer>
#include <QUrl>
#include <QUuid>

using namespace ASql;
using namespace Qt::StringLiterals;

void recursiveLoop()
{
    auto db = APool::database(u"memory_loop");
    db.exec(u"SELECT now()", {QJsonObject{{u"foo"_s, true}}}, nullptr, [](AResult &result) {
        if (result.hasError()) {
            qDebug() << "Error memory_loop" << result.errorString();
        } else {
            recursiveLoop();
        }
    });
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    {
        APool::create(APg::factory(u"postgres:///")), u"move_db_pool"_s;
        APool::setMaxConnections(1, u"move_db_pool");
        APool::database(nullptr, [](ADatabase db) {
            db.exec(u"SELECT 'I ♥ Cutelyst!' AS utf8", nullptr, [](AResult &result) {
                qDebug() << "=====iterator single row" << result.toHash();
                if (result.hasError()) {
                    qDebug() << "Error" << result.errorString();
                }
            });
        }, u"move_db_pool");
        APool::database(nullptr, [](ADatabase db) {
            db.exec(u"SELECT 'I ♥ Cutelyst!' AS utf8", nullptr, [](AResult &result) {
                qDebug() << "=====iterator single row" << result.toHash();
                if (result.hasError()) {
                    qDebug() << "Error" << result.errorString();
                }
            });
        }, u"move_db_pool");
    }

    {
        // regresion test crash - where selfDriver gets released
        APool::create(APg::factory(u"postgres:///")), u"delete_db_after_use"_s;
        APool::setMaxIdleConnections(0, u"delete_db_after_use");

        {
            APool::database(u"delete_db_after_use")
                .exec(u"SELECT 'I ♥ Cutelyst!' AS utf8", nullptr, [](AResult &result) {
                qDebug() << "=====iterator single row" << result.toHash();
                if (result.hasError()) {
                    qDebug() << "Error" << result.errorString();
                }
            });
        }
    }

    {
        // memory loop
        APool::create(APg::factory(u"postgres:///")), u"memory_loop"_s;
        APool::setMaxIdleConnections(5, u"memory_loop");
        //        APool::setMaxConnections(0, u"memory_loop");

        {
            for (int i = 0; i < 20; ++i) {
                recursiveLoop();
            }
        }
    }

    APool::create(APg::factory(u"postgres:///"_s));
    APool::setMaxIdleConnections(10);

    {
        auto db = APool::database();
        // Zero-copy and zero allocation
        db.exec(u8"SELECT 'I ♥ Cutelyst!' AS utf8", nullptr, [](AResult &result) {
            qDebug() << "=====iterator single row" << result.toHash();
            if (result.hasError()) {
                qDebug() << "Error" << result.errorString();
            }
        });

        // Zero-copy but allocates due toUtf8()
        db.exec(u"SELECT 'I ♥ Cutelyst!' AS utf8", nullptr, [](AResult &result) {
            qDebug() << "=====iterator single row" << result.toHash();
            if (result.hasError()) {
                qDebug() << "Error" << result.errorString();
            }
        });

        db.exec(u"SELECT 'I ♥ Cutelyst!' AS utf8, $1",
                {"I ♥ Cutelyst!"_ba},
                nullptr,
                [](AResult &result) {
            qDebug() << "=====iterator qba row" << result.toHash();
            if (result.hasError()) {
                qDebug() << "Error" << result.errorString();
            }
        });
    }

    QVariantList series;
    {
        auto db = APool::database();
        qDebug() << "123";
        db.exec(u"SELECT generate_series(1, 10) AS number",
                nullptr,
                [&series](AResult &result) mutable {
            qDebug() << "=====iterator single row" << result.errorString() << result.size()
                     << "last" << result.lastResultSet() << "mutable" << series.size();
            if (result.hasError()) {
                qDebug() << "Error" << result.errorString();
            }

            // For range
            for (const auto &row : result) {
                qDebug() << "for loop row numbered" << row.value(0) << row.value(u"number"_s);
                series.append(row[0].value());
            }

            // Iterators
            auto it = result.begin();
            while (it != result.end()) {
                qDebug() << "iterator" << it.at() << it.value(0) << it[u"number"_s].value()
                         << it[0].toInt();
                ++it;
            }
        });
        db.setLastQuerySingleRowMode();

        db.exec(u"SELECT generate_series(1, 10) AS number"_s,
                nullptr,
                [&series](AResult &result) mutable {
            qDebug() << "=====iterator" << result.errorString() << result.size() << "last"
                     << result.lastResultSet() << "mutable" << series.size();
            if (result.hasError()) {
                qDebug() << "Error" << result.errorString();
            }

            // For range
            for (const auto &row : result) {
                qDebug() << "for loop row numbered" << row.value(0) << row[u"number"_s].value()
                         << row[0].toInt();
                series.append(row[0].value());
            }

            // Iterators
            auto it = result.begin();
            while (it != result.end()) {
                qDebug() << "iterator" << it.at() << it[0].value() << it.value(u"number"_s)
                         << it[0].toInt();
                ++it;
            }
        });
    }

    APool::database().exec(u"SELECT $1, $2, $3, $4, now()"_s,
                           {
                               QJsonValue(true),
                               QJsonValue(123.4567),
                               QJsonValue(u"fooo"_s),
                               QJsonValue(QJsonObject{}),
                           },
                           nullptr,
                           [&series](AResult &result) mutable {
        qDebug() << "=====iterator JSON" << result.errorString() << result.size() << "last"
                 << result.lastResultSet() << "mutable" << series.size();
        if (result.hasError()) {
            qDebug() << "Error" << result.errorString();
        }

        // For range
        qDebug() << "JSON result" << result[0].toList();
    });

    APool::database().exec(
        u"select jsonb_build_object('foo', now());"_s, nullptr, [](AResult &result) mutable {
        qDebug() << "=====iterator JSON" << result.errorString() << result.size() << "last"
                 << result[0][0].toJsonValue();
        if (result.hasError()) {
            qDebug() << "Error" << result.errorString();
        }

        // For range
        qDebug() << "JSON result" << result[0].toList();
    });

    auto cache = new ACache;
    cache->setDatabase(APool::database());
    cache->exec(u"SELECT now()"_s, nullptr, [=](AResult &result) {
        qDebug() << "CACHED 1" << result.errorString() << result.size();
        if (result.hasError()) {
            qDebug() << "Error" << result.errorString();
        }

        for (const auto &row : result) {
            for (int i = 0; i < result.fields(); ++i) {
                qDebug() << "cached 1" << i << row.value(i);
            }
        }

        qDebug() << "LOOP 1" << result.errorString() << result.size();
        auto it = result.constBegin();
        while (it != result.constEnd()) {
            qDebug() << "cached 1" << it[0].value() << it[0].toDateTime();
            ++it;

            //            for (int i = 0; i < result.fields(); ++i) {
            //                qDebug() << "cached 1" << result.at() << i << result.value(i);
            //            }
        }
    });

    QTimer::singleShot(2000, cache, [=] {
        cache->exec(u"SELECT now()"_s, nullptr, [=](AResult &result) {
            qDebug() << "CACHED 2" << result.errorString() << result.size();
            if (result.hasError()) {
                qDebug() << "Error" << result.errorString();
            }

            for (const auto &row : result) {
                for (int i = 0; i < result.fields(); ++i) {
                    qDebug() << "cached 2" << i << row.value(i);
                }
            }
        });

        bool ret = cache->clear(u"SELECT now()"_s);
        qDebug() << "CACHED - CLEARED" << ret;

        cache->exec(u"SELECT now()"_s, nullptr, [=](AResult &result) {
            qDebug() << "CACHED 3" << result.errorString() << result.size();
            if (result.hasError()) {
                qDebug() << "Error 3" << result.errorString();
            }

            for (const auto &row : result) {
                for (int i = 0; i < result.fields(); ++i) {
                    qDebug() << "cached 3" << row.value(i) << row[i].value();
                }
            }
        });
    });

    //    auto obj = new QObject;

    //    APool::database().exec(u"select 100, pg_sleep(1)"_s, [=] (AResult &result) {
    //        qDebug() << "data" << result.size() << result.hasError() << result.errorString() <<
    //        "LAST" << result.lastResulSet(); delete obj; while (result.next()) {
    //            for (int i = 0; i < result.fields(); ++i) {
    //                qDebug() << "data" << result.at() << i << result.value(i);
    //            }
    //        }
    //    }, obj);

    //    APool::database().exec(u"select 200, pg_sleep(5)"_s, [=] (AResult &result) {
    //        qDebug() << "data" << result.size() << result.hasError() << result.errorString() <<
    //        "LAST" << result.lastResulSet(); delete obj; while (result.next()) {
    //            for (int i = 0; i < result.fields(); ++i) {
    //                qDebug() << "data" << result.at() << i << result.value(i);
    //            }
    //        }
    //    }, obj);
    //    ADatabase db(APool::database());
    //    db.open([=] (bool isOpen, const QString &error) {
    //        qDebug() << "OPEN" << isOpen << error;

    ////        ADatabase(db).exec(u"select 1; select 2"_s, [=] (AResult &result) {
    ////            qDebug() << "data" << result.size() << result.hasError() << result.errorString()
    ///<< "LAST" << result.lastResulSet(); /            while (result.next()) { /                for
    ///(int i = 0; i < result.fields(); ++i) { /                    qDebug() << "data" <<
    /// result.at() << i << result.value(i); /                } /            } /        }); /
    /// QJsonObject obj{ /            {u"foo"_s, 234} /        };
    //        ADatabase db1 = APool::database();
    //        db1.begin();
    //        QUuid uuid = QUuid::createUuid();
    //        qDebug() << "uuid" << uuid.toString();
    //        db1.exec(u"insert into temp4 values ($1, $2, $3, $4, $5, $6)"_s,
    //        {true, u"bla bla"_s, QVariant::Int, QDateTime::currentDateTime(),
    //        123456.78, uuid},
    //                [=] (AResult &result) {
    //            qDebug() << "data" << result.size() << result.hasError() << result.errorString();
    //            while (result.next()) {
    //                for (int i = 0; i < result.fields(); ++i) {
    //                    qDebug() << "data" << result.at() << i << result.value(i);
    //                }
    //            }
    //        });
    //        db1.commit();

    ////        db1.subscribeToNotification(u"minha_notifyç_ã3"_s, [=] (const QString
    ///&payload, bool self){ /            qDebug() << "notificação" << payload << self; /        });
    //    });

    //    APool::database().exec(u"select * from aaa.users limit 3"_s, [=] (AResult
    //    &result) {
    //        qDebug() << "data" << result.size() << result.hasError() << result.errorString() <<
    //        "LAST" << result.lastResulSet(); qDebug() << "data" << result.hash(); qDebug() <<
    //        "data" << result.hashes();
    ////        while (result.next()) {
    ////            for (int i = 0; i < result.fields(); ++i) {
    ////                qDebug() << "data" << result.at() << i << result.value(i);
    ////            }
    ////        }
    //    });

    //    auto mig = new AMigrations();
    //    mig->connect(mig, &AMigrations::ready, [=] (bool error, const QString &erroString) {
    //        qDebug() << "ready" << error << erroString;
    //        mig->migrate([=] (bool error, const QString &errorString) {
    //            qDebug() << "MIGRATED" << error << errorString;
    //        });
    //    });
    //    mig->load(APool::database(), u"foo"_s);

    //    mig->fromString(uR"V0G0N(
    //                                  -- 1 up
    //                                  create table messages (message text);
    //                                  insert into messages values ('I ♥ Cutelyst!');
    //                                  -- 1 down
    //                                  drop table messages;
    //                                   -- 2 up
    //                                   create table log (message text);
    //                                   insert into log values ('logged');
    //                                   -- 2 down
    //                                   drop table log;
    //                                   -- 3 up
    //                                   create tabsle log (message text);
    //                                  )V0G0N"_s);
    //    qDebug() << "MIG" << mig.latest() << mig.active();
    //    qDebug() << "sqlFor" << mig.sqlFor(0, 2);

    QElapsedTimer t;
    t.start();

    auto count = new int{0};

    auto db1 = APool::database();
    for (int i = 0; i < 100000; ++i) {
        db1.exec(APreparedQueryLiteral(u"SELECT * from world"),
                 {},
                 [&count, t](AResult &result) mutable {
            (*count)++;
            if (!result.hasError()) {
                auto data = result.toHash();
                if (data.size() && *count == 10000) {
                    qDebug() << "finish" << count << "elap" << t.elapsed();
                    qApp->quit();
                }
            }
        });
    }

    auto loopT = new QTimer{&app};
    loopT->setInterval(1000);
    loopT->setSingleShot(false);
    QObject::connect(loopT, &QTimer::timeout, loopT, [=] {
        auto db = APool::database();
        db.exec(u"SELECT now()", nullptr, [](AResult &result) {
            if (result.hasError()) {
                qDebug() << "Error" << result.errorString();
            } else {
                qDebug() << "1s loop" << result.toHash();
            }
        });
    });
    loopT->start();

    app.exec();
}
