/* 
 * SPDX-FileCopyrightText: (C) 2020-2022 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef ADRIVER_H
#define ADRIVER_H

#include <QString>
#include <QSocketNotifier>

#include <adatabase.h>

#include <functional>

#include <asqlexports.h>

namespace ASql {

class AResult;
class APreparedQuery;
class ASQL_EXPORT ADriver : public QObject
{
    Q_OBJECT
public:
    ADriver();
    ADriver(const QString &connectionInfo);
    virtual ~ADriver() = default;

    QString connectionInfo() const;

    virtual bool isValid() const;
    virtual void open(std::function<void(bool isOpen, const QString &error)> cb);

    virtual ADatabase::State state() const;
    virtual void onStateChanged(std::function<void(ADatabase::State state, const QString &status)> cb);

    virtual bool isOpen() const;

    virtual void begin(const std::shared_ptr<ADriver> &driver, AResultFn cb, QObject *receiver);
    virtual void commit(const std::shared_ptr<ADriver> &driver, AResultFn cb, QObject *receiver);
    virtual void rollback(const std::shared_ptr<ADriver> &driver, AResultFn cb, QObject *receiver);

    virtual void exec(const std::shared_ptr<ADriver> &driver, const QString &query, const QVariantList &params, AResultFn cb, QObject *receiver);
    virtual void exec(const std::shared_ptr<ADriver> &driver, QStringView query, const QVariantList &params, AResultFn cb, QObject *receiver);
    virtual void exec(const std::shared_ptr<ADriver> &driver, const APreparedQuery &query, const QVariantList &params, AResultFn cb, QObject *receiver);

    virtual void setLastQuerySingleRowMode();

    virtual void subscribeToNotification(const std::shared_ptr<ADriver> &driver, const QString &name, ANotificationFn cb, QObject *receiver);
    virtual QStringList subscribedToNotifications() const;
    virtual void unsubscribeFromNotification(const std::shared_ptr<ADriver> &driver, const QString &name);

private:
    QString m_info;
};

}

#endif // ADRIVER_H
