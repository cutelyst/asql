/*
 * SPDX-FileCopyrightText: (C) 2020 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "../../src/acache.h"
#include "../../src/adatabase.h"
#include "../../src/amigrations.h"
#include "../../src/apool.h"
#include "../../src/aresult.h"
#include "../../src/atransaction.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QTimer>
#include <QUrl>
#include <QUuid>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    auto obj = new QObject;
    {
        ADatabase db(u"postgres:///?target_session_attrs=read-write"_s);
        db.open([db, obj](bool ok, const QString &status) {
            qDebug() << "OPEN value" << ok << status;

            ADatabase(db).exec(u"SELECT now()"_s, [db](AResult &result) {
                if (result.error()) {
                    qDebug() << "SELECT error" << result.errorString();
                    return;
                }

                if (result.next()) {
                    qDebug() << "SELECT value" << result.value(0);
                }
            }, obj);
        });
    }

    QTimer::singleShot(2000, [=] {
        qDebug() << "Delete Obj";
        delete obj;
    });

    app.exec();
}
