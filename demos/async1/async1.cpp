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

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    {
        // regresion test crash - where selfDriver gets released
        APool::create(APg::factory(QStringLiteral("postgres:///")), u"delete_db_after_use");
        APool::setMaxIdleConnections(0, u"delete_db_after_use");

        {
            APool::database(u"delete_db_after_use").exec(u"SELECT 'I ♥ Cutelyst!' AS utf8", nullptr, [](AResult &result) {
                qDebug() << "=====iterator single row" << result.toHash();
                if (result.error()) {
                    qDebug() << "Error" << result.errorString();
                }
            });
        }
    }

    APool::create(APg::factory(QStringLiteral("postgres:///")));
    APool::setMaxIdleConnections(10);

    {
        auto db = APool::database();
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        // Zero-copy and zero allocation
        db.exec(u8"SELECT 'I ♥ Cutelyst!' AS utf8", nullptr, [](AResult &result) {
            qDebug() << "=====iterator single row" << result.toHash();
            if (result.error()) {
                qDebug() << "Error" << result.errorString();
            }
        });
#endif
        // Zero-copy but allocates due toUtf8()
        db.exec(u"SELECT 'I ♥ Cutelyst!' AS utf8", nullptr, [](AResult &result) {
            qDebug() << "=====iterator single row" << result.toHash();
            if (result.error()) {
                qDebug() << "Error" << result.errorString();
            }
        });
    }

    QVariantList series;
    {
        auto db = APool::database();
        qDebug() << "123";
        db.exec(u"SELECT generate_series(1, 10) AS number", nullptr, [&series](AResult &result) mutable {
            qDebug() << "=====iterator single row" << result.errorString() << result.size() << "last" << result.lastResulSet() << "mutable" << series.size();
            if (result.error()) {
                qDebug() << "Error" << result.errorString();
            }

            // For range
            for (const auto &row : result) {
                qDebug() << "for loop row numbered" << row.value(0) << row.value(QStringLiteral("number"));
                series.append(row[0].value());
            }

            // Iterators
            auto it = result.begin();
            while (it != result.end()) {
                qDebug() << "iterator" << it.at() << it.value(0) << it[QStringLiteral("number")].value() << it[0].toInt();
                ++it;
            }
        });
        db.setLastQuerySingleRowMode();

        db.exec(QStringLiteral("SELECT generate_series(1, 10) AS number"), nullptr, [&series](AResult &result) mutable {
            qDebug() << "=====iterator" << result.errorString() << result.size() << "last" << result.lastResulSet() << "mutable" << series.size();
            if (result.error()) {
                qDebug() << "Error" << result.errorString();
            }

            // For range
            for (const auto &row : result) {
                qDebug() << "for loop row numbered" << row.value(0) << row[QStringLiteral("number")].value() << row[0].toInt();
                series.append(row[0].value());
            }

            // Iterators
            auto it = result.begin();
            while (it != result.end()) {
                qDebug() << "iterator" << it.at() << it[0].value() << it.value(QStringLiteral("number")) << it[0].toInt();
                ++it;
            }
        });
    }

    APool::database().exec(QStringLiteral("SELECT $1, $2, $3, $4, now()"),
                           {
                               QJsonValue(true),
                               QJsonValue(123.4567),
                               QJsonValue(QStringLiteral("fooo")),
                               QJsonValue(QJsonObject{}),
                           },
                           nullptr,
                           [&series](AResult &result) mutable {
        qDebug() << "=====iterator JSON" << result.errorString() << result.size() << "last" << result.lastResulSet() << "mutable" << series.size();
        if (result.error()) {
            qDebug() << "Error" << result.errorString();
        }

        // For range
        qDebug() << "JSON result" << result[0].toList();
    });

    APool::database().exec(QStringLiteral("select jsonb_build_object('foo', now());"),
                           nullptr,
                           [](AResult &result) mutable {
        qDebug() << "=====iterator JSON" << result.errorString() << result.size() << "last" << result[0][0].toJsonValue();
        if (result.error()) {
            qDebug() << "Error" << result.errorString();
        }

        // For range
        qDebug() << "JSON result" << result[0].toList();
    });

    auto cache = new ACache;
    cache->setDatabase(APool::database());
    cache->exec(QStringLiteral("SELECT now()"), nullptr, [=](AResult &result) {
        qDebug() << "CACHED 1" << result.errorString() << result.size();
        if (result.error()) {
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
        cache->exec(QStringLiteral("SELECT now()"), nullptr, [=](AResult &result) {
            qDebug() << "CACHED 2" << result.errorString() << result.size();
            if (result.error()) {
                qDebug() << "Error" << result.errorString();
            }

            for (const auto &row : result) {
                for (int i = 0; i < result.fields(); ++i) {
                    qDebug() << "cached 2" << i << row.value(i);
                }
            }
        });

        bool ret = cache->clear(QStringLiteral("SELECT now()"));
        qDebug() << "CACHED - CLEARED" << ret;

        cache->exec(QStringLiteral("SELECT now()"), nullptr, [=](AResult &result) {
            qDebug() << "CACHED 3" << result.errorString() << result.size();
            if (result.error()) {
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

    //    APool::database().exec(QStringLiteral("select 100, pg_sleep(1)"), [=] (AResult &result) {
    //        qDebug() << "data" << result.size() << result.error() << result.errorString() << "LAST" << result.lastResulSet();
    //        delete obj;
    //        while (result.next()) {
    //            for (int i = 0; i < result.fields(); ++i) {
    //                qDebug() << "data" << result.at() << i << result.value(i);
    //            }
    //        }
    //    }, obj);

    //    APool::database().exec(QStringLiteral("select 200, pg_sleep(5)"), [=] (AResult &result) {
    //        qDebug() << "data" << result.size() << result.error() << result.errorString() << "LAST" << result.lastResulSet();
    //        delete obj;
    //        while (result.next()) {
    //            for (int i = 0; i < result.fields(); ++i) {
    //                qDebug() << "data" << result.at() << i << result.value(i);
    //            }
    //        }
    //    }, obj);
    //    ADatabase db(APool::database());
    //    db.open([=] (bool isOpen, const QString &error) {
    //        qDebug() << "OPEN" << isOpen << error;

    ////        ADatabase(db).exec(QStringLiteral("select 1; select 2"), [=] (AResult &result) {
    ////            qDebug() << "data" << result.size() << result.error() << result.errorString() << "LAST" << result.lastResulSet();
    ////            while (result.next()) {
    ////                for (int i = 0; i < result.fields(); ++i) {
    ////                    qDebug() << "data" << result.at() << i << result.value(i);
    ////                }
    ////            }
    ////        });
    ////        QJsonObject obj{
    ////            {QStringLiteral("foo"), 234}
    ////        };
    //        ADatabase db1 = APool::database();
    //        db1.begin();
    //        QUuid uuid = QUuid::createUuid();
    //        qDebug() << "uuid" << uuid.toString();
    //        db1.exec(QStringLiteral("insert into temp4 values ($1, $2, $3, $4, $5, $6)"),
    //        {true, QStringLiteral("bla bla"), QVariant::Int, QDateTime::currentDateTime(), 123456.78, uuid},
    //                [=] (AResult &result) {
    //            qDebug() << "data" << result.size() << result.error() << result.errorString();
    //            while (result.next()) {
    //                for (int i = 0; i < result.fields(); ++i) {
    //                    qDebug() << "data" << result.at() << i << result.value(i);
    //                }
    //            }
    //        });
    //        db1.commit();

    ////        db1.subscribeToNotification(QStringLiteral("minha_notifyç_ã3"), [=] (const QString &payload, bool self){
    ////            qDebug() << "notificação" << payload << self;
    ////        });
    //    });

    //    APool::database().exec(QStringLiteral("select * from aaa.users limit 3"), [=] (AResult &result) {
    //        qDebug() << "data" << result.size() << result.error() << result.errorString() << "LAST" << result.lastResulSet();
    //        qDebug() << "data" << result.hash();
    //        qDebug() << "data" << result.hashes();
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
    //    mig->load(APool::database(), QStringLiteral("foo"));

    //    mig->fromString(QStringLiteral(R"V0G0N(
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
    //                                  )V0G0N"));
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
            if (!result.error()) {
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
            if (result.error()) {
                qDebug() << "Error" << result.errorString();
            } else {
                qDebug() << "1s loop" << result.toHash();
            }
        });
    });
    loopT->start();

    app.exec();
}
