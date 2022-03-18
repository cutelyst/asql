/* 
 * SPDX-FileCopyrightText: (C) 2020-2022 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef ATRANSACTION_H
#define ATRANSACTION_H

#include <adatabase.h>
#include <asqlexports.h>

namespace ASql {

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
     * \note the commit will get into the front of the query queue as best as possible
     *
     * \param cb
     */
    void commit(AResultFn cb = {}, QObject *receiver = nullptr);

    /*!
     * \brief rollback a transaction, this operation usually succeeds,
     * but one can hook up a callback to check it's result.
     * \note the commit will get into the front of the query queue as best as possible
     *
     * \param cb
     */
    void rollback(AResultFn cb = {}, QObject *receiver = nullptr);

private:
    std::shared_ptr<ATransactionPrivate> d;
};

}

#endif // ATRANSACTION_H
