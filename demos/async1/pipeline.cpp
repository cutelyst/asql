/*
 * SPDX-FileCopyrightText: (C) 2022 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "../../src/acoroexpected.h"
#include "../../src/adatabase.h"
#include "../../src/apg.h"
#include "../../src/apool.h"
#include "../../src/apreparedquery.h"
#include "../../src/aresult.h"
#include "../../src/atransaction.h"

#include <thread>

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QTimer>
#include <QUrl>
#include <QUuid>

using namespace ASql;
using namespace Qt::StringLiterals;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    APool::create(APg::factory(u"postgres:///?target_session_attrs=read-write"_s));
    APool::setMaxIdleConnections(10);

    {
        auto db = APool::database();
        db.onStateChanged(
            nullptr, [db](ADatabase::State state, const QString &msg) mutable -> ACoroTerminator {
            // Must be called with an empty db query queue and after it is connected (state)
            qDebug() << "PIPELINE ENTER" << state << db.enterPipelineMode();

            qDebug() << "PIPELINE STATUS" << int(db.pipelineStatus());

            // TODO need to store a list of awaitables
            for (int i = 0; i < 10; ++i) {
                auto result = co_await db.coExec(u"SELECT now(), $1", {i}, nullptr);
                if (!result) {
                    qDebug() << "PIPELINE SELECT error" << i << result.error();
                    co_return;
                }

                if (result->size()) {
                    qDebug() << "PIPELINE SELECT value" << i << result->begin()[1].toInt()
                             << result->begin().value(0);
                }
            }
            // Must be called either after some X number of queries or periodically
            // if enterPipelineMode did not set autoSync
            qDebug() << "PIPELINE SYNC" << db.pipelineSync();
        });
    }

    {
        auto db = APool::database();
        db.onStateChanged(
            nullptr, [db](ADatabase::State state, const QString &msg) mutable -> ACoroTerminator {
            using namespace std::chrono;
            // Must be called with an empty db query queue and after it is connected (state)
            qDebug() << "2 PIPELINE ENTER" << state << db.enterPipelineMode(2s);

#if 0
            qDebug() << "2 PIPELINE STATUS" << int(db.pipelineStatus());
            auto callDb = [db](int id) mutable {
                db.exec(APreparedQuery(u"SELECT now(), $1"), {id}, nullptr, [=](AResult &result) {
                    if (result.hasError()) {
                        qDebug() << "2 PIPELINE SELECT error" << id << result.errorString();
                        return;
                    }

                    if (result.size()) {
                        qDebug() << "2 PIPELINE SELECT value" << id << result.begin()[1].toInt()
                                 << result.begin().value(0);
                    }
                });
            };

            auto callStaticDb = [db](int id) mutable  -> ACoroTerminator{
                db.exec(APreparedQueryLiteral(u"SELECT now(), $1"),
                        {id},
                        nullptr,
                        [=](AResult &result) {
                    if (result.hasError()) {
                        qDebug() << "2 PIPELINE SELECT error" << id << result.errorString();
                        return;
                    }

                    if (result.size()) {
                        qDebug() << "2 PIPELINE SELECT value" << id << result.begin()[1].toInt()
                                 << result.begin().value(0);
                    }
                });
            };

            for (int i = 0; i < 3; ++i) {
                callDb(i);
                callStaticDb(-i);
            }
#endif
        });
    }

    app.exec();
}
