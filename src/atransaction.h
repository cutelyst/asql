/* 
 * SPDX-FileCopyrightText: (C) 2020 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef ATRANSACTION_H
#define ATRANSACTION_H

#include <QSharedPointer>

#include <adatabase.h>
#include <aqsqlexports.h>

class ATransactionPrivate;
class ASQL_EXPORT ATransaction
{
public:
    ATransaction();
    ~ATransaction();

    ATransaction(const ADatabase &db);

    ATransaction(const ATransaction &other);

    ADatabase database() const;

    ATransaction &operator =(const ATransaction &copy);

    /*!
     * \brief begin a transaction, this operation usually succeeds,
     * but one can hook up a callback to check it's result.
     *
     * \param cb
     */
    void begin(AResultFn cb = {}, QObject *receiver = nullptr);

    /*!
     * \brief commit a transaction, this operation usually succeeds,
     * but one can hook up a callback to check it's result.
     *
     * \param cb
     */
    void commit(AResultFn cb = {}, QObject *receiver = nullptr);

    /*!
     * \brief rollback a transaction, this operation usually succeeds,
     * but one can hook up a callback to check it's result.
     *
     * \param cb
     */
    void rollback(AResultFn cb = {}, QObject *receiver = nullptr);

private:
    QSharedPointer<ATransactionPrivate> d;
};

#endif // ATRANSACTION_H
