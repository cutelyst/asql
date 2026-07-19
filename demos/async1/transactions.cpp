/*
 * SPDX-FileCopyrightText: (C) 2020-2026 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "../../src/acoroexpected.h"
#include "../../src/adatabase.h"
#include "../../src/apg.h"
#include "../../src/apool.h"
#include "../../src/aresult.h"
#include "../../src/atransaction.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QJsonObject>
#include <QScopeGuard>

using namespace ASql;
using namespace Qt::StringLiterals;

ACoroTerminator runTransactionsDemo(std::shared_ptr<QObject> finished)
{
    auto _ = qScopeGuard([finished] {});

    {
        auto t = co_await APool::begin();
        if (!t) {
            qDebug() << "BEGIN error:" << t.error();
            co_return;
        }

        auto result = co_await t->database().exec(u"SELECT now()"_s);
        if (!result) {
            qDebug() << "SELECT error:" << result.error();
            co_return;
        }
        qDebug() << "SELECT" << result->toJsonObject();

        auto commit = co_await t->commit();
        qDebug() << "COMMIT" << (commit ? u"ok"_s : commit.error());
    }

    auto setup = co_await APool::database();
    if (!setup) {
        qDebug() << "database error:" << setup.error();
        co_return;
    }

    auto create = co_await setup->exec(u"CREATE TEMP TABLE IF NOT EXISTS tx_demo (id INTEGER)"_s);
    if (!create) {
        qDebug() << "CREATE error:" << create.error();
        co_return;
    }

    {
        auto t = co_await APool::begin();
        if (!t) {
            qDebug() << "BEGIN error:" << t.error();
            co_return;
        }

        auto insert = co_await t->database().exec(u"INSERT INTO tx_demo VALUES (1)"_s);
        if (!insert) {
            qDebug() << "INSERT error:" << insert.error();
            co_return;
        }
        qDebug() << "INSERT rows" << insert->numRowsAffected();
        // Leave scope without commit — RAII rolls back.
    }

    auto count = co_await setup->exec(u"SELECT COUNT(*) FROM tx_demo"_s);
    if (count) {
        qDebug() << "rows after RAII rollback" << (*count)[0][0].toInt();
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    APool::create(APg::factory(u"postgres:///?target_session_attrs=read-write"_s));
    APool::setMaxIdleConnections(10);

    QEventLoop loop;
    {
        auto finished = std::make_shared<QObject>();
        QObject::connect(finished.get(), &QObject::destroyed, &loop, &QEventLoop::quit);
        runTransactionsDemo(finished);
    }
    loop.exec();

    return 0;
}
