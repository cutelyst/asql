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
    APool::setSetupCallback([] (ADatabase &db) {
        qDebug() << "setup db";
        db.exec(u"SET TIME ZONE 'Europe/Rome';", [] (AResult &result) {
            qDebug() << "SETUP" << result.error() << result.errorString() << result.toJsonObject();
        });
    });

    APool::setReuseCallback([] (ADatabase &db) {
        qDebug() << "reuse db";
        db.exec(u"DISCARD ALL", [] (AResult &result) {
            qDebug() << "REUSE" << result.error() << result.errorString() << result.toJsonObject();
        });
    });


    auto obj = new QObject;
    {
        ADatabase db(APg::factory(QStringLiteral("postgres:///?target_session_attrs=read-write")));
        db.open([] (bool ok, const QString &status) {
            qDebug() << "OPEN value" << ok << status;

        });
        db.exec(QStringLiteral("SELECT now()"), [] (AResult &result) {
            if (result.error()) {
                qDebug() << "SELECT error" << result.errorString();
                return;
            }

            if (result.size()) {
                qDebug() << "SELECT value" << result.begin().value(0);
            }
        }, obj);
    }

    APool::database().exec(u"SELECT pg_sleep(5)", [] (AResult &result) {
        qDebug() << "SELECT result.size()" << result.error() << result.errorString() << result.size();
    }, obj);

    APool::database().exec(u"SELECT now()", [] (AResult &result) {
        qDebug() << "SELECT result.size()" << result.error() << result.errorString() << result.toJsonObject();
    }, obj);

    QTimer::singleShot(2000, obj, [=] {
        qDebug() << "Delete Obj";
        delete obj;
    });

    QTimer::singleShot(2500, [=] {
        qDebug() << "Reuse Timer Obj";
        APool::database().exec(u"SELECT now()", [] (AResult &result) {
            qDebug() << "SELECT result.size()" << result.error() << result.errorString() << result.toJsonObject();
        });
    });

    app.exec();
}
