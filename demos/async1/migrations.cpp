/*
 * SPDX-FileCopyrightText: (C) 2020 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "../../src/acache.h"
#include "../../src/adatabase.h"
#include "../../src/amigrations.h"
#include "../../src/apg.h"
#include "../../src/apool.h"
#include "../../src/apreparedquery.h"
#include "../../src/aresult.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
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

    //    APool::addDatabase(u"postgres://server.com,server2.com/mydb?target_session_attrs=read-write"_s);
    APool::create(APg::factory(u"postgres:///"_s));

    auto mig = new AMigrations();
    mig->connect(mig, &AMigrations::ready, [=](bool error, const QString &erroString) {
        qDebug() << "Read to migrate" << error << erroString;
        mig->migrate(0, [=](bool error, const QString &errorString) {
            qDebug() << "Migration Error" << error << errorString;
        });
    });
    mig->load(APool::database(), u"foo"_s);

    mig->fromString(uR"V0G0N(
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
)V0G0N"_s);
    qDebug() << "MIG" << mig->latest() << mig->active();
    //    qDebug() << "sqlFor" << mig->sqlFor(0, 2);

    app.exec();
}
