/*
 * SPDX-FileCopyrightText: (C) 2024 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "atransactioncommit.h"

#include "aresult.h"

#include <QLoggingCategory>
#include <QPointer>

Q_LOGGING_CATEGORY(ASQL_TRANSACTION, "asql.transaction", QtInfoMsg)

namespace ASql {

class ATransactionCommitPrivate
{
public:
    ATransactionCommitPrivate(ADatabase _db, QObject *_receiver, AResultCommitFn _cb)
        : db{_db}
        , receiver{_receiver}
        , cb{_cb}
    {
    }
    ~ATransactionCommitPrivate()
    {
        if (running && db.isValid()) {
            AResultCommitFn callback = cb;
            if (Q_UNLIKELY(rollback)) {
                qInfo(ASQL_TRANSACTION, "Rolling back transaction");
                db.rollback(receiver, [callback](AResult &result) {
                    if (callback) {
                        callback(result, true);
                    }
                });
            } else {
                qInfo(ASQL_TRANSACTION, "Commiting transaction");
                db.commit(receiver, [callback](AResult &result) {
                    if (callback) {
                        callback(result, false);
                    }
                });
            }
        }
    }

    ADatabase db;
    QPointer<QObject> receiver;
    AResultCommitFn cb;
    bool running  = false;
    bool rollback = false;
};

} // namespace ASql

using namespace ASql;

ATransactionCommit::ATransactionCommit() = default;

ATransactionCommit::~ATransactionCommit() = default;

ATransactionCommit::ATransactionCommit(ADatabase db, QObject *receiver, AResultCommitFn cb)
    : d{std::make_shared<ATransactionCommitPrivate>(db, receiver, cb)}
{
}

ATransactionCommit::ATransactionCommit(const ATransactionCommit &other)
    : d(other.d)
{
}

ATransactionCommit::ATransactionCommit(ATransactionCommit &&other) noexcept
    : d(std::move(other.d))
{
}

ADatabase ATransactionCommit::database() const
{
    return d->db;
}

void ATransactionCommit::begin(QObject *receiver, AResultFn cb)
{
    Q_ASSERT(d);
    if (!d->running) {
        QPointer<QObject> receiverPtr = receiver;
        bool receiverWasSet           = !receiverPtr.isNull();

        auto priv = d;
        d->db.begin(receiver, [priv, receiverPtr, receiverWasSet, cb](AResult &result) {
            priv->running = !result.error();
            if (cb && (!receiverWasSet || !receiverPtr.isNull())) {
                cb(result);
            }
        });
    } else {
        qWarning(ASQL_TRANSACTION, "Transaction already started");
    }
}

ATransactionCommit &ATransactionCommit::operator=(const ATransactionCommit &copy)
{
    d = copy.d;
    return *this;
}

void ATransactionCommit::rollback()
{
    Q_ASSERT(d);
    d->rollback = true;
}

bool ATransactionCommit::isRollback()
{
    Q_ASSERT(d);
    return d->rollback;
}
