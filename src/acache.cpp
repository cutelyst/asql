#include "acache.h"
#include "adatabase.h"
#include "aresult.h"

#include <QPointer>

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(ASQL_CACHE, "asql.cache", QtInfoMsg)

typedef std::pair<QString, QVariantList> ACacheKey;

typedef struct {
    AResultFn cb;
    QPointer<QObject> receiver;
    QObject *checkReceiver = nullptr;
} ACacheReceiverCb;

typedef struct {
    AResult result;
    std::vector<ACacheReceiverCb> receivers;
    bool hasResult = false;
} ACacheValue;

class ACachePrivate
{
public:
    ADatabase db;
    QMap<ACacheKey, ACacheValue> cache;
};

ACache::ACache(QObject *parent) : QObject(parent)
  , d_ptr(new ACachePrivate)
{

}

void ACache::setDatabase(const ADatabase &db)
{
    Q_D(ACache);
    d->db = db;
}

bool ACache::clear(const QString &query, const QVariantList &params)
{
    Q_D(ACache);
    int ret = d->cache.remove({query, params});
    qDebug(ASQL_CACHE) << "clearing cache" << ret << query;
    return ret;
}

void ACache::exec(const QString &query, AResultFn cb, QObject *receiver)
{
    exec(query, QVariantList(), cb, receiver);
}

void ACache::exec(const QString &query, const QVariantList &params, AResultFn cb, QObject *receiver)
{
    Q_D(ACache);
    auto it = d->cache.find({query, params});
    if (it != d->cache.end()) {
        ACacheValue &value = it.value();
        if (value.hasResult) {
            qDebug(ASQL_CACHE) << "cached data ready" << query;
            if (cb) {
                cb(value.result);
                value.result.setAt(-1);
            }
        } else {
            qDebug(ASQL_CACHE) << "data was requested already" << query;
            // queue another request
            ACacheReceiverCb receiverObj;
            receiverObj.cb = cb;
            receiverObj.receiver = receiver;
            receiverObj.checkReceiver = receiver;
            value.receivers.push_back(receiverObj);
        }
    } else {
        qDebug(ASQL_CACHE) << "requesting data" << query;
        ACacheValue value;
        ACacheReceiverCb receiverObj;
        receiverObj.cb = cb;
        receiverObj.receiver = receiver;
        receiverObj.checkReceiver = receiver;
        value.receivers.push_back(receiverObj);
        d->cache.insert({query, params}, value);

        auto dbFn = [=] (AResult &result) {
            auto it = d->cache.find({query, params});
            if (it != d->cache.end()) {
                 ACacheValue &value = it.value();
                 value.result = result;
                 value.hasResult = true;
                 qDebug(ASQL_CACHE) << "got request data, dispatching to receivers" << value.receivers.size() << query;
                 for (const ACacheReceiverCb &receiverObj : value.receivers) {
                     if (receiverObj.checkReceiver == nullptr || !receiverObj.receiver.isNull()) {
                         qDebug(ASQL_CACHE) << "dispatching to receiver" << receiverObj.checkReceiver << query;
                         receiverObj.cb(result);
                         result.setAt(-1);
                     }
                 }
                 value.receivers.clear();
            }
        };

        if (params.isEmpty()) {
            d->db.exec(query, dbFn, this);
        } else {
            d->db.exec(query, params, dbFn, this);
        }
    }
}
