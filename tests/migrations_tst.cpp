/*
 * SPDX-FileCopyrightText: (C) 2026 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "ASqlite.hpp"
#include "CoverageObject.hpp"
#include "acoroexpected.h"
#include "adatabase.h"
#include "amigrations.h"
#include "apool.h"

#include <functional>

#include <QObject>
#include <QScopeGuard>
#include <QTest>

using namespace ASql;
using namespace Qt::Literals::StringLiterals;

namespace {

const QString kContiguousMigrations = uR"(
-- 1 up
CREATE TABLE mig_v1 (id INTEGER);
INSERT INTO mig_v1 VALUES (1);
-- 1 down
DROP TABLE mig_v1;
-- 2 up
CREATE TABLE mig_v2 (id INTEGER);
INSERT INTO mig_v2 VALUES (2);
-- 2 down
DROP TABLE mig_v2;
-- 3 up
CREATE TABLE mig_v3 (id INTEGER);
INSERT INTO mig_v3 VALUES (3);
-- 3 down
DROP TABLE mig_v3;
)"_s;

const QString kSparseMigrations = uR"(
-- 1 up
CREATE TABLE mig_step1 (id INTEGER);
-- 1 down
DROP TABLE mig_step1;
-- 5 up
CREATE TABLE mig_step5 (id INTEGER);
-- 5 down
DROP TABLE mig_step5;
-- 10 up
CREATE TABLE mig_step10 (id INTEGER);
-- 10 down
DROP TABLE mig_step10;
)"_s;

ACoroTerminator runMigrationUpAndDown(std::shared_ptr<QObject> finished)
{
    auto _ = qScopeGuard([finished] {});

    auto db = co_await APool::database(nullptr, u"migrations"_s);
    AVERIFY(db);

    AMigrations mig;
    mig.fromString(kContiguousMigrations);

    AVERIFY(co_await mig.load(*db, u"contiguous"_s));
    AVERIFY(co_await mig.migrate(3));
    AVERIFY(co_await mig.migrate(0));

    auto versionResult = co_await db->exec(u"SELECT version FROM asql_migrations WHERE name = ?"_s,
                                           {u"contiguous"_s});
    AVERIFY(versionResult);
    ACOMPARE_EQ((*versionResult)[0][0].toInt(), 0);

    auto tablesResult = co_await db->exec(
        u"SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name LIKE 'mig_v%'"_s);
    AVERIFY(tablesResult);
    ACOMPARE_EQ((*tablesResult)[0][0].toInt(), 0);
}

ACoroTerminator runSparseMigrationDown(std::shared_ptr<QObject> finished)
{
    auto _ = qScopeGuard([finished] {});

    auto db = co_await APool::database(nullptr, u"migrations"_s);
    AVERIFY(db);

    AMigrations mig;
    mig.fromString(kSparseMigrations);

    AVERIFY(co_await mig.load(*db, u"sparse"_s));
    AVERIFY(co_await mig.migrate(10));
    AVERIFY(co_await mig.migrate(0));

    auto versionResult =
        co_await db->exec(u"SELECT version FROM asql_migrations WHERE name = ?"_s, {u"sparse"_s});
    AVERIFY(versionResult);
    ACOMPARE_EQ((*versionResult)[0][0].toInt(), 9);
}

ACoroTerminator runMigrationNameBinding(std::shared_ptr<QObject> finished)
{
    auto _ = qScopeGuard([finished] {});

    auto db = co_await APool::database(nullptr, u"migrations"_s);
    AVERIFY(db);

    AMigrations mig;
    mig.fromString(uR"(
-- 1 up
CREATE TABLE mig_name (id INTEGER);
-- 1 down
DROP TABLE mig_name;
)"_s);

    AVERIFY(co_await mig.load(*db, u"name'with'quote"_s));
    AVERIFY(co_await mig.migrate(1));

    auto versionResult = co_await db->exec(u"SELECT version FROM asql_migrations WHERE name = ?"_s,
                                           {u"name'with'quote"_s});
    AVERIFY(versionResult);
    ACOMPARE_EQ((*versionResult)[0][0].toInt(), 1);
}

void runCoroutineTest(const std::function<ACoroTerminator(std::shared_ptr<QObject>)> &fn)
{
    QEventLoop loop;
    {
        auto finished = std::make_shared<QObject>();
        QObject::connect(finished.get(), &QObject::destroyed, &loop, &QEventLoop::quit);
        fn(finished);
    }
    loop.exec();
}

} // namespace

class TestMigrations : public CoverageObject
{
    Q_OBJECT
public:
    void initTest() override;
    void cleanupTest() override;

private Q_SLOTS:
    void testFromStringParsing();
    void testMigrateUpAndDown();
    void testDownOneStepFromSparseVersions();
    void testMigrationNameBinding();
};

void TestMigrations::initTest()
{
    APool::create(ASqlite::factory(u"sqlite://?MEMORY"_s));
    APool::setMaxIdleConnections(5);
    APool::setMaxConnections(10);

    APool::create(ASqlite::factory(u"sqlite://?MEMORY"_s), u"migrations"_s);
    APool::setMaxIdleConnections(2, u"migrations"_s);
}

void TestMigrations::cleanupTest()
{
    APool::remove(u"migrations"_s);
    APool::remove();
}

void TestMigrations::testFromStringParsing()
{
    AMigrations mig;
    mig.fromString(kContiguousMigrations);

    QCOMPARE(mig.latest(), 3);

    const QStringList upSql = mig.sqlListFor(0, 3);
    QCOMPARE(upSql.size(), 3);
    QVERIFY(upSql.at(0).contains(u"mig_v1"_s));
    QVERIFY(upSql.at(2).contains(u"mig_v3"_s));

    const QStringList downSql = mig.sqlListFor(3, 0);
    QCOMPARE(downSql.size(), 3);
    QVERIFY(downSql.first().contains(u"mig_v3"_s));
    QVERIFY(downSql.last().contains(u"mig_v1"_s));
}

void TestMigrations::testMigrateUpAndDown()
{
    runCoroutineTest(runMigrationUpAndDown);
}

void TestMigrations::testDownOneStepFromSparseVersions()
{
    runCoroutineTest(runSparseMigrationDown);
}

void TestMigrations::testMigrationNameBinding()
{
    runCoroutineTest(runMigrationNameBinding);
}

QTEST_MAIN(TestMigrations)
#include "migrations_tst.moc"
