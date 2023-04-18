/*
 * SPDX-FileCopyrightText: (C) 2020 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef ACACHE_H
#define ACACHE_H

#include <adatabase.h>
#include <asqlexports.h>

#include <QObject>

namespace ASql {

class ACachePrivate;
class ASQL_EXPORT ACache : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(ACache)
public:
    explicit ACache(QObject *parent = nullptr);
    virtual ~ACache();

    void setDatabasePool(const QString &poolName);
    void setDatabasePool(QStringView poolName);
    void setDatabase(const ADatabase &db);

    /*!
     * \brief clear que requested query from the cache, do not call this from the exec callback
     * \param query
     * \param params
     * \return
     */
    bool clear(QStringView query, const QVariantList &params = {});
    bool expire(qint64 maxAgeMs, QStringView query, const QVariantList &params = {});
    int expireAll(qint64 maxAgeMs);

    void exec(QStringView query, QObject *receiver, AResultFn cb);
    void exec(QStringView query, const QVariantList &args, QObject *receiver, AResultFn cb);
    void execExpiring(QStringView query, qint64 maxAgeMs, QObject *receiver, AResultFn cb);
    void execExpiring(QStringView query, qint64 maxAgeMs, const QVariantList &args, QObject *receiver, AResultFn cb);

    void exec(const QString &query, QObject *receiver, AResultFn cb);
    void exec(const QString &query, const QVariantList &args, QObject *receiver, AResultFn cb);
    void execExpiring(const QString &query, qint64 maxAgeMs, QObject *receiver, AResultFn cb);
    void execExpiring(const QString &query, qint64 maxAgeMs, const QVariantList &args, QObject *receiver, AResultFn cb);

private:
    ACachePrivate *d_ptr;
};

} // namespace ASql

#endif // ACACHE_H
