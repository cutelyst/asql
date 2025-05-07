#include "ADriverSqlite.hpp"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QMetaMethod>
#include <QUrl>
#include <QUrlQuery>

Q_LOGGING_CATEGORY(ASQL_SQLITE, "asql.sqlite", QtInfoMsg)

using namespace Qt::StringLiterals;

namespace ASql {

ADriverSqlite::ADriverSqlite(const QString &connInfo)
    : ADriver{connInfo}
    , m_worker{connInfo}
{
    m_worker.moveToThread(&m_thread);

    connect(&m_worker, &ASqliteThread::openned, this, [this](OpenPromise promisse) {
        if (promisse.isOpen) {
            setState(ADatabase::State::Connected, {});
        } else {
            setState(ADatabase::State::Disconnected, promisse.error);
        }

        if (!promisse.checkReceiver || !promisse.receiver.isNull()) {
            if (promisse.cb) {
                promisse.cb(promisse.isOpen, promisse.error);
            }
        }
    });

    connect(&m_worker, &ASqliteThread::queryFinished, this, [this](QueryPromise promisse) {
        if (--m_queueSize == 0) {
            // This might not be needed if we only use coroutines
            // since db object won't go out of scope when we are waiting for a reply
            // unless ofc the user forget to co_await, in which case we
            // should try to do some cleanup or prevent it if possible.
            selfDriver.reset();
        }

        if (!promisse.checkReceiver || !promisse.receiver.isNull()) {
            if (promisse.cb) {
                AResult result{promisse.result};
                promisse.cb(result);
            }
        }
    });

    m_thread.start();
}

ADriverSqlite::~ADriverSqlite()
{
    m_thread.requestInterruption();
    m_thread.wait();
    m_thread.quit();
}

QString ADriverSqlite::driverName() const
{
    return u"sqlite"_s;
}

bool ADriverSqlite::isValid() const
{
    return true;
}

void ADriverSqlite::open(const std::shared_ptr<ADriver> &driver,
                         QObject *receiver,
                         std::function<void(bool, const QString &)> cb)
{
    setState(ADatabase::State::Connecting, {});

    OpenPromise data{
        .driver        = driver,
        .cb            = cb,
        .receiver      = receiver,
        .checkReceiver = static_cast<bool>(receiver),
    };

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    QMetaObject::invokeMethod(
        &m_worker, &ASqliteThread::open, Qt::QueuedConnection, std::move(data));
#else
    QMetaObject::invokeMethod(
        &m_worker, "open", Qt::QueuedConnection, Q_ARG(ASql::OpenPromise, std::move(data)));
#endif
}

bool ADriverSqlite::isOpen() const
{
    return m_state == ADatabase::State::Connected;
}

void ADriverSqlite::setState(ADatabase::State state, const QString &status)
{
    m_state = state;
    if (m_stateChangedCb && (!m_stateChangedReceiverSet || !m_stateChangedReceiver.isNull())) {
        m_stateChangedCb(state, status);
    }
}

ADatabase::State ADriverSqlite::state() const
{
    return m_state;
}

void ADriverSqlite::onStateChanged(QObject *receiver,
                                   std::function<void(ADatabase::State, const QString &)> cb)
{
    m_stateChangedCb          = cb;
    m_stateChangedReceiver    = receiver;
    m_stateChangedReceiverSet = receiver;
}

void ADriverSqlite::begin(const std::shared_ptr<ADriver> &db, QObject *receiver, AResultFn cb)
{
    exec(db, u8"BEGIN", receiver, cb);
}

void ADriverSqlite::commit(const std::shared_ptr<ADriver> &db, QObject *receiver, AResultFn cb)
{
    exec(db, u8"COMMIT", receiver, cb);
}

void ADriverSqlite::rollback(const std::shared_ptr<ADriver> &db, QObject *receiver, AResultFn cb)
{
    exec(db, u8"ROLLBACK", receiver, cb);
}

void ADriverSqlite::exec(const std::shared_ptr<ADriver> &db,
                         QUtf8StringView query,
                         QObject *receiver,
                         AResultFn cb)
{
    ++m_queueSize;
    selfDriver = db;

    QueryPromise data{
        .driver        = db,
        .cb            = cb,
        .result        = std::make_shared<AResultSqlite>(),
        .receiver      = receiver,
        .checkReceiver = static_cast<bool>(receiver),
    };
    data.result->m_query.setRawData(query.data(), query.size());

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    QMetaObject::invokeMethod(
        &m_worker, &ASqliteThread::queryExec, Qt::QueuedConnection, std::move(data));
#else
    QMetaObject::invokeMethod(
        &m_worker, "queryExec", Qt::QueuedConnection, Q_ARG(ASql::QueryPromise, std::move(data)));
#endif
}

void ADriverSqlite::exec(const std::shared_ptr<ADriver> &db,
                         QStringView query,
                         QObject *receiver,
                         AResultFn cb)
{
    ++m_queueSize;

    QueryPromise data{
        .driver        = db,
        .cb            = cb,
        .result        = std::make_shared<AResultSqlite>(),
        .receiver      = receiver,
        .checkReceiver = static_cast<bool>(receiver),
    };
    data.result->m_query = query.toUtf8();

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    QMetaObject::invokeMethod(
        &m_worker, &ASqliteThread::queryExec, Qt::QueuedConnection, std::move(data));
#else
    QMetaObject::invokeMethod(
        &m_worker, "queryExec", Qt::QueuedConnection, Q_ARG(ASql::QueryPromise, std::move(data)));
#endif
}

void ADriverSqlite::exec(const std::shared_ptr<ADriver> &db,
                         QUtf8StringView query,
                         const QVariantList &params,
                         QObject *receiver,
                         AResultFn cb)
{
    ++m_queueSize;
    selfDriver = db;

    QueryPromise data{
        .driver        = db,
        .cb            = cb,
        .result        = std::make_shared<AResultSqlite>(),
        .receiver      = receiver,
        .checkReceiver = static_cast<bool>(receiver),
    };
    data.result->m_query.setRawData(query.data(), query.size());
    data.result->m_queryArgs = params;

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    QMetaObject::invokeMethod(
        &m_worker, &ASqliteThread::query, Qt::QueuedConnection, std::move(data));
#else
    QMetaObject::invokeMethod(
        &m_worker, "query", Qt::QueuedConnection, Q_ARG(ASql::QueryPromise, std::move(data)));
#endif
}

void ADriverSqlite::exec(const std::shared_ptr<ADriver> &db,
                         QStringView query,
                         const QVariantList &params,
                         QObject *receiver,
                         AResultFn cb)
{
    ++m_queueSize;

    QueryPromise data{
        .driver        = db,
        .cb            = cb,
        .result        = std::make_shared<AResultSqlite>(),
        .receiver      = receiver,
        .checkReceiver = static_cast<bool>(receiver),
    };
    data.result->m_query     = query.toUtf8();
    data.result->m_queryArgs = params;

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    QMetaObject::invokeMethod(
        &m_worker, &ASqliteThread::query, Qt::QueuedConnection, std::move(data));
#else
    QMetaObject::invokeMethod(
        &m_worker, "query", Qt::QueuedConnection, Q_ARG(ASql::QueryPromise, std::move(data)));
#endif
}

void ADriverSqlite::exec(const std::shared_ptr<ADriver> &db,
                         const APreparedQuery &query,
                         const QVariantList &params,
                         QObject *receiver,
                         AResultFn cb)
{
    ++m_queueSize;

    QueryPromise data{
        .driver        = db,
        .preparedQuery = query,
        .cb            = cb,
        .result        = std::make_shared<AResultSqlite>(),
        .receiver      = receiver,
        .checkReceiver = static_cast<bool>(receiver),
    };
    data.result->m_query     = query.query();
    data.result->m_queryArgs = params;

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    QMetaObject::invokeMethod(
        &m_worker, &ASqliteThread::queryPrepared, Qt::QueuedConnection, std::move(data));
#else
    QMetaObject::invokeMethod(&m_worker,
                              "queryPrepared",
                              Qt::QueuedConnection,
                              Q_ARG(ASql::QueryPromise, std::move(data)));
#endif
}

void ADriverSqlite::setLastQuerySingleRowMode()
{
}

bool ADriverSqlite::enterPipelineMode(std::chrono::milliseconds timeout)
{
    return false;
}

bool ADriverSqlite::exitPipelineMode()
{
    return false;
}

ADatabase::PipelineStatus ADriverSqlite::pipelineStatus() const
{
    return ADatabase::PipelineStatus::Off;
}

bool ADriverSqlite::pipelineSync()
{
    return false;
}

int ADriverSqlite::queueSize() const
{
    return m_queueSize;
}

void ADriverSqlite::subscribeToNotification(const std::shared_ptr<ADriver> &db,
                                            const QString &name,
                                            QObject *receiver,
                                            ANotificationFn cb)
{
}

QStringList ADriverSqlite::subscribedToNotifications() const
{
    return {};
}

void ADriverSqlite::unsubscribeFromNotification(const std::shared_ptr<ADriver> &db,
                                                const QString &name)
{
}

ASqliteThread::ASqliteThread(const QString &connInfo)
    : m_uri(connInfo)
{
}

ASqliteThread::~ASqliteThread()
{
    sqlite3_close_v2(m_db);
}

void ASqliteThread::open(OpenPromise promise)
{
    QUrl uri{m_uri};
    uri.setScheme(u"file"_s);

    const QUrlQuery query{uri.query()};

    const bool openReadOnlyOption = query.hasQueryItem(u"READONLY"_s);
    const bool sharedCache        = query.hasQueryItem(u"SHAREDCACHE"_s);
    const bool openUriOption      = true; // query.hasQueryItem(u"URI"_s);
    const bool memoryOption       = query.hasQueryItem(u"MEMORY"_s);

    int openMode =
        (openReadOnlyOption ? SQLITE_OPEN_READONLY : (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE));
    openMode |= (sharedCache ? SQLITE_OPEN_SHAREDCACHE : SQLITE_OPEN_PRIVATECACHE);
    openMode |= SQLITE_OPEN_NOMUTEX;

    const QString filename = [&]() {
        if (memoryOption) {
            return u":memory:"_s;
        } else if (openUriOption) {
            openMode |= SQLITE_OPEN_URI;
            return uri.toString(QUrl::FullyEncoded);
        } else {
            return uri.toLocalFile();
        }
    }();

    const int res = sqlite3_open_v2(filename.toUtf8().constData(), &m_db, openMode, NULL);
    if (res == SQLITE_OK) {
        promise.isOpen = true;
    } else {
        const char *sqliteError = sqlite3_errmsg(m_db);
        promise.error           = u"Failed to open database: %1"_s.arg(
            sqliteError ? QString::fromUtf8(sqliteError) : u"Unknown error"_s);
        sqlite3_close_v2(m_db);
        m_db = nullptr;
    }

    Q_EMIT openned(promise);
}

QStringList fillColumns(sqlite3_stmt *stmt)
{
    QStringList columns;
    const int columnsCount = sqlite3_column_count(stmt);
    for (int i = 0; i < columnsCount; ++i) {
        columns << QString::fromUtf8(sqlite3_column_name(stmt, i));
    }
    return columns;
}

void fillRow(sqlite3_stmt *stmt, int columnsCount, QVariantList &rows)
{
    const int currentRow = rows.size();
    rows.resize(rows.size() + columnsCount);

    for (int i = 0; i < columnsCount; i++) {
        switch (sqlite3_column_type(stmt, i)) {
        case SQLITE_BLOB:
            rows[currentRow + i] =
                QByteArray(static_cast<const char *>(sqlite3_column_blob(stmt, i)),
                           sqlite3_column_bytes(stmt, i));
            break;
        case SQLITE_INTEGER:
            rows[currentRow + i] = sqlite3_column_int64(stmt, i);
            break;
        case SQLITE_FLOAT:
            rows[currentRow + i] = sqlite3_column_double(stmt, i);
            break;
            break;
        case SQLITE_NULL:
            rows[currentRow + i] = QVariant(QMetaType::fromType<QString>());
            break;
        default:
            rows[currentRow + i] =
                QString(reinterpret_cast<const QChar *>(sqlite3_column_text16(stmt, i)),
                        sqlite3_column_bytes16(stmt, i) / sizeof(QChar));
            break;
        }
    }
}

std::optional<QString> bindValues(sqlite3 *db, sqlite3_stmt *stmt, const QVariantList &params)
{
    int res = SQLITE_OK;
    for (int i = 0; i < params.size(); ++i) {
        const QVariant &value = params.at(i);
        if (value.isNull()) {
            res = sqlite3_bind_null(stmt, i + 1);
        } else {
            switch (value.userType()) {
            case QMetaType::QByteArray:
            {
                const QByteArray *ba = static_cast<const QByteArray *>(value.constData());
                res = sqlite3_bind_blob(stmt, i + 1, ba->constData(), ba->size(), SQLITE_STATIC);
                break;
            }
            case QMetaType::Int:
            case QMetaType::Bool:
                res = sqlite3_bind_int(stmt, i + 1, value.toInt());
                break;
            case QMetaType::Double:
                res = sqlite3_bind_double(stmt, i + 1, value.toDouble());
                break;
            case QMetaType::UInt:
            case QMetaType::LongLong:
                res = sqlite3_bind_int64(stmt, i + 1, value.toLongLong());
                break;
            case QMetaType::QDateTime:
            {
                const QDateTime dateTime = value.toDateTime();
                const QString str        = dateTime.toString(Qt::ISODateWithMs);
                res                      = sqlite3_bind_text16(
                    stmt, i + 1, str.data(), int(str.size() * sizeof(ushort)), SQLITE_TRANSIENT);
                break;
            }
            case QMetaType::QTime:
            {
                const QTime time  = value.toTime();
                const QString str = time.toString(u"hh:mm:ss.zzz");
                res               = sqlite3_bind_text16(
                    stmt, i + 1, str.data(), int(str.size() * sizeof(ushort)), SQLITE_TRANSIENT);
                break;
            }
            case QMetaType::QString:
            {
                // lifetime of string == lifetime of its qvariant
                const QString *str = static_cast<const QString *>(value.constData());
                res                = sqlite3_bind_text16(
                    stmt, i + 1, str->unicode(), int(str->size()) * sizeof(QChar), SQLITE_STATIC);
                break;
            }
            default:
            {
                const QString str = value.toString();
                // SQLITE_TRANSIENT makes sure that sqlite buffers the data
                res = sqlite3_bind_text16(
                    stmt, i + 1, str.data(), int(str.size()) * sizeof(QChar), SQLITE_TRANSIENT);
                break;
            }
            }
        }

        if (res != SQLITE_OK) {
            const char *sqliteError = sqlite3_errmsg(db);
            return u"Failed to bind parameter %1 '%2': '%3'"_s.arg(QString::number(i))
                .arg(value.toString())
                .arg(value.toString())
                .arg(sqliteError ? QString::fromUtf8(sqliteError) : u"Unknown error"_s);
        }
    }
    return {};
}

void ASqliteThread::query(QueryPromise promise)
{
    auto _ = qScopeGuard([&] { Q_EMIT queryFinished(std::move(promise)); });

    auto stmt = prepare(promise, 0x0);
    if (!stmt) {
        return;
    }

    auto bindError = bindValues(m_db, stmt.get(), promise.result->m_queryArgs);
    if (bindError.has_value()) {
        promise.result->m_error = bindError;
        return;
    }

    promise.result->m_fields = fillColumns(stmt.get());

    QVariantList rows;
    do {
        if (QThread::currentThread()->isInterruptionRequested()) {
            promise.result->m_error = u"Interrupt requested"_s;
            return;
        }

        int res = sqlite3_step(stmt.get());
        if (res != SQLITE_ROW) {
            if (res != SQLITE_DONE) {
                const char *sqliteError = sqlite3_errmsg(m_db);
                promise.result->m_error = u"Failed to execute query: '%2'"_s.arg(
                    sqliteError ? QString::fromUtf8(sqliteError) : u"Unknown error"_s);
                return;
            }
            break;
        }

        fillRow(stmt.get(), promise.result->m_fields.size(), rows);
    } while (true);

    promise.result->m_numRowsAffected = sqlite3_changes64(m_db);
    promise.result->m_rows            = rows;
}

void ASqliteThread::queryPrepared(QueryPromise promise)
{
    std::shared_ptr<sqlite3_stmt> stmt;

    const auto queryId = promise.preparedQuery->identification();
    auto _             = qScopeGuard([&] {
        // Make the statement ready to be used later
        if (sqlite3_reset(stmt.get()) != SQLITE_OK) {
            m_preparedQueries.remove(queryId);

            const char *sqliteError = sqlite3_errmsg(m_db);
            qWarning(ASQL_SQLITE) << u"Failed to reset prepared statement: '%1'"_s.arg(
                sqliteError ? QString::fromUtf8(sqliteError) : u"Unknown error"_s);
            return;
        }

        if (sqlite3_clear_bindings(stmt.get()) != SQLITE_OK) {
            m_preparedQueries.remove(queryId);

            const char *sqliteError = sqlite3_errmsg(m_db);
            qWarning(ASQL_SQLITE) << u"Failed to reset prepared statement bindings: '%1'"_s.arg(
                sqliteError ? QString::fromUtf8(sqliteError) : u"Unknown error"_s);
            return;
        }
        Q_EMIT queryFinished(std::move(promise));
    });

    auto it = m_preparedQueries.constFind(queryId);
    if (it != m_preparedQueries.constEnd()) {
        stmt = it.value();
    } else {
        stmt = prepare(promise, SQLITE_PREPARE_PERSISTENT);
        if (stmt) {
            m_preparedQueries.insert(queryId, stmt);
        } else {
            return;
        }
    }

    auto bindError = bindValues(m_db, stmt.get(), promise.result->m_queryArgs);
    if (bindError.has_value()) {
        promise.result->m_error = bindError;
        return;
    }

    promise.result->m_fields = fillColumns(stmt.get());

    QVariantList rows;
    do {
        if (QThread::currentThread()->isInterruptionRequested()) {
            promise.result->m_error = u"Interrupt requested"_s;
            return;
        }

        int res = sqlite3_step(stmt.get());
        if (res != SQLITE_ROW) {
            if (res != SQLITE_DONE) {
                const char *sqliteError = sqlite3_errmsg(m_db);
                promise.result->m_error = u"Failed to execute query: '%2'"_s.arg(
                    sqliteError ? QString::fromUtf8(sqliteError) : u"Unknown error"_s);
                return;
            }
            break;
        }

        fillRow(stmt.get(), promise.result->m_fields.size(), rows);
    } while (true);

    promise.result->m_numRowsAffected = sqlite3_changes64(m_db);
    promise.result->m_rows            = rows;
}

/**
 * This method mimics sqlite3_exec() method which allows for
 * more than one query to be processed at the same time, which
 * we use for migrations and for non prepared statements, it
 * also doesn't allows for parameters which is fine for this use case.
 */
void ASqliteThread::queryExec(QueryPromise promise)
{
    auto _ = qScopeGuard([&] { Q_EMIT queryFinished(std::move(promise)); });

    int res                = SQLITE_OK;
    const QByteArray query = promise.result->m_query;

    bool emitQuery   = false;
    const char *zSql = query.data();
    while (res == SQLITE_OK && zSql[0]) {
        const char *zLeftover; /* Tail of unprocessed SQL */

        std::shared_ptr<sqlite3_stmt> stmt;
        {
            sqlite3_stmt *pStmt = nullptr;
            res                 = sqlite3_prepare_v2(m_db, zSql, -1, &pStmt, &zLeftover);
            if (res != SQLITE_OK) {
                const char *sqliteError = sqlite3_errmsg(m_db);
                promise.result->m_error = u"Failed to execute query: '%2'"_s.arg(
                    sqliteError ? QString::fromUtf8(sqliteError) : u"Unknown error"_s);
                return;
            }

            if (!pStmt) {
                /* this happens for a comment or white-space */
                zSql = zLeftover;
                continue;
            }
            stmt = std::shared_ptr<sqlite3_stmt>(
                pStmt, [](sqlite3_stmt *stmt) { sqlite3_finalize(stmt); });
        }

        if (emitQuery) {
            promise.result->m_lastResultSet = false;
            Q_EMIT queryFinished(promise);

            promise.result          = std::make_shared<AResultSqlite>();
            promise.result->m_query = query;
        }
        emitQuery = true;

        // We must copy this here because query object is likely the only
        // reference to the query data and it's going out of scope
        promise.result->m_query  = QByteArray{zSql, zLeftover - zSql};
        promise.result->m_fields = fillColumns(stmt.get());

        QVariantList rows;
        do {
            if (QThread::currentThread()->isInterruptionRequested()) {
                promise.result->m_error = u"Interrupt requested"_s;
                return;
            }

            res = sqlite3_step(stmt.get());

            if (res != SQLITE_ROW) {
                if (res != SQLITE_DONE) {
                    const char *sqliteError = sqlite3_errmsg(m_db);
                    promise.result->m_error = u"Failed to execute query: '%2'"_s.arg(
                        sqliteError ? QString::fromUtf8(sqliteError) : u"Unknown error"_s);
                    return;
                }
                break;
            }

            fillRow(stmt.get(), promise.result->m_fields.size(), rows);
        } while (true);

        promise.result->m_numRowsAffected = sqlite3_changes64(m_db);
        promise.result->m_rows            = rows;

        zSql = zLeftover;
        while (std::isspace(zSql[0])) {
            zSql++;
        }
        res = SQLITE_OK;
    }
}

std::shared_ptr<sqlite3_stmt> ASqliteThread::prepare(QueryPromise &promise, int flags)
{
    const auto size = promise.result->m_query.size() + 1;

    sqlite3_stmt *stmt = nullptr;
    int res =
        sqlite3_prepare_v3(m_db, promise.result->m_query.constData(), size, flags, &stmt, nullptr);
    if (res != SQLITE_OK) {
        const char *sqliteError = sqlite3_errmsg(m_db);
        promise.result->m_error = u"Failed to prepare statement: %1"_s.arg(
            sqliteError ? QString::fromUtf8(sqliteError) : u"Unknown error"_s);
        return {};
    }

    return std::shared_ptr<sqlite3_stmt>(stmt, [](sqlite3_stmt *stmt) { sqlite3_finalize(stmt); });
}

bool AResultSqlite::lastResultSet() const
{
    return m_lastResultSet;
}

bool AResultSqlite::hasError() const
{
    return m_error.has_value();
}

QString AResultSqlite::errorString() const
{
    return m_error.value();
}

QByteArray AResultSqlite::query() const
{
    return m_query;
}

QVariantList AResultSqlite::queryArgs() const
{
    return m_queryArgs;
}

int AResultSqlite::size() const
{
    return m_fields.empty() ? 0 : m_rows.size() / m_fields.size();
}

int AResultSqlite::fields() const
{
    return m_fields.size();
}

qint64 AResultSqlite::numRowsAffected() const
{
    return m_numRowsAffected;
}

int AResultSqlite::indexOfField(QLatin1String name) const
{
    return m_fields.indexOf(name);
}

QString AResultSqlite::fieldName(int column) const
{
    return m_fields.at(column);
}

QVariant AResultSqlite::value(int row, int column) const
{
    return m_rows.at(row * m_fields.size() + column);
}

bool AResultSqlite::isNull(int row, int column) const
{
    return value(row, column).isNull();
}

bool AResultSqlite::toBool(int row, int column) const
{
    return value(row, column).toBool();
}

int AResultSqlite::toInt(int row, int column) const
{
    return value(row, column).toInt();
}

qint64 AResultSqlite::toLongLong(int row, int column) const
{
    return value(row, column).toLongLong();
}

quint64 AResultSqlite::toULongLong(int row, int column) const
{
    return value(row, column).toULongLong();
}

double AResultSqlite::toDouble(int row, int column) const
{
    return value(row, column).toDouble();
}

QString AResultSqlite::toString(int row, int column) const
{
    return value(row, column).toString();
}

std::string AResultSqlite::toStdString(int row, int column) const
{
    return toString(row, column).toStdString();
}

QDate AResultSqlite::toDate(int row, int column) const
{
    return value(row, column).toDate();
}

QTime AResultSqlite::toTime(int row, int column) const
{
    return value(row, column).toTime();
}

QDateTime AResultSqlite::toDateTime(int row, int column) const
{
    return value(row, column).toDateTime();
}

QJsonValue AResultSqlite::toJsonValue(int row, int column) const
{
    auto doc = QJsonDocument::fromJson(toString(row, column).toUtf8());
    return doc.isObject() ? doc.object() : doc.isArray() ? doc.array() : QJsonValue{};
}

QCborValue AResultSqlite::toCborValue(int row, int column) const
{
    return QCborValue::fromCbor(toByteArray(row, column));
}

QByteArray AResultSqlite::toByteArray(int row, int column) const
{
    return value(row, column).toByteArray();
}

} // namespace ASql

#include "moc_ADriverSqlite.cpp"
