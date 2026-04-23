/*
 * SPDX-FileCopyrightText: (C) 2025 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */
#include "AOdbc.hpp"
#include "apool.h"
#include "tst_prepared_common.h"

#include <QTest>

using namespace ASql;
using namespace Qt::Literals::StringLiterals;

class TestPreparedOdbc : public TestPreparedBase
{
    Q_OBJECT
public:
    void initTest() override;
    void cleanupTest() override;
};

void TestPreparedOdbc::initTest()
{
    const QString url = qEnvironmentVariable("ASQL_ODBC_TEST_DB", QString{});
    if (url.isEmpty()) {
        QSKIP("ASQL_ODBC_TEST_DB not set; skipping ODBC prepared tests");
    }
    APool::create(AOdbc::factory(url));
    APool::setMaxIdleConnections(2);
    APool::setMaxConnections(5);
}

void TestPreparedOdbc::cleanupTest()
{
    APool::remove();
}

QTEST_MAIN(TestPreparedOdbc)
#include "tst_PreparedOdbc.moc"
