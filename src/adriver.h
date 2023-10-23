/*
 * SPDX-FileCopyrightText: (C) 2020-2022 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef ADRIVER_H
#define ADRIVER_H

#include <adatabase.h>
#include <asqlexports.h>
#include <functional>

#include <QSocketNotifier>
#include <QString>

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
    virtual void open(QObject *receiver, std::function<void(bool isOpen, const QString &error)> cb);

    virtual ADatabase::State state() const;
    virtual void onStateChanged(QObject *receiver, std::function<void(ADatabase::State state, const QString &status)> cb);

    virtual bool isOpen() const;

    virtual void begin(const std::shared_ptr<ADriver> &driver, QObject *receiver, AResultFn cb);
    virtual void commit(const std::shared_ptr<ADriver> &driver, QObject *receiver, AResultFn cb);
    virtual void rollback(const std::shared_ptr<ADriver> &driver, QObject *receiver, AResultFn cb);

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    virtual void exec(const std::shared_ptr<ADriver> &driver, QUtf8StringView query, const QVariantList &params, QObject *receiver, AResultFn cb);
#endif
    virtual void exec(const std::shared_ptr<ADriver> &driver, QStringView query, const QVariantList &params, QObject *receiver, AResultFn cb);

    virtual void exec(const std::shared_ptr<ADriver> &driver, const APreparedQuery &query, const QVariantList &params, QObject *receiver, AResultFn cb);

    virtual void setLastQuerySingleRowMode();

    virtual bool enterPipelineMode(qint64 autoSyncMS);

    virtual bool exitPipelineMode();

    virtual ADatabase::PipelineStatus pipelineStatus() const;

    virtual bool pipelineSync();

    virtual int queueSize() const;

    virtual void subscribeToNotification(const std::shared_ptr<ADriver> &driver, const QString &name, QObject *receiver, ANotificationFn cb);
    virtual QStringList subscribedToNotifications() const;
    virtual void unsubscribeFromNotification(const std::shared_ptr<ADriver> &driver, const QString &name);

private:
    QString m_info;
};

} // namespace ASql

#endif // ADRIVER_H
