/* 
 * SPDX-FileCopyrightText: (C) 2020 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef ADRIVER_H
#define ADRIVER_H

#include <QString>
#include <QSocketNotifier>

#include <adatabase.h>

#include <functional>

class AResult;
class APreparedQuery;
class ADriver : public QObject
{
    Q_OBJECT
public:
    ADriver();
    virtual ~ADriver() = default;

    QString connectionInfo() const;
    void setConnectionInfo(const QString &connectionInfo);

    virtual void open(std::function<void(bool isOpen, const QString &error)> cb);

    virtual ADatabase::State state() const;
    virtual void onStateChanged(std::function<void(ADatabase::State state, const QString &status)> cb);

    virtual bool isOpen() const;

    virtual void begin(const std::shared_ptr<ADatabasePrivate> &db, AResultFn cb, QObject *receiver);
    virtual void commit(const std::shared_ptr<ADatabasePrivate> &db, AResultFn cb, bool now, QObject *receiver);
    virtual void rollback(const std::shared_ptr<ADatabasePrivate> &db, AResultFn cb, bool now, QObject *receiver);

    virtual void exec(const std::shared_ptr<ADatabasePrivate> &db, const QString &query, const QVariantList &params, AResultFn cb, QObject *receiver);
    virtual void exec(const std::shared_ptr<ADatabasePrivate> &db, QStringView query, const QVariantList &params, AResultFn cb, QObject *receiver);
    virtual void exec(const std::shared_ptr<ADatabasePrivate> &db, const APreparedQuery &query, const QVariantList &params, AResultFn cb, QObject *receiver);

    virtual void setLastQuerySingleRowMode();

    virtual void subscribeToNotification(const std::shared_ptr<ADatabasePrivate> &db, const QString &name, ANotificationFn cb, QObject *receiver);
    virtual QStringList subscribedToNotifications() const;
    virtual void unsubscribeFromNotification(const std::shared_ptr<ADatabasePrivate> &db, const QString &name);

private:
    QString m_info;
};

#endif // ADRIVER_H
