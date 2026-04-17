/*
 * SPDX-FileCopyrightText: (C) 2025 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */
#include "tst_types_common.h"

#include "acoroexpected.h"
#include "apool.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>
#include <QUuid>

using namespace ASql;
using namespace Qt::Literals::StringLiterals;

TestTypesBase::TestTypesBase(QObject *parent)
    : CoverageObject(parent)
{
}

QString TestTypesBase::selectParam() const
{
    return u"SELECT ?"_s;
}

// ─── test slots ───────────────────────────────────────────────────────────────

void TestTypesBase::testBool()
{
    for (bool sent : {true, false}) {
        QEventLoop loop;
        {
            auto finished = std::make_shared<QObject>();
            connect(finished.get(), &QObject::destroyed, &loop, &QEventLoop::quit);
            const bool capturedSent = sent;
            [this, finished, capturedSent]() -> ACoroTerminator {
                auto _          = qScopeGuard([finished] {});
                const QString q = selectParam();
                auto result     = co_await APool::exec(q, {capturedSent});
                AVERIFY(result);
                AVERIFY(result->size() == 1);
                ACOMPARE_EQ((*result)[0][0].toBool(), capturedSent);
            }();
        }
        loop.exec();
    }
}

void TestTypesBase::testInt()
{
    for (int sent : {0, 42, -1, std::numeric_limits<int>::max()}) {
        QEventLoop loop;
        {
            auto finished = std::make_shared<QObject>();
            connect(finished.get(), &QObject::destroyed, &loop, &QEventLoop::quit);
            const int capturedSent = sent;
            [this, finished, capturedSent]() -> ACoroTerminator {
                auto _          = qScopeGuard([finished] {});
                const QString q = selectParam();
                auto result     = co_await APool::exec(q, {capturedSent});
                AVERIFY(result);
                AVERIFY(result->size() == 1);
                ACOMPARE_EQ((*result)[0][0].toInt(), capturedSent);
            }();
        }
        loop.exec();
    }
}

void TestTypesBase::testLongLong()
{
    const QList<qint64> values{Q_INT64_C(0),
                               Q_INT64_C(9876543210),
                               Q_INT64_C(-9876543210),
                               std::numeric_limits<qint64>::max()};
    for (qint64 sent : values) {
        QEventLoop loop;
        {
            auto finished = std::make_shared<QObject>();
            connect(finished.get(), &QObject::destroyed, &loop, &QEventLoop::quit);
            const qint64 capturedSent = sent;
            [this, finished, capturedSent]() -> ACoroTerminator {
                auto _          = qScopeGuard([finished] {});
                const QString q = selectParam();
                auto result     = co_await APool::exec(q, {capturedSent});
                AVERIFY(result);
                AVERIFY(result->size() == 1);
                ACOMPARE_EQ((*result)[0][0].toLongLong(), capturedSent);
            }();
        }
        loop.exec();
    }
}

void TestTypesBase::testDouble()
{
    // Use values exactly representable in IEEE 754 double.
    for (double sent : {0.0, 1234567.5, -9876543.25}) {
        QEventLoop loop;
        {
            auto finished = std::make_shared<QObject>();
            connect(finished.get(), &QObject::destroyed, &loop, &QEventLoop::quit);
            const double capturedSent = sent;
            [this, finished, capturedSent]() -> ACoroTerminator {
                auto _          = qScopeGuard([finished] {});
                const QString q = selectParam();
                auto result     = co_await APool::exec(q, {capturedSent});
                AVERIFY(result);
                AVERIFY(result->size() == 1);
                ACOMPARE_EQ((*result)[0][0].toDouble(), capturedSent);
            }();
        }
        loop.exec();
    }
}

void TestTypesBase::testString()
{
    const QStringList values{
        u""_s,
        u"Hello, World!"_s,
        u"Olá, 世界! 🌍"_s,
        u"tab\there\nnewline"_s,
        u"single'quote double\"quote"_s,
    };
    for (const QString &sent : values) {
        QEventLoop loop;
        {
            auto finished = std::make_shared<QObject>();
            connect(finished.get(), &QObject::destroyed, &loop, &QEventLoop::quit);
            const QString capturedSent = sent;
            [this, finished, capturedSent]() -> ACoroTerminator {
                auto _          = qScopeGuard([finished] {});
                const QString q = selectParam();
                auto result     = co_await APool::exec(q, {capturedSent});
                AVERIFY(result);
                AVERIFY(result->size() == 1);
                ACOMPARE_EQ((*result)[0][0].toString(), capturedSent);
            }();
        }
        loop.exec();
    }
}

void TestTypesBase::testByteArray()
{
    // Include non-ASCII bytes and a null byte to verify true binary storage.
    const QList<QByteArray> values{
        QByteArray{},
        QByteArray("\x01\x00\x02\xfe\xff", 5),
        QByteArray("plain ASCII text"),
    };
    for (const QByteArray &sent : values) {
        QEventLoop loop;
        {
            auto finished = std::make_shared<QObject>();
            connect(finished.get(), &QObject::destroyed, &loop, &QEventLoop::quit);
            const QByteArray capturedSent = sent;
            [this, finished, capturedSent]() -> ACoroTerminator {
                auto _          = qScopeGuard([finished] {});
                const QString q = selectParam();
                auto result     = co_await APool::exec(q, {capturedSent});
                AVERIFY(result);
                AVERIFY(result->size() == 1);
                ACOMPARE_EQ((*result)[0][0].toByteArray(), capturedSent);
            }();
        }
        loop.exec();
    }
}

void TestTypesBase::testUuid()
{
    const QUuid sent = QUuid::fromString(u"550e8400-e29b-41d4-a716-446655440000"_s);
    QEventLoop loop;
    {
        auto finished = std::make_shared<QObject>();
        connect(finished.get(), &QObject::destroyed, &loop, &QEventLoop::quit);
        [this, finished, sent]() -> ACoroTerminator {
            auto _          = qScopeGuard([finished] {});
            const QString q = selectParam();
            auto result     = co_await APool::exec(q, {sent});
            AVERIFY(result);
            AVERIFY(result->size() == 1);
            ACOMPARE_EQ((*result)[0][0].toUuid(), sent);
        }();
    }
    loop.exec();
}

void TestTypesBase::testDateTime()
{
    // Use a fixed wall-clock time; compare date and time components separately
    // to avoid QDateTime::operator== failing on mismatched Qt::TimeSpec between drivers.
    const QDate sentDate(2023, 6, 15);
    const QTime sentTime(14, 30, 45, 123);
    const QDateTime sent(sentDate, sentTime, Qt::LocalTime);
    QEventLoop loop;
    {
        auto finished = std::make_shared<QObject>();
        connect(finished.get(), &QObject::destroyed, &loop, &QEventLoop::quit);
        [this, finished, sent, sentDate, sentTime]() -> ACoroTerminator {
            auto _          = qScopeGuard([finished] {});
            const QString q = selectParam();
            auto result     = co_await APool::exec(q, {sent});
            AVERIFY(result);
            AVERIFY(result->size() == 1);
            const QDateTime got = (*result)[0][0].toDateTime();
            ACOMPARE_EQ(got.date(), sentDate);
            ACOMPARE_EQ(got.time(), sentTime);
        }();
    }
    loop.exec();
}

void TestTypesBase::testDate()
{
    const QDate sent(2023, 6, 15);
    QEventLoop loop;
    {
        auto finished = std::make_shared<QObject>();
        connect(finished.get(), &QObject::destroyed, &loop, &QEventLoop::quit);
        [this, finished, sent]() -> ACoroTerminator {
            auto _          = qScopeGuard([finished] {});
            const QString q = selectParam();
            auto result     = co_await APool::exec(q, {sent});
            AVERIFY(result);
            AVERIFY(result->size() == 1);
            ACOMPARE_EQ((*result)[0][0].toDate(), sent);
        }();
    }
    loop.exec();
}

void TestTypesBase::testTime()
{
    const QTime sent(14, 30, 45, 123);
    QEventLoop loop;
    {
        auto finished = std::make_shared<QObject>();
        connect(finished.get(), &QObject::destroyed, &loop, &QEventLoop::quit);
        [this, finished, sent]() -> ACoroTerminator {
            auto _          = qScopeGuard([finished] {});
            const QString q = selectParam();
            auto result     = co_await APool::exec(q, {sent});
            AVERIFY(result);
            AVERIFY(result->size() == 1);
            ACOMPARE_EQ((*result)[0][0].toTime(), sent);
        }();
    }
    loop.exec();
}

void TestTypesBase::testJson()
{
    const QJsonObject sentObj{{u"key"_s, u"value"_s}, {u"num"_s, 42}, {u"flag"_s, true}};
    // Send as a compact JSON string — drivers store it as text, toJsonValue() re-parses it.
    const QString sentStr =
        QString::fromUtf8(QJsonDocument(sentObj).toJson(QJsonDocument::Compact));
    QEventLoop loop;
    {
        auto finished = std::make_shared<QObject>();
        connect(finished.get(), &QObject::destroyed, &loop, &QEventLoop::quit);
        [this, finished, sentStr, sentObj]() -> ACoroTerminator {
            auto _          = qScopeGuard([finished] {});
            const QString q = selectParam();
            auto result     = co_await APool::exec(q, {sentStr});
            AVERIFY(result);
            AVERIFY(result->size() == 1);
            const QJsonValue got = (*result)[0][0].toJsonValue();
            AVERIFY(got.isObject());
            ACOMPARE_EQ(got.toObject(), sentObj);
        }();
    }
    loop.exec();
}

void TestTypesBase::testNull()
{
    QEventLoop loop;
    {
        auto finished = std::make_shared<QObject>();
        connect(finished.get(), &QObject::destroyed, &loop, &QEventLoop::quit);
        [this, finished]() -> ACoroTerminator {
            auto _          = qScopeGuard([finished] {});
            const QString q = selectParam();
            auto result     = co_await APool::exec(q, {QVariant{}});
            AVERIFY(result);
            AVERIFY(result->size() == 1);
            AVERIFY((*result)[0][0].isNull());
        }();
    }
    loop.exec();
}

#include "moc_tst_types_common.cpp"
