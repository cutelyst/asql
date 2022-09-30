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

    ATransaction(ATransaction &&other) noexcept;

    ADatabase database() const;

    ATransaction &operator =(const ATransaction &copy);

    ATransaction &operator =(ATransaction &&other) noexcept
    {
        std::swap(d, other.d);
        return *this;
    }

    /*!
     * \brief begin a transaction, this operation usually succeeds,
     * but one can hook up a callback to check it's result.
     *
     * \param cb
     */
    void begin(QObject *receiver = nullptr, AResultFn cb = {});

    /*!
     * \brief commit a transaction, this operation usually succeeds,
     * but one can hook up a callback to check it's result.
     * \note the commit will get into the front of the query queue as best as possible
     *
     * \param cb
     */
    void commit(QObject *receiver = nullptr, AResultFn cb = {});

    /*!
     * \brief rollback a transaction, this operation usually succeeds,
     * but one can hook up a callback to check it's result.
     * \note the commit will get into the front of the query queue as best as possible
     *
     * \param cb
     */
    void rollback(QObject *receiver = nullptr, AResultFn cb = {});

private:
    std::shared_ptr<ATransactionPrivate> d;
};

}

#endif // ATRANSACTION_H
