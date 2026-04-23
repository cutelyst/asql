/*
 * SPDX-FileCopyrightText: (C) 2025 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */
#include "apg.h"
#include "apool.h"
#include "apreparedquery.h"
#include "tst_prepared_common.h"

#include <QTest>

using namespace ASql;
using namespace Qt::Literals::StringLiterals;

class TestPreparedPostgres : public TestPreparedBase
{
    Q_OBJECT
public:
    void initTest() override;
    void cleanupTest() override;
    QString preparedParam() const override;
    APreparedQuery preparedParamLiteral() const override;
};

void TestPreparedPostgres::initTest()
{
    const QString url = qEnvironmentVariable("ASQL_POSTGRES_TEST_DB", u"postgresql:///"_s);
    if (url == u"postgresql:///"_s) {
        QSKIP("ASQL_POSTGRES_TEST_DB not set; skipping Postgres prepared tests");
    }
    APool::create(APg::factory(url));
    APool::setMaxIdleConnections(2);
    APool::setMaxConnections(5);
}

void TestPreparedPostgres::cleanupTest()
{
    APool::remove();
}

QString TestPreparedPostgres::preparedParam() const
{
    return u"SELECT $1"_s;
}

APreparedQuery TestPreparedPostgres::preparedParamLiteral() const
{
    return APreparedQueryLiteral(u"SELECT $1"_s);
}

QTEST_MAIN(TestPreparedPostgres)
#include "tst_PreparedPostgres.moc"
