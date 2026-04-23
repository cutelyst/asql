/*
 * SPDX-FileCopyrightText: (C) 2025 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */
#include "tst_prepared_common.h"

#include "acoroexpected.h"
#include "adatabase.h"
#include "apool.h"
#include "apreparedquery.h"

#include <QTest>

using namespace ASql;
using namespace Qt::Literals::StringLiterals;

TestPreparedBase::TestPreparedBase(QObject *parent)
    : CoverageObject(parent)
{
}

QString TestPreparedBase::preparedParam() const
{
    return u"SELECT ?"_s;
}

APreparedQuery TestPreparedBase::preparedParamLiteral() const
{
    // The static APreparedQuery lives here — in the base-class method.
    // Drivers that need a different query (e.g. Postgres "SELECT $1")
    // override this method so their static lives in their own scope.
    return APreparedQueryLiteral(u"SELECT ?"_s);
}

// ─── test slots ───────────────────────────────────────────────────────────────

/*!
 * Execute the same APreparedQuery object multiple times with different integer
 * values.  Verifies that:
 *   - The first call triggers a lazy prepare and returns the correct result.
 *   - Subsequent calls reuse the server-side prepared statement (the same
 *     APreparedQuery identity) and still bind new parameters correctly.
 */
void TestPreparedBase::testPreparedReuse()
{
    // One persistent APreparedQuery object shared across all iterations.
    const APreparedQuery pq(preparedParam());

    const QList<int> values{1, 2, 100, -42, 0, std::numeric_limits<int>::max()};
    for (int sent : values) {
        QEventLoop loop;
        {
            auto finished = std::make_shared<QObject>();
            connect(finished.get(), &QObject::destroyed, &loop, &QEventLoop::quit);
            const int capturedSent = sent;
            [&pq, finished, capturedSent]() -> ACoroTerminator {
                auto _      = qScopeGuard([finished] {});
                auto result = co_await APool::exec(pq, {capturedSent});
                AVERIFY(result);
                AVERIFY(result->size() == 1);
                ACOMPARE_EQ((*result)[0][0].toInt(), capturedSent);
            }();
        }
        loop.exec();
    }
}

/*!
 * Execute a prepared query that takes no parameters (SELECT 1).
 * Verifies that APreparedQuery works for parameter-free statements.
 */
void TestPreparedBase::testPreparedNoParams()
{
    const APreparedQuery pq(u"SELECT 1"_s);

    QEventLoop loop;
    {
        auto finished = std::make_shared<QObject>();
        connect(finished.get(), &QObject::destroyed, &loop, &QEventLoop::quit);
        [&pq, finished]() -> ACoroTerminator {
            auto _      = qScopeGuard([finished] {});
            auto result = co_await APool::exec(pq);
            AVERIFY(result);
            AVERIFY(result->size() == 1);
            ACOMPARE_EQ((*result)[0][0].toInt(), 1);
        }();
    }
    loop.exec();

    // Execute again to exercise the reuse path.
    QEventLoop loop2;
    {
        auto finished = std::make_shared<QObject>();
        connect(finished.get(), &QObject::destroyed, &loop2, &QEventLoop::quit);
        [&pq, finished]() -> ACoroTerminator {
            auto _      = qScopeGuard([finished] {});
            auto result = co_await APool::exec(pq);
            AVERIFY(result);
            AVERIFY(result->size() == 1);
            ACOMPARE_EQ((*result)[0][0].toInt(), 1);
        }();
    }
    loop2.exec();
}

/*!
 * Verify the APreparedQueryLiteral convenience macro, which creates a
 * function-local static APreparedQuery (prepared at most once per process).
 */
void TestPreparedBase::testPreparedLiteral()
{
    const QStringList values{
        u"hello"_s,
        u"world"_s,
        u"foo bar"_s,
    };
    for (const QString &sent : values) {
        QEventLoop loop;
        {
            auto finished = std::make_shared<QObject>();
            connect(finished.get(), &QObject::destroyed, &loop, &QEventLoop::quit);
            const QString capturedSent = sent;
            const APreparedQuery pq    = preparedParamLiteral();
            [pq, finished, capturedSent]() -> ACoroTerminator {
                auto _ = qScopeGuard([finished] {});
                // pq was built via APreparedQueryLiteral: it carries the same
                // identity on every call, so only one PREPARE is ever sent.
                auto result = co_await APool::exec(pq, {capturedSent});
                AVERIFY(result);
                AVERIFY(result->size() == 1);
                ACOMPARE_EQ((*result)[0][0].toString(), capturedSent);
            }();
        }
        loop.exec();
    }
}

/*!
 * Exercise the same APreparedQuery on two pool connections concurrently.
 * Each connection must independently lazy-prepare the statement on its first
 * use; subsequent calls on that connection reuse its prepared handle.
 *
 * Strategy:
 *   1. Acquire two ADatabase handles from the pool simultaneously.
 *   2. Execute the prepared query on each handle with a distinct value.
 *   3. Verify both results are correct.
 *   4. Execute on each handle again (reuse path) with new values.
 */
void TestPreparedBase::testPreparedMultipleConnections()
{
    APool::setMaxConnections(2);

    const APreparedQuery pq(preparedParam());

    QEventLoop loop;
    {
        auto finished = std::make_shared<QObject>();
        connect(finished.get(), &QObject::destroyed, &loop, &QEventLoop::quit);
        [&pq, finished]() -> ACoroTerminator {
            auto _ = qScopeGuard([finished] {});

            // Acquire two separate connections from the pool.
            auto db1 = co_await APool::database();
            AVERIFY(db1);
            AVERIFY(db1->isOpen());

            auto db2 = co_await APool::database();
            AVERIFY(db2);
            AVERIFY(db2->isOpen());

            // First use on each connection: triggers lazy PREPARE.
            auto r1 = co_await db1->exec(pq, {10});
            AVERIFY(r1);
            AVERIFY(r1->size() == 1);
            ACOMPARE_EQ((*r1)[0][0].toInt(), 10);

            auto r2 = co_await db2->exec(pq, {20});
            AVERIFY(r2);
            AVERIFY(r2->size() == 1);
            ACOMPARE_EQ((*r2)[0][0].toInt(), 20);

            // Second use on each connection: exercises the reuse path.
            auto r3 = co_await db1->exec(pq, {30});
            AVERIFY(r3);
            AVERIFY(r3->size() == 1);
            ACOMPARE_EQ((*r3)[0][0].toInt(), 30);

            auto r4 = co_await db2->exec(pq, {40});
            AVERIFY(r4);
            AVERIFY(r4->size() == 1);
            ACOMPARE_EQ((*r4)[0][0].toInt(), 40);
        }();
    }
    loop.exec();
}

#include "moc_tst_prepared_common.cpp"
