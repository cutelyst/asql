/*
 * SPDX-FileCopyrightText: (C) 2025 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */
#include "ADriverOdbc.hpp"

#include <QCborValue>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLoggingCategory>
#include <QMutexLocker>
#include <QTimeZone>
#include <QUuid>

Q_LOGGING_CATEGORY(ASQL_ODBC, "asql.odbc", QtInfoMsg)

using namespace Qt::StringLiterals;

namespace ASql {

// SQLWCHAR is unsigned short (2 bytes) on Linux/unixODBC, same size as QChar.
static_assert(sizeof(SQLWCHAR) == sizeof(QChar),
              "SQLWCHAR and QChar must be the same size for direct casting");

namespace {

/*!
 * \brief Strips the "odbc:" or "odbc://" scheme prefix from the connection string.
 * The remaining string is passed directly to SQLDriverConnect.
 */
QString extractConnString(const QString &connInfo)
{
    if (connInfo.startsWith(u"odbc://"_s)) {
        return connInfo.mid(7);
    } else if (connInfo.startsWith(u"odbc:"_s)) {
        return connInfo.mid(5);
    }
    return connInfo;
}

} // namespace

// ─────────────────────────────── AOdbcThread ──────────────────────────────────

AOdbcThread::AOdbcThread(const QString &connInfo)
    : m_connString(connInfo)
{
}

AOdbcThread::~AOdbcThread()
{
    // Free all cached prepared statement handles
    for (auto it = m_preparedStmts.begin(); it != m_preparedStmts.end(); ++it) {
        SQLFreeHandle(SQL_HANDLE_STMT, it.value());
    }
    m_preparedStmts.clear();

    if (m_dbc != SQL_NULL_HDBC) {
        SQLDisconnect(m_dbc);
        SQLFreeHandle(SQL_HANDLE_DBC, m_dbc);
    }
    if (m_env != SQL_NULL_HENV) {
        SQLFreeHandle(SQL_HANDLE_ENV, m_env);
    }
}

QString AOdbcThread::odbcError(SQLSMALLINT handleType, SQLHANDLE handle)
{
    SQLWCHAR state[6];
    SQLWCHAR msg[SQL_MAX_MESSAGE_LENGTH];
    SQLINTEGER nativeError;
    SQLSMALLINT msgLen;
    SQLSMALLINT recNum = 1;
    QString result;

    while (SQLGetDiagRecW(handleType,
                          handle,
                          recNum,
                          state,
                          &nativeError,
                          msg,
                          SQL_MAX_MESSAGE_LENGTH,
                          &msgLen) != SQL_NO_DATA) {
        if (!result.isEmpty()) {
            result += u'\n';
        }
        result += QString(reinterpret_cast<const QChar *>(msg), msgLen);
        ++recNum;
    }
    return result;
}

void AOdbcThread::open()
{
    const QString dsn = extractConnString(m_connString);

    // Allocate environment handle
    SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &m_env);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
        Q_EMIT openned(false, u"Failed to allocate ODBC environment handle"_s);
        return;
    }

    // Set ODBC version
    ret =
        SQLSetEnvAttr(m_env, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
        const QString error = u"Failed to set ODBC version: "_s + odbcError(SQL_HANDLE_ENV, m_env);
        SQLFreeHandle(SQL_HANDLE_ENV, m_env);
        m_env = SQL_NULL_HENV;
        Q_EMIT openned(false, error);
        return;
    }

    // Allocate connection handle
    ret = SQLAllocHandle(SQL_HANDLE_DBC, m_env, &m_dbc);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
        const QString error =
            u"Failed to allocate ODBC connection handle: "_s + odbcError(SQL_HANDLE_ENV, m_env);
        SQLFreeHandle(SQL_HANDLE_ENV, m_env);
        m_env = SQL_NULL_HENV;
        Q_EMIT openned(false, error);
        return;
    }

    // Connect using the connection string
    const std::wstring wDsn = dsn.toStdWString();
    SQLWCHAR outConnStr[1024];
    SQLSMALLINT outConnStrLen = 0;

    ret = SQLDriverConnectW(m_dbc,
                            nullptr,
                            reinterpret_cast<SQLWCHAR *>(const_cast<wchar_t *>(wDsn.c_str())),
                            SQL_NTS,
                            outConnStr,
                            sizeof(outConnStr) / sizeof(SQLWCHAR),
                            &outConnStrLen,
                            SQL_DRIVER_NOPROMPT);

    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
        const QString error =
            u"Failed to connect to ODBC data source: "_s + odbcError(SQL_HANDLE_DBC, m_dbc);
        SQLFreeHandle(SQL_HANDLE_DBC, m_dbc);
        SQLFreeHandle(SQL_HANDLE_ENV, m_env);
        m_dbc = SQL_NULL_HDBC;
        m_env = SQL_NULL_HENV;
        Q_EMIT openned(false, error);
        return;
    }

    // Enable manual commit (auto-commit is on by default; keep it on for simplicity)
    Q_EMIT openned(true, {});
}

QString AOdbcThread::readWCharColumn(SQLHSTMT stmt, SQLUSMALLINT col)
{
    // Read in chunks of 256 SQLWCHAR (512 bytes) + null terminator
    constexpr SQLLEN CHUNK_SIZE = 256;
    SQLWCHAR buf[CHUNK_SIZE + 1];
    SQLLEN indicator = 0;
    QString result;
    SQLRETURN ret;

    do {
        ret = SQLGetData(stmt,
                         col,
                         SQL_C_WCHAR,
                         buf,
                         (CHUNK_SIZE + 1) * static_cast<SQLLEN>(sizeof(SQLWCHAR)),
                         &indicator);

        if (ret == SQL_ERROR) {
            return {};
        }

        if (indicator == SQL_NULL_DATA) {
            return {};
        }

        if (ret == SQL_SUCCESS_WITH_INFO) {
            // Buffer was too small; CHUNK_SIZE chars were written (without null)
            result += QString(reinterpret_cast<const QChar *>(buf), CHUNK_SIZE);
        } else if (ret == SQL_SUCCESS) {
            // indicator == SQL_NO_TOTAL means length unknown; use strnlen equivalent
            if (indicator == SQL_NO_TOTAL) {
                // Scan for null terminator
                SQLLEN len = 0;
                while (len < CHUNK_SIZE && buf[len] != 0) {
                    ++len;
                }
                result += QString(reinterpret_cast<const QChar *>(buf), static_cast<int>(len));
            } else {
                // indicator is number of bytes (not including null terminator)
                const int numChars = static_cast<int>(indicator / sizeof(SQLWCHAR));
                result += QString(reinterpret_cast<const QChar *>(buf), numChars);
            }
        }
    } while (ret == SQL_SUCCESS_WITH_INFO);

    return result;
}

QVariant AOdbcThread::columnValue(SQLHSTMT stmt, SQLUSMALLINT col, SQLSMALLINT sqlType)
{
    SQLLEN indicator = 0;

    switch (sqlType) {
    case SQL_BIT:
    {
        SQLCHAR val   = 0;
        SQLRETURN ret = SQLGetData(stmt, col, SQL_C_BIT, &val, sizeof(val), &indicator);
        if (ret == SQL_ERROR || indicator == SQL_NULL_DATA) {
            return QVariant(QMetaType::fromType<bool>());
        }
        return val != 0;
    }

    case SQL_TINYINT:
    {
        SQLSCHAR val  = 0;
        SQLRETURN ret = SQLGetData(stmt, col, SQL_C_STINYINT, &val, sizeof(val), &indicator);
        if (ret == SQL_ERROR || indicator == SQL_NULL_DATA) {
            return QVariant(QMetaType::fromType<int>());
        }
        return static_cast<int>(val);
    }

    case SQL_SMALLINT:
    {
        SQLSMALLINT val = 0;
        SQLRETURN ret   = SQLGetData(stmt, col, SQL_C_SSHORT, &val, sizeof(val), &indicator);
        if (ret == SQL_ERROR || indicator == SQL_NULL_DATA) {
            return QVariant(QMetaType::fromType<int>());
        }
        return static_cast<int>(val);
    }

    case SQL_INTEGER:
    {
        SQLINTEGER val = 0;
        SQLRETURN ret  = SQLGetData(stmt, col, SQL_C_SLONG, &val, sizeof(val), &indicator);
        if (ret == SQL_ERROR || indicator == SQL_NULL_DATA) {
            return QVariant(QMetaType::fromType<int>());
        }
        return static_cast<int>(val);
    }

    case SQL_BIGINT:
    {
        SQLBIGINT val = 0;
        SQLRETURN ret = SQLGetData(stmt, col, SQL_C_SBIGINT, &val, sizeof(val), &indicator);
        if (ret == SQL_ERROR || indicator == SQL_NULL_DATA) {
            return QVariant(QMetaType::fromType<qint64>());
        }
        return static_cast<qint64>(val);
    }

    case SQL_REAL:
    {
        SQLREAL val   = 0;
        SQLRETURN ret = SQLGetData(stmt, col, SQL_C_FLOAT, &val, sizeof(val), &indicator);
        if (ret == SQL_ERROR || indicator == SQL_NULL_DATA) {
            return QVariant(QMetaType::fromType<double>());
        }
        return static_cast<double>(val);
    }

    case SQL_FLOAT:
    case SQL_DOUBLE:
    {
        SQLDOUBLE val = 0;
        SQLRETURN ret = SQLGetData(stmt, col, SQL_C_DOUBLE, &val, sizeof(val), &indicator);
        if (ret == SQL_ERROR || indicator == SQL_NULL_DATA) {
            return QVariant(QMetaType::fromType<double>());
        }
        return static_cast<double>(val);
    }

    case SQL_BINARY:
    case SQL_VARBINARY:
    case SQL_LONGVARBINARY:
    {
        // Get data length first
        SQLRETURN ret = SQLGetData(stmt, col, SQL_C_BINARY, nullptr, 0, &indicator);
        if (ret == SQL_ERROR || indicator == SQL_NULL_DATA) {
            return QVariant(QMetaType::fromType<QByteArray>());
        }
        if (indicator == SQL_NO_TOTAL || indicator < 0) {
            // Read in chunks
            QByteArray result;
            constexpr SQLLEN CHUNK = 4096;
            QByteArray chunk(CHUNK, Qt::Uninitialized);
            do {
                ret = SQLGetData(stmt, col, SQL_C_BINARY, chunk.data(), CHUNK, &indicator);
                if (ret == SQL_ERROR) {
                    break;
                }
                const SQLLEN got =
                    (ret == SQL_SUCCESS_WITH_INFO) ? CHUNK : (indicator > 0 ? indicator : 0);
                result.append(chunk.constData(), static_cast<qsizetype>(got));
            } while (ret == SQL_SUCCESS_WITH_INFO);
            return result;
        }
        QByteArray data(static_cast<qsizetype>(indicator), Qt::Uninitialized);
        SQLGetData(stmt, col, SQL_C_BINARY, data.data(), indicator, &indicator);
        return data;
    }

    case SQL_TYPE_DATE:
    {
        DATE_STRUCT ds{};
        SQLRETURN ret = SQLGetData(stmt, col, SQL_C_TYPE_DATE, &ds, sizeof(ds), &indicator);
        if (ret == SQL_ERROR || indicator == SQL_NULL_DATA) {
            return QVariant(QMetaType::fromType<QDate>());
        }
        return QDate(ds.year, ds.month, ds.day);
    }

    case SQL_TYPE_TIME:
    {
        TIME_STRUCT ts{};
        SQLRETURN ret = SQLGetData(stmt, col, SQL_C_TYPE_TIME, &ts, sizeof(ts), &indicator);
        if (ret == SQL_ERROR || indicator == SQL_NULL_DATA) {
            return QVariant(QMetaType::fromType<QTime>());
        }
        return QTime(ts.hour, ts.minute, ts.second);
    }

    case SQL_TYPE_TIMESTAMP:
    {
        TIMESTAMP_STRUCT tss{};
        SQLRETURN ret = SQLGetData(stmt, col, SQL_C_TYPE_TIMESTAMP, &tss, sizeof(tss), &indicator);
        if (ret == SQL_ERROR || indicator == SQL_NULL_DATA) {
            return QVariant(QMetaType::fromType<QDateTime>());
        }
        const QDate date(tss.year, tss.month, tss.day);
        const QTime time(
            tss.hour, tss.minute, tss.second, static_cast<int>(tss.fraction / 1000000));
        return QDateTime(date, time, Qt::UTC);
    }

    default:
        // Default: read as Unicode string
        {
            const QString str = readWCharColumn(stmt, col);
            if (str.isNull()) {
                return QVariant(QMetaType::fromType<QString>());
            }
            return str;
        }
    }
}

void AOdbcThread::fetchResults(SQLHSTMT stmt, OdbcQueryPromise &promise)
{
    SQLSMALLINT numCols = 0;
    SQLRETURN ret       = SQLNumResultCols(stmt, &numCols);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
        promise.result->m_error =
            u"Failed to get column count: "_s + odbcError(SQL_HANDLE_STMT, stmt);
        return;
    }

    // Fetch column names and types
    QVector<SQLSMALLINT> colTypes(numCols);
    for (SQLSMALLINT i = 1; i <= numCols; ++i) {
        SQLWCHAR colName[256];
        SQLSMALLINT colNameLen = 0;
        SQLSMALLINT colType    = 0;
        SQLULEN colSize        = 0;
        SQLSMALLINT decDigits  = 0;
        SQLSMALLINT nullable   = 0;

        ret = SQLDescribeColW(stmt,
                              static_cast<SQLUSMALLINT>(i),
                              colName,
                              sizeof(colName) / sizeof(SQLWCHAR),
                              &colNameLen,
                              &colType,
                              &colSize,
                              &decDigits,
                              &nullable);
        if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
            promise.result->m_error =
                u"Failed to describe column %1: "_s.arg(i) + odbcError(SQL_HANDLE_STMT, stmt);
            return;
        }

        promise.result->m_fields << QString(reinterpret_cast<const QChar *>(colName), colNameLen);
        colTypes[i - 1] = colType;
    }

    // Fetch rows
    while (true) {
        if (QThread::currentThread()->isInterruptionRequested()) {
            promise.result->m_error = u"Interrupt requested"_s;
            return;
        }

        ret = SQLFetch(stmt);
        if (ret == SQL_NO_DATA) {
            break;
        }
        if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
            promise.result->m_error = u"Failed to fetch row: "_s + odbcError(SQL_HANDLE_STMT, stmt);
            return;
        }

        for (SQLSMALLINT i = 0; i < numCols; ++i) {
            promise.result->m_rows
                << columnValue(stmt, static_cast<SQLUSMALLINT>(i + 1), colTypes[i]);
        }
    }

    // Get the number of affected rows
    SQLLEN rowCount = 0;
    SQLRowCount(stmt, &rowCount);
    if (rowCount >= 0) {
        promise.result->m_numRowsAffected = static_cast<qint64>(rowCount);
    }
}

void AOdbcThread::bindParameters(SQLHSTMT stmt,
                                 const QVariantList &params,
                                 OdbcQueryPromise &promise,
                                 QList<QByteArray> &buffers,
                                 QList<SQLLEN> &indicators)
{
    buffers.resize(params.size());
    indicators.resize(params.size());

    for (int i = 0; i < params.size(); ++i) {
        const QVariant &val   = params.at(i);
        SQLUSMALLINT paramNum = static_cast<SQLUSMALLINT>(i + 1);
        SQLRETURN ret         = SQL_SUCCESS;

        if (val.isNull()) {
            indicators[i] = SQL_NULL_DATA;
            ret           = SQLBindParameter(stmt,
                                             paramNum,
                                             SQL_PARAM_INPUT,
                                             SQL_C_DEFAULT,
                                             SQL_VARCHAR,
                                             0,
                                             0,
                                             nullptr,
                                             0,
                                             &indicators[i]);
        } else {
            switch (val.typeId()) {
            case QMetaType::Bool:
            {
                SQLCHAR bval  = val.toBool() ? 1 : 0;
                buffers[i]    = QByteArray(reinterpret_cast<const char *>(&bval), sizeof(bval));
                indicators[i] = sizeof(SQLCHAR);
                ret           = SQLBindParameter(stmt,
                                                 paramNum,
                                                 SQL_PARAM_INPUT,
                                                 SQL_C_BIT,
                                                 SQL_BIT,
                                                 1,
                                                 0,
                                                 reinterpret_cast<SQLCHAR *>(buffers[i].data()),
                                                 sizeof(SQLCHAR),
                                                 &indicators[i]);
                break;
            }
            case QMetaType::Int:
            case QMetaType::Short:
            case QMetaType::UShort:
            {
                const SQLINTEGER ival = static_cast<SQLINTEGER>(val.toInt());
                buffers[i]    = QByteArray(reinterpret_cast<const char *>(&ival), sizeof(ival));
                indicators[i] = 0;
                ret           = SQLBindParameter(stmt,
                                                 paramNum,
                                                 SQL_PARAM_INPUT,
                                                 SQL_C_SLONG,
                                                 SQL_INTEGER,
                                                 10,
                                                 0,
                                                 reinterpret_cast<SQLPOINTER>(buffers[i].data()),
                                                 sizeof(SQLINTEGER),
                                                 &indicators[i]);
                break;
            }
            case QMetaType::UInt:
            case QMetaType::LongLong:
            case QMetaType::ULongLong:
            {
                const SQLBIGINT bival = static_cast<SQLBIGINT>(val.toLongLong());
                buffers[i]    = QByteArray(reinterpret_cast<const char *>(&bival), sizeof(bival));
                indicators[i] = 0;
                ret           = SQLBindParameter(stmt,
                                                 paramNum,
                                                 SQL_PARAM_INPUT,
                                                 SQL_C_SBIGINT,
                                                 SQL_BIGINT,
                                                 19,
                                                 0,
                                                 reinterpret_cast<SQLPOINTER>(buffers[i].data()),
                                                 sizeof(SQLBIGINT),
                                                 &indicators[i]);
                break;
            }
            case QMetaType::Float:
            case QMetaType::Double:
            {
                const SQLDOUBLE dval = val.toDouble();
                buffers[i]    = QByteArray(reinterpret_cast<const char *>(&dval), sizeof(dval));
                indicators[i] = 0;
                ret           = SQLBindParameter(stmt,
                                                 paramNum,
                                                 SQL_PARAM_INPUT,
                                                 SQL_C_DOUBLE,
                                                 SQL_DOUBLE,
                                                 15,
                                                 0,
                                                 reinterpret_cast<SQLPOINTER>(buffers[i].data()),
                                                 sizeof(SQLDOUBLE),
                                                 &indicators[i]);
                break;
            }
            case QMetaType::QByteArray:
            {
                const QByteArray &ba = *static_cast<const QByteArray *>(val.constData());
                buffers[i]           = ba;
                indicators[i]        = static_cast<SQLLEN>(ba.size());
                ret = SQLBindParameter(stmt,
                                       paramNum,
                                       SQL_PARAM_INPUT,
                                       SQL_C_BINARY,
                                       SQL_VARBINARY,
                                       static_cast<SQLULEN>(ba.size()),
                                       0,
                                       reinterpret_cast<SQLPOINTER>(buffers[i].data()),
                                       static_cast<SQLLEN>(ba.size()),
                                       &indicators[i]);
                break;
            }
            case QMetaType::QDate:
            {
                const QDate date = val.toDate();
                DATE_STRUCT ds{};
                ds.year       = static_cast<SQLSMALLINT>(date.year());
                ds.month      = static_cast<SQLUSMALLINT>(date.month());
                ds.day        = static_cast<SQLUSMALLINT>(date.day());
                buffers[i]    = QByteArray(reinterpret_cast<const char *>(&ds), sizeof(ds));
                indicators[i] = sizeof(DATE_STRUCT);
                ret           = SQLBindParameter(stmt,
                                                 paramNum,
                                                 SQL_PARAM_INPUT,
                                                 SQL_C_TYPE_DATE,
                                                 SQL_TYPE_DATE,
                                                 10,
                                                 0,
                                                 reinterpret_cast<SQLPOINTER>(buffers[i].data()),
                                                 sizeof(DATE_STRUCT),
                                                 &indicators[i]);
                break;
            }
            case QMetaType::QTime:
            {
                const QTime time = val.toTime();
                TIME_STRUCT ts{};
                ts.hour       = static_cast<SQLUSMALLINT>(time.hour());
                ts.minute     = static_cast<SQLUSMALLINT>(time.minute());
                ts.second     = static_cast<SQLUSMALLINT>(time.second());
                buffers[i]    = QByteArray(reinterpret_cast<const char *>(&ts), sizeof(ts));
                indicators[i] = sizeof(TIME_STRUCT);
                ret           = SQLBindParameter(stmt,
                                                 paramNum,
                                                 SQL_PARAM_INPUT,
                                                 SQL_C_TYPE_TIME,
                                                 SQL_TYPE_TIME,
                                                 8,
                                                 0,
                                                 reinterpret_cast<SQLPOINTER>(buffers[i].data()),
                                                 sizeof(TIME_STRUCT),
                                                 &indicators[i]);
                break;
            }
            case QMetaType::QDateTime:
            {
                const QDateTime dt = val.toDateTime();
                TIMESTAMP_STRUCT tss{};
                tss.year      = static_cast<SQLSMALLINT>(dt.date().year());
                tss.month     = static_cast<SQLUSMALLINT>(dt.date().month());
                tss.day       = static_cast<SQLUSMALLINT>(dt.date().day());
                tss.hour      = static_cast<SQLUSMALLINT>(dt.time().hour());
                tss.minute    = static_cast<SQLUSMALLINT>(dt.time().minute());
                tss.second    = static_cast<SQLUSMALLINT>(dt.time().second());
                tss.fraction  = static_cast<SQLUINTEGER>(dt.time().msec()) * 1000000u;
                buffers[i]    = QByteArray(reinterpret_cast<const char *>(&tss), sizeof(tss));
                indicators[i] = sizeof(TIMESTAMP_STRUCT);
                ret           = SQLBindParameter(stmt,
                                                 paramNum,
                                                 SQL_PARAM_INPUT,
                                                 SQL_C_TYPE_TIMESTAMP,
                                                 SQL_TYPE_TIMESTAMP,
                                                 23,
                                                 3,
                                                 reinterpret_cast<SQLPOINTER>(buffers[i].data()),
                                                 sizeof(TIMESTAMP_STRUCT),
                                                 &indicators[i]);
                break;
            }
            default:
            {
                // Fallback: convert to string
                const QString str = val.toString();
                // Store as UTF-16 in buffer; QByteArray stores bytes
                const qsizetype byteLen = str.size() * static_cast<qsizetype>(sizeof(QChar));
                buffers[i]    = QByteArray(reinterpret_cast<const char *>(str.unicode()), byteLen);
                indicators[i] = SQL_NTS;
                ret           = SQLBindParameter(
                    stmt,
                    paramNum,
                    SQL_PARAM_INPUT,
                    SQL_C_WCHAR,
                    SQL_WVARCHAR,
                    static_cast<SQLULEN>(str.size()),
                    0,
                    reinterpret_cast<SQLPOINTER>(buffers[i].data()),
                    static_cast<SQLLEN>(buffers[i].size() + sizeof(SQLWCHAR)), // + null terminator
                    &indicators[i]);
                break;
            }
            }
        }

        if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
            promise.result->m_error =
                u"Failed to bind parameter %1: "_s.arg(i + 1) + odbcError(SQL_HANDLE_STMT, stmt);
            return;
        }
    }
}

void AOdbcThread::query(OdbcQueryPromise promise)
{
    auto _ = qScopeGuard([&] {
        {
            QMutexLocker _(&m_promisesMutex);
            m_promisesReady.enqueue(std::move(promise));
        }
        Q_EMIT queryReady();
    });

    SQLHSTMT stmt = SQL_NULL_HSTMT;
    SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, m_dbc, &stmt);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
        promise.result->m_error =
            u"Failed to allocate statement handle: "_s + odbcError(SQL_HANDLE_DBC, m_dbc);
        return;
    }

    auto stmtGuard = qScopeGuard([&] { SQLFreeHandle(SQL_HANDLE_STMT, stmt); });

    // Prepare the statement
    const QString queryStr = QString::fromUtf8(promise.result->m_query);
    ret                    = SQLPrepareW(
        stmt, reinterpret_cast<SQLWCHAR *>(const_cast<QChar *>(queryStr.unicode())), SQL_NTS);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
        promise.result->m_error =
            u"Failed to prepare statement: "_s + odbcError(SQL_HANDLE_STMT, stmt);
        return;
    }

    // Bind parameters
    QList<QByteArray> buffers;
    QList<SQLLEN> indicators;
    bindParameters(stmt, promise.result->m_queryArgs, promise, buffers, indicators);
    if (promise.result->m_error.has_value()) {
        return;
    }

    // Execute
    ret = SQLExecute(stmt);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
        promise.result->m_error =
            u"Failed to execute statement: "_s + odbcError(SQL_HANDLE_STMT, stmt);
        return;
    }

    fetchResults(stmt, promise);
}

void AOdbcThread::queryPrepared(OdbcQueryPromise promise)
{
    auto _ = qScopeGuard([&] {
        {
            QMutexLocker _(&m_promisesMutex);
            m_promisesReady.enqueue(std::move(promise));
        }
        Q_EMIT queryReady();
    });

    const int queryId = promise.preparedQuery->identification();

    // Look up or create the prepared statement handle
    SQLHSTMT stmt = SQL_NULL_HSTMT;
    auto it       = m_preparedStmts.constFind(queryId);
    if (it != m_preparedStmts.constEnd()) {
        stmt = it.value();
        // Reset for reuse
        SQLRETURN resetRet = SQLFreeStmt(stmt, SQL_RESET_PARAMS);
        if (resetRet != SQL_SUCCESS && resetRet != SQL_SUCCESS_WITH_INFO) {
            qWarning(ASQL_ODBC) << u"Failed to reset prepared statement, re-preparing"_s;
            SQLFreeHandle(SQL_HANDLE_STMT, stmt);
            m_preparedStmts.remove(queryId);
            stmt = SQL_NULL_HSTMT;
        }
    }

    if (stmt == SQL_NULL_HSTMT) {
        SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, m_dbc, &stmt);
        if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
            promise.result->m_error =
                u"Failed to allocate statement handle: "_s + odbcError(SQL_HANDLE_DBC, m_dbc);
            return;
        }

        const QString queryStr = QString::fromUtf8(promise.preparedQuery->query());
        ret                    = SQLPrepareW(
            stmt, reinterpret_cast<SQLWCHAR *>(const_cast<QChar *>(queryStr.unicode())), SQL_NTS);
        if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
            promise.result->m_error =
                u"Failed to prepare statement: "_s + odbcError(SQL_HANDLE_STMT, stmt);
            SQLFreeHandle(SQL_HANDLE_STMT, stmt);
            return;
        }
        m_preparedStmts.insert(queryId, stmt);
    }

    // Bind parameters
    QList<QByteArray> buffers;
    QList<SQLLEN> indicators;
    bindParameters(stmt, promise.result->m_queryArgs, promise, buffers, indicators);
    if (promise.result->m_error.has_value()) {
        return;
    }

    // Execute
    SQLRETURN ret = SQLExecute(stmt);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
        promise.result->m_error =
            u"Failed to execute prepared statement: "_s + odbcError(SQL_HANDLE_STMT, stmt);
        // Close the cursor so the statement can be reused
        SQLCloseCursor(stmt);
        return;
    }

    fetchResults(stmt, promise);

    // Close cursor so the handle can be reused
    SQLCloseCursor(stmt);
}

void AOdbcThread::queryExec(OdbcQueryPromise promise)
{
    auto _ = qScopeGuard([&] {
        {
            QMutexLocker _(&m_promisesMutex);
            m_promisesReady.enqueue(std::move(promise));
        }
        Q_EMIT queryReady();
    });

    SQLHSTMT stmt = SQL_NULL_HSTMT;
    SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, m_dbc, &stmt);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
        promise.result->m_error =
            u"Failed to allocate statement handle: "_s + odbcError(SQL_HANDLE_DBC, m_dbc);
        return;
    }

    auto stmtGuard = qScopeGuard([&] { SQLFreeHandle(SQL_HANDLE_STMT, stmt); });

    const QString queryStr = QString::fromUtf8(promise.result->m_query);
    ret                    = SQLExecDirectW(
        stmt, reinterpret_cast<SQLWCHAR *>(const_cast<QChar *>(queryStr.unicode())), SQL_NTS);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
        promise.result->m_error =
            u"Failed to execute statement: "_s + odbcError(SQL_HANDLE_STMT, stmt);
        return;
    }

    fetchResults(stmt, promise);
}

// ─────────────────────────────── ADriverOdbc ──────────────────────────────────

ADriverOdbc::ADriverOdbc(const QString &connInfo)
    : ADriver{connInfo}
    , m_worker{connInfo}
{
    m_thread.setObjectName(connInfo);
    m_worker.moveToThread(&m_thread);

    connect(&m_worker, &AOdbcThread::queryReady, this, [this] {
        QMutexLocker _(&m_worker.m_promisesMutex);
        while (!m_worker.m_promisesReady.isEmpty()) {
            OdbcQueryPromise promise = m_worker.m_promisesReady.dequeue();
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

ADriverOdbc::~ADriverOdbc()
{
    Q_ASSERT(m_thread.thread() == QThread::currentThread());

    m_thread.requestInterruption();
    m_thread.quit();
    m_thread.wait();
}

QString ADriverOdbc::driverName() const
{
    return u"odbc"_s;
}

bool ADriverOdbc::isValid() const
{
    return true;
}

void ADriverOdbc::open(const std::shared_ptr<ADriver> &driver, QObject *receiver, AOpenFn cb)
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

    OdbcOpenPromise promise{
        .cb = cb,
    };

    if (receiver) {
        promise.receiver = receiver;
    }

    connect(&m_worker, &AOdbcThread::openned, this, [this, promise](bool isOpen, QString error) {
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
    QMetaObject::invokeMethod(&m_worker, &AOdbcThread::open, Qt::QueuedConnection);
#else
    QMetaObject::invokeMethod(&m_worker, "open", Qt::QueuedConnection);
#endif
}

bool ADriverOdbc::isOpen() const
{
    return m_state == ADatabase::State::Connected;
}

void ADriverOdbc::setState(ADatabase::State state, const QString &status)
{
    m_state = state;
    if (m_stateChangedCb &&
        (!m_stateChangedReceiver.has_value() || !m_stateChangedReceiver->isNull())) {
        m_stateChangedCb(state, status);
    }
}

ADatabase::State ADriverOdbc::state() const
{
    return m_state;
}

void ADriverOdbc::onStateChanged(QObject *receiver,
                                 std::function<void(ADatabase::State, const QString &)> cb)
{
    m_stateChangedCb = cb;
    if (receiver) {
        m_stateChangedReceiver = receiver;
    }
}

void ADriverOdbc::begin(const std::shared_ptr<ADriver> &db, QObject *receiver, ACoroDataRef cb)
{
    exec(db, u8"BEGIN", receiver, std::move(cb));
}

void ADriverOdbc::commit(const std::shared_ptr<ADriver> &db, QObject *receiver, ACoroDataRef cb)
{
    exec(db, u8"COMMIT", receiver, std::move(cb));
}

void ADriverOdbc::rollback(const std::shared_ptr<ADriver> &db, QObject *receiver, ACoroDataRef cb)
{
    exec(db, u8"ROLLBACK", receiver, std::move(cb));
}

void ADriverOdbc::exec(const std::shared_ptr<ADriver> &db,
                       QUtf8StringView query,
                       QObject *receiver,
                       ACoroDataRef cb)
{
    ++m_queueSize;
    selfDriver = db;

    OdbcQueryPromise data{
        .cb     = std::move(cb),
        .result = std::make_shared<AResultOdbc>(),
    };
    if (receiver) {
        data.receiver = receiver;
    }
    data.result->m_query.setRawData(query.data(), query.size());

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    QMetaObject::invokeMethod(
        &m_worker, &AOdbcThread::queryExec, Qt::QueuedConnection, std::move(data));
#else
    QMetaObject::invokeMethod(&m_worker,
                              "queryExec",
                              Qt::QueuedConnection,
                              Q_ARG(ASql::OdbcQueryPromise, std::move(data)));
#endif
}

void ADriverOdbc::exec(const std::shared_ptr<ADriver> &db,
                       QStringView query,
                       QObject *receiver,
                       ACoroDataRef cb)
{
    ++m_queueSize;
    selfDriver = db;

    OdbcQueryPromise data{
        .cb     = std::move(cb),
        .result = std::make_shared<AResultOdbc>(),
    };
    if (receiver) {
        data.receiver = receiver;
    }
    data.result->m_query = query.toUtf8();

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    QMetaObject::invokeMethod(
        &m_worker, &AOdbcThread::queryExec, Qt::QueuedConnection, std::move(data));
#else
    QMetaObject::invokeMethod(&m_worker,
                              "queryExec",
                              Qt::QueuedConnection,
                              Q_ARG(ASql::OdbcQueryPromise, std::move(data)));
#endif
}

void ADriverOdbc::exec(const std::shared_ptr<ADriver> &db,
                       QUtf8StringView query,
                       const QVariantList &params,
                       QObject *receiver,
                       ACoroDataRef cb)
{
    ++m_queueSize;
    selfDriver = db;

    OdbcQueryPromise data{
        .cb     = std::move(cb),
        .result = std::make_shared<AResultOdbc>(),
    };
    if (receiver) {
        data.receiver = receiver;
    }
    data.result->m_query.setRawData(query.data(), query.size());
    data.result->m_queryArgs = params;

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    QMetaObject::invokeMethod(
        &m_worker, &AOdbcThread::query, Qt::QueuedConnection, std::move(data));
#else
    QMetaObject::invokeMethod(
        &m_worker, "query", Qt::QueuedConnection, Q_ARG(ASql::OdbcQueryPromise, std::move(data)));
#endif
}

void ADriverOdbc::exec(const std::shared_ptr<ADriver> &db,
                       QStringView query,
                       const QVariantList &params,
                       QObject *receiver,
                       ACoroDataRef cb)
{
    ++m_queueSize;
    selfDriver = db;

    OdbcQueryPromise data{
        .cb     = std::move(cb),
        .result = std::make_shared<AResultOdbc>(),
    };
    if (receiver) {
        data.receiver = receiver;
    }
    data.result->m_query     = query.toUtf8();
    data.result->m_queryArgs = params;

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    QMetaObject::invokeMethod(
        &m_worker, &AOdbcThread::query, Qt::QueuedConnection, std::move(data));
#else
    QMetaObject::invokeMethod(
        &m_worker, "query", Qt::QueuedConnection, Q_ARG(ASql::OdbcQueryPromise, std::move(data)));
#endif
}

void ADriverOdbc::exec(const std::shared_ptr<ADriver> &db,
                       const APreparedQuery &query,
                       const QVariantList &params,
                       QObject *receiver,
                       ACoroDataRef cb)
{
    ++m_queueSize;
    selfDriver = db;

    OdbcQueryPromise data{
        .preparedQuery = query,
        .cb            = std::move(cb),
        .result        = std::make_shared<AResultOdbc>(),
    };
    if (receiver) {
        data.receiver = receiver;
    }
    data.result->m_query     = query.query();
    data.result->m_queryArgs = params;

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    QMetaObject::invokeMethod(
        &m_worker, &AOdbcThread::queryPrepared, Qt::QueuedConnection, std::move(data));
#else
    QMetaObject::invokeMethod(&m_worker,
                              "queryPrepared",
                              Qt::QueuedConnection,
                              Q_ARG(ASql::OdbcQueryPromise, std::move(data)));
#endif
}

void ADriverOdbc::setLastQuerySingleRowMode()
{
}

bool ADriverOdbc::enterPipelineMode(std::chrono::milliseconds)
{
    return false;
}

bool ADriverOdbc::exitPipelineMode()
{
    return false;
}

ADatabase::PipelineStatus ADriverOdbc::pipelineStatus() const
{
    return ADatabase::PipelineStatus::Off;
}

bool ADriverOdbc::pipelineSync()
{
    return false;
}

int ADriverOdbc::queueSize() const
{
    return m_queueSize;
}

void ADriverOdbc::subscribeToNotification(const std::shared_ptr<ADriver> &,
                                          const QString &,
                                          QObject *,
                                          ANotificationFn)
{
}

QStringList ADriverOdbc::subscribedToNotifications() const
{
    return {};
}

void ADriverOdbc::unsubscribeFromNotification(const std::shared_ptr<ADriver> &, const QString &)
{
}

// ─────────────────────────────── AResultOdbc ──────────────────────────────────

bool AResultOdbc::lastResultSet() const
{
    return m_lastResultSet;
}

bool AResultOdbc::hasError() const
{
    return m_error.has_value();
}

QString AResultOdbc::errorString() const
{
    return m_error.value();
}

QByteArray AResultOdbc::query() const
{
    return m_query;
}

QVariantList AResultOdbc::queryArgs() const
{
    return m_queryArgs;
}

int AResultOdbc::size() const
{
    return m_fields.empty() ? 0 : m_rows.size() / m_fields.size();
}

int AResultOdbc::fields() const
{
    return m_fields.size();
}

qint64 AResultOdbc::numRowsAffected() const
{
    return m_numRowsAffected;
}

int AResultOdbc::indexOfField(QLatin1String name) const
{
    return m_fields.indexOf(name);
}

QString AResultOdbc::fieldName(int column) const
{
    return m_fields.at(column);
}

QVariant AResultOdbc::value(int row, int column) const
{
    return m_rows.at(row * m_fields.size() + column);
}

bool AResultOdbc::isNull(int row, int column) const
{
    return value(row, column).isNull();
}

bool AResultOdbc::toBool(int row, int column) const
{
    return value(row, column).toBool();
}

int AResultOdbc::toInt(int row, int column) const
{
    return value(row, column).toInt();
}

qint64 AResultOdbc::toLongLong(int row, int column) const
{
    return value(row, column).toLongLong();
}

quint64 AResultOdbc::toULongLong(int row, int column) const
{
    return value(row, column).toULongLong();
}

double AResultOdbc::toDouble(int row, int column) const
{
    return value(row, column).toDouble();
}

QString AResultOdbc::toString(int row, int column) const
{
    return value(row, column).toString();
}

std::string AResultOdbc::toStdString(int row, int column) const
{
    return toString(row, column).toStdString();
}

QUuid AResultOdbc::toUuid(int row, int column) const
{
    return QUuid::fromString(value(row, column).toString());
}

QDate AResultOdbc::toDate(int row, int column) const
{
    return value(row, column).toDate();
}

QTime AResultOdbc::toTime(int row, int column) const
{
    return value(row, column).toTime();
}

QDateTime AResultOdbc::toDateTime(int row, int column) const
{
    return value(row, column).toDateTime();
}

QJsonValue AResultOdbc::toJsonValue(int row, int column) const
{
    const auto doc = QJsonDocument::fromJson(toString(row, column).toUtf8());
    return doc.isObject() ? doc.object() : doc.isArray() ? doc.array() : QJsonValue{};
}

QCborValue AResultOdbc::toCborValue(int row, int column) const
{
    return QCborValue::fromCbor(toByteArray(row, column));
}

QByteArray AResultOdbc::toByteArray(int row, int column) const
{
    return value(row, column).toByteArray();
}

} // namespace ASql

#include "moc_ADriverOdbc.cpp"
