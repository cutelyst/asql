/*
 * SPDX-FileCopyrightText: (C) 2025 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */
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
};

void TestTypesPostgres::initTest()
{
    const QString url = qEnvironmentVariable("ASQL_PG_TEST_DB", u"postgresql:///"_s);
    APool::create(APg::factory(url));
    APool::setMaxIdleConnections(2);
    APool::setMaxConnections(5);
}

void TestTypesPostgres::cleanupTest()
{
    APool::remove();
}

QTEST_MAIN(TestTypesPostgres)
#include "tst_TypesPostgres.moc"
