/* 
 * SPDX-FileCopyrightText: (C) 2020 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "acache.h"
#include "adatabase.h"
#include "apool.h"
#include "aresult.h"

#include <QDateTime>
#include <QPointer>

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(ASQL_CACHE, "asql.cache", QtWarningMsg)

namespace ASql {

struct ACacheReceiverCb {
    AResultFn cb;
    QPointer<QObject> receiver;
    QObject *checkReceiver = nullptr;
};

struct ACacheValue {
    QString query;
    QVariantList args;
    std::shared_ptr<QObject> cancellable;
    std::vector<ACacheReceiverCb> receivers;
    AResult result;
    qint64 hasResultTs = 0;
};

class ACachePrivate
{
public:
    enum class DbSource {
        Unset,
        Database,
        Pool,
    };

    bool searchOrQueue(QStringView query, qint64 maxAgeMs, const QVariantList &args, AResultFn cb, QObject *receiver);
    void requestData(const QString &query, qint64 maxAgeMs, const QVariantList &args, AResultFn cb, QObject *receiver);

    QString poolName;
    ADatabase db;
    QMultiHash<QStringView, ACacheValue> cache;
    DbSource dbSource = DbSource::Unset;
};

bool ACachePrivate::searchOrQueue(QStringView query, qint64 maxAgeMs, const QVariantList &args, AResultFn cb, QObject *receiver)
{
    auto it = cache.find(query);
    while (it != cache.end() && it.key() == query) {
        auto &value = *it;
        if (value.args == args) {
            if (value.hasResultTs) {
                if (maxAgeMs != -1) {
                    const qint64 cutAge = QDateTime::currentMSecsSinceEpoch() - maxAgeMs;
                    if (value.hasResultTs < cutAge) {
                        cache.erase(it);
                        break;
                    } else {
                        qDebug(ASQL_CACHE) << "cached data ready" << query;
                        if (cb) {
                            cb(value.result);
                        }
                    }
                } else {
                    qDebug(ASQL_CACHE) << "cached data ready" << query;
                    if (cb) {
                        cb(value.result);
                    }
                }
            } else {
                qDebug(ASQL_CACHE) << "queuing request" << query;
                // queue another request
                value.receivers.emplace_back(ACacheReceiverCb {
                                                 cb,
                                                 receiver,
                                                 receiver
                                             });
            }

            return true;
        }
        ++it;
    }

    return false;
}

void ACachePrivate::requestData(const QString &query, qint64 maxAgeMs, const QVariantList &args, AResultFn cb, QObject *receiver)
{
    qCDebug(ASQL_CACHE) << "requesting data" << query << int(dbSource);

    ADatabase _db;
    if (dbSource == ACachePrivate::DbSource::Database) {
        _db = db;
    } else if (dbSource == ACachePrivate::DbSource::Pool) {
        _db = APool::database(poolName);
    } else {
        qCCritical(ASQL_CACHE) << "Pool database was not set" << int(dbSource);
        AResult result;
        cb(result);
        return;
    }

    ACacheValue _value;
    _value.query = query;
    _value.args = args;
    _value.cancellable = std::make_shared<QObject>();
    _value.receivers.emplace_back(ACacheReceiverCb {
                                      cb,
                                      receiver,
                                      receiver
                                  });
    cache.insert(query, _value);

    _db.exec(query, args, [query, args, this] (AResult &result) {
        auto it = cache.find(query);
        while (it != cache.end() && it.key() == query) {
            ACacheValue &value = it.value();
            if (value.args == args) {
                value.result = result;
                value.hasResultTs = QDateTime::currentMSecsSinceEpoch();
                qInfo(ASQL_CACHE) << "got request data, dispatching to" << value.receivers.size() << "receivers" << query;
                for (const ACacheReceiverCb &receiverObj : value.receivers) {
                    if (receiverObj.checkReceiver == nullptr || !receiverObj.receiver.isNull()) {
                        qDebug(ASQL_CACHE) << "dispatching to receiver" << receiverObj.checkReceiver << query;
                        receiverObj.cb(result);
                    }
                }
                value.receivers.clear();
            }
            ++it;
        }
    }, _value.cancellable.get());
}

}

using namespace ASql;

ACache::ACache(QObject *parent) : QObject(parent)
  , d_ptr(new ACachePrivate)
{

}

ACache::~ACache() = default;

void ACache::setDatabasePool(const QString &poolName)
{
    Q_D(ACache);
    d->poolName = poolName;
    d->db = ADatabase();
    d->dbSource = ACachePrivate::DbSource::Pool;
}

void ACache::setDatabasePool(QStringView poolName)
{
    ACache::setDatabasePool(poolName.toString());
}

void ACache::setDatabase(const ADatabase &db)
{
    Q_D(ACache);
    d->poolName.clear();
    d->db = db;
    d->dbSource = ACachePrivate::DbSource::Database;
}

bool ACache::clear(QStringView query, const QVariantList &params)
{
    Q_D(ACache);
    auto it = d->cache.constFind(query);
    while (it != d->cache.constEnd() && it.key() == query) {
        if (it.value().args == params) {
            d->cache.erase(it);
            return true;
        }
        ++it;
    }
//    qDebug(ASQL_CACHE) << "cleared" << ret << "cache entries" << query << params;
    return false;
}

bool ACache::expire(qint64 maxAgeMs, QStringView query, const QVariantList &params)
{
    Q_D(ACache);
    int ret = false;
    const qint64 cutAge = QDateTime::currentMSecsSinceEpoch() - maxAgeMs;
    auto it = d->cache.constFind(query);
    while (it != d->cache.constEnd() && it.key() == query) {
        const ACacheValue &value = *it;
        if (value.args == params) {
            if (value.hasResultTs && value.hasResultTs < cutAge) {
                ret = true;
                qDebug(ASQL_CACHE) << "clearing cache" << query << params;
                d->cache.erase(it);
            }
            break;
        }
        ++it;
    }
    return ret;
}

int ACache::expireAll(qint64 maxAgeMs)
{
    Q_D(ACache);
    int ret = 0;
    const qint64 cutAge = QDateTime::currentMSecsSinceEpoch() - maxAgeMs;
    auto it = d->cache.begin();
    while (it != d->cache.end()) {
        const ACacheValue &value = *it;
        if (value.hasResultTs && value.hasResultTs < cutAge) {
            it = d->cache.erase(it);
            ++ret;
        } else {
            ++it;
        }
    }
    return ret;
}

void ACache::exec(QStringView query, AResultFn cb, QObject *receiver)
{
    execExpiring(query, -1, {}, cb, receiver);
}

void ACache::exec(QStringView query, const QVariantList &args, AResultFn cb, QObject *receiver)
{
    execExpiring(query, -1, args, cb, receiver);
}

void ACache::execExpiring(QStringView query, qint64 maxAgeMs, AResultFn cb, QObject *receiver)
{
    execExpiring(query, maxAgeMs, {}, cb, receiver);
}

void ACache::execExpiring(QStringView query, qint64 maxAgeMs, const QVariantList &args, AResultFn cb, QObject *receiver)
{
    Q_D(ACache);
    if (!d->searchOrQueue(query, maxAgeMs, args, cb, receiver)) {
        d->requestData(query.toString(), maxAgeMs, args, cb, receiver);
    }
}

void ACache::exec(const QString &query, AResultFn cb, QObject *receiver)
{
    execExpiring(query, -1, {}, cb, receiver);
}

void ACache::exec(const QString &query, const QVariantList &args, AResultFn cb, QObject *receiver)
{
    execExpiring(query, -1, args, cb, receiver);
}

void ACache::execExpiring(const QString &query, qint64 maxAgeMs, AResultFn cb, QObject *receiver)
{
    execExpiring(query, maxAgeMs, {}, cb, receiver);
}

void ACache::execExpiring(const QString &query, qint64 maxAgeMs, const QVariantList &args, AResultFn cb, QObject *receiver)
{
    Q_D(ACache);
    if (!d->searchOrQueue(query, maxAgeMs, args, cb, receiver)) {
        d->requestData(query, maxAgeMs, args, cb, receiver);
    }
}

#include "moc_acache.cpp"
