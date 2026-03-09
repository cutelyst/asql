/*
 * SPDX-FileCopyrightText: (C) 2020 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "../../src/acoroexpected.h"
#include "../../src/adatabase.h"
#include "../../src/apool.h"
#include "../../src/aresult.h"

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

    auto obj = new QObject;
    {
        ADatabase db(u"postgres:///?target_session_attrs=read-write"_s);
        [](ADatabase db, QObject *obj) -> ACoroTerminator {
            auto opened = co_await db.coOpen(obj);
            qDebug() << "OPEN value" << opened.has_value() << opened.error();

            if (opened.has_value()) {
                auto result = co_await db.exec(u"SELECT now()"_s, obj);
                if (!result.has_value()) {
                    qDebug() << "SELECT error" << result.error();
                } else {
                    qDebug() << "SELECT value" << result->toJsonObject();
                }
            }
        }(db, obj);
    }

    QTimer::singleShot(2000, [=] {
        qDebug() << "Delete Obj";
        delete obj;
    });

    app.exec();
}
