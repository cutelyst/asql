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
#include <QElapsedTimer>

#include "../../src/apool.h"
#include "../../src/adatabase.h"
#include "../../src/aresult.h"
#include "../../src/amigrations.h"
#include "../../src/acache.h"
#include "../../src/apreparedquery.h"
#include "../../src/apg.h"

using namespace ASql;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

//    APool::addDatabase(QStringLiteral("postgres://server.com,server2.com/mydb?target_session_attrs=read-write"));
    APool::create(APg::factory(QStringLiteral("postgres:///")));

    auto mig = new AMigrations();
    mig->connect(mig, &AMigrations::ready, [=] (bool error, const QString &erroString) {
        qDebug() << "Read to migrate" << error << erroString;
        mig->migrate(0, [=] (bool error, const QString &errorString) {
            qDebug() << "Migration Error" << error << errorString;
        });
    });
    mig->load(APool::database(), QStringLiteral("foo"));

    mig->fromString(QStringLiteral(R"V0G0N(
                                   -- 1 up
                                   create table messages (message text);
                                   insert into messages values ('I ♥ Cutelyst!');
                                   -- 1 down
                                   drop table messages;
                                   -- 2 up
                                   create table log (message text);
                                   insert into log values ('logged');
                                   -- 2 down
                                   drop table log;
                                   -- 3 up
                                   create tabsle log (message text);
                                   )V0G0N"));
    qDebug() << "MIG" << mig->latest() << mig->active();
//    qDebug() << "sqlFor" << mig->sqlFor(0, 2);

    app.exec();
}
