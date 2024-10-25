/*
 * SPDX-FileCopyrightText: (C) 2024 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <adatabase.h>
#include <asqlexports.h>

namespace ASql {

using AResultCommitFn = std::function<void(AResult &result, bool rollback)>;

class ATransactionCommitPrivate;
class ASQL_EXPORT ATransactionCommit
{
public:
    ATransactionCommit();
    ~ATransactionCommit();

    /**
     * @brief ATransactionCommit
     *
     * A RAII style class that automatically performs COMMITS unless told otherwise.
     *
     * Since we usually have several queries to be performed to the database,
     * keeping track when the last one succeeded is harder than just signaling
     * that we are rolling back.
     *
     * When COMMIT or ROLLBACK is performed the callback is called with the
     * result and if it was a rollback.
     *
     **/
    ATransactionCommit(ADatabase db, QObject *receiver = nullptr, AResultCommitFn cb = {});

    ATransactionCommit(const ATransactionCommit &other);

    ATransactionCommit(ATransactionCommit &&other) noexcept;

    ADatabase database() const;

    ATransactionCommit &operator=(const ATransactionCommit &copy);

    ATransactionCommit &operator=(ATransactionCommit &&other) noexcept
    {
        std::swap(d, other.d);
        return *this;
    }

    /*!
     * \brief begin a transaction, this operation usually succeeds,
     * but one can hook up a callback to check it's result.
     *
     * @param db
     * @param receiver this can turn into a rollback in case it was deleted before COMMIT.
     * @param cb called when we BEGIN/COMMIT/ROLLBACK.
     */
    void begin(QObject *receiver = nullptr, AResultFn cb = {});

    /*!
     * \brief Sets this object to perform a rollback instead
     */
    void rollback();

    /*!
     * \brief Returns true if this object is going to perform a rollback instead
     */
    [[nodiscard]] bool isRollback();

private:
    std::shared_ptr<ATransactionCommitPrivate> d;
};

} // namespace ASql
