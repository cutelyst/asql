#pragma once

#include <adatabase.h>
#include <aresult.h>
#include <atransaction.h>
#include <coroutine>
#include <expected>
#include <utility>

#include <QPointer>
#include <QQueue>

namespace ASql {

/*!
 * \brief ACoroData is the concrete coroutine state class for \c ACoroExpected<T>.
 *
 * It merges the abstract delivery interface (\c ACoroResult or \c ACoroDatabase) with the actual
 * coroutine state (handle, result, status).  There is no separate inner "Data" struct —
 * \c ACoroExpected<T> holds a \c std::shared_ptr<ACoroData<T>> directly.
 *
 *  - When \c T == \c ADatabase the class inherits from \c ACoroDatabase and \c deliver(ADatabase)
 *    stores the connection.
 *  - For all other \c T the class inherits from \c ACoroResult and \c deliver(AResult &)
 *    converts the result into \c std::expected<T, QString>.
 */
template<typename T>
class ACoroData
    : public std::conditional_t<std::is_same_v<T, ADatabase>, ACoroDatabase, ACoroResult>
{
public:
    enum Status {
        Waiting,
        Suspended,
        Done,
        Finished,
    };

    std::coroutine_handle<> handle;
    std::expected<T, QString> result;
    Status status = Waiting;

    using DeliverArg =
        std::conditional_t<std::is_same_v<T, ADatabase>, ADatabase, AResult &>;

    void deliver(DeliverArg arg) override
    {
        if (status == Finished) {
            return;
        }

        if constexpr (std::is_same_v<T, ADatabase>) {
            if (arg.isValid()) {
                result = std::move(arg);
            } else {
                result = std::unexpected(
                    QStringLiteral("Could not get a valid database connection"));
            }
        } else {
            if (arg.hasError()) {
                result = std::unexpected(arg.errorString());
            } else {
                if constexpr (std::is_constructible_v<T, AResult &>) {
                    result = arg;
                } else {
                    result =
                        std::unexpected(QStringLiteral("deliver called on incompatible type; "
                                                       "use deliverDirect() instead"));
                }
            }
        }

        const auto prevStatus = std::exchange(status, Done);
        if (prevStatus == Suspended && handle) {
            handle.resume();
        }
    }

    template<typename U>
    void deliverDirect(U &&value)
    {
        if (status == Finished) {
            return;
        }

        result = std::forward<U>(value);

        const auto prevStatus = std::exchange(status, Done);
        if (prevStatus == Suspended && handle) {
            handle.resume();
        }
    }
};

template <typename T>
class ACoroExpected
{
public:
    bool await_ready() const noexcept
    {
        return m_data->status == ACoroData<T>::Done; // skips suspension
    }

    bool await_suspend(std::coroutine_handle<> h) noexcept
    {
        if (await_ready()) {
            return false;
        }

        m_data->handle = h;
        m_data->status = ACoroData<T>::Suspended;
        return true;
    }

    std::expected<T, QString> await_resume() { return m_data->result; }

    ACoroExpected(QObject *receiver)
        : m_receiver(receiver)
        , m_data{std::make_shared<ACoroData<T>>()}
    {
        if (receiver) {
            m_destroyConn = QObject::connect(
                receiver,
                &QObject::destroyed,
                [weak_data = std::weak_ptr<ACoroData<T>>(m_data)] {
                    auto data = weak_data.lock();
                    if (!data || data->status == ACoroData<T>::Finished) {
                        return;
                    }

                    data->result = std::unexpected(QStringLiteral("QObject receiver* destroyed"));
                    if (data->handle) {
                        data->handle.destroy();
                    }
                });
        }
    }

    ~ACoroExpected()
    {
        m_data->status = ACoroData<T>::Finished;
        QObject::disconnect(m_destroyConn);
    }

    [[nodiscard]] AExpectedResultRef ref() const
        requires(!std::is_same_v<T, ADatabase>)
    {
        return AExpectedResultRef{std::weak_ptr<ACoroResult>(m_data)};
    }

protected:
    friend class ADatabase;
    friend class ACache;
    friend class ATransaction;
    friend class APool;
    std::shared_ptr<ACoroData<T>> m_data;

private:
    QMetaObject::Connection m_destroyConn;
    QPointer<QObject> m_receiver;
};

template <typename T>
class ACoroMultiExpected
{
    enum Status {
        Waiting,
        Suspended,
        Done,
        Finished,
    };

    struct Data : public ACoroResult {
        std::coroutine_handle<> handle;
        QQueue<std::expected<T, QString>> results;
        Status status = Waiting;

        void deliver(AResult &result) override
        {
            if (status == Finished) {
                return;
            }

            const auto prevStatus = status;

            if (result.hasError()) {
                results.enqueue(std::unexpected(result.errorString()));
            } else {
                results.enqueue(result);
            }

            status = result.lastResultSet() ? Done : Waiting;

            if (prevStatus == Suspended && handle) {
                handle.resume();
            }
        }
    };

public:
    bool await_ready() const noexcept
    {
        return !m_data->results.isEmpty() || m_data->status == Done; // skips suspension
    }

    bool await_suspend(std::coroutine_handle<> h) noexcept
    {
        if (await_ready()) {
            return false;
        }

        m_data->handle = h;
        m_data->status = Suspended;
        return true;
    }

    std::expected<T, QString> await_resume()
    {
        return !m_data->results.empty()
                   ? m_data->results.dequeue()
                   : std::unexpected(QStringLiteral("no results available"));
    }

    ACoroMultiExpected(QObject *receiver)
        : m_receiver(receiver)
        , m_data{std::make_shared<Data>()}
    {
        if (receiver) {
            m_destroyConn = QObject::connect(
                receiver,
                &QObject::destroyed,
                [weak_data = std::weak_ptr<Data>(m_data)] {
                    auto data = weak_data.lock();
                    if (!data || data->status == Finished) {
                        return;
                    }

                    data->results.clear();
                    data->results.enqueue(
                        std::unexpected(QStringLiteral("QObject receiver* destroyed")));
                    if (data->handle) {
                        data->handle.destroy();
                    }
                });
        }
    }

    ~ACoroMultiExpected()
    {
        m_data->status = Finished;
        QObject::disconnect(m_destroyConn);
    }

    [[nodiscard]] AExpectedResultRef ref() const
    {
        return AExpectedResultRef{std::weak_ptr<ACoroResult>(m_data)};
    }

protected:
    friend class ADatabase;
    friend class ACache;
    friend class APool;
    std::shared_ptr<Data> m_data;

private:
    QMetaObject::Connection m_destroyConn;
    QPointer<QObject> m_receiver;
};

class AExpectedOpen
{
    struct Data : public ACoroOpenData {
        std::coroutine_handle<> handle;
        std::expected<bool, QString> result;
        bool delivered = false;
        bool finished  = false;

        void deliverOpen(bool isOpen, const QString &error) override
        {
            if (finished) {
                return;
            }

            delivered = true;
            if (isOpen) {
                result = true;
            } else {
                result = std::unexpected(error.isEmpty() ? QStringLiteral("Connection failed")
                                                         : error);
            }

            if (handle) {
                handle.resume();
            }
        }
    };

public:
    bool await_ready() const noexcept { return m_data->delivered; }

    bool await_suspend(std::coroutine_handle<> h) noexcept
    {
        m_data->handle = h;
        if (m_receiver) {
            m_destroyConn = QObject::connect(
                m_receiver,
                &QObject::destroyed,
                [weak_data = std::weak_ptr<Data>(m_data)] {
                    auto data = weak_data.lock();
                    if (!data || data->finished) {
                        return;
                    }

                    data->delivered = true;
                    data->result    = std::unexpected(QStringLiteral("QObject receiver* destroyed"));
                    if (data->handle) {
                        data->handle.destroy();
                    }
                });
        }

        return !await_ready();
    }

    std::expected<bool, QString> await_resume()
    {
        m_data->handle   = nullptr;
        m_data->finished = true;
        return m_data->result;
    }

    AExpectedOpen(QObject *receiver)
        : m_receiver(receiver)
        , m_data{std::make_shared<Data>()}
    {
    }

    ~AExpectedOpen()
    {
        m_data->finished = true;
        QObject::disconnect(m_destroyConn);
    }

protected:
    friend class ADatabase;
    std::shared_ptr<Data> m_data;

private:
    QMetaObject::Connection m_destroyConn;
    QPointer<QObject> m_receiver;
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

template <typename T>
class ATask
{
public:
    struct promise_type {
        std::coroutine_handle<promise_type> handle;
        std::vector<QMetaObject::Connection> connections;
        std::optional<T> value;                  // Store the return value
        std::coroutine_handle<> awaiting_handle; // Store the awaiting coroutine

        promise_type() = default; // Required for lambdas

        template <typename... ArgTypes>
        promise_type(QObject &obj, ArgTypes &&...)
        {
            yield_value(&obj);
        }

        ~promise_type()
        {
            clean();
            if (awaiting_handle) {
                awaiting_handle.destroy(); // Resume the awaiting coroutine
            }
        }

        void clean()
        {
            for (auto &conn : connections) {
                QObject::disconnect(conn);
            }
            connections.clear();
        }

        void return_value(T v) noexcept
        {
            value = v; // Store the return value
            if (awaiting_handle) {
                auto tmpHandle = std::exchange(awaiting_handle, {});
                tmpHandle.resume(); // Resume the awaiting coroutine
            }
        }

        ATask<T> get_return_object()
        {
            qDebug() << "get_return_object" << this;
            handle = std::coroutine_handle<promise_type>::from_promise(*this);
            return {*this};
        }

        std::suspend_never initial_suspend() const noexcept { return {}; }
        std::suspend_always final_suspend() noexcept
        {
            return {};
        } // Suspend at end to control resumption
        void unhandled_exception() {}

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

    // Access the stored value
    T get() const { return promise.value.value(); } // Use value() to access optional

    // Awaitable interface
    bool await_ready() const noexcept { return promise.value.has_value(); } // Ready if value is set
    void await_suspend(std::coroutine_handle<> h) noexcept
    {
        promise.awaiting_handle = h; // Store the awaiting coroutine's handle
    }
    T await_resume() const noexcept { return promise.value.value(); } // Return the stored value

    ~ATask()
    {
        qDebug() << Q_FUNC_INFO;
        promise.handle = {};
    }

private:
    promise_type &promise;
    ATask(promise_type &p)
        : promise(p)
    {
        qDebug() << Q_FUNC_INFO;
    }
};

} // namespace ASql
