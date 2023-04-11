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

        struct FunctionCallback {
            std::function<void(const Message &message)> callback;
            std::function<void(const std::exception_ptr& exception)> error_callback;
        };

        struct PromiseCallback {
            Expectation promise;
            int message_id;
            int system_id;
            int component_id;
        };

        using Callback = std::variant<FunctionCallback, PromiseCallback>;

        static constexpr int CONNECTION_TIMEOUT = 3000;

        // connection properties
        const MessageSet& _message_set;
        ConnectionPartner _partner;

        // connection state
        uint64_t _last_received_ms = 0;

        // callbacks
        std::function<void(Message &message)> _send_to_network_function;

        std::mutex _message_callback_mtx;

        CallbackHandle _next_handle = 0;
        std::unordered_map<CallbackHandle, Callback> _message_callbacks;

    public:

        void removeAllCallbacks() {
            std::scoped_lock<std::mutex> lock(_message_callback_mtx);
            _message_callbacks.clear();
        }

        Connection(const MessageSet &message_set, ConnectionPartner partner) :
        _message_set(message_set), _partner(partner) {
            _last_received_ms = millis();
        }

        ConnectionPartner partner() const {
            return _partner;
        }

        void consumeMessageFromNetwork(const Message& message) {
            // in case we received a heartbeat, update last heartbeat time, to keep the connection alive.
            _last_received_ms = millis();

            {
                std::scoped_lock<std::mutex> lock(_message_callback_mtx);
                auto it = _message_callbacks.begin();
                while (it != _message_callbacks.end()) {
                    Callback &callback = it->second;
                    std::visit([this, &message, &it](auto&& arg) {
                        using T = std::decay_t<decltype(arg)>;
                        if constexpr (std::is_same_v<T, FunctionCallback>) {
                            if (arg.callback) {
                                arg.callback(message);
                            }
                            it++;
                        } else if constexpr (std::is_same_v<T, PromiseCallback>) {
                            if (message.id() == arg.message_id &&
                                    (arg.system_id == mav::ANY_ID || message.header().systemId() == arg.system_id) &&
                                    (arg.component_id == mav::ANY_ID || message.header().componentId() == arg.component_id)) {
                                arg.promise->set_value(message);
                                it = _message_callbacks.erase(it);
                            } else {
                                it++;
                            }
                        }
                    }, callback);
                }
            }
        }

        void consumeNetworkExceptionFromNetwork(const std::exception_ptr& exception) {
            std::scoped_lock<std::mutex> lock(_message_callback_mtx);
            auto it = _message_callbacks.begin();
            while (it != _message_callbacks.end()) {
                Callback &callback = it->second;
                std::visit([this, &exception, &it](auto&& arg) {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, FunctionCallback>) {
                        if (arg.error_callback) {
                            arg.error_callback(exception);
                        }
                        it++;
                    } else if constexpr (std::is_same_v<T, PromiseCallback>) {
                        arg.promise->set_exception(exception);
                        it = _message_callbacks.erase(it);
                    }
                }, callback);
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
            return millis() - _last_received_ms < CONNECTION_TIMEOUT;
        }

        template<typename T, typename E>
        CallbackHandle addMessageCallback(const T &on_message, const E &on_error) {
            std::scoped_lock<std::mutex> lock(_message_callback_mtx);
            CallbackHandle handle = _next_handle;
            _message_callbacks[handle] = FunctionCallback{on_message, on_error};
            _next_handle++;
            return handle;
        }

        template<typename T>
        CallbackHandle addMessageCallback(const T &on_message) {
            return addMessageCallback(on_message, nullptr);
        }

        void removeMessageCallback(CallbackHandle handle) {
            std::scoped_lock<std::mutex> lock(_message_callback_mtx);
            _message_callbacks.erase(handle);
        }


        [[nodiscard]] Expectation expect(int message_id, int source_id=mav::ANY_ID,
                                         int component_id=mav::ANY_ID) {

            auto promise = std::make_shared<std::promise<Message>>();
            std::scoped_lock<std::mutex> lock(_message_callback_mtx);
            CallbackHandle handle = _next_handle;
            _message_callbacks[handle] = PromiseCallback{promise, message_id, source_id, component_id};
            _next_handle++;

            auto prom = std::make_shared<std::promise<Message>>();
            return promise;
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
    };

};


#endif //MAV_CONNECTION_H
