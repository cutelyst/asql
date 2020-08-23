#include "atransaction.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(ASQL_TRANSACTION, "asql.transaction", QtInfoMsg)

class ATransactionPrivate
{
public:
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
    d = QSharedPointer<ATransactionPrivate>(priv, [=] (ATransactionPrivate *priv) {
        if (priv->running) {
            priv->db.rollback();
            qDebug(ASQL_TRANSACTION, "Rolling back transaction");
        }
    });
}

ATransaction::ATransaction(const ATransaction &other)
{
    d = other.d;
}

ATransaction &ATransaction::operator =(const ATransaction &copy)
{
    d = copy.d;
    return *this;
}

void ATransaction::begin(AResultFn cb)
{
    Q_ASSERT(!d.isNull());
    if (!d->running) {
        d->running = true;
        d->db.begin(cb);
    } else {
        qWarning(ASQL_TRANSACTION, "Transaction already started");
    }
}

void ATransaction::commit(AResultFn cb)
{
    Q_ASSERT(!d.isNull());
    if (d->running) {
        d->running = false;
        d->db.commit(cb);
    } else {
        qWarning(ASQL_TRANSACTION, "Transaction not started");
    }
}

void ATransaction::rollback(AResultFn cb)
{
    Q_ASSERT(!d.isNull());
    if (d->running) {
        d->running = false;
        d->db.commit(cb);
    } else {
        qWarning(ASQL_TRANSACTION, "Transaction not started");
    }
}
