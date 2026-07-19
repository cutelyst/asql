/*
 * SPDX-FileCopyrightText: (C) 2020-2026 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "../../src/acoroexpected.h"
#include "../../src/adatabase.h"
#include "../../src/apg.h"
#include "../../src/apool.h"
#include "../../src/apreparedquery.h"
#include "../../src/aresult.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QScopeGuard>

using namespace ASql;
using namespace Qt::StringLiterals;

ACoroTerminator runPrepared(std::shared_ptr<QObject> finished)
{
    auto _ = qScopeGuard([finished] {});

    {
        APreparedQuery q1(u"SELECT now()");
        APreparedQuery q2(u"SELECT now()");
        Q_ASSERT(q1.identification() != q2.identification());
    }

    auto db = co_await APool::database();
    if (!db) {
        qDebug() << "Could not get a connection:" << db.error();
        co_return;
    }

    static APreparedQuery nowQuery(u"SELECT now()"_s);
    auto result = co_await db->exec(nowQuery);
    if (!result) {
        qDebug() << "SELECT error" << result.error();
        co_return;
    }
    if (result->size()) {
        qDebug() << "SELECT value" << (*result)[0].value(0);
    }

    static APreparedQuery paramQuery(u"SELECT now(), $1"_s);
    result = co_await db->exec(paramQuery, {qint64(12345)});
    if (!result) {
        qDebug() << "SELECT error" << result.error();
        co_return;
    }
    if (result->size()) {
        qDebug() << "SELECT value" << (*result)[0].value(0) << (*result)[0].value(1)
                 << paramQuery.identification();
    }

    result = co_await db->exec(APreparedQueryLiteral(u"SELECT now(), $1"), {qint64(-1)});
    if (!result) {
        qDebug() << "SELECT literal error" << result.error();
        co_return;
    }
    if (result->size()) {
        qDebug() << "SELECT literal value" << (*result)[0].value(0) << (*result)[0].value(1);
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    APool::create(APg::factory(u"postgres:///?target_session_attrs=read-write"_s));
    APool::setMaxIdleConnections(2);
    APool::setMaxConnections(4);

    QEventLoop loop;
    {
        auto finished = std::make_shared<QObject>();
        QObject::connect(finished.get(), &QObject::destroyed, &loop, &QEventLoop::quit);
        runPrepared(finished);
    }
    loop.exec();

    return 0;
}
