#ifndef SQLITE_TST_H
#define SQLITE_TST_H

#include "ASqlite.hpp"
#include "CoverageObject.hpp"
#include "acoroexpected.h"
#include "adatabase.h"
#include "apool.h"
#include "apreparedquery.h"

#include <QJsonObject>
#include <QObject>
#include <QStandardPaths>
#include <QTest>
#include <QUrl>

using namespace ASql;
using namespace Qt::Literals::StringLiterals;

class TestSqlite : public CoverageObject
{
    Q_OBJECT
public:
    void initTest() override;
    void cleanupTest() override;

private Q_SLOTS:
    void testQueries();
    void testPoolBeginCommit();
    void testPoolBeginRollback();
    void testDatabaseBeginCommit();
    void testDatabaseBeginRollback();
};

void TestSqlite::initTest()
{
    const QString tmpDb =
        QStandardPaths::writableLocation(QStandardPaths::TempLocation) + u"/tmp.db"_s;

    // Build a portable SQLite file URL.  On Windows, tmpDb starts with a
    // drive letter ("C:/...") so naive concatenation "sqlite://" + tmpDb
    // produces "sqlite://C:/..." where QUrl interprets "C" as the host.
    // SQLite then receives "file://C:/..." and opens the wrong path.
    // QUrl::fromLocalFile() always produces the correct three-slash form
    // ("file:///C:/...") regardless of platform.
    QUrl fileUrl = QUrl::fromLocalFile(tmpDb);
    fileUrl.setScheme(u"sqlite"_s); // "sqlite:///C:/..." or "sqlite:///tmp/..."
    const QString sqliteFileUrl = fileUrl.toString();

    APool::create(ASqlite::factory(u"sqlite://?MEMORY"_s));
    APool::setMaxIdleConnections(5);
    APool::setMaxConnections(10);

    APool::create(ASqlite::factory(sqliteFileUrl), u"file"_s);
    APool::setMaxIdleConnections(10, u"file");

    APool::create(ASqlite::factory(u"sqlite://?MEMORY"_s), u"pool"_s);
    APool::setMaxIdleConnections(5, u"pool"_s);
    APool::setMaxConnections(3, u"pool"_s);
}

void TestSqlite::cleanupTest()
{
    APool::remove();
    APool::remove(u"file"_s);
    APool::remove(u"pool"_s);
}

void TestSqlite::testQueries()
{

    QEventLoop loop;
    {
        // CP.51 this only works because qScopeGuard increases the finished ref count.
        auto finished = std::make_shared<QObject>();
        connect(finished.get(), &QObject::destroyed, &loop, &QEventLoop::quit);

        auto multipleQueries = [](std::shared_ptr<QObject> finished) -> ACoroTerminator {
            auto _ = qScopeGuard(
                [finished] { qDebug() << "multipleQueries exited" << finished.use_count(); });

            QStringList columns = {
                u"a"_s,
                u"b"_s,
                u"c"_s,
            };
            QByteArrayList queries = {
                "SELECT 'a' a, 1;"_ba,
                "SELECT 'b' b, 2;"_ba,
                "SELECT 'c' c, 3"_ba,
            };
            int count = 0;

            bool last = true;
            auto awaitable =
                APool::execMulti(u"SELECT 'a' a, 1; SELECT 'b' b, 2; SELECT 'c' c, 3"_s);
            do {
                auto result = co_await awaitable;
                AVERIFY(result);
                ++count;

                last = result->lastResultSet();

                auto column  = columns.takeFirst();
                QString col1 = result->columnNames()[0];
                QString col2 = result->columnNames()[1];
                ACOMPARE_EQ(column, col1);
                ACOMPARE_EQ(QString::number(count), col2);

                ACOMPARE_EQ((*result)[0][0].toString(), column);
                ACOMPARE_EQ((*result)[0][1].toInt(), count);

                auto query = queries.takeFirst();
                ACOMPARE_EQ(query, result->query());
            } while (!last);

            AVERIFY(columns.isEmpty());
        };
        multipleQueries(finished);

        auto multipleCreateQueries = [](std::shared_ptr<QObject> finished) -> ACoroTerminator {
            auto _ = qScopeGuard(
                [finished] { qDebug() << "multipleQueries exited" << finished.use_count(); });

            QByteArrayList queries = {
                "CREATE TABLE a (a TEXT);"_ba,
                "CREATE TABLE b (b TEXT);"_ba,
                "CREATE TABLE c (c TEXT)"_ba,
            };

            bool last      = true;
            auto awaitable = APool::execMulti(
                u"CREATE TABLE a (a TEXT);CREATE TABLE b (b TEXT);CREATE TABLE c (c TEXT)"_s);
            do {
                auto result = co_await awaitable;
                AVERIFY(result);

                last = result->lastResultSet();

                auto query = queries.takeFirst();
                ACOMPARE_EQ(query, result->query());
            } while (!last);

            AVERIFY(queries.isEmpty());
        };
        multipleCreateQueries(finished);

        auto multipleCreateQueriesTransaction =
            [](std::shared_ptr<QObject> finished) -> ACoroTerminator {
            auto _ = qScopeGuard(
                [finished] { qDebug() << "multipleQueries exited" << finished.use_count(); });

            auto db = co_await APool::database();
            AVERIFY(db);

            // TODO use APool::begin() once new coro class is ready
            auto t = co_await db->begin(nullptr);
            AVERIFY(t);
            AVERIFY(t->database().isValid());

            // This test checks if we do not crash by not consuming all results
            auto result = co_await t->database().execMulti(
                u"CREATE TABLE a (a TEXT);CREATE TABLE b (b TEXT);CREATE TABLE c (c TEXT)"_s);
            AVERIFY(result);
            ACOMPARE_EQ(result->lastResultSet(), false);
        };
        multipleCreateQueriesTransaction(finished);

        auto singleQuery = [](std::shared_ptr<QObject> finished) -> ACoroTerminator {
            auto _ = qScopeGuard(
                [finished] { qDebug() << "singleQuery exited" << finished.use_count(); });

            auto result = co_await APool::exec(u"SELECT 'a' a, 1"_s);
            AVERIFY(result);

            QString col1 = result->columnNames()[0];
            QString col2 = result->columnNames()[1];
            ACOMPARE_EQ(u"a"_s, col1);
            ACOMPARE_EQ(u"1"_s, col2);

            ACOMPARE_EQ((*result)[0][0].toString(), u"a"_s);
            ACOMPARE_EQ((*result)[0][1].toInt(), 1);
        };
        singleQuery(finished);

        auto queryParams = [](std::shared_ptr<QObject> finished) -> ACoroTerminator {
            auto _ = qScopeGuard(
                [finished] { qDebug() << "queryParams exited" << finished.use_count(); });

            auto result = co_await APool::exec(u"SELECT ?, ? second"_s,
                                               {
                                                   1,
                                                   true,
                                               });
            AVERIFY(result);
            AVERIFY(result->columnNames().size() == 2);

            QString col1 = result->columnNames()[0];
            QString col2 = result->columnNames()[1];
            ACOMPARE_EQ(u"?"_s, col1);
            ACOMPARE_EQ(u"second"_s, col2);

            ACOMPARE_EQ((*result)[0][0].toInt(), 1);
            ACOMPARE_EQ((*result)[0][1].toBool(), true);
        };
        queryParams(finished);

        auto queryPrepared = [](std::shared_ptr<QObject> finished) -> ACoroTerminator {
            auto _ = qScopeGuard(
                [finished] { qDebug() << "queryPrepared exited" << finished.use_count(); });

            auto db = co_await APool::database(); // Must be the same db
            AVERIFY(db);

            for (int i = 0; i < 5; ++i) {
                auto result = co_await db->exec(APreparedQueryLiteral(u"SELECT ?"_s),
                                                {
                                                    i,
                                                });
                AVERIFY(result);
                ACOMPARE_EQ((*result)[0][0].toInt(), i);
            }
        };
        queryPrepared(finished);

        auto rowsAffected = [](std::shared_ptr<QObject> finished) -> ACoroTerminator {
            auto _ = qScopeGuard(
                [finished] { qDebug() << "rowsAffected exited" << finished.use_count(); });

            auto db = co_await APool::database(); // Must be the same memory db
            AVERIFY(db);

            auto create = co_await db->exec(u"CREATE TABLE temp (name TEXT)");
            AVERIFY(create);
            ACOMPARE_EQ(create->numRowsAffected(), 0);

            auto result =
                co_await db->exec(u8"INSERT INTO temp (name) VALUES ('foo'),('bar'),('baz')");
            AVERIFY(result);
            ACOMPARE_EQ(result->numRowsAffected(), 3);

            result = co_await db->exec(u"INSERT INTO temp (name) VALUES (?),(?)"_s,
                                       {
                                           4,
                                           5,
                                       });
            AVERIFY(result);
            ACOMPARE_EQ(result->numRowsAffected(), 2);

            result =
                co_await db->exec(APreparedQueryLiteral(u8"INSERT INTO temp (name) VALUES (?)"),
                                  {
                                      6,
                                  });
            AVERIFY(result);
            ACOMPARE_EQ(result->numRowsAffected(), 1);

            result = co_await db->exec(u8"UPDATE temp SET name = null");
            AVERIFY(result);
            ACOMPARE_EQ(result->numRowsAffected(), 6);

            result = co_await db->exec(u8"DELETE FROM temp");
            AVERIFY(result);
            ACOMPARE_EQ(result->numRowsAffected(), 6);
        };
        rowsAffected(finished);

        auto cancelator = [](std::shared_ptr<QObject> finished) -> ACoroTerminator {
            auto _ = qScopeGuard(
                [finished] { qDebug() << "cancelator exited" << finished.use_count(); });

            auto cancelator = new QObject;
            cancelator->deleteLater();
            auto result = co_await APool::exec("SELECT 1", cancelator);
            Q_ASSERT_X(true, "cancelator", "Coroutine must not be called");
        };
        cancelator(finished);

        auto cancelatorYielded = [](std::shared_ptr<QObject> finished) -> ACoroTerminator {
            auto _ = qScopeGuard(
                [finished] { qDebug() << "cancelatorYielded exited" << finished.use_count(); });

            auto cancelator = new QObject;
            co_yield cancelator;

            cancelator->deleteLater();
            auto result = co_await APool::exec("SELECT 1", cancelator);
            Q_ASSERT_X(true, "cancelatorYielded", "Coroutine must not be called");
        };
        cancelatorYielded(finished);
    }

    loop.exec();
}

void TestSqlite::testPoolBeginCommit()
{
    QEventLoop loop;
    {
        auto finished = std::make_shared<QObject>();
        connect(finished.get(), &QObject::destroyed, &loop, &QEventLoop::quit);

        auto poolBeginCommit = [](std::shared_ptr<QObject> finished) -> ACoroTerminator {
            auto _ = qScopeGuard(
                [finished] { qDebug() << "poolBeginCommit exited" << finished.use_count(); });

            auto setupDb = co_await APool::database(nullptr, u"file"_s);
            AVERIFY(setupDb);
            auto create = co_await setupDb->exec(
                u"CREATE TABLE IF NOT EXISTS pool_commit_test (id INTEGER)"_s);
            AVERIFY(create);

            auto t = co_await APool::begin(nullptr, u"file"_s);
            AVERIFY(t);
            AVERIFY(t->isActive());

            auto insert = co_await t->database().exec(u"INSERT INTO pool_commit_test VALUES (1)"_s);
            AVERIFY(insert);
            ACOMPARE_EQ(insert->numRowsAffected(), qint64(1));

            auto commitResult = co_await t->commit();
            AVERIFY(commitResult);
            AVERIFY(!commitResult->hasError());

            auto verifyDb = co_await APool::database(nullptr, u"file"_s);
            AVERIFY(verifyDb);
            auto select = co_await verifyDb->exec(u"SELECT COUNT(*) FROM pool_commit_test"_s);
            AVERIFY(select);
            ACOMPARE_EQ((*select)[0][0].toInt(), 1);

            co_await verifyDb->exec(u"DROP TABLE pool_commit_test"_s);
        };
        poolBeginCommit(finished);
    }
    loop.exec();
}

void TestSqlite::testPoolBeginRollback()
{
    QEventLoop loop;
    {
        auto finished = std::make_shared<QObject>();
        connect(finished.get(), &QObject::destroyed, &loop, &QEventLoop::quit);

        auto poolBeginRollback = [](std::shared_ptr<QObject> finished) -> ACoroTerminator {
            auto _ = qScopeGuard(
                [finished] { qDebug() << "poolBeginRollback exited" << finished.use_count(); });

            auto setupDb = co_await APool::database(nullptr, u"file"_s);
            AVERIFY(setupDb);
            auto create = co_await setupDb->exec(
                u"CREATE TABLE IF NOT EXISTS pool_rollback_test (id INTEGER)"_s);
            AVERIFY(create);

            auto t = co_await APool::begin(nullptr, u"file"_s);
            AVERIFY(t);
            AVERIFY(t->isActive());

            auto insert =
                co_await t->database().exec(u"INSERT INTO pool_rollback_test VALUES (42)"_s);
            AVERIFY(insert);
            ACOMPARE_EQ(insert->numRowsAffected(), qint64(1));

            auto rollbackResult = co_await t->rollback();
            AVERIFY(rollbackResult);
            AVERIFY(!rollbackResult->hasError());

            auto verifyDb = co_await APool::database(nullptr, u"file"_s);
            AVERIFY(verifyDb);
            auto select = co_await verifyDb->exec(u"SELECT COUNT(*) FROM pool_rollback_test"_s);
            AVERIFY(select);
            ACOMPARE_EQ((*select)[0][0].toInt(), 0);

            co_await verifyDb->exec(u"DROP TABLE pool_rollback_test"_s);
        };
        poolBeginRollback(finished);
    }
    loop.exec();
}

void TestSqlite::testDatabaseBeginCommit()
{
    QEventLoop loop;
    {
        auto finished = std::make_shared<QObject>();
        connect(finished.get(), &QObject::destroyed, &loop, &QEventLoop::quit);

        auto dbBeginCommit = [](std::shared_ptr<QObject> finished) -> ACoroTerminator {
            auto _ = qScopeGuard(
                [finished] { qDebug() << "dbBeginCommit exited" << finished.use_count(); });

            auto db = co_await APool::database(nullptr, u"file"_s);
            AVERIFY(db);

            auto create =
                co_await db->exec(u"CREATE TABLE IF NOT EXISTS db_commit_test (id INTEGER)"_s);
            AVERIFY(create);

            auto t = co_await db->begin();
            AVERIFY(t);
            AVERIFY(t->isActive());

            auto insert = co_await t->database().exec(u"INSERT INTO db_commit_test VALUES (1)"_s);
            AVERIFY(insert);
            ACOMPARE_EQ(insert->numRowsAffected(), qint64(1));

            auto commitResult = co_await t->commit();
            AVERIFY(commitResult);
            AVERIFY(!commitResult->hasError());

            auto verifyDb = co_await APool::database(nullptr, u"file"_s);
            AVERIFY(verifyDb);
            auto select = co_await verifyDb->exec(u"SELECT COUNT(*) FROM db_commit_test"_s);
            AVERIFY(select);
            ACOMPARE_EQ((*select)[0][0].toInt(), 1);

            co_await verifyDb->exec(u"DROP TABLE db_commit_test"_s);
        };
        dbBeginCommit(finished);
    }
    loop.exec();
}

void TestSqlite::testDatabaseBeginRollback()
{
    QEventLoop loop;
    {
        auto finished = std::make_shared<QObject>();
        connect(finished.get(), &QObject::destroyed, &loop, &QEventLoop::quit);

        auto dbBeginRollback = [](std::shared_ptr<QObject> finished) -> ACoroTerminator {
            auto _ = qScopeGuard(
                [finished] { qDebug() << "dbBeginRollback exited" << finished.use_count(); });

            auto db = co_await APool::database(nullptr, u"file"_s);
            AVERIFY(db);

            auto create =
                co_await db->exec(u"CREATE TABLE IF NOT EXISTS db_rollback_test (id INTEGER)"_s);
            AVERIFY(create);

            auto t = co_await db->begin();
            AVERIFY(t);
            AVERIFY(t->isActive());

            auto insert =
                co_await t->database().exec(u"INSERT INTO db_rollback_test VALUES (42)"_s);
            AVERIFY(insert);
            ACOMPARE_EQ(insert->numRowsAffected(), qint64(1));

            auto rollbackResult = co_await t->rollback();
            AVERIFY(rollbackResult);
            AVERIFY(!rollbackResult->hasError());

            auto verifyDb = co_await APool::database(nullptr, u"file"_s);
            AVERIFY(verifyDb);
            auto select = co_await verifyDb->exec(u"SELECT COUNT(*) FROM db_rollback_test"_s);
            AVERIFY(select);
            ACOMPARE_EQ((*select)[0][0].toInt(), 0);

            co_await verifyDb->exec(u"DROP TABLE db_rollback_test"_s);
        };
        dbBeginRollback(finished);
    }
    loop.exec();
}

QTEST_MAIN(TestSqlite)
#include "sqlite_tst.moc"

#endif // SQLITE_TST_H
