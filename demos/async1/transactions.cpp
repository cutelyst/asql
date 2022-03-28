/* 
 * SPDX-FileCopyrightText: (C) 2020 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

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
#include "../../src/atransaction.h"
#include "../../src/aresult.h"
#include "../../src/amigrations.h"
#include "../../src/acache.h"
#include "../../src/apg.h"

using namespace ASql;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    APool::create(APg::factory(QStringLiteral("postgres:///?target_session_attrs=read-write")));
    APool::setMaxIdleConnections(10);

    {
        auto db = APool::database();
        ATransaction t(db);
        t.begin();
        db.exec(QStringLiteral("SELECT now()"), [=] (AResult &result) {
            if (result.error()) {
                qDebug() << "SELECT error" << result.errorString();
                return;
            }

            if (result.size()) {
                qDebug() << "SELECT value" << result.begin().value(0);
                ATransaction(t).commit();
            }
        });
    }

    {
        ATransaction t(APool::database());
        t.begin([t] (AResult &result){
            if (result.error()) {
                qDebug() << "BEGIN error" << result.errorString();
                return;
            }

            ADatabase(t.database()).exec(QStringLiteral("SELECT now()"),
                                         [=] (AResult &result) {
                if (result.error()) {
                    qDebug() << "SELECT error" << result.errorString();
                    return;
                }

                if (result.size()) {
                    qDebug() << "SELECT value" << result.begin().value(0);
                }
            });
        });
    }

    app.exec();
}
