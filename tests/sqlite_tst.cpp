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
};

void TestSqlite::initTest()
{
    const QString tmpDb =
        QStandardPaths::writableLocation(QStandardPaths::TempLocation) + u"/tmp.db"_s;

    APool::create(ASqlite::factory(u"sqlite://?MEMORY"_s));
    APool::setMaxIdleConnections(5);
    APool::setMaxConnections(10);

    APool::create(ASqlite::factory(u"sqlite://"_s + tmpDb), u"file"_s);
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
        auto finished = std::make_shared<QObject>();
        connect(finished.get(), &QObject::destroyed, &loop, [&] {
            qDebug("testQueries finished");
            loop.quit();
        });

        auto multipleQueries = [finished]() -> ACoroTerminator {
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

            bool last      = true;
            auto awaitable = APool::exec(u"SELECT 'a' a, 1; SELECT 'b' b, 2; SELECT 'c' c, 3"_s);
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
        multipleQueries();

        auto multipleCreateQueries = [finished]() -> ACoroTerminator {
            auto _ = qScopeGuard(
                [finished] { qDebug() << "multipleCreateQueries exited" << finished.use_count(); });

            QByteArrayList queries = {
                "CREATE TABLE a (a TEXT);"_ba,
                "CREATE TABLE b (b TEXT);"_ba,
                "CREATE TABLE c (c TEXT)"_ba,
            };

            bool last      = true;
            auto awaitable = APool::exec(
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
        multipleCreateQueries();

        // FIXME THIS test crash because the driver holds
        // a callback to the awaitable that is going out of scope
        // This is not a problem for single queries as long as we
        // co_await the awaitable object.

        // auto multipleCreateQueriesTransaction = [finished]() -> ACoroTerminator {
        //     auto _ = qScopeGuard(
        //         [finished] { qDebug() << "multipleCreateQueriesTransaction exited" << finished.use_count(); });

        //     auto t = co_await APool::begin(nullptr);
        //     AVERIFY(t);

        //     // This test checks if we do not crash by not consuming all results
        //     auto result = co_await t->database().coExec(
        //         u"CREATE TABLE a (a TEXT);CREATE TABLE b (b TEXT);CREATE TABLE c (c TEXT)"_s);
        //     qDebug() << "multipleCreateQueriesTransaction result.error" << result.has_value();
        //     AVERIFY(result);
        //     ACOMPARE_EQ(result->lastResultSet(), false);
        // };
        // multipleCreateQueriesTransaction();

        auto singleQuery = [finished]() -> ACoroTerminator {
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
        singleQuery();

        auto queryParams = [finished]() -> ACoroTerminator {
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
        queryParams();

        auto queryPrepared = [finished]() -> ACoroTerminator {
            auto _ = qScopeGuard(
                [finished] { qDebug() << "queryPrepared exited" << finished.use_count(); });

            auto db = co_await APool::coDatabase(); // Must be the same db
            AVERIFY(db);

            for (int i = 0; i < 5; ++i) {
                auto result = co_await db->coExec(APreparedQueryLiteral(u"SELECT ?"_s),
                                                  {
                                                      i,
                                                  });
                AVERIFY(result);
                ACOMPARE_EQ((*result)[0][0].toInt(), i);
            }
        };
        queryPrepared();

        auto rowsAffected = [finished]() -> ACoroTerminator {
            auto _ = qScopeGuard(
                [finished] { qDebug() << "rowsAffected exited" << finished.use_count(); });

            auto db = co_await APool::coDatabase(); // Must be the same memory db
            AVERIFY(db);

            auto create = co_await db->coExec(u"CREATE TABLE temp (name TEXT)");
            AVERIFY(create);
            ACOMPARE_EQ(create->numRowsAffected(), 0);

            auto result =
                co_await db->coExec(u8"INSERT INTO temp (name) VALUES ('foo'),('bar'),('baz')");
            AVERIFY(result);
            ACOMPARE_EQ(result->numRowsAffected(), 3);

            result = co_await db->coExec(u"INSERT INTO temp (name) VALUES (?),(?)"_s,
                                         {
                                             4,
                                             5,
                                         });
            AVERIFY(result);
            ACOMPARE_EQ(result->numRowsAffected(), 2);

            result =
                co_await db->coExec(APreparedQueryLiteral(u8"INSERT INTO temp (name) VALUES (?)"),
                                    {
                                        6,
                                    });
            AVERIFY(result);
            ACOMPARE_EQ(result->numRowsAffected(), 1);

            result = co_await db->coExec(u8"UPDATE temp SET name = null");
            AVERIFY(result);
            ACOMPARE_EQ(result->numRowsAffected(), 6);

            result = co_await db->coExec(u8"DELETE FROM temp");
            AVERIFY(result);
            ACOMPARE_EQ(result->numRowsAffected(), 6);
        };
        rowsAffected();
    }

    loop.exec();
}

QTEST_MAIN(TestSqlite)
#include "sqlite_tst.moc"

#endif // SQLITE_TST_H
