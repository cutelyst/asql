/*
 * SPDX-FileCopyrightText: (C) 2020 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "adriverpg.h"

#include "aresult.h"

#include <QLoggingCategory>
#include <QThread>
#include <QDate>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QUuid>
#include <QtEndian>
#include <QTimer>

#include <libpq-fe.h>

Q_LOGGING_CATEGORY(ASQL_PG, "asql.pg", QtInfoMsg)

// workaround for postgres defining their OIDs in a private header file
#define QBOOLOID 16
#define QINT8OID 20
#define QINT2OID 21
#define QINT4OID 23
#define QTEXTOID 25
#define QNUMERICOID 1700
#define QFLOAT4OID 700
#define QFLOAT8OID 701
#define QABSTIMEOID 702
#define QRELTIMEOID 703
#define QUNKNOWNOID 705
#define QDATEOID 1082
#define QTIMEOID 1083
#define QTIMETZOID 1266
#define QTIMESTAMPOID 1114
#define QTIMESTAMPTZOID 1184
#define QOIDOID 2278
#define QBYTEAOID 17
#define QREGPROCOID 24
#define QXIDOID 28
#define QCIDOID 29
#define QJSONBOID 3802
#define QUUIDOID 2950
#define QBITOID 1560
#define QVARBITOID 1562

#define VARHDRSZ 4

using namespace ASql;

ADriverPg::ADriverPg(const QString &connInfo) : ADriver(connInfo)
{

}

ADriverPg::~ADriverPg()
{
    if (m_conn) {
        PQfinish(m_conn);
    }
}

bool ADriverPg::isValid() const
{
    return true;
}

QString connectionStatus(ConnStatusType type) {
    switch (type) {
    case CONNECTION_OK:
        return QStringLiteral("CONNECTION_OK");
    case CONNECTION_BAD:
        return QStringLiteral("CONNECTION_BAD");
    case CONNECTION_STARTED:
        return QStringLiteral("CONNECTION_STARTED");
    case CONNECTION_AWAITING_RESPONSE:
        return QStringLiteral("CONNECTION_AWAITING_RESPONSE");
    case CONNECTION_AUTH_OK:
        return QStringLiteral("CONNECTION_AUTH_OK");

    default:
        return QStringLiteral("STATUS: ").arg(type);
    }
}

void ADriverPg::open(std::function<void(bool, const QString &)> cb)
{
    qDebug(ASQL_PG) << "Open" << connectionInfo();

    m_conn = PQconnectStart(connectionInfo().toUtf8().constData());
    if (m_conn) {
        const auto socket = PQsocket(m_conn);
        if (socket > 0) {
            m_writeNotify = new QSocketNotifier(socket, QSocketNotifier::Write, this);
            m_readNotify = new QSocketNotifier(socket, QSocketNotifier::Read, this);

            const QString error = QString::fromLocal8Bit(PQerrorMessage(m_conn));
            setState(ADatabase::State::Connecting, error);

            auto connFn = [=]  {
                PostgresPollingStatusType type = PQconnectPoll(m_conn);
//                qDebug(ASQL_PG) << "poll" << type << "status" << connectionStatus(PQstatus(m_conn));
                switch (type) {
                case PGRES_POLLING_READING:
//                    qDebug(ASQL_PG) << "PGRES_POLLING_READING" << type;
                    return;
                case PGRES_POLLING_WRITING:
                    qDebug(ASQL_PG) << "PGRES_POLLING_WRITING 1" << type << m_writeNotify->isEnabled();
                    m_writeNotify->setEnabled(true);
                    qDebug(ASQL_PG) << "PGRES_POLLING_WRITING 2" << type << m_writeNotify->isEnabled();
                    return;
                case PGRES_POLLING_OK:
                    qDebug(ASQL_PG) << "PGRES_POLLING_OK 1" << type << m_writeNotify->isEnabled();
                    m_writeNotify->setEnabled(false);
                    qDebug(ASQL_PG) << "PGRES_POLLING_OK 2" << type << m_writeNotify->isEnabled();
                    m_connected = true;
                    if (cb) {
                        cb(true, QString());
                    }

                    setState(ADatabase::State::Connected, QString());

                    // see if we have queue queries
                    nextQuery();
                    return;
                case PGRES_POLLING_FAILED:
                {
                    const QString error = QString::fromLocal8Bit(PQerrorMessage(m_conn));
                    qDebug(ASQL_PG) << "PGRES_POLLING_FAILED" << type << error;
                    finishConnection();

                    if (cb) {
                        cb(false, error);
                    }
                    setState(ADatabase::State::Disconnected, error);
                    return;
                }
                default:
                    qDebug(ASQL_PG) << "PGRES_POLLING : " << type;
                    break;
                }
            };

            connect(m_writeNotify, &QSocketNotifier::activated, this, [=] {
//                qDebug(ASQL_PG) << "PG write" << connectionStatus(PQstatus(m_conn)) << PQisBusy(m_conn);
                m_writeNotify->setEnabled(false);
                if (!m_connected) {
                    connFn();
                } else if (m_flush) {
                    m_flush = false;
                    cmdFlush();
                }
            });

            connect(m_readNotify, &QSocketNotifier::activated, this, [=] {
//                qDebug(ASQL_PG) << "PG read" << this;
                if (!m_connected) {
                    connFn();
                } else {
                    if (PQconsumeInput(m_conn) == 1) {
                        while (PQisBusy(m_conn) == 0) {
                            PGresult *result = PQgetResult(m_conn);

//                            qCWarning(ASQL_PG) << "Not busy: RESULT" << result << "queue" << m_queuedQueries.size();

                            if (result) {
#ifdef LIBPQ_HAS_PIPELINING
                                ExecStatusType status = PQresultStatus(result);
                                if (status == PGRES_PIPELINE_SYNC) {
                                    --m_pipelineSync;
                                    PQclear(result);
                                    continue;
                                }
#endif

                                APGQuery &pgQuery = m_queuedQueries.head();
//                                qDebug(ASQL_PG) << "RESULT" << result << "status" << status << PGRES_TUPLES_OK << "shared_ptr result" << pgQuery.result;
                                if (pgQuery.result->m_result) {
                                    // when we had already had a result it means we should emit the
                                    // first one and keep waiting till a null result is returned
                                    pgQuery.result->m_lastResultSet = false;
                                    pgQuery.done();

                                    // allocate a new result
                                    pgQuery.result = std::make_shared<AResultPg>();
                                }
                                pgQuery.result->m_result = result;
                                pgQuery.result->processResult();
                                continue;
                            } else if (m_queuedQueries.size()) {
                                APGQuery &pgQuery = m_queuedQueries.head();
                                m_queryRunning = false;

                                if (Q_UNLIKELY(pgQuery.prepared && pgQuery.preparing)) {
                                    if (Q_UNLIKELY(pgQuery.result->error())) {
                                        // PREPARE OR PREPARED QUERY ERROR
                                        auto query = m_queuedQueries.dequeue();
                                        nextQuery();
                                        query.done();
                                    } else {
                                        // Query prepared
                                        m_preparedQueries.append(pgQuery.preparedQuery.identification());
                                        pgQuery.result = std::make_shared<AResultPg>();
                                        pgQuery.preparing = false;
                                        nextQuery();
                                    }
                                } else {
                                    auto query = m_queuedQueries.dequeue();
                                    nextQuery();
                                    query.done();
                                }
                            }

                            if (pipelineStatus() == ADatabase::PipelineStatus::Off || !m_pipelineSync) {
                                // In PIPELINE mode a null result means the end of a query
                                // but PQisBusy() should indicate it's end instead
                                break;
                            }
                        }
//                        qDebug(ASQL_PG) << "Not busy OUT" << this;

                        if (!m_queuedQueries.isEmpty() && m_pipelineSync == 0 &&
                                pipelineStatus() != ADatabase::PipelineStatus::Off && m_autoSyncTimer && !m_autoSyncTimer->isActive()) {
                            m_autoSyncTimer->start();
                        }

                        PGnotify *notify = nullptr;
                        while ((notify = PQnotifies(m_conn)) != nullptr) {
                            const QString name = QString::fromUtf8(notify->relname);
//                            qDebug(ASQL_PG) << "NOTIFICATION" << name << notify;

                            auto it = m_subscribedNotifications.constFind(name);
                            if (it != m_subscribedNotifications.constEnd()) {
                                if (it.value()) {
                                    QString payload;
                                    if (notify->extra) {
                                        payload = QString::fromUtf8(notify->extra);
                                    }
                                    const bool self = (notify->be_pid == PQbackendPID(m_conn)) ? true : false;
//                                qDebug(ASQL_PG) << "NOTIFICATION" << self << name << payload;
                                    it.value()(ADatabaseNotification{name, payload, self});
                                }
                            } else {
                                qWarning(ASQL_PG, "received notification for '%s' which isn't subscribed to.", qPrintable(name));
                            }

                            PQfreemem(notify);
                        }
                    } else {
                        const QString error = QString::fromLocal8Bit(PQerrorMessage(m_conn));
                        qDebug(ASQL_PG) << "CONSUME ERROR" <<  error << PQstatus(m_conn) << connectionStatus(PQstatus(m_conn));
                        if (PQstatus(m_conn) == CONNECTION_BAD) {
                            finishConnection();
                            finishQueries(error);

                            setState(ADatabase::State::Disconnected, error);
                        }
                    }
                }
            });
        }
//        qDebug(ASQL_PG) << "PG Socket" << m_conn << socket;
    } else {
        if (cb) {
            cb(false, QStringLiteral("PQconnectStart failed"));
        }
    }
}

bool ADriverPg::isOpen() const
{
    return m_connected;
}

void ADriverPg::setState(ADatabase::State state, const QString &status)
{
    m_state = state;
    if (m_stateChangedCb) {
        m_stateChangedCb(state, status);
    }
}

ADatabase::State ADriverPg::state() const
{
    return m_state;
}

void ADriverPg::onStateChanged(std::function<void (ADatabase::State, const QString &)> cb)
{
    m_stateChangedCb = cb;
}

void ADriverPg::begin(const std::shared_ptr<ADriver> &db, AResultFn cb, QObject *receiver)
{
    exec(db, QStringLiteral("BEGIN"), QVariantList(), cb, receiver);
}

void ADriverPg::commit(const std::shared_ptr<ADriver> &db, AResultFn cb, QObject *receiver)
{
    exec(db, QStringLiteral("COMMIT"), QVariantList(), cb, receiver);
}

void ADriverPg::rollback(const std::shared_ptr<ADriver> &db, AResultFn cb, QObject *receiver)
{
    exec(db, QStringLiteral("ROLLBACK"), QVariantList(), cb, receiver);
}

void ADriverPg::queryConstructed(APGQuery &pgQuery)
{
    if (pgQuery.checkReceiver) {
        connect(pgQuery.checkReceiver, &QObject::destroyed, this, [=] (QObject *obj) {
            if (m_queryRunning && !m_queuedQueries.empty() && m_queuedQueries.head().checkReceiver == obj) {
                PGcancel *cancel = PQgetCancel(m_conn);
                char errbuf[256];
                int ret = PQcancel(cancel, errbuf, 256);
                if (ret == 1) {
                    qDebug(ASQL_PG) << "PQcancel sent";
                } else {
                    qDebug(ASQL_PG) << "PQcancel failed" << ret << errbuf;
                }
                PQfreeCancel(cancel);
            }
//            qDebug(ASQL_PG) << "destroyed" << m_queryRunning << m_queuedQueries.empty() ;
        });
    }

    if (pipelineStatus() != ADatabase::PipelineStatus::On &&
            (m_queryRunning || !m_connected || m_queuedQueries.size() > 1)) {
        m_queuedQueries.append(pgQuery);
        return;
    }

    if (pgQuery.params.isEmpty()) {
        doExec(pgQuery);
    } else {
        doExecParams(pgQuery);
    }
    m_queuedQueries.append(pgQuery);

    if (pipelineStatus() != ADatabase::PipelineStatus::Off && m_autoSyncTimer && !m_autoSyncTimer->isActive()) {
        m_autoSyncTimer->start();
    }
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void ADriverPg::exec(const std::shared_ptr<ADriver> &db, QUtf8StringView query, const QVariantList &params, AResultFn cb, QObject *receiver)
{
    APGQuery pgQuery;
    pgQuery.query.setRawData(query.data(), query.size());
    pgQuery.params = params;
    pgQuery.cb = cb;
    selfDriver = db;
    pgQuery.receiver = receiver;
    pgQuery.checkReceiver = receiver;

    queryConstructed(pgQuery);
}
#endif

void ADriverPg::exec(const std::shared_ptr<ADriver> &db, QStringView query, const QVariantList &params, AResultFn cb, QObject *receiver)
{
    APGQuery pgQuery;
    pgQuery.query = query.toUtf8();
    pgQuery.params = params;
    pgQuery.cb = cb;
    selfDriver = db;
    pgQuery.receiver = receiver;
    pgQuery.checkReceiver = receiver;

    queryConstructed(pgQuery);
}

void ADriverPg::exec(const std::shared_ptr<ADriver> &db, const APreparedQuery &query, const QVariantList &params, AResultFn cb, QObject *receiver)
{
    APGQuery pgQuery;
    pgQuery.preparedQuery = query;
    pgQuery.params = params;
    pgQuery.cb = cb;
    selfDriver = db;
    pgQuery.receiver = receiver;
    pgQuery.checkReceiver = receiver;
    pgQuery.prepared = true;

    queryConstructed(pgQuery);
}

void ADriverPg::setLastQuerySingleRowMode()
{
    if (m_queuedQueries.size() == 1) {
        APGQuery &pgQuery = m_queuedQueries.head();
        pgQuery.setSingleRow = true;
        if (!pgQuery.preparing && m_state == ADatabase::State::Connected) {
            setSingleRowMode();
        }
    } else if (m_queuedQueries.size() > 1) {
        APGQuery &pgQuery = m_queuedQueries.last();
        pgQuery.setSingleRow = true;
    }
}

bool ADriverPg::enterPipelineMode(qint64 autoSyncMS)
{
#ifdef LIBPQ_HAS_PIPELINING
    // Refuse to enter Pipeline mode if we have queued queries
    if (m_connected && m_queuedQueries.isEmpty() && PQenterPipelineMode(m_conn) == 1) {
        if (autoSyncMS && !m_autoSyncTimer) {
            m_autoSyncTimer = new QTimer{this};
            m_autoSyncTimer->setInterval(autoSyncMS);
            m_autoSyncTimer->setSingleShot(autoSyncMS);
            connect(m_autoSyncTimer, &QTimer::timeout, this, &ADriverPg::pipelineSync);
        }
        return true;
    }
#endif

    return false;
}

bool ADriverPg::exitPipelineMode()
{
#ifdef LIBPQ_HAS_PIPELINING
    return m_connected && PQexitPipelineMode(m_conn) == 1;
#else
    return false;
#endif
}

ADatabase::PipelineStatus ADriverPg::pipelineStatus() const
{
#ifdef LIBPQ_HAS_PIPELINING
    if (m_connected) {
        return static_cast<ADatabase::PipelineStatus>(PQpipelineStatus(m_conn));
    }
#endif
    return ADatabase::PipelineStatus::Off;
}

bool ADriverPg::pipelineSync()
{
#ifdef LIBPQ_HAS_PIPELINING
    if (m_connected && PQpipelineSync(m_conn) == 1) {
        ++m_pipelineSync;
        return true;
    }
#endif
    return false;
}

void ADriverPg::subscribeToNotification(const std::shared_ptr<ADriver> &db, const QString &name, ANotificationFn cb, QObject *receiver)
{
    if (m_subscribedNotifications.contains(name)) {
        qWarning(ASQL_PG) << "Already subscribed to notification" << name;
        return;
    }

    m_subscribedNotifications.insert(name, cb);
    exec(db, QString(u"LISTEN " + name), {}, [=] (AResult &result) {
        qDebug(ASQL_PG) << "subscribed" << !result.error() << result.errorString();
        if (result.error()) {
            m_subscribedNotifications.remove(name);
        }
    }, receiver);

    connect(receiver, &QObject::destroyed, this, [=] {
        m_subscribedNotifications.remove(name);
    });
}

QStringList ADriverPg::subscribedToNotifications() const
{
    return m_subscribedNotifications.keys();
}

void ADriverPg::unsubscribeFromNotification(const std::shared_ptr<ADriver> &db, const QString &name)
{
    if (m_subscribedNotifications.remove(name)) {
        exec(db, QString(u"UNLISTEN " + name), {}, [=] (AResult &result) {
            qDebug(ASQL_PG) << "unsubscribed" << !result.error() << result.errorString();
        }, this);
    }
}

void ADriverPg::nextQuery()
{
    const bool pipelineOff = pipelineStatus() == ADatabase::PipelineStatus::Off;
    while (pipelineOff && !m_queuedQueries.isEmpty() && !m_queryRunning) {
        APGQuery &pgQuery = m_queuedQueries.head();
        if (pgQuery.checkReceiver && pgQuery.receiver.isNull()) {
            m_queuedQueries.dequeue();
        } else {
            if (pgQuery.params.isEmpty()) {
                doExec(pgQuery);
            } else {
                doExecParams(pgQuery);
            }
        }
    }

    if (m_queuedQueries.isEmpty()) {
        selfDriver = {};
    }
}

void ADriverPg::finishConnection()
{
    if (m_conn) {
        PQfinish(m_conn);
        m_conn = nullptr;
    }

    m_subscribedNotifications.clear();
    m_preparedQueries.clear();
    m_connected = false;
    m_pipelineSync = 0;
    delete m_autoSyncTimer;
    m_autoSyncTimer = nullptr;
    if (m_readNotify) {
        m_readNotify->setEnabled(false);
        m_readNotify->deleteLater();
        m_readNotify = nullptr;
    }
    if (m_writeNotify) {
        m_writeNotify->setEnabled(false);
        m_writeNotify->deleteLater();
        m_writeNotify = nullptr;
    }
}

void ADriverPg::finishQueries(const QString &error)
{
    while (!m_queuedQueries.isEmpty()) {
        APGQuery pgQuery = m_queuedQueries.dequeue();
        pgQuery.result->m_error = true;
        pgQuery.result->m_errorString = error;
        pgQuery.done();
    }
    selfDriver = {};
}

void ADriverPg::doExec(APGQuery &pgQuery)
{
    int ret;
    if (pgQuery.prepared) {
        bool isPrepared = m_preparedQueries.contains(pgQuery.preparedQuery.identification());
        if (!isPrepared) {
            ret = PQsendPrepare(m_conn,
                                pgQuery.preparedQuery.identification().constData(),
                                pgQuery.preparedQuery.query().constData(),
                                0,
                                nullptr); // perhaps later use binary results

            if (pipelineStatus() == ADatabase::PipelineStatus::On) {
                // pretend that it was prepared otherwise it can't be used in in the pipeline
                m_preparedQueries.append(pgQuery.preparedQuery.identification());
                isPrepared = true;
            }
            pgQuery.preparing = true;
        }

        if (isPrepared) {
            ret = PQsendQueryPrepared(m_conn,
                                      pgQuery.preparedQuery.identification().constData(),
                                      0,
                                      nullptr,
                                      nullptr,
                                      nullptr,
                                      0); // perhaps later use binary results
            if (pgQuery.setSingleRow) {
                setSingleRowMode();
            }
        }
    } else {
        ret = PQsendQuery(m_conn, pgQuery.query.constData());
    }

    if (ret == 1) {
        m_queryRunning = true;
        if (pgQuery.setSingleRow) {
            setSingleRowMode();
        }
        cmdFlush();
    } else {
        pgQuery.result->m_error = true;
        pgQuery.result->m_errorString = QString::fromLocal8Bit(PQerrorMessage(m_conn));
        m_queuedQueries.dequeue().done();
        if (m_queuedQueries.isEmpty()) {
            selfDriver = {};
        }
    }
}

void ADriverPg::doExecParams(APGQuery &pgQuery)
{
    const QVariantList &params = pgQuery.params;
    auto paramTypes = std::make_unique<Oid[]>(params.size());
    auto paramValues = std::make_unique<const char *[]>(params.size());
    auto paramLengths = std::make_unique<int[]>(params.size());
    auto paramFormats = std::make_unique<int[]>(params.size());

    QByteArrayList deleteLater;
    for (int i = 0; i < params.size(); ++i) {
        QVariant v = params[i];
        QByteArray data;
//        qDebug(ASQL_PG) << v << v.type() << v.isNull() << "---" << QString::number(v.toInt()).toLatin1().constData() << v.toString().isNull();
        if (!v.isNull()) {
            switch (v.userType()) {
            case QMetaType::QString:
            {
                const QString text = v.toString();
                paramTypes[i] = !text.isNull() ? QTEXTOID : QUNKNOWNOID;
                paramFormats[i] = 0;
                data = text.toUtf8();
            }
                break;
            case QMetaType::QByteArray:
                paramTypes[i] = QBYTEAOID;
                paramFormats[i] = 1;
                data = v.toByteArray();
                break;
            case QMetaType::Int:
                paramTypes[i] = QINT4OID;
                paramFormats[i] = 1;
            {
                const qint32 number = v.toInt();
                data.resize(4);
                qToBigEndian<qint32>(number, data.data());
            }
                break;
            case QMetaType::LongLong:
                paramTypes[i] = QINT8OID;
                paramFormats[i] = 1;
            {
                const qint64 number = v.toLongLong();
                data.resize(8);
                qToBigEndian<qint64>(number, data.data());
            }
                break;
            case QMetaType::QUuid:
                paramTypes[i] = QUUIDOID;
                paramFormats[i] = 1;
                data = v.toUuid().toRfc4122();
                break;
            case QMetaType::Bool:
                paramTypes[i] = QBOOLOID;
                paramFormats[i] = 1;
                data.append(v.toBool() ? 0x01 : 0x00);
                break;
            case QMetaType::UnknownType:
                paramTypes[i] = QUNKNOWNOID;
                paramFormats[i] = 0;
                break;
            case QMetaType::QJsonObject:
                paramTypes[i] = QJSONBOID;
                paramFormats[i] = 0;
                data = QJsonDocument(v.toJsonObject()).toJson(QJsonDocument::Compact);
                break;
            case QMetaType::QJsonArray:
                paramTypes[i] = QJSONBOID;
                paramFormats[i] = 0;
                data = QJsonDocument(v.toJsonArray()).toJson(QJsonDocument::Compact);
                break;
            case QMetaType::QJsonValue:
            {
                const QJsonValue jValue = v.toJsonValue();
                switch (jValue.type()) {
                case QJsonValue::Bool:
                    paramTypes[i] = QBOOLOID;
                    paramFormats[i] = 1;
                    data.append(jValue.toBool() ? 0x01 : 0x00);
                    break;;
                case QJsonValue::Double:
                    paramTypes[i] = QUNKNOWNOID; // This allows PG to try to deduce the type
                    paramFormats[i] = 0;
                    data = jValue.toVariant().toString().toLatin1();
                    break;
                case QJsonValue::String:
                {
                    const QString text = v.toString();
                    paramTypes[i] = !text.isNull() ? QTEXTOID : QUNKNOWNOID;
                    paramFormats[i] = 0;
                    data = jValue.toString().toUtf8();
                }
                    break;
                case QJsonValue::Array:
                    paramTypes[i] = QJSONBOID;
                    paramFormats[i] = 0;
                    data = QJsonDocument(jValue.toArray()).toJson(QJsonDocument::Compact);
                    break;
                case QJsonValue::Object:
                    paramTypes[i] = QJSONBOID;
                    paramFormats[i] = 0;
                    data = QJsonDocument(jValue.toObject()).toJson(QJsonDocument::Compact);
                    break;
                default:
                    paramTypes[i] = QUNKNOWNOID;
                    paramFormats[i] = 0;
                    paramValues[i] = nullptr;
                    paramLengths[i] = 0;
                }
            }
                break;
            case QMetaType::QJsonDocument:
                paramTypes[i] = QJSONBOID;
                paramFormats[i] = 0;
                data = v.toJsonDocument().toJson(QJsonDocument::Compact);
                break;
            default:
                paramTypes[i] = QUNKNOWNOID; // This allows PG to try to deduce the type
                paramFormats[i] = 0;
                data = v.toString().toUtf8();
            }

            if (data.size() || paramTypes[i] != QUNKNOWNOID) {
                deleteLater.append(data); // Otherwise our temporary data will be deleted
                paramValues[i] = data.constData();
                paramLengths[i] = data.size();
            } else {
                paramValues[i] = nullptr;
                paramLengths[i] = 0;
            }
        } else {
            paramTypes[i] = QUNKNOWNOID;
            paramFormats[i] = 0;
            paramValues[i] = nullptr;
            paramLengths[i] = 0;
        }
    }

    int ret;
    if (pgQuery.prepared) {
        bool isPrepared = m_preparedQueries.contains(pgQuery.preparedQuery.identification());
        if (!isPrepared) {
            ret = PQsendPrepare(m_conn,
                                pgQuery.preparedQuery.identification().constData(),
                                pgQuery.preparedQuery.query().constData(),
                                params.size(),
                                paramTypes.get());

            if (pipelineStatus() == ADatabase::PipelineStatus::On) {
                // pretend that it was prepared otherwise it can't be used in in the pipeline
                m_preparedQueries.append(pgQuery.preparedQuery.identification());
                isPrepared = true;
            }
            pgQuery.preparing = true;
        }

        if (isPrepared) {
            ret = PQsendQueryPrepared(m_conn,
                                      pgQuery.preparedQuery.identification().constData(),
                                      params.size(),
                                      paramValues.get(),
                                      paramLengths.get(),
                                      paramFormats.get(),
                                      0); // perhaps later use binary results
            if (pgQuery.setSingleRow) {
                setSingleRowMode();
            }
        }
    } else {
        ret = PQsendQueryParams(m_conn,
                                pgQuery.query.constData(),
                                params.size(),
                                paramTypes.get(),
                                paramValues.get(),
                                paramLengths.get(),
                                paramFormats.get(),
                                0); // perhaps later use binary results
        if (pgQuery.setSingleRow) {
            setSingleRowMode();
        }
    }
    cmdFlush();

    if (ret == 1) {
        m_queryRunning = true;
    } else {
        pgQuery.result->m_error = true;
        pgQuery.result->m_errorString = QString::fromLocal8Bit(PQerrorMessage(m_conn));
        m_queuedQueries.dequeue().done();
        if (m_queuedQueries.isEmpty()) {
            selfDriver = {};
        }
    }
}

void ADriverPg::setSingleRowMode()
{
    if (PQsetSingleRowMode(m_conn) != 1) {
        qWarning(ASQL_PG) << "Failed to set single row mode";
    }
}

void ADriverPg::cmdFlush()
{
    int ret = PQflush(m_conn);
    if (Q_UNLIKELY(ret == -1)) {
        qWarning(ASQL_PG) << "Failed to flush" << QString::fromLocal8Bit(PQerrorMessage(m_conn));
    } else if (Q_UNLIKELY(ret == 1)) {
        // Wait for write-ready and call it again
        m_flush = true;
        m_writeNotify->setEnabled(true);
    }
}

AResultPg::AResultPg() = default;

AResultPg::~AResultPg()
{
    PQclear(m_result);
}

bool AResultPg::lastResulSet() const
{
    return m_lastResultSet;
}

bool AResultPg::error() const
{
    return m_error;
}

QString AResultPg::errorString() const
{
    return m_errorString;
}

int AResultPg::size() const
{
    return PQntuples(m_result);
}

int AResultPg::fields() const
{
    return PQnfields(m_result);
}

int AResultPg::numRowsAffected() const
{
    return QString::fromLatin1(PQcmdTuples(m_result)).toInt();
}

int AResultPg::indexOfField(QLatin1String name) const
{
    for (int i = 0; i < fields(); ++i) {
        if (qstrcmp(name.data(), PQfname(m_result, i)) == 0) {
            return i;
        }
    }
    return -1;
}

QString AResultPg::fieldName(int column) const
{
    return QString::fromUtf8(PQfname(m_result, column));
}

static QMetaType::Type qDecodePSQLType(int t)
{
    QMetaType::Type type = QMetaType::UnknownType;
    switch (t) {
    case QBOOLOID:
        type = QMetaType::Bool;
        break;
    case QINT8OID:
        type = QMetaType::LongLong;
        break;
    case QINT2OID:
    case QINT4OID:
    case QOIDOID:
    case QREGPROCOID:
    case QXIDOID:
    case QCIDOID:
        type = QMetaType::Int;
        break;
    case QNUMERICOID:
    case QFLOAT4OID:
    case QFLOAT8OID:
        type = QMetaType::Double;
        break;
    case QABSTIMEOID:
    case QRELTIMEOID:
    case QDATEOID:
        type = QMetaType::QDate;
        break;
    case QTIMEOID:
    case QTIMETZOID:
        type = QMetaType::QTime;
        break;
    case QTIMESTAMPOID:
    case QTIMESTAMPTZOID:
        type = QMetaType::QDateTime;
        break;
    case QBYTEAOID:
        type = QMetaType::QByteArray;
        break;
    default:
        type = QMetaType::QString;
        break;
    }
//    qDebug(ASQL_PG) << "decode pg type" << t << type;
    return type;
}

QVariant AResultPg::value(int row, int column) const
{
    if (column >= PQnfields(m_result)) {
        qWarning(ASQL_PG, "column %d out of range", column);
        return {};
    }

    int ptype = PQftype(m_result, column);
    QMetaType::Type type = qDecodePSQLType(ptype);
    if (PQgetisnull(m_result, row, column))
        return QVariant(static_cast<QVariant::Type>(type));
    const char *val = PQgetvalue(m_result, row, column);
    switch (type) {
    case QMetaType::Bool:
        return QVariant((bool)(val[0] == 't'));
    case QMetaType::QString:
        return QString::fromUtf8(val);
    case QMetaType::LongLong:
        if (val[0] == '-')
            return QString::fromLatin1(val).toLongLong();
        else
            return QString::fromLatin1(val).toULongLong();
    case QMetaType::Int:
        return atoi(val);
    case QMetaType::Double: {
//        if (ptype == QNUMERICOID) {
//            if (numericalPrecisionPolicy() != QSql::HighPrecision) {
//                QVariant retval;
//                bool convert;
//                double dbl=QString::fromLatin1(val).toDouble(&convert);
//                if (numericalPrecisionPolicy() == QSql::LowPrecisionInt64)
//                    retval = (qlonglong)dbl;
//                else if (numericalPrecisionPolicy() == QSql::LowPrecisionInt32)
//                    retval = (int)dbl;
//                else if (numericalPrecisionPolicy() == QSql::LowPrecisionDouble)
//                    retval = dbl;
//                if (!convert)
//                    return QVariant();
//                return retval;
//            }
//            return QString::fromLatin1(val);
//        }
        if (qstricmp(val, "Infinity") == 0)
            return qInf();
        if (qstricmp(val, "-Infinity") == 0)
            return -qInf();
        return QString::fromLatin1(val).toDouble();
    }
    case QMetaType::QDate:
        if (val[0] == '\0') {
            return QDate();
        } else {
#ifndef QT_NO_DATESTRING
            return QDate::fromString(QString::fromLatin1(val), Qt::ISODate);
#else
            return QString::fromLatin1(val);
#endif
        }
    case QMetaType::QTime: {
        const QString str = QString::fromLatin1(val);
#ifndef QT_NO_DATESTRING
        if (str.isEmpty())
            return QTime();
        else
            return QTime::fromString(str, Qt::ISODate);
#else
        return QVariant(str);
#endif
    }
    case QMetaType::QDateTime: {
        QString dtval = QString::fromLatin1(val);
#ifndef QT_NO_DATESTRING
        if (dtval.length() < 10) {
            return QDateTime();
        } else {
            QChar sign = dtval[dtval.size() - 3];
            if (sign == QLatin1Char('-') || sign == QLatin1Char('+')) dtval += QLatin1String(":00");
            return QDateTime::fromString(dtval, Qt::ISODate);
        }
#else
        return dtval;
#endif
    }
    case QMetaType::QByteArray: {
        size_t len;
        unsigned char *data = PQunescapeBytea((const unsigned char*)val, &len);
        QByteArray ba(reinterpret_cast<const char *>(data), int(len));
        PQfreemem(data);
        return QVariant(ba);
    }
    default:
    case QMetaType::UnknownType:
        qWarning(ASQL_PG, "unknown data type");
    }
    return {};
}

bool AResultPg::isNull(int row, int column) const
{
    Q_ASSERT_X(column < PQnfields(m_result), "isNull", "column out of range");
    return PQgetisnull(m_result, row, column) == 1;
}

bool AResultPg::toBool(int row, int column) const
{
    Q_ASSERT_X(column < PQnfields(m_result), "toBool", "column out of range");
    const char *val = PQgetvalue(m_result, row, column);
    return val[0] == 't';
}

int AResultPg::toInt(int row, int column) const
{
    Q_ASSERT_X(column < PQnfields(m_result), "toInt", "column out of range");
    const char *val = PQgetvalue(m_result, row, column);
    return atoi(val);
}

qint64 AResultPg::toLongLong(int row, int column) const
{
    Q_ASSERT_X(column < PQnfields(m_result), "toLongLong", "column out of range");
    const char *val = PQgetvalue(m_result, row, column);
    return QString::fromLatin1(val).toLongLong();
}

quint64 AResultPg::toULongLong(int row, int column) const
{
    Q_ASSERT_X(column < PQnfields(m_result), "toULongLong", "column out of range");
    const char *val = PQgetvalue(m_result, row, column);
    return QString::fromLatin1(val).toULongLong();
}

double AResultPg::toDouble(int row, int column) const
{
    Q_ASSERT_X(column < PQnfields(m_result), "toDouble", "column out of range");
    const char *val = PQgetvalue(m_result, row, column);
    if (qstricmp(val, "Infinity") == 0)
        return qInf();
    if (qstricmp(val, "-Infinity") == 0)
        return -qInf();
    return QString::fromLatin1(val).toDouble();
}

QString AResultPg::toString(int row, int column) const
{
    Q_ASSERT_X(column < PQnfields(m_result), "toString", "column out of range");
    if (PQgetisnull(m_result, row, column) == 1) {
        return {};
    }

    const char *val = PQgetvalue(m_result, row, column);
    return QString::fromUtf8(val);
}

std::string AResultPg::toStdString(int row, int column) const
{
    Q_ASSERT_X(column < PQnfields(m_result), "toStdString", "column out of range");
    if (PQgetisnull(m_result, row, column) == 1) {
        return {};
    }

    const char *val = PQgetvalue(m_result, row, column);
    return std::string(val);
}

QDate AResultPg::toDate(int row, int column) const
{
    Q_ASSERT_X(column < PQnfields(m_result), "toDate", "column out of range");
    const char *val = PQgetvalue(m_result, row, column);
    if (val[0] == '\0') {
        return {};
    } else {
#ifndef QT_NO_DATESTRING
        return QDate::fromString(QString::fromLatin1(val), Qt::ISODate);
#else
        return {};
#endif
    }
}

QTime AResultPg::toTime(int row, int column) const
{
    Q_ASSERT_X(column < PQnfields(m_result), "toTime", "column out of range");
    const char *val = PQgetvalue(m_result, row, column);
    const QString str = QString::fromLatin1(val);
#ifndef QT_NO_DATESTRING
    if (str.isEmpty())
        return {};
    else
        return QTime::fromString(str, Qt::ISODate);
#else
    return {};
#endif
}

QDateTime AResultPg::toDateTime(int row, int column) const
{
    Q_ASSERT_X(column < PQnfields(m_result), "toDateTime", "column out of range");
    const char *val = PQgetvalue(m_result, row, column);
    QString dtval = QString::fromLatin1(val);
#ifndef QT_NO_DATESTRING
    if (dtval.length() < 10) {
        return {};
    } else {
        QChar sign = dtval[dtval.size() - 3];
        if (sign == QLatin1Char('-') || sign == QLatin1Char('+')) dtval += QLatin1String(":00");
        return QDateTime::fromString(dtval, Qt::ISODate);
    }
#else
    return {};
#endif
}

QJsonValue AResultPg::toJsonValue(int row, int column) const
{
    QJsonValue ret;
    Q_ASSERT_X(column < PQnfields(m_result), "toJsonValue", "column out of range");
    if (PQgetisnull(m_result, row, column) == 1) {
        return ret;
    }

    const char *val = PQgetvalue(m_result, row, column);
    auto doc = QJsonDocument::fromJson(val);
    if (doc.isObject()) {
        ret = doc.object();
    } else if (doc.isArray()) {
        ret = doc.array();
    }
    return ret;
}

QByteArray AResultPg::toByteArray(int row, int column) const
{
    Q_ASSERT_X(column < PQnfields(m_result), "toByteArray", "column out of range");
    const char *val = PQgetvalue(m_result, row, column);
    size_t len;
    unsigned char *data = PQunescapeBytea((const unsigned char*)val, &len);
    QByteArray ba(reinterpret_cast<const char *>(data), int(len));
    PQfreemem(data);
    return ba;
}

void AResultPg::processResult()
{
    ExecStatusType status = PQresultStatus(m_result);
    switch (status) {
    case PGRES_TUPLES_OK:
    case PGRES_SINGLE_TUPLE:
    case PGRES_COMMAND_OK:
        return;
    default:
        break;
    }

    m_error = true;
    m_errorString = QString::fromLocal8Bit(PQresultErrorMessage(m_result));
}

#include "moc_adriverpg.cpp"
