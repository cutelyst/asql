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

void ADatabase::open(QObject *receiver, ADatabaseOpenFn cb)
{
    if (!d) {
        d = std::make_shared<ADriver>();
    }

    if (d->state() == ADatabase::State::Disconnected) {
        if (cb) {
            // Wrap the legacy callback in an ACoroOpenData adapter.
            // The adapter holds a self-reference so it stays alive until
            // deliverOpen() is invoked, at which point the self-reference
            // is released and only the driver's weak_ptr remains.
            struct OpenAdapter final : public ACoroOpenData {
                ADatabaseOpenFn fn;
                std::optional<QPointer<QObject>> receiverPtr;
                std::shared_ptr<OpenAdapter> keepAlive;

                void deliverOpen(bool isOpen, const QString &error) override
                {
                    auto ref = std::move(keepAlive); // release self-ref after call
                    if (!receiverPtr.has_value() || !receiverPtr->isNull()) {
                        fn(isOpen, error);
                    }
                }
            };
            auto adapter       = std::make_shared<OpenAdapter>();
            adapter->fn        = std::move(cb);
            adapter->keepAlive = adapter; // self-reference
            if (receiver) {
                adapter->receiverPtr = receiver;
            }
            d->open(d, receiver, AOpenFn{std::weak_ptr<ACoroOpenData>(adapter)});
        } else {
            d->open(d, receiver, AOpenFn{});
        }
    }
}

AExpectedOpen ADatabase::coOpen(QObject *receiver)
{
    AExpectedOpen coro(receiver);
    if (!d) {
        d = std::make_shared<ADriver>();
    }

    if (d->state() == ADatabase::State::Connected) {
        coro.m_data->deliverOpen(true, {});
    } else if (d->state() == ADatabase::State::Disconnected) {
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

void ADatabase::onStateChanged(QObject *receiver, StateChangedFn cb)
{
    Q_ASSERT(d);
    d->onStateChanged(receiver, cb);
}

bool ADatabase::isOpen() const
{
    Q_ASSERT(d);
    return d != nullptr && d->isOpen();
}

AExpectedResult beginHelper(const std::shared_ptr<ADriver> &d, QObject *receiver)
{
    Q_ASSERT(d);
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
        if (result.has_value()) {
            chainData->deliverDirect(ATransaction::fromStarted(db));
        } else {
            chainData->deliverDirect(std::unexpected(result.error()));
        }
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

AExpectedMultiResult ADatabase::execMulti(QUtf8StringView query, QObject *receiver)
{
    Q_ASSERT(d);
    AExpectedMultiResult coro(receiver);
    d->exec(d, query, receiver, coro.ref());
    return coro;
}

AExpectedResult ADatabase::exec(QUtf8StringView query, QObject *receiver)
{
    Q_ASSERT(d);
    AExpectedResult coro(receiver);
    d->exec(d, query, QVariantList(), receiver, coro.ref());
    return coro;
}

AExpectedResult
    ADatabase::exec(QUtf8StringView query, const QVariantList &params, QObject *receiver)
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
    d->exec(d, query, QVariantList(), receiver, coro.ref());
    return coro;
}

AExpectedResult
    ADatabase::exec(const APreparedQuery &query, const QVariantList &params, QObject *receiver)
{
    Q_ASSERT(d);
    AExpectedResult coro(receiver);
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
