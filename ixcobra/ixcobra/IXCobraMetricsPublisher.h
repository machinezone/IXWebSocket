/*
 *  IXCobraMetricsPublisher.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017 Machine Zone. All rights reserved.
 */

#pragma once

#include "IXCobraMetricsThreadedPublisher.h"
#include <atomic>
#include <chrono>
#include <json/json.h>
#include <string>
#include <unordered_map>

namespace ix
{
    struct SocketTLSOptions;

    class CobraMetricsPublisher
    {
    public:
        CobraMetricsPublisher();
        ~CobraMetricsPublisher();

        /// Thread safety notes:
        ///
        /// 1. _enabled, _blacklist and _rate_control read/writes are not protected by a mutex
        /// to make shouldPush as fast as possible. _enabled default to false.
        ///
        /// The code that set those is ran only once at init, and
        /// the last value to be set is _enabled, which is also the first value checked in
        /// shouldPush, so there shouldn't be any race condition.
        ///
        /// 2. The queue of messages is thread safe, so multiple metrics can be safely pushed on
        /// multiple threads
        ///
        /// 3. Access to _last_update is protected as it needs to be read/write.
        ///

        /// Configuration / set keys, etc...
        /// All input data but the channel name is encrypted with rc4
        void configure(const CobraConfig& config, const std::string& channel);

        /// Setter for the list of blacklisted metrics ids.
        /// That list is sorted internally for fast lookups
        void setBlacklist(const std::vector<std::string>& blacklist);

        /// Set the maximum rate at which a metrics can be sent. Unit is seconds
        /// if rate_control = { 'foo_id': 60 },
        /// the foo_id metric cannot be pushed more than once every 60 seconds
        void setRateControl(const std::unordered_map<std::string, int>& rate_control);

        /// Configuration / enable/disable
        void enable(bool enabled);

        /// Simple interface, list of key value pairs where typeof(key) == typeof(value) == string
        typedef std::unordered_map<std::string, std::string> Message;
        CobraConnection::MsgId push(
            const std::string& id,
            const CobraMetricsPublisher::Message& data = CobraMetricsPublisher::Message());

        /// Richer interface using json, which supports types (bool, int, float) and hierarchies of
        /// elements
        ///
        /// The shouldPushTest argument should be set to false, and used in combination with the
        /// shouldPush method for places where we want to be as lightweight as possible when
        /// collecting metrics. When set to false, it is used so that we don't do double work when
        /// computing whether a metrics should be sent or not.
        CobraConnection::MsgId push(const std::string& id,
                                    const Json::Value& data,
                                    bool shouldPushTest = true);

        /// Interface used by lua. msg is a json encoded string.
        CobraConnection::MsgId push(const std::string& id,
                                    const std::string& data,
                                    bool shouldPushTest = true);

        /// Tells whether a metric can be pushed.
        /// A metric can be pushed if it satisfies those conditions:
        ///
        /// 1. the metrics system should be enabled
        /// 2. the metrics shouldn't be black-listed
        /// 3. the metrics shouldn't have reached its rate control limit at this
        /// "sampling"/"calling" time
        bool shouldPush(const std::string& id) const;

        /// Get generic information json object
        Json::Value& getGenericAttributes();

        /// Set generic information values
        void setGenericAttributes(const std::string& attrName, const Json::Value& value);

        /// Set a unique id for the session. A uuid can be used.
        void setSession(const std::string& session)
        {
            _session = session;
        }

        /// Get the unique id used to identify the current session
        const std::string& getSession() const
        {
            return _session;
        }

        /// Return the number of milliseconds since the epoch (~1970)
        uint64_t getMillisecondsSinceEpoch() const;

        /// Set satori connection publish mode
        void setPublishMode(CobraConnectionPublishMode publishMode);

        /// Flush the publish queue
        bool flushQueue();

        /// Lifecycle management. Free resources when backgrounding
        void suspend();
        void resume();

        /// Tells whether the socket connection is opened
        bool isConnected() const;

        /// Returns true only if we're authenticated
        bool isAuthenticated() const;

    private:
        /// Lookup an id in our metrics to see whether it is blacklisted
        /// Complexity is logarithmic
        bool isMetricBlacklisted(const std::string& id) const;

        /// Tells whether we should drop a metrics or not as part of an enqueuing
        /// because it exceed the max update rate (it is sent too often)
        bool isAboveMaxUpdateRate(const std::string& id) const;

        /// Record when a metric was last sent. Used for rate control
        void setLastUpdate(const std::string& id);

        ///
        /// Member variables
        ///

        CobraMetricsThreadedPublisher _cobra_metrics_theaded_publisher;

        /// A boolean to enable or disable this system
        /// push becomes a no-op when _enabled is false
        std::atomic<bool> _enabled;

        /// A uuid used to uniquely identify a session
        std::string _session;

        /// The _device json blob is populated once when configuring this system
        /// It record generic metadata about the client, run (version, device model, etc...)
        Json::Value _device;
        mutable std::mutex _device_mutex; // protect access to _device

        /// Metrics control (black list + rate control)
        std::vector<std::string> _blacklist;
        std::unordered_map<std::string, int> _rate_control;
        std::unordered_map<std::string, std::chrono::time_point<std::chrono::steady_clock>>
            _last_update;
        mutable std::mutex _last_update_mutex; // protect access to _last_update

        /// Bump a counter for each metric type
        std::unordered_map<std::string, int> _counters;
        mutable std::mutex _counters_mutex; // protect access to _counters

        // const strings for internal ids
        static const std::string kSetRateControlId;
        static const std::string kSetBlacklistId;

        /// Our protocol version. Can be used by subscribers who would want to be backward
        /// compatible if we change the way we arrange data
        static const int kVersion;
    };

} // namespace ix
