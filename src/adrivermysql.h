/*
 * SPDX-FileCopyrightText: (C) 2025 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "acoroexpected.h"
#include "apreparedquery.h"
#include "aresult.h"

#include <adriver.h>
#include <mysql/mysql.h>
#include <optional>

#include <QHash>
#include <QPointer>
#include <queue>

namespace ASql {

class AResultMysql final : public AResultPrivate
{
public:
    AResultMysql() = default;
    virtual ~AResultMysql() override;

    bool lastResultSet() const override;
    bool hasError() const override;
    QString errorString() const override;

    QByteArray query() const override;
    QVariantList queryArgs() const override;

    int size() const override;
    int fields() const override;
    qint64 numRowsAffected() const override;

    int indexOfField(QLatin1String name) const override;
    QString fieldName(int column) const override;
    QVariant value(int row, int column) const override;

    bool isNull(int row, int column) const override;
    bool toBool(int row, int column) const override;
    int toInt(int row, int column) const override;
    qint64 toLongLong(int row, int column) const override;
    quint64 toULongLong(int row, int column) const override;
    double toDouble(int row, int column) const override;
    QString toString(int row, int column) const override;
    std::string toStdString(int row, int column) const override;
    QUuid toUuid(int row, int column) const override;
    QDate toDate(int row, int column) const override;
    QTime toTime(int row, int column) const override;
    QDateTime toDateTime(int row, int column) const override;
    QJsonValue toJsonValue(int row, int column) const override;
    QCborValue toCborValue(int row, int column) const override;
    QByteArray toByteArray(int row, int column) const override;

    QByteArray m_query;
    QVariantList m_queryArgs;
    QStringList m_fields;
    QList<QVariantList> m_rows;
    qint64 m_numRowsAffected = -1;
    QString m_errorString;
    bool m_error         = false;
    bool m_lastResultSet = true;
};

class AMysqlQuery
{
public:
    AMysqlQuery() = default;

    QByteArray query;
    std::optional<APreparedQuery> preparedQuery;
    std::shared_ptr<AResultMysql> result;
    QVariantList params;
    ACoroDataRef cb;
    QPointer<QObject> receiver;
    QObject *checkReceiver = nullptr;

    inline void done()
    {
        if (cb && (!checkReceiver || !receiver.isNull())) {
            result->m_query     = query;
            result->m_queryArgs = params;
            AResult r(std::move(result));
            cb.deliverResult(r);
        }
    }

    inline void doneError(const QString &error)
    {
        if (cb && (!checkReceiver || !receiver.isNull())) {
            result                = std::make_shared<AResultMysql>();
            result->m_query       = query;
            result->m_queryArgs   = params;
            result->m_errorString = error;
            result->m_error       = true;
            AResult r(std::move(result));
            cb.deliverResult(r);
        }
    }
};

class ADriverMysql final : public ADriver
{
    Q_OBJECT
public:
    ADriverMysql(const QString &connInfo);
    virtual ~ADriverMysql() override;

    QString driverName() const override;

    bool isValid() const override;
    void open(const std::shared_ptr<ADriver> &driver, QObject *receiver, AOpenFn cb) override;
    bool isOpen() const override;

    void setState(ADatabase::State state, const QString &status);
    ADatabase::State state() const override;
    void onStateChanged(
        QObject *receiver,
        std::function<void(ADatabase::State state, const QString &status)> cb) override;

    void begin(const std::shared_ptr<ADriver> &db, QObject *receiver, ACoroDataRef cb) override;
    void commit(const std::shared_ptr<ADriver> &db, QObject *receiver, ACoroDataRef cb) override;
    void rollback(const std::shared_ptr<ADriver> &db, QObject *receiver, ACoroDataRef cb) override;

    void exec(const std::shared_ptr<ADriver> &db,
              QUtf8StringView query,
              QObject *receiver,
              ACoroDataRef cb) override;

    void exec(const std::shared_ptr<ADriver> &db,
              QStringView query,
              QObject *receiver,
              ACoroDataRef cb) override;

    void exec(const std::shared_ptr<ADriver> &db,
              QUtf8StringView query,
              const QVariantList &params,
              QObject *receiver,
              ACoroDataRef cb) override;

    void exec(const std::shared_ptr<ADriver> &db,
              QStringView query,
              const QVariantList &params,
              QObject *receiver,
              ACoroDataRef cb) override;

    void exec(const std::shared_ptr<ADriver> &db,
              const APreparedQuery &query,
              const QVariantList &params,
              QObject *receiver,
              ACoroDataRef cb) override;

    void setLastQuerySingleRowMode() override;

    bool enterPipelineMode(std::chrono::milliseconds timeout) override;

    bool exitPipelineMode() override;

    ADatabase::PipelineStatus pipelineStatus() const override;

    bool pipelineSync() override;

    int queueSize() const override;

    void subscribeToNotification(const std::shared_ptr<ADriver> &db,
                                 const QString &name,
                                 QObject *receiver,
                                 ANotificationFn cb) override;
    QStringList subscribedToNotifications() const override;
    void unsubscribeFromNotification(const std::shared_ptr<ADriver> &db,
                                     const QString &name) override;

private:
    inline bool isConnected() const;
    void setupCheckReceiver(AMysqlQuery &mysqlQuery, QObject *receiver);
    void nextQuery();
    void finishConnection(const QString &error);

    struct OpenCaller {
        std::shared_ptr<ADriver> driver;
        AOpenFn cb;
        std::optional<QPointer<QObject>> receiverPtr;

        void emit(bool isOpen, const QString &error)
        {
            if (cb && (!receiverPtr.has_value() || !receiverPtr->isNull())) {
                cb(isOpen, error);
            }
        }
    };
    std::unique_ptr<OpenCaller> m_openCaller;

    std::optional<QPointer<QObject>> m_stateChangedReceiver;
    std::function<void(ADatabase::State, const QString &)> m_stateChangedCb;

    std::queue<AMysqlQuery> m_queuedQueries;
    std::shared_ptr<ADriver> selfDriver;

    std::unique_ptr<QSocketNotifier> m_writeNotify;
    std::unique_ptr<QSocketNotifier> m_readNotify;

    MYSQL *m_mysql           = nullptr;
    ADatabase::State m_state = ADatabase::State::Disconnected;
    bool m_queryRunning      = false;

    // Connection parameters stored for non-blocking re-poll
    QByteArray m_host;
    QByteArray m_user;
    QByteArray m_password;
    QByteArray m_database;
    unsigned int m_port = 3306;
};

} // namespace ASql
