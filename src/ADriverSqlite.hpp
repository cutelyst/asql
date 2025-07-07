#ifndef ADRIVERSQLITE_HPP
#define ADRIVERSQLITE_HPP

#include "adriver.h"
#include "apreparedquery.h"
#include "aresult.h"
#include "sqlite/sqlite3.h"

#include <chrono>
#include <optional>

#include <QHash>
#include <QMutex>
#include <QPointer>
#include <QQueue>
#include <QThread>

using namespace std::chrono_literals;

namespace ASql {

class AResultSqlite final : public AResultPrivate
{
public:
    AResultSqlite()          = default;
    virtual ~AResultSqlite() = default;

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
    QDate toDate(int row, int column) const override;
    QTime toTime(int row, int column) const override;
    QDateTime toDateTime(int row, int column) const override;
    QJsonValue toJsonValue(int row, int column) const final;
    QCborValue toCborValue(int row, int column) const final;
    QByteArray toByteArray(int row, int column) const override;

    inline void processResult();

    QByteArray m_query;
    QVariantList m_queryArgs;
    QVariantList m_rows;
    std::optional<QString> m_error;
    QStringList m_fields;
    qint64 m_numRowsAffected = -1;
    bool m_lastResultSet     = true;
};

struct OpenPromise {
    ADatabaseOpenFn cb;
    std::optional<QPointer<QObject>> receiver;
};

struct QueryPromise {
    std::optional<APreparedQuery> preparedQuery;
    AResultFn cb;
    std::shared_ptr<AResultSqlite> result;
    std::optional<QPointer<QObject>> receiver;
};

class ASqliteThread final : public QThread
{
    Q_OBJECT
public:
    ASqliteThread(const QString &connInfo);
    ~ASqliteThread();

    QMutex m_promisesMutex;
    QQueue<ASql::QueryPromise> m_promisesReady;

public Q_SLOTS:
    void open();
    // This is likely safe because we move our
    // results to m_promisesReady queue,
    // which are consumed from the main thread.

    // The issue we used to face is that the promisse.cb
    // sometimes hold the last reference to the cb,
    // meaning the cb dtor was called on the worker thread
    // thus lambdas captures were wrongly deleted here.
    void query(ASql::QueryPromise promise);
    void queryPrepared(ASql::QueryPromise promise);
    void queryExec(ASql::QueryPromise promise);

Q_SIGNALS:
    void openned(bool isOpen, QString error);
    void queryReady();

private:
    std::shared_ptr<sqlite3_stmt> prepare(QueryPromise &promise, int flags);
    static int busyHandler(void *data, int retry_count);

    QHash<int, std::shared_ptr<sqlite3_stmt>> m_preparedQueries;
    QString m_uri;
    sqlite3 *m_db                              = nullptr;
    std::chrono::milliseconds m_busyRetrySleep = 100ms;
    int m_busyRetries                          = 1;
};

class ADriverSqlite final : public ADriver
{
    Q_OBJECT
public:
    ADriverSqlite(const QString &connInfo);
    virtual ~ADriverSqlite();

    QString driverName() const override;

    bool isValid() const override;
    void open(const std::shared_ptr<ADriver> &driver,
              QObject *receiver,
              std::function<void(bool isOpen, const QString &error)> cb) override;
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
    std::optional<QPointer<QObject>> m_stateChangedReceiver;
    std::function<void(ADatabase::State, const QString &)> m_stateChangedCb;
    std::shared_ptr<ADriver> selfDriver;
    ASqliteThread m_worker;
    QThread m_thread;
    ADatabase::State m_state  = ADatabase::State::Disconnected;
    int m_pipelineSync        = 0;
    int m_queueSize           = 0;
    bool m_flush              = false;
    bool m_queryRunning       = false;
    bool m_notificationPtrSet = false;
};

} // namespace ASql

Q_DECLARE_METATYPE(ASql::OpenPromise)
Q_DECLARE_METATYPE(ASql::QueryPromise)

#endif // ADRIVERSQLITE_HPP
