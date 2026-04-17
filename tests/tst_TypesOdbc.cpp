/*
 * SPDX-FileCopyrightText: (C) 2025 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */
#include "AOdbc.hpp"
#include "apool.h"
#include "tst_types_common.h"

#include <QTest>

using namespace ASql;
using namespace Qt::Literals::StringLiterals;

class TestTypesOdbc : public TestTypesBase
{
    Q_OBJECT
public:
    void initTest() override;
    void cleanupTest() override;
};

void TestTypesOdbc::initTest()
{
    const QString url = qEnvironmentVariable("ASQL_ODBC_TEST_DB");
    if (url.isEmpty()) {
        QSKIP("ASQL_ODBC_TEST_DB not set; skipping ODBC type tests");
    }
    APool::create(AOdbc::factory(url));
    APool::setMaxIdleConnections(2);
    APool::setMaxConnections(5);
}

void TestTypesOdbc::cleanupTest()
{
    APool::remove();
}

QTEST_MAIN(TestTypesOdbc)
#include "tst_TypesOdbc.moc"
