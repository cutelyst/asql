/*
 * SPDX-FileCopyrightText: (C) 2020 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "acache.h"

#include "acoroexpected.h"
#include "adatabase.h"
#include "apool.h"
#include "aresult.h"

#include <optional>

#include <QLoggingCategory>
#include <QPointer>

Q_LOGGING_CATEGORY(ASQL_CACHE, "asql.cache", QtWarningMsg)

using namespace std::chrono;

namespace ASql {

struct ACacheReceiverCb {
    AResultFn cb;
    QPointer<QObject> receiver;
    QObject *checkReceiver = nullptr;

    void emitResult(AResult result) const
    {
        if (cb && (checkReceiver == nullptr || !receiver.isNull())) {
            cb(result);
        }
    }
};

struct ACacheValue {
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

    bool searchOrQueue(const QString &query,
                       std::chrono::milliseconds maxAge,
                       const QVariantList &args,
                       QObject *receiver,
                       AResultFn cb);
    ACoroTerminator requestData(QString query, QVariantList args, QObject *receiver, AResultFn cb);

    QObject *q_ptr;
    QString poolName;
    ADatabase db;

    // For some unknown reason sometimes when QMultiHash<QStringView
    // is used and we try to find(query) an entry the iterator returned
    // is not the latest inserted, and our while loop to find the
    // matching ACacheValue.args will not find it and silently ignore
    // the return causing the caller to "leak" as it's stuck untill
    // the cache is cleaned.
    // With QString that does not happen, and eventually in Qt 6.8 we
    // can use the view to do lookups.
    QMultiHash<QString, ACacheValue> cache;
    DbSource dbSource = DbSource::Unset;
};

bool ACachePrivate::searchOrQueue(const QString &query,
                                  std::chrono::milliseconds maxAge,
                                  const QVariantList &args,
                                  QObject *receiver,
                                  AResultFn cb)
{
    auto it = cache.constFind(query);
    while (it != cache.constEnd() && it.key() == query) {
        auto &value = *it;
        if (value.args == args) {
            if (value.hasResultTP.has_value()) {
                if (maxAge >= 0ms) {
                    const auto cutAge = steady_clock::now() - maxAge;
                    if (value.hasResultTP.value() < cutAge) {
                        qDebug(ASQL_CACHE) << "Expiring cache" << query.left(15) << args;
                        cache.erase(it);
                        break;
                    }
                }

                qDebug(ASQL_CACHE) << "Cached query ready" << query.left(15) << args;
                if (cb) {
                    cb(value.result);
                }
            } else {
                qDebug(ASQL_CACHE) << "Queuing request" << query.left(15) << args;
                // queue another request
                value.receivers.emplace_back(ACacheReceiverCb{cb, receiver, receiver});
            }

            return true;
        }
        ++it;
    }

    return false;
}

ACoroTerminator
    ACachePrivate::requestData(QString query, QVariantList args, QObject *receiver, AResultFn cb)
{
    qCDebug(ASQL_CACHE) << "Requesting data" << query.left(15) << args << int(dbSource);
    co_yield q_ptr;

    ACacheReceiverCb cacheReceiver{cb, receiver, receiver};

    ADatabase localDb;
    switch (dbSource) {
    case ACachePrivate::DbSource::Database:
        localDb = db;
        break;
    case ACachePrivate::DbSource::Pool:
    {
        auto dbFromPool = co_await APool::coDatabase(q_ptr, poolName);
        if (!dbFromPool) {
            qCritical(ASQL_CACHE) << "Failed to get connection from pool" << dbFromPool.error();
            if (cb) {
                AResult result;
                cb(result);
            }
            co_return;
        }
        localDb = *dbFromPool;
        break;
    }
    default:
        qCCritical(ASQL_CACHE) << "Cache database source was not set";
        if (cb) {
            AResult result;
            cb(result);
        }
        co_return;
    }

    ACacheValue cacheValue;
    cacheValue.args = args;
    cacheValue.receivers.emplace_back(cacheReceiver);

    cache.emplace(query, std::move(cacheValue));

    auto result = co_await localDb.exec(query, args, q_ptr);
    bool found  = false;
    auto it     = cache.constFind(query);
    while (it != cache.constEnd() && it.key() == query) {
        ACacheValue &value = it.value();
        if (value.args == args) {
            value.result      = *result;
            value.hasResultTP = steady_clock::now();

            // Copy the receivers as the callback call might invalidade the cache
            std::vector<ACacheReceiverCb> receivers = std::move(value.receivers);
            value.receivers.clear();

            qDebug(ASQL_CACHE) << "Got request data, dispatching to" << receivers.size()
                               << "receivers" << query.left(15) << args;
            for (const ACacheReceiverCb &receiverObj : receivers) {
                qDebug(ASQL_CACHE) << "Dispatching to receiver" << receiverObj.checkReceiver
                                   << query.left(15) << args;
                receiverObj.emitResult(*result);
            }
            found = true;

            break;
        }
        ++it;
    }

    if (!found) {
        qWarning(ASQL_CACHE) << "Queued request not found" << query.left(15) << args;
        AResult result;
        cacheReceiver.emitResult(result);
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

void ACache::setDatabase(const ADatabase &db)
{
    Q_D(ACache);
    d->poolName.clear();
    d->db       = db;
    d->dbSource = ACachePrivate::DbSource::Database;
}

bool ACache::clear(const QString &query, const QVariantList &params)
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

bool ACache::expire(std::chrono::milliseconds maxAge,
                    const QString &query,
                    const QVariantList &params)
{
    Q_D(ACache);
    int ret           = false;
    const auto cutAge = steady_clock::now() - maxAge;
    auto it           = d->cache.constFind(query);
    while (it != d->cache.constEnd() && it.key() == query) {
        const ACacheValue &value = *it;
        if (value.args == params) {
            if (value.hasResultTP.has_value() && value.hasResultTP.value() < cutAge) {
                ret = true;
                // qDebug(ASQL_CACHE) << "clearing cache" << query << params;
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
        if (value.hasResultTP.has_value() && value.hasResultTP.value() < cutAge) {
            it = d->cache.erase(it);
            ++ret;
        } else {
            ++it;
        }
    }
    // qDebug(ASQL_CACHE) << "cleared cache" << ret;
    return ret;
}

int ACache::size() const
{
    Q_D(const ACache);
    return d->cache.size();
}

AExpectedResult ACache::coExec(const QString &query, QObject *receiver)
{
    AExpectedResult coro(receiver);
    execExpiring(query, -1ms, {}, receiver, coro.callback);
    return coro;
}

AExpectedResult ACache::coExec(const QString &query, const QVariantList &args, QObject *receiver)
{
    AExpectedResult coro(receiver);
    execExpiring(query, -1ms, args, receiver, coro.callback);
    return coro;
}

AExpectedResult ACache::coExecExpiring(const QString &query,
                                       std::chrono::milliseconds maxAge,
                                       QObject *receiver)
{
    AExpectedResult coro(receiver);
    execExpiring(query, maxAge, {}, receiver, coro.callback);
    return coro;
}

AExpectedResult ACache::coExecExpiring(const QString &query,
                                       std::chrono::milliseconds maxAge,
                                       const QVariantList &args,
                                       QObject *receiver)
{
    AExpectedResult coro(receiver);
    execExpiring(query, maxAge, args, receiver, coro.callback);
    return coro;
}

void ACache::exec(const QString &query, QObject *receiver, AResultFn cb)
{
    execExpiring(query, -1ms, {}, receiver, cb);
}

void ACache::exec(const QString &query, const QVariantList &args, QObject *receiver, AResultFn cb)
{
    execExpiring(query, -1ms, args, receiver, cb);
}

void ACache::execExpiring(const QString &query,
                          std::chrono::milliseconds maxAge,
                          QObject *receiver,
                          AResultFn cb)
{
    execExpiring(query, maxAge, {}, receiver, cb);
}

void ACache::execExpiring(const QString &query,
                          std::chrono::milliseconds maxAge,
                          const QVariantList &args,
                          QObject *receiver,
                          AResultFn cb)
{
    Q_D(ACache);
    if (!d->searchOrQueue(query, maxAge, args, receiver, cb)) {
        d->requestData(query, args, receiver, cb);
    }
}

#include "moc_acache.cpp"
