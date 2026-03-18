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
    if (m_currentResult) {
        mysql_free_result(m_currentResult);
        m_currentResult = nullptr;
    }
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
    enum net_async_status status =
        mysql_real_connect_nonblocking(m_mysql,
                                       m_host.isEmpty() ? nullptr : m_host.constData(),
                                       m_user.isEmpty() ? nullptr : m_user.constData(),
                                       m_password.isEmpty() ? nullptr : m_password.constData(),
                                       m_database.isEmpty() ? nullptr : m_database.constData(),
                                       m_port,
                                       nullptr, // unix socket
                                       0        // client flags
        );

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

    // Get the socket fd - valid even for immediately-complete connections
    const int fd = static_cast<int>(m_mysql->net.fd);
    if (fd < 0) {
        const QString error = u"Failed to obtain socket descriptor"_s;
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

    if (status == NET_ASYNC_COMPLETE) {
        // Connected immediately: set up notifiers for query execution
        qDebug(ASQL_MYSQL) << "Connected immediately";
        connect(m_writeNotify.get(), &QSocketNotifier::activated, this, [this] {
            handleQueryProgress();
        });
        connect(m_readNotify.get(), &QSocketNotifier::activated, this, [this] {
            handleQueryProgress();
        });
        m_writeNotify->setEnabled(false);
        m_readNotify->setEnabled(false);

        setState(ADatabase::State::Connected, {});
        if (m_openCaller) {
            m_openCaller->emit(true, {});
            m_openCaller.reset();
        }
        nextQuery();
        return;
    }

    // NET_ASYNC_NOT_READY - poll the connection via socket notifiers
    auto connFn = [this] {
        enum net_async_status s =
            mysql_real_connect_nonblocking(m_mysql,
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

        // Connection finished - switch notifiers from connection polling to query handling
        disconnect(m_writeNotify.get(), nullptr, this, nullptr);
        disconnect(m_readNotify.get(), nullptr, this, nullptr);
        connect(m_writeNotify.get(), &QSocketNotifier::activated, this, [this] {
            handleQueryProgress();
        });
        connect(m_readNotify.get(), &QSocketNotifier::activated, this, [this] {
            handleQueryProgress();
        });
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

/*!
 * \brief Interpolates \a params into a query template that uses \c ? placeholders.
 *
 * Each \c ? in \a query (outside quoted strings) is replaced by the properly
 * escaped and quoted representation of the corresponding element of \a params.
 * The result is a self-contained SQL string that can be sent via
 * mysql_real_query_nonblocking() without a separate bind step.
 *
 * Type mapping:
 *  - QMetaType::Nullptr / isNull()  → NULL
 *  - Bool                           → 1 / 0
 *  - Int / UInt                     → decimal integer literal
 *  - LongLong / ULongLong           → decimal integer literal
 *  - Double / Float                 → decimal floating-point literal
 *  - QByteArray                     → X'<hex>' hex literal
 *  - everything else                → 'escaped UTF-8 string'
 */

// Significant digits retained when converting a floating-point param to text.
// 15 is the number of significant decimal digits that round-trips a double (DBL_DIG).
static constexpr int kFloatPrecision = 15;

static QByteArray buildQueryWithParams(MYSQL *mysql,
                                       const QByteArray &query,
                                       const QVariantList &params)
{
    if (params.isEmpty()) {
        return query;
    }

    QByteArray result;
    result.reserve(query.size() + params.size() * 16);

    int paramIdx = 0;
    const int len = query.size();
    for (int i = 0; i < len; ++i) {
        const char c = query[i];

        // Skip over single-quoted strings so we don't mistake a literal '?' for a placeholder
        if (c == '\'') {
            result.append(c);
            ++i;
            while (i < len) {
                const char sc = query[i];
                result.append(sc);
                if (sc == '\\') {
                    // escaped character - consume the next char raw
                    ++i;
                    if (i < len) {
                        result.append(query[i]);
                    }
                } else if (sc == '\'') {
                    // check for doubled-quote escape: ''
                    if (i + 1 < len && query[i + 1] == '\'') {
                        ++i;
                        result.append(query[i]);
                    } else {
                        break; // end of quoted string
                    }
                }
                ++i;
            }
            continue;
        }

        if (c != '?') {
            result.append(c);
            continue;
        }

        // Placeholder found — check that a param is available
        if (paramIdx >= params.size()) {
            qWarning(ASQL_MYSQL) << "More ? placeholders than params supplied; "
                                    "leaving remaining ? unsubstituted";
            result.append(c);
            continue;
        }

        const QVariant &v = params.at(paramIdx++);
        if (v.isNull()) {
            result.append("NULL");
            continue;
        }

        switch (v.userType()) {
        case QMetaType::Bool:
            result.append(v.toBool() ? "1" : "0");
            break;
        case QMetaType::Int:
            result.append(QByteArray::number(v.toInt()));
            break;
        case QMetaType::UInt:
            result.append(QByteArray::number(v.toUInt()));
            break;
        case QMetaType::LongLong:
            result.append(QByteArray::number(v.toLongLong()));
            break;
        case QMetaType::ULongLong:
            result.append(QByteArray::number(v.toULongLong()));
            break;
        case QMetaType::Double:
        case QMetaType::Float:
            // kFloatPrecision matches DBL_DIG — enough digits for a lossless round-trip
            result.append(QByteArray::number(v.toDouble(), 'g', kFloatPrecision));
            break;
        case QMetaType::QByteArray: {
            // Use hex literal X'...' for binary data - no charset issues
            const QByteArray ba = v.toByteArray();
            result.append("X'");
            result.append(ba.toHex());
            result.append('\'');
            break;
        }
        default: {
            // Treat as string: convert to UTF-8 and escape.
            // mysql_real_escape_string() requires a buffer of at least length*2+1 bytes
            // (per MySQL docs) where length is the byte length of the source string.
            const QByteArray str = v.toString().toUtf8();
            QByteArray escaped(static_cast<int>(str.size()) * 2 + 1, Qt::Uninitialized);
            const unsigned long escapedLen =
                mysql_real_escape_string(mysql, escaped.data(), str.constData(),
                                         static_cast<unsigned long>(str.size()));
            escaped.resize(static_cast<int>(escapedLen));
            result.append('\'');
            result.append(escaped);
            result.append('\'');
            break;
        }
        }
    }

    return result;
}

bool ADriverMysql::isConnected() const
{
    return m_state == ADatabase::State::Connected && m_mysql != nullptr;
}

/*!
 * \brief Removes cancelled and callback-less queries from the front of the queue.
 * \return true if there is at least one valid query left to process.
 */
bool ADriverMysql::skipInvalidQueries()
{
    while (!m_queuedQueries.empty()) {
        const AMysqlQuery &front = m_queuedQueries.front();
        if ((front.checkReceiver && front.receiver.isNull()) || !front.cb) {
            m_queuedQueries.pop();
        } else {
            return true;
        }
    }
    return false;
}

void ADriverMysql::nextQuery()
{
    if (!isConnected() || m_queryRunning || m_queuedQueries.empty()) {
        return;
    }

    if (!skipInvalidQueries()) {
        selfDriver.reset();
        return;
    }

    // Build the final SQL text, substituting any ? placeholders with escaped values
    AMysqlQuery &front = m_queuedQueries.front();
    if (!front.params.isEmpty()) {
        front.query = buildQueryWithParams(m_mysql, front.query, front.params);
    }

    m_queryRunning = true;
    m_queryPhase   = QueryPhase::Sending;
    handleQueryProgress();
}

void ADriverMysql::handleQueryProgress()
{
    // Guard against spurious notifier calls after a query has already completed
    if (m_queryPhase == QueryPhase::None || m_queuedQueries.empty()) {
        return;
    }

    // Disable notifiers immediately to prevent re-entrant calls while we process
    m_readNotify->setEnabled(false);
    m_writeNotify->setEnabled(false);

    while (true) {
        switch (m_queryPhase) {
        case QueryPhase::None:
            return;

        case QueryPhase::Sending:
        {
            const QByteArray &sql         = m_queuedQueries.front().query;
            const enum net_async_status s = mysql_real_query_nonblocking(
                m_mysql, sql.constData(), static_cast<unsigned long>(sql.size()));

            if (s == NET_ASYNC_NOT_READY) {
                // Wait for the socket to be ready for the next send chunk
                m_readNotify->setEnabled(true);
                m_writeNotify->setEnabled(true);
                return;
            }

            if (s == NET_ASYNC_ERROR) {
                const QString error = QString::fromUtf8(mysql_error(m_mysql));
                qWarning(ASQL_MYSQL) << "Query send error:" << error;
                finishCurrentQuery(error);
                return;
            }

            // NET_ASYNC_COMPLETE: query sent, move on to reading the result
            m_queryPhase = QueryPhase::StoringResult;
            continue;
        }

        case QueryPhase::StoringResult:
        {
            MYSQL_RES *res                = nullptr;
            const enum net_async_status s = mysql_store_result_nonblocking(m_mysql, &res);

            if (s == NET_ASYNC_NOT_READY) {
                m_readNotify->setEnabled(true);
                m_writeNotify->setEnabled(true);
                return;
            }

            if (s == NET_ASYNC_ERROR) {
                const QString error = QString::fromUtf8(mysql_error(m_mysql));
                qWarning(ASQL_MYSQL) << "Store result error:" << error;
                finishCurrentQuery(error);
                return;
            }

            // NET_ASYNC_COMPLETE
            if (res) {
                m_currentResult = res;

                // Read field names (synchronous, cheap metadata access)
                const unsigned int numFields = mysql_num_fields(res);
                MYSQL_FIELD *fields          = mysql_fetch_fields(res);
                auto &q                      = m_queuedQueries.front();
                q.result->m_fields.reserve(static_cast<int>(numFields));
                for (unsigned int i = 0; i < numFields; ++i) {
                    q.result->m_fields.append(QString::fromUtf8(fields[i].name));
                }

                m_queryPhase = QueryPhase::FetchingRows;
                continue;
            } else {
                // No result set: INSERT/UPDATE/DELETE or error
                auto &q = m_queuedQueries.front();
                if (mysql_field_count(m_mysql) == 0) {
                    q.result->m_numRowsAffected = static_cast<qint64>(mysql_affected_rows(m_mysql));
                } else {
                    // mysql_store_result() returned NULL despite having fields → error
                    q.result->m_errorString = QString::fromUtf8(mysql_error(m_mysql));
                    q.result->m_error       = true;
                }
                finishCurrentQuery();
                return;
            }
        }

        case QueryPhase::FetchingRows:
        {
            MYSQL_ROW row                 = nullptr;
            const enum net_async_status s = mysql_fetch_row_nonblocking(m_currentResult, &row);

            if (s == NET_ASYNC_NOT_READY) {
                m_readNotify->setEnabled(true);
                m_writeNotify->setEnabled(true);
                return;
            }

            if (s == NET_ASYNC_ERROR) {
                const QString error = QString::fromUtf8(mysql_error(m_mysql));
                qWarning(ASQL_MYSQL) << "Fetch row error:" << error;
                mysql_free_result(m_currentResult);
                m_currentResult = nullptr;
                finishCurrentQuery(error);
                return;
            }

            // NET_ASYNC_COMPLETE
            if (row == nullptr) {
                // All rows received
                m_queuedQueries.front().result->m_numRowsAffected =
                    static_cast<qint64>(mysql_affected_rows(m_mysql));
                mysql_free_result(m_currentResult);
                m_currentResult = nullptr;
                finishCurrentQuery();
                return;
            }

            // Append the row data
            const unsigned int numFields = mysql_num_fields(m_currentResult);
            unsigned long *lengths       = mysql_fetch_lengths(m_currentResult);
            QVariantList rowData;
            rowData.reserve(static_cast<int>(numFields));
            for (unsigned int i = 0; i < numFields; ++i) {
                if (row[i] == nullptr) {
                    rowData.append(QVariant{});
                } else {
                    rowData.append(QString::fromUtf8(row[i], static_cast<int>(lengths[i])));
                }
            }
            m_queuedQueries.front().result->m_rows.append(std::move(rowData));

            // Try to fetch the next row immediately without re-entering the event loop
            continue;
        }
        }
    }
}

void ADriverMysql::finishCurrentQuery()
{
    m_queryRunning = false;
    m_queryPhase   = QueryPhase::None;

    AMysqlQuery q = std::move(m_queuedQueries.front());
    m_queuedQueries.pop();
    q.done();

    if (!m_queuedQueries.empty()) {
        nextQuery();
    } else {
        selfDriver.reset();
    }
}

void ADriverMysql::finishCurrentQuery(const QString &error)
{
    m_queryRunning = false;
    m_queryPhase   = QueryPhase::None;

    AMysqlQuery q = std::move(m_queuedQueries.front());
    m_queuedQueries.pop();
    q.doneError(error);

    if (!m_queuedQueries.empty()) {
        nextQuery();
    } else {
        selfDriver.reset();
    }
}

void ADriverMysql::finishConnection(const QString &error)
{
    m_readNotify.reset();
    m_writeNotify.reset();

    if (m_currentResult) {
        mysql_free_result(m_currentResult);
        m_currentResult = nullptr;
    }

    if (m_mysql) {
        mysql_close(m_mysql);
        m_mysql = nullptr;
    }

    m_queryRunning = false;
    m_queryPhase   = QueryPhase::None;

    setState(ADatabase::State::Disconnected, error);

    while (!m_queuedQueries.empty()) {
        AMysqlQuery mysqlQuery = std::move(m_queuedQueries.front());
        m_queuedQueries.pop();
        mysqlQuery.doneError(error);
    }

    selfDriver.reset();
}
