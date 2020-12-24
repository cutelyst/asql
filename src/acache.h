/* 
 * SPDX-FileCopyrightText: (C) 2020 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef ACACHE_H
#define ACACHE_H

#include <QObject>

#include <adatabase.h>

#include <aqsqlexports.h>

class ACachePrivate;
class ASQL_EXPORT ACache : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(ACache)
public:
    explicit ACache(QObject *parent = nullptr);

    void setDatabasePool(const QString &poolName);
    void setDatabase(const ADatabase &db);

    /*!
     * \brief clear que requested query from the cache, do not call this from the exec callback
     * \param query
     * \param params
     * \return
     */
    bool clear(const QString &query, const QVariantList &params = QVariantList());
    bool expire(qint64 maxAgeMs, const QString &query, const QVariantList &params = QVariantList());
    int expireAll(qint64 maxAgeMs);

    void exec(const QString &query, AResultFn cb, QObject *receiver = nullptr);
    void exec(const QString &query, const QVariantList &params, AResultFn cb, QObject *receiver = nullptr);
    void exec(const QString &query, qint64 maxAgeMs, AResultFn cb, QObject *receiver = nullptr);
    void exec(const QString &query, qint64 maxAgeMs, const QVariantList &params, AResultFn cb, QObject *receiver = nullptr);

private:
    ACachePrivate *d_ptr;
};

#endif // ACACHE_H
