//
// Created by thomas on 13.01.23.
//

#ifndef LIBMAVLINK_CONNECTION_H
#define LIBMAVLINK_CONNECTION_H

namespace libmavlink {

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

    };

};


#endif //LIBMAVLINK_CONNECTION_H
