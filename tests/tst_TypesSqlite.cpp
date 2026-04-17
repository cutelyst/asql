/*
 * SPDX-FileCopyrightText: (C) 2025 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */
#include "ASqlite.hpp"
#include "apool.h"
#include "tst_types_common.h"

#include <QTest>

using namespace ASql;
using namespace Qt::Literals::StringLiterals;

class TestTypesSqlite : public TestTypesBase
{
    Q_OBJECT
public:
    void initTest() override;
    void cleanupTest() override;
};

void TestTypesSqlite::initTest()
{
    APool::create(ASqlite::factory(u"sqlite://?MEMORY"_s));
    APool::setMaxIdleConnections(2);
    APool::setMaxConnections(5);
}

void TestTypesSqlite::cleanupTest()
{
    APool::remove();
}

QTEST_MAIN(TestTypesSqlite)
#include "tst_TypesSqlite.moc"
