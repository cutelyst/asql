#include <QCoreApplication>
#include <QLoggingCategory>

#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QTimer>
#include <QUuid>
#include <QUrl>

#include "../../src/apool.h"
#include "../../src/adatabase.h"
#include "../../src/aresult.h"
#include "../../src/amigrations.h"
#include "../../src/acache.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    APool::addDatabase(QStringLiteral("postgres://server.com,server2.com/mydb?target_session_attrs=read-write"));
    APool::setDatabaseMaxIdleConnections(10);

    auto cache = new ACache;
    cache->setDatabase(APool::database());
    cache->exec(QStringLiteral("SELECT now()"), [=] (AResult &result) {
        qDebug() << "CACHED 1" << result.errorString() << result.size();
        if (result.error()) {
            qDebug() << "Error" << result.errorString();
        }

        while (result.next()) {
            for (int i = 0; i < result.fields(); ++i) {
                qDebug() << "cached 1" << result.at() << i << result.value(i);
            }
        }
    }, new QObject);

    QTimer::singleShot(2000, [=] {
        cache->exec(QStringLiteral("SELECT now()"), [=] (AResult &result) {
            qDebug() << "CACHED 2" << result.errorString() << result.size();
            if (result.error()) {
                qDebug() << "Error" << result.errorString();
            }

            while (result.next()) {
                for (int i = 0; i < result.fields(); ++i) {
                    qDebug() << "cached 2" << result.at() << i << result.value(i);
                }
            }
        }, new QObject);

        bool ret = cache->clear(QStringLiteral("SELECT now()"));
        qDebug() << "CACHED - CLEARED" << ret;

        cache->exec(QStringLiteral("SELECT now()"), [=] (AResult &result) {
            qDebug() << "CACHED 3" << result.errorString() << result.size();
            if (result.error()) {
                qDebug() << "Error 3" << result.errorString();
            }

            while (result.next()) {
                for (int i = 0; i < result.fields(); ++i) {
                    qDebug() << "cached 3" << result.at() << i << result.value(i);
                }
            }
        }, new QObject);
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

    app.exec();
}
