#include "atransaction.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(ASQL_TRANSACTION, "asql.transaction", QtInfoMsg)

class ATransactionPrivate
{
public:
    ~ATransactionPrivate() {
        if (running && db.isValid()) {
            qInfo(ASQL_TRANSACTION, "Rolling back transaction");
            db.rollback();
        }
    }

    ADatabase db;
    bool running = false;
};

ATransaction::ATransaction()
{
}

ATransaction::~ATransaction()
{

}

ATransaction::ATransaction(const ADatabase &db)
{
    auto priv = new ATransactionPrivate;
    priv->db = db;
    d = QSharedPointer<ATransactionPrivate>(priv);
}

ATransaction::ATransaction(const ATransaction &other)
{
    d = other.d;
}

ADatabase ATransaction::database() const
{
    return d->db;
}

ATransaction &ATransaction::operator =(const ATransaction &copy)
{
    d = copy.d;
    return *this;
}

void ATransaction::begin(AResultFn cb, QObject *receiver)
{
    Q_ASSERT(!d.isNull());
    if (!d->running) {
        d->running = true;
        d->db.begin(cb, receiver);
    } else {
        qWarning(ASQL_TRANSACTION, "Transaction already started");
    }
}

void ATransaction::commit(AResultFn cb, QObject *receiver)
{
    Q_ASSERT(!d.isNull());
    if (d->running) {
        d->running = false;
        d->db.commit(cb, receiver);
    } else {
        qWarning(ASQL_TRANSACTION, "Transaction not started");
    }
}

void ATransaction::rollback(AResultFn cb, QObject *receiver)
{
    Q_ASSERT(!d.isNull());
    if (d->running) {
        d->running = false;
        d->db.rollback(cb, receiver);
    } else {
        qWarning(ASQL_TRANSACTION, "Transaction not started");
    }
}
