/*
 * SPDX-FileCopyrightText: (C) 2025 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */
#include "amysql.h"
#include "apool.h"
#include "tst_prepared_common.h"

#include <QTest>

using namespace ASql;
using namespace Qt::Literals::StringLiterals;

class TestPreparedMysql : public TestPreparedBase
{
    Q_OBJECT
public:
    void initTest() override;
    void cleanupTest() override;
};

void TestPreparedMysql::initTest()
{
    const QString url = qEnvironmentVariable("ASQL_MYSQL_TEST_DB", u"mysql:///"_s);
    if (url == u"mysql:///"_s) {
        QSKIP("ASQL_MYSQL_TEST_DB not set; skipping MySQL prepared tests");
    }
    APool::create(AMysql::factory(url));
    APool::setMaxIdleConnections(2);
    APool::setMaxConnections(5);
}

void TestPreparedMysql::cleanupTest()
{
    APool::remove();
}

QTEST_MAIN(TestPreparedMysql)
#include "tst_PreparedMysql.moc"
