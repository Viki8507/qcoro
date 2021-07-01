// SPDX-FileCopyrightText: 2021 Daniel Vrátil <dvratil@kde.org>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QEventLoop>
#include <QObject>
#include <QTest>
#include <QTimer>
#include <QVariant>

#include "qcoro/task.h"

#include <chrono>

using namespace std::chrono_literals;

namespace QCoro {

class TestContext {
public:
    TestContext(QEventLoop &el);
    TestContext(TestContext &&) noexcept;
    TestContext(const TestContext &) = delete;

    ~TestContext();

    TestContext &operator=(TestContext &&) noexcept;
    TestContext &operator=(const TestContext &) = delete;

    void setShouldNotSuspend();

private:
    QEventLoop *mEventLoop = {};
};

class EventLoopChecker : public QTimer {
    Q_OBJECT
public:
    explicit EventLoopChecker(int minTicks = 10, std::chrono::milliseconds interval = 10ms)
        : mMinTicks{minTicks} {
        connect(this, &EventLoopChecker::timeout, this, &EventLoopChecker::timeoutCheck);
        setInterval(interval);
        start();
    }

    operator bool() const {
        return mTick > mMinTicks;
    }

private Q_SLOTS:
    void timeoutCheck() {
        ++mTick;
    }

private:
    int mTick = 0;
    int mMinTicks = 10;
};

template<typename TestClass>
class TestObject : public QObject {
protected:
    void coroWrapper(QCoro::Task<> (TestClass::*testFunction)(TestContext)) {
        QEventLoop el;
        QTimer::singleShot(5s, &el, [&el]() mutable { el.exit(1); });

        [[maybe_unused]] const auto unused = (static_cast<TestClass *>(this)->*testFunction)(el);

        bool testFinished = el.property("testFinished").toBool();
        const bool shouldNotSuspend = el.property("shouldNotSuspend").toBool();
        if (testFinished) {
            QVERIFY(shouldNotSuspend);
        } else {
            QVERIFY(!shouldNotSuspend);

            const auto result = el.exec();
            QVERIFY2(result == 0, "Test function has timed out");

            testFinished = el.property("testFinished").toBool();
            QVERIFY(testFinished);
        }
    }
};

#define addTest(name)                                                                              \
    void test##name() {                                                                            \
        coroWrapper(&std::remove_cvref_t<decltype(*this)>::test##name##_coro);                     \
    }
} // namespace QCoro

#define QCORO_VERIFY(statement)                                                                    \
    do {                                                                                           \
        if (!QTest::qVerify(static_cast<bool>(statement), #statement, "", __FILE__, __LINE__))     \
            co_return;                                                                             \
    } while (false)

#define QCORO_COMPARE(actual, expected)                                                            \
    do {                                                                                           \
        if (!QTest::qCompare(actual, expected, #actual, #expected, __FILE__, __LINE__))            \
            co_return;                                                                             \
    } while (false)
