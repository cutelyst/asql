/* 
 * SPDX-FileCopyrightText: (C) 2020 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include <QCoreApplication>
#include <QLoggingCategory>
#include <QElapsedTimer>

#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QTimer>
#include <QUuid>
#include <QUrl>

#include "../../src/apool.h"
#include "../../src/adatabase.h"
#include "../../src/atransaction.h"
#include "../../src/apreparedquery.h"
#include "../../src/aresult.h"
#include "../../src/amigrations.h"
#include "../../src/acache.h"
#include "../../src/apg.h"

using namespace ASql;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    APool::create(APg::factory(QStringLiteral("postgres:///?target_session_attrs=read-write")));
    APool::setMaxIdleConnections(2);
    APool::setMaxConnections(4);

    {
        auto db2 = APool::database();
        auto db3 = APool::database();
        auto db4 = APool::database();
        auto db5 = APool::database();
        auto db6 = APool::database();
        auto db7 = APool::database();
        qDebug() << "db7 valid" << db7.isValid();
        ATransaction t(db7);
        t.begin();
        db7.exec(QStringLiteral("SELECT now()"), [=] (AResult &result) {
            if (result.error()) {
                qDebug() << "SELECT error" << result.errorString();
                return;
            }

            if (result.size()) {
                qDebug() << "SELECT value" << result.begin().value(0);
                ATransaction(t).commit();
            }
        });

//        ADatabase().rollback(); assert

        APool::database([=] (ADatabase &db) {
            qDebug() << "Got db" << db.isOpen() << db.state();

            db.exec(QStringLiteral("SELECT now()"), [=] (AResult &result) {
                if (result.error()) {
                    qDebug() << "got db, SELECT error" << result.errorString();
                    return;
                }

                if (result.size()) {
                    qDebug() << "got db, SELECT value" << result.begin().value(0);
                    ATransaction(t).commit();
                }
            });
            qDebug() << "Got db2" << db.isOpen() << db.state();
        });

    }

    auto db = APool::database();
    static APreparedQuery query(QStringLiteral("SELECT now()"));
    db.exec(query, [=] (AResult &result) {
        if (result.error()) {
            qDebug() << "SELECT 1 error" << result.errorString();
            return;
        }

        if (result.size()) {
            qDebug() << "SELECT 1 value" << result.begin().value(0);
        }
    });

    db.exec(query, [=] (AResult &result) {
        if (result.error()) {
            qDebug() << "SELECT error" << result.errorString();
            return;
        }

        if (result.size()) {
            qDebug() << "SELECT value" << result.begin().value(0);
        }
    });

    static APreparedQuery query2(QStringLiteral("SELECT now(), $1"));
    db.exec(query2, { qint64(12345) }, [=] (AResult &result) {
        if (result.error()) {
            qDebug() << "SELECT error" << result.errorString();
            return;
        }

        if (result.size()) {
            qDebug() << "SELECT value 2" << result.begin().value(0) << result.begin().value(1) << query2.identification();
        }
    });

    db.exec(query2, { qint64(12345) }, [=] (AResult &result) {
        if (result.error()) {
            qDebug() << "SELECT error" << result.errorString();
            return;
        }

        if (result.size()) {
            qDebug() << "SELECT value 2" << result.begin().value(0) << result.begin().value(1) << query2.identification();
        }
    });

    auto loopFn = [=] {
        auto queryStatic = APreparedQueryLiteral(u"SELECT $1, now()");
        ADatabase(db).exec(queryStatic,
                           { qint64(12345) }, [=] (AResult &result) {
            if (result.error()) {
                qDebug() << "SELECT error" << result.errorString();
                return;
            }

            if (result.size()) {
                qDebug() << "SELECT value 3" << result.begin().value(0) << result.begin().value(1) << queryStatic.identification();
            }
        });
    };

    loopFn();
    loopFn();

    app.exec();
}
