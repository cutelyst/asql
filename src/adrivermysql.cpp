/*
 * SPDX-FileCopyrightText: (C) 2025 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "adrivermysql.h"

#include "acoroexpected.h"
#include "aresult.h"

#include <mysql/mysql.h>
#include <type_traits>
#include <vector>

#include <QCborValue>
#include <QDate>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QMutexLocker>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>

Q_LOGGING_CATEGORY(ASQL_MYSQL, "asql.mysql", QtInfoMsg)

using namespace ASql;
using namespace Qt::StringLiterals;

// std::vector<bool> uses bit-packing, so operator[] returns a proxy and &vec[i]
// is not a valid pointer.  Use unsigned char as storage (same size as bool/my_bool)
// and reinterpret_cast when assigning to MYSQL_BIND fields that expect bool* (MySQL 8+)
// or my_bool* = char* (MariaDB).
using MysqlBool = unsigned char;

// ---------------------------------------------------------------------------
// AResultMysql
// ---------------------------------------------------------------------------

bool AResultMysql::lastResultSet() const
{
    return m_lastResultSet;
}

bool AResultMysql::hasError() const
{
    return m_error.has_value();
}

QString AResultMysql::errorString() const
{
    return m_error.value_or(QString{});
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
    return m_fields.empty() ? 0 : static_cast<int>(m_rows.size()) / m_fields.size();
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
    const int idx = row * m_fields.size() + column;
    if (idx >= 0 && idx < m_rows.size()) {
        return m_rows.at(idx);
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
    const QString s = v.toString();
    QTime t         = QTime::fromString(s, u"hh:mm:ss.zzz"_s);
    if (!t.isValid()) {
        t = QTime::fromString(s, Qt::ISODate);
    }
    return t;
}

QDateTime AResultMysql::toDateTime(int row, int column) const
{
    const QVariant v = value(row, column);
    if (v.typeId() == QMetaType::QDateTime) {
        return v.toDateTime();
    }
    const QString s = v.toString();
    QDateTime dt    = QDateTime::fromString(s, Qt::ISODateWithMs);
    if (!dt.isValid()) {
        dt = QDateTime::fromString(s, Qt::ISODate);
    }
    return dt;
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
// AMysqlThread helpers
// ---------------------------------------------------------------------------

/*!
 * \brief Reads all columns of the current row from a text-protocol result set.
 *
 * Each column value is appended to \a rows as a QString (or a null QVariant
 * for SQL NULL).  \a numFields must equal mysql_num_fields(res).
 */
static void
    mysqlFillRow(MYSQL_ROW row, unsigned int numFields, unsigned long *lengths, QVariantList &rows)
{
    for (unsigned int i = 0; i < numFields; ++i) {
        if (row[i] == nullptr) {
            rows.append(QVariant{});
        } else {
            rows.append(QString::fromUtf8(row[i], static_cast<int>(lengths[i])));
        }
    }
}

/*!
 * \brief Binds \a params to a prepared statement \a stmt.
 *
 * All data buffers passed to mysql_stmt_bind_param() must remain valid until
 * mysql_stmt_execute() returns, so we store them in the vectors passed in.
 *
 * \return empty optional on success; an error message string on failure.
 */
static std::optional<QString> mysqlBindParams(MYSQL_STMT *stmt,
                                              const QVariantList &params,
                                              std::vector<MYSQL_BIND> &binds,
                                              std::vector<long long> &intVals,
                                              std::vector<double> &doubleVals,
                                              std::unique_ptr<bool[]> &nullFlags,
                                              std::vector<QByteArray> &strVals,
                                              std::vector<unsigned long> &strLengths)
{
    const int n = params.size();
    binds.assign(n, MYSQL_BIND{});
    intVals.assign(n, 0LL);
    doubleVals.assign(n, 0.0);
    nullFlags = std::make_unique<bool[]>(n); // value-initialized to false
    strVals.resize(n);
    strLengths.assign(n, 0UL);

    for (int i = 0; i < n; ++i) {
        const QVariant &v = params.at(i);

        if (v.isNull()) {
            nullFlags[i]         = true;
            binds[i].buffer_type = MYSQL_TYPE_NULL;
            binds[i].is_null     = &nullFlags[i];
            continue;
        }

        binds[i].is_null = &nullFlags[i]; // points to false (not null)

        switch (v.userType()) {
        case QMetaType::Bool:
            intVals[i]           = v.toBool() ? 1 : 0;
            binds[i].buffer_type = MYSQL_TYPE_TINY;
            binds[i].buffer      = &intVals[i];
            break;
        case QMetaType::Int:
            intVals[i]           = v.toInt();
            binds[i].buffer_type = MYSQL_TYPE_LONG;
            binds[i].buffer      = &intVals[i];
            break;
        case QMetaType::UInt:
            intVals[i]           = static_cast<long long>(v.toUInt());
            binds[i].buffer_type = MYSQL_TYPE_LONG;
            binds[i].buffer      = &intVals[i];
            binds[i].is_unsigned = true;
            break;
        case QMetaType::LongLong:
            intVals[i]           = v.toLongLong();
            binds[i].buffer_type = MYSQL_TYPE_LONGLONG;
            binds[i].buffer      = &intVals[i];
            break;
        case QMetaType::ULongLong:
            intVals[i]           = static_cast<long long>(v.toULongLong());
            binds[i].buffer_type = MYSQL_TYPE_LONGLONG;
            binds[i].buffer      = &intVals[i];
            binds[i].is_unsigned = true;
            break;
        case QMetaType::Double:
        case QMetaType::Float:
            doubleVals[i]        = v.toDouble();
            binds[i].buffer_type = MYSQL_TYPE_DOUBLE;
            binds[i].buffer      = &doubleVals[i];
            break;
        case QMetaType::QByteArray:
            strVals[i]             = v.toByteArray();
            strLengths[i]          = static_cast<unsigned long>(strVals[i].size());
            binds[i].buffer_type   = MYSQL_TYPE_BLOB;
            binds[i].buffer        = strVals[i].data();
            binds[i].buffer_length = strLengths[i];
            binds[i].length        = &strLengths[i];
            break;
        case QMetaType::QDate:
            strVals[i]             = v.value<QDate>().toString(Qt::ISODate).toUtf8();
            strLengths[i]          = static_cast<unsigned long>(strVals[i].size());
            binds[i].buffer_type   = MYSQL_TYPE_STRING;
            binds[i].buffer        = strVals[i].data();
            binds[i].buffer_length = strLengths[i];
            binds[i].length        = &strLengths[i];
            break;
        case QMetaType::QTime:
            strVals[i]             = v.value<QTime>().toString(u"hh:mm:ss.zzz"_s).toUtf8();
            strLengths[i]          = static_cast<unsigned long>(strVals[i].size());
            binds[i].buffer_type   = MYSQL_TYPE_STRING;
            binds[i].buffer        = strVals[i].data();
            binds[i].buffer_length = strLengths[i];
            binds[i].length        = &strLengths[i];
            break;
        case QMetaType::QDateTime:
            strVals[i]             = v.value<QDateTime>().toString(Qt::ISODateWithMs).toUtf8();
            strLengths[i]          = static_cast<unsigned long>(strVals[i].size());
            binds[i].buffer_type   = MYSQL_TYPE_STRING;
            binds[i].buffer        = strVals[i].data();
            binds[i].buffer_length = strLengths[i];
            binds[i].length        = &strLengths[i];
            break;
        default:
        {
            // For QJsonObject / QJsonValue serialize to compact JSON; for all
            // other types fall back to toString().
            const int typeId = v.userType();
            if (typeId == QMetaType::QJsonObject || typeId == QMetaType::QJsonValue ||
                typeId == QMetaType::QJsonArray) {
                strVals[i] = QJsonDocument::fromVariant(v).toJson(QJsonDocument::Compact);
            } else {
                strVals[i] = v.toString().toUtf8();
            }
            strLengths[i]          = static_cast<unsigned long>(strVals[i].size());
            binds[i].buffer_type   = MYSQL_TYPE_STRING;
            binds[i].buffer        = strVals[i].data();
            binds[i].buffer_length = strLengths[i];
            binds[i].length        = &strLengths[i];
            break;
        }
        }
    }

    if (mysql_stmt_bind_param(stmt, binds.data())) {
        return QString::fromUtf8(mysql_stmt_error(stmt));
    }
    return {};
}

/*!
 * \brief Fetches all rows from a prepared-statement result set into \a rows.
 *
 * Binds all result columns with a 256-byte initial buffer (large enough for
 * typical scalar values).  If a value is truncated, mysql_stmt_fetch_column()
 * is used to retrieve the full data.  Binary/BLOB columns are returned as
 * QByteArray; all other columns are returned as QString (UTF-8 decoded).
 *
 * \return empty optional on success; an error message string on failure.
 */
static std::optional<QString> mysqlFetchStmtRows(MYSQL_STMT *stmt,
                                                 unsigned int numFields,
                                                 MYSQL_FIELD *fields,
                                                 QVariantList &rows)
{
    // Initial buffer per column — covers nearly all scalar values without a
    // second fetch, but is small enough to avoid excessive allocation for
    // wide SELECT results.
    constexpr unsigned long kInitBufSize = 256;

    std::vector<MYSQL_BIND> resBind(numFields, MYSQL_BIND{});
    std::vector<unsigned long> lengths(numFields, 0UL);
    // Use proper bool arrays so mysql_stmt_fetch() can write bool values
    // without triggering undefined behaviour through type-punning.
    auto isNull  = std::make_unique<bool[]>(numFields); // value-initialized to false
    auto isError = std::make_unique<bool[]>(numFields);
    // Allocate per-column storage on the heap.
    std::vector<QByteArray> bufs(numFields, QByteArray(kInitBufSize, Qt::Uninitialized));

    for (unsigned int i = 0; i < numFields; ++i) {
        // MariaDB/MySQL set BINARY_FLAG on ALL numeric types too, so we must
        // NOT rely on BINARY_FLAG alone.  Only treat a column as binary when
        // it is a genuine blob/binary type or a string column with the binary
        // charset (charset number 63 == binary in MySQL protocol).
        const enum_field_types ft = fields[i].type;
        const bool isBlobType     = ft == MYSQL_TYPE_BLOB || ft == MYSQL_TYPE_TINY_BLOB ||
                                    ft == MYSQL_TYPE_MEDIUM_BLOB || ft == MYSQL_TYPE_LONG_BLOB;
        const bool isBinaryString =
            (ft == MYSQL_TYPE_STRING || ft == MYSQL_TYPE_VAR_STRING || ft == MYSQL_TYPE_VARCHAR) &&
            fields[i].charsetnr == 63; // binary pseudo-charset
        const bool isBinary      = isBlobType || isBinaryString;
        resBind[i].buffer_type   = isBinary ? MYSQL_TYPE_BLOB : MYSQL_TYPE_STRING;
        resBind[i].buffer        = bufs[i].data();
        resBind[i].buffer_length = kInitBufSize;
        resBind[i].length        = &lengths[i];
        resBind[i].is_null       = &isNull[i];
        resBind[i].error         = &isError[i];
    }

    if (mysql_stmt_bind_result(stmt, resBind.data())) {
        return QString::fromUtf8(mysql_stmt_error(stmt));
    }

    int fetchRet;
    while ((fetchRet = mysql_stmt_fetch(stmt)) == 0 || fetchRet == MYSQL_DATA_TRUNCATED) {
        for (unsigned int i = 0; i < numFields; ++i) {
            if (isNull[i]) {
                rows.append(QVariant{});
                continue;
            }

            const bool isBinary  = resBind[i].buffer_type == MYSQL_TYPE_BLOB;
            const auto actualLen = static_cast<int>(lengths[i]);

            if (isError[i]) {
                // Value was truncated — fetch the full data now that we know its length.
                QByteArray bigBuf(actualLen, Qt::Uninitialized);
                MYSQL_BIND colBind{};
                colBind.buffer_type   = resBind[i].buffer_type;
                colBind.buffer        = bigBuf.data();
                colBind.buffer_length = lengths[i];
                if (mysql_stmt_fetch_column(stmt, &colBind, i, 0) != 0) {
                    rows.append(QVariant{});
                    continue;
                }
                if (isBinary) {
                    rows.append(bigBuf);
                } else {
                    rows.append(QString::fromUtf8(bigBuf.constData(), actualLen));
                }
            } else {
                // Data fits in the initial buffer.
                if (isBinary) {
                    rows.append(bufs[i].left(actualLen));
                } else {
                    rows.append(QString::fromUtf8(bufs[i].constData(), actualLen));
                }
            }
        }
    }

    if (fetchRet != MYSQL_NO_DATA) {
        return QString::fromUtf8(mysql_stmt_error(stmt));
    }
    return {};
}

// ---------------------------------------------------------------------------
// AMysqlThread
// ---------------------------------------------------------------------------

AMysqlThread::AMysqlThread(const QString &connInfo)
    : m_connInfo(connInfo)
{
}

AMysqlThread::~AMysqlThread()
{
    if (m_mysql) {
        // Close any cached prepared statements first
        for (MYSQL_STMT *stmt : m_preparedQueries) {
            mysql_stmt_close(stmt);
        }
        mysql_close(m_mysql);
    }
}

void AMysqlThread::enqueueAndSignal(MysqlQueryPromise &promise)
{
    {
        QMutexLocker _(&m_promisesMutex);
        m_promisesReady.enqueue(std::move(promise));
    }
    Q_EMIT queryReady();
}

void AMysqlThread::open()
{
    m_mysql = mysql_init(nullptr);
    if (!m_mysql) {
        Q_EMIT openned(false, u"mysql_init() failed: out of memory"_s);
        return;
    }

    QUrl url(m_connInfo);
    const QByteArray host     = url.host().toUtf8();
    const QByteArray user     = url.userName().toUtf8();
    const QByteArray password = url.password().toUtf8();
    const QByteArray database = url.path().mid(1).toUtf8(); // strip leading '/'
    const unsigned int port   = (url.port() > 0) ? static_cast<unsigned int>(url.port()) : 3306u;

    MYSQL *conn = mysql_real_connect(m_mysql,
                                     host.isEmpty() ? nullptr : host.constData(),
                                     user.isEmpty() ? nullptr : user.constData(),
                                     password.isEmpty() ? nullptr : password.constData(),
                                     database.isEmpty() ? nullptr : database.constData(),
                                     port,
                                     nullptr, // unix socket
                                     0        // client flags
    );

    if (!conn) {
        const QString error = QString::fromUtf8(mysql_error(m_mysql));
        mysql_close(m_mysql);
        m_mysql = nullptr;
        Q_EMIT openned(false, error);
        return;
    }

    Q_EMIT openned(true, {});
}

MYSQL_STMT *AMysqlThread::prepare(MysqlQueryPromise &promise)
{
    MYSQL_STMT *stmt = mysql_stmt_init(m_mysql);
    if (!stmt) {
        promise.result->m_error = QString::fromUtf8(mysql_error(m_mysql));
        return nullptr;
    }

    const QByteArray &sql = promise.result->m_query;
    if (mysql_stmt_prepare(stmt, sql.constData(), static_cast<unsigned long>(sql.size())) != 0) {
        promise.result->m_error = QString::fromUtf8(mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return nullptr;
    }

    return stmt;
}

void AMysqlThread::query(MysqlQueryPromise promise)
{
    auto _ = qScopeGuard([&] { enqueueAndSignal(promise); });

    MYSQL_STMT *stmt = prepare(promise);
    if (!stmt) {
        return;
    }
    auto stmtGuard = qScopeGuard([&] { mysql_stmt_close(stmt); });

    const QVariantList &params = promise.result->m_queryArgs;
    // Bind data must outlive mysql_stmt_execute() since MySQL stores pointers
    // into these buffers.  Declare outside the if-block intentionally.
    std::vector<MYSQL_BIND> binds;
    std::vector<long long> intVals;
    std::vector<double> doubleVals;
    std::unique_ptr<bool[]> nullFlags;
    std::vector<QByteArray> strVals;
    std::vector<unsigned long> strLengths;

    if (!params.isEmpty()) {
        auto bindErr = mysqlBindParams(
            stmt, params, binds, intVals, doubleVals, nullFlags, strVals, strLengths);
        if (bindErr.has_value()) {
            promise.result->m_error = bindErr;
            return;
        }
    }

    if (mysql_stmt_execute(stmt) != 0) {
        promise.result->m_error = QString::fromUtf8(mysql_stmt_error(stmt));
        return;
    }

    MYSQL_RES *meta = mysql_stmt_result_metadata(stmt);
    if (meta) {
        auto metaGuard = qScopeGuard([&] { mysql_free_result(meta); });

        const unsigned int numFields = mysql_num_fields(meta);
        MYSQL_FIELD *fields          = mysql_fetch_fields(meta);
        promise.result->m_fields.reserve(static_cast<int>(numFields));
        for (unsigned int i = 0; i < numFields; ++i) {
            promise.result->m_fields.append(QString::fromUtf8(fields[i].name));
        }

        auto fetchErr = mysqlFetchStmtRows(stmt, numFields, fields, promise.result->m_rows);
        if (fetchErr.has_value()) {
            promise.result->m_error = fetchErr;
            return;
        }
    }

    promise.result->m_numRowsAffected = static_cast<qint64>(mysql_stmt_affected_rows(stmt));
}

void AMysqlThread::queryPrepared(MysqlQueryPromise promise)
{
    const int queryId = promise.preparedQuery->identification();

    MYSQL_STMT *stmt = nullptr;
    auto it          = m_preparedQueries.constFind(queryId);
    if (it != m_preparedQueries.constEnd()) {
        stmt = it.value();
    } else {
        stmt = prepare(promise);
        if (!stmt) {
            enqueueAndSignal(promise);
            return;
        }
        m_preparedQueries.insert(queryId, stmt);
    }

    auto _ = qScopeGuard([&] {
        enqueueAndSignal(promise);
        // Reset the statement so it can be reused
        if (mysql_stmt_reset(stmt) != 0) {
            qWarning(ASQL_MYSQL) << "Failed to reset prepared statement:" << mysql_stmt_error(stmt);
            m_preparedQueries.remove(queryId);
            mysql_stmt_close(stmt);
        }
    });

    const QVariantList &params = promise.result->m_queryArgs;
    // Bind data must outlive mysql_stmt_execute() since MySQL stores pointers
    // into these buffers.  Declare outside the if-block intentionally.
    std::vector<MYSQL_BIND> binds;
    std::vector<long long> intVals;
    std::vector<double> doubleVals;
    std::unique_ptr<bool[]> nullFlags;
    std::vector<QByteArray> strVals;
    std::vector<unsigned long> strLengths;

    if (!params.isEmpty()) {
        auto bindErr = mysqlBindParams(
            stmt, params, binds, intVals, doubleVals, nullFlags, strVals, strLengths);
        if (bindErr.has_value()) {
            promise.result->m_error = bindErr;
            return;
        }
    }

    if (mysql_stmt_execute(stmt) != 0) {
        promise.result->m_error = QString::fromUtf8(mysql_stmt_error(stmt));
        return;
    }

    MYSQL_RES *meta = mysql_stmt_result_metadata(stmt);
    if (meta) {
        auto metaGuard = qScopeGuard([&] { mysql_free_result(meta); });

        const unsigned int numFields = mysql_num_fields(meta);
        MYSQL_FIELD *fields          = mysql_fetch_fields(meta);
        promise.result->m_fields.reserve(static_cast<int>(numFields));
        for (unsigned int i = 0; i < numFields; ++i) {
            promise.result->m_fields.append(QString::fromUtf8(fields[i].name));
        }

        auto fetchErr = mysqlFetchStmtRows(stmt, numFields, fields, promise.result->m_rows);
        if (fetchErr.has_value()) {
            promise.result->m_error = fetchErr;
            return;
        }
    }

    promise.result->m_numRowsAffected = static_cast<qint64>(mysql_stmt_affected_rows(stmt));
}

void AMysqlThread::queryExec(MysqlQueryPromise promise)
{
    auto _ = qScopeGuard([&] { enqueueAndSignal(promise); });

    const QByteArray &sql = promise.result->m_query;
    if (mysql_real_query(m_mysql, sql.constData(), static_cast<unsigned long>(sql.size())) != 0) {
        promise.result->m_error = QString::fromUtf8(mysql_error(m_mysql));
        return;
    }

    MYSQL_RES *res = mysql_store_result(m_mysql);
    if (res) {
        auto resGuard = qScopeGuard([&] { mysql_free_result(res); });

        const unsigned int numFields = mysql_num_fields(res);
        MYSQL_FIELD *fields          = mysql_fetch_fields(res);
        promise.result->m_fields.reserve(static_cast<int>(numFields));
        for (unsigned int i = 0; i < numFields; ++i) {
            promise.result->m_fields.append(QString::fromUtf8(fields[i].name));
        }

        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res)) != nullptr) {
            unsigned long *lengths = mysql_fetch_lengths(res);
            mysqlFillRow(row, numFields, lengths, promise.result->m_rows);
        }
    } else if (mysql_field_count(m_mysql) != 0) {
        // Query should have produced a result set but didn't → error
        promise.result->m_error = QString::fromUtf8(mysql_error(m_mysql));
        return;
    }

    promise.result->m_numRowsAffected = static_cast<qint64>(mysql_affected_rows(m_mysql));
}

// ---------------------------------------------------------------------------
// ADriverMysql
// ---------------------------------------------------------------------------

ADriverMysql::ADriverMysql(const QString &connInfo)
    : ADriver(connInfo)
    , m_worker(connInfo)
{
    m_thread.setObjectName(connInfo);
    m_worker.moveToThread(&m_thread);

    connect(&m_worker, &AMysqlThread::queryReady, this, [this] {
        QMutexLocker _(&m_worker.m_promisesMutex);
        while (!m_worker.m_promisesReady.isEmpty()) {
            MysqlQueryPromise promise = m_worker.m_promisesReady.dequeue();
            if (!promise.receiver.has_value() || !promise.receiver->isNull()) {
                if (promise.cb) {
                    AResult result{promise.result};
                    promise.cb.deliverResult(result);
                }
            }

            if (promise.result->m_lastResultSet && --m_queueSize == 0) {
                selfDriver.reset();
            }
        }
    }, Qt::QueuedConnection);

    m_thread.start();
}

ADriverMysql::~ADriverMysql()
{
    Q_ASSERT(m_thread.thread() == QThread::currentThread());

    m_thread.requestInterruption();
    m_thread.quit();
    m_thread.wait();
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

    setState(ADatabase::State::Connecting, {});
    ++m_queueSize;
    selfDriver = driver;

    OpenPromise promise{
        .cb = cb,
    };
    if (receiver) {
        promise.receiver = receiver;
    }

    connect(&m_worker, &AMysqlThread::openned, this, [this, promise](bool isOpen, QString error) {
        if (isOpen) {
            setState(ADatabase::State::Connected, {});
        } else {
            setState(ADatabase::State::Disconnected, error);
        }

        if (!promise.receiver.has_value() || !promise.receiver->isNull()) {
            if (promise.cb) {
                promise.cb(isOpen, error);
            }
        }

        if (--m_queueSize == 0) {
            selfDriver.reset();
        }
    }, Qt::SingleShotConnection);

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    QMetaObject::invokeMethod(&m_worker, &AMysqlThread::open, Qt::QueuedConnection);
#else
    QMetaObject::invokeMethod(&m_worker, "open", Qt::QueuedConnection);
#endif
}

bool ADriverMysql::isOpen() const
{
    return m_state == ADatabase::State::Connected;
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

void ADriverMysql::exec(const std::shared_ptr<ADriver> &db,
                        QUtf8StringView query,
                        QObject *receiver,
                        ACoroDataRef cb)
{
    ++m_queueSize;
    selfDriver = db;

    MysqlQueryPromise data{
        .cb     = std::move(cb),
        .result = std::make_shared<AResultMysql>(),
    };
    if (receiver) {
        data.receiver = receiver;
    }
    data.result->m_query.setRawData(query.data(), query.size());

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    QMetaObject::invokeMethod(
        &m_worker, &AMysqlThread::queryExec, Qt::QueuedConnection, std::move(data));
#else
    QMetaObject::invokeMethod(&m_worker,
                              "queryExec",
                              Qt::QueuedConnection,
                              Q_ARG(ASql::MysqlQueryPromise, std::move(data)));
#endif
}

void ADriverMysql::exec(const std::shared_ptr<ADriver> &db,
                        QStringView query,
                        QObject *receiver,
                        ACoroDataRef cb)
{
    ++m_queueSize;
    selfDriver = db;

    MysqlQueryPromise data{
        .cb     = std::move(cb),
        .result = std::make_shared<AResultMysql>(),
    };
    if (receiver) {
        data.receiver = receiver;
    }
    data.result->m_query = query.toUtf8();

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    QMetaObject::invokeMethod(
        &m_worker, &AMysqlThread::queryExec, Qt::QueuedConnection, std::move(data));
#else
    QMetaObject::invokeMethod(&m_worker,
                              "queryExec",
                              Qt::QueuedConnection,
                              Q_ARG(ASql::MysqlQueryPromise, std::move(data)));
#endif
}

void ADriverMysql::exec(const std::shared_ptr<ADriver> &db,
                        QUtf8StringView query,
                        const QVariantList &params,
                        QObject *receiver,
                        ACoroDataRef cb)
{
    ++m_queueSize;
    selfDriver = db;

    MysqlQueryPromise data{
        .cb     = std::move(cb),
        .result = std::make_shared<AResultMysql>(),
    };
    if (receiver) {
        data.receiver = receiver;
    }
    data.result->m_query.setRawData(query.data(), query.size());
    data.result->m_queryArgs = params;

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    QMetaObject::invokeMethod(
        &m_worker, &AMysqlThread::query, Qt::QueuedConnection, std::move(data));
#else
    QMetaObject::invokeMethod(
        &m_worker, "query", Qt::QueuedConnection, Q_ARG(ASql::MysqlQueryPromise, std::move(data)));
#endif
}

void ADriverMysql::exec(const std::shared_ptr<ADriver> &db,
                        QStringView query,
                        const QVariantList &params,
                        QObject *receiver,
                        ACoroDataRef cb)
{
    ++m_queueSize;
    selfDriver = db;

    MysqlQueryPromise data{
        .cb     = std::move(cb),
        .result = std::make_shared<AResultMysql>(),
    };
    if (receiver) {
        data.receiver = receiver;
    }
    data.result->m_query     = query.toUtf8();
    data.result->m_queryArgs = params;

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    QMetaObject::invokeMethod(
        &m_worker, &AMysqlThread::query, Qt::QueuedConnection, std::move(data));
#else
    QMetaObject::invokeMethod(
        &m_worker, "query", Qt::QueuedConnection, Q_ARG(ASql::MysqlQueryPromise, std::move(data)));
#endif
}

void ADriverMysql::exec(const std::shared_ptr<ADriver> &db,
                        const APreparedQuery &query,
                        const QVariantList &params,
                        QObject *receiver,
                        ACoroDataRef cb)
{
    ++m_queueSize;
    selfDriver = db;

    MysqlQueryPromise data{
        .preparedQuery = query,
        .cb            = std::move(cb),
        .result        = std::make_shared<AResultMysql>(),
    };
    if (receiver) {
        data.receiver = receiver;
    }
    data.result->m_query     = query.query();
    data.result->m_queryArgs = params;

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    QMetaObject::invokeMethod(
        &m_worker, &AMysqlThread::queryPrepared, Qt::QueuedConnection, std::move(data));
#else
    QMetaObject::invokeMethod(&m_worker,
                              "queryPrepared",
                              Qt::QueuedConnection,
                              Q_ARG(ASql::MysqlQueryPromise, std::move(data)));
#endif
}

void ADriverMysql::setLastQuerySingleRowMode()
{
    // Not supported for MySQL driver
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
    return m_queueSize;
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
