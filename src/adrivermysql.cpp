/*
 * SPDX-FileCopyrightText: (C) 2025 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "adrivermysql.h"

#include "acoroexpected.h"
#include "aresult.h"

#include <mysql/mysql.h>

#include <QCborValue>
#include <QDate>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QSocketNotifier>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>

Q_LOGGING_CATEGORY(ASQL_MYSQL, "asql.mysql", QtInfoMsg)

using namespace ASql;
using namespace Qt::StringLiterals;

// ---------------------------------------------------------------------------
// AResultMysql
// ---------------------------------------------------------------------------

AResultMysql::~AResultMysql() = default;

bool AResultMysql::lastResultSet() const
{
    return m_lastResultSet;
}

bool AResultMysql::hasError() const
{
    return m_error;
}

QString AResultMysql::errorString() const
{
    return m_errorString;
}

QByteArray AResultMysql::query() const
{
    return m_query;
}

QVariantList AResultMysql::queryArgs() const
{
    return m_queryArgs;
}

int AResultMysql::size() const
{
    return static_cast<int>(m_rows.size());
}

int AResultMysql::fields() const
{
    return m_fields.size();
}

qint64 AResultMysql::numRowsAffected() const
{
    return m_numRowsAffected;
}

int AResultMysql::indexOfField(QLatin1String name) const
{
    return m_fields.indexOf(name);
}

QString AResultMysql::fieldName(int column) const
{
    if (column >= 0 && column < m_fields.size()) {
        return m_fields.at(column);
    }
    return {};
}

QVariant AResultMysql::value(int row, int column) const
{
    if (row >= 0 && row < m_rows.size()) {
        const QVariantList &r = m_rows.at(row);
        if (column >= 0 && column < r.size()) {
            return r.at(column);
        }
    }
    return {};
}

bool AResultMysql::isNull(int row, int column) const
{
    return value(row, column).isNull();
}

bool AResultMysql::toBool(int row, int column) const
{
    return value(row, column).toBool();
}

int AResultMysql::toInt(int row, int column) const
{
    return value(row, column).toInt();
}

qint64 AResultMysql::toLongLong(int row, int column) const
{
    return value(row, column).toLongLong();
}

quint64 AResultMysql::toULongLong(int row, int column) const
{
    return value(row, column).toULongLong();
}

double AResultMysql::toDouble(int row, int column) const
{
    return value(row, column).toDouble();
}

QString AResultMysql::toString(int row, int column) const
{
    return value(row, column).toString();
}

std::string AResultMysql::toStdString(int row, int column) const
{
    return value(row, column).toString().toStdString();
}

QUuid AResultMysql::toUuid(int row, int column) const
{
    return QUuid::fromString(value(row, column).toString());
}

QDate AResultMysql::toDate(int row, int column) const
{
    const QVariant v = value(row, column);
    if (v.typeId() == QMetaType::QDate) {
        return v.toDate();
    }
    return QDate::fromString(v.toString(), Qt::ISODate);
}

QTime AResultMysql::toTime(int row, int column) const
{
    const QVariant v = value(row, column);
    if (v.typeId() == QMetaType::QTime) {
        return v.toTime();
    }
    return QTime::fromString(v.toString(), Qt::ISODate);
}

QDateTime AResultMysql::toDateTime(int row, int column) const
{
    const QVariant v = value(row, column);
    if (v.typeId() == QMetaType::QDateTime) {
        return v.toDateTime();
    }
    return QDateTime::fromString(v.toString(), Qt::ISODate);
}

QJsonValue AResultMysql::toJsonValue(int row, int column) const
{
    const QVariant v = value(row, column);
    if (v.isNull()) {
        return QJsonValue::Null;
    }
    const QByteArray data = v.toByteArray();
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error == QJsonParseError::NoError) {
        if (doc.isObject()) {
            return doc.object();
        } else if (doc.isArray()) {
            return doc.array();
        }
    }
    return QJsonValue::fromVariant(v);
}

QCborValue AResultMysql::toCborValue(int row, int column) const
{
    return QCborValue::fromVariant(value(row, column));
}

QByteArray AResultMysql::toByteArray(int row, int column) const
{
    return value(row, column).toByteArray();
}

// ---------------------------------------------------------------------------
// ADriverMysql
// ---------------------------------------------------------------------------

ADriverMysql::ADriverMysql(const QString &connInfo)
    : ADriver(connInfo)
{
}

ADriverMysql::~ADriverMysql()
{
    if (m_mysql) {
        mysql_close(m_mysql);
        m_mysql = nullptr;
    }
}

QString ADriverMysql::driverName() const
{
    return u"mysql"_s;
}

bool ADriverMysql::isValid() const
{
    return true;
}

void ADriverMysql::open(const std::shared_ptr<ADriver> &driver, QObject *receiver, AOpenFn cb)
{
    if (m_state == ADatabase::State::Connected) {
        if (cb) {
            cb(true, {});
        }
        return;
    }

    if (m_state == ADatabase::State::Connecting) {
        qWarning(ASQL_MYSQL) << "Already connecting";
        return;
    }

    qDebug(ASQL_MYSQL) << "Open" << connectionInfo();

    m_mysql = mysql_init(nullptr);
    if (!m_mysql) {
        const QString error = u"mysql_init() failed: out of memory"_s;
        qWarning(ASQL_MYSQL) << error;
        if (cb) {
            cb(false, error);
        }
        return;
    }

    // Parse the connection URL: mysql://user:password@host:port/database
    QUrl url(connectionInfo());
    m_host     = url.host().toUtf8();
    m_user     = url.userName().toUtf8();
    m_password = url.password().toUtf8();
    m_database = url.path().mid(1).toUtf8(); // strip leading '/'
    m_port     = (url.port() > 0) ? static_cast<unsigned int>(url.port()) : 3306u;

    m_openCaller         = std::make_unique<OpenCaller>();
    m_openCaller->driver = driver;
    m_openCaller->cb     = cb;
    if (receiver) {
        m_openCaller->receiverPtr = receiver;
    }

    setState(ADatabase::State::Connecting, {});

    // Start non-blocking connection
    enum net_async_status status = mysql_real_connect_nonblocking(
        m_mysql,
        m_host.isEmpty() ? nullptr : m_host.constData(),
        m_user.isEmpty() ? nullptr : m_user.constData(),
        m_password.isEmpty() ? nullptr : m_password.constData(),
        m_database.isEmpty() ? nullptr : m_database.constData(),
        m_port,
        nullptr, // unix socket
        0        // client flags
    );

    if (status == NET_ASYNC_COMPLETE) {
        qDebug(ASQL_MYSQL) << "Connected immediately";
        setState(ADatabase::State::Connected, {});
        if (m_openCaller) {
            m_openCaller->emit(true, {});
            m_openCaller.reset();
        }
        nextQuery();
        return;
    }

    if (status == NET_ASYNC_ERROR) {
        const QString error = QString::fromUtf8(mysql_error(m_mysql));
        qWarning(ASQL_MYSQL) << "Connection error:" << error;
        mysql_close(m_mysql);
        m_mysql = nullptr;
        setState(ADatabase::State::Disconnected, error);
        if (m_openCaller) {
            m_openCaller->emit(false, error);
            m_openCaller.reset();
        }
        return;
    }

    // NET_ASYNC_NOT_READY - set up socket notifiers to poll the connection
    const int fd = static_cast<int>(m_mysql->net.fd);
    if (fd < 0) {
        const QString error = u"Failed to obtain socket descriptor during async connect"_s;
        qWarning(ASQL_MYSQL) << error;
        mysql_close(m_mysql);
        m_mysql = nullptr;
        setState(ADatabase::State::Disconnected, error);
        if (m_openCaller) {
            m_openCaller->emit(false, error);
            m_openCaller.reset();
        }
        return;
    }

    m_writeNotify = std::make_unique<QSocketNotifier>(fd, QSocketNotifier::Write);
    m_readNotify  = std::make_unique<QSocketNotifier>(fd, QSocketNotifier::Read);

    auto connFn = [this] {
        enum net_async_status s = mysql_real_connect_nonblocking(
            m_mysql,
            m_host.isEmpty() ? nullptr : m_host.constData(),
            m_user.isEmpty() ? nullptr : m_user.constData(),
            m_password.isEmpty() ? nullptr : m_password.constData(),
            m_database.isEmpty() ? nullptr : m_database.constData(),
            m_port,
            nullptr,
            0);

        if (s == NET_ASYNC_NOT_READY) {
            return; // keep waiting
        }

        // Disable notifiers - connection is done (success or failure)
        m_writeNotify->setEnabled(false);
        m_readNotify->setEnabled(false);

        if (s == NET_ASYNC_COMPLETE) {
            qDebug(ASQL_MYSQL) << "Connected";
            setState(ADatabase::State::Connected, {});
            if (m_openCaller) {
                m_openCaller->emit(true, {});
                m_openCaller.reset();
            }
            nextQuery();
        } else {
            const QString error = QString::fromUtf8(mysql_error(m_mysql));
            qWarning(ASQL_MYSQL) << "Connection failed:" << error;
            mysql_close(m_mysql);
            m_mysql = nullptr;
            setState(ADatabase::State::Disconnected, error);
            if (m_openCaller) {
                m_openCaller->emit(false, error);
                m_openCaller.reset();
            }
        }
    };

    connect(m_writeNotify.get(), &QSocketNotifier::activated, this, connFn);
    connect(m_readNotify.get(), &QSocketNotifier::activated, this, connFn);
}

bool ADriverMysql::isOpen() const
{
    return isConnected();
}

void ADriverMysql::setState(ADatabase::State state, const QString &status)
{
    m_state = state;
    if (m_stateChangedCb &&
        (!m_stateChangedReceiver.has_value() || !m_stateChangedReceiver->isNull())) {
        m_stateChangedCb(state, status);
    }
}

ADatabase::State ADriverMysql::state() const
{
    return m_state;
}

void ADriverMysql::onStateChanged(QObject *receiver,
                                  std::function<void(ADatabase::State, const QString &)> cb)
{
    m_stateChangedCb = cb;
    if (receiver) {
        m_stateChangedReceiver = receiver;
    }
}

void ADriverMysql::begin(const std::shared_ptr<ADriver> &db, QObject *receiver, ACoroDataRef cb)
{
    exec(db, u8"START TRANSACTION", receiver, std::move(cb));
}

void ADriverMysql::commit(const std::shared_ptr<ADriver> &db, QObject *receiver, ACoroDataRef cb)
{
    exec(db, u8"COMMIT", receiver, std::move(cb));
}

void ADriverMysql::rollback(const std::shared_ptr<ADriver> &db, QObject *receiver, ACoroDataRef cb)
{
    exec(db, u8"ROLLBACK", receiver, std::move(cb));
}

void ADriverMysql::setupCheckReceiver(AMysqlQuery &mysqlQuery, QObject *receiver)
{
    if (receiver) {
        mysqlQuery.receiver      = receiver;
        mysqlQuery.checkReceiver = receiver;
    }
}

void ADriverMysql::exec(const std::shared_ptr<ADriver> &db,
                        QUtf8StringView query,
                        QObject *receiver,
                        ACoroDataRef cb)
{
    AMysqlQuery mysqlQuery;
    mysqlQuery.query.setRawData(query.data(), query.size());
    mysqlQuery.cb     = std::move(cb);
    mysqlQuery.result = std::make_shared<AResultMysql>();

    setupCheckReceiver(mysqlQuery, receiver);

    selfDriver = db;
    m_queuedQueries.emplace(std::move(mysqlQuery));
    nextQuery();
}

void ADriverMysql::exec(const std::shared_ptr<ADriver> &db,
                        QStringView query,
                        QObject *receiver,
                        ACoroDataRef cb)
{
    AMysqlQuery mysqlQuery;
    mysqlQuery.query  = query.toUtf8();
    mysqlQuery.cb     = std::move(cb);
    mysqlQuery.result = std::make_shared<AResultMysql>();

    setupCheckReceiver(mysqlQuery, receiver);

    selfDriver = db;
    m_queuedQueries.emplace(std::move(mysqlQuery));
    nextQuery();
}

void ADriverMysql::exec(const std::shared_ptr<ADriver> &db,
                        QUtf8StringView query,
                        const QVariantList &params,
                        QObject *receiver,
                        ACoroDataRef cb)
{
    AMysqlQuery mysqlQuery;
    mysqlQuery.query.setRawData(query.data(), query.size());
    mysqlQuery.params = params;
    mysqlQuery.cb     = std::move(cb);
    mysqlQuery.result = std::make_shared<AResultMysql>();

    setupCheckReceiver(mysqlQuery, receiver);

    selfDriver = db;
    m_queuedQueries.emplace(std::move(mysqlQuery));
    nextQuery();
}

void ADriverMysql::exec(const std::shared_ptr<ADriver> &db,
                        QStringView query,
                        const QVariantList &params,
                        QObject *receiver,
                        ACoroDataRef cb)
{
    AMysqlQuery mysqlQuery;
    mysqlQuery.query  = query.toUtf8();
    mysqlQuery.params = params;
    mysqlQuery.cb     = std::move(cb);
    mysqlQuery.result = std::make_shared<AResultMysql>();

    setupCheckReceiver(mysqlQuery, receiver);

    selfDriver = db;
    m_queuedQueries.emplace(std::move(mysqlQuery));
    nextQuery();
}

void ADriverMysql::exec(const std::shared_ptr<ADriver> &db,
                        const APreparedQuery &query,
                        const QVariantList &params,
                        QObject *receiver,
                        ACoroDataRef cb)
{
    AMysqlQuery mysqlQuery;
    mysqlQuery.preparedQuery = query;
    mysqlQuery.query         = query.query();
    mysqlQuery.params        = params;
    mysqlQuery.cb            = std::move(cb);
    mysqlQuery.result        = std::make_shared<AResultMysql>();

    setupCheckReceiver(mysqlQuery, receiver);

    selfDriver = db;
    m_queuedQueries.emplace(std::move(mysqlQuery));
    nextQuery();
}

void ADriverMysql::setLastQuerySingleRowMode()
{
    // Not supported for MySQL driver yet
}

bool ADriverMysql::enterPipelineMode(std::chrono::milliseconds timeout)
{
    Q_UNUSED(timeout)
    return false;
}

bool ADriverMysql::exitPipelineMode()
{
    return false;
}

ADatabase::PipelineStatus ADriverMysql::pipelineStatus() const
{
    return ADatabase::PipelineStatus::Off;
}

bool ADriverMysql::pipelineSync()
{
    return false;
}

int ADriverMysql::queueSize() const
{
    return static_cast<int>(m_queuedQueries.size());
}

void ADriverMysql::subscribeToNotification(const std::shared_ptr<ADriver> &db,
                                           const QString &name,
                                           QObject *receiver,
                                           ANotificationFn cb)
{
    Q_UNUSED(db)
    Q_UNUSED(name)
    Q_UNUSED(receiver)
    Q_UNUSED(cb)
    // MySQL does not support server-side notifications
}

QStringList ADriverMysql::subscribedToNotifications() const
{
    return {};
}

void ADriverMysql::unsubscribeFromNotification(const std::shared_ptr<ADriver> &db,
                                               const QString &name)
{
    Q_UNUSED(db)
    Q_UNUSED(name)
    // MySQL does not support server-side notifications
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

bool ADriverMysql::isConnected() const
{
    return m_state == ADatabase::State::Connected && m_mysql != nullptr;
}

void ADriverMysql::nextQuery()
{
    if (!isConnected() || m_queryRunning || m_queuedQueries.empty()) {
        return;
    }

    AMysqlQuery &mysqlQuery = m_queuedQueries.front();

    // Skip cancelled queries
    if (mysqlQuery.checkReceiver && mysqlQuery.receiver.isNull()) {
        m_queuedQueries.pop();
        nextQuery();
        return;
    }

    if (!mysqlQuery.cb) {
        m_queuedQueries.pop();
        nextQuery();
        return;
    }

    m_queryRunning = true;

    // Execute the query (blocking - async query execution is a future enhancement)
    const QByteArray &sql = mysqlQuery.query;
    if (mysql_real_query(m_mysql, sql.constData(), static_cast<unsigned long>(sql.size())) != 0) {
        const QString error = QString::fromUtf8(mysql_error(m_mysql));
        qWarning(ASQL_MYSQL) << "Query error:" << error;
        m_queryRunning = false;

        AMysqlQuery q = std::move(m_queuedQueries.front());
        m_queuedQueries.pop();
        q.doneError(error);

        if (m_queuedQueries.empty()) {
            selfDriver.reset();
        } else {
            nextQuery();
        }
        return;
    }

    MYSQL_RES *res = mysql_store_result(m_mysql);
    AMysqlQuery query = std::move(m_queuedQueries.front());
    m_queuedQueries.pop();

    if (res) {
        const unsigned int numFields = mysql_num_fields(res);
        MYSQL_FIELD *fields = mysql_fetch_fields(res);

        query.result->m_fields.reserve(static_cast<int>(numFields));
        for (unsigned int i = 0; i < numFields; ++i) {
            query.result->m_fields.append(QString::fromUtf8(fields[i].name));
        }

        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res)) != nullptr) {
            unsigned long *lengths = mysql_fetch_lengths(res);
            QVariantList rowData;
            rowData.reserve(static_cast<int>(numFields));
            for (unsigned int i = 0; i < numFields; ++i) {
                if (row[i] == nullptr) {
                    rowData.append(QVariant{});
                } else {
                    rowData.append(QString::fromUtf8(row[i], static_cast<int>(lengths[i])));
                }
            }
            query.result->m_rows.append(std::move(rowData));
        }
        mysql_free_result(res);
        query.result->m_numRowsAffected = static_cast<qint64>(mysql_affected_rows(m_mysql));
    } else {
        // No result set - could be INSERT/UPDATE/DELETE
        if (mysql_field_count(m_mysql) == 0) {
            query.result->m_numRowsAffected = static_cast<qint64>(mysql_affected_rows(m_mysql));
        } else {
            // mysql_store_result() returned NULL even though there were fields
            const QString error = QString::fromUtf8(mysql_error(m_mysql));
            query.result->m_errorString = error;
            query.result->m_error       = true;
        }
    }

    m_queryRunning = false;
    query.done();

    if (m_queuedQueries.empty()) {
        selfDriver.reset();
    } else {
        nextQuery();
    }
}

void ADriverMysql::finishConnection(const QString &error)
{
    m_readNotify.reset();
    m_writeNotify.reset();

    if (m_mysql) {
        mysql_close(m_mysql);
        m_mysql = nullptr;
    }

    setState(ADatabase::State::Disconnected, error);

    while (!m_queuedQueries.empty()) {
        AMysqlQuery mysqlQuery = std::move(m_queuedQueries.front());
        m_queuedQueries.pop();
        mysqlQuery.doneError(error);
    }

    selfDriver.reset();
}
