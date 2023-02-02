//
// Created by thomas on 13.01.23.
//

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
    private:
        static constexpr int CONNECTION_TIMEOUT = 5000;

        // connection properties
        const MessageSet& _message_set;
        Identifier _partner_id;

        // connection state
        int _heartbeat_message_id;
        uint64_t _last_heartbeat_ms = 0;

        // callbacks
        std::function<void(Message &message)> _send_to_network_function;
        std::function<void(void)> _on_connect_callback;
        std::function<void(void)> _on_disconnect_callback;

        std::mutex _message_callback_mtx;

        CallbackHandle _next_handle = 0;
        std::unordered_map<CallbackHandle, std::function<void(const Message &message)>> _message_callbacks;

    public:
        Connection(const MessageSet &message_set, Identifier partner_id) :
        _message_set(message_set), _partner_id(partner_id) {
            _heartbeat_message_id = _message_set.idForMessage("HEARTBEAT");
        }

        void consumeMessageFromNetwork(const Message& message) {
            // This connection is not concerned about messages from this partner. Ignore.
            if (message.header().source() != _partner_id) {
                return;
            }

            // in case we received a heartbeat, update last heartbeat time, to keep the connection alive.
            if (message.header().msgId() == _heartbeat_message_id) {
                _last_heartbeat_ms = millis();
            }
            {
                std::scoped_lock<std::mutex> lock(_message_callback_mtx);
                for (const auto& item : _message_callbacks) {
                    item.second(message);
                }
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
            if (millis() - _last_heartbeat_ms >= CONNECTION_TIMEOUT) {
                //throw TimeoutException{"Mavlink connection timed out"};
            }
            forceSend(message);
        }

        template<typename T>
        CallbackHandle addMessageCallback(const T &on_message) {
            std::scoped_lock<std::mutex> lock(_message_callback_mtx);
            CallbackHandle handle = _next_handle;
            _message_callbacks[handle] = on_message;
            _next_handle++;
            return handle;
        }

        void removeMessageCallback(CallbackHandle handle) {
            std::scoped_lock<std::mutex> lock(_message_callback_mtx);
            _message_callbacks.erase(handle);
        }


        class Expectation {
            friend Connection;
        private:
            CallbackHandle _handle;
            std::shared_ptr<std::promise<Message>> _promise;
        public:
            Expectation(CallbackHandle handle, std::shared_ptr<std::promise<Message>> promise) : _handle(handle),
            _promise(std::move(promise)) {}
        };


        [[nodiscard]] Expectation expect(int message_id, int source_id=mav::ANY_ID,
                                         int component_id=mav::ANY_ID) {
            auto prom = std::make_shared<std::promise<Message>>();
            CallbackHandle handle = addMessageCallback(
                    [prom, message_id, source_id, component_id](const Message &message) {
                if (message.id() == message_id) {
                    if ((source_id == mav::ANY_ID || message.header().systemId() == source_id) &&
                            (component_id == mav::ANY_ID || message.header().componentId() == component_id)) {
                        prom->set_value(message);
                    }
                }
            });
            return {handle, prom};
        }

        [[nodiscard]] inline Expectation expect(const std::string &message_name, int source_id=mav::ANY_ID,
                                         int component_id=mav::ANY_ID) {
            return expect(_message_set.idForMessage(message_name), source_id, component_id);
        }

        Message receive(const Expectation &expectation, int timeout_ms=-1) {
            auto fut = expectation._promise->get_future();
            if (timeout_ms >= 0) {
                if (fut.wait_for(std::chrono::milliseconds(timeout_ms)) == std::future_status::timeout) {
                    removeMessageCallback(expectation._handle);
                    throw TimeoutException("Expected message timed out");
                }
            }
            auto message = fut.get();
            removeMessageCallback(expectation._handle);
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
