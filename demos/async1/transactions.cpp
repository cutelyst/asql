/*
 * SPDX-FileCopyrightText: (C) 2020 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "../../src/acache.h"
#include "../../src/adatabase.h"
#include "../../src/amigrations.h"
#include "../../src/apg.h"
#include "../../src/apool.h"
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
        ATransaction t(db);
        t.begin();
        db.exec(u"SELECT now()"_s, nullptr, [=](AResult &result) {
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
        t.begin(nullptr, [t](AResult &result) {
            if (result.error()) {
                qDebug() << "BEGIN error" << result.errorString();
                return;
            }

            t.database().exec(u"SELECT now()"_s, nullptr, [=](AResult &result) {
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
