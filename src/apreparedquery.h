/*
 * SPDX-FileCopyrightText: (C) 2020-2023 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <asql_export.h>

#include <QString>

namespace ASql {

#define APreparedQueryLiteral(str) \
    ([]() Q_DECL_NOEXCEPT -> APreparedQuery { \
        static const APreparedQuery aprepared_literal_temp(str); \
        return aprepared_literal_temp; \
    }()) /**/

/*!
 * \brief The APreparedQuery class
 *
 * This class holds an identification to a prepared query, there
 * are 3 ways for correct usage:
 * * Creating it as a member of some object class.
 * * Creating it as a static variable before using it with a database.
 * * Using \sa APreparedQueryLiteral() macro which creates a static object.
 *
 * This class doesn't prepare the query on the database so it's thread-safe,
 * once a database executes this prepared query it will check if the query was
 * prepared, if not it will prepare, on successful preparation it will store
 * the indication internally to know this was prepared already.
 *
 * This way this object can be seem as a simple generator of unique prepared
 * statements identifiers (always prefixed with asql_), you can however for debugging
 * purposes manually set the identification.
 */
class ASQL_EXPORT APreparedQuery
{
public:
    /*!
     * \brief APreparedQuery constructs an empty and invalid prepared query object
     */
    APreparedQuery();

    /*!
     * \brief APreparedQuery constructs a prepared \p query with an automatic unique identification
     * \param query
     */
    APreparedQuery(QStringView query);

    /*!
     * \brief APreparedQuery constructs a prepared \p query with an automatic unique identification
     * from UTF-8 \param query
     */
    APreparedQuery(QUtf8StringView query);

    /*!
     * \brief APreparedQuery constructs a prepared \p query with a manual \p identification
     * \note \p identification should be used for debugging purposes, use negative values.
     * \param query
     */
    APreparedQuery(QStringView query, int identification);

    /*!
     * \brief APreparedQuery constructs a prepared \p query with a manual \p identification from
     * UTF-8 \param query
     * \note \p identification should be used for debugging purposes, use negative values.
     */
    APreparedQuery(QUtf8StringView query, int identification);

    QByteArray query() const;
    int identification() const;

private:
    QByteArray m_query;
    int m_identification;
};

} // namespace ASql
