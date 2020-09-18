#include "adatabase.h"
#include "adatabase_p.h"

#include "adriver.h"
#include "adriverpg.h"

#include <QLoggingCategory>

ADatabase::ADatabase()
{

}

ADatabase::ADatabase(const QString &connectionInfo)
    : d(new ADatabasePrivate(connectionInfo))
{

}

ADatabase::ADatabase(const ADatabase &other)
{
    d = other.d;
}

ADatabase::~ADatabase()
{

}

bool ADatabase::isValid()
{
    return !d.isNull();
}

void ADatabase::open(std::function<void(bool error, const QString &fff)> cb)
{
    if (d.isNull()) {
        d = QSharedPointer<ADatabasePrivate>(new ADatabasePrivate(QString()));
    }

    if (!d->driver->isOpen()) {
        d->driver->open(cb);
    } else {
        if (cb) {
            cb(false, QString());
        }
    }
}

ADatabase::State ADatabase::state() const
{
    Q_ASSERT(d);
    return d->driver->state();
}

void ADatabase::onStateChanged(std::function<void (ADatabase::State, const QString &)> cb)
{
    Q_ASSERT(d);
    d->driver->onStateChanged(cb);
}

bool ADatabase::isOpen() const
{
    Q_ASSERT(d);
    return d->driver->isOpen();
}

void ADatabase::begin(AResultFn cb, QObject *receiver)
{
    Q_ASSERT(d);
    d->driver->begin(d, cb, receiver);
}

void ADatabase::commit(AResultFn cb, QObject *receiver)
{
    Q_ASSERT(d);
    d->driver->commit(d, cb, receiver);
}

void ADatabase::rollback(AResultFn cb, QObject *receiver)
{
    Q_ASSERT(d);
    d->driver->rollback(d, cb, receiver);
}

void ADatabase::exec(const QString &query, AResultFn cb, QObject *receiver)
{
    Q_ASSERT(d);
    d->driver->exec(d, query, QVariantList(), cb, receiver);
}

void ADatabase::execPrepared(const APreparedQuery &query, AResultFn cb, QObject *receiver)
{
    Q_ASSERT(d);
    d->driver->exec(d, query, QVariantList(), cb, receiver);
}

void ADatabase::exec(const QString &query, const QVariantList &params, AResultFn cb, QObject *receiver)
{
    Q_ASSERT(d);
    d->driver->exec(d, query, params, cb, receiver);
}

void ADatabase::execPrepared(const APreparedQuery &query, const QVariantList &params, AResultFn cb, QObject *receiver)
{
    Q_ASSERT(d);
    d->driver->exec(d, query, params, cb, receiver);
}

void ADatabase::setLastQuerySingleRowMode()
{
    Q_ASSERT(d);
    d->driver->setLastQuerySingleRowMode();
}

void ADatabase::subscribeToNotification(const QString &channel, ANotificationFn cb, QObject *receiver)
{
    Q_ASSERT(d);
    d->driver->subscribeToNotification(d, channel, cb, receiver);
}

void ADatabase::unsubscribeFromNotification(const QString &channel, QObject *receiver)
{
    Q_ASSERT(d);
    d->driver->unsubscribeFromNotification(d, channel, receiver);
}

ADatabase &ADatabase::operator =(const ADatabase &copy)
{
    d = copy.d;
    return *this;
}

ADatabasePrivate::ADatabasePrivate(const QString &ci)
    : connectionInfo(ci)
{
    if (ci.startsWith(QStringLiteral("postgres://")) || ci.startsWith(QStringLiteral("postgresql://"))) {
        driver = new ADriverPg;
        driver->setConnectionInfo(ci);
    } else {
        driver = new ADriver;
    }
}

ADatabasePrivate::~ADatabasePrivate()
{
    if (driver) {
        driver->deleteLater();
    }
}
