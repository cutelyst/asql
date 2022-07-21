/*
 * SPDX-FileCopyrightText: (C) 2022 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include <QCoreApplication>
#include <QLoggingCategory>

#include <thread>

#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QTimer>
#include <QUuid>
#include <QUrl>

#include "../../src/apool.h"
#include "../../src/adatabase.h"
#include "../../src/atransaction.h"
#include "../../src/aresult.h"
#include "../../src/apreparedquery.h"
#include "../../src/apg.h"

using namespace ASql;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    APool::create(APg::factory(QStringLiteral("postgres:///?target_session_attrs=read-write")));
    APool::setMaxIdleConnections(10);

    {
        auto db = APool::database();
        db.onStateChanged([db] (ADatabase::State state, const QString &msg) mutable {
            // Must be called with an empty db query queue and after it is connected (state)
            qDebug() << "PIPELINE ENTER" << state << db.enterPipelineMode();

            qDebug() << "PIPELINE STATUS" << int(db.pipelineStatus());
            auto callDb = [db](int id) mutable {
                db.exec(u"SELECT now(), $1", {id},
                        [=] (AResult &result) {
                    if (result.error()) {
                        qDebug() << "PIPELINE SELECT error" << id << result.errorString();
                        return;
                    }

                    if (result.size()) {
                        qDebug() << "PIPELINE SELECT value" << id << result.begin()[1].toInt() << result.begin().value(0);
                    }
                });
            };

            for (int i = 0; i < 10; ++i) {
                callDb(i);
            }
            // Must be called either after some X number of queries or periodically
            // if enterPipelineMode did not set autoSync
            qDebug() << "PIPELINE SYNC" << db.pipelineSync();
        });
    }

    {
        auto db = APool::database();
        db.onStateChanged([db] (ADatabase::State state, const QString &msg) mutable {
            // Must be called with an empty db query queue and after it is connected (state)
            qDebug() << "2 PIPELINE ENTER" << state << db.enterPipelineMode(2000);

            qDebug() << "2 PIPELINE STATUS" << int(db.pipelineStatus());
            auto callDb = [db](int id) mutable {
                db.exec(APreparedQuery(u"SELECT now(), $1"), {id},
                        [=] (AResult &result) {
                    if (result.error()) {
                        qDebug() << "2 PIPELINE SELECT error" << id << result.errorString();
                        return;
                    }

                    if (result.size()) {
                        qDebug() << "2 PIPELINE SELECT value" << id << result.begin()[1].toInt() << result.begin().value(0);
                    }
                });
            };

            auto callStaticDb = [db](int id) mutable {
                db.exec(APreparedQueryLiteral(u"SELECT now(), $1"), {id},
                        [=] (AResult &result) {
                    if (result.error()) {
                        qDebug() << "2 PIPELINE SELECT error" << id << result.errorString();
                        return;
                    }

                    if (result.size()) {
                        qDebug() << "2 PIPELINE SELECT value" << id << result.begin()[1].toInt() << result.begin().value(0);
                    }
                });
            };

            for (int i = 0; i < 3; ++i) {
                callDb(i);
                callStaticDb(-i);
            }
        });
    }

    app.exec();
}
