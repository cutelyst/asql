#ifndef ADRIVER_H
#define ADRIVER_H

#include <QString>
#include <QSocketNotifier>

#include <adatabase.h>

#include <functional>

class AResult;
class APreparedQuery;
class ADriver : public QObject
{
    Q_OBJECT
public:
    ADriver();
    virtual ~ADriver() = default;

    QString connectionInfo() const;
    void setConnectionInfo(const QString &connectionInfo);

    virtual void open(std::function<void(bool isOpen, const QString &error)> cb) = 0;

    virtual ADatabase::State state() const = 0;
    virtual void onStateChanged(std::function<void(ADatabase::State state, const QString &status)> cb) = 0;

    virtual bool isOpen() const = 0;

    virtual void begin(QSharedPointer<ADatabasePrivate> db, AResultFn cb, QObject *receiver) = 0;
    virtual void commit(QSharedPointer<ADatabasePrivate> db, AResultFn cb, QObject *receiver) = 0;
    virtual void rollback(QSharedPointer<ADatabasePrivate> db, AResultFn cb, QObject *receiver) = 0;

    virtual void exec(QSharedPointer<ADatabasePrivate> db, const QString &query, const QVariantList &params, AResultFn cb, QObject *receiver) = 0;
    virtual void exec(QSharedPointer<ADatabasePrivate> db, const APreparedQuery &query, const QVariantList &params, AResultFn cb, QObject *receiver) = 0;

    virtual void subscribeToNotification(QSharedPointer<ADatabasePrivate> db, const QString &name, ANotificationFn cb, QObject *receiver) = 0;
    virtual void unsubscribeFromNotification(QSharedPointer<ADatabasePrivate> db, const QString &name, QObject *receiver) = 0;

private:
    QString m_info;
};

#endif // ADRIVER_H
