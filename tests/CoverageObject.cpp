#include "CoverageObject.hpp"

#include "acoroexpected.h"
#include "apool.h"

#include <QTest>

using namespace ASql;

CoverageObject::CoverageObject(QObject *parent)
    : QObject(parent)
{
}

void CoverageObject::initTest()
{
}

void CoverageObject::cleanupTest()
{
}

void CoverageObject::init()
{
    initTest();
}

QString CoverageObject::generateTestName() const
{
    QString test_name;
    test_name += QString::fromLatin1(metaObject()->className());
    test_name += QLatin1Char('/');
    test_name += QString::fromLatin1(QTest::currentTestFunction());
    if (QTest::currentDataTag()) {
        test_name += QLatin1Char('/');
        test_name += QString::fromLatin1(QTest::currentDataTag());
    }
    return test_name;
}

void CoverageObject::saveCoverageData()
{
#ifdef __COVERAGESCANNER__
    QString test_name;
    test_name += generateTestName();

    __coveragescanner_testname(test_name.toStdString().c_str());
    if (QTest::currentTestFailed()) {
        __coveragescanner_teststate("FAILED");
    } else {
        __coveragescanner_teststate("PASSED");
    }
    __coveragescanner_save();
    __coveragescanner_testname("");
    __coveragescanner_clear();
#endif
}

void CoverageObject::cleanup()
{
    cleanupTest();
    saveCoverageData();
}

void CoverageObject::testPool()
{
    APool::setMaxConnections(2);

    QEventLoop loop;
    {
        auto finished = std::make_shared<QObject>();
        connect(finished.get(), &QObject::destroyed, &loop, &QEventLoop::quit);

        auto testPool = [finished]() -> ACoroTerminator {
            auto _ = qScopeGuard(
                [finished] { qDebug() << "rowsAffected exited" << finished.use_count(); });

            {
                auto db1 = co_await APool::coDatabase();
                AVERIFY(db1);
                AVERIFY(db1->isOpen());
                ACOMPARE_EQ(APool::currentConnections(), 1);

                auto db2 = co_await APool::coDatabase();
                AVERIFY(db2);
                ACOMPARE_EQ(APool::currentConnections(), 2);
            }

            auto db3 = co_await APool::coDatabase();
            AVERIFY(db3);
            ACOMPARE_EQ(APool::currentConnections(), 2);

            auto db4 = co_await APool::coDatabase();
            AVERIFY(db4);
            ACOMPARE_EQ(APool::currentConnections(), 2);
        };
        testPool();
    }
    loop.exec();
}

#include "moc_CoverageObject.cpp"
