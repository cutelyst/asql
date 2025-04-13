/*
 * SPDX-FileCopyrightText: (C) 2025 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "../../src/ASqlite.hpp"
#include "../../src/acache.h"
#include "../../src/acoroexpected.h"
#include "../../src/adatabase.h"
#include "../../src/amigrations.h"
#include "../../src/apool.h"
#include "../../src/aresult.h"
#include "../../src/atransaction.h"

#include <thread>

#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QScopeGuard>
#include <QTimer>

using namespace ASql;
using namespace Qt::StringLiterals;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    APool::create(ASqlite::factory(u"sqlite:///tmp/test.sqlite"_s));
    APool::setMaxIdleConnections(10);

    AResultFn resultFn;
    QElapsedTimer t;
    if (true) {
        auto callTerminatorEarly = []() -> ACoroTerminator {
            auto _ = qScopeGuard([] { qDebug() << "coro exited"; });
            qDebug() << "coro started";

            auto result = co_await APool::exec(u8"SELECT 123");
            if (result) {
                qDebug() << "coro result has value" << result->toJsonObject();
            } else {
                qDebug() << "coro result error" << result.error();
            }
        };
        callTerminatorEarly();
    }

    app.exec();
}
