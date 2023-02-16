//
// Created by thomas on 13.01.23.
//

#ifndef MAV_NETWORK_H
#define MAV_NETWORK_H

#include <list>
#include <thread>
#include <array>
#include <thread>
#include <atomic>
#include <iostream>
#include <memory>
#include <utility>
#include <future>
#include "Connection.h"
#include "utils.h"

namespace mav {


    class NetworkError : public std::runtime_error {
    public:
        explicit NetworkError(const char* message) : std::runtime_error(message) {}
        explicit NetworkError(const std::string& message) : std::runtime_error(message) {}
    };

    class NetworkClosed : public NetworkError {
    public:
        explicit NetworkClosed(const char* message) : NetworkError(message) {}
        explicit NetworkClosed(const std::string& message) : NetworkError(message) {}
    };

    class NetworkInterfaceInterrupt : public std::exception {};

    class NetworkInterface {
    public:
        virtual void close() const = 0;
        virtual void send(const uint8_t* data, uint32_t size, ConnectionPartner partner) = 0;
        virtual void flush() = 0;
        virtual ConnectionPartner receive(uint8_t* destination, uint32_t size) = 0;
    };


    class StreamParser {
    private:

        NetworkInterface& _interface;
        const MessageSet& _message_set;

        [[nodiscard]] bool _checkMagicByte() const {
            uint8_t v;
            _interface.receive(&v, 1);
            return v == 0xFD;
        }

    public:
        StreamParser(const MessageSet &message_set, NetworkInterface &interface) :
        _interface(interface), _message_set(message_set) {}

        [[nodiscard]] Message next() const {

            std::array<uint8_t, MessageDefinition::MAX_MESSAGE_SIZE> backing_memory{};
            while (true) {
                // synchronize
                while (!_checkMagicByte()) {}

                backing_memory[0] = 0xFD;
                _interface.receive(backing_memory.data() + 1, MessageDefinition::HEADER_SIZE -1);
                Header header{backing_memory.data()};
                const bool message_is_signed = header.incompatFlags() & 0x01;
                const int wire_length = MessageDefinition::HEADER_SIZE + header.len() + MessageDefinition::CHECKSUM_SIZE +
                                  (message_is_signed ? MessageDefinition::SIGNATURE_SIZE : 0);
                auto partner = _interface.receive(backing_memory.data() + MessageDefinition::HEADER_SIZE,
                                   wire_length - MessageDefinition::HEADER_SIZE);
                const int crc_offset = MessageDefinition::HEADER_SIZE + header.len();

                auto definition_opt = _message_set.getMessageDefinition(header.msgId());
                if (!definition_opt) {
                    // we do not know this message we can not do anything here, not even check the CRC,
                    // do nothing and continue
                    continue;
                }
                auto &definition = definition_opt.get();

                CRC crc;
                crc.accumulate(backing_memory.begin() + 1, backing_memory.begin() + crc_offset);
                crc.accumulate(definition.crcExtra());
                auto crc_received = deserialize<uint16_t>(backing_memory.data() + crc_offset);
                if (crc_received != crc.crc16()) {
                    // crc error. Try to re-sync.
                    continue;
                }

                return Message::_instantiateFromMemory(definition, partner, std::move(backing_memory));
            }
        }
    };


    class NetworkRuntime {
    private:
        std::atomic_bool _should_terminate{false};
        std::thread _receive_thread;

        NetworkInterface& _interface;
        const MessageSet& _message_set;
        StreamParser _parser;
        Identifier _own_id;
        std::mutex _connections_mutex;
        std::unordered_map<ConnectionPartner, std::shared_ptr<Connection>, _ConnectionPartnerHash> _connections;
        std::unique_ptr<std::promise<std::shared_ptr<Connection>>> _first_connection_promise = nullptr;
        uint8_t _seq = 0;

        std::function<void(const std::shared_ptr<Connection>&)> _on_connection;


        void _sendMessage(Message &message, const ConnectionPartner &partner) {
            int wire_length = static_cast<int>(message.finalize(_seq++, _own_id));
            _interface.send(message.data(), wire_length, partner);
        }

        std::shared_ptr<Connection> _addConnection(const ConnectionPartner partner) {
            auto new_connection = std::make_shared<Connection>(_message_set, partner);
            new_connection->template setSendMessageToNetworkFunc([this, partner](Message &message){
                this->_sendMessage(message, partner);
            });

            _connections.insert({partner, new_connection});
            if (_on_connection) {
                _on_connection(new_connection);
            }

            if (_first_connection_promise) {
                _first_connection_promise->set_value(new_connection);
                _first_connection_promise = nullptr;
            }
            return new_connection;
        }


        void _receive_thread_function() {
            while (!_should_terminate.load()) {
                try {
                    auto message = _parser.next();
                    std::lock_guard<std::mutex> lock(_connections_mutex);
                    // get correct connection for this message
                    auto connection_entry = _connections.find(message.source());
                    if (connection_entry == _connections.end()) {
                        // we do not have a connection for this message, create one
                        auto connection = _addConnection(message.source());
                    } else {
                        connection_entry->second->consumeMessageFromNetwork(message);
                    }
                } catch (NetworkError &e) {
                    _should_terminate.store(true);
                    // Spread the network error to all connections
                    for (auto& connection : _connections) {
                        connection.second->consumeNetworkExceptionFromNetwork(std::make_exception_ptr(e));
                    }
                } catch (NetworkInterfaceInterrupt &e) {
                    _should_terminate.store(true);
                }
            }
        }


    public:
        NetworkRuntime(const Identifier &own_id, const MessageSet &message_set, NetworkInterface &interface) :
                _own_id(own_id), _message_set(message_set),
                _interface(interface), _parser(_message_set, _interface) {

            _receive_thread = std::thread{
                [this]() {
                    _receive_thread_function();
                }
            };
        }

        NetworkRuntime(const MessageSet &message_set, NetworkInterface &interface) :
                NetworkRuntime({LIBMAV_DEFAULT_ID, LIBMAV_DEFAULT_ID},
                               message_set, interface) {}

        void onConnection(std::function<void(const std::shared_ptr<Connection>&)> on_connection) {
            _on_connection = std::move(on_connection);
        }

        std::shared_ptr<Connection> awaitConnection(int timeout_ms = -1) {
            {
                std::lock_guard<std::mutex> lock(_connections_mutex);
                if (!_connections.empty()) {
                    return _connections.begin()->second;
                }
            }
            _first_connection_promise = std::make_unique<std::promise<std::shared_ptr<Connection>>>();
            if (timeout_ms >= 0) {
                if (_first_connection_promise->get_future().wait_for(std::chrono::milliseconds(timeout_ms)) == std::future_status::timeout) {
                    throw TimeoutException("Timeout while waiting for first connection");
                }
            }
            return _first_connection_promise->get_future().get();
        }

        void stop() {
            _interface.close();
            _should_terminate.store(true);
            if (_receive_thread.joinable()) {
                _receive_thread.join();
            }
        }

        ~NetworkRuntime() {
            stop();
        }
    };
}




#endif //MAV_NETWORK_H
