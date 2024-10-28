/*
 * SPDX-FileCopyrightText: (C) 2020-2023 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <adatabase.h>
#include <asqlexports.h>
#include <chrono>

#include <QObject>

namespace ASql {

template <typename T>
class ACoroExpected;

using AExpectedResult = ACoroExpected<AResult>;

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
    bool expire(std::chrono::milliseconds maxAge,
                QStringView query,
                const QVariantList &params = {});
    int expireAll(std::chrono::milliseconds maxAge);

    /*!
     * \brief size of the cache
     * \return the number of entries in the cache
     */
    [[nodiscard]] int size() const;

    AExpectedResult coExec(QStringView query, QObject *receiver = nullptr);
    AExpectedResult
        coExec(QStringView query, const QVariantList &args, QObject *receiver = nullptr);
    AExpectedResult coExecExpiring(QStringView query,
                                   std::chrono::milliseconds maxAge,
                                   QObject *receiver = nullptr);
    AExpectedResult coExecExpiring(QStringView query,
                                   std::chrono::milliseconds maxAge,
                                   const QVariantList &args,
                                   QObject *receiver = nullptr);

    void exec(QStringView query, QObject *receiver, AResultFn cb);
    void exec(QStringView query, const QVariantList &args, QObject *receiver, AResultFn cb);
    void execExpiring(QStringView query,
                      std::chrono::milliseconds maxAge,
                      QObject *receiver,
                      AResultFn cb);
    void execExpiring(QStringView query,
                      std::chrono::milliseconds maxAge,
                      const QVariantList &args,
                      QObject *receiver,
                      AResultFn cb);

    void exec(const QString &query, QObject *receiver, AResultFn cb);
    void exec(const QString &query, const QVariantList &args, QObject *receiver, AResultFn cb);
    void execExpiring(const QString &query,
                      std::chrono::milliseconds maxAge,
                      QObject *receiver,
                      AResultFn cb);
    void execExpiring(const QString &query,
                      std::chrono::milliseconds maxAge,
                      const QVariantList &args,
                      QObject *receiver,
                      AResultFn cb);

private:
    ACachePrivate *d_ptr;
};

} // namespace ASql
