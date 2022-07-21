/* 
 * SPDX-FileCopyrightText: (C) 2020 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "adriver.h"
#include "aresult.h"

#include <QDate>
#include <QJsonValue>

using namespace ASql;

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

    bool isNull(int row, int column) const final { return true; };
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
    QJsonValue toJsonValue(int row, int column) const final { return  {}; };
    QByteArray toByteArray(int row, int column) const final { return {}; };
};

static const QString INVALID_DRIVER = QStringLiteral("INVALID DATABASE DRIVER");

ADriver::ADriver() = default;

ADriver::ADriver(const QString &connectionInfo) : m_info(connectionInfo)
{

}

QString ADriver::connectionInfo() const
{
    return m_info;
}

bool ADriver::isValid() const
{
    return false;
}

void ADriver::open(std::function<void (bool, const QString &)> cb)
{
    if (cb) {
        cb(false, INVALID_DRIVER);
    }
}

ADatabase::State ADriver::state() const
{
    return ADatabase::State::Disconnected;
}

void ADriver::onStateChanged(std::function<void (ADatabase::State, const QString &)> cb)
{
    Q_UNUSED(cb)
}

bool ADriver::isOpen() const
{
    return false;
}

void ADriver::begin(const std::shared_ptr<ADriver> &db, AResultFn cb, QObject *receiver)
{
    Q_UNUSED(db)
    Q_UNUSED(receiver)
    if (cb) {
        AResult result(std::shared_ptr<AResultInvalid>(new AResultInvalid));
        cb(result);
    }
}

void ADriver::commit(const std::shared_ptr<ADriver> &db, AResultFn cb, QObject *receiver)
{
    Q_UNUSED(db)
    Q_UNUSED(receiver)
    if (cb) {
        AResult result(std::shared_ptr<AResultInvalid>(new AResultInvalid));
        cb(result);
    }
}

void ADriver::rollback(const std::shared_ptr<ADriver> &db, AResultFn cb, QObject *receiver)
{
    Q_UNUSED(db)
    Q_UNUSED(receiver)
    if (cb) {
        AResult result(std::shared_ptr<AResultInvalid>(new AResultInvalid));
        cb(result);
    }
}

void ADriver::exec(const std::shared_ptr<ADriver> &db, QStringView query, const QVariantList &params, AResultFn cb, QObject *receiver)
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

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void ADriver::exec(const std::shared_ptr<ADriver> &db, QUtf8StringView query, const QVariantList &params, AResultFn cb, QObject *receiver)
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
#endif

void ADriver::exec(const std::shared_ptr<ADriver> &db, const APreparedQuery &query, const QVariantList &params, AResultFn cb, QObject *receiver)
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

bool ADriver::enterPipelineMode(qint64 autoSyncMS)
{
    Q_UNUSED(autoSyncMS);
    return false;
}

bool ADriver::exitPipelineMode()
{
    return false;
}

ADatabase::PipelineStatus ADriver::pipelineStatus() const
{
    return ADatabase::PipelineStatus::Off;
}

bool ADriver::pipelineSync()
{
    return false;
}

void ADriver::subscribeToNotification(const std::shared_ptr<ADriver> &db, const QString &name, ANotificationFn cb, QObject *receiver)
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

void ADriver::unsubscribeFromNotification(const std::shared_ptr<ADriver> &db, const QString &name)
{
    Q_UNUSED(db)
    Q_UNUSED(name)
}

#include "moc_adriver.cpp"
