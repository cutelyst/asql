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
#include <QMutex>
#include <QPointer>
#include <QQueue>
#include <QThread>

namespace ASql {

class AResultMysql final : public AResultPrivate
{
public:
    AResultMysql()          = default;
    virtual ~AResultMysql() = default;

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
    QVariantList m_rows; // flat: row*fields+col
    qint64 m_numRowsAffected = -1;
    std::optional<QString> m_error;
    bool m_lastResultSet = true;
};

struct OpenPromise {
    AOpenFn cb;
    std::optional<QPointer<QObject>> receiver;
};

struct MysqlQueryPromise {
    std::optional<APreparedQuery> preparedQuery;
    ACoroDataRef cb;
    std::shared_ptr<AResultMysql> result;
    std::optional<QPointer<QObject>> receiver;
};

class AMysqlThread final : public QThread
{
    Q_OBJECT
public:
    AMysqlThread(const QString &connInfo);
    ~AMysqlThread();

    QMutex m_promisesMutex;
    QQueue<ASql::MysqlQueryPromise> m_promisesReady;

public Q_SLOTS:
    void open();
    void query(ASql::MysqlQueryPromise promise);
    void queryPrepared(ASql::MysqlQueryPromise promise);
    void queryExec(ASql::MysqlQueryPromise promise);

Q_SIGNALS:
    void openned(bool isOpen, QString error);
    void queryReady();

private:
    MYSQL_STMT *prepare(MysqlQueryPromise &promise);
    void enqueueAndSignal(MysqlQueryPromise &promise);

    QHash<int, MYSQL_STMT *> m_preparedQueries;
    QString m_connInfo;
    MYSQL *m_mysql = nullptr;
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
    std::optional<QPointer<QObject>> m_stateChangedReceiver;
    std::function<void(ADatabase::State, const QString &)> m_stateChangedCb;
    std::shared_ptr<ADriver> selfDriver;
    AMysqlThread m_worker;
    QThread m_thread;
    ADatabase::State m_state = ADatabase::State::Disconnected;
    int m_queueSize          = 0;
};

} // namespace ASql

Q_DECLARE_METATYPE(ASql::OpenPromise)
Q_DECLARE_METATYPE(ASql::MysqlQueryPromise)
