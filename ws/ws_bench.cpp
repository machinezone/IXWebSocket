/*
 * ws_bench.cpp
 * Autoroute-compatible receive benchmark. No per-message logging or locking.
 */
#include "ws_bench.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iomanip>
#include <iostream>
#include <ixwebsocket/IXUrlParser.h>
#include <ixwebsocket/IXWebSocket.h>
#include <limits>
#include <mutex>
#include <vector>

namespace
{
    using Clock = std::chrono::steady_clock;
    struct Result
    {
        std::string callback;
        uint64_t messages = 0;
        uint64_t bytes = 0;
        double seconds = 0;
        double rate() const
        {
            return messages / seconds;
        }
    };

    bool receive(const ix::WebSocketBenchOptions& options, const std::string& mode, Result& result)
    {
        std::mutex mutex;
        std::condition_variable changed;
        bool done = false;
        bool success = false;
        bool opened = false;
        std::string error;
        Clock::time_point start;
        std::atomic<uint64_t> progress {0};
        uint64_t received = 0;
        uint64_t bytes = 0;
        ix::WebSocket socket; // Destroy before callback state.
        const auto query = options.url.find('?');
        std::string endpoint = options.url.substr(0, query);
        while (!endpoint.empty() && endpoint.back() == '/')
        {
            endpoint.pop_back();
        }
        endpoint += "/" + std::to_string(options.messages);
        if (query != std::string::npos)
        {
            endpoint += options.url.substr(query);
        }
        socket.setUrl(endpoint);
        socket.setTLSOptions(options.tlsOptions);
        socket.setProxyOptions(options.proxyOptions);
        socket.setHandshakeTimeout(options.timeout);
        socket.disableAutomaticReconnection();
        if (!options.compression)
        {
            socket.disablePerMessageDeflate();
        }

        const auto callback = [&](const ix::WebSocketMessage& message)
        {
            if (message.type == ix::WebSocketMessageType::Message)
            {
                if (received >= options.messages)
                {
                    return;
                }
                ++received;
                bytes += message.str.size();
                if ((received & 1023) == 0)
                {
                    progress.store(received, std::memory_order_relaxed);
                }
                if (received == options.messages)
                {
                    const auto end = Clock::now();
                    std::lock_guard<std::mutex> lock(mutex);
                    if (done)
                    {
                        return;
                    }
                    result.callback = mode;
                    result.messages = received;
                    result.bytes = bytes;
                    result.seconds = std::chrono::duration<double>(end - start).count();
                    success = true;
                    done = true;
                    changed.notify_one();
                }
            }
            else if (message.type == ix::WebSocketMessageType::Open)
            {
                std::lock_guard<std::mutex> lock(mutex);
                start = Clock::now();
                opened = true;
            }
            else if (message.type == ix::WebSocketMessageType::Error ||
                     message.type == ix::WebSocketMessageType::Close)
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (!done)
                {
                    error = message.type == ix::WebSocketMessageType::Error
                                ? message.errorInfo.reason
                                : "Connection closed before all messages arrived";
                    done = true;
                    changed.notify_one();
                }
            }
        };
        if (mode == "ref")
        {
            socket.setOnMessageCallbackRef(callback);
        }
        else
        {
            socket.setOnMessageCallback([&](const ix::WebSocketMessagePtr& message)
                                        { callback(*message); });
        }
        const auto deadline = Clock::now() + std::chrono::seconds(options.timeout);
        auto nextReport = Clock::now() + std::chrono::seconds(1);
        socket.start();
        std::unique_lock<std::mutex> lock(mutex);
        uint64_t previous = 0;
        auto lastReport = Clock::now();
        while (!done)
        {
            if (changed.wait_until(lock, (std::min) (deadline, nextReport), [&] { return done; }))
            {
                break;
            }
            if (Clock::now() >= deadline)
            {
                error = "Benchmark timed out";
                done = true;
                break;
            }
            const auto count = progress.load(std::memory_order_relaxed);
            if (!options.quiet && opened)
            {
                const auto now = Clock::now();
                const double seconds =
                    std::chrono::duration<double>(now - (std::max) (start, lastReport)).count();
                std::cerr << mode << ": ~" << count << "/" << options.messages << " received, "
                          << static_cast<uint64_t>((count - previous) / seconds) << " messages/s\n";
                lastReport = now;
            }
            previous = count;
            nextReport += std::chrono::seconds(1);
        }
        lock.unlock();
        socket.stop();
        if (!success || !error.empty() || result.seconds <= 0)
        {
            std::cerr << "bench " << mode << ": " << error << " (received " << received << "/"
                      << options.messages << ")\n";
            return false;
        }
        return true;
    }

    double median(std::vector<double> values)
    {
        std::sort(values.begin(), values.end());
        const size_t middle = values.size() / 2;
        return values.size() % 2 ? values[middle] : (values[middle - 1] + values[middle]) / 2;
    }
} // namespace

namespace ix
{
    int ws_bench(const WebSocketBenchOptions& options)
    {
        std::string protocol, host, path, query;
        int port;
        if (!UrlParser::parse(options.url, protocol, host, path, query, port) ||
            (protocol != "ws" && protocol != "wss") || options.url.find('#') != std::string::npos ||
            options.messages == 0 ||
            options.messages > static_cast<uint64_t>((std::numeric_limits<int>::max)()) ||
            options.runs == 0 || options.runs > 1000 || options.warmup > 100 ||
            options.timeout <= 0 ||
            (options.callback != "legacy" && options.callback != "ref" &&
             options.callback != "both"))
        {
            std::cerr << "Invalid benchmark URL, count, runs, timeout, or callback mode\n";
            return 1;
        }
        std::vector<Result> results;
        std::vector<std::string> modes = options.callback == "both"
                                             ? std::vector<std::string> {"legacy", "ref"}
                                             : std::vector<std::string> {options.callback};
        const unsigned total = options.warmup + options.runs;
        for (unsigned round = 0; round < total; ++round)
        {
            // Alternate which mode goes first, including warmup connections.
            if (modes.size() == 2 && round > 0)
            {
                std::reverse(modes.begin(), modes.end());
            }
            for (const auto& mode : modes)
            {
                if (!options.quiet)
                {
                    std::cerr << (round < options.warmup ? "warmup " : "run ") << mode << " "
                              << (round + 1) << "/" << total << "\n";
                }
                Result result;
                if (!receive(options, mode, result))
                {
                    return 1;
                }
                if (round >= options.warmup)
                {
                    results.push_back(result);
                }
            }
        }
        std::cout << std::setprecision(10);
        if (options.json)
        {
            std::cout << "{\"messages\":" << options.messages << ",\"runs\":" << options.runs
                      << ",\"warmup\":" << options.warmup
                      << ",\"compression_requested\":" << (options.compression ? "true" : "false")
                      << ",\"trials\":[";
            for (size_t i = 0; i < results.size(); ++i)
            {
                const auto& r = results[i];
                if (i)
                {
                    std::cout << ',';
                }
                std::cout << "{\"callback\":\"" << r.callback << "\",\"messages\":" << r.messages
                          << ",\"payload_bytes\":" << r.bytes
                          << ",\"elapsed_ms\":" << r.seconds * 1000
                          << ",\"messages_per_second\":" << r.rate()
                          << ",\"mib_per_second\":" << r.bytes / r.seconds / 1048576 << '}';
            }
            std::cout << "],\"summary\":[";
        }
        bool first = true;
        double legacyRate = 0, referenceRate = 0;
        for (const auto& mode : std::vector<std::string> {"legacy", "ref"})
        {
            std::vector<double> rates;
            for (const auto& r : results)
            {
                if (r.callback == mode)
                {
                    rates.push_back(r.rate());
                }
            }
            if (rates.empty())
            {
                continue;
            }
            const double middle = median(rates);
            if (mode == "legacy")
            {
                legacyRate = middle;
            }
            else
            {
                referenceRate = middle;
            }
            if (options.json)
            {
                if (!first)
                {
                    std::cout << ',';
                }
                std::cout << "{\"callback\":\"" << mode
                          << "\",\"median_messages_per_second\":" << middle
                          << ",\"min_messages_per_second\":"
                          << *std::min_element(rates.begin(), rates.end())
                          << ",\"max_messages_per_second\":"
                          << *std::max_element(rates.begin(), rates.end()) << '}';
            }
            else
            {
                std::cout << mode << ": median " << middle << " messages/s (min "
                          << *std::min_element(rates.begin(), rates.end()) << ", max "
                          << *std::max_element(rates.begin(), rates.end()) << "), " << rates.size()
                          << " runs\n";
            }
            first = false;
        }
        if (options.json)
        {
            std::cout << ']';
        }
        if (legacyRate && referenceRate)
        {
            const double gain = (referenceRate / legacyRate - 1) * 100;
            if (options.json)
            {
                std::cout << ",\"reference_gain_percent\":" << gain;
            }
            else
            {
                std::cout << "Reference median throughput change: " << gain << "%\n";
            }
        }
        if (options.json)
        {
            std::cout << '}';
        }
        std::cout << '\n';
        return 0;
    }
} // namespace ix
