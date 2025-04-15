#include "CoverageObject.hpp"

#include <QTest>

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

#include "moc_CoverageObject.cpp"
