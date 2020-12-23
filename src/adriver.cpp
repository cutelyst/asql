/* 
 * SPDX-FileCopyrightText: (C) 2020 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "adriver.h"
#include "aresult.h"

static const QString INVALID_DRIVER = QStringLiteral("INVALID DATABASE DRIVER");

ADriver::ADriver()
{

}

QString ADriver::connectionInfo() const
{
    return m_info;
}

void ADriver::setConnectionInfo(const QString &info)
{
    m_info = info;
}

void ADriver::open(std::function<void (bool, const QString &)> cb)
{
    if (cb) {
        cb(false, INVALID_DRIVER);
    }
}

ADatabase::State ADriver::state() const
{
    return ADatabase::Disconnected;
}

void ADriver::onStateChanged(std::function<void (ADatabase::State, const QString &)> cb)
{
    Q_UNUSED(cb)
}

bool ADriver::isOpen() const
{
    return false;
}

void ADriver::begin(QSharedPointer<ADatabasePrivate> db, AResultFn cb, QObject *receiver)
{
    Q_UNUSED(db)
    Q_UNUSED(receiver)
    if (cb) {
        AResult result;
        cb(result);
    }
}

void ADriver::commit(QSharedPointer<ADatabasePrivate> db, AResultFn cb, QObject *receiver)
{
    Q_UNUSED(db)
    Q_UNUSED(receiver)
    if (cb) {
        AResult result;
        cb(result);
    }
}

void ADriver::rollback(QSharedPointer<ADatabasePrivate> db, AResultFn cb, QObject *receiver)
{
    Q_UNUSED(db)
    Q_UNUSED(receiver)
    if (cb) {
        AResult result;
        cb(result);
    }
}

void ADriver::exec(QSharedPointer<ADatabasePrivate> db, const QString &query, const QVariantList &params, AResultFn cb, QObject *receiver)
{
    Q_UNUSED(db)
    Q_UNUSED(query)
    Q_UNUSED(params)
    Q_UNUSED(receiver)
    if (cb) {
        AResult result;
        cb(result);
    }
}

void ADriver::exec(QSharedPointer<ADatabasePrivate> db, const APreparedQuery &query, const QVariantList &params, AResultFn cb, QObject *receiver)
{
    Q_UNUSED(db)
    Q_UNUSED(query)
    Q_UNUSED(params)
    Q_UNUSED(receiver)
    if (cb) {
        AResult result;
        cb(result);
    }
}

void ADriver::setLastQuerySingleRowMode()
{

}

void ADriver::subscribeToNotification(QSharedPointer<ADatabasePrivate> db, const QString &name)
{
    Q_UNUSED(db)
    Q_UNUSED(name)
}

void ADriver::onNotification(QSharedPointer<ADatabasePrivate> db, ANotificationFn cb, QObject *receiver)
{
    Q_UNUSED(db)
    Q_UNUSED(cb)
    Q_UNUSED(receiver)
}

QStringList ADriver::subscribedToNotifications() const
{
    return {};
}

void ADriver::unsubscribeFromNotification(QSharedPointer<ADatabasePrivate> db, const QString &name, QObject *receiver)
{
    Q_UNUSED(db)
    Q_UNUSED(name)
    Q_UNUSED(receiver)
}

#include "moc_adriver.cpp"
