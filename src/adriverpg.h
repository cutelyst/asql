/*
 * SPDX-FileCopyrightText: (C) 2020-2022 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "apreparedquery.h"
#include "aresult.h"

#include <adriver.h>
#include <libpq-fe.h>
#include <optional>

#include <QHash>
#include <QPointer>
#include <queue>

class QTimer;

namespace ASql {

class AResultPg final : public AResultPrivate
{
public:
    AResultPg(PGresult *result);
    virtual ~AResultPg();

    ExecStatusType status() const;

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
    QDate toDate(int row, int column) const override;
    QTime toTime(int row, int column) const override;
    QDateTime toDateTime(int row, int column) const override;
    QJsonValue toJsonValue(int row, int column) const final;
    QCborValue toCborValue(int row, int column) const final;
    QByteArray toByteArray(int row, int column) const override;

    inline void processResult();

    QByteArray m_query;
    QVariantList m_queryArgs;
    QString m_errorString;
    PGresult *m_result   = nullptr;
    bool m_error         = false;
    bool m_lastResultSet = true;
};

class APGQuery
{
public:
    APGQuery() = default;
    QByteArray query;
    std::optional<APreparedQuery> preparedQuery;
    std::shared_ptr<AResultPg> result;
    QVariantList params;
    AResultFn cb;
    QPointer<QObject> receiver;
    QObject *checkReceiver = nullptr;
    bool preparing         = false;
    bool setSingleRow      = false;

    inline void done()
    {
        if (cb && (!checkReceiver || !receiver.isNull())) {
            result->m_query     = query;
            result->m_queryArgs = params;
            AResult r(std::move(result));
            cb(r);
        }
    }

    inline void doneError(const QString &error)
    {
        if (cb && (!checkReceiver || !receiver.isNull())) {
            result                = std::make_shared<AResultPg>(nullptr);
            result->m_query       = query;
            result->m_queryArgs   = params;
            result->m_errorString = error;
            result->m_error       = true;
            AResult r(std::move(result));
            cb(r);
        }
    }
};

class APgConn
{
public:
    APgConn(const QString &connInfo)
        : m_conn(PQconnectStart(connInfo.toUtf8().constData()))
    {
    }
    ~APgConn() { PQfinish(m_conn); }

    PGconn *conn() const { return m_conn; }

    PGnotify *notifies() const { return PQnotifies(m_conn); }

    int socket() const { return PQsocket(m_conn); }

    ConnStatusType status() const { return PQstatus(m_conn); }

    QString errorMessage() const { return QString::fromUtf8(PQerrorMessage(m_conn)); }

    PostgresPollingStatusType connectPoll() const { return PQconnectPoll(m_conn); }

private:
    PGconn *m_conn;
};

class ADriverPg final : public ADriver
{
    Q_OBJECT
public:
    ADriverPg(const QString &connInfo);
    virtual ~ADriverPg();

    QString driverName() const override;

    bool isValid() const override;
    void open(const std::shared_ptr<ADriver> &driver,
              QObject *receiver,
              ADatabaseOpenFn cb) override;
    bool isOpen() const override;

    void setState(ADatabase::State state, const QString &status);
    ADatabase::State state() const override;
    void onStateChanged(
        QObject *receiver,
        std::function<void(ADatabase::State state, const QString &status)> cb) override;

    void begin(const std::shared_ptr<ADriver> &db, QObject *receiver, AResultFn cb) override;
    void commit(const std::shared_ptr<ADriver> &db, QObject *receiver, AResultFn cb) override;
    void rollback(const std::shared_ptr<ADriver> &db, QObject *receiver, AResultFn cb) override;

    void exec(const std::shared_ptr<ADriver> &db,
              QUtf8StringView query,
              QObject *receiver,
              AResultFn cb) override;

    void exec(const std::shared_ptr<ADriver> &db,
              QStringView query,
              QObject *receiver,
              AResultFn cb) override;

    void exec(const std::shared_ptr<ADriver> &db,
              QUtf8StringView query,
              const QVariantList &params,
              QObject *receiver,
              AResultFn cb) override;
    void exec(const std::shared_ptr<ADriver> &db,
              QStringView query,
              const QVariantList &params,
              QObject *receiver,
              AResultFn cb) override;
    void exec(const std::shared_ptr<ADriver> &db,
              const APreparedQuery &query,
              const QVariantList &params,
              QObject *receiver,
              AResultFn cb) override;

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
    inline void setupCheckReceiver(APGQuery &pgQuery, QObject *receiver);
    inline bool runQuery(APGQuery &pgQuery);
    inline bool queryShouldBeQueued() const;
    void nextQuery();
    void finishConnection(const QString &error);
    inline int doExec(APGQuery &pgQuery);
    inline int doExecParams(APGQuery &query);
    inline void setSingleRowMode();
    inline void cmdFlush();
    inline bool isConnected() const;

    struct OpenCaller {
        std::shared_ptr<ADriver> driver;
        ADatabaseOpenFn cb;
        QPointer<QObject> receiverPtr;
        bool checkReceiver = false;

        void emit(bool isOpen, const QString &error)
        {
            if ((!checkReceiver || receiverPtr.isNull()) && cb) {
                cb(isOpen, error);
            }
        }
    };
    std::unique_ptr<OpenCaller> m_openCaller;

    QPointer<QObject> m_stateChangedReceiver;
    std::function<void(ADatabase::State, const QString &)> m_stateChangedCb;
    QHash<QString, ANotificationFn> m_subscribedNotifications;
    std::queue<APGQuery> m_queuedQueries;
    std::shared_ptr<ADriver> selfDriver;
    QHash<int, QByteArray> m_preparedQueries;
    std::unique_ptr<QSocketNotifier> m_writeNotify;
    std::unique_ptr<QSocketNotifier> m_readNotify;
    std::unique_ptr<QTimer> m_autoSyncTimer;
    std::unique_ptr<APgConn> m_conn;
    ADatabase::State m_state       = ADatabase::State::Disconnected;
    int m_pipelineSync             = 0;
    bool m_stateChangedReceiverSet = false;
    bool m_flush                   = false;
    bool m_queryRunning            = false;
    bool m_notificationPtrSet      = false;
};

} // namespace ASql
