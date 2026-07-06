#ifndef CACHE_TST_H
#    define CACHE_TST_H

#    include "ASqlite.hpp"
#    include "CoverageObject.hpp"
#    include "acache.h"
#    include "acoroexpected.h"
#    include "adatabase.h"
#    include "apool.h"

#    include <QObject>
#    include <QScopeGuard>
#    include <QTest>

using namespace ASql;
using namespace Qt::Literals::StringLiterals;

class TestCache : public CoverageObject
{
    Q_OBJECT
public:
    void initTest() override;
    void cleanupTest() override;

private Q_SLOTS:
    void testCacheHitAndClear();
    void testCacheConcurrentWaiters();
    void testCachePoolFailure();
    void testCacheDoesNotServeErrors();
    void testCacheDifferentArgs();
};

void TestCache::initTest()
{
    APool::create(ASqlite::factory(u"sqlite://?MEMORY"_s));
    APool::setMaxIdleConnections(5);
    APool::setMaxConnections(10);

    APool::create(ASqlite::factory(u"sqlite://?MEMORY"_s), u"cache"_s);
    APool::setMaxIdleConnections(2, u"cache"_s);
}

void TestCache::cleanupTest()
{
    APool::remove(u"cache"_s);
    APool::remove();
}

void TestCache::testCacheHitAndClear()
{
    QEventLoop loop;
    {
        auto finished = std::make_shared<QObject>();
        connect(finished.get(), &QObject::destroyed, &loop, &QEventLoop::quit);

        auto run = [](std::shared_ptr<QObject> finished) -> ACoroTerminator {
            auto _ = qScopeGuard([finished] {});

            auto db = co_await APool::database(nullptr, u"cache"_s);
            AVERIFY(db);
            co_await db->exec(u"CREATE TABLE cache_hit (v INTEGER)"_s);
            co_await db->exec(u"INSERT INTO cache_hit VALUES (42)"_s);

            ACache cache;
            cache.setDatabase(*db);
            ACOMPARE_EQ(cache.size(), 0);

            const QString query = u"SELECT v FROM cache_hit"_s;
            auto first          = co_await cache.exec(query);
            AVERIFY(first);
            ACOMPARE_EQ((*first)[0][0].toInt(), 42);
            ACOMPARE_EQ(cache.size(), 1);

            co_await db->exec(u"UPDATE cache_hit SET v = 99"_s);

            auto cached = co_await cache.exec(query);
            AVERIFY(cached);
            ACOMPARE_EQ((*cached)[0][0].toInt(), 42);

            AVERIFY(cache.clear(query));
            ACOMPARE_EQ(cache.size(), 0);

            auto fresh = co_await cache.exec(query);
            AVERIFY(fresh);
            ACOMPARE_EQ((*fresh)[0][0].toInt(), 99);
        };
        run(finished);
    }
    loop.exec();
}

void TestCache::testCacheConcurrentWaiters()
{
    QEventLoop loop;
    {
        auto finished = std::make_shared<QObject>();
        connect(finished.get(), &QObject::destroyed, &loop, &QEventLoop::quit);

        auto run = [](std::shared_ptr<QObject> finished) -> ACoroTerminator {
            auto _ = qScopeGuard([finished] {});

            auto db = co_await APool::database(nullptr, u"cache"_s);
            AVERIFY(db);
            co_await db->exec(u"CREATE TABLE cache_wait (v INTEGER)"_s);
            co_await db->exec(u"INSERT INTO cache_wait VALUES (7)"_s);

            ACache cache;
            cache.setDatabase(*db);

            const QString query = u"SELECT v FROM cache_wait"_s;
            auto pending1       = cache.exec(query);
            auto pending2       = cache.exec(query);

            auto first = co_await pending1;
            AVERIFY(first);
            ACOMPARE_EQ((*first)[0][0].toInt(), 7);

            auto second = co_await pending2;
            AVERIFY(second);
            ACOMPARE_EQ((*second)[0][0].toInt(), 7);
            ACOMPARE_EQ(cache.size(), 1);
        };
        run(finished);
    }
    loop.exec();
}

void TestCache::testCachePoolFailure()
{
    QEventLoop loop;
    {
        auto finished = std::make_shared<QObject>();
        connect(finished.get(), &QObject::destroyed, &loop, &QEventLoop::quit);

        auto run = [](std::shared_ptr<QObject> finished) -> ACoroTerminator {
            auto _ = qScopeGuard([finished] {});

            ACache cache;
            cache.setDatabasePool(u"missing_pool"_s);

            auto result = co_await cache.exec(u"SELECT 1"_s);
            AVERIFY(!result);
            AVERIFY(!result.error().isEmpty());
            ACOMPARE_EQ(cache.size(), 0);
        };
        run(finished);
    }
    loop.exec();
}

void TestCache::testCacheDoesNotServeErrors()
{
    QEventLoop loop;
    {
        auto finished = std::make_shared<QObject>();
        connect(finished.get(), &QObject::destroyed, &loop, &QEventLoop::quit);

        auto run = [](std::shared_ptr<QObject> finished) -> ACoroTerminator {
            auto _ = qScopeGuard([finished] {});

            auto db = co_await APool::database(nullptr, u"cache"_s);
            AVERIFY(db);

            ACache cache;
            cache.setDatabase(*db);

            const QString query = u"SELECT v FROM cache_err_missing"_s;
            auto failed         = co_await cache.exec(query);
            AVERIFY(!failed);
            ACOMPARE_EQ(cache.size(), 0);

            co_await db->exec(u"CREATE TABLE cache_err_missing (v INTEGER)"_s);
            co_await db->exec(u"INSERT INTO cache_err_missing VALUES (3)"_s);

            auto recovered = co_await cache.exec(query);
            AVERIFY(recovered);
            ACOMPARE_EQ((*recovered)[0][0].toInt(), 3);
        };
        run(finished);
    }
    loop.exec();
}

void TestCache::testCacheDifferentArgs()
{
    QEventLoop loop;
    {
        auto finished = std::make_shared<QObject>();
        connect(finished.get(), &QObject::destroyed, &loop, &QEventLoop::quit);

        auto run = [](std::shared_ptr<QObject> finished) -> ACoroTerminator {
            auto _ = qScopeGuard([finished] {});

            auto db = co_await APool::database(nullptr, u"cache"_s);
            AVERIFY(db);

            ACache cache;
            cache.setDatabase(*db);

            const QString query = u"SELECT ?"_s;
            auto one            = co_await cache.exec(query, {1});
            AVERIFY(one);
            ACOMPARE_EQ((*one)[0][0].toInt(), 1);

            auto two = co_await cache.exec(query, {2});
            AVERIFY(two);
            ACOMPARE_EQ((*two)[0][0].toInt(), 2);

            ACOMPARE_EQ(cache.size(), 2);

            auto oneAgain = co_await cache.exec(query, {1});
            AVERIFY(oneAgain);
            ACOMPARE_EQ((*oneAgain)[0][0].toInt(), 1);
        };
        run(finished);
    }
    loop.exec();
}

#endif // CACHE_TST_H

QTEST_MAIN(TestCache)
#include "cache_tst.moc"
