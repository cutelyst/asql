/*
 * SPDX-FileCopyrightText: (C) 2025 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */
#include "amysql.h"
#include "apool.h"
#include "tst_types_common.h"

#include <QTest>

using namespace ASql;
using namespace Qt::Literals::StringLiterals;

class TestTypesMysql : public TestTypesBase
{
    Q_OBJECT
public:
    void initTest() override;
    void cleanupTest() override;
};

void TestTypesMysql::initTest()
{
    const QString url = qEnvironmentVariable("ASQL_MYSQL_TEST_DB", u"mysql:///"_s);
    APool::create(AMysql::factory(url));
    APool::setMaxIdleConnections(2);
    APool::setMaxConnections(5);
}

void TestTypesMysql::cleanupTest()
{
    APool::remove();
}

QTEST_MAIN(TestTypesMysql)
#include "tst_TypesMysql.moc"
