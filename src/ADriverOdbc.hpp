/*
 * SPDX-FileCopyrightText: (C) 2025 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "adriver.h"
#include "apreparedquery.h"
#include "aresult.h"

#include <optional>
#include <sql.h>
#include <sqlext.h>

#include <QHash>
#include <QMutex>
#include <QPointer>
#include <QQueue>
#include <QThread>

namespace ASql {

class AResultOdbc final : public AResultPrivate
{
public:
    AResultOdbc()          = default;
    virtual ~AResultOdbc() = default;

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
    inline QVariant value(int row, int column) const override;

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
    QJsonValue toJsonValue(int row, int column) const final;
    QCborValue toCborValue(int row, int column) const final;
    QByteArray toByteArray(int row, int column) const override;

    QByteArray m_query;
    QVariantList m_queryArgs;
    QVariantList m_rows;
    std::optional<QString> m_error;
    QStringList m_fields;
    qint64 m_numRowsAffected = -1;
    bool m_lastResultSet     = true;
};

struct OdbcOpenPromise {
    AOpenFn cb;
    std::optional<QPointer<QObject>> receiver;
};

struct OdbcQueryPromise {
    std::optional<APreparedQuery> preparedQuery;
    ACoroDataRef cb;
    std::shared_ptr<AResultOdbc> result;
    std::optional<QPointer<QObject>> receiver;
};

class AOdbcThread final : public QThread
{
    Q_OBJECT
public:
    AOdbcThread(const QString &connInfo);
    ~AOdbcThread();

    QMutex m_promisesMutex;
    QQueue<ASql::OdbcQueryPromise> m_promisesReady;

public Q_SLOTS:
    void open();
    void query(ASql::OdbcQueryPromise promise);
    void queryPrepared(ASql::OdbcQueryPromise promise);
    void queryExec(ASql::OdbcQueryPromise promise);

Q_SIGNALS:
    void openned(bool isOpen, QString error);
    void queryReady();

private:
    QString odbcError(SQLSMALLINT handleType, SQLHANDLE handle);
    void fetchResults(SQLHSTMT stmt, OdbcQueryPromise &promise);
    QVariant columnValue(SQLHSTMT stmt, SQLUSMALLINT col, SQLSMALLINT sqlType);
    QString readWCharColumn(SQLHSTMT stmt, SQLUSMALLINT col);
    void bindParameters(SQLHSTMT stmt,
                        const QVariantList &params,
                        OdbcQueryPromise &promise,
                        QList<QByteArray> &buffers,
                        QList<SQLLEN> &indicators);

    QHash<int, SQLHSTMT> m_preparedStmts;
    QString m_connString;
    SQLHENV m_env = SQL_NULL_HENV;
    SQLHDBC m_dbc = SQL_NULL_HDBC;
};

class ADriverOdbc final : public ADriver
{
    Q_OBJECT
public:
    ADriverOdbc(const QString &connInfo);
    virtual ~ADriverOdbc();

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
    AOdbcThread m_worker;
    QThread m_thread;
    ADatabase::State m_state = ADatabase::State::Disconnected;
    int m_queueSize          = 0;
};

} // namespace ASql

Q_DECLARE_METATYPE(ASql::OdbcOpenPromise)
Q_DECLARE_METATYPE(ASql::OdbcQueryPromise)
