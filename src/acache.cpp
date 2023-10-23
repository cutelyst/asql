/*
 * SPDX-FileCopyrightText: (C) 2020 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "acache.h"

#include "adatabase.h"
#include "apool.h"
#include "aresult.h"

#include <QLoggingCategory>
#include <QPointer>

#include <optional>

Q_LOGGING_CATEGORY(ASQL_CACHE, "asql.cache", QtWarningMsg)

using namespace std::chrono;

namespace ASql {

struct ACacheReceiverCb {
    AResultFn cb;
    QPointer<QObject> receiver;
    QObject *checkReceiver = nullptr;
};

struct ACacheValue {
    QString query;
    QVariantList args;
    std::vector<ACacheReceiverCb> receivers;
    AResult result;
    std::optional<time_point<steady_clock>> hasResultTP;
};

class ACachePrivate
{
public:
    enum class DbSource {
        Unset,
        Database,
        Pool,
    };

    bool searchOrQueue(QStringView query, std::chrono::milliseconds maxAge, const QVariantList &args, QObject *receiver, AResultFn cb);
    void requestData(const QString &query, const QVariantList &args, QObject *receiver, AResultFn cb);

    QObject *q_ptr;
    QString poolName;
    ADatabase db;
    QMultiHash<QStringView, ACacheValue> cache;
    DbSource dbSource = DbSource::Unset;
};

bool ACachePrivate::searchOrQueue(QStringView query, std::chrono::milliseconds maxAge, const QVariantList &args, QObject *receiver, AResultFn cb)
{
    auto it = cache.find(query);
    while (it != cache.end() && it.key() == query) {
        auto &value = *it;
        if (value.args == args) {
            if (value.hasResultTP) {
                if (maxAge != -1ms) {
                    const auto cutAge = steady_clock::now() - maxAge;
                    if (value.hasResultTP < cutAge) {
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
                value.receivers.emplace_back(ACacheReceiverCb{
                    cb,
                    receiver,
                    receiver});
            }

            return true;
        }
        ++it;
    }

    return false;
}

void ACachePrivate::requestData(const QString &query, const QVariantList &args, QObject *receiver, AResultFn cb)
{
    qCDebug(ASQL_CACHE) << "requesting data" << query << int(dbSource);

    ACacheValue cacheValue;
    cacheValue.query = query;
    cacheValue.args  = args;
    cacheValue.receivers.emplace_back(ACacheReceiverCb{
        cb,
        receiver,
        receiver});

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    cache.emplace(query, std::move(cacheValue));
#else
    cache.insert(query, cacheValue);
#endif

    auto performQuery = [this, query, args](ADatabase db) {
        db.exec(query, args, q_ptr, [query, args, this](AResult &result) {
            auto it = cache.find(query);
            while (it != cache.end() && it.key() == query) {
                ACacheValue &value = it.value();
                if (value.args == args) {
                    value.result      = result;
                    value.hasResultTP = steady_clock::now();
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
        });
    };

    switch (dbSource) {
    case ACachePrivate::DbSource::Database:
        performQuery(db);
        break;
    case ACachePrivate::DbSource::Pool:
        APool::database(q_ptr, performQuery, poolName);
        break;
    default:
        qCCritical(ASQL_CACHE) << "Pool database was not set" << int(dbSource);
        AResult result;
        cb(result);
    }
}

} // namespace ASql

using namespace ASql;

ACache::ACache(QObject *parent)
    : QObject(parent)
    , d_ptr(new ACachePrivate)
{
    d_ptr->q_ptr = this;
}

ACache::~ACache() = default;

void ACache::setDatabasePool(const QString &poolName)
{
    Q_D(ACache);
    d->poolName = poolName;
    d->db       = ADatabase();
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
    d->db       = db;
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

bool ACache::expire(std::chrono::milliseconds maxAge, QStringView query, const QVariantList &params)
{
    Q_D(ACache);
    int ret           = false;
    const auto cutAge = steady_clock::now() - maxAge;
    auto it           = d->cache.constFind(query);
    while (it != d->cache.constEnd() && it.key() == query) {
        const ACacheValue &value = *it;
        if (value.args == params) {
            if (value.hasResultTP && value.hasResultTP < cutAge) {
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

int ACache::expireAll(std::chrono::milliseconds maxAge)
{
    Q_D(ACache);
    int ret           = 0;
    const auto cutAge = steady_clock::now() - maxAge;
    auto it           = d->cache.begin();
    while (it != d->cache.end()) {
        const ACacheValue &value = *it;
        if (value.hasResultTP && value.hasResultTP < cutAge) {
            it = d->cache.erase(it);
            ++ret;
        } else {
            ++it;
        }
    }
    return ret;
}

int ACache::size() const
{
    Q_D(const ACache);
    return d->cache.size();
}

void ACache::exec(QStringView query, QObject *receiver, AResultFn cb)
{
    execExpiring(query, -1ms, {}, receiver, cb);
}

void ACache::exec(QStringView query, const QVariantList &args, QObject *receiver, AResultFn cb)
{
    execExpiring(query, -1ms, args, receiver, cb);
}

void ACache::execExpiring(QStringView query, std::chrono::milliseconds maxAge, QObject *receiver, AResultFn cb)
{
    execExpiring(query, maxAge, {}, receiver, cb);
}

void ACache::execExpiring(QStringView query, std::chrono::milliseconds maxAge, const QVariantList &args, QObject *receiver, AResultFn cb)
{
    Q_D(ACache);
    if (!d->searchOrQueue(query, maxAge, args, receiver, cb)) {
        d->requestData(query.toString(), args, receiver, cb);
    }
}

void ACache::exec(const QString &query, QObject *receiver, AResultFn cb)
{
    execExpiring(query, -1ms, {}, receiver, cb);
}

void ACache::exec(const QString &query, const QVariantList &args, QObject *receiver, AResultFn cb)
{
    execExpiring(query, -1ms, args, receiver, cb);
}

void ACache::execExpiring(const QString &query, std::chrono::milliseconds maxAge, QObject *receiver, AResultFn cb)
{
    execExpiring(query, maxAge, {}, receiver, cb);
}

void ACache::execExpiring(const QString &query, std::chrono::milliseconds maxAge, const QVariantList &args, QObject *receiver, AResultFn cb)
{
    Q_D(ACache);
    if (!d->searchOrQueue(query, maxAge, args, receiver, cb)) {
        d->requestData(query, args, receiver, cb);
    }
}

#include "moc_acache.cpp"
