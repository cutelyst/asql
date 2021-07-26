/* 
 * SPDX-FileCopyrightText: (C) 2020 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "adriver.h"
#include "aresult.h"

#include <QDate>

class AResultInvalid : public AResultPrivate
{
public:
    bool lastResulSet() const final { return true; };
    bool error() const final { return true; };
    QString errorString() const { return {}; };

    int size() const final { return 0; };
    int fields() const final { return 0; };
    int numRowsAffected() const final { return 0; };

    QString fieldName(int column) const final { return {}; };
    QVariant value(int row, int column) const final { return {}; };

    bool toBool(int row, int column) const final { return false; };
    int toInt(int row, int column) const final { return 0; };
    qint64 toLongLong(int row, int column) const final { return 0; };
    quint64 toULongLong(int row, int column) const final { return 0; };
    double toDouble(int row, int column) const final { return 0; };
    QString toString(int row, int column) const final { return {}; };
    std::string toStdString(int row, int column) const final { return {}; };
    QDate toDate(int row, int column) const final { return {}; };
    QTime toTime(int row, int column) const final { return {}; };
    QDateTime toDateTime(int row, int column) const final { return {}; };
    QByteArray toByteArray(int row, int column) const final { return {}; };
};

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

void ADriver::begin(std::shared_ptr<ADatabasePrivate> db, AResultFn cb, QObject *receiver)
{
    Q_UNUSED(db)
    Q_UNUSED(receiver)
    if (cb) {
        AResult result(std::shared_ptr<AResultInvalid>(new AResultInvalid));
        cb(result);
    }
}

void ADriver::commit(std::shared_ptr<ADatabasePrivate> db, AResultFn cb, bool now, QObject *receiver)
{
    Q_UNUSED(db)
    Q_UNUSED(now)
    Q_UNUSED(receiver)
    if (cb) {
        AResult result(std::shared_ptr<AResultInvalid>(new AResultInvalid));
        cb(result);
    }
}

void ADriver::rollback(std::shared_ptr<ADatabasePrivate> db, AResultFn cb, bool now, QObject *receiver)
{
    Q_UNUSED(db)
    Q_UNUSED(now)
    Q_UNUSED(receiver)
    if (cb) {
        AResult result(std::shared_ptr<AResultInvalid>(new AResultInvalid));
        cb(result);
    }
}

void ADriver::exec(std::shared_ptr<ADatabasePrivate> db, const QString &query, const QVariantList &params, AResultFn cb, QObject *receiver)
{
    Q_UNUSED(db)
    Q_UNUSED(query)
    Q_UNUSED(params)
    Q_UNUSED(receiver)
    if (cb) {
        AResult result(std::shared_ptr<AResultInvalid>(new AResultInvalid));
        cb(result);
    }
}

void ADriver::exec(std::shared_ptr<ADatabasePrivate> db, QStringView query, const QVariantList &params, AResultFn cb, QObject *receiver)
{
    Q_UNUSED(db)
    Q_UNUSED(query)
    Q_UNUSED(params)
    Q_UNUSED(receiver)
    if (cb) {
        AResult result(std::shared_ptr<AResultInvalid>(new AResultInvalid));
        cb(result);
    }
}

void ADriver::exec(std::shared_ptr<ADatabasePrivate> db, const APreparedQuery &query, const QVariantList &params, AResultFn cb, QObject *receiver)
{
    Q_UNUSED(db)
    Q_UNUSED(query)
    Q_UNUSED(params)
    Q_UNUSED(receiver)
    if (cb) {
        AResult result(std::shared_ptr<AResultInvalid>(new AResultInvalid));
        cb(result);
    }
}

void ADriver::setLastQuerySingleRowMode()
{

}

void ADriver::subscribeToNotification(std::shared_ptr<ADatabasePrivate> db, const QString &name, ANotificationFn cb, QObject *receiver)
{
    Q_UNUSED(db)
    Q_UNUSED(name)
    Q_UNUSED(cb)
    Q_UNUSED(receiver)
}

QStringList ADriver::subscribedToNotifications() const
{
    return {};
}

void ADriver::unsubscribeFromNotification(std::shared_ptr<ADatabasePrivate> db, const QString &name)
{
    Q_UNUSED(db)
    Q_UNUSED(name)
}

#include "moc_adriver.cpp"
