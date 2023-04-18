/*
 * SPDX-FileCopyrightText: (C) 2020 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "atransaction.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(ASQL_TRANSACTION, "asql.transaction", QtInfoMsg)

namespace ASql {

class ATransactionPrivate
{
public:
    ATransactionPrivate(ADatabase _db)
        : db(_db)
    {
    }
    ~ATransactionPrivate()
    {
        if (running && db.isValid()) {
            qInfo(ASQL_TRANSACTION, "Rolling back transaction");
            db.rollback();
        }
    }

    ADatabase db;
    bool running = false;
};

} // namespace ASql

using namespace ASql;

ATransaction::ATransaction() = default;

ATransaction::~ATransaction() = default;

ATransaction::ATransaction(const ADatabase &db)
    : d(std::make_shared<ATransactionPrivate>(db))
{
}

ATransaction::ATransaction(const ATransaction &other)
    : d(other.d)
{
}

ATransaction::ATransaction(ATransaction &&other) noexcept
    : d(std::move(other.d))
{
}

ADatabase ATransaction::database() const
{
    return d->db;
}

ATransaction &ATransaction::operator=(const ATransaction &copy)
{
    d = copy.d;
    return *this;
}

void ATransaction::begin(QObject *receiver, AResultFn cb)
{
    Q_ASSERT(d);
    if (!d->running) {
        d->running = true;
        d->db.begin(receiver, cb);
    } else {
        qWarning(ASQL_TRANSACTION, "Transaction already started");
    }
}

void ATransaction::commit(QObject *receiver, AResultFn cb)
{
    Q_ASSERT(d);
    if (d->running) {
        d->running = false;
        d->db.commit(receiver, cb);
    } else {
        qWarning(ASQL_TRANSACTION, "Transaction not started");
    }
}

void ATransaction::rollback(QObject *receiver, AResultFn cb)
{
    Q_ASSERT(d);
    if (d->running) {
        d->running = false;
        d->db.rollback(receiver, cb);
    } else {
        qWarning(ASQL_TRANSACTION, "Transaction not started");
    }
}
