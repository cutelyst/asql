# Copilot Instructions

## C++ coroutine lambdas — NEVER use captures

Lambda-coroutines (`[](...) -> ACoroTerminator { co_await ...; }`) must have an
**empty capture list `[]`**. The lambda's closure object is a temporary that is
destroyed at the first `co_await` suspension point. Any captured variable then
becomes a dangling reference inside the coroutine frame, causing undefined
behaviour and hard-to-reproduce crashes.

**Forbidden:**
```cpp
[this, finished, value]() -> ACoroTerminator {
    auto result = co_await APool::exec(...);
    // 'this', 'finished', 'value' are dangling here
}();
```

**Required — pass everything as named parameters:**
```cpp
[](MyClass *self, std::shared_ptr<QObject> finished, int value) -> ACoroTerminator {
    auto result = co_await APool::exec(...);  // parameters live in the coroutine frame
}(this, finished, value);
```

Parameters are stored directly in the coroutine frame and remain valid for the
entire lifetime of the coroutine.

The same rule applies to named lambda variables that are called immediately or
stored and invoked later — no captures of any kind on any lambda that contains
a `co_await` or `co_yield` expression.

Note: inner non-coroutine lambdas (e.g. `qScopeGuard([finished] {})`) are not
coroutines and may capture normally.
