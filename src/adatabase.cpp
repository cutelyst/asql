/*
 * SPDX-FileCopyrightText: (C) 2020 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "adatabase.h"

#include "acoroexpected.h"
#include "adriver.h"
#include "adriverfactory.h"
#include "apreparedquery.h"
#include "atransaction.h"

#include <QLoggingCategory>

using namespace ASql;
using namespace Qt::StringLiterals;

ADatabase::ADatabase() = default;

ADatabase::ADatabase(std::shared_ptr<ADriver> driver)
    : d(std::move(driver))
{
}

ADatabase::ADatabase(const std::shared_ptr<ADriverFactory> &factory)
    : d(factory->createDriver())
{
}

ADatabase::ADatabase(const ADatabase &other)
    : d(other.d)
{
}

ADatabase::ADatabase(ADatabase &&other) noexcept
    : d(std::move(other.d))
{
}

ADatabase::~ADatabase() = default;

bool ADatabase::isValid() const
{
    return d && d->isValid();
}

QString ADatabase::driverName() const
{
    return d ? d->driverName() : u"INVALID_DRIVER"_s;
}

ADriver *ADatabase::driver() const
{
    return d.get();
}

AExpectedOpen ADatabase::coOpen(QObject *receiver)
{
    AExpectedOpen coro(receiver);
    if (!d) {
        d = std::make_shared<ADriver>();
    }

    if (d->state() == ADatabase::State::Connected) {
        coro.m_data->deliverOpen(true, {});
    } else if (d->state() == ADatabase::State::Disconnected ||
               d->state() == ADatabase::State::Connecting) {
        d->open(d, receiver, AOpenFn{std::weak_ptr<ACoroOpenData>{coro.m_data}});
    }
    return coro;
}

ADatabase::State ADatabase::state() const
{
    if (d) {
        return d->state();
    }
    return ADatabase::State::Disconnected;
}

bool ADatabase::isOpen() const
{
    Q_ASSERT(d);
    return d != nullptr && d->isOpen();
}

AExpectedResult beginHelper(const std::shared_ptr<ADriver> &d, QObject *receiver)
{
    AExpectedResult coro(receiver);
    d->begin(d, receiver, coro.ref());
    return coro;
}

AExpectedTransaction ADatabase::begin(QObject *receiver)
{
    Q_ASSERT(d);
    AExpectedTransaction coro(receiver);
    [](auto chainData, ADatabase db, QObject *receiver) -> ACoroTerminator {
        auto result = co_await beginHelper(db.d, receiver);
        if (result) {
            chainData->deliverDirect(ATransaction(db, true));
            co_return;
        }
        chainData->deliverDirect(std::unexpected(result.error()));
    }(coro.m_data, *this, receiver);
    return coro;
}

AExpectedResult ADatabase::commit(QObject *receiver)
{
    Q_ASSERT(d);
    AExpectedResult coro(receiver);
    d->commit(d, receiver, coro.ref());
    return coro;
}

AExpectedResult ADatabase::rollback(QObject *receiver)
{
    Q_ASSERT(d);
    AExpectedResult coro(receiver);
    d->rollback(d, receiver, coro.ref());
    return coro;
}

AExpectedResult ADatabase::exec(QStringView query, QObject *receiver)
{
    Q_ASSERT(d);
    AExpectedResult coro(receiver);
    d->exec(d, query, receiver, coro.ref());
    return coro;
}

AExpectedResult ADatabase::exec(QStringView query, const QVariantList &params, QObject *receiver)
{
    Q_ASSERT(d);
    AExpectedResult coro(receiver);
    d->exec(d, query, params, receiver, coro.ref());
    return coro;
}

AExpectedMultiResult ADatabase::execMulti(QStringView query, QObject *receiver)
{
    Q_ASSERT(d);
    AExpectedMultiResult coro(receiver);
    d->exec(d, query, receiver, coro.ref());
    return coro;
}

AExpectedMultiResult ADatabase::execMultiUtf8(QUtf8StringView query, QObject *receiver)
{
    Q_ASSERT(d);
    AExpectedMultiResult coro(receiver);
    d->exec(d, query, receiver, coro.ref());
    return coro;
}

AExpectedResult ADatabase::execUtf8(QUtf8StringView query, QObject *receiver)
{
    Q_ASSERT(d);
    AExpectedResult coro(receiver);
    d->exec(d, query, receiver, coro.ref());
    return coro;
}

AExpectedResult
    ADatabase::execUtf8(QUtf8StringView query, const QVariantList &params, QObject *receiver)
{
    Q_ASSERT(d);
    AExpectedResult coro(receiver);
    d->exec(d, query, params, receiver, coro.ref());
    return coro;
}

AExpectedResult ADatabase::exec(const APreparedQuery &query, QObject *receiver)
{
    Q_ASSERT(d);
    AExpectedResult coro(receiver);
    if (!query.isValid()) {
        coro.m_data->deliverDirect(std::unexpected(QStringLiteral("Invalid prepared query")));
        return coro;
    }
    d->exec(d, query, QVariantList(), receiver, coro.ref());
    return coro;
}

AExpectedResult
    ADatabase::exec(const APreparedQuery &query, const QVariantList &params, QObject *receiver)
{
    Q_ASSERT(d);
    AExpectedResult coro(receiver);
    if (!query.isValid()) {
        coro.m_data->deliverDirect(std::unexpected(QStringLiteral("Invalid prepared query")));
        return coro;
    }
    d->exec(d, query, params, receiver, coro.ref());
    return coro;
}

void ADatabase::setLastQuerySingleRowMode()
{
    Q_ASSERT(d);
    d->setLastQuerySingleRowMode();
}

bool ADatabase::enterPipelineMode(std::chrono::milliseconds timeout)
{
    Q_ASSERT(d);
    return d->enterPipelineMode(timeout);
}

bool ADatabase::exitPipelineMode()
{
    Q_ASSERT(d);
    return d->exitPipelineMode();
}

ADatabase::PipelineStatus ADatabase::pipelineStatus() const
{
    Q_ASSERT(d);
    return d->pipelineStatus();
}

bool ADatabase::pipelineSync()
{
    Q_ASSERT(d);
    return d->pipelineSync();
}

void ADatabase::subscribeToNotification(const QString &channel, QObject *receiver)
{
    Q_ASSERT(d);
    d->subscribeToNotification(d, channel, receiver);
}

QStringList ADatabase::subscribedToNotifications() const
{
    Q_ASSERT(d);
    return d->subscribedToNotifications();
}

void ADatabase::unsubscribeFromNotification(const QString &channel)
{
    Q_ASSERT(d);
    d->unsubscribeFromNotification(d, channel);
}

int ADatabase::queueSize() const
{
    Q_ASSERT(d);
    return d->queueSize();
}

ADatabase &ADatabase::operator=(const ADatabase &copy)
{
    d = copy.d;
    return *this;
}

#include "moc_adatabase.cpp"
