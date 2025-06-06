#pragma once

#include <adatabase.h>
#include <aresult.h>
#include <atransaction.h>
#include <coroutine>
#include <expected>

#include <QPointer>
#include <QQueue>

namespace ASql {

template <typename T>
class ACoroExpected
{
public:
    bool await_ready() const noexcept
    {
        return *m_status == Done; // skips suspension
    }

    bool await_suspend(std::coroutine_handle<> h) noexcept
    {
        if (await_ready()) {
            return false;
        }

        m_handle  = h;
        *m_status = Suspended;
        return true;
    }

    std::expected<T, QString> await_resume() { return m_result; }

    ACoroExpected(QObject *receiver)
        : m_receiver(receiver)
        , m_status{std::make_shared<Status>(Waiting)}
    {
        callback = [this, status = m_status](AResult &result) {
            const auto currentState = *status;
            if (currentState == Done || currentState == Finished) {
                return;
            }

            if (result.hasError()) {
                m_result = std::unexpected(result.errorString());
            } else {
                if constexpr (std::is_same_v<T, ATransaction>) {
                    m_result = ATransaction(database, true);
                } else {
                    m_result = result;
                }
            }

            *status = Done;

            if (currentState == Suspended && m_handle) {
                m_handle.resume();
            }
        };

        if (receiver) {
            m_destroyConn =
                QObject::connect(receiver, &QObject::destroyed, [this, status = m_status] {
                if (*status == Finished) {
                    return;
                }

                m_result = std::unexpected(QStringLiteral("QObject receiver* destroyed"));
                if (m_handle) {
                    m_handle.destroy();
                }
            });
        }
    }

    ~ACoroExpected()
    {
        *m_status = Finished;
        QObject::disconnect(m_destroyConn);
    }

protected:
    friend class ADatabase;
    friend class ACache;
    friend class ATransaction;
    friend class APool;
    std::function<void(AResult &result)> callback;
    ADatabase database;

private:
    enum Status {
        Waiting,
        Suspended,
        Done,
        Finished,
    };
    QMetaObject::Connection m_destroyConn;
    QPointer<QObject> m_receiver;
    std::expected<T, QString> m_result;
    std::shared_ptr<Status> m_status;
    std::coroutine_handle<> m_handle;
};

template <typename T>
class ACoroMultiExpected
{
public:
    bool await_ready() const noexcept
    {
        return !m_results.isEmpty() || *m_status == Done; // skips suspension
    }

    bool await_suspend(std::coroutine_handle<> h) noexcept
    {
        if (await_ready()) {
            return false;
        }

        m_handle  = h;
        *m_status = Suspended;
        return true;
    }

    std::expected<T, QString> await_resume()
    {
        return !m_results.empty() ? m_results.dequeue()
                                  : std::unexpected(QStringLiteral("no results available"));
    }

    ACoroMultiExpected(QObject *receiver)
        : m_receiver(receiver)
        , m_status{std::make_shared<Status>(Waiting)}
    {
        callback = [this, status = m_status](AResult &result) {
            const auto currentState = *status;
            if (currentState == Finished) {
                return;
            }

            if (result.hasError()) {
                m_results.enqueue(std::unexpected(result.errorString()));
            } else {
                if constexpr (std::is_same_v<T, ATransaction>) {
                    m_results.enqueue(ATransaction(database, true));
                } else {
                    m_results.enqueue(result);
                }
            }

            if (result.lastResultSet()) {
                *status = Done;
            } else {
                *m_status = Waiting;
            }

            if (currentState == Suspended && m_handle) {
                m_handle.resume();
            }
        };

        if (receiver) {
            m_destroyConn =
                QObject::connect(receiver, &QObject::destroyed, [this, status = m_status] {
                if (*status == Finished) {
                    return;
                }

                m_results.clear();
                m_results.enqueue(std::unexpected(QStringLiteral("QObject receiver* destroyed")));
                if (m_handle) {
                    m_handle.destroy();
                }
            });
        }
    }

    ~ACoroMultiExpected()
    {
        *m_status = Finished;
        QObject::disconnect(m_destroyConn);
    }

protected:
    friend class ADatabase;
    friend class ACache;
    friend class ATransaction;
    friend class APool;
    std::function<void(AResult &result)> callback;
    ADatabase database;

private:
    enum Status {
        Waiting,
        Suspended,
        Done,
        Finished,
    };
    QMetaObject::Connection m_destroyConn;
    QPointer<QObject> m_receiver;
    QQueue<std::expected<T, QString>> m_results;
    std::shared_ptr<Status> m_status;
    std::coroutine_handle<> m_handle;
};

class AExpectedDatabase
{
public:
    bool await_ready() const noexcept
    {
        return m_result.has_value() || !m_result.error().isEmpty();
    }

    bool await_suspend(std::coroutine_handle<> h) noexcept
    {
        m_handle = h;
        if (m_receiver) {
            m_destroyConn = QObject::connect(m_receiver, &QObject::destroyed, [this] {
                m_result = std::unexpected(QStringLiteral("QObject receiver* destroyed"));
                if (m_handle) {
                    m_handle.destroy();
                }
            });
        }

        return !await_ready();
    }

    std::expected<ADatabase, QString> await_resume()
    {
        m_handle = nullptr;
        return m_result;
    }

    AExpectedDatabase(QObject *receiver)
        : m_receiver(receiver)
        , m_result{std::unexpected(QString{})}
        , m_finished{std::make_shared<bool>(false)}
    {
        callback = [this, finished = m_finished](ADatabase db) {
            if (*finished) {
                return;
            }

            if (db.isValid()) {
                m_result = db;
            } else {
                m_result =
                    std::unexpected(QStringLiteral("Could not get a valid database connection"));
            }

            if (m_handle) {
                m_handle.resume();
            }
        };
    }

    ~AExpectedDatabase()
    {
        *m_finished = true;
        QObject::disconnect(m_destroyConn);
    }

protected:
    friend class APool;
    std::function<void(ADatabase db)> callback;

private:
    QMetaObject::Connection m_destroyConn;
    QPointer<QObject> m_receiver;
    std::expected<ADatabase, QString> m_result;
    std::shared_ptr<bool> m_finished;
    std::coroutine_handle<> m_handle;
};

/**
 * @brief The ACoroTerminator class
 * co_yield object; that should destroy this corouting
 * on their desctruction, note that if the caller of the
 * coroutine delete object; this might result into a double
 * free due coroutine dtor already be on the stack, in such
 * cases always object->deleteLater();
 */
class ACoroTerminator
{
public:
    struct promise_type {
        std::coroutine_handle<promise_type> handle;
        std::vector<QMetaObject::Connection> connections;

        promise_type() = default; // required for lambdas

        template <typename... ArgTypes>
        promise_type(QObject &obj, ArgTypes &&...)
        {
            yield_value(&obj);
        }

        ~promise_type() { clean(); }

        void clean()
        {
            for (auto &conn : connections) {
                QObject::disconnect(conn);
            }
            connections.clear();
        }

        void return_void() noexcept {}

        ACoroTerminator get_return_object()
        {
            handle = std::coroutine_handle<promise_type>::from_promise(*this);
            return {};
        }

        std::suspend_never initial_suspend() const noexcept { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void unhandled_exception() {}

        bool await_ready() const noexcept { return false; }

        std::suspend_never yield_value(QObject *obj)
        {
            auto conn = QObject::connect(obj, &QObject::destroyed, [this] {
                clean();
                if (handle) {
                    handle.destroy();
                }
            });
            connections.emplace_back(std::move(conn));
            return {};
        }
    };
};

} // namespace ASql
