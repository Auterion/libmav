/****************************************************************************
 * 
 * Copyright (c) 2023, libmav development team
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in 
 *    the documentation and/or other materials provided with the 
 *    distribution.
 * 3. Neither the name libmav nor the names of its contributors may be 
 *    used to endorse or promote products derived from this software 
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE 
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, 
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS 
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED 
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN 
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 ****************************************************************************/
#ifndef MAV_CONNECTION_H
#define MAV_CONNECTION_H

#include <mutex>
#include <unordered_map>
#include <future>
#include <utility>
#include <vector>
#include "MessageSet.h"

namespace mav {

    using CallbackHandle = uint64_t;

    class TimeoutException : public std::runtime_error {
    public:
        explicit TimeoutException(const char* msg) : std::runtime_error(msg) {}
    };


    class Connection {
    public:
        using Expectation = std::shared_ptr<std::promise<Message>>;

    private:
        using ExpectationWeakRef = std::weak_ptr<std::promise<Message>>;

        struct FunctionCallback {
            std::function<void(const Message &message)> callback;
            std::function<void(const std::exception_ptr& exception)> error_callback;
        };

        struct PromiseCallback {
            ExpectationWeakRef promise;
            std::function<bool(const Message &message)> selector;
        };

        using Callback = std::variant<FunctionCallback, PromiseCallback>;

        static constexpr std::chrono::milliseconds CONNECTION_TIMEOUT = std::chrono::milliseconds(3000);

        // connection properties
        const MessageSet& _message_set;
        ConnectionPartner _partner;

        // connection state
        std::chrono::time_point<std::chrono::steady_clock> _last_received_ms = std::chrono::steady_clock::time_point(
            std::chrono::milliseconds(0));
        bool _underlying_network_fault = false;

        // callbacks
        std::function<void(Message &message)> _send_to_network_function;

        std::mutex _message_callback_mtx;

        CallbackHandle _next_handle = 0;
        std::unordered_map<CallbackHandle, Callback> _message_callbacks;

    public:

        size_t callbackCount() {
            std::scoped_lock<std::mutex> lock(_message_callback_mtx);
            return _message_callbacks.size();
        }

        void removeAllCallbacks() {
            std::scoped_lock<std::mutex> lock(_message_callback_mtx);
            _message_callbacks.clear();
        }

        Connection(const MessageSet &message_set, ConnectionPartner partner) :
        _message_set(message_set), _partner(partner) {
            _last_received_ms = std::chrono::steady_clock::now();
        }

        ConnectionPartner partner() const {
            return _partner;
        }

        // Snapshot the registry, then run callbacks with the lock released, so a callback can take its
        // own locks or call back into the registry without deadlocking.
        void consumeMessageFromNetwork(const Message& message) noexcept {
            // in case we received a heartbeat, update last heartbeat time, to keep the connection alive.
            _last_received_ms = std::chrono::steady_clock::now();

            // if we received a message, we can assume that the connection is working again.
            _underlying_network_fault = false;

            std::vector<std::pair<CallbackHandle, Callback>> snapshot;
            {
                std::scoped_lock<std::mutex> lock(_message_callback_mtx);
                snapshot.reserve(_message_callbacks.size());
                for (const auto &entry : _message_callbacks) {
                    snapshot.emplace_back(entry.first, entry.second);
                }
            }

            std::vector<CallbackHandle> finished_promises;
            std::vector<Expectation> to_fulfill;
            for (auto &entry : snapshot) {
                std::visit([&](auto&& arg) {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, FunctionCallback>) {
                        if (arg.callback) {
                            arg.callback(message);
                        }
                    } else if constexpr (std::is_same_v<T, PromiseCallback>) {
                        auto promise = arg.promise.lock();
                        if (!promise) {
                            finished_promises.push_back(entry.first);
                        } else if (arg.selector(message)) {
                            to_fulfill.push_back(std::move(promise));
                            finished_promises.push_back(entry.first);
                        }
                    }
                }, entry.second);
            }

            // erase before fulfilling, so a woken waiter never observes a stale registry
            if (!finished_promises.empty()) {
                std::scoped_lock<std::mutex> lock(_message_callback_mtx);
                for (const auto handle : finished_promises) {
                    _message_callbacks.erase(handle);
                }
            }
            for (auto &promise : to_fulfill) {
                promise->set_value(message);
            }
        }

        void consumeNetworkExceptionFromNetwork(const std::exception_ptr& exception) noexcept {
            _underlying_network_fault = true;

            std::vector<std::pair<CallbackHandle, Callback>> snapshot;
            {
                std::scoped_lock<std::mutex> lock(_message_callback_mtx);
                snapshot.reserve(_message_callbacks.size());
                for (const auto &entry : _message_callbacks) {
                    snapshot.emplace_back(entry.first, entry.second);
                }
            }

            std::vector<CallbackHandle> finished_promises;
            std::vector<Expectation> to_fail;
            for (auto &entry : snapshot) {
                std::visit([&](auto&& arg) {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, FunctionCallback>) {
                        if (arg.error_callback) {
                            arg.error_callback(exception);
                        }
                    } else if constexpr (std::is_same_v<T, PromiseCallback>) {
                        if (auto promise = arg.promise.lock()) {
                            to_fail.push_back(std::move(promise));
                        }
                        finished_promises.push_back(entry.first);
                    }
                }, entry.second);
            }

            if (!finished_promises.empty()) {
                std::scoped_lock<std::mutex> lock(_message_callback_mtx);
                for (const auto handle : finished_promises) {
                    _message_callbacks.erase(handle);
                }
            }
            for (auto &promise : to_fail) {
                promise->set_exception(exception);
            }
        }

        template<typename T>
        void setSendMessageToNetworkFunc(T send_function) {
            _send_to_network_function = send_function;
        }

        void forceSend(Message &message) {
            if (!_send_to_network_function) {
                return;
            }

            _send_to_network_function(message);
        }


        void send(Message &message) {
            forceSend(message);
        }

        bool alive() const {
            return !_underlying_network_fault && (
                       std::chrono::steady_clock::now() - _last_received_ms < CONNECTION_TIMEOUT);
        }

        CallbackHandle addMessageCallback(const std::function<void(const mav::Message&)> &on_message,
                                          const std::function<void(const std::exception_ptr&)> &on_error) {
            std::scoped_lock<std::mutex> lock(_message_callback_mtx);
            CallbackHandle handle = _next_handle;
            _message_callbacks[handle] = FunctionCallback{on_message, on_error};
            _next_handle++;
            return handle;
        }

        CallbackHandle addMessageCallback(const std::function<void(const mav::Message&)> &on_message) {
            return addMessageCallback(on_message, std::function<void(const std::exception_ptr&)>{});
        }

        CallbackHandle addMessageCallback(const std::function<bool(const mav::Message&)> &selector,
                                          const std::function<void(const mav::Message&)> &on_message,
                                          const std::function<void(const std::exception_ptr&)> &on_error) {
            return addMessageCallback([selector, on_message](const Message &message) {
                if (selector(message)) {
                    on_message(message);
                }
            }, on_error);
        }

        CallbackHandle addMessageCallback(int message_id, const std::function<void(const mav::Message&)> &on_message,
                                          int source_id=mav::ANY_ID, int component_id=mav::ANY_ID) {
            return addMessageCallback([message_id, source_id, component_id](const Message &message) {
                return message.id() == message_id &&
                    (source_id == mav::ANY_ID || message.header().systemId() == source_id) &&
                    (component_id == mav::ANY_ID || message.header().componentId() == component_id);
            }, on_message, std::function<void(const std::exception_ptr&)>{});
        }

        CallbackHandle addMessageCallback(const std::string &message_name, const std::function<void(const mav::Message&)> &on_message,
                                          int source_id=mav::ANY_ID, int component_id=mav::ANY_ID) {
            return addMessageCallback(_message_set.idForMessage(message_name), on_message, source_id, component_id);
        }

        // A callback already in flight may run once more after this returns.
        void removeMessageCallback(CallbackHandle handle) {
            std::scoped_lock<std::mutex> lock(_message_callback_mtx);
            _message_callbacks.erase(handle);
        }

        [[nodiscard]] Expectation expect(std::function<bool(const mav::Message&)> selector) {
            auto promise = std::make_shared<std::promise<Message>>();
            std::scoped_lock<std::mutex> lock(_message_callback_mtx);
            CallbackHandle handle = _next_handle;
            _message_callbacks[handle] = PromiseCallback{promise, std::move(selector)};
            _next_handle++;
            return promise;
        }

        [[nodiscard]] Expectation expect(int message_id, int source_id=mav::ANY_ID,
                                         int component_id=mav::ANY_ID) {
            return expect([message_id, source_id, component_id](const Message &message) {
                    return message.id() == message_id &&
                           (source_id == mav::ANY_ID || message.header().systemId() == source_id) &&
                           (component_id == mav::ANY_ID || message.header().componentId() == component_id);
            });
        }

        [[nodiscard]] inline Expectation expect(const std::string &message_name, int source_id=mav::ANY_ID,
                                         int component_id=mav::ANY_ID) {
            return expect(_message_set.idForMessage(message_name), source_id, component_id);
        }

        Message receive(const Expectation &expectation, int timeout_ms=-1) const {
            auto fut = expectation->get_future();
            if (timeout_ms >= 0) {
                if (fut.wait_for(std::chrono::milliseconds(timeout_ms)) == std::future_status::timeout) {
                    throw TimeoutException("Expected message timed out");
                }
            } else {
                fut.wait();
            }
            auto message = fut.get();
            return message;
        }

        Message inline receive(const std::string &message_type,
                        int source_id,
                        int component_id,
                        int timeout_ms=-1) {
            return receive(expect(message_type, source_id, component_id), timeout_ms);
        }

        Message inline receive(const std::string &message_type, int timeout_ms=-1) {
            return receive(message_type, mav::ANY_ID, mav::ANY_ID, timeout_ms);
        }


        Message inline receive(int message_id, int source_id, int component_id, int timeout_ms=-1) {
            return receive(expect(message_id, source_id, component_id), timeout_ms);
        }

        Message inline receive(int message_id, int timeout_ms=-1) {
            return receive(message_id, mav::ANY_ID, mav::ANY_ID, timeout_ms);
        }

        Message inline receive(std::function<bool(const mav::Message&)> selector, int timeout_ms=-1) {
            return receive(expect(std::move(selector)), timeout_ms);
        }
    };

}


#endif //MAV_CONNECTION_H
