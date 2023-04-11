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
        virtual void close() = 0;
        virtual void send(const uint8_t* data, uint32_t size, ConnectionPartner partner) = 0;
        virtual ConnectionPartner receive(uint8_t* destination, uint32_t size) = 0;
        virtual void markMessageBoundary() {};
        virtual void markSyncing() {};
        virtual bool isConnectionOriented() const {
            return false;
        };
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
                _interface.markMessageBoundary();
                if (!_checkMagicByte()) {
                    _interface.markSyncing();
                    // synchronize
                    while (!_checkMagicByte()) {}
                }

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
                auto crc_received = deserialize<uint16_t>(backing_memory.data() + crc_offset, sizeof(uint16_t));
                if (crc_received != crc.crc16()) {
                    // crc error. Try to re-sync.
                    continue;
                }
                return Message::_instantiateFromMemory(definition, partner, crc_offset, std::move(backing_memory));
            }
        }
    };


    class NetworkRuntime {
    private:
        std::atomic_bool _should_terminate{false};
        std::thread _receive_thread;
        std::thread _heartbeat_thread;

        NetworkInterface& _interface;
        const MessageSet& _message_set;
        std::optional<Message> _heartbeat_message;
        std::mutex _heartbeat_message_mutex;
        StreamParser _parser;
        Identifier _own_id;
        std::mutex _connections_mutex;
        std::mutex _send_mutex;
        std::unordered_map<ConnectionPartner, std::shared_ptr<Connection>, _ConnectionPartnerHash> _connections;
        std::unique_ptr<std::promise<std::shared_ptr<Connection>>> _first_connection_promise = nullptr;
        uint8_t _seq = 0;

        std::function<void(const std::shared_ptr<Connection>&)> _on_connection;
        std::function<void(const std::shared_ptr<Connection>&)> _on_connection_lost;

        void _sendMessage(Message &message, const ConnectionPartner &partner) {
            int wire_length = static_cast<int>(message.finalize(_seq++, _own_id));
            std::unique_lock<std::mutex> lock(_send_mutex);
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
                        connection->consumeMessageFromNetwork(message);
                    } else {
                        connection_entry->second->consumeMessageFromNetwork(message);
                    }
                } catch (NetworkError &e) {
                    _should_terminate.store(true);
                    // Spread the network error to all connections
                    std::lock_guard<std::mutex> lock(_connections_mutex);
                    for (auto& connection : _connections) {
                        connection.second->consumeNetworkExceptionFromNetwork(std::make_exception_ptr(e));
                    }
                } catch (NetworkInterfaceInterrupt &e) {
                    _should_terminate.store(true);
                }
            }
        }

        void _heartbeat_thread_function() {
            while (!_should_terminate.load()) {
                try {
                    std::lock_guard<std::mutex> lock(_connections_mutex);
                    std::lock_guard<std::mutex> heartbeat_message_lock(_heartbeat_message_mutex);
                        if (_heartbeat_message) {
                            if (_interface.isConnectionOriented()) {
                                // For physical protocols that maintain in internal "connection" state, we can
                                // send the heartbeat to all the partners even if there is no "mavlink" connection
                                // established yet (e.g. they don't send us heartbeats).
                                _sendMessage(_heartbeat_message.value(), {});
                            } else {
                                // For connectionless protocols, the only connection state we have is the mavlink
                                // connection state. So we only send the heartbeat to the partners that have
                                // sent us heartbeats before.
                                for (auto& connection : _connections) {
                                    _sendMessage(_heartbeat_message.value(), connection.first);
                                }
                            }
                        }
                } catch (NetworkError &e) {
                    _should_terminate.store(true);
                    // Spread the network error to all connections
                    std::lock_guard<std::mutex> lock(_connections_mutex);
                    for (auto& connection : _connections) {
                        connection.second->consumeNetworkExceptionFromNetwork(std::make_exception_ptr(e));
                    }
                } catch (NetworkInterfaceInterrupt &e) {
                    _should_terminate.store(true);
                }

                // clean up dead connections
                {
                    std::unique_lock<std::mutex> lock(_connections_mutex);
                    for (auto it = _connections.begin(); it != _connections.end();) {

                        if (!it->second->alive()) {
                            if (_on_connection_lost) {
                                _on_connection_lost(it->second);
                            }
                            it = _connections.erase(it);
                        } else {
                            ++it;
                        }
                    }
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
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

            _heartbeat_thread = std::thread{
                    [this]() {
                        _heartbeat_thread_function();
                    }
            };
        }

        NetworkRuntime(const MessageSet &message_set, NetworkInterface &interface) :
                NetworkRuntime({LIBMAV_DEFAULT_ID, LIBMAV_DEFAULT_ID},
                               message_set, interface) {}

        NetworkRuntime(const Identifier &own_id, const MessageSet &message_set, const Message &heartbeat_message, NetworkInterface &interface) :
                NetworkRuntime(own_id, message_set, interface) {
            setHeartbeatMessage(heartbeat_message);
        }

        NetworkRuntime(const MessageSet &message_set, const Message &heartbeat_message, NetworkInterface &interface) :
                NetworkRuntime({LIBMAV_DEFAULT_ID, LIBMAV_DEFAULT_ID}, message_set, heartbeat_message, interface) {}


        void onConnection(std::function<void(const std::shared_ptr<Connection>&)> on_connection) {
            _on_connection = std::move(on_connection);
        }

        void onConnectionLost(std::function<void(const std::shared_ptr<Connection>&)> on_connection_lost) {
            _on_connection_lost = std::move(on_connection_lost);
        }

        std::shared_ptr<Connection> awaitConnection(int timeout_ms = -1) {
            {
                std::lock_guard<std::mutex> lock(_connections_mutex);
                if (!_connections.empty()) {
                    return _connections.begin()->second;
                }
            }
            _first_connection_promise = std::make_unique<std::promise<std::shared_ptr<Connection>>>();
            auto fut = _first_connection_promise->get_future();
            if (timeout_ms >= 0) {
                if (fut.wait_for(std::chrono::milliseconds(timeout_ms)) == std::future_status::timeout) {
                    throw TimeoutException("Timeout while waiting for first connection");
                }
            }
            return fut.get();
        }

        void setHeartbeatMessage(const Message &message) {
            std::unique_lock<std::mutex> lock(_send_mutex);
            _heartbeat_message = message;
        }

        void clearHeartbeat() {
            std::unique_lock<std::mutex> lock(_send_mutex);
            _heartbeat_message = std::nullopt;
        }

        void stop() {
            _interface.close();
            _should_terminate.store(true);
            if (_receive_thread.joinable()) {
                _receive_thread.join();
            }
            if (_heartbeat_thread.joinable()) {
                _heartbeat_thread.join();
            }
        }

        ~NetworkRuntime() {
            stop();
        }
    };
}




#endif //MAV_NETWORK_H
