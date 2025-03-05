/*
 * SPDX-FileCopyrightText: (C) 2020 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "adatabase.h"

#include "acoroexpected.h"
#include "adriver.h"
#include "adriverfactory.h"
#include "atransaction.h"

#include <QLoggingCategory>

using namespace ASql;

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

bool ADatabase::isValid()
{
    return d && d->isValid();
}

void ADatabase::open(QObject *receiver, std::function<void(bool error, const QString &fff)> cb)
{
    if (!d) {
        d = std::make_shared<ADriver>();
    }

    if (d->state() == ADatabase::State::Disconnected) {
        d->open(receiver, cb);
    }
}

ADatabase::State ADatabase::state() const
{
    if (d) {
        return d->state();
    }
    return ADatabase::State::Disconnected;
}

void ADatabase::onStateChanged(QObject *receiver,
                               std::function<void(ADatabase::State, const QString &)> cb)
{
    Q_ASSERT(d);
    d->onStateChanged(receiver, cb);
}

bool ADatabase::isOpen() const
{
    Q_ASSERT(d);
    return d && d->isOpen();
}

void ADatabase::begin(QObject *receiver, AResultFn cb)
{
    Q_ASSERT(d);
    d->begin(d, receiver, cb);
}

AExpectedTransaction ADatabase::coBegin(QObject *receiver)
{
    Q_ASSERT(d);
    AExpectedTransaction coro(receiver);
    coro.database = d;
    d->begin(d, receiver, coro.callback);
    return coro;
}

void ADatabase::commit(QObject *receiver, AResultFn cb)
{
    Q_ASSERT(d);
    d->commit(d, receiver, cb);
}

void ADatabase::rollback(QObject *receiver, AResultFn cb)
{
    Q_ASSERT(d);
    d->rollback(d, receiver, cb);
}

AExpectedResult ADatabase::coExec(QStringView query, QObject *receiver)
{
    Q_ASSERT(d);
    AExpectedResult coro(receiver);
    d->exec(d, query, QVariantList(), receiver, coro.callback);
    return coro;
}

AExpectedResult ADatabase::coExec(QStringView query, const QVariantList &params, QObject *receiver)
{
    Q_ASSERT(d);
    AExpectedResult coro(receiver);
    d->exec(d, query, params, receiver, coro.callback);
    return coro;
}

AExpectedResult ADatabase::coExec(QUtf8StringView query, QObject *receiver)
{
    Q_ASSERT(d);
    AExpectedResult coro(receiver);
    d->exec(d, query, QVariantList(), receiver, coro.callback);
    return coro;
}

AExpectedResult
    ADatabase::coExec(QUtf8StringView query, const QVariantList &params, QObject *receiver)
{
    Q_ASSERT(d);
    AExpectedResult coro(receiver);
    d->exec(d, query, params, receiver, coro.callback);
    return coro;
}

AExpectedResult ADatabase::coExec(const APreparedQuery &query, QObject *receiver)
{
    Q_ASSERT(d);
    AExpectedResult coro(receiver);
    d->exec(d, query, QVariantList(), receiver, coro.callback);
    return coro;
}

AExpectedResult
    ADatabase::coExec(const APreparedQuery &query, const QVariantList &params, QObject *receiver)
{
    Q_ASSERT(d);
    AExpectedResult coro(receiver);
    d->exec(d, query, params, receiver, coro.callback);
    return coro;
}

void ADatabase::exec(QStringView query, QObject *receiver, AResultFn cb)
{
    Q_ASSERT(d);
    d->exec(d, query, QVariantList(), receiver, cb);
}

void ADatabase::exec(QUtf8StringView query, QObject *receiver, AResultFn cb)
{
    Q_ASSERT(d);
    d->exec(d, query, QVariantList(), receiver, cb);
}

void ADatabase::exec(const APreparedQuery &query, QObject *receiver, AResultFn cb)
{
    Q_ASSERT(d);
    d->exec(d, query, QVariantList(), receiver, cb);
}

void ADatabase::exec(QStringView query, const QVariantList &params, QObject *receiver, AResultFn cb)
{
    Q_ASSERT(d);
    d->exec(d, query, params, receiver, cb);
}

void ADatabase::exec(QUtf8StringView query,
                     const QVariantList &params,
                     QObject *receiver,
                     AResultFn cb)
{
    Q_ASSERT(d);
    d->exec(d, query, params, receiver, cb);
}

void ADatabase::exec(const APreparedQuery &query,
                     const QVariantList &params,
                     QObject *receiver,
                     AResultFn cb)
{
    Q_ASSERT(d);
    d->exec(d, query, params, receiver, cb);
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

void ADatabase::subscribeToNotification(const QString &channel,
                                        QObject *receiver,
                                        ANotificationFn cb)
{
    Q_ASSERT(d);
    d->subscribeToNotification(d, channel, receiver, cb);
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
