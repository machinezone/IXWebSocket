/*
 *  IXCobraMetricsPublisher.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017 Machine Zone. All rights reserved.
 */

#include "IXCobraMetricsPublisher.h"

#include <algorithm>
#include <ixwebsocket/IXSocketTLSOptions.h>
#include <stdexcept>


namespace ix
{
    const int CobraMetricsPublisher::kVersion = 1;
    const std::string CobraMetricsPublisher::kSetRateControlId = "sms_set_rate_control_id";
    const std::string CobraMetricsPublisher::kSetBlacklistId = "sms_set_blacklist_id";

    CobraMetricsPublisher::CobraMetricsPublisher()
        : _enabled(true)
    {
    }

    CobraMetricsPublisher::~CobraMetricsPublisher()
    {
        ;
    }

    void CobraMetricsPublisher::configure(const CobraConfig& config, const std::string& channel)
    {
        // Configure the satori connection and start its publish background thread
        _cobra_metrics_theaded_publisher.configure(config, channel);
        _cobra_metrics_theaded_publisher.start();
    }

    Json::Value& CobraMetricsPublisher::getGenericAttributes()
    {
        std::lock_guard<std::mutex> lock(_device_mutex);
        return _device;
    }

    void CobraMetricsPublisher::setGenericAttributes(const std::string& attrName,
                                                     const Json::Value& value)
    {
        std::lock_guard<std::mutex> lock(_device_mutex);
        _device[attrName] = value;
    }

    void CobraMetricsPublisher::enable(bool enabled)
    {
        _enabled = enabled;
    }

    void CobraMetricsPublisher::setBlacklist(const std::vector<std::string>& blacklist)
    {
        _blacklist = blacklist;
        std::sort(_blacklist.begin(), _blacklist.end());

        // publish our blacklist
        Json::Value data;
        Json::Value metrics;
        for (auto&& metric : blacklist)
        {
            metrics.append(metric);
        }
        data["blacklist"] = metrics;
        push(kSetBlacklistId, data);
    }

    bool CobraMetricsPublisher::isMetricBlacklisted(const std::string& id) const
    {
        return std::binary_search(_blacklist.begin(), _blacklist.end(), id);
    }

    void CobraMetricsPublisher::setRateControl(
        const std::unordered_map<std::string, int>& rate_control)
    {
        for (auto&& it : rate_control)
        {
            if (it.second >= 0)
            {
                _rate_control[it.first] = it.second;
            }
        }

        // publish our rate_control
        Json::Value data;
        Json::Value metrics;
        for (auto&& it : _rate_control)
        {
            metrics[it.first] = it.second;
        }
        data["rate_control"] = metrics;
        push(kSetRateControlId, data);
    }

    bool CobraMetricsPublisher::isAboveMaxUpdateRate(const std::string& id) const
    {
        // Is this metrics rate controlled ?
        auto rate_control_it = _rate_control.find(id);
        if (rate_control_it == _rate_control.end()) return false;

        // Was this metrics already sent ?
        std::lock_guard<std::mutex> lock(_last_update_mutex);
        auto last_update = _last_update.find(id);
        if (last_update == _last_update.end()) return false;

        auto timeDeltaFromLastSend = std::chrono::steady_clock::now() - last_update->second;

        return timeDeltaFromLastSend < std::chrono::seconds(rate_control_it->second);
    }

    void CobraMetricsPublisher::setLastUpdate(const std::string& id)
    {
        std::lock_guard<std::mutex> lock(_last_update_mutex);
        _last_update[id] = std::chrono::steady_clock::now();
    }

    uint64_t CobraMetricsPublisher::getMillisecondsSinceEpoch() const
    {
        auto now = std::chrono::system_clock::now();
        auto ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

        return ms;
    }

    CobraConnection::MsgId CobraMetricsPublisher::push(const std::string& id,
                                                       const std::string& data,
                                                       bool shouldPushTest)
    {
        if (!_enabled) return CobraConnection::kInvalidMsgId;

        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(data, root)) return CobraConnection::kInvalidMsgId;

        return push(id, root, shouldPushTest);
    }

    CobraConnection::MsgId CobraMetricsPublisher::push(const std::string& id,
                                                       const CobraMetricsPublisher::Message& data)
    {
        if (!_enabled) return CobraConnection::kInvalidMsgId;

        Json::Value root;
        for (auto it : data)
        {
            root[it.first] = it.second;
        }

        return push(id, root);
    }

    bool CobraMetricsPublisher::shouldPush(const std::string& id) const
    {
        if (!_enabled) return false;
        if (isMetricBlacklisted(id)) return false;
        if (isAboveMaxUpdateRate(id)) return false;

        return true;
    }

    CobraConnection::MsgId CobraMetricsPublisher::push(const std::string& id,
                                                       const Json::Value& data,
                                                       bool shouldPushTest)
    {
        if (shouldPushTest && !shouldPush(id)) return CobraConnection::kInvalidMsgId;

        setLastUpdate(id);

        Json::Value msg;
        msg["id"] = id;
        msg["data"] = data;
        msg["session"] = _session;
        msg["version"] = kVersion;
        msg["timestamp"] = Json::UInt64(getMillisecondsSinceEpoch());

        {
            std::lock_guard<std::mutex> lock(_device_mutex);
            msg["device"] = _device;
        }

        {
            //
            // Bump a counter for each id
            // This is used to make sure that we are not
            // dropping messages, by checking that all the ids is the list of
            // all natural numbers until the last value sent (0, 1, 2, ..., N)
            //
            std::lock_guard<std::mutex> lock(_device_mutex);
            auto it = _counters.emplace(id, 0);
            msg["per_id_counter"] = it.first->second;
            it.first->second += 1;
        }

        // Now actually enqueue the task
        return _cobra_metrics_theaded_publisher.push(msg);
    }

    void CobraMetricsPublisher::setPublishMode(CobraConnectionPublishMode publishMode)
    {
        _cobra_metrics_theaded_publisher.setPublishMode(publishMode);
    }

    bool CobraMetricsPublisher::flushQueue()
    {
        return _cobra_metrics_theaded_publisher.flushQueue();
    }

    void CobraMetricsPublisher::suspend()
    {
        _cobra_metrics_theaded_publisher.suspend();
    }

    void CobraMetricsPublisher::resume()
    {
        _cobra_metrics_theaded_publisher.resume();
    }

    bool CobraMetricsPublisher::isConnected() const
    {
        return _cobra_metrics_theaded_publisher.isConnected();
    }

    bool CobraMetricsPublisher::isAuthenticated() const
    {
        return _cobra_metrics_theaded_publisher.isAuthenticated();
    }

} // namespace ix
