/*
 * SPDX-FileCopyrightText: (C) 2020-2022 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <adatabase.h>
#include <asql_export.h>
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
    virtual QString driverName() const;

    virtual bool isValid() const;
    virtual void
        open(const std::shared_ptr<ADriver> &driver, QObject *receiver, ADatabaseOpenFn cb);

    virtual ADatabase::State state() const;
    virtual void onStateChanged(QObject *receiver, ADatabase::StateChangedFn cb);

    virtual bool isOpen() const;

    virtual void begin(const std::shared_ptr<ADriver> &driver, QObject *receiver, AResultFn cb);
    virtual void commit(const std::shared_ptr<ADriver> &driver, QObject *receiver, AResultFn cb);
    virtual void rollback(const std::shared_ptr<ADriver> &driver, QObject *receiver, AResultFn cb);

    virtual void exec(const std::shared_ptr<ADriver> &driver,
                      QUtf8StringView query,
                      QObject *receiver,
                      AResultFn cb);

    virtual void exec(const std::shared_ptr<ADriver> &driver,
                      QStringView query,
                      QObject *receiver,
                      AResultFn cb);

    virtual void exec(const std::shared_ptr<ADriver> &driver,
                      QUtf8StringView query,
                      const QVariantList &params,
                      QObject *receiver,
                      AResultFn cb);

    virtual void exec(const std::shared_ptr<ADriver> &driver,
                      QStringView query,
                      const QVariantList &params,
                      QObject *receiver,
                      AResultFn cb);

    virtual void exec(const std::shared_ptr<ADriver> &driver,
                      const APreparedQuery &query,
                      const QVariantList &params,
                      QObject *receiver,
                      AResultFn cb);

    virtual void setLastQuerySingleRowMode();

    virtual bool enterPipelineMode(std::chrono::milliseconds timeout);

    virtual bool exitPipelineMode();

    virtual ADatabase::PipelineStatus pipelineStatus() const;

    virtual bool pipelineSync();

    virtual int queueSize() const;

    virtual void subscribeToNotification(const std::shared_ptr<ADriver> &driver,
                                         const QString &name,
                                         QObject *receiver,
                                         ANotificationFn cb);
    virtual QStringList subscribedToNotifications() const;
    virtual void unsubscribeFromNotification(const std::shared_ptr<ADriver> &driver,
                                             const QString &name);

private:
    QString m_info;
};

} // namespace ASql
