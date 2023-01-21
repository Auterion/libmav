//
// Created by thomas on 13.01.23.
//

#ifndef MAV_CONNECTION_H
#define MAV_CONNECTION_H

#include <mutex>

namespace mav {

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
        std::function<void(const Message &message)> _send_to_network_function;
        std::function<void(void)> _on_connect_callback;
        std::function<void(void)> _on_disconnect_callback;

        std::mutex _message_callback_mtx;
        std::function<void(const Message &message)> _message_callback;

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

            if (_message_callback) {
                std::scoped_lock<std::mutex> lock(_message_callback_mtx);
                _message_callback(message);
            }
        }

        template<typename T>
        void setSendMessageToNetworkFunc(T send_function) {
            _send_to_network_function = send_function;
        }

        void forceSend(const Message &message) {
            if (!_send_to_network_function) {
                return;
            }

            _send_to_network_function(message);
        }


        void send(const Message &message) {
            if (millis() - _last_heartbeat_ms >= CONNECTION_TIMEOUT) {
                throw TimeoutException{"Mavlink connection timed out"};
            }
            forceSend(message);
        }

        template<typename T>
        void setMessageCallback(const T &on_message) {
            std::scoped_lock<std::mutex> lock(_message_callback_mtx);
            _message_callback = on_message;
        }

    };

};


#endif //MAV_CONNECTION_H
