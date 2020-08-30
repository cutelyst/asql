#ifndef ADRIVERPG_H
#define ADRIVERPG_H

#include <adriver.h>
#include <libpq-fe.h>

#include "aresult.h"

#include <QQueue>
#include <QPointer>
#include <QHash>

typedef struct pg_conn PGconn;

class AResultPg : public AResultPrivate
{
public:
    AResultPg();
    virtual ~AResultPg();

    virtual bool next() override;
    virtual bool lastResulSet() const override;
    virtual bool error() const override;
    virtual QString errorString() const override;

    virtual void setAt(int row) override;
    virtual int at() const override;
    virtual int size() const override;
    virtual int fields() const override;
    virtual int numRowsAffected() const override;

    virtual QString fieldName(int column) const override;
    virtual QVariant value(int column) const override;

    void processResult();

    QString m_errorString;
    PGresult *m_result = nullptr;
    int m_pos = -1;
    bool m_error = false;
    bool m_lastResultSet = true;
};

class APGQuery
{
public:
    APGQuery() : result(QSharedPointer<AResultPg>(new AResultPg))
    { }
    QString query;
    QSharedPointer<AResultPg> result;
    QVariantList params;
    AResultFn cb;
    QSharedPointer<ADatabasePrivate> db;
    QPointer<QObject> receiver;
    QObject *checkReceiver;

    inline void done() {
        AResult r(result);
        if (cb && (!checkReceiver || !receiver.isNull())) {
            cb(r);
        }
    }
};

class ADriverPg : public ADriver
{
public:
    ADriverPg();
    virtual ~ADriverPg();

    virtual void open(std::function<void(bool isOpen, const QString &error)> cb) override;
    virtual bool isOpen() const override;

    virtual void onStateChanged(std::function<void(ADatabase::State state, const QString &status)> cb) override;

    virtual void begin(QSharedPointer<ADatabasePrivate> db, AResultFn cb, QObject *receiver) override;
    virtual void commit(QSharedPointer<ADatabasePrivate> db, AResultFn cb, QObject *receiver) override;
    virtual void rollback(QSharedPointer<ADatabasePrivate> db, AResultFn cb, QObject *receiver) override;

    virtual bool exec(QSharedPointer<ADatabasePrivate> db, const QString &query, AResultFn cb, QObject *receiver) override;
    virtual bool exec(QSharedPointer<ADatabasePrivate> db, const QString &query, const QVariantList &params, AResultFn cb, QObject *receiver) override;

    virtual void subscribeToNotification(QSharedPointer<ADatabasePrivate> db, const QString &name, ANotificationFn cb, QObject *receiver) override;
    virtual void unsubscribeFromNotification(QSharedPointer<ADatabasePrivate> db, const QString &name, QObject *receiver) override;

private:
    void nextQuery();
    void finishConnection();
    void finishQueries(const QString &error);
    inline void doExec(APGQuery &pgQuery);
    inline void doExecParams(APGQuery &query);

    PGconn *m_conn = nullptr;
    bool m_connected = false;
    bool m_queryRunning = false;
    std::function<void (ADatabase::State, const QString &)> m_stateChangedCb;
    QHash<QString, ANotificationFn> m_subscribedNotifications;
    QQueue<APGQuery> m_queuedQueries;
    QSocketNotifier *m_writeNotify;
    QSocketNotifier *m_readNotify;
};

#endif // ADRIVERPG_H
