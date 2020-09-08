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

#include <arpa/inet.h>
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

ADriverPg::ADriverPg()
{

}

ADriverPg::~ADriverPg()
{
    if (m_conn) {
        PQfinish(m_conn);
    }
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

//static QString qQuote(QString s)
//{
//    s.replace(QLatin1Char('\\'), QLatin1String("\\\\"));
//    s.replace(QLatin1Char('\''), QLatin1String("\\'"));
//    s.append(QLatin1Char('\'')).prepend(QLatin1Char('\''));
//    return s;
//}

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
            setState(ADatabase::Connecting, error);

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
                        cb(false, QString());
                    }

                    setState(ADatabase::Connected, QString());

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
                    setState(ADatabase::Disconnected, error);
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
                }
            });

            connect(m_readNotify, &QSocketNotifier::activated, this, [=] {
                qDebug(ASQL_PG) << "PG read" << this;
                if (!m_connected) {
                    connFn();
                } else {
                    if (PQconsumeInput(m_conn) == 1) {
                        while (PQisBusy(m_conn) == 0) {
                            qDebug(ASQL_PG) << "Not busy";
                            PGresult *result = PQgetResult(m_conn);
                            qDebug(ASQL_PG) << "RESULT" << result << "busy" << PQisBusy(m_conn);
                            if (result != nullptr) {
                                int status = PQresultStatus(result);
                                APGQuery &pgQuery = m_queuedQueries.head();
                                qDebug(ASQL_PG) << "RESULT" << result << "status" << status << PGRES_TUPLES_OK;
                                if (pgQuery.result->m_result) {
                                    // when we had already had a result it means we should emit the
                                    // first one and keep waiting till a null result is returned
                                    pgQuery.result->m_lastResultSet = false;
                                    pgQuery.done();

                                    // allocate a new result
                                    pgQuery.result = QSharedPointer<AResultPg>(new AResultPg());
                                }
                                pgQuery.result->m_result = result;
                                pgQuery.result->processResult();
                            } else {
                                APGQuery &pgQuery = m_queuedQueries.head();
                                if (pgQuery.query.isEmpty() && pgQuery.preparing) {
                                    if (pgQuery.result->error()) {
                                        // PREPARE OR PREPARED QUERY ERROR
                                        m_queuedQueries.dequeue();
                                        pgQuery.done();
                                    } else {
                                        // Query prepared
                                        m_preparedQueries.append(pgQuery.preparedQuery.identification());
                                        pgQuery.result = QSharedPointer<AResultPg>(new AResultPg());
                                        pgQuery.preparing = false;
                                    }
                                    m_queryRunning = false;
                                } else {
                                    APGQuery pgQuery = m_queuedQueries.dequeue();
                                    pgQuery.done();
                                    m_queryRunning = false;
                                }
                                nextQuery();
                                break;
                            }
                        }
                        qDebug(ASQL_PG) << "Not busy OUT" << this;

                        PGnotify *notify = nullptr;
                        while ((notify = PQnotifies(m_conn)) != nullptr) {
                            const QString name = QString::fromUtf8(notify->relname);
                            qDebug(ASQL_PG) << "NOTIFICATION" << name << notify;
                            auto it = m_subscribedNotifications.constFind(name);
                            if (it != m_subscribedNotifications.constEnd()) {
                                QString payload;
                                if (notify->extra) {
                                    payload = QString::fromUtf8(notify->extra);
                                }
                                bool self = (notify->be_pid == PQbackendPID(m_conn)) ? true : false;
                                qDebug(ASQL_PG) << "NOTIFICATION" << self << name << payload;
                                it.value()(payload, self);
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

                            setState(ADatabase::Disconnected, error);
                        }
                    }
                }
            });
        }
        qDebug(ASQL_PG) << "PG Socket" << m_conn << socket;
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

void ADriverPg::begin(QSharedPointer<ADatabasePrivate> db, AResultFn cb, QObject *receiver)
{
    exec(db, QStringLiteral("BEGIN"), QVariantList(), cb, receiver);
}

void ADriverPg::commit(QSharedPointer<ADatabasePrivate> db, AResultFn cb, QObject *receiver)
{
    exec(db, QStringLiteral("COMMIT"), QVariantList(), cb, receiver);
}

void ADriverPg::rollback(QSharedPointer<ADatabasePrivate> db, AResultFn cb, QObject *receiver)
{
    exec(db, QStringLiteral("ROLLBACK"), QVariantList(), cb, receiver);
}

void ADriverPg::exec(QSharedPointer<ADatabasePrivate> db, const QString &query, const QVariantList &params, AResultFn cb, QObject *receiver)
{
    APGQuery pgQuery;
    pgQuery.query = query;
    pgQuery.params = params;
    pgQuery.cb = cb;
    pgQuery.db = db;
    pgQuery.receiver = receiver;
    pgQuery.checkReceiver = receiver;

    if (receiver) {
        connect(receiver, &QObject::destroyed, this, [=] (QObject *obj) {
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
            qDebug(ASQL_PG) << "destroyed" << m_queryRunning << m_queuedQueries.empty() ;
        });
    }

    m_queuedQueries.append(pgQuery);

    if (m_queryRunning || !m_conn || !m_connected) {
        return;
    }

    if (params.isEmpty()) {
        doExec(pgQuery);
    } else {
        doExecParams(pgQuery);
    }
}

void ADriverPg::exec(QSharedPointer<ADatabasePrivate> db, const APreparedQuery &query, const QVariantList &params, AResultFn cb, QObject *receiver)
{
    APGQuery pgQuery;
    pgQuery.preparedQuery = query;
    pgQuery.params = params;
    pgQuery.cb = cb;
    pgQuery.db = db;
    pgQuery.receiver = receiver;
    pgQuery.checkReceiver = receiver;

    if (receiver) {
        connect(receiver, &QObject::destroyed, this, [=] (QObject *obj) {
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
            qDebug(ASQL_PG) << "destroyed" << m_queryRunning << m_queuedQueries.empty() ;
        });
    }

    m_queuedQueries.append(pgQuery);

    if (m_queryRunning || !m_conn || !m_connected) {
        return;
    }

    doExecParams(pgQuery);
}

void ADriverPg::subscribeToNotification(QSharedPointer<ADatabasePrivate> db, const QString &name, ANotificationFn cb, QObject *receiver)
{
    if (m_subscribedNotifications.contains(name)) {
        qWarning(ASQL_PG) << "Already subscribed to notification" << name;
        return;
    }

    exec(db, QStringLiteral("LISTEN %1").arg(name), {}, [=] (AResult &result) {
        qDebug(ASQL_PG) << "subscribed" << result.error() << result.errorString();
        m_subscribedNotifications.insert(name, cb);
    }, receiver);
}

void ADriverPg::unsubscribeFromNotification(QSharedPointer<ADatabasePrivate> db, const QString &name, QObject *receiver)
{
    if (m_subscribedNotifications.remove(name)) {
        exec(db, QStringLiteral("UNLISTEN %1").arg(name), {}, [=] (AResult &result) {
            qDebug(ASQL_PG) << "unsubscribed" << result.error() << result.errorString();
        }, receiver);
    }
}

void ADriverPg::nextQuery()
{
    while (!m_queuedQueries.isEmpty() && !m_queryRunning) {
        APGQuery &pgQuery = m_queuedQueries.head();
        if (pgQuery.checkReceiver && pgQuery.receiver.isNull()) {
            m_queuedQueries.dequeue();
        } else {
            if (pgQuery.params.isEmpty() && !pgQuery.query.isEmpty()) {
                doExec(pgQuery);
            } else {
                doExecParams(pgQuery);
            }
        }
    }
}

void ADriverPg::finishConnection()
{
    if (m_conn) {
        PQfinish(m_conn);
        m_conn = nullptr;
    }

    m_preparedQueries.clear();
    m_connected = false;
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
}

void ADriverPg::doExec(APGQuery &pgQuery)
{
    int ret = PQsendQuery(m_conn, pgQuery.query.toUtf8().constData());
    if (ret == 1) {
        m_queryRunning = true;
    } else {
        m_queuedQueries.dequeue();
        pgQuery.result->m_error = true;
        pgQuery.result->m_errorString = QString::fromLocal8Bit(PQerrorMessage(m_conn));
        pgQuery.done();
    }
}

void ADriverPg::doExecParams(APGQuery &pgQuery)
{
    const QVariantList params = pgQuery.params;
    Oid paramTypes[params.size()];
    const char *paramValues[params.size()];
    int paramLengths[params.size()];
    int paramFormats[params.size()];

    QByteArrayList deleteLater;
    for (int i = 0; i < params.size(); ++i) {
        QVariant v = params[i];
        QByteArray data;
        qDebug(ASQL_PG) << v << v.type() << v.isNull() << "---" << QString::number(v.toInt()).toLatin1().constData() << v.toString().isNull();
        if (!v.isNull()) {
            switch (v.userType()) {
            case QVariant::String:
            {
                const QString text = v.toString();
                paramTypes[i] = !text.isNull() ? QTEXTOID : QUNKNOWNOID;
                paramFormats[i] = 0;
                data = text.toUtf8();
            }
                break;
            case QVariant::ByteArray:
                paramTypes[i] = QBYTEAOID;
                paramFormats[i] = 1;
                data = v.toByteArray();
                break;
            case QVariant::Int:
                paramTypes[i] = QINT4OID;
                paramFormats[i] = 1;
            {
                const qint32 number = v.toInt();
                data.resize(4);
                qToBigEndian<qint32>(number, data.data());
            }
                break;
            case QVariant::LongLong:
                paramTypes[i] = QINT8OID;
                paramFormats[i] = 1;
            {
                const qint64 number = v.toLongLong();
                data.resize(8);
                qToBigEndian<qint64>(number, data.data());
            }
                break;
            case QVariant::Uuid:
                paramTypes[i] = QUUIDOID;
                paramFormats[i] = 1;
                data = v.toUuid().toRfc4122();
                break;
            case QVariant::Bool:
                paramTypes[i] = QBOOLOID;
                paramFormats[i] = 1;
                data.append(v.toBool() ? 0x01 : 0x00);
                break;
            case QVariant::Invalid:
                paramTypes[i] = QUNKNOWNOID;
                paramFormats[i] = 0;
                break;
            case QMetaType::QJsonArray:
                paramTypes[i] = QJSONBOID;
                paramFormats[i] = 0;
                data = QJsonDocument(v.toJsonObject()).toJson();
                break;
            case QMetaType::QJsonObject:
                paramTypes[i] = QJSONBOID;
                paramFormats[i] = 0;
                data = QJsonDocument(v.toJsonArray()).toJson();
                break;
            case QMetaType::QJsonDocument:
                paramTypes[i] = QJSONBOID;
                paramFormats[i] = 0;
                data = v.toJsonDocument().toJson();
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
    if (pgQuery.query.isEmpty()) {
        if (m_preparedQueries.contains(pgQuery.preparedQuery.identification())) {
            ret = PQsendQueryPrepared(m_conn,
                                      pgQuery.preparedQuery.identification().toUtf8().constData(),
                                      params.size(),
                                      paramValues,
                                      paramLengths,
                                      paramFormats,
                                      0); // perhaps later use binary results
        } else {
            pgQuery.preparing = true;
            ret = PQsendPrepare(m_conn,
                                pgQuery.preparedQuery.identification().toUtf8().constData(),
                                pgQuery.preparedQuery.query().toUtf8().constData(),
                                params.size(),
                                paramTypes); // perhaps later use binary results
        }
    } else {
        ret = PQsendQueryParams(m_conn,
                                pgQuery.query.toUtf8().constData(),
                                params.size(),
                                paramTypes,
                                paramValues,
                                paramLengths,
                                paramFormats,
                                0); // perhaps later use binary results
    }

    if (ret == 1) {
        m_queryRunning = true;
    } else {
        pgQuery.result->m_error = true;
        pgQuery.result->m_errorString = QString::fromLocal8Bit(PQerrorMessage(m_conn));
        pgQuery.done();
        m_queuedQueries.dequeue();
    }
}

AResultPg::AResultPg()
{

}

AResultPg::~AResultPg()
{
    PQclear(m_result);
}

bool AResultPg::next()
{
    return ++m_pos < size();
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

void AResultPg::setAt(int row)
{
    m_pos = row;
}

int AResultPg::at() const
{
    return m_pos;
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

QString AResultPg::fieldName(int column) const
{
    return QString::fromUtf8(PQfname(m_result, column));
}

static QVariant::Type qDecodePSQLType(int t)
{
    QVariant::Type type = QVariant::Invalid;
    switch (t) {
    case QBOOLOID:
        type = QVariant::Bool;
        break;
    case QINT8OID:
        type = QVariant::LongLong;
        break;
    case QINT2OID:
    case QINT4OID:
    case QOIDOID:
    case QREGPROCOID:
    case QXIDOID:
    case QCIDOID:
        type = QVariant::Int;
        break;
    case QNUMERICOID:
    case QFLOAT4OID:
    case QFLOAT8OID:
        type = QVariant::Double;
        break;
    case QABSTIMEOID:
    case QRELTIMEOID:
    case QDATEOID:
        type = QVariant::Date;
        break;
    case QTIMEOID:
    case QTIMETZOID:
        type = QVariant::Time;
        break;
    case QTIMESTAMPOID:
    case QTIMESTAMPTZOID:
        type = QVariant::DateTime;
        break;
    case QBYTEAOID:
        type = QVariant::ByteArray;
        break;
    default:
        type = QVariant::String;
        break;
    }
    qDebug(ASQL_PG) << "decode pg type" << t << type;
    return type;
}

QVariant AResultPg::value(int i) const
{
    if (i >= PQnfields(m_result)) {
        qWarning(ASQL_PG, "column %d out of range", i);
        return QVariant();
    }
    const int currentRow = /*isForwardOnly() ? 0 : */at();
    int ptype = PQftype(m_result, i);
    QVariant::Type type = qDecodePSQLType(ptype);
    if (PQgetisnull(m_result, currentRow, i))
        return QVariant(type);
    const char *val = PQgetvalue(m_result, currentRow, i);
    switch (type) {
    case QVariant::Bool:
        return QVariant((bool)(val[0] == 't'));
    case QVariant::String:
        return QString::fromUtf8(val);
    case QVariant::LongLong:
        if (val[0] == '-')
            return QString::fromLatin1(val).toLongLong();
        else
            return QString::fromLatin1(val).toULongLong();
    case QVariant::Int:
        return atoi(val);
    case QVariant::Double: {
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
    case QVariant::Date:
        if (val[0] == '\0') {
            return QVariant(QDate());
        } else {
#ifndef QT_NO_DATESTRING
            return QVariant(QDate::fromString(QString::fromLatin1(val), Qt::ISODate));
#else
            return QVariant(QString::fromLatin1(val));
#endif
        }
    case QVariant::Time: {
        const QString str = QString::fromLatin1(val);
#ifndef QT_NO_DATESTRING
        if (str.isEmpty())
            return QVariant(QTime());
        else
            return QVariant(QTime::fromString(str, Qt::ISODate));
#else
        return QVariant(str);
#endif
    }
    case QVariant::DateTime: {
        QString dtval = QString::fromLatin1(val);
#ifndef QT_NO_DATESTRING
        if (dtval.length() < 10) {
            return QVariant(QDateTime());
        } else {
            QChar sign = dtval[dtval.size() - 3];
            if (sign == QLatin1Char('-') || sign == QLatin1Char('+')) dtval += QLatin1String(":00");
            return QVariant(QDateTime::fromString(dtval, Qt::ISODate).toLocalTime());
        }
#else
        return QVariant(dtval);
#endif
    }
    case QVariant::ByteArray: {
        size_t len;
        unsigned char *data = PQunescapeBytea((const unsigned char*)val, &len);
        QByteArray ba(reinterpret_cast<const char *>(data), int(len));
        PQfreemem(data);
        return QVariant(ba);
    }
    default:
    case QVariant::Invalid:
        qWarning(ASQL_PG, "unknown data type");
    }
    return QVariant();
}

void AResultPg::processResult()
{
    if (!m_result) {
//        q->setSelect(false);
//        q->setActive(false);
//        currentSize = -1;
//        canFetchMoreRows = false;
//        return false;
        return;
    }
    ExecStatusType status = PQresultStatus(m_result);
    switch (status) {
    case PGRES_TUPLES_OK:
//        q->setSelect(true);
//        q->setActive(true);
//        currentSize = q->isForwardOnly() ? -1 : PQntuples(result);
//        canFetchMoreRows = false;
//        return true;
    case PGRES_SINGLE_TUPLE:
//        q->setSelect(true);
//        q->setActive(true);
//        currentSize = -1;
//        canFetchMoreRows = true;
//        return true;
    case PGRES_COMMAND_OK:
//        q->setSelect(false);
//        q->setActive(true);
//        currentSize = -1;
//        canFetchMoreRows = false;
//        return true;
        return;
    default:
        break;
    }
//    q->setSelect(false);
//    q->setActive(false);
//    currentSize = -1;
//    canFetchMoreRows = false;

    m_error = true;
    m_errorString = QString::fromLocal8Bit(PQresultErrorMessage(m_result));
}
