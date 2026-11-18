/*
 * IXWebSocketCallbackTest.cpp
 * Allocation checks and an opt-in dispatch benchmark, separate from network timing.
 */
#include <catch_amalgamated.hpp>
#include <chrono>
#include <cstdlib>
#include <future>
#include <iostream>
#include <ixwebsocket/IXGetFreePort.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketServer.h>
#include <new>
#include <type_traits>

namespace
{
    thread_local bool countAllocations = false;
    thread_local size_t allocations = 0;
} // namespace

// Only this test executable overrides allocation. Callback registration is excluded.
void* operator new(std::size_t size)
{
    if (countAllocations)
    {
        ++allocations;
    }
    if (void* result = std::malloc(size ? size : 1))
    {
        return result;
    }
    throw std::bad_alloc();
}
void operator delete(void* pointer) noexcept
{
    std::free(pointer);
}
void* operator new[](std::size_t size)
{
    return ::operator new(size);
}
void operator delete[](void* pointer) noexcept
{
    ::operator delete(pointer);
}
#if defined(__cpp_sized_deallocation)
void operator delete(void* pointer, std::size_t) noexcept
{
    ::operator delete(pointer);
}
void operator delete[](void* pointer, std::size_t) noexcept
{
    ::operator delete(pointer);
}
#endif

namespace ix
{
    struct WebSocketCallbackTestAccess
    {
        static void emit(WebSocket& socket,
                         const std::string& payload,
                         WebSocketMessageType type = WebSocketMessageType::Message)
        {
            socket.invokeMessageCallback(type,
                                         payload,
                                         payload.size(),
                                         WebSocketErrorInfo(),
                                         WebSocketOpenInfo(),
                                         WebSocketCloseInfo(),
                                         true);
        }
    };
} // namespace ix

namespace
{
    struct GenericCallback
    {
        unsigned* count;
        template<class T>
        void operator()(const T&) const
        {
            ++*count;
        }
    };
} // namespace

TEST_CASE("Legacy callbacks, generic callables and null clearing remain source compatible",
          "[callback]")
{
    static_assert(std::is_same<ix::OnMessageCallback,
                               std::function<void(const ix::WebSocketMessagePtr&)>>::value,
                  "The existing callback type must remain unchanged");
    ix::WebSocket socket;
    auto oldSetter = &ix::WebSocket::setOnMessageCallback; // Still unambiguous.
    (socket.*oldSetter)(nullptr);
    CHECK_FALSE(socket.isOnMessageCallbackRegistered());
    unsigned oldCalls = 0, refCalls = 0;
    const std::string payload = "hello";
    socket.setOnMessageCallback(GenericCallback {&oldCalls});
    ix::WebSocketCallbackTestAccess::emit(socket, payload);
    CHECK(oldCalls == 1);
    socket.setOnMessageCallbackRef(
        [&](const ix::WebSocketMessage& event)
        {
            CHECK(&event.str == &payload);
            CHECK(event.str == "hello");
            ++refCalls;
        });
    ix::WebSocketCallbackTestAccess::emit(socket, payload);
    CHECK(oldCalls == 1);
    CHECK(refCalls == 1);
    socket.setOnMessageCallback(
        [&](const ix::WebSocketMessagePtr& event)
        {
            event->wireSize = 42; // Legacy metadata remains mutable through the pointer.
            ++oldCalls;
        });
    ix::WebSocketCallbackTestAccess::emit(socket, payload);
    CHECK(oldCalls == 2);
    CHECK(refCalls == 1);
    socket.setOnMessageCallbackRef(nullptr);
    CHECK_FALSE(socket.isOnMessageCallbackRegistered());
    socket.setOnMessageCallbackRef({});
    CHECK_FALSE(socket.isOnMessageCallbackRegistered());
    socket.setOnMessageCallback({});
    CHECK_FALSE(socket.isOnMessageCallbackRegistered());
}

TEST_CASE("Reference callbacks avoid event allocations when RTTI is enabled", "[callback]")
{
    const bool reference = GENERATE(false, true);
    ix::WebSocket socket;
    unsigned count = 0;
    auto callback = [&](const ix::WebSocketMessage&) { ++count; };
    if (reference)
    {
        socket.setOnMessageCallbackRef(callback);
    }
    else
    {
        socket.setOnMessageCallback([&](const ix::WebSocketMessagePtr& event)
                                    { callback(*event); });
    }
    const std::string payload = "hello";
    allocations = 0;
    countAllocations = true;
    for (unsigned i = 0; i < 100; ++i)
    {
        ix::WebSocketCallbackTestAccess::emit(socket, payload);
    }
    countAllocations = false;
    const size_t measured = allocations;
    CHECK(count == 100);
#if defined(__cpp_rtti) || defined(__GXX_RTTI) || defined(_CPPRTTI)
    CHECK(measured == (reference ? 0 : 100));
#else
    CHECK(measured == 100);
#endif
}

TEST_CASE("Reference callbacks receive every event type with borrowed payloads", "[callback]")
{
    const auto type = GENERATE(ix::WebSocketMessageType::Message,
                               ix::WebSocketMessageType::Open,
                               ix::WebSocketMessageType::Close,
                               ix::WebSocketMessageType::Error,
                               ix::WebSocketMessageType::Ping,
                               ix::WebSocketMessageType::Pong,
                               ix::WebSocketMessageType::Fragment);
    ix::WebSocket socket;
    const std::string payload = "payload";
    unsigned count = 0;
    socket.setOnMessageCallbackRef(
        [&](const ix::WebSocketMessage& message)
        {
            ++count;
            CHECK(message.type == type);
            CHECK(&message.str == &payload);
            CHECK(message.wireSize == payload.size());
            CHECK(message.binary);
        });
    ix::WebSocketCallbackTestAccess::emit(socket, payload, type);
    CHECK(count == 1);
}

TEST_CASE("Reference callbacks work on accepted sockets and client connections", "[callback]")
{
    ix::WebSocketServer server(ix::getFreePort(), "127.0.0.1");
    server.setOnConnectionCallback(
        [](std::weak_ptr<ix::WebSocket> weak, std::shared_ptr<ix::ConnectionState>)
        {
            auto socket = weak.lock();
            auto* raw = socket.get();
            socket->setOnMessageCallbackRef(
                [raw](const ix::WebSocketMessage& event)
                {
                    if (event.type == ix::WebSocketMessageType::Message)
                    {
                        raw->send(event.str, event.binary);
                    }
                });
        });
    REQUIRE(server.listen().first);
    server.start();
    std::promise<void> opened;
    auto open = opened.get_future();
    std::promise<std::string> echoed;
    auto echo = echoed.get_future();
    std::promise<void> closed;
    auto close = closed.get_future();
    ix::WebSocket client;
    client.disableAutomaticReconnection();
    client.setUrl("ws://127.0.0.1:" + std::to_string(server.getPort()));
    client.setOnMessageCallbackRef(
        [&](const ix::WebSocketMessage& message)
        {
            if (message.type == ix::WebSocketMessageType::Open)
            {
                opened.set_value();
            }
            if (message.type == ix::WebSocketMessageType::Message)
            {
                echoed.set_value(message.str);
            }
            if (message.type == ix::WebSocketMessageType::Close)
            {
                closed.set_value();
            }
        });
    client.start();
    REQUIRE(open.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
    REQUIRE(client.sendBinary("hello").success);
    REQUIRE(echo.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
    CHECK(echo.get() == "hello");
    client.stop();
    REQUIRE(close.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
}

TEST_CASE("Callback dispatch timing without network I/O", "[.callback_dispatch]")
{
    // Hidden from the normal suite. Run explicitly in a Release build.
    const uint64_t messages = 1000000;
    const std::string payload = "hello world";
    volatile uint64_t checksum = 0;
    for (unsigned round = 0; round < 9; ++round)
    {
        for (unsigned order = 0; order < 2; ++order)
        {
            const bool reference = ((round + order) % 2) != 0;
            ix::WebSocket socket;
            auto callback = [&](const ix::WebSocketMessage& message)
            { checksum += message.str.size() + message.wireSize; };
            if (reference)
            {
                socket.setOnMessageCallbackRef(callback);
            }
            else
            {
                socket.setOnMessageCallback([&](const ix::WebSocketMessagePtr& event)
                                            { callback(*event); });
            }
            for (unsigned i = 0; i < 10000; ++i)
            {
                ix::WebSocketCallbackTestAccess::emit(socket, payload);
            }
            const auto start = std::chrono::steady_clock::now();
            for (uint64_t i = 0; i < messages; ++i)
            {
                ix::WebSocketCallbackTestAccess::emit(socket, payload);
            }
            const auto end = std::chrono::steady_clock::now();
            const double ns =
                std::chrono::duration<double, std::nano>(end - start).count() / messages;
            std::cout << "{\"callback\":\"" << (reference ? "ref" : "legacy")
                      << "\",\"round\":" << round << ",\"messages\":" << messages
                      << ",\"ns_per_message\":" << ns << "}\n";
        }
    }
    REQUIRE(checksum != 0);
}
