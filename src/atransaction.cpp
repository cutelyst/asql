/*
 * SPDX-FileCopyrightText: (C) 2020 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "atransaction.h"

#include "acoroexpected.h"

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
            qDebug(ASQL_TRANSACTION, "Rolling back transaction");
            std::ignore = db.rollback();
        }
    }

    ADatabase db;
    bool running = false;
};

} // namespace ASql

using namespace ASql;

ATransaction::ATransaction() = default;

ATransaction::~ATransaction() = default;

ATransaction::ATransaction(const ADatabase &db, bool started)
    : ATransaction{db}
{
    d->running = started;
}

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

AExpectedResult ATransaction::begin(QObject *receiver)
{
    Q_ASSERT(d);
    if (!d->running) {
        d->running = true;
        return d->db.begin(receiver);
    } else {
        qWarning(ASQL_TRANSACTION, "Transaction already started");
    }

    return {receiver};
}

AExpectedResult ATransaction::commit(QObject *receiver)
{
    Q_ASSERT(d);
    if (d->running) {
        if (d.use_count() == 1) {
            d->running = false;
            return d->db.commit(receiver);
        }
    } else {
        qWarning(ASQL_TRANSACTION, "Transaction not started");
    }

    return {receiver};
}

AExpectedResult ATransaction::rollback(QObject *receiver)
{
    Q_ASSERT(d);
    if (d->running) {
        d->running = false;
        return d->db.rollback(receiver);
    } else {
        qWarning(ASQL_TRANSACTION, "Transaction not started");
    }

    return {receiver};
}

bool ATransaction::isActive() const
{
    Q_ASSERT(d);
    return d->running;
}
