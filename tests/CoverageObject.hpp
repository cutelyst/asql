#pragma once

#include <QBuffer>
#include <QObject>
#include <QTest>

using namespace Qt::StringLiterals;

class CoverageObject : public QObject
{
    Q_OBJECT
public:
    explicit CoverageObject(QObject *parent = nullptr);
    virtual void initTest();
    virtual void cleanupTest();
protected Q_SLOTS:
    void init();
    void cleanup();

private:
    void saveCoverageData();
    QString generateTestName() const;
};

#define ACOMPARE_OP_IMPL(lhs, rhs, op, opId) \
    do { \
        if (![](auto &&qt_lhs_arg, auto &&qt_rhs_arg) { \
            /* assumes that op does not actually move from qt_{lhs, rhs}_arg */ \
            return QTest::reportResult(std::forward<decltype(qt_lhs_arg)>(qt_lhs_arg) \
                                           op std::forward<decltype(qt_rhs_arg)>(qt_rhs_arg), \
                                       [&qt_lhs_arg] { return QTest::toString(qt_lhs_arg); }, \
                                       [&qt_rhs_arg] { return QTest::toString(qt_rhs_arg); }, \
                                       #lhs, \
                                       #rhs, \
                                       QTest::ComparisonOperation::opId, \
                                       __FILE__, \
                                       __LINE__); \
        }(lhs, rhs)) { \
            co_return; \
        } \
    } while (false)

#define ACOMPARE_EQ(lhs, rhs) ACOMPARE_OP_IMPL(lhs, rhs, ==, Equal)
#define ACOMPARE_NE(lhs, rhs) ACOMPARE_OP_IMPL(lhs, rhs, !=, NotEqual)
#define ACOMPARE_LT(lhs, rhs) ACOMPARE_OP_IMPL(lhs, rhs, <, LessThan)
#define ACOMPARE_LE(lhs, rhs) ACOMPARE_OP_IMPL(lhs, rhs, <=, LessThanOrEqual)
#define ACOMPARE_GT(lhs, rhs) ACOMPARE_OP_IMPL(lhs, rhs, >, GreaterThan)
#define ACOMPARE_GE(lhs, rhs) ACOMPARE_OP_IMPL(lhs, rhs, >=, GreaterThanOrEqual)

#define AVERIFY(statement) \
    do { \
        if (!QTest::qVerify(static_cast<bool>(statement), #statement, "", __FILE__, __LINE__)) \
            co_return; \
    } while (false)
