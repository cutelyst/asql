/*
 * SPDX-FileCopyrightText: (C) 2025 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */
#include "acoroexpected.h"
#include "apg.h"
#include "apool.h"
#include "tst_types_common.h"

#include <QTest>

using namespace ASql;
using namespace Qt::Literals::StringLiterals;

class TestTypesPostgres : public TestTypesBase
{
    Q_OBJECT
public:
    void initTest() override;
    void cleanupTest() override;
    QString selectParam() const override { return u"SELECT $1"_s; }

private Q_SLOTS:
    void testJsonbToByteArray();
};

void TestTypesPostgres::initTest()
{
    if (!qEnvironmentVariableIsSet("ASQL_PG_TEST_DB")) {
        QSKIP("ASQL_PG_TEST_DB not set; skipping PostgreSQL types tests");
    }
    const QString url = qEnvironmentVariable("ASQL_PG_TEST_DB", u"postgresql:///"_s);
    APool::create(APg::factory(url));
    APool::setMaxIdleConnections(2);
    APool::setMaxConnections(5);
}

void TestTypesPostgres::cleanupTest()
{
    APool::remove();
}

void TestTypesPostgres::testJsonbToByteArray()
{
    const QByteArray json = R"({"key":"value","num":42,"arr":[1,2]})";

    QEventLoop loop;
    {
        auto finished = std::make_shared<QObject>();
        connect(finished.get(), &QObject::destroyed, &loop, &QEventLoop::quit);

        [](std::shared_ptr<QObject> finished, QByteArray json) -> ACoroTerminator {
            auto _ = qScopeGuard([finished] {});

            auto result = co_await APool::exec(u"SELECT $1::jsonb"_s, {QString::fromUtf8(json)});
            AVERIFY(result);
            AVERIFY(result->size() == 1);
            ACOMPARE_EQ((*result)[0][0].toByteArray(), json);
        }(finished, json);
    }
    loop.exec();
}

QTEST_MAIN(TestTypesPostgres)
#include "tst_TypesPostgres.moc"
