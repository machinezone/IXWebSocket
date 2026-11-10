/*
 *  IXWebSocketSendFailureTest.cpp
 *  Copyright (c) 2026 Machine Zone, Inc. All rights reserved.
 */
#include <atomic>
#include <catch_amalgamated.hpp>
#include <chrono>
#include <condition_variable>
#include <future>
#include <ixwebsocket/IXSocket.h>
#include <ixwebsocket/IXWebSocketTransport.h>
#include <mutex>
#include <thread>
#include <vector>

namespace ix
{
    struct WebSocketTransportTestAccess
    {
        static void install(WebSocketTransport& transport,
                            std::unique_ptr<Socket> socket,
                            bool blocking)
        {
            transport._socket = std::move(socket);
            transport._useMask = false;
            transport._blockingSend = blocking;
            transport._sendTimeoutSecs = 1;
            transport._rxbuf = {0x81, 3, 'o', 'n', 'e', 0x81, 3, 't', 'w', 'o'};
            transport.setReadyState(WebSocketTransport::ReadyState::OPEN);
        }
    };
} // namespace ix

namespace
{
    using Transport = ix::WebSocketTransport;

    class FailedWriteSocket final : public ix::Socket
    {
    public:
        explicit FailedWriteSocket(int mode)
            : mode(mode)
        {
        }

        std::ptrdiff_t send(char*, size_t) override
        {
            ++writes;
            setErrno(mode < 2 ? EIO : EWOULDBLOCK);
            return -1;
        }

        ix::PollResultType isReadyToWrite(int) override
        {
            if (mode == 3)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1100));
                return ix::PollResultType::Timeout;
            }
            if (mode == 4)
            {
                ++polls;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                return ix::PollResultType::Timeout;
            }
            return ix::PollResultType::Error;
        }

        void close() override
        {
            ++closes;
        }

        const int mode;
        std::atomic<unsigned> polls {0};
        std::atomic<unsigned> writes {0};
        std::atomic<unsigned> closes {0};
    };
} // namespace

TEST_CASE("Send failures finish callbacks on the I/O thread", "[websocket][close][send_failure]")
{
    // Client fatal write, server fatal write, server flush error, server send timeout.
    const int mode = GENERATE(0, 1, 2, 3);
    CAPTURE(mode);
    Transport transport;
    auto* socket = new FailedWriteSocket(mode);
    ix::WebSocketTransportTestAccess::install(
        transport, std::unique_ptr<ix::Socket>(socket), mode != 0);
    std::mutex mutex;
    std::condition_variable changed;
    bool entered = false;
    bool release = false;
    bool messageActive = false;
    bool overlapped = false;
    bool closedInCallback = false;
    uint16_t closeCode = 0;
    std::string closeReason;
    std::vector<std::string> events;
    std::vector<std::thread::id> callbackThreads;
    transport.setOnCloseCallback(
        [&](uint16_t code, const std::string& reason, size_t, bool)
        {
            {
                std::lock_guard<std::mutex> lock(mutex);
                overlapped = messageActive;
                closedInCallback = transport.getReadyState() == Transport::ReadyState::CLOSED;
                closeCode = code;
                closeReason = reason;
                events.push_back("close");
                callbackThreads.push_back(std::this_thread::get_id());
            }
            // Calling close() from a terminal notification must not deadlock.
            if (closedInCallback) transport.close();
        });
    std::thread::id ioThread;
    auto owner = std::async(
        std::launch::async,
        [&]
        {
            ioThread = std::this_thread::get_id();
            auto message = [&](const std::string& value, size_t, bool, Transport::MessageKind)
            {
                std::unique_lock<std::mutex> lock(mutex);
                events.push_back(value);
                callbackThreads.push_back(std::this_thread::get_id());
                if (value == "one")
                {
                    messageActive = true;
                    entered = true;
                    changed.notify_all();
                    changed.wait(lock, [&] { return release; });
                    messageActive = false;
                }
            };
            transport.dispatch(Transport::PollResult::Succeeded, message);
            transport.dispatch(transport.poll(), message);
        });
    bool received;
    {
        std::unique_lock<std::mutex> lock(mutex);
        received = changed.wait_for(lock, std::chrono::seconds(2), [&] { return entered; });
    }
    auto sender =
        std::async(std::launch::async,
                   [&] { return transport.sendText(std::string("outgoing"), nullptr).success; });
    const bool returned = sender.wait_for(std::chrono::seconds(3)) == std::future_status::ready;
    {
        std::lock_guard<std::mutex> lock(mutex);
        release = true;
    }
    changed.notify_all();
    const bool success = sender.get();
    owner.get();
    REQUIRE(received);
    REQUIRE(returned);
    CHECK_FALSE(success);
    CHECK_FALSE(overlapped);
    CHECK(closedInCallback);
    CHECK(closeCode == ix::WebSocketCloseConstants::kAbnormalCloseCode);
    CHECK(closeReason == (mode == 3 ? ix::WebSocketCloseConstants::kSendTimeoutMessage
                                    : ix::WebSocketCloseConstants::kAbnormalCloseMessage));
    CHECK(events == std::vector<std::string> {"one", "close"});
    CHECK(callbackThreads == std::vector<std::thread::id>(2, ioThread));
    CHECK(socket->closes == 1);
    CHECK(socket->writes == 1); // An abnormal close must not attempt a Close frame.
    CHECK(transport.bufferedAmount() == 0);
}

TEST_CASE("Closing cancels a blocked send without waiting for its timeout",
          "[websocket][close][send_failure]")
{
    Transport transport;
    auto* socket = new FailedWriteSocket(4);
    ix::WebSocketTransportTestAccess::install(transport, std::unique_ptr<ix::Socket>(socket), true);
    auto sender = std::async(std::launch::async,
                             [&] { return transport.sendText(std::string("outgoing"), nullptr); });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (socket->polls == 0 && std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    const bool blocked = socket->polls != 0;
    const auto start = std::chrono::steady_clock::now();
    transport.close(4000, "cancelled");
    const bool returned =
        sender.wait_for(std::chrono::milliseconds(500)) == std::future_status::ready;
    sender.get();
    CHECK(blocked);
    CHECK(returned);
    CHECK(std::chrono::steady_clock::now() - start < std::chrono::milliseconds(500));
    CHECK(transport.getReadyState() == Transport::ReadyState::CLOSING);
}

TEST_CASE("A close request keeps its code when a write fails", "[websocket][close][send_failure]")
{
    const bool closeFirst = GENERATE(false, true);
    Transport transport;
    ix::WebSocketTransportTestAccess::install(
        transport, std::unique_ptr<ix::Socket>(new FailedWriteSocket(0)), false);
    uint16_t code = 0;
    unsigned notifications = 0;
    transport.setOnCloseCallback(
        [&](uint16_t value, const std::string&, size_t, bool)
        {
            code = value;
            ++notifications;
            CHECK_FALSE(transport.sendText(std::string("after close"), nullptr).success);
        });
    if (!closeFirst) CHECK_FALSE(transport.sendText(std::string("outgoing"), nullptr).success);
    transport.close(4000, "cancelled");
    auto message = [](const std::string&, size_t, bool, Transport::MessageKind) { };
    transport.dispatch(transport.poll(), message);
    transport.dispatch(transport.poll(), message);
    CHECK(code == (closeFirst ? 4000 : ix::WebSocketCloseConstants::kAbnormalCloseCode));
    CHECK(notifications == 1);
}

TEST_CASE("Abnormal close before connecting is harmless", "[websocket][close][send_failure]")
{
    Transport transport;
    transport.close(ix::WebSocketCloseConstants::kAbnormalCloseCode);
    CHECK(transport.getReadyState() == Transport::ReadyState::CLOSED);
}
