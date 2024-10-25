/*
 * SPDX-FileCopyrightText: (C) 2020-2023 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */
#pragma once

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

    ATransaction &operator=(const ATransaction &copy);

    ATransaction &operator=(ATransaction &&other) noexcept
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
     * \brief commit a transaction only if our usage count equals 1
     *
     * Because we are implicit shared we can see if this is the last
     * reference to this object, this call can be make multiple times
     * but it will be ignored if it was not using the last reference.
     *
     * This is so that you can call this on a INSERT loop and have
     * COMMIT being issued only on the last INSERT result.
     *
     * \param cb
     */
    void commit(QObject *receiver = nullptr, AResultFn cb = {});

    /*!
     * \brief rollback a transaction, this operation usually succeeds,
     * but one can hook up a callback to check it's result.
     *
     * It might be useful to explict rollback a transaction in case
     * we want to reuse the database connection for a query not
     * related to the transaction.
     *
     * \param cb
     */
    void rollback(QObject *receiver = nullptr, AResultFn cb = {});

private:
    std::shared_ptr<ATransactionPrivate> d;
};

} // namespace ASql
