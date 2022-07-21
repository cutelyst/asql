/* 
 * SPDX-FileCopyrightText: (C) 2020 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "adatabase.h"

#include "adriver.h"
#include "adriverfactory.h"

#include <QLoggingCategory>

using namespace ASql;

ADatabase::ADatabase() = default;

ADatabase::ADatabase(const std::shared_ptr<ADriver> &driver) : d(driver)
{

}

ADatabase::ADatabase(const std::shared_ptr<ADriverFactory> &factory) : d(factory->createDriver())
{

}

ADatabase::ADatabase(const ADatabase &other)
{
    d = other.d;
}

ADatabase::~ADatabase() = default;

bool ADatabase::isValid()
{
    return d && d->isValid();
}

void ADatabase::open(std::function<void(bool error, const QString &fff)> cb)
{
    if (!d) {
        d = std::make_shared<ADriver>();
    }

    if (d->state() == ADatabase::State::Disconnected) {
        d->open(cb);
    }
}

ADatabase::State ADatabase::state() const
{
    if (d) {
        return d->state();
    }
    return ADatabase::State::Disconnected;
}

void ADatabase::onStateChanged(std::function<void (ADatabase::State, const QString &)> cb)
{
    Q_ASSERT(d);
    d->onStateChanged(cb);
}

bool ADatabase::isOpen() const
{
    Q_ASSERT(d);
    return d && d->isOpen();
}

void ADatabase::begin(AResultFn cb, QObject *receiver)
{
    Q_ASSERT(d);
    d->begin(d, cb, receiver);
}

void ADatabase::commit(AResultFn cb, QObject *receiver)
{
    Q_ASSERT(d);
    d->commit(d, cb, receiver);
}

void ADatabase::rollback(AResultFn cb, QObject *receiver)
{
    Q_ASSERT(d);
    d->rollback(d, cb, receiver);
}

void ADatabase::exec(QStringView query, AResultFn cb, QObject *receiver)
{
    Q_ASSERT(d);
    d->exec(d, query, QVariantList(), cb, receiver);
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void ADatabase::exec(QUtf8StringView query, AResultFn cb, QObject *receiver)
{
    Q_ASSERT(d);
    d->exec(d, query, QVariantList(), cb, receiver);
}
#endif

void ADatabase::exec(const APreparedQuery &query, AResultFn cb, QObject *receiver)
{
    Q_ASSERT(d);
    d->exec(d, query, QVariantList(), cb, receiver);
}

void ADatabase::exec(QStringView query, const QVariantList &params, AResultFn cb, QObject *receiver)
{
    Q_ASSERT(d);
    d->exec(d, query, params, cb, receiver);
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void ADatabase::exec(QUtf8StringView query, const QVariantList &params, AResultFn cb, QObject *receiver)
{
    Q_ASSERT(d);
    d->exec(d, query, params, cb, receiver);
}
#endif

void ADatabase::exec(const APreparedQuery &query, const QVariantList &params, AResultFn cb, QObject *receiver)
{
    Q_ASSERT(d);
    d->exec(d, query, params, cb, receiver);
}

void ADatabase::setLastQuerySingleRowMode()
{
    Q_ASSERT(d);
    d->setLastQuerySingleRowMode();
}

bool ADatabase::enterPipelineMode(qint64 autoSyncMS)
{
    Q_ASSERT(d);
    return d->enterPipelineMode(autoSyncMS);
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

void ADatabase::subscribeToNotification(const QString &channel, ANotificationFn cb, QObject *receiver)
{
    Q_ASSERT(d);
    d->subscribeToNotification(d, channel, cb, receiver);
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

ADatabase &ADatabase::operator =(const ADatabase &copy)
{
    d = copy.d;
    return *this;
}

#include "moc_adatabase.cpp"
